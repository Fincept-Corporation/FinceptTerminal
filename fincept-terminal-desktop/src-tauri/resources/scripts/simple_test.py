#!/usr/bin/env python3
"""
Simple World Bank API Test
Tests each endpoint individually without Unicode characters
"""

import subprocess
import json
import sys

def run_test(name, command):
    """Run a single test"""
    print(f"\n{'='*50}")
    print(f"Testing: {name}")
    print(f"Command: {' '.join(command)}")
    print(f"{'='*50}")

    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)

        if result.returncode != 0:
            print(f"FAILED: Command failed with return code {result.returncode}")
            print(f"STDERR: {result.stderr}")
            return False

        try:
            data = json.loads(result.stdout)

            if isinstance(data, dict) and data.get("error"):
                print(f"API ERROR: {data['error']}")
                return False

            if isinstance(data, dict) and "data" in data:
                data_count = len(data["data"])
                print(f"SUCCESS: Got {data_count} data items")

                if "metadata" in data:
                    print(f"Metadata: {data['metadata']}")

                if data_count > 0:
                    print(f"Sample: {json.dumps(data['data'][0], indent=2)[:200]}...")
            else:
                print(f"SUCCESS: Got valid response")
                print(f"Response: {json.dumps(data, indent=2)[:200]}...")

            return True

        except json.JSONDecodeError as e:
            print(f"FAILED: Invalid JSON - {e}")
            print(f"Raw output: {result.stdout[:500]}...")
            return False

    except subprocess.TimeoutExpired:
        print("FAILED: Command timed out")
        return False
    except Exception as e:
        print(f"FAILED: Unexpected error - {e}")
        return False

def main():
    """Run all tests"""
    print("Starting World Bank API Tests")

    tests = [
        ("Get Countries", ["python", "worldbank_data.py", "countries"]),
        ("Get Sources", ["python", "worldbank_data.py", "sources"]),
        ("Get GDP per Capita", ["python", "worldbank_data.py", "gdp_per_capita", "USA,CHN,IND", "5"]),
        ("Get Population Data", ["python", "worldbank_data.py", "indicators", "USA", "SP.POP.TOTL", "2020:2024"]),
        ("Get Economic Snapshot", ["python", "worldbank_data.py", "economic_snapshot", "USA"]),
        ("Get Commodity Prices", ["python", "worldbank_data.py", "commodity_prices", "PCRUDE_BRENT", "2"]),
    ]

    passed = 0
    failed = 0

    for name, command in tests:
        if run_test(name, command):
            passed += 1
        else:
            failed += 1

    print(f"\n{'='*50}")
    print("FINAL REPORT")
    print(f"{'='*50}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Total: {passed + failed}")

    if failed == 0:
        print("\nAll tests PASSED! World Bank API wrapper is working correctly.")
    else:
        print(f"\n{failed} test(s) failed. Check the errors above.")

    return failed == 0

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)