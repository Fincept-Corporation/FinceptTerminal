#!/usr/bin/env python3
"""
Universal CLI fixer for all AKShare wrapper scripts
Replaces hardcoded endpoint maps with dynamic method resolution
"""
import os
import re

DYNAMIC_CLI_TEMPLATE = """
# ==================== CLI ====================
if __name__ == "__main__":
    import sys
    import json

    # Get wrapper instance
    wrapper = WRAPPER_CLASS()

    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python SCRIPT_NAME <endpoint> [args...]"}))
        sys.exit(1)

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Handle get_all_endpoints
    if endpoint == "get_all_endpoints":
        if hasattr(wrapper, 'get_all_available_endpoints'):
            result = wrapper.get_all_available_endpoints()
        elif hasattr(wrapper, 'get_all_endpoints'):
            result = wrapper.get_all_endpoints()
        else:
            result = {"success": False, "error": "Endpoint list not available"}
        print(json.dumps(result, ensure_ascii=True))
        sys.exit(0)

    # Dynamic method resolution
    method_name = f"get_{endpoint}" if not endpoint.startswith("get_") else endpoint

    if hasattr(wrapper, method_name):
        method = getattr(wrapper, method_name)
        try:
            result = method(*args) if args else method()
            print(json.dumps(result, ensure_ascii=True))
        except Exception as e:
            print(json.dumps({"success": False, "error": str(e), "endpoint": endpoint}))
    else:
        print(json.dumps({"success": False, "error": f"Unknown endpoint: {endpoint}. Method '{method_name}' not found."}))
"""

def find_wrapper_class(content):
    """Find the wrapper class name in the file"""
    patterns = [
        r'class\s+(\w+Wrapper)\s*[\(:]',
        r'class\s+(\w+API)\s*[\(:]',
        r'class\s+(AKShare\w+)\s*[\(:]',
        r'class\s+(\w+)\s*[\(:]'  # Any class
    ]
    for pattern in patterns:
        matches = re.findall(pattern, content)
        for match in matches:
            # Skip error classes and base classes
            if 'Error' not in match and 'Encoder' not in match and match != 'object':
                return match
    return None

def fix_file(filename):
    """Fix CLI routing in a single file"""
    print(f"\nProcessing {filename}...")

    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()

    # Find wrapper class
    wrapper_class = find_wrapper_class(content)
    if not wrapper_class:
        print(f"  SKIP: No wrapper class found")
        return False

    print(f"  Found wrapper: {wrapper_class}")

    # Find where main block starts
    main_idx = content.rfind('if __name__ == "__main__":')
    if main_idx == -1:
        main_idx = content.rfind("if __name__ == '__main__':")

    if main_idx == -1:
        print(f"  SKIP: No main block")
        return False

    # Also find CLI comment
    cli_idx = content.rfind('# =' + '=' * 10 + ' CLI ')
    if cli_idx != -1 and cli_idx < main_idx:
        main_idx = cli_idx

    # Cut at main/CLI block
    new_content = content[:main_idx].rstrip()

    # Add new CLI
    new_cli = DYNAMIC_CLI_TEMPLATE.replace('WRAPPER_CLASS', wrapper_class)
    new_cli = new_cli.replace('SCRIPT_NAME', filename)

    new_content += '\n' + new_cli + '\n'

    # Write back
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(new_content)

    print(f"  DONE: Fixed with {wrapper_class}")
    return True

# Files to fix
files_to_fix = [
    'akshare_analysis.py',
    'akshare_data.py',
    'akshare_derivatives.py',
    'akshare_economics_china.py',
    'akshare_economics_global.py',
    'akshare_funds_expanded.py',
    'akshare_alternative.py',
    'akshare_company_info.py'
]

print("=" * 60)
print("AKShare CLI Fixer - Dynamic Method Resolution")
print("=" * 60)

success_count = 0
for filename in files_to_fix:
    if os.path.exists(filename):
        if fix_file(filename):
            success_count += 1
    else:
        print(f"\nSKIP: {filename} not found")

print("\n" + "=" * 60)
print(f"Fixed {success_count}/{len(files_to_fix)} files")
print("=" * 60)
