// QuantLib Panels — Curves (Build, Transform, Fitting, Specialized)
//                   Pricing (BS, Black76, Bachelier, Numerical)
//                   Risk (VaR, EVT/XVA, Sensitivities)
//                   Instruments (Bonds, Swaps, Markets, Credit/Futures)
#include "quantlib_screen.h"
#include <imgui.h>

namespace fincept::quantlib {

// ── Curves ─────────────────────────────────────────────────────────────────

void QuantLibScreen::render_curves_build() {
    if (begin_endpoint_card("crv-zero", "Zero Curve", "Build zero-rate yield curve from market data")) {
        input_array("crv-zero", "Tenors (yr)", "tenors", "0.25,0.5,1,2,3,5,7,10,20,30");
        input_array("crv-zero", "Rates", "rates", "0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049");
        input_select("crv-zero", "Interp", "interp", {"linear", "log_linear", "cubic"});
        input_array("crv-zero", "Query Tenors", "query", "0.75,1.5,4,8,15,25");
        if (run_button("crv-zero")) {
            auto& s = ep("crv-zero");
            fire_post("crv-zero", "curves/build/zero", {
                {"tenors", json::array({0.25,0.5,1,2,3,5,7,10,20,30})},
                {"rates", json::array({0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049})},
                {"interpolation", s.get("interp", "linear")},
                {"query_tenors", json::array({0.75,1.5,4,8,15,25})}
            });
        }
        render_result("crv-zero");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-disc", "Discount Curve", "Build discount factor curve")) {
        input_array("crv-disc", "Tenors", "tenors", "0.25,0.5,1,2,5,10");
        input_array("crv-disc", "Discount Factors", "dfs", "0.99,0.98,0.957,0.914,0.783,0.614");
        input_array("crv-disc", "Query", "query", "0.75,3,7");
        if (run_button("crv-disc")) {
            fire_post("crv-disc", "curves/build/discount", {
                {"tenors", json::array({0.25,0.5,1,2,5,10})},
                {"discount_factors", json::array({0.99,0.98,0.957,0.914,0.783,0.614})},
                {"query_tenors", json::array({0.75,3,7})}
            });
        }
        render_result("crv-disc");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-fwd", "Forward Curve", "Build forward rate curve")) {
        input_array("crv-fwd", "Tenors", "tenors", "0.25,0.5,1,2,5,10");
        input_array("crv-fwd", "Fwd Rates", "fwds", "0.04,0.042,0.044,0.046,0.048,0.049");
        if (run_button("crv-fwd")) {
            fire_post("crv-fwd", "curves/build/forward", {
                {"tenors", json::array({0.25,0.5,1,2,5,10})},
                {"forward_rates", json::array({0.04,0.042,0.044,0.046,0.048,0.049})}
            });
        }
        render_result("crv-fwd");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_transform() {
    if (begin_endpoint_card("crv-z2d", "Zero -> Discount", "Convert zero rates to discount factors")) {
        input_array("crv-z2d", "Tenors", "tenors", "1,2,5,10");
        input_array("crv-z2d", "Zero Rates", "rates", "0.04,0.045,0.047,0.048");
        input_select("crv-z2d", "Compounding", "comp", {"continuous", "annual", "semi_annual"});
        if (run_button("crv-z2d")) {
            fire_post("crv-z2d", "curves/transform/zero-to-discount", {
                {"tenors", json::array({1,2,5,10})},
                {"zero_rates", json::array({0.04,0.045,0.047,0.048})},
                {"compounding", ep("crv-z2d").get("comp", "continuous")}
            });
        }
        render_result("crv-z2d");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-d2z", "Discount -> Zero", "Convert discount factors to zero rates")) {
        input_array("crv-d2z", "Tenors", "tenors", "1,2,5,10");
        input_array("crv-d2z", "Discount Factors", "dfs", "0.961,0.914,0.783,0.614");
        if (run_button("crv-d2z")) {
            fire_post("crv-d2z", "curves/transform/discount-to-zero", {
                {"tenors", json::array({1,2,5,10})},
                {"discount_factors", json::array({0.961,0.914,0.783,0.614})}
            });
        }
        render_result("crv-d2z");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-z2f", "Zero -> Forward", "Extract forward rates from zero curve")) {
        input_array("crv-z2f", "Tenors", "tenors", "1,2,3,5,10");
        input_array("crv-z2f", "Zero Rates", "rates", "0.04,0.045,0.046,0.047,0.048");
        if (run_button("crv-z2f")) {
            fire_post("crv-z2f", "curves/transform/zero-to-forward", {
                {"tenors", json::array({1,2,3,5,10})},
                {"zero_rates", json::array({0.04,0.045,0.046,0.047,0.048})}
            });
        }
        render_result("crv-z2f");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_fitting() {
    if (begin_endpoint_card("crv-ns", "Nelson-Siegel", "Fit Nelson-Siegel model to yield data")) {
        input_array("crv-ns", "Tenors", "tenors", "0.25,0.5,1,2,3,5,7,10,20,30");
        input_array("crv-ns", "Yields", "yields", "0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049");
        input_array("crv-ns", "Query", "query", "0.5,1.5,4,8,15,25");
        if (run_button("crv-ns")) {
            fire_post("crv-ns", "curves/fitting/nelson-siegel", {
                {"tenors", json::array({0.25,0.5,1,2,3,5,7,10,20,30})},
                {"yields", json::array({0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049})},
                {"query_tenors", json::array({0.5,1.5,4,8,15,25})}
            });
        }
        render_result("crv-ns");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-nss", "Svensson (NSS)", "Fit Nelson-Siegel-Svensson model")) {
        input_array("crv-nss", "Tenors", "tenors", "0.25,0.5,1,2,3,5,7,10,20,30");
        input_array("crv-nss", "Yields", "yields", "0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049");
        if (run_button("crv-nss")) {
            fire_post("crv-nss", "curves/fitting/svensson", {
                {"tenors", json::array({0.25,0.5,1,2,3,5,7,10,20,30})},
                {"yields", json::array({0.04,0.041,0.043,0.045,0.046,0.047,0.0475,0.048,0.0485,0.049})}
            });
        }
        render_result("crv-nss");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_curves_specialized() {
    if (begin_endpoint_card("crv-ois", "OIS Curve", "Overnight Index Swap curve construction")) {
        input_array("crv-ois", "Tenors", "tenors", "0.25,0.5,1,2,5,10");
        input_array("crv-ois", "OIS Rates", "rates", "0.039,0.04,0.042,0.044,0.046,0.047");
        if (run_button("crv-ois")) {
            fire_post("crv-ois", "curves/specialized/ois", {
                {"tenors", json::array({0.25,0.5,1,2,5,10})},
                {"ois_rates", json::array({0.039,0.04,0.042,0.044,0.046,0.047})}
            });
        }
        render_result("crv-ois");
        end_endpoint_card();
    }
    if (begin_endpoint_card("crv-basis", "Basis Spread", "Curve basis spread analysis")) {
        input_array("crv-basis", "Tenors", "tenors", "1,2,5,10");
        input_array("crv-basis", "Curve A", "curveA", "0.04,0.045,0.047,0.048");
        input_array("crv-basis", "Curve B", "curveB", "0.038,0.043,0.045,0.046");
        if (run_button("crv-basis")) {
            fire_post("crv-basis", "curves/specialized/basis-spread", {
                {"tenors", json::array({1,2,5,10})},
                {"curve_a", json::array({0.04,0.045,0.047,0.048})},
                {"curve_b", json::array({0.038,0.043,0.045,0.046})}
            });
        }
        render_result("crv-basis");
        end_endpoint_card();
    }
}

// ── Pricing ────────────────────────────────────────────────────────────────

void QuantLibScreen::render_pricing_bs() {
    if (begin_endpoint_card("pr-bs-call", "BS Call/Put", "Black-Scholes European option pricing")) {
        input_float("pr-bs-call", "Spot (S)", "S", "100");
        input_float("pr-bs-call", "Strike (K)", "K", "100");
        input_float("pr-bs-call", "Time (T)", "T", "1.0");
        input_float("pr-bs-call", "Rate (r)", "r", "0.05");
        input_float("pr-bs-call", "Vol (σ)", "sigma", "0.2");
        input_select("pr-bs-call", "Type", "type", {"call", "put"});
        if (run_button("pr-bs-call")) {
            auto& s = ep("pr-bs-call");
            fire_post("pr-bs-call", "pricing/black-scholes/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")},
                {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-bs-call");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-bs-greeks", "BS Greeks", "Full Greeks surface (Delta, Gamma, Vega, Theta, Rho)")) {
        input_float("pr-bs-greeks", "Spot", "S", "100"); input_float("pr-bs-greeks", "Strike", "K", "100");
        input_float("pr-bs-greeks", "Time", "T", "1.0"); input_float("pr-bs-greeks", "Rate", "r", "0.05");
        input_float("pr-bs-greeks", "Vol", "sigma", "0.2");
        input_select("pr-bs-greeks", "Type", "type", {"call", "put"});
        if (run_button("pr-bs-greeks")) {
            auto& s = ep("pr-bs-greeks");
            fire_post("pr-bs-greeks", "pricing/black-scholes/greeks", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-bs-greeks");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-bs-iv", "BS Implied Vol", "Implied volatility from option price")) {
        input_float("pr-bs-iv", "Spot", "S", "100"); input_float("pr-bs-iv", "Strike", "K", "100");
        input_float("pr-bs-iv", "Time", "T", "1.0"); input_float("pr-bs-iv", "Rate", "r", "0.05");
        input_float("pr-bs-iv", "Market Price", "price", "10.45");
        input_select("pr-bs-iv", "Type", "type", {"call", "put"});
        if (run_button("pr-bs-iv")) {
            auto& s = ep("pr-bs-iv");
            fire_post("pr-bs-iv", "pricing/black-scholes/implied-vol", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"market_price", s.getd("price")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-bs-iv");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_black76() {
    if (begin_endpoint_card("pr-b76", "Black76 Price", "Black76 futures/forward option pricing")) {
        input_float("pr-b76", "Forward (F)", "F", "100"); input_float("pr-b76", "Strike (K)", "K", "100");
        input_float("pr-b76", "Time (T)", "T", "1.0"); input_float("pr-b76", "Rate (r)", "r", "0.05");
        input_float("pr-b76", "Vol (σ)", "sigma", "0.2");
        input_select("pr-b76", "Type", "type", {"call", "put"});
        if (run_button("pr-b76")) {
            auto& s = ep("pr-b76");
            fire_post("pr-b76", "pricing/black76/price", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-b76");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-b76-grk", "Black76 Greeks", "Greeks for Black76 model")) {
        input_float("pr-b76-grk", "Forward", "F", "100"); input_float("pr-b76-grk", "Strike", "K", "100");
        input_float("pr-b76-grk", "Time", "T", "1.0"); input_float("pr-b76-grk", "Rate", "r", "0.05");
        input_float("pr-b76-grk", "Vol", "sigma", "0.2");
        if (run_button("pr-b76-grk")) {
            auto& s = ep("pr-b76-grk");
            fire_post("pr-b76-grk", "pricing/black76/greeks", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("pr-b76-grk");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_bachelier() {
    if (begin_endpoint_card("pr-bach", "Bachelier Price", "Normal model (Bachelier) option pricing")) {
        input_float("pr-bach", "Forward", "F", "100"); input_float("pr-bach", "Strike", "K", "100");
        input_float("pr-bach", "Time", "T", "1.0"); input_float("pr-bach", "Rate", "r", "0.05");
        input_float("pr-bach", "Normal Vol", "sigma_n", "20.0");
        input_select("pr-bach", "Type", "type", {"call", "put"});
        if (run_button("pr-bach")) {
            auto& s = ep("pr-bach");
            fire_post("pr-bach", "pricing/bachelier/price", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma_normal", s.getd("sigma_n")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-bach");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-bach-iv", "Bachelier Implied Vol", "Normal implied volatility")) {
        input_float("pr-bach-iv", "Forward", "F", "100"); input_float("pr-bach-iv", "Strike", "K", "100");
        input_float("pr-bach-iv", "Time", "T", "1.0"); input_float("pr-bach-iv", "Rate", "r", "0.05");
        input_float("pr-bach-iv", "Market Price", "price", "8.0");
        if (run_button("pr-bach-iv")) {
            auto& s = ep("pr-bach-iv");
            fire_post("pr-bach-iv", "pricing/bachelier/implied-vol", {
                {"F", s.getd("F")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"market_price", s.getd("price")}
            });
        }
        render_result("pr-bach-iv");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_pricing_numerical() {
    if (begin_endpoint_card("pr-binom", "Binomial Tree", "Cox-Ross-Rubinstein binomial tree pricing")) {
        input_float("pr-binom", "Spot", "S", "100"); input_float("pr-binom", "Strike", "K", "100");
        input_float("pr-binom", "Time", "T", "1.0"); input_float("pr-binom", "Rate", "r", "0.05");
        input_float("pr-binom", "Vol", "sigma", "0.2"); input_int("pr-binom", "Steps", "steps", "100");
        input_select("pr-binom", "Type", "type", {"call", "put"});
        input_select("pr-binom", "Style", "style", {"european", "american"});
        if (run_button("pr-binom")) {
            auto& s = ep("pr-binom");
            fire_post("pr-binom", "pricing/numerical/binomial", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"n_steps", s.geti("steps")},
                {"option_type", s.get("type", "call")}, {"exercise", s.get("style", "european")}
            });
        }
        render_result("pr-binom");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-mc", "Monte Carlo", "MC option pricing with variance reduction")) {
        input_float("pr-mc", "Spot", "S", "100"); input_float("pr-mc", "Strike", "K", "100");
        input_float("pr-mc", "Time", "T", "1.0"); input_float("pr-mc", "Rate", "r", "0.05");
        input_float("pr-mc", "Vol", "sigma", "0.2"); input_int("pr-mc", "Paths", "paths", "10000");
        input_select("pr-mc", "Type", "type", {"call", "put"});
        if (run_button("pr-mc")) {
            auto& s = ep("pr-mc");
            fire_post("pr-mc", "pricing/numerical/monte-carlo", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")}, {"n_paths", s.geti("paths")},
                {"option_type", s.get("type", "call")}
            });
        }
        render_result("pr-mc");
        end_endpoint_card();
    }
    if (begin_endpoint_card("pr-fd", "Finite Difference", "PDE finite difference option pricing")) {
        input_float("pr-fd", "Spot", "S", "100"); input_float("pr-fd", "Strike", "K", "100");
        input_float("pr-fd", "Time", "T", "1.0"); input_float("pr-fd", "Rate", "r", "0.05");
        input_float("pr-fd", "Vol", "sigma", "0.2");
        input_int("pr-fd", "S Steps", "s_steps", "100"); input_int("pr-fd", "T Steps", "t_steps", "100");
        input_select("pr-fd", "Method", "method", {"explicit", "implicit", "crank_nicolson"});
        if (run_button("pr-fd")) {
            auto& s = ep("pr-fd");
            fire_post("pr-fd", "pricing/numerical/finite-difference", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")},
                {"r", s.getd("r")}, {"sigma", s.getd("sigma")},
                {"s_steps", s.geti("s_steps")}, {"t_steps", s.geti("t_steps")},
                {"method", s.get("method", "crank_nicolson")}
            });
        }
        render_result("pr-fd");
        end_endpoint_card();
    }
}

// ── Risk ───────────────────────────────────────────────────────────────────

void QuantLibScreen::render_risk_var() {
    if (begin_endpoint_card("rk-var", "Value at Risk", "Historical / Parametric / Monte Carlo VaR")) {
        input_float("rk-var", "Confidence", "conf", "0.99");
        input_int("rk-var", "Horizon (days)", "horizon", "10");
        input_float("rk-var", "Portfolio Value", "pv", "1000000");
        input_select("rk-var", "Method", "method", {"parametric", "historical", "monte_carlo"});
        if (run_button("rk-var")) {
            auto& s = ep("rk-var");
            fire_post("rk-var", "risk/var/compute", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01,-0.015,0.008})},
                {"confidence", s.getd("conf")}, {"horizon", s.geti("horizon")},
                {"portfolio_value", s.getd("pv")}, {"method", s.get("method")}
            });
        }
        render_result("rk-var");
        end_endpoint_card();
    }
    if (begin_endpoint_card("rk-es", "Expected Shortfall", "Conditional VaR / CVaR")) {
        input_float("rk-es", "Confidence", "conf", "0.95");
        input_float("rk-es", "Portfolio Value", "pv", "1000000");
        if (run_button("rk-es")) {
            fire_post("rk-es", "risk/var/expected-shortfall", {
                {"returns", json::array({0.01,-0.02,0.015,-0.01,0.02,-0.03,0.005,0.01,-0.015,0.008})},
                {"confidence", ep("rk-es").getd("conf")}, {"portfolio_value", ep("rk-es").getd("pv")}
            });
        }
        render_result("rk-es");
        end_endpoint_card();
    }
    if (begin_endpoint_card("rk-stress", "Stress Scenarios", "Predefined stress test scenarios")) {
        input_select("rk-stress", "Scenario", "scenario", {"2008_crisis", "covid_crash", "rate_shock_up", "rate_shock_down", "custom"});
        input_float("rk-stress", "Shock (%)", "shock", "-20");
        if (run_button("rk-stress")) {
            auto& s = ep("rk-stress");
            fire_post("rk-stress", "risk/stress/scenario", {
                {"portfolio_value", 1000000.0}, {"scenario", s.get("scenario", "2008_crisis")},
                {"custom_shock", s.getd("shock") / 100.0}
            });
        }
        render_result("rk-stress");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_risk_evt_xva() {
    if (begin_endpoint_card("rk-evt", "EVT Tail Risk", "Extreme Value Theory — GPD tail estimation")) {
        input_float("rk-evt", "Threshold", "threshold", "0.02");
        input_float("rk-evt", "Confidence", "conf", "0.999");
        if (run_button("rk-evt")) {
            fire_post("rk-evt", "risk/evt/gpd-fit", {
                {"losses", json::array({0.01,0.005,0.03,0.025,0.04,0.015,0.035,0.05,0.02,0.008})},
                {"threshold", ep("rk-evt").getd("threshold")}, {"confidence", ep("rk-evt").getd("conf")}
            });
        }
        render_result("rk-evt");
        end_endpoint_card();
    }
    if (begin_endpoint_card("rk-cva", "CVA", "Credit Valuation Adjustment")) {
        input_float("rk-cva", "Notional", "notional", "1000000");
        input_float("rk-cva", "PD", "pd", "0.02"); input_float("rk-cva", "LGD", "lgd", "0.4");
        input_float("rk-cva", "Maturity", "T", "5.0");
        if (run_button("rk-cva")) {
            auto& s = ep("rk-cva");
            fire_post("rk-cva", "risk/xva/cva", {
                {"notional", s.getd("notional")}, {"pd", s.getd("pd")},
                {"lgd", s.getd("lgd")}, {"maturity", s.getd("T")}
            });
        }
        render_result("rk-cva");
        end_endpoint_card();
    }
    if (begin_endpoint_card("rk-dva", "DVA / FVA", "Debit & Funding Valuation Adjustments")) {
        input_float("rk-dva", "Notional", "notional", "1000000");
        input_float("rk-dva", "Own PD", "own_pd", "0.01");
        input_float("rk-dva", "Funding Spread", "spread", "0.005");
        input_float("rk-dva", "Maturity", "T", "5.0");
        if (run_button("rk-dva")) {
            auto& s = ep("rk-dva");
            fire_post("rk-dva", "risk/xva/dva-fva", {
                {"notional", s.getd("notional")}, {"own_pd", s.getd("own_pd")},
                {"funding_spread", s.getd("spread")}, {"maturity", s.getd("T")}
            });
        }
        render_result("rk-dva");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_risk_sensitivities() {
    if (begin_endpoint_card("rk-dv01", "DV01 / Duration", "Interest rate sensitivity")) {
        input_float("rk-dv01", "Par Rate", "rate", "0.05");
        input_float("rk-dv01", "Maturity", "T", "10");
        input_float("rk-dv01", "Coupon", "coupon", "0.05");
        input_float("rk-dv01", "Face", "face", "100");
        if (run_button("rk-dv01")) {
            auto& s = ep("rk-dv01");
            fire_post("rk-dv01", "risk/sensitivities/dv01", {
                {"rate", s.getd("rate")}, {"maturity", s.getd("T")},
                {"coupon", s.getd("coupon")}, {"face", s.getd("face")}
            });
        }
        render_result("rk-dv01");
        end_endpoint_card();
    }
    if (begin_endpoint_card("rk-keyrate", "Key Rate Duration", "Bucket-level rate sensitivities")) {
        input_array("rk-keyrate", "Key Rates", "key_rates", "1,2,5,10,20,30");
        input_float("rk-keyrate", "Shift (bp)", "shift", "1");
        if (run_button("rk-keyrate")) {
            fire_post("rk-keyrate", "risk/sensitivities/key-rate-duration", {
                {"key_rates", json::array({1,2,5,10,20,30})},
                {"shift_bp", ep("rk-keyrate").getd("shift")},
                {"coupon", 0.05}, {"face", 100.0}, {"yield", 0.05}
            });
        }
        render_result("rk-keyrate");
        end_endpoint_card();
    }
}

// ── Instruments ────────────────────────────────────────────────────────────

void QuantLibScreen::render_instr_bonds() {
    if (begin_endpoint_card("in-bond-price", "Bond Price", "Price a fixed-rate bond")) {
        input_float("in-bond-price", "Face Value", "face", "100");
        input_float("in-bond-price", "Coupon (%)", "coupon", "5.0");
        input_float("in-bond-price", "Yield (%)", "yield", "4.5");
        input_float("in-bond-price", "Maturity (yr)", "T", "10");
        input_int("in-bond-price", "Freq", "freq", "2");
        if (run_button("in-bond-price")) {
            auto& s = ep("in-bond-price");
            fire_post("in-bond-price", "instruments/bonds/price", {
                {"face", s.getd("face")}, {"coupon_rate", s.getd("coupon") / 100.0},
                {"yield_rate", s.getd("yield") / 100.0}, {"maturity", s.getd("T")},
                {"frequency", s.geti("freq")}
            });
        }
        render_result("in-bond-price");
        end_endpoint_card();
    }
    if (begin_endpoint_card("in-bond-ytm", "Yield to Maturity", "Solve for YTM from bond price")) {
        input_float("in-bond-ytm", "Face", "face", "100");
        input_float("in-bond-ytm", "Coupon (%)", "coupon", "5.0");
        input_float("in-bond-ytm", "Price", "price", "102.5");
        input_float("in-bond-ytm", "Maturity", "T", "10");
        input_int("in-bond-ytm", "Freq", "freq", "2");
        if (run_button("in-bond-ytm")) {
            auto& s = ep("in-bond-ytm");
            fire_post("in-bond-ytm", "instruments/bonds/ytm", {
                {"face", s.getd("face")}, {"coupon_rate", s.getd("coupon") / 100.0},
                {"price", s.getd("price")}, {"maturity", s.getd("T")}, {"frequency", s.geti("freq")}
            });
        }
        render_result("in-bond-ytm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("in-bond-dur", "Bond Duration", "Macaulay & Modified duration")) {
        input_float("in-bond-dur", "Face", "face", "100"); input_float("in-bond-dur", "Coupon (%)", "coupon", "5.0");
        input_float("in-bond-dur", "Yield (%)", "yield", "4.5"); input_float("in-bond-dur", "Maturity", "T", "10");
        if (run_button("in-bond-dur")) {
            auto& s = ep("in-bond-dur");
            fire_post("in-bond-dur", "instruments/bonds/duration", {
                {"face", s.getd("face")}, {"coupon_rate", s.getd("coupon") / 100.0},
                {"yield_rate", s.getd("yield") / 100.0}, {"maturity", s.getd("T")}
            });
        }
        render_result("in-bond-dur");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_swaps() {
    if (begin_endpoint_card("in-irs", "Interest Rate Swap", "Price a vanilla IRS")) {
        input_float("in-irs", "Notional", "notional", "1000000");
        input_float("in-irs", "Fixed Rate (%)", "fixed", "5.0");
        input_float("in-irs", "Float Spread (bp)", "spread", "0");
        input_float("in-irs", "Maturity (yr)", "T", "5");
        input_select("in-irs", "Frequency", "freq", {"quarterly", "semi_annual", "annual"});
        if (run_button("in-irs")) {
            auto& s = ep("in-irs");
            fire_post("in-irs", "instruments/swaps/irs/price", {
                {"notional", s.getd("notional")}, {"fixed_rate", s.getd("fixed") / 100.0},
                {"float_spread", s.getd("spread") / 10000.0}, {"maturity", s.getd("T")},
                {"frequency", s.get("freq", "semi_annual")}
            });
        }
        render_result("in-irs");
        end_endpoint_card();
    }
    if (begin_endpoint_card("in-fra", "FRA", "Forward Rate Agreement pricing")) {
        input_float("in-fra", "Notional", "notional", "1000000");
        input_float("in-fra", "FRA Rate (%)", "fra_rate", "5.0");
        input_float("in-fra", "Start (yr)", "start", "0.5");
        input_float("in-fra", "End (yr)", "end", "1.0");
        input_float("in-fra", "Market Rate (%)", "mkt_rate", "5.2");
        if (run_button("in-fra")) {
            auto& s = ep("in-fra");
            fire_post("in-fra", "instruments/fra/price", {
                {"notional", s.getd("notional")}, {"fra_rate", s.getd("fra_rate") / 100.0},
                {"start", s.getd("start")}, {"end", s.getd("end")},
                {"market_rate", s.getd("mkt_rate") / 100.0}
            });
        }
        render_result("in-fra");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_markets() {
    if (begin_endpoint_card("in-fx-fwd", "FX Forward", "FX forward / NDF pricing")) {
        input_float("in-fx-fwd", "Spot", "spot", "1.10"); input_float("in-fx-fwd", "Domestic Rate (%)", "rd", "5.0");
        input_float("in-fx-fwd", "Foreign Rate (%)", "rf", "3.0"); input_float("in-fx-fwd", "Maturity (yr)", "T", "1.0");
        input_float("in-fx-fwd", "Notional", "notional", "1000000");
        if (run_button("in-fx-fwd")) {
            auto& s = ep("in-fx-fwd");
            fire_post("in-fx-fwd", "instruments/fx/forward", {
                {"spot", s.getd("spot")}, {"domestic_rate", s.getd("rd") / 100.0},
                {"foreign_rate", s.getd("rf") / 100.0}, {"maturity", s.getd("T")},
                {"notional", s.getd("notional")}
            });
        }
        render_result("in-fx-fwd");
        end_endpoint_card();
    }
    if (begin_endpoint_card("in-comm-fwd", "Commodity Forward", "Cost-of-carry commodity forward")) {
        input_float("in-comm-fwd", "Spot", "spot", "50"); input_float("in-comm-fwd", "Rate (%)", "r", "5.0");
        input_float("in-comm-fwd", "Storage Cost (%)", "storage", "2.0");
        input_float("in-comm-fwd", "Conv Yield (%)", "conv", "1.0");
        input_float("in-comm-fwd", "Maturity", "T", "1.0");
        if (run_button("in-comm-fwd")) {
            auto& s = ep("in-comm-fwd");
            fire_post("in-comm-fwd", "instruments/commodity/forward", {
                {"spot", s.getd("spot")}, {"rate", s.getd("r") / 100.0},
                {"storage_cost", s.getd("storage") / 100.0},
                {"convenience_yield", s.getd("conv") / 100.0}, {"maturity", s.getd("T")}
            });
        }
        render_result("in-comm-fwd");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_instr_creditfut() {
    if (begin_endpoint_card("in-cds", "CDS Spread", "Credit Default Swap pricing")) {
        input_float("in-cds", "Notional", "notional", "10000000");
        input_float("in-cds", "Spread (bp)", "spread", "100");
        input_float("in-cds", "Recovery Rate", "recovery", "0.4");
        input_float("in-cds", "Maturity (yr)", "T", "5");
        input_float("in-cds", "Risk-Free Rate", "rf", "0.04");
        if (run_button("in-cds")) {
            auto& s = ep("in-cds");
            fire_post("in-cds", "instruments/credit/cds/price", {
                {"notional", s.getd("notional")}, {"spread", s.getd("spread") / 10000.0},
                {"recovery_rate", s.getd("recovery")}, {"maturity", s.getd("T")},
                {"risk_free_rate", s.getd("rf")}
            });
        }
        render_result("in-cds");
        end_endpoint_card();
    }
    if (begin_endpoint_card("in-fut", "Futures Price", "Cost-of-carry futures pricing")) {
        input_float("in-fut", "Spot", "spot", "100"); input_float("in-fut", "Rate (%)", "r", "5.0");
        input_float("in-fut", "Div Yield (%)", "q", "2.0"); input_float("in-fut", "Maturity", "T", "0.25");
        if (run_button("in-fut")) {
            auto& s = ep("in-fut");
            fire_post("in-fut", "instruments/futures/price", {
                {"spot", s.getd("spot")}, {"rate", s.getd("r") / 100.0},
                {"dividend_yield", s.getd("q") / 100.0}, {"maturity", s.getd("T")}
            });
        }
        render_result("in-fut");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
