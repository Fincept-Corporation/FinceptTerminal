#!/usr/bin/env python3
"""
Government of Canada API Test Suite
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
        print(f"[FAILED]: {result['error']}")
        if "stderr" in result:
            print(f"STDERR: {result['stderr']}")
        return {"endpoint": endpoint_name, "status": "failed", "error": result["error"]}

    data = result["data"]

    # Check for API errors in the response
    if isinstance(data, dict) and data.get("error"):
        print(f"[API ERROR]: {data['error']}")
        return {"endpoint": endpoint_name, "status": "api_error", "error": data["error"]}

    # Validate data type
    if not isinstance(data, expected_data_type):
        print(f"[DATA TYPE ERROR]: Expected {expected_data_type.__name__}, got {type(data).__name__}")
        return {"endpoint": endpoint_name, "status": "type_error", "error": f"Wrong data type"}

    # Check for minimum data items
    if isinstance(data, dict) and "data" in data:
        data_items = data["data"]
        if min_data_items > 0 and len(data_items) < min_data_items:
            print(f"[INSUFFICIENT DATA]: Expected at least {min_data_items} items, got {len(data_items)}")
            return {"endpoint": endpoint_name, "status": "insufficient_data", "error": "Not enough data items"}

        print(f"[SUCCESS]: Got {len(data_items)} data items")

        # Show metadata if available
        if "metadata" in data:
            metadata = data["metadata"]
            print(f"[Metadata]: {metadata}")

        # Show sample data
        if data_items:
            print(f"[Sample data]: {json.dumps(data_items[0], indent=2)}")

    else:
        print(f"[SUCCESS]: Got valid {expected_data_type.__name__} response")
        print(f"[Response]: {json.dumps(data, indent=2)[:500]}...")

    return {
        "endpoint": endpoint_name,
        "status": "success",
        "data_count": len(data.get("data", [])) if isinstance(data, dict) else 1
    }

def run_all_tests():
    """Run all tests and report results"""
    print("Starting Government of Canada API Test Suite")
    print("Testing each endpoint independently...")

    test_results = []

    # Test 1: Get all publishers (catalogue)
    test_results.append(
        test_endpoint(
            "Get Publishers (Catalogue)",
            ["python", "canada_gov_api.py", "publishers"],
            dict,
            5  # Expect at least 5 publishers
        )
    )

    # Test 2: Get datasets by publisher (using a common publisher)
    test_results.append(
        test_endpoint(
            "Get Datasets by Publisher (Statistics Canada)",
            ["python", "canada_gov_api.py", "datasets", "statistics-canada", "10"],
            dict,
            1  # Expect at least 1 dataset
        )
    )

    # Test 3: Search datasets
    test_results.append(
        test_endpoint(
            "Search Datasets (CPI)",
            ["python", "canada_gov_api.py", "search", "consumer price index", "5"],
            dict,
            1  # Expect at least 1 dataset
        )
    )

    # Test 4: Get dataset details
    test_results.append(
        test_endpoint(
            "Get Dataset Details (CPI)",
            ["python", "canada_gov_api.py", "dataset-details", "consumer-price-index-cpi"],
            dict,
            1  # Expect dataset details
        )
    )

    # Test 5: Get dataset resources
    test_results.append(
        test_endpoint(
            "Get Dataset Resources (CPI)",
            ["python", "canada_gov_api.py", "resources", "consumer-price-index-cpi"],
            dict,
            1  # Expect at least 1 resource
        )
    )

    # Test 6: Get recent datasets
    test_results.append(
        test_endpoint(
            "Get Recent Datasets",
            ["python", "canada_gov_api.py", "recent-datasets", "10"],
            dict,
            5  # Expect at least 5 recent datasets
        )
    )

    # Test 7: Get popular publishers
    test_results.append(
        test_endpoint(
            "Get Popular Publishers",
            ["python", "canada_gov_api.py", "popular-publishers", "10"],
            dict,
            3  # Expect at least 3 popular publishers
        )
    )

    # Test 8: Invalid publisher ID (error handling test)
    test_results.append(
        test_endpoint(
            "Invalid Publisher ID (Error Handling)",
            ["python", "canada_gov_api.py", "datasets", "invalid-publisher-123", "5"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Test 9: Invalid dataset ID (error handling test)
    test_results.append(
        test_endpoint(
            "Invalid Dataset ID (Error Handling)",
            ["python", "canada_gov_api.py", "dataset-details", "invalid-dataset-123"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Test 10: Publisher details (using Statistics Canada)
    test_results.append(
        test_endpoint(
            "Get Publisher Details (Statistics Canada)",
            ["python", "canada_gov_api.py", "publisher-details", "statistics-canada"],
            dict,
            1  # Expect publisher details
        )
    )

    # Test 11: Resource info (will test if we can get a valid resource ID)
    test_results.append(
        test_endpoint(
            "Get Resource Info",
            ["python", "canada_gov_api.py", "resource-info", "test-resource-id"],
            dict,
            0  # May fail due to invalid resource ID, but should not crash
        )
    )

    # Test 12: Preview (test with a known CSV URL)
    test_results.append(
        test_endpoint(
            "Resource Preview (CSV URL)",
            ["python", "canada_gov_api.py", "preview", "https://www150.statcan.gc.ca/t1/tbl1/en/csv/18100004-eng.zip"],
            dict,
            0  # May fail due to URL changes, but should not crash
        )
    )

    # Generate final report
    print(f"\n{'='*60}")
    print("FINAL TEST REPORT")
    print(f"{'='*60}")

    success_count = 0
    failed_count = 0
    api_error_count = 0

    for result in test_results:
        status = result["status"]
        endpoint = result["endpoint"]

        if status == "success":
            success_count += 1
            print(f"[SUCCESS] {endpoint}")
            if "data_count" in result:
                print(f"   Data points: {result['data_count']}")
        elif status == "api_error":
            api_error_count += 1
            print(f"[API ERROR] {endpoint} - {result['error']}")
        else:
            failed_count += 1
            print(f"[FAILED] {endpoint} - {result['error']}")

    print(f"\nSUMMARY:")
    print(f"   Successful: {success_count}")
    print(f"   API Errors: {api_error_count}")
    print(f"   Failed: {failed_count}")
    print(f"   Total Tests: {len(test_results)}")

    if success_count == len(test_results):
        print(f"\nALL TESTS PASSED! The Government of Canada API wrapper is fully functional.")
    elif failed_count == 0 and api_error_count > 0:
        print(f"\nMOSTLY SUCCESSFUL: {success_count} tests passed, {api_error_count} API errors (likely rate limiting or service issues)")
    else:
        print(f"\nISSUES FOUND: {failed_count} tests failed. Check the errors above.")

    return test_results

def main():
    """Main test runner"""
    try:
        results = run_all_tests()

        # Save results to file for reference
        with open("canada_gov_test_results.json", "w") as f:
            json.dump(results, f, indent=2)

        print(f"\nTest results saved to: canada_gov_test_results.json")

        # Exit with appropriate code
        failed_tests = sum(1 for r in results if r["status"] not in ["success", "api_error"])
        if failed_tests > 0:
            sys.exit(1)
        else:
            sys.exit(0)

    except KeyboardInterrupt:
        print(f"\nTests interrupted by user")
        sys.exit(2)
    except Exception as e:
        print(f"\nTest suite crashed: {str(e)}")
        sys.exit(3)

if __name__ == "__main__":
    main()