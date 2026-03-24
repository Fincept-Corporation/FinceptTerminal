// src/screens/portfolio/views/CustomIndexView.cpp
#include "screens/portfolio/views/CustomIndexView.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineSeries>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace fincept::screens {

static const QStringList kMethods = {
    "Price Weighted",      "Market Cap Weighted", "Equal Weighted", "Float Adjusted", "Fundamental Weighted",
    "Modified Market Cap", "Factor Weighted",     "Risk Parity",    "Geometric Mean", "Capped Weighted",
};

static const QStringList kMethodDescriptions = {
    "Dow Jones style — sum of prices / divisor",
    "S&P 500 style — total market cap / divisor",
    "Equal weight per constituent (1/N)",
    "Market cap variant adjusted for float",
    "Weighted by fundamental score",
    "Modified market cap with smoothing",
    "Custom factor weights",
    "Inverse volatility weighted",
    "Geometric average of price relatives",
    "Market cap with weight cap limit",
};

CustomIndexView::CustomIndexView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void CustomIndexView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                                  ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    tabs_->addTab(build_create_panel(), "CREATE INDEX");
    tabs_->addTab(build_index_list_panel(), "MY INDICES");
    tabs_->addTab(build_performance_panel(), "PERFORMANCE");

    layout->addWidget(tabs_);
}

QWidget* CustomIndexView::build_create_panel() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto* title = new QLabel("CREATE CUSTOM INDEX");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    // Config row
    auto* config = new QHBoxLayout;
    config->setSpacing(8);

    auto add_field = [&](const QString& label, QWidget* widget) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::TEXT_TERTIARY));
        config->addWidget(lbl);
        config->addWidget(widget);
    };

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("My Custom Index");
    name_edit_->setFixedSize(160, 24);
    name_edit_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:10px; }"
                "QLineEdit:focus { border-color:%4; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER));
    add_field("NAME:", name_edit_);

    method_cb_ = new QComboBox;
    method_cb_->addItems(kMethods);
    method_cb_->setFixedHeight(24);
    method_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:10px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
    add_field("METHOD:", method_cb_);

    base_edit_ = new QLineEdit("1000");
    base_edit_->setFixedSize(80, 24);
    base_edit_->setStyleSheet(name_edit_->styleSheet());
    add_field("BASE:", base_edit_);

    config->addStretch();

    create_btn_ = new QPushButton("CREATE INDEX");
    create_btn_->setFixedHeight(26);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:#000; border:none;"
                                       "  padding:0 16px; font-size:10px; font-weight:700; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::AMBER, ui::colors::WARNING));
    connect(create_btn_, &QPushButton::clicked, this, &CustomIndexView::create_index);
    config->addWidget(create_btn_);

    layout->addLayout(config);

    // Method description
    auto* method_desc = new QLabel;
    method_desc->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    method_desc->setText(kMethodDescriptions[0]);
    connect(method_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [method_desc](int idx) {
        if (idx >= 0 && idx < kMethodDescriptions.size())
            method_desc->setText(kMethodDescriptions[idx]);
    });
    layout->addWidget(method_desc);

    // Constituents table (from portfolio holdings)
    auto* const_title = new QLabel("CONSTITUENTS (from portfolio holdings)");
    const_title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(const_title);

    const_table_ = new QTableWidget;
    const_table_->setColumnCount(5);
    const_table_->setHorizontalHeaderLabels({"INCLUDE", "SYMBOL", "PRICE", "WEIGHT", "MKT VALUE"});
    const_table_->setSelectionMode(QAbstractItemView::NoSelection);
    const_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const_table_->setShowGrid(false);
    const_table_->verticalHeader()->setVisible(false);
    const_table_->horizontalHeader()->setStretchLastSection(true);
    const_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                "QHeaderView::section { background:%4; color:%5; border:none;"
                "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px; font-weight:700; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BG_SURFACE,
                 ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    layout->addWidget(const_table_, 1);

    return w;
}

QWidget* CustomIndexView::build_index_list_panel() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("MY CUSTOM INDICES");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    index_list_table_ = new QTableWidget;
    index_list_table_->setColumnCount(5);
    index_list_table_->setHorizontalHeaderLabels({"NAME", "METHOD", "BASE VALUE", "CURRENT VALUE", "CHANGE"});
    index_list_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    index_list_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    index_list_table_->setShowGrid(false);
    index_list_table_->verticalHeader()->setVisible(false);
    index_list_table_->horizontalHeader()->setStretchLastSection(true);
    index_list_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                "QTableWidget::item:selected { background:%7; color:%8; }"
                "QHeaderView::section { background:%4; color:%5; border:none;"
                "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px; font-weight:700; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BG_SURFACE,
                 ui::colors::TEXT_SECONDARY, ui::colors::AMBER, ui::colors::AMBER_DIM, ui::colors::AMBER));

    auto* empty_msg =
        new QLabel("No custom indices created yet.\nGo to CREATE INDEX tab to build one from your portfolio.");
    empty_msg->setAlignment(Qt::AlignCenter);
    empty_msg->setStyleSheet(QString("color:%1; font-size:11px; padding:40px;").arg(ui::colors::TEXT_TERTIARY));

    layout->addWidget(index_list_table_, 1);
    layout->addWidget(empty_msg);

    return w;
}

QWidget* CustomIndexView::build_performance_panel() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("INDEX PERFORMANCE");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE));
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY));
    chart->setAnimationOptions(QChart::NoAnimation);

    perf_chart_ = new QChartView(chart);
    perf_chart_->setRenderHint(QPainter::Antialiasing);
    perf_chart_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(perf_chart_, 1);

    auto* note = new QLabel("Create a custom index to see performance tracking here.");
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(note);

    return w;
}

void CustomIndexView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_constituents();
}

void CustomIndexView::update_constituents() {
    const_table_->setRowCount(summary_.holdings.size());

    for (int r = 0; r < summary_.holdings.size(); ++r) {
        const auto& h = summary_.holdings[r];
        const_table_->setRowHeight(r, 28);

        // Include checkbox (text-based)
        auto* check = new QTableWidgetItem("\u2611");
        check->setTextAlignment(Qt::AlignCenter);
        check->setForeground(QColor(ui::colors::POSITIVE));
        const_table_->setItem(r, 0, check);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            const_table_->setItem(r, col, item);
        };

        set(1, h.symbol, ui::colors::CYAN);
        set(2, QString::number(h.current_price, 'f', 2));
        set(3, QString("%1%").arg(QString::number(h.weight, 'f', 1)), ui::colors::AMBER);
        set(4, QString("%1 %2").arg(currency_, QString::number(h.market_value, 'f', 2)));
    }
}

void CustomIndexView::create_index() {
    QString name = name_edit_->text().trimmed();
    if (name.isEmpty())
        return;

    LOG_INFO("CustomIndex", QString("Creating index '%1' with method '%2'").arg(name, method_cb_->currentText()));

    // TODO: Persist to database via a CustomIndexRepository (future enhancement)
    // For now, just log the creation
    name_edit_->clear();
}

} // namespace fincept::screens
