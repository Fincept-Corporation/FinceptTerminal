"""
"bt" engine of the Fincept backtesting backend (provider: bt_provider.py).

Allocation strategies (equal weight, inverse volatility, mean-variance, minimum variance, risk
parity, target volatility, momentum top-N / inverse-vol, dual momentum, buy & hold): the bt
library's algo stack (pmorissette/bt, MIT) decides the target weights; base.simulator executes
them, or bt.Backtest itself at fillTiming next_close. Signal strategies run on base.simulator.
Modules: bt_engine (run / optimise / walk-forward), bt_strategies (catalogue + rules, no bt
import), bt_algos (bt algo stack, decision pass, native execution), bt_evaluate (optimiser
callback), bt_data (imports the bt library past this package's own name).

NOTE: this directory is itself named `bt`; nothing here may `import bt` directly - use
bt_data.import_bt_library(). The provider class is intentionally not re-exported here (avoids
RuntimeWarning when bt_provider is run via `python -m`).
"""
