// src/screens/alt_investments/AltInvestmentsScreen_Analysis.cpp
//
// Analysis flow: rebuild_form, on_category/analyzer/analyze, collect_form_data,
// run_analysis, display_verdict, append_metric_row.
//
// Part of the partial-class split of AltInvestmentsScreen.cpp.

#include "screens/alt_investments/AltInvestmentsScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "services/python_cli/PythonCliService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>

namespace fincept::screens {

using namespace fincept::ui;

static AltField text_field(const QString& key, const QString& label, const QString& def = "") {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Text;
    f.combo_items = def;
    return f;
}

static AltField spin_field(const QString& key, const QString& label, double def, double min_v, double max_v,
                           int dec = 2, const QString& pfx = "", const QString& sfx = "", bool div100 = false) {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Spin;
    f.default_val = def;
    f.min_val = min_v;
    f.max_val = max_v;
    f.decimals = dec;
    f.prefix = pfx;
    f.suffix = sfx;
    f.divide_100 = div100;
    return f;
}

static AltField combo_field(const QString& key, const QString& label, const QString& items) {
    AltField f;
    f.key = key;
    f.label = label;
    f.type = AltField::Combo;
    f.combo_items = items;
    return f;
}

// ── Category definitions ─────────────────────────────────────────────────────

static QList<AltCategory> build_categories() {
    return {
        {"bonds",
         "Bonds & Fixed Income",
         "#3B82F6",
         "B",
         {
             {"high-yield", "High-Yield Bonds", "Spread analysis, default probability, equity-like behavior"},
             {"em-bonds", "Emerging Market Bonds", "Yield spread, sovereign default risk, currency risk"},
             {"convertible-bonds", "Convertible Bonds", "Conversion premium, bond floor, upside participation"},
             {"preferred-stocks", "Preferred Stocks", "Current yield, call risk, dividend safety analysis"},
         }},
        {"real_estate",
         "Real Estate",
         "#8B5CF6",
         "RE",
         {
             {"real-estate", "Direct Property / REIT", "NOI, cap rate, DCF valuation, financing analysis"},
         }},
        {"hedge_funds",
         "Hedge Funds",
         "#EAB308",
         "HF",
         {
             {"hedge-funds", "Long/Short Equity", "Strategy metrics, fee impact, liquidity risk"},
             {"managed-futures", "Managed Futures / CTA", "Fee drag, trend following, crisis alpha analysis"},
             {"market-neutral", "Market Neutral", "Beta neutrality, factor exposure, leverage risk"},
         }},
        {"commodities",
         "Commodities",
         "#F97316",
         "C",
         {
             {"natural-resources", "Natural Resources", "Futures basis, contango/backwardation, roll yield"},
             {"pme", "Precious Metals Equities", "Correlation to metals, inflation hedge, drawdowns"},
         }},
        {"private_capital",
         "Private Capital",
         "#EC4899",
         "PE",
         {
             {"private-capital", "Private Equity / Venture Capital", "IRR, MOIC, DPI, RVPI, J-curve, fee drag"},
         }},
        {"annuities",
         "Annuities",
         "#22C55E",
         "AN",
         {
             {"annuities", "Fixed Annuity", "Payout analysis, breakeven years, inflation erosion"},
             {"variable-annuities", "Variable Annuity", "Total fee drag, tax deferral myth, vs. alternatives"},
             {"eia", "Equity-Indexed Annuity", "Crediting rate, upside cap limitations, surrender charges"},
             {"inflation-annuity", "Inflation-Indexed Annuity",
              "vs. fixed annuity, vs. TIPS ladder, longevity break-even"},
         }},
        {"structured",
         "Structured Products",
         "#06B6D4",
         "SP",
         {
             {"structured-products", "Structured Notes", "Complexity score, hidden costs, issuer credit risk"},
             {"leveraged-funds", "Leveraged ETFs (2x/3x)", "Volatility decay, path-dependency, holding cost"},
         }},
        {"inflation",
         "Inflation Protected",
         "#FB923C",
         "IP",
         {
             {"tips", "TIPS — Inflation-Linked Treasuries", "Real yield, inflation scenarios, tax efficiency"},
             {"ibonds", "I-Bonds — Series I Savings Bonds", "Composite rate, early redemption penalty, vs. TIPS"},
             {"stable-value", "Stable Value Funds", "Market-to-book ratio, crediting rate, wrap contract risk"},
         }},
        {"strategies",
         "Strategies & Analysis",
         "#67E8F9",
         "ST",
         {
             {"covered-calls", "Covered Call Strategy", "Tax consequences, opportunity cost, premium income"},
             {"sri", "SRI / ESG Funds", "Performance vs. benchmark, screening costs, approaches"},
             {"asset-location", "Tax-Efficient Asset Location",
              "Optimal account placement, muni bonds, annual tax drag"},
         }},
        {"crypto",
         "Digital Assets",
         "#F87171",
         "DA",
         {
             {"digital-assets", "Cryptocurrency", "Fundamental metrics, NVT ratio, market cap analysis"},
         }},
    };
}

// ── Per-analyzer form fields ─────────────────────────────────────────────────

static QList<AltField> fields_for(const QString& id) {
    if (id == "high-yield")
        return {
            text_field("name", "BOND NAME", "HY Corp Bond"),
            spin_field("par_value", "PAR VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("price", "MARKET PRICE ($)", 950, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 8.5, 0, 30, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 5, 0.5, 30, 1),
            combo_field("credit_rating", "CREDIT RATING", "BB|B|CCC|BB+|BB-|B+|B-|CCC+"),
        };
    if (id == "em-bonds")
        return {
            text_field("name", "BOND NAME", "Brazil 2030"),
            spin_field("face_value", "FACE VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("current_market_value", "MARKET PRICE ($)", 950, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 6.0, 0, 25, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 10, 0.5, 30, 1),
            spin_field("credit_spread", "CREDIT SPREAD (%)", 3.0, 0, 20, 2, "", "%", true),
        };
    if (id == "convertible-bonds")
        return {
            text_field("name", "BOND NAME", "Tesla Conv 2028"),
            spin_field("par_value", "PAR VALUE ($)", 1000, 100, 1e9, 0, "$"),
            spin_field("current_price", "MARKET PRICE ($)", 1100, 100, 1e9, 2, "$"),
            spin_field("coupon_rate", "COUPON RATE (%)", 2.0, 0, 15, 2, "", "%"),
            spin_field("maturity_years", "MATURITY (years)", 5, 0.5, 30, 1),
            spin_field("stock_price", "STOCK PRICE ($)", 55, 1, 1e6, 2, "$"),
            spin_field("conversion_ratio", "CONVERSION RATIO", 18, 1, 1e4, 2),
        };
    if (id == "preferred-stocks")
        return {
            text_field("name", "SECURITY NAME", "JPM Series J Preferred"),
            spin_field("par_value", "PAR VALUE ($)", 25, 1, 1e4, 2, "$"),
            spin_field("current_price", "MARKET PRICE ($)", 24.5, 1, 1e4, 2, "$"),
            spin_field("dividend_rate", "DIVIDEND RATE (%)", 5.5, 0, 20, 2, "", "%", true),
        };
    if (id == "real-estate")
        return {
            text_field("name", "PROPERTY NAME", "Class A Office"),
            spin_field("acquisition_price", "PURCHASE PRICE ($)", 1000000, 1000, 1e12, 0, "$"),
            spin_field("gross_income", "GROSS INCOME ($/yr)", 120000, 0, 1e9, 0, "$"),
            spin_field("vacancy_rate", "VACANCY RATE (%)", 5.0, 0, 100, 1, "", "%", true),
            spin_field("operating_expenses", "OPER. EXPENSES ($/yr)", 40000, 0, 1e9, 0, "$"),
            spin_field("loan_amount", "LOAN AMOUNT ($)", 700000, 0, 1e12, 0, "$"),
            spin_field("interest_rate", "MORTGAGE RATE (%)", 6.5, 0, 30, 2, "", "%", true),
            spin_field("loan_term", "LOAN TERM (years)", 30, 5, 50, 0),
        };
    if (id == "hedge-funds")
        return {
            text_field("name", "FUND NAME", "Global Macro Fund"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
            spin_field("hurdle_rate", "HURDLE RATE (%)", 6.0, 0, 20, 2, "", "%", true),
            combo_field("strategy_type", "STRATEGY",
                        "equity_long_short|global_macro|event_driven|relative_value|multi_strategy"),
        };
    if (id == "managed-futures")
        return {
            text_field("name", "FUND NAME", "CTA Trend Fund"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
            spin_field("gross_return", "GROSS RETURN (%)", 12.0, -50, 100, 2, "", "%", true),
        };
    if (id == "market-neutral")
        return {
            text_field("name", "FUND NAME", "Market Neutral Fund"),
            spin_field("gross_leverage", "GROSS LEVERAGE", 2.0, 0.5, 10, 1),
            spin_field("management_fee", "MGMT FEE (%)", 1.5, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "PERF FEE (%)", 20.0, 0, 50, 1, "", "%", true),
        };
    if (id == "natural-resources")
        return {
            text_field("name", "COMMODITY NAME", "WTI Crude Oil"),
            spin_field("spot_price", "SPOT PRICE ($)", 80, 0, 1e6, 2, "$"),
            spin_field("three_month_futures", "3M FUTURES ($)", 82, 0, 1e6, 2, "$"),
            spin_field("six_month_futures", "6M FUTURES ($)", 84, 0, 1e6, 2, "$"),
            spin_field("twelve_month_futures", "12M FUTURES ($)", 87, 0, 1e6, 2, "$"),
            combo_field("sector", "SECTOR", "energy|metals|agriculture|livestock"),
        };
    if (id == "pme")
        return {
            text_field("name", "FUND NAME", "Gold Miners ETF"),
        };
    if (id == "private-capital")
        return {
            text_field("name", "FUND NAME", "Buyout Fund III"),
            spin_field("management_fee", "MGMT FEE (%)", 2.0, 0, 10, 2, "", "%", true),
            spin_field("performance_fee", "CARRIED INT (%)", 20.0, 0, 40, 1, "", "%", true),
            spin_field("hurdle_rate", "HURDLE RATE (%)", 8.0, 0, 20, 2, "", "%", true),
        };
    if (id == "annuities")
        return {
            text_field("name", "ANNUITY NAME", "Fixed Annuity"),
            spin_field("premium", "PREMIUM ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("annual_payout_rate", "ANNUAL PAYOUT RATE (%)", 5.0, 0.1, 20, 2, "", "%", true),
            spin_field("payout_years", "PAYOUT TERM (years)", 20, 5, 50, 0),
            spin_field("guaranteed_rate", "GUARANTEED RATE (%)", 4.0, 0, 15, 2, "", "%", true),
        };
    if (id == "variable-annuities")
        return {
            text_field("name", "ANNUITY NAME", "Variable Annuity"),
            spin_field("premium", "PREMIUM ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("me_fee", "M&E FEE (%)", 1.25, 0, 5, 2, "", "%", true),
            spin_field("investment_fee", "INVESTMENT FEE (%)", 0.75, 0, 5, 2, "", "%", true),
            spin_field("admin_fee", "ADMIN FEE (%)", 0.15, 0, 3, 2, "", "%", true),
            spin_field("surrender_period", "SURRENDER PERIOD (yrs)", 7, 0, 20, 0),
        };
    if (id == "eia")
        return {
            text_field("name", "PRODUCT NAME", "EIA Product"),
            spin_field("index_return", "INDEX RETURN (%)", 10.0, -50, 100, 2, "", "%", true),
            spin_field("participation_rate", "PARTICIPATION (%)", 80.0, 10, 100, 1, "", "%", true),
            spin_field("cap_rate", "CAP RATE (%)", 6.0, 0, 50, 2, "", "%", true),
            spin_field("floor_rate", "FLOOR RATE (%)", 0.0, -10, 10, 2, "", "%", true),
        };
    if (id == "inflation-annuity")
        return {
            text_field("name", "ANNUITY NAME", "Inflation-Indexed Annuity"),
            spin_field("real_payout_rate", "REAL PAYOUT RATE (%)", 4.0, 0, 15, 2, "", "%", true),
            spin_field("inflation_rate", "ASSUMED INFLATION (%)", 3.0, 0, 15, 2, "", "%", true),
            spin_field("payout_years", "PAYOUT TERM (years)", 20, 5, 50, 0),
            spin_field("fixed_payout_rate", "FIXED ALT RATE (%)", 5.5, 0, 15, 2, "", "%", true),
        };
    if (id == "structured-products")
        return {
            text_field("name", "PRODUCT NAME", "Principal Protected Note"),
            spin_field("principal", "PRINCIPAL ($)", 50000, 1000, 1e9, 0, "$"),
            spin_field("participation_rate", "PARTICIPATION (%)", 80.0, 10, 100, 1, "", "%", true),
            spin_field("cap_rate", "UPSIDE CAP (%)", 20.0, 0, 100, 1, "", "%", true),
            spin_field("maturity_years", "MATURITY (years)", 5, 1, 30, 0),
        };
    if (id == "leveraged-funds")
        return {
            text_field("name", "FUND NAME", "3x S&P 500 ETF"),
            spin_field("leverage_ratio", "LEVERAGE RATIO", 3.0, 2.0, 4.0, 1),
        };
    if (id == "tips")
        return {
            text_field("name", "SECURITY NAME", "TIPS 2031"),
            spin_field("face_value", "FACE VALUE ($)", 10000, 1000, 1e9, 0, "$"),
            spin_field("current_market_value", "MARKET PRICE ($)", 10200, 1000, 1e9, 2, "$"),
            spin_field("coupon_rate", "REAL COUPON (%)", 2.5, 0, 10, 2, "", "%", true),
            spin_field("maturity_years", "MATURITY (years)", 7, 1, 30, 1),
        };
    if (id == "ibonds")
        return {
            text_field("name", "BOND NAME", "I-Bond 2025"),
            spin_field("purchase_price", "PURCHASE PRICE ($)", 10000, 25, 10000, 0, "$"),
            spin_field("fixed_rate", "FIXED RATE (%)", 0.4, 0, 5, 2, "", "%", true),
            spin_field("inflation_rate", "CPI-U INFLATION (%)", 3.4, 0, 20, 2, "", "%", true),
        };
    if (id == "stable-value")
        return {
            text_field("name", "FUND NAME", "Stable Value Fund"),
            spin_field("book_value", "BOOK VALUE ($)", 100000, 1000, 1e9, 0, "$"),
            spin_field("market_value", "MARKET VALUE ($)", 98000, 1000, 1e9, 0, "$"),
            spin_field("crediting_rate", "CREDITING RATE (%)", 3.2, 0, 15, 2, "", "%", true),
        };
    if (id == "covered-calls")
        return {
            text_field("name", "POSITION NAME", "AAPL Covered Call"),
            spin_field("stock_price", "STOCK PRICE ($)", 150, 1, 1e6, 2, "$"),
            spin_field("strike_price", "STRIKE PRICE ($)", 160, 1, 1e6, 2, "$"),
            spin_field("premium", "PREMIUM / SHARE ($)", 5, 0, 1e4, 2, "$"),
            spin_field("shares", "SHARES", 100, 1, 1e6, 0),
            spin_field("cost_basis", "COST BASIS ($)", 125, 1, 1e6, 2, "$"),
            spin_field("holding_period_days", "HOLDING DAYS", 400, 1, 3650, 0),
        };
    if (id == "sri")
        return {
            text_field("name", "FUND NAME", "ESG Equity Fund"),
            spin_field("sri_return", "SRI RETURN (%)", 9.0, -30, 100, 2, "", "%", true),
            spin_field("benchmark_return", "BENCHMARK RETURN (%)", 10.0, -30, 100, 2, "", "%", true),
            spin_field("expense_ratio", "EXPENSE RATIO (%)", 0.8, 0, 5, 2, "", "%", true),
            spin_field("time_period_years", "TIME PERIOD (years)", 10, 1, 50, 0),
        };
    if (id == "asset-location")
        return {
            text_field("name", "ASSET NAME", "REITs / High-Yield"),
            combo_field("asset_class", "ASSET CLASS",
                        "reits|high_yield_bonds|stocks|municipal_bonds|tips|commodities|cash"),
            spin_field("tax_bracket", "TAX BRACKET (%)", 24.0, 10, 45, 1, "", "%", true),
        };
    if (id == "digital-assets")
        return {
            text_field("name", "ASSET NAME", "Bitcoin"),
            spin_field("price", "PRICE ($)", 45000, 0, 1e9, 2, "$"),
            spin_field("market_cap", "MARKET CAP ($B)", 850, 0, 1e6, 1, "$", "B"),
            spin_field("volume_24h", "24H VOLUME ($B)", 25, 0, 1e6, 1, "$", "B"),
            spin_field("circulating_supply", "CIRC. SUPPLY (M)", 19.5, 0, 1e6, 1, "", "M"),
            spin_field("max_supply", "MAX SUPPLY (M)", 21.0, 0, 1e6, 1, "", "M"),
        };
    return {text_field("name", "NAME", "")};
}


void AltInvestmentsScreen::rebuild_form(int cat, int ana) {
    for (auto* w : field_widgets_)
        w->deleteLater();
    field_widgets_.clear();
    current_fields_.clear();

    while (form_layout_->count() > 0) {
        auto* item = form_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const QString ana_id = categories_[cat].analyzers[ana].id;
    current_fields_ = fields_for(ana_id);

    int i = 0;
    while (i < current_fields_.size()) {
        const auto& f = current_fields_[i];
        const bool pair = (f.type == AltField::Spin && i + 1 < current_fields_.size() &&
                           current_fields_[i + 1].type == AltField::Spin);

        if (pair) {
            auto* row = new QWidget(this);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(8);

            auto build_spin_col = [&](const AltField& field) -> QWidget* {
                auto* col = new QWidget(this);
                auto* cl = new QVBoxLayout(col);
                cl->setContentsMargins(0, 0, 0, 0);
                cl->setSpacing(3);
                auto* lbl = new QLabel(field.label);
                lbl->setObjectName("altFieldLabel");
                cl->addWidget(lbl);
                auto* sp = new QDoubleSpinBox;
                sp->setRange(field.min_val, field.max_val);
                sp->setValue(field.default_val);
                sp->setDecimals(field.decimals);
                sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
                if (!field.prefix.isEmpty())
                    sp->setPrefix(field.prefix);
                if (!field.suffix.isEmpty())
                    sp->setSuffix(field.suffix);
                cl->addWidget(sp);
                field_widgets_.append(sp);
                return col;
            };

            rl->addWidget(build_spin_col(f), 1);
            rl->addWidget(build_spin_col(current_fields_[i + 1]), 1);
            form_layout_->addWidget(row);
            i += 2;
        } else {
            auto* col = new QWidget(this);
            auto* cl = new QVBoxLayout(col);
            cl->setContentsMargins(0, 0, 0, 0);
            cl->setSpacing(3);
            auto* lbl = new QLabel(f.label);
            lbl->setObjectName("altFieldLabel");
            cl->addWidget(lbl);

            if (f.type == AltField::Text) {
                auto* le = new QLineEdit(f.combo_items);
                cl->addWidget(le);
                field_widgets_.append(le);
            } else if (f.type == AltField::Combo) {
                auto* cb = new QComboBox;
                for (const auto& item : f.combo_items.split('|'))
                    cb->addItem(item.trimmed());
                cl->addWidget(cb);
                field_widgets_.append(cb);
            } else {
                auto* sp = new QDoubleSpinBox;
                sp->setRange(f.min_val, f.max_val);
                sp->setValue(f.default_val);
                sp->setDecimals(f.decimals);
                sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
                if (!f.prefix.isEmpty())
                    sp->setPrefix(f.prefix);
                if (!f.suffix.isEmpty())
                    sp->setSuffix(f.suffix);
                cl->addWidget(sp);
                field_widgets_.append(sp);
            }
            form_layout_->addWidget(col);
            ++i;
        }
    }
    form_layout_->addStretch(1);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AltInvestmentsScreen::on_category_changed(int index) {
    if (index < 0 || index >= categories_.size())
        return;
    active_category_ = index;

    for (int i = 0; i < cat_btns_.size(); ++i) {
        cat_btns_[i]->setProperty("active", i == index);
        cat_btns_[i]->style()->unpolish(cat_btns_[i]);
        cat_btns_[i]->style()->polish(cat_btns_[i]);
    }

    const auto& cat = categories_[index];
    center_title_->setText(cat.name.toUpper());
    status_category_->setText("CATEGORY: " + cat.name.toUpper());

    analyzer_combo_->blockSignals(true);
    analyzer_combo_->clear();
    for (const auto& a : cat.analyzers)
        analyzer_combo_->addItem(a.name);
    analyzer_combo_->blockSignals(false);

    on_analyzer_changed(0);
    LOG_INFO("AltInvestments", "Category: " + cat.name);
}

void AltInvestmentsScreen::on_analyzer_changed(int index) {
    if (index < 0 || index >= categories_[active_category_].analyzers.size())
        return;
    active_analyzer_ = index;

    const auto& ana = categories_[active_category_].analyzers[index];
    center_desc_->setText(ana.description);
    status_analyzer_->setText(ana.name.toUpper());
    analyzer_combo_->setCurrentIndex(index);
    rebuild_form(active_category_, index);

    fincept::ScreenStateManager::instance().notify_changed(this);
}

void AltInvestmentsScreen::on_analyze() {
    if (loading_)
        return;
    const auto& ana = categories_[active_category_].analyzers[active_analyzer_];
    run_analysis(ana.id, collect_form_data());
}

// ── Form data collection ─────────────────────────────────────────────────────

QJsonObject AltInvestmentsScreen::collect_form_data() const {
    QJsonObject form;
    for (int i = 0; i < current_fields_.size() && i < field_widgets_.size(); ++i) {
        const auto& f = current_fields_[i];
        auto* w = field_widgets_[i];
        if (f.type == AltField::Text) {
            if (auto* le = qobject_cast<QLineEdit*>(w))
                form[f.key] = le->text();
        } else if (f.type == AltField::Combo) {
            if (auto* cb = qobject_cast<QComboBox*>(w))
                form[f.key] = cb->currentText();
        } else {
            if (auto* sp = qobject_cast<QDoubleSpinBox*>(w)) {
                double val = sp->value();
                if (f.suffix == "B")
                    val *= 1e9;
                else if (f.suffix == "M")
                    val *= 1e6;
                else if (f.divide_100)
                    val /= 100.0;
                form[f.key] = val;
            }
        }
    }
    return form;
}

// ── Python execution ──────────────────────────────────────────────────────────

void AltInvestmentsScreen::run_analysis(const QString& command, const QJsonObject& form) {
    const QString data_json = QString::fromUtf8(QJsonDocument(form).toJson(QJsonDocument::Compact));
    const QString cache_key = "alt_screen:" + command + ":" + data_json;

    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (doc.isObject()) {
            display_verdict(doc.object(), command);
            return;
        }
    }

    set_loading(true);
    verdict_badge_->setText("ANALYZING...");
    verdict_badge_->setStyleSheet(QString("color:%1; background:rgba(217,119,6,0.15);"
                                          " font-size:11px; font-weight:700; padding:4px 14px;")
                                      .arg(colors::AMBER()));
    verdict_rating_->clear();
    verdict_rec_->clear();

    while (metrics_layout_->count() > 0) {
        auto* item = metrics_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    metrics_layout_->addStretch(1);

    QPointer<AltInvestmentsScreen> self = this;

    services::python_cli::PythonCliService::instance().run(
        QStringLiteral("Analytics/alternateInvestment/cli.py"),
        {command, QStringLiteral("--data"), data_json},
        [self, command, cache_key](const services::python_cli::CliResult& r) {
            if (!self)
                return;
            self->set_loading(false);

            if (!r.success) {
                self->display_error(r.error.isEmpty() ? "Analysis failed" : r.error);
                return;
            }

            const QJsonObject obj = r.data;

            fincept::CacheManager::instance().put(
                cache_key,
                QVariant(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact))),
                10 * 60, "alt_investments");

            self->display_verdict(obj, command);
            LOG_INFO("AltInvestments", "Analysis complete: " + command);
        });
}

// ── Verdict display ───────────────────────────────────────────────────────────

static QString stars_for(const QString& rating_str) {
    const QRegularExpression re(R"((\d+)\s*/\s*10)");
    const auto match = re.match(rating_str);
    if (!match.hasMatch())
        return rating_str;
    const int n = match.captured(1).toInt();
    const int stars = qMax(1, qMin(5, (n + 1) / 2));
    return QString("\u2605").repeated(stars) + QString("\u2606").repeated(5 - stars) + "  " + rating_str;
}

void AltInvestmentsScreen::display_verdict(const QJsonObject& result, const QString& /*command*/) {
    const auto metrics = result.contains("metrics") ? result["metrics"].toObject() : result;

    // ── Badge ─────────────────────────────────────────────────────────────────
    QString category = metrics.value("analysis_category").toString(metrics.value("category").toString());
    if (category.isEmpty())
        category = result.value("asset_type")
                       .toString(result.value("product_type").toString(result.value("fund_type").toString("RESULT")));

    QString badge_color, badge_bg;
    const QString lcat = category.toLower();
    if (lcat.contains("good") || lcat.contains("ok")) {
        badge_color = colors::POSITIVE();
        badge_bg = "rgba(22,163,74,0.15)";
    } else if (lcat.contains("ugly") || lcat.contains("bad") || lcat.contains("avoid")) {
        badge_color = colors::NEGATIVE();
        badge_bg = "rgba(220,38,38,0.15)";
    } else if (lcat.contains("flaw") || lcat.contains("warn") || lcat.contains("caution")) {
        badge_color = "#FFD700";
        badge_bg = "rgba(255,215,0,0.15)";
    } else {
        badge_color = colors::CYAN();
        badge_bg = "rgba(8,145,178,0.15)";
    }

    verdict_badge_->setText(category.toUpper());
    verdict_badge_->setStyleSheet(QString("color:%1; background:%2; font-size:11px; font-weight:700;"
                                          " padding:4px 14px; letter-spacing:0.5px;")
                                      .arg(badge_color, badge_bg));

    // ── Rating ────────────────────────────────────────────────────────────────
    const QString rating = metrics.value("rating").toString(result.value("rating").toString());
    if (!rating.isEmpty())
        verdict_rating_->setText(stars_for(rating));

    // ── Recommendation ────────────────────────────────────────────────────────
    const QString rec =
        metrics.value("analysis_recommendation")
            .toString(metrics.value("bottom_line").toString(metrics.value("analysis_summary").toString()));
    if (!rec.isEmpty())
        verdict_rec_->setText(rec);

    // ── Metric rows ───────────────────────────────────────────────────────────
    while (metrics_layout_->count() > 0) {
        auto* item = metrics_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (auto it = metrics.begin(); it != metrics.end(); ++it)
        append_metric_row(metrics_container_, metrics_layout_, it.key(), it.value(), 0);

    metrics_layout_->addStretch(1);
}

// ── Recursive metric renderer ─────────────────────────────────────────────────

void AltInvestmentsScreen::append_metric_row(QWidget* /*parent*/, QVBoxLayout* vl, const QString& key,
                                             const QJsonValue& val, int depth) {
    static const QSet<QString> kSkip = {
        "success",     "asset_type",        "method",   "product_type", "fund_type",
        "strategy",    "analysis_category", "category", "rating",       "analysis_recommendation",
        "bottom_line", "analysis_summary"};
    if (kSkip.contains(key))
        return;

    // Object → section header + recurse
    if (val.isObject()) {
        QString sec_text = key;
        sec_text.replace('_', ' ').replace('-', ' ');
        auto* sec = new QLabel(sec_text.toUpper());
        sec->setObjectName("altMetricSection");
        vl->addWidget(sec);
        for (auto it = val.toObject().begin(); it != val.toObject().end(); ++it)
            append_metric_row(nullptr, vl, it.key(), it.value(), depth + 1);
        return;
    }

    // Array → section + bullet list
    if (val.isArray()) {
        const auto arr = val.toArray();
        if (arr.isEmpty())
            return;
        QString sec_text = key;
        sec_text.replace('_', ' ').replace('-', ' ');
        auto* sec = new QLabel(sec_text.toUpper());
        sec->setObjectName("altMetricSection");
        vl->addWidget(sec);
        for (const auto& elem : arr) {
            const QString text = "\xB7  " + (elem.isString() ? elem.toString() : format_value(elem));
            auto* row = new QWidget(this);
            row->setObjectName("altMetricRow");
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(12, 4, 12, 4);
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            const QString lc = text.toLower();
            if (lc.contains("warn") || lc.contains("risk") || lc.contains("bad") || lc.contains("avoid") ||
                lc.contains("toxic") || lc.contains("never"))
                lbl->setObjectName("altMetricBad");
            else if (lc.contains("good") || lc.contains("accept") || lc.contains("safe") || lc.contains("benefit"))
                lbl->setObjectName("altMetricGood");
            else
                lbl->setObjectName("altMetricBullet");
            rl->addWidget(lbl, 1);
            vl->addWidget(row);
        }
        return;
    }

    // Annotation string — full-width coloured row
    if (val.isString()) {
        const QString text = val.toString();
        if (text.isEmpty())
            return;
        const bool is_annotation = key.startsWith("analysis_") || key == "note" || key == "interpretation" ||
                                   key == "situation" || key == "investor_risk" || key == "reason";
        if (is_annotation) {
            auto* row = new QWidget(this);
            row->setObjectName("altMetricRow");
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(12, 5, 12, 5);
            auto* lbl = new QLabel(text);
            lbl->setWordWrap(true);
            const QString lc = text.toLower();
            if (lc.contains("warn") || lc.contains("toxic") || lc.contains("avoid") || lc.contains("bad") ||
                lc.contains("never"))
                lbl->setObjectName("altMetricWarn");
            else if (lc.contains("good") || lc.contains("safe") || lc.contains("acceptable"))
                lbl->setObjectName("altMetricGood");
            else
                lbl->setObjectName("altMetricBulletVal");
            rl->addWidget(lbl, 1);
            vl->addWidget(row);
            return;
        }
    }

    // Normal key = value row
    QStringList words = key.split('_');
    for (auto& w : words)
        if (!w.isEmpty())
            w[0] = w[0].toUpper();

    auto* row = new QWidget(this);
    row->setObjectName("altMetricRow");
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(12 + depth * 8, 5, 12, 5);

    auto* klbl = new QLabel(words.join(' '));
    klbl->setObjectName("altMetricKey");
    auto* vlbl = new QLabel(format_value(val));
    vlbl->setObjectName("altMetricVal");
    vlbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    rl->addWidget(klbl, 1);
    rl->addWidget(vlbl);
    vl->addWidget(row);
}

} // namespace fincept::screens
