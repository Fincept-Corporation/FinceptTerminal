#include "ai_quant_lab_screen.h"
#include "ui/theme.h"
#include "core/logger.h"
#include <imgui.h>
#include <implot.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>

namespace fincept::ai_quant_lab {

// ============================================================================
// PANEL 10: Rolling Retraining — CYAN THEME
// ============================================================================
void AIQuantLabScreen::render_rolling_retraining() {
    using namespace theme::colors;
    section_header("ROLLING RETRAINING", panel_colors::CYAN);
    ImGui::TextColored(TEXT_DIM, "Scheduled model updates and retraining pipelines");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* frequencies[] = {"Hourly", "Daily", "Weekly"};

    config_input_text("Model Name", rr_model_name_, sizeof(rr_model_name_), 130);
    config_input_int("Window (days)", &rr_window_days_, 130);
    config_combo("Frequency", &rr_frequency_, frequencies, 3, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!rr_create_op_.loading.load()) {
        if (theme::AccentButton("Create Schedule")) {
            rr_create_op_.loading.store(true);
            json config = {
                {"model_name", rr_model_name_},
                {"training_window_days", rr_window_days_},
                {"frequency", frequencies[rr_frequency_]},
            };
            log_status("Creating retraining schedule...", panel_colors::CYAN);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_rolling_create_schedule(config).get();
                if (result.success) {
                    rr_create_op_.set_result(result.data);
                    log_status("Schedule created", SUCCESS);
                } else {
                    rr_create_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Creating schedule...");
    }

    ImGui::SameLine();
    if (theme::SecondaryButton("List Schedules")) {
        rr_list_op_.loading.store(true);
        std::thread([this]() {
            auto result = QuantLabApi::instance().qlib_rolling_list_schedules().get();
            if (result.success) rr_list_op_.set_result(result.data);
            else rr_list_op_.set_error(result.error);
        }).detach();
    }

    if (!rr_create_op_.error.empty()) theme::ErrorMessage(rr_create_op_.error.c_str());
    if (!rr_list_op_.error.empty()) theme::ErrorMessage(rr_list_op_.error.c_str());
}

// ============================================================================
// PANEL 11: Advanced Models
// ============================================================================
void AIQuantLabScreen::render_advanced_models() {
    using namespace theme::colors;
    section_header("ADVANCED MODELS", panel_colors::BLUE);
    ImGui::TextColored(TEXT_DIM, "LSTM, GRU, and Transformer neural network architectures");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* models[] = {"LSTM", "GRU", "Transformer"};
    config_combo("Architecture", &am_model_idx_, models, 3, 130);

    // Model details
    ImGui::Dummy(ImVec2(0, 8));
    const char* descs[] = {
        "Long Short-Term Memory — excels at capturing long-range temporal dependencies in sequential financial data",
        "Gated Recurrent Unit — lightweight alternative to LSTM with comparable performance and faster training",
        "Transformer — attention-based architecture that captures complex multi-variate relationships",
    };
    ImGui::TextColored(TEXT_SECONDARY, "%s", descs[am_model_idx_]);

    ImGui::Dummy(ImVec2(0, 8));

    if (!am_train_op_.loading.load()) {
        if (theme::AccentButton("Train Model")) {
            am_train_op_.loading.store(true);
            json config = {{"model_type", models[am_model_idx_]}};
            log_status(std::string("Training ") + models[am_model_idx_] + "...", panel_colors::BLUE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().qlib_advanced_train(config).get();
                if (result.success) {
                    am_train_op_.set_result(result.data);
                    log_status("Training complete", SUCCESS);
                } else {
                    am_train_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Training neural network...");
    }

    if (am_train_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("TRAINING RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(am_train_op_.mutex);
        auto& d = am_train_op_.result;
        double loss = d.value("final_loss", 0.0);
        int epochs = d.value("epochs", 0);
        double val_acc = d.value("val_accuracy", 0.0);
        char epochs_buf[16];
        std::snprintf(epochs_buf, sizeof(epochs_buf), "%d", epochs);
        metric_card("Final Loss", loss, "%.6f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("Epochs", epochs_buf, TEXT_PRIMARY);
        ImGui::SameLine();
        metric_card("Val Accuracy", val_acc * 100, "%.1f%%", val_acc > 0.6 ? MARKET_GREEN : WARNING);
    }

    if (!am_train_op_.error.empty()) theme::ErrorMessage(am_train_op_.error.c_str());
}

// ============================================================================
// PANEL 12: GluonTS — GLUON BLUE THEME
// ============================================================================
void AIQuantLabScreen::render_gluonts() {
    using namespace theme::colors;
    section_header("GLUONTS", panel_colors::GLUON_BLUE);
    ImGui::TextColored(TEXT_DIM, "Deep learning time series forecasting");
    ImGui::Dummy(ImVec2(0, 8));

    auto models = get_gluonts_models();
    // Build combo items
    std::vector<const char*> model_names;
    for (auto& m : models) model_names.push_back(m.label);

    config_combo("Model", &gts_model_idx_, model_names.data(), (int)model_names.size(), 130);
    config_input_text("Data Path (CSV)", gts_data_path_, sizeof(gts_data_path_), 130);
    config_input_int("Prediction Length", &gts_pred_length_, 130);

    static const char* freqs[] = {"H (Hourly)", "D (Daily)", "W (Weekly)", "M (Monthly)"};
    config_combo("Frequency", &gts_freq_idx_, freqs, 4, 130);
    config_input_int("Epochs", &gts_epochs_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!gts_op_.loading.load()) {
        if (theme::AccentButton("Run Forecast")) {
            gts_op_.loading.store(true);
            static const char* freq_codes[] = {"H", "D", "W", "M"};
            json config = {
                {"model", models[gts_model_idx_].name},
                {"data_path", gts_data_path_},
                {"prediction_length", gts_pred_length_},
                {"freq", freq_codes[gts_freq_idx_]},
                {"epochs", gts_epochs_},
            };
            log_status("Running GluonTS forecast...", panel_colors::GLUON_BLUE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().gluonts_forecast(config).get();
                if (result.success) {
                    gts_op_.set_result(result.data);
                    log_status("Forecast complete", SUCCESS);
                } else {
                    gts_op_.set_error(result.error);
                    log_status("Forecast failed", ERROR_RED);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Forecasting...");
    }

    // Forecast results
    if (gts_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("FORECAST RESULTS", panel_colors::GLUON_BLUE);

        std::lock_guard<std::mutex> lock(gts_op_.mutex);
        auto& d = gts_op_.result;

        // Metrics
        double mse = d.value("mse", 0.0);
        double mae = d.value("mae", 0.0);
        double mape = d.value("mape", 0.0);
        metric_card("MSE", mse, "%.4f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("MAE", mae, "%.4f", TEXT_SECONDARY);
        ImGui::SameLine();
        metric_card("MAPE", mape * 100, "%.2f%%", mape < 0.1 ? MARKET_GREEN : WARNING);

        // Forecast plot with ImPlot
        if (d.contains("forecast") && d["forecast"].is_array()) {
            ImGui::Dummy(ImVec2(0, 8));
            auto& fc = d["forecast"];
            std::vector<double> values;
            for (auto& v : fc) values.push_back(v.get<double>());

            if (ImPlot::BeginPlot("##forecast_plot", ImVec2(-1, 250))) {
                ImPlot::SetupAxes("Time", "Value");
                ImPlot::PushStyleColor(ImPlotCol_Line, panel_colors::GLUON_BLUE);
                std::vector<double> xs(values.size());
                for (size_t i = 0; i < xs.size(); i++) xs[i] = (double)i;
                ImPlot::PlotLine("Forecast", xs.data(), values.data(), (int)values.size());
                ImPlot::PopStyleColor();
                ImPlot::EndPlot();
            }
        }
    }

    if (!gts_op_.error.empty()) theme::ErrorMessage(gts_op_.error.c_str());
}

// ============================================================================
// PANEL 13: GS Quant — GS PURPLE THEME
// ============================================================================
void AIQuantLabScreen::render_gs_quant() {
    using namespace theme::colors;
    section_header("GS QUANT", panel_colors::GS_PURPLE);
    ImGui::TextColored(TEXT_DIM, "Goldman Sachs-style quantitative analytics");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* modes[] = {
        "Risk Metrics", "Portfolio Analytics", "Greeks",
        "VaR", "Stress Testing", "Backtesting", "Statistics"
    };
    config_combo("Analysis Mode", &gsq_mode_idx_, modes, 7, 130);
    config_input_text("Symbols", gsq_symbols_, sizeof(gsq_symbols_), 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!gsq_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            gsq_op_.loading.store(true);
            json config = {
                {"mode", modes[gsq_mode_idx_]},
                {"symbols", gsq_symbols_},
            };
            log_status("Running GS Quant analysis...", panel_colors::GS_PURPLE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().gs_quant_analyze(config).get();
                if (result.success) {
                    gsq_op_.set_result(result.data);
                    log_status("GS Quant analysis complete", SUCCESS);
                } else {
                    gsq_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    // Results
    if (gsq_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("ANALYSIS RESULTS", panel_colors::GS_PURPLE);

        std::lock_guard<std::mutex> lock(gsq_op_.mutex);
        // Render JSON results in a structured way
        if (gsq_op_.result.is_object()) {
            for (auto& [key, val] : gsq_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.4f", val.get<double>());
                } else if (val.is_string()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_SECONDARY, "%s", val.get<std::string>().c_str());
                }
            }
        }
    }

    if (!gsq_op_.error.empty()) theme::ErrorMessage(gsq_op_.error.c_str());
}

// ============================================================================
// PANEL 14: Statsmodels — TEAL THEME
// ============================================================================
void AIQuantLabScreen::render_statsmodels() {
    using namespace theme::colors;
    section_header("STATSMODELS", panel_colors::TEAL);
    ImGui::TextColored(TEXT_DIM, "Statistical time series analysis (ARIMA, SARIMAX, decomposition)");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* analyses[] = {
        "ARIMA", "SARIMAX", "Exp Smoothing", "STL Decompose",
        "ADF Test", "KPSS Test", "ACF", "PACF"
    };
    static const char* data_sources[] = {"Manual Data", "Symbol (Yahoo Finance)"};

    config_combo("Analysis", &sm_analysis_idx_, analyses, 8, 130);
    config_combo("Data Source", &sm_data_source_, data_sources, 2, 130);

    if (sm_data_source_ == 1) {
        config_input_text("Symbol", sm_symbol_, sizeof(sm_symbol_), 130);
    } else {
        ImGui::TextColored(TEXT_DIM, "Data (CSV):");
        ImGui::InputTextMultiline("##sm_data", sm_data_, sizeof(sm_data_), ImVec2(-1, 80));
    }

    // ARIMA params
    if (sm_analysis_idx_ <= 1) {
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextColored(TEXT_DIM, "ARIMA Order:");
        ImGui::SameLine(130);
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##p", &sm_arima_p_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##d", &sm_arima_d_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("##q", &sm_arima_q_);
    }

    ImGui::Dummy(ImVec2(0, 8));

    if (!sm_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            sm_op_.loading.store(true);
            json config = {
                {"analysis_type", analyses[sm_analysis_idx_]},
                {"data_source", sm_data_source_ == 1 ? "symbol" : "manual"},
                {"symbol", sm_symbol_},
                {"data", sm_data_},
                {"p", sm_arima_p_},
                {"d", sm_arima_d_},
                {"q", sm_arima_q_},
            };
            log_status("Running statsmodels analysis...", panel_colors::TEAL);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().statsmodels_analyze(config).get();
                if (result.success) {
                    sm_op_.set_result(result.data);
                    log_status("Analysis complete", SUCCESS);
                } else {
                    sm_op_.set_error(result.error);
                }
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    // Results
    if (sm_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 12));
        section_header("RESULTS", panel_colors::TEAL);
        std::lock_guard<std::mutex> lock(sm_op_.mutex);
        if (sm_op_.result.is_object()) {
            for (auto& [key, val] : sm_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.6f", val.get<double>());
                } else if (val.is_string()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_SECONDARY, "%s", val.get<std::string>().c_str());
                }
            }
        }
    }

    if (!sm_op_.error.empty()) theme::ErrorMessage(sm_op_.error.c_str());
}

// ============================================================================
// PANEL 15: Functime
// ============================================================================
void AIQuantLabScreen::render_functime() {
    using namespace theme::colors;
    section_header("FUNCTIME", ACCENT);
    ImGui::TextColored(TEXT_DIM, "Fast time series forecasting with Functime library");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* models[] = {
        "Naive", "SeasonalNaive", "LinearForecaster",
        "LightGBMForecaster", "AutoARIMA", "AutoETS",
    };
    config_input_text("Symbol", ft_symbol_, sizeof(ft_symbol_), 130);
    config_combo("Model", &ft_model_idx_, models, 6, 130);
    config_input_int("Horizon", &ft_horizon_, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!ft_op_.loading.load()) {
        if (theme::AccentButton("Forecast")) {
            ft_op_.loading.store(true);
            json config = {
                {"symbol", ft_symbol_},
                {"model", models[ft_model_idx_]},
                {"horizon", ft_horizon_},
            };
            log_status("Running functime forecast...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().functime_forecast(config).get();
                if (result.success) ft_op_.set_result(result.data);
                else ft_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Forecasting...");
    }

    if (ft_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("FORECAST", SUCCESS);
        std::lock_guard<std::mutex> lock(ft_op_.mutex);
        auto& d = ft_op_.result;

        // Plot forecast if available
        if (d.contains("forecast") && d["forecast"].is_array()) {
            std::vector<double> values;
            for (auto& v : d["forecast"]) values.push_back(v.get<double>());

            if (ImPlot::BeginPlot("##ft_forecast", ImVec2(-1, 200))) {
                ImPlot::SetupAxes("Step", "Value");
                std::vector<double> xs(values.size());
                for (size_t i = 0; i < xs.size(); i++) xs[i] = (double)i;
                ImPlot::PlotLine("Forecast", xs.data(), values.data(), (int)values.size());
                ImPlot::EndPlot();
            }
        }
    }

    if (!ft_op_.error.empty()) theme::ErrorMessage(ft_op_.error.c_str());
}

// ============================================================================
// PANEL 16: Fortitudo
// ============================================================================
void AIQuantLabScreen::render_fortitudo() {
    using namespace theme::colors;
    section_header("FORTITUDO", ACCENT);
    ImGui::TextColored(TEXT_DIM, "Entropy pooling, portfolio optimization, and risk analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* modes[] = {"Entropy Pooling", "Optimization", "Option Pricing", "Portfolio Risk"};
    config_combo("Mode", &fort_mode_idx_, modes, 4, 130);
    config_input_text("Symbols", fort_symbols_, sizeof(fort_symbols_), 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!fort_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            fort_op_.loading.store(true);
            json config = {
                {"mode", modes[fort_mode_idx_]},
                {"symbols", fort_symbols_},
            };
            log_status("Running Fortitudo analysis...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().fortitudo_analyze(config).get();
                if (result.success) fort_op_.set_result(result.data);
                else fort_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    if (fort_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(fort_op_.mutex);
        if (fort_op_.result.is_object()) {
            for (auto& [key, val] : fort_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.6f", val.get<double>());
                }
            }
        }
    }

    if (!fort_op_.error.empty()) theme::ErrorMessage(fort_op_.error.c_str());
}

// ============================================================================
// PANEL 17: CFA Quant
// ============================================================================
void AIQuantLabScreen::render_cfa_quant() {
    using namespace theme::colors;
    section_header("CFA QUANT", ACCENT);
    ImGui::TextColored(TEXT_DIM, "CFA-level quantitative financial analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* analyses[] = {
        "Equity Valuation", "Fixed Income", "Derivatives",
        "Portfolio Management", "Corporate Finance", "Economics",
    };
    config_input_text("Symbol", cfa_symbol_, sizeof(cfa_symbol_), 130);
    config_combo("Analysis", &cfa_analysis_idx_, analyses, 6, 130);

    ImGui::Dummy(ImVec2(0, 8));

    if (!cfa_op_.loading.load()) {
        if (theme::AccentButton("Run Analysis")) {
            cfa_op_.loading.store(true);
            json config = {
                {"symbol", cfa_symbol_},
                {"analysis_type", analyses[cfa_analysis_idx_]},
            };
            log_status("Running CFA analysis...", ACCENT);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().cfa_quant_analyze(config).get();
                if (result.success) cfa_op_.set_result(result.data);
                else cfa_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Analyzing...");
    }

    if (cfa_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("RESULTS", SUCCESS);
        std::lock_guard<std::mutex> lock(cfa_op_.mutex);
        if (cfa_op_.result.is_object()) {
            for (auto& [key, val] : cfa_op_.result.items()) {
                if (val.is_number()) {
                    ImGui::TextColored(TEXT_DIM, "%s:", key.c_str());
                    ImGui::SameLine(200);
                    ImGui::TextColored(TEXT_PRIMARY, "%.4f", val.get<double>());
                }
            }
        }
    }

    if (!cfa_op_.error.empty()) theme::ErrorMessage(cfa_op_.error.c_str());
}

// ============================================================================
// PANEL 18: Pattern Intelligence — VisionQuant
// ============================================================================
void AIQuantLabScreen::render_pattern_intelligence() {
    using namespace theme::colors;
    section_header("PATTERN INTELLIGENCE", panel_colors::PURPLE);
    ImGui::TextColored(TEXT_DIM, "VisionQuant K-line pattern recognition and analysis");
    ImGui::Dummy(ImVec2(0, 8));

    static const char* patterns[] = {
        "Doji", "Hammer", "Engulfing", "Morning Star", "Evening Star",
        "Three White Soldiers", "Three Black Crows", "Harami",
        "Piercing Line", "Dark Cloud Cover",
    };
    config_input_text("Symbol", pi_symbol_, sizeof(pi_symbol_), 130);
    config_combo("Pattern", &pi_pattern_idx_, patterns, 10, 130);

    ImGui::Dummy(ImVec2(0, 8));

    // Three action buttons
    if (!pi_search_op_.loading.load()) {
        if (theme::AccentButton("Search Patterns")) {
            pi_search_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            log_status("Searching patterns...", panel_colors::PURPLE);
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_search(config).get();
                if (result.success) pi_search_op_.set_result(result.data);
                else pi_search_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Searching...");
    }

    ImGui::SameLine();

    if (!pi_score_op_.loading.load()) {
        if (theme::SecondaryButton("Score Pattern")) {
            pi_score_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_score(config).get();
                if (result.success) pi_score_op_.set_result(result.data);
                else pi_score_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Scoring...");
    }

    ImGui::SameLine();

    if (!pi_backtest_op_.loading.load()) {
        if (theme::SecondaryButton("Backtest Pattern")) {
            pi_backtest_op_.loading.store(true);
            json config = {
                {"symbol", pi_symbol_},
                {"pattern", patterns[pi_pattern_idx_]},
            };
            std::thread([this, config]() {
                auto result = QuantLabApi::instance().visionquant_backtest(config).get();
                if (result.success) pi_backtest_op_.set_result(result.data);
                else pi_backtest_op_.set_error(result.error);
            }).detach();
        }
    } else {
        theme::LoadingSpinner("Backtesting...");
    }

    // Search results
    if (pi_search_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN MATCHES", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_search_op_.mutex);
        if (pi_search_op_.result.contains("matches") && pi_search_op_.result["matches"].is_array()) {
            for (auto& m : pi_search_op_.result["matches"]) {
                ImGui::TextColored(TEXT_PRIMARY, "%s at %s",
                    m.value("pattern", "").c_str(),
                    m.value("date", "").c_str());
                ImGui::SameLine(300);
                double conf = m.value("confidence", 0.0);
                ImGui::TextColored(conf > 0.8 ? MARKET_GREEN : WARNING, "%.1f%%", conf * 100);
            }
        }
    }

    // Score results
    if (pi_score_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN SCORE", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_score_op_.mutex);
        auto& d = pi_score_op_.result;
        double score = d.value("score", 0.0);
        double reliability = d.value("reliability", 0.0);
        metric_card("Score", score, "%.2f", score > 0.7 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Reliability", reliability * 100, "%.1f%%", reliability > 0.6 ? MARKET_GREEN : WARNING);
    }

    // Backtest results
    if (pi_backtest_op_.has_result) {
        ImGui::Dummy(ImVec2(0, 8));
        section_header("PATTERN BACKTEST", panel_colors::PURPLE);
        std::lock_guard<std::mutex> lock(pi_backtest_op_.mutex);
        auto& d = pi_backtest_op_.result;
        double win_rate = d.value("win_rate", 0.0);
        double avg_return = d.value("avg_return", 0.0);
        int occurrences = d.value("occurrences", 0);
        metric_card("Win Rate", win_rate * 100, "%.1f%%", win_rate > 0.5 ? MARKET_GREEN : WARNING);
        ImGui::SameLine();
        metric_card("Avg Return", avg_return * 100, "%.2f%%", avg_return > 0 ? MARKET_GREEN : MARKET_RED);
        ImGui::SameLine();
        char occ_buf[16];
        std::snprintf(occ_buf, sizeof(occ_buf), "%d", occurrences);
        metric_card("Occurrences", occ_buf, TEXT_PRIMARY);
    }

    if (!pi_search_op_.error.empty()) theme::ErrorMessage(pi_search_op_.error.c_str());
    if (!pi_score_op_.error.empty()) theme::ErrorMessage(pi_score_op_.error.c_str());
    if (!pi_backtest_op_.error.empty()) theme::ErrorMessage(pi_backtest_op_.error.c_str());
}

} // namespace fincept::ai_quant_lab
