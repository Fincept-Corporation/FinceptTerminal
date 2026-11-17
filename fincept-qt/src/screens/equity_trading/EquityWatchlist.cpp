// EquityWatchlist.cpp — dense, sortable quote list with live quote updates
#include "screens/equity_trading/EquityWatchlist.h"

#include "core/symbol/SymbolDragSource.h"
#include "core/symbol/SymbolRef.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/equity_trading/EquityTypes.h"
#include "services/markets/MarketDataService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/SparklineDelegate.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMutexLocker>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens::equity {

namespace fmt = fincept::ui::formatting;

namespace {

// File-local helpers carry an eqwl_ prefix (unity-build safety).
constexpr int eqwl_kKeySelectDelayMs = 140;
// A good quote older than this (no update since) reads as stale in the row.
constexpr qint64 eqwl_kStaleMs = 5 * 60 * 1000;

QString eqwl_arrow(double v) {
    if (v > 0.0)
        return QStringLiteral("\xE2\x96\xB2 "); // ▲
    if (v < 0.0)
        return QStringLiteral("\xE2\x96\xBC "); // ▼
    return QStringLiteral("  ");
}
} // namespace

EquityWatchlist::EquityWatchlist(QWidget* parent) : QWidget(parent) {
    setObjectName("eqWatchlist");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header — the shared panel header: WATCHLIST + symbol count, then the
    // named-watchlist switcher (combo) and its manage menu as controls.
    auto* header = new fincept::ui::PanelHeader(tr("Watchlist"), this);

    wl_combo_ = new QComboBox;
    wl_combo_->setObjectName("eqWatchlistCombo");
    wl_combo_->setToolTip(tr("Active watchlist"));
    wl_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    wl_combo_->setMinimumContentsLength(8);
    connect(wl_combo_, qOverload<int>(&QComboBox::activated), this, &EquityWatchlist::on_watchlist_combo_activated);
    header->add_control(wl_combo_, 1);

    wl_menu_btn_ = new QToolButton;
    wl_menu_btn_->setObjectName("eqWatchlistMenuBtn");
    wl_menu_btn_->setText(QStringLiteral("\xE2\x8B\xAF")); // ⋯
    wl_menu_btn_->setCursor(Qt::PointingHandCursor);
    wl_menu_btn_->setAutoRaise(true);
    wl_menu_btn_->setToolTip(tr("New / Rename / Delete watchlist"));
    connect(wl_menu_btn_, &QToolButton::clicked, this, &EquityWatchlist::on_watchlist_menu);
    header->add_control(wl_menu_btn_);

    // Symbol count: the header's badge (dim, after the title).
    header->set_badge(QStringLiteral("0"));
    count_label_ = header->badge_label();
    layout->addWidget(header);

    // Add (shared symbol search: any instrument, any market) + filter, one row each.
    auto* add_row = new QWidget(this);
    auto* add_layout = new QHBoxLayout(add_row);
    add_layout->setContentsMargins(4, 2, 4, 2);
    add_layout->setSpacing(3);
    add_box_ = make_symbol_box(SymbolBoxRole::WatchlistAdd, add_row);
    add_box_->setObjectName("eqWatchlistAddBox");
    add_layout->addWidget(add_box_, 1);
    layout->addWidget(add_row);
    connect(add_box_, &SymbolBox::instrument_picked, this,
            [this](const fincept::core::instrument::Instrument& inst) { emit instrument_added(inst); });
    // Multi entry / a pasted list: every resolved instrument is added.
    connect(add_box_, &SymbolBox::instruments_picked, this,
            [this](const QVector<fincept::core::instrument::Instrument>& list) {
                for (const auto& inst : list)
                    emit instrument_added(inst);
            });
    connect(add_box_, &SymbolBox::text_committed_raw, this, [this](const QString& text) {
        // Raw text (no instrument resolved): the screen decides (broker master lookup).
        emit symbol_added(text.trimmed().toUpper());
    });
    connect(add_box_, &SymbolBox::unresolved, this, [this](const QStringList& texts, const QString& message) {
        emit add_unresolved(texts.join(QStringLiteral(", ")), message);
    });

    filter_edit_ = new QLineEdit;
    filter_edit_->setObjectName("eqWatchlistSearch");
    filter_edit_->setPlaceholderText(tr("Filter rows..."));
    filter_edit_->setClearButtonEnabled(true);
    connect(filter_edit_, &QLineEdit::textChanged, this, &EquityWatchlist::on_filter_changed);
    layout->addWidget(filter_edit_);

    // Source-level notice (set_feed_notice).
    notice_box_ = new QWidget(this);
    notice_box_->setObjectName("eqWatchlistNotice");
    notice_box_->setAttribute(Qt::WA_StyledBackground, true);
    auto* notice_layout = new QVBoxLayout(notice_box_);
    notice_layout->setContentsMargins(8, 6, 8, 6);
    notice_layout->setSpacing(2);
    notice_title_ = new QLabel(notice_box_);
    notice_title_->setObjectName("eqWatchlistNoticeTitle");
    notice_title_->setWordWrap(true);
    notice_layout->addWidget(notice_title_);
    notice_detail_ = new QLabel(notice_box_);
    notice_detail_->setObjectName("eqWatchlistNoticeDetail");
    notice_detail_->setWordWrap(true);
    notice_detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    notice_layout->addWidget(notice_detail_);
    notice_box_->setStyleSheet(
        QString("QWidget#eqWatchlistNotice{background:%1;border:none;border-bottom:1px solid %2;}"
                "QLabel#eqWatchlistNoticeTitle{color:%3;background:transparent;font-weight:700;font-size:" +
                fincept::ui::fonts::px(-3) +
                "px;}"
                "QLabel#eqWatchlistNoticeDetail{color:%4;background:transparent;font-size:" +
                fincept::ui::fonts::px(-4) + "px;}")
            .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_DIM(), fincept::ui::colors::WARNING(),
                 fincept::ui::colors::TEXT_SECONDARY()));
    notice_box_->hide();
    layout->addWidget(notice_box_);

    // Table
    table_ = new QTableWidget;
    table_->setObjectName("eqWatchlistTable");
    table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; gridline-color:%3;"
                " font-size:%4px; font-family:%5; }"
                "QTableWidget::item { padding:1px 2px; border:none; }"
                "QTableWidget::item:selected { background:%6; color:%2; }"
                "QHeaderView::section { background:%7; color:%8; font-size:%4px; font-weight:700;"
                " font-family:%5; border:none; border-bottom:1px solid %3; padding:2px 2px; }")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::SMALL.get())
            .arg(fincept::ui::fonts::DATA_FAMILY(), fincept::ui::colors::BG_HOVER(), fincept::ui::colors::BG_RAISED(),
                 fincept::ui::colors::TEXT_SECONDARY()));
    table_->setColumnCount(ColCount);
    table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LTP"), tr("CHG%"), tr("TREND")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::StrongFocus); // arrow keys move, Enter / selection opens the symbol
    // Fixed sections sized by fit_columns() — never resize-to-contents (GUI stalls).
    table_->horizontalHeader()->setStretchLastSection(false);
    for (int c = 0; c < ColCount; ++c)
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
    table_->horizontalHeader()->setMinimumSectionSize(0);
    table_->horizontalHeader()->setSectionsClickable(true);
    table_->horizontalHeader()->setSortIndicatorShown(false);
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, &EquityWatchlist::on_header_clicked);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->viewport()->installEventFilter(this);
    table_->installEventFilter(this);
    table_->verticalHeader()->setDefaultSectionSize(table_row_height(-1, 24));
    auto* spark = new fincept::ui::SparklineDelegate(table_);
    spark->set_width_chars(8);
    table_->setItemDelegateForColumn(ColTrend, spark);

    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::theme_changed, this,
            [this](const fincept::ui::ThemeTokens&) { retheme_items(table_); });
    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this, [this]() {
        table_->verticalHeader()->setDefaultSectionSize(table_row_height(-1, 24));
        schedule_fit();
    });
    // Currency / number-format change: re-format every row from the quotes held (no fetch).
    fmt::on_format_changed(this, [this]() {
        QMutexLocker lock(&mutex_);
        for (int i = 0; i < entries_.size() && i < table_->rowCount(); ++i)
            render_row(i, entries_[i]);
        schedule_fit();
    });

    connect(table_, &QTableWidget::cellClicked, this, &EquityWatchlist::on_cell_clicked);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this, &EquityWatchlist::on_table_context_menu);
    // Keyboard navigation: a row reached with the arrow keys opens after a short pause.
    key_select_timer_ = new QTimer(this);
    key_select_timer_->setSingleShot(true);
    key_select_timer_->setInterval(eqwl_kKeySelectDelayMs);
    connect(key_select_timer_, &QTimer::timeout, this, [this]() {
        if (!pending_key_select_.isEmpty() && pending_key_select_ != active_symbol_) {
            const QString sym = pending_key_select_;
            set_active_symbol(sym);
            emit symbol_selected(sym);
        }
        pending_key_select_.clear();
    });
    connect(table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int prev_row, int) {
        if (row >= 0 && row != prev_row && table_->hasFocus())
            select_row_by_keyboard(row);
    });
    layout->addWidget(table_, 1);

    setAccessibleName(tr("Watchlist"));
    wl_combo_->setAccessibleName(tr("Active watchlist"));
    wl_menu_btn_->setAccessibleName(tr("Watchlist actions"));
    filter_edit_->setAccessibleName(tr("Filter watchlist"));
    add_box_->setAccessibleName(tr("Add symbol to watchlist"));
    count_label_->setAccessibleName(tr("Symbol count"));
    table_->setAccessibleName(tr("Watchlist symbols"));
    setTabOrder(wl_combo_, wl_menu_btn_);
    setTabOrder(wl_menu_btn_, add_box_);
    setTabOrder(add_box_, filter_edit_);
    setTabOrder(filter_edit_, table_);

    // Drag-out: hold-and-drag a row to ship the instrument to any drop target.
    symbol_dnd::installDragSource(table_->viewport(), [this]() -> SymbolRef {
        if (!table_)
            return {};
        const int r = table_->currentRow();
        if (r < 0)
            return {};
        auto* item = table_->item(r, ColSymbol);
        if (!item)
            return {};
        const QString sym = item->data(Qt::UserRole).toString();
        if (sym.isEmpty())
            return {};
        const auto info = info_.value(sym);
        return SymbolRef::equity(info.market_key.isEmpty() ? sym : info.market_key);
    });
}

void EquityWatchlist::set_watchlists(const QVector<QPair<QString, QString>>& lists, const QString& active_id) {
    if (!wl_combo_)
        return;
    QSignalBlocker block(wl_combo_); // programmatic — don't emit watchlist_selected
    wl_combo_->clear();
    int active_idx = 0;
    for (int i = 0; i < lists.size(); ++i) {
        wl_combo_->addItem(lists[i].second, lists[i].first); // (name, id)
        if (lists[i].first == active_id)
            active_idx = i;
    }
    if (wl_combo_->count() > 0)
        wl_combo_->setCurrentIndex(active_idx);
}

void EquityWatchlist::on_watchlist_combo_activated(int index) {
    if (index < 0 || !wl_combo_)
        return;
    const QString id = wl_combo_->itemData(index).toString();
    if (!id.isEmpty())
        emit watchlist_selected(id);
}

void EquityWatchlist::on_watchlist_menu() {
    QMenu menu(this);
    QAction* new_act = menu.addAction(tr("New watchlist…"));
    QAction* rename_act = menu.addAction(tr("Rename…"));
    QAction* delete_act = menu.addAction(tr("Delete"));
    delete_act->setProperty("fincept_agent_risk", "delete");

    const int idx = wl_combo_ ? wl_combo_->currentIndex() : -1;
    const QString cur_id = (idx >= 0) ? wl_combo_->itemData(idx).toString() : QString();
    const QString cur_name = (idx >= 0) ? wl_combo_->itemText(idx) : QString();
    rename_act->setEnabled(!cur_id.isEmpty());
    delete_act->setEnabled(!cur_id.isEmpty());

    QAction* chosen = menu.exec(wl_menu_btn_->mapToGlobal(QPoint(0, wl_menu_btn_->height())));
    if (!chosen)
        return;

    if (chosen == new_act) {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("New Watchlist"), tr("Name:"), QLineEdit::Normal, {}, &ok).trimmed();
        if (ok && !name.isEmpty())
            emit watchlist_create_requested(name);
    } else if (chosen == rename_act && !cur_id.isEmpty()) {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Rename Watchlist"), tr("Name:"), QLineEdit::Normal, cur_name, &ok)
                .trimmed();
        if (ok && !name.isEmpty())
            emit watchlist_rename_requested(cur_id, name);
    } else if (chosen == delete_act && !cur_id.isEmpty()) {
        const auto btn = QMessageBox::question(this, tr("Delete Watchlist"), tr("Delete \"%1\"?").arg(cur_name));
        if (btn == QMessageBox::Yes)
            emit watchlist_delete_requested(cur_id);
    }
}

void EquityWatchlist::on_table_context_menu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0)
        return;
    auto* item = table_->item(row, ColSymbol);
    if (!item)
        return;
    const QString sym = item->data(Qt::UserRole).toString();
    if (sym.isEmpty())
        return;
    const QString label = symbol_text(sym);

    QMenu menu(this);
    QAction* buy_act = menu.addAction(tr("Buy %1…").arg(label));
    QAction* sell_act = menu.addAction(tr("Sell %1…").arg(label));
    const RowInfo info = info_.value(sym);
    if (!info.blocked_reason.isEmpty()) {
        buy_act->setEnabled(false);
        sell_act->setEnabled(false);
        buy_act->setToolTip(info.blocked_reason);
    }
    QAction* chart_act = menu.addAction(tr("Show chart"));
    menu.addSeparator();
    QAction* research_act = menu.addAction(tr("Open in Equity Research"));
    QAction* news_act = menu.addAction(tr("News for %1").arg(label));
    QAction* alert_act = menu.addAction(tr("Add price alert…"));
    const bool has_public = !info.market_key.isEmpty();
    research_act->setEnabled(has_public);
    alert_act->setEnabled(has_public);
    menu.addSeparator();
    QAction* remove_act = menu.addAction(tr("Remove %1").arg(label));
    remove_act->setProperty("fincept_agent_risk", "delete");
    QAction* chosen = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;
    if (chosen == remove_act) {
        emit symbol_removed(sym);
    } else if (chosen == buy_act || chosen == sell_act) {
        emit trade_requested(sym, chosen == buy_act);
    } else if (chosen == chart_act) {
        set_active_symbol(sym);
        emit symbol_selected(sym);
    } else if (chosen == research_act) {
        emit research_requested(sym);
    } else if (chosen == news_act) {
        emit news_requested(sym);
    } else if (chosen == alert_act) {
        emit alert_requested(sym);
    }
}

void EquityWatchlist::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::FontChange)
        schedule_fit();
    QWidget::changeEvent(event);
}

void EquityWatchlist::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    visible_ = true;
    subscribe_sparklines();
}

void EquityWatchlist::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    visible_ = false;
    unsubscribe_sparklines(); // nothing polls for a hidden list
}

bool EquityWatchlist::eventFilter(QObject* obj, QEvent* event) {
    if (table_ && obj == table_->viewport() && event->type() == QEvent::Resize)
        fit_columns(); // synchronously: a dragged splitter never paints a stale layout
    if (table_ && obj == table_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const int row = table_->currentRow();
        if (row >= 0) {
            const QString sym = table_->item(row, ColSymbol) ? table_->item(row, ColSymbol)->data(Qt::UserRole).toString()
                                                             : QString();
            if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !sym.isEmpty()) {
                key_select_timer_->stop();
                set_active_symbol(sym);
                emit symbol_selected(sym);
                return true;
            }
            if (ke->key() == Qt::Key_Delete && !sym.isEmpty()) {
                emit symbol_removed(sym);
                return true;
            }
            if (ke->key() == Qt::Key_B && ke->modifiers() == Qt::NoModifier && !sym.isEmpty()) {
                emit trade_requested(sym, true);
                return true;
            }
            if (ke->key() == Qt::Key_S && ke->modifiers() == Qt::NoModifier && !sym.isEmpty()) {
                emit trade_requested(sym, false);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void EquityWatchlist::select_row_by_keyboard(int row) {
    auto* item = table_->item(row, ColSymbol);
    if (!item)
        return;
    pending_key_select_ = item->data(Qt::UserRole).toString();
    key_select_timer_->start();
}

EquityWatchlist::ColumnNeeds EquityWatchlist::column_needs(bool price_floor) const {
    auto* hh = table_->horizontalHeader();
    const QAbstractItemView* view = table_;
    auto need = [&](int c) { return std::max(hh->sectionSizeHint(c), view->sizeHintForColumn(c)); };
    ColumnNeeds n{need(ColSymbol), need(ColLtp), need(ColChg)};
    if (price_floor) {
        const QFontMetrics fm(table_->font());
        const int chrome = 12;
        n.ltp = std::max(n.ltp, chrome + fm.horizontalAdvance(QStringLiteral("00000.00")));
        n.chg = std::max(n.chg, chrome + fm.horizontalAdvance(QStringLiteral("\xE2\x96\xB2 +00.00%")));
    }
    return n;
}

int EquityWatchlist::content_width() const {
    table_->ensurePolished();
    const ColumnNeeds n = column_needs(/*price_floor=*/true);
    const QScrollBar* vbar = table_->verticalScrollBar();
    const int bar = vbar ? vbar->sizeHint().width() : 0;
    const int trend = QFontMetrics(table_->font()).averageCharWidth() * 7;
    return n.symbol + n.ltp + n.chg + trend + bar + 2 * table_->frameWidth();
}

void EquityWatchlist::schedule_fit() {
    if (fit_pending_)
        return;
    fit_pending_ = true;
    QTimer::singleShot(0, this, [this]() { fit_columns(); });
}

void EquityWatchlist::fit_columns() {
    fit_pending_ = false;
    if (!table_ || table_->columnCount() < ColCount)
        return;
    qsizetype ltp_len = 0;
    qsizetype chg_len = 0;
    for (int r = 0; r < table_->rowCount(); ++r) {
        if (const QTableWidgetItem* it = table_->item(r, ColLtp))
            ltp_len = std::max(ltp_len, it->text().size());
        if (const QTableWidgetItem* it = table_->item(r, ColChg))
            chg_len = std::max(chg_len, it->text().size());
    }
    fitted_ltp_len_ = ltp_len;
    fitted_chg_len_ = chg_len;

    const int avail = table_->viewport()->width();
    if (avail <= 0)
        return;
    const ColumnNeeds n = column_needs(/*price_floor=*/false);
    int sym = n.symbol;
    int ltp = n.ltp;
    int chg = n.chg;
    const int min_trend = QFontMetrics(table_->font()).averageCharWidth() * 6;
    int trend = 0;
    int spare = avail - sym - ltp - chg;
    if (spare >= min_trend) {
        // The trend line takes what is left beyond a little air for the numbers.
        trend = spare;
    } else if (spare >= 0) {
        ltp += spare / 2;
        chg += spare - spare / 2;
    } else {
        // Narrower than the content: prices stay whole, the ticker is cut (elided).
        sym = std::max(24, avail - ltp - chg);
    }
    table_->setColumnWidth(ColSymbol, sym);
    table_->setColumnWidth(ColLtp, ltp);
    table_->setColumnWidth(ColChg, chg);
    table_->setColumnWidth(ColTrend, trend);
    table_->setColumnHidden(ColTrend, trend <= 0);
}

void EquityWatchlist::retranslateUi() {
    if (filter_edit_)
        filter_edit_->setPlaceholderText(tr("Filter rows..."));
    if (table_)
        table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LTP"), tr("CHG%"), tr("TREND")});
}

QString EquityWatchlist::symbol_text(const QString& symbol) const {
    const auto it = info_.constFind(symbol);
    if (it == info_.constEnd() || it->display.isEmpty())
        return symbol;
    return it->venue.isEmpty() ? it->display : it->display + QStringLiteral(" \xC2\xB7 ") + it->venue;
}

QString EquityWatchlist::price_text(const QString& symbol, double v) const {
    const auto it = info_.constFind(symbol);
    if (it == info_.constEnd()) {
        fmt::InstrumentFmt inst;
        inst.decimals = 2;
        return fmt::price(v, inst);
    }
    // The column mixes venues (NSE, NASDAQ, LSE ...): each price names its own currency
    // ("₹2,450.10", "$351.71", "117.70p"). An index level or FX rate gets no unit.
    const QString key = it->market_key.isEmpty() ? symbol : it->market_key;
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(key, it->currency);
    inst.decimals = it->digits;
    const bool money = !it->currency.isEmpty() && fmt::is_money_quote(QString(), key) &&
                       it->venue != QLatin1String("FX");
    return money ? fmt::price_with_symbol(v, inst) : fmt::price(v, inst);
}

QString EquityWatchlist::short_reason(const QString& error) {
    const QString e = error.toLower();
    if (e.contains(QLatin1String("not tradable")) || e.contains(QLatin1String("not traded")))
        return tr("not on broker");
    if (e.contains(QLatin1String("not found")) || e.contains(QLatin1String("delisted")) ||
        e.contains(QLatin1String("unknown")))
        return tr("not found");
    if (e.contains(QLatin1String("rate")) || e.contains(QLatin1String("429")) || e.contains(QLatin1String("too many")))
        return tr("rate-limited");
    if (e.contains(QLatin1String("time")) || e.contains(QLatin1String("answer")) || e.contains(QLatin1String("retry")))
        return tr("retrying");
    if (e.contains(QLatin1String("no public")) || e.contains(QLatin1String("derivative")) ||
        e.contains(QLatin1String("index with no")))
        return tr("no public data");
    if (e.contains(QLatin1String("network")) || e.contains(QLatin1String("offline")) ||
        e.contains(QLatin1String("connect")))
        return tr("offline");
    return tr("no quote");
}

void EquityWatchlist::set_symbols(const QStringList& symbols) {
    QMutexLocker lock(&mutex_);
    // Carry the last-known quote forward for symbols that survive a re-set
    // (the list is re-set whenever the watchlist or the held-symbol union changes).
    QHash<QString, WatchlistEntry> previous;
    previous.reserve(entries_.size());
    for (const auto& e : entries_)
        previous.insert(e.symbol, e);
    entries_.clear();
    entries_.reserve(symbols.size());
    for (const auto& sym : symbols) {
        WatchlistEntry e;
        const auto prev = previous.constFind(sym);
        if (prev != previous.constEnd())
            e = prev.value();
        e.symbol = sym;
        entries_.append(e);
    }
    count_label_->setText(QString::number(entries_.size()));
    sort_entries();
    rebuild_table();
    if (visible_)
        subscribe_sparklines();
}

void EquityWatchlist::set_row_info(const QHash<QString, RowInfo>& info) {
    QMutexLocker lock(&mutex_);
    info_ = info;
    for (int i = 0; i < entries_.size() && i < table_->rowCount(); ++i)
        render_row(i, entries_[i]);
    schedule_fit();
    if (visible_)
        subscribe_sparklines();
}

void EquityWatchlist::update_quote(const trading::BrokerQuote& quote) {
    QMutexLocker lock(&mutex_);
    const auto found = row_of_.constFind(quote.symbol);
    if (found == row_of_.constEnd())
        return;
    WatchlistEntry* entry = &entries_[found.value()];
    const bool was_special = !entry->error.isEmpty() || !entry->has_data ||
                             (entry->updated_ms > 0 &&
                              QDateTime::currentMSecsSinceEpoch() - entry->updated_ms > eqwl_kStaleMs);
    entry->ltp = quote.ltp;
    entry->change_pct = quote.change_pct;
    entry->volume = quote.volume;
    entry->has_data = true;
    entry->error.clear();
    entry->updated_ms = QDateTime::currentMSecsSinceEpoch();

    const int row = row_for(quote.symbol);
    if (row < 0)
        return;
    if (was_special) {
        render_row(row, *entry); // one-off: colours, tooltips and markers back to normal
        return;
    }
    if (auto* ltp_item = table_->item(row, ColLtp)) {
        ltp_item->setText(price_text(entry->symbol, entry->ltp));
        if (ltp_item->text().size() > fitted_ltp_len_)
            schedule_fit();
    }
    if (auto* chg_item = table_->item(row, ColChg)) {
        chg_item->setText(eqwl_arrow(entry->change_pct) + fmt::pct(entry->change_pct, 2, true));
        chg_item->setData(Qt::UserRole, entry->change_pct);
        chg_item->setForeground(QColor(entry->change_pct > 0.0   ? fincept::ui::colors::POSITIVE()
                                       : entry->change_pct < 0.0 ? fincept::ui::colors::NEGATIVE()
                                                                 : fincept::ui::colors::TEXT_SECONDARY()));
        if (chg_item->text().size() > fitted_chg_len_)
            schedule_fit();
    }
}

int EquityWatchlist::row_for(const QString& symbol) const {
    const auto found = row_of_.constFind(symbol);
    if (found != row_of_.constEnd()) {
        auto* it = table_->item(found.value(), ColSymbol);
        if (it && it->data(Qt::UserRole).toString() == symbol)
            return found.value();
    }
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* it = table_->item(r, ColSymbol);
        if (it && it->data(Qt::UserRole).toString() == symbol)
            return r;
    }
    return -1;
}

void EquityWatchlist::render_row(int row, const WatchlistEntry& e) {
    namespace c = fincept::ui::colors;
    auto* sym_item = table_->item(row, ColSymbol);
    auto* ltp_item = table_->item(row, ColLtp);
    auto* chg_item = table_->item(row, ColChg);
    auto* trend_item = table_->item(row, ColTrend);
    if (!sym_item || !ltp_item || !chg_item)
        return;
    const RowInfo info = info_.value(e.symbol);
    sym_item->setText(symbol_text(e.symbol));

    const bool failed = !e.error.isEmpty();
    const bool stale = e.has_data && (failed || (e.updated_ms > 0 &&
                                                 QDateTime::currentMSecsSinceEpoch() - e.updated_ms > eqwl_kStaleMs));
    QString tip;
    if (e.has_data) {
        // A price we have: shown, greyed with a clock marker when it is no longer updating.
        ltp_item->setText(price_text(e.symbol, e.ltp) + (stale ? QStringLiteral(" \xE2\x8F\xB1") : QString()));
        ltp_item->setForeground(QColor(stale ? c::TEXT_TERTIARY() : c::TEXT_PRIMARY()));
        chg_item->setText(eqwl_arrow(e.change_pct) + fmt::pct(e.change_pct, 2, true));
        chg_item->setForeground(QColor(stale            ? c::TEXT_TERTIARY()
                                       : e.change_pct > 0 ? c::POSITIVE()
                                       : e.change_pct < 0 ? c::NEGATIVE()
                                                          : c::TEXT_SECONDARY()));
        if (failed)
            tip = tr("%1 is not updating: %2\nShowing the last price received.").arg(symbol_text(e.symbol), e.error);
        else if (stale)
            tip = tr("%1: no update for over %2 minutes — showing the last price received.")
                      .arg(symbol_text(e.symbol))
                      .arg(eqwl_kStaleMs / 60000);
    } else if (failed || !info.blocked_reason.isEmpty()) {
        // No price at all: say why, in the row (never a bare "ERR").
        const QString reason = failed ? short_reason(e.error) : tr("not on broker");
        ltp_item->setText(reason);
        ltp_item->setForeground(QColor(c::WARNING()));
        chg_item->setText(fmt::placeholder());
        chg_item->setForeground(QColor(c::TEXT_TERTIARY()));
        tip = failed ? tr("%1: no quote. %2").arg(symbol_text(e.symbol), e.error) : info.blocked_reason;
    } else {
        // Loading: the first quote has not arrived yet.
        ltp_item->setText(QStringLiteral("\xC2\xB7\xC2\xB7\xC2\xB7"));
        ltp_item->setForeground(QColor(c::TEXT_TERTIARY()));
        chg_item->setText(QString());
        chg_item->setForeground(QColor(c::TEXT_TERTIARY()));
        tip = tr("%1: waiting for the first quote").arg(symbol_text(e.symbol));
    }
    sym_item->setForeground(QColor(!info.blocked_reason.isEmpty() ? c::TEXT_TERTIARY()
                                   : failed && !e.has_data        ? c::TEXT_SECONDARY()
                                                                  : c::TEXT_PRIMARY()));
    QString full_tip = info.tooltip.isEmpty() ? QString() : info.tooltip;
    if (!info.blocked_reason.isEmpty() && !full_tip.contains(info.blocked_reason))
        full_tip += (full_tip.isEmpty() ? QString() : QStringLiteral("\n")) + info.blocked_reason;
    if (!tip.isEmpty())
        full_tip += (full_tip.isEmpty() ? QString() : QStringLiteral("\n")) + tip;
    sym_item->setToolTip(full_tip);
    ltp_item->setToolTip(full_tip);
    chg_item->setToolTip(full_tip);
    ltp_item->setData(Qt::UserRole, e.has_data ? e.ltp : 0.0);
    chg_item->setData(Qt::UserRole, e.has_data ? e.change_pct : 0.0);
    if (trend_item) {
        const auto spark = sparks_.constFind(info.market_key);
        if (spark != sparks_.constEnd())
            trend_item->setData(fincept::ui::SparklineDelegate::kDefaultRole, QVariant::fromValue(spark.value()));
        else
            trend_item->setData(fincept::ui::SparklineDelegate::kDefaultRole, QVariant());
    }
    if (ltp_item->text().size() > fitted_ltp_len_ || chg_item->text().size() > fitted_chg_len_)
        schedule_fit();
}

void EquityWatchlist::set_quote_error(const QString& symbol, const QString& reason) {
    QMutexLocker lock(&mutex_);
    const auto found = row_of_.constFind(symbol);
    if (found == row_of_.constEnd())
        return;
    WatchlistEntry& e = entries_[found.value()];
    if (e.error == reason)
        return; // repeated failure (every retry reports it) — nothing to repaint
    e.error = reason;
    const int row = row_for(symbol);
    if (row >= 0)
        render_row(row, e);
}

void EquityWatchlist::clear_quotes() {
    QMutexLocker lock(&mutex_);
    for (auto& e : entries_) {
        e.ltp = 0.0;
        e.change_pct = 0.0;
        e.volume = 0.0;
        e.has_data = false;
        e.error.clear();
        e.updated_ms = 0;
    }
    for (int i = 0; i < entries_.size() && i < table_->rowCount(); ++i)
        render_row(i, entries_[i]);
}

void EquityWatchlist::set_feed_notice(const QString& title, const QString& detail) {
    const bool show = !title.isEmpty();
    if (show == notice_box_->isVisibleTo(this) && notice_title_->text() == title && notice_detail_->text() == detail)
        return;
    notice_title_->setText(title);
    notice_detail_->setText(detail);
    notice_detail_->setVisible(!detail.isEmpty());
    notice_box_->setAccessibleName(show ? title : QString());
    notice_box_->setVisible(show);
}

void EquityWatchlist::set_active_symbol(const QString& symbol) {
    active_symbol_ = symbol;
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, ColSymbol);
        if (!item)
            continue;
        const bool active = item->data(Qt::UserRole).toString() == symbol;
        for (int col = 0; col < ColCount; ++col) {
            if (auto* cell = table_->item(row, col))
                cell->setBackground(active ? QColor(fincept::ui::colors::BG_HOVER())
                                           : QColor(fincept::ui::colors::BG_BASE()));
        }
    }
}

void EquityWatchlist::on_cell_clicked(int row, int /*col*/) {
    auto* item = table_->item(row, ColSymbol);
    if (!item)
        return;
    const QString symbol = item->data(Qt::UserRole).toString();
    if (symbol.isEmpty())
        return;
    key_select_timer_->stop();
    set_active_symbol(symbol);
    emit symbol_selected(symbol);
}

void EquityWatchlist::on_filter_changed(const QString& text) {
    const QString filter = text.trimmed().toUpper();
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, ColSymbol);
        if (!item)
            continue;
        const bool match = filter.isEmpty() || item->data(Qt::UserRole).toString().toUpper().contains(filter) ||
                           item->text().toUpper().contains(filter) || item->toolTip().toUpper().contains(filter);
        table_->setRowHidden(row, !match);
    }
    schedule_fit();
}

void EquityWatchlist::on_header_clicked(int section) {
    if (section == ColTrend)
        return;
    // Click cycles: ascending → descending → list order.
    if (sort_col_ != section) {
        sort_col_ = section;
        sort_desc_ = (section != ColSymbol); // numbers: biggest first
    } else if (sort_desc_ == (section != ColSymbol)) {
        sort_desc_ = !sort_desc_;
    } else {
        sort_col_ = -1;
    }
    auto* hh = table_->horizontalHeader();
    hh->setSortIndicatorShown(sort_col_ >= 0);
    if (sort_col_ >= 0)
        hh->setSortIndicator(sort_col_, sort_desc_ ? Qt::DescendingOrder : Qt::AscendingOrder);
    {
        QMutexLocker lock(&mutex_);
        sort_entries();
        rebuild_table();
    }
}

void EquityWatchlist::sort_entries() {
    if (sort_col_ < 0)
        return; // list order as given
    const bool desc = sort_desc_;
    const int col = sort_col_;
    std::stable_sort(entries_.begin(), entries_.end(), [this, col, desc](const WatchlistEntry& a, const WatchlistEntry& b) {
        // Rows without data always sink to the bottom.
        if (col != ColSymbol && a.has_data != b.has_data)
            return a.has_data;
        if (col == ColSymbol) {
            const int cmp = QString::compare(symbol_text(a.symbol), symbol_text(b.symbol), Qt::CaseInsensitive);
            return desc ? cmp > 0 : cmp < 0;
        }
        const double va = col == ColLtp ? a.ltp : a.change_pct;
        const double vb = col == ColLtp ? b.ltp : b.change_pct;
        return desc ? va > vb : va < vb;
    });
}

void EquityWatchlist::rebuild_table() {
    const int scroll = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;
    row_of_.clear();
    row_of_.reserve(entries_.size());
    table_->setUpdatesEnabled(false);
    table_->setRowCount(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        if (!row_of_.contains(e.symbol))
            row_of_.insert(e.symbol, i);

        auto* sym_item = new QTableWidgetItem(symbol_text(e.symbol));
        sym_item->setData(Qt::UserRole, e.symbol);
        table_->setItem(i, ColSymbol, sym_item);
        auto* ltp_item = new QTableWidgetItem;
        ltp_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColLtp, ltp_item);
        auto* chg_item = new QTableWidgetItem;
        chg_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(i, ColChg, chg_item);
        table_->setItem(i, ColTrend, new QTableWidgetItem);
        render_row(i, e);
    }
    table_->setUpdatesEnabled(true);
    if (!active_symbol_.isEmpty())
        set_active_symbol(active_symbol_);
    if (filter_edit_ && !filter_edit_->text().trimmed().isEmpty())
        on_filter_changed(filter_edit_->text());
    if (table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(qMin(scroll, table_->verticalScrollBar()->maximum()));
    fit_columns();
}

void EquityWatchlist::subscribe_sparklines() {
    auto& hub = datahub::DataHub::instance();
    QSet<QString> want;
    for (const auto& e : entries_) {
        const QString key = info_.value(e.symbol).market_key;
        if (!key.isEmpty())
            want.insert(key);
    }
    for (auto it = spark_subs_.begin(); it != spark_subs_.end();) {
        if (!want.contains(*it)) {
            hub.unsubscribe(this, fincept::services::MarketDataService::sparkline_topic(*it));
            it = spark_subs_.erase(it);
        } else {
            ++it;
        }
    }
    for (const QString& key : want) {
        if (spark_subs_.contains(key))
            continue;
        spark_subs_.insert(key);
        hub.subscribe(this, fincept::services::MarketDataService::sparkline_topic(key), [this, key](const QVariant& v) {
            if (!v.canConvert<QVector<double>>())
                return;
            on_sparkline(key, v.value<QVector<double>>());
        });
    }
}

void EquityWatchlist::unsubscribe_sparklines() {
    auto& hub = datahub::DataHub::instance();
    for (const QString& key : std::as_const(spark_subs_))
        hub.unsubscribe(this, fincept::services::MarketDataService::sparkline_topic(key));
    spark_subs_.clear();
}

void EquityWatchlist::on_sparkline(const QString& market_key, const QVector<double>& closes) {
    sparks_.insert(market_key, closes);
    for (int r = 0; r < table_->rowCount(); ++r) {
        auto* sym_item = table_->item(r, ColSymbol);
        auto* trend_item = table_->item(r, ColTrend);
        if (!sym_item || !trend_item)
            continue;
        if (info_.value(sym_item->data(Qt::UserRole).toString()).market_key == market_key)
            trend_item->setData(fincept::ui::SparklineDelegate::kDefaultRole, QVariant::fromValue(closes));
    }
}

void EquityWatchlist::set_broker_id(const QString& broker_id) {
    broker_id_ = broker_id;
}

void EquityWatchlist::add_symbol(const QString& symbol) {
    const QString sym = symbol.trimmed();
    if (sym.isEmpty())
        return;
    {
        QMutexLocker lock(&mutex_);
        for (const auto& e : entries_)
            if (e.symbol == sym)
                return; // already in the watchlist
    }
    emit symbol_added(sym);
}

} // namespace fincept::screens::equity
