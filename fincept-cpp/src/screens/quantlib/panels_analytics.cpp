// QuantLib Panels — ML (Credit, Regression, Clustering, Preprocessing, Features,
//                       Validation, TimeSeries, GP/NN, Factor/Cov)
//                   Statistics (Continuous, Discrete, TimeSeries)
//                   Portfolio (Optimization, Risk Metrics)
#include "quantlib_screen.h"
#include <imgui.h>

namespace fincept::quantlib {

// ── ML ──────────────────────────────────────────────────────────────────────

void QuantLibScreen::render_ml_credit() {
    if (begin_endpoint_card("ml-cr-disc", "Discrimination Metrics", "AUC, KS, Gini for credit scoring")) {
        input_array("ml-cr-disc", "y_true", "y_true", "0,0,1,1,0,1");
        input_array("ml-cr-disc", "y_score", "y_score", "0.1,0.3,0.8,0.9,0.2,0.7");
        if (run_button("ml-cr-disc")) {
            fire_post("ml-cr-disc", "ml/credit/discrimination", {
                {"y_true", json::array({0,0,1,1,0,1})}, {"y_score", json::array({0.1,0.3,0.8,0.9,0.2,0.7})}
            });
        }
        render_result("ml-cr-disc");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cr-perf", "Model Performance", "Credit model performance metrics")) {
        input_array("ml-cr-perf", "y_true", "y_true", "0,0,1,1,0,1");
        input_array("ml-cr-perf", "y_score", "y_score", "0.1,0.3,0.8,0.9,0.2,0.7");
        if (run_button("ml-cr-perf")) {
            fire_post("ml-cr-perf", "ml/credit/performance", {
                {"y_true", json::array({0,0,1,1,0,1})}, {"y_score", json::array({0.1,0.3,0.8,0.9,0.2,0.7})}
            });
        }
        render_result("ml-cr-perf");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_regression() {
    if (begin_endpoint_card("ml-reg-fit", "Regression Fit", "Linear/Ridge/Lasso regression")) {
        input_select("ml-reg-fit", "Method", "method", {"ols", "ridge", "lasso", "elastic_net"});
        input_float("ml-reg-fit", "Alpha", "alpha", "1.0");
        if (run_button("ml-reg-fit")) {
            fire_post("ml-reg-fit", "ml/regression/fit", {
                {"X", json::array({json::array({1,2}), json::array({3,4}), json::array({5,6})})},
                {"y", json::array({1.5, 3.5, 5.5})}, {"method", ep("ml-reg-fit").get("method", "ols")}
            });
        }
        render_result("ml-reg-fit");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_clustering() {
    if (begin_endpoint_card("ml-cl-km", "K-Means", "K-Means clustering")) {
        input_int("ml-cl-km", "K Clusters", "k", "3"); input_int("ml-cl-km", "Max Iter", "max_iter", "100");
        if (run_button("ml-cl-km")) {
            fire_post("ml-cl-km", "ml/clustering/kmeans", {
                {"X", json::array({json::array({1,2}), json::array({1.5,1.8}), json::array({5,8}),
                    json::array({8,8}), json::array({1,0.6}), json::array({9,11})})},
                {"n_clusters", ep("ml-cl-km").geti("k", 3)}
            });
        }
        render_result("ml-cl-km");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cl-pca", "PCA", "Principal Component Analysis")) {
        input_int("ml-cl-pca", "N Components", "n", "2");
        if (run_button("ml-cl-pca")) {
            fire_post("ml-cl-pca", "ml/clustering/pca", {
                {"X", json::array({json::array({1,2,3}), json::array({4,5,6}), json::array({7,8,9})})},
                {"n_components", ep("ml-cl-pca").geti("n", 2)}
            });
        }
        render_result("ml-cl-pca");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_preprocessing() {
    if (begin_endpoint_card("ml-pp-scale", "Scale Data", "StandardScaler / MinMaxScaler / RobustScaler")) {
        input_select("ml-pp-scale", "Method", "method", {"standard", "minmax", "robust"});
        if (run_button("ml-pp-scale")) {
            fire_post("ml-pp-scale", "ml/preprocessing/scale", {
                {"X", json::array({json::array({1,200}), json::array({2,300}), json::array({3,400})})},
                {"method", ep("ml-pp-scale").get("method", "standard")}
            });
        }
        render_result("ml-pp-scale");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-pp-stat", "Stationarity Test", "ADF/KPSS stationarity tests")) {
        input_array("ml-pp-stat", "Data", "data", "1,2,3,4,5,6,7,8,9,10");
        if (run_button("ml-pp-stat")) {
            fire_post("ml-pp-stat", "ml/preprocessing/stationarity", {
                {"data", json::array({1,2,3,4,5,6,7,8,9,10})}
            });
        }
        render_result("ml-pp-stat");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_features() {
    if (begin_endpoint_card("ml-ft-tech", "Technical Features", "Generate technical indicator features")) {
        input_select("ml-ft-tech", "Indicator", "indicator", {"sma", "ema", "rsi", "macd", "bbands"});
        input_int("ml-ft-tech", "Period", "period", "14");
        input_array("ml-ft-tech", "Prices", "prices", "100,101,102,100,99,101,103,105,104,106");
        if (run_button("ml-ft-tech")) {
            fire_post("ml-ft-tech", "ml/features/technical", {
                {"prices", json::array({100,101,102,100,99,101,103,105,104,106})},
                {"indicator", ep("ml-ft-tech").get("indicator", "sma")},
                {"period", ep("ml-ft-tech").geti("period", 14)}
            });
        }
        render_result("ml-ft-tech");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-ft-roll", "Rolling Features", "Rolling window statistics")) {
        input_int("ml-ft-roll", "Window", "window", "5");
        input_select("ml-ft-roll", "Statistic", "stat", {"mean", "std", "skew", "kurt", "sharpe"});
        if (run_button("ml-ft-roll")) {
            fire_post("ml-ft-roll", "ml/features/rolling", {
                {"data", json::array({1,2,3,4,5,6,7,8,9,10})},
                {"window", ep("ml-ft-roll").geti("window", 5)},
                {"statistic", ep("ml-ft-roll").get("stat", "mean")}
            });
        }
        render_result("ml-ft-roll");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_validation() {
    if (begin_endpoint_card("ml-val-stab", "Stability (PSI)", "Population Stability Index")) {
        input_array("ml-val-stab", "Expected", "expected", "0.1,0.2,0.3,0.2,0.1,0.05,0.05");
        input_array("ml-val-stab", "Actual", "actual", "0.12,0.18,0.28,0.22,0.1,0.05,0.05");
        if (run_button("ml-val-stab")) {
            fire_post("ml-val-stab", "ml/validation/stability", {
                {"expected", json::array({0.1,0.2,0.3,0.2,0.1,0.05,0.05})},
                {"actual", json::array({0.12,0.18,0.28,0.22,0.1,0.05,0.05})}
            });
        }
        render_result("ml-val-stab");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_timeseries() {
    if (begin_endpoint_card("ml-ts-hmm", "HMM Regime Detection", "Hidden Markov Model for regime detection")) {
        input_array("ml-ts-hmm", "Returns", "returns", "0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005");
        input_int("ml-ts-hmm", "N States", "states", "2");
        if (run_button("ml-ts-hmm")) {
            fire_post("ml-ts-hmm", "ml/hmm/fit", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005})},
                {"n_states", ep("ml-ts-hmm").geti("states", 2)}
            });
        }
        render_result("ml-ts-hmm");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_gpnn() {
    if (begin_endpoint_card("ml-gp-curve", "GP Yield Curve", "Gaussian Process yield curve fitting")) {
        input_array("ml-gp-curve", "Tenors", "tenors", "0.25,0.5,1,2,5,10,30");
        input_array("ml-gp-curve", "Rates", "rates", "0.04,0.041,0.043,0.045,0.047,0.048,0.049");
        input_array("ml-gp-curve", "Query", "query", "0.5,1.5,3,7,15,20");
        if (run_button("ml-gp-curve")) {
            fire_post("ml-gp-curve", "ml/gp/curve", {
                {"tenors", json::array({0.25,0.5,1,2,5,10,30})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.047,0.048,0.049})},
                {"query_tenors", json::array({0.5,1.5,3,7,15,20})}
            });
        }
        render_result("ml-gp-curve");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_ml_factor() {
    if (begin_endpoint_card("ml-fac-stat", "Statistical Factor Model", "PCA-based factor decomposition")) {
        input_int("ml-fac-stat", "N Factors", "n", "3");
        if (run_button("ml-fac-stat")) {
            fire_post("ml-fac-stat", "ml/factor/statistical", {
                {"returns", json::array({json::array({0.01,0.02,-0.01}), json::array({0.015,-0.01,0.02}), json::array({-0.005,0.01,0.005})})},
                {"n_factors", ep("ml-fac-stat").geti("n", 3)}
            });
        }
        render_result("ml-fac-stat");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ml-cov-est", "Covariance Estimation", "Shrinkage and factor covariance estimation")) {
        input_select("ml-cov-est", "Method", "method", {"sample", "ledoit_wolf", "oas", "factor"});
        if (run_button("ml-cov-est")) {
            fire_post("ml-cov-est", "ml/covariance/estimate", {
                {"returns", json::array({json::array({0.01,0.02}), json::array({0.015,-0.01}), json::array({-0.005,0.01})})},
                {"method", ep("ml-cov-est").get("method", "ledoit_wolf")}
            });
        }
        render_result("ml-cov-est");
        end_endpoint_card();
    }
}

// ── Statistics ──────────────────────────────────────────────────────────────

void QuantLibScreen::render_stat_continuous() {
    if (begin_endpoint_card("st-norm", "Normal Distribution", "Properties, PDF, CDF, PPF")) {
        input_float("st-norm", "Mu", "mu", "0"); input_float("st-norm", "Sigma", "sigma", "1");
        input_float("st-norm", "x", "x", "1.96");
        input_select("st-norm", "Function", "func", {"properties", "pdf", "cdf", "ppf"});
        if (run_button("st-norm")) {
            auto& s = ep("st-norm");
            fire_post("st-norm", "statistics/distributions/normal/" + s.get("func", "properties"), {
                {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}, {"x", s.getd("x")}
            });
        }
        render_result("st-norm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-lognorm", "Lognormal", "Lognormal distribution")) {
        input_float("st-lognorm", "Mu", "mu", "0"); input_float("st-lognorm", "Sigma", "sigma", "0.5");
        input_float("st-lognorm", "x", "x", "2.0");
        if (run_button("st-lognorm")) {
            auto& s = ep("st-lognorm");
            fire_post("st-lognorm", "statistics/distributions/lognormal/properties", {
                {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("st-lognorm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-t", "Student-t", "Student-t distribution")) {
        input_float("st-t", "df", "df", "5"); input_float("st-t", "x", "x", "2.0");
        if (run_button("st-t")) {
            fire_post("st-t", "statistics/distributions/student-t/properties", {{"df", ep("st-t").getd("df")}});
        }
        render_result("st-t");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_stat_discrete() {
    if (begin_endpoint_card("st-binom", "Binomial", "Binomial distribution")) {
        input_int("st-binom", "n", "n", "10"); input_float("st-binom", "p", "p", "0.5");
        input_int("st-binom", "k", "k", "5");
        input_select("st-binom", "Function", "func", {"properties", "pmf", "cdf"});
        if (run_button("st-binom")) {
            auto& s = ep("st-binom");
            fire_post("st-binom", "statistics/distributions/binomial/" + s.get("func", "properties"), {
                {"n", s.geti("n")}, {"p", s.getd("p")}, {"k", s.geti("k")}
            });
        }
        render_result("st-binom");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-poisson", "Poisson", "Poisson distribution")) {
        input_float("st-poisson", "Lambda", "lam", "5.0"); input_int("st-poisson", "k", "k", "3");
        if (run_button("st-poisson")) {
            fire_post("st-poisson", "statistics/distributions/poisson/properties", {{"lam", ep("st-poisson").getd("lam")}});
        }
        render_result("st-poisson");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_stat_timeseries() {
    if (begin_endpoint_card("st-arima", "ARIMA Fit", "Fit ARIMA(p,d,q) model")) {
        input_int("st-arima", "p", "p", "1"); input_int("st-arima", "d", "d", "1"); input_int("st-arima", "q", "q", "1");
        input_array("st-arima", "Data", "data", "100,102,101,105,108,107,110,112");
        if (run_button("st-arima")) {
            auto& s = ep("st-arima");
            fire_post("st-arima", "statistics/timeseries/arima/fit", {
                {"data", json::array({100,102,101,105,108,107,110,112})},
                {"p", s.geti("p")}, {"d", s.geti("d")}, {"q", s.geti("q")}
            });
        }
        render_result("st-arima");
        end_endpoint_card();
    }
    if (begin_endpoint_card("st-garch", "GARCH Fit", "Fit GARCH(p,q) volatility model")) {
        input_int("st-garch", "p", "p", "1"); input_int("st-garch", "q", "q", "1");
        input_array("st-garch", "Returns", "returns", "0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01");
        if (run_button("st-garch")) {
            fire_post("st-garch", "statistics/timeseries/garch/fit", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01})},
                {"p", ep("st-garch").geti("p")}, {"q", ep("st-garch").geti("q")}
            });
        }
        render_result("st-garch");
        end_endpoint_card();
    }
}

// ── Portfolio ───────────────────────────────────────────────────────────────

void QuantLibScreen::render_port_optimize() {
    if (begin_endpoint_card("pt-minsv", "Min Variance", "Minimum variance portfolio optimization")) {
        input_array("pt-minsv", "Expected Returns", "rets", "0.1,0.12,0.08");
        input_float("pt-minsv", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-minsv")) {
            fire_post("pt-minsv", "portfolio/optimize/min-variance", {
                {"expected_returns", json::array({0.1,0.12,0.08})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"rf_rate", ep("pt-minsv").getd("rf", 0.03)}
            });
        }
        render_result("pt-minsv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pt-maxsr", "Max Sharpe", "Maximum Sharpe ratio portfolio")) {
        input_float("pt-maxsr", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-maxsr")) {
            fire_post("pt-maxsr", "portfolio/optimize/max-sharpe", {
                {"expected_returns", json::array({0.1,0.12,0.08})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"rf_rate", ep("pt-maxsr").getd("rf", 0.03)}
            });
        }
        render_result("pt-maxsr");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_port_risk() {
    if (begin_endpoint_card("pt-var", "Portfolio VaR", "Value at Risk computation")) {
        input_float("pt-var", "Confidence", "conf", "0.95");
        input_float("pt-var", "Portfolio Value", "pv", "1000000");
        input_select("pt-var", "Method", "method", {"parametric", "historical", "monte_carlo"});
        if (run_button("pt-var")) {
            auto& s = ep("pt-var");
            fire_post("pt-var", "portfolio/risk/var", {
                {"weights", json::array({0.4,0.35,0.25})},
                {"covariance_matrix", json::array({json::array({0.04,0.006,0.002}), json::array({0.006,0.09,0.009}), json::array({0.002,0.009,0.01})})},
                {"confidence", s.getd("conf")}, {"portfolio_value", s.getd("pv")}, {"method", s.get("method")}
            });
        }
        render_result("pt-var");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pt-ratios", "Performance Ratios", "Sharpe, Sortino, Calmar, Information ratio")) {
        input_float("pt-ratios", "Risk-Free Rate", "rf", "0.03");
        if (run_button("pt-ratios")) {
            fire_post("pt-ratios", "portfolio/risk/ratios", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01,0.008,-0.005})},
                {"rf_rate", ep("pt-ratios").getd("rf", 0.03)}
            });
        }
        render_result("pt-ratios");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
