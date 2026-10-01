// src/screens/quantlib/QuantLibScreen_Data.cpp
//
// Static catalogs for the QuantLib screen — REST endpoint paths per module
// (module_endpoints) and request-body examples (endpoint_examples). Isolated
// from the rest of the screen so editing the catalogs doesn't recompile the
// UI/dispatch code.
//
// Part of the partial-class split of QuantLibScreen.cpp.

#include "screens/quantlib/QuantLibScreen.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace fincept::screens {

// Real API endpoint paths per module (from api.fincept.in OpenAPI spec).
const QHash<QString, QStringList>& module_endpoints() {
    static const QHash<QString, QStringList> data = {
        {"core",
         {"core/types/currencies",
          "core/types/frequencies",
          "core/types/money/create",
          "core/types/money/convert",
          "core/types/rate/convert",
          "core/types/spread/from-bps",
          "core/types/tenor/add-to-date",
          "core/types/notional-schedule",
          "core/conventions/parse-date",
          "core/conventions/format-date",
          "core/conventions/days-to-years",
          "core/conventions/years-to-days",
          "core/conventions/normalize-rate",
          "core/conventions/normalize-volatility",
          "core/autodiff/dual-eval",
          "core/autodiff/gradient",
          "core/autodiff/taylor-expand",
          "core/distributions/normal/cdf",
          "core/distributions/normal/pdf",
          "core/distributions/normal/ppf",
          "core/distributions/t/cdf",
          "core/distributions/t/pdf",
          "core/distributions/t/ppf",
          "core/distributions/chi2/cdf",
          "core/distributions/chi2/pdf",
          "core/distributions/gamma/cdf",
          "core/distributions/gamma/pdf",
          "core/distributions/exponential/cdf",
          "core/distributions/exponential/pdf",
          "core/distributions/exponential/ppf",
          "core/distributions/bivariate-normal/cdf",
          "core/math/eval",
          "core/math/two-arg",
          "core/ops/black-scholes",
          "core/ops/black76",
          "core/ops/forward-rate",
          "core/ops/discount-cashflows",
          "core/ops/interpolate",
          "core/ops/statistics",
          "core/ops/var",
          "core/ops/percentile",
          "core/ops/covariance-matrix",
          "core/ops/cholesky",
          "core/ops/gbm-paths",
          "core/ops/zero-rate-convert",
          "core/legs/fixed",
          "core/legs/float",
          "core/legs/zero-coupon",
          "core/periods/day-count-fraction",
          "core/periods/fixed-coupon",
          "core/periods/float-coupon"}},
        {"pricing",
         {"pricing/bs/price",
          "pricing/bs/greeks",
          "pricing/bs/greeks-full",
          "pricing/bs/implied-vol",
          "pricing/bs/digital-call",
          "pricing/bs/digital-put",
          "pricing/bs/asset-or-nothing-call",
          "pricing/bs/asset-or-nothing-put",
          "pricing/black76/price",
          "pricing/black76/greeks",
          "pricing/black76/greeks-full",
          "pricing/black76/implied-vol",
          "pricing/black76/caplet",
          "pricing/black76/floorlet",
          "pricing/black76/swaption",
          "pricing/bachelier/price",
          "pricing/bachelier/greeks",
          "pricing/bachelier/greeks-full",
          "pricing/bachelier/implied-vol",
          "pricing/bachelier/shifted-lognormal",
          "pricing/bachelier/vol-conversion",
          "pricing/binomial/european",
          "pricing/binomial/american",
          "pricing/binomial/bermudan",
          "pricing/binomial/barrier",
          "pricing/kirk/spread-price",
          "pricing/kirk/spread-greeks",
          "pricing/margrabe",
          "pricing/basket-levy"}},
        {"curves",
         {"curves/build",
          "curves/zero-rate",
          "curves/forward-rate",
          "curves/discount-factor",
          "curves/curve-points",
          "curves/interpolate",
          "curves/interpolate-derivative",
          "curves/instantaneous-forward",
          "curves/parallel-shift",
          "curves/twist",
          "curves/butterfly",
          "curves/key-rate-shift",
          "curves/nelson-siegel/fit",
          "curves/nelson-siegel/evaluate",
          "curves/nss/fit",
          "curves/nss/evaluate",
          "curves/roll",
          "curves/scale",
          "curves/time-shift",
          "curves/composite",
          "curves/proxy",
          "curves/real-rate",
          "curves/monotonicity-check",
          "curves/smoothness-penalty",
          "curves/constrained-fit",
          "curves/multicurve/setup",
          "curves/multicurve/basis-spread",
          "curves/cross-currency-basis",
          "curves/inflation/build",
          "curves/inflation/bootstrap",
          "curves/inflation/seasonality"}},
        {"volatility",
         {"volatility/surface/flat", "volatility/surface/from-points", "volatility/surface/grid",
          "volatility/surface/smile", "volatility/surface/term-structure", "volatility/surface/total-variance",
          "volatility/sabr/implied-vol", "volatility/sabr/calibrate", "volatility/sabr/smile",
          "volatility/sabr/normal-vol", "volatility/sabr/density", "volatility/sabr/dynamics",
          "volatility/local-vol/constant", "volatility/local-vol/implied-to-local"}},
        {"models",
         {"models/short-rate/bond-price", "models/short-rate/bond-option", "models/short-rate/yield-curve",
          "models/short-rate/monte-carlo", "models/hull-white/calibrate", "models/heston/price",
          "models/heston/monte-carlo", "models/heston/implied-vol", "models/merton/price", "models/merton/fft",
          "models/kou/price", "models/dupire/price", "models/svi/calibrate", "models/variance-gamma/price"}},
        {"stochastic",
         {"stochastic/gbm/simulate",
          "stochastic/gbm/properties",
          "stochastic/ou/simulate",
          "stochastic/cir/simulate",
          "stochastic/cir/bond-price",
          "stochastic/heston/simulate",
          "stochastic/merton/simulate",
          "stochastic/vasicek/simulate",
          "stochastic/vasicek/bond-price",
          "stochastic/wiener/simulate",
          "stochastic/poisson/simulate",
          "stochastic/variance-gamma/simulate",
          "stochastic/brownian-bridge/simulate",
          "stochastic/correlated-bm/simulate",
          "stochastic/exact/gbm",
          "stochastic/exact/ou",
          "stochastic/exact/cir",
          "stochastic/exact/heston",
          "stochastic/simulation/euler-maruyama",
          "stochastic/simulation/milstein",
          "stochastic/simulation/euler-maruyama-nd",
          "stochastic/simulation/milstein-nd",
          "stochastic/simulation/multilevel-mc",
          "stochastic/sampling/sobol",
          "stochastic/sampling/antithetic",
          "stochastic/sampling/correlated-normals",
          "stochastic/sampling/multivariate-normal",
          "stochastic/sampling/distribution",
          "stochastic/sampling/jump",
          "stochastic/theory/ito-lemma",
          "stochastic/theory/ito-product-rule",
          "stochastic/theory/quadratic-variation",
          "stochastic/theory/covariation",
          "stochastic/theory/martingale-test",
          "stochastic/theory/girsanov/measure-change",
          "stochastic/theory/girsanov/risk-neutral-drift"}},
        {"risk",
         {"risk/var/parametric",
          "risk/var/historical",
          "risk/var/component",
          "risk/var/incremental",
          "risk/var/marginal",
          "risk/var/es-optimization",
          "risk/backtest",
          "risk/stress/scenario",
          "risk/correlation-stress",
          "risk/tail-risk/comprehensive",
          "risk/tail-risk/tail-dependence",
          "risk/evt/gpd",
          "risk/evt/gev",
          "risk/evt/hill",
          "risk/xva/cva",
          "risk/xva/pfe",
          "risk/copula/sample",
          "risk/portfolio-risk/exposure-profile",
          "risk/portfolio-risk/optimal-hedge",
          "risk/sensitivities/greeks",
          "risk/sensitivities/key-rate-duration",
          "risk/sensitivities/parallel-shift",
          "risk/sensitivities/twist",
          "risk/sensitivities/cross-gamma",
          "risk/sensitivities/bucket-delta"}},
        {"portfolio",
         {"portfolio/optimize/min-variance", "portfolio/optimize/max-sharpe", "portfolio/optimize/efficient-frontier",
          "portfolio/optimize/target-return", "portfolio/black-litterman/equilibrium",
          "portfolio/black-litterman/posterior", "portfolio/risk-parity", "portfolio/risk/inverse-volatility",
          "portfolio/risk/var", "portfolio/risk/cvar", "portfolio/risk/incremental-var", "portfolio/risk/contribution",
          "portfolio/risk/ratios", "portfolio/risk/comprehensive", "portfolio/risk/portfolio-comprehensive"}},
        {"instruments",
         {"instruments/bond/fixed/price",
          "instruments/bond/fixed/yield",
          "instruments/bond/fixed/analytics",
          "instruments/bond/fixed/cashflows",
          "instruments/bond/zero-coupon/price",
          "instruments/bond/inflation-linked",
          "instruments/swap/irs/value",
          "instruments/swap/irs/par-rate",
          "instruments/swap/irs/dv01",
          "instruments/fra/value",
          "instruments/fra/break-even",
          "instruments/ois/value",
          "instruments/ois/build-curve",
          "instruments/cds/value",
          "instruments/cds/hazard-rate",
          "instruments/cds/survival-probability",
          "instruments/fx/forward",
          "instruments/fx/garman-kohlhagen",
          "instruments/money-market/deposit",
          "instruments/money-market/repo",
          "instruments/money-market/tbill",
          "instruments/commodity/future",
          "instruments/futures/stir",
          "instruments/futures/bond/ctd",
          "instruments/equity/variance-swap",
          "instruments/equity/volatility-swap"}},
        {"solver",
         {"solver/finance/bond-yield",
          "solver/finance/duration",
          "solver/finance/modified-duration",
          "solver/finance/convexity",
          "solver/finance/convexity-adjustment",
          "solver/finance/pv01",
          "solver/finance/implied-vol",
          "solver/finance/implied-vol-black76",
          "solver/finance/forward-rate",
          "solver/finance/zero-rate",
          "solver/finance/discount-factor",
          "solver/finance/par-rate",
          "solver/finance/z-spread",
          "solver/finance/i-spread",
          "solver/finance/g-spread",
          "solver/finance/oas",
          "solver/finance/asw-spread",
          "solver/finance/basis",
          "solver/finance/carry",
          "solver/finance/implied-repo-rate",
          "solver/finance/forward-futures-conversion",
          "solver/finance/irr",
          "solver/finance/xirr",
          "solver/bootstrap/curve",
          "solver/calibration/vasicek"}},
        {"economics",
         {"economics/equilibrium/cobb-douglas",
          "economics/equilibrium/ces",
          "economics/equilibrium/walrasian",
          "economics/equilibrium/exchange-economy",
          "economics/games/create",
          "economics/games/classic",
          "economics/games/mixed-nash",
          "economics/games/nash-check",
          "economics/games/best-response",
          "economics/games/eliminate-dominated",
          "economics/games/fictitious-play",
          "economics/auctions/run",
          "economics/auctions/simulate",
          "economics/auctions/equilibrium-bid",
          "economics/auctions/expected-revenue",
          "economics/utility/cara",
          "economics/utility/crra",
          "economics/utility/log",
          "economics/utility/quadratic",
          "economics/utility/expected-utility",
          "economics/utility/certainty-equivalent",
          "economics/utility/certainty-equivalent-approximation",
          "economics/utility/risk-premium",
          "economics/utility/prospect-theory",
          "economics/utility/stochastic-dominance"}},
        {"regulatory",
         {"regulatory/basel/capital-ratios", "regulatory/basel/credit-rwa", "regulatory/basel/operational-rwa",
          "regulatory/saccr/ead", "regulatory/ifrs9/stage-assessment", "regulatory/ifrs9/sicr",
          "regulatory/ifrs9/ecl-12m", "regulatory/ifrs9/ecl-lifetime", "regulatory/liquidity/lcr",
          "regulatory/liquidity/nsfr", "regulatory/stress/capital-projection"}},
        {"scheduling",
         {"scheduling/calendar/list", "scheduling/calendar/is-business-day", "scheduling/calendar/next-business-day",
          "scheduling/calendar/previous-business-day", "scheduling/calendar/business-days-between",
          "scheduling/calendar/add-business-days", "scheduling/daycount/conventions",
          "scheduling/daycount/year-fraction", "scheduling/daycount/day-count",
          "scheduling/daycount/batch-year-fraction", "scheduling/adjustment/adjust-date",
          "scheduling/adjustment/batch-adjust", "scheduling/adjustment/methods", "scheduling/schedule/generate"}},
        {"numerical",
         {"numerical/differentiation/derivative",
          "numerical/differentiation/gradient",
          "numerical/differentiation/hessian",
          "numerical/fft/forward",
          "numerical/fft/inverse",
          "numerical/fft/convolve",
          "numerical/integration/quadrature",
          "numerical/integration/monte-carlo",
          "numerical/integration/stratified",
          "numerical/interpolation/evaluate",
          "numerical/interpolation/spline-curve",
          "numerical/interpolation/spline-derivative",
          "numerical/linalg/matmul",
          "numerical/linalg/matvec",
          "numerical/linalg/solve",
          "numerical/linalg/inverse",
          "numerical/linalg/decompose",
          "numerical/linalg/dot",
          "numerical/linalg/outer",
          "numerical/linalg/norm",
          "numerical/linalg/transpose",
          "numerical/linalg/lstsq",
          "numerical/least-squares/fit",
          "numerical/ode/solve",
          "numerical/roots/find-1d",
          "numerical/roots/find-nd",
          "numerical/roots/newton",
          "numerical/optimize/minimize"}},
        {"physics",
         {"physics/entropy/shannon",
          "physics/entropy/renyi",
          "physics/entropy/tsallis",
          "physics/entropy/cross",
          "physics/entropy/conditional",
          "physics/entropy/joint",
          "physics/entropy/differential",
          "physics/entropy/mutual-information",
          "physics/entropy/transfer",
          "physics/entropy/fisher-information",
          "physics/entropy/markov-rate",
          "physics/divergence/kl",
          "physics/divergence/js",
          "physics/boltzmann",
          "physics/max-entropy",
          "physics/ising",
          "physics/ising/critical-temperature",
          "physics/thermodynamics/ideal-gas",
          "physics/thermodynamics/van-der-waals",
          "physics/thermodynamics/carnot",
          "physics/thermodynamics/free-energy",
          "physics/thermodynamics/clausius-clapeyron",
          "physics/thermodynamics/maxwell-relations",
          "physics/thermodynamics/joule-thomson"}},
        {"statistics",
         {"statistics/distributions/normal/cdf",
          "statistics/distributions/normal/pdf",
          "statistics/distributions/normal/ppf",
          "statistics/distributions/normal/properties",
          "statistics/distributions/lognormal/cdf",
          "statistics/distributions/lognormal/pdf",
          "statistics/distributions/lognormal/ppf",
          "statistics/distributions/lognormal/properties",
          "statistics/distributions/student-t/cdf",
          "statistics/distributions/student-t/pdf",
          "statistics/distributions/student-t/properties",
          "statistics/distributions/chi-squared/cdf",
          "statistics/distributions/chi-squared/pdf",
          "statistics/distributions/chi-squared/properties",
          "statistics/distributions/f/pdf",
          "statistics/distributions/f/properties",
          "statistics/distributions/gamma/cdf",
          "statistics/distributions/gamma/pdf",
          "statistics/distributions/gamma/properties",
          "statistics/distributions/beta/cdf",
          "statistics/distributions/beta/pdf",
          "statistics/distributions/beta/properties",
          "statistics/distributions/exponential/cdf",
          "statistics/distributions/exponential/pdf",
          "statistics/distributions/exponential/ppf",
          "statistics/distributions/exponential/properties",
          "statistics/distributions/poisson/cdf",
          "statistics/distributions/poisson/pmf",
          "statistics/distributions/poisson/properties",
          "statistics/distributions/binomial/cdf",
          "statistics/distributions/binomial/pmf",
          "statistics/distributions/binomial/properties",
          "statistics/distributions/geometric/cdf",
          "statistics/distributions/geometric/pmf",
          "statistics/distributions/geometric/ppf",
          "statistics/distributions/geometric/properties",
          "statistics/distributions/hypergeometric/cdf",
          "statistics/distributions/hypergeometric/pmf",
          "statistics/distributions/hypergeometric/properties",
          "statistics/distributions/negative-binomial/cdf",
          "statistics/distributions/negative-binomial/pmf",
          "statistics/distributions/negative-binomial/properties",
          "statistics/distributions/pgf",
          "statistics/timeseries/ar/fit",
          "statistics/timeseries/ar/forecast",
          "statistics/timeseries/arima/fit",
          "statistics/timeseries/arima/forecast",
          "statistics/timeseries/ma/fit",
          "statistics/timeseries/garch/fit",
          "statistics/timeseries/garch/forecast",
          "statistics/timeseries/egarch/fit",
          "statistics/timeseries/gjr-garch/fit"}},
        {"ml",
         {"ml/credit/scorecard",
          "ml/credit/logistic-regression",
          "ml/credit/woe-binning",
          "ml/credit/calibration",
          "ml/credit/discrimination",
          "ml/credit/performance",
          "ml/credit/migration",
          "ml/credit/beta-lgd",
          "ml/credit/two-stage-lgd",
          "ml/regression/fit",
          "ml/regression/ensemble",
          "ml/regression/tree",
          "ml/regression/lgd",
          "ml/regression/ead",
          "ml/clustering/kmeans",
          "ml/clustering/hierarchical",
          "ml/clustering/dbscan",
          "ml/clustering/pca",
          "ml/clustering/isolation-forest",
          "ml/preprocessing/scale",
          "ml/preprocessing/transform",
          "ml/preprocessing/outliers",
          "ml/preprocessing/winsorize",
          "ml/preprocessing/stationarity",
          "ml/features/lags",
          "ml/features/rolling",
          "ml/features/calendar",
          "ml/features/technical",
          "ml/features/financial-ratios",
          "ml/features/cross-sectional",
          "ml/validation/stability",
          "ml/validation/discrimination-report",
          "ml/validation/calibration-report",
          "ml/validation/interpretability",
          "ml/timeseries/feature-importance",
          "ml/changepoint/detect",
          "ml/gp/curve",
          "ml/gp/vol-surface",
          "ml/nn/curve",
          "ml/nn/vol-surface",
          "ml/nn/portfolio",
          "ml/hmm/fit",
          "ml/garch-hybrid",
          "ml/metrics/regression",
          "ml/metrics/classification",
          "ml/factor/statistical",
          "ml/factor/cross-sectional",
          "ml/covariance/estimate"}},
        {"analysis",
         {"analysis/fundamentals/profitability",
          "analysis/fundamentals/liquidity",
          "analysis/fundamentals/efficiency",
          "analysis/fundamentals/growth",
          "analysis/fundamentals/solvency",
          "analysis/fundamentals/cashflow",
          "analysis/fundamentals/comprehensive",
          "analysis/fundamentals/dupont",
          "analysis/fundamentals/quality",
          "analysis/fundamentals/capital-structure/wacc",
          "analysis/fundamentals/capital-structure/optimal",
          "analysis/ratios/profitability/roa",
          "analysis/ratios/profitability/roe",
          "analysis/ratios/profitability/roic",
          "analysis/ratios/profitability/gross-margin",
          "analysis/ratios/profitability/net-margin",
          "analysis/ratios/profitability/ebitda-margin",
          "analysis/ratios/profitability/operating-margin",
          "analysis/ratios/profitability/roce",
          "analysis/ratios/profitability/basic-earning-power",
          "analysis/ratios/profitability/cash-roa",
          "analysis/ratios/profitability/cash-roe",
          "analysis/ratios/profitability/cash-roic",
          "analysis/ratios/liquidity/current-ratio",
          "analysis/ratios/liquidity/quick-ratio",
          "analysis/ratios/liquidity/cash-ratio",
          "analysis/ratios/liquidity/absolute-liquidity",
          "analysis/ratios/liquidity/defensive-interval",
          "analysis/ratios/liquidity/working-capital",
          "analysis/ratios/liquidity/working-capital-ratio",
          "analysis/ratios/leverage/debt-to-equity",
          "analysis/ratios/leverage/interest-coverage",
          "analysis/ratios/leverage/debt-to-assets",
          "analysis/ratios/leverage/debt-to-capital",
          "analysis/ratios/leverage/equity-ratio",
          "analysis/ratios/leverage/equity-multiplier",
          "analysis/ratios/leverage/ebitda-interest-coverage",
          "analysis/ratios/leverage/cash-coverage",
          "analysis/ratios/leverage/net-debt-to-ebitda",
          "analysis/ratios/leverage/long-term-debt-to-equity",
          "analysis/ratios/leverage/financial-leverage",
          "analysis/ratios/leverage/capitalization-ratio",
          "analysis/ratios/leverage/debt-service-coverage",
          "analysis/ratios/valuation/pe",
          "analysis/ratios/valuation/pb",
          "analysis/ratios/valuation/ps",
          "analysis/ratios/valuation/ev-ebitda",
          "analysis/ratios/valuation/dividend-yield",
          "analysis/ratios/valuation/pcf",
          "analysis/ratios/valuation/pfcf",
          "analysis/ratios/valuation/peg",
          "analysis/ratios/valuation/ev-revenue",
          "analysis/ratios/valuation/ev-fcf",
          "analysis/ratios/valuation/earnings-yield",
          "analysis/ratios/valuation/fcf-yield",
          "analysis/ratios/valuation/tobins-q",
          "analysis/ratios/efficiency/asset-turnover",
          "analysis/ratios/efficiency/fixed-asset-turnover",
          "analysis/ratios/efficiency/inventory-turnover",
          "analysis/ratios/efficiency/receivables-turnover",
          "analysis/ratios/efficiency/payables-turnover",
          "analysis/ratios/efficiency/days-sales-outstanding",
          "analysis/ratios/efficiency/days-inventory-outstanding",
          "analysis/ratios/efficiency/days-payables-outstanding",
          "analysis/ratios/efficiency/cash-conversion-cycle",
          "analysis/ratios/efficiency/operating-cycle",
          "analysis/ratios/efficiency/working-capital-turnover",
          "analysis/ratios/efficiency/equity-turnover",
          "analysis/ratios/growth/yoy",
          "analysis/ratios/growth/cagr",
          "analysis/ratios/growth/sustainable-growth",
          "analysis/ratios/growth/internal-growth",
          "analysis/ratios/cashflow/ocf-to-debt",
          "analysis/ratios/cashflow/ocf-ratio",
          "analysis/ratios/cashflow/ocf-margin",
          "analysis/ratios/cashflow/fcf",
          "analysis/ratios/cashflow/fcf-margin",
          "analysis/ratios/cashflow/cash-conversion-quality",
          "analysis/ratios/cashflow/capex-to-ocf",
          "analysis/ratios/cashflow/capex-to-depreciation",
          "analysis/ratios/cashflow/cash-coverage-of-dividends",
          "analysis/ratios/cashflow/reinvestment-rate",
          "analysis/ratios/quality/accruals-ratio",
          "analysis/ratios/quality/sloan-accruals",
          "analysis/ratios/quality/earnings-persistence",
          "analysis/ratios/quality/earnings-variability",
          "analysis/ratios/quality/cash-earnings",
          "analysis/ratios/quality/quality-of-earnings",
          "analysis/valuation/dcf/fcff",
          "analysis/valuation/dcf/ddm",
          "analysis/valuation/dcf/gordon-growth",
          "analysis/valuation/dcf/two-stage",
          "analysis/valuation/dcf/wacc",
          "analysis/valuation/dcf/terminal-value",
          "analysis/valuation/dcf/cost-of-equity",
          "analysis/valuation/comparable",
          "analysis/valuation/screen",
          "analysis/valuation/factor-models",
          "analysis/valuation/credit/merton-model",
          "analysis/valuation/credit/distance-to-default",
          "analysis/valuation/credit/spread-from-pd",
          "analysis/valuation/credit/expected-loss",
          "analysis/valuation/credit/rating-pd",
          "analysis/valuation/predictive/altman-z",
          "analysis/valuation/predictive/piotroski-f",
          "analysis/valuation/predictive/beneish-m",
          "analysis/valuation/predictive/ohlson-o",
          "analysis/valuation/predictive/springate",
          "analysis/valuation/predictive/zmijewski",
          "analysis/valuation/residual-income/eva",
          "analysis/valuation/residual-income/ri",
          "analysis/valuation/residual-income/valuation",
          "analysis/valuation/startup/vc-method",
          "analysis/valuation/startup/berkus",
          "analysis/valuation/startup/first-chicago",
          "analysis/valuation/startup/dilution",
          "analysis/valuation/proforma/adjustments",
          "analysis/valuation/segment/sotp",
          "analysis/industry/banking",
          "analysis/industry/insurance",
          "analysis/industry/reits",
          "analysis/industry/utilities"}},
    };
    return data;
}

// ── Endpoint example bodies (verified against live API) ─────────────────────

const QHash<QString, QString>& QuantLibScreen::endpoint_examples() {
    static const QHash<QString, QString> map = {
        // core/types
        {"core/types/money/create", R"({"amount":100,"currency":"USD"})"},
        {"core/types/money/convert", R"({"amount":100,"from_currency":"USD","to_currency":"EUR","rate":0.92})"},
        {"core/types/rate/convert", R"({"value":0.05,"from_type":"annual","to_type":"continuous"})"},
        {"core/types/spread/from-bps", R"({"bps":50})"},
        {"core/types/tenor/add-to-date", R"({"start_date":"2024-01-01","tenor":"3M"})"},
        {"core/types/notional-schedule", R"({"notional":1000000,"periods":4,"schedule_type":"constant"})"},
        // core/conventions
        {"core/conventions/parse-date", R"({"date_string":"2024-01-15","format":"%Y-%m-%d"})"},
        {"core/conventions/format-date", R"({"date_str":"2024-01-15","format":"%d/%m/%Y"})"},
        {"core/conventions/days-to-years", R"({"value":365,"day_count":"ACT/365"})"},
        {"core/conventions/years-to-days", R"({"value":1.0,"day_count":"ACT/365"})"},
        {"core/conventions/normalize-rate", R"({"value":0.05,"compounding":"annual"})"},
        {"core/conventions/normalize-volatility", R"({"value":0.2,"tenor":"1Y"})"},
        // core/autodiff
        {"core/autodiff/dual-eval", R"({"func_name":"sin","x":1.0})"},
        {"core/autodiff/gradient", R"({"func_name":"sin","x":[1.0]})"},
        {"core/autodiff/taylor-expand", R"({"func_name":"sin","x0":0.0,"order":3})"},
        // core/distributions
        {"core/distributions/normal/cdf", R"({"x":1.645,"mean":0,"std":1})"},
        {"core/distributions/normal/pdf", R"({"x":0.0,"mean":0,"std":1})"},
        {"core/distributions/normal/ppf", R"({"p":0.95,"mean":0,"std":1})"},
        {"core/distributions/t/cdf", R"({"x":1.96,"df":30})"},
        {"core/distributions/t/pdf", R"({"x":0.0,"df":10})"},
        {"core/distributions/t/ppf", R"({"p":0.975,"df":30})"},
        {"core/distributions/chi2/cdf", R"({"x":3.84,"df":1})"},
        {"core/distributions/chi2/pdf", R"({"x":2.0,"df":3})"},
        {"core/distributions/gamma/cdf", R"({"x":2.0,"alpha":2.0,"beta":1.0})"},
        {"core/distributions/gamma/pdf", R"({"x":2.0,"alpha":2.0,"beta":1.0})"},
        {"core/distributions/exponential/cdf", R"({"x":1.0,"rate":1.0})"},
        {"core/distributions/exponential/pdf", R"({"x":1.0,"rate":1.0})"},
        {"core/distributions/exponential/ppf", R"({"p":0.95,"rate":1.0})"},
        {"core/distributions/bivariate-normal/cdf", R"({"x":1.0,"y":1.0,"rho":0.5})"},
        // core/math
        {"core/math/eval", R"({"func_name":"sqrt","x":2.0})"},
        {"core/math/two-arg", R"({"func_name":"power","x":2.0,"y":10.0})"},
        // core/ops
        {"core/ops/black-scholes",
         R"({"spot":100,"strike":105,"rate":0.05,"volatility":0.2,"time":1.0,"option_type":"call"})"},
        {"core/ops/black76",
         R"({"forward":100,"strike":105,"discount_factor":0.95,"volatility":0.2,"time":1.0,"option_type":"call"})"},
        {"core/ops/forward-rate", R"({"df1":0.95,"df2":0.90,"t1":1.0,"t2":2.0})"},
        {"core/ops/discount-cashflows",
         R"({"cashflows":[100,100,1100],"times":[1,2,3],"discount_factors":[0.95,0.90,0.86]})"},
        {"core/ops/interpolate", R"({"x_data":[1,2,3,4],"y_data":[1,4,9,16],"x":2.5,"method":"linear"})"},
        {"core/ops/statistics", R"({"values":[1,2,3,4,5,6,7,8,9,10]})"},
        {"core/ops/var", R"({"returns":[-0.02,0.01,-0.015,0.03,-0.01,0.02],"confidence":0.95,"method":"historical"})"},
        {"core/ops/percentile", R"({"values":[1,2,3,4,5,6,7,8,9,10],"p":0.9})"},
        {"core/ops/covariance-matrix", R"({"returns":[[0.01,0.02],[0.03,-0.01],[0.02,0.01],[-0.01,0.03]]})"},
        {"core/ops/cholesky", R"({"matrix":[[4,2],[2,3]]})"},
        {"core/ops/gbm-paths", R"({"spot":100,"drift":0.05,"volatility":0.2,"time":1.0,"n_steps":52,"n_paths":5})"},
        {"core/ops/zero-rate-convert", R"({"direction":"continuous_to_annual","value":0.05,"t":1.0})"},
        // core/legs
        {"core/legs/fixed",
         R"({"notional":1000000,"rate":0.05,"frequency":"6M","start_date":"2024-01-01","end_date":"2026-01-01"})"},
        {"core/legs/float",
         R"({"notional":1000000,"spread":0.01,"frequency":"3M","start_date":"2024-01-01","end_date":"2026-01-01"})"},
        {"core/legs/zero-coupon",
         R"({"notional":1000000,"rate":0.05,"start_date":"2024-01-01","end_date":"2029-01-01"})"},
        // core/periods
        {"core/periods/day-count-fraction",
         R"({"start_date":"2024-01-01","end_date":"2024-07-01","convention":"ACT/365"})"},
        {"core/periods/fixed-coupon",
         R"({"notional":1000000,"rate":0.05,"start_date":"2024-01-01","end_date":"2024-07-01"})"},
        {"core/periods/float-coupon",
         R"({"notional":1000000,"spread":0.01,"start_date":"2024-01-01","end_date":"2024-04-01"})"},

        // pricing/bs
        {"pricing/bs/price",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/bs/greeks",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/bs/greeks-full",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/bs/implied-vol",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"time_to_maturity":1.0,"market_price":8.0,"option_type":"call"})"},
        {"pricing/bs/digital-call",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0})"},
        {"pricing/bs/digital-put",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0})"},
        {"pricing/bs/asset-or-nothing-call",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0})"},
        {"pricing/bs/asset-or-nothing-put",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0})"},
        // pricing/black76
        {"pricing/black76/price",
         R"({"forward":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/black76/greeks",
         R"({"forward":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/black76/greeks-full",
         R"({"forward":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"option_type":"call"})"},
        {"pricing/black76/implied-vol",
         R"({"forward":100,"strike":105,"risk_free_rate":0.05,"time_to_maturity":1.0,"market_price":8.0,"option_type":"call"})"},
        {"pricing/black76/caplet",
         R"({"forward_rate":0.05,"discount_factor":0.95,"volatility":0.2,"t_start":1.0,"t_end":1.25,"strike":0.048,"notional":1000000})"},
        {"pricing/black76/floorlet",
         R"({"forward_rate":0.05,"discount_factor":0.95,"volatility":0.2,"t_start":1.0,"t_end":1.25,"strike":0.052,"notional":1000000})"},
        {"pricing/black76/swaption",
         R"({"forward_swap_rate":0.05,"annuity":4.5,"volatility":0.2,"t_expiry":1.0,"strike":0.055,"notional":1000000,"option_type":"call"})"},
        // pricing/bachelier
        {"pricing/bachelier/price",
         R"({"forward":100,"strike":105,"normal_volatility":5.0,"time_to_maturity":1.0,"risk_free_rate":0.05,"option_type":"call"})"},
        {"pricing/bachelier/greeks",
         R"({"forward":100,"strike":105,"normal_volatility":5.0,"time_to_maturity":1.0,"risk_free_rate":0.05,"option_type":"call"})"},
        {"pricing/bachelier/greeks-full",
         R"({"forward":100,"strike":105,"normal_volatility":5.0,"time_to_maturity":1.0,"risk_free_rate":0.05,"option_type":"call"})"},
        {"pricing/bachelier/implied-vol",
         R"({"forward":100,"strike":105,"time_to_maturity":1.0,"market_price":5.0,"risk_free_rate":0.05,"option_type":"call"})"},
        {"pricing/bachelier/shifted-lognormal",
         R"({"forward":100,"strike":105,"volatility":0.2,"time_to_maturity":1.0,"shift":0.03,"risk_free_rate":0.05,"option_type":"call"})"},
        {"pricing/bachelier/vol-conversion",
         R"({"normal_vol":5.0,"volatility":0.2,"forward":100,"strike":105,"time_to_maturity":1.0})"},
        // pricing/binomial
        {"pricing/binomial/european",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"steps":100,"option_type":"call"})"},
        {"pricing/binomial/american",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"steps":100,"option_type":"call"})"},
        {"pricing/binomial/bermudan",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"steps":100,"exercise_dates":[0.5,1.0],"option_type":"call"})"},
        {"pricing/binomial/barrier",
         R"({"spot":100,"strike":105,"risk_free_rate":0.05,"volatility":0.2,"time_to_maturity":1.0,"steps":100,"barrier":110,"is_knock_in":false,"is_down":false,"option_type":"call"})"},
        // pricing/kirk & exotic
        {"pricing/kirk/spread-price",
         R"({"F1":100,"F2":95,"strike":5,"sigma1":0.2,"sigma2":0.18,"rho":0.7,"risk_free_rate":0.05,"time_to_maturity":1.0})"},
        {"pricing/kirk/spread-greeks",
         R"({"F1":100,"F2":95,"strike":5,"sigma1":0.2,"sigma2":0.18,"rho":0.7,"risk_free_rate":0.05,"time_to_maturity":1.0})"},
        {"pricing/margrabe",
         R"({"S1":100,"S2":95,"sigma1":0.2,"sigma2":0.18,"rho":0.7,"r":0.05,"time_to_maturity":1.0,"Q1":0,"Q2":0})"},
        {"pricing/basket-levy",
         R"({"forwards":[100,95,105],"weights":[0.4,0.3,0.3],"strike":100,"sigmas":[0.2,0.18,0.22],"correlations":[1,0.5,0.3,0.5,1,0.4,0.3,0.4,1],"risk_free_rate":0.05,"time_to_maturity":1.0,"option_type":"call"})"},

        // stochastic/gbm
        {"stochastic/gbm/simulate", R"({"S0":100,"mu":0.05,"sigma":0.2,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/gbm/properties", R"({"S0":100,"mu":0.05,"sigma":0.2,"T":0.999})"},
        // stochastic/ou
        {"stochastic/ou/simulate",
         R"({"X0":0.0,"kappa":2.0,"theta":0.05,"sigma":0.1,"T":1.0,"n_steps":52,"n_paths":3})"},
        // stochastic/cir
        {"stochastic/cir/simulate",
         R"({"r0":0.05,"kappa":1.5,"theta":0.04,"sigma":0.1,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/cir/bond-price", R"({"r0":0.05,"kappa":1.5,"theta":0.04,"sigma":0.1,"T":5.0})"},
        // stochastic/heston
        {"stochastic/heston/simulate",
         R"({"S0":100,"v0":0.04,"r":0.05,"kappa":1.5,"theta":0.04,"sigma_v":0.3,"rho":-0.7,"T":1.0,"n_steps":52,"n_paths":3})"},
        // stochastic/merton
        {"stochastic/merton/simulate",
         R"({"S0":100,"mu":0.05,"sigma":0.2,"lam":0.5,"jump_mean":0.0,"jump_std":0.1,"T":1.0,"n_steps":52,"n_paths":3})"},
        // stochastic/vasicek
        {"stochastic/vasicek/simulate",
         R"({"r0":0.05,"kappa":1.5,"theta":0.04,"sigma":0.01,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/vasicek/bond-price", R"({"r0":0.05,"kappa":1.5,"theta":0.04,"sigma":0.01,"T":5.0})"},
        // stochastic misc processes
        {"stochastic/wiener/simulate", R"({"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/poisson/simulate", R"({"lam":2.0,"T":1.0,"n_paths":3})"},
        {"stochastic/variance-gamma/simulate",
         R"({"S0":100,"mu":0.05,"sigma":0.2,"nu":0.2,"theta_vg":0.1,"r":0.02,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/brownian-bridge/simulate", R"({"x0":0.0,"x_end":1.0,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/correlated-bm/simulate",
         R"({"n_assets":2,"correlation_matrix":[[1,0.7],[0.7,1]],"T":1.0,"n_steps":52,"n_paths":3})"},
        // stochastic/exact
        {"stochastic/exact/gbm", R"({"S0":100,"mu":0.05,"sigma":0.2,"T":1.0,"n_paths":100})"},
        {"stochastic/exact/ou", R"({"X0":0.0,"kappa":2.0,"theta":0.05,"sigma":0.1,"T":1.0,"n_paths":100})"},
        {"stochastic/exact/cir", R"({"r0":0.05,"kappa":1.5,"theta":0.04,"sigma":0.1,"T":1.0,"n_paths":100})"},
        {"stochastic/exact/heston",
         R"({"S0":100,"v0":0.04,"r":0.05,"kappa":1.5,"theta":0.04,"sigma_v":0.3,"rho":-0.7,"T":1.0,"n_paths":100})"},
        // stochastic/simulation
        {"stochastic/simulation/euler-maruyama",
         R"({"x0":1.0,"mu":0.05,"sigma":0.2,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/simulation/milstein", R"({"x0":1.0,"mu":0.05,"sigma":0.2,"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/simulation/euler-maruyama-nd",
         R"({"x0":[1.0,1.0],"mu":[0.05,0.03],"sigma":[[0.2,0.05],[0.05,0.15]],"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/simulation/milstein-nd",
         R"({"x0":[1.0,1.0],"mu":[0.05,0.03],"sigma":[[0.2,0.05],[0.05,0.15]],"T":1.0,"n_steps":52,"n_paths":3})"},
        {"stochastic/simulation/multilevel-mc", R"({"S0":100,"mu":0.05,"sigma":0.2,"T":1.0,"levels":4})"},
        // stochastic/sampling
        {"stochastic/sampling/sobol", R"({"n":100,"dim":3})"},
        {"stochastic/sampling/antithetic", R"({"S0":100,"mu":0.05,"sigma":0.2,"T":1.0,"n_steps":52,"n":50})"},
        {"stochastic/sampling/correlated-normals", R"({"rho":0.7,"n":100})"},
        {"stochastic/sampling/multivariate-normal", R"({"mean":[0,0],"cov":[[1,0.5],[0.5,1]],"n_samples":100})"},
        {"stochastic/sampling/distribution",
         R"({"distribution":"gamma","params":{"shape":2.0,"scale":1.0},"n_samples":100})"},
        {"stochastic/sampling/jump", R"({"lam":0.5,"mu_j":0.0,"sigma_j":0.1,"T":1.0,"n_paths":100})"},
        // stochastic/theory
        {"stochastic/theory/ito-lemma", R"({"path":[100,101,99,102,103],"times":[0,0.25,0.5,0.75,1.0]})"},
        {"stochastic/theory/ito-product-rule",
         R"({"path_X":[100,101,99,102],"path_Y":[50,51,49,52],"times":[0,0.33,0.67,1.0]})"},
        {"stochastic/theory/quadratic-variation", R"({"path":[100,101,99,102,103],"times":[0,0.25,0.5,0.75,1.0]})"},
        {"stochastic/theory/covariation",
         R"({"path_X":[100,101,99,102],"path_Y":[50,51,49,52],"times":[0,0.33,0.67,1.0]})"},
        {"stochastic/theory/martingale-test",
         R"({"paths":[[100,102,101,103],[100,99,101,100]],"times":[0,0.33,0.67,1.0],"drift":0.0})"},
        {"stochastic/theory/girsanov/measure-change",
         R"({"paths":[[100,102,101,103],[100,99,101,100]],"times":[0,0.33,0.67,1.0],"theta":0.5,"T":1.0})"},
        {"stochastic/theory/girsanov/risk-neutral-drift", R"({"mu":0.05,"r":0.02,"sigma":0.2})"},
        // ── Generated from the live OpenAPI spec (api.fincept.in/openapi.json) ───────────
        // Every body below validates against its request schema (required fields, types,
        // enums). Values are illustrative starting points: edit them to your own inputs.
        // curves
        {"curves/build",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052]})"},
        {"curves/zero-rate",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"query_tenor":3.0})"},
        {"curves/forward-rate",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"start_tenor":1.0,"end_tenor":2.0})"},
        {"curves/discount-factor",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"query_tenor":3.0})"},
        {"curves/curve-points",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"query_tenors":[0.75,1.5,3,7]})"},
        {"curves/interpolate", R"({"x":[1,2,3,4,5],"y":[2.1,3.9,6.2,7.8,10.1],"query_x":[1.5,2.5,3.5]})"},
        {"curves/interpolate-derivative", R"({"x":[1,2,3,4,5],"y":[2.1,3.9,6.2,7.8,10.1],"query_x":2.5})"},
        {"curves/instantaneous-forward",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"query_tenor":3.0})"},
        {"curves/parallel-shift",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"shift_bps":10})"},
        {"curves/twist",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"pivot_years":5,"short_shift_bps":-10,"long_shift_bps":10})"},
        {"curves/butterfly",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"belly_years":5,"wing_shift_bps":10,"belly_shift_bps":-5})"},
        {"curves/key-rate-shift",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"tenor_years":5,"shift_bps":10})"},
        {"curves/nelson-siegel/fit",
         R"({"tenors":[0.25,0.5,1,2,5,10],"rates":[0.04,0.042,0.045,0.047,0.05,0.052]})"},
        {"curves/nelson-siegel/evaluate",
         R"({"beta0":0.05,"beta1":-0.02,"beta2":0.01,"tau":1.5,"query_tenors":[0.75,1.5,3,7]})"},
        {"curves/nss/fit", R"({"tenors":[0.25,0.5,1,2,5,10],"rates":[0.04,0.042,0.045,0.047,0.05,0.052]})"},
        {"curves/nss/evaluate",
         R"({"beta0":0.05,"beta1":-0.02,"beta2":0.01,"beta3":0.005,"tau1":1.5,"tau2":5.0,)"
         R"("query_tenors":[0.75,1.5,3,7]})"},
        {"curves/roll",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"days":90})"},
        {"curves/scale",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"scale_factor":1.1})"},
        {"curves/time-shift",
         R"({"reference_date":"2024-01-15","tenors":[0.25,0.5,1,2,5,10],"values":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"shift_days":30,"query_tenors":[0.75,1.5,3,7]})"},
        {"curves/composite",
         R"({"reference_date":"2024-01-15","base_tenors":[0.25,0.5,1,2,5,10],"base_values":[0.04,0.042,)"
         R"(0.045,0.047,0.05,0.052],"forecast_tenors":[0.25,0.5,1,2,5,10],"forecast_values":[0.04,0.042,)"
         R"(0.045,0.047,0.05,0.052]})"},
        {"curves/proxy",
         R"({"reference_date":"2024-01-15","base_tenors":[0.25,0.5,1,2,5,10],"base_values":[0.04,0.042,)"
         R"(0.045,0.047,0.05,0.052],"spread_bps":100})"},
        {"curves/real-rate",
         R"({"reference_date":"2024-01-15","nominal_tenors":[0.25,0.5,1,2,5,10],"nominal_rates":[0.04,0.042,)"
         R"(0.045,0.047,0.05,0.052],"base_cpi":100,"inflation_tenors":[0.25,0.5,1,2,5,10],)"
         R"("inflation_rates":[0.02,0.021,0.022,0.023,0.024,0.025],"query_tenors":[0.75,1.5,3,7]})"},
        {"curves/monotonicity-check", R"({"x":[1,2,3,4,5],"y":[2.1,3.9,6.2,7.8,10.1],"query_x":[1.5,2.5,3.5]})"},
        {"curves/smoothness-penalty", R"({"x":[1,2,3,4,5],"y":[2.1,3.9,6.2,7.8,10.1],"query_x":[1.5,2.5,3.5]})"},
        {"curves/constrained-fit", R"({"tenors":[0.25,0.5,1,2,5,10],"rates":[0.04,0.042,0.045,0.047,0.05,0.052]})"},
        {"curves/multicurve/setup",
         R"({"reference_date":"2024-01-15","ois_tenors":[0.25,0.5,1,2,5,10],"ois_rates":[0.04,0.042,0.045,)"
         R"(0.047,0.05,0.052],"ibor_tenors":[0.25,0.5,1,2,5,10],"ibor_rates":[0.045,0.047,0.05,0.052,0.054,)"
         R"(0.055],"query_tenors":[0.75,1.5,3,7]})"},
        {"curves/multicurve/basis-spread",
         R"({"reference_date":"2024-01-15","ois_tenors":[0.25,0.5,1,2,5,10],"ois_rates":[0.04,0.042,0.045,)"
         R"(0.047,0.05,0.052],"ibor_tenors":[0.25,0.5,1,2,5,10],"ibor_rates":[0.045,0.047,0.05,0.052,0.054,)"
         R"(0.055],"query_tenor":3.0})"},
        {"curves/cross-currency-basis",
         R"({"reference_date":"2024-01-15","domestic_currency":"USD","foreign_currency":"EUR",)"
         R"("tenors":[0.25,0.5,1,2,5,10],"basis_spreads_bps":[-10,-12,-15,-18,-20,-22],"query_tenors":[0.75,)"
         R"(1.5,3,7]})"},
        {"curves/inflation/build",
         R"({"reference_date":"2024-01-15","base_cpi":100,"tenors":[0.25,0.5,1,2,5,10],)"
         R"("inflation_rates":[0.02,0.021,0.022,0.023,0.024,0.025]})"},
        {"curves/inflation/bootstrap",
         R"({"reference_date":"2024-01-15","base_cpi":100,"swap_tenors":[0.25,0.5,1,2,5,10],)"
         R"("swap_rates":[0.04,0.042,0.045,0.047,0.05,0.052],"nominal_tenors":[0.25,0.5,1,2,5,10],)"
         R"("nominal_rates":[0.04,0.042,0.045,0.047,0.05,0.052]})"},
        {"curves/inflation/seasonality",
         R"({"monthly_factors":[1.0,1.01,1.02,1.01,0.99,0.98,0.99,1.0,1.01,1.02,1.01,1.0],"cpi_values":[100,)"
         R"(100.2,100.5,100.9,101.2,101.5,101.9,102.2,102.5,102.9,103.2,103.5],"months":[1,2,3,4,5,6,7,8,9,)"
         R"(10,11,12]})"},
        // volatility
        {"volatility/surface/flat", R"({"volatility":0.2,"query_expiry":1.0,"query_strike":100})"},
        {"volatility/surface/from-points",
         R"({"tenors":[0.5,0.5,1,1,2],"strikes":[95,105,95,105,100],"vols":[0.22,0.21,0.21,0.2,0.2]})"},
        {"volatility/surface/grid",
         R"({"expiries":[0.25,0.5,1.0,2.0],"strikes":[90,95,100,105,110],"vol_matrix":[[0.25,0.22,0.2,0.21,)"
         R"(0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23]],)"
         R"("query_expiry":1.0,"query_strike":100})"},
        {"volatility/surface/smile",
         R"({"expiries":[0.25,0.5,1.0,2.0],"strikes":[90,95,100,105,110],"vol_matrix":[[0.25,0.22,0.2,0.21,)"
         R"(0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23]],)"
         R"("query_expiry":1.0,"query_strikes":[97,102]})"},
        {"volatility/surface/term-structure",
         R"({"expiries":[0.25,0.5,1.0,2.0],"volatilities":[0.22,0.21,0.2,0.2],"query_expiry":1.0})"},
        {"volatility/surface/total-variance",
         R"({"expiries":[0.25,0.5,1.0,2.0],"strikes":[90,95,100,105,110],"vol_matrix":[[0.25,0.22,0.2,0.21,)"
         R"(0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23]],)"
         R"("query_expiry":1.0,"query_strike":100})"},
        {"volatility/sabr/implied-vol",
         R"({"forward":100,"strike":100,"expiry":1.0,"alpha":0.2,"beta":0.5,"rho":-0.5,"nu":0.4})"},
        {"volatility/sabr/calibrate",
         R"({"forward":100,"expiry":1.0,"strikes":[90,95,100,105,110],"market_vols":[0.25,0.22,0.2,0.21,0.23]})"},
        {"volatility/sabr/smile",
         R"({"forward":100,"expiry":1.0,"alpha":0.2,"beta":0.5,"rho":-0.5,"nu":0.4,"strikes":[90,95,100,105,)"
         R"(110]})"},
        {"volatility/sabr/normal-vol",
         R"({"forward":100,"strike":100,"expiry":1.0,"alpha":0.2,"beta":0.5,"rho":-0.5,"nu":0.4})"},
        {"volatility/sabr/density",
         R"({"forward":100,"expiry":1.0,"alpha":0.2,"beta":0.5,"rho":-0.5,"nu":0.4,"strikes":[90,95,100,105,)"
         R"(110]})"},
        {"volatility/sabr/dynamics",
         R"({"forward":100,"expiry":1.0,"alpha":0.2,"beta":0.5,"rho":-0.5,"nu":0.4,"strike":100})"},
        {"volatility/local-vol/constant", R"({"volatility":0.2,"spot":100,"time":1.0})"},
        {"volatility/local-vol/implied-to-local",
         R"({"spot":100,"rate":0.05,"strike":100,"expiry":1.0,"implied_vol":0.2})"},
        // models
        {"models/short-rate/bond-price", R"({"maturities":[0.25,0.5,1.0,2.0]})"},
        {"models/short-rate/bond-option",
         R"({"model":"vasicek","kappa":0.1,"theta":0.05,"sigma":0.01,"r0":0.03,"T":1.0})"},
        {"models/short-rate/yield-curve", R"({"maturities":[0.25,0.5,1.0,2.0]})"},
        {"models/short-rate/monte-carlo",
         R"({"model":"vasicek","kappa":0.1,"theta":0.05,"sigma":0.01,"r0":0.03,"T":1.0,"n_steps":252,)"
         R"("n_paths":100})"},
        {"models/hull-white/calibrate",
         R"({"market_tenors":[0.25,0.5,1,2,5,10],"market_rates":[0.04,0.042,0.045,0.047,0.05,0.052]})"},
        {"models/heston/price",
         R"({"S0":100,"v0":0.04,"r":0.05,"kappa":2.0,"theta":0.04,"sigma_v":0.3,"rho":-0.5,"strike":100,"T":1.0})"},
        {"models/heston/monte-carlo",
         R"({"S0":100,"v0":0.04,"r":0.05,"kappa":2.0,"theta":0.04,"sigma_v":0.3,"rho":-0.5,"strike":100,"T":1.0})"},
        {"models/heston/implied-vol",
         R"({"S0":100,"v0":0.04,"r":0.05,"kappa":2.0,"theta":0.04,"sigma_v":0.3,"rho":-0.5,"strike":100,"T":1.0})"},
        {"models/merton/price",
         R"({"S":100,"K":100,"T":1.0,"r":0.05,"sigma":0.2,"lambda_jump":0.5,"mu_jump":-0.05,"sigma_jump":0.15})"},
        {"models/merton/fft",
         R"({"S":100,"K":100,"T":1.0,"r":0.05,"sigma":0.2,"lambda_jump":0.5,"mu_jump":-0.05,"sigma_jump":0.15})"},
        {"models/kou/price",
         R"({"S":100,"K":100,"T":1.0,"r":0.05,"sigma":0.2,"lambda_jump":0.5,"p":0.95,"eta1":10.0,"eta2":5.0})"},
        {"models/dupire/price", R"({"spot":100,"strike":100,"T":1.0})"},
        {"models/svi/calibrate",
         R"({"strikes":[90,95,100,105,110],"market_vols":[0.25,0.22,0.2,0.21,0.23],"forward":100,"T":1.0})"},
        {"models/variance-gamma/price",
         R"({"S":100,"K":100,"T":1.0,"r":0.05,"sigma":0.2,"theta_vg":-0.1,"nu":0.4})"},
        // risk
        {"risk/var/parametric", R"({"portfolio_value":1000000,"volatility":0.2})"},
        {"risk/var/historical",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"risk/var/component",
         R"({"returns_by_factor":{"equity":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,)"
         R"(-0.013,-0.0138,0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,)"
         R"(0.0015,0.002,-0.0034,-0.0003],"rates":[0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,)"
         R"(-0.0015,-0.0158,0.0004,-0.0082,0.0028,0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,)"
         R"(0.0045,0.0015,-0.0107,0.0066,-0.0085]},"weights":{"equity":0.6,"rates":0.4}})"},
        {"risk/var/incremental",
         R"({"portfolio_returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,)"
         R"(-0.0138,0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,)"
         R"(-0.0034,-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,)"
         R"(-0.0082,0.0028,0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,)"
         R"(0.0066,-0.0085,0.021,-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,)"
         R"(-0.0082],"new_position_returns":[-0.0095,0.0047,0.01,-0.0032,-0.0104,0.0008,0.0056,0.011,)"
         R"(-0.0094,-0.01,0.0076,0.0055,0.0148,0.01,0.0004,-0.0011,0.024,-0.0012,-0.0056,-0.0087,0.0022,)"
         R"(0.0026,-0.0017,0.0008,0.0037,0.0074,0.011,0.0032,-0.0144,0.0075,-0.0112,-0.0002,-0.0116,0.0013,)"
         R"(-0.0056,0.0032,0.0304,0.0009,0.0088,-0.0104,-0.0012,0.0072,0.0005,0.0046,0.0022,-0.0076,0.0063,)"
         R"(-0.0058,0.0178,-0.0032,0.0073,0.0013,0.0097,-0.004,0.0101,0.0002,0.0124,0.0068,0.0026,-0.0056],)"
         R"("position_weight":0.1})"},
        {"risk/var/marginal",
         R"({"portfolio_returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,)"
         R"(-0.0138,0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,)"
         R"(-0.0034,-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,)"
         R"(-0.0082,0.0028,0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,)"
         R"(0.0066,-0.0085,0.021,-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,)"
         R"(-0.0082],"new_position_returns":[-0.0095,0.0047,0.01,-0.0032,-0.0104,0.0008,0.0056,0.011,)"
         R"(-0.0094,-0.01,0.0076,0.0055,0.0148,0.01,0.0004,-0.0011,0.024,-0.0012,-0.0056,-0.0087,0.0022,)"
         R"(0.0026,-0.0017,0.0008,0.0037,0.0074,0.011,0.0032,-0.0144,0.0075,-0.0112,-0.0002,-0.0116,0.0013,)"
         R"(-0.0056,0.0032,0.0304,0.0009,0.0088,-0.0104,-0.0012,0.0072,0.0005,0.0046,0.0022,-0.0076,0.0063,)"
         R"(-0.0058,0.0178,-0.0032,0.0073,0.0013,0.0097,-0.004,0.0101,0.0002,0.0124,0.0068,0.0026,-0.0056],)"
         R"("position_weight":0.1})"},
        {"risk/var/es-optimization",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]]})"},
        {"risk/backtest",
         R"({"actual_returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,)"
         R"(0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082],)"
         R"("var_estimates":[-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,)"
         R"(-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,)"
         R"(-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,)"
         R"(-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02,-0.02]})"},
        {"risk/stress/scenario", R"({"name":"scenario","factor_shocks":{"equity":-0.2,"rates":0.01}})"},
        {"risk/correlation-stress", R"({"correlation_matrix":[[1,0.5,0.3],[0.5,1,0.4],[0.3,0.4,1]]})"},
        {"risk/tail-risk/comprehensive",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"risk/evt/gpd",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"risk/evt/gev",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"risk/evt/hill",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"risk/xva/cva",
         R"({"exposure_paths":[[0,1,2,1,0],[0,2,3,2,0],[0,0.5,1,0.5,0]],"counterparty_spread":0.01})"},
        {"risk/xva/pfe",
         R"({"exposure_paths":[[0,1,2,1,0],[0,2,3,2,0],[0,0.5,1,0.5,0]],"counterparty_spread":0.01})"},
        {"risk/copula/sample", R"({"copula_type":"gaussian"})"},
        {"risk/portfolio-risk/exposure-profile", R"({"paths":[[100,102,101,103],[100,99,101,100]]})"},
        {"risk/portfolio-risk/optimal-hedge", R"({"portfolio_delta":1000,"hedge_delta":0.5})"},
        {"risk/sensitivities/greeks", R"({"S":100.0,"K":100.0,"T":1.0,"r":0.05,"sigma":0.2,"q":0.0})"},
        {"risk/sensitivities/key-rate-duration",
         R"({"cashflows":[5,5,5,5,105],"times":[1,2,3,4,5],"curve_rates":[0.04,0.042,0.045,0.047,0.05]})"},
        {"risk/sensitivities/parallel-shift",
         R"({"cashflows":[5,5,5,5,105],"times":[1,2,3,4,5],"curve_rates":[0.04,0.042,0.045,0.047,0.05]})"},
        {"risk/sensitivities/twist",
         R"({"cashflows":[5,5,5,5,105],"times":[1,2,3,4,5],"curve_rates":[0.04,0.042,0.045,0.047,0.05]})"},
        {"risk/sensitivities/cross-gamma", R"({"S":100.0,"K":100.0,"T":1.0,"r":0.05,"sigma":0.2,"bump":0.01})"},
        // portfolio
        {"portfolio/optimize/min-variance",
         R"({"expected_returns":[0.08,0.1,0.12],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],)"
         R"([0.004,0.012,0.0625]]})"},
        {"portfolio/optimize/max-sharpe",
         R"({"expected_returns":[0.08,0.1,0.12],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],)"
         R"([0.004,0.012,0.0625]]})"},
        {"portfolio/optimize/efficient-frontier",
         R"({"expected_returns":[0.08,0.1,0.12],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],)"
         R"([0.004,0.012,0.0625]]})"},
        {"portfolio/optimize/target-return",
         R"({"expected_returns":[0.08,0.1,0.12],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],)"
         R"([0.004,0.012,0.0625]],"target_return":0.1})"},
        {"portfolio/black-litterman/equilibrium",
         R"({"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,0.012,0.0625]],)"
         R"("market_caps":[1000,800,600]})"},
        {"portfolio/black-litterman/posterior",
         R"({"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,0.012,0.0625]],)"
         R"("market_caps":[1000,800,600]})"},
        {"portfolio/risk-parity",
         R"({"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,0.012,0.0625]]})"},
        {"portfolio/risk/inverse-volatility",
         R"({"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,0.012,0.0625]]})"},
        {"portfolio/risk/var",
         R"({"weights":[0.4,0.3,0.3],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,)"
         R"(0.012,0.0625]]})"},
        {"portfolio/risk/cvar",
         R"({"weights":[0.4,0.3,0.3],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,)"
         R"(0.012,0.0625]]})"},
        {"portfolio/risk/incremental-var",
         R"({"weights":[0.4,0.3,0.3],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,)"
         R"(0.012,0.0625]]})"},
        {"portfolio/risk/contribution",
         R"({"weights":[0.4,0.3,0.3],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,)"
         R"(0.012,0.0625]]})"},
        {"portfolio/risk/ratios",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"portfolio/risk/comprehensive",
         R"({"weights":[0.4,0.3,0.3],"covariance_matrix":[[0.04,0.006,0.004],[0.006,0.09,0.012],[0.004,)"
         R"(0.012,0.0625]]})"},
        {"portfolio/risk/portfolio-comprehensive",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]],"weights":[0.4,0.3,0.3]})"},
        // instruments
        {"instruments/bond/fixed/price",
         R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15","coupon_rate":0.05,"yield_rate":0.045})"},
        {"instruments/bond/fixed/yield",
         R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15","coupon_rate":0.05,"clean_price":99.5})"},
        {"instruments/bond/fixed/analytics",
         R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15","coupon_rate":0.05,"yield_rate":0.045})"},
        {"instruments/bond/fixed/cashflows",
         R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15","coupon_rate":0.05})"},
        {"instruments/bond/zero-coupon/price", R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15"})"},
        {"instruments/bond/inflation-linked",
         R"({"issue_date":"2024-01-15","maturity_date":"2029-01-15","coupon_rate":0.05,"base_cpi":100,)"
         R"("current_cpi":103})"},
        {"instruments/swap/irs/value",
         R"({"effective_date":"2024-01-15","maturity_date":"2029-01-15","notional":1000000,)"
         R"("fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"reference_date":"2024-01-15"})"},
        {"instruments/swap/irs/par-rate",
         R"({"effective_date":"2024-01-15","maturity_date":"2029-01-15","notional":1000000,)"
         R"("fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"reference_date":"2024-01-15"})"},
        {"instruments/swap/irs/dv01",
         R"({"effective_date":"2024-01-15","maturity_date":"2029-01-15","notional":1000000,)"
         R"("fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"reference_date":"2024-01-15"})"},
        {"instruments/fra/value",
         R"({"trade_date":"2024-01-15","start_months":6,"end_months":9,"notional":1000000,)"
         R"("fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"reference_date":"2024-01-15"})"},
        {"instruments/fra/break-even",
         R"({"trade_date":"2024-01-15","start_months":6,"end_months":9,"notional":1000000,)"
         R"("fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05,0.052],"reference_date":"2024-01-15"})"},
        {"instruments/ois/value",
         R"({"reference_date":"2024-01-15","effective_date":"2024-01-15","maturity_date":"2029-01-15",)"
         R"("notional":1000000,"fixed_rate":0.045,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,)"
         R"(0.042,0.045,0.047,0.05,0.052]})"},
        {"instruments/cds/value",
         R"({"reference_date":"2024-01-15","effective_date":"2024-01-15","maturity_date":"2029-01-15",)"
         R"("notional":1000000,"spread_bps":100,"curve_tenors":[0.25,0.5,1,2,5,10],"curve_rates":[0.04,)"
         R"(0.042,0.045,0.047,0.05,0.052]})"},
        {"instruments/cds/hazard-rate", R"({"spread_bps":100})"},
        {"instruments/cds/survival-probability", R"({"hazard_rate":0.02,"time_horizon":1.0})"},
        {"instruments/fx/forward",
         R"({"spot_rate":1.1,"base_rate":0.05,"quote_rate":0.03,"time_to_maturity":1.0,"notional":1000000})"},
        {"instruments/fx/garman-kohlhagen",
         R"({"spot":100,"strike":100,"domestic_rate":0.05,"foreign_rate":0.03,"volatility":0.2,)"
         R"("time_to_expiry":1.0})"},
        {"instruments/money-market/deposit",
         R"({"start_date":"2024-01-15","maturity_date":"2024-07-15","notional":1000000,"rate":0.05})"},
        {"instruments/money-market/repo",
         R"({"start_date":"2024-01-15","end_date":"2024-04-15","collateral_value":1000000,"repo_rate":0.04})"},
        {"instruments/money-market/tbill",
         R"({"settlement_date":"2024-01-17","maturity_date":"2024-07-15","discount_rate":0.05})"},
        {"instruments/commodity/future",
         R"({"commodity_type":"oil","spot_price":70,"delivery_date":"2024-12-15","trade_date":"2024-01-15",)"
         R"("futures_price":72})"},
        {"instruments/futures/stir",
         R"({"price":95.5,"contract_date":"2024-01-15","reference_date":"2024-01-15"})"},
        {"instruments/equity/variance-swap",
         R"({"strike_variance":0.04,"realized_returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,)"
         R"(0.0057,0.0125,-0.013,-0.0138,0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,)"
         R"(-0.0121,0.0015,0.002,-0.0034,-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,)"
         R"(-0.0158,0.0004,-0.0082,0.0028,0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,)"
         R"(0.0015,-0.0107,0.0066,-0.0085,0.021,-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,)"
         R"(0.0073,0.002,-0.0082],"notional":1000000})"},
        {"instruments/equity/volatility-swap",
         R"({"strike_vol":0.2,"realized_returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,)"
         R"(0.0125,-0.013,-0.0138,0.0082,0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,)"
         R"(-0.0121,0.0015,0.002,-0.0034,-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,)"
         R"(-0.0158,0.0004,-0.0082,0.0028,0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,)"
         R"(0.0015,-0.0107,0.0066,-0.0085,0.021,-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,)"
         R"(0.0073,0.002,-0.0082],"notional":1000000})"},
        // solver
        {"solver/finance/bond-yield", R"({"price":100,"coupon":0.05,"maturity":5.0})"},
        {"solver/finance/duration", R"({"coupon":0.05,"maturity":5.0,"ytm":0.045})"},
        {"solver/finance/modified-duration", R"({"coupon":0.05,"maturity":5.0,"ytm":0.045})"},
        {"solver/finance/convexity", R"({"coupon":0.05,"maturity":5.0,"ytm":0.045})"},
        {"solver/finance/convexity-adjustment", R"({"forward_rate":0.05,"volatility":0.2,"t1":1.0,"t2":2.0})"},
        {"solver/finance/pv01",
         R"({"notional":1000000,"discount_factors":[0.9875,0.975,0.9625,0.95,0.9375],"times":[0.5,1,1.5,2,2.5]})"},
        {"solver/finance/implied-vol", R"({"price":10.45,"spot":100,"strike":100,"time":1.0,"rate":0.05})"},
        {"solver/finance/implied-vol-black76",
         R"({"price":7.58,"forward":100,"strike":100,"time":1.0,"rate":0.05})"},
        {"solver/finance/forward-rate", R"({"df1":0.98,"df2":0.95,"t1":1.0,"t2":2.0})"},
        {"solver/finance/zero-rate", R"({"discount_factor":0.95,"t":1.0})"},
        {"solver/finance/discount-factor", R"({"rate":0.05,"t":1.0})"},
        {"solver/finance/par-rate",
         R"({"discount_factors":[0.9875,0.975,0.9625,0.95,0.9375],"times":[0.5,1,1.5,2,2.5]})"},
        {"solver/finance/z-spread",
         R"({"price":98,"cashflows":[5,5,5,5,105],"times":[1,2,3,4,5],"base_rates":[0.04,0.042,0.045,0.047,)"
         R"(0.05]})"},
        {"solver/finance/i-spread", R"({"bond_yield":0.045,"swap_rate":0.045})"},
        {"solver/finance/g-spread", R"({"bond_yield":0.045,"govt_yield":0.04})"},
        {"solver/finance/oas",
         R"({"price":100,"scenario_cashflows":[[5,5,5,5,105],[5,5,105,0,0],[5,5,5,105,0]],"times":[1,2,3,4,)"
         R"(5],"probabilities":[0.5,0.3,0.2],"base_rates":[0.04,0.042,0.045,0.047,0.05,0.052],)"
         R"("base_times":[0.25,0.5,1,2,5,10]})"},
        {"solver/finance/asw-spread",
         R"({"bond_price":99.5,"coupon":0.05,"maturity":5.0,"base_rates":[0.04,0.042,0.045,0.047,0.05,)"
         R"(0.052],"base_times":[0.25,0.5,1,2,5,10]})"},
        {"solver/finance/basis", R"({"spot":100,"futures":101.0})"},
        {"solver/finance/carry", R"({"spot":100,"futures":101.0,"time_to_expiry":1.0})"},
        {"solver/finance/implied-repo-rate", R"({"spot":100,"futures":101.0,"time_to_expiry":1.0})"},
        {"solver/finance/forward-futures-conversion", R"({"rate":0.05,"volatility":0.2,"t1":1.0,"t2":2.0})"},
        {"solver/finance/irr", R"({"cashflows":[5,5,5,5,105]})"},
        {"solver/finance/xirr", R"({"cashflows":[-1000,300,400,500],"dates":[0,0.5,1.0,1.5]})"},
        {"solver/calibration/vasicek",
         R"({"zero_rates":[0.04,0.042,0.045,0.047,0.05,0.052],"times":[0.25,0.5,1,2,5,10]})"},
        // economics
        {"economics/equilibrium/cobb-douglas", R"({"alphas":[0.5,0.5],"prices":[1.0,2.0]})"},
        {"economics/equilibrium/ces", R"({"alphas":[0.5,0.5],"rho":0.5,"prices":[1.0,2.0]})"},
        {"economics/equilibrium/walrasian", R"({"endowments":[[10,5],[5,10]],"alphas":[0.5,0.5]})"},
        {"economics/equilibrium/exchange-economy", R"({"endowments":[[10,5],[5,10]],"alphas":[0.5,0.5]})"},
        {"economics/games/create", R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]]})"},
        {"economics/games/mixed-nash", R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]]})"},
        {"economics/games/nash-check",
         R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]],"strategy_1":[0.5,0.5],)"
         R"("strategy_2":[0.5,0.5]})"},
        {"economics/games/best-response",
         R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]],"strategy_1":[0.5,0.5],)"
         R"("strategy_2":[0.5,0.5]})"},
        {"economics/games/eliminate-dominated",
         R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]]})"},
        {"economics/games/fictitious-play", R"({"payoff_matrix_1":[[3,0],[5,1]],"payoff_matrix_2":[[3,5],[0,1]]})"},
        {"economics/auctions/run", R"({"auction_type":"first_price","n_bidders":5})"},
        {"economics/auctions/simulate", R"({"auction_type":"first_price","n_bidders":5})"},
        {"economics/auctions/equilibrium-bid", R"({"auction_type":"first_price","n_bidders":5})"},
        {"economics/auctions/expected-revenue", R"({"auction_type":"first_price","n_bidders":5})"},
        {"economics/utility/cara", R"({"risk_aversion":2.0,"wealth":100})"},
        {"economics/utility/crra", R"({"gamma":2.0,"wealth":100})"},
        {"economics/utility/log", R"({"wealth":100})"},
        {"economics/utility/quadratic", R"({"wealth":100})"},
        {"economics/utility/expected-utility",
         R"({"utility_type":"crra","outcomes":[100,50,0],"param":2.0,"probabilities":[0.5,0.3,0.2]})"},
        {"economics/utility/certainty-equivalent",
         R"({"utility_type":"crra","outcomes":[100,50,0],"param":2.0,"probabilities":[0.5,0.3,0.2]})"},
        {"economics/utility/certainty-equivalent-approximation",
         R"({"mean":0.0,"variance":1.0,"risk_aversion":2.0})"},
        {"economics/utility/risk-premium",
         R"({"utility_type":"crra","outcomes":[100,50,0],"param":2.0,"probabilities":[0.5,0.3,0.2]})"},
        {"economics/utility/prospect-theory",
         R"({"alpha":0.88,"beta":0.88,"lambda_param":2.25,"wealth":0.0,"outcome":100.0})"},
        // regulatory
        {"regulatory/basel/capital-ratios",
         R"({"cet1_capital":120,"tier1_capital":140,"total_capital":180,"risk_weighted_assets":1000})"},
        {"regulatory/basel/operational-rwa", R"({"gross_income_3y":[100,110,120]})"},
        {"regulatory/saccr/ead", R"({"mtm_value":5,"addon":10})"},
        {"regulatory/ifrs9/stage-assessment",
         R"({"days_past_due":35,"rating_downgrade":2,"pd_increase_ratio":2.5})"},
        {"regulatory/ifrs9/sicr", R"({"origination_pd":0.01,"current_pd":0.03})"},
        {"regulatory/ifrs9/ecl-12m", R"({"pd_12m":0.02,"lgd":0.4,"ead":1000000})"},
        {"regulatory/ifrs9/ecl-lifetime",
         R"({"pd_curve":[0.01,0.015,0.02,0.025],"lgd":0.4,"ead_curve":[100,90,80,70],"discount_rates":[0.05,)"
         R"(0.05,0.05,0.05]})"},
        {"regulatory/liquidity/lcr",
         R"({"hqla_level1":100,"hqla_level2a":20,"retail_deposits_stable":200,)"
         R"("retail_deposits_less_stable":150,"unsecured_wholesale":120,"inflows":30})"},
        {"regulatory/liquidity/nsfr",
         R"({"capital":100,"stable_deposits_retail":200,"less_stable_deposits_retail":150,)"
         R"("wholesale_funding":200,"cash_and_reserves":50,"securities_level1":80,"loans_retail":250,)"
         R"("loans_corporate":300,"other_assets":40})"},
        {"regulatory/stress/capital-projection",
         R"({"initial_capital":100,"initial_rwa":1000,"earnings":[10,12,11],"losses":[5,8,6],)"
         R"("rwa_changes":[20,10,5]})"},
        // scheduling
        {"scheduling/calendar/is-business-day", R"({"date":"2024-01-15"})"},
        {"scheduling/calendar/next-business-day", R"({"date":"2024-01-15"})"},
        {"scheduling/calendar/previous-business-day", R"({"date":"2024-01-15"})"},
        {"scheduling/calendar/business-days-between", R"({"start_date":"2024-01-15","end_date":"2024-03-15"})"},
        {"scheduling/calendar/add-business-days", R"({"date":"2024-01-15","days":10})"},
        {"scheduling/daycount/year-fraction", R"({"start_date":"2024-01-15","end_date":"2024-07-15"})"},
        {"scheduling/daycount/day-count", R"({"start_date":"2024-01-15","end_date":"2024-07-15"})"},
        {"scheduling/daycount/batch-year-fraction",
         R"({"date_pairs":[["2024-01-15","2024-07-15"],["2024-01-15","2025-01-15"]]})"},
        {"scheduling/adjustment/adjust-date", R"({"date":"2024-01-15"})"},
        {"scheduling/adjustment/batch-adjust",
         R"({"dates":["2024-01-15","2024-07-15","2025-01-15","2025-07-15"]})"},
        {"scheduling/schedule/generate", R"({"effective_date":"2024-01-15","termination_date":"2029-01-15"})"},
        // numerical
        {"numerical/differentiation/derivative", R"({"func_name":"sin","x":1.0})"},
        {"numerical/differentiation/gradient", R"({"func_name":"sin","x":[1.0,2.0]})"},
        {"numerical/differentiation/hessian", R"({"func_name":"sin","x":[1.0,2.0]})"},
        {"numerical/fft/forward",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"numerical/fft/inverse",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"numerical/fft/convolve", R"({"a":[1,2,3],"b":[0,1,0.5]})"},
        {"numerical/integration/quadrature", R"({"func_name":"sin","a":0.0,"b":3.14159})"},
        {"numerical/integration/monte-carlo", R"({"func_name":"sin","bounds":[[0.0,3.14159]]})"},
        {"numerical/integration/stratified", R"({"func_name":"sin","a":0.0,"b":3.14159})"},
        {"numerical/interpolation/evaluate", R"({"x_data":[1,2,3,4,5],"y_data":[2.1,3.9,6.2,7.8,10.1],"xi":2.5})"},
        {"numerical/interpolation/spline-curve",
         R"({"x_data":[1,2,3,4,5],"y_data":[2.1,3.9,6.2,7.8,10.1],"xi":[1.5,2.5,3.5]})"},
        {"numerical/interpolation/spline-derivative",
         R"({"x_data":[1,2,3,4,5],"y_data":[2.1,3.9,6.2,7.8,10.1],"xi":2.5})"},
        {"numerical/linalg/matmul", R"({"A":[[4,1],[1,3]],"B":[[1,2],[3,4]]})"},
        {"numerical/linalg/matvec", R"({"A":[[4,1],[1,3]],"x":[1.0,2.0]})"},
        {"numerical/linalg/solve", R"({"A":[[4,1],[1,3]],"b":[1.0,2.0]})"},
        {"numerical/linalg/inverse", R"({"A":[[4,1],[1,3]]})"},
        {"numerical/linalg/decompose", R"({"A":[[4,1],[1,3]]})"},
        {"numerical/linalg/dot", R"({"a":[1,2,3],"b":[4,5,6]})"},
        {"numerical/linalg/outer", R"({"a":[1,2,3],"b":[4,5,6]})"},
        {"numerical/linalg/norm", R"({"v":[3.0,4.0]})"},
        {"numerical/linalg/transpose", R"({"A":[[4,1],[1,3]]})"},
        {"numerical/linalg/lstsq", R"({"A":[[1,1],[1,2],[1,3]],"b":[1.0,2.0,2.0]})"},
        {"numerical/roots/find-1d", R"({"func_name":"sin","a":3.0,"b":4.0})"},
        {"numerical/roots/newton", R"({"func_name":"sin","x0":3.0})"},
        {"numerical/optimize/minimize", R"({"func_name":"sin","x0":[1.0]})"},
        // physics
        {"physics/entropy/shannon", R"({"probabilities":[0.3,0.5,0.2]})"},
        {"physics/entropy/renyi", R"({"probabilities":[0.3,0.5,0.2],"alpha":2.0})"},
        {"physics/entropy/tsallis", R"({"probabilities":[0.3,0.5,0.2],"q":2.0})"},
        {"physics/entropy/cross", R"({"p":[0.4,0.3,0.3],"q":[0.3,0.4,0.3]})"},
        {"physics/entropy/conditional", R"({"joint_prob":[[0.2,0.3],[0.1,0.4]]})"},
        {"physics/entropy/joint", R"({"joint_prob":[[0.2,0.3],[0.1,0.4]]})"},
        {"physics/entropy/differential", R"({"distribution":"gaussian","sigma":1.0,"rate":1.0})"},
        {"physics/entropy/mutual-information", R"({"joint_prob":[[0.2,0.3],[0.1,0.4]]})"},
        {"physics/entropy/transfer",
         R"({"x_history":[0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8],"y_history":[0.2,0.1,0.4,0.3,0.6,0.5,0.8,0.7]})"},
        {"physics/entropy/fisher-information",
         R"({"distribution":"gaussian","theta":1.0,"h":1e-05,"n_samples":10000})"},
        {"physics/entropy/markov-rate", R"({"transition_matrix":[[0.9,0.1],[0.2,0.8]]})"},
        {"physics/divergence/kl", R"({"p":[0.4,0.3,0.3],"q":[0.3,0.4,0.3]})"},
        {"physics/divergence/js", R"({"p":[0.4,0.3,0.3],"q":[0.3,0.4,0.3]})"},
        {"physics/boltzmann", R"({"energies":[0.0,1.0,2.0],"temperature":300.0})"},
        {"physics/ising", R"({"size":10,"J":1.0,"h":0.0,"temperature":2.0,"n_steps":1000})"},
        {"physics/ising/critical-temperature", R"({"J":1.0})"},
        {"physics/thermodynamics/ideal-gas", R"({"n":1.0,"temperature":300.0,"volume":1.0,"pressure":101325.0})"},
        {"physics/thermodynamics/van-der-waals", R"({"a":1.0,"b":0.1,"temperature":300.0,"volume":1.0})"},
        {"physics/thermodynamics/carnot", R"({"T_hot":500.0,"T_cold":300.0})"},
        {"physics/thermodynamics/free-energy", R"({"internal_energy":100.0,"temperature":300.0,"entropy":10.0})"},
        {"physics/thermodynamics/clausius-clapeyron", R"({"delta_H":40660,"T":350.0})"},
        {"physics/thermodynamics/joule-thomson", R"({"Cp":29.1,"V":0.0224,"T":300.0,"dV_dT_P":7.5e-05})"},
        // statistics
        {"statistics/distributions/normal/cdf", R"({"mu":0.0,"sigma":1.0,"x":0.0})"},
        {"statistics/distributions/normal/pdf", R"({"mu":0.0,"sigma":1.0,"x":0.0})"},
        {"statistics/distributions/normal/ppf", R"({"mu":0.0,"sigma":1.0,"p":0.5})"},
        {"statistics/distributions/normal/properties", R"({"mu":0.0,"sigma":1.0})"},
        {"statistics/distributions/lognormal/cdf", R"({"mu":0.0,"sigma":1.0,"x":1.0})"},
        {"statistics/distributions/lognormal/pdf", R"({"mu":0.0,"sigma":1.0,"x":1.0})"},
        {"statistics/distributions/lognormal/ppf", R"({"mu":0.0,"sigma":1.0,"p":0.5})"},
        {"statistics/distributions/lognormal/properties", R"({"mu":0.0,"sigma":1.0})"},
        {"statistics/distributions/student-t/cdf", R"({"df":5,"x":0.5})"},
        {"statistics/distributions/student-t/pdf", R"({"df":5,"x":0.5})"},
        {"statistics/distributions/student-t/properties", R"({"df":5})"},
        {"statistics/distributions/chi-squared/cdf", R"({"df":5,"x":3.0})"},
        {"statistics/distributions/chi-squared/pdf", R"({"df":5,"x":3.0})"},
        {"statistics/distributions/chi-squared/properties", R"({"df":5})"},
        {"statistics/distributions/f/pdf", R"({"df1":5,"df2":10,"x":1.0})"},
        {"statistics/distributions/f/properties", R"({"df1":5,"df2":10})"},
        {"statistics/distributions/gamma/cdf", R"({"alpha":2.0,"beta":1.0,"x":1.5})"},
        {"statistics/distributions/gamma/pdf", R"({"alpha":2.0,"beta":1.0,"x":1.5})"},
        {"statistics/distributions/gamma/properties", R"({"alpha":2.0,"beta":1.0})"},
        {"statistics/distributions/beta/cdf", R"({"alpha":2.0,"beta":5.0,"x":0.3})"},
        {"statistics/distributions/beta/pdf", R"({"alpha":2.0,"beta":5.0,"x":0.3})"},
        {"statistics/distributions/beta/properties", R"({"alpha":2.0,"beta":5.0})"},
        {"statistics/distributions/exponential/cdf", R"({"rate":1.5,"x":1.0})"},
        {"statistics/distributions/exponential/pdf", R"({"rate":1.5,"x":1.0})"},
        {"statistics/distributions/exponential/ppf", R"({"rate":1.5,"p":0.5})"},
        {"statistics/distributions/exponential/properties", R"({"rate":1.5})"},
        {"statistics/distributions/poisson/cdf", R"({"lam":3.0,"k":2})"},
        {"statistics/distributions/poisson/pmf", R"({"lam":3.0,"k":2})"},
        {"statistics/distributions/poisson/properties", R"({"lam":3.0})"},
        {"statistics/distributions/binomial/cdf", R"({"n":10,"p":0.3,"k":3})"},
        {"statistics/distributions/binomial/pmf", R"({"n":10,"p":0.3,"k":3})"},
        {"statistics/distributions/binomial/properties", R"({"n":10,"p":0.3})"},
        {"statistics/distributions/geometric/cdf", R"({"p":0.3,"k":3})"},
        {"statistics/distributions/geometric/pmf", R"({"p":0.3,"k":3})"},
        {"statistics/distributions/geometric/ppf", R"({"p":0.3,"q":0.5})"},
        {"statistics/distributions/geometric/properties", R"({"p":0.3})"},
        {"statistics/distributions/hypergeometric/cdf", R"({"N":50,"K":10,"n":5,"k":2})"},
        {"statistics/distributions/hypergeometric/pmf", R"({"N":50,"K":10,"n":5,"k":2})"},
        {"statistics/distributions/hypergeometric/properties", R"({"N":50,"K":10,"n":5})"},
        {"statistics/distributions/negative-binomial/cdf", R"({"r":5,"p":0.5,"k":3})"},
        {"statistics/distributions/negative-binomial/pmf", R"({"r":5,"p":0.5,"k":3})"},
        {"statistics/distributions/negative-binomial/properties", R"({"r":5,"p":0.5})"},
        {"statistics/distributions/pgf", R"({"distribution":"poisson","z":0.5,"lam":3.0})"},
        {"statistics/timeseries/ar/fit",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/ar/forecast",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/arima/fit",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/arima/forecast",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/ma/fit",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/garch/fit",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/garch/forecast",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/egarch/fit",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"statistics/timeseries/gjr-garch/fit",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        // ml
        {"ml/credit/scorecard",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[0,1,)"
         R"(0,1,1,0,1,0]})"},
        {"ml/credit/logistic-regression",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[0,1,)"
         R"(0,1,1,0,1,0]})"},
        {"ml/credit/woe-binning", R"({"feature":[0.1,0.5,0.3,0.9,0.7,0.2,0.8,0.4],"target":[0,1,0,1,1,0,1,0]})"},
        {"ml/credit/calibration", R"({"y_true":[0,1,0,1,1,0,1,0],"y_score":[0.2,0.8,0.3,0.7,0.6,0.4,0.9,0.1]})"},
        {"ml/credit/discrimination", R"({"y_true":[0,1,0,1,1,0,1,0],"y_score":[0.2,0.8,0.3,0.7,0.6,0.4,0.9,0.1]})"},
        {"ml/credit/performance", R"({"y_true":[0,1,0,1,1,0,1,0],"y_score":[0.2,0.8,0.3,0.7,0.6,0.4,0.9,0.1]})"},
        {"ml/credit/migration", R"({"transitions":[[0.9,0.08,0.02],[0.1,0.8,0.1],[0.0,0.0,1.0]]})"},
        {"ml/credit/beta-lgd",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],)"
         R"("lgd":[0.4,0.5,0.3,0.6,0.45,0.35,0.5,0.55]})"},
        {"ml/credit/two-stage-lgd",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],)"
         R"("lgd":[0.4,0.5,0.3,0.6,0.45,0.35,0.5,0.55],"cure_indicator":[0,1,0,1,0,0,1,0]})"},
        {"ml/regression/fit",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/regression/ensemble",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/regression/tree",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/regression/lgd",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/regression/ead",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/clustering/kmeans",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/clustering/hierarchical",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/clustering/dbscan",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/clustering/pca",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/clustering/isolation-forest",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/preprocessing/scale",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/preprocessing/transform",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/preprocessing/outliers",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]]})"},
        {"ml/preprocessing/winsorize",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/preprocessing/stationarity",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/features/lags",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/features/rolling",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082],"window":5})"},
        {"ml/features/calendar", R"({"dates":["2024-01-15","2024-07-15","2025-01-15","2025-07-15"]})"},
        {"ml/features/technical",
         R"({"prices":[100.0,101.35,102.4,102.94,102.87,102.25,101.25,100.17,99.31,98.91,99.1,99.89,101.1,)"
         R"(102.49,103.74,104.59,104.87,104.55,103.73,102.66,101.64,100.95,100.8,101.26,102.26,103.58,)"
         R"(104.95,106.06,106.68,106.69],"indicator":"rsi"})"},
        {"ml/features/financial-ratios", R"({"data":{"revenue":[100,110,125,130],"net_income":[10,12,14,13]}})"},
        {"ml/features/cross-sectional",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/validation/stability", R"({"expected":[10,20,30],"actual":[12,18,31]})"},
        {"ml/validation/discrimination-report",
         R"({"y_true":[0,1,0,1,1,0,1,0],"y_score":[0.2,0.8,0.3,0.7,0.6,0.4,0.9,0.1]})"},
        {"ml/validation/calibration-report",
         R"({"y_true":[0,1,0,1,1,0,1,0],"y_score":[0.2,0.8,0.3,0.7,0.6,0.4,0.9,0.1]})"},
        {"ml/validation/interpretability",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/timeseries/feature-importance",
         R"({"X":[[1.0,2.0],[2.0,1.0],[3.0,4.0],[4.0,3.0],[5.0,6.0],[6.0,5.0],[7.0,8.0],[8.0,7.0]],"y":[2.1,)"
         R"(3.9,6.2,7.8,10.1,2.1,3.9,6.2]})"},
        {"ml/changepoint/detect",
         R"({"data":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/gp/curve",
         R"({"tenors":[0.25,0.5,1,2,5,10],"rates":[0.04,0.042,0.045,0.047,0.05,0.052],"query_tenors":[0.75,)"
         R"(1.5,3,7]})"},
        {"ml/gp/vol-surface",
         R"({"strikes":[90,95,100,105,110],"expiries":[0.25,0.5,1.0,2.0],"vols":[[0.25,0.22,0.2,0.21,0.23],)"
         R"([0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23]],)"
         R"("query_strikes":[97,102],"query_expiries":[0.75,1.5]})"},
        {"ml/nn/curve",
         R"({"tenors":[0.25,0.5,1,2,5,10],"rates":[0.04,0.042,0.045,0.047,0.05,0.052],"query_tenors":[0.75,)"
         R"(1.5,3,7]})"},
        {"ml/nn/vol-surface",
         R"({"strikes":[90,95,100,105,110],"expiries":[0.25,0.5,1.0,2.0],"vols":[[0.25,0.22,0.2,0.21,0.23],)"
         R"([0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23],[0.25,0.22,0.2,0.21,0.23]],)"
         R"("query_strikes":[97,102],"query_expiries":[0.75,1.5]})"},
        {"ml/nn/portfolio",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]]})"},
        {"ml/hmm/fit",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/garch-hybrid",
         R"({"returns":[-0.0131,0.0046,0.0113,-0.0052,-0.0142,-0.0003,0.0057,0.0125,-0.013,-0.0138,0.0082,)"
         R"(0.0056,0.0173,0.0112,-0.0007,-0.0026,0.0287,-0.0028,-0.0082,-0.0121,0.0015,0.002,-0.0034,)"
         R"(-0.0003,0.0034,0.008,0.0125,0.0027,-0.0193,0.0081,-0.0152,-0.0015,-0.0158,0.0004,-0.0082,0.0028,)"
         R"(0.0367,-0.0001,0.0097,-0.0142,-0.0028,0.0077,-0.0006,0.0045,0.0015,-0.0107,0.0066,-0.0085,0.021,)"
         R"(-0.0052,0.0079,0.0004,0.0109,-0.0062,0.0114,-0.001,0.0143,0.0073,0.002,-0.0082]})"},
        {"ml/metrics/regression", R"({"y_true":[2,4,6,8,10],"y_pred":[2.1,3.9,6.2,7.8,10.1]})"},
        {"ml/metrics/classification", R"({"y_true":[0,1,0,1,1,0,1,0],"y_pred":[0,1,0,1,1,0,1,0]})"},
        {"ml/factor/statistical",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]]})"},
        {"ml/factor/cross-sectional",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]],"characteristics":[[0.5,1.2,-0.3],[0.1,0.8,0.6],[-0.2,0.4,1.1]]})"},
        {"ml/covariance/estimate",
         R"({"returns":[[0.0088,0.0055,0.0185],[-0.0037,0.0065,0.006],[0.0035,0.0027,0.0024],[-0.0062,)"
         R"(-0.0108,-0.004],[0.0069,0.017,0.0111],[0.012,0.009,0.0081],[0.0024,0.0089,0.0199],[-0.0032,)"
         R"(-0.0072,0.022],[0.0031,0.0111,0.01],[-0.0121,0.0272,0.0199],[0.0023,0.0087,0.0018],[-0.0093,)"
         R"(-0.0008,0.0045],[0.0127,0.0082,0.0011],[0.0153,-0.008,0.0091],[0.0126,-0.0028,-0.0193],[-0.0096,)"
         R"(0.003,0.0062],[0.0174,-0.0106,0.0004],[0.0075,-0.0013,-0.0257],[0.0093,0.0258,0.0114],[-0.0097,)"
         R"(-0.0041,0.0028],[0.0278,0.0072,0.0017],[0.0175,-0.0005,0.0209],[-0.0071,-0.0048,0.0083],[0.0161,)"
         R"(0.0076,0.0037]]})"},
        // analysis
        {"analysis/fundamentals/profitability",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/liquidity",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/efficiency",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/growth",
         R"({"current_income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("prior_income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("current_balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("prior_balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/solvency",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/cashflow",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("cash_flow":{"operating_cash_flow":180000,"capital_expenditures":60000,"depreciation":50000,)"
         R"("investing_cash_flow":-100000,"financing_cash_flow":-50000}})"},
        {"analysis/fundamentals/comprehensive",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("cash_flow":{"operating_cash_flow":180000,"capital_expenditures":60000,"depreciation":50000,)"
         R"("investing_cash_flow":-100000,"financing_cash_flow":-50000}})"},
        {"analysis/fundamentals/dupont",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/fundamentals/quality",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("cash_flow":{"operating_cash_flow":180000,"capital_expenditures":60000,"depreciation":50000,)"
         R"("investing_cash_flow":-100000,"financing_cash_flow":-50000}})"},
        {"analysis/fundamentals/capital-structure/wacc", R"({"debt":600000,"equity":1000000})"},
        {"analysis/fundamentals/capital-structure/optimal", R"({"debt":600000,"equity":1000000})"},
        {"analysis/ratios/profitability/roa",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/roe",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/roic",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/gross-margin",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/net-margin",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/ebitda-margin",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/operating-margin",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/roce",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/basic-earning-power",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/cash-roa",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/cash-roe",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/profitability/cash-roic",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,)"
         R"("ebit":200000,"ebitda":250000,"net_income":150000,"total_assets":2000000,"total_equity":1000000,)"
         R"("invested_capital":1500000,"operating_cash_flow":180000})"},
        {"analysis/ratios/liquidity/current-ratio",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/quick-ratio",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/cash-ratio",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/absolute-liquidity",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/defensive-interval",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/working-capital",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/liquidity/working-capital-ratio",
         R"({"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("marketable_securities":50000,"accounts_receivable":150000,"inventory":250000,)"
         R"("prepaid_expenses":20000,"operating_expenses":800000})"},
        {"analysis/ratios/leverage/debt-to-equity",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/interest-coverage",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/debt-to-assets",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/debt-to-capital",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/equity-ratio",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/equity-multiplier",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/ebitda-interest-coverage",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/cash-coverage",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/net-debt-to-ebitda",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/long-term-debt-to-equity",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/financial-leverage",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/capitalization-ratio",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/leverage/debt-service-coverage",
         R"({"total_debt":600000,"total_equity":1000000,"total_assets":2000000,"total_liabilities":1000000,)"
         R"("long_term_debt":450000,"ebit":200000,"ebitda":250000,"interest_expense":20000,)"
         R"("operating_cash_flow":180000,"total_capital":190000})"},
        {"analysis/ratios/valuation/pe",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/pb",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/ps",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/ev-ebitda",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/dividend-yield",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/pcf",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/pfcf",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/peg",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/ev-revenue",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/ev-fcf",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/earnings-yield",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/fcf-yield",
         R"({"market_price":50,"earnings_per_share":2.5,"book_value_per_share":25,"sales_per_share":20,)"
         R"("cash_flow_per_share":3.2,"dividends_per_share":1.0,"earnings_growth_rate":0.12,)"
         R"("enterprise_value":3600000,"ebitda":250000,"revenue":1000000,"free_cash_flow":120000})"},
        {"analysis/ratios/valuation/tobins-q", R"({"market_value":1500000,"replacement_cost":1200000})"},
        {"analysis/ratios/efficiency/asset-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/fixed-asset-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/inventory-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/receivables-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/payables-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/days-sales-outstanding",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/days-inventory-outstanding",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/days-payables-outstanding",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/cash-conversion-cycle",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/operating-cycle",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/working-capital-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/efficiency/equity-turnover",
         R"({"revenue":1000000,"cost_of_goods_sold":600000,"accounts_receivable":150000,"inventory":250000,)"
         R"("accounts_payable":120000,"total_assets":2000000,"fixed_assets":900000,"total_equity":1000000})"},
        {"analysis/ratios/growth/yoy", R"({"current_value":1500000,"previous_value":1000000})"},
        {"analysis/ratios/growth/cagr", R"({"current_value":1500000,"previous_value":1000000})"},
        {"analysis/ratios/growth/sustainable-growth", R"({"retention_ratio":0.6,"roe":0.15})"},
        {"analysis/ratios/growth/internal-growth", R"({"retention_ratio":0.6,"roa":0.075})"},
        {"analysis/ratios/cashflow/ocf-to-debt",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/ocf-ratio",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/ocf-margin", R"({"operating_cash_flow":180000,"revenue":1000000})"},
        {"analysis/ratios/cashflow/fcf",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/fcf-margin",
         R"({"operating_cash_flow":180000,"capital_expenditures":60000,"revenue":1000000})"},
        {"analysis/ratios/cashflow/cash-conversion-quality",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/capex-to-ocf",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/capex-to-depreciation",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/cash-coverage-of-dividends",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/cashflow/reinvestment-rate",
         R"({"operating_cash_flow":180000,"investing_cash_flow":-100000,"financing_cash_flow":-50000,)"
         R"("net_income":150000,"total_debt":600000,"current_liabilities":400000,)"
         R"("capital_expenditures":60000,"dividends":40000,"depreciation":50000,"total_assets":2000000})"},
        {"analysis/ratios/quality/accruals-ratio",
         R"({"net_income":150000,"operating_cash_flow":180000,"total_assets":2000000})"},
        {"analysis/ratios/quality/sloan-accruals",
         R"({"net_income":150000,"operating_cash_flow":180000,"total_assets":2000000})"},
        {"analysis/ratios/quality/earnings-persistence", R"({"earnings_history":[100,110,105,120,130,128]})"},
        {"analysis/ratios/quality/earnings-variability", R"({"earnings_history":[100,110,105,120,130,128]})"},
        {"analysis/ratios/quality/cash-earnings", R"({"operating_cash_flow":180000,"net_income":150000})"},
        {"analysis/ratios/quality/quality-of-earnings", R"({"operating_cash_flow":180000,"net_income":150000})"},
        {"analysis/valuation/dcf/fcff", R"({"fcff":[100000,110000,121000,133100,146410],"wacc":0.09})"},
        {"analysis/valuation/dcf/ddm", R"({"dividend":2.0,"growth_rate":0.03,"required_return":0.1})"},
        {"analysis/valuation/dcf/gordon-growth", R"({"dividend":2.0,"required_return":0.1,"growth_rate":0.03})"},
        {"analysis/valuation/dcf/two-stage", R"({"fcff":[100000,110000,121000,133100,146410],"wacc":0.09})"},
        {"analysis/valuation/dcf/wacc",
         R"({"equity_weight":0.6,"debt_weight":0.4,"cost_of_equity":0.1,"cost_of_debt":0.05,"tax_rate":0.25})"},
        {"analysis/valuation/dcf/terminal-value", R"({"fcf":146410,"growth_rate":0.03,"discount_rate":0.1})"},
        {"analysis/valuation/dcf/cost-of-equity", R"({"risk_free_rate":0.04,"beta":0.5,"market_premium":0.055})"},
        {"analysis/valuation/comparable",
         R"({"target":{"pe":15,"ev_ebitda":10,"pb":2.0},"peers":[{"pe":14,"ev_ebitda":9,"pb":1.8},{"pe":18,)"
         R"("ev_ebitda":11,"pb":2.4},{"pe":12,"ev_ebitda":8,"pb":1.5}]})"},
        {"analysis/valuation/screen",
         R"({"companies":[{"name":"A","pe":12,"pb":1.2,"roe":0.15},{"name":"B","pe":25,"pb":4.0,"roe":0.22},)"
         R"({"name":"C","pe":9,"pb":0.9,"roe":0.08}]})"},
        {"analysis/valuation/factor-models",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("market":{"market_cap":3000000,"share_price":50,"shares_outstanding":1.0,)"
         R"("enterprise_value":3600000}})"},
        {"analysis/valuation/credit/merton-model",
         R"({"equity_value":5000000,"debt_face":3000000,"risk_free_rate":0.04,"equity_volatility":0.4})"},
        {"analysis/valuation/credit/distance-to-default",
         R"({"asset_value":8000000,"debt":3000000,"asset_volatility":0.25,"risk_free_rate":0.04})"},
        {"analysis/valuation/credit/spread-from-pd", R"({"pd":0.02})"},
        {"analysis/valuation/credit/expected-loss", R"({"pd":0.02,"lgd":0.4,"ead":1000000})"},
        {"analysis/valuation/credit/rating-pd", R"({"rating":"BBB"})"},
        {"analysis/valuation/predictive/altman-z",
         R"({"working_capital":400000,"total_assets":2000000,"retained_earnings":500000,"ebit":200000,)"
         R"("market_cap":3000000,"total_liabilities":1000000,"revenue":1000000})"},
        {"analysis/valuation/predictive/piotroski-f",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("cash_flow":{"operating_cash_flow":180000,"capital_expenditures":60000,"depreciation":50000,)"
         R"("investing_cash_flow":-100000,"financing_cash_flow":-50000},"prior_income":{"revenue":1000000,)"
         R"("cost_of_goods_sold":600000,"gross_profit":400000,"operating_income":200000,"ebit":200000,)"
         R"("ebitda":250000,"net_income":150000,"interest_expense":20000,"depreciation":50000,)"
         R"("tax_expense":40000},"prior_balance":{"total_assets":2000000,"total_equity":1000000,)"
         R"("total_liabilities":1000000,"current_assets":800000,"current_liabilities":400000,"cash":200000,)"
         R"("inventory":250000,"accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/valuation/predictive/beneish-m",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000},)"
         R"("prior_income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("prior_balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/valuation/predictive/ohlson-o",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/valuation/predictive/springate",
         R"({"working_capital":400000,"total_assets":2000000,"retained_earnings":500000,"ebit":200000,)"
         R"("market_cap":3000000,"total_liabilities":1000000,"revenue":1000000})"},
        {"analysis/valuation/predictive/zmijewski",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/valuation/residual-income/eva", R"({"nopat":150000,"invested_capital":1000000,"wacc":0.09})"},
        {"analysis/valuation/residual-income/ri", R"({"net_income":150000,"equity":1000000,"cost_of_equity":0.1})"},
        {"analysis/valuation/residual-income/valuation",
         R"({"book_value":1000000,"residual_incomes":[50000,55000,60000],"cost_of_equity":0.1})"},
        {"analysis/valuation/startup/vc-method",
         R"({"investment":1000000,"exit_value":10000000,"years":5,"target_return":0.3})"},
        {"analysis/valuation/startup/first-chicago",
         R"({"scenarios":[{"name":"bull","probability":0.25,"exit_value":20000000},{"name":"base",)"
         R"("probability":0.5,"exit_value":10000000},{"name":"bear","probability":0.25,)"
         R"("exit_value":2000000}],"discount_rate":0.1,"years":5})"},
        {"analysis/valuation/startup/dilution", R"({"pre_money":4000000,"investment":1000000,"option_pool":0.1})"},
        {"analysis/valuation/proforma/adjustments",
         R"({"income":{"revenue":1000000,"cost_of_goods_sold":600000,"gross_profit":400000,)"
         R"("operating_income":200000,"ebit":200000,"ebitda":250000,"net_income":150000,)"
         R"("interest_expense":20000,"depreciation":50000,"tax_expense":40000},)"
         R"("balance":{"total_assets":2000000,"total_equity":1000000,"total_liabilities":1000000,)"
         R"("current_assets":800000,"current_liabilities":400000,"cash":200000,"inventory":250000,)"
         R"("accounts_receivable":150000,"accounts_payable":120000,"total_debt":600000,)"
         R"("long_term_debt":450000,"invested_capital":1500000,"retained_earnings":500000}})"},
        {"analysis/industry/banking",
         R"({"net_interest_income":80000,"total_interest_income":150000,"total_interest_expense":70000,)"
         R"("earning_assets":1800000,"total_assets":2000000,"total_deposits":1500000,"total_loans":1200000,)"
         R"("non_performing_loans":24000,"loan_loss_provisions":12000,"net_charge_offs":8000,)"
         R"("operating_expenses":70000,"total_revenue":120000,"fee_income":30000,"trading_income":10000,)"
         R"("net_income":150000,"tier1_capital":160000,"total_capital":190000,)"
         R"("risk_weighted_assets":1400000,"total_equity":1000000})"},
        {"analysis/industry/insurance",
         R"({"net_premiums_earned":500000,"losses_incurred":320000,"loss_adjustment_expenses":40000,)"
         R"("underwriting_expenses":110000,"net_investment_income":60000,"total_assets":2000000,)"
         R"("total_liabilities":1000000,"total_equity":1000000,"net_income":150000,)"
         R"("policyholders_surplus":300000,"reserves":700000,"reinsurance_recoverables":50000,)"
         R"("ceded_premiums":80000,"gross_premiums":580000})"},
        {"analysis/industry/reits",
         R"({"net_income":150000,"depreciation":50000,"capital_expenditures":60000,"total_revenue":120000,)"
         R"("total_assets":2000000,"total_debt":600000,"total_equity":1000000,"ebitda":250000,)"
         R"("interest_expense":20000,"share_price":50,"shares_outstanding":60000,"dividends_per_share":1.0})"},
        {"analysis/industry/utilities",
         R"({"total_revenue":120000,"operating_expenses":800000,"depreciation":50000,"net_income":150000,)"
         R"("interest_expense":20000,"total_assets":2000000,"total_equity":1000000,"total_debt":600000,)"
         R"("ebitda":250000,"capital_expenditures":60000})"},
    };
    return map;
}

} // namespace fincept::screens
