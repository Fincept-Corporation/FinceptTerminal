// EquityWatchlist.cpp — compact watchlist with live quote updates
#include "screens/equity_trading/EquityWatchlist.h"

#include "screens/equity_trading/EquityTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMutexLocker>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::equity {

namespace {

// Distinct broker ids for every connected (active) account, with the focused
// broker first so its matches sort to the top of unified search. Falls back to
// just the focused broker when no accounts are enumerated.
QStringList connected_broker_ids(const QString& active) {
    QStringList ids;
    if (!active.isEmpty())
        ids << active;
    for (const auto& a : trading::AccountManager::instance().active_accounts()) {
        if (!a.broker_id.isEmpty() && !ids.contains(a.broker_id))
            ids << a.broker_id;
    }
    return ids;
}

// Human-readable broker name for the suggestion tag.
QString broker_display(const QString& broker_id) {
    auto* b = trading::BrokerRegistry::instance().get(broker_id);
    return b ? b->profile().display_name : broker_id;
}

// True if at least one of the connected brokers has its instrument catalog loaded.
bool any_loaded(const QStringList& ids) {
    for (const QString& id : ids)
        if (trading::InstrumentService::instance().is_loaded(id))
            return true;
    return false;
}

} // namespace

EquityWatchlist::EquityWatchlist(QWidget* parent) : QWidget(parent) {
    setObjectName("eqWatchlist");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header — named-watchlist switcher (combo) + manage menu + symbol count
    auto* header = new QWidget(this);
    header->setObjectName("eqWatchlistHeader");
    header->setFixedHeight(30);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(6, 2, 6, 2);
    h_layout->setSpacing(4);

    wl_combo_ = new QComboBox;
    wl_combo_->setObjectName("eqWatchlistCombo");
    wl_combo_->setToolTip(tr("Active watchlist"));
    wl_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(wl_combo_, qOverload<int>(&QComboBox::activated), this,
            &EquityWatchlist::on_watchlist_combo_activated);
    h_layout->addWidget(wl_combo_, 1);

    wl_menu_btn_ = new QToolButton;
    wl_menu_btn_->setObjectName("eqWatchlistMenuBtn");
    wl_menu_btn_->setText(QStringLiteral("⋯"));
    wl_menu_btn_->setCursor(Qt::PointingHandCursor);
    wl_menu_btn_->setAutoRaise(true);
    wl_menu_btn_->setToolTip(tr("New / Rename / Delete watchlist"));
    connect(wl_menu_btn_, &QToolButton::clicked, this, &EquityWatchlist::on_watchlist_menu);
    h_layout->addWidget(wl_menu_btn_);

    count_label_ = new QLabel("0");
    count_label_->setObjectName("eqWatchlistCount");
    h_layout->addWidget(count_label_);

    layout->addWidget(header);

    // Search / filter
    filter_edit_ = new QLineEdit;
    filter_edit_->setObjectName("eqWatchlistSearch");
    filter_edit_->setPlaceholderText(tr("Filter..."));
    filter_edit_->setFixedHeight(26);
    connect(filter_edit_, &QLineEdit::textChanged, this, &EquityWatchlist::on_filter_changed);
    layout->addWidget(filter_edit_);

    // Add-symbol row
    auto* add_row = new QWidget(this);
    add_row->setFixedHeight(28);
    auto* add_layout = new QHBoxLayout(add_row);
    add_layout->setContentsMargins(4, 1, 4, 1);
    add_layout->setSpacing(3);

    add_edit_ = new QLineEdit;
    add_edit_->setObjectName("eqWatchlistAddEdit");
    add_edit_->setPlaceholderText(tr("Add symbol..."));
    add_edit_->setFixedHeight(22);
    connect(add_edit_, &QLineEdit::textChanged, this, &EquityWatchlist::on_add_text_changed);
    connect(add_edit_, &QLineEdit::returnPressed, this, &EquityWatchlist::on_add_symbol_entered);

    // Completer backed by InstrumentService::search_all(). The model already holds
    // server-side-filtered results, so use UnfilteredPopupCompletion — otherwise
    // the completer re-filters them against the typed text and hides name-based
    // matches (e.g. typing "infosys" finds symbol "INFY", whose display string
    // doesn't contain "infosys", so a MatchContains filter would drop it).
    // Parent the completer to the edit and the model to the completer so teardown
    // frees the completer (and its internal QCompletionModel) before the source
    // model — avoiding a QCompletionModel::filter crash on a half-destroyed model.
    completer_ = new QCompleter(add_edit_);
    completer_model_ = new QStringListModel(completer_);
    completer_->setModel(completer_model_);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer_->setMaxVisibleItems(8);
    add_edit_->setCompleter(completer_);

    add_btn_ = new QPushButton("+");
    add_btn_->setObjectName("eqWatchlistAddBtn");
    add_btn_->setFixedSize(22, 22);
    add_btn_->setCursor(Qt::PointingHandCursor);
    connect(add_btn_, &QPushButton::clicked, this, &EquityWatchlist::on_add_symbol_entered);

    add_layout->addWidget(add_edit_, 1);
    add_layout->addWidget(add_btn_);
    layout->addWidget(add_row);

    // Table
    table_ = new QTableWidget;
    table_->setObjectName("eqWatchlistTable");
    // Pin the monospace data font explicitly (like every other terminal table).
    // The global `*{font-family}` rule is the lowest-specificity selector and
    // item-view delegates don't reliably honour it for cell text, so without this
    // the LTP/CHG numbers fall back to a proportional font and render uneven.
    table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; gridline-color:%3;"
                " font-size:%4px; font-family:%5; }"
                "QTableWidget::item { padding:1px 4px; border:none; }"
                "QTableWidget::item:selected { background:%6; color:%2; }"
                "QHeaderView::section { background:%7; color:%8; font-size:%4px; font-weight:700;"
                " font-family:%5; border:none; border-bottom:1px solid %3; padding:2px 4px; }")
            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::TEXT_PRIMARY(),
                 fincept::ui::colors::BORDER_DIM())
            .arg(fincept::ui::fonts::SMALL)
            .arg(fincept::ui::fonts::DATA_FAMILY(), fincept::ui::colors::BG_HOVER(),
                 fincept::ui::colors::BG_RAISED(), fincept::ui::colors::TEXT_TERTIARY()));
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LTP"), tr("CHG%")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    // SYMBOL (col 0) is the SOLE stretch column so it claims all leftover width and
    // shows full names; LTP/CHG stay fixed and compact. stretchLastSection must be
    // OFF — with it on, the last column (CHG%) overrides its fixed width and steals
    // room from SYMBOL, which is what truncated the names.
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->horizontalHeader()->setMinimumSectionSize(48);
    table_->setColumnWidth(1, 84); // LTP — fits up to "24500.50" without eliding
    table_->setColumnWidth(2, 64); // CHG% — fits "+12.34%"
    table_->verticalHeader()->setDefaultSectionSize(24);

    connect(table_, &QTableWidget::cellClicked, this, &EquityWatchlist::on_cell_clicked);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableWidget::customContextMenuRequested, this,
            &EquityWatchlist::on_table_context_menu);
    layout->addWidget(table_, 1);
}

void EquityWatchlist::set_watchlists(const QVector<QPair<QString, QString>>& lists,
                                     const QString& active_id) {
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
            QInputDialog::getText(this, tr("New Watchlist"), tr("Name:"), QLineEdit::Normal, {}, &ok)
                .trimmed();
        if (ok && !name.isEmpty())
            emit watchlist_create_requested(name);
    } else if (chosen == rename_act && !cur_id.isEmpty()) {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename Watchlist"), tr("Name:"),
                                                   QLineEdit::Normal, cur_name, &ok)
                                 .trimmed();
        if (ok && !name.isEmpty())
            emit watchlist_rename_requested(cur_id, name);
    } else if (chosen == delete_act && !cur_id.isEmpty()) {
        const auto btn = QMessageBox::question(this, tr("Delete Watchlist"),
                                               tr("Delete \"%1\"?").arg(cur_name));
        if (btn == QMessageBox::Yes)
            emit watchlist_delete_requested(cur_id);
    }
}

void EquityWatchlist::on_table_context_menu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0)
        return;
    auto* item = table_->item(row, 0);
    if (!item)
        return;
    const QString sym = item->data(Qt::UserRole).toString();
    if (sym.isEmpty())
        return;

    QMenu menu(this);
    QAction* remove_act = menu.addAction(tr("Remove %1").arg(sym));
    if (menu.exec(table_->viewport()->mapToGlobal(pos)) == remove_act)
        emit symbol_removed(sym);
}

void EquityWatchlist::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityWatchlist::retranslateUi() {
    if (title_label_)  title_label_->setText(tr("WATCHLIST"));
    if (filter_edit_)  filter_edit_->setPlaceholderText(tr("Filter..."));
    if (add_edit_)     add_edit_->setPlaceholderText(tr("Add symbol..."));
    if (table_)        table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LTP"), tr("CHG%")});
}

void EquityWatchlist::set_symbols(const QStringList& symbols) {
    QMutexLocker lock(&mutex_);
    entries_.clear();
    entries_.reserve(symbols.size());
    for (const auto& sym : symbols) {
        WatchlistEntry e;
        e.symbol = sym;
        entries_.append(e);
    }
    count_label_->setText(QString::number(entries_.size()));
    rebuild_table();
}

void EquityWatchlist::update_quotes(const QVector<trading::BrokerQuote>& quotes) {
    QMutexLocker lock(&mutex_);
    for (const auto& q : quotes) {
        for (auto& e : entries_) {
            if (e.symbol == q.symbol) {
                e.ltp = q.ltp;
                e.change_pct = q.change_pct;
                e.volume = q.volume;
                e.has_data = true;
                break;
            }
        }
    }
    // Update visible rows without full rebuild
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* sym_item = table_->item(row, 0);
        if (!sym_item)
            continue;
        const QString sym = sym_item->data(Qt::UserRole).toString();
        for (const auto& e : entries_) {
            if (e.symbol == sym && e.has_data) {
                auto* ltp_item = table_->item(row, 1);
                auto* chg_item = table_->item(row, 2);
                if (ltp_item)
                    ltp_item->setText(QString::number(e.ltp, 'f', 2));
                if (chg_item) {
                    chg_item->setText(QString("%1%2%").arg(e.change_pct >= 0 ? "+" : "").arg(e.change_pct, 0, 'f', 2));
                    chg_item->setForeground(e.change_pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                                              : QColor(fincept::ui::colors::NEGATIVE()));
                }
                break;
            }
        }
    }
}

void EquityWatchlist::set_active_symbol(const QString& symbol) {
    active_symbol_ = symbol;
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        if (!item)
            continue;
        const bool active = item->data(Qt::UserRole).toString() == symbol;
        for (int col = 0; col < 3; ++col) {
            auto* cell = table_->item(row, col);
            if (cell) {
                cell->setBackground(active ? QColor(fincept::ui::colors::BG_HOVER())
                                           : QColor(fincept::ui::colors::BG_BASE()));
            }
        }
    }
}

void EquityWatchlist::on_cell_clicked(int row, int /*col*/) {
    auto* item = table_->item(row, 0);
    if (item) {
        const QString symbol = item->data(Qt::UserRole).toString();
        if (!symbol.isEmpty()) {
            set_active_symbol(symbol);
            emit symbol_selected(symbol);
        }
    }
}

void EquityWatchlist::on_filter_changed(const QString& text) {
    const QString filter = text.trimmed().toUpper();
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* item = table_->item(row, 0);
        if (!item)
            continue;
        const bool match = filter.isEmpty() || item->data(Qt::UserRole).toString().contains(filter);
        table_->setRowHidden(row, !match);
    }
}

void EquityWatchlist::rebuild_table() {
    table_->setRowCount(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];

        auto* sym_item = new QTableWidgetItem(e.symbol);
        sym_item->setData(Qt::UserRole, e.symbol);
        sym_item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table_->setItem(i, 0, sym_item);

        auto* ltp_item = new QTableWidgetItem(e.has_data ? QString::number(e.ltp, 'f', 2) : "--");
        ltp_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ltp_item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table_->setItem(i, 1, ltp_item);

        auto* chg_item = new QTableWidgetItem(
            e.has_data ? QString("%1%2%").arg(e.change_pct >= 0 ? "+" : "").arg(e.change_pct, 0, 'f', 2) : "--");
        chg_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        chg_item->setForeground(e.change_pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                                  : QColor(fincept::ui::colors::NEGATIVE()));
        table_->setItem(i, 2, chg_item);
    }
}

void EquityWatchlist::set_broker_id(const QString& broker_id) {
    broker_id_ = broker_id;
}

void EquityWatchlist::add_symbol(const QString& symbol) {
    if (symbol.trimmed().isEmpty())
        return;
    const QString sym = symbol.trimmed().toUpper();
    {
        QMutexLocker lock(&mutex_);
        for (const auto& e : entries_)
            if (e.symbol == sym)
                return; // already in watchlist
        WatchlistEntry entry;
        entry.symbol = sym;
        entries_.append(entry);
        count_label_->setText(QString::number(entries_.size()));
        rebuild_table();
    }
    emit symbol_added(sym);
}

void EquityWatchlist::on_add_symbol_entered() {
    const QString raw = add_edit_->text().trimmed().toUpper();
    if (raw.isEmpty())
        return;

    // The completer suggestion is "SYMBOL  ·  EXCH  ·  Broker" — keep only the
    // leading symbol token. Also tolerate the legacy "EXCHANGE:SYMBOL" form.
    QString sym = raw.section(QStringLiteral("  ·  "), 0, 0).trimmed();
    if (sym.contains(':'))
        sym = sym.section(':', 1);

    // Validate against every connected broker (not just the focused one) so a
    // symbol that only one connected broker carries is still accepted.
    const QStringList ids = connected_broker_ids(broker_id_);
    if (any_loaded(ids)) {
        auto results = trading::InstrumentService::instance().search_all(sym, "", ids, 1);
        if (results.isEmpty()) {
            // Not found — flash the edit red briefly to signal invalid
            add_edit_->setStyleSheet(QString("border: 1px solid %1;").arg(fincept::ui::colors::NEGATIVE()));
            QTimer::singleShot(800, add_edit_, [this]() { add_edit_->setStyleSheet(""); });
            return;
        }
        sym = results.first().symbol;
    }

    add_symbol(sym);
    add_edit_->clear();
}

void EquityWatchlist::on_add_text_changed(const QString& text) {
    if (text.trimmed().length() < 2)
        return;
    const QStringList ids = connected_broker_ids(broker_id_);
    if (ids.isEmpty())
        return;

    // Unified search across all connected brokers — one row per (broker,
    // instrument). search_all hits the SQLite catalog directly, so we do NOT
    // gate on the in-memory cache (is_loaded) here: as soon as a broker's
    // master is in the DB the symbols are searchable, even if the in-memory
    // cache rebuild hasn't landed yet. The symbol stays the leading token (so
    // prefix completion on the typed text still works); exchange + broker follow.
    const QString query = text.trimmed().toUpper();
    auto results = trading::InstrumentService::instance().search_all(query, "", ids, 24);
    QStringList suggestions;
    suggestions.reserve(results.size());
    for (const auto& inst : results) {
        suggestions.append(
            QString("%1  ·  %2  ·  %3").arg(inst.symbol, inst.exchange, broker_display(inst.broker_id)));
    }
    completer_model_->setStringList(suggestions);
    // Force the popup to refresh against the just-updated model (otherwise the
    // completer can filter against the previous, stale/empty model for this
    // keystroke and show nothing).
    if (!suggestions.isEmpty())
        completer_->complete();
}

} // namespace fincept::screens::equity
