"""
PxWeb Statistics Data Fetcher
Location: fincept-terminal-desktop/src-tauri/resources/scripts/
Description: Standardized PxWeb API wrapper for Statistics Finland.
"""

import sys
import json
import time
import requests
from datetime import datetime
from typing import Dict, Any, Optional, List

# ===== DATA SOURCES REQUIRED =====
# INPUT: 
#   - command (string): nodes, metadata, or data.
#   - path (string): The sub-path to the database level or table.
#   - query (json): Optional JSON query for POST data retrieval.
# OUTPUT: 
#   - success (boolean)
#   - total_available (int)
#   - total_fetched (int)
#   - data (list/dict): The actual statistical content.
#   - cap_reached (boolean)
#   - timestamp (int)

class PxWebWrapper:
    def __init__(self, base_url: str = "https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin"):
        """
        Initializes the wrapper with the correct /PXWeb/ prefix.
        Source: Statistics Finland API Help
        """
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        self.session.headers.update({'User-Agent': 'Fincept-Desktop-Tauri/1.0'})

    def _make_request(self, path: str, method: str = "GET", json_data: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Handles requests with the mandated rate limiting (429 handling).
        Source: StatFi status codes
        """
        # Ensure path is formatted correctly relative to base_url
        clean_path = path.lstrip('/')
        url = f"{self.base_url}/{clean_path}" if clean_path else self.base_url
        
        try:
            if method == "POST":
                response = self.session.post(url, json=json_data, timeout=30)
            else:
                response = self.session.get(url, timeout=30)

            # 429 Error: Too many requests. Documented limit: 30 queries per 10 seconds.
            if response.status_code == 429:
                time.sleep(10) # Wait for sliding window
                return self._make_request(path, method, json_data)

            response.raise_for_status()
            raw_data = response.json()

            # Metadata calculation to match FDIC output format
            total_count = len(raw_data) if isinstance(raw_data, list) else 1
            
            return {
                "success": True,
                "total_available": total_count,
                "total_fetched": total_count,
                "data": raw_data,
                "cap_reached": False,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"PxWeb API Error: {str(e)}",
                "endpoint": path
            }

    def get_database_nodes(self, path: str = ""):
        """Browse database levels or subject realms."""
        return self._make_request(path, method="GET")

    def get_table_metadata(self, table_path: str):
        """Retrieve table metadata/variables."""
        return self._make_request(table_path, method="GET")

    def get_table_data(self, table_path: str, query_params: List[Dict]):
        """
        Fetch statistical data using POST. 
        Note: .px extension is required for table names in StatFi.
        """
        payload = {
            "query": query_params,
            "response": {"format": "json-stat2"} # json-stat2 is the recommended format
        }
        result = self._make_request(table_path, method="POST", json_data=payload)
        
        # Standardize 'total_fetched' for statistical data lists
        if result["success"] and isinstance(result["data"], dict):
            values = result["data"].get("value", [])
            result["total_fetched"] = len(values)
            result["total_available"] = len(values)
            
        return result

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command provided"}), flush=True)
        return

    command = sys.argv[1]
    path = sys.argv[2] if len(sys.argv) > 2 else ""
    
    wrapper = PxWebWrapper()

    if command == "nodes":
        result = wrapper.get_database_nodes(path)
    elif command == "metadata":
        result = wrapper.get_table_metadata(path)
    elif command == "data":
        try:
            # Expects JSON string for query as 3rd arg
            query_json = json.loads(sys.argv[3]) if len(sys.argv) > 3 else []
            result = wrapper.get_table_data(path, query_json)
        except json.JSONDecodeError:
            result = {"success": False, "error": "Invalid query JSON format"}
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    # Standard minified JSON output for Tauri
    print(json.dumps(result, separators=(',', ':')), flush=True)

if __name__ == "__main__":
    main()