// QuantLib Panels — Core (Types, Conventions, AutoDiff, Distributions, Math, Ops, Legs, Periods)
//                   Regulatory (Basel, SA-CCR, IFRS9, Liquidity, Stress)
#include "quantlib_screen.h"
#include <imgui.h>
#include <sstream>

namespace fincept::quantlib {

// ============================================================================
// Core — Types (8 endpoints)
// ============================================================================
void QuantLibScreen::render_types() {
    if (begin_endpoint_card("types-tenor", "Tenor Add to Date", "Add a tenor period to a date")) {
        input_text("types-tenor", "Tenor", "tenor", "6M");
        input_text("types-tenor", "Start Date", "start_date", "2024-01-15");
        if (run_button("types-tenor")) {
            auto& s = ep("types-tenor");
            fire_post("types-tenor", "core/types/tenor/add-to-date", {
                {"tenor", s.get("tenor", "6M")},
                {"start_date", s.get("start_date", "2024-01-15")}
            });
        }
        render_result("types-tenor");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-rate", "Rate Convert", "Convert between rate conventions")) {
        input_float("types-rate", "Value", "value", "0.05");
        input_select("types-rate", "Unit", "unit", {"percent", "bps", "decimal"});
        if (run_button("types-rate")) {
            auto& s = ep("types-rate");
            fire_post("types-rate", "core/types/rate/convert", {
                {"value", s.getd("value", 0.05)}, {"unit", s.get("unit", "percent")}
            });
        }
        render_result("types-rate");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-money-create", "Money Create", "Create money amount")) {
        input_float("types-money-create", "Amount", "amount", "1000000");
        input_text("types-money-create", "Currency", "currency", "USD");
        if (run_button("types-money-create")) {
            auto& s = ep("types-money-create");
            fire_post("types-money-create", "core/types/money/create", {
                {"amount", s.getd("amount")}, {"currency", s.get("currency", "USD")}
            });
        }
        render_result("types-money-create");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-money-convert", "Money Convert", "Convert between currencies")) {
        input_float("types-money-convert", "Amount", "amount", "1000000");
        input_text("types-money-convert", "From", "from_currency", "USD");
        input_text("types-money-convert", "To", "to_currency", "EUR");
        input_float("types-money-convert", "Rate", "rate", "0.92");
        if (run_button("types-money-convert")) {
            auto& s = ep("types-money-convert");
            fire_post("types-money-convert", "core/types/money/convert", {
                {"amount", s.getd("amount")}, {"from_currency", s.get("from_currency")},
                {"to_currency", s.get("to_currency")}, {"rate", s.getd("rate")}
            });
        }
        render_result("types-money-convert");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-spread", "Spread from BPS", "Convert basis points to spread")) {
        input_float("types-spread", "BPS", "bps", "250");
        if (run_button("types-spread")) {
            fire_get("types-spread", "core/types/spread/from-bps",
                {{"bps", ep("types-spread").get("bps", "250")}});
        }
        render_result("types-spread");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-notional", "Notional Schedule", "Generate amortizing notional schedule")) {
        input_select("types-notional", "Type", "schedule_type", {"bullet", "linear", "annuity"});
        input_float("types-notional", "Notional", "notional", "1000000");
        input_int("types-notional", "Periods", "periods", "10");
        input_float("types-notional", "Rate", "rate", "0.05");
        if (run_button("types-notional")) {
            auto& s = ep("types-notional");
            fire_post("types-notional", "core/types/notional-schedule", {
                {"schedule_type", s.get("schedule_type", "bullet")},
                {"notional", s.getd("notional")}, {"periods", s.geti("periods", 10)},
                {"rate", s.getd("rate", 0.05)}
            });
        }
        render_result("types-notional");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-currencies", "List Currencies", "All supported currencies")) {
        if (run_button("types-currencies")) {
            fire_get("types-currencies", "core/types/currencies");
        }
        render_result("types-currencies");
        end_endpoint_card();
    }
    if (begin_endpoint_card("types-frequencies", "List Frequencies", "All supported payment frequencies")) {
        if (run_button("types-frequencies")) {
            fire_get("types-frequencies", "core/types/frequencies");
        }
        render_result("types-frequencies");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Conventions (6 endpoints)
// ============================================================================
void QuantLibScreen::render_conventions() {
    if (begin_endpoint_card("conv-format", "Format Date", "Format a date string")) {
        input_text("conv-format", "Date", "date_str", "2024-01-15");
        input_text("conv-format", "Format", "format", "%Y-%m-%d");
        if (run_button("conv-format")) {
            auto& s = ep("conv-format");
            fire_post("conv-format", "core/conventions/format-date", {
                {"date_str", s.get("date_str")}, {"format", s.get("format")}
            });
        }
        render_result("conv-format");
        end_endpoint_card();
    }
    if (begin_endpoint_card("conv-parse", "Parse Date", "Parse a date string")) {
        input_text("conv-parse", "Date String", "date_string", "January 15, 2024");
        if (run_button("conv-parse")) {
            fire_post("conv-parse", "core/conventions/parse-date", {
                {"date_string", ep("conv-parse").get("date_string")}
            });
        }
        render_result("conv-parse");
        end_endpoint_card();
    }
    if (begin_endpoint_card("conv-d2y", "Days to Years", "Convert day count to year fraction")) {
        input_float("conv-d2y", "Value", "value", "180");
        input_select("conv-d2y", "Convention", "convention", {"ACT/360", "ACT/365", "ACT/ACT", "30/360"});
        if (run_button("conv-d2y")) {
            auto& s = ep("conv-d2y");
            fire_post("conv-d2y", "core/conventions/days-to-years", {
                {"value", s.getd("value")}, {"convention", s.get("convention")}
            });
        }
        render_result("conv-d2y");
        end_endpoint_card();
    }
    if (begin_endpoint_card("conv-y2d", "Years to Days", "Convert year fraction to day count")) {
        input_float("conv-y2d", "Value", "value", "0.5");
        input_select("conv-y2d", "Convention", "convention", {"ACT/360", "ACT/365", "ACT/ACT", "30/360"});
        if (run_button("conv-y2d")) {
            auto& s = ep("conv-y2d");
            fire_post("conv-y2d", "core/conventions/years-to-days", {
                {"value", s.getd("value")}, {"convention", s.get("convention")}
            });
        }
        render_result("conv-y2d");
        end_endpoint_card();
    }
    if (begin_endpoint_card("conv-norm-rate", "Normalize Rate", "Normalize rate value")) {
        input_float("conv-norm-rate", "Value", "value", "5.0");
        input_select("conv-norm-rate", "Unit", "unit", {"percent", "bps", "decimal"});
        if (run_button("conv-norm-rate")) {
            auto& s = ep("conv-norm-rate");
            fire_post("conv-norm-rate", "core/conventions/normalize-rate", {
                {"value", s.getd("value")}, {"unit", s.get("unit")}
            });
        }
        render_result("conv-norm-rate");
        end_endpoint_card();
    }
    if (begin_endpoint_card("conv-norm-vol", "Normalize Volatility", "Normalize volatility value")) {
        input_float("conv-norm-vol", "Value", "value", "20");
        input_select("conv-norm-vol", "Type", "vol_type", {"normal", "lognormal"});
        input_select("conv-norm-vol", "Is Percent", "is_percent", {"true", "false"});
        if (run_button("conv-norm-vol")) {
            auto& s = ep("conv-norm-vol");
            fire_post("conv-norm-vol", "core/conventions/normalize-volatility", {
                {"value", s.getd("value")}, {"vol_type", s.get("vol_type")},
                {"is_percent", s.get("is_percent") == "true"}
            });
        }
        render_result("conv-norm-vol");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — AutoDiff (3 endpoints)
// ============================================================================
void QuantLibScreen::render_autodiff() {
    if (begin_endpoint_card("ad-dual", "Dual Eval", "Evaluate function with dual numbers")) {
        input_select("ad-dual", "Function", "func_name", {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_float("ad-dual", "x", "x", "1.0");
        input_int("ad-dual", "Power N", "power_n", "2");
        if (run_button("ad-dual")) {
            auto& s = ep("ad-dual");
            fire_post("ad-dual", "core/autodiff/dual-eval", {
                {"func_name", s.get("func_name")}, {"x", s.getd("x")}, {"power_n", s.geti("power_n", 2)}
            });
        }
        render_result("ad-dual");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ad-gradient", "Gradient", "Compute gradient vector")) {
        input_select("ad-gradient", "Function", "func_name", {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_array("ad-gradient", "x[]", "x", "1.0, 2.0, 3.0");
        if (run_button("ad-gradient")) {
            auto& s = ep("ad-gradient");
            json x_arr = json::array();
            std::stringstream ss(s.get("x", "1.0, 2.0, 3.0"));
            std::string token;
            while (std::getline(ss, token, ',')) {
                try { x_arr.push_back(std::stod(token)); } catch (...) {}
            }
            fire_post("ad-gradient", "core/autodiff/gradient", {
                {"func_name", s.get("func_name")}, {"x", x_arr}
            });
        }
        render_result("ad-gradient");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ad-taylor", "Taylor Expand", "Taylor series expansion")) {
        input_select("ad-taylor", "Function", "func_name", {"exp", "log", "sin", "cos", "sqrt", "power"});
        input_float("ad-taylor", "x0", "x0", "0.0");
        input_int("ad-taylor", "Order", "order", "5");
        if (run_button("ad-taylor")) {
            auto& s = ep("ad-taylor");
            fire_post("ad-taylor", "core/autodiff/taylor-expand", {
                {"func_name", s.get("func_name")}, {"x0", s.getd("x0")}, {"order", s.geti("order", 5)}
            });
        }
        render_result("ad-taylor");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Distributions (13 endpoints)
// ============================================================================
void QuantLibScreen::render_distributions() {
    auto dist_card = [&](const char* id_suffix, const char* title, const char* path,
                         const std::vector<std::pair<const char*, const char*>>& fields) {
        std::string eid = std::string("dist-") + id_suffix;
        if (begin_endpoint_card(eid, title)) {
            for (auto& [label, def_val] : fields) {
                input_float(eid, label, label, def_val);
            }
            if (run_button(eid)) {
                auto& s = ep(eid);
                json body;
                for (auto& [label, def_val] : fields) {
                    body[label] = s.getd(label, std::stod(def_val));
                }
                fire_post(eid, path, body);
            }
            render_result(eid);
            end_endpoint_card();
        }
    };
    dist_card("norm-cdf", "Normal CDF", "core/distributions/normal/cdf", {{"x", "0.0"}, {"mean", "0.0"}, {"std", "1.0"}});
    dist_card("norm-pdf", "Normal PDF", "core/distributions/normal/pdf", {{"x", "0.0"}, {"mean", "0.0"}, {"std", "1.0"}});
    dist_card("norm-ppf", "Normal PPF", "core/distributions/normal/ppf", {{"p", "0.975"}, {"mean", "0.0"}, {"std", "1.0"}});
    dist_card("t-cdf", "Student-t CDF", "core/distributions/t/cdf", {{"x", "1.96"}, {"df", "10"}});
    dist_card("t-pdf", "Student-t PDF", "core/distributions/t/pdf", {{"x", "0.0"}, {"df", "10"}});
    dist_card("t-ppf", "Student-t PPF", "core/distributions/t/ppf", {{"p", "0.975"}, {"df", "10"}});
    dist_card("chi2-cdf", "Chi-Squared CDF", "core/distributions/chi2/cdf", {{"x", "3.84"}, {"df", "1"}});
    dist_card("chi2-pdf", "Chi-Squared PDF", "core/distributions/chi2/pdf", {{"x", "1.0"}, {"df", "5"}});
    dist_card("gamma-cdf", "Gamma CDF", "core/distributions/gamma/cdf", {{"x", "2.0"}, {"shape", "2.0"}, {"scale", "1.0"}});
    dist_card("gamma-pdf", "Gamma PDF", "core/distributions/gamma/pdf", {{"x", "1.0"}, {"shape", "2.0"}, {"scale", "1.0"}});
    dist_card("exp-cdf", "Exponential CDF", "core/distributions/exponential/cdf", {{"x", "1.0"}, {"rate", "1.0"}});
    dist_card("exp-pdf", "Exponential PDF", "core/distributions/exponential/pdf", {{"x", "1.0"}, {"rate", "1.0"}});
    dist_card("exp-ppf", "Exponential PPF", "core/distributions/exponential/ppf", {{"p", "0.95"}, {"rate", "1.0"}});
}

// ============================================================================
// Core — Math (2 endpoints)
// ============================================================================
void QuantLibScreen::render_math() {
    if (begin_endpoint_card("math-eval", "Math Eval", "Evaluate mathematical function")) {
        input_select("math-eval", "Function", "func_name",
            {"exp", "log", "sin", "cos", "sqrt", "abs", "erf", "erfc", "lgamma"});
        input_float("math-eval", "x", "x", "1.0");
        if (run_button("math-eval")) {
            auto& s = ep("math-eval");
            fire_post("math-eval", "core/math/eval", {
                {"func_name", s.get("func_name")}, {"x", s.getd("x")}
            });
        }
        render_result("math-eval");
        end_endpoint_card();
    }
    if (begin_endpoint_card("math-two", "Two-Arg Math", "Two-argument math function")) {
        input_select("math-two", "Function", "func_name",
            {"pow", "fmod", "max", "min", "atan2", "copysign"});
        input_float("math-two", "x", "x", "2.0");
        input_float("math-two", "y", "y", "3.0");
        if (run_button("math-two")) {
            auto& s = ep("math-two");
            fire_post("math-two", "core/math/two-arg", {
                {"func_name", s.get("func_name")}, {"x", s.getd("x")}, {"y", s.getd("y")}
            });
        }
        render_result("math-two");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Operations (12 endpoints)
// ============================================================================
void QuantLibScreen::render_operations() {
    if (begin_endpoint_card("ops-bs", "Black-Scholes", "Quick BS price")) {
        input_select("ops-bs", "Type", "option_type", {"call", "put"});
        input_float("ops-bs", "Spot", "spot", "100");
        input_float("ops-bs", "Strike", "strike", "100");
        input_float("ops-bs", "Rate", "rate", "0.05");
        input_float("ops-bs", "Time", "time", "1.0");
        input_float("ops-bs", "Vol", "volatility", "0.2");
        if (run_button("ops-bs")) {
            auto& s = ep("ops-bs");
            fire_post("ops-bs", "core/ops/black-scholes", {
                {"option_type", s.get("option_type")}, {"spot", s.getd("spot")},
                {"strike", s.getd("strike")}, {"rate", s.getd("rate")},
                {"time", s.getd("time")}, {"volatility", s.getd("volatility")}
            });
        }
        render_result("ops-bs");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-b76", "Black76", "Black76 option price")) {
        input_select("ops-b76", "Type", "option_type", {"call", "put"});
        input_float("ops-b76", "Forward", "forward", "100");
        input_float("ops-b76", "Strike", "strike", "100");
        input_float("ops-b76", "Vol", "volatility", "0.2");
        input_float("ops-b76", "Time", "time", "1.0");
        input_float("ops-b76", "DF", "discount_factor", "0.95");
        if (run_button("ops-b76")) {
            auto& s = ep("ops-b76");
            fire_post("ops-b76", "core/ops/black76", {
                {"option_type", s.get("option_type")}, {"forward", s.getd("forward")},
                {"strike", s.getd("strike")}, {"volatility", s.getd("volatility")},
                {"time", s.getd("time")}, {"discount_factor", s.getd("discount_factor")}
            });
        }
        render_result("ops-b76");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-var", "Value at Risk", "VaR calculation")) {
        input_select("ops-var", "Method", "method", {"parametric", "historical", "monte_carlo"});
        input_float("ops-var", "Portfolio Value", "portfolio_value", "1000000");
        input_float("ops-var", "Portfolio Std", "portfolio_std", "0.02");
        input_float("ops-var", "Confidence", "confidence", "0.95");
        input_int("ops-var", "Holding Period", "holding_period", "1");
        if (run_button("ops-var")) {
            auto& s = ep("ops-var");
            fire_post("ops-var", "core/ops/var", {
                {"method", s.get("method")}, {"portfolio_value", s.getd("portfolio_value")},
                {"portfolio_std", s.getd("portfolio_std")}, {"confidence", s.getd("confidence")},
                {"holding_period", s.geti("holding_period", 1)}
            });
        }
        render_result("ops-var");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-interp", "Interpolate", "1D interpolation")) {
        input_select("ops-interp", "Method", "method", {"linear", "cubic", "log_linear"});
        input_float("ops-interp", "x", "x", "2.5");
        input_array("ops-interp", "x_data", "x_data", "1,2,3,4,5");
        input_array("ops-interp", "y_data", "y_data", "1,4,9,16,25");
        if (run_button("ops-interp")) {
            auto& s = ep("ops-interp");
            auto parse_arr = [](const std::string& str) {
                json arr = json::array();
                std::stringstream ss(str);
                std::string tok;
                while (std::getline(ss, tok, ',')) {
                    try { arr.push_back(std::stod(tok)); } catch (...) {}
                }
                return arr;
            };
            fire_post("ops-interp", "core/ops/interpolate", {
                {"method", s.get("method")}, {"x", s.getd("x")},
                {"x_data", parse_arr(s.get("x_data"))}, {"y_data", parse_arr(s.get("y_data"))}
            });
        }
        render_result("ops-interp");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-fwd", "Forward Rate", "Implied forward rate from discount factors")) {
        input_float("ops-fwd", "DF1", "df1", "0.98");
        input_float("ops-fwd", "DF2", "df2", "0.95");
        input_float("ops-fwd", "t1", "t1", "0.5");
        input_float("ops-fwd", "t2", "t2", "1.0");
        if (run_button("ops-fwd")) {
            auto& s = ep("ops-fwd");
            fire_post("ops-fwd", "core/ops/forward-rate", {
                {"df1", s.getd("df1")}, {"df2", s.getd("df2")},
                {"t1", s.getd("t1")}, {"t2", s.getd("t2")}
            });
        }
        render_result("ops-fwd");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-zero", "Zero Rate Convert", "Convert zero rate / discount factor")) {
        input_select("ops-zero", "Direction", "direction", {"rate_to_df", "df_to_rate"});
        input_float("ops-zero", "Value", "value", "0.05");
        input_float("ops-zero", "t", "t", "1.0");
        if (run_button("ops-zero")) {
            auto& s = ep("ops-zero");
            fire_post("ops-zero", "core/ops/zero-rate-convert", {
                {"direction", s.get("direction")}, {"value", s.getd("value")}, {"t", s.getd("t")}
            });
        }
        render_result("ops-zero");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-gbm", "GBM Paths", "Geometric Brownian Motion simulation")) {
        input_float("ops-gbm", "Spot", "spot", "100");
        input_float("ops-gbm", "Drift", "drift", "0.05");
        input_float("ops-gbm", "Vol", "volatility", "0.2");
        input_float("ops-gbm", "Time", "time", "1.0");
        input_int("ops-gbm", "Paths", "n_paths", "100");
        input_int("ops-gbm", "Steps", "n_steps", "252");
        if (run_button("ops-gbm")) {
            auto& s = ep("ops-gbm");
            fire_post("ops-gbm", "core/ops/gbm-paths", {
                {"spot", s.getd("spot")}, {"drift", s.getd("drift")},
                {"volatility", s.getd("volatility")}, {"time", s.getd("time")},
                {"n_paths", s.geti("n_paths", 100)}, {"n_steps", s.geti("n_steps", 252)},
                {"seed", 42}, {"antithetic", true}
            });
        }
        render_result("ops-gbm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ops-stats", "Statistics", "Descriptive statistics on values")) {
        input_array("ops-stats", "Values", "values", "1.2, 2.3, 3.1, 4.5, 2.8");
        input_int("ops-stats", "DDOF", "ddof", "1");
        if (run_button("ops-stats")) {
            auto& s = ep("ops-stats");
            json vals = json::array();
            std::stringstream ss(s.get("values"));
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                try { vals.push_back(std::stod(tok)); } catch (...) {}
            }
            fire_post("ops-stats", "core/ops/statistics", {{"values", vals}, {"ddof", s.geti("ddof", 1)}});
        }
        render_result("ops-stats");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Legs (3 endpoints)
// ============================================================================
void QuantLibScreen::render_legs() {
    if (begin_endpoint_card("legs-fixed", "Fixed Leg", "Generate fixed leg cashflows")) {
        input_float("legs-fixed", "Notional", "notional", "1000000");
        input_float("legs-fixed", "Rate", "rate", "0.05");
        input_text("legs-fixed", "Start", "start_date", "2024-01-15");
        input_text("legs-fixed", "End", "end_date", "2029-01-15");
        input_select("legs-fixed", "Freq", "frequency", {"annual", "semi_annual", "quarterly"});
        input_select("legs-fixed", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_text("legs-fixed", "Currency", "currency", "USD");
        if (run_button("legs-fixed")) {
            auto& s = ep("legs-fixed");
            fire_post("legs-fixed", "core/legs/fixed", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"frequency", s.get("frequency")}, {"day_count", s.get("day_count")},
                {"currency", s.get("currency")}
            });
        }
        render_result("legs-fixed");
        end_endpoint_card();
    }
    if (begin_endpoint_card("legs-float", "Float Leg", "Generate floating leg cashflows")) {
        input_float("legs-float", "Notional", "notional", "1000000");
        input_text("legs-float", "Start", "start_date", "2024-01-15");
        input_text("legs-float", "End", "end_date", "2029-01-15");
        input_float("legs-float", "Spread", "spread", "0.005");
        input_select("legs-float", "Freq", "frequency", {"quarterly", "semi_annual", "annual"});
        input_select("legs-float", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_text("legs-float", "Index", "index_name", "SOFR");
        if (run_button("legs-float")) {
            auto& s = ep("legs-float");
            fire_post("legs-float", "core/legs/float", {
                {"notional", s.getd("notional")}, {"start_date", s.get("start_date")},
                {"end_date", s.get("end_date")}, {"spread", s.getd("spread")},
                {"frequency", s.get("frequency")}, {"day_count", s.get("day_count")},
                {"index_name", s.get("index_name")}
            });
        }
        render_result("legs-float");
        end_endpoint_card();
    }
    if (begin_endpoint_card("legs-zero", "Zero-Coupon Leg", "Generate zero-coupon leg")) {
        input_float("legs-zero", "Notional", "notional", "1000000");
        input_float("legs-zero", "Rate", "rate", "0.05");
        input_text("legs-zero", "Start", "start_date", "2024-01-15");
        input_text("legs-zero", "End", "end_date", "2029-01-15");
        input_select("legs-zero", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        if (run_button("legs-zero")) {
            auto& s = ep("legs-zero");
            fire_post("legs-zero", "core/legs/zero-coupon", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"day_count", s.get("day_count")}
            });
        }
        render_result("legs-zero");
        end_endpoint_card();
    }
}

// ============================================================================
// Core — Periods (3 endpoints)
// ============================================================================
void QuantLibScreen::render_periods() {
    if (begin_endpoint_card("per-fixed", "Fixed Coupon Period", "Calculate fixed coupon period")) {
        input_float("per-fixed", "Notional", "notional", "1000000");
        input_float("per-fixed", "Rate", "rate", "0.05");
        input_text("per-fixed", "Start", "start_date", "2024-01-15");
        input_text("per-fixed", "End", "end_date", "2024-07-15");
        input_select("per-fixed", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        if (run_button("per-fixed")) {
            auto& s = ep("per-fixed");
            fire_post("per-fixed", "core/periods/fixed-coupon", {
                {"notional", s.getd("notional")}, {"rate", s.getd("rate")},
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"day_count", s.get("day_count")}
            });
        }
        render_result("per-fixed");
        end_endpoint_card();
    }
    if (begin_endpoint_card("per-float", "Float Coupon Period", "Calculate floating coupon period")) {
        input_float("per-float", "Notional", "notional", "1000000");
        input_text("per-float", "Start", "start_date", "2024-01-15");
        input_text("per-float", "End", "end_date", "2024-07-15");
        input_float("per-float", "Spread", "spread", "0.005");
        input_select("per-float", "Day Count", "day_count", {"ACT/360", "ACT/365", "30/360"});
        input_float("per-float", "Fixing Rate", "fixing_rate", "0.04");
        if (run_button("per-float")) {
            auto& s = ep("per-float");
            fire_post("per-float", "core/periods/float-coupon", {
                {"notional", s.getd("notional")}, {"start_date", s.get("start_date")},
                {"end_date", s.get("end_date")}, {"spread", s.getd("spread")},
                {"day_count", s.get("day_count")}, {"fixing_rate", s.getd("fixing_rate")}
            });
        }
        render_result("per-float");
        end_endpoint_card();
    }
    if (begin_endpoint_card("per-dcf", "Day Count Fraction", "Calculate day count fraction")) {
        input_text("per-dcf", "Start", "start_date", "2024-01-15");
        input_text("per-dcf", "End", "end_date", "2024-07-15");
        input_select("per-dcf", "Convention", "convention", {"ACT/360", "ACT/365", "ACT/ACT", "30/360", "30E/360"});
        if (run_button("per-dcf")) {
            auto& s = ep("per-dcf");
            fire_post("per-dcf", "core/periods/day-count-fraction", {
                {"start_date", s.get("start_date")}, {"end_date", s.get("end_date")},
                {"convention", s.get("convention")}
            });
        }
        render_result("per-dcf");
        end_endpoint_card();
    }
}

// ============================================================================
// Regulatory — Basel III
// ============================================================================
void QuantLibScreen::render_basel() {
    if (begin_endpoint_card("basel-capital", "Capital Ratios", "Calculate CET1, Tier1, Total capital ratios")) {
        input_float("basel-capital", "CET1 Capital", "cet1_capital", "50000");
        input_float("basel-capital", "Tier1 Capital", "tier1_capital", "60000");
        input_float("basel-capital", "Total Capital", "total_capital", "80000");
        input_float("basel-capital", "RWA", "rwa", "500000");
        if (run_button("basel-capital")) {
            auto& s = ep("basel-capital");
            fire_post("basel-capital", "regulatory/basel/capital-ratios", {
                {"cet1_capital", s.getd("cet1_capital")}, {"tier1_capital", s.getd("tier1_capital")},
                {"total_capital", s.getd("total_capital")}, {"risk_weighted_assets", s.getd("rwa")}
            });
        }
        render_result("basel-capital");
        end_endpoint_card();
    }
    if (begin_endpoint_card("basel-oprwa", "Operational RWA", "Basic indicator approach")) {
        input_array("basel-oprwa", "Gross Income (3y)", "gross_income_3y", "100000,110000,120000");
        if (run_button("basel-oprwa")) {
            fire_post("basel-oprwa", "regulatory/basel/operational-rwa",
                {{"gross_income_3y", json::array({100000, 110000, 120000})}});
        }
        render_result("basel-oprwa");
        end_endpoint_card();
    }
}

// ============================================================================
// Regulatory — SA-CCR
// ============================================================================
void QuantLibScreen::render_saccr() {
    if (begin_endpoint_card("saccr-ead", "SA-CCR EAD", "Standardised approach for counterparty credit risk")) {
        input_float("saccr-ead", "MtM Value", "mtm_value", "1000000");
        input_float("saccr-ead", "Add-on", "addon", "50000");
        input_float("saccr-ead", "Collateral", "collateral", "200000");
        input_float("saccr-ead", "Alpha", "alpha", "1.4");
        if (run_button("saccr-ead")) {
            auto& s = ep("saccr-ead");
            fire_post("saccr-ead", "regulatory/saccr/ead", {
                {"mtm_value", s.getd("mtm_value")}, {"addon", s.getd("addon")},
                {"collateral", s.getd("collateral")}, {"alpha", s.getd("alpha", 1.4)}
            });
        }
        render_result("saccr-ead");
        end_endpoint_card();
    }
}

// ============================================================================
// Regulatory — IFRS 9
// ============================================================================
void QuantLibScreen::render_ifrs9() {
    if (begin_endpoint_card("ifrs9-stage", "Stage Assessment", "IFRS 9 staging classification")) {
        input_int("ifrs9-stage", "Days Past Due", "dpd", "30");
        input_float("ifrs9-stage", "Rating Downgrade", "downgrade", "2");
        input_float("ifrs9-stage", "PD Increase Ratio", "pd_ratio", "2.5");
        if (run_button("ifrs9-stage")) {
            auto& s = ep("ifrs9-stage");
            fire_post("ifrs9-stage", "regulatory/ifrs9/stage-assessment", {
                {"days_past_due", s.geti("dpd")}, {"rating_downgrade", s.getd("downgrade")},
                {"pd_increase_ratio", s.getd("pd_ratio")}
            });
        }
        render_result("ifrs9-stage");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ifrs9-ecl12m", "ECL 12-Month", "12-month expected credit loss")) {
        input_float("ifrs9-ecl12m", "PD 12m", "pd", "0.02");
        input_float("ifrs9-ecl12m", "LGD", "lgd", "0.45");
        input_float("ifrs9-ecl12m", "EAD", "ead", "1000000");
        input_float("ifrs9-ecl12m", "Discount Rate", "dr", "0.05");
        if (run_button("ifrs9-ecl12m")) {
            auto& s = ep("ifrs9-ecl12m");
            fire_post("ifrs9-ecl12m", "regulatory/ifrs9/ecl-12m", {
                {"pd_12m", s.getd("pd")}, {"lgd", s.getd("lgd")},
                {"ead", s.getd("ead")}, {"discount_rate", s.getd("dr")}
            });
        }
        render_result("ifrs9-ecl12m");
        end_endpoint_card();
    }
}

// ============================================================================
// Regulatory — Liquidity
// ============================================================================
void QuantLibScreen::render_liquidity() {
    if (begin_endpoint_card("liq-lcr", "Liquidity Coverage Ratio", "Basel III LCR calculation")) {
        input_float("liq-lcr", "HQLA Level 1", "hqla1", "500000");
        input_float("liq-lcr", "Retail Deposits (stable)", "rd_stable", "200000");
        input_float("liq-lcr", "Unsecured Wholesale", "wholesale", "300000");
        if (run_button("liq-lcr")) {
            auto& s = ep("liq-lcr");
            fire_post("liq-lcr", "regulatory/liquidity/lcr", {
                {"hqla_level1", s.getd("hqla1")}, {"retail_deposits_stable", s.getd("rd_stable")},
                {"unsecured_wholesale", s.getd("wholesale")}
            });
        }
        render_result("liq-lcr");
        end_endpoint_card();
    }
    if (begin_endpoint_card("liq-nsfr", "Net Stable Funding Ratio", "Basel III NSFR calculation")) {
        input_float("liq-nsfr", "Capital", "capital", "100000");
        input_float("liq-nsfr", "Stable Deposits", "stable", "300000");
        input_float("liq-nsfr", "Loans Retail", "loans_retail", "400000");
        if (run_button("liq-nsfr")) {
            auto& s = ep("liq-nsfr");
            fire_post("liq-nsfr", "regulatory/liquidity/nsfr", {
                {"capital", s.getd("capital")}, {"stable_deposits_retail", s.getd("stable")},
                {"loans_retail", s.getd("loans_retail")}
            });
        }
        render_result("liq-nsfr");
        end_endpoint_card();
    }
}

// ============================================================================
// Regulatory — Stress Test
// ============================================================================
void QuantLibScreen::render_stress() {
    if (begin_endpoint_card("stress-proj", "Capital Projection", "Multi-period stress test capital projection")) {
        input_float("stress-proj", "Initial Capital", "capital", "100000");
        input_float("stress-proj", "Initial RWA", "rwa", "500000");
        input_array("stress-proj", "Earnings", "earnings", "10000,9000,8000");
        input_array("stress-proj", "Losses", "losses", "5000,15000,20000");
        if (run_button("stress-proj")) {
            auto& s = ep("stress-proj");
            fire_post("stress-proj", "regulatory/stress/capital-projection", {
                {"initial_capital", s.getd("capital")}, {"initial_rwa", s.getd("rwa")},
                {"earnings", json::array({10000,9000,8000})},
                {"losses", json::array({5000,15000,20000})},
                {"rwa_changes", json::array({0,10000,20000})}
            });
        }
        render_result("stress-proj");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
