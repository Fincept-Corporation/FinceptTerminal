// src/screens/geopolitics/TradeAnalysisPanel.cpp
#include "screens/geopolitics/TradeAnalysisPanel.h"

#include "services/geopolitics/GeopoliticsService.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>

namespace fincept::screens {

using namespace fincept::services::geo;

TradeAnalysisPanel::TradeAnalysisPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    connect_service();
}

void TradeAnalysisPanel::connect_service() {
    auto& svc = GeopoliticsService::instance();
    connect(&svc, &GeopoliticsService::trade_result_ready, this, &TradeAnalysisPanel::on_trade_result);
    connect(&svc, &GeopoliticsService::error_occurred, this, &TradeAnalysisPanel::on_error);
}

void TradeAnalysisPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setFixedHeight(40);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(16, 0, 16, 0);
    auto* title = new QLabel("TRADE GEOPOLITICS ANALYSIS", header);
    title->setStyleSheet(QString("color:#FFC400; font-size:%1px; font-weight:700; font-family:%2; letter-spacing:1px;")
                             .arg(ui::fonts::TINY)
                             .arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(title);
    hhl->addStretch();
    status_label_ = new QLabel(header);
    status_label_->setStyleSheet(
        QString("color:%1; font-size:9px; font-family:%2;").arg(ui::colors::TEXT_TERTIARY).arg(ui::fonts::DATA_FAMILY));
    hhl->addWidget(status_label_);
    root->addWidget(header);

    // Scrollable content
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    auto* content = new QWidget(scroll);
    auto* cvl = new QVBoxLayout(content);
    cvl->setContentsMargins(16, 16, 16, 16);
    cvl->setSpacing(12);

    tabs_ = new QTabWidget(content);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                 "QTabBar::tab { background:%3; color:%4; padding:6px 16px;"
                                 "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                 "QTabBar::tab:selected { background:%2; color:#FFC400; font-weight:700;"
                                 "border-bottom:2px solid #FFC400; }")
                             .arg(ui::colors::BORDER_DIM, ui::colors::BG_SURFACE, ui::colors::BG_RAISED)
                             .arg(ui::colors::TEXT_SECONDARY)
                             .arg(ui::fonts::DATA_FAMILY)
                             .arg(ui::fonts::SMALL));

    auto label_style = QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_SECONDARY)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY);
    auto input_style = QString("QDoubleSpinBox, QLineEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:4px 6px; border-radius:2px; }"
                               "QDoubleSpinBox:focus, QLineEdit:focus { border-color:#FFC400; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL);

    // ── Benefits & Costs Tab ──
    auto* benefits = new QWidget;
    auto* bvl = new QVBoxLayout(benefits);
    bvl->setContentsMargins(12, 12, 12, 12);
    bvl->setSpacing(8);

    auto* ben_hint = new QLabel("Analyzes trade benefits (efficiency, consumer, growth) and costs "
                                "(adjustment, distributional, vulnerability) for a given trade scenario.",
                                benefits);
    ben_hint->setWordWrap(true);
    ben_hint->setStyleSheet(label_style);
    bvl->addWidget(ben_hint);

    auto* ben_country = new QLineEdit(benefits);
    ben_country->setPlaceholderText("Country (e.g. United States)");
    ben_country->setStyleSheet(input_style);
    bvl->addWidget(ben_country);

    auto* ben_partner = new QLineEdit(benefits);
    ben_partner->setPlaceholderText("Trade partner (e.g. China)");
    ben_partner->setStyleSheet(input_style);
    bvl->addWidget(ben_partner);

    auto* ben_sector = new QLineEdit(benefits);
    ben_sector->setPlaceholderText("Sector (e.g. technology, agriculture)");
    ben_sector->setStyleSheet(input_style);
    bvl->addWidget(ben_sector);

    auto* ben_run = new QPushButton("ANALYZE TRADE BENEFITS & COSTS", benefits);
    ben_run->setCursor(Qt::PointingHandCursor);
    ben_run->setStyleSheet(QString("QPushButton { background:#FFC400; color:%1; font-family:%2; font-size:%3px;"
                                   "font-weight:700; border:none; padding:8px; border-radius:2px; letter-spacing:1px; }"
                                   "QPushButton:hover { background:#E5B000; }")
                               .arg(ui::colors::BG_BASE)
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::fonts::SMALL));
    connect(ben_run, &QPushButton::clicked, this, [this, ben_country, ben_partner, ben_sector]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        params["country"] = ben_country->text();
        params["partner"] = ben_partner->text();
        params["sector"] = ben_sector->text();
        GeopoliticsService::instance().analyze_trade_benefits(params);
    });
    bvl->addWidget(ben_run);
    bvl->addStretch();
    tabs_->addTab(benefits, "Benefits & Costs");

    // ── Restrictions Tab ──
    auto* restrict = new QWidget;
    auto* rvl = new QVBoxLayout(restrict);
    rvl->setContentsMargins(12, 12, 12, 12);
    rvl->setSpacing(8);

    auto* res_hint = new QLabel("Analyzes impact of trade restrictions: tariffs, quotas, export subsidies, "
                                "non-tariff barriers, and sanctions on bilateral trade.",
                                restrict);
    res_hint->setWordWrap(true);
    res_hint->setStyleSheet(label_style);
    rvl->addWidget(res_hint);

    auto* res_country = new QLineEdit(restrict);
    res_country->setPlaceholderText("Imposing country");
    res_country->setStyleSheet(input_style);
    rvl->addWidget(res_country);

    auto* res_target = new QLineEdit(restrict);
    res_target->setPlaceholderText("Target country");
    res_target->setStyleSheet(input_style);
    rvl->addWidget(res_target);

    auto* res_type = new QLineEdit(restrict);
    res_type->setPlaceholderText("Restriction type (tariff, quota, sanction)");
    res_type->setStyleSheet(input_style);
    rvl->addWidget(res_type);

    auto* res_run = new QPushButton("ANALYZE TRADE RESTRICTIONS", restrict);
    res_run->setCursor(Qt::PointingHandCursor);
    res_run->setStyleSheet(ben_run->styleSheet());
    connect(res_run, &QPushButton::clicked, this, [this, res_country, res_target, res_type]() {
        status_label_->setText("Analyzing...");
        QJsonObject params;
        params["imposing_country"] = res_country->text();
        params["target_country"] = res_target->text();
        params["restriction_type"] = res_type->text();
        GeopoliticsService::instance().analyze_trade_restrictions(params);
    });
    rvl->addWidget(res_run);
    rvl->addStretch();
    tabs_->addTab(restrict, "Restrictions");

    cvl->addWidget(tabs_);

    // Results area
    results_container_ = new QWidget(content);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    cvl->addWidget(results_container_);

    scroll->setWidget(content);
    root->addWidget(scroll, 1);
}

void TradeAnalysisPanel::display_result(const QJsonObject& data) {
    // Clear previous
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    auto* header = new QLabel("ANALYSIS RESULTS");
    header->setStyleSheet(QString("color:#FFC400; font-size:9px; font-weight:700; font-family:%1; letter-spacing:1px;")
                              .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(header);

    // Key-value table for scalar fields
    QStringList keys;
    for (auto it = data.begin(); it != data.end(); ++it)
        if (!it.value().isObject() && !it.value().isArray())
            keys.append(it.key());

    if (!keys.isEmpty()) {
        auto* table = new QTableWidget(keys.size(), 2);
        table->setHorizontalHeaderLabels({"Metric", "Value"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setStretchLastSection(true);
        table->verticalHeader()->setVisible(false);
        table->setMaximumHeight(400);
        table->setStyleSheet(QString("QTableWidget { background:%1; color:%2; gridline-color:%3;"
                                     "font-family:%4; font-size:%5px; border:1px solid %3; }"
                                     "QTableWidget::item { padding:4px 8px; }"
                                     "QHeaderView::section { background:%6; color:%7; font-weight:700;"
                                     "padding:4px 8px; border:1px solid %3; font-family:%4; font-size:%5px; }"
                                     "QTableWidget::item:alternate { background:%8; }")
                                 .arg(ui::colors::BG_SURFACE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL)
                                 .arg(ui::colors::BG_RAISED)
                                 .arg(ui::colors::TEXT_SECONDARY)
                                 .arg(ui::colors::ROW_ALT));

        for (int r = 0; r < keys.size(); ++r) {
            auto label = keys[r];
            label.replace('_', ' ');
            table->setItem(r, 0, new QTableWidgetItem(label.toUpper()));
            auto val = data[keys[r]];
            QString text = val.isDouble() ? QString::number(val.toDouble(), 'f', 2)
                           : val.isBool() ? (val.toBool() ? "YES" : "NO")
                                          : val.toString();
            auto* vi = new QTableWidgetItem(text);
            vi->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table->setItem(r, 1, vi);
        }
        table->resizeColumnsToContents();
        results_layout_->addWidget(table);
    }

    // Raw JSON
    auto* raw = new QTextEdit;
    raw->setReadOnly(true);
    raw->setMaximumHeight(200);
    raw->setPlainText(QJsonDocument(data).toJson(QJsonDocument::Indented));
    raw->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                               "font-family:%4; font-size:%5px; padding:8px; }")
                           .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM)
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL));
    results_layout_->addWidget(raw);

    status_label_->setText("Done");
}

void TradeAnalysisPanel::on_trade_result(const QString& context, const QJsonObject& data) {
    if (context == "trade_benefits" || context == "trade_restrictions")
        display_result(data);
}

void TradeAnalysisPanel::on_error(const QString& context, const QString& message) {
    if (context != "trade_benefits" && context != "trade_restrictions")
        return;
    status_label_->setText("Error");
    // Clear and show error
    while (results_layout_->count() > 0) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    auto* err = new QLabel(QString("[%1] %2").arg(context, message));
    err->setWordWrap(true);
    err->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; padding:12px;"
                               "background:rgba(220,38,38,0.08); border:1px solid rgba(220,38,38,0.3);"
                               "border-radius:2px;")
                           .arg(ui::colors::NEGATIVE)
                           .arg(ui::fonts::SMALL)
                           .arg(ui::fonts::DATA_FAMILY));
    results_layout_->addWidget(err);
}

} // namespace fincept::screens
