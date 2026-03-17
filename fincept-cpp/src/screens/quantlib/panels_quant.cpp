// QuantLib Panels — Economics (Equilibrium, GameTheory, Auctions, Utility)
//                   Solver (Bond, Rates, Cashflow)
//                   Numerical (DiffFFT, InterpLinAlg, Solvers)
//                   Scheduling (Calendar, DayCount)
//                   Physics (Entropy, Thermo)
//                   Analysis (Fundamentals, ProfitLiq, EffGrowth, LevCF,
//                             ValQual, Industry, DCF, Models)
#include "quantlib_screen.h"
#include <imgui.h>

namespace fincept::quantlib {

// ── Economics ──────────────────────────────────────────────────────────────

void QuantLibScreen::render_econ_equilibrium() {
    if (begin_endpoint_card("ec-capm", "CAPM", "Capital Asset Pricing Model expected return")) {
        input_float("ec-capm", "Risk-Free Rate", "rf", "0.04");
        input_float("ec-capm", "Market Return", "rm", "0.10");
        input_float("ec-capm", "Beta", "beta", "1.2");
        if (run_button("ec-capm")) {
            auto& s = ep("ec-capm");
            fire_post("ec-capm", "economics/equilibrium/capm", {
                {"rf", s.getd("rf")}, {"rm", s.getd("rm")}, {"beta", s.getd("beta")}
            });
        }
        render_result("ec-capm");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ec-apt", "APT", "Arbitrage Pricing Theory multi-factor model")) {
        input_array("ec-apt", "Factor Betas", "betas", "1.2,0.8,0.5");
        input_array("ec-apt", "Factor Premia", "premia", "0.05,0.03,0.02");
        input_float("ec-apt", "Risk-Free Rate", "rf", "0.04");
        if (run_button("ec-apt")) {
            fire_post("ec-apt", "economics/equilibrium/apt", {
                {"betas", json::array({1.2,0.8,0.5})},
                {"factor_premia", json::array({0.05,0.03,0.02})},
                {"rf", ep("ec-apt").getd("rf")}
            });
        }
        render_result("ec-apt");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_gametheory() {
    if (begin_endpoint_card("ec-nash", "Nash Equilibrium", "Find Nash equilibrium in 2-player game")) {
        input_text("ec-nash", "Player A Payoffs (row;row)", "payA", "3,0;5,1");
        input_text("ec-nash", "Player B Payoffs (row;row)", "payB", "3,5;0,1");
        if (run_button("ec-nash")) {
            fire_post("ec-nash", "economics/gametheory/nash", {
                {"payoff_A", json::array({json::array({3,0}), json::array({5,1})})},
                {"payoff_B", json::array({json::array({3,5}), json::array({0,1})})}
            });
        }
        render_result("ec-nash");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ec-minimax", "Minimax", "Zero-sum game minimax solution")) {
        input_text("ec-minimax", "Payoff Matrix (row;row)", "matrix", "3,-2,4;-1,5,-3;2,1,-2");
        if (run_button("ec-minimax")) {
            fire_post("ec-minimax", "economics/gametheory/minimax", {
                {"payoff_matrix", json::array({json::array({3,-2,4}), json::array({-1,5,-3}), json::array({2,1,-2})})}
            });
        }
        render_result("ec-minimax");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_auctions() {
    if (begin_endpoint_card("ec-vickrey", "Vickrey Auction", "Second-price sealed-bid auction")) {
        input_array("ec-vickrey", "Bids", "bids", "100,85,92,78,110");
        if (run_button("ec-vickrey")) {
            fire_post("ec-vickrey", "economics/auctions/vickrey", {
                {"bids", json::array({100,85,92,78,110})}
            });
        }
        render_result("ec-vickrey");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ec-english", "English Auction", "Ascending price auction simulation")) {
        input_array("ec-english", "Valuations", "vals", "100,85,92,78,110");
        input_float("ec-english", "Reserve", "reserve", "50");
        input_float("ec-english", "Increment", "increment", "5");
        if (run_button("ec-english")) {
            fire_post("ec-english", "economics/auctions/english", {
                {"valuations", json::array({100,85,92,78,110})},
                {"reserve_price", ep("ec-english").getd("reserve")},
                {"increment", ep("ec-english").getd("increment")}
            });
        }
        render_result("ec-english");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_econ_utility() {
    if (begin_endpoint_card("ec-crra", "CRRA Utility", "Constant Relative Risk Aversion utility")) {
        input_float("ec-crra", "Wealth", "w", "100000");
        input_float("ec-crra", "Gamma", "gamma", "2.0");
        if (run_button("ec-crra")) {
            fire_post("ec-crra", "economics/utility/crra", {
                {"wealth", ep("ec-crra").getd("w")}, {"gamma", ep("ec-crra").getd("gamma")}
            });
        }
        render_result("ec-crra");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ec-cara", "CARA Utility", "Constant Absolute Risk Aversion utility")) {
        input_float("ec-cara", "Wealth", "w", "100000");
        input_float("ec-cara", "Alpha", "alpha", "0.00001");
        if (run_button("ec-cara")) {
            fire_post("ec-cara", "economics/utility/cara", {
                {"wealth", ep("ec-cara").getd("w")}, {"alpha", ep("ec-cara").getd("alpha")}
            });
        }
        render_result("ec-cara");
        end_endpoint_card();
    }
}

// ── Solver ─────────────────────────────────────────────────────────────────

void QuantLibScreen::render_solver_bond() {
    if (begin_endpoint_card("sv-bond-px", "Bond Pricer", "Analytical bond pricing with full analytics")) {
        input_float("sv-bond-px", "Face", "face", "100"); input_float("sv-bond-px", "Coupon (%)", "coupon", "5.0");
        input_float("sv-bond-px", "Yield (%)", "yield", "4.5"); input_float("sv-bond-px", "Maturity", "T", "10");
        input_int("sv-bond-px", "Freq", "freq", "2");
        if (run_button("sv-bond-px")) {
            auto& s = ep("sv-bond-px");
            fire_post("sv-bond-px", "solver/bond/price", {
                {"face", s.getd("face")}, {"coupon_rate", s.getd("coupon") / 100.0},
                {"yield_rate", s.getd("yield") / 100.0}, {"maturity", s.getd("T")},
                {"frequency", s.geti("freq")}
            });
        }
        render_result("sv-bond-px");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sv-bond-convex", "Convexity", "Bond convexity calculation")) {
        input_float("sv-bond-convex", "Face", "face", "100"); input_float("sv-bond-convex", "Coupon (%)", "coupon", "5.0");
        input_float("sv-bond-convex", "Yield (%)", "yield", "4.5"); input_float("sv-bond-convex", "Maturity", "T", "10");
        if (run_button("sv-bond-convex")) {
            auto& s = ep("sv-bond-convex");
            fire_post("sv-bond-convex", "solver/bond/convexity", {
                {"face", s.getd("face")}, {"coupon_rate", s.getd("coupon") / 100.0},
                {"yield_rate", s.getd("yield") / 100.0}, {"maturity", s.getd("T")}
            });
        }
        render_result("sv-bond-convex");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_solver_rates() {
    if (begin_endpoint_card("sv-swap-rate", "Swap Rate", "Par swap rate from discount curve")) {
        input_array("sv-swap-rate", "Tenors", "tenors", "1,2,3,5,7,10");
        input_array("sv-swap-rate", "Discount Factors", "dfs", "0.961,0.924,0.889,0.822,0.760,0.676");
        input_float("sv-swap-rate", "Maturity", "T", "5");
        if (run_button("sv-swap-rate")) {
            fire_post("sv-swap-rate", "solver/rates/swap-rate", {
                {"tenors", json::array({1,2,3,5,7,10})},
                {"discount_factors", json::array({0.961,0.924,0.889,0.822,0.760,0.676})},
                {"maturity", ep("sv-swap-rate").getd("T")}
            });
        }
        render_result("sv-swap-rate");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sv-iv-solve", "IV Solver", "Newton-Raphson implied volatility solver")) {
        input_float("sv-iv-solve", "Market Price", "price", "10.45");
        input_float("sv-iv-solve", "Spot", "S", "100"); input_float("sv-iv-solve", "Strike", "K", "100");
        input_float("sv-iv-solve", "Time", "T", "1.0"); input_float("sv-iv-solve", "Rate", "r", "0.05");
        input_select("sv-iv-solve", "Type", "type", {"call", "put"});
        if (run_button("sv-iv-solve")) {
            auto& s = ep("sv-iv-solve");
            fire_post("sv-iv-solve", "solver/rates/implied-vol", {
                {"market_price", s.getd("price")}, {"S", s.getd("S")}, {"K", s.getd("K")},
                {"T", s.getd("T")}, {"r", s.getd("r")}, {"option_type", s.get("type", "call")}
            });
        }
        render_result("sv-iv-solve");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_solver_cashflow() {
    if (begin_endpoint_card("sv-npv", "NPV / IRR", "Net Present Value and Internal Rate of Return")) {
        input_array("sv-npv", "Cash Flows", "cfs", "-1000,200,300,400,500");
        input_float("sv-npv", "Discount Rate (%)", "rate", "10");
        if (run_button("sv-npv")) {
            fire_post("sv-npv", "solver/cashflow/npv-irr", {
                {"cashflows", json::array({-1000,200,300,400,500})},
                {"discount_rate", ep("sv-npv").getd("rate") / 100.0}
            });
        }
        render_result("sv-npv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sv-annuity", "Annuity", "Annuity PV / FV / Payment calculation")) {
        input_float("sv-annuity", "Payment", "pmt", "1000");
        input_float("sv-annuity", "Rate (%)", "rate", "5");
        input_int("sv-annuity", "Periods", "n", "20");
        input_select("sv-annuity", "Type", "type", {"ordinary", "due"});
        if (run_button("sv-annuity")) {
            auto& s = ep("sv-annuity");
            fire_post("sv-annuity", "solver/cashflow/annuity", {
                {"payment", s.getd("pmt")}, {"rate", s.getd("rate") / 100.0},
                {"periods", s.geti("n")}, {"type", s.get("type", "ordinary")}
            });
        }
        render_result("sv-annuity");
        end_endpoint_card();
    }
}

// ── Numerical ──────────────────────────────────────────────────────────────

void QuantLibScreen::render_num_difffft() {
    if (begin_endpoint_card("nm-diff", "Numerical Differentiation", "Forward / central / backward finite difference")) {
        input_text("nm-diff", "Function", "func", "x^2 + 3*x");
        input_float("nm-diff", "x", "x", "2.0"); input_float("nm-diff", "h", "h", "0.001");
        input_select("nm-diff", "Method", "method", {"forward", "central", "backward"});
        if (run_button("nm-diff")) {
            auto& s = ep("nm-diff");
            fire_post("nm-diff", "numerical/differentiation/compute", {
                {"function", s.get("func")}, {"x", s.getd("x")}, {"h", s.getd("h")},
                {"method", s.get("method", "central")}
            });
        }
        render_result("nm-diff");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-fft", "FFT", "Fast Fourier Transform")) {
        input_array("nm-fft", "Signal (real)", "signal", "1,0,1,0,1,0,1,0");
        if (run_button("nm-fft")) {
            fire_post("nm-fft", "numerical/fft/compute", {
                {"signal", json::array({1,0,1,0,1,0,1,0})}
            });
        }
        render_result("nm-fft");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-integ", "Integration", "Numerical quadrature (Simpson, Gauss-Legendre)")) {
        input_text("nm-integ", "Function", "func", "x^2");
        input_float("nm-integ", "a", "a", "0"); input_float("nm-integ", "b", "b", "1");
        input_select("nm-integ", "Method", "method", {"simpson", "gauss_legendre", "trapezoidal"});
        if (run_button("nm-integ")) {
            auto& s = ep("nm-integ");
            fire_post("nm-integ", "numerical/integration/compute", {
                {"function", s.get("func")}, {"a", s.getd("a")}, {"b", s.getd("b")},
                {"method", s.get("method", "simpson")}
            });
        }
        render_result("nm-integ");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_num_interplinalg() {
    if (begin_endpoint_card("nm-interp", "Interpolation", "1D interpolation (linear, cubic, spline)")) {
        input_array("nm-interp", "X", "x", "1,2,3,4,5");
        input_array("nm-interp", "Y", "y", "1,4,9,16,25");
        input_array("nm-interp", "Query X", "qx", "1.5,2.5,3.5,4.5");
        input_select("nm-interp", "Method", "method", {"linear", "cubic", "cubic_spline"});
        if (run_button("nm-interp")) {
            fire_post("nm-interp", "numerical/interpolation/compute", {
                {"x", json::array({1,2,3,4,5})}, {"y", json::array({1,4,9,16,25})},
                {"query_x", json::array({1.5,2.5,3.5,4.5})},
                {"method", ep("nm-interp").get("method", "linear")}
            });
        }
        render_result("nm-interp");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-linalg", "Linear Algebra", "Matrix operations (det, inv, eigen, SVD)")) {
        input_select("nm-linalg", "Operation", "op", {"determinant", "inverse", "eigenvalues", "svd"});
        if (run_button("nm-linalg")) {
            fire_post("nm-linalg", "numerical/linalg/compute", {
                {"matrix", json::array({json::array({4,7}), json::array({2,6})})},
                {"operation", ep("nm-linalg").get("op", "determinant")}
            });
        }
        render_result("nm-linalg");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_num_solvers() {
    if (begin_endpoint_card("nm-ode", "ODE Solver", "Ordinary differential equation solver")) {
        input_text("nm-ode", "dy/dt =", "func", "-2*y");
        input_float("nm-ode", "y0", "y0", "1.0"); input_float("nm-ode", "t0", "t0", "0");
        input_float("nm-ode", "t_end", "t_end", "5"); input_int("nm-ode", "Steps", "steps", "100");
        input_select("nm-ode", "Method", "method", {"euler", "rk4", "rk45"});
        if (run_button("nm-ode")) {
            auto& s = ep("nm-ode");
            fire_post("nm-ode", "numerical/ode/solve", {
                {"function", s.get("func")}, {"y0", s.getd("y0")}, {"t0", s.getd("t0")},
                {"t_end", s.getd("t_end")}, {"n_steps", s.geti("steps")},
                {"method", s.get("method", "rk4")}
            });
        }
        render_result("nm-ode");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-root", "Root Finding", "Find roots of f(x) = 0")) {
        input_text("nm-root", "f(x) =", "func", "x^3 - x - 2");
        input_float("nm-root", "x0 (initial)", "x0", "1.5");
        input_select("nm-root", "Method", "method", {"newton", "bisection", "brent", "secant"});
        if (run_button("nm-root")) {
            auto& s = ep("nm-root");
            fire_post("nm-root", "numerical/roots/solve", {
                {"function", s.get("func")}, {"x0", s.getd("x0")},
                {"method", s.get("method", "newton")}
            });
        }
        render_result("nm-root");
        end_endpoint_card();
    }
    if (begin_endpoint_card("nm-optim", "Optimization", "Minimize/maximize objective function")) {
        input_text("nm-optim", "f(x) =", "func", "(x-3)^2 + 1");
        input_float("nm-optim", "x0", "x0", "0");
        input_select("nm-optim", "Method", "method", {"bfgs", "nelder_mead", "gradient_descent"});
        if (run_button("nm-optim")) {
            auto& s = ep("nm-optim");
            fire_post("nm-optim", "numerical/optimization/minimize", {
                {"function", s.get("func")}, {"x0", s.getd("x0")},
                {"method", s.get("method", "bfgs")}
            });
        }
        render_result("nm-optim");
        end_endpoint_card();
    }
}

// ── Scheduling ─────────────────────────────────────────────────────────────

void QuantLibScreen::render_sched_calendar() {
    if (begin_endpoint_card("sc-bdays", "Business Days", "Count / list business days between dates")) {
        input_text("sc-bdays", "Start Date", "start", "2025-01-01");
        input_text("sc-bdays", "End Date", "end", "2025-12-31");
        input_select("sc-bdays", "Calendar", "cal", {"US", "UK", "EU", "JP", "IN", "CN"});
        if (run_button("sc-bdays")) {
            auto& s = ep("sc-bdays");
            fire_post("sc-bdays", "scheduling/calendar/business-days", {
                {"start_date", s.get("start")}, {"end_date", s.get("end")},
                {"calendar", s.get("cal", "US")}
            });
        }
        render_result("sc-bdays");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sc-holidays", "Holidays", "List holidays for a calendar year")) {
        input_int("sc-holidays", "Year", "year", "2025");
        input_select("sc-holidays", "Calendar", "cal", {"US", "UK", "EU", "JP", "IN", "CN"});
        if (run_button("sc-holidays")) {
            fire_post("sc-holidays", "scheduling/calendar/holidays", {
                {"year", ep("sc-holidays").geti("year")}, {"calendar", ep("sc-holidays").get("cal", "US")}
            });
        }
        render_result("sc-holidays");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sched_daycount() {
    if (begin_endpoint_card("sc-dcf", "Day Count Fraction", "Year fraction between dates")) {
        input_text("sc-dcf", "Start", "start", "2025-01-15");
        input_text("sc-dcf", "End", "end", "2025-07-15");
        input_select("sc-dcf", "Convention", "conv", {"ACT/360", "ACT/365", "ACT/ACT", "30/360", "30E/360"});
        if (run_button("sc-dcf")) {
            auto& s = ep("sc-dcf");
            fire_post("sc-dcf", "scheduling/daycount/fraction", {
                {"start_date", s.get("start")}, {"end_date", s.get("end")},
                {"convention", s.get("conv", "ACT/360")}
            });
        }
        render_result("sc-dcf");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sc-sched", "Payment Schedule", "Generate coupon/payment schedule")) {
        input_text("sc-sched", "Start", "start", "2025-01-15");
        input_text("sc-sched", "Maturity", "end", "2030-01-15");
        input_select("sc-sched", "Frequency", "freq", {"monthly", "quarterly", "semi_annual", "annual"});
        input_select("sc-sched", "Convention", "conv", {"ACT/360", "ACT/365", "30/360"});
        if (run_button("sc-sched")) {
            auto& s = ep("sc-sched");
            fire_post("sc-sched", "scheduling/schedule/generate", {
                {"start_date", s.get("start")}, {"maturity_date", s.get("end")},
                {"frequency", s.get("freq", "semi_annual")}, {"day_count", s.get("conv", "ACT/360")}
            });
        }
        render_result("sc-sched");
        end_endpoint_card();
    }
}

// ── Physics ────────────────────────────────────────────────────────────────

void QuantLibScreen::render_phys_entropy() {
    if (begin_endpoint_card("ph-shannon", "Shannon Entropy", "Information entropy of a distribution")) {
        input_array("ph-shannon", "Probabilities", "probs", "0.25,0.25,0.25,0.25");
        if (run_button("ph-shannon")) {
            fire_post("ph-shannon", "physics/entropy/shannon", {
                {"probabilities", json::array({0.25,0.25,0.25,0.25})}
            });
        }
        render_result("ph-shannon");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ph-kl", "KL Divergence", "Kullback-Leibler divergence between distributions")) {
        input_array("ph-kl", "P", "p", "0.3,0.4,0.3");
        input_array("ph-kl", "Q", "q", "0.25,0.5,0.25");
        if (run_button("ph-kl")) {
            fire_post("ph-kl", "physics/entropy/kl-divergence", {
                {"p", json::array({0.3,0.4,0.3})}, {"q", json::array({0.25,0.5,0.25})}
            });
        }
        render_result("ph-kl");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ph-maxent", "Max Entropy", "Maximum entropy distribution")) {
        input_int("ph-maxent", "N States", "n", "4");
        input_float("ph-maxent", "Mean Constraint", "mean", "2.0");
        if (run_button("ph-maxent")) {
            fire_post("ph-maxent", "physics/entropy/max-entropy", {
                {"n_states", ep("ph-maxent").geti("n")}, {"mean_constraint", ep("ph-maxent").getd("mean")}
            });
        }
        render_result("ph-maxent");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_phys_thermo() {
    if (begin_endpoint_card("ph-boltz", "Boltzmann Distribution", "Canonical ensemble energy distribution")) {
        input_array("ph-boltz", "Energy Levels", "energies", "0,1,2,3,4");
        input_float("ph-boltz", "Temperature", "temp", "300");
        if (run_button("ph-boltz")) {
            fire_post("ph-boltz", "physics/thermo/boltzmann", {
                {"energies", json::array({0,1,2,3,4})}, {"temperature", ep("ph-boltz").getd("temp")}
            });
        }
        render_result("ph-boltz");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ph-partition", "Partition Function", "Statistical mechanical partition function")) {
        input_array("ph-partition", "Energy Levels", "energies", "0,1,2,3");
        input_array("ph-partition", "Degeneracies", "degens", "1,3,5,7");
        input_float("ph-partition", "Temperature", "temp", "300");
        if (run_button("ph-partition")) {
            fire_post("ph-partition", "physics/thermo/partition", {
                {"energies", json::array({0,1,2,3})}, {"degeneracies", json::array({1,3,5,7})},
                {"temperature", ep("ph-partition").getd("temp")}
            });
        }
        render_result("ph-partition");
        end_endpoint_card();
    }
}

// ── Analysis ───────────────────────────────────────────────────────────────

void QuantLibScreen::render_analysis_fundamentals() {
    if (begin_endpoint_card("an-ratios", "Financial Ratios", "Key fundamental ratios from financials")) {
        input_text("an-ratios", "Ticker", "ticker", "AAPL");
        if (run_button("an-ratios")) {
            fire_get("an-ratios", "analysis/fundamentals/ratios", {{"ticker", ep("an-ratios").get("ticker", "AAPL")}});
        }
        render_result("an-ratios");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-dupont", "DuPont Analysis", "DuPont 3/5-factor decomposition of ROE")) {
        input_text("an-dupont", "Ticker", "ticker", "AAPL");
        input_select("an-dupont", "Model", "model", {"3_factor", "5_factor"});
        if (run_button("an-dupont")) {
            fire_get("an-dupont", "analysis/fundamentals/dupont", {
                {"ticker", ep("an-dupont").get("ticker", "AAPL")},
                {"model", ep("an-dupont").get("model", "3_factor")}
            });
        }
        render_result("an-dupont");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_profit_liq() {
    if (begin_endpoint_card("an-profit", "Profitability", "Margin analysis and profitability metrics")) {
        input_text("an-profit", "Ticker", "ticker", "MSFT");
        if (run_button("an-profit")) {
            fire_get("an-profit", "analysis/profitability/margins", {{"ticker", ep("an-profit").get("ticker", "MSFT")}});
        }
        render_result("an-profit");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-liq", "Liquidity Ratios", "Current, quick, cash ratios")) {
        input_text("an-liq", "Ticker", "ticker", "MSFT");
        if (run_button("an-liq")) {
            fire_get("an-liq", "analysis/liquidity/ratios", {{"ticker", ep("an-liq").get("ticker", "MSFT")}});
        }
        render_result("an-liq");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_eff_growth() {
    if (begin_endpoint_card("an-eff", "Efficiency Ratios", "Asset turnover, inventory days, receivables")) {
        input_text("an-eff", "Ticker", "ticker", "AMZN");
        if (run_button("an-eff")) {
            fire_get("an-eff", "analysis/efficiency/ratios", {{"ticker", ep("an-eff").get("ticker", "AMZN")}});
        }
        render_result("an-eff");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-growth", "Growth Metrics", "Revenue, EPS, FCF growth rates")) {
        input_text("an-growth", "Ticker", "ticker", "AMZN");
        input_select("an-growth", "Period", "period", {"1y", "3y", "5y", "10y"});
        if (run_button("an-growth")) {
            fire_get("an-growth", "analysis/growth/metrics", {
                {"ticker", ep("an-growth").get("ticker", "AMZN")},
                {"period", ep("an-growth").get("period", "5y")}
            });
        }
        render_result("an-growth");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_lev_cf() {
    if (begin_endpoint_card("an-lev", "Leverage Ratios", "D/E, interest coverage, debt ratios")) {
        input_text("an-lev", "Ticker", "ticker", "JPM");
        if (run_button("an-lev")) {
            fire_get("an-lev", "analysis/leverage/ratios", {{"ticker", ep("an-lev").get("ticker", "JPM")}});
        }
        render_result("an-lev");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-cf", "Cash Flow Analysis", "FCF, operating CF, CF-to-debt")) {
        input_text("an-cf", "Ticker", "ticker", "JPM");
        if (run_button("an-cf")) {
            fire_get("an-cf", "analysis/cashflow/analysis", {{"ticker", ep("an-cf").get("ticker", "JPM")}});
        }
        render_result("an-cf");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_val_qual() {
    if (begin_endpoint_card("an-val", "Valuation Ratios", "P/E, P/B, P/S, EV/EBITDA, PEG")) {
        input_text("an-val", "Ticker", "ticker", "GOOGL");
        if (run_button("an-val")) {
            fire_get("an-val", "analysis/valuation/ratios", {{"ticker", ep("an-val").get("ticker", "GOOGL")}});
        }
        render_result("an-val");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-qual", "Quality Score", "Piotroski F-Score, Altman Z-Score, Beneish M-Score")) {
        input_text("an-qual", "Ticker", "ticker", "GOOGL");
        if (run_button("an-qual")) {
            fire_get("an-qual", "analysis/quality/scores", {{"ticker", ep("an-qual").get("ticker", "GOOGL")}});
        }
        render_result("an-qual");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_industry() {
    if (begin_endpoint_card("an-peer", "Peer Comparison", "Compare ratios vs industry peers")) {
        input_text("an-peer", "Ticker", "ticker", "AAPL");
        input_select("an-peer", "Metric", "metric", {"pe", "pb", "roe", "margins", "growth"});
        if (run_button("an-peer")) {
            fire_get("an-peer", "analysis/industry/peer-comparison", {
                {"ticker", ep("an-peer").get("ticker", "AAPL")},
                {"metric", ep("an-peer").get("metric", "pe")}
            });
        }
        render_result("an-peer");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-sector", "Sector Analysis", "Sector-level aggregated statistics")) {
        input_select("an-sector", "Sector", "sector", {"Technology", "Healthcare", "Financials", "Energy", "Consumer", "Industrials"});
        if (run_button("an-sector")) {
            fire_get("an-sector", "analysis/industry/sector", {{"sector", ep("an-sector").get("sector", "Technology")}});
        }
        render_result("an-sector");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_dcf() {
    if (begin_endpoint_card("an-dcf", "DCF Valuation", "Discounted Cash Flow intrinsic value")) {
        input_text("an-dcf", "Ticker", "ticker", "AAPL");
        input_float("an-dcf", "WACC (%)", "wacc", "10");
        input_float("an-dcf", "Terminal Growth (%)", "tg", "2.5");
        input_int("an-dcf", "Projection Years", "years", "10");
        if (run_button("an-dcf")) {
            auto& s = ep("an-dcf");
            fire_post("an-dcf", "analysis/dcf/valuation", {
                {"ticker", s.get("ticker", "AAPL")}, {"wacc", s.getd("wacc") / 100.0},
                {"terminal_growth", s.getd("tg") / 100.0}, {"projection_years", s.geti("years")}
            });
        }
        render_result("an-dcf");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-ddm", "DDM", "Dividend Discount Model (Gordon Growth / Multi-stage)")) {
        input_text("an-ddm", "Ticker", "ticker", "JNJ");
        input_float("an-ddm", "Required Return (%)", "rr", "8");
        input_float("an-ddm", "Growth (%)", "g", "3");
        input_select("an-ddm", "Model", "model", {"gordon", "two_stage", "three_stage"});
        if (run_button("an-ddm")) {
            auto& s = ep("an-ddm");
            fire_post("an-ddm", "analysis/dcf/ddm", {
                {"ticker", s.get("ticker", "JNJ")}, {"required_return", s.getd("rr") / 100.0},
                {"growth_rate", s.getd("g") / 100.0}, {"model", s.get("model", "gordon")}
            });
        }
        render_result("an-ddm");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_analysis_models() {
    if (begin_endpoint_card("an-comps", "Comparable Companies", "Trading comps valuation")) {
        input_text("an-comps", "Ticker", "ticker", "META");
        input_select("an-comps", "Multiple", "multiple", {"ev_ebitda", "pe", "ps", "pb"});
        if (run_button("an-comps")) {
            fire_get("an-comps", "analysis/models/comps", {
                {"ticker", ep("an-comps").get("ticker", "META")},
                {"multiple", ep("an-comps").get("multiple", "ev_ebitda")}
            });
        }
        render_result("an-comps");
        end_endpoint_card();
    }
    if (begin_endpoint_card("an-residual", "Residual Income", "Residual income / EVA valuation model")) {
        input_text("an-residual", "Ticker", "ticker", "BRK-B");
        input_float("an-residual", "Cost of Equity (%)", "coe", "10");
        if (run_button("an-residual")) {
            auto& s = ep("an-residual");
            fire_post("an-residual", "analysis/models/residual-income", {
                {"ticker", s.get("ticker", "BRK-B")}, {"cost_of_equity", s.getd("coe") / 100.0}
            });
        }
        render_result("an-residual");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
