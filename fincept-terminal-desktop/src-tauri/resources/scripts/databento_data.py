"""
Databento Data Fetcher
Test script to verify Databento API connection and data retrieval
Returns JSON output for Rust integration
"""

import sys
import json
import databento as db
from datetime import datetime
from typing import Dict, Any, List, Optional

def test_databento_connection():
    """Test basic Databento connection and data retrieval"""
    try:
        # Initialize historical client with API key
        client = db.Historical("db-Lw43s35pYsdmfBVDx7fcREKQHniUq")

        # Fetch sample data
        data = client.timeseries.get_range(
            dataset="GLBX.MDP3",
            symbols="ALL_SYMBOLS",
            start="2022-06-02T14:20:00",
            end="2022-06-02T14:30:00",
        )

        # Save raw data to file without conversion
        raw_data_file = "databento_raw_data.bin"
        count = 0

        def save_raw_record(record):
            nonlocal count
            count += 1
            # Save raw record object directly to file (binary mode)
            with open(raw_data_file, "ab") as f:
                # Convert to string representation for storage
                f.write(str(record).encode('utf-8'))
                f.write(b'\n')

        # Clear any existing file
        with open(raw_data_file, "wb") as f:
            f.write(b"")

        # Replay data and save raw records
        data.replay(save_raw_record)

        return {
            "success": True,
            "message": "Databento connection successful - raw data saved",
            "total_records": count,
            "raw_data_file": raw_data_file,
            "timestamp": int(datetime.now().timestamp())
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "message": "Databento connection failed",
            "timestamp": int(datetime.now().timestamp())
        }

def main():
    """Main Command-Line Interface entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python databento_data.py <command>",
            "commands": ["test"]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1].lower()
    result = {}

    try:
        if command == "test":
            result = test_databento_connection()
        else:
            result = {"error": f"Unknown command: {command}"}
    except Exception as e:
        result = {"error": f"An unexpected error occurred: {e}"}

    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()