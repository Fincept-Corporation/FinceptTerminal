// EquityWatchlist.cpp — compact watchlist with live quote updates
#include "screens/equity_trading/EquityWatchlist.h"

#include "screens/equity_trading/EquityTypes.h"
#include "trading/AccountManager.h"
#include "trading/BrokerRegistry.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMutexLocker>
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

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("eqWatchlistHeader");
    header->setFixedHeight(28);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    title_label_ = new QLabel(tr("WATCHLIST"));
    title_label_->setObjectName("eqWatchlistTitle");
    h_layout->addWidget(title_label_);
    h_layout->addStretch();

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

    // Completer backed by InstrumentService::search()
    completer_model_ = new QStringListModel(this);
    completer_ = new QCompleter(completer_model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setFilterMode(Qt::MatchContains);
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
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("LTP"), tr("CHG%")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table_->setColumnWidth(1, 80);
    table_->setColumnWidth(2, 60);
    table_->verticalHeader()->setDefaultSectionSize(24);

    connect(table_, &QTableWidget::cellClicked, this, &EquityWatchlist::on_cell_clicked);
    layout->addWidget(table_, 1);
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
