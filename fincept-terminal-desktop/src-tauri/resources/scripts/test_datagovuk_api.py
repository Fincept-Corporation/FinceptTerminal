#!/usr/bin/env python3
"""
data.gov.uk API Test Suite
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
        print(f"FAILED: {result['error']}")
        if "stderr" in result:
            print(f"STDERR: {result['stderr']}")
        return {"endpoint": endpoint_name, "status": "failed", "error": result["error"]}

    data = result["data"]

    # Check for API errors in the response
    if isinstance(data, dict) and data.get("error"):
        print(f"API ERROR: {data['error']}")
        return {"endpoint": endpoint_name, "status": "api_error", "error": data["error"]}

    # Validate data type
    if not isinstance(data, expected_data_type):
        print(f"DATA TYPE ERROR: Expected {expected_data_type.__name__}, got {type(data).__name__}")
        return {"endpoint": endpoint_name, "status": "type_error", "error": f"Wrong data type"}

    # Check for minimum data items
    if isinstance(data, dict) and "data" in data:
        data_items = data["data"]
        if min_data_items > 0 and len(data_items) < min_data_items:
            print(f"INSUFFICIENT DATA: Expected at least {min_data_items} items, got {len(data_items)}")
            return {"endpoint": endpoint_name, "status": "insufficient_data", "error": "Not enough data items"}

        print(f"SUCCESS: Got {len(data_items)} data items")

        # Show metadata if available
        if "metadata" in data:
            metadata = data["metadata"]
            print(f"Metadata: {json.dumps(metadata, indent=2)[:200]}...")

        # Show sample data
        if data_items:
            print(f"Sample data: {json.dumps(data_items[0], indent=2)[:300]}...")

    else:
        print(f"SUCCESS: Got valid {expected_data_type.__name__} response")
        print(f"Response: {json.dumps(data, indent=2)[:500]}...")

    return {
        "endpoint": endpoint_name,
        "status": "success",
        "data_count": len(data.get("data", [])) if isinstance(data, dict) else 1
    }

def run_all_tests():
    """Run all tests and report results"""
    print("Starting data.gov.uk API Test Suite")
    print("Testing each endpoint independently...")
    print("Note: Some tests may fail due to rate limiting or network issues")

    test_results = []

    # Test 1: Get all publishers (catalogue)
    test_results.append(
        test_endpoint(
            "Get All Publishers",
            ["python", "datagovuk_api.py", "publishers"],
            dict,
            5  # Expect at least 5 publishers
        )
    )

    # Test 2: Get popular publishers
    test_results.append(
        test_endpoint(
            "Get Popular Publishers",
            ["python", "datagovuk_api.py", "popular-publishers", "10"],
            dict,
            3  # Expect at least 3 popular publishers
        )
    )

    # Test 3: Search datasets
    test_results.append(
        test_endpoint(
            "Search Datasets (CPI)",
            ["python", "datagovuk_api.py", "search", "consumer price inflation", "10"],
            dict,
            1  # Expect at least 1 dataset
        )
    )

    # Test 4: Get recent datasets
    test_results.append(
        test_endpoint(
            "Get Recent Datasets",
            ["python", "datagovuk_api.py", "recent-datasets", "5"],
            dict,
            1  # Expect at least 1 recent dataset
        )
    )

    # Test 5: Get datasets from ONS (if it exists)
    test_results.append(
        test_endpoint(
            "Get Datasets from ONS",
            ["python", "datagovuk_api.py", "datasets", "office-for-national-statistics", "5"],
            dict,
            1  # Expect at least 1 dataset from ONS
        )
    )

    # Test 6: Get dataset details (using a common dataset)
    test_results.append(
        test_endpoint(
            "Get Dataset Details (CPI)",
            ["python", "datagovuk_api.py", "dataset-details", "consumer-price-inflation-time-series"],
            dict,
            0  # May not exist, but should not crash
        )
    )

    # Test 7: Get resources for a dataset
    test_results.append(
        test_endpoint(
            "Get Dataset Resources",
            ["python", "datagovuk_api.py", "resources", "consumer-price-inflation-time-series"],
            dict,
            0  # May not exist, but should not crash
        )
    )

    # Test 8: Test with invalid publisher ID (error handling)
    test_results.append(
        test_endpoint(
            "Invalid Publisher ID (Error Handling)",
            ["python", "datagovuk_api.py", "datasets", "invalid-publisher-123"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Test 9: Test with invalid dataset ID (error handling)
    test_results.append(
        test_endpoint(
            "Invalid Dataset ID (Error Handling)",
            ["python", "datagovuk_api.py", "dataset-details", "invalid-dataset-123"],
            dict,
            0  # Expect no data, but should not crash
        )
    )

    # Test 10: Search with no results (edge case)
    test_results.append(
        test_endpoint(
            "Search with No Results",
            ["python", "datagovuk_api.py", "search", "xyz123nonexistentdataset", "5"],
            dict,
            0  # Expect 0 results, but should not crash
        )
    )

    # Test 11: Get publisher details (using a common publisher)
    test_results.append(
        test_endpoint(
            "Get Publisher Details",
            ["python", "datagovuk_api.py", "publisher-details", "office-for-national-statistics"],
            dict,
            0  # May not exist, but should not crash
        )
    )

    # Test 12: Get resource info (using sample resource ID)
    test_results.append(
        test_endpoint(
            "Get Resource Info",
            ["python", "datagovuk_api.py", "resource-info", "sample-resource-id"],
            dict,
            0  # Expect error, but should not crash
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
            print(f"[SUCCESS] {endpoint}: SUCCESS")
            if "data_count" in result:
                print(f"   Data points: {result['data_count']}")
        elif status == "api_error":
            api_error_count += 1
            print(f"[API_ERROR] {endpoint}: API ERROR - {result['error']}")
        else:
            failed_count += 1
            print(f"[FAILED] {endpoint}: FAILED - {result['error']}")

    print(f"\nSUMMARY:")
    print(f"   Successful: {success_count}")
    print(f"   API Errors: {api_error_count}")
    print(f"   Failed: {failed_count}")
    print(f"   Total Tests: {len(test_results)}")

    if success_count == len(test_results):
        print(f"\nALL TESTS PASSED! The data.gov.uk API wrapper is fully functional.")
    elif failed_count == 0 and api_error_count > 0:
        print(f"\nMOSTLY SUCCESSFUL: {success_count} tests passed, {api_error_count} API errors (likely rate limiting or missing data)")
    else:
        print(f"\nISSUES FOUND: {failed_count} tests failed. Check the errors above.")

    print(f"\nNOTE: Some tests may fail due to:")
    print(f"   - Rate limiting from data.gov.uk")
    print(f"   - Network connectivity issues")
    print(f"   - Dataset/publisher IDs that don't exist")
    print(f"   - API key requirements (if enforced)")

    return test_results

def main():
    """Main test runner"""
    try:
        results = run_all_tests()

        # Save results to file for reference
        with open("datagovuk_test_results.json", "w") as f:
            json.dump(results, f, indent=2)

        print(f"\nTest results saved to: datagovuk_test_results.json")

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