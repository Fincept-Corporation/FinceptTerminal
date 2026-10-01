#include "screens/screener/ScreenerScreen.h"

#include "core/events/EventBus.h"
#include "core/symbol/SymbolContext.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

namespace {
// Broad large-cap basket across sectors — mirrors the dashboard ScreenerWidget
// basket, extended for a full-screen view.
const QStringList kBasket = {
    "AAPL", "MSFT", "GOOGL", "AMZN",  "NVDA", "META", "TSLA", "NFLX", "AMD",  "INTC", "AVGO", "ORCL", "CRM",
    "ADBE", "CSCO", "QCOM",  "TXN",   "JPM",  "GS",   "BAC",  "WFC",  "MS",   "C",    "BRK-B", "V",    "MA",
    "AXP",  "PYPL", "WMT",   "COST",  "TGT",  "HD",   "LOW",  "NKE",  "MCD",  "SBUX", "AMGN", "PFE",  "JNJ",
    "MRK",  "ABBV", "LLY",   "UNH",   "XOM",  "CVX",  "SLB",  "COP",  "NEE",  "DUK",  "SO",   "CAT",  "GE",
    "HON",  "RTX",  "BA",    "DE",    "PLTR", "COIN", "SOFI", "SNAP", "UBER", "ABNB", "SHOP", "SQ"};
} // namespace

ScreenerScreen::ScreenerScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_styles();
    retranslate();
}

void ScreenerScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ──────────────────────────────────────────────────────────
    header_bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(header_bar_);
    hb->setContentsMargins(16, 12, 16, 12);
    hb->setSpacing(10);

    auto* title_box = new QVBoxLayout;
    title_box->setSpacing(1);
    title_lbl_ = new QLabel;
    title_lbl_->setObjectName("screenerTitle");
    subtitle_lbl_ = new QLabel;
    subtitle_lbl_->setObjectName("screenerSubtitle");
    title_box->addWidget(title_lbl_);
    title_box->addWidget(subtitle_lbl_);
    hb->addLayout(title_box);
    hb->addStretch();

    search_ = new QLineEdit;
    search_->setClearButtonEnabled(true);
    search_->setFixedWidth(220);
    connect(search_, &QLineEdit::textChanged, this, [this](const QString&) { apply_filter(); });
    hb->addWidget(search_);

    sort_combo_ = new QComboBox;
    connect(sort_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { apply_filter(); });
    hb->addWidget(sort_combo_);

    count_lbl_ = new QLabel;
    count_lbl_->setObjectName("screenerCount");
    hb->addWidget(count_lbl_);

    refresh_btn_ = new QPushButton;
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &ScreenerScreen::refresh_now);
    hb->addWidget(refresh_btn_);

    root->addWidget(header_bar_);

    // ── Results table ───────────────────────────────────────────────────────
    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    // Results were a dead end: no way to act on a row. Double-click researches the symbol,
    // right-click offers the other places a ticker can go (nav.open_symbol / watchlist).
    // A user pick of a row becomes the linked group's symbol. render_rows() blocks the table's
    // signals while it re-selects the row after a refresh, so quote ticks don't re-publish.
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (link_group_ == SymbolGroup::None)
            return;
        const SymbolRef ref = current_symbol();
        if (ref.is_valid())
            SymbolContext::instance().set_group_symbol(link_group_, ref, this);
    });
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested, this, &ScreenerScreen::show_row_menu);
    connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { open_symbol_in(QStringLiteral("equity_research"), symbol_at_row(row)); });
    root->addWidget(table_, 1);

    // Stands in for the table while there is nothing to list: still loading, every quote
    // failed, or the search matched nothing (previously an unexplained blank grid).
    empty_lbl_ = new QLabel(this);
    empty_lbl_->setObjectName("screenerEmpty");
    empty_lbl_->setAlignment(Qt::AlignCenter);
    empty_lbl_->setWordWrap(true);
    root->addWidget(empty_lbl_, 1);
}

void ScreenerScreen::apply_styles() {
    setStyleSheet(
        QString("ScreenerScreen { background: %1; }").arg(ui::colors::BG_BASE()) +
        QString("#screenerTitle { color: %1; font-size: 16px; font-weight: 700; }").arg(ui::colors::TEXT_PRIMARY()) +
        QString("#screenerSubtitle { color: %1; font-size: 11px; }").arg(ui::colors::TEXT_TERTIARY()) +
        QString("#screenerCount { color: %1; font-size: 11px; }").arg(ui::colors::TEXT_TERTIARY()) +
        QString("#screenerEmpty { color: %1; font-size: 13px; padding: 24px; }").arg(ui::colors::TEXT_TERTIARY()) +
        QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 8px; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()) +
        QString("QComboBox { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 8px; }"
                "QComboBox QAbstractItemView { background: %1; color: %2; border: 1px solid %3; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()) +
        QString("QPushButton { background: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 4px 12px; }"
                "QPushButton:hover { border-color: %4; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::INFO()));

    if (header_bar_)
        header_bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                       .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    if (table_)
        table_->setStyleSheet(
            QString("QTableWidget { background: %1; color: %2; border: none; gridline-color: %3; }")
                .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
            QString("QTableWidget::item { padding: 6px 8px; }") +
            QString("QTableWidget::item:alternate { background: %1; }").arg(ui::colors::BG_RAISED()) +
            QString("QHeaderView::section { background: %1; color: %2; border: none; border-bottom: 1px solid %3; "
                    "padding: 6px 8px; font-size: 10px; font-weight: 700; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_MED()));
}

void ScreenerScreen::retranslate() {
    title_lbl_->setText(tr("STOCK SCREENER"));
    subtitle_lbl_->setText(
        tr("Filter a broad large-cap basket by change, volume, or price — double-click a row to research it"));
    search_->setPlaceholderText(tr("Search symbol or name…"));
    refresh_btn_->setText(tr("REFRESH"));

    search_->setAccessibleName(tr("Filter by symbol or company name"));
    sort_combo_->setAccessibleName(tr("Sort order"));
    refresh_btn_->setAccessibleName(tr("Refresh all quotes now"));

    const int prev = sort_combo_->currentIndex();
    QSignalBlocker block(sort_combo_);
    sort_combo_->clear();
    // Index 0/1 sort by change but the old "% CHANGE ↑ / ↓" labels read as sort
    // direction while actually meaning gainers/losers — name the intent instead.
    sort_combo_->addItems({tr("TOP GAINERS"), tr("TOP LOSERS"), tr("VOLUME ↓"), tr("PRICE ↓"), tr("PRICE ↑")});
    sort_combo_->setCurrentIndex(prev < 0 ? 0 : prev);

    table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("NAME"), tr("PRICE"), tr("CHG%"), tr("VOLUME")});

    rebuild_from_cache();
}

void ScreenerScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void ScreenerScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void ScreenerScreen::changeEvent(QEvent* e) {
    QWidget::changeEvent(e);
    if (!e)
        return;
    if (e->type() == QEvent::StyleChange || e->type() == QEvent::PaletteChange) {
        // Re-entrancy guard — WITHOUT THIS THE PROCESS DIES WITH A STACK
        // OVERFLOW (0xC00000FD).
        //
        // apply_styles() calls setStyleSheet() on header_bar_ and table_.
        // setStyleSheet re-polishes the widget, which posts a fresh StyleChange
        // — straight back into this handler, which calls apply_styles() again.
        // Nothing breaks the loop, so it recurses until the stack is exhausted.
        //
        // Found by --smoke-test, which aborted deterministically while
        // constructing this screen (the constructor calls apply_styles()
        // directly). Reproduced across runs with different walk orders.
        if (restyling_)
            return;
        restyling_ = true;
        apply_styles();
        restyling_ = false;
    } else if (e->type() == QEvent::LanguageChange) {
        retranslate();
    }
}

void ScreenerScreen::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    failed_.clear();
    for (const auto& sym : kBasket) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            failed_.remove(sym);
            row_cache_.insert(sym, v.value<services::QuoteData>());
            schedule_rebuild();
        });
        // A failed refresh used to be invisible: with nothing cached the grid just stayed
        // blank. Track it so the empty state can say the quotes are unavailable.
        hub.subscribe_errors(this, topic, [this, sym](const QString&) {
            failed_.insert(sym);
            schedule_rebuild();
        });
    }
    hub_active_ = true;

    // Real company names for the NAME column and the search box (cached on disk by the
    // service — the lookup runs once per symbol, ever). The first callback is synchronous
    // with whatever is already cached; the second lands when the lookup finishes.
    QPointer<ScreenerScreen> self = this;
    services::MarketDataService::instance().resolve_names(kBasket, [self](const QHash<QString, QString>& m) {
        if (!self || m.isEmpty())
            return;
        bool changed = false;
        for (auto it = m.constBegin(); it != m.constEnd(); ++it) {
            if (self->names_.value(it.key()) != it.value()) {
                self->names_.insert(it.key(), it.value());
                changed = true;
            }
        }
        if (changed && self->hub_active_) // hidden: the next show re-renders from names_
            self->schedule_rebuild();
    });
}

void ScreenerScreen::schedule_rebuild() {
    // 62 topics deliver 62 separate callbacks per refresh sweep. Rebuilding the
    // whole table on each one meant ~62 full sorts and ~19,000 QTableWidgetItem
    // allocations per sweep, and reset the scroll position 62 times. Coalesce
    // into one rebuild on the next event-loop turn.
    //
    // This is a render coalescer, not a data-refresh timer (D3): it is a
    // zero-delay single shot armed by an incoming hub delivery, never a cadence.
    if (rebuild_pending_)
        return;
    rebuild_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        rebuild_pending_ = false;
        rebuild_from_cache();
    });
}

void ScreenerScreen::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void ScreenerScreen::refresh_now() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(kBasket.size());
    for (const auto& sym : kBasket)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true); // user-triggered: bypass min_interval
}

void ScreenerScreen::rebuild_from_cache() {
    all_quotes_.clear();
    all_quotes_.reserve(row_cache_.size());
    for (const auto& sym : kBasket) {
        auto it = row_cache_.constFind(sym);
        if (it == row_cache_.constEnd())
            continue;
        services::QuoteData q = it.value();
        const QString real_name = names_.value(sym);
        if (!real_name.isEmpty())
            q.name = real_name; // the feed's "name" is just the ticker
        all_quotes_.append(q);
    }
    apply_filter();
}

void ScreenerScreen::apply_filter() {
    QVector<services::QuoteData> rows = all_quotes_;

    const QString needle = search_ ? search_->text().trimmed() : QString();
    if (!needle.isEmpty()) {
        QVector<services::QuoteData> filtered;
        filtered.reserve(rows.size());
        for (const auto& q : rows) {
            if (q.symbol.contains(needle, Qt::CaseInsensitive) || q.name.contains(needle, Qt::CaseInsensitive))
                filtered.append(q);
        }
        rows = filtered;
    }

    // A quote with no price carries no usable data (it renders as dashes). Rank only the
    // real ones and park the rest at the bottom — otherwise "top losers" / "price ↑" put the
    // empty rows first (their change and price are 0).
    const auto mid = std::stable_partition(rows.begin(), rows.end(),
                                           [](const services::QuoteData& q) { return q.price > 0.0; });

    const int idx = sort_combo_ ? sort_combo_->currentIndex() : 0;
    switch (idx) {
        case 0: // % change desc (top gainers first)
            std::sort(rows.begin(), mid, [](const auto& a, const auto& b) { return a.change_pct > b.change_pct; });
            break;
        case 1: // % change asc (top losers first)
            std::sort(rows.begin(), mid, [](const auto& a, const auto& b) { return a.change_pct < b.change_pct; });
            break;
        case 2: // volume desc
            std::sort(rows.begin(), mid, [](const auto& a, const auto& b) { return a.volume > b.volume; });
            break;
        case 3: // price desc
            std::sort(rows.begin(), mid, [](const auto& a, const auto& b) { return a.price > b.price; });
            break;
        case 4: // price asc
            std::sort(rows.begin(), mid, [](const auto& a, const auto& b) { return a.price < b.price; });
            break;
        default:
            break;
    }

    // Empty states: say why instead of showing a blank grid.
    if (empty_lbl_ && table_) {
        QString message;
        if (rows.isEmpty()) {
            if (all_quotes_.isEmpty())
                message = failed_.size() >= kBasket.size()
                              ? tr("Quotes are unavailable right now. Press REFRESH to try again.")
                              : tr("Loading quotes…");
            else if (!needle.isEmpty())
                message = tr("No symbols match \"%1\".").arg(needle);
            else
                message = tr("No symbols to show.");
        }
        empty_lbl_->setText(message);
        empty_lbl_->setVisible(!message.isEmpty());
        table_->setVisible(message.isEmpty());
    }

    render_rows(rows);
    if (count_lbl_)
        count_lbl_->setText(tr("%1 of %2 symbols").arg(rows.size()).arg(kBasket.size()));
}

void ScreenerScreen::render_rows(const QVector<services::QuoteData>& rows) {
    // Rebuilding every item drops the scroll offset and the selected row. Quotes
    // arrive continuously, so without this the list jumps back to the top under
    // the user's cursor on every sweep.
    const int scroll_pos = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;
    const QString selected_symbol =
        table_->currentRow() >= 0 && table_->item(table_->currentRow(), 0)
            ? table_->item(table_->currentRow(), 0)->text()
            : QString();

    // The rebuild clears and re-applies the selection on every quote tick; none of that is a
    // user pick, so keep it from being published to the symbol group.
    QSignalBlocker block_selection_signals(table_);
    table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& q = rows[r];

        auto* sym = new QTableWidgetItem(q.symbol);
        sym->setForeground(QColor(ui::colors::INFO()));
        QFont bold = sym->font();
        bold.setBold(true);
        sym->setFont(bold);
        table_->setItem(r, 0, sym);

        table_->setItem(r, 1, new QTableWidgetItem(q.name));

        // A quote with no price is "no data" (0 arrives for a missing figure): show dashes, not
        // "$0.00" / "+0.00%".
        const bool has_price = q.price > 0.0;
        const QString dash = QStringLiteral("—");

        auto* price = new QTableWidgetItem(has_price ? QString("$%1").arg(q.price, 0, 'f', 2) : dash);
        price->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(r, 2, price);

        auto* chg = new QTableWidgetItem(
            has_price ? QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2) : dash);
        chg->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        chg->setForeground(QColor(!has_price          ? ui::colors::TEXT_TERTIARY()
                                  : q.change_pct > 0 ? ui::colors::POSITIVE()
                                  : q.change_pct < 0 ? ui::colors::NEGATIVE()
                                                     : ui::colors::TEXT_PRIMARY()));
        table_->setItem(r, 3, chg);

        auto* vol = new QTableWidgetItem(ui::formatting::format_compact_volume(static_cast<qint64>(q.volume)));
        vol->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vol->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        table_->setItem(r, 4, vol);
    }

    if (!selected_symbol.isEmpty()) {
        for (int r = 0; r < rows.size(); ++r) {
            if (rows[r].symbol == selected_symbol) {
                table_->selectRow(r);
                break;
            }
        }
    }
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(scroll_pos);
}

QString ScreenerScreen::symbol_at_row(int row) const {
    if (!table_ || row < 0 || row >= table_->rowCount())
        return {};
    auto* item = table_->item(row, 0);
    return item ? item->text() : QString();
}

SymbolRef ScreenerScreen::current_symbol() const {
    const QString sym = table_ ? symbol_at_row(table_->currentRow()) : QString();
    return sym.isEmpty() ? SymbolRef{} : SymbolRef::equity(sym);
}

void ScreenerScreen::on_group_symbol_changed(const SymbolRef& ref) {
    if (!table_ || !ref.is_valid())
        return;
    // Highlight the symbol if it is in the current (filtered) results; no-op otherwise.
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (symbol_at_row(r).compare(ref.symbol, Qt::CaseInsensitive) == 0) {
            QSignalBlocker block(table_); // a follower must not re-publish
            table_->selectRow(r);
            table_->scrollToItem(table_->item(r, 0), QAbstractItemView::PositionAtCenter);
            return;
        }
    }
}

void ScreenerScreen::open_symbol_in(const QString& screen_id, const QString& symbol) {
    if (symbol.isEmpty())
        return;
    // Navigates first (constructing the target when needed), then delivers the ticker through
    // IGroupLinked — nav.switch_screen + a screen-specific load event drops it for a screen
    // that has not been opened yet.
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}});
}

void ScreenerScreen::show_row_menu(const QPoint& pos) {
    auto* item = table_->itemAt(pos);
    if (!item)
        return;
    table_->selectRow(item->row());
    const QString symbol = symbol_at_row(item->row());
    if (symbol.isEmpty())
        return;
    const QString name = names_.value(symbol);

    QMenu menu(this);
    connect(menu.addAction(tr("Open in Equity Research")), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("equity_research"), symbol); });
    connect(menu.addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("equity_trading"), symbol); });
    connect(menu.addAction(tr("Open in News")), &QAction::triggered, this,
            [this, symbol]() { open_symbol_in(QStringLiteral("news"), symbol); });

    // One entry per watchlist (the screener had no way to keep a promising result).
    const auto lists = fincept::WatchlistRepository::instance().list_all();
    if (lists.is_ok() && !lists.value().isEmpty()) {
        QMenu* wl_menu = menu.addMenu(tr("Add to Watchlist"));
        for (const auto& wl : lists.value()) {
            connect(wl_menu->addAction(wl.name), &QAction::triggered, this,
                    [this, symbol, name, list_id = wl.id, list_name = wl.name]() {
                        auto& repo = fincept::WatchlistRepository::instance();
                        const auto stocks = repo.get_stocks(list_id);
                        if (stocks.is_ok()) {
                            for (const auto& s : stocks.value()) {
                                if (s.symbol.compare(symbol, Qt::CaseInsensitive) == 0) {
                                    QToolTip::showText(QCursor::pos(),
                                                       tr("%1 is already in %2").arg(symbol, list_name), table_);
                                    return;
                                }
                            }
                        }
                        const auto r = repo.add_stock(list_id, symbol, name);
                        QToolTip::showText(QCursor::pos(),
                                           r.is_ok() ? tr("Added %1 to %2").arg(symbol, list_name)
                                                     : tr("Could not add %1 to %2").arg(symbol, list_name),
                                           table_);
                        if (r.is_ok()) // let an open Watchlist panel pick the change up
                            EventBus::instance().publish("watchlist.updated",
                                                         {{"action", QStringLiteral("add")}, {"symbol", symbol}});
                    });
        }
    }

    menu.addSeparator();
    connect(menu.addAction(tr("Copy Symbol")), &QAction::triggered, this,
            [symbol]() { QApplication::clipboard()->setText(symbol); });
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

} // namespace fincept::screens
