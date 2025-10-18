#!/usr/bin/env python3
"""
World Bank API Test Suite
Tests each endpoint individually to ensure modularity and proper functionality
Each test runs independently - failure of one endpoint doesn't affect others
"""

import subprocess
import json
import sys
from typing import Dict, Any, List
import time

def run_command(command: List[str]) -> Dict[str, Any]:
    """Run a command and return the JSON result"""
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=60
        )

        if result.returncode != 0:
            return {
                "success": False,
                "error": f"Command failed with return code {result.returncode}",
                "stderr": result.stderr
            }

        try:
            data = json.loads(result.stdout)
            return {
                "success": True,
                "data": data,
                "raw_output": result.stdout
            }
        except json.JSONDecodeError as e:
            return {
                "success": False,
                "error": f"Invalid JSON output: {str(e)}",
                "raw_output": result.stdout
            }

    except subprocess.TimeoutExpired:
        return {
            "success": False,
            "error": "Command timed out after 60 seconds"
        }
    except Exception as e:
        return {
            "success": False,
            "error": f"Unexpected error: {str(e)}"
        }

def test_endpoint(endpoint_name: str, command: List[str],
                 expected_data_type: type = dict,
                 min_data_items: int = 0) -> Dict[str, Any]:
    """Test a single endpoint with validation"""
    print(f"\n{'='*50}")
    print(f"Testing: {endpoint_name}")
    print(f"Command: {' '.join(command)}")
    print(f"{'='*50}")

    result = run_command(command)

    if not result["success"]:
        print(f"❌ FAILED: {result['error']}")
        if "stderr" in result:
            print(f"STDERR: {result['stderr']}")
        return {"endpoint": endpoint_name, "status": "failed", "error": result["error"]}

    data = result["data"]

    # Check for API errors in the response
    if isinstance(data, dict) and data.get("error"):
        print(f"❌ API ERROR: {data['error']}")
        return {"endpoint": endpoint_name, "status": "api_error", "error": data["error"]}

    # Validate data type
    if not isinstance(data, expected_data_type):
        print(f"❌ DATA TYPE ERROR: Expected {expected_data_type.__name__}, got {type(data).__name__}")
        return {"endpoint": endpoint_name, "status": "type_error", "error": f"Wrong data type"}

    # Check for minimum data items
    if isinstance(data, dict) and "data" in data:
        data_items = data["data"]
        if min_data_items > 0 and len(data_items) < min_data_items:
            print(f"❌ INSUFFICIENT DATA: Expected at least {min_data_items} items, got {len(data_items)}")
            return {"endpoint": endpoint_name, "status": "insufficient_data", "error": "Not enough data items"}

        print(f"✅ SUCCESS: Got {len(data_items)} data items")

        # Show metadata if available
        if "metadata" in data:
            metadata = data["metadata"]
            print(f"📊 Metadata: {metadata}")

        # Show sample data
        if data_items:
            print(f"📋 Sample data: {json.dumps(data_items[0], indent=2)}")

    else:
        print(f"✅ SUCCESS: Got valid {expected_data_type.__name__} response")
        print(f"📋 Response: {json.dumps(data, indent=2)[:500]}...")

    return {
        "endpoint": endpoint_name,
        "status": "success",
        "data_count": len(data.get("data", [])) if isinstance(data, dict) else 1
    }

def run_all_tests():
    """Run all tests and report results"""
    print("Starting World Bank API Test Suite")
    print("Testing each endpoint independently...")

    test_results = []

    # Test 1: Get all countries
    test_results.append(
        test_endpoint(
            "Get Countries",
            ["python", "worldbank_data.py", "countries"],
            dict,
            50  # Expect at least 50 countries
        )
    )

    # Test 2: Get countries by region
    test_results.append(
        test_endpoint(
            "Get Countries by Region (Africa)",
            ["python", "worldbank_data.py", "countries", "AFR"],
            dict,
            10  # Expect at least 10 African countries
        )
    )

    # Test 3: Get sources
    test_results.append(
        test_endpoint(
            "Get Data Sources",
            ["python", "worldbank_data.py", "sources"],
            dict,
            10  # Expect at least 10 data sources
        )
    )

    # Test 4: Get GDP per capita for major economies
    test_results.append(
        test_endpoint(
            "Get GDP per Capita (USA, China, India)",
            ["python", "worldbank_data.py", "gdp_per_capita", "USA,CHN,IND", "5"],
            dict,
            10  # Expect at least 10 data points
        )
    )

    # Test 5: Get specific indicator data
    test_results.append(
        test_endpoint(
            "Get Population Data (USA)",
            ["python", "worldbank_data.py", "indicators", "USA", "SP.POP.TOTL", "2020:2024"],
            dict,
            3  # Expect at least 3 years of data
        )
    )

    # Test 6: Get commodity prices (Crude Oil)
    test_results.append(
        test_endpoint(
            "Get Crude Oil Prices",
            ["python", "worldbank_data.py", "commodity_prices", "PCRUDE_BRENT", "2"],
            dict,
            10  # Expect at least 10 price points
        )
    )

    # Test 7: Get economic snapshot
    test_results.append(
        test_endpoint(
            "Get Economic Snapshot (USA)",
            ["python", "worldbank_data.py", "economic_snapshot", "USA"],
            dict,
            1  # Expect at least some data
        )
    )

    # Test 8: Get regional comparison
    test_results.append(
        test_endpoint(
            "Get Regional Comparison (Africa - GDP per Capita)",
            ["python", "worldbank_data.py", "regional_comparison", "AFR", "NY.GDP.PCAP.CD", "2"],
            dict,
            20  # Expect data from multiple African countries
        )
    )

    # Test 9: Invalid country code (error handling test)
    test_results.append(
        test_endpoint(
            "Invalid Country Code (Error Handling)",
            ["python", "worldbank_data.py", "gdp_per_capita", "INVALID123", "5"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Test 10: Invalid indicator code (error handling test)
    test_results.append(
        test_endpoint(
            "Invalid Indicator Code (Error Handling)",
            ["python", "worldbank_data.py", "indicators", "USA", "INVALID.INDICATOR.CODE"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Generate final report
    print(f"\n{'='*60}")
    print("🏁 FINAL TEST REPORT")
    print(f"{'='*60}")

    success_count = 0
    failed_count = 0
    api_error_count = 0

    for result in test_results:
        status = result["status"]
        endpoint = result["endpoint"]

        if status == "success":
            success_count += 1
            print(f"✅ {endpoint}: SUCCESS")
            if "data_count" in result:
                print(f"   📊 Data points: {result['data_count']}")
        elif status == "api_error":
            api_error_count += 1
            print(f"⚠️  {endpoint}: API ERROR - {result['error']}")
        else:
            failed_count += 1
            print(f"❌ {endpoint}: FAILED - {result['error']}")

    print(f"\n📈 SUMMARY:")
    print(f"   ✅ Successful: {success_count}")
    print(f"   ⚠️  API Errors: {api_error_count}")
    print(f"   ❌ Failed: {failed_count}")
    print(f"   📊 Total Tests: {len(test_results)}")

    if success_count == len(test_results):
        print(f"\n🎉 ALL TESTS PASSED! The World Bank API wrapper is fully functional.")
    elif failed_count == 0 and api_error_count > 0:
        print(f"\n⚠️  MOSTLY SUCCESSFUL: {success_count} tests passed, {api_error_count} API errors (likely rate limiting or service issues)")
    else:
        print(f"\n❌ ISSUES FOUND: {failed_count} tests failed. Check the errors above.")

    return test_results

def main():
    """Main test runner"""
    try:
        results = run_all_tests()

        # Save results to file for reference
        with open("worldbank_test_results.json", "w") as f:
            json.dump(results, f, indent=2)

        print(f"\n💾 Test results saved to: worldbank_test_results.json")

        # Exit with appropriate code
        failed_tests = sum(1 for r in results if r["status"] not in ["success", "api_error"])
        if failed_tests > 0:
            sys.exit(1)
        else:
            sys.exit(0)

    except KeyboardInterrupt:
        print(f"\n⏹️  Tests interrupted by user")
        sys.exit(2)
    except Exception as e:
        print(f"\n💥 Test suite crashed: {str(e)}")
        sys.exit(3)

if __name__ == "__main__":
    main()