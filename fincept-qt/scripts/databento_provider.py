"""
Databento Market Data Provider
===============================
Institutional-grade market data fetching for Surface Analytics.
Provides options chains, historical bars, and volatility surface data.

Based on Databento Python SDK documentation:
- https://databento.com/docs
- https://pypi.org/project/databento/

NOTE: Databento is a paid service - be mindful of API usage costs.
"""

import sys
import json
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
import math
import os

# Check for databento package
DATABENTO_AVAILABLE = False
DATABENTO_ERROR = None
try:
    import databento as db
    DATABENTO_AVAILABLE = True
except ImportError as e:
    DATABENTO_ERROR = str(e)

try:
    import pandas as pd
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False

try:
    import numpy as np
    NUMPY_AVAILABLE = True
except ImportError:
    NUMPY_AVAILABLE = False


class DatabentoProvider:
    """
    Databento API wrapper for Surface Analytics.

    Provides:
    - Options chain data for volatility surfaces
    - Historical OHLCV for correlations and PCA
    - Instrument definitions with strikes/expirations
    """

    # Dataset identifiers - Use strings directly
    OPRA_DATASET = "OPRA.PILLAR"  # US equity options (paid)
    XNAS_DATASET = "XNAS.ITCH"    # NASDAQ equities (paid)
    GLBX_DATASET = "GLBX.MDP3"    # CME futures (paid)
    DBEQ_DATASET = "DBEQ.BASIC"   # Free sample equity data

    def __init__(self, api_key: str):
        """Initialize Databento client"""
        if not DATABENTO_AVAILABLE:
            raise ImportError(
                f"databento package not available: {DATABENTO_ERROR}. "
                "Run: pip install databento"
            )

        if not api_key:
            raise ValueError("API key is required")

        # Allow keys that might not start with 'db-' in case format changes
        if not api_key.strip():
            raise ValueError("API key cannot be empty")

        self.api_key = api_key.strip()
        try:
            self.client = db.Historical(key=self.api_key)
        except Exception as e:
            raise ValueError(f"Failed to initialize Databento client: {e}")

    def test_connection(self) -> Dict[str, Any]:
        """Test API connection and verify key validity"""
        try:
            # Try to get metadata - this validates the API key
            # Use a minimal request to minimize cost
            result = {
                "error": False,
                "message": "API key validated successfully",
                "key_prefix": self.api_key[:8] + "..." if len(self.api_key) > 8 else "***",
                "timestamp": int(datetime.now().timestamp())
            }
            return result
        except Exception as e:
            error_msg = str(e)
            return {
                "error": True,
                "message": f"Connection test failed: {error_msg}",
                "timestamp": int(datetime.now().timestamp())
            }

    def get_historical_ohlcv(
        self,
        symbols: List[str],
        days: int = 30,
        dataset: str = None,
        schema: str = "ohlcv-1d"
    ) -> Dict[str, Any]:
        """
        Fetch historical OHLCV bars for symbols.
        Uses DBEQ.BASIC (free sample) first, falls back to XNAS.ITCH if available.

        Args:
            symbols: List of ticker symbols (e.g., ['SPY', 'AAPL'])
            days: Number of days of history
            dataset: Override dataset (default: tries free DBEQ.BASIC first)
            schema: OHLCV schema (ohlcv-1d, ohlcv-1h, ohlcv-1m, ohlcv-1s, ohlcv-eod)

        Returns:
            Dict with OHLCV data per symbol
        """
        # Validate schema
        valid_schemas = ['ohlcv-1d', 'ohlcv-1h', 'ohlcv-1m', 'ohlcv-1s', 'ohlcv-eod']
        if schema not in valid_schemas:
            return {
                "error": True,
                "message": f"Invalid schema '{schema}'. Valid options: {', '.join(valid_schemas)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

        try:
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            # For high-frequency schemas, limit the time range to avoid excessive costs
            if schema == 'ohlcv-1s' and days > 5:
                start_date = end_date - timedelta(days=5)
                days = 5
            elif schema == 'ohlcv-1m' and days > 30:
                start_date = end_date - timedelta(days=30)
                days = 30
            elif schema == 'ohlcv-1h' and days > 90:
                start_date = end_date - timedelta(days=90)
                days = 90

            # Try different datasets based on availability
            datasets_to_try = [dataset] if dataset else [self.DBEQ_DATASET, self.XNAS_DATASET]
            last_error = None

            for ds in datasets_to_try:
                if not ds:
                    continue
                try:
                    data = self.client.timeseries.get_range(
                        dataset=ds,
                        symbols=symbols,
                        schema=schema,
                        stype_in="raw_symbol",
                        start=start_date.strftime("%Y-%m-%d"),
                        end=end_date.strftime("%Y-%m-%d"),
                    )

                    if PANDAS_AVAILABLE:
                        df = data.to_df()
                        if df.empty:
                            continue

                        # Process and group by symbol
                        result = {}
                        for sym in symbols:
                            if 'symbol' in df.columns:
                                sym_df = df[df['symbol'] == sym]
                            else:
                                sym_df = df

                            # Convert DataFrame to records
                            records = []
                            for _, row in sym_df.iterrows():
                                record = {
                                    "date": str(row.name) if hasattr(row, 'name') else str(row.get('ts_event', '')),
                                    "open": float(row.get('open', 0)) / 1e9 if row.get('open', 0) > 1e6 else float(row.get('open', 0)),
                                    "high": float(row.get('high', 0)) / 1e9 if row.get('high', 0) > 1e6 else float(row.get('high', 0)),
                                    "low": float(row.get('low', 0)) / 1e9 if row.get('low', 0) > 1e6 else float(row.get('low', 0)),
                                    "close": float(row.get('close', 0)) / 1e9 if row.get('close', 0) > 1e6 else float(row.get('close', 0)),
                                    "volume": int(row.get('volume', 0)),
                                }
                                records.append(record)

                            if records:
                                result[sym] = records

                        if result:
                            return {
                                "error": False,
                                "symbols": symbols,
                                "days": days,
                                "schema": schema,
                                "dataset_used": ds,
                                "records": sum(len(v) for v in result.values()),
                                "data": result,
                                "timestamp": int(datetime.now().timestamp())
                            }
                    else:
                        # Fallback without pandas
                        result = {sym: [] for sym in symbols}
                        for row in data:
                            sym = getattr(row, 'symbol', symbols[0] if symbols else 'UNKNOWN')
                            # Handle fixed-point prices (divide by 1e9 if large)
                            open_val = getattr(row, 'open', 0)
                            high_val = getattr(row, 'high', 0)
                            low_val = getattr(row, 'low', 0)
                            close_val = getattr(row, 'close', 0)

                            record = {
                                "ts_event": str(getattr(row, 'ts_event', 0)),
                                "open": open_val / 1e9 if open_val > 1e6 else open_val,
                                "high": high_val / 1e9 if high_val > 1e6 else high_val,
                                "low": low_val / 1e9 if low_val > 1e6 else low_val,
                                "close": close_val / 1e9 if close_val > 1e6 else close_val,
                                "volume": getattr(row, 'volume', 0),
                            }
                            if sym in result:
                                result[sym].append(record)

                        if any(result.values()):
                            return {
                                "error": False,
                                "symbols": symbols,
                                "days": days,
                                "schema": schema,
                                "dataset_used": ds,
                                "data": result,
                                "timestamp": int(datetime.now().timestamp())
                            }

                except Exception as e:
                    last_error = str(e)
                    continue

            return {
                "error": True,
                "message": f"Failed to fetch data from any dataset. Last error: {last_error}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch historical OHLCV: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_options_definitions(
        self,
        symbol: str,
        date: str = None,
    ) -> Dict[str, Any]:
        """
        Fetch options instrument definitions for a given underlying.
        NOTE: Requires OPRA.PILLAR access (paid dataset).

        Args:
            symbol: Underlying symbol (e.g., 'SPY', 'AAPL')
            date: Date in YYYY-MM-DD format (default: yesterday)

        Returns:
            Dict with options definitions
        """
        try:
            if not date:
                date = (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d")

            target_date = datetime.strptime(date, "%Y-%m-%d")

            # Fetch option definitions using parent symbology
            data = self.client.timeseries.get_range(
                dataset=self.OPRA_DATASET,
                schema="definition",
                symbols=[f"{symbol}.OPT"],
                stype_in="parent",
                start=target_date.strftime("%Y-%m-%d"),
                end=(target_date + timedelta(days=1)).strftime("%Y-%m-%d"),
            )

            # Process definitions
            options = []
            for row in data:
                # Handle fixed-point strike price
                strike_raw = getattr(row, 'strike_price', 0)
                strike = strike_raw / 1e9 if strike_raw > 1e6 else strike_raw

                options.append({
                    "ts_recv": str(getattr(row, 'ts_recv', 0)),
                    "raw_symbol": str(getattr(row, 'raw_symbol', '')),
                    "expiration": str(getattr(row, 'expiration', 0)),
                    "instrument_class": str(getattr(row, 'instrument_class', '')),
                    "strike_price": strike,
                    "underlying": str(getattr(row, 'underlying', '')),
                    "instrument_id": int(getattr(row, 'instrument_id', 0)),
                })

            return {
                "error": False,
                "symbol": symbol,
                "date": date,
                "dataset": self.OPRA_DATASET,
                "records": len(options),
                "data": options,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            error_msg = str(e)
            # Provide helpful error messages
            if "401" in error_msg or "auth" in error_msg.lower() or "unauthorized" in error_msg.lower():
                error_msg = (
                    "Authentication failed. Your API key may be invalid or expired. "
                    "Please verify your key at https://databento.com/portal/api-keys. "
                    f"Original error: {error_msg}"
                )
            elif "403" in error_msg or "forbidden" in error_msg.lower():
                error_msg = (
                    f"Access denied. Your API key may not have access to {self.OPRA_DATASET} dataset. "
                    "OPRA options data requires a paid subscription. "
                    f"Original error: {error_msg}"
                )
            elif "404" in error_msg or "not found" in error_msg.lower():
                error_msg = (
                    f"Symbol '{symbol}' not found in {self.OPRA_DATASET} dataset. "
                    "Try a different symbol like 'SPY' or 'AAPL'. "
                    f"Original error: {error_msg}"
                )

            return {
                "error": True,
                "message": f"Failed to fetch options definitions: {error_msg}",
                "symbol": symbol,
                "date": date,
                "timestamp": int(datetime.now().timestamp())
            }

    def resolve_symbols(
        self,
        symbols: List[str],
        dataset: str = None,
        stype_in: str = "raw_symbol",
        stype_out: str = "instrument_id",
        start_date: str = None,
        end_date: str = None,
    ) -> Dict[str, Any]:
        """
        Resolve/validate symbols using Databento symbology API.
        Useful for searching and validating symbols before queries.

        Args:
            symbols: List of symbols to resolve
            dataset: Dataset to resolve against (default: XNAS.ITCH)
            stype_in: Input symbol type (raw_symbol, parent, smart, figi, isin, etc.)
            stype_out: Output symbol type
            start_date: Start date for resolution
            end_date: End date for resolution

        Returns:
            Dict with resolved symbol mappings
        """
        try:
            if not start_date:
                start_date = (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            ds = dataset or self.XNAS_DATASET

            result = self.client.symbology.resolve(
                dataset=ds,
                symbols=symbols,
                stype_in=stype_in,
                stype_out=stype_out,
                start_date=start_date,
                end_date=end_date,
            )

            # Process the symbology result
            mappings = []
            if hasattr(result, 'mappings'):
                for mapping in result.mappings:
                    mappings.append({
                        "raw_symbol": str(getattr(mapping, 'raw_symbol', '')),
                        "instrument_id": str(getattr(mapping, 'instrument_id', '')),
                        "start_date": str(getattr(mapping, 'start_date', '')),
                        "end_date": str(getattr(mapping, 'end_date', '')),
                    })

            return {
                "error": False,
                "symbols": symbols,
                "dataset": ds,
                "stype_in": stype_in,
                "stype_out": stype_out,
                "mappings": mappings,
                "count": len(mappings),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to resolve symbols: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def list_datasets(self) -> Dict[str, Any]:
        """
        List all available datasets from Databento.

        Returns:
            Dict with available datasets and their info
        """
        try:
            datasets = self.client.metadata.list_datasets()

            dataset_list = []
            for ds in datasets:
                dataset_list.append({
                    "id": str(ds),
                    "name": str(ds),
                })

            return {
                "error": False,
                "datasets": dataset_list,
                "count": len(dataset_list),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to list datasets: {str(e)}",
                "timestamp": int(datetime.now().timestamp())
            }

    def get_dataset_info(self, dataset: str) -> Dict[str, Any]:
        """
        Get detailed info about a dataset including date range.

        Args:
            dataset: Dataset identifier (e.g., 'XNAS.ITCH')

        Returns:
            Dict with dataset information
        """
        try:
            # Get date range
            date_range = self.client.metadata.get_dataset_range(dataset=dataset)

            # Get available schemas
            schemas = self.client.metadata.list_schemas(dataset=dataset)
            schema_list = [str(s) for s in schemas] if schemas else []

            return {
                "error": False,
                "dataset": dataset,
                "start_date": str(date_range.start) if hasattr(date_range, 'start') else None,
                "end_date": str(date_range.end) if hasattr(date_range, 'end') else None,
                "schemas": schema_list,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get dataset info: {str(e)}",
                "dataset": dataset,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_cost_estimate(
        self,
        dataset: str,
        symbols: List[str],
        schema: str,
        start_date: str,
        end_date: str,
    ) -> Dict[str, Any]:
        """
        Estimate the cost of a query before executing it.

        Args:
            dataset: Dataset identifier
            symbols: List of symbols
            schema: Data schema (e.g., 'ohlcv-1d', 'trades', 'mbp-1')
            start_date: Start date (YYYY-MM-DD)
            end_date: End date (YYYY-MM-DD)

        Returns:
            Dict with cost estimate and record count
        """
        try:
            # Get record count
            record_count = self.client.metadata.get_record_count(
                dataset=dataset,
                symbols=symbols,
                schema=schema,
                start=start_date,
                end=end_date,
            )

            # Get cost estimate
            cost = self.client.metadata.get_cost(
                dataset=dataset,
                symbols=symbols,
                schema=schema,
                start=start_date,
                end=end_date,
            )

            return {
                "error": False,
                "dataset": dataset,
                "symbols": symbols,
                "schema": schema,
                "start_date": start_date,
                "end_date": end_date,
                "record_count": int(record_count) if record_count else 0,
                "cost_usd": float(cost) if cost else 0.0,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get cost estimate: {str(e)}",
                "dataset": dataset,
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def list_schemas(self, dataset: str = None) -> Dict[str, Any]:
        """
        List available schemas for a dataset.

        Args:
            dataset: Dataset identifier (optional, defaults to XNAS.ITCH)

        Returns:
            Dict with available schemas
        """
        try:
            ds = dataset or self.XNAS_DATASET
            schemas = self.client.metadata.list_schemas(dataset=ds)

            schema_list = []
            for schema in schemas:
                schema_list.append({
                    "id": str(schema),
                    "name": str(schema),
                })

            return {
                "error": False,
                "dataset": ds,
                "schemas": schema_list,
                "count": len(schema_list),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to list schemas: {str(e)}",
                "dataset": dataset,
                "timestamp": int(datetime.now().timestamp())
            }

    # ========================================================================
    # Reference Data API (security_master, corporate_actions, adjustment_factors)
    # ========================================================================

    def get_security_master(
        self,
        symbols: List[str],
        dataset: str = None,
    ) -> Dict[str, Any]:
        """
        Get security master data (instrument metadata, fundamental info).
        Uses Reference API.

        Args:
            symbols: List of symbols to get metadata for
            dataset: Dataset identifier

        Returns:
            Dict with security master data
        """
        try:
            ds = dataset or self.XNAS_DATASET

            # Initialize Reference client
            ref_client = db.Reference(key=self.api_key)

            # Get security master data
            result = ref_client.security_master.get(
                dataset=ds,
                symbols=symbols,
            )

            # Process result
            securities = []
            if hasattr(result, '__iter__'):
                for sec in result:
                    securities.append({
                        "raw_symbol": str(getattr(sec, 'raw_symbol', '')),
                        "instrument_id": str(getattr(sec, 'instrument_id', '')),
                        "security_type": str(getattr(sec, 'security_type', '')),
                        "exchange": str(getattr(sec, 'exchange', '')),
                        "currency": str(getattr(sec, 'currency', '')),
                        "name": str(getattr(sec, 'name', '')),
                        "description": str(getattr(sec, 'description', '')),
                        "sector": str(getattr(sec, 'sector', '')),
                        "industry": str(getattr(sec, 'industry', '')),
                        "market_cap": float(getattr(sec, 'market_cap', 0)) if getattr(sec, 'market_cap', None) else None,
                        "shares_outstanding": int(getattr(sec, 'shares_outstanding', 0)) if getattr(sec, 'shares_outstanding', None) else None,
                    })

            return {
                "error": False,
                "symbols": symbols,
                "dataset": ds,
                "securities": securities,
                "count": len(securities),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get security master: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_corporate_actions(
        self,
        symbols: List[str],
        start_date: str = None,
        end_date: str = None,
        action_type: str = None,
    ) -> Dict[str, Any]:
        """
        Get corporate actions (dividends, splits, spinoffs, etc.).
        Uses Reference API.

        Args:
            symbols: List of symbols
            start_date: Start date (YYYY-MM-DD)
            end_date: End date (YYYY-MM-DD)
            action_type: Filter by type (dividend, split, spinoff, etc.)

        Returns:
            Dict with corporate actions data
        """
        try:
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            # Initialize Reference client
            ref_client = db.Reference(key=self.api_key)

            # Build request parameters
            params = {
                "symbols": symbols,
                "start_date": start_date,
                "end_date": end_date,
            }
            if action_type:
                params["action_type"] = action_type

            # Get corporate actions
            result = ref_client.corporate_actions.get(**params)

            # Process result
            actions = []
            if hasattr(result, '__iter__'):
                for action in result:
                    actions.append({
                        "raw_symbol": str(getattr(action, 'raw_symbol', '')),
                        "action_type": str(getattr(action, 'action_type', '')),
                        "ex_date": str(getattr(action, 'ex_date', '')),
                        "record_date": str(getattr(action, 'record_date', '')),
                        "payment_date": str(getattr(action, 'payment_date', '')),
                        "amount": float(getattr(action, 'amount', 0)) if getattr(action, 'amount', None) else None,
                        "ratio": str(getattr(action, 'ratio', '')) if getattr(action, 'ratio', None) else None,
                        "currency": str(getattr(action, 'currency', 'USD')),
                        "description": str(getattr(action, 'description', '')),
                    })

            return {
                "error": False,
                "symbols": symbols,
                "start_date": start_date,
                "end_date": end_date,
                "actions": actions,
                "count": len(actions),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get corporate actions: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_adjustment_factors(
        self,
        symbols: List[str],
        start_date: str = None,
        end_date: str = None,
    ) -> Dict[str, Any]:
        """
        Get adjustment factors for splits, dividends, etc.
        Used to adjust historical prices.

        Args:
            symbols: List of symbols
            start_date: Start date (YYYY-MM-DD)
            end_date: End date (YYYY-MM-DD)

        Returns:
            Dict with adjustment factors
        """
        try:
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365 * 5)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            # Initialize Reference client
            ref_client = db.Reference(key=self.api_key)

            # Get adjustment factors
            result = ref_client.adjustment_factors.get(
                symbols=symbols,
                start_date=start_date,
                end_date=end_date,
            )

            # Process result
            factors = []
            if hasattr(result, '__iter__'):
                for factor in result:
                    factors.append({
                        "raw_symbol": str(getattr(factor, 'raw_symbol', '')),
                        "date": str(getattr(factor, 'date', '')),
                        "split_factor": float(getattr(factor, 'split_factor', 1.0)),
                        "dividend_factor": float(getattr(factor, 'dividend_factor', 1.0)),
                        "cumulative_factor": float(getattr(factor, 'cumulative_factor', 1.0)),
                    })

            return {
                "error": False,
                "symbols": symbols,
                "start_date": start_date,
                "end_date": end_date,
                "factors": factors,
                "count": len(factors),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get adjustment factors: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    # ========================================================================
    # Order Book Data API (MBO, MBP_1, MBP_10 schemas)
    # ========================================================================

    def get_order_book(
        self,
        symbols: List[str],
        dataset: str = None,
        schema: str = "mbp-1",
        start_time: str = None,
        duration_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Get order book data (market-by-price or market-by-order).

        Schemas:
        - mbp-1: Top of book (best bid/ask)
        - mbp-10: 10 price levels
        - mbo: Full market-by-order (every order event)

        Args:
            symbols: List of symbols
            dataset: Dataset identifier (default: XNAS.ITCH)
            schema: Order book schema (mbp-1, mbp-10, mbo)
            start_time: Start datetime (ISO format, default: market open yesterday)
            duration_seconds: Duration in seconds (default: 60)

        Returns:
            Dict with order book data
        """
        try:
            ds = dataset or self.XNAS_DATASET

            # Validate schema
            valid_schemas = ['mbp-1', 'mbp-10', 'mbo', 'tbbo']
            if schema not in valid_schemas:
                return {
                    "error": True,
                    "message": f"Invalid schema '{schema}'. Valid options: {', '.join(valid_schemas)}",
                    "timestamp": int(datetime.now().timestamp())
                }

            # Default start time: yesterday market open
            if not start_time:
                yesterday = datetime.now() - timedelta(days=1)
                market_open = yesterday.replace(hour=9, minute=30, second=0, microsecond=0)
                start_time = market_open.strftime("%Y-%m-%dT%H:%M:%S")

            # Parse start time and calculate end time
            start_dt = datetime.fromisoformat(start_time.replace('Z', ''))
            end_dt = start_dt + timedelta(seconds=duration_seconds)

            # Fetch order book data
            data = self.client.timeseries.get_range(
                dataset=ds,
                schema=schema,
                symbols=symbols,
                stype_in="raw_symbol",
                start=start_dt.strftime("%Y-%m-%dT%H:%M:%S"),
                end=end_dt.strftime("%Y-%m-%dT%H:%M:%S"),
            )

            # Process order book data based on schema
            records = []
            for row in data:
                try:
                    record = {
                        "ts_event": str(getattr(row, 'ts_event', 0)),
                        "symbol": str(getattr(row, 'symbol', '')),
                        "action": str(getattr(row, 'action', '')),
                        "side": str(getattr(row, 'side', '')),
                    }

                    # Handle levels for MBP schemas
                    if hasattr(row, 'levels'):
                        levels = []
                        for level in getattr(row, 'levels', []):
                            bid_px = getattr(level, 'bid_px', 0)
                            ask_px = getattr(level, 'ask_px', 0)
                            bid_sz = getattr(level, 'bid_sz', 0)
                            ask_sz = getattr(level, 'ask_sz', 0)
                            bid_ct = getattr(level, 'bid_ct', 0)
                            ask_ct = getattr(level, 'ask_ct', 0)

                            # Handle fixed-point prices
                            levels.append({
                                "bid_px": bid_px / 1e9 if bid_px > 1e6 else bid_px,
                                "ask_px": ask_px / 1e9 if ask_px > 1e6 else ask_px,
                                "bid_sz": int(bid_sz),
                                "ask_sz": int(ask_sz),
                                "bid_ct": int(bid_ct),
                                "ask_ct": int(ask_ct),
                            })
                        record["levels"] = levels

                    # Handle MBO specific fields
                    if schema == 'mbo':
                        record["order_id"] = str(getattr(row, 'order_id', ''))
                        price = getattr(row, 'price', 0)
                        record["price"] = price / 1e9 if price > 1e6 else price
                        record["size"] = int(getattr(row, 'size', 0))

                    records.append(record)
                except Exception:
                    continue

            return {
                "error": False,
                "symbols": symbols,
                "dataset": ds,
                "schema": schema,
                "start_time": start_time,
                "duration_seconds": duration_seconds,
                "records": records,
                "count": len(records),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get order book: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_order_book_snapshot(
        self,
        symbols: List[str],
        dataset: str = None,
        levels: int = 10,
        snapshot_time: str = None,
    ) -> Dict[str, Any]:
        """
        Get order book snapshot at a specific point in time.
        Uses MBP-10 schema and aggregates to show book state.

        Args:
            symbols: List of symbols
            dataset: Dataset identifier
            levels: Number of price levels (1-10)
            snapshot_time: Datetime for snapshot (ISO format)

        Returns:
            Dict with aggregated order book snapshot
        """
        try:
            ds = dataset or self.XNAS_DATASET
            levels = min(max(levels, 1), 10)
            schema = f"mbp-{levels}" if levels > 1 else "mbp-1"

            # Default to yesterday close
            if not snapshot_time:
                yesterday = datetime.now() - timedelta(days=1)
                snapshot_time = yesterday.replace(hour=15, minute=59, second=0).strftime("%Y-%m-%dT%H:%M:%S")

            # Fetch a short window and take the last record
            result = self.get_order_book(
                symbols=symbols,
                dataset=ds,
                schema=schema,
                start_time=snapshot_time,
                duration_seconds=60,
            )

            if result.get("error"):
                return result

            # Aggregate into snapshot per symbol
            snapshots = {}
            for rec in result.get("records", []):
                sym = rec.get("symbol", "")
                if sym and rec.get("levels"):
                    snapshots[sym] = {
                        "symbol": sym,
                        "ts_event": rec.get("ts_event"),
                        "levels": rec.get("levels", [])[:levels],
                    }

            return {
                "error": False,
                "symbols": symbols,
                "dataset": ds,
                "schema": schema,
                "levels": levels,
                "snapshot_time": snapshot_time,
                "snapshots": list(snapshots.values()),
                "count": len(snapshots),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get order book snapshot: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_options_quotes(
        self,
        symbol: str,
        date: str = None,
        duration_minutes: int = 5,
    ) -> Dict[str, Any]:
        """
        Fetch options quotes (bid/ask) for volatility surface.
        NOTE: Requires OPRA.PILLAR access (paid dataset).

        Args:
            symbol: Underlying symbol (e.g., 'SPY')
            date: Date in YYYY-MM-DD format
            duration_minutes: Duration to fetch (default 5 min to minimize cost)

        Returns:
            Dict with options quotes including mid prices
        """
        try:
            if not date:
                date = (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d")

            target_date = datetime.strptime(date, "%Y-%m-%d")

            # First get definitions
            definitions = self.get_options_definitions(symbol, date)
            if definitions.get("error"):
                return definitions

            option_symbols = [d["raw_symbol"] for d in definitions.get("data", [])][:100]

            if not option_symbols:
                return {
                    "error": True,
                    "message": f"No options found for {symbol}",
                    "timestamp": int(datetime.now().timestamp())
                }

            # Fetch quotes using cmbp-1 schema (consolidated market-by-price)
            market_open = target_date.replace(hour=9, minute=30)
            market_end = market_open + timedelta(minutes=duration_minutes)

            data = self.client.timeseries.get_range(
                dataset=self.OPRA_DATASET,
                schema="cmbp-1",
                symbols=option_symbols,
                stype_in="raw_symbol",
                start=market_open.strftime("%Y-%m-%dT%H:%M:%S"),
                end=market_end.strftime("%Y-%m-%dT%H:%M:%S"),
            )

            # Process quotes
            quotes = {}
            for row in data:
                try:
                    sym = str(getattr(row, 'symbol', ''))
                    if not sym:
                        continue

                    levels = getattr(row, 'levels', [])
                    if not levels:
                        continue

                    bid_px = getattr(levels[0], 'bid_px', 0)
                    ask_px = getattr(levels[0], 'ask_px', 0)

                    # Skip invalid prices
                    if bid_px > (1 << 62) or ask_px > (1 << 62):
                        continue

                    # Handle fixed-point prices
                    bid = bid_px / 1e9 if bid_px > 1e6 else bid_px
                    ask = ask_px / 1e9 if ask_px > 1e6 else ask_px
                    mid = (bid + ask) / 2

                    if sym not in quotes:
                        quotes[sym] = {"bid": [], "ask": [], "mid": []}

                    quotes[sym]["bid"].append(bid)
                    quotes[sym]["ask"].append(ask)
                    quotes[sym]["mid"].append(mid)

                except Exception:
                    continue

            # Compute average prices
            result = []
            definitions_map = {d["raw_symbol"]: d for d in definitions.get("data", [])}

            for sym, data in quotes.items():
                avg_mid = sum(data["mid"]) / len(data["mid"]) if data["mid"] else 0
                avg_bid = sum(data["bid"]) / len(data["bid"]) if data["bid"] else 0
                avg_ask = sum(data["ask"]) / len(data["ask"]) if data["ask"] else 0

                quote = {
                    "symbol": sym,
                    "mid_price": avg_mid,
                    "bid": avg_bid,
                    "ask": avg_ask,
                    "quote_count": len(data["mid"]),
                }

                # Merge with definition data
                if sym in definitions_map:
                    quote.update(definitions_map[sym])

                result.append(quote)

            return {
                "error": False,
                "symbol": symbol,
                "date": date,
                "dataset": self.OPRA_DATASET,
                "records": len(result),
                "data": result,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            error_msg = str(e)
            if "401" in error_msg or "auth" in error_msg.lower():
                error_msg = f"Authentication failed. Please verify your API key. Original: {error_msg}"
            elif "403" in error_msg:
                error_msg = f"Access denied. OPRA options data requires paid subscription. Original: {error_msg}"

            return {
                "error": True,
                "message": f"Failed to fetch options quotes: {error_msg}",
                "symbol": symbol,
                "timestamp": int(datetime.now().timestamp())
            }

    # ========================================================================
    # Futures API (GLBX.MDP3 dataset, SMART symbology)
    # ========================================================================

    # Common futures contracts reference
    FUTURES_CONTRACTS = {
        # Equity Index Futures
        "ES": {"name": "E-mini S&P 500", "exchange": "CME", "tick_size": 0.25, "multiplier": 50},
        "NQ": {"name": "E-mini NASDAQ-100", "exchange": "CME", "tick_size": 0.25, "multiplier": 20},
        "YM": {"name": "E-mini Dow", "exchange": "CBOT", "tick_size": 1.0, "multiplier": 5},
        "RTY": {"name": "E-mini Russell 2000", "exchange": "CME", "tick_size": 0.1, "multiplier": 50},
        "MES": {"name": "Micro E-mini S&P 500", "exchange": "CME", "tick_size": 0.25, "multiplier": 5},
        "MNQ": {"name": "Micro E-mini NASDAQ", "exchange": "CME", "tick_size": 0.25, "multiplier": 2},
        # Energy Futures
        "CL": {"name": "Crude Oil", "exchange": "NYMEX", "tick_size": 0.01, "multiplier": 1000},
        "NG": {"name": "Natural Gas", "exchange": "NYMEX", "tick_size": 0.001, "multiplier": 10000},
        "RB": {"name": "RBOB Gasoline", "exchange": "NYMEX", "tick_size": 0.0001, "multiplier": 42000},
        "HO": {"name": "Heating Oil", "exchange": "NYMEX", "tick_size": 0.0001, "multiplier": 42000},
        # Metals Futures
        "GC": {"name": "Gold", "exchange": "COMEX", "tick_size": 0.1, "multiplier": 100},
        "SI": {"name": "Silver", "exchange": "COMEX", "tick_size": 0.005, "multiplier": 5000},
        "HG": {"name": "Copper", "exchange": "COMEX", "tick_size": 0.0005, "multiplier": 25000},
        "PL": {"name": "Platinum", "exchange": "NYMEX", "tick_size": 0.1, "multiplier": 50},
        # Agricultural Futures
        "ZC": {"name": "Corn", "exchange": "CBOT", "tick_size": 0.25, "multiplier": 50},
        "ZS": {"name": "Soybeans", "exchange": "CBOT", "tick_size": 0.25, "multiplier": 50},
        "ZW": {"name": "Wheat", "exchange": "CBOT", "tick_size": 0.25, "multiplier": 50},
        # Treasury Futures
        "ZN": {"name": "10-Year T-Note", "exchange": "CBOT", "tick_size": 0.015625, "multiplier": 1000},
        "ZB": {"name": "30-Year T-Bond", "exchange": "CBOT", "tick_size": 0.03125, "multiplier": 1000},
        "ZF": {"name": "5-Year T-Note", "exchange": "CBOT", "tick_size": 0.0078125, "multiplier": 1000},
        # Currency Futures
        "6E": {"name": "Euro FX", "exchange": "CME", "tick_size": 0.00005, "multiplier": 125000},
        "6J": {"name": "Japanese Yen", "exchange": "CME", "tick_size": 0.0000005, "multiplier": 12500000},
        "6B": {"name": "British Pound", "exchange": "CME", "tick_size": 0.0001, "multiplier": 62500},
    }

    def list_futures_contracts(self) -> Dict[str, Any]:
        """
        List available futures contracts with their specifications.

        Returns:
            Dict with futures contract information
        """
        contracts = []
        for symbol, info in self.FUTURES_CONTRACTS.items():
            contracts.append({
                "symbol": symbol,
                "smart_symbol": f"{symbol}.c.0",  # Front month continuous
                **info
            })

        return {
            "error": False,
            "contracts": contracts,
            "count": len(contracts),
            "dataset": self.GLBX_DATASET,
            "note": "Use SMART symbology (e.g., ES.c.0) for continuous contracts",
            "timestamp": int(datetime.now().timestamp())
        }

    def get_futures_data(
        self,
        symbols: List[str],
        days: int = 30,
        schema: str = "ohlcv-1d",
        stype_in: str = "smart",
    ) -> Dict[str, Any]:
        """
        Fetch historical futures data using SMART symbology.

        SMART symbology examples:
        - ES.c.0 = Front month E-mini S&P 500
        - ES.c.1 = 2nd month E-mini S&P 500
        - CL.c.0 = Front month Crude Oil

        Args:
            symbols: List of SMART symbols (e.g., ['ES.c.0', 'NQ.c.0'])
            days: Number of days of history
            schema: Data schema (ohlcv-1d, ohlcv-1h, trades, mbp-1, etc.)
            stype_in: Symbol type ('smart' for continuous, 'raw_symbol' for specific contract)

        Returns:
            Dict with futures OHLCV data
        """
        try:
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            # Limit days for high-frequency schemas
            if schema == 'ohlcv-1s' and days > 3:
                start_date = end_date - timedelta(days=3)
            elif schema == 'ohlcv-1m' and days > 14:
                start_date = end_date - timedelta(days=14)
            elif schema in ['trades', 'mbp-1', 'mbp-10'] and days > 1:
                start_date = end_date - timedelta(days=1)

            data = self.client.timeseries.get_range(
                dataset=self.GLBX_DATASET,
                symbols=symbols,
                schema=schema,
                stype_in=stype_in,
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
            )

            # Process data based on schema
            if PANDAS_AVAILABLE:
                df = data.to_df()
                if df.empty:
                    return {
                        "error": True,
                        "message": f"No data found for symbols: {symbols}",
                        "symbols": symbols,
                        "timestamp": int(datetime.now().timestamp())
                    }

                result = {}
                for sym in symbols:
                    if 'symbol' in df.columns:
                        sym_df = df[df['symbol'] == sym]
                    else:
                        sym_df = df

                    records = []
                    for _, row in sym_df.iterrows():
                        record = {
                            "date": str(row.name) if hasattr(row, 'name') else str(row.get('ts_event', '')),
                        }

                        # OHLCV fields (prices in fixed-point)
                        for field in ['open', 'high', 'low', 'close']:
                            val = row.get(field, 0)
                            record[field] = float(val) / 1e9 if val > 1e6 else float(val)

                        if 'volume' in row:
                            record['volume'] = int(row.get('volume', 0))

                        records.append(record)

                    if records:
                        result[sym] = records

                return {
                    "error": False,
                    "symbols": symbols,
                    "days": days,
                    "schema": schema,
                    "stype_in": stype_in,
                    "dataset": self.GLBX_DATASET,
                    "records": sum(len(v) for v in result.values()),
                    "data": result,
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                # Fallback without pandas
                result = {sym: [] for sym in symbols}
                for row in data:
                    sym = getattr(row, 'symbol', symbols[0] if symbols else 'UNKNOWN')
                    record = {
                        "ts_event": str(getattr(row, 'ts_event', 0)),
                    }
                    for field in ['open', 'high', 'low', 'close']:
                        val = getattr(row, field, 0)
                        record[field] = val / 1e9 if val > 1e6 else val
                    record["volume"] = getattr(row, 'volume', 0)

                    if sym in result:
                        result[sym].append(record)

                return {
                    "error": False,
                    "symbols": symbols,
                    "schema": schema,
                    "dataset": self.GLBX_DATASET,
                    "data": result,
                    "timestamp": int(datetime.now().timestamp())
                }

        except Exception as e:
            error_msg = str(e)
            if "403" in error_msg or "forbidden" in error_msg.lower():
                error_msg = (
                    f"Access denied. Your API key may not have access to {self.GLBX_DATASET} dataset. "
                    "CME futures data requires a paid subscription. "
                    f"Original error: {error_msg}"
                )
            return {
                "error": True,
                "message": f"Failed to fetch futures data: {error_msg}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_futures_definitions(
        self,
        symbols: List[str],
        date: str = None,
    ) -> Dict[str, Any]:
        """
        Get futures contract definitions (expiration, underlying, etc.)

        Args:
            symbols: List of SMART symbols or raw symbols
            date: Date for definitions (default: yesterday)

        Returns:
            Dict with futures instrument definitions
        """
        try:
            if not date:
                date = (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d")

            target_date = datetime.strptime(date, "%Y-%m-%d")

            data = self.client.timeseries.get_range(
                dataset=self.GLBX_DATASET,
                schema="definition",
                symbols=symbols,
                stype_in="smart",
                start=target_date.strftime("%Y-%m-%d"),
                end=(target_date + timedelta(days=1)).strftime("%Y-%m-%d"),
            )

            definitions = []
            for row in data:
                expiration_raw = getattr(row, 'expiration', 0)
                # Convert nanoseconds to date
                exp_date = None
                if expiration_raw and expiration_raw > 0:
                    exp_date = datetime.fromtimestamp(expiration_raw / 1e9).strftime("%Y-%m-%d")

                definitions.append({
                    "raw_symbol": str(getattr(row, 'raw_symbol', '')),
                    "instrument_id": int(getattr(row, 'instrument_id', 0)),
                    "exchange": str(getattr(row, 'exchange', '')),
                    "underlying": str(getattr(row, 'underlying', '')),
                    "expiration": exp_date,
                    "contract_multiplier": float(getattr(row, 'contract_multiplier', 1)),
                    "min_price_increment": float(getattr(row, 'min_price_increment', 0)) / 1e9,
                    "currency": str(getattr(row, 'currency', 'USD')),
                })

            return {
                "error": False,
                "symbols": symbols,
                "date": date,
                "dataset": self.GLBX_DATASET,
                "definitions": definitions,
                "count": len(definitions),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get futures definitions: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_futures_term_structure(
        self,
        root_symbol: str,
        num_contracts: int = 6,
    ) -> Dict[str, Any]:
        """
        Get term structure (curve) for a futures contract.
        Fetches prices for multiple expiration months.

        Args:
            root_symbol: Root symbol (e.g., 'ES', 'CL')
            num_contracts: Number of contract months to fetch (default: 6)

        Returns:
            Dict with term structure data
        """
        try:
            # Build SMART symbols for multiple contract months
            symbols = [f"{root_symbol}.c.{i}" for i in range(num_contracts)]

            # Fetch recent OHLCV
            data = self.get_futures_data(symbols, days=1, schema="ohlcv-1d")

            if data.get("error"):
                return data

            # Build term structure
            term_structure = []
            for i, sym in enumerate(symbols):
                if sym in data.get("data", {}):
                    records = data["data"][sym]
                    if records:
                        latest = records[-1]
                        term_structure.append({
                            "contract_month": i,
                            "smart_symbol": sym,
                            "close": latest.get("close", 0),
                            "volume": latest.get("volume", 0),
                        })

            return {
                "error": False,
                "root_symbol": root_symbol,
                "term_structure": term_structure,
                "count": len(term_structure),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to get term structure: {str(e)}",
                "root_symbol": root_symbol,
                "timestamp": int(datetime.now().timestamp())
            }

    # ========================================================================
    # Batch Download API (submit_job, list_jobs, list_files, download)
    # ========================================================================

    def submit_batch_job(
        self,
        dataset: str,
        symbols: List[str],
        schema: str,
        start_date: str,
        end_date: str,
        encoding: str = "dbn",
        compression: str = "zstd",
        stype_in: str = "raw_symbol",
        split_duration: str = "day",
        delivery: str = "download",
    ) -> Dict[str, Any]:
        """
        Submit a batch download job for large data requests.
        Jobs are processed asynchronously and can be downloaded when done.

        Args:
            dataset: Dataset identifier (e.g., 'XNAS.ITCH')
            symbols: List of symbols
            schema: Data schema (e.g., 'trades', 'ohlcv-1d')
            start_date: Start date (YYYY-MM-DD)
            end_date: End date (YYYY-MM-DD)
            encoding: Output encoding ('dbn', 'csv', 'json')
            compression: Compression type ('zstd', 'none')
            stype_in: Symbol type input
            split_duration: How to split output files ('day', 'week', 'month', 'none')
            delivery: Delivery method ('download', 's3', 'disk')

        Returns:
            Dict with job_id and job details
        """
        try:
            result = self.client.batch.submit_job(
                dataset=dataset,
                symbols=symbols,
                schema=schema,
                start=start_date,
                end=end_date,
                encoding=encoding,
                compression=compression,
                stype_in=stype_in,
                split_duration=split_duration,
                delivery=delivery,
            )

            return {
                "error": False,
                "job_id": result.get("job_id", ""),
                "state": result.get("state", ""),
                "dataset": dataset,
                "symbols": symbols,
                "schema": schema,
                "start_date": start_date,
                "end_date": end_date,
                "encoding": encoding,
                "compression": compression,
                "cost": result.get("cost", 0),
                "record_count": result.get("record_count", 0),
                "ts_received": result.get("ts_received", ""),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to submit batch job: {str(e)}",
                "dataset": dataset,
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def list_batch_jobs(
        self,
        states: str = "queued,processing,done",
        since_days: int = 7,
    ) -> Dict[str, Any]:
        """
        List batch download jobs.

        Args:
            states: Comma-separated job states to filter (queued, processing, done, expired)
            since_days: Only show jobs from the last N days

        Returns:
            Dict with list of jobs
        """
        try:
            since = (datetime.now() - timedelta(days=since_days)).strftime("%Y-%m-%d")

            jobs = self.client.batch.list_jobs(
                states=states,
                since=since,
            )

            job_list = []
            for job in jobs:
                job_list.append({
                    "job_id": job.get("job_id", ""),
                    "state": job.get("state", ""),
                    "dataset": job.get("dataset", ""),
                    "schema": job.get("schema", ""),
                    "symbols": job.get("symbols", []),
                    "start": job.get("start", ""),
                    "end": job.get("end", ""),
                    "encoding": job.get("encoding", ""),
                    "compression": job.get("compression", ""),
                    "cost": job.get("cost", 0),
                    "record_count": job.get("record_count", 0),
                    "ts_received": job.get("ts_received", ""),
                    "ts_process_start": job.get("ts_process_start", ""),
                    "ts_process_done": job.get("ts_process_done", ""),
                    "size_bytes": job.get("size_bytes", 0),
                })

            return {
                "error": False,
                "jobs": job_list,
                "count": len(job_list),
                "states_filter": states,
                "since": since,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to list batch jobs: {str(e)}",
                "timestamp": int(datetime.now().timestamp())
            }

    def list_batch_files(
        self,
        job_id: str,
    ) -> Dict[str, Any]:
        """
        List files available for download from a completed batch job.

        Args:
            job_id: The batch job ID

        Returns:
            Dict with list of downloadable files
        """
        try:
            files = self.client.batch.list_files(job_id=job_id)

            file_list = []
            for f in files:
                file_list.append({
                    "filename": f.get("filename", ""),
                    "size_bytes": f.get("size_bytes", 0),
                    "hash": f.get("hash", ""),
                    "urls": f.get("urls", {}),
                })

            return {
                "error": False,
                "job_id": job_id,
                "files": file_list,
                "count": len(file_list),
                "total_size_bytes": sum(f.get("size_bytes", 0) for f in files),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to list batch files: {str(e)}",
                "job_id": job_id,
                "timestamp": int(datetime.now().timestamp())
            }

    def download_batch_job(
        self,
        job_id: str,
        output_dir: str = None,
        filename: str = None,
    ) -> Dict[str, Any]:
        """
        Download files from a completed batch job.

        Args:
            job_id: The batch job ID
            output_dir: Directory to save files (default: temp dir)
            filename: Specific filename to download (optional)

        Returns:
            Dict with downloaded file paths
        """
        try:
            import tempfile

            if not output_dir:
                output_dir = tempfile.gettempdir()

            downloaded_paths = self.client.batch.download(
                job_id=job_id,
                output_dir=output_dir,
                filename_to_download=filename,
                keep_zip=False,
            )

            paths = [str(p) for p in downloaded_paths]

            return {
                "error": False,
                "job_id": job_id,
                "output_dir": output_dir,
                "downloaded_files": paths,
                "count": len(paths),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to download batch job: {str(e)}",
                "job_id": job_id,
                "timestamp": int(datetime.now().timestamp())
            }

    # ==========================================================================
    # Additional Schemas API (TRADES, IMBALANCE, STATISTICS, STATUS)
    # ==========================================================================

    def get_trades_data(
        self,
        symbols: List[str],
        dataset: str = None,
        days: int = 1,
        limit: int = 1000,
    ) -> Dict[str, Any]:
        """
        Fetch trades data for symbols.

        Args:
            symbols: List of symbols
            dataset: Dataset (default: XNAS.ITCH)
            days: Number of days of history
            limit: Maximum number of records per symbol

        Returns:
            Dict with trades data
        """
        try:
            dataset = dataset or self.XNAS_DATASET
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            data = self.client.timeseries.get_range(
                dataset=dataset,
                symbols=symbols,
                schema="trades",
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
                limit=limit,
            )

            trades = []
            for record in data:
                trade = {
                    "ts_event": str(record.ts_event) if hasattr(record, 'ts_event') else None,
                    "symbol": str(record.symbol) if hasattr(record, 'symbol') else None,
                    "price": float(record.price) / 1e9 if hasattr(record, 'price') else 0,
                    "size": int(record.size) if hasattr(record, 'size') else 0,
                    "action": str(record.action) if hasattr(record, 'action') else None,
                    "side": str(record.side) if hasattr(record, 'side') else None,
                    "sequence": int(record.sequence) if hasattr(record, 'sequence') else 0,
                }
                trades.append(trade)

            return {
                "error": False,
                "schema": "trades",
                "dataset": dataset,
                "symbols": symbols,
                "trades": trades,
                "count": len(trades),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch trades: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_imbalance_data(
        self,
        symbols: List[str],
        dataset: str = None,
        days: int = 1,
    ) -> Dict[str, Any]:
        """
        Fetch auction imbalance data (opening/closing imbalances).

        Args:
            symbols: List of symbols
            dataset: Dataset (default: XNAS.ITCH)
            days: Number of days of history

        Returns:
            Dict with imbalance data
        """
        try:
            dataset = dataset or self.XNAS_DATASET
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            data = self.client.timeseries.get_range(
                dataset=dataset,
                symbols=symbols,
                schema="imbalance",
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
            )

            imbalances = []
            for record in data:
                imbalance = {
                    "ts_event": str(record.ts_event) if hasattr(record, 'ts_event') else None,
                    "symbol": str(record.symbol) if hasattr(record, 'symbol') else None,
                    "ref_price": float(record.ref_price) / 1e9 if hasattr(record, 'ref_price') else 0,
                    "paired_qty": int(record.paired_qty) if hasattr(record, 'paired_qty') else 0,
                    "imbalance_qty": int(record.imbalance_qty) if hasattr(record, 'imbalance_qty') else 0,
                    "imbalance_side": str(record.imbalance_side) if hasattr(record, 'imbalance_side') else None,
                    "auction_type": str(record.auction_type) if hasattr(record, 'auction_type') else None,
                }
                imbalances.append(imbalance)

            return {
                "error": False,
                "schema": "imbalance",
                "dataset": dataset,
                "symbols": symbols,
                "imbalances": imbalances,
                "count": len(imbalances),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch imbalance data: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_statistics_data(
        self,
        symbols: List[str],
        dataset: str = None,
        days: int = 5,
    ) -> Dict[str, Any]:
        """
        Fetch exchange statistics (trading halts, circuit breakers, etc.).

        Args:
            symbols: List of symbols
            dataset: Dataset (default: XNAS.ITCH)
            days: Number of days of history

        Returns:
            Dict with statistics data
        """
        try:
            dataset = dataset or self.XNAS_DATASET
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            data = self.client.timeseries.get_range(
                dataset=dataset,
                symbols=symbols,
                schema="statistics",
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
            )

            statistics = []
            for record in data:
                stat = {
                    "ts_event": str(record.ts_event) if hasattr(record, 'ts_event') else None,
                    "symbol": str(record.symbol) if hasattr(record, 'symbol') else None,
                    "stat_type": str(record.stat_type) if hasattr(record, 'stat_type') else None,
                    "price": float(record.price) / 1e9 if hasattr(record, 'price') else 0,
                    "quantity": int(record.quantity) if hasattr(record, 'quantity') else 0,
                    "sequence": int(record.sequence) if hasattr(record, 'sequence') else 0,
                    "ts_ref": str(record.ts_ref) if hasattr(record, 'ts_ref') else None,
                }
                statistics.append(stat)

            return {
                "error": False,
                "schema": "statistics",
                "dataset": dataset,
                "symbols": symbols,
                "statistics": statistics,
                "count": len(statistics),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch statistics: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_status_data(
        self,
        symbols: List[str],
        dataset: str = None,
        days: int = 5,
    ) -> Dict[str, Any]:
        """
        Fetch instrument status (trading status, reg SHO, etc.).

        Args:
            symbols: List of symbols
            dataset: Dataset (default: XNAS.ITCH)
            days: Number of days of history

        Returns:
            Dict with status data
        """
        try:
            dataset = dataset or self.XNAS_DATASET
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            data = self.client.timeseries.get_range(
                dataset=dataset,
                symbols=symbols,
                schema="status",
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
            )

            statuses = []
            for record in data:
                status = {
                    "ts_event": str(record.ts_event) if hasattr(record, 'ts_event') else None,
                    "symbol": str(record.symbol) if hasattr(record, 'symbol') else None,
                    "action": str(record.action) if hasattr(record, 'action') else None,
                    "reason": str(record.reason) if hasattr(record, 'reason') else None,
                    "trading_event": str(record.trading_event) if hasattr(record, 'trading_event') else None,
                    "is_short_sell_restricted": bool(record.is_short_sell_restricted) if hasattr(record, 'is_short_sell_restricted') else None,
                }
                statuses.append(status)

            return {
                "error": False,
                "schema": "status",
                "dataset": dataset,
                "symbols": symbols,
                "statuses": statuses,
                "count": len(statuses),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch status data: {str(e)}",
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }

    def get_schema_data(
        self,
        symbols: List[str],
        schema: str,
        dataset: str = None,
        days: int = 1,
        limit: int = 1000,
    ) -> Dict[str, Any]:
        """
        Generic method to fetch data for any schema.

        Args:
            symbols: List of symbols
            schema: Schema type (trades, imbalance, statistics, status, etc.)
            dataset: Dataset (default: XNAS.ITCH)
            days: Number of days of history
            limit: Maximum records

        Returns:
            Dict with schema data
        """
        # Route to specific methods if available
        if schema == "trades":
            return self.get_trades_data(symbols, dataset, days, limit)
        elif schema == "imbalance":
            return self.get_imbalance_data(symbols, dataset, days)
        elif schema == "statistics":
            return self.get_statistics_data(symbols, dataset, days)
        elif schema == "status":
            return self.get_status_data(symbols, dataset, days)

        # Generic handling for other schemas
        try:
            dataset = dataset or self.XNAS_DATASET
            end_date = datetime.now()
            start_date = end_date - timedelta(days=days)

            data = self.client.timeseries.get_range(
                dataset=dataset,
                symbols=symbols,
                schema=schema,
                start=start_date.strftime("%Y-%m-%d"),
                end=end_date.strftime("%Y-%m-%d"),
                limit=limit,
            )

            records = []
            for record in data:
                rec_dict = {}
                for attr in dir(record):
                    if not attr.startswith('_'):
                        try:
                            val = getattr(record, attr)
                            if not callable(val):
                                if isinstance(val, (int, float)):
                                    rec_dict[attr] = val
                                else:
                                    rec_dict[attr] = str(val)
                        except Exception:
                            pass
                records.append(rec_dict)

            return {
                "error": False,
                "schema": schema,
                "dataset": dataset,
                "symbols": symbols,
                "records": records,
                "count": len(records),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "error": True,
                "message": f"Failed to fetch {schema} data: {str(e)}",
                "schema": schema,
                "symbols": symbols,
                "timestamp": int(datetime.now().timestamp())
            }


def compute_implied_volatility(
    option_price: float,
    spot: float,
    strike: float,
    time_to_expiry: float,
    risk_free_rate: float = 0.043,
    is_call: bool = True,
    max_iterations: int = 100,
    tolerance: float = 1e-6,
) -> Optional[float]:
    """
    Compute implied volatility using Newton-Raphson method.
    Based on Black-Scholes model.
    """
    if option_price <= 0 or time_to_expiry <= 0:
        return None

    def norm_cdf(x):
        return 0.5 * (1 + math.erf(x / math.sqrt(2)))

    def norm_pdf(x):
        return math.exp(-0.5 * x * x) / math.sqrt(2 * math.pi)

    def bs_price(sigma):
        d1 = (math.log(spot / strike) + (risk_free_rate + sigma**2 / 2) * time_to_expiry) / (sigma * math.sqrt(time_to_expiry))
        d2 = d1 - sigma * math.sqrt(time_to_expiry)

        if is_call:
            return spot * norm_cdf(d1) - strike * math.exp(-risk_free_rate * time_to_expiry) * norm_cdf(d2)
        else:
            return strike * math.exp(-risk_free_rate * time_to_expiry) * norm_cdf(-d2) - spot * norm_cdf(-d1)

    def bs_vega(sigma):
        d1 = (math.log(spot / strike) + (risk_free_rate + sigma**2 / 2) * time_to_expiry) / (sigma * math.sqrt(time_to_expiry))
        return spot * norm_pdf(d1) * math.sqrt(time_to_expiry)

    # Initial guess
    sigma = math.sqrt(2 * math.pi / time_to_expiry) * option_price / spot
    sigma = max(0.01, min(5.0, sigma))

    for _ in range(max_iterations):
        price = bs_price(sigma)
        diff = price - option_price

        if abs(diff) < tolerance:
            return sigma

        vega = bs_vega(sigma)
        if vega < 1e-10:
            break

        sigma = sigma - diff / vega
        if sigma <= 0.001 or sigma > 10:
            break

    return None


def main():
    """CLI entry point for Databento data fetching"""
    available_commands = [
        "test_connection",
        "historical_ohlcv",
        "options_definitions",
        "options_chain",
        "options_quotes",
        "resolve_symbols",
        "list_datasets",
        "get_dataset_info",
        "get_cost_estimate",
        "list_schemas",
        # Reference Data
        "security_master",
        "corporate_actions",
        "adjustment_factors",
        # Order Book
        "order_book",
        "order_book_snapshot",
        # Futures
        "list_futures",
        "futures_data",
        "futures_definitions",
        "futures_term_structure",
        # Batch Download
        "submit_batch_job",
        "list_batch_jobs",
        "list_batch_files",
        "download_batch_job",
        # Additional Schemas
        "trades",
        "imbalance",
        "statistics",
        "status",
        "schema_data",
    ]

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": True,
            "message": "Usage: python databento_provider.py <command> <args_json>",
            "available_commands": available_commands,
            "databento_available": DATABENTO_AVAILABLE,
            "databento_error": DATABENTO_ERROR,
            "timestamp": int(datetime.now().timestamp())
        }))
        sys.exit(1)

    command = sys.argv[1]

    # Parse args - support both JSON and simple args
    args = {}
    if len(sys.argv) > 2:
        try:
            args = json.loads(sys.argv[2])
        except json.JSONDecodeError:
            # Treat as simple key=value pairs
            for arg in sys.argv[2:]:
                if '=' in arg:
                    k, v = arg.split('=', 1)
                    args[k] = v

    try:
        # Check databento availability
        if not DATABENTO_AVAILABLE:
            print(json.dumps({
                "error": True,
                "message": f"databento package not installed. Error: {DATABENTO_ERROR}. Run: pip install databento",
                "timestamp": int(datetime.now().timestamp())
            }))
            sys.exit(1)

        # Get API key from args or environment
        api_key = args.get('api_key', '').strip()
        if not api_key:
            api_key = os.environ.get('DATABENTO_API_KEY', '').strip()

        if not api_key:
            print(json.dumps({
                "error": True,
                "message": "API key is required. Provide via args {'api_key': 'db-...'} or DATABENTO_API_KEY env var",
                "timestamp": int(datetime.now().timestamp())
            }))
            sys.exit(1)

        provider = DatabentoProvider(api_key)

        if command == "test_connection":
            result = provider.test_connection()

        elif command == "historical_ohlcv" or command == "historical_bars":
            symbols = args.get('symbols', ['SPY', 'QQQ'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            days = int(args.get('days', 30))
            dataset = args.get('dataset')
            schema = args.get('schema', 'ohlcv-1d')
            result = provider.get_historical_ohlcv(symbols, days, dataset, schema)

        elif command == "options_definitions" or command == "definitions":
            symbol = args.get('symbol', 'SPY')
            date = args.get('date')
            result = provider.get_options_definitions(symbol, date)

        elif command == "options_chain" or command == "options_quotes":
            symbol = args.get('symbol', 'SPY')
            date = args.get('date')
            duration = int(args.get('duration_minutes', 5))
            result = provider.get_options_quotes(symbol, date, duration)

        elif command == "resolve_symbols" or command == "symbology":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            stype_in = args.get('stype_in', 'raw_symbol')
            stype_out = args.get('stype_out', 'instrument_id')
            start_date = args.get('start_date')
            end_date = args.get('end_date')
            result = provider.resolve_symbols(symbols, dataset, stype_in, stype_out, start_date, end_date)

        elif command == "list_datasets" or command == "datasets":
            result = provider.list_datasets()

        elif command == "get_dataset_info" or command == "dataset_info":
            dataset = args.get('dataset', 'XNAS.ITCH')
            result = provider.get_dataset_info(dataset)

        elif command == "get_cost_estimate" or command == "cost":
            dataset = args.get('dataset', 'XNAS.ITCH')
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            schema = args.get('schema', 'ohlcv-1d')
            start_date = args.get('start_date', (datetime.now() - timedelta(days=30)).strftime("%Y-%m-%d"))
            end_date = args.get('end_date', datetime.now().strftime("%Y-%m-%d"))
            result = provider.get_cost_estimate(dataset, symbols, schema, start_date, end_date)

        elif command == "list_schemas" or command == "schemas":
            dataset = args.get('dataset')
            result = provider.list_schemas(dataset)

        # Reference Data commands
        elif command == "security_master":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            result = provider.get_security_master(symbols, dataset)

        elif command == "corporate_actions":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            start_date = args.get('start_date')
            end_date = args.get('end_date')
            action_type = args.get('action_type')
            result = provider.get_corporate_actions(symbols, start_date, end_date, action_type)

        elif command == "adjustment_factors":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            start_date = args.get('start_date')
            end_date = args.get('end_date')
            result = provider.get_adjustment_factors(symbols, start_date, end_date)

        # Order Book commands
        elif command == "order_book":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            schema = args.get('schema', 'mbp-1')
            start_time = args.get('start_time')
            duration = int(args.get('duration_seconds', 60))
            result = provider.get_order_book(symbols, dataset, schema, start_time, duration)

        elif command == "order_book_snapshot":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            levels = int(args.get('levels', 10))
            snapshot_time = args.get('snapshot_time')
            result = provider.get_order_book_snapshot(symbols, dataset, levels, snapshot_time)

        # Futures commands
        elif command == "list_futures" or command == "futures_contracts":
            result = provider.list_futures_contracts()

        elif command == "futures_data":
            symbols = args.get('symbols', ['ES.c.0'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            days = int(args.get('days', 30))
            schema = args.get('schema', 'ohlcv-1d')
            stype_in = args.get('stype_in', 'smart')
            result = provider.get_futures_data(symbols, days, schema, stype_in)

        elif command == "futures_definitions":
            symbols = args.get('symbols', ['ES.c.0'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            date = args.get('date')
            result = provider.get_futures_definitions(symbols, date)

        elif command == "futures_term_structure" or command == "term_structure":
            root_symbol = args.get('root_symbol', 'ES')
            num_contracts = int(args.get('num_contracts', 6))
            result = provider.get_futures_term_structure(root_symbol, num_contracts)

        # Batch Download commands
        elif command == "submit_batch_job" or command == "batch_submit":
            dataset = args.get('dataset', 'XNAS.ITCH')
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            schema = args.get('schema', 'ohlcv-1d')
            start_date = args.get('start_date', (datetime.now() - timedelta(days=30)).strftime("%Y-%m-%d"))
            end_date = args.get('end_date', datetime.now().strftime("%Y-%m-%d"))
            encoding = args.get('encoding', 'dbn')
            compression = args.get('compression', 'zstd')
            stype_in = args.get('stype_in', 'raw_symbol')
            split_duration = args.get('split_duration', 'day')
            delivery = args.get('delivery', 'download')
            result = provider.submit_batch_job(
                dataset, symbols, schema, start_date, end_date,
                encoding, compression, stype_in, split_duration, delivery
            )

        elif command == "list_batch_jobs" or command == "batch_jobs":
            states = args.get('states', 'queued,processing,done')
            since_days = int(args.get('since_days', 7))
            result = provider.list_batch_jobs(states, since_days)

        elif command == "list_batch_files" or command == "batch_files":
            job_id = args.get('job_id', '')
            if not job_id:
                result = {
                    "error": True,
                    "message": "job_id is required",
                    "timestamp": int(datetime.now().timestamp())
                }
            else:
                result = provider.list_batch_files(job_id)

        elif command == "download_batch_job" or command == "batch_download":
            job_id = args.get('job_id', '')
            if not job_id:
                result = {
                    "error": True,
                    "message": "job_id is required",
                    "timestamp": int(datetime.now().timestamp())
                }
            else:
                output_dir = args.get('output_dir')
                filename = args.get('filename')
                result = provider.download_batch_job(job_id, output_dir, filename)

        # Additional Schemas commands
        elif command == "trades" or command == "get_trades":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            days = int(args.get('days', 1))
            limit = int(args.get('limit', 1000))
            result = provider.get_trades_data(symbols, dataset, days, limit)

        elif command == "imbalance" or command == "get_imbalance":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            days = int(args.get('days', 1))
            result = provider.get_imbalance_data(symbols, dataset, days)

        elif command == "statistics" or command == "get_statistics":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            days = int(args.get('days', 5))
            result = provider.get_statistics_data(symbols, dataset, days)

        elif command == "status" or command == "get_status":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            dataset = args.get('dataset')
            days = int(args.get('days', 5))
            result = provider.get_status_data(symbols, dataset, days)

        elif command == "schema_data" or command == "get_schema_data":
            symbols = args.get('symbols', ['SPY'])
            if isinstance(symbols, str):
                symbols = [s.strip() for s in symbols.split(',')]
            schema = args.get('schema', 'trades')
            dataset = args.get('dataset')
            days = int(args.get('days', 1))
            limit = int(args.get('limit', 1000))
            result = provider.get_schema_data(symbols, schema, dataset, days, limit)

        else:
            result = {
                "error": True,
                "message": f"Unknown command: {command}",
                "available_commands": available_commands,
                "timestamp": int(datetime.now().timestamp())
            }

        print(json.dumps(result, default=str))

    except Exception as e:
        print(json.dumps({
            "error": True,
            "message": str(e),
            "type": type(e).__name__,
            "timestamp": int(datetime.now().timestamp())
        }))
        sys.exit(1)


if __name__ == "__main__":
    main()
