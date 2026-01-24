# Backtesting System Documentation

Complete guide to the Fincept Terminal backtesting system architecture, implemented providers, and how to add new backtesting libraries.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Implemented Providers](#implemented-providers)
4. [Adding a New Provider](#adding-a-new-provider)
5. [Common Pitfalls & Solutions](#common-pitfalls--solutions)
6. [Testing Checklist](#testing-checklist)
7. [Best Practices](#best-practices)

---

## Overview

The Fincept Terminal backtesting system is a **provider-agnostic** architecture that allows integration of multiple backtesting engines through a unified interface. Users can switch between different providers (Backtesting.py, QuantConnect Lean, VectorBT, etc.) seamlessly.

### Key Features

- ✅ **Provider Registry**: Central management of all backtesting engines
- ✅ **Unified Interface**: Consistent API across all providers
- ✅ **Hot-Swapping**: Switch between providers without disconnecting others
- ✅ **Type Safety**: Full TypeScript + Python type hints
- ✅ **Error Handling**: Robust error propagation and logging

### Tech Stack

- **Frontend**: TypeScript + React (Tauri)
- **Backend**: Rust (Tauri commands)
- **Providers**: Python 3.11+ (backtesting engines)

---

## Architecture

### System Flow

```
┌─────────────────┐
│  React UI       │ (BacktestingTab.tsx)
│  TypeScript     │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Provider       │ (BacktestingProviderRegistry.ts)
│  Registry       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  TypeScript     │ (BacktestingPyAdapter.ts, etc.)
│  Adapters       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Rust Commands  │ (backtesting.rs)
│  Tauri IPC      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Python         │ (backtestingpy_provider.py, etc.)
│  Providers      │
└─────────────────┘
```

### Directory Structure

```
src-tauri/resources/scripts/Analytics/backtesting/
├── README.md                           # This file
├── base/
│   └── base_provider.py               # Base class for all providers
├── backtestingpy/
│   └── backtestingpy_provider.py      # Backtesting.py implementation
├── lean/
│   └── lean_provider.py               # QuantConnect Lean implementation
└── vectorbt/
    └── vectorbt_provider.py           # VectorBT implementation

src/services/backtesting/
├── BacktestingProviderRegistry.ts     # Provider registry
├── interfaces/
│   ├── IBacktestingProvider.ts        # TypeScript interface
│   └── types.ts                       # TypeScript types
└── adapters/
    ├── backtestingpy/
    │   └── BacktestingPyAdapter.ts    # TypeScript adapter
    ├── lean/
    │   └── LeanAdapter.ts
    └── vectorbt/
        └── VectorBTAdapter.ts

src-tauri/src/commands/
└── backtesting.rs                     # Rust Tauri commands
```

---

## Implemented Providers

### 1. Backtesting.py

**Status**: ✅ Fully Implemented

**Description**: Lightweight, fast event-driven backtesting library with Python API.

**Capabilities**:
- Event-driven bar-by-bar processing
- Custom Python strategies
- SMA, EMA, RSI indicators
- Parameter optimization
- Commission, slippage, margin support

**Python Dependencies**:
```bash
pip install backtesting==0.6.4
```

**Files**:
- Python: `backtestingpy/backtestingpy_provider.py`
- TypeScript: `adapters/backtestingpy/BacktestingPyAdapter.ts`

**Configuration**: No configuration required (uses synthetic data for testing)

---

### 2. QuantConnect Lean

**Status**: ⚠️ Partial Implementation

**Description**: Institutional-grade backtesting engine (C# based).

**Capabilities**:
- C# strategy development
- Real market data integration
- Options, futures, forex support
- Live trading ready

**Files**:
- Python: `lean/lean_provider.py`
- TypeScript: `adapters/lean/LeanAdapter.ts`

**Configuration Required**:
```json
{
  "leanCliPath": "/path/to/lean",
  "projectsPath": "/path/to/projects",
  "dataPath": "/path/to/data"
}
```

---

### 3. VectorBT

**Status**: ⚠️ Partial Implementation

**Description**: Ultra-fast vectorized backtesting using NumPy/Pandas.

**Capabilities**:
- Vectorized operations (fastest)
- Portfolio optimization
- Large-scale backtesting
- Custom indicators

**Files**:
- Python: `vectorbt/vectorbt_provider.py`
- TypeScript: `adapters/vectorbt/VectorBTAdapter.ts`

---

### 4. Fast-Trade

**Status**: ✅ Fully Implemented

**Description**: Fast, low-code backtesting library with JSON-based strategy definition and 80+ FINTA technical indicators.

**Capabilities**:
- JSON-based strategy logic (no coding required for basic strategies)
- 80+ technical indicators via FINTA library
- Very fast execution (50+ concurrent backtests)
- Trailing stop loss support
- Conditional logic operators (>, <, =, >=, <=)
- Lookback period confirmation
- Multiple timeframe support (1Min, 5Min, 1H, 1D, etc.)

**Python Dependencies**:
```bash
pip install fast-trade==1.0.2
```

**Files**:
- Python: `fasttrade/fasttrade_provider.py`
- TypeScript: `adapters/fasttrade/FastTradeAdapter.ts`

**Configuration**: No configuration required (uses synthetic data for testing)

**Strategy Types Supported**:
- SMA Crossover
- RSI (Relative Strength Index)
- Custom JSON-based logic

**Example Strategy Parameters**:
```json
{
  "type": "sma_crossover",
  "parameters": {
    "fastPeriod": 9,
    "slowPeriod": 21,
    "trailingStopLoss": 0.05
  }
}
```

**Key Features**:
- No Python coding required for basic strategies
- JSON-based entry/exit logic
- Extensive indicator library (SMA, EMA, RSI, MACD, Bollinger Bands, etc.)
- Fast execution with minimal overhead
- Ideal for rapid strategy iteration

---

## Adding a New Provider

Follow these steps to add a new backtesting library (e.g., "Backtrader", "Zipline", etc.)

### Step 1: Create Python Provider

Create: `src-tauri/resources/scripts/Analytics/backtesting/<provider_name>/<provider_name>_provider.py`

```python
"""
<Provider Name> Provider Implementation
"""

import sys
import json
from typing import Dict, Any
from pathlib import Path

# Import base provider
sys.path.append(str(Path(__file__).parent.parent))
from base.base_provider import (
    BacktestingProviderBase,
    BacktestResult,
    PerformanceMetrics,
    Trade,
    EquityPoint,
    BacktestStatistics,
    json_response,
    parse_json_input
)


class MyProviderProvider(BacktestingProviderBase):
    """
    <Provider Name> Provider

    Description of what this provider does.
    """

    def __init__(self):
        super().__init__()
        self.provider_module = None

    @property
    def name(self) -> str:
        return '<Provider Name>'

    @property
    def version(self) -> str:
        return '1.0.0'

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'liveTrading': False,
            'research': True,
            'multiAsset': ['stocks', 'options', 'crypto', 'forex'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 10,
            'supportedTimeframes': ['tick', 'minute', 'hour', 'daily'],
            'supportedMarkets': ['us-equity', 'crypto', 'forex']
        }

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        """Initialize the provider"""
        try:
            # Try to import the provider library
            import <provider_module> as provider_lib

            self.provider_module = provider_lib
            self.initialized = True
            self.connected = True

            return self._create_success_result(
                f'{self.name} {self.version} initialized'
            )

        except ImportError as e:
            return self._create_error_result(
                f'{self.name} not installed. Run: pip install <package_name>'
            )
        except Exception as e:
            return self._create_error_result(f'Initialization failed: {str(e)}')

    def test_connection(self) -> Dict[str, Any]:
        """Test connection to provider"""
        self._ensure_initialized()

        try:
            # Test provider availability
            if self.provider_module is None:
                raise RuntimeError('Provider not initialized')

            return self._create_success_result(
                f'{self.name} is available'
            )

        except Exception as e:
            return self._create_error_result(str(e))

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run backtest"""
        self._ensure_connected()

        try:
            # Extract parameters
            strategy_def = request.get('strategy', {})
            start_date = request.get('startDate')
            end_date = request.get('endDate')
            initial_capital = request.get('initialCapital', 10000)
            assets = request.get('assets', [])

            # TODO: Implement your backtest logic here
            # 1. Load/fetch data
            # 2. Create strategy
            # 3. Run backtest
            # 4. Extract results

            # Build result using BacktestResult dataclass
            result = BacktestResult(
                id=self._generate_id(),
                status='completed',
                performance=PerformanceMetrics(...),  # Fill metrics
                trades=[],  # List of Trade objects
                equity=[],  # List of EquityPoint objects
                statistics=BacktestStatistics(...),  # Fill statistics
                logs=[],
                error=None,
                start_time=self._current_timestamp(),
                end_time=self._current_timestamp(),
                duration=0,
                charts=None
            )

            return {
                'success': True,
                'message': 'Backtest completed',
                'data': result.to_dict()
            }

        except Exception as e:
            return self._create_error_result(f'Backtest failed: {str(e)}')

    def get_historical_data(self, request: Dict[str, Any]):
        """Get historical data"""
        # Implement if provider supports data fetching
        raise NotImplementedError('Historical data not supported')

    def calculate_indicator(self, indicator_type: str, params: Dict[str, Any]):
        """Calculate indicator"""
        # Implement if provider supports indicators
        raise NotImplementedError('Indicators not supported')


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    """Main entry point for CLI execution"""
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: python <provider>_provider.py <command> <json_args>'
        }))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]

    try:
        args = parse_json_input(json_args)
        provider = MyProviderProvider()

        if command == 'initialize':
            result = provider.initialize(args)
        elif command == 'test_connection':
            result = provider.test_connection()
        elif command == 'run_backtest':
            result = provider.run_backtest(args)
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
        elif command == 'calculate_indicator':
            indicator_type = args.get('type', '')
            params = args.get('params', {})
            result = provider.calculate_indicator(indicator_type, params)
        elif command == 'optimize':
            result = provider.optimize(args)
        elif command == 'disconnect':
            provider.disconnect()
            result = {'success': True, 'message': 'Disconnected'}
        else:
            result = {'success': False, 'error': f'Unknown command: {command}'}

        print(json_response(result))

    except Exception as e:
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))
        sys.exit(1)


if __name__ == '__main__':
    main()
```

**⚠️ CRITICAL: Use `json_response()` for ALL output**

The `json_response()` helper automatically:
- Converts snake_case to camelCase (Python → TypeScript compatibility)
- Handles JSON serialization with `default=str`
- Ensures consistent output format

❌ **WRONG**:
```python
print(json.dumps(result))  # Will output snake_case keys!
```

✅ **CORRECT**:
```python
print(json_response(result))  # Converts to camelCase automatically
```

---

### Step 2: Create TypeScript Adapter

Create: `src/services/backtesting/adapters/<provider_name>/<ProviderName>Adapter.ts`

```typescript
/**
 * <Provider Name> Adapter
 */

import { invoke } from '@tauri-apps/api/core';
import { BaseBacktestingAdapter } from '../BaseAdapter';
import {
  ProviderCapabilities,
  ProviderConfig,
  InitResult,
  TestResult,
  BacktestRequest,
  BacktestResult,
  BacktestStatus,
} from '../../interfaces/types';

const PROVIDER_CAPABILITIES: ProviderCapabilities = {
  backtesting: true,
  optimization: true,
  liveTrading: false,
  research: true,
  multiAsset: ['stocks', 'crypto'],
  indicators: true,
  customStrategies: true,
  maxConcurrentBacktests: 10,
  supportedTimeframes: ['minute', 'hour', 'daily'],
  supportedMarkets: ['us-equity', 'crypto']
};

export class MyProviderAdapter extends BaseBacktestingAdapter {
  readonly name = '<Provider Name>';
  readonly version = '1.0.0';
  readonly capabilities = PROVIDER_CAPABILITIES;
  readonly description = 'Description of the provider';

  private activeBacktests: Map<string, BacktestStatus> = new Map();

  /**
   * Extract JSON from Python output
   * CRITICAL: Handles log messages mixed with JSON
   */
  private extractJSON(stdout: string): any {
    const lines = stdout.split('\n');
    let jsonText = '';
    let inJson = false;
    let braceCount = 0;
    let lastValidJson: any = null;

    for (const line of lines) {
      const trimmed = line.trim();

      if (trimmed.startsWith('{')) {
        inJson = true;
        braceCount = 0;
        jsonText = '';
      }

      if (inJson) {
        jsonText += line + '\n';

        for (const char of line) {
          if (char === '{') braceCount++;
          if (char === '}') braceCount--;
        }

        if (braceCount === 0 && jsonText.trim().length > 0) {
          try {
            lastValidJson = JSON.parse(jsonText);
            jsonText = '';
            inJson = false;
          } catch (e) {
            jsonText = '';
            inJson = false;
          }
        }
      }
    }

    if (lastValidJson) {
      return lastValidJson;
    }

    throw new Error('No valid JSON found in output: ' + stdout.substring(0, 200));
  }

  async initialize(config: ProviderConfig): Promise<InitResult> {
    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: '<provider_name>',  // lowercase folder name
        command: 'initialize',
        args: JSON.stringify(config || {}),
      });

      const result = this.extractJSON(stdout) as {
        success: boolean;
        data?: any;
        error?: string;
        message?: string;
      };

      if (!result.success) {
        return this.createErrorResult(
          result.error || '<Provider> initialization failed'
        );
      }

      this.config = config;
      this.initialized = true;

      return this.createSuccessResult(
        result.message || '<Provider> initialized successfully'
      );
    } catch (error) {
      console.error('[<Provider>Adapter] Initialize error:', error);
      return this.createErrorResult(
        error instanceof Error ? error.message : String(error)
      );
    }
  }

  async testConnection(): Promise<TestResult> {
    this.ensureInitialized();

    try {
      const stdout = await invoke<string>('execute_python_backtest', {
        provider: '<provider_name>',
        command: 'test_connection',
        args: JSON.stringify({}),
      });

      const result = this.extractJSON(stdout);

      if (!result.success) {
        return {
          success: false,
          message: '<Provider> connection test failed',
          error: result.error,
        };
      }

      this.connected = true;

      return {
        success: true,
        message: result.message || '<Provider> is available',
        latency: 0,
      };
    } catch (error) {
      return {
        success: false,
        message: 'Connection test failed',
        error: String(error),
      };
    }
  }

  async disconnect(): Promise<void> {
    try {
      await invoke('execute_python_backtest', {
        provider: '<provider_name>',
        command: 'disconnect',
        args: JSON.stringify({}),
      });

      this.activeBacktests.clear();
      this.connected = false;
      this.initialized = false;
    } catch (error) {
      console.error('[<Provider>Adapter] Disconnect error:', error);
    }
  }

  async runBacktest(request: BacktestRequest): Promise<BacktestResult> {
    this.ensureConnected();

    const backtestId = this.generateBacktestId();

    try {
      this.activeBacktests.set(backtestId, {
        id: backtestId,
        status: 'running',
        progress: 0,
        message: 'Running backtest...',
      });

      const stdout = await invoke<string>('execute_python_backtest', {
        provider: '<provider_name>',
        command: 'run_backtest',
        args: JSON.stringify({
          id: backtestId,
          strategy: request.strategy,
          startDate: request.startDate,
          endDate: request.endDate,
          initialCapital: request.initialCapital,
          assets: request.assets,
        }),
      });

      const result = this.extractJSON(stdout);

      this.activeBacktests.delete(backtestId);

      if (!result.success) {
        throw new Error(result.error || 'Backtest failed');
      }

      if (!result.data) {
        throw new Error('No data returned');
      }

      return result.data as BacktestResult;
    } catch (error) {
      this.activeBacktests.delete(backtestId);
      console.error('[<Provider>Adapter] Backtest error:', error);
      throw error;
    }
  }

  protected generateBacktestId(): string {
    return `bt-<provider>-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

export const myProviderAdapter = new MyProviderAdapter();
```

**⚠️ CRITICAL Points**:

1. **Always use `extractJSON()`**: Python may output log messages before JSON
2. **Return LAST valid JSON**: Python may output multiple JSON objects
3. **Type responses properly**: Use `as { success: boolean; data?: any; ... }`
4. **Handle undefined safely**: Use optional chaining and nullish coalescing

---

### Step 3: Register Provider

**File**: `src/services/backtesting/index.ts`

Add your provider registration:

```typescript
import { myProviderAdapter } from './adapters/myprovider/MyProviderAdapter';

// Register all providers
backtestingRegistry.registerProvider(myProviderAdapter);
```

---

### Step 4: Update UI (Optional Provider-Specific Features)

**File**: `src/components/tabs/BacktestingTabNew.tsx`

Add provider-specific configuration if needed:

```typescript
const resetConfigForProvider = (providerName: string) => {
  if (providerName === '<Provider Name>') {
    setConfig(prev => ({
      ...prev,
      strategyType: 'sma_crossover',
      parameters: { /* provider-specific defaults */ },
    }));
  }
};
```

---

## Common Pitfalls & Solutions

### 1. ❌ Double JSON Encoding

**Problem**: Python outputs quoted JSON string instead of raw JSON

```python
# WRONG
print(json.dumps(json_response(result)))
# Output: "{\"success\": true}"  <- Quoted string!
```

**Solution**: Use `json_response()` directly OR `json.dumps()` on raw data

```python
# CORRECT
print(json_response(result))
# Output: {"success": true}  <- Raw JSON
```

---

### 2. ❌ snake_case vs camelCase Mismatch

**Problem**: Python uses `total_return`, TypeScript expects `totalReturn`

```python
# Python outputs this
{"total_return": 0.05, "sharpe_ratio": 1.2}

# TypeScript expects this
{"totalReturn": 0.05, "sharpeRatio": 1.2}
```

**Solution**: The `json_response()` helper automatically converts snake_case → camelCase

```python
# Already handled by json_response()!
print(json_response(result))
```

---

### 3. ❌ Missing CLI Argument Handler

**Problem**: Script has test code in `if __name__ == '__main__'` that runs every time

```python
# WRONG
if __name__ == '__main__':
    provider = MyProvider()
    result = provider.initialize({})
    print(json.dumps(result))  # Outputs initialization EVERY time
```

**Solution**: Implement proper CLI argument handler

```python
# CORRECT
def main():
    if len(sys.argv) < 3:
        print(json_response({'success': False, 'error': 'Usage error'}))
        sys.exit(1)

    command = sys.argv[1]
    json_args = sys.argv[2]
    args = parse_json_input(json_args)
    provider = MyProvider()

    if command == 'initialize':
        result = provider.initialize(args)
    elif command == 'run_backtest':
        result = provider.run_backtest(args)
    # ... other commands

    print(json_response(result))

if __name__ == '__main__':
    main()
```

---

### 4. ❌ Python Script Path Resolution

**Problem**: Rust can't find Python script in development mode

**Solution**: Rust command already handles multi-path lookup:

```rust
// backtesting.rs handles this automatically
let possible_paths = vec![
    PathBuf::from(&script_relative_path), // Production
    PathBuf::from("src-tauri").join(&script_relative_path), // Development
    PathBuf::from("../src-tauri").join(&script_relative_path), // Alt dev
];
```

---

### 5. ❌ Log Messages Mixed with JSON

**Problem**: Python logs like `[Provider] Starting...` appear before JSON

```
[MyProvider] Initializing...
[MyProvider] Loading data...
{"success": true, "message": "Done"}
```

**Solution**: TypeScript `extractJSON()` helper filters out non-JSON lines

```typescript
private extractJSON(stdout: string): any {
  // Finds JSON objects by matching braces
  // Ignores log messages
  // Returns LAST valid JSON found
}
```

---

### 6. ❌ `.toFixed()` / `.toLocaleString()` on undefined

**Problem**: Performance metrics might be undefined, causing runtime errors

```typescript
// WRONG
const value = result.performance.sharpeRatio.toFixed(2);
// Error if sharpeRatio is undefined!
```

**Solution**: Use nullish coalescing

```typescript
// CORRECT
const value = (result.performance.sharpeRatio ?? 0).toFixed(2);
```

---

### 7. ❌ Provider Auto-Disconnect on Switch

**Problem**: Switching providers disconnects the previous one

**Solution**: Registry now keeps all providers initialized

```typescript
// BacktestingProviderRegistry.ts - Fixed
async switchProvider(from: string | null, to: string): Promise<void> {
  // Just switch - don't disconnect old provider
  await this.setActiveProvider(to);
}
```

---

## Testing Checklist

When adding a new provider, test these scenarios:

### Python Provider
- [ ] Script runs: `python provider.py initialize "{}"`
- [ ] Returns valid JSON (not quoted string)
- [ ] Uses camelCase keys (not snake_case)
- [ ] Handles all commands: `initialize`, `test_connection`, `run_backtest`
- [ ] Error messages are clear and actionable

### TypeScript Adapter
- [ ] Initializes successfully
- [ ] Tests connection
- [ ] Runs backtest and returns result
- [ ] Handles Python errors gracefully
- [ ] Logs useful debug information

### UI Integration
- [ ] Provider appears in dropdown
- [ ] Can activate provider
- [ ] Can switch between providers
- [ ] Previous providers stay initialized
- [ ] Results display correctly
- [ ] Error messages show in UI

### End-to-End
- [ ] Run backtest on SPY (or test symbol)
- [ ] Check console for errors
- [ ] Verify performance metrics display
- [ ] Verify trades list populates
- [ ] Test provider switching
- [ ] Test with different strategies

---

## Best Practices

### Python Provider Development

1. **Use Type Hints**: Full type annotations for all functions
2. **Inherit from Base**: Always extend `BacktestingProviderBase`
3. **Implement All Methods**: Even if you raise `NotImplementedError`
4. **Use json_response()**: For ALL JSON output
5. **Log to stderr**: Use `self._log()` and `self._error()` helpers
6. **Handle Imports**: Check if library is installed, provide install instructions

### TypeScript Adapter Development

1. **Extend BaseAdapter**: Use `BaseBacktestingAdapter` as base class
2. **Use extractJSON()**: Always parse Python output with this helper
3. **Type Everything**: No `any` types without good reason
4. **Handle Errors**: Try-catch with meaningful error messages
5. **Log Liberally**: Use `console.log` for debugging
6. **Safe Rendering**: Use `??` for undefined values in UI

### General

1. **Test Incrementally**: Test each layer (Python → Rust → TypeScript → UI)
2. **Read Logs**: Check browser console AND Tauri terminal output
3. **Follow Patterns**: Look at Backtesting.py implementation as reference
4. **Document**: Add comments for complex logic
5. **Version Lock**: Pin Python package versions in requirements

---

## Provider Comparison

| Feature | Backtesting.py | QuantConnect Lean | VectorBT | Fast-Trade |
|---------|---------------|-------------------|----------|------------|
| Language | Python | C# | Python | Python (JSON) |
| Speed | Fast | Medium | Ultra-fast | Very Fast |
| Ease of Use | ✅ Easy | ⚠️ Complex | ✅ Easy | ✅ Very Easy |
| Custom Strategies | Python code | C# code | Python | JSON logic |
| Live Trading | ❌ No | ✅ Yes | ❌ No | ❌ No |
| Data Sources | Limited | Extensive | Custom | Limited |
| Optimization | Grid search | ✅ Advanced | ✅ Advanced | ❌ Manual |
| Indicators | Basic | Extensive | Extensive | 80+ FINTA |
| Strategy Format | Python class | C# class | Python | JSON config |
| Learning Curve | Medium | High | Medium | Low |
| Status | ✅ Complete | ⚠️ Partial | ⚠️ Partial | ✅ Complete |

---

## Troubleshooting

### Python script doesn't execute

**Check**:
1. Python path in Rust command (`get_python_path()`)
2. Script file exists at expected path
3. Python dependencies installed
4. Script has execute permissions (Unix/Mac)

### JSON parsing errors

**Check**:
1. Python uses `json_response()` for output
2. TypeScript uses `extractJSON()` to parse
3. No `print()` statements outputting non-JSON
4. Proper CLI argument handling in Python

### Provider shows as disconnected

**Check**:
1. `initialize()` returns `success: true`
2. `test_connection()` returns `success: true`
3. Provider registered in `index.ts`
4. Database has provider entry with `is_active = 1`

### Results show all zeros

**Check**:
1. Python outputs camelCase keys (use `json_response()`)
2. TypeScript expects matching keys
3. `result.data` contains actual backtest result
4. UI safe-renders undefined values (`?? 0`)

---

## Support

For questions or issues:

1. Check this README first
2. Review Backtesting.py implementation (reference example)
3. Check browser console and Tauri terminal logs
4. Open GitHub issue with full error logs

---

**Last Updated**: 2026-01-23
**Maintainer**: Fincept Corporation
**License**: MIT
