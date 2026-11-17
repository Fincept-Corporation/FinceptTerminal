// src/screens/portfolio/views/CustomIndexView.cpp
#include "screens/portfolio/views/CustomIndexView.h"

#include "core/logging/Logger.h"
#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// Method keys (English) — used as canonical identifiers in storage and the
// compute_index_value switch. Display strings are looked up via tr() so the
// combo box label is localised while the persisted method name stays stable.
// Only the behaviours PortfolioService::compute_custom_index_value() actually
// implements are offered. The seven names it used to list (Market Cap Weighted,
// Float Adjusted, Fundamental Weighted, Modified Market Cap, Factor Weighted,
// Risk Parity, Capped Weighted) all computed the same portfolio-weighted index;
// indices already stored under them still compute through that fallback.
static const QStringList kMethods = {
    "Price Weighted",
    "Portfolio Weighted",
    "Equal Weighted",
    "Geometric Mean",
};

static QStringList method_descriptions() {
    return {
        QObject::tr("Dow Jones style — sum of prices / divisor"),
        QObject::tr("Weighted by each holding's portfolio weight when the index was created"),
        QObject::tr("Equal weight per constituent (1/N)"),
        QObject::tr("Geometric average of price relatives"),
    };
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString table_style() {
    return QString("QTableWidget { background:%1; color:%2; border:none; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                   "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                   "QTableWidget::item:selected { background:%4; color:%5; }"
                   "QHeaderView::section { background:%6; color:%7; border:none;"
                   "  border-bottom:2px solid %8; padding:4px 8px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; }")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM(),
             ui::colors::TEXT_ON_ACCENT(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER());
}

// Table row height: `design_h` at the default font size, growing with it.
static int row_height(int design_h) {
    return std::max(design_h, ui::fonts::font_px(-3) + design_h - 11);
}

// The page stack inside `tabs` (QTabWidget's own child), so a tab page built there is
// not moved — and re-styled as a whole — by addTab(). `fallback` when it is not found.
static QWidget* civ_page_parent(QTabWidget* tabs, QWidget* fallback) {
    if (!tabs)
        return fallback;
    QWidget* stack = tabs->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                     Qt::FindDirectChildrenOnly);
    return stack ? stack : fallback;
}

static QString input_style() {
    return QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                   "QLineEdit:focus { border-color:%4; }")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER());
}

// ── Constructor ───────────────────────────────────────────────────────────────

CustomIndexView::CustomIndexView(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_indices();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
    // Currency / number format changed: re-format the rows already held (no database
    // read); the PERFORMANCE tiles redraw on the tab's next activation, its chart now.
    ui::formatting::on_format_changed(this, [this]() {
        update_constituents();
        render_index_list();
        perf_dirty_ = true;
        if (perf_chart_)
            perf_chart_->update();
    });
}

void CustomIndexView::refresh_theme() {
    // Table item colours are data (the chart and tiles paint from the live theme): re-render
    // the rows held once an appearance change has settled — no database read.
    update_constituents();
    render_index_list();
}

// ── UI construction ───────────────────────────────────────────────────────────

void CustomIndexView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget(this);
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    create_tab_index_ = tabs_->addTab(build_create_panel(), tr("CREATE INDEX"));
    my_indices_tab_index_ = tabs_->addTab(build_index_list_panel(), tr("MY INDICES"));
    performance_tab_index_ = tabs_->addTab(build_performance_panel(), tr("PERFORMANCE"));
    // PERFORMANCE draws when it is shown (local data, no click needed elsewhere).
    connect(tabs_, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == performance_tab_index_ && perf_dirty_)
            refresh_performance();
    });

    layout->addWidget(tabs_);
}

QWidget* CustomIndexView::build_create_panel() {
    auto* w = new QWidget(civ_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    create_title_ = new QLabel(tr("CREATE CUSTOM INDEX"));
    create_title_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(create_title_);

    // Config row
    auto* config = new QHBoxLayout;
    config->setSpacing(8);

    auto add_field = [&](const QString& label, QLabel*& slot, QWidget* widget) {
        slot = new QLabel(label);
        slot->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
        config->addWidget(slot);
        config->addWidget(widget);
    };

    name_edit_ = new QLineEdit;
    name_edit_->setPlaceholderText(tr("My Custom Index"));
    name_edit_->setFixedWidth(160);
    name_edit_->setMinimumHeight(24);
    name_edit_->setStyleSheet(input_style());
    add_field(tr("NAME:"), name_field_label_, name_edit_);

    method_cb_ = new QComboBox;
    // Add localized method names; itemData() holds the English key for storage.
    for (const QString& m : kMethods)
        method_cb_->addItem(tr(m.toUtf8().constData()), m);
    method_cb_->setMinimumHeight(24);
    method_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    add_field(tr("METHOD:"), method_field_label_, method_cb_);

    base_edit_ = new QLineEdit("1000");
    base_edit_->setFixedWidth(80);
    base_edit_->setMinimumHeight(24);
    base_edit_->setStyleSheet(input_style());
    add_field(tr("BASE:"), base_field_label_, base_edit_);

    config->addStretch();

    create_btn_ = new QPushButton(tr("CREATE INDEX"));
    create_btn_->setMinimumHeight(26);
    create_btn_->setCursor(Qt::PointingHandCursor);
    create_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%3; border:none;"
                                       "  padding:0 16px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%2; }")
                                   .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::TEXT_ON_ACCENT()));
    connect(create_btn_, &QPushButton::clicked, this, &CustomIndexView::create_index);
    config->addWidget(create_btn_);

    layout->addLayout(config);

    // Method description
    const auto descs = method_descriptions();
    method_desc_ = new QLabel(descs.value(0));
    method_desc_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    connect(method_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const auto d = method_descriptions();
        if (idx >= 0 && idx < d.size())
            method_desc_->setText(d[idx]);
    });
    layout->addWidget(method_desc_);

    // Status label
    create_status_ = new QLabel;
    create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::POSITIVE()));
    create_status_->hide();
    layout->addWidget(create_status_);

    // Constituents table header
    const_title_ = new QLabel(tr("CONSTITUENTS (from portfolio holdings)"));
    const_title_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(const_title_);

    const_table_ = new QTableWidget;
    const_table_->setColumnCount(5);
    const_table_->setHorizontalHeaderLabels({tr("INCLUDE"), tr("SYMBOL"), tr("PRICE"), tr("WEIGHT"), tr("MKT VALUE")});
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
    auto* w = new QWidget(civ_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    // Header row
    auto* header_row = new QHBoxLayout;
    indices_title_ = new QLabel(tr("MY CUSTOM INDICES"));
    indices_title_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    header_row->addWidget(indices_title_);
    header_row->addStretch();

    delete_btn_ = new QPushButton(tr("DELETE SELECTED"));
    delete_btn_->setProperty("fincept_agent_risk", "delete");
    delete_btn_->setMinimumHeight(26);
    delete_btn_->setCursor(Qt::PointingHandCursor);
    delete_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       "  padding:0 14px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; }"
                                       "QPushButton:hover { background:%1; }")
                                   .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_ON_ACCENT()));
    connect(delete_btn_, &QPushButton::clicked, this, &CustomIndexView::delete_selected_index);
    header_row->addWidget(delete_btn_);
    layout->addLayout(header_row);

    index_list_table_ = new QTableWidget;
    index_list_table_->setColumnCount(6);
    index_list_table_->setHorizontalHeaderLabels(
        {tr("NAME"), tr("METHOD"), tr("BASE"), tr("CURRENT VALUE"), tr("CHANGE"), tr("CREATED")});
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
        perf_index_id_ = loaded_indices_[row].id;
        perf_dirty_ = true;
        if (tabs_->currentIndex() == performance_tab_index_)
            refresh_performance();
        else
            tabs_->setCurrentIndex(performance_tab_index_); // draws on activation
    });

    layout->addWidget(index_list_table_, 1);

    list_empty_ = ui::EmptyState::create(
        ui::EmptyState::no_data_spec(tr("No custom index yet"),
                                     tr("Build one from your holdings on CREATE INDEX: its level is recorded on "
                                        "every portfolio refresh."),
                                     tr("CREATE INDEX")),
        w);
    connect(list_empty_, &ui::EmptyState::primary_clicked, this,
            [this]() { tabs_->setCurrentIndex(create_tab_index_); });
    list_empty_->setVisible(false);
    layout->addWidget(list_empty_, 1);

    return w;
}

QWidget* CustomIndexView::build_performance_panel() {
    auto* w = new QWidget(civ_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    perf_title_ = new QLabel(tr("INDEX PERFORMANCE"), w);
    perf_title_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    head->addWidget(perf_title_, 1);
    perf_index_label_ = new QLabel(tr("INDEX:"), w);
    perf_index_label_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY()));
    head->addWidget(perf_index_label_);
    perf_index_cb_ = new QComboBox(w);
    perf_index_cb_->setMinimumHeight(24);
    perf_index_cb_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    perf_index_cb_->setMinimumContentsLength(14);
    perf_index_cb_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:0 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; }"
                "QComboBox::drop-down { border:none; }"
                "QComboBox QAbstractItemView { background:%1; color:%2; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    connect(perf_index_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
        if (i < 0)
            return;
        perf_index_id_ = perf_index_cb_->itemData(i).toString();
        perf_dirty_ = true;
        refresh_performance();
    });
    head->addWidget(perf_index_cb_);
    layout->addLayout(head);

    perf_stack_ = new QStackedWidget(w);

    // Page 0 — designed empty state (no index yet / one level so far / read failure)
    perf_empty_ = new ui::EmptyState(perf_stack_);
    connect(perf_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        if (loaded_indices_.isEmpty()) {
            tabs_->setCurrentIndex(create_tab_index_);
        } else {
            perf_dirty_ = true; // RETRY a failed read
            refresh_performance();
        }
    });
    perf_stack_->addWidget(perf_empty_);

    // Page 1 — tiles + chart
    auto* page = new QWidget(perf_stack_);
    auto* pv = new QVBoxLayout(page);
    pv->setContentsMargins(0, 0, 0, 0);
    pv->setSpacing(6);
    perf_kpis_ = new ui::KpiStrip(page);
    perf_kpis_->set_min_tile_width(ui::metrics::width(120));
    for (const auto& [k, l] : {std::pair{"base", tr("BASE")}, std::pair{"current", tr("CURRENT")},
                               std::pair{"change", tr("CHANGE")}, std::pair{"high", tr("HIGH")},
                               std::pair{"low", tr("LOW")}, std::pair{"since", tr("SINCE")}}) {
        auto* t = perf_kpis_->add_tile(QString::fromLatin1(k), l);
        t->set_compact(true);
    }
    pv->addWidget(perf_kpis_);
    perf_chart_ = new ui::TimeSeriesChart(page);
    perf_chart_->set_utc(true);
    perf_chart_->set_show_mode_buttons(false);
    perf_chart_->set_time_formatter(
        [](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    perf_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                     [](double v) { return ui::formatting::number(v, 2); });
    perf_chart_->set_export_name(QStringLiteral("custom_index"));
    pv->addWidget(perf_chart_, 1);
    perf_stack_->addWidget(page);

    layout->addWidget(perf_stack_, 1);
    return w;
}

void CustomIndexView::refresh_performance() {
    // The combo follows the indices (rebuilt only when ids / names changed).
    QString sig;
    for (const auto& idx : loaded_indices_)
        sig += idx.id + QLatin1Char('\x1f') + idx.name + QLatin1Char('\x1e');
    bool found = false;
    for (const auto& idx : loaded_indices_)
        if (idx.id == perf_index_id_)
            found = true;
    if (!found)
        perf_index_id_ = loaded_indices_.isEmpty() ? QString() : loaded_indices_.first().id;
    if (sig != perf_combo_sig_) {
        perf_combo_sig_ = sig;
        QSignalBlocker block(perf_index_cb_);
        perf_index_cb_->clear();
        for (const auto& idx : loaded_indices_)
            perf_index_cb_->addItem(idx.name, idx.id);
    }
    {
        QSignalBlocker block(perf_index_cb_);
        const int at = perf_index_cb_->findData(perf_index_id_);
        if (at >= 0 && perf_index_cb_->currentIndex() != at)
            perf_index_cb_->setCurrentIndex(at);
    }
    perf_index_cb_->setEnabled(loaded_indices_.size() > 1);

    if (tabs_->currentIndex() != performance_tab_index_) {
        perf_dirty_ = true; // drawn on the next activation
        return;
    }
    perf_dirty_ = false;
    if (loaded_indices_.isEmpty()) {
        last_perf_index_name_.clear();
        perf_title_->setText(tr("INDEX PERFORMANCE"));
        perf_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No custom index yet"),
            tr("Build an index from your holdings on CREATE INDEX: its level is recorded on every portfolio "
               "refresh and charted here."),
            tr("CREATE INDEX")));
        perf_stack_->setCurrentIndex(0);
        return;
    }
    for (const auto& idx : loaded_indices_) {
        if (idx.id == perf_index_id_) {
            show_index_performance(idx.id, idx.name);
            break;
        }
    }
}

// ── Data population ───────────────────────────────────────────────────────────

void CustomIndexView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_constituents();
    // PortfolioService records each index's level on every summary refresh
    // (before it emits the summary), so this reads current values.
    load_indices();
}

void CustomIndexView::update_constituents() {
    // set_data() runs on every portfolio poll, so this table is rebuilt while
    // the user is still ticking constituents. Snapshot their choices first and
    // put them back \u2014 otherwise every unticked row silently re-ticked itself
    // roughly once a minute.
    QHash<QString, Qt::CheckState> prev_checks;
    for (int r = 0; r < const_table_->rowCount(); ++r) {
        const auto* sym_item = const_table_->item(r, 1);
        const auto* chk = const_table_->item(r, 0);
        if (sym_item && chk)
            prev_checks.insert(sym_item->text(), chk->checkState());
    }

    const_table_->setRowCount(static_cast<int>(summary_.holdings.size()));

    for (int r = 0; r < static_cast<int>(summary_.holdings.size()); ++r) {
        const auto& h = summary_.holdings[r];
        const_table_->setRowHeight(r, row_height(28));

        // Native check box only (ticked by default for newly-seen symbols). A "\u2611"
        // text glyph next to it stayed ticked when the box was cleared.
        auto* check = new QTableWidgetItem(QString());
        check->setTextAlignment(Qt::AlignCenter);
        check->setFlags(check->flags() | Qt::ItemIsUserCheckable);
        check->setCheckState(prev_checks.value(h.symbol, Qt::Checked));
        const_table_->setItem(r, 0, check);

        auto set_item = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            const_table_->setItem(r, col, item);
        };

        set_item(1, h.symbol, ui::colors::CYAN);
        // A price in its own quote currency, with its unit: the column mixes listings.
        set_item(2, ui::formatting::price_with_symbol(h.current_price,
                                                      ui::formatting::instrument_for_symbol(h.symbol, h.price_currency)));
        set_item(3, ui::formatting::pct(h.weight, 1, false), ui::colors::AMBER);
        set_item(4, ui::formatting::money(h.market_value, currency_));
    }
}

// ── Index creation ────────────────────────────────────────────────────────────

void CustomIndexView::create_index() {
    const QString name = name_edit_->text().trimmed();
    if (name.isEmpty()) {
        create_status_->setText(tr("Enter a name for the index."));
        create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
        create_status_->show();
        name_edit_->setFocus();
        return;
    }

    const double base = base_edit_->text().toDouble();
    if (base <= 0.0) {
        create_status_->setText(tr("Base value must be positive."));
        create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
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
        create_status_->setText(tr("Select at least one constituent."));
        create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
        create_status_->show();
        return;
    }

    CustomIndex idx;
    idx.name = name;
    // Persist the English method key (currentData), not the localized display label.
    idx.method = method_cb_->currentData().toString();
    idx.base_value = base;
    idx.portfolio_id = summary_.portfolio.id;
    idx.constituents = constituents;

    auto result = CustomIndexRepository::instance().create(idx);
    if (result.is_err()) {
        const QString msg = QString::fromStdString(result.error());
        LOG_ERROR("CustomIndex", "Failed to create index: " + msg);
        create_status_->setText(tr("Error: %1").arg(msg));
        create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::NEGATIVE()));
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

    create_status_->setText(tr("Index '%1' created successfully.").arg(name));
    create_status_->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::POSITIVE()));
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
    latest_values_.clear();
    for (const auto& idx : loaded_indices_)
        latest_values_.insert(idx.id, CustomIndexRepository::instance().latest_value(idx.id));
    render_index_list();
    refresh_performance();
}

void CustomIndexView::render_index_list() {
    const bool has_indices = !loaded_indices_.isEmpty();
    list_empty_->setVisible(!has_indices);
    index_list_table_->setVisible(has_indices);
    delete_btn_->setVisible(has_indices);

    index_list_table_->setRowCount(static_cast<int>(loaded_indices_.size()));

    for (int r = 0; r < static_cast<int>(loaded_indices_.size()); ++r) {
        const auto& idx = loaded_indices_[r];

        const double current_val = latest_values_.value(idx.id, 0.0);
        const double base = idx.base_value;
        const double change_pct = base > 0.0 ? ((current_val - base) / base * 100.0) : 0.0;
        const bool positive = change_pct >= 0.0;

        index_list_table_->setRowHeight(r, row_height(30));

        auto set_item = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            index_list_table_->setItem(r, col, item);
        };

        set_item(0, idx.name, ui::colors::CYAN);
        // Indices stored under a retired method name were always computed by
        // portfolio weight — say so rather than repeat a name that promised more.
        set_item(1, kMethods.contains(idx.method) ? tr(idx.method.toUtf8().constData())
                                                  : tr("Portfolio Weighted (%1)").arg(idx.method));
        set_item(2, ui::formatting::number(base, 2));
        set_item(3, current_val > 0.0 ? ui::formatting::number(current_val, 2) : ui::formatting::placeholder());
        set_item(4, current_val > 0.0 ? ui::formatting::pct(change_pct, 2) : ui::formatting::placeholder(),
                 positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set_item(5, idx.created_at.left(10));
    }
}

void CustomIndexView::delete_selected_index() {
    const int row = index_list_table_->currentRow();
    if (row < 0 || row >= static_cast<int>(loaded_indices_.size()))
        return;

    const auto& idx = loaded_indices_[row];

    // Destructive and irreversible (the index and its whole value history go),
    // and it was one click away with no confirmation at all.
    const auto answer = QMessageBox::question(
        this, tr("Delete Custom Index"),
        tr("Delete the index \"%1\"?\n\nIts entire recorded value history will be removed. This cannot be undone.")
            .arg(idx.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    auto r = CustomIndexRepository::instance().remove(idx.id);
    if (r.is_err()) {
        LOG_ERROR("CustomIndex", "Delete failed: " + QString::fromStdString(r.error()));
        return;
    }
    LOG_INFO("CustomIndex", "Deleted index: " + idx.name);
    load_indices(); // PERFORMANCE moves to the next index (or says there is none)
}

// ── Performance chart ─────────────────────────────────────────────────────────

void CustomIndexView::show_index_performance(const QString& index_id, const QString& name) {
    auto result = CustomIndexRepository::instance().get_values(index_id, 365);
    last_perf_index_name_ = name;
    last_perf_has_data_ = result.is_ok() && result.value().size() >= 2;
    perf_title_->setText(tr("PERFORMANCE — %1").arg(name));

    double base = 0;
    QString created;
    for (const auto& idx : loaded_indices_) {
        if (idx.id == index_id) {
            base = idx.base_value;
            created = idx.created_at.left(10);
        }
    }

    if (result.is_err()) {
        LOG_WARN("CustomIndex", logf("Index history could not be read",
                                     {{"index", index_id}, {"error", QString::fromStdString(result.error())}}));
        perf_empty_->apply(ui::EmptyState::error_spec(tr("The recorded levels of “%1” could not be read.").arg(name),
                                                      tr("Could not read the index history")));
        perf_stack_->setCurrentIndex(0);
        return;
    }
    const auto& values = result.value();
    if (values.size() < 2) {
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            values.isEmpty() ? tr("No recorded level yet") : tr("One recorded level so far"),
            tr("This index has one recorded level so far; the history grows with every portfolio refresh."));
        spec.details = {{tr("Base"), ui::formatting::number(base, 2)}};
        if (!values.isEmpty())
            spec.details.append({tr("Level on %1").arg(pfmt::date(values.first().date)),
                                 ui::formatting::number(values.first().value, 2)});
        spec.details_heading = name.toUpper();
        spec.illustration = true;
        spec.illustration_shape = ui::LoadingSkeleton::Shape::Chart;
        perf_empty_->apply(spec);
        perf_stack_->setCurrentIndex(0);
        return;
    }

    QVector<QPointF> pts;
    pts.reserve(values.size());
    double hi = -1e300, lo = 1e300;
    for (const auto& v : values) {
        pts.append({pfmt::utc_ms(v.date), v.value});
        hi = std::max(hi, v.value);
        lo = std::min(lo, v.value);
    }
    const double last = values.last().value;
    const double ref = base > 0.0 ? base : values.first().value;
    const double change_pct = ref > 0.0 ? (last - ref) / ref * 100.0 : portfolio::kAbsent;

    using Tone = ui::StatTile::Tone;
    auto tile = [this](const char* k) { return perf_kpis_->tile(QString::fromLatin1(k)); };
    tile("base")->set_value(ui::formatting::number(base, 2));
    tile("base")->set_sub_label(created.isEmpty() ? QString() : tr("created %1").arg(pfmt::date(created)));
    tile("current")->set_value(ui::formatting::number(last, 2));
    tile("current")->set_sub_label(pfmt::date(values.last().date));
    tile("change")->set_value(std::isfinite(change_pct) ? ui::formatting::pct(change_pct, 2) : pfmt::dash());
    tile("change")->set_tone(!std::isfinite(change_pct) || change_pct == 0 ? Tone::Neutral
                             : change_pct > 0                            ? Tone::Positive
                                                                         : Tone::Negative);
    tile("change")->set_sub_label(base > 0.0 ? tr("vs base") : tr("vs first level"));
    tile("high")->set_value(ui::formatting::number(hi, 2));
    tile("low")->set_value(ui::formatting::number(lo, 2));
    tile("since")->set_value(pfmt::date(values.first().date, ui::formatting::DateStyle::Short));
    tile("since")->set_sub_label(tr("%n level(s) recorded", "", static_cast<int>(values.size())));

    ui::TimeSeriesChart::SeriesSpec spec;
    spec.name = name;
    spec.points = pts;
    spec.style = ui::TimeSeriesChart::Style::Area;
    spec.color = ui::TimeSeriesChart::ColorRole::Accent;
    spec.rebase = false;
    spec.formatter = [](double v) { return ui::formatting::number(v, 2); };
    perf_chart_->clear();
    perf_chart_->add_series(spec);
    perf_chart_->set_state(ui::TimeSeriesChart::State::Ready);
    perf_chart_->reset_x_range();
    perf_stack_->setCurrentIndex(1);
}

void CustomIndexView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CustomIndexView::retranslateUi() {
    if (tabs_) {
        if (create_tab_index_ >= 0)
            tabs_->setTabText(create_tab_index_, tr("CREATE INDEX"));
        if (my_indices_tab_index_ >= 0)
            tabs_->setTabText(my_indices_tab_index_, tr("MY INDICES"));
        if (performance_tab_index_ >= 0)
            tabs_->setTabText(performance_tab_index_, tr("PERFORMANCE"));
    }
    if (create_title_)
        create_title_->setText(tr("CREATE CUSTOM INDEX"));
    if (name_field_label_)
        name_field_label_->setText(tr("NAME:"));
    if (method_field_label_)
        method_field_label_->setText(tr("METHOD:"));
    if (base_field_label_)
        base_field_label_->setText(tr("BASE:"));
    if (name_edit_)
        name_edit_->setPlaceholderText(tr("My Custom Index"));
    if (create_btn_)
        create_btn_->setText(tr("CREATE INDEX"));
    if (const_title_)
        const_title_->setText(tr("CONSTITUENTS (from portfolio holdings)"));
    if (indices_title_)
        indices_title_->setText(tr("MY CUSTOM INDICES"));
    if (delete_btn_)
        delete_btn_->setText(tr("DELETE SELECTED"));
    if (list_empty_)
        list_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No custom index yet"),
            tr("Build one from your holdings on CREATE INDEX: its level is recorded on every portfolio refresh."),
            tr("CREATE INDEX")));
    if (perf_index_label_)
        perf_index_label_->setText(tr("INDEX:"));
    perf_dirty_ = true;
    if (tabs_ && tabs_->currentIndex() == performance_tab_index_)
        refresh_performance();

    if (const_table_)
        const_table_->setHorizontalHeaderLabels(
            {tr("INCLUDE"), tr("SYMBOL"), tr("PRICE"), tr("WEIGHT"), tr("MKT VALUE")});
    if (index_list_table_)
        index_list_table_->setHorizontalHeaderLabels(
            {tr("NAME"), tr("METHOD"), tr("BASE"), tr("CURRENT VALUE"), tr("CHANGE"), tr("CREATED")});

    // Repopulate method combo items: itemData() preserves the English key, label gets new tr().
    if (method_cb_) {
        const int saved = method_cb_->currentIndex();
        QSignalBlocker block(method_cb_);
        method_cb_->clear();
        for (const QString& m : kMethods)
            method_cb_->addItem(tr(m.toUtf8().constData()), m);
        if (saved >= 0 && saved < method_cb_->count())
            method_cb_->setCurrentIndex(saved);
    }
    // Re-render method description from the (now-localized) list
    if (method_desc_ && method_cb_) {
        const auto d = method_descriptions();
        const int idx = method_cb_->currentIndex();
        if (idx >= 0 && idx < d.size())
            method_desc_->setText(d[idx]);
    }

    // Re-render perf title if an index is currently shown
    if (perf_title_ && !last_perf_index_name_.isEmpty())
        perf_title_->setText(tr("PERFORMANCE — %1").arg(last_perf_index_name_));
    else if (perf_title_)
        perf_title_->setText(tr("INDEX PERFORMANCE"));
}

// ── Index value computation ───────────────────────────────────────────────────

double CustomIndexView::compute_index_value(const CustomIndex& idx) const {
    // The maths lives in PortfolioService so the per-refresh recorder (which
    // keeps the index history growing even when this view is closed) and the
    // creation path always agree on the level.
    return services::PortfolioService::compute_custom_index_value(idx, summary_);
}

} // namespace fincept::screens
