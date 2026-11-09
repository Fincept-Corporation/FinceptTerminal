# Analytics

Python analytics behind several terminal screens, plus calculators that only AI
agents run (through the MCP `python` tool, which lists every script with an
`if __name__ == "__main__":` block — see `../README.md`). Every CLI follows the
same contract: `python <script> <command> '<json params>'` prints one JSON
document, `{"error": ...}` on failure.

## Used by the terminal

| Path | Screen / service | What it covers |
|---|---|---|
| `corporateFinance/` | M&A Analytics (`MAAnalyticsService`) | DCF, trading comps, precedent transactions, LBO model / returns / debt schedule / sensitivity, merger model (accretion-dilution, pro forma, sources & uses, contribution), deal structures (payment mix, earn-outs, exchange ratios, collars, CVRs), synergies, fairness opinions, industry metrics, startup valuation, deal database and SEC filing scanner |
| `alternateInvestment/cli.py` | Alternative Investments | Real estate, REITs, hedge funds, private equity / VC, commodities, infrastructure, annuities, structured products and more — `cli.py <analyzer> --data '<json>' [--method <m>]`; `cli.py --help` lists the analyzers |
| `economics/` | Geopolitics › trade analysis (`trade_geopolitics.py`) | Trade and geopolitics models; also `business_cycle.py`, `equity_risk_premium.py` |
| `quant/`, `quant_analytics_cli.py` | AI Quant Lab | CFA quantitative-methods analytics |
| `fortitudo_tech_wrapper/`, `functime_wrapper/`, `gluonts_wrapper/`, `gs_quant_wrapper/`, `statsmodels_wrapper/` | AI Quant Lab sub-tabs | One `*_service.py` CLI each (see each folder's README) |
| `backtesting/` | Backtesting (`BacktestingService` runs `backtesting/<provider>/<provider>_provider.py`) | Providers: `vectorbt`, `backtestingpy`, `zipline`, `bt`, `fincept` (the last runs the strategy library in `../strategies/`); shared code in `base/` |

## Available to AI agents only

| Path | What it does |
|---|---|
| `options/` | `gex_calculator.py`, `iv_smile.py`, `iv_surface.py`, `straddle_simulator.py` — gamma exposure, IV smile and surface, short-straddle simulation on an option chain passed in as JSON |
| `financial_analysis_cli.py`, `finanicalanalysis/` | Financial-statement analysis (income statement, balance sheet, cash flow, key metrics) |
| `fixedIncome/cli.py` | Bond analytics |
| `portfolioManagement/` | CFA portfolio-management modules (portfolio analytics, risk and active management, planning, economics & markets) |
| `quantstats_analytics.py` | Performance statistics, returns, drawdowns, rolling metrics |
| `statsmodels_cli.py` | Regression, time-series and statistical tests on the `statsmodels_wrapper` modules |
| `technical_analysis/momentum_indicators.py` | Momentum indicators for a ticker |
| `ffn_wrapper/`, `py_vollib_wrapper/`, `pypme_wrapper/`, `pyportfolioopt_wrapper/` | ffn performance analytics, Black / Black-Scholes(-Merton) prices, Greeks and IV, public-market-equivalent (PME) metrics, PyPortfolioOpt optimisation |

## Notes

- Scripts whose path contains `backtesting`, `ffn`, `functime`, `gluonts`,
  `pyportfolioopt`, `vectorbt` or `financepy` run in the NumPy 1.x environment
  (`resources/requirements-numpy1.txt`); everything else in `venv-numpy2`.
- `../fincept_irr.py` is the shared IRR helper (bracketed root find; never a
  silent 0) used by the LBO, Monte Carlo and annuity analytics.
- Option Greeks/IV, the Derivatives calculators and technical indicators are
  C++ (`src/services/options`, `src/services/equity/TechnicalIndicators`); do not
  add Python versions of them here.
