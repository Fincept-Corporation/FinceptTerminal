// src/screens/portfolio/PortfolioBlotter.cpp
#include "screens/portfolio/PortfolioBlotter.h"

#include "screens/portfolio/PortfolioSparkline.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#    include "datahub/DataHub.h"
#    include "datahub/DataHubMetaTypes.h"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMetaObject>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

static const QStringList kColumns = {"SYMBOL", "QTY",  "LAST", "AVG COST", "MKT VAL", "COST BASIS",
                                     "P&L",    "P&L%", "CHG%", "TREND",    "WT%"};

PortfolioBlotter::PortfolioBlotter(QWidget* parent) : QWidget(parent) {
    // Restore persisted page size before building the UI so the combo and
    // populate_table see the right value on first paint.
    {
        auto r = SettingsRepository::instance().get("portfolio.blotter.page_size");
        if (r.is_ok() && !r.value().isEmpty()) {
            bool ok = false;
            const int saved = r.value().toInt(&ok);
            // Only accept the four sanctioned page sizes — anything else
            // (corrupted setting, future value) silently falls back to 10.
            if (ok && (saved == 10 || saved == 20 || saved == 50 || saved == 100))
                page_size_ = saved;
        }
    }
    build_ui();
}

void PortfolioBlotter::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(kColumns.size());
    table_->setHorizontalHeaderLabels(kColumns);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->verticalHeader()->setVisible(false);
    table_->setFocusPolicy(Qt::NoFocus);

    // Column widths — stretch all columns evenly, no last-section special treatment
    auto* hdr = table_->horizontalHeader();
    hdr->setMinimumSectionSize(40);
    hdr->setStretchLastSection(false);
    hdr->setSortIndicatorShown(false);
    hdr->setSectionResizeMode(QHeaderView::Stretch);

    table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none;"
                                  "  font-size:11px; font-family:%3; gridline-color:transparent; }"
                                  "QTableWidget::item { padding:5px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:rgba(217,119,6,0.10); color:%6; }"
                                  "QTableWidget::item:hover { background:%7; }"
                                  "QScrollBar:vertical { width:5px; background:%1; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM(), ui::colors::AMBER(),
                                   ui::colors::BG_HOVER()));

    // Apply the header rule directly on the header widget — the global qApp
    // stylesheet (ThemeManager.cpp) defines QHeaderView::section with
    // text_dim color, and Qt resolves header sub-controls from the header's
    // OWN stylesheet, not the parent QTableWidget's. Setting it on the table
    // is silently overridden, which is why headers stayed dim grey.
    //
    // 1px BORDER_MED bottom rule (was 2px AMBER) — only the CommandBar should
    // hang off the brand rail; tables get a hairline so they don't compete.
    // 11px font (was 10px) to match the unified type scale.
    hdr->setStyleSheet(QString("QHeaderView::section { background:%1; color:%2; border:none;"
                               "  border-bottom:1px solid %3; border-right:1px solid %4;"
                               "  padding:5px 8px; font-size:11px; font-weight:700;"
                               "  letter-spacing:0.5px; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(),
                                ui::colors::BORDER_MED(), ui::colors::BORDER_DIM()));

    connect(hdr, &QHeaderView::sectionClicked, this, &PortfolioBlotter::on_header_clicked);
    connect(table_, &QTableWidget::cellClicked, this, &PortfolioBlotter::on_row_clicked);

    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &PortfolioBlotter::on_context_menu);

    layout->addWidget(table_, 1);

    // Pagination footer sits flush at the bottom of the panel.
    build_pagination_footer();
    layout->addWidget(footer_);
}

void PortfolioBlotter::build_pagination_footer() {
    footer_ = new QWidget(this);
    footer_->setObjectName("pfBlotterFooter");
    footer_->setFixedHeight(28);
    footer_->setStyleSheet(
        QString("#pfBlotterFooter { background:%1; border-top:1px solid %2; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* h = new QHBoxLayout(footer_);
    h->setContentsMargins(12, 0, 10, 0);
    h->setSpacing(8);

    // ── Left: "Showing X-Y of Z" ─────────────────────────────────────────────
    footer_status_ = new QLabel("Showing 0 of 0");
    footer_status_->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    h->addWidget(footer_status_);

    h->addStretch(1);

    // ── Center: nav buttons + page label ─────────────────────────────────────
    // All four buttons share the same neutral square style. Disabled state
    // dims to TEXT_DIM and removes hover hint.
    auto make_nav_btn = [](const QString& glyph, const QString& tip) {
        auto* b = new QPushButton(glyph);
        b->setFixedSize(22, 22);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setStyleSheet(
            QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                    "  font-size:11px; font-weight:700; }"
                    "QPushButton:hover:enabled { color:%3; border-color:%3; }"
                    "QPushButton:disabled { color:%4; border-color:%5; }")
                .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                     ui::colors::AMBER(), ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM()));
        return b;
    };

    btn_first_ = make_nav_btn(QStringLiteral("«"), "First page");  // «
    btn_prev_  = make_nav_btn(QStringLiteral("‹"), "Previous page"); // ‹
    h->addWidget(btn_first_);
    h->addWidget(btn_prev_);

    footer_page_label_ = new QLabel("Page 1 of 1");
    footer_page_label_->setMinimumWidth(80);
    footer_page_label_->setAlignment(Qt::AlignCenter);
    footer_page_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; background:transparent;")
            .arg(ui::colors::TEXT_PRIMARY()));
    h->addWidget(footer_page_label_);

    btn_next_ = make_nav_btn(QStringLiteral("›"), "Next page");  // ›
    btn_last_ = make_nav_btn(QStringLiteral("»"), "Last page");  // »
    h->addWidget(btn_next_);
    h->addWidget(btn_last_);

    h->addStretch(1);

    // ── Right: "Rows: [10 ▾]" ───────────────────────────────────────────────
    auto* rows_label = new QLabel("Rows:");
    rows_label->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent;")
            .arg(ui::colors::TEXT_TERTIARY()));
    h->addWidget(rows_label);

    page_size_combo_ = new QComboBox(footer_);
    page_size_combo_->setFixedHeight(22);
    page_size_combo_->setCursor(Qt::PointingHandCursor);
    page_size_combo_->addItem("10", 10);
    page_size_combo_->addItem("20", 20);
    page_size_combo_->addItem("50", 50);
    page_size_combo_->addItem("100", 100);
    page_size_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                "  padding:0 6px; font-size:11px; font-weight:700; min-width:48px; }"
                "QComboBox::drop-down { border:none; width:14px; }"
                "QComboBox QAbstractItemView { background:%1; color:%2;"
                "  selection-background-color:%4; selection-color:%5; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM(), ui::colors::AMBER()));
    // Set initial selection to the persisted (or default) value.
    for (int i = 0; i < page_size_combo_->count(); ++i) {
        if (page_size_combo_->itemData(i).toInt() == page_size_) {
            QSignalBlocker block(page_size_combo_);
            page_size_combo_->setCurrentIndex(i);
            break;
        }
    }
    h->addWidget(page_size_combo_);

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect(btn_first_, &QPushButton::clicked, this, [this]() {
        if (current_page_ != 1) { current_page_ = 1; populate_table(); }
    });
    connect(btn_prev_, &QPushButton::clicked, this, [this]() {
        if (current_page_ > 1) { --current_page_; populate_table(); }
    });
    connect(btn_next_, &QPushButton::clicked, this, [this]() {
        if (current_page_ < total_pages()) { ++current_page_; populate_table(); }
    });
    connect(btn_last_, &QPushButton::clicked, this, [this]() {
        const int last = total_pages();
        if (current_page_ != last) { current_page_ = last; populate_table(); }
    });
    connect(page_size_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const int new_size = page_size_combo_->itemData(idx).toInt();
                if (new_size <= 0 || new_size == page_size_)
                    return;
                // Preserve the topmost visible row across page-size changes:
                // figure out which row is at the top right now, then jump to
                // the page that contains it under the new page size.
                const int top_row_idx = (current_page_ - 1) * page_size_;
                page_size_ = new_size;
                current_page_ = (top_row_idx / page_size_) + 1;
                SettingsRepository::instance().set("portfolio.blotter.page_size",
                                                   QString::number(page_size_), "portfolio");
                populate_table();
            });
}

void PortfolioBlotter::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    populate_table();
    fetch_sparklines(); // async — repaints sparkline cells when data arrives
}

void PortfolioBlotter::fetch_sparklines() {
    if (holdings_.isEmpty())
        return;

    // Mark all symbols as pending before the fetch.
    for (const auto& h : holdings_)
        sparkline_state_[h.symbol] = SparklineState::Pending;

    hub_resubscribe_sparklines();
}


void PortfolioBlotter::repaint_sparkline_cells() {
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* item = table_->item(r, 0);
        if (!item)
            continue;
        const QString sym = item->text();
        auto* w = qobject_cast<PortfolioSparkline*>(table_->cellWidget(r, 9));
        if (!w)
            continue;

        const auto state = sparkline_state_.value(sym, SparklineState::Failed);
        if (state == SparklineState::Loaded && sparkline_cache_.contains(sym)) {
            const auto& prices = sparkline_cache_[sym];
            w->set_data(prices);
            bool up = prices.size() >= 2 ? prices.last() >= prices.first() : true;
            w->set_color(QColor(up ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        } else {
            QVector<double> dash(6, 0.0);
            w->set_data(dash);
            w->set_color(QColor(ui::colors::BORDER_MED()));
        }
    }
}

void PortfolioBlotter::hub_resubscribe_sparklines() {
    auto& hub = datahub::DataHub::instance();
    // Holdings set is dynamic (portfolio edits replace it wholesale).
    hub.unsubscribe(this);
    hub_active_ = false;

    if (holdings_.isEmpty())
        return;

    QStringList topics;
    topics.reserve(holdings_.size());
    for (const auto& h : holdings_) {
        const QString sym = h.symbol;
        const QString topic = QStringLiteral("market:sparkline:") + sym;
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<QVector<double>>())
                return;
            const auto prices = v.value<QVector<double>>();
            sparkline_cache_.insert(sym, prices);
            sparkline_state_[sym] = SparklineState::Loaded;
            repaint_sparkline_cells();
        });
    }
    // force=true: holdings set changes on portfolio edits; bypass the sparkline
    // min_interval so the row reflects the new symbol without waiting 30s.
    hub.request(topics, /*force=*/true);
    hub_active_ = true;
}

void PortfolioBlotter::hub_unsubscribe_all() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}


void PortfolioBlotter::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* item = table_->item(r, 0);
        if (item && item->text() == symbol) {
            table_->selectRow(r);
            return;
        }
    }
}

void PortfolioBlotter::on_header_clicked(int section) {
    // Map column index to SortColumn
    portfolio::SortColumn col;
    switch (section) {
        case 0:
            col = portfolio::SortColumn::Symbol;
            break;
        case 2:
            col = portfolio::SortColumn::Price;
            break;
        case 4:
            col = portfolio::SortColumn::MarketValue;
            break;
        case 6:
            col = portfolio::SortColumn::Pnl;
            break;
        case 7:
            col = portfolio::SortColumn::PnlPct;
            break;
        case 8:
            col = portfolio::SortColumn::Change;
            break;
        case 10:
            col = portfolio::SortColumn::Weight;
            break;
        default:
            return;
    }

    if (col == sort_col_) {
        sort_dir_ = (sort_dir_ == portfolio::SortDirection::Asc) ? portfolio::SortDirection::Desc
                                                                 : portfolio::SortDirection::Asc;
    } else {
        sort_col_ = col;
        sort_dir_ = portfolio::SortDirection::Desc;
    }

    emit sort_changed(sort_col_, sort_dir_);
    // Sorting changes which rows are at the top — jump to page 1 so users
    // see the new "highest" rows instead of staying on a deep page that may
    // now contain unrelated content.
    current_page_ = 1;
    populate_table();
}

void PortfolioBlotter::on_row_clicked(int row, int) {
    // Row index is into the current page slice (paged_view), not sorted_.
    const auto page = paged_view();
    if (row >= 0 && row < page.size()) {
        selected_symbol_ = page[row].symbol;
        emit symbol_selected(selected_symbol_);
    }
}

// ── Pagination helpers ──────────────────────────────────────────────────────

QVector<portfolio::HoldingWithQuote> PortfolioBlotter::visible_view() const {
    // Apply text filter + sector filter on top of the already-sorted set.
    // We rebuild this every populate_table — the dataset is bounded (a few
    // hundred holdings at most for a real portfolio).
    QVector<portfolio::HoldingWithQuote> out;
    out.reserve(sorted_.size());
    const QString needle = filter_text_.toLower();
    const bool has_sector = !sector_symbols_.isEmpty();

    QSet<QString> sector_set;
    if (has_sector) {
        for (const auto& s : sector_symbols_)
            sector_set.insert(s.toLower());
    }

    for (const auto& h : sorted_) {
        if (!needle.isEmpty() && !h.symbol.toLower().contains(needle))
            continue;
        if (has_sector && !sector_set.contains(h.symbol.toLower()))
            continue;
        out.append(h);
    }
    return out;
}

QVector<portfolio::HoldingWithQuote> PortfolioBlotter::paged_view() const {
    const auto all = visible_view();
    if (page_size_ <= 0)
        return all;
    const int start = (current_page_ - 1) * page_size_;
    if (start >= all.size())
        return {};
    const int end = std::min<int>(start + page_size_, all.size());
    return all.mid(start, end - start);
}

int PortfolioBlotter::total_pages() const {
    const int total = visible_view().size();
    if (total == 0 || page_size_ <= 0)
        return 1;
    return (total + page_size_ - 1) / page_size_;
}

void PortfolioBlotter::clamp_current_page() {
    const int last = total_pages();
    if (current_page_ < 1) current_page_ = 1;
    if (current_page_ > last) current_page_ = last;
}

void PortfolioBlotter::update_pagination_controls() {
    if (!footer_status_)
        return;

    const int total = visible_view().size();
    const int last_page = total_pages();
    const int start_idx = (total == 0) ? 0 : (current_page_ - 1) * page_size_ + 1;
    const int end_idx = std::min<int>(current_page_ * page_size_, total);

    if (total == 0) {
        footer_status_->setText("No positions");
    } else {
        footer_status_->setText(QString("Showing %1-%2 of %3").arg(start_idx).arg(end_idx).arg(total));
    }

    footer_page_label_->setText(QString("Page %1 of %2").arg(current_page_).arg(last_page));

    btn_first_->setEnabled(current_page_ > 1);
    btn_prev_->setEnabled(current_page_ > 1);
    btn_next_->setEnabled(current_page_ < last_page);
    btn_last_->setEnabled(current_page_ < last_page);
}

void PortfolioBlotter::populate_table() {
    // Clean up old sparkline cell widgets before repopulating (prevents memory leak)
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* w = table_->cellWidget(r, 9);
        if (w) {
            table_->removeCellWidget(r, 9);
            w->deleteLater();
        }
    }

    sorted_ = holdings_;

    // Sort
    bool asc = (sort_dir_ == portfolio::SortDirection::Asc);
    auto cmp = [&](const portfolio::HoldingWithQuote& a, const portfolio::HoldingWithQuote& b) {
        double va = 0, vb = 0;
        switch (sort_col_) {
            case portfolio::SortColumn::Symbol:
                return asc ? a.symbol < b.symbol : a.symbol > b.symbol;
            case portfolio::SortColumn::Price:
                va = a.current_price;
                vb = b.current_price;
                break;
            case portfolio::SortColumn::Change:
                va = a.day_change_percent;
                vb = b.day_change_percent;
                break;
            case portfolio::SortColumn::Pnl:
                va = a.unrealized_pnl;
                vb = b.unrealized_pnl;
                break;
            case portfolio::SortColumn::PnlPct:
                va = a.unrealized_pnl_percent;
                vb = b.unrealized_pnl_percent;
                break;
            case portfolio::SortColumn::Weight:
                va = a.weight;
                vb = b.weight;
                break;
            case portfolio::SortColumn::MarketValue:
                va = a.market_value;
                vb = b.market_value;
                break;
        }
        return asc ? va < vb : va > vb;
    };
    std::stable_sort(sorted_.begin(), sorted_.end(), cmp);

    // Compute the current page's row slice — the rest of the function only
    // populates the visible window. clamp_current_page handles the case where
    // a filter/sort change shrinks the view below the current page index.
    clamp_current_page();
    const auto page_rows = paged_view();
    table_->setRowCount(page_rows.size());

    for (int r = 0; r < page_rows.size(); ++r) {
        const auto& h = page_rows[r];
        table_->setRowHeight(r, 28);

        auto set_cell = [&](int col, const QString& text, const char* color = nullptr,
                            Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(align);
            // QTableWidgetItem ignores QTableWidget's stylesheet `color` and
            // falls back to QPalette::Text (which renders as black on Windows).
            // Always set an explicit foreground.
            item->setForeground(QColor(color ? color : ui::colors::TEXT_PRIMARY()));
            table_->setItem(r, col, item);
        };

        // SYMBOL
        set_cell(0, h.symbol, ui::colors::CYAN, Qt::AlignLeft | Qt::AlignVCenter);

        // QTY
        set_cell(1, format_value(h.quantity, h.quantity == std::floor(h.quantity) ? 0 : 2));

        // LAST (price)
        set_cell(2, format_value(h.current_price));

        // AVG COST
        set_cell(3, format_value(h.avg_buy_price));

        // MKT VAL
        set_cell(4, format_value(h.market_value), ui::colors::WARNING);

        // COST BASIS
        set_cell(5, format_value(h.cost_basis));

        // P&L
        const char* pnl_color = h.unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(6, QString("%1%2").arg(h.unrealized_pnl >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl)),
                 pnl_color);

        // P&L%
        set_cell(
            7,
            QString("%1%2%").arg(h.unrealized_pnl_percent >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl_percent)),
            pnl_color);

        // CHG%
        const char* chg_color = h.day_change_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(8, QString("%1%2%").arg(h.day_change_percent >= 0 ? "+" : "").arg(format_value(h.day_change_percent)),
                 chg_color);

        // TREND — show loaded data, a pending shimmer, or a failure dash
        auto* sparkline = new PortfolioSparkline(0, 0);
        sparkline->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        const auto state = sparkline_state_.value(h.symbol, SparklineState::Pending);
        if (state == SparklineState::Loaded && sparkline_cache_.contains(h.symbol)) {
            const auto& prices = sparkline_cache_[h.symbol];
            sparkline->set_data(prices);
            bool up = prices.size() >= 2 ? prices.last() >= prices.first() : h.day_change >= 0;
            sparkline->set_color(QColor(up ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        } else if (state == SparklineState::Failed) {
            // Show flat dash line in muted color to indicate unavailable data
            QVector<double> dash(6, 0.0);
            sparkline->set_data(dash);
            sparkline->set_color(QColor(ui::colors::BORDER_MED()));
        } else {
            // Pending: subtle animated-looking flat line in dim color
            QVector<double> pending(8, h.current_price > 0 ? h.current_price : 1.0);
            sparkline->set_data(pending);
            sparkline->set_color(QColor(ui::colors::TEXT_TERTIARY()));
        }
        table_->setCellWidget(r, 9, sparkline);

        // WT%
        set_cell(10, QString("%1%").arg(format_value(h.weight, 1)));

        // Highlight selected row
        if (h.symbol == selected_symbol_) {
            table_->selectRow(r);
        }
    }

    // Footer status + nav button enabled-state must reflect what we just rendered.
    update_pagination_controls();
}

QString PortfolioBlotter::format_value(double v, int dp) const {
    return QString::number(v, 'f', dp);
}

void PortfolioBlotter::set_filter(const QString& text) {
    filter_text_ = text.trimmed().toLower();
    apply_filter();
}

void PortfolioBlotter::set_sector_filter(const QStringList& symbols) {
    sector_symbols_ = symbols;
    apply_filter();
}

void PortfolioBlotter::apply_filter() {
    // With pagination, the table only contains the current page slice — there
    // are no hidden rows to toggle. Filter changes shrink the visible_view, so
    // jump to page 1 and rebuild. visible_view() inside populate_table re-applies
    // both filter_text_ and sector_symbols_ from scratch.
    current_page_ = 1;
    populate_table();
}

void PortfolioBlotter::on_context_menu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    const auto page = paged_view();
    if (row < 0 || row >= page.size())
        return;

    const QString symbol = page[row].symbol;

    QMenu menu(this);
    menu.setStyleSheet(QString("QMenu { background:%1; color:%2; border:1px solid %3; padding:4px; }"
                               "QMenu::item { padding:6px 20px 6px 12px; font-size:11px; }"
                               "QMenu::item:selected { background:%4; color:%5; }"
                               "QMenu::separator { height:1px; background:%3; margin:3px 0; }")
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                                ui::colors::AMBER_DIM(), ui::colors::AMBER()));

    auto* sym_label = menu.addAction(symbol);
    sym_label->setEnabled(false);
    sym_label->setFont([&] {
        QFont f;
        f.setBold(true);
        f.setPointSize(10);
        return f;
    }());
    menu.addSeparator();

    auto* edit_act = menu.addAction("Edit Transaction");
    auto* delete_act = menu.addAction("Close / Delete Position");

    edit_act->setIcon(QIcon());
    delete_act->setIcon(QIcon());

    // Style delete action in red
    delete_act->setData("danger");
    menu.setStyleSheet(menu.styleSheet() +
                       QString("QMenu::item[data='danger'] { color:%1; }").arg(ui::colors::NEGATIVE()));

    connect(edit_act, &QAction::triggered, this, [this, symbol]() { emit edit_transaction_requested(symbol); });
    connect(delete_act, &QAction::triggered, this, [this, symbol]() { emit delete_position_requested(symbol); });

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void PortfolioBlotter::refresh_theme() {
    const QString bsz = QString::number(ui::fonts::font_px(0));
    const QString hsz = QString::number(ui::fonts::font_px(-2));
    table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none;"
                                  "  font-size:" +
                                  bsz +
                                  "px; font-family:%3; gridline-color:transparent; }"
                                  "QTableWidget::item { padding:5px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:rgba(217,119,6,0.10); color:%6; }"
                                  "QTableWidget::item:hover { background:%7; }"
                                  "QScrollBar:vertical { width:5px; background:%1; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM(), ui::colors::AMBER_DIM(), ui::colors::AMBER(),
                                   ui::colors::BG_HOVER()));
    // See note in build constructor — header rule must live on the header
    // widget itself or the global qApp stylesheet wins.
    table_->horizontalHeader()->setStyleSheet(
        QString("QHeaderView::section { background:%1; color:%2; border:none;"
                "  border-bottom:1px solid %3; border-right:1px solid %4;"
                "  padding:5px 8px; font-size:" + hsz +
                "px; font-weight:700; letter-spacing:0.5px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_MED(), ui::colors::BORDER_DIM()));
    populate_table();
}

} // namespace fincept::screens
