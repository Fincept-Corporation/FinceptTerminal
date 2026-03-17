// QuantLib Panels — Volatility (Surface, SABR, LocalVol)
//                   Models (ShortRate, HullWhite, Heston, JumpDiff, VolModels)
//                   Stochastic (Processes, Exact, Simulation, Sampling, Theory)
#include "quantlib_screen.h"
#include <imgui.h>

namespace fincept::quantlib {

// ── Volatility ──────────────────────────────────────────────────────────────

void QuantLibScreen::render_surface() {
    if (begin_endpoint_card("vol-flat", "Flat Surface", "Constant volatility surface")) {
        input_float("vol-flat", "Volatility", "vol", "0.20");
        input_float("vol-flat", "Query Expiry", "expiry", "1.0");
        input_float("vol-flat", "Query Strike", "strike", "100");
        if (run_button("vol-flat")) {
            auto& s = ep("vol-flat");
            fire_post("vol-flat", "volatility/surface/flat", {
                {"volatility", s.getd("vol")}, {"query_expiry", s.getd("expiry")}, {"query_strike", s.getd("strike")}
            });
        }
        render_result("vol-flat");
        end_endpoint_card();
    }
    if (begin_endpoint_card("vol-term", "Term Structure", "Volatility term structure interpolation")) {
        input_array("vol-term", "Expiries", "expiries", "0.25,0.5,1.0,2.0");
        input_array("vol-term", "Volatilities", "vols", "0.25,0.22,0.20,0.19");
        input_float("vol-term", "Query Expiry", "query", "0.75");
        if (run_button("vol-term")) {
            fire_post("vol-term", "volatility/surface/term-structure", {
                {"expiries", json::array({0.25,0.5,1.0,2.0})},
                {"volatilities", json::array({0.25,0.22,0.20,0.19})},
                {"query_expiry", ep("vol-term").getd("query", 0.75)}
            });
        }
        render_result("vol-term");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sabr() {
    if (begin_endpoint_card("sabr-iv", "SABR Implied Vol", "SABR model implied volatility")) {
        input_float("sabr-iv", "Forward", "fwd", "100");
        input_float("sabr-iv", "Strike", "strike", "100");
        input_float("sabr-iv", "Expiry", "expiry", "1.0");
        input_float("sabr-iv", "Alpha", "alpha", "0.3");
        input_float("sabr-iv", "Beta", "beta", "0.5");
        input_float("sabr-iv", "Rho", "rho", "-0.3");
        input_float("sabr-iv", "Nu", "nu", "0.4");
        if (run_button("sabr-iv")) {
            auto& s = ep("sabr-iv");
            fire_post("sabr-iv", "volatility/sabr/implied-vol", {
                {"forward", s.getd("fwd")}, {"strike", s.getd("strike")}, {"expiry", s.getd("expiry")},
                {"alpha", s.getd("alpha")}, {"beta", s.getd("beta")}, {"rho", s.getd("rho")}, {"nu", s.getd("nu")}
            });
        }
        render_result("sabr-iv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sabr-cal", "SABR Calibrate", "Calibrate SABR to market vols")) {
        input_float("sabr-cal", "Forward", "fwd", "100");
        input_float("sabr-cal", "Expiry", "expiry", "1.0");
        input_array("sabr-cal", "Strikes", "strikes", "90,95,100,105,110");
        input_array("sabr-cal", "Market Vols", "vols", "0.25,0.22,0.20,0.21,0.24");
        if (run_button("sabr-cal")) {
            fire_post("sabr-cal", "volatility/sabr/calibrate", {
                {"forward", ep("sabr-cal").getd("fwd")}, {"expiry", ep("sabr-cal").getd("expiry")},
                {"strikes", json::array({90,95,100,105,110})},
                {"market_vols", json::array({0.25,0.22,0.20,0.21,0.24})}
            });
        }
        render_result("sabr-cal");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_localvol() {
    if (begin_endpoint_card("lv-const", "Constant Local Vol", "Constant local volatility")) {
        input_float("lv-const", "Volatility", "vol", "0.20");
        input_float("lv-const", "Spot", "spot", "100");
        input_float("lv-const", "Time", "time", "1.0");
        if (run_button("lv-const")) {
            auto& s = ep("lv-const");
            fire_post("lv-const", "volatility/local-vol/constant", {
                {"volatility", s.getd("vol")}, {"spot", s.getd("spot")}, {"time", s.getd("time")}
            });
        }
        render_result("lv-const");
        end_endpoint_card();
    }
    if (begin_endpoint_card("lv-impl", "Implied to Local", "Convert implied vol to local vol via Dupire")) {
        input_float("lv-impl", "Spot", "spot", "100"); input_float("lv-impl", "Rate", "rate", "0.05");
        input_float("lv-impl", "Strike", "strike", "100"); input_float("lv-impl", "Expiry", "expiry", "1.0");
        input_float("lv-impl", "Implied Vol", "iv", "0.20");
        if (run_button("lv-impl")) {
            auto& s = ep("lv-impl");
            fire_post("lv-impl", "volatility/local-vol/implied-to-local", {
                {"spot", s.getd("spot")}, {"rate", s.getd("rate")}, {"strike", s.getd("strike")},
                {"expiry", s.getd("expiry")}, {"implied_vol", s.getd("iv")}
            });
        }
        render_result("lv-impl");
        end_endpoint_card();
    }
}

// ── Models ──────────────────────────────────────────────────────────────────

void QuantLibScreen::render_shortrate() {
    if (begin_endpoint_card("sr-bond", "Bond Price", "Short rate model bond pricing")) {
        input_select("sr-bond", "Model", "model", {"vasicek", "cir", "hw"});
        input_float("sr-bond", "Kappa", "kappa", "0.5"); input_float("sr-bond", "Theta", "theta", "0.05");
        input_float("sr-bond", "Sigma", "sigma", "0.01"); input_float("sr-bond", "r0", "r0", "0.03");
        if (run_button("sr-bond")) {
            auto& s = ep("sr-bond");
            fire_post("sr-bond", "models/short-rate/bond-price", {
                {"model", s.get("model", "vasicek")}, {"kappa", s.getd("kappa")},
                {"theta", s.getd("theta")}, {"sigma", s.getd("sigma")}, {"r0", s.getd("r0")}
            });
        }
        render_result("sr-bond");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sr-mc", "Monte Carlo", "Short rate Monte Carlo simulation")) {
        input_select("sr-mc", "Model", "model", {"vasicek", "cir", "hw"});
        input_float("sr-mc", "r0", "r0", "0.03"); input_float("sr-mc", "T", "T", "1.0");
        input_int("sr-mc", "Steps", "steps", "100"); input_int("sr-mc", "Paths", "paths", "1000");
        if (run_button("sr-mc")) {
            auto& s = ep("sr-mc");
            fire_post("sr-mc", "models/short-rate/monte-carlo", {
                {"model", s.get("model")}, {"r0", s.getd("r0")}, {"T", s.getd("T")},
                {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("sr-mc");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_hullwhite() {
    if (begin_endpoint_card("hw-cal", "Hull-White Calibrate", "Calibrate Hull-White model to market")) {
        input_float("hw-cal", "Kappa", "kappa", "0.1"); input_float("hw-cal", "Sigma", "sigma", "0.01");
        input_float("hw-cal", "r0", "r0", "0.03");
        if (run_button("hw-cal")) {
            auto& s = ep("hw-cal");
            fire_post("hw-cal", "models/hull-white/calibrate", {
                {"kappa", s.getd("kappa")}, {"sigma", s.getd("sigma")}, {"r0", s.getd("r0")}
            });
        }
        render_result("hw-cal");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_heston() {
    if (begin_endpoint_card("hes-price", "Heston Price", "Heston stochastic vol option pricing")) {
        input_float("hes-price", "S0", "S0", "100"); input_float("hes-price", "v0", "v0", "0.04");
        input_float("hes-price", "r", "r", "0.05"); input_float("hes-price", "Kappa", "kappa", "2.0");
        input_float("hes-price", "Theta", "theta", "0.04"); input_float("hes-price", "Sigma_v", "sigma_v", "0.3");
        input_float("hes-price", "Rho", "rho", "-0.7"); input_float("hes-price", "Strike", "strike", "100");
        input_float("hes-price", "T", "T", "1.0");
        if (run_button("hes-price")) {
            auto& s = ep("hes-price");
            fire_post("hes-price", "models/heston/price", {
                {"S0", s.getd("S0")}, {"v0", s.getd("v0")}, {"r", s.getd("r")},
                {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")}, {"sigma_v", s.getd("sigma_v")},
                {"rho", s.getd("rho")}, {"strike", s.getd("strike")}, {"T", s.getd("T")}
            });
        }
        render_result("hes-price");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_jumpdiff() {
    if (begin_endpoint_card("jd-merton", "Merton Jump-Diffusion", "Merton model option pricing")) {
        input_float("jd-merton", "S", "S", "100"); input_float("jd-merton", "K", "K", "100");
        input_float("jd-merton", "T", "T", "1.0"); input_float("jd-merton", "r", "r", "0.05");
        input_float("jd-merton", "Sigma", "sigma", "0.2"); input_float("jd-merton", "Lambda", "lambda", "0.1");
        input_float("jd-merton", "Jump Mean", "mu_j", "-0.1"); input_float("jd-merton", "Jump Std", "sigma_j", "0.3");
        if (run_button("jd-merton")) {
            auto& s = ep("jd-merton");
            fire_post("jd-merton", "models/merton/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"lambda_jump", s.getd("lambda")},
                {"mu_jump", s.getd("mu_j")}, {"sigma_jump", s.getd("sigma_j")}
            });
        }
        render_result("jd-merton");
        end_endpoint_card();
    }
    if (begin_endpoint_card("jd-kou", "Kou Double Exponential", "Kou model option pricing")) {
        input_float("jd-kou", "S", "S", "100"); input_float("jd-kou", "K", "K", "100");
        input_float("jd-kou", "T", "T", "1.0"); input_float("jd-kou", "r", "r", "0.05");
        input_float("jd-kou", "Sigma", "sigma", "0.2"); input_float("jd-kou", "Lambda", "lambda", "0.1");
        input_float("jd-kou", "p (up prob)", "p", "0.4");
        input_float("jd-kou", "Eta1", "eta1", "10"); input_float("jd-kou", "Eta2", "eta2", "5");
        if (run_button("jd-kou")) {
            auto& s = ep("jd-kou");
            fire_post("jd-kou", "models/kou/price", {
                {"S", s.getd("S")}, {"K", s.getd("K")}, {"T", s.getd("T")}, {"r", s.getd("r")},
                {"sigma", s.getd("sigma")}, {"lambda_jump", s.getd("lambda")},
                {"p", s.getd("p")}, {"eta1", s.getd("eta1")}, {"eta2", s.getd("eta2")}
            });
        }
        render_result("jd-kou");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_volmodels() {
    if (begin_endpoint_card("dupire-price", "Dupire Price", "Local vol model pricing")) {
        input_float("dupire-price", "Spot", "spot", "100"); input_float("dupire-price", "r", "r", "0.05");
        input_float("dupire-price", "Sigma", "sigma", "0.2");
        input_float("dupire-price", "Strike", "strike", "100"); input_float("dupire-price", "T", "T", "1.0");
        if (run_button("dupire-price")) {
            auto& s = ep("dupire-price");
            fire_post("dupire-price", "models/dupire/price", {
                {"spot", s.getd("spot")}, {"r", s.getd("r")}, {"sigma", s.getd("sigma")},
                {"strike", s.getd("strike")}, {"T", s.getd("T")}
            });
        }
        render_result("dupire-price");
        end_endpoint_card();
    }
    if (begin_endpoint_card("svi-cal", "SVI Calibrate", "SVI volatility smile calibration")) {
        input_float("svi-cal", "Forward", "fwd", "100"); input_float("svi-cal", "T", "T", "1.0");
        input_array("svi-cal", "Strikes", "strikes", "90,95,100,105,110");
        input_array("svi-cal", "Market Vols", "vols", "0.25,0.22,0.20,0.21,0.24");
        if (run_button("svi-cal")) {
            fire_post("svi-cal", "models/svi/calibrate", {
                {"forward", ep("svi-cal").getd("fwd")}, {"T", ep("svi-cal").getd("T")},
                {"strikes", json::array({90,95,100,105,110})},
                {"market_vols", json::array({0.25,0.22,0.20,0.21,0.24})}
            });
        }
        render_result("svi-cal");
        end_endpoint_card();
    }
}

// ── Stochastic ──────────────────────────────────────────────────────────────

void QuantLibScreen::render_processes() {
    if (begin_endpoint_card("gbm-sim", "GBM Simulate", "Geometric Brownian Motion simulation")) {
        input_float("gbm-sim", "S0", "S0", "100"); input_float("gbm-sim", "Mu", "mu", "0.1");
        input_float("gbm-sim", "Sigma", "sigma", "0.2"); input_float("gbm-sim", "T", "T", "1.0");
        input_int("gbm-sim", "Steps", "steps", "252"); input_int("gbm-sim", "Paths", "paths", "5");
        if (run_button("gbm-sim")) {
            auto& s = ep("gbm-sim");
            fire_post("gbm-sim", "stochastic/gbm/simulate", {
                {"S0", s.getd("S0")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")},
                {"T", s.getd("T")}, {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("gbm-sim");
        end_endpoint_card();
    }
    if (begin_endpoint_card("ou-sim", "OU Simulate", "Ornstein-Uhlenbeck mean-reverting process")) {
        input_float("ou-sim", "X0", "X0", "0.05"); input_float("ou-sim", "Kappa", "kappa", "5.0");
        input_float("ou-sim", "Theta", "theta", "0.05"); input_float("ou-sim", "Sigma", "sigma", "0.01");
        input_float("ou-sim", "T", "T", "1.0");
        if (run_button("ou-sim")) {
            auto& s = ep("ou-sim");
            fire_post("ou-sim", "stochastic/ou/simulate", {
                {"X0", s.getd("X0")}, {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")},
                {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("ou-sim");
        end_endpoint_card();
    }
    if (begin_endpoint_card("cir-sim", "CIR Simulate", "Cox-Ingersoll-Ross interest rate model")) {
        input_float("cir-sim", "r0", "r0", "0.05"); input_float("cir-sim", "Kappa", "kappa", "0.5");
        input_float("cir-sim", "Theta", "theta", "0.05"); input_float("cir-sim", "Sigma", "sigma", "0.1");
        input_float("cir-sim", "T", "T", "5.0");
        if (run_button("cir-sim")) {
            auto& s = ep("cir-sim");
            fire_post("cir-sim", "stochastic/cir/simulate", {
                {"r0", s.getd("r0")}, {"kappa", s.getd("kappa")}, {"theta", s.getd("theta")},
                {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("cir-sim");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_exact() {
    if (begin_endpoint_card("exact-gbm", "Exact GBM", "Exact analytical GBM solution")) {
        input_float("exact-gbm", "S0", "S0", "100"); input_float("exact-gbm", "Mu", "mu", "0.1");
        input_float("exact-gbm", "Sigma", "sigma", "0.2"); input_float("exact-gbm", "T", "T", "1.0");
        if (run_button("exact-gbm")) {
            auto& s = ep("exact-gbm");
            fire_post("exact-gbm", "stochastic/exact/gbm", {
                {"S0", s.getd("S0")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}, {"T", s.getd("T")}
            });
        }
        render_result("exact-gbm");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_simulation() {
    if (begin_endpoint_card("sim-euler", "Euler-Maruyama", "SDE numerical integration")) {
        input_float("sim-euler", "x0", "x0", "1.0"); input_float("sim-euler", "T", "T", "1.0");
        input_float("sim-euler", "Mu", "mu", "0.1"); input_float("sim-euler", "Sigma", "sigma", "0.2");
        input_int("sim-euler", "Steps", "steps", "1000"); input_int("sim-euler", "Paths", "paths", "10");
        if (run_button("sim-euler")) {
            auto& s = ep("sim-euler");
            fire_post("sim-euler", "stochastic/simulation/euler-maruyama", {
                {"x0", s.getd("x0")}, {"T", s.getd("T")}, {"mu", s.getd("mu")},
                {"sigma", s.getd("sigma")}, {"n_steps", s.geti("steps")}, {"n_paths", s.geti("paths")}
            });
        }
        render_result("sim-euler");
        end_endpoint_card();
    }
    if (begin_endpoint_card("sim-milstein", "Milstein", "Higher-order SDE integration")) {
        input_float("sim-milstein", "x0", "x0", "1.0"); input_float("sim-milstein", "T", "T", "1.0");
        input_float("sim-milstein", "Mu", "mu", "0.1"); input_float("sim-milstein", "Sigma", "sigma", "0.2");
        if (run_button("sim-milstein")) {
            auto& s = ep("sim-milstein");
            fire_post("sim-milstein", "stochastic/simulation/milstein", {
                {"x0", s.getd("x0")}, {"T", s.getd("T")}, {"mu", s.getd("mu")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("sim-milstein");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_sampling() {
    if (begin_endpoint_card("samp-sobol", "Sobol Sequence", "Quasi-random low-discrepancy sequence")) {
        input_int("samp-sobol", "Dimensions", "dim", "2"); input_int("samp-sobol", "N Points", "n", "100");
        if (run_button("samp-sobol")) {
            auto& s = ep("samp-sobol");
            fire_post("samp-sobol", "stochastic/sampling/sobol", {{"dim", s.geti("dim")}, {"n", s.geti("n")}});
        }
        render_result("samp-sobol");
        end_endpoint_card();
    }
    if (begin_endpoint_card("samp-anti", "Antithetic Sampling", "Variance reduction via antithetic variates")) {
        input_int("samp-anti", "N", "n", "1000");
        if (run_button("samp-anti")) {
            fire_post("samp-anti", "stochastic/sampling/antithetic", {{"n", ep("samp-anti").geti("n")}});
        }
        render_result("samp-anti");
        end_endpoint_card();
    }
}

void QuantLibScreen::render_theory() {
    if (begin_endpoint_card("th-qv", "Quadratic Variation", "Compute quadratic variation of a path")) {
        input_array("th-qv", "Path", "path", "1.0,1.1,1.05,1.15,1.2");
        input_array("th-qv", "Times", "times", "0,0.25,0.5,0.75,1.0");
        if (run_button("th-qv")) {
            fire_post("th-qv", "stochastic/theory/quadratic-variation", {
                {"path", json::array({1.0,1.1,1.05,1.15,1.2})},
                {"times", json::array({0,0.25,0.5,0.75,1.0})}
            });
        }
        render_result("th-qv");
        end_endpoint_card();
    }
    if (begin_endpoint_card("th-rnd", "Risk-Neutral Drift", "Girsanov risk-neutral drift")) {
        input_float("th-rnd", "Mu", "mu", "0.1"); input_float("th-rnd", "r", "r", "0.05");
        input_float("th-rnd", "Sigma", "sigma", "0.2");
        if (run_button("th-rnd")) {
            auto& s = ep("th-rnd");
            fire_post("th-rnd", "stochastic/theory/girsanov/risk-neutral-drift", {
                {"mu", s.getd("mu")}, {"r", s.getd("r")}, {"sigma", s.getd("sigma")}
            });
        }
        render_result("th-rnd");
        end_endpoint_card();
    }
}

} // namespace fincept::quantlib
