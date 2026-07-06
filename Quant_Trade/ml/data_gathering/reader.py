"""
Utilities for loading and inspecting recorded tick data.

Used during feature engineering (Phase 2) and model training (Phase 4).
"""

import logging
import os
from datetime import date, datetime
from typing import Optional

import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq

from ml.data_gathering.schema import ARROW_SCHEMA

logger = logging.getLogger(__name__)


def load_ticks(
    data_dir: str,
    symbol: str,
    start_date: Optional[date] = None,
    end_date: Optional[date] = None,
    columns: Optional[list[str]] = None,
) -> pd.DataFrame:
    """
    Load tick data for a symbol into a DataFrame.

    Args:
        data_dir:   Root data directory (same as DataConfig.output_dir).
        symbol:     e.g. "BTC-USD"
        start_date: Only load files on or after this date.
        end_date:   Only load files on or before this date.
        columns:    Subset of columns to load (None = all).

    Returns:
        DataFrame sorted by timestamp_ns with a UTC datetime index.
    """
    safe_sym = symbol.replace("/", "-")
    symbol_dir = os.path.join(data_dir, safe_sym)

    if not os.path.isdir(symbol_dir):
        raise FileNotFoundError(
            f"No data directory for symbol '{symbol}' at {symbol_dir}"
        )

    files = _find_parquet_files(symbol_dir, start_date, end_date)
    if not files:
        raise FileNotFoundError(
            f"No Parquet files found for {symbol} in {symbol_dir} "
            f"(range: {start_date} → {end_date})"
        )

    logger.info("loading %d files for symbol=%s", len(files), symbol)

    tables = [pq.read_table(f, schema=ARROW_SCHEMA, columns=columns) for f in files]
    df = pa.concat_tables(tables).to_pandas()
    df = df.sort_values("timestamp_ns").reset_index(drop=True)
    df.index = pd.to_datetime(df["timestamp_ns"], unit="ns", utc=True)

    logger.info(
        "loaded symbol=%s rows=%d range=[%s, %s]",
        symbol, len(df), df.index[0].isoformat(), df.index[-1].isoformat(),
    )
    return df


def describe_data(data_dir: str, symbol: str) -> None:
    """Print a quick health summary for recorded data.  Run before feature engineering."""
    df = load_ticks(data_dir, symbol)
    print(f"\n{'=' * 50}")
    print(f"Symbol:    {symbol}")
    print(f"Rows:      {len(df):,}")
    print(f"Start:     {df.index[0]}")
    print(f"End:       {df.index[-1]}")
    duration_h = (df.index[-1] - df.index[0]).total_seconds() / 3600
    print(f"Duration:  {duration_h:.1f} hours")
    print(f"Rate:      {len(df) / duration_h:.0f} ticks/hour")
    print(f"\nPrice range:")
    print(f"  bid:     {df['bid'].min():.4f} → {df['bid'].max():.4f}")
    print(f"  ask:     {df['ask'].min():.4f} → {df['ask'].max():.4f}")
    print(f"  spread:  {(df['ask'] - df['bid']).mean():.6f} avg")
    nulls = df.isnull().sum()
    print("\nNulls:")
    print(nulls[nulls > 0].to_string() if nulls.any() else "  none")
    print(f"{'=' * 50}\n")


def check_sequence_gaps(data_dir: str, symbol: str) -> pd.DataFrame:
    """
    Return a DataFrame of sequence number gap events for a symbol.
    Useful for auditing data quality before training.
    """
    df = load_ticks(data_dir, symbol, columns=["timestamp_ns", "sequence"])
    gaps = df["sequence"].diff()
    gap_rows = df[gaps > 1].copy()
    gap_rows["gap_size"] = gaps[gaps > 1]

    if gap_rows.empty:
        logger.info("no sequence gaps found for %s", symbol)
    else:
        logger.warning(
            "%d sequence gaps found for %s (total dropped ticks ≈ %d)",
            len(gap_rows), symbol, int(gap_rows["gap_size"].sum()) - len(gap_rows),
        )
    return gap_rows


# ------------------------------------------------------------------
# Internal helpers
# ------------------------------------------------------------------

def _find_parquet_files(
    symbol_dir: str,
    start_date: Optional[date],
    end_date: Optional[date],
) -> list[str]:
    """Return a sorted list of Parquet files optionally filtered by date."""
    files = []
    for fname in sorted(os.listdir(symbol_dir)):
        if not fname.endswith(".parquet"):
            continue
        if start_date or end_date:
            # filename format: ticks_YYYYMMDD_NNNN.parquet
            try:
                date_str = fname.split("_")[1]
                file_date = datetime.strptime(date_str, "%Y%m%d").date()
            except (IndexError, ValueError):
                files.append(os.path.join(symbol_dir, fname))
                continue
            if start_date and file_date < start_date:
                continue
            if end_date and file_date > end_date:
                continue
        files.append(os.path.join(symbol_dir, fname))
    return files
