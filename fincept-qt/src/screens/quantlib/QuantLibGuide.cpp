// src/screens/quantlib/QuantLibGuide.cpp — see QuantLibGuide.h.
//
// One entry per endpoint group (first match wins, so specific prefixes come
// before general ones). A key ending in '/' matches every endpoint under it;
// any other key matches one endpoint exactly. Group names and summaries are
// marked for translation in the "QuantLibGuide" context.

#include "screens/quantlib/QuantLibGuide.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringList>

namespace fincept::screens {

namespace {

struct QlGuideEntry {
    const char* key;
    const char* group;   ///< readable group name (title prefix)
    const char* summary; ///< what the calculation returns
};

#define QLG(text) QT_TRANSLATE_NOOP("QuantLibGuide", text)

// clang-format off
constexpr QlGuideEntry kQlGuide[] = {
    // ── core ──
    {"core/types/", QLG("Core types"), QLG("Reference data and conversions for money, rates, spreads, tenors and notional schedules — the building blocks every other module takes as input.")},
    {"core/conventions/", QLG("Conventions"), QLG("Parses and formats dates and converts between days and year fractions, rate units and volatility quotes.")},
    {"core/autodiff/", QLG("Automatic differentiation"), QLG("Evaluates a function with dual numbers and returns its value with exact derivatives (gradient or Taylor coefficients), not finite differences.")},
    {"core/distributions/", QLG("Distributions"), QLG("Density, cumulative probability or quantile of the chosen distribution at the given point.")},
    {"core/math/", QLG("Math"), QLG("Evaluates an elementary function of one or two arguments at full precision.")},
    {"core/ops/", QLG("Operations"), QLG("Vectorised pricing and statistics primitives — Black-Scholes / Black-76 values, forward rates, discounted cash flows, interpolation, VaR, covariance and simulated GBM paths.")},
    {"core/legs/", QLG("Legs"), QLG("Builds a fixed, floating or zero-coupon leg and returns every cash flow with its dates, accrual and amount.")},
    {"core/periods/", QLG("Periods"), QLG("Day-count fraction of a period and the amount of a single fixed or floating coupon.")},
    // ── pricing ──
    {"pricing/bs/", QLG("Black-Scholes"), QLG("Closed-form European option value under Black-Scholes, with the Greeks (delta, gamma, vega, theta, rho) or the implied volatility for a market price.")},
    {"pricing/black76/", QLG("Black-76"), QLG("Black-76 values for options on forwards and futures — caplets, floorlets and swaptions — with their Greeks and implied volatility.")},
    {"pricing/bachelier/", QLG("Bachelier"), QLG("Normal-model option values and Greeks (rates near zero or negative), implied normal volatility and conversion to lognormal vol.")},
    {"pricing/binomial/", QLG("Binomial tree"), QLG("Option value from a recombining binomial tree — European, American, Bermudan or barrier exercise — with the early-exercise premium.")},
    {"pricing/kirk/", QLG("Kirk spread option"), QLG("Kirk's approximation for an option on the spread between two assets, with its sensitivities.")},
    {"pricing/margrabe", QLG("Margrabe exchange option"), QLG("Value of the option to exchange one asset for another (Margrabe's formula).")},
    {"pricing/basket-levy", QLG("Basket option (Levy)"), QLG("Levy moment-matching value of an option on a weighted basket of assets.")},
    // ── curves ──
    {"curves/nelson-siegel/", QLG("Nelson-Siegel"), QLG("Fits the Nelson-Siegel level / slope / curvature factors to market yields, or evaluates the fitted curve at any maturity.")},
    {"curves/nss/", QLG("Nelson-Siegel-Svensson"), QLG("Fits or evaluates the six-parameter Svensson curve — a smooth yield curve through observed zero rates.")},
    {"curves/multicurve/", QLG("Multi-curve"), QLG("Sets up separate discounting and projection curves and returns the basis spread between them.")},
    {"curves/inflation/", QLG("Inflation curves"), QLG("Builds or bootstraps a zero-coupon inflation curve, with seasonality adjustment.")},
    {"curves/", QLG("Yield curves"), QLG("Builds a yield curve from market points and queries it — zero rates, forward rates, discount factors, interpolated points — or applies shifts, twists and butterflies.")},
    // ── volatility ──
    {"volatility/surface/", QLG("Volatility surface"), QLG("Builds a volatility surface (flat, from points or a grid) and returns smiles, term structures and total variance.")},
    {"volatility/sabr/", QLG("SABR"), QLG("SABR implied volatility, smile, normal vol, risk-neutral density and calibration of alpha / rho / nu to market vols.")},
    {"volatility/local-vol/", QLG("Local volatility"), QLG("Dupire local volatility from an implied-volatility surface.")},
    // ── models ──
    {"models/short-rate/", QLG("Short-rate models"), QLG("Vasicek / CIR bond prices, bond options, model yield curves and Monte Carlo rate paths.")},
    {"models/hull-white/", QLG("Hull-White"), QLG("Calibrates Hull-White mean reversion and volatility to market instruments.")},
    {"models/heston/", QLG("Heston"), QLG("Heston stochastic-volatility option prices (semi-closed form or Monte Carlo) and the implied volatility they correspond to.")},
    {"models/merton/", QLG("Merton jump-diffusion"), QLG("Option value under Merton jump-diffusion, by series expansion or FFT.")},
    {"models/kou/", QLG("Kou double-exponential"), QLG("Option value under Kou's double-exponential jump model.")},
    {"models/dupire/", QLG("Dupire"), QLG("Option value under a Dupire local-volatility model.")},
    {"models/svi/", QLG("SVI"), QLG("Calibrates the SVI parameterisation of the volatility smile to market quotes.")},
    {"models/variance-gamma/", QLG("Variance Gamma"), QLG("Option value under the Variance Gamma model.")},
    // ── stochastic ──
    {"stochastic/exact/", QLG("Exact simulation"), QLG("Draws from the exact transition law of GBM / OU / CIR / Heston — sample paths with their distribution statistics.")},
    {"stochastic/simulation/", QLG("SDE simulation"), QLG("Euler-Maruyama / Milstein discretisation of a stochastic differential equation (one or many dimensions), or a multilevel Monte Carlo estimate.")},
    {"stochastic/sampling/", QLG("Sampling"), QLG("Quasi-random (Sobol), antithetic, correlated-normal, multivariate-normal and jump samples, with their moments.")},
    {"stochastic/theory/", QLG("Stochastic calculus"), QLG("Itô's lemma and product rule, quadratic variation and covariation, martingale tests and Girsanov measure changes, worked symbolically or numerically.")},
    {"stochastic/", QLG("Stochastic processes"), QLG("Simulated sample paths of the process (GBM, OU, CIR, Heston, Merton, Vasicek, Wiener, Poisson …) with summary statistics, or its closed-form bond price.")},
    // ── risk ──
    {"risk/var/", QLG("Value at Risk"), QLG("Parametric or historical VaR and expected shortfall of a portfolio, with component, marginal and incremental contributions.")},
    {"risk/stress/", QLG("Stress testing"), QLG("Portfolio P&L under user-defined market scenarios.")},
    {"risk/tail-risk/", QLG("Tail risk"), QLG("Tail-risk measures (VaR, ES, tail index, drawdowns) and tail dependence between assets.")},
    {"risk/evt/", QLG("Extreme value theory"), QLG("Fits a generalised Pareto / GEV distribution to the tail of the returns (or the Hill estimator) and returns tail VaR and ES.")},
    {"risk/xva/", QLG("XVA"), QLG("Credit valuation adjustment and potential future exposure profile of a netting set.")},
    {"risk/copula/", QLG("Copulas"), QLG("Samples dependent uniforms from a Gaussian or Student-t copula.")},
    {"risk/portfolio-risk/", QLG("Exposure"), QLG("Exposure profile over time and the minimum-variance hedge ratio.")},
    {"risk/sensitivities/", QLG("Sensitivities"), QLG("Greeks and rate sensitivities — key-rate durations, parallel shift, twist, cross-gamma and bucketed delta.")},
    {"risk/backtest", QLG("VaR backtest"), QLG("Kupiec / Christoffersen backtest of a VaR model against realised returns: exceptions, coverage and the traffic-light zone.")},
    {"risk/correlation-stress", QLG("Correlation stress"), QLG("Portfolio risk when correlations are stressed toward one.")},
    // ── portfolio ──
    {"portfolio/optimize/", QLG("Portfolio optimisation"), QLG("Mean-variance optimal weights — minimum variance, maximum Sharpe, a target return or the whole efficient frontier — with expected return, volatility and Sharpe ratio.")},
    {"portfolio/black-litterman/", QLG("Black-Litterman"), QLG("Equilibrium implied returns, and the posterior returns and weights after blending in your views.")},
    {"portfolio/risk-parity", QLG("Risk parity"), QLG("Weights that equalise each asset's contribution to portfolio risk.")},
    {"portfolio/risk/", QLG("Portfolio risk"), QLG("Volatility, VaR / CVaR, risk contributions and risk-adjusted ratios of a weighted portfolio.")},
    // ── instruments ──
    {"instruments/bond/", QLG("Bonds"), QLG("Fixed-rate (or inflation-linked) bond price, yield, duration, convexity and the full cash-flow schedule.")},
    {"instruments/swap/", QLG("Interest-rate swaps"), QLG("Swap value of each leg, the par swap rate and DV01.")},
    {"instruments/fra/", QLG("FRAs"), QLG("Forward-rate agreement value and break-even rate.")},
    {"instruments/ois/", QLG("OIS"), QLG("Overnight-index swap value, and an OIS discount curve bootstrapped from quotes.")},
    {"instruments/cds/", QLG("Credit default swaps"), QLG("CDS value, implied hazard rate and survival probabilities.")},
    {"instruments/fx/", QLG("FX"), QLG("FX forward points and Garman-Kohlhagen FX option values with Greeks.")},
    {"instruments/money-market/", QLG("Money market"), QLG("Deposit, repo and T-bill pricing: interest, yields and discount rates.")},
    {"instruments/commodity/", QLG("Commodities"), QLG("Commodity futures fair value from spot, carry and convenience yield.")},
    {"instruments/futures/", QLG("Futures"), QLG("Short-term interest-rate futures and the cheapest-to-deliver bond of a bond future.")},
    {"instruments/equity/", QLG("Equity derivatives"), QLG("Fair strike and value of variance and volatility swaps.")},
    // ── solver ──
    {"solver/bootstrap/", QLG("Curve bootstrap"), QLG("Bootstraps a zero curve from deposit and swap quotes.")},
    {"solver/calibration/", QLG("Calibration"), QLG("Calibrates Vasicek parameters to an observed curve.")},
    {"solver/finance/", QLG("Fixed-income solver"), QLG("Solves for yields, durations, convexity, PV01 / DV01, implied vols, forward / zero / par rates, spreads, IRR / XIRR and carry from the inputs.")},
    // ── economics ──
    {"economics/equilibrium/", QLG("General equilibrium"), QLG("Cobb-Douglas / CES production and utility, Walrasian prices and exchange-economy allocations.")},
    {"economics/games/", QLG("Game theory"), QLG("Nash equilibria (pure and mixed), best responses, dominated-strategy elimination and fictitious play for a payoff matrix.")},
    {"economics/auctions/", QLG("Auctions"), QLG("Runs or simulates first-price, second-price and other auctions — winners, payments, equilibrium bids and expected revenue.")},
    {"economics/utility/", QLG("Utility theory"), QLG("CARA / CRRA / log / quadratic utility, expected utility, certainty equivalents, risk premia, prospect theory and stochastic dominance.")},
    // ── regulatory ──
    {"regulatory/basel/", QLG("Basel III"), QLG("Capital ratios and risk-weighted assets for credit and operational risk.")},
    {"regulatory/saccr/", QLG("SA-CCR"), QLG("Exposure at default of a derivatives netting set under the standardised approach.")},
    {"regulatory/ifrs9/", QLG("IFRS 9"), QLG("Stage assessment, significant increase in credit risk, and 12-month or lifetime expected credit loss.")},
    {"regulatory/liquidity/", QLG("Liquidity ratios"), QLG("Liquidity coverage ratio and net stable funding ratio.")},
    {"regulatory/stress/", QLG("Capital stress"), QLG("Capital ratio projected through a stress scenario.")},
    // ── scheduling ──
    {"scheduling/calendar/", QLG("Business calendars"), QLG("Holiday calendars: whether a date is a business day, the next / previous business day and business days between or after dates.")},
    {"scheduling/daycount/", QLG("Day count"), QLG("Year fraction and day count between dates under ACT/360, ACT/365, 30/360 and other conventions.")},
    {"scheduling/adjustment/", QLG("Date adjustment"), QLG("Rolls dates onto business days (following, modified following, preceding …).")},
    {"scheduling/schedule/", QLG("Schedules"), QLG("Generates a payment schedule between two dates at the chosen frequency and roll convention.")},
    // ── numerical ──
    {"numerical/differentiation/", QLG("Numerical differentiation"), QLG("Derivative, gradient or Hessian of a function at a point.")},
    {"numerical/fft/", QLG("FFT"), QLG("Forward / inverse discrete Fourier transform or the convolution of two signals.")},
    {"numerical/integration/", QLG("Integration"), QLG("Definite integral by quadrature, plain or stratified Monte Carlo, with the error estimate.")},
    {"numerical/interpolation/", QLG("Interpolation"), QLG("Interpolated values and cubic-spline curves (and their derivatives) through the given points.")},
    {"numerical/linalg/", QLG("Linear algebra"), QLG("Matrix products, solves, inverses, decompositions, norms and least squares.")},
    {"numerical/least-squares/", QLG("Least squares"), QLG("Non-linear least-squares fit of a model to data, with the fitted parameters and residuals.")},
    {"numerical/ode/", QLG("ODE solver"), QLG("Numerical solution of an ordinary differential equation over the time grid.")},
    {"numerical/roots/", QLG("Root finding"), QLG("Root of a function in one or many dimensions (bracketing or Newton).")},
    {"numerical/optimize/", QLG("Optimisation"), QLG("Minimum of a function and where it is reached.")},
    // ── physics ──
    {"physics/entropy/", QLG("Information theory"), QLG("Shannon, Rényi and Tsallis entropies, mutual information, transfer entropy and Fisher information of the data.")},
    {"physics/divergence/", QLG("Divergences"), QLG("Kullback-Leibler or Jensen-Shannon divergence between two distributions.")},
    {"physics/thermodynamics/", QLG("Thermodynamics"), QLG("Ideal-gas and van der Waals states, Carnot efficiency, free energy and related thermodynamic relations.")},
    {"physics/", QLG("Statistical physics"), QLG("Boltzmann distributions, maximum-entropy fits and the Ising model.")},
    // ── statistics ──
    {"statistics/distributions/", QLG("Probability distributions"), QLG("Density / mass, cumulative probability, quantile or the moments of the distribution for the given parameters.")},
    {"statistics/timeseries/", QLG("Time-series models"), QLG("Fits an AR / MA / ARIMA / GARCH-family model to the series and returns its coefficients, fit statistics and forecasts.")},
    {"statistics/", QLG("Statistics"), QLG("Statistical calculation on the given data.")},
    // ── machine learning ──
    {"ml/credit/", QLG("Credit models"), QLG("Credit scorecards, logistic PD models, WOE binning, calibration / discrimination tests, rating migration and LGD models.")},
    {"ml/regression/", QLG("Regression"), QLG("Fits a regression (linear, tree or ensemble, LGD / EAD models) and returns coefficients, fit quality and predictions.")},
    {"ml/clustering/", QLG("Clustering"), QLG("k-means, hierarchical and DBSCAN clusters, principal components or isolation-forest outliers.")},
    {"ml/preprocessing/", QLG("Preprocessing"), QLG("Scaling, transforms, outlier and winsorisation steps and stationarity tests for model inputs.")},
    {"ml/features/", QLG("Feature engineering"), QLG("Lagged, rolling, calendar, technical and financial-ratio features built from the data.")},
    {"ml/validation/", QLG("Model validation"), QLG("Stability, discrimination and calibration reports and interpretability of a model.")},
    {"ml/gp/", QLG("Gaussian processes"), QLG("Gaussian-process fit of a yield curve or volatility surface with uncertainty bands.")},
    {"ml/nn/", QLG("Neural networks"), QLG("Neural-network fit of a curve, volatility surface or portfolio allocation.")},
    {"ml/factor/", QLG("Factor models"), QLG("Statistical and cross-sectional factor models: loadings, factor returns and explained variance.")},
    {"ml/covariance/", QLG("Covariance estimation"), QLG("Shrinkage and robust covariance matrix estimates.")},
    {"ml/", QLG("Machine learning"), QLG("Fits the model to the data and returns its parameters, diagnostics and predictions.")},
    // ── analysis ──
    {"analysis/fundamentals/", QLG("Fundamental analysis"), QLG("Ratio analysis of the statements you enter — profitability, liquidity, efficiency, growth, solvency, cash flow, DuPont and earnings quality, with scores.")},
    {"analysis/ratios/", QLG("Financial ratios"), QLG("The named financial ratio computed from the statement values you enter, with its interpretation.")},
    {"analysis/valuation/dcf/", QLG("DCF valuation"), QLG("Discounted-cash-flow value (FCFF, dividend discount, Gordon or two-stage growth), terminal value, WACC and cost of equity.")},
    {"analysis/valuation/credit/", QLG("Credit valuation"), QLG("Merton structural model, distance to default, spread implied by a PD, expected loss and rating-implied PD.")},
    {"analysis/valuation/predictive/", QLG("Distress scores"), QLG("Altman Z, Piotroski F, Beneish M, Ohlson O, Springate and Zmijewski scores with the zone they put the company in.")},
    {"analysis/valuation/residual-income/", QLG("Residual income"), QLG("Economic value added and residual-income valuation of the company.")},
    {"analysis/valuation/startup/", QLG("Startup valuation"), QLG("VC method, Berkus, First Chicago and dilution through funding rounds.")},
    {"analysis/valuation/", QLG("Valuation"), QLG("Valuation from comparables, screens, factor models, pro-forma adjustments or a sum of the parts.")},
    {"analysis/industry/", QLG("Industry analysis"), QLG("Industry-specific metrics for banks, insurers, REITs and utilities.")},
    {"analysis/", QLG("Analysis"), QLG("Corporate-finance analysis of the figures you enter.")},
};
// clang-format on

#undef QLG

const QlGuideEntry* ql_guide_entry(const QString& endpoint) {
    for (const auto& e : kQlGuide) {
        const QLatin1String key(e.key);
        if (key.endsWith(QLatin1Char('/')) ? endpoint.startsWith(key) : endpoint == key)
            return &e;
    }
    return nullptr;
}

QString ql_guide_tr(const char* text) {
    return QCoreApplication::translate("QuantLibGuide", text);
}

// Words that read better as written here than capitalised.
const QHash<QString, QString>& ql_guide_words() {
    static const QHash<QString, QString> k = {
        {"bs", "Black-Scholes"}, {"var", "VaR"}, {"cvar", "CVaR"}, {"es", "ES"}, {"cdf", "CDF"},
        {"pdf", "PDF"}, {"ppf", "quantile"}, {"pmf", "PMF"}, {"pgf", "PGF"}, {"dv01", "DV01"},
        {"pv01", "PV01"}, {"cva", "CVA"}, {"pfe", "PFE"}, {"gpd", "GPD"}, {"gev", "GEV"},
        {"rwa", "RWA"}, {"ead", "EAD"}, {"lcr", "LCR"}, {"nsfr", "NSFR"}, {"ecl", "ECL"},
        {"sicr", "SICR"}, {"12m", "12M"}, {"sabr", "SABR"}, {"svi", "SVI"}, {"ois", "OIS"},
        {"fra", "FRA"}, {"cds", "CDS"}, {"fx", "FX"}, {"wacc", "WACC"}, {"dcf", "DCF"},
        {"ddm", "DDM"}, {"fcff", "FCFF"}, {"eva", "EVA"}, {"ri", "RI"}, {"sotp", "SOTP"},
        {"roa", "ROA"}, {"roe", "ROE"}, {"roic", "ROIC"}, {"roce", "ROCE"}, {"pe", "P/E"},
        {"pb", "P/B"}, {"ps", "P/S"}, {"pcf", "P/CF"}, {"pfcf", "P/FCF"}, {"peg", "PEG"},
        {"ev", "EV"}, {"fcf", "FCF"}, {"ocf", "OCF"}, {"ebitda", "EBITDA"}, {"ebit", "EBIT"},
        {"capex", "capex"}, {"yoy", "YoY"}, {"cagr", "CAGR"}, {"ar", "AR"}, {"ma", "MA"},
        {"arima", "ARIMA"}, {"garch", "GARCH"}, {"egarch", "EGARCH"}, {"gjr", "GJR"},
        {"gp", "GP"}, {"nn", "NN"}, {"hmm", "HMM"}, {"pca", "PCA"}, {"kmeans", "k-means"},
        {"dbscan", "DBSCAN"}, {"lgd", "LGD"}, {"woe", "WOE"}, {"mc", "MC"}, {"nd", "n-D"},
        {"1d", "1-D"}, {"fft", "FFT"}, {"ode", "ODE"}, {"kl", "KL"}, {"js", "JS"},
        {"cara", "CARA"}, {"crra", "CRRA"}, {"ces", "CES"}, {"irr", "IRR"}, {"xirr", "XIRR"},
        {"asw", "ASW"}, {"oas", "OAS"}, {"ctd", "CTD"}, {"stir", "STIR"}, {"nss", "NSS"},
        {"gbm", "GBM"}, {"ou", "OU"}, {"cir", "CIR"}, {"bps", "bps"}, {"vc", "VC"},
        {"lstsq", "least squares"}, {"matmul", "matrix product"}, {"matvec", "matrix × vector"},
        {"black76", "Black-76"}, {"saccr", "SA-CCR"}, {"ifrs9", "IFRS 9"},
    };
    return k;
}

QString ql_guide_words_of(const QString& segment) {
    QStringList out;
    const QStringList parts = segment.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        const QString& w = parts.at(i);
        const auto it = ql_guide_words().constFind(w.toLower());
        if (it != ql_guide_words().constEnd()) {
            out << it.value();
            continue;
        }
        out << (i == 0 ? w.left(1).toUpper() + w.mid(1) : w);
    }
    return out.join(QLatin1Char(' '));
}

} // namespace

QString ql_endpoint_title(const QString& endpoint) {
    const QString leaf = ql_guide_words_of(endpoint.section(QLatin1Char('/'), -1));
    const QlGuideEntry* e = ql_guide_entry(endpoint);
    if (!e)
        return leaf;
    const QString group = ql_guide_tr(e->group);
    // An exact-endpoint entry is its own title.
    if (!QLatin1String(e->key).endsWith(QLatin1Char('/')))
        return group;
    // "core/distributions/normal/cdf" → "Distributions · Normal CDF"
    const QStringList segs = endpoint.split(QLatin1Char('/'));
    const int key_depth = int(QString::fromLatin1(e->key).count(QLatin1Char('/')));
    QStringList rest;
    for (int i = key_depth; i < segs.size(); ++i)
        rest << ql_guide_words_of(segs.at(i));
    const QString tail = rest.isEmpty() ? leaf : rest.join(QLatin1Char(' '));
    return group + QStringLiteral(" · ") + tail;
}

QString ql_endpoint_summary(const QString& endpoint) {
    const QlGuideEntry* e = ql_guide_entry(endpoint);
    return e ? ql_guide_tr(e->summary)
             : QCoreApplication::translate("QuantLibGuide", "Runs this QuantLib calculation with the inputs on the left.");
}

QString ql_module_glyph(const QString& module_id) {
    static const QHash<QString, QString> kGlyphs = {
        {"core", QStringLiteral("Σ")},         // Σ
        {"analysis", QStringLiteral("%")},
        {"curves", QStringLiteral("∫")},       // ∫
        {"economics", QStringLiteral("€")},    // €
        {"instruments", QStringLiteral("$")},
        {"ml", QStringLiteral("λ")},           // λ
        {"models", QStringLiteral("μ")},       // μ
        {"numerical", QStringLiteral("∂")},    // ∂
        {"physics", QStringLiteral("Ψ")},      // Ψ
        {"portfolio", QStringLiteral("π")},    // π
        {"pricing", QStringLiteral("Δ")},      // Δ
        {"regulatory", QStringLiteral("§")},   // §
        {"risk", QStringLiteral("∇")},         // ∇
        {"scheduling", QStringLiteral("#")},
        {"solver", QStringLiteral("√")},       // √
        {"statistics", QStringLiteral("∝")},   // ∝
        {"stochastic", QStringLiteral("≈")},   // ≈
        {"volatility", QStringLiteral("σ")},   // σ
    };
    return kGlyphs.value(module_id, QStringLiteral("Σ"));
}

} // namespace fincept::screens
