"""
Market data recorder — Phase 1 entry point.

Connects to the exchange simulator WebSocket feed, validates each tick,
and writes to Parquet via a buffered writer.

Run with:
    PYTHONPATH=. python -m ml.data_gathering.recorder

Or set environment variables:
    EXCHANGE_WS_URL=ws://localhost:8080/market-data  \\
    ML_DATA_DIR=ml/data/raw                          \\
    PYTHONPATH=. python -m ml.data_gathering.recorder
"""

import asyncio
import json
import logging
import signal
import time

import websockets
from websockets.exceptions import ConnectionClosed, WebSocketException

from ml.data_gathering.config import DataConfig
from ml.data_gathering.schema import Tick, from_ws_message
from ml.data_gathering.validator import TickValidator
from ml.data_gathering.writer import ParquetWriter

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
    datefmt="%Y-%m-%dT%H:%M:%S",
)
logger = logging.getLogger(__name__)


class MarketDataRecorder:
    """
    Subscribes to the exchange simulator WebSocket and records ticks to Parquet.

    Reconnects with exponential backoff on connection failures.
    Handles SIGINT / SIGTERM gracefully — flushes all buffers before exiting.
    """

    def __init__(self, config: DataConfig) -> None:
        self.config = config
        self.validator = TickValidator()
        self.writer = ParquetWriter(
            output_dir=config.output_dir,
            buffer_size=config.buffer_size,
            flush_interval_s=config.flush_interval_s,
            max_file_ticks=config.max_file_ticks,
        )
        self._running = False
        self._ticks_received: int = 0
        self._ticks_since_stat: int = 0
        self._last_stat_time: float = time.monotonic()

    # ------------------------------------------------------------------
    # Public
    # ------------------------------------------------------------------

    async def run(self) -> None:
        self._running = True
        await self.writer.start()

        loop = asyncio.get_running_loop()
        # add_signal_handler is Unix-only; fall back to signal.signal on Windows.
        try:
            for sig in (signal.SIGINT, signal.SIGTERM):
                loop.add_signal_handler(sig, lambda: asyncio.ensure_future(self._shutdown()))
        except NotImplementedError:
            # Windows: use standard signal module instead.
            import signal as _signal
            _signal.signal(_signal.SIGINT,  lambda s, f: asyncio.ensure_future(self._shutdown()))
            _signal.signal(_signal.SIGTERM, lambda s, f: asyncio.ensure_future(self._shutdown()))

        logger.info(
            "recorder starting url=%s symbols=%s",
            self.config.ws_url,
            self.config.symbols or "all",
        )

        delay = self.config.reconnect_delay_s
        while self._running:
            try:
                await self._connect_and_record()
                delay = self.config.reconnect_delay_s  # reset on clean disconnect
            except (ConnectionClosed, WebSocketException, OSError) as exc:
                if not self._running:
                    break
                logger.warning("connection lost: %s — reconnecting in %.1fs", exc, delay)
                await asyncio.sleep(delay)
                delay = min(delay * 2, self.config.max_reconnect_delay_s)
            except Exception as exc:
                logger.error("unexpected error: %s", exc, exc_info=True)
                await asyncio.sleep(delay)
                delay = min(delay * 2, self.config.max_reconnect_delay_s)

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    async def _connect_and_record(self) -> None:
        logger.info("connecting to %s", self.config.ws_url)
        async with websockets.connect(
            self.config.ws_url,
            # Disable client-side pings — the Go backend sends its own pings every 20s.
            # Having both sides send pings causes duplicate ping handling and can
            # expose raw control frames as application messages in some websockets versions.
            ping_interval=None,
            open_timeout=10,
            max_size=2 ** 20,
        ) as ws:
            logger.info("connected")
            if self.config.symbols:
                await ws.send(json.dumps({"action": "subscribe", "symbols": self.config.symbols}))
                logger.info("subscribed to symbols=%s", self.config.symbols)

            async for raw_msg in ws:
                if not self._running:
                    return
                self._handle_message(raw_msg)

    def _handle_message(self, raw_msg) -> None:
        # The Go gateway sends JSON text frames, but WebSocket control frames
        # (e.g. server-side pings from pingLoop) can arrive as bytes objects in
        # some versions of the websockets library.  We must handle both.
        try:
            if isinstance(raw_msg, bytes):
                # Binary control frames (pings) contain no JSON — skip them.
                # Text frames mis-delivered as bytes are UTF-8 JSON; decode first.
                try:
                    raw_msg = raw_msg.decode("utf-8")
                except UnicodeDecodeError:
                    # Not a UTF-8 text payload — definitely a control/binary frame.
                    return
            msg = json.loads(raw_msg)
        except json.JSONDecodeError:
            return

        if msg.get("type") != "tick":
            return

        try:
            tick: Tick = from_ws_message(msg)
        except (KeyError, TypeError, ValueError) as exc:
            logger.warning("malformed tick: %s — %s", exc, msg)
            return

        result = self.validator.validate(tick)
        if not result.valid:
            return

        self.writer.add(tick)
        self._ticks_received += 1
        self._ticks_since_stat += 1
        self._maybe_log_stats()

    def _maybe_log_stats(self) -> None:
        now = time.monotonic()
        elapsed = now - self._last_stat_time
        if elapsed < self.config.stats_interval_s:
            return
        rate = self._ticks_since_stat / elapsed
        logger.info("stats total_ticks=%d rate=%.0f ticks/s", self._ticks_received, rate)
        self.validator.report()
        self._ticks_since_stat = 0
        self._last_stat_time = now

    async def _shutdown(self) -> None:
        logger.info("shutdown received — flushing buffers")
        self._running = False
        await self.writer.stop()
        logger.info("shutdown complete total_ticks=%d", self._ticks_received)


# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------

async def main() -> None:
    config = DataConfig.from_env()
    recorder = MarketDataRecorder(config)
    await recorder.run()


if __name__ == "__main__":
    asyncio.run(main())
