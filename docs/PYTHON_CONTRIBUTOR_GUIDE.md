# Python Contributor Guide

How to write and change the Python scripts in `fincept-qt/scripts/` — the
terminal's data connectors (~200), analytics modules, AI agents and
backtesting providers.

> **Prerequisites**: read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

**Related guides:**
- [C++ Guide](./CPP_CONTRIBUTOR_GUIDE.md) — how C++ executes Python and renders results
- [`fincept-qt/scripts/README.md`](../fincept-qt/scripts/README.md) — folder layout, who runs which script, what already moved to C++

---

## Where Python fits

The terminal is C++20/Qt. Python is used where a Python library does the job
best: market and macro data APIs, quantitative libraries (statsmodels,
PyPortfolioOpt, ffn, Qlib, …), backtesting engines and LLM agent frameworks.
Anything computational on a hot UI path belongs in C++ — option Greeks, the
Derivatives calculators, technical indicators and TOTP were moved there, so do
not add Python for those.

```
Screen (src/screens/…)          renders only
   ↓ calls / subscribes
Service (src/services/…)        fetching, caching, DataHub producer
   ↓ PythonRunner::run(script, args, …)
src/python/PythonRunner         spawns: python <script> <command> [args…]
   ↓                            (cwd + PYTHONPATH = scripts/, ≤ 3 at once, watchdog)
Python script                   prints ONE JSON document on stdout
   ↑
Service parses the JSON, caches it, emits a signal / publishes on the DataHub
```

AI agents reach the same scripts through the MCP `python` tool, which lists
every script that has an `if __name__ == "__main__":` block and shows the agent
its module docstring.

## Script standards

```python
"""World Widget Bureau data connector.

Commands:
    series <series_id> [start_year]   one time series
    search <query>                    find series ids
API key: WWB_API_KEY (optional; anonymous requests are rate-limited).
"""
import json
import os
import sys


def _series(series_id: str, start: int | None) -> dict:
    ...


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({"error": "usage: wwb_data.py <series|search> ..."}))
        sys.exit(1)
    command, args = sys.argv[1], sys.argv[2:]
    try:
        if command == "series" and args:
            result = _series(args[0], int(args[1]) if len(args) > 1 else None)
        elif command == "search" and args:
            result = {"data": []}  # ...
        else:
            result = {"error": f"unknown command or missing args: {command}"}
    except Exception as exc:  # report, never crash without JSON
        result = {"error": str(exc)}
    print(json.dumps(result, allow_nan=False, default=str))


if __name__ == "__main__":
    main()
```

- **One JSON document on stdout**; logs and warnings go to **stderr** (a stray
  `print` corrupts the payload).
- **Failure = `{"error": "<message>"}`.** PythonRunner treats it as a failure even
  on exit code 0. `"error": null` / `false` / `""` means success.
- **No `NaN` / `Infinity`.** Qt's JSON parser rejects the whole document. Convert
  them to `null` (`fincept_json.py` does this for pandas/numpy values) and keep
  `allow_nan=False` so a regression fails loudly.
- **Secrets and large inputs on stdin.** Accept a `--stdin` flag and read the JSON
  from standard input; never take a key, token or password on argv (readable by
  any same-user process; Windows caps a command line at ~32 KB). API keys saved in
  Settings arrive as environment variables.
- **Docstring first.** It is the only documentation an AI agent sees, and
  `.github/scripts/gen_data_connectors.py` reads it (and the command dispatch)
  to build the Data Sources catalogue.
- Type hints on public functions; no network calls at import time.

## Data connectors

A connector is a `<source>_data.py` that follows the standards above. After
adding one, or changing its commands or environment variables, run from the
repository root:

```bash
python .github/scripts/gen_data_connectors.py           # regenerate
python .github/scripts/gen_data_connectors.py --check   # exit 1 if stale
```

and commit `fincept-qt/resources/data_connectors.json` and
`fincept-qt/src/config/KeyedConnectorCredentials.inc`. Hand corrections (titles,
categories, `key_required` for keyed connectors) go in
`fincept-qt/scripts/connector_overrides.json`, never in the generated JSON.

## Dependencies

| File | Environment | Notes |
|---|---|---|
| `fincept-qt/resources/requirements-numpy2.txt` | `venv-numpy2` (default) | Full set. |
| `fincept-qt/resources/requirements-numpy2-core.txt` | `venv-numpy2` | Essentials installed on first run; every line must also be in the full set with the same specifier. |
| `fincept-qt/resources/requirements-numpy1.txt` | `venv-numpy1` | Libraries that still need NumPy 1.x (VectorBT, Zipline, bt, ffn, GluonTS, functime, financepy, PyPortfolioOpt). Scripts whose path contains `vectorbt`, `backtesting`, `gluonts`, `functime`, `pyportfolioopt`, `financepy` or `ffn` run here. |

Pin every new package, import it lazily if it is heavy or optional, and check the
set still resolves on all three release platforms:

```bash
uv pip compile fincept-qt/resources/requirements-numpy2.txt --python-version 3.11 --python-platform x86_64-pc-windows-msvc
uv pip compile fincept-qt/resources/requirements-numpy2.txt --python-version 3.11 --python-platform x86_64-unknown-linux-gnu
uv pip compile fincept-qt/resources/requirements-numpy2.txt --python-version 3.11 --python-platform aarch64-apple-darwin
```

A script that imports a package listed in none of the requirement files works on
your machine and fails for every user. Installed environments re-sync on their
own when a requirements file changes.

## Testing a script

Run it the way the terminal does — from `fincept-qt/scripts/`, with the app's
interpreter (`%LOCALAPPDATA%\com.fincept.terminal\venv-numpy2\Scripts\python.exe`
on Windows; `venv-numpy1` for the NumPy 1.x scripts):

```bash
cd fincept-qt/scripts
python -B yfinance_data.py quote AAPL
python -B fred_data.py series GDP
echo '{"symbol": "SPY", "expiry": "2026-06-20"}' | python -B databento_fno_chain.py get_chain --stdin
```

`-B` keeps `__pycache__/` out of the source tree. Check that stdout parses as
JSON and that every failure path prints `{"error": ...}`.

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
