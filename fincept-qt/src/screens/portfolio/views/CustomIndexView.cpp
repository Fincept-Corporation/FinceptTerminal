// src/screens/portfolio/views/CustomIndexView.cpp
#include "screens/portfolio/views/CustomIndexView.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#define QT_CHARTS_USE_NAMESPACE
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineSeries>
#include <QMessageBox>
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

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString table_style() {
    return QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                   "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                   "QTableWidget::item:selected { background:%4; color:%5; }"
                   "QHeaderView::section { background:%6; color:%7; border:none;"
                   "  border-bottom:2px solid %8; padding:4px 8px; font-size:9px; font-weight:700; }")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM(),
             ui::colors::AMBER(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER());
}

static QString input_style() {
    return QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:10px; }"
                   "QLineEdit:focus { border-color:%4; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER());
}

// ── Constructor ───────────────────────────────────────────────────────────────

CustomIndexView::CustomIndexView(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_indices();
}

// ── UI construction ───────────────────────────────────────────────────────────

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
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    tabs_->addTab(build_create_panel(), "CREATE INDEX");
    tabs_->addTab(build_index_list_panel(), "MY INDICES");
    tabs_->addTab(build_performance_panel(), "PERFORMANCE");

    layout->addWidget(tabs_);
}

QWidget* CustomIndexView::build_create_panel() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    auto* title = new QLabel("CREATE CUSTOM INDEX");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    // Config row
    auto* config = new QHBoxLayout;
    config->setSpacing(8);

    auto add_field = [&](const QString& label, QWidget* widget) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::TEXT_TERTIARY()));
        config->addWidget(lbl);
        config->addWidget(widget);
    };

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText("My Custom Index");
    name_edit_->setFixedSize(160, 24);
    name_edit_->setStyleSheet(input_style());
    add_field("NAME:", name_edit_);

    method_cb_ = new QComboBox;
    method_cb_->addItems(kMethods);
    method_cb_->setFixedHeight(24);
    method_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:10px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    add_field("METHOD:", method_cb_);

    base_edit_ = new QLineEdit("1000");
    base_edit_->setFixedSize(80, 24);
    base_edit_->setStyleSheet(input_style());
    add_field("BASE:", base_edit_);

    config->addStretch();

    create_btn_ = new QPushButton("CREATE INDEX");
    create_btn_->setFixedHeight(26);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                       "  padding:0 16px; font-size:10px; font-weight:700; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_BASE()));
    connect(create_btn_, &QPushButton::clicked, this, &CustomIndexView::create_index);
    config->addWidget(create_btn_);

    layout->addLayout(config);

    // Method description
    auto* method_desc = new QLabel(kMethodDescriptions[0]);
    method_desc->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    connect(method_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [method_desc](int idx) {
        if (idx >= 0 && idx < kMethodDescriptions.size())
            method_desc->setText(kMethodDescriptions[idx]);
    });
    layout->addWidget(method_desc);

    // Status label
    create_status_ = new QLabel;
    create_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE()));
    create_status_->hide();
    layout->addWidget(create_status_);

    // Constituents table header
    auto* const_title = new QLabel("CONSTITUENTS (from portfolio holdings)");
    const_title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(const_title);

    const_table_ = new QTableWidget;
    const_table_->setColumnCount(5);
    const_table_->setHorizontalHeaderLabels({"INCLUDE", "SYMBOL", "PRICE", "WEIGHT", "MKT VALUE"});
    const_table_->setSelectionMode(QAbstractItemView::NoSelection);
    const_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    const_table_->setShowGrid(false);
    const_table_->verticalHeader()->setVisible(false);
    const_table_->horizontalHeader()->setStretchLastSection(true);
    const_table_->setStyleSheet(table_style());
    layout->addWidget(const_table_, 1);

    return w;
}

QWidget* CustomIndexView::build_index_list_panel() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    // Header row
    auto* header_row = new QHBoxLayout;
    auto* title = new QLabel("MY CUSTOM INDICES");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    header_row->addWidget(title);
    header_row->addStretch();

    delete_btn_ = new QPushButton("DELETE SELECTED");
    delete_btn_->setFixedHeight(26);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  padding:0 14px; font-size:9px; font-weight:700; }"
                                       "QPushButton:hover { background:%1; }")
                                   .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
    connect(delete_btn_, &QPushButton::clicked, this, &CustomIndexView::delete_selected_index);
    header_row->addWidget(delete_btn_);
    layout->addLayout(header_row);

    index_list_table_ = new QTableWidget;
    index_list_table_->setColumnCount(6);
    index_list_table_->setHorizontalHeaderLabels({"NAME", "METHOD", "BASE", "CURRENT VALUE", "CHANGE", "CREATED"});
    index_list_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    index_list_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    index_list_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    index_list_table_->setShowGrid(false);
    index_list_table_->verticalHeader()->setVisible(false);
    index_list_table_->horizontalHeader()->setStretchLastSection(true);
    index_list_table_->setStyleSheet(table_style());

    // Single-click on row → show performance
    connect(index_list_table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= loaded_indices_.size())
            return;
        const auto& idx = loaded_indices_[row];
        show_index_performance(idx.id, idx.name);
        tabs_->setCurrentIndex(2);
    });

    layout->addWidget(index_list_table_, 1);

    list_empty_msg_ =
        new QLabel("No custom indices created yet.\nGo to CREATE INDEX tab to build one from your portfolio.");
    list_empty_msg_->setAlignment(Qt::AlignCenter);
    list_empty_msg_->setStyleSheet(QString("color:%1; font-size:11px; padding:40px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(list_empty_msg_);

    return w;
}

QWidget* CustomIndexView::build_performance_panel() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    perf_title_ = new QLabel("INDEX PERFORMANCE");
    perf_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(perf_title_);

    perf_stack_ = new QStackedWidget;

    // Page 0 — placeholder
    auto* placeholder = new QLabel("Select an index from MY INDICES to see its performance.");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QString("color:%1; font-size:11px; padding:40px;").arg(ui::colors::TEXT_TERTIARY()));
    perf_stack_->addWidget(placeholder);

    // Page 1 — chart
    auto* chart_widget = new QWidget(this);
    auto* chart_layout = new QVBoxLayout(chart_widget);
    chart_layout->setContentsMargins(0, 0, 0, 0);

    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->setAnimationOptions(QChart::NoAnimation);

    perf_chart_view_ = new QChartView(chart);
    perf_chart_view_->setRenderHint(QPainter::Antialiasing);
    perf_chart_view_->setStyleSheet("border:none; background:transparent;");
    chart_layout->addWidget(perf_chart_view_);

    perf_stack_->addWidget(chart_widget);
    layout->addWidget(perf_stack_, 1);

    return w;
}

// ── Data population ───────────────────────────────────────────────────────────

void CustomIndexView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_constituents();
    load_indices(); // refresh with latest portfolio prices
}

void CustomIndexView::update_constituents() {
    const_table_->setRowCount(static_cast<int>(summary_.holdings.size()));

    for (int r = 0; r < static_cast<int>(summary_.holdings.size()); ++r) {
        const auto& h = summary_.holdings[r];
        const_table_->setRowHeight(r, 28);

        // Check box (text-based, ticked by default)
        auto* check = new QTableWidgetItem("\u2611");
        check->setTextAlignment(Qt::AlignCenter);
        check->setForeground(QColor(ui::colors::POSITIVE()));
        check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
        check->setCheckState(Qt::Checked);
        const_table_->setItem(r, 0, check);

        auto set_item = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            const_table_->setItem(r, col, item);
        };

        set_item(1, h.symbol, ui::colors::CYAN);
        set_item(2, QString::number(h.current_price, 'f', 2));
        set_item(3, QString("%1%").arg(QString::number(h.weight, 'f', 1)), ui::colors::AMBER);
        set_item(4, QString("%1 %2").arg(currency_, QString::number(h.market_value, 'f', 2)));
    }
}

// ── Index creation ────────────────────────────────────────────────────────────

void CustomIndexView::create_index() {
    const QString name = name_edit_->text().trimmed();
    if (name.isEmpty())
        return;

    const double base = base_edit_->text().toDouble();
    if (base <= 0.0) {
        create_status_->setText("Base value must be positive.");
        create_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
        create_status_->show();
        return;
    }

    // Collect checked constituents
    QVector<CustomIndexConstituent> constituents;
    for (int r = 0; r < const_table_->rowCount(); ++r) {
        const auto* chk = const_table_->item(r, 0);
        if (!chk || chk->checkState() != Qt::Checked)
            continue;

        const QString symbol = const_table_->item(r, 1) ? const_table_->item(r, 1)->text() : QString();
        if (symbol.isEmpty())
            continue;

        // Find matching holding
        for (const auto& h : summary_.holdings) {
            if (h.symbol == symbol) {
                CustomIndexConstituent c;
                c.symbol = h.symbol;
                c.weight = h.weight;
                c.price_at_create = h.current_price;
                constituents.append(c);
                break;
            }
        }
    }

    if (constituents.isEmpty()) {
        create_status_->setText("Select at least one constituent.");
        create_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
        create_status_->show();
        return;
    }

    CustomIndex idx;
    idx.name = name;
    idx.method = method_cb_->currentText();
    idx.base_value = base;
    idx.portfolio_id = summary_.portfolio.id;
    idx.constituents = constituents;

    auto result = CustomIndexRepository::instance().create(idx);
    if (result.is_err()) {
        const QString msg = QString::fromStdString(result.error());
        LOG_ERROR("CustomIndex", "Failed to create index: " + msg);
        create_status_->setText("Error: " + msg);
        create_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
        create_status_->show();
        return;
    }

    LOG_INFO("CustomIndex",
             QString("Created index '%1' (%2) with %3 constituents").arg(name, idx.method).arg(constituents.size()));

    // Save initial value
    const QString index_id = result.value();
    const double current_val = compute_index_value(idx);
    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    CustomIndexRepository::instance().save_value(index_id, today, current_val);

    create_status_->setText(QString("Index '%1' created successfully.").arg(name));
    create_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE()));
    create_status_->show();
    name_edit_->clear();

    load_indices();
    tabs_->setCurrentIndex(1); // switch to MY INDICES
}

// ── Index list ────────────────────────────────────────────────────────────────

void CustomIndexView::load_indices() {
    auto result = CustomIndexRepository::instance().list_all();
    if (result.is_err()) {
        LOG_WARN("CustomIndex", "Failed to load indices: " + QString::fromStdString(result.error()));
        return;
    }
    loaded_indices_ = result.value();

    const bool has_indices = !loaded_indices_.isEmpty();
    list_empty_msg_->setVisible(!has_indices);
    delete_btn_->setVisible(has_indices);

    index_list_table_->setRowCount(static_cast<int>(loaded_indices_.size()));

    for (int r = 0; r < static_cast<int>(loaded_indices_.size()); ++r) {
        const auto& idx = loaded_indices_[r];

        const double current_val = CustomIndexRepository::instance().latest_value(idx.id);
        const double base = idx.base_value;
        const double change_pct = base > 0.0 ? ((current_val - base) / base * 100.0) : 0.0;
        const bool positive = change_pct >= 0.0;

        index_list_table_->setRowHeight(r, 30);

        auto set_item = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            index_list_table_->setItem(r, col, item);
        };

        set_item(0, idx.name, ui::colors::CYAN);
        set_item(1, idx.method);
        set_item(2, QString::number(base, 'f', 2));
        set_item(3, current_val > 0.0 ? QString::number(current_val, 'f', 2) : "--");
        set_item(4, current_val > 0.0 ? QString("%1%2%").arg(positive ? "+" : "").arg(change_pct, 0, 'f', 2) : "--",
                 positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set_item(5, idx.created_at.left(10));
    }
}

void CustomIndexView::delete_selected_index() {
    const int row = index_list_table_->currentRow();
    if (row < 0 || row >= static_cast<int>(loaded_indices_.size()))
        return;

    const auto& idx = loaded_indices_[row];

    auto r = CustomIndexRepository::instance().remove(idx.id);
    if (r.is_err()) {
        LOG_ERROR("CustomIndex", "Delete failed: " + QString::fromStdString(r.error()));
        return;
    }
    LOG_INFO("CustomIndex", "Deleted index: " + idx.name);
    perf_stack_->setCurrentIndex(0);
    load_indices();
}

// ── Performance chart ─────────────────────────────────────────────────────────

void CustomIndexView::show_index_performance(const QString& index_id, const QString& name) {
    auto result = CustomIndexRepository::instance().get_values(index_id, 365);

    if (result.is_err() || result.value().isEmpty()) {
        perf_title_->setText(QString("PERFORMANCE — %1  (no data)").arg(name));
        perf_stack_->setCurrentIndex(0);
        return;
    }

    const auto& values = result.value();

    auto* series = new QLineSeries;
    series->setName(name);
    series->setColor(QColor(QString(ui::colors::CYAN())));
    QPen pen{QColor(QString(ui::colors::CYAN()))};
    pen.setWidth(2);
    series->setPen(pen);

    for (const auto& v : values) {
        const QDateTime dt = QDateTime::fromString(v.date, "yyyy-MM-dd");
        series->append(dt.toMSecsSinceEpoch(), v.value);
    }

    auto* chart = perf_chart_view_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes)
        chart->removeAxis(ax);

    chart->addSeries(series);
    chart->setTitle(QString());

    auto* x_axis = new QDateTimeAxis;
    x_axis->setTickCount(6);
    x_axis->setFormat("MMM yy");
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    chart->addAxis(x_axis, Qt::AlignBottom);
    series->attachAxis(x_axis);

    auto* y_axis = new QValueAxis;
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    y_axis->setLabelFormat("%.0f");
    chart->addAxis(y_axis, Qt::AlignLeft);
    series->attachAxis(y_axis);

    perf_title_->setText(QString("PERFORMANCE — %1").arg(name));
    perf_stack_->setCurrentIndex(1);
}

// ── Index value computation ───────────────────────────────────────────────────

double CustomIndexView::compute_index_value(const CustomIndex& idx) const {
    if (idx.constituents.isEmpty())
        return idx.base_value;

    // Build a map from symbol → current price from live holdings
    QHash<QString, double> price_map;
    for (const auto& h : summary_.holdings) {
        price_map[h.symbol] = h.current_price;
    }

    const int n = idx.constituents.size();
    const QString method = idx.method;

    if (method == "Price Weighted") {
        // Sum of current prices / sum of creation prices * base
        double sum_cur = 0.0;
        double sum_base = 0.0;
        for (const auto& c : idx.constituents) {
            const double cur = price_map.value(c.symbol, c.price_at_create);
            sum_cur += cur;
            sum_base += c.price_at_create;
        }
        return sum_base > 0.0 ? (sum_cur / sum_base * idx.base_value) : idx.base_value;

    } else if (method == "Equal Weighted") {
        double ratio_sum = 0.0;
        for (const auto& c : idx.constituents) {
            if (c.price_at_create > 0.0) {
                const double cur = price_map.value(c.symbol, c.price_at_create);
                ratio_sum += cur / c.price_at_create;
            }
        }
        return (ratio_sum / n) * idx.base_value;

    } else if (method == "Geometric Mean") {
        double log_sum = 0.0;
        int valid = 0;
        for (const auto& c : idx.constituents) {
            if (c.price_at_create > 0.0) {
                const double cur = price_map.value(c.symbol, c.price_at_create);
                if (cur > 0.0) {
                    log_sum += std::log(cur / c.price_at_create);
                    ++valid;
                }
            }
        }
        return valid > 0 ? (std::exp(log_sum / valid) * idx.base_value) : idx.base_value;

    } else {
        // Market Cap Weighted / Float Adjusted / Fundamental / Modified / Factor / Risk Parity / Capped
        // All fall back to weight-based: sum(weight_i * price_ratio_i) * base
        double weighted_ratio = 0.0;
        double total_weight = 0.0;
        for (const auto& c : idx.constituents) {
            if (c.price_at_create > 0.0 && c.weight > 0.0) {
                const double cur = price_map.value(c.symbol, c.price_at_create);
                weighted_ratio += c.weight * (cur / c.price_at_create);
                total_weight += c.weight;
            }
        }
        return total_weight > 0.0 ? (weighted_ratio / total_weight * idx.base_value) : idx.base_value;
    }
}

} // namespace fincept::screens
