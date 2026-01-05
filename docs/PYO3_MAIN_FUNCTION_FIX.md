# PyO3 Python Script Integration Fix

## Problem Summary

Python scripts called via PyO3 (Rust-Python bridge) were failing with:
```
TypeError: main() takes 0 positional arguments but 1 was given
```

## Root Cause

**Mismatch between PyO3 execution and Python script expectations:**

### How PyO3 Calls Python Functions
In `src-tauri/src/python_runtime.rs` (line 84):
```rust
let main_fn = module.getattr("main")?;
let result = main_fn.call1((args,))?;  // ← Passes args as function parameter
```

PyO3 calls the Python `main()` function and passes arguments as a **function parameter**.

### How Scripts Were Written (INCORRECT)
```python
def main():  # ❌ No parameters accepted
    import sys
    if len(sys.argv) < 2:  # Expecting CLI-style sys.argv
        print(json.dumps({"error": "No command specified"}))
    command = sys.argv[1]
```

Scripts expected arguments to come from `sys.argv` (traditional command-line style), not as function parameters.

## The Fix

Update Python scripts to accept arguments as a function parameter while maintaining CLI compatibility:

### Pattern to Apply

**BEFORE (Broken):**
```python
def main():
    """CLI entry point"""
    import sys

    if len(sys.argv) < 2:
        print(json.dumps({"error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    param1 = sys.argv[2] if len(sys.argv) > 2 else "default"

    # ... process command ...
    print(json.dumps(result))
```

**AFTER (Fixed):**
```python
def main(args=None):
    """CLI entry point - supports both PyO3 and CLI execution"""
    import sys

    # Support both PyO3 call (args passed as parameter) and CLI call (sys.argv)
    if args is None:
        args = sys.argv[1:]  # Skip script name for CLI

    if len(args) < 1:
        return json.dumps({"error": "No command specified"})

    command = args[0]
    param1 = args[1] if len(args) > 1 else "default"

    # ... process command ...
    output = json.dumps(result)
    return output  # Return instead of print for PyO3
```

### Key Changes Required

1. **Function Signature**: `def main():` → `def main(args=None):`
2. **Argument Handling**:
   - Add fallback: `if args is None: args = sys.argv[1:]`
   - Use `args[0]` instead of `sys.argv[1]`
   - Use `args[1]` instead of `sys.argv[2]`, etc.
3. **Return Values**: Use `return json.dumps(...)` instead of `print(json.dumps(...))`
4. **Error Handling**: Return error JSON instead of calling `sys.exit(1)`

### Index Adjustment Table

| Old (sys.argv) | New (args parameter) | Description |
|----------------|----------------------|-------------|
| `sys.argv[1]`  | `args[0]`           | First argument (command) |
| `sys.argv[2]`  | `args[1]`           | Second argument |
| `sys.argv[3]`  | `args[2]`           | Third argument |
| `len(sys.argv) < 2` | `len(args) < 1` | Check if command exists |
| `len(sys.argv) > 2` | `len(args) > 1` | Check if param exists |

**Note**: The index shift happens because `sys.argv[0]` contains the script name, but when args are passed via PyO3, the script name is not included.

## Scripts Already Fixed

✅ **Portfolio Optimization:**
- `src-tauri/resources/scripts/Analytics/skfolio_wrapper.py` - PyO3 main() function fixed
- `src-tauri/resources/scripts/Analytics/pyportfolioopt_wrapper/core.py` - Indentation errors fixed, import paths corrected

## Scripts That May Need Fixing

Scripts with `def main():` pattern that are called via `python_runtime::execute_python_script()`:

### High Priority (Actively Used)
- `agno_trading_service.py` - Trading service
- `ai_quant_lab/qlib_service.py` - Quantitative lab
- `akshare_*.py` - Chinese market data scripts
- Any script referenced in `src-tauri/src/commands/*.rs`

### Medium Priority (Wrapper Scripts)
- `gs_quant_wrapper/markets_wrapper.py` - GS Quant markets
- `gs_quant_wrapper/instrument_wrapper.py` - GS Quant instruments

### Low Priority
- Test scripts and examples that don't need PyO3 integration

## How to Identify Scripts That Need Fixing

### 1. Find Python Scripts Called from Rust
```bash
grep -r "execute_python_script" src-tauri/src/commands/ | grep -o '"[^"]*\.py"' | sort -u
```

### 2. Check if Script Has main() Function
```bash
grep -l "def main():" src-tauri/resources/scripts/**/*.py
```

### 3. Verify Script Uses sys.argv
```bash
grep -A 10 "def main():" <script_path> | grep "sys.argv"
```

If all three match, the script needs this fix.

## Testing the Fix

After applying the fix, verify:

1. **PyO3 Execution**: Script works when called from Rust/Tauri
   - Check console logs for successful execution
   - Verify JSON response is returned correctly

2. **CLI Execution**: Script still works from command line
   ```bash
   python script.py command arg1 arg2
   ```

3. **Error Handling**: Errors return JSON, not exceptions
   ```python
   {"error": "description", "traceback": "..."}
   ```

## Prevention

For **new Python scripts**:
- Always define `main(args=None)` instead of `main()`
- Always handle both PyO3 and CLI argument styles
- Always return JSON instead of printing (for PyO3 compatibility)

## Example: Complete Fixed Script

```python
#!/usr/bin/env python3
import json
import sys

def main(args=None):
    """
    Main entry point - compatible with both PyO3 and CLI execution.

    Args:
        args: List of command-line arguments (PyO3 style), or None for CLI style

    Returns:
        JSON string with results or error
    """
    # Support both execution styles
    if args is None:
        args = sys.argv[1:]

    # Validate arguments
    if len(args) < 1:
        return json.dumps({"error": "No command specified"})

    command = args[0]

    try:
        if command == "optimize":
            symbols = args[1] if len(args) > 1 else "AAPL,GOOGL,MSFT"
            method = args[2] if len(args) > 2 else "mean_variance"

            # Process optimization
            result = {
                "weights": {"AAPL": 0.33, "GOOGL": 0.33, "MSFT": 0.34},
                "sharpe_ratio": 1.5,
                "method": method
            }
            return json.dumps(result)

        elif command == "backtest":
            # Backtest logic
            return json.dumps({"status": "success"})

        else:
            return json.dumps({"error": f"Unknown command: {command}"})

    except Exception as e:
        import traceback
        return json.dumps({
            "error": str(e),
            "traceback": traceback.format_exc()
        })

if __name__ == "__main__":
    # CLI execution
    result = main()
    print(result)
```

## Summary

This issue affected all Python scripts using the `def main():` pattern with `sys.argv` that are called via PyO3. The fix makes scripts compatible with both PyO3 (function parameter style) and traditional CLI execution (sys.argv style) by accepting an optional `args` parameter and returning values instead of printing them.

**Status**: Fixed for portfolio optimization (skfolio_wrapper.py and pyportfolioopt_wrapper). Other scripts should be fixed as they're encountered or proactively based on priority.

## Additional Fixes for PyPortfolioOpt Module

### Indentation Errors (2026-01-01)
The `pyportfolioopt_wrapper/core.py` file had systematic indentation errors where all class methods used 4-space indentation instead of 8 spaces for method bodies. Fixed 18 methods including:
- Optimization methods (efficient_cvar_optimization, efficient_cdar_optimization, optimize_portfolio)
- Frontier generation (generate_efficient_frontier, _efficient_frontier_optimization)
- Analysis methods (backtest_strategy, risk_decomposition, sensitivity_analysis)
- Plotting methods (plot_weights, plot_efficient_frontier, plot_risk_decomposition, etc.)
- Utility methods (generate_report, save_report, export_weights)

### Import Path Correction
- **Old (incorrect)**: `from pypfopt.utils import portfolio_performance`
- **New (correct)**: `from pypfopt.base_optimizer import portfolio_performance`
- The `pypfopt.utils` module does not exist in PyPortfolioOpt v1.5.6

### Dependency Installation
Added to `src-tauri/resources/requirements.txt`:
```
PyPortfolioOpt==1.5.6
```

**Verification**: All tests pass, module imports successfully, full functionality confirmed.

See `docs/PORTFOLIO_OPTIMIZATION_FIX_SUMMARY.md` for complete details.

---
**Last Updated**: 2026-01-01
**Author**: Development Team
**Related Files**:
- `src-tauri/src/python_runtime.rs` (PyO3 execution logic)
- `src-tauri/resources/scripts/Analytics/skfolio_wrapper.py` (PyO3 fix example)
- `src-tauri/resources/scripts/Analytics/pyportfolioopt_wrapper/core.py` (Indentation fix example)
- `docs/PORTFOLIO_OPTIMIZATION_FIX_SUMMARY.md` (Complete fix documentation)
