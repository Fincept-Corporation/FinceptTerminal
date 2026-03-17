// Alternative Investments — CFA-Level analytics for 10 asset categories
// Port of AlternativeInvestmentsTab.tsx — sidebar + form + result panels
// Calls Python CLI: Analytics/alternateInvestment/cli.py

#include "alt_investments_screen.h"
#include "ui/yoga_helpers.h"
#include "ui/theme.h"
#include "python/python_runner.h"
#include "core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <chrono>
#include <map>

namespace fincept::alt_investments {

using namespace theme::colors;

// ============================================================================
// Static category catalog
// ============================================================================

static std::vector<Category> build_categories() {
    return {
        {"bonds", "Bonds & Fixed Income", "BONDS",
         "High-yield, EM, convertible, preferred",
         "Fixed Income",
         {1.0f, 0.53f, 0.0f, 1.0f}, 4,
         {{"high-yield", "High-Yield Bonds", "Below investment-grade corporate bonds"},
          {"em-bonds", "Emerging Market Bonds", "Government bonds from developing countries"},
          {"convertible-bonds", "Convertible Bonds", "Bonds convertible to company stock"},
          {"preferred-stocks", "Preferred Stocks", "Hybrid equity with fixed dividends"}}},

        {"real-estate", "Real Estate", "REALESTATE",
         "Direct property, REITs",
         "Real Assets",
         {0.0f, 0.84f, 0.44f, 1.0f}, 2,
         {{"real-estate", "Direct Real Estate", "Physical property investment analysis"},
          {"reit", "REIT Analysis", "Real Estate Investment Trust valuation"}}},

        {"hedge-funds", "Hedge Funds", "HEDGE",
         "Long/short, managed futures, market neutral",
         "Alternatives",
         {0.0f, 0.90f, 1.0f, 1.0f}, 3,
         {{"hedge-funds", "Hedge Fund", "Multi-strategy hedge fund analysis"},
          {"managed-futures", "Managed Futures", "CTA and systematic strategies"},
          {"market-neutral", "Market Neutral", "Beta-zero strategy analysis"}}},

        {"commodities", "Commodities", "COMMODITIES",
         "Precious metals, natural resources",
         "Real Assets",
         {1.0f, 0.84f, 0.0f, 1.0f}, 2,
         {{"precious-metals", "Precious Metals", "Gold, silver, platinum analysis"},
          {"natural-resources", "Natural Resources", "Timberland, farmland, energy"}}},

        {"private-capital", "Private Capital", "PE/VC",
         "Private equity, venture capital",
         "Alternatives",
         {0.62f, 0.31f, 0.87f, 1.0f}, 1,
         {{"private-equity", "Private Equity", "PE/VC fund performance analysis"}}},

        {"annuities", "Annuities", "ANNUITIES",
         "Fixed, variable, equity-indexed, inflation",
         "Structured",
         {0.0f, 0.90f, 1.0f, 1.0f}, 4,
         {{"fixed-annuities", "Fixed Annuities", "Guaranteed rate annuity analysis"},
          {"variable-annuities", "Variable Annuities", "Market-linked annuity with fees"},
          {"equity-indexed-annuities", "Equity-Indexed", "Index-linked with participation rate"},
          {"inflation-annuities", "Inflation Annuities", "Inflation-adjusted payouts"}}},

        {"structured", "Structured Products", "STRUCTURED",
         "Structured notes, leveraged funds",
         "Structured",
         {1.0f, 0.27f, 0.27f, 1.0f}, 2,
         {{"structured-products", "Structured Notes", "Principal-protected and barrier notes"},
          {"leveraged-funds", "Leveraged Funds", "2x/3x ETF decay and risk analysis"}}},

        {"inflation-protected", "Inflation Protected", "TIPS",
         "TIPS, I-Bonds, stable value",
         "Fixed Income",
         {0.0f, 0.84f, 0.44f, 1.0f}, 3,
         {{"tips", "TIPS", "Treasury Inflation-Protected Securities"},
          {"i-bonds", "I-Bonds", "Series I Savings Bonds"},
          {"stable-value", "Stable Value", "Capital preservation with yield"}}},

        {"strategies", "Strategies", "STRATEGIES",
         "Covered calls, SRI, asset location, performance, risk",
         "Alternatives",
         {1.0f, 0.53f, 0.0f, 1.0f}, 5,
         {{"covered-calls", "Covered Calls", "Option income strategy analysis"},
          {"asset-location", "Asset Location", "Tax-efficient account placement"},
          {"sri-funds", "SRI/ESG Funds", "Socially responsible fund analysis"},
          {"performance", "Performance Analysis", "Risk-adjusted return metrics"},
          {"risk-analysis", "Risk Analysis", "VaR, drawdown, tail risk"}}},

        {"crypto", "Digital Assets", "CRYPTO",
         "Cryptocurrency analysis",
         "Alternatives",
         {0.0f, 0.90f, 1.0f, 1.0f}, 1,
         {{"digital-assets", "Digital Assets", "Crypto valuation and risk metrics"}}},
    };
}

// ============================================================================
// Dynamic form field definitions per analyzer
// ============================================================================

void AltInvestmentsScreen::build_fields_for_analyzer(const std::string& id) {
    current_fields_.clear();
    float_values_.clear();
    int_values_.clear();
    text_values_.clear();
    combo_values_.clear();

    auto F = [&](const std::string& key, const std::string& label, float def, float lo = 0, float hi = 1e6) {
        current_fields_.push_back({key, label, InputField::Type::Float, def, lo, hi, {}});
        float_values_[key] = def;
    };
    auto I = [&](const std::string& key, const std::string& label, int def) {
        current_fields_.push_back({key, label, InputField::Type::Int, (float)def, 0, 100, {}});
        int_values_[key] = def;
    };
    auto C = [&](const std::string& key, const std::string& label, std::vector<std::string> opts, int def = 0) {
        current_fields_.push_back({key, label, InputField::Type::Combo, 0, 0, 0, opts});
        combo_values_[key] = def;
    };

    // Bonds
    if (id == "high-yield") {
        F("coupon_rate", "Coupon Rate (%)", 6.5, 0, 25);
        F("current_price", "Current Price", 95.0, 0, 200);
        C("credit_rating", "Credit Rating", {"BB", "B", "CCC", "CC", "C"});
        I("maturity_years", "Maturity (years)", 5);
    } else if (id == "em-bonds") {
        F("coupon_rate", "Coupon Rate (%)", 7.0, 0, 30);
        F("current_price", "Current Price", 92.0, 0, 200);
        I("maturity_years", "Maturity (years)", 10);
        C("credit_rating", "Credit Rating", {"BBB", "BB", "B", "CCC"});
    } else if (id == "convertible-bonds") {
        F("bond_price", "Bond Price", 105.0, 0, 300);
        F("conversion_ratio", "Conversion Ratio", 20.0, 0, 200);
        F("stock_price", "Stock Price", 50.0, 0, 5000);
        I("maturity_years", "Maturity (years)", 5);
    } else if (id == "preferred-stocks") {
        F("dividend_rate", "Dividend Rate (%)", 5.5, 0, 20);
        F("par_value", "Par Value", 25.0, 0, 1000);
        F("current_price", "Current Price", 24.0, 0, 200);
    }
    // Real Estate
    else if (id == "real-estate") {
        F("acquisition_price", "Acquisition Price", 500000, 0, 1e9);
        F("annual_noi", "Annual NOI", 40000, 0, 1e8);
        F("cap_rate", "Cap Rate (%)", 5.5, 0, 20);
        F("ltv_ratio", "LTV Ratio (%)", 65, 0, 100);
    } else if (id == "reit") {
        F("share_price", "Share Price", 45.0, 0, 1000);
        F("dividend_yield", "Dividend Yield (%)", 4.5, 0, 20);
        F("ffo_per_share", "FFO/Share", 3.50, 0, 100);
        F("nav_per_share", "NAV/Share", 50.0, 0, 500);
    }
    // Hedge Funds
    else if (id == "hedge-funds") {
        C("strategy", "Strategy", {"long_short_equity", "global_macro", "multi_strategy", "merger_arbitrage", "distressed_securities"});
        F("management_fee", "Management Fee (%)", 2.0, 0, 5);
        F("performance_fee", "Performance Fee (%)", 20.0, 0, 50);
        I("lock_up_period", "Lock-up (months)", 12);
    } else if (id == "managed-futures") {
        C("strategy_type", "Strategy Type", {"systematic", "discretionary", "hybrid"});
        F("leverage_ratio", "Leverage Ratio", 2.0, 0, 10);
    } else if (id == "market-neutral") {
        F("annual_return", "Annual Return (%)", 6.0, -50, 100);
        F("volatility", "Volatility (%)", 4.0, 0, 50);
        F("market_beta", "Market Beta", 0.05, -1, 1);
        F("management_fee", "Management Fee (%)", 1.5, 0, 5);
    }
    // Commodities
    else if (id == "precious-metals") {
        C("metal_type", "Metal Type", {"gold", "silver", "platinum", "palladium"});
        C("investment_method", "Method", {"physical", "etf", "mining_stocks", "futures"});
        F("spot_price", "Spot Price", 2000.0, 0, 1e5);
    } else if (id == "natural-resources") {
        F("purchase_price", "Purchase Price", 100000, 0, 1e9);
        F("current_value", "Current Value", 120000, 0, 1e9);
        F("annual_yield", "Annual Yield (%)", 3.5, 0, 30);
    }
    // Private Capital
    else if (id == "private-equity") {
        C("investment_type", "Type", {"venture", "growth", "buyout", "distressed"});
        F("commitment", "Commitment ($)", 1000000, 0, 1e9);
        F("called_capital", "Called Capital ($)", 600000, 0, 1e9);
        F("distributions", "Distributions ($)", 200000, 0, 1e9);
        F("nav", "NAV ($)", 800000, 0, 1e9);
    }
    // Annuities
    else if (id == "fixed-annuities") {
        F("premium", "Premium ($)", 100000, 0, 1e8);
        F("fixed_rate", "Fixed Rate (%)", 3.5, 0, 15);
        I("term_years", "Term (years)", 7);
        F("surrender_charge", "Surrender Charge (%)", 7.0, 0, 20);
    } else if (id == "variable-annuities") {
        F("premium", "Premium ($)", 100000, 0, 1e8);
        F("me_fee", "M&E Fee (%)", 1.25, 0, 5);
        F("investment_fee", "Investment Fee (%)", 0.75, 0, 3);
        F("admin_fee", "Admin Fee ($)", 50.0, 0, 500);
    } else if (id == "equity-indexed-annuities") {
        F("premium", "Premium ($)", 100000, 0, 1e8);
        F("participation_rate", "Participation Rate (%)", 80.0, 0, 200);
        F("cap_rate", "Cap Rate (%)", 10.0, 0, 30);
        F("floor_rate", "Floor Rate (%)", 0.0, -5, 5);
    }
    // Structured Products
    else if (id == "structured-products") {
        F("principal", "Principal ($)", 100000, 0, 1e8);
        F("participation_rate", "Participation Rate (%)", 100, 0, 300);
        F("cap_rate", "Cap Rate (%)", 15, 0, 50);
        I("maturity_years", "Maturity (years)", 5);
        F("protection_level", "Protection Level (%)", 90, 0, 100);
    } else if (id == "leveraged-funds") {
        F("leverage_ratio", "Leverage Multiple", 2.0, 1, 5);
        F("daily_volatility", "Daily Volatility (%)", 1.5, 0, 10);
        F("expense_ratio", "Expense Ratio (%)", 0.95, 0, 5);
    }
    // Inflation Protected
    else if (id == "tips") {
        F("face_value", "Face Value ($)", 10000, 0, 1e7);
        F("real_yield", "Real Yield (%)", 1.5, -2, 10);
        F("inflation_rate", "Inflation Rate (%)", 3.0, 0, 20);
        I("maturity_years", "Maturity (years)", 10);
    } else if (id == "i-bonds") {
        F("purchase_amount", "Purchase Amount ($)", 10000, 0, 10000);
        F("fixed_rate", "Fixed Rate (%)", 0.9, 0, 5);
        F("inflation_rate", "Inflation Rate (%)", 3.24, 0, 20);
    } else if (id == "stable-value") {
        F("face_value", "Face Value ($)", 100000, 0, 1e7);
        F("credited_rate", "Credited Rate (%)", 3.0, 0, 10);
        F("book_value", "Book Value ($)", 100000, 0, 1e7);
        F("market_value", "Market Value ($)", 98000, 0, 1e7);
    }
    // Strategies
    else if (id == "covered-calls") {
        F("stock_price", "Stock Price ($)", 150, 0, 1e5);
        I("shares_owned", "Shares Owned", 100);
        F("strike_price", "Strike Price ($)", 160, 0, 1e5);
        F("premium_received", "Premium ($)", 3.50, 0, 1000);
        I("days_to_expiration", "Days to Expiry", 30);
    } else if (id == "asset-location") {
        F("tax_bracket", "Tax Bracket (%)", 32, 0, 50);
        F("portfolio_value", "Portfolio Value ($)", 500000, 0, 1e9);
        F("taxable_account_value", "Taxable Acct ($)", 200000, 0, 1e9);
        F("tax_deferred_value", "Tax-Deferred ($)", 200000, 0, 1e9);
        F("tax_free_value", "Tax-Free ($)", 100000, 0, 1e9);
    } else if (id == "sri-funds") {
        F("expense_ratio", "Expense Ratio (%)", 0.25, 0, 3);
        F("annual_return", "Annual Return (%)", 10.0, -50, 100);
        F("esg_rating", "ESG Rating (1-10)", 7.5, 1, 10);
    }
    // Performance/Risk (generic)
    else if (id == "performance" || id == "risk-analysis") {
        F("annual_return", "Annual Return (%)", 8.0, -50, 200);
        F("volatility", "Volatility (%)", 15.0, 0, 100);
        F("benchmark_return", "Benchmark Return (%)", 10.0, -50, 200);
        F("risk_free_rate", "Risk-Free Rate (%)", 4.0, 0, 20);
    }
    // Digital Assets
    else if (id == "digital-assets") {
        F("current_price", "Current Price ($)", 50000, 0, 1e7);
        F("purchase_price", "Purchase Price ($)", 30000, 0, 1e7);
        F("volatility", "Annualized Vol (%)", 60, 0, 300);
        F("market_cap_billions", "Market Cap ($B)", 900, 0, 1e5);
    }
    // Fallback
    else {
        F("amount", "Investment Amount ($)", 100000, 0, 1e9);
        F("expected_return", "Expected Return (%)", 8.0, -50, 200);
    }
}

// ============================================================================
// Async helpers
// ============================================================================

bool AltInvestmentsScreen::is_async_busy() const {
    return async_task_.valid() &&
           async_task_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready;
}

void AltInvestmentsScreen::check_async() {
    if (async_task_.valid() &&
        async_task_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        try { async_task_.get(); } catch (...) {}
    }
}

// ============================================================================
// Init
// ============================================================================

void AltInvestmentsScreen::init() {
    categories_ = build_categories();
    if (!categories_.empty() && !categories_[0].analyzers.empty())
        build_fields_for_analyzer(categories_[0].analyzers[0].id);
}

// ============================================================================
// Build params JSON & run analysis
// ============================================================================

nlohmann::json AltInvestmentsScreen::build_params() const {
    nlohmann::json params;
    for (auto& field : current_fields_) {
        switch (field.type) {
            case InputField::Type::Float: {
                auto it = float_values_.find(field.key);
                if (it != float_values_.end()) params[field.key] = it->second;
                break;
            }
            case InputField::Type::Int: {
                auto it = int_values_.find(field.key);
                if (it != int_values_.end()) params[field.key] = it->second;
                break;
            }
            case InputField::Type::Combo: {
                auto it = combo_values_.find(field.key);
                if (it != combo_values_.end() && it->second >= 0 && it->second < (int)field.combo_options.size())
                    params[field.key] = field.combo_options[it->second];
                break;
            }
            case InputField::Type::Text: {
                auto it = text_values_.find(field.key);
                if (it != text_values_.end()) params[field.key] = it->second;
                break;
            }
        }
    }
    return params;
}

void AltInvestmentsScreen::run_analysis() {
    if (is_async_busy()) return;
    if (active_category_idx_ < 0 || active_category_idx_ >= (int)categories_.size()) return;
    auto& cat = categories_[active_category_idx_];
    if (active_analyzer_idx_ < 0 || active_analyzer_idx_ >= (int)cat.analyzers.size()) return;

    analysis_status_ = AsyncStatus::Loading;
    result_ = nlohmann::json();
    error_.clear();

    std::string command = cat.analyzers[active_analyzer_idx_].id;
    nlohmann::json params = build_params();
    std::string params_str = params.dump();

    async_task_ = std::async(std::launch::async, [this, command, params_str]() {
        auto result = python::execute("Analytics/alternateInvestment/cli.py",
                                       {command, "--data", params_str, "--method", "verdict"});
        if (!result.success || result.output.empty()) {
            analysis_status_ = AsyncStatus::Error;
            error_ = result.error.empty() ? "Analysis failed" : result.error;
            return;
        }
        try {
            result_ = nlohmann::json::parse(result.output);
            analysis_status_ = AsyncStatus::Success;
        } catch (const std::exception& ex) {
            analysis_status_ = AsyncStatus::Error;
            error_ = std::string("Parse error: ") + ex.what();
        }
    });
}

// ============================================================================
// Main render
// ============================================================================

void AltInvestmentsScreen::render() {
    if (!initialized_) { init(); initialized_ = true; }
    check_async();

    ui::ScreenFrame frame("##altinv_screen", ImVec2(0, 0), BG_DARK);
    if (!frame.begin()) { frame.end(); return; }

    float w = frame.width();
    float h = frame.height();

    render_header(w);

    float content_h = h - 56.0f; // header + status
    float sidebar_w = left_collapsed_ ? 32.0f : 200.0f;
    float center_w = w - sidebar_w - 2.0f;

    ImGui::BeginGroup();
    {
        ImGui::BeginChild("##ai_sidebar", ImVec2(sidebar_w, content_h), false);
        render_sidebar(sidebar_w, content_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        ImGui::BeginChild("##ai_center", ImVec2(center_w, content_h), false);
        render_center_panel(center_w, content_h);
        ImGui::EndChild();
    }
    ImGui::EndGroup();

    render_status_bar(w);
    frame.end();
}

// ============================================================================
// Header
// ============================================================================

void AltInvestmentsScreen::render_header(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ai_header", ImVec2(w, 32), false);

    ImGui::SetCursorPos(ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::SmallButton("ALTERNATIVE INVESTMENTS");
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "CFA-Level Analytics | 27 Analyzers");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Sidebar — category groups
// ============================================================================

void AltInvestmentsScreen::render_sidebar(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    if (left_collapsed_) {
        if (ImGui::SmallButton(">>##ai")) left_collapsed_ = false;
        ImGui::PopStyleColor();
        return;
    }

    // Header
    ImGui::SetCursorPos(ImVec2(8, 4));
    ImGui::TextColored(TEXT_DIM, "CATEGORIES");
    ImGui::SameLine(w - 24);
    if (ImGui::SmallButton("<<##ai")) left_collapsed_ = true;
    ImGui::Separator();

    // Group categories
    const char* groups[] = {"Fixed Income", "Real Assets", "Alternatives", "Structured"};
    for (auto* grp : groups) {
        ImGui::TextColored(TEXT_DISABLED, "%s", grp);
        for (int i = 0; i < (int)categories_.size(); i++) {
            auto& cat = categories_[i];
            if (cat.group != grp) continue;

            bool active = (i == active_category_idx_);
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(cat.color.x, cat.color.y, cat.color.z, 0.2f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(cat.color.x, cat.color.y, cat.color.z, 0.3f));

            if (ImGui::Selectable(("##cat_" + cat.id).c_str(), active, 0, ImVec2(0, 34))) {
                active_category_idx_ = i;
                active_analyzer_idx_ = 0;
                if (!cat.analyzers.empty())
                    build_fields_for_analyzer(cat.analyzers[0].id);
                analysis_status_ = AsyncStatus::Idle;
            }

            // Overlay
            ImVec2 p = ImGui::GetItemRectMin();
            if (active) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(p.x, p.y), ImVec2(p.x + 2, p.y + 34),
                    ImGui::ColorConvertFloat4ToU32(cat.color));
            }
            ImGui::SetCursorScreenPos(ImVec2(p.x + 8, p.y + 4));
            ImGui::TextColored(active ? cat.color : TEXT_SECONDARY, "%s", cat.short_name.c_str());
            ImGui::SetCursorScreenPos(ImVec2(p.x + 8, p.y + 20));
            ImGui::TextColored(TEXT_DIM, "%d tools", cat.analyzer_count);

            // Restore cursor
            ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + 38));
            ImGui::PopStyleColor(2);
        }
        ImGui::Spacing();
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Center panel
// ============================================================================

void AltInvestmentsScreen::render_center_panel(float w, float h) {
    if (active_category_idx_ < 0 || active_category_idx_ >= (int)categories_.size()) return;
    auto& cat = categories_[active_category_idx_];

    // Sub-header
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ai_subhdr", ImVec2(w, 28), false);
    ImGui::SetCursorPos(ImVec2(8, 5));
    ImGui::TextColored(cat.color, "%s", cat.name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(TEXT_DIM, "| %s", cat.group.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Analyzer selector
    render_analyzer_selector(w);

    // Split: form left, result right
    float form_w = w * 0.45f;
    float result_w = w - form_w - 4.0f;
    float panel_h = h - 56.0f;

    ImGui::BeginGroup();
    {
        ImGui::BeginChild("##ai_form", ImVec2(form_w, panel_h), false);
        render_input_form(form_w, panel_h);
        ImGui::EndChild();

        ImGui::SameLine(0, 2);

        ImGui::BeginChild("##ai_result", ImVec2(result_w, panel_h), false);
        render_result_panel(result_w, panel_h);
        ImGui::EndChild();
    }
    ImGui::EndGroup();
}

// ============================================================================
// Analyzer selector (sub-type tabs)
// ============================================================================

void AltInvestmentsScreen::render_analyzer_selector(float w) {
    auto& cat = categories_[active_category_idx_];
    if (cat.analyzers.size() <= 1) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ai_ansel", ImVec2(w, 28), false);
    ImGui::SetCursorPos(ImVec2(4, 3));

    for (int i = 0; i < (int)cat.analyzers.size(); i++) {
        bool active = (i == active_analyzer_idx_);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cat.color.x, cat.color.y, cat.color.z, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_Text, cat.color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, BG_WIDGET);
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
        }
        if (ImGui::SmallButton(cat.analyzers[i].name.c_str())) {
            active_analyzer_idx_ = i;
            build_fields_for_analyzer(cat.analyzers[i].id);
            analysis_status_ = AsyncStatus::Idle;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 4);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ============================================================================
// Input form (dynamic fields)
// ============================================================================

void AltInvestmentsScreen::render_input_form(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

    auto& cat = categories_[active_category_idx_];
    if (active_analyzer_idx_ >= 0 && active_analyzer_idx_ < (int)cat.analyzers.size()) {
        ImGui::TextColored(TEXT_SECONDARY, "%s", cat.analyzers[active_analyzer_idx_].description.c_str());
    }
    ImGui::Separator();

    float field_w = w - 24.0f;

    for (auto& field : current_fields_) {
        ImGui::Text("%s", field.label.c_str());
        ImGui::SetNextItemWidth(field_w);

        switch (field.type) {
            case InputField::Type::Float: {
                float& val = float_values_[field.key];
                ImGui::DragFloat(("##" + field.key).c_str(), &val, val * 0.01f + 0.1f, field.min_val, field.max_val, "%.2f");
                break;
            }
            case InputField::Type::Int: {
                int& val = int_values_[field.key];
                ImGui::InputInt(("##" + field.key).c_str(), &val);
                break;
            }
            case InputField::Type::Combo: {
                int& val = combo_values_[field.key];
                // Build combo items
                std::string preview = (val >= 0 && val < (int)field.combo_options.size()) ?
                    field.combo_options[val] : "";
                if (ImGui::BeginCombo(("##" + field.key).c_str(), preview.c_str())) {
                    for (int ci = 0; ci < (int)field.combo_options.size(); ci++) {
                        if (ImGui::Selectable(field.combo_options[ci].c_str(), ci == val))
                            val = ci;
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            case InputField::Type::Text: {
                // Not commonly used but supported
                break;
            }
        }
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Analyze button
    ImGui::PushStyleColor(ImGuiCol_Button, ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACCENT_HOVER);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    bool loading = is_async_busy();
    if (ImGui::Button(loading ? "Analyzing..." : "Run Analysis", ImVec2(field_w, 32)) && !loading) {
        run_analysis();
    }
    ImGui::PopStyleColor(3);

    ImGui::PopStyleColor();
}

// ============================================================================
// Result panel
// ============================================================================

void AltInvestmentsScreen::render_result_panel(float w, float h) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(BG_PANEL.x + 0.01f, BG_PANEL.y + 0.01f, BG_PANEL.z + 0.01f, 1));

    if (analysis_status_ == AsyncStatus::Loading) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 50, h / 2));
        ImGui::TextColored(WARNING, "Analyzing...");
    } else if (analysis_status_ == AsyncStatus::Error) {
        ImGui::SetCursorPos(ImVec2(12, 12));
        ImGui::TextColored(ERROR_RED, "Error: %s", error_.c_str());
    } else if (analysis_status_ == AsyncStatus::Idle) {
        ImGui::SetCursorPos(ImVec2(w / 2 - 100, h / 2));
        ImGui::TextColored(TEXT_DIM, "Configure parameters and click Run Analysis");
    } else {
        // Success — render verdict + metrics
        render_verdict(w);
        ImGui::Separator();
        render_metrics(w);
    }

    ImGui::PopStyleColor();
}

// ============================================================================
// Verdict rendering
// ============================================================================

void AltInvestmentsScreen::render_verdict(float w) {
    if (!result_.contains("metrics")) return;
    auto& metrics = result_["metrics"];

    // Verdict category
    std::string verdict_cat = metrics.value("verdict_category", metrics.value("category", ""));
    std::string rating = metrics.value("rating", "");
    std::string summary = metrics.value("analysis_summary", metrics.value("summary", ""));
    std::string recommendation = metrics.value("recommendation", "");

    // Color based on verdict
    ImVec4 verdict_color = TEXT_PRIMARY;
    if (verdict_cat == "THE GOOD") verdict_color = SUCCESS;
    else if (verdict_cat == "THE BAD" || verdict_cat == "THE UGLY") verdict_color = ERROR_RED;
    else if (verdict_cat == "THE FLAWED") verdict_color = WARNING;
    else if (verdict_cat == "MIXED") verdict_color = INFO;

    ImGui::Spacing();
    if (!verdict_cat.empty()) {
        ImGui::TextColored(verdict_color, "%s", verdict_cat.c_str());
        ImGui::SameLine();
    }
    if (!rating.empty()) {
        ImGui::TextColored(ACCENT, "Rating: %s", rating.c_str());
    }

    if (!summary.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(w - 16);
        ImGui::TextWrapped("%s", summary.c_str());
        ImGui::PopTextWrapPos();
    }

    // Key findings
    if (metrics.contains("key_findings") && metrics["key_findings"].is_array()) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_SECONDARY, "Key Findings:");
        for (auto& finding : metrics["key_findings"]) {
            if (finding.is_string()) {
                ImGui::BulletText("%s", finding.get<std::string>().c_str());
            }
        }
    }

    if (!recommendation.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ACCENT, "Recommendation:");
        ImGui::PushTextWrapPos(w - 16);
        ImGui::TextWrapped("%s", recommendation.c_str());
        ImGui::PopTextWrapPos();
    }
}

// ============================================================================
// Metrics rendering (generic key-value from JSON)
// ============================================================================

void AltInvestmentsScreen::render_metrics(float w) {
    if (!result_.contains("metrics")) return;
    auto& metrics = result_["metrics"];
    if (!metrics.is_object()) return;

    // Skip already-rendered verdict fields
    static const std::set<std::string> skip_keys = {
        "verdict_category", "category", "rating", "analysis_summary", "summary",
        "recommendation", "key_findings", "when_acceptable", "better_alternatives"
    };

    ImGui::TextColored(TEXT_SECONDARY, "Detailed Metrics:");
    ImGui::Separator();

    for (auto it = metrics.begin(); it != metrics.end(); ++it) {
        if (skip_keys.count(it.key())) continue;

        ImGui::TextColored(TEXT_DIM, "%s:", it.key().c_str());
        ImGui::SameLine();

        const auto& val = it.value();
        if (val.is_number_float()) {
            ImGui::TextColored(TEXT_PRIMARY, "%.4f", val.get<double>());
        } else if (val.is_number_integer()) {
            ImGui::TextColored(TEXT_PRIMARY, "%lld", (long long)val.get<int64_t>());
        } else if (val.is_string()) {
            ImGui::TextColored(TEXT_PRIMARY, "%s", val.get<std::string>().c_str());
        } else if (val.is_boolean()) {
            ImGui::TextColored(val.get<bool>() ? SUCCESS : ERROR_RED, "%s", val.get<bool>() ? "Yes" : "No");
        } else if (val.is_object() || val.is_array()) {
            ImGui::TextColored(TEXT_DIM, "%s", val.dump(2).c_str());
        } else {
            ImGui::TextColored(TEXT_DISABLED, "N/A");
        }
    }
}

// ============================================================================
// Status bar
// ============================================================================

void AltInvestmentsScreen::render_status_bar(float w) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##ai_status", ImVec2(w, 24), false);
    ImGui::SetCursorPos(ImVec2(8, 4));

    if (active_category_idx_ >= 0 && active_category_idx_ < (int)categories_.size()) {
        auto& cat = categories_[active_category_idx_];
        ImGui::TextColored(TEXT_DIM, "Active: %s | %d analyzers",
            cat.name.c_str(), cat.analyzer_count);
    }

    ImGui::SameLine(w - 200);
    ImGui::TextColored(TEXT_DIM, "Python Analytics Engine");

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace fincept::alt_investments
