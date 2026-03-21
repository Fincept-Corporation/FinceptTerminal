"""
Databento Data Explorer
Script to explore and analyze the raw data saved in databento_raw_data.bin
Shows data structure, sample records, and statistics
"""

import sys
import json
from datetime import datetime
from typing import Dict, Any, List
import re

def explore_raw_data():
    """Explore the raw Databento data file"""
    try:
        raw_data_file = "databento_raw_data.bin"

        # Read all records from the file
        records = []
        with open(raw_data_file, "r", encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if line:
                    records.append({
                        "line_number": line_num,
                        "raw_content": line
                    })

        if not records:
            return {
                "success": False,
                "error": "No data found in the raw data file"
            }

        # Analyze the data structure
        analysis = {
            "total_records": len(records),
            "file_size_bytes": len(open(raw_data_file, "rb").read()),
            "sample_records": records[:5],  # First 5 records
            "last_few_records": records[-3:],  # Last 3 records
        }

        # Extract data patterns
        record_types = {}
        symbol_counts = {}
        action_types = {}
        side_types = {}
        price_ranges = []

        # Parse first 100 records to understand structure
        sample_size = min(100, len(records))
        parsed_records = []

        for i, record in enumerate(records[:sample_size]):
            content = record["raw_content"]

            # Try to extract structured information using regex
            parsed = {
                "line_number": record["line_number"],
                "raw_content": content
            }

            # Extract message type
            if "Msg" in content:
                msg_type = re.search(r'(\w+Msg)', content)
                if msg_type:
                    parsed["message_type"] = msg_type.group(1)
                    record_types[parsed["message_type"]] = record_types.get(parsed["message_type"], 0) + 1

            # Extract symbol
            symbol_match = re.search(r"symbol=([^\s,\)]+)", content)
            if symbol_match:
                symbol = symbol_match.group(1)
                parsed["symbol"] = symbol
                if symbol != "None":
                    symbol_counts[symbol] = symbol_counts.get(symbol, 0) + 1

            # Extract action
            action_match = re.search(r"action=([A-Za-z])", content)
            if action_match:
                action = action_match.group(1)
                parsed["action"] = action
                action_types[action] = action_types.get(action, 0) + 1

            # Extract side
            side_match = re.search(r"side=([A-Za-z])", content)
            if side_match:
                side = side_match.group(1)
                parsed["side"] = side
                side_types[side] = side_types.get(side, 0) + 1

            # Extract price
            price_match = re.search(r"price=([\d.]+)", content)
            if price_match:
                price = float(price_match.group(1))
                parsed["price"] = price
                price_ranges.append(price)

            # Extract size
            size_match = re.search(r"size=(\d+)", content)
            if size_match:
                size = int(size_match.group(1))
                parsed["size"] = size

            # Extract timestamp
            ts_match = re.search(r"ts_event=(\d+)", content)
            if ts_match:
                timestamp = int(ts_match.group(1))
                parsed["timestamp_ns"] = timestamp
                # Convert nanoseconds to readable datetime
                parsed["timestamp_readable"] = datetime.fromtimestamp(timestamp / 1_000_000_000).strftime('%Y-%m-%d %H:%M:%S.%f')

            parsed_records.append(parsed)

        # Calculate statistics
        stats = {
            "record_types": record_types,
            "unique_symbols": list(symbol_counts.keys()),
            "symbol_counts": symbol_counts,
            "action_types": action_types,
            "side_types": side_types,
        }

        if price_ranges:
            stats["price_statistics"] = {
                "min_price": min(price_ranges),
                "max_price": max(price_ranges),
                "avg_price": sum(price_ranges) / len(price_ranges),
                "total_records_with_price": len(price_ranges)
            }

        # Time range analysis
        timestamps = [r["timestamp_ns"] for r in parsed_records if "timestamp_ns" in r]
        if timestamps:
            stats["time_range"] = {
                "start_time": datetime.fromtimestamp(min(timestamps) / 1_000_000_000).strftime('%Y-%m-%d %H:%M:%S.%f'),
                "end_time": datetime.fromtimestamp(max(timestamps) / 1_000_000_000).strftime('%Y-%m-%d %H:%M:%S.%f'),
                "duration_seconds": (max(timestamps) - min(timestamps)) / 1_000_000_000
            }

        return {
            "success": True,
            "file_analysis": analysis,
            "parsed_records_sample": parsed_records[:10],  # First 10 parsed records
            "statistics": stats,
            "exploration_summary": f"File contains {len(records)} records with {len(record_types)} different message types"
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "message": "Failed to explore raw data file"
        }

def show_sample_records(n=10):
    """Show a specific number of raw records"""
    try:
        raw_data_file = "databento_raw_data.bin"
        records = []

        with open(raw_data_file, "r", encoding='utf-8') as f:
            for i, line in enumerate(f, 1):
                if i <= n:
                    records.append({
                        "record_number": i,
                        "content": line.strip()
                    })
                else:
                    break

        return {
            "success": True,
            "sample_size": len(records),
            "records": records
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }

def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python explore_databento_data.py <command>",
            "commands": ["explore", "sample [n]"]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1].lower()
    result = {}

    try:
        if command == "explore":
            result = explore_raw_data()
        elif command == "sample":
            n = int(sys.argv[2]) if len(sys.argv) > 2 else 10
            result = show_sample_records(n)
        else:
            result = {"error": f"Unknown command: {command}"}
    except Exception as e:
        result = {"error": f"An unexpected error occurred: {e}"}

    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()