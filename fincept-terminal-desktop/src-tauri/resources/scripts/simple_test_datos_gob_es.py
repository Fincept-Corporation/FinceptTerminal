#!/usr/bin/env python3
"""
Simple test script for datos.gob.es API wrapper
Tests basic functionality without complex Unicode characters
"""

import sys
import json
import subprocess
import time
from datetime import datetime

def run_command(command: str):
    """Run a command and return the result as JSON"""
    try:
        result = subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=30
        )

        if result.returncode == 0:
            try:
                return json.loads(result.stdout)
            except json.JSONDecodeError:
                return {
                    "error": "Invalid JSON response",
                    "stdout": result.stdout[:200],
                    "stderr": result.stderr[:200]
                }
        else:
            return {
                "error": f"Command failed with return code {result.returncode}",
                "stdout": result.stdout[:200],
                "stderr": result.stderr[:200]
            }
    except subprocess.TimeoutExpired:
        return {"error": "Command timed out"}
    except Exception as e:
        return {"error": f"Unexpected error: {str(e)}"}

def test_command(test_name: str, command: str):
    """Test a single command"""
    print(f"\n{'-'*50}")
    print(f"Testing: {test_name}")
    print(f"Command: {command}")
    print(f"{'-'*50}")

    result = run_command(command)

    if result.get("error"):
        print(f"RESULT: ERROR - {result['error']}")
        return False
    else:
        print(f"RESULT: SUCCESS")

        # Show basic info about response
        if isinstance(result, dict):
            if "data" in result and "metadata" in result:
                data = result.get("data", [])
                if isinstance(data, list):
                    print(f"Data count: {len(data)} items")
                elif isinstance(data, dict):
                    print(f"Data type: Dictionary with {len(data)} keys")
                else:
                    print(f"Data type: {type(data).__name__}")

                metadata = result.get("metadata", {})
                source = metadata.get("source", "Unknown")
                print(f"Source: {source}")
            else:
                print(f"Response keys: {list(result.keys())}")
        else:
            print(f"Response type: {type(result).__name__}")

        return True

def main():
    """Run basic tests on the API wrapper"""
    print("Starting tests for datos.gob.es API wrapper")
    print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    script_path = "datos_gob_es_api.py"
    total_tests = 0
    passed_tests = 0

    # List of tests to run
    tests = [
        ("Help/Usage", f"python {script_path}"),
        ("Get Publishers", f"python {script_path} publishers"),
        ("Search Datasets", f"python {script_path} search economia 10"),
        ("Get Popular Publishers", f"python {script_path} popular-publishers 5"),
        ("Get Recent Datasets", f"python {script_path} recent-datasets 10"),
        ("Get Taxonomy", f"python {script_path} taxonomy"),
        ("Get Coverage", f"python {script_path} coverage"),
        ("Invalid Command", f"python {script_path} invalid-command"),
    ]

    # Run each test
    for test_name, command in tests:
        total_tests += 1
        success = test_command(test_name, command)
        if success:
            passed_tests += 1
        time.sleep(1)  # Small delay between tests

    # Summary
    print(f"\n{'='*50}")
    print("TEST SUMMARY")
    print(f"{'='*50}")
    print(f"Total tests: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {total_tests - passed_tests}")
    print(f"Success rate: {(passed_tests/total_tests)*100:.1f}%")

    if passed_tests == total_tests:
        print("All tests passed!")
    elif passed_tests >= total_tests * 0.8:
        print("Most tests passed - wrapper is functional")
    else:
        print("Several tests failed - wrapper needs attention")

    print(f"\nTest completed at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    return passed_tests == total_tests

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)