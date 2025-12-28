# Rateslib Wrapper for Fincept Terminal

A comprehensive Python wrapper for the [rateslib](https://github.com/attack68/rateslib) fixed income library, providing CFA-level analytics for interest rate derivatives, bonds, FX products, and more.

## Overview

This wrapper integrates rateslib's sophisticated fixed income analysis capabilities into Fincept Terminal's Python analytics environment. It provides high-level, easy-to-use interfaces for:

- **Interest Rate Swaps** (IRS, XCS, SBS, ZCIS, IIRS)
- **Fixed Income Bonds** (Fixed rate, Floating rate, Index-linked, Bills)
- **FX Products** (Spot, Forwards, Swaps, Options, Strategies)
- **Derivatives** (STIR Futures, Bond Futures, Portfolios)
- **Scheduling Utilities** (Calendars, Date calculations, Schedules)

## Installation

The rateslib library is already installed in the Fincept Terminal Python environment:

```bash
# Installation was done via:
cd src-tauri
uv pip install rateslib==2.1.1 --python target/debug/python/python.exe
```

Version: **rateslib 2.1.1**

## Modules

### 1. Swaps Analytics (`swaps_analytics.py`)

**Purpose:** Interest rate swap pricing, analytics, and risk management.

**Supported Instruments:**
- IRS (Interest Rate Swap)
- XCS (Cross-Currency Swap)
- FRA (Forward Rate Agreement)
- SBS (Single Currency Basis Swap)
- ZCIS (Zero Coupon Inflation Swap)

**Key Features:**
- Swap creation and configuration
- NPV calculation
- Cash flow generation
- Risk sensitivities (delta, gamma)
- Plotly visualizations
- JSON export

**Example Usage:**
```python
from rateslib_wrapper.swaps_analytics import SwapsAnalytics, SwapConfig
import rateslib as rl

# Configure
config = SwapConfig(notional=10_000_000, currency="USD")
analytics = SwapsAnalytics(config)

# Create 5Y IRS paying fixed 5%
irs = analytics.create_irs(
    effective_date=rl.dt(2024, 1, 15),
    termination="5Y",
    fixed_rate=5.0,
    pay_or_receive="pay"
)

# Get cashflows
cashflows = analytics.get_cashflows()

# Calculate NPV (requires curve)
npv = analytics.calculate_npv(curve)

# Plot cashflows
fig = analytics.plot_cashflows()
```

---

### 2. Bonds Analytics (`bonds_analytics.py`)

**Purpose:** Bond pricing, yield calculations, and duration/convexity analysis.

**Supported Instruments:**
- FixedRateBond
- FloatRateNote (FRN)
- IndexFixedRateBond
- Bill (zero-coupon)

**Key Features:**
- Bond pricing from yield or curve
- Yield to maturity calculations
- Duration and convexity metrics
- Accrued interest calculations
- Cash flow schedules
- Ex-dividend date handling

**Example Usage:**
```python
from rateslib_wrapper.bonds_analytics import BondsAnalytics, BondConfig
import rateslib as rl

# Configure
config = BondConfig(currency="USD", convention="ActActICMA")
analytics = BondsAnalytics(config)

# Create 10Y bond with 5% coupon
bond = analytics.create_fixed_rate_bond(
    effective_date=rl.dt(2020, 1, 15),
    termination="10Y",
    coupon_rate=5.0,
    frequency="S",  # Semi-annual
    notional=100
)

# Calculate accrued interest
settlement = rl.dt(2024, 6, 15)
accrued = analytics.calculate_accrued_interest(settlement)

# Get duration and convexity
duration = analytics.calculate_duration(curve, settlement)
convexity = analytics.calculate_convexity(curve, settlement)

# Export to JSON
json_data = analytics.export_to_json()
```

---

### 3. FX Analytics (`fx_analytics.py`)

**Purpose:** FX spot, forward, swap, and option pricing and analytics.

**Supported Instruments:**
- FXRates (spot rates)
- FXForwards (forward curves)
- FXExchange (spot/forward exchange)
- FXSwap (near + far legs)
- FXCall, FXPut (vanilla options)
- FXStraddle, FXStrangle, FXRiskReversal (option strategies)
- FXBrokerFly (butterfly strategy)

**Key Features:**
- FX rate management
- Forward pricing
- Option pricing (Black-Scholes)
- Greeks calculation (delta, gamma, vega)
- Option strategy payoff diagrams
- Currency conversion

**Example Usage:**
```python
from rateslib_wrapper.fx_analytics import FXAnalytics, FXConfig
import rateslib as rl

# Configure
config = FXConfig()
analytics = FXAnalytics(config)

# Create FX rates
rates = analytics.create_fx_rates({
    ("USD", "EUR"): 0.92,
    ("USD", "GBP"): 0.79,
    ("USD", "JPY"): 149.50
})

# Create FX call option
call = analytics.create_fx_call(
    pair=("EUR", "USD"),
    expiry=rl.dt(2024, 12, 31),
    strike=1.10,
    notional=1_000_000
)

# Calculate option value and greeks
npv = analytics.calculate_npv()
greeks = analytics.calculate_greeks()

# Plot payoff diagram
fig = analytics.plot_option_payoff(spot_range=(1.00, 1.20))
```

---

### 4. Derivatives Analytics (`derivatives_analytics.py`)

**Purpose:** Futures contracts and portfolio analytics.

**Supported Instruments:**
- STIRFuture (Short-Term Interest Rate Futures)
- BondFuture
- Portfolio (instrument aggregation)
- Value (valuation wrapper)

**Key Features:**
- STIR future pricing
- Bond future CTD (cheapest-to-deliver) analysis
- Portfolio construction
- Aggregated NPV and sensitivities
- Conversion factor calculations

**Example Usage:**
```python
from rateslib_wrapper.derivatives_analytics import DerivativesAnalytics, DerivativesConfig
import rateslib as rl

# Configure
config = DerivativesConfig()
analytics = DerivativesAnalytics(config)

# Create STIR future
stir = analytics.create_stir_future(
    effective_date=rl.dt(2024, 3, 15),
    termination="3M",
    price=95.50  # Implies 4.50% rate
)

# Create bond future
bond_future = analytics.create_bond_future(
    delivery_date=rl.dt(2024, 12, 15),
    coupon=6.0,
    basket=[
        {"bond": bond1, "conversion_factor": 0.9523},
        {"bond": bond2, "conversion_factor": 1.0234}
    ]
)

# Calculate NPV
npv = analytics.calculate_npv()

# Get sensitivities
delta = analytics.calculate_delta()
```

---

### 5. Scheduling Analytics (`scheduling_analytics.py`)

**Purpose:** Date scheduling, calendar management, and day count conventions.

**Supported Utilities:**
- Schedule (payment schedule generation)
- add_tenor (date arithmetic)
- dcf (day count fraction)
- Cal (business day calendars)
- Frequency (payment frequency handling)
- get_calendar (named calendar retrieval)

**Key Features:**
- Payment schedule generation
- Business day adjustments
- Tenor calculations
- Day count conventions (Act360, Act365F, 30E360, etc.)
- Calendar merging and customization
- Stub period handling

**Example Usage:**
```python
from rateslib_wrapper.scheduling_analytics import SchedulingAnalytics, SchedulingConfig
import rateslib as rl

# Configure
config = SchedulingConfig(calendar="nyc", convention="Act360")
analytics = SchedulingAnalytics(config)

# Create schedule
schedule = analytics.create_schedule(
    effective_date=rl.dt(2024, 1, 15),
    termination="5Y",
    frequency="Q",  # Quarterly
    stub="ShortFront"
)

# Add tenor to date
new_date = analytics.add_tenor_to_date(
    start_date=rl.dt(2024, 1, 15),
    tenor="3M",
    modifier="MF"  # Modified Following
)

# Calculate day count fraction
dcf_value = analytics.calculate_dcf(
    start_date=rl.dt(2024, 1, 15),
    end_date=rl.dt(2024, 7, 15),
    convention="Act360"
)

# Check if business day
is_bus_day = analytics.is_business_day(rl.dt(2024, 7, 4))

# Plot schedule
fig = analytics.plot_schedule()
```

---

## Configuration Classes

Each module uses a `@dataclass` config class for type-safe configuration:

### SwapConfig
```python
@dataclass
class SwapConfig:
    notional: float = 1_000_000
    currency: str = "USD"
    frequency_fixed: str = "S"  # Semi-annual
    frequency_float: str = "Q"  # Quarterly
    calendar: str = "nyc"
    convention_fixed: str = "30e360"
    convention_float: str = "Act360"
```

### BondConfig
```python
@dataclass
class BondConfig:
    currency: str = "USD"
    calendar: str = "nyc"
    convention: str = "ActActICMA"
    ex_div_days: int = 7
    settle_days: int = 2
```

### FXConfig
```python
@dataclass
class FXConfig:
    base_currency: str = "USD"
    settlement_days: int = 2
    calendar: str = "nyc"
```

### DerivativesConfig
```python
@dataclass
class DerivativesConfig:
    currency: str = "USD"
    calendar: str = "nyc"
    contracts: int = 1
    tick_value: float = 25.0
```

### SchedulingConfig
```python
@dataclass
class SchedulingConfig:
    calendar: str = "nyc"
    convention: str = "Act360"
    modifier: str = "MF"  # Modified Following
    eom: bool = False  # End of month
```

---

## Common Methods

All analytics classes share common methods:

### Calculation Methods
- `calculate_npv(curve=None)` - Net present value
- `calculate_sensitivities(curve=None)` - Delta, gamma, vega
- `get_cashflows()` - Cash flow schedule as DataFrame

### Visualization Methods
- `plot_cashflows()` - Cash flow bar chart
- `plot_*()` - Instrument-specific plots

### Export Methods
- `export_to_json()` - Structured JSON export

---

## Testing

Each module includes a `main()` function with comprehensive tests. Run tests:

```bash
# Test individual modules
cd src-tauri
./target/debug/python/python.exe -c "
import sys
sys.path.insert(0, 'target/debug/python/Lib/site-packages')
sys.path.insert(0, 'resources/scripts/Analytics')
exec(open('resources/scripts/Analytics/rateslib_wrapper/swaps_analytics.py').read())
"

# Test all modules
for module in swaps_analytics bonds_analytics fx_analytics derivatives_analytics scheduling_analytics; do
    echo "Testing $module..."
    ./target/debug/python/python.exe -c "
import sys
sys.path.insert(0, 'target/debug/python/Lib/site-packages')
sys.path.insert(0, 'resources/scripts/Analytics')
exec(open('resources/scripts/Analytics/rateslib_wrapper/${module}.py').read())
"
done
```

---

## Rateslib Coverage

### Classes Covered (85 total)

**Curves (9):**
- Curve, LineCurve, CompositeCurve, ProxyCurve, MultiCsaCurve, RolledCurve, ShiftedCurve, TranslatedCurve, CreditImpliedCurve

**Instruments (36):**
- IRS, XCS, SBS, ZCIS, ZCS, IIRS, FRA
- FixedRateBond, FloatRateNote, IndexFixedRateBond, Bill
- FXCall, FXPut, FXExchange, FXSwap, FXStraddle, FXRiskReversal, FXStrangle, FXBrokerFly
- STIRFuture, BondFuture, CDS
- Portfolio, Value, VolValue, Spread, Fly
- (and more...)

**FX (6):**
- FXRates, FXForwards, FXDeltaVolSmile, FXDeltaVolSurface, FXSabrSmile, FXSabrSurface

**Scheduling (9):**
- Schedule, Cal, NamedCal, UnionCal, Frequency, Adjuster, RollDay, Imm, StubInference

**Legs & Periods (27):**
- FixedLeg, FloatLeg, IndexFixedLeg, ZeroFixedLeg, ZeroFloatLeg, ZeroIndexLeg
- FixedPeriod, FloatPeriod, IndexFixedPeriod, IndexCashflow, Cashflow
- CreditPremiumLeg, CreditProtectionLeg
- (and more...)

**Dual Numbers (4):**
- Dual, Dual2, Variable, ADOrder

**Splines (3):**
- PPSplineF64, PPSplineDual, PPSplineDual2

**Solver (1):**
- Solver

### Functions Covered (14 total)

- add_tenor, dcf, get_calendar, get_imm, next_imm
- dual_exp, dual_log, dual_solve, gradient
- bspldnev_single, bsplev_single
- index_left, index_value, from_json

---

## Integration with Fincept Terminal

### From Rust Backend

Use Tauri commands to invoke Python analytics:

```rust
#[tauri::command]
async fn run_swap_analytics(
    effective: String,
    termination: String,
    rate: f64
) -> Result<String, String> {
    // Execute Python script
    let output = Command::new("./target/debug/python/python.exe")
        .args(&[
            "-c",
            &format!(
                "import sys; \
                 sys.path.insert(0, 'target/debug/python/Lib/site-packages'); \
                 sys.path.insert(0, 'resources/scripts/Analytics'); \
                 from rateslib_wrapper.swaps_analytics import SwapsAnalytics; \
                 analytics = SwapsAnalytics(); \
                 result = analytics.create_irs('{}', '{}', {}); \
                 print(analytics.export_to_json())",
                effective, termination, rate
            )
        ])
        .output()
        .map_err(|e| e.to_string())?;

    String::from_utf8(output.stdout).map_err(|e| e.to_string())
}
```

### From TypeScript Frontend

```typescript
import { invoke } from '@tauri-apps/api/core';

async function analyzeSwap() {
    const result = await invoke('run_swap_analytics', {
        effective: '2024-01-15',
        termination: '5Y',
        rate: 5.0
    });

    console.log(JSON.parse(result));
}
```

---

## Performance Considerations

- **Curve Construction:** Most expensive operation, cache curves when possible
- **NPV Calculations:** Require discount curves, pre-build curves for batch operations
- **Sensitivities:** Use analytic methods when available (faster than numerical)
- **Plotting:** Generate plots on-demand, not in batch processing

---

## Error Handling

All modules use try-except blocks with graceful degradation:

```python
try:
    result = analytics.calculate_npv(curve)
except Exception as e:
    print(f"Error calculating NPV: {e}")
    result = {'npv': 0.0, 'error': str(e)}
```

Errors return structured dictionaries with `'error'` key.

---

## Dependencies

- **rateslib >= 2.1.1** - Core fixed income library
- **pandas >= 2.3.3** - DataFrames
- **numpy >= 1.26.4** - Numerical operations
- **plotly >= 6.5.0** - Visualizations
- **dataclasses** - Config classes (built-in Python 3.7+)
- **typing** - Type hints (built-in)
- **datetime** - Date handling (built-in)
- **json** - JSON export (built-in)

---

## License & Attribution

**Wrapper Code:** MIT License (Fincept Corporation)

**Rateslib Library:** Creative Commons Attribution, Non-Commercial, No-Derivatives 4.0 International License
- Free for academic and personal educational use
- Commercial applications require license extension
- See: https://github.com/attack68/rateslib

---

## Contributing

To add new rateslib functionality:

1. Follow existing module patterns
2. Use @dataclass for configs
3. Add type hints and docstrings
4. Include error handling
5. Add main() test function
6. Update this README
7. Test with embedded Python runtime

---

## References

- **Rateslib Documentation:** https://rateslib.com/py/
- **GitHub Repository:** https://github.com/attack68/rateslib
- **Book:** *Coding Interest Rates: FX, Swaps and Bonds* by J.H.M Darbyshire
- **Integration Guide:** `../LIBRARY_INTEGRATION_GUIDE.md`

---

## Support

For issues with:
- **Wrapper code:** Create issue in Fincept Terminal repository
- **Rateslib library:** See https://github.com/attack68/rateslib/issues

---

**Version:** 1.0.0
**Last Updated:** 2025-12-28
**Rateslib Version:** 2.1.1
**Maintainer:** Fincept Corporation
