// src/screens/portfolio/PortfolioBlotter.cpp
#include "screens/portfolio/PortfolioBlotter.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/portfolio/PortfolioSparkline.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/AccountManager.h"
#include "trading/BrokerTopic.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMenu>
#include <QMetaObject>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

// Column source keys (English). Translated at runtime in build_ui() so
// lupdate can scan the literals here.
static const char* const kColumnKeys[] = {
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "SYMBOL"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "QTY"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "LAST"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "AVG COST"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "MKT VAL"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "COST BASIS"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "P&L"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "P&L%"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "CHG%"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "TREND"),
    QT_TRANSLATE_NOOP("fincept::screens::PortfolioBlotter", "WT%"),
};
static constexpr int kColumnCount = sizeof(kColumnKeys) / sizeof(kColumnKeys[0]);

// Named column indices — the layout was previously spelled out as bare
// integers in five places (populate_table, update_row_price,
// repaint_sparkline_cells, on_header_clicked), which is exactly how the live
// broker tick ended up writing the last price into the AVG COST column.
enum Column {
    kColSymbol = 0,
    kColQty = 1,
    kColLast = 2,
    kColAvgCost = 3,
    kColMktVal = 4,
    kColCostBasis = 5,
    kColPnl = 6,
    kColPnlPct = 7,
    kColChgPct = 8,
    kColTrend = 9,
    kColWeight = 10,
};
static_assert(kColWeight + 1 == kColumnCount, "Column enum and kColumnKeys are out of sync");

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
    table_->setColumnCount(kColumnCount);
    QStringList headers;
    headers.reserve(kColumnCount);
    for (int i = 0; i < kColumnCount; ++i)
        headers << tr(kColumnKeys[i]);
    table_->setHorizontalHeaderLabels(headers);
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

    // Markers must be contiguous and match the args 1:1 — QString::arg fills the
    // LOWEST marker present, so the old %5-less numbering pushed AMBER onto
    // :hover and dropped BG_HOVER with an "Argument missing" warning.
    table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none;"
                                  "  font-size:11px; font-family:%3; gridline-color:transparent; }"
                                  "QTableWidget::item { padding:5px 8px; border-bottom:1px solid %4; }"
                                  "QTableWidget::item:selected { background:rgba(217,119,6,0.10); color:%5; }"
                                  "QTableWidget::item:hover { background:%6; }"
                                  "QScrollBar:vertical { width:5px; background:%1; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM(), ui::colors::AMBER(), ui::colors::BG_HOVER()));

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
                           .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                                ui::colors::BORDER_DIM()));

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
    footer_->setStyleSheet(QString("#pfBlotterFooter { background:%1; border-top:1px solid %2; }")
                               .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* h = new QHBoxLayout(footer_);
    h->setContentsMargins(12, 0, 10, 0);
    h->setSpacing(8);

    // ── Left: "Showing X-Y of Z" ─────────────────────────────────────────────
    footer_status_ = new QLabel(tr("Showing 0 of 0"));
    footer_status_->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
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
        b->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                 "  font-size:11px; font-weight:700; }"
                                 "QPushButton:hover:enabled { color:%3; border-color:%3; }"
                                 "QPushButton:disabled { color:%4; border-color:%5; }")
                             .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
                                  ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM()));
        return b;
    };

    btn_first_ = make_nav_btn(QStringLiteral("«"), tr("First page"));
    btn_prev_ = make_nav_btn(QStringLiteral("‹"), tr("Previous page"));
    // Glyph-only buttons are opaque to screen readers.
    btn_first_->setAccessibleName(tr("First page"));
    btn_prev_->setAccessibleName(tr("Previous page"));
    h->addWidget(btn_first_);
    h->addWidget(btn_prev_);

    footer_page_label_ = new QLabel(tr("Page 1 of 1"));
    footer_page_label_->setMinimumWidth(80);
    footer_page_label_->setAlignment(Qt::AlignCenter);
    footer_page_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    h->addWidget(footer_page_label_);

    btn_next_ = make_nav_btn(QStringLiteral("›"), tr("Next page"));
    btn_last_ = make_nav_btn(QStringLiteral("»"), tr("Last page"));
    btn_next_->setAccessibleName(tr("Next page"));
    btn_last_->setAccessibleName(tr("Last page"));
    h->addWidget(btn_next_);
    h->addWidget(btn_last_);

    h->addStretch(1);

    // ── Right: "Rows: [10 ▾]" ───────────────────────────────────────────────
    footer_rows_label_ = new QLabel(tr("Rows:"));
    footer_rows_label_->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:600; background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
    h->addWidget(footer_rows_label_);

    page_size_combo_ = new QComboBox(footer_);
    page_size_combo_->setFixedHeight(22);
    page_size_combo_->setCursor(Qt::PointingHandCursor);
    page_size_combo_->addItem("10", 10);
    page_size_combo_->addItem("20", 20);
    page_size_combo_->addItem("50", 50);
    page_size_combo_->addItem("100", 100);
    page_size_combo_->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
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
        if (current_page_ != 1) {
            current_page_ = 1;
            populate_table();
        }
    });
    connect(btn_prev_, &QPushButton::clicked, this, [this]() {
        if (current_page_ > 1) {
            --current_page_;
            populate_table();
        }
    });
    connect(btn_next_, &QPushButton::clicked, this, [this]() {
        if (current_page_ < total_pages()) {
            ++current_page_;
            populate_table();
        }
    });
    connect(btn_last_, &QPushButton::clicked, this, [this]() {
        const int last = total_pages();
        if (current_page_ != last) {
            current_page_ = last;
            populate_table();
        }
    });
    connect(page_size_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const int new_size = page_size_combo_->itemData(idx).toInt();
        if (new_size <= 0 || new_size == page_size_)
            return;
        // Preserve the topmost visible row across page-size changes:
        // figure out which row is at the top right now, then jump to
        // the page that contains it under the new page size.
        const int top_row_idx = (current_page_ - 1) * page_size_;
        page_size_ = new_size;
        current_page_ = (top_row_idx / page_size_) + 1;
        SettingsRepository::instance().set("portfolio.blotter.page_size", QString::number(page_size_), "portfolio");
        populate_table();
    });
}

void PortfolioBlotter::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    invalidate_view_cache();
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
        auto* item = table_->item(r, kColSymbol);
        if (!item)
            continue;
        // Use the stashed raw symbol, not the cell text — the two are the same
        // today but the text is a display string and may gain decoration.
        const QString sym = item->data(Qt::UserRole).toString().isEmpty() ? item->text()
                                                                         : item->data(Qt::UserRole).toString();
        auto* w = qobject_cast<PortfolioSparkline*>(table_->cellWidget(r, kColTrend));
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

void PortfolioBlotter::hub_resubscribe_broker_quotes(const QString& broker_account_id) {
    if (broker_account_id.isEmpty() || holdings_.isEmpty())
        return;
    auto account = trading::AccountManager::instance().get_account(broker_account_id);
    if (account.broker_id.isEmpty() || account.state != trading::ConnectionState::Connected)
        return;

    auto& hub = datahub::DataHub::instance();
    const QString bid = account.broker_id;
    const QString aid = broker_account_id;

    for (const auto& h : holdings_) {
        // Indian stocks in yfinance format end with .NS or .BO
        QString broker_sym;
        if (h.symbol.endsWith(QStringLiteral(".NS")) || h.symbol.endsWith(QStringLiteral(".BO")))
            broker_sym = h.symbol.left(h.symbol.length() - 3);
        else
            continue; // not an Indian stock — skip broker subscription

        const QString topic = trading::broker_topic(bid, aid, QStringLiteral("quote"), broker_sym);
        const QString yf_sym = h.symbol;
        hub.subscribe(this, topic, [this, yf_sym](const QVariant& v) {
            if (!v.canConvert<trading::BrokerQuote>())
                return;
            const auto quote = v.value<trading::BrokerQuote>();
            update_row_price(yf_sym, quote.ltp, quote.change_pct);
        });
    }
}

void PortfolioBlotter::update_row_price(const QString& symbol, double ltp, double change_pct) {
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* item = table_->item(r, kColSymbol);
        if (!item || item->data(Qt::UserRole).toString() != symbol)
            continue;
        if (auto* last_item = table_->item(r, kColLast))
            last_item->setText(format_value(ltp));
        // Recalculate market value & P&L from holdings data
        const int idx = r + (current_page_ - 1) * page_size_;
        const auto& view = visible_view();
        if (idx >= 0 && idx < view.size()) {
            const auto& h = view[idx];
            const double mkt_val = ltp * h.quantity;
            const double pnl = mkt_val - h.cost_basis;
            const double pnl_pct = h.cost_basis > 0 ? (pnl / h.cost_basis) * 100.0 : 0.0;
            const char* pnl_color = pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
            if (auto* mv = table_->item(r, kColMktVal))
                mv->setText(format_value(mkt_val));
            if (auto* pnl_item = table_->item(r, kColPnl)) {
                pnl_item->setText(QString("%1%2").arg(pnl >= 0 ? "+" : "").arg(format_value(pnl)));
                // Recolour too: a tick that flipped a position from green to
                // red kept the stale green foreground until the next full poll.
                pnl_item->setForeground(QColor(pnl_color));
            }
            if (auto* pnl_pct_item = table_->item(r, kColPnlPct)) {
                pnl_pct_item->setText(
                    QString("%1%2%").arg(pnl_pct >= 0 ? "+" : "").arg(format_value(pnl_pct)));
                pnl_pct_item->setForeground(QColor(pnl_color));
            }
        }
        if (auto* chg = table_->item(r, kColChgPct)) {
            // Signed, matching how populate_table renders the same column.
            chg->setText(QString("%1%2%").arg(change_pct >= 0 ? "+" : "").arg(format_value(change_pct)));
            chg->setForeground(QColor(change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
        }
        break;
    }
}

void PortfolioBlotter::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* item = table_->item(r, kColSymbol);
        // Match on the stashed raw symbol (see populate_table) so this keeps
        // working if the symbol cell ever gains display decoration.
        if (item && item->data(Qt::UserRole).toString() == symbol) {
            table_->selectRow(r);
            return;
        }
    }
    // Not on this page: clear the highlight instead of leaving the previous
    // row looking selected while the selection actually lives elsewhere.
    table_->clearSelection();
}

void PortfolioBlotter::on_header_clicked(int section) {
    // Map column index to SortColumn
    portfolio::SortColumn col;
    switch (section) {
        case kColSymbol:
            col = portfolio::SortColumn::Symbol;
            break;
        case kColLast:
            col = portfolio::SortColumn::Price;
            break;
        case kColMktVal:
            col = portfolio::SortColumn::MarketValue;
            break;
        case kColPnl:
            col = portfolio::SortColumn::Pnl;
            break;
        case kColPnlPct:
            col = portfolio::SortColumn::PnlPct;
            break;
        case kColChgPct:
            col = portfolio::SortColumn::Change;
            break;
        case kColWeight:
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

void PortfolioBlotter::invalidate_view_cache() {
    view_cache_valid_ = false;
    view_cache_.clear();
}

const QVector<portfolio::HoldingWithQuote>& PortfolioBlotter::visible_view() const {
    if (view_cache_valid_)
        return view_cache_;

    // Apply text filter + sector filter on top of the already-sorted set.
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

    view_cache_ = std::move(out);
    view_cache_valid_ = true;
    return view_cache_;
}

QVector<portfolio::HoldingWithQuote> PortfolioBlotter::paged_view() const {
    const auto& all = visible_view();
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
    if (current_page_ < 1)
        current_page_ = 1;
    if (current_page_ > last)
        current_page_ = last;
}

void PortfolioBlotter::update_pagination_controls() {
    if (!footer_status_)
        return;

    const int total = visible_view().size();
    const int last_page = total_pages();
    const int start_idx = (total == 0) ? 0 : (current_page_ - 1) * page_size_ + 1;
    const int end_idx = std::min<int>(current_page_ * page_size_, total);

    if (total == 0) {
        footer_status_->setText(tr("No positions"));
    } else {
        footer_status_->setText(tr("Showing %1-%2 of %3").arg(start_idx).arg(end_idx).arg(total));
    }

    footer_page_label_->setText(tr("Page %1 of %2").arg(current_page_).arg(last_page));

    btn_first_->setEnabled(current_page_ > 1);
    btn_prev_->setEnabled(current_page_ > 1);
    btn_next_->setEnabled(current_page_ < last_page);
    btn_last_->setEnabled(current_page_ < last_page);
}

void PortfolioBlotter::populate_table() {
    // Remember the scroll offset so a background refresh does not yank the
    // table back to the top while the user is reading a row further down.
    const int prev_scroll = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;

    sorted_ = holdings_;
    invalidate_view_cache();

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

    // Sparklines are pooled per row: they own a cached QPixmap and re-creating
    // one per row on every 60 s poll threw that cache away. Only rows that are
    // disappearing release their widget.
    for (int r = page_rows.size(); r < table_->rowCount(); ++r) {
        if (auto* w = table_->cellWidget(r, kColTrend)) {
            table_->removeCellWidget(r, kColTrend);
            w->deleteLater();
        }
    }
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
        set_cell(kColSymbol, h.symbol, ui::colors::CYAN, Qt::AlignLeft | Qt::AlignVCenter);
        // Stash the raw symbol on the cell so live broker-price ticks
        // (update_row_price) can locate this row — its guard matches col-0
        // Qt::UserRole, which was never set, so live prices never updated.
        if (auto* sym_item = table_->item(r, kColSymbol))
            sym_item->setData(Qt::UserRole, h.symbol);

        // QTY
        set_cell(kColQty, format_value(h.quantity, h.quantity == std::floor(h.quantity) ? 0 : 2));

        // LAST (price)
        set_cell(kColLast, format_value(h.current_price));

        // AVG COST
        set_cell(kColAvgCost, format_value(h.avg_buy_price));

        // MKT VAL
        set_cell(kColMktVal, format_value(h.market_value), ui::colors::WARNING);

        // COST BASIS
        set_cell(kColCostBasis, format_value(h.cost_basis));

        // P&L
        const char* pnl_color = h.unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(kColPnl, QString("%1%2").arg(h.unrealized_pnl >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl)),
                 pnl_color);

        // P&L%
        set_cell(
            kColPnlPct,
            QString("%1%2%").arg(h.unrealized_pnl_percent >= 0 ? "+" : "").arg(format_value(h.unrealized_pnl_percent)),
            pnl_color);

        // CHG%
        const char* chg_color = h.day_change_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set_cell(kColChgPct,
                 QString("%1%2%").arg(h.day_change_percent >= 0 ? "+" : "").arg(format_value(h.day_change_percent)),
                 chg_color);

        // TREND — show loaded data, a pending shimmer, or a failure dash.
        // Reuse the row's existing sparkline widget when there is one.
        auto* sparkline = qobject_cast<PortfolioSparkline*>(table_->cellWidget(r, kColTrend));
        if (!sparkline) {
            sparkline = new PortfolioSparkline(0, 0);
            sparkline->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            table_->setCellWidget(r, kColTrend, sparkline);
        }
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

        // WT%
        set_cell(kColWeight, QString("%1%").arg(format_value(h.weight, 1)));

        // Highlight selected row
        if (h.symbol == selected_symbol_) {
            table_->selectRow(r);
        }
    }

    // Empty state — an unexplained blank grid reads as a bug. Distinguish
    // "portfolio has nothing in it" from "your filter hid everything".
    table_->clearSpans();
    if (page_rows.isEmpty()) {
        table_->setRowCount(1);
        table_->setRowHeight(0, 40);
        const bool filtered = !filter_text_.isEmpty() || !sector_symbols_.isEmpty();
        auto* msg = new QTableWidgetItem(filtered ? tr("No positions match the current filter.")
                                                  : tr("No positions yet — use BUY to add your first holding."));
        msg->setTextAlignment(Qt::AlignCenter);
        msg->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        msg->setFlags(Qt::ItemIsEnabled); // informational, not selectable
        table_->setItem(0, kColSymbol, msg);
        table_->setSpan(0, 0, 1, kColumnCount);
    }

    // Restore the scroll offset (clamped by the scrollbar itself when the row
    // count shrank).
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(prev_scroll);

    // Footer status + nav button enabled-state must reflect what we just rendered.
    update_pagination_controls();
}

QString PortfolioBlotter::format_value(double v, int dp) const {
    // Locale-aware: groups thousands so a six-figure market value is readable
    // and matches PortfolioStatsRibbon / PortfolioStatusBar.
    return QLocale().toString(v, 'f', dp);
}

void PortfolioBlotter::set_filter(const QString& text) {
    const QString needle = text.trimmed().toLower();
    if (needle == filter_text_)
        return;
    filter_text_ = needle;
    invalidate_view_cache();
    apply_filter();
}

void PortfolioBlotter::set_sector_filter(const QStringList& symbols) {
    if (sector_symbols_ == symbols)
        return;
    sector_symbols_ = symbols;
    invalidate_view_cache();
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

    auto* edit_act = menu.addAction(tr("Edit Transaction"));
    auto* delete_act = menu.addAction(tr("Close / Delete Position"));

    // Mark the destructive item so it does not read as a peer of "Edit".
    // The previous attempt used `QMenu::item[data='danger']` — QSS attribute
    // selectors match Q_PROPERTYs on the styled *widget*, not QAction::setData,
    // so it matched nothing and the item rendered in the default colour. Two
    // setIcon(QIcon()) calls next to it were pure no-ops and are gone.
    delete_act->setFont([&] {
        QFont f = menu.font();
        f.setBold(true);
        return f;
    }());

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
                                  "QTableWidget::item:selected { background:rgba(217,119,6,0.10); color:%5; }"
                                  "QTableWidget::item:hover { background:%6; }"
                                  "QScrollBar:vertical { width:5px; background:%1; }"
                                  "QScrollBar::handle:vertical { background:%4; min-height:20px; }")
                              .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::fonts::DATA_FAMILY,
                                   ui::colors::BORDER_DIM(), ui::colors::AMBER(), ui::colors::BG_HOVER()));
    // See note in build constructor — header rule must live on the header
    // widget itself or the global qApp stylesheet wins.
    table_->horizontalHeader()->setStyleSheet(QString("QHeaderView::section { background:%1; color:%2; border:none;"
                                                      "  border-bottom:1px solid %3; border-right:1px solid %4;"
                                                      "  padding:5px 8px; font-size:" +
                                                      hsz + "px; font-weight:700; letter-spacing:0.5px; }")
                                                  .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(),
                                                       ui::colors::BORDER_MED(), ui::colors::BORDER_DIM()));
    populate_table();
}

void PortfolioBlotter::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioBlotter::retranslateUi() {
    // Column headers — same source-key array used in build_ui().
    if (table_) {
        QStringList headers;
        headers.reserve(kColumnCount);
        for (int i = 0; i < kColumnCount; ++i)
            headers << tr(kColumnKeys[i]);
        table_->setHorizontalHeaderLabels(headers);
    }

    if (btn_first_) {
        btn_first_->setToolTip(tr("First page"));
        btn_first_->setAccessibleName(tr("First page"));
    }
    if (btn_prev_) {
        btn_prev_->setToolTip(tr("Previous page"));
        btn_prev_->setAccessibleName(tr("Previous page"));
    }
    if (btn_next_) {
        btn_next_->setToolTip(tr("Next page"));
        btn_next_->setAccessibleName(tr("Next page"));
    }
    if (btn_last_) {
        btn_last_->setToolTip(tr("Last page"));
        btn_last_->setAccessibleName(tr("Last page"));
    }
    if (page_size_combo_)
        page_size_combo_->setAccessibleName(tr("Rows per page"));
    if (footer_rows_label_)
        footer_rows_label_->setText(tr("Rows:"));

    // Footer status + page label hold dynamic strings — re-run the computation
    // so "Showing %1-%2 of %3" and "Page %1 of %2" pick up the new locale.
    update_pagination_controls();
}

} // namespace fincept::screens
