# Python Output Standardization Guide

## Overview

This guide explains how to use the `fincept_output_standard.py` library to ensure all Python scripts output data in a consistent, parsable format.

## The Problem

With 1100+ Python scripts, we have various output formats:
- Raw JSON objects
- Arrays of dictionaries
- Pandas DataFrames
- Simple strings
- Nested multi-format outputs
- Direct print statements
- Inconsistent error handling

This makes frontend parsing difficult and requires custom parsers for every script.

## The Solution

The `fincept_output_standard.py` library provides:
- **Automatic type detection**: Intelligently detects DataFrame, dict, array, etc.
- **Consistent JSON structure**: All outputs follow the same schema
- **Rich metadata**: Includes timestamps, execution time, script name
- **Error standardization**: Consistent error format with tracebacks
- **Type preservation**: Maintains information about original data structure

## Standard Output Format

```json
{
  "success": true,
  "data": {
    "type": "table|dict|array|text|number|multi|matrix|timeseries|empty",
    "...": "type-specific fields"
  },
  "metadata": {
    "script": "script_name",
    "timestamp": "2026-02-11T05:18:12.469581Z",
    "output_type": "table",
    "version": "1.0.0",
    "execution_time_ms": 123.45
  },
  "error": null
}
```

### Data Types

#### 1. Table (DataFrame/Tabular Data)
```json
{
  "type": "table",
  "columns": ["Date", "Close", "Volume"],
  "rows": [
    {"Date": "2024-01-01", "Close": 150.25, "Volume": 1000000},
    {"Date": "2024-01-02", "Close": 152.50, "Volume": 1200000}
  ],
  "shape": [2, 3],
  "dtypes": {"Date": "object", "Close": "float64", "Volume": "int64"}
}
```

#### 2. Dictionary
```json
{
  "type": "dict",
  "value": {
    "active_return": 0.15,
    "sharpe_ratio": 1.2,
    "volatility": 0.18
  }
}
```

#### 3. Array
```json
{
  "type": "array",
  "items": [100, 110, 105, 115, 120],
  "length": 5
}
```

#### 4. Text
```json
{
  "type": "text",
  "value": "Analysis complete: Portfolio shows positive alpha",
  "length": 47
}
```

#### 5. Number
```json
{
  "type": "number",
  "value": 0.15
}
```

#### 6. Multi (Multiple outputs)
```json
{
  "type": "multi",
  "outputs": [
    {
      "name": "summary",
      "data": {"type": "dict", "value": {...}}
    },
    {
      "name": "details",
      "data": {"type": "table", "columns": [...], "rows": [...]}
    }
  ],
  "count": 2
}
```

## Usage

### Method 1: Simple Function Call (Recommended for New Scripts)

```python
from fincept_output_standard import print_standard_output

def main():
    # Your analysis logic
    result = {
        "return": 0.15,
        "volatility": 0.18,
        "sharpe_ratio": 0.83
    }

    # Standardize and print
    print_standard_output(result, script_name="my_analysis")

if __name__ == "__main__":
    main()
```

### Method 2: Using Decorator (Cleanest)

```python
from fincept_output_standard import wrap_script_execution

@wrap_script_execution
def main():
    # Your logic here
    data = perform_analysis()
    return data  # Automatically standardized

if __name__ == "__main__":
    main()
```

### Method 3: Manual Standardization (Most Control)

```python
from fincept_output_standard import FinceptOutputStandardizer
import json

def main():
    standardizer = FinceptOutputStandardizer(script_name="my_script")

    try:
        # Your logic
        result = compute_something()

        # Standardize
        output = standardizer.standardize(result)
        print(json.dumps(output))

    except Exception as e:
        # Standardized error
        error_output = standardizer._create_error_response(e)
        print(json.dumps(error_output))
        sys.exit(1)
```

### Method 4: Explicit Type Specification

```python
from fincept_output_standard import print_standard_output, OutputType

# Force specific type
print_standard_output(
    data,
    script_name="my_script",
    output_type=OutputType.TABLE  # Or DICT, ARRAY, MULTI, etc.
)
```

## Migration Strategy

### Phase 1: New Scripts (Immediate)
All new scripts should use the standardization library from day one.

### Phase 2: High-Priority Scripts (Week 1-2)
Migrate the most-used scripts:
- `get_historical_data.py`
- Portfolio analytics scripts
- Market data fetchers
- Common technical indicators

### Phase 3: Gradual Migration (Ongoing)
Migrate scripts as they're updated or when bugs are found.

### Phase 4: Rust-Level Interception (Parallel)
While migrating Python scripts, implement Rust-level parsing to handle legacy outputs.

## Examples by Use Case

### Example 1: Time Series Data

```python
import pandas as pd
from fincept_output_standard import print_standard_output

def get_price_history(symbol, period):
    # Fetch data
    df = yf.Ticker(symbol).history(period=period)

    # Standardize automatically detects DataFrame
    print_standard_output(df, script_name="price_history")
```

Output:
```json
{
  "success": true,
  "data": {
    "type": "table",
    "columns": ["Open", "High", "Low", "Close", "Volume"],
    "rows": [...],
    "shape": [100, 5]
  }
}
```

### Example 2: Portfolio Metrics

```python
from fincept_output_standard import print_standard_output

def calculate_metrics(portfolio_returns, benchmark_returns):
    metrics = {
        "total_return": calculate_return(portfolio_returns),
        "volatility": calculate_volatility(portfolio_returns),
        "sharpe_ratio": calculate_sharpe(portfolio_returns),
        "max_drawdown": calculate_drawdown(portfolio_returns),
        "alpha": calculate_alpha(portfolio_returns, benchmark_returns),
        "beta": calculate_beta(portfolio_returns, benchmark_returns)
    }

    print_standard_output(metrics, script_name="portfolio_metrics")
```

### Example 3: Multi-Output Analysis

```python
from fincept_output_standard import print_standard_output, OutputType

def comprehensive_analysis(data):
    result = {
        "summary_stats": {
            "mean": data.mean(),
            "std": data.std(),
            "min": data.min(),
            "max": data.max()
        },
        "time_series": data.tolist(),
        "recommendations": [
            "Increase equity exposure",
            "Reduce duration risk"
        ]
    }

    # Force multi-output type
    print_standard_output(
        result,
        script_name="comprehensive_analysis",
        output_type=OutputType.MULTI
    )
```

### Example 4: Error Handling

```python
from fincept_output_standard import FinceptOutputStandardizer
import json
import sys

def main():
    standardizer = FinceptOutputStandardizer(script_name="risky_operation")

    try:
        if len(sys.argv) < 2:
            raise ValueError("Symbol required")

        symbol = sys.argv[1]
        result = fetch_data(symbol)

        output = standardizer.standardize(result)
        print(json.dumps(output))

    except ValueError as e:
        error_output = standardizer._create_error_response(e)
        print(json.dumps(error_output))
        sys.exit(1)
```

Error output:
```json
{
  "success": false,
  "data": null,
  "error": {
    "message": "Symbol required",
    "type": "ValueError",
    "traceback": "Traceback (most recent call last):\n  ..."
  },
  "metadata": {
    "script": "risky_operation",
    "output_type": "error"
  }
}
```

## Testing

Run the test suite:
```bash
cd fincept-cpp/resources/scripts
python test_standardization.py
```

This will show examples of all output types.

## Frontend Integration

### TypeScript Interface

```typescript
interface StandardPythonOutput<T = any> {
  success: boolean;
  data: {
    type: 'table' | 'dict' | 'array' | 'text' | 'number' | 'multi' | 'matrix' | 'timeseries' | 'empty';
    // Type-specific fields
  } | null;
  metadata?: {
    script: string;
    timestamp: string;
    output_type: string;
    version: string;
    execution_time_ms: number;
  };
  error?: {
    message: string;
    type: string;
    traceback: string;
  } | null;
}

interface TableData {
  type: 'table';
  columns: string[];
  rows: Record<string, any>[];
  shape: [number, number];
  dtypes?: Record<string, string>;
}

interface DictData {
  type: 'dict';
  value: Record<string, any>;
}

interface ArrayData {
  type: 'array';
  items: any[];
  length: number;
}
```

### Universal Parser

```typescript
function parsePythonOutput(rawOutput: string): StandardPythonOutput {
  try {
    const parsed = JSON.parse(rawOutput);

    // Check if already standardized
    if (parsed.success !== undefined && parsed.data !== undefined) {
      return parsed;
    }

    // Legacy format - needs manual handling or Rust-level standardization
    return {
      success: true,
      data: {
        type: 'dict',
        value: parsed
      },
      metadata: {
        script: 'unknown',
        timestamp: new Date().toISOString(),
        output_type: 'dict',
        version: 'legacy'
      }
    };
  } catch (e) {
    return {
      success: false,
      data: null,
      error: {
        message: 'Failed to parse Python output',
        type: 'ParseError',
        traceback: rawOutput
      }
    };
  }
}
```

## Best Practices

1. **Always use the library for new scripts**
2. **Specify script names clearly** - use module path or descriptive name
3. **Let auto-detection work** - only specify `output_type` when necessary
4. **Include execution context** - metadata helps with debugging
5. **Handle errors consistently** - use the standardizer's error response
6. **Test outputs** - verify JSON is valid and parsable
7. **Document complex outputs** - add comments for multi-output structures

## Common Pitfalls

### ❌ Don't do this:
```python
# Direct print of mixed formats
print(json.dumps({"result": result}))
print(f"Analysis complete: {summary}")
print(dataframe.to_json())
```

### ✅ Do this instead:
```python
from fincept_output_standard import print_standard_output

# Single standardized output
result = {
    "analysis": result,
    "summary": summary,
    "data": dataframe
}
print_standard_output(result, script_name="my_analysis")
```

## FAQ

**Q: What about existing scripts that users depend on?**
A: Use Rust-level interception to standardize their output without breaking changes.

**Q: Does this add significant overhead?**
A: Minimal - typically < 10ms for most outputs. Metadata includes execution_time_ms.

**Q: Can I customize the output format?**
A: Yes, but standardization is the goal. For custom needs, use OutputType.MULTI.

**Q: What if I need to output progress updates?**
A: Use logging to stderr, final result to stdout. Or use multi-output with progress field.

**Q: How do I handle very large DataFrames?**
A: Consider pagination or summary statistics. The library handles serialization but large JSON can be slow.

## Support

For questions or issues with standardization:
1. Check this guide
2. Run `test_standardization.py` for examples
3. Review `fincept_output_standard.py` source code
4. Ask in team chat or create an issue

---

**Version**: 1.0.0
**Last Updated**: 2026-02-11
**Maintainer**: Fincept Terminal Team
