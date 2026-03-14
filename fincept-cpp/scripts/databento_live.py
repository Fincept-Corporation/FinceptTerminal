"""
Databento Live Streaming Provider
==================================
Real-time market data streaming using Databento Live API.
Streams data via stdout for Tauri to capture and forward as events.

Usage:
    python databento_live.py start <args_json>
    python databento_live.py stop

Output format (JSON lines):
    {"type": "data", "schema": "trades", "record": {...}}
    {"type": "status", "message": "Connected", "connected": true}
    {"type": "error", "message": "..."}
"""

import sys
import json
import signal
import threading
from datetime import datetime
from typing import Dict, List, Any, Optional
import os

# Check for databento package
DATABENTO_AVAILABLE = False
DATABENTO_ERROR = None
try:
    import databento as db
    DATABENTO_AVAILABLE = True
except ImportError as e:
    DATABENTO_ERROR = str(e)


class LiveStreamManager:
    """
    Manages Databento Live streaming connections.
    Outputs data as JSON lines to stdout for Tauri event forwarding.
    """

    def __init__(self, api_key: str):
        if not DATABENTO_AVAILABLE:
            raise ImportError(f"databento package not available: {DATABENTO_ERROR}")

        self.api_key = api_key
        self.client: Optional[db.Live] = None
        self.running = False
        self._stop_event = threading.Event()

    def _emit(self, event_type: str, data: Dict[str, Any]):
        """Emit a JSON line event to stdout"""
        event = {
            "type": event_type,
            "timestamp": int(datetime.now().timestamp() * 1000),
            **data
        }
        # Print JSON line and flush immediately
        print(json.dumps(event, default=str), flush=True)

    def _handle_record(self, record):
        """Process incoming record and emit as event"""
        try:
            # Determine record type
            record_type = type(record).__name__

            # Build base record data
            data = {
                "record_type": record_type,
                "ts_event": str(getattr(record, 'ts_event', 0)),
                "symbol": str(getattr(record, 'symbol', '')),
            }

            # Handle different record types
            if record_type == "TradeMsg":
                price = getattr(record, 'price', 0)
                data.update({
                    "schema": "trades",
                    "price": price / 1e9 if price > 1e6 else price,
                    "size": int(getattr(record, 'size', 0)),
                    "side": str(getattr(record, 'side', '')),
                    "action": str(getattr(record, 'action', '')),
                })

            elif record_type in ["MBP1Msg", "MBP10Msg", "TBBOMsg"]:
                levels = []
                for level in getattr(record, 'levels', []):
                    bid_px = getattr(level, 'bid_px', 0)
                    ask_px = getattr(level, 'ask_px', 0)
                    levels.append({
                        "bid_px": bid_px / 1e9 if bid_px > 1e6 else bid_px,
                        "ask_px": ask_px / 1e9 if ask_px > 1e6 else ask_px,
                        "bid_sz": int(getattr(level, 'bid_sz', 0)),
                        "ask_sz": int(getattr(level, 'ask_sz', 0)),
                    })
                data.update({
                    "schema": "mbp" if "MBP" in record_type else "tbbo",
                    "levels": levels,
                })

            elif record_type == "OHLCVMsg":
                data.update({
                    "schema": "ohlcv",
                    "open": getattr(record, 'open', 0) / 1e9,
                    "high": getattr(record, 'high', 0) / 1e9,
                    "low": getattr(record, 'low', 0) / 1e9,
                    "close": getattr(record, 'close', 0) / 1e9,
                    "volume": int(getattr(record, 'volume', 0)),
                })

            elif record_type == "ImbalanceMsg":
                data.update({
                    "schema": "imbalance",
                    "ref_price": getattr(record, 'ref_price', 0) / 1e9,
                    "paired_qty": int(getattr(record, 'paired_qty', 0)),
                    "imbalance_qty": int(getattr(record, 'imbalance_qty', 0)),
                    "imbalance_side": str(getattr(record, 'imbalance_side', '')),
                })

            elif record_type == "StatMsg":
                data.update({
                    "schema": "statistics",
                    "stat_type": str(getattr(record, 'stat_type', '')),
                    "price": getattr(record, 'price', 0) / 1e9,
                    "quantity": int(getattr(record, 'quantity', 0)),
                })

            elif record_type == "StatusMsg":
                data.update({
                    "schema": "status",
                    "trading_status": str(getattr(record, 'trading_status', '')),
                    "halt_reason": str(getattr(record, 'halt_reason', '')),
                })

            elif record_type == "ErrorMsg":
                data.update({
                    "schema": "error",
                    "error": str(getattr(record, 'err', '')),
                })
                self._emit("error", {"message": data["error"]})
                return

            elif record_type == "SymbolMappingMsg":
                data.update({
                    "schema": "symbol_mapping",
                    "stype_in_symbol": str(getattr(record, 'stype_in_symbol', '')),
                    "stype_out_symbol": str(getattr(record, 'stype_out_symbol', '')),
                })

            elif record_type == "SystemMsg":
                data.update({
                    "schema": "system",
                    "msg": str(getattr(record, 'msg', '')),
                })

            else:
                # Generic handling for unknown types
                data["schema"] = "unknown"
                data["raw"] = str(record)

            self._emit("data", {"record": data})

        except Exception as e:
            self._emit("error", {"message": f"Error processing record: {str(e)}"})

    def _on_reconnect(self):
        """Handle reconnection events"""
        self._emit("status", {
            "message": "Reconnecting...",
            "connected": False,
            "reconnecting": True,
        })

    def start(
        self,
        dataset: str,
        schema: str,
        symbols: List[str],
        stype_in: str = "raw_symbol",
        snapshot: bool = False,
    ):
        """
        Start live streaming.

        Args:
            dataset: Dataset to stream from (e.g., 'XNAS.ITCH', 'GLBX.MDP3')
            schema: Data schema (e.g., 'trades', 'mbp-1', 'mbp-10', 'ohlcv-1s')
            symbols: List of symbols to subscribe to
            stype_in: Symbol type (raw_symbol, parent, smart)
            snapshot: Request initial snapshot of current state
        """
        if self.running:
            self._emit("error", {"message": "Stream already running"})
            return

        try:
            self._emit("status", {
                "message": "Initializing connection...",
                "connected": False,
            })

            # Create live client
            self.client = db.Live(key=self.api_key)

            # Add callbacks
            self.client.add_callback(self._handle_record)
            self.client.add_reconnect_callback(self._on_reconnect)

            # Subscribe to data
            self.client.subscribe(
                dataset=dataset,
                schema=schema,
                symbols=symbols,
                stype_in=stype_in,
                snapshot=snapshot,
            )

            self.running = True
            self._emit("status", {
                "message": f"Connected to {dataset}",
                "connected": True,
                "dataset": dataset,
                "schema": schema,
                "symbols": symbols,
            })

            # Start streaming
            self.client.start()

            # Block until stopped
            self.client.block_for_close()

            self._emit("status", {
                "message": "Stream closed",
                "connected": False,
            })

        except Exception as e:
            self._emit("error", {
                "message": f"Stream error: {str(e)}",
            })
            raise
        finally:
            self.running = False
            self.client = None

    def stop(self):
        """Stop the live stream"""
        if self.client and self.running:
            self._emit("status", {
                "message": "Stopping stream...",
                "connected": True,
            })
            try:
                self.client.stop()
            except Exception as e:
                self._emit("error", {"message": f"Error stopping: {str(e)}"})
            finally:
                self.running = False


def main():
    """CLI entry point for live streaming"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "type": "error",
            "message": "Usage: python databento_live.py <command> <args_json>",
            "commands": ["start", "info"],
            "databento_available": DATABENTO_AVAILABLE,
        }), flush=True)
        sys.exit(1)

    command = sys.argv[1]

    if command == "info":
        # Return info about live streaming capabilities
        print(json.dumps({
            "type": "info",
            "databento_available": DATABENTO_AVAILABLE,
            "databento_error": DATABENTO_ERROR,
            "schemas": [
                "trades",
                "mbp-1",
                "mbp-10",
                "tbbo",
                "ohlcv-1s",
                "ohlcv-1m",
                "imbalance",
                "statistics",
                "status",
            ],
            "datasets": [
                "XNAS.ITCH",
                "XNYS.PILLAR",
                "GLBX.MDP3",
                "OPRA.PILLAR",
                "DBEQ.BASIC",
            ],
            "stype_options": ["raw_symbol", "parent", "smart"],
        }), flush=True)
        return

    if command != "start":
        print(json.dumps({
            "type": "error",
            "message": f"Unknown command: {command}. Use 'start' or 'info'",
        }), flush=True)
        sys.exit(1)

    # Parse args
    args = {}
    if len(sys.argv) > 2:
        try:
            args = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            print(json.dumps({
                "type": "error",
                "message": f"Invalid JSON args: {e}",
            }), flush=True)
            sys.exit(1)

    # Check databento availability
    if not DATABENTO_AVAILABLE:
        print(json.dumps({
            "type": "error",
            "message": f"databento package not installed: {DATABENTO_ERROR}",
        }), flush=True)
        sys.exit(1)

    # Get API key
    api_key = args.get('api_key', '').strip()
    if not api_key:
        api_key = os.environ.get('DATABENTO_API_KEY', '').strip()

    if not api_key:
        print(json.dumps({
            "type": "error",
            "message": "API key is required",
        }), flush=True)
        sys.exit(1)

    # Extract streaming parameters
    dataset = args.get('dataset', 'XNAS.ITCH')
    schema = args.get('schema', 'trades')
    symbols = args.get('symbols', ['SPY'])
    if isinstance(symbols, str):
        symbols = [s.strip() for s in symbols.split(',')]
    stype_in = args.get('stype_in', 'raw_symbol')
    snapshot = args.get('snapshot', False)

    # Create manager and handle signals
    manager = LiveStreamManager(api_key)

    def signal_handler(signum, frame):
        """Handle shutdown signals"""
        print(json.dumps({
            "type": "status",
            "message": "Received shutdown signal",
            "connected": False,
        }), flush=True)
        manager.stop()
        sys.exit(0)

    # Register signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        manager.start(
            dataset=dataset,
            schema=schema,
            symbols=symbols,
            stype_in=stype_in,
            snapshot=snapshot,
        )
    except Exception as e:
        print(json.dumps({
            "type": "error",
            "message": str(e),
        }), flush=True)
        sys.exit(1)


if __name__ == "__main__":
    main()
