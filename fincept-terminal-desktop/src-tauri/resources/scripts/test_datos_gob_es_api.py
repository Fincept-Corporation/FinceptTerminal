#!/usr/bin/env python3
"""
Test script for datos.gob.es API wrapper
Tests all endpoints independently to ensure proper error handling and functionality
"""

import sys
import json
import subprocess
import time
from datetime import datetime
from typing import Dict, Any, List

def run_command(command: str) -> Dict[str, Any]:
    """Run a command and return the result as JSON"""
    try:
        result = subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=60
        )

        if result.returncode == 0:
            try:
                return json.loads(result.stdout)
            except json.JSONDecodeError:
                return {
                    "error": "Invalid JSON response",
                    "stdout": result.stdout,
                    "stderr": result.stderr
                }
        else:
            return {
                "error": f"Command failed with return code {result.returncode}",
                "stdout": result.stdout,
                "stderr": result.stderr
            }
    except subprocess.TimeoutExpired:
        return {"error": "Command timed out"}
    except Exception as e:
        return {"error": f"Unexpected error: {str(e)}"}

def test_endpoint(test_name: str, command: str, expected_error: bool = False) -> bool:
    """Test a single endpoint and report results"""
    print(f"\n{'='*60}")
    print(f"Testing: {test_name}")
    print(f"Command: {command}")
    print(f"{'='*60}")

    result = run_command(command)

    if result.get("error"):
        if expected_error:
            print(f"✅ PASS - Expected error occurred: {result['error']}")
            return True
        else:
            print(f"❌ FAIL - Unexpected error: {result['error']}")
            if result.get("stdout"):
                print(f"stdout: {result['stdout']}")
            if result.get("stderr"):
                print(f"stderr: {result['stderr']}")
            return False
    else:
        print(f"✅ PASS - Command executed successfully")

        # Analyze response structure
        if "data" in result and "metadata" in result:
            print(f"  - Response structure: ✅ Valid (data + metadata)")

            data = result.get("data", [])
            if isinstance(data, list):
                print(f"  - Data count: {len(data)} items")
                if data:
                    print(f"  - Sample item: {str(data[0])[:100]}...")
            elif isinstance(data, dict):
                print(f"  - Data type: Dictionary")
                if data:
                    keys = list(data.keys())[:5]
                    print(f"  - Sample keys: {keys}")

            metadata = result.get("metadata", {})
            if metadata:
                print(f"  - Metadata source: {metadata.get('source', 'Unknown')}")
                print(f"  - Metadata last_updated: {metadata.get('last_updated', 'Unknown')}")
        else:
            print(f"  - Response structure: ⚠️  Non-standard format")
            print(f"  - Keys: {list(result.keys())}")

        return True

def main():
    """Run comprehensive tests on all datos.gob.es API endpoints"""
    print("Starting comprehensive test of datos.gob.es API wrapper")
    print(f"Test started at: {datetime.now().isoformat()}")

    script_path = "datos_gob_es_api.py"

    # Test results tracking
    total_tests = 0
    passed_tests = 0
    failed_tests = []

    # Define all tests to run
    tests = [
        # Catalogue Tests
        ("Get Publishers", f"python {script_path} publishers"),
        ("Get Publisher Details (Invalid)", f"python {script_path} publisher-details invalid-id", True),
        ("Get Publisher Details (Sample)", f"python {script_path} publisher-details economia-y-hacienda"),

        # Dataset Tests
        ("Get Datasets by Publisher (Invalid)", f"python {script_path} datasets invalid-publisher 10", True),
        ("Get Datasets by Publisher (Sample)", f"python {script_path} datasets economia-y-hacienda 10"),
        ("Search Datasets", f"python {script_path} search economia 20"),
        ("Search Datasets (No Results)", f"python {script_path} search xyz123noresults 5"),
        ("Get Dataset Details (Invalid)", f"python {script_path} dataset-details invalid-dataset", True),

        # Resource Tests
        ("Get Dataset Resources (Invalid)", f"python {script_path} resources invalid-dataset", True),
        ("Get Resource Info (Invalid)", f"python {script_path} resource-info invalid-resource", True),
        ("Get Series Data (Invalid)", f"python {script_path} series-data invalid-resource json", True),
        ("Download Resource Preview (Invalid URL)", f"python {script_path} preview http://invalid-url", True),

        # Utility Tests
        ("Get Popular Publishers", f"python {script_path} popular-publishers 10"),
        ("Get Recent Datasets", f"python {script_path} recent-datasets 20"),
        ("Get Taxonomy Categories", f"python {script_path} taxonomy"),
        ("Get Geographical Coverage", f"python {script_path} coverage"),

        # Invalid Command Tests
        ("Invalid Command", f"python {script_path} invalid-command", True),
        ("Missing Arguments", f"python {script_path}", True),
    ]

    # Run all tests
    for test_name, command, *expect_error in tests:
        total_tests += 1
        expected_error = expect_error[0] if expect_error else False

        try:
            success = test_endpoint(test_name, command, expected_error)
            if success:
                passed_tests += 1
            else:
                failed_tests.append((test_name, command))
        except Exception as e:
            print(f"❌ FAIL - Test execution error: {str(e)}")
            failed_tests.append((test_name, f"EXECUTION_ERROR: {command} - {str(e)}"))

        # Small delay between tests to avoid rate limiting
        time.sleep(0.5)

    # Print final results
    print(f"\n{'='*80}")
    print("🏁 TEST SUMMARY")
    print(f"{'='*80}")
    print(f"Total Tests: {total_tests}")
    print(f"Passed: {passed_tests} ✅")
    print(f"Failed: {len(failed_tests)} ❌")
    print(f"Success Rate: {(passed_tests/total_tests)*100:.1f}%")

    if failed_tests:
        print(f"\n❌ FAILED TESTS:")
        for test_name, command in failed_tests:
            print(f"  - {test_name}: {command}")

    print(f"\n🎯 Overall Assessment:")
    if passed_tests == total_tests:
        print("  ✅ All tests passed! The API wrapper is working correctly.")
    elif (passed_tests / total_tests) >= 0.8:
        print("  ✅ Most tests passed! The API wrapper is functional with minor issues.")
    elif (passed_tests / total_tests) >= 0.6:
        print("  ⚠️  Several tests failed. The API wrapper needs attention.")
    else:
        print("  ❌ Many tests failed. The API wrapper requires significant fixes.")

    print(f"\n📊 Test completed at: {datetime.now().isoformat()}")

    # Exit with appropriate code
    sys.exit(0 if passed_tests == total_tests else 1)

if __name__ == "__main__":
    main()