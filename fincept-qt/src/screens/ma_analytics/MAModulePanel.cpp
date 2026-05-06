// src/screens/ma_analytics/MAModulePanel.cpp
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QScrollArea>
#include <QTableWidget>

namespace fincept::screens {

using namespace fincept::services::ma;

// ── Helpers: create styled input widgets ─────────────────────────────────────

QDoubleSpinBox* MAModulePanel::make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                                QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QDoubleSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));
    return spin;
}

QSpinBox* MAModulePanel::make_int_spin(int min, int max, int val, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL)
                            .arg(module_.color.name()));
    return spin;
}

QPushButton* MAModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                               "font-weight:700; border:none; padding:8px 20px;"
                               "letter-spacing:1px; }"
                               "QPushButton:hover { background:%5; }"
                               "QPushButton:pressed { background:%6; }")
                           .arg(module_.color.name())
                           .arg(ui::colors::BG_BASE())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL)
                           .arg(module_.color.lighter(120).name())
                           .arg(module_.color.darker(120).name()));
    return btn;
}

QWidget* MAModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(label, row);
    lbl->setFixedWidth(160);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    return row;
}

QWidget* MAModulePanel::build_metric_card(const QString& label, const QString& value, const QString& color,
                                          QWidget* parent) {
    auto* card = new QWidget(parent);
    card->setStyleSheet(QString("background:%1; border:1px solid %2; padding:10px;")
                            .arg(ui::colors::BG_RAISED())
                            .arg(ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 8, 10, 8);
    vl->setSpacing(4);
    auto* lbl = new QLabel(label, card);
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; letter-spacing:1px;")
                           .arg(ui::colors::TEXT_TERTIARY())
                           .arg(ui::fonts::TINY)
                           .arg(ui::fonts::DATA_FAMILY));
    auto* val = new QLabel(value, card);
    val->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; font-family:%3;")
                           .arg(color)
                           .arg(ui::fonts::HEADER)
                           .arg(ui::fonts::DATA_FAMILY));
    vl->addWidget(lbl);
    vl->addWidget(val);
    return card;
}

// ── Constructor ──────────────────────────────────────────────────────────────

MAModulePanel::MAModulePanel(const ModuleInfo& info, QWidget* parent) : QWidget(parent), module_(info) {
    build_ui();
    connect_service();
    refresh_theme();
}

void MAModulePanel::connect_service() {
    auto& svc = MAAnalyticsService::instance();
    connect(&svc, &MAAnalyticsService::result_ready, this, &MAModulePanel::on_result_ready);
    connect(&svc, &MAAnalyticsService::error_occurred, this, &MAModulePanel::on_error);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void MAModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header bar
    header_bar_ = new QWidget(this);
    header_bar_->setFixedHeight(36);
    auto* hhl = new QHBoxLayout(header_bar_);
    hhl->setContentsMargins(16, 0, 16, 0);
    header_title_ = new QLabel(module_.label.toUpper(), header_bar_);
    hhl->addWidget(header_title_);

    auto* div = new QWidget(header_bar_);
    div->setFixedSize(1, 14);
    div->setObjectName("maPanelDivider");
    hhl->addWidget(div);

    header_category_ = new QLabel(module_.category.toUpper(), header_bar_);
    hhl->addWidget(header_category_);
    hhl->addStretch();

    status_label_ = new QLabel(header_bar_);
    hhl->addWidget(status_label_);
    root->addWidget(header_bar_);

    // Scrollable content area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* panel_content = nullptr;
    switch (module_.id) {
        case ModuleId::Valuation:
            panel_content = build_valuation_panel();
            break;
        case ModuleId::Merger:
            panel_content = build_merger_panel();
            break;
        case ModuleId::Deals:
            panel_content = build_deals_panel();
            break;
        case ModuleId::Startup:
            panel_content = build_startup_panel();
            break;
        case ModuleId::Fairness:
            panel_content = build_fairness_panel();
            break;
        case ModuleId::Industry:
            panel_content = build_industry_panel();
            break;
        case ModuleId::Advanced:
            panel_content = build_advanced_panel();
            break;
        case ModuleId::Comparison:
            panel_content = build_comparison_panel();
            break;
    }

    scroll->setWidget(panel_content);
    root->addWidget(scroll, 1);
}





// ═══════════════════════════════════════════════════════════════════════════════
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════

void MAModulePanel::clear_results() {
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

void MAModulePanel::display_error(const QString& msg) {
    clear_results();
    auto* err = new QLabel(msg);
    err->setWordWrap(true);
    QColor neg(ui::colors::NEGATIVE());
    auto neg_rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(%4,0.08); border:1px solid rgba(%4,0.3);")
                           .arg(ui::colors::NEGATIVE())
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY())
                           .arg(neg_rgb));
    results_layout_->addWidget(err);
    status_label_->setText("Error");
}

static QString format_value(const QJsonValue& val) {
    if (val.isDouble()) {
        double v = val.toDouble();
        if (std::abs(v) >= 1e9)
            return QString("$%1B").arg(v / 1e9, 0, 'f', 1);
        if (std::abs(v) >= 1e6)
            return QString("$%1M").arg(v / 1e6, 0, 'f', 1);
        if (std::abs(v) >= 1e3)
            return QString("$%1K").arg(v / 1e3, 0, 'f', 0);
        if (std::abs(v) < 1.0 && std::abs(v) > 0.0001)
            return QString("%1%").arg(v * 100, 0, 'f', 1);
        return QString::number(v, 'f', 2);
    }
    if (val.isBool())
        return val.toBool() ? "YES" : "NO";
    if (val.isString())
        return val.toString();
    return QString::fromUtf8("—");
}

static QTableWidget* build_json_table(const QJsonArray& arr, const QString& accent, QWidget* parent) {
    if (arr.isEmpty())
        return nullptr;
    // Collect all column keys from first object
    auto first = arr[0].toObject();
    QStringList cols;
    for (auto it = first.begin(); it != first.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            cols.append(it.key());
    if (cols.isEmpty())
        return nullptr;

    auto* table = new QTableWidget(arr.size(), cols.size(), parent);
    table->setHorizontalHeaderLabels(cols);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMaximumHeight(300);
    table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                 "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                 "QTableWidget::item { padding:4px 8px; }"
                                 "QTableWidget::item:selected { background:%6; }"
                                 "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                                 "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                 "QTableWidget::item:alternate { background:%9; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL)
                             .arg(QString("rgba(%1,0.15)").arg(accent))
                             .arg(ui::colors::BG_RAISED())
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::colors::ROW_ALT()));

    for (int r = 0; r < arr.size(); ++r) {
        auto obj = arr[r].toObject();
        for (int c = 0; c < cols.size(); ++c) {
            auto* item = new QTableWidgetItem(format_value(obj[cols[c]]));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, c, item);
        }
    }
    table->resizeColumnsToContents();
    return table;
}

static QTableWidget* build_kv_table(const QJsonObject& obj, const QString& accent, QWidget* parent) {
    // Collect only scalar key-value pairs
    QStringList keys;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            keys.append(it.key());
    if (keys.isEmpty())
        return nullptr;

    auto* table = new QTableWidget(keys.size(), 2, parent);
    table->setHorizontalHeaderLabels({"Metric", "Value"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMaximumHeight(400);
    table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                 "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                 "QTableWidget::item { padding:4px 8px; }"
                                 "QTableWidget::item:selected { background:%6; }"
                                 "QHeaderView::section { background:%7; color:%8; font-weight:700;"
                                 "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                 "QTableWidget::item:alternate { background:%9; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL)
                             .arg(QString("rgba(%1,0.15)").arg(accent))
                             .arg(ui::colors::BG_RAISED())
                             .arg(ui::colors::TEXT_SECONDARY())
                             .arg(ui::colors::ROW_ALT()));

    for (int r = 0; r < keys.size(); ++r) {
        auto label = keys[r];
        label.replace('_', ' ');
        auto* key_item = new QTableWidgetItem(label.toUpper());
        key_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->setItem(r, 0, key_item);
        auto* val_item = new QTableWidgetItem(format_value(obj[keys[r]]));
        val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(r, 1, val_item);
    }
    table->resizeColumnsToContents();
    return table;
}

void MAModulePanel::display_result(const QJsonObject& payload) {
    clear_results();

    QString accent = QString("%1,%2,%3").arg(module_.color.red()).arg(module_.color.green()).arg(module_.color.blue());

    // Section header
    auto* header = new QLabel("RESULTS");
    header->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2; letter-spacing:1px;"
                                  "padding:4px 0;")
                              .arg(module_.color.name())
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // 1. Metric cards for top-level scalar values
    auto* grid = new QWidget(this);
    auto* gl = new QGridLayout(grid);
    gl->setContentsMargins(0, 0, 0, 0);
    gl->setSpacing(8);

    int col = 0, row = 0;
    bool has_scalars = false;
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        if (it.value().isObject() || it.value().isArray())
            continue;
        has_scalars = true;
        QString label = it.key();
        label.replace('_', ' ');
        auto* card = build_metric_card(label.toUpper(), format_value(it.value()), module_.color.name(), grid);
        gl->addWidget(card, row, col);
        col++;
        if (col >= 3) {
            col = 0;
            row++;
        }
    }
    if (has_scalars)
        results_layout_->addWidget(grid);
    else
        grid->deleteLater();

    // 2. Tables for nested objects and arrays
    for (auto it = payload.begin(); it != payload.end(); ++it) {
        if (it.value().isArray()) {
            auto arr = it.value().toArray();
            if (arr.isEmpty())
                continue;

            QString sec_label = it.key();
            sec_label.replace('_', ' ');
            auto* sec = new QLabel(sec_label.toUpper());
            sec->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                       "letter-spacing:1px; padding:8px 0 4px 0;")
                                   .arg(module_.color.name())
                                   .arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(sec);

            if (arr[0].isObject()) {
                auto* table = build_json_table(arr, accent, this);
                if (table)
                    results_layout_->addWidget(table);
            } else {
                // Simple array — display as comma-separated
                QStringList items;
                for (const auto& v : arr)
                    items.append(format_value(v));
                auto* lbl = new QLabel(items.join(", "));
                lbl->setWordWrap(true);
                lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;"
                                           "padding:4px; background:%4; border:1px solid %5;")
                                       .arg(ui::colors::TEXT_PRIMARY())
                                       .arg(ui::fonts::SMALL)
                                       .arg(ui::fonts::DATA_FAMILY)
                                       .arg(ui::colors::BG_RAISED())
                                       .arg(ui::colors::BORDER_DIM()));
                results_layout_->addWidget(lbl);
            }
        } else if (it.value().isObject()) {
            auto obj = it.value().toObject();
            if (obj.isEmpty())
                continue;

            QString sec_label = it.key();
            sec_label.replace('_', ' ');
            auto* sec = new QLabel(sec_label.toUpper());
            sec->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; font-family:%2;"
                                       "letter-spacing:1px; padding:8px 0 4px 0;")
                                   .arg(module_.color.name())
                                   .arg(ui::fonts::DATA_FAMILY));
            results_layout_->addWidget(sec);

            auto* table = build_kv_table(obj, accent, this);
            if (table)
                results_layout_->addWidget(table);
        }
    }

    // 3. Raw JSON viewer (collapsed)
    auto* raw_btn = new QPushButton("Show Raw JSON", this);
    raw_btn->setCursor(Qt::PointingHandCursor);
    raw_btn->setStyleSheet(QString("QPushButton { color:%1; font-size:%2px; font-family:%3;"
                                   "background:transparent; border:1px solid %4; padding:4px 12px; }"
                                   "QPushButton:hover { background:%5; }")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::TINY)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::colors::BORDER_DIM())
                               .arg(ui::colors::BG_HOVER()));

    auto* raw_text = new QTextEdit;
    raw_text->setReadOnly(true);
    raw_text->setVisible(false);
    raw_text->setMaximumHeight(300);
    raw_text->setPlainText(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    raw_text->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                    "font-family:%4; font-size:%5px; padding:8px; }")
                                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
                                .arg(ui::fonts::DATA_FAMILY)
                                .arg(ui::fonts::SMALL));

    connect(raw_btn, &QPushButton::clicked, this, [raw_text, raw_btn]() {
        bool showing = raw_text->isVisible();
        raw_text->setVisible(!showing);
        raw_btn->setText(showing ? "Show Raw JSON" : "Hide Raw JSON");
    });

    results_layout_->addWidget(raw_btn);
    results_layout_->addWidget(raw_text);

    status_label_->setText("Done");
}

// ── Service signal handlers ──────────────────────────────────────────────────

static const QHash<ModuleId, QStringList>& get_context_map() {
    static const QHash<ModuleId, QStringList> map = {
        {ModuleId::Valuation,
         {"dcf", "dcf_sensitivity", "football_field", "lbo_returns", "lbo_model", "lbo_debt_schedule",
          "lbo_sensitivity", "trading_comps", "precedent_txns"}},
        {ModuleId::Merger,
         {"merger_model", "accretion_dilution", "pro_forma", "sources_uses", "contribution", "revenue_synergies",
          "cost_synergies", "synergy_dcf", "integration_costs", "payment_structure", "earnout", "exchange_ratio",
          "collar", "cvr"}},
        {ModuleId::Deals, {"scan_filings", "all_deals", "search_deals", "create_deal", "update_deal", "parse_filing"}},
        {ModuleId::Startup,
         {"berkus", "scorecard", "vc_method", "first_chicago", "risk_factor", "startup_comprehensive"}},
        {ModuleId::Fairness, {"fairness_opinion", "premium_analysis", "process_quality"}},
        {ModuleId::Industry, {"tech_metrics", "healthcare_metrics", "finserv_metrics"}},
        {ModuleId::Advanced, {"monte_carlo", "regression"}},
        {ModuleId::Comparison,
         {"compare_deals", "rank_deals", "benchmark_premium", "payment_structures", "industry_deals"}},
    };
    return map;
}

void MAModulePanel::on_result_ready(const QString& context, const QJsonObject& payload) {
    auto it = get_context_map().find(module_.id);
    if (it != get_context_map().end() && it->contains(context)) {
        display_result(payload);
    }
}

void MAModulePanel::on_error(const QString& context, const QString& message) {
    auto it = get_context_map().find(module_.id);
    if (it != get_context_map().end() && it->contains(context)) {
        display_error(QString("[%1] %2").arg(context, message));
    }
}

// ── Tab stylesheet helper ───────────────────────────────────────────────────
void MAModulePanel::apply_tab_stylesheet() {
    if (!sub_tabs_)
        return;
    sub_tabs_->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                     "QTabBar::tab { background:%3; color:%4; padding:6px 16px;"
                                     "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                     "QTabBar::tab:selected { background:%2; color:%7; font-weight:700;"
                                     "border-bottom:2px solid %7; }")
                                 .arg(ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(), ui::colors::BG_RAISED())
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::DATA_FAMILY())
                                 .arg(ui::fonts::SMALL)
                                 .arg(module_.color.name()));
}

// ── Theme refresh ───────────────────────────────────────────────────────────
void MAModulePanel::refresh_theme() {
    // Header bar
    if (header_bar_)
        header_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    // Header title
    if (header_title_)
        header_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(module_.color.name())
                .arg(ui::fonts::TINY)
                .arg(ui::fonts::DATA_FAMILY()));

    // Header divider
    if (header_bar_) {
        auto* div = header_bar_->findChild<QWidget*>("maPanelDivider");
        if (div)
            div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    }

    // Header category
    if (header_category_)
        header_category_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                            .arg(ui::colors::TEXT_TERTIARY())
                                            .arg(ui::fonts::TINY)
                                            .arg(ui::fonts::DATA_FAMILY()));

    // Status label
    if (status_label_)
        status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                         .arg(ui::colors::TEXT_TERTIARY())
                                         .arg(ui::fonts::TINY)
                                         .arg(ui::fonts::DATA_FAMILY()));

    // Tab widget
    apply_tab_stylesheet();
}

} // namespace fincept::screens
