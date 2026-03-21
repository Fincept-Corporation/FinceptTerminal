#!/usr/bin/env python3
"""
Fincept Terminal - Strategy Test Suite
Tests all strategies for import/instantiation/initialization errors.
"""
import os
import sys
import importlib
import traceback

# Set up path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

# Import the framework first to ensure stubs are registered
try:
    from AlgorithmImports import *
    print("OK: AlgorithmImports loaded successfully")
except Exception as e:
    print(f"FATAL: AlgorithmImports failed to load: {e}")
    traceback.print_exc()
    sys.exit(1)

# Strategy files to test (all .py files in the strategies directory, excluding infrastructure)
EXCLUDE = {
    '__init__', '_registry', 'AlgorithmImports', 'main', 'test_strategies',
    'live_runner'
}

def get_strategy_files():
    """Collect all strategy .py files."""
    strategies = []
    # Top-level strategies
    for f in sorted(os.listdir(SCRIPT_DIR)):
        if f.endswith('.py') and not f.startswith('_') and f[:-3] not in EXCLUDE:
            strategies.append(('', f[:-3]))
    # Subdirectory strategies (alphas, benchmarks, Execution, Portfolio, Risk, Selection)
    for subdir in ['alphas', 'benchmarks', 'Execution', 'Portfolio', 'Risk', 'Selection', 'Alphas']:
        subpath = os.path.join(SCRIPT_DIR, subdir)
        if os.path.isdir(subpath):
            for f in sorted(os.listdir(subpath)):
                if f.endswith('.py') and not f.startswith('_'):
                    strategies.append((subdir, f[:-3]))
    return strategies

def test_strategy_import(subdir, module_name):
    """Test that a strategy module can be imported and its class instantiated."""
    errors = []

    # Build the module path
    if subdir:
        full_module = f"{subdir}.{module_name}"
        file_path = os.path.join(SCRIPT_DIR, subdir, f"{module_name}.py")
    else:
        full_module = module_name
        file_path = os.path.join(SCRIPT_DIR, f"{module_name}.py")

    # Phase 1: Import the module
    try:
        if subdir:
            # For subdirectory modules, use importlib with spec
            spec = importlib.util.spec_from_file_location(full_module, file_path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
        else:
            spec = importlib.util.spec_from_file_location(module_name, file_path)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
    except Exception as e:
        tb = traceback.format_exc()
        return [('IMPORT', str(e), tb)]

    # Phase 2: Find and instantiate the algorithm class
    algo_classes = []
    for name in dir(mod):
        obj = getattr(mod, name)
        if (isinstance(obj, type)
            and issubclass(obj, QCAlgorithm)
            and obj is not QCAlgorithm
            and obj.__module__ != 'fincept_engine.algorithm'):
            algo_classes.append((name, obj))

    if not algo_classes:
        # Not necessarily an error - some files are base classes or utilities
        return []

    for class_name, cls in algo_classes:
        try:
            instance = cls()
        except Exception as e:
            tb = traceback.format_exc()
            errors.append(('INSTANTIATE', f"{class_name}: {e}", tb))
            continue

        # Phase 3: Try calling initialize() if it exists
        if hasattr(instance, 'initialize'):
            try:
                instance.initialize()
            except Exception as e:
                tb = traceback.format_exc()
                errors.append(('INITIALIZE', f"{class_name}.initialize(): {e}", tb))

    return errors


def main():
    strategies = get_strategy_files()
    print(f"\nTesting {len(strategies)} strategy files...\n")
    print("=" * 80)

    passed = 0
    failed = 0
    failures = []

    for subdir, module_name in strategies:
        display_name = f"{subdir}/{module_name}" if subdir else module_name
        errors = test_strategy_import(subdir, module_name)

        if errors:
            failed += 1
            for phase, msg, tb in errors:
                failures.append((display_name, phase, msg, tb))
                print(f"FAIL [{phase}]: {display_name}")
                print(f"  Error: {msg}")
                # Print just the last few lines of traceback
                tb_lines = tb.strip().split('\n')
                for line in tb_lines[-4:]:
                    print(f"  {line}")
                print()
        else:
            passed += 1
            # Only print passes for verification
            # print(f"PASS: {display_name}")

    print("=" * 80)
    print(f"\nRESULTS: {passed} passed, {failed} failed out of {len(strategies)} total")
    print()

    if failures:
        print("=" * 80)
        print("FAILURE SUMMARY")
        print("=" * 80)
        for i, (name, phase, msg, tb) in enumerate(failures, 1):
            print(f"\n{i}. {name} [{phase}]")
            print(f"   Error: {msg}")
        print()

    return failed


if __name__ == '__main__':
    sys.exit(main())
