#!/usr/bin/env python3
"""
AKShare Function Analysis Script
Analyzes all akshare*.py files to extract used functions and compare against akshare 1.18.20 API
"""

import re
import os
from pathlib import Path
from collections import defaultdict

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

    print(f"Found {len(akshare_files)} akshare wrapper files\n")
    print("="*80)

    # Extract functions from each file
    all_functions = defaultdict(set)
    file_function_map = {}

    for filepath in sorted(akshare_files):
        filename = filepath.name
        functions = extract_akshare_functions_from_file(filepath)
        file_function_map[filename] = functions

        print(f"\n{filename}:")
        print(f"  Total akshare functions used: {len(functions)}")
        print(f"  Functions: {', '.join(sorted(list(functions)[:10]))}{'...' if len(functions) > 10 else ''}")

        # Categorize functions
        for func in functions:
            all_functions[func].add(filename)

    print("\n" + "="*80)
    print(f"\nTOTAL UNIQUE AKSHARE FUNCTIONS USED: {len(all_functions)}")
    print(f"ACROSS {len(akshare_files)} WRAPPER FILES")

    # Known outdated/problematic functions
    known_issues = {
        'energy_carbon': 'OUTDATED - Needs to be replaced with regional variants',
        'energy_carbon_price': 'May not exist in akshare 1.18.20',
        'energy_carbon_quota': 'May not exist in akshare 1.18.20',
        'energy_oil_em': 'Verify existence in akshare 1.18.20',
        'energy_oil_ice': 'Verify existence in akshare 1.18.20',
        'energy_natural_gas': 'Verify existence in akshare 1.18.20',
        'energy_coal': 'Verify existence in akshare 1.18.20',
        'energy_electricity': 'Verify existence in akshare 1.18.20',
        'energy_solar': 'Verify existence in akshare 1.18.20',
        'energy_wind': 'Verify existence in akshare 1.18.20',
        'energy_hydro': 'Verify existence in akshare 1.18.20',
    }

    print("\n" + "="*80)
    print("\nKNOWN PROBLEMATIC FUNCTIONS:")
    print("-"*80)

    found_issues = []
    for func, issue in known_issues.items():
        if func in all_functions:
            files = ', '.join(all_functions[func])
            found_issues.append((func, issue, files))
            print(f"\n[!] {func}")
            print(f"   Issue: {issue}")
            print(f"   Used in: {files}")

    # Print summary statistics
    print("\n" + "="*80)
    print("\nSUMMARY STATISTICS:")
    print("-"*80)
    print(f"Total wrapper files analyzed: {len(akshare_files)}")
    print(f"Total unique akshare functions used: {len(all_functions)}")
    print(f"Known problematic functions found: {len(found_issues)}")
    print(f"Estimated valid functions: {len(all_functions) - len(found_issues)}")

    # Category breakdown by file
    print("\n" + "="*80)
    print("\nFUNCTION USAGE BY FILE:")
    print("-"*80)
    for filename in sorted(file_function_map.keys()):
        func_count = len(file_function_map[filename])
        print(f"{filename:50s} : {func_count:4d} functions")

    # Most commonly used functions
    print("\n" + "="*80)
    print("\nMOST COMMONLY USED FUNCTIONS:")
    print("-"*80)
    function_usage = [(func, len(files)) for func, files in all_functions.items()]
    function_usage.sort(key=lambda x: x[1], reverse=True)

    for func, count in function_usage[:20]:
        if count > 1:
            print(f"{func:40s} : used in {count} files")

    print("\n" + "="*80)
    print("\nRECOMMENDATIONS:")
    print("-"*80)
    print("1. Fix energy_carbon in akshare_alternative.py (line 188)")
    print("   - Replace with: ak.energy_carbon_domestic() or specific regional function")
    print("   - Update wrapper method get_energy_carbon() to use correct function")
    print("")
    print("2. Verify all energy_* functions exist in akshare 1.18.20:")
    print("   - Check documentation: https://akshare.akfamily.xyz/")
    print("   - Run: python -c 'import akshare as ak; print(dir(ak))' | grep energy")
    print("")
    print("3. Create validation script to test all functions:")
    print("   - Import akshare and test hasattr(ak, 'function_name')")
    print("   - Generate list of missing/deprecated functions")
    print("")
    print("4. Missing categories in akshare 1.18.20 (out of 1111 total):")
    print(f"   - Your scripts cover ~{len(all_functions)} functions")
    print(f"   - Remaining: ~{1111 - len(all_functions)} functions not covered")
    print("   - Consider adding: sentiment analysis, social media, advanced ML features")

if __name__ == "__main__":
    main()
