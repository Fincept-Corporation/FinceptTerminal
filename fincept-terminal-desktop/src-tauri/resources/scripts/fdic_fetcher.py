"""
FDIC Bank Data Fetcher
Location: fincept-terminal-desktop/src-tauri/resources/scripts/
Description: Robust, auto-paginated wrapper for the FDIC Bank Data API.
"""

import sys
import json
import requests
from datetime import datetime
from typing import Dict, Any, Optional, List

# ===== DATA SOURCES REQUIRED =====
# INPUT: 
#   - command (string): institutions, failures, financials, history, sod, etc.
#   - filters (string): Lucene/Elasticsearch query syntax.
#   - max_records (int): Optional safety cap.
# OUTPUT: 
#   - success (boolean)
#   - data (list): Flattened list of bank objects.
#   - cap_reached (boolean): True if safety limit was hit.
# PARAMETERS:
#   - default_limit: 1000 (FDIC API maximum)

FDIC_BASE_URL = "https://banks.data.fdic.gov/api"

class FDICWrapper:
    """Modular FDIC API wrapper with automatic pagination and safety caps."""

    def __init__(self, api_key: Optional[str] = None):
        self.base_url = FDIC_BASE_URL
        self.api_key = api_key
        self.session = requests.Session()
        self.session.headers.update({'User-Agent': 'Fincept-Desktop-Tauri/1.0'})

    def _fetch_all(self, endpoint: str, filters: str = "", fields: str = "", max_records: int = 5000) -> Dict[str, Any]:
        """
        Handles automatic pagination with a safety cap for desktop stability.
        """
        all_records = []
        offset = 0
        default_limit = 1000  # FDIC maximum per-page limit
        
        try:
            total_available = 0
            # Continue fetching while we are under the safety cap
            while len(all_records) < max_records:
                # Dynamic Limit: Request only what is needed to hit the exact cap
                remaining_needed = max_records - len(all_records)
                current_limit = min(default_limit, remaining_needed)

                params = {
                    "filters": filters,
                    "fields": fields,
                    "offset": offset,
                    "limit": current_limit
                }
                
                if self.api_key:
                    params['api_key'] = self.api_key

                response = self.session.get(f"{self.base_url}/{endpoint}", params=params, timeout=30)
                response.raise_for_status()
                payload = response.json()
                
                # Extract and flatten the FDIC data structure
                batch = [item['data'] for item in payload.get('data', [])]
                if not batch:
                    break
                    
                all_records.extend(batch)
                total_available = payload.get('meta', {}).get('total', 0)
                
                # Update offset by actual batch size to ensure continuity
                offset += len(batch)

                # Break if we have exhausted the server-side data
                if len(all_records) >= total_available:
                    break
                
            return {
                "success": True,
                "total_available": total_available,
                "total_fetched": len(all_records),
                "data": all_records,
                "cap_reached": len(all_records) >= max_records and len(all_records) < total_available,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            # Task #4: Improved Error Messaging for the Desktop UI
            return {
                "success": False, 
                "error": f"FDIC API Error: {str(e)}", 
                "endpoint": endpoint
            }

    # ===== 1 FUNCTION PER ENDPOINT =====

    def get_institutions(self, filters: str = "", max_records: int = 5000):
        """Get financial institution demographic and location information."""
        return self._fetch_all("institutions", filters, max_records=max_records)

    def get_locations(self, filters: str = "", max_records: int = 5000):
        """Get information on specific bank branches and locations."""
        return self._fetch_all("locations", filters, max_records=max_records)

    def get_history(self, filters: str = "", max_records: int = 5000):
        """Get details on structure change events like mergers and name changes."""
        return self._fetch_all("history", filters, max_records=max_records)

    def get_summary(self, filters: str = "", max_records: int = 5000):
        """Get historical aggregate financial data subtotaled by year."""
        return self._fetch_all("summary", filters, max_records=max_records)

    def get_failures(self, filters: str = "", max_records: int = 5000):
        """Get details on historical bank failures from 1934 to present."""
        return self._fetch_all("failures", filters, max_records=max_records)

    def get_sod(self, filters: str = "", max_records: int = 5000):
        """Get Summary of Deposits information for insured institutions."""
        return self._fetch_all("sod", filters, max_records=max_records)

    def get_financials(self, filters: str = "", max_records: int = 5000):
        """Get detailed financial reporting data for banks."""
        return self._fetch_all("financials", filters, max_records=max_records)

    def get_demographics(self, filters: str = "", max_records: int = 5000):
        """Get summary demographic information."""
        return self._fetch_all("demographics", filters, max_records=max_records)

# ===== TAURI CLI HANDLER =====

def main():
    """CLI Entry point for Tauri. Outputs minified JSON to stdout."""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command provided"}), flush=True)
        return

    command = sys.argv[1]
    filters = sys.argv[2] if len(sys.argv) > 2 else ""
    
    # Optional: Allow passing a custom safety cap from Tauri via 3rd argument
    try:
        custom_limit = int(sys.argv[3]) if len(sys.argv) > 3 else 5000
    except (ValueError, IndexError):
        custom_limit = 5000
    
    wrapper = FDICWrapper()
    
    # Mapping CLI commands to class methods
    mapping = {
        "institutions": wrapper.get_institutions,
        "locations": wrapper.get_locations,
        "history": wrapper.get_history,
        "summary": wrapper.get_summary,
        "failures": wrapper.get_failures,
        "sod": wrapper.get_sod,
        "financials": wrapper.get_financials,
        "demographics": wrapper.get_demographics
    }

    if command in mapping:
        result = mapping[command](filters, max_records=custom_limit)
        # Using separators=(',', ':') creates a compact JSON string for faster parsing
        print(json.dumps(result, separators=(',', ':')), flush=True)
    else:
        error_payload = {"success": False, "error": f"Unknown endpoint: {command}"}
        print(json.dumps(error_payload, separators=(',', ':')), flush=True)

if __name__ == "__main__":
    main()