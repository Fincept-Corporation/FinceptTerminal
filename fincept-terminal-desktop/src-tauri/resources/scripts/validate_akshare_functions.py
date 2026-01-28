#!/usr/bin/env python3
"""
AKShare Function Validator
Tests all extracted functions against akshare 1.18.20 to identify invalid/missing functions
"""

import sys
import json
import re
from pathlib import Path
from collections import defaultdict

try:
    import akshare as ak
    print(f"AKShare version: {ak.__version__}")
except ImportError:
    print("ERROR: akshare not installed")
    sys.exit(1)

def extract_akshare_functions_from_file(filepath):
    """Extract all ak.function_name() calls from a Python file"""
    functions = set()

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

        # Find all ak.function_name patterns
        pattern = r'ak\.([a-z_][a-z0-9_]*)'
        matches = re.findall(pattern, content, re.IGNORECASE)
        functions.update(matches)

    return functions

def main():
    # Path to scripts directory
    scripts_dir = Path(__file__).parent

    # Find all akshare*.py files
    akshare_files = list(scripts_dir.glob('akshare*.py'))

    # Extract all functions
    all_functions = set()
    file_function_map = {}

    for filepath in akshare_files:
        filename = filepath.name
        if filename == 'validate_akshare_functions.py' or filename == 'analyze_akshare_functions.py':
            continue
        functions = extract_akshare_functions_from_file(filepath)
        file_function_map[filename] = functions
        all_functions.update(functions)

    print(f"\nTotal unique functions to validate: {len(all_functions)}")
    print("="*80)

    # Get all available akshare functions
    available_functions = set(dir(ak))

    # Validate each function
    valid_functions = []
    invalid_functions = []

    for func in sorted(all_functions):
        if hasattr(ak, func):
            valid_functions.append(func)
        else:
            # Find which files use this function
            files_using = [f for f, funcs in file_function_map.items() if func in funcs]
            invalid_functions.append((func, files_using))

    print(f"\nVALIDATION RESULTS:")
    print("="*80)
    print(f"Valid functions: {len(valid_functions)}")
    print(f"Invalid/Missing functions: {len(invalid_functions)}")
    print(f"Success rate: {len(valid_functions)/len(all_functions)*100:.1f}%")

    if invalid_functions:
        print("\n" + "="*80)
        print("INVALID/MISSING FUNCTIONS:")
        print("="*80)

        for func, files in sorted(invalid_functions):
            print(f"\n[!] {func}")
            print(f"    Used in: {', '.join(files)}")

            # Try to find similar functions
            similar = [f for f in available_functions if func.replace('_', '') in f.replace('_', '').lower()][:3]
            if similar:
                print(f"    Similar functions: {', '.join(similar)}")

    # Save detailed report
    report = {
        "akshare_version": ak.__version__,
        "total_functions_used": len(all_functions),
        "valid_functions": len(valid_functions),
        "invalid_functions": len(invalid_functions),
        "success_rate": len(valid_functions)/len(all_functions)*100,
        "invalid_function_details": [
            {
                "function": func,
                "files": files
            }
            for func, files in sorted(invalid_functions)
        ],
        "valid_function_list": sorted(valid_functions)
    }

    report_path = scripts_dir / "akshare_validation_report.json"
    with open(report_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    print(f"\n\nDetailed report saved to: {report_path}")

    # Category analysis for invalid functions
    if invalid_functions:
        print("\n" + "="*80)
        print("CATEGORY ANALYSIS:")
        print("="*80)

        categories = defaultdict(list)
        for func, files in invalid_functions:
            prefix = func.split('_')[0] if '_' in func else 'other'
            categories[prefix].append(func)

        for category, funcs in sorted(categories.items()):
            print(f"\n{category}: {len(funcs)} functions")
            for func in sorted(funcs)[:5]:
                print(f"  - {func}")
            if len(funcs) > 5:
                print(f"  ... and {len(funcs) - 5} more")

    print("\n" + "="*80)
    print("RECOMMENDATIONS:")
    print("="*80)
    print("1. Review invalid functions and update wrapper files")
    print("2. Check akshare documentation for correct function names")
    print("3. Remove or replace deprecated functions")
    print("4. Consider using similar functions where available")
    print("\nDocumentation: https://akshare.akfamily.xyz/")

if __name__ == "__main__":
    main()
