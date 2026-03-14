# skfolio Backend System

A comprehensive backend system for portfolio optimization using skfolio, designed for integration with Tauri desktop applications.

## Overview

This backend provides a complete, modular portfolio optimization system built on the skfolio library. It exposes all skfolio capabilities through a clean, JSON-based API that can be easily integrated with your Tauri frontend.

## Architecture

The system is organized into 7 main modules:

### Core Modules

1. **skfolio_core.py** - Main Engine & Configuration
   - Central configuration management
   - Core optimization engine coordination
   - JSON serialization for frontend integration
   - Progress tracking and error handling
   - Model factory for dynamic optimization

2. **skfolio_optimization.py** - Optimization Models Interface
   - All optimization models (Mean-Risk, HRP, Risk Parity, etc.)
   - Dynamic model selection based on parameters
   - Hyperparameter tuning with grid/random search
   - Model comparison utilities
   - Efficient frontier generation

3. **skfolio_data.py** - Data Management & Preprocessing
   - Multi-source data ingestion (CSV, Excel, databases, APIs)
   - Data quality validation and cleaning
   - Missing data imputation strategies
   - Return calculation and frequency conversion
   - Factor data processing

4. **skfolio_risk.py** - Risk Analysis & Management
   - Complete suite of risk measures (VaR, CVaR, drawdown, etc.)
   - Risk attribution and decomposition
   - Stress testing and scenario analysis
   - Monte Carlo simulation with copulas
   - Risk budgeting and constraints

5. **skfolio_portfolio.py** - Portfolio Construction & Management
   - Portfolio construction workflows
   - Dynamic rebalancing strategies
   - Performance attribution analysis
   - Multi-period portfolio optimization
   - Portfolio monitoring and alerts

6. **skfolio_validation.py** - Model Validation & Testing
   - Multiple cross-validation strategies
   - Model selection and comparison
   - Hyperparameter tuning
   - Statistical significance testing
   - Overfitting detection

7. **skfolio_api.py** - Frontend Integration Layer
   - JSON-based RESTful API endpoints
   - Parameter validation and sanitization
   - Async task management for long computations
   - Progress tracking and callbacks
   - Comprehensive error handling

## Key Features

### Comprehensive Portfolio Optimization
- All major optimization methods (Mean-Risk, HRP, Risk Parity, etc.)
- Multiple risk measures (VaR, CVaR, drawdown, etc.)
- Advanced estimation techniques (shrinkage, denoising, etc.)
- Black-Litterman and factor models
- Uncertainty set optimization

### Professional Risk Management
- Coherent risk measures
- Stress testing and scenario analysis
- Monte Carlo simulation with copulas
- Risk attribution and budgeting
- Portfolio monitoring

### Advanced Validation
- Walk-forward analysis
- Combinatorial purged cross-validation
- Statistical significance testing
- Overfitting detection
- Model comparison and selection

### Easy Frontend Integration
- JSON-based API responses
- Parameter validation
- Async task processing
- Progress tracking
- Comprehensive error handling

## Usage Examples

### Basic Portfolio Optimization

```python
from skfolio_api import SkfolioAPI

# Initialize API
api = SkfolioAPI()

# Load data
data_params = {
    "source_type": "csv",
    "source_path": "prices.csv",
    "date_column": "date",
    "value_columns": ["close"]
}
load_result = api.load_data(data_params)

# Optimize portfolio
params = {
    "optimization_method": "mean_risk",
    "objective_function": "maximize_ratio",
    "risk_measure": "cvar",
    "train_test_split_ratio": 0.8
}

# Sample data format
sample_data = {
    "2020-01-01": [0.01, -0.02, 0.015],
    "2020-01-02": [0.02, 0.01, -0.01],
    "2020-01-03": [-0.01, 0.03, 0.02]
}

result = api.optimize_portfolio(sample_data, params)
```

### Risk Analysis

```python
# Calculate risk metrics
returns_data = [0.01, -0.02, 0.015, 0.005, -0.01]
risk_result = api.calculate_risk_metrics(returns_data)

# Stress testing
weights = [0.4, 0.3, 0.3]
assets = ["AAPL", "MSFT", "GOOG"]

scenarios = [
    {
        "name": "market_crash",
        "description": "Severe market downturn",
        "shocks": {"AAPL": -0.3, "MSFT": -0.25, "GOOG": -0.35}
    }
]

stress_result = api.stress_test_portfolio(weights, assets, returns_data, scenarios)
```

### Async Processing

```python
# Enable async for large datasets
async_result = api.optimize_portfolio(large_data, params, async_execution=True)

# Check task status
status = api.get_task_status(async_result.data["task_id"])
```

## Integration with Tauri

### 1. File Structure
Place the `skfolio_backend` folder in your Tauri resources directory:

```
src-tauri/
└── resources/
    └── scripts/
        └── skfolio_backend/
            ├── skfolio_core.py
            ├── skfolio_optimization.py
            ├── skfolio_data.py
            ├── skfolio_risk.py
            ├── skfolio_portfolio.py
            ├── skfolio_validation.py
            ├── skfolio_api.py
            └── README.md
```

### 2. Rust Integration
Use the embedded Python interpreter to call the API functions:

```rust
use tauri::Manager;
use std::process::Command;

fn optimize_portfolio(data: &str, params: &str) -> Result<String, String> {
    let output = Command::new("python")
        .arg("C:/path/to/tauri/resources/scripts/skfolio_backend/skfolio_api.py")
        .arg("optimize")
        .arg("--data")
        .arg(data)
        .arg("--params")
        .arg(params)
        .output()
        .map_err(|e| e.to_string())?;

    Ok(String::from_utf8_lossy(&output.stdout))
}
```

### 3. Data Flow

1. **Frontend** → JSON request → **Backend API** → Process → **JSON response** → **Frontend**
2. **Data Sources** → **Data Manager** → **Core Engine** → **Optimization** → **Results**

## Configuration

### Optimization Parameters

```json
{
  "optimization_method": "mean_risk",
  "objective_function": "maximize_ratio",
  "risk_measure": "cvar",
  "train_test_split_ratio": 0.7,
  "risk_aversion": 1.0,
  "l1_coef": 0.01,
  "l2_coef": 0.01,
  "confidence_level": 0.95,
  "covariance_estimator": "empirical",
  "mu_estimator": "empirical"
}
```

### Data Source Configuration

```json
{
  "source_type": "csv",
  "source_path": "path/to/prices.csv",
  "date_column": "date",
  "value_columns": ["close", "open", "high", "low"],
  "frequency": "daily"
}
```

### Stress Test Scenarios

```json
[
  {
    "name": "market_crash",
    "description": "Severe market downturn",
    "shocks": {"AAPL": -0.30, "MSFT": -0.25},
    "probability": 0.05
  },
  {
    "name": "volatility_spike",
    "description": "Extreme volatility increase",
    "volatility_changes": {"market": 2.0},
    "probability": 0.10
  }
]
```

## Dependencies

Required Python packages:
- skfolio
- pandas
- numpy
- scipy
- scikit-learn

Install with:
```bash
pip install skfolio pandas numpy scipy scikit-learn
```

## Error Handling

The API provides comprehensive error handling with standardized error codes:

- `INVALID_PARAMS` - Parameter validation failed
- `DATA_CONVERSION_ERROR` - Data format error
- `OPTIMIZATION_ERROR` - Optimization failed
- `RISK_METRICS_ERROR` - Risk calculation failed
- `STRESS_TEST_ERROR` - Stress test failed
- `TASK_NOT_FOUND` - Task ID not found
- `ASYNC_DISABLED` - Async operations not enabled

## Performance Considerations

### Async Processing
- Automatic async for large datasets (>1000 rows)
- Database and API sources default to async
- Configurable concurrent task limits
- Background thread processing

### Memory Management
- Efficient data structures
- Automatic cleanup of old tasks
- Resource monitoring
- Configurable cleanup intervals

### Optimization Performance
- Model caching for repeated operations
- Parallel processing where applicable
- Efficient numerical computations
- Optimized data structures

## Extensibility

The modular design makes it easy to extend functionality:

1. **Add new optimization models**: Modify `skfolio_optimization.py`
2. **Add data sources**: Extend `skfolio_data.py`
3. **Add risk measures**: Enhance `skfolio_risk.py`
4. **Add validation methods**: Expand `skfolio_validation.py`
5. **Custom API endpoints**: Extend `skfolio_api.py`

## Support

For issues, questions, or contributions, please refer to the skfolio documentation or create an issue in the project repository.