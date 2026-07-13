"""
Tick data validation.

Every tick is checked before entering the write buffer.

Sequence gaps are NOT failures — the tick is still valid and is kept.
seq_gap=True is stamped on it so Phase 3 can mask labels whose forward
window crosses the gap.  Everything else that fails is dropped and logged.
"""

import logging
from dataclasses import dataclass
from typing import Optional

from ml.data_gathering.schema import Tick

logger = logging.getLogger(__name__)


@dataclass
class ValidationResult:
    valid: bool
    reason: Optional[str] = None


class TickValidator:
    """
    Validates ticks and tracks aggregate pass/fail statistics.
    Call report() periodically to log validation health.
    """

    def __init__(
        self,
        max_spread_bps: float = 500.0,
        max_price_jump_pct: float = 5.0,
        min_size: float = 0.0,
    ) -> None:
        self.max_spread_bps = max_spread_bps
        self.max_price_jump_pct = max_price_jump_pct
        self.min_size = min_size

        self._last_price: dict[str, float] = {}
        self._last_seq: dict[str, int] = {}

        self.total: int = 0
        self.passed: int = 0
        self.seq_gaps_detected: int = 0
        self.seq_gaps_ticks_missing: int = 0
        self.failed: dict[str, int] = {}

    def validate(self, tick: Tick) -> ValidationResult:
        """
        Run all checks on tick.

        Sequence gaps set seq_gap=True but do NOT reject the tick.
        All other failures drop it and return valid=False.
        """
        self.total += 1

        # --- structural checks ---
        if tick.bid <= 0 or tick.ask <= 0:
            return self._fail(tick, "non_positive_price")

        if tick.bid >= tick.ask:
            return self._fail(tick, "crossed_book")

        if tick.bid_sz < self.min_size or tick.ask_sz < self.min_size:
            return self._fail(tick, "size_below_minimum")

        if tick.last_price < 0:
            return self._fail(tick, "negative_last_price")

        if tick.volume < 0:
            return self._fail(tick, "negative_volume")

        # --- spread sanity ---
        spread_bps = (tick.ask - tick.bid) / tick.bid * 10_000
        if spread_bps > self.max_spread_bps:
            return self._fail(tick, f"spread_too_wide_{spread_bps:.0f}bps")

        # --- price jump check (skipped on first tick for a symbol) ---
        last_px = self._last_price.get(tick.symbol)
        if last_px is not None and last_px > 0:
            jump_pct = abs(tick.last_price - last_px) / last_px * 100
            if jump_pct > self.max_price_jump_pct:
                return self._fail(tick, f"price_jump_{jump_pct:.1f}pct")

        # --- sequence gap check ---
        # A gap marks the tick as suspect for label masking but keeps it.
        last_seq = self._last_seq.get(tick.symbol)
        if last_seq is not None and tick.sequence > last_seq + 1:
            missing = tick.sequence - last_seq - 1
            tick.seq_gap = True
            self.seq_gaps_detected += 1
            self.seq_gaps_ticks_missing += missing
            logger.warning(
                "sequence_gap symbol=%s last_seq=%d current_seq=%d missing=%d — "
                "tick stamped seq_gap=True",
                tick.symbol, last_seq, tick.sequence, missing,
            )

        # --- all checks passed: update state ---
        self._last_price[tick.symbol] = tick.last_price
        self._last_seq[tick.symbol] = tick.sequence
        self.passed += 1
        return ValidationResult(valid=True)

    def _fail(self, tick: Tick, reason: str) -> ValidationResult:
        self.failed[reason] = self.failed.get(reason, 0) + 1
        logger.debug(
            "tick_rejected symbol=%s seq=%d reason=%s",
            tick.symbol, tick.sequence, reason,
        )
        return ValidationResult(valid=False, reason=reason)

    def report(self) -> None:
        """Log a one-line health summary."""
        if self.total == 0:
            return
        pass_rate = self.passed / self.total * 100
        logger.info(
            "validation total=%d passed=%d (%.1f%%) "
            "seq_gap_events=%d seq_gap_ticks_missing=%d failed_reasons=%s",
            self.total, self.passed, pass_rate,
            self.seq_gaps_detected, self.seq_gaps_ticks_missing,
            self.failed,
        )
        if pass_rate < 95.0:
            logger.warning(
                "validation pass rate %.1f%% is below 95%% — "
                "check exchange simulator output", pass_rate,
            )
