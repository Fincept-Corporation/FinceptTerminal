#include "screens/watchlist/WatchlistScreen.h"

#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui::colors;

// ── Style constants (Obsidian Design System) ────────────────────────────────

// §5.5 Accent button
static const QString kAccentBtn =
    "QPushButton { background: rgba(217,119,6,0.1); color: #d97706; "
    "border: 1px solid #78350f; padding: 0 12px; height: 24px; "
    "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #d97706; color: #080808; }";

// §5.5 Standard button
static const QString kStdBtn = "QPushButton { background: #111111; color: #808080; border: 1px solid #1a1a1a; "
                               "padding: 0 10px; height: 24px; "
                               "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
                               "QPushButton:hover { color: #e5e5e5; background: #161616; }";

// §5.5 Danger button
static const QString kDangerBtn =
    "QPushButton { background: rgba(220,38,38,0.1); color: #dc2626; "
    "border: 1px solid #7f1d1d; padding: 0 10px; height: 24px; "
    "font-size: 11px; font-weight: 700; font-family: 'Consolas','Courier New',monospace; }"
    "QPushButton:hover { background: #dc2626; color: #e5e5e5; }";

// §5.6 Input
static const QString kInput =
    "QLineEdit { background: #0a0a0a; color: #e5e5e5; border: 1px solid #1a1a1a; "
    "padding: 3px 6px; font-size: 13px; font-family: 'Consolas','Courier New',monospace; height: 28px; }"
    "QLineEdit:focus { border-color: #333333; }"
    "QLineEdit::selection { background: #d97706; color: #080808; }";

// §5.2 List
static const QString kList =
    "QListWidget { background: %1; border: none; font-family: 'Consolas','Courier New',monospace; font-size: 12px; }"
    "QListWidget::item { padding: 6px 12px; color: #e5e5e5; border-bottom: 1px solid %2; height: 26px; }"
    "QListWidget::item:selected { background: #161616; }"
    "QListWidget::item:hover { background: #161616; }";

// ── Constructor ──────────────────────────────────────────────────────────────

WatchlistScreen::WatchlistScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(BG_BASE));
    build_ui();
    load_watchlists();

    // Auto-refresh every 30 seconds — starts via showEvent, not eagerly
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &WatchlistScreen::on_refresh);
    refresh_timer_->setInterval(30000);
}

void WatchlistScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (refresh_timer_)
        refresh_timer_->start();
    on_refresh();
}

void WatchlistScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (refresh_timer_)
        refresh_timer_->stop();
}

void WatchlistScreen::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setStyleSheet("QSplitter::handle { background: #1a1a1a; width: 1px; }");

    splitter->addWidget(build_sidebar());
    splitter->addWidget(build_main_panel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    root->addWidget(splitter);
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_sidebar() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(180);
    panel->setMaximumWidth(280);
    panel->setStyleSheet(QString("background: %1; border-right: 1px solid %2;").arg(BG_SURFACE, BORDER_DIM));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // §5.1 Panel header — 34px, #111111 bg, amber title
    auto* header = new QWidget;
    header->setFixedHeight(34);
    header->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG_RAISED, BORDER_DIM));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(12, 0, 8, 0);
    hl->setSpacing(6);

    auto* title = new QLabel("WATCHLISTS");
    title->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: 700; letter-spacing: 0.5px; "
                                 "font-family: 'Consolas','Courier New',monospace; background: transparent;")
                             .arg(AMBER));
    hl->addWidget(title);
    hl->addStretch();

    auto* add_wl_btn = new QPushButton("+");
    add_wl_btn->setFixedSize(24, 24);
    add_wl_btn->setStyleSheet(kAccentBtn);
    connect(add_wl_btn, &QPushButton::clicked, this, &WatchlistScreen::on_add_watchlist);
    hl->addWidget(add_wl_btn);

    lay->addWidget(header);

    // Watchlist list
    wl_list_ = new QListWidget;
    wl_list_->setStyleSheet(kList.arg(BG_SURFACE, BORDER_DIM));
    connect(wl_list_, &QListWidget::currentRowChanged, this, &WatchlistScreen::on_watchlist_selected);
    lay->addWidget(wl_list_);

    // Footer count
    wl_count_ = new QLabel("0 lists");
    wl_count_->setFixedHeight(26);
    wl_count_->setAlignment(Qt::AlignCenter);
    wl_count_->setStyleSheet(QString("background: %1; color: %2; font-size: 11px; border-top: 1px solid %3; "
                                     "font-family: 'Consolas','Courier New',monospace;")
                                 .arg(BG_SURFACE, TEXT_TERTIARY, BORDER_DIM));
    lay->addWidget(wl_count_);

    return panel;
}

// ── Main Panel ───────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_main_panel() {
    auto* panel = new QWidget;
    panel->setStyleSheet(QString("background: %1;").arg(BG_BASE));

    auto* lay = new QVBoxLayout(panel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Top bar: title + controls
    auto* top_bar = new QWidget;
    top_bar->setFixedHeight(34);
    top_bar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG_RAISED, BORDER_DIM));
    auto* tl = new QHBoxLayout(top_bar);
    tl->setContentsMargins(14, 0, 14, 0);
    tl->setSpacing(8);

    panel_title_ = new QLabel("Select a watchlist");
    panel_title_->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 700; "
                                        "font-family: 'Consolas','Courier New',monospace; background: transparent;")
                                    .arg(TEXT_PRIMARY));
    tl->addWidget(panel_title_);

    tl->addStretch();

    stock_count_ = new QLabel;
    stock_count_->setStyleSheet(QString("color: %1; font-size: 11px; font-family: 'Consolas','Courier New',monospace; "
                                        "background: transparent;")
                                    .arg(TEXT_TERTIARY));
    tl->addWidget(stock_count_);

    auto* refresh_btn = new QPushButton("REFRESH");
    refresh_btn->setStyleSheet(kStdBtn);
    connect(refresh_btn, &QPushButton::clicked, this, &WatchlistScreen::on_refresh);
    tl->addWidget(refresh_btn);

    auto* del_wl_btn = new QPushButton("DELETE LIST");
    del_wl_btn->setStyleSheet(kDangerBtn);
    connect(del_wl_btn, &QPushButton::clicked, this, &WatchlistScreen::on_delete_watchlist);
    tl->addWidget(del_wl_btn);

    lay->addWidget(top_bar);

    // Add stock bar
    auto* add_bar = new QWidget;
    add_bar->setFixedHeight(34);
    add_bar->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;").arg(BG_SURFACE, BORDER_DIM));
    auto* al = new QHBoxLayout(add_bar);
    al->setContentsMargins(14, 0, 14, 0);
    al->setSpacing(6);

    auto* add_label = new QLabel("ADD:");
    add_label->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; letter-spacing: 0.5px; "
                                     "font-family: 'Consolas','Courier New',monospace; background: transparent;")
                                 .arg(TEXT_SECONDARY));
    al->addWidget(add_label);

    add_input_ = new QLineEdit;
    add_input_->setPlaceholderText("AAPL, MSFT, TSLA...");
    add_input_->setStyleSheet(kInput);
    add_input_->setFixedHeight(28);
    al->addWidget(add_input_, 1);

    auto* add_btn = new QPushButton("ADD");
    add_btn->setStyleSheet(kAccentBtn);
    connect(add_btn, &QPushButton::clicked, this, &WatchlistScreen::on_add_stock);
    // Also add on Enter key
    connect(add_input_, &QLineEdit::returnPressed, this, &WatchlistScreen::on_add_stock);
    al->addWidget(add_btn);

    auto* remove_btn = new QPushButton("REMOVE SELECTED");
    remove_btn->setStyleSheet(kDangerBtn);
    connect(remove_btn, &QPushButton::clicked, this, &WatchlistScreen::on_remove_stock);
    al->addWidget(remove_btn);

    lay->addWidget(add_bar);

    // §5.4 Table — the main data area
    table_ = new ui::DataTable;
    table_->set_headers({"SYMBOL", "NAME", "PRICE", "CHANGE", "CHG %", "HIGH", "LOW", "VOLUME"});
    table_->set_column_widths({100, 160, 100, 90, 80, 90, 90, 110});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(table_, 1);

    return panel;
}

// ── Data Loading ─────────────────────────────────────────────────────────────

void WatchlistScreen::load_watchlists() {
    auto r = fincept::WatchlistRepository::instance().list_all();
    if (r.is_err()) {
        LOG_ERROR("Watchlist", "Failed to load watchlists");
        watchlists_.clear();
    } else {
        watchlists_ = r.value();
    }

    // Ensure at least one default watchlist exists
    if (watchlists_.isEmpty()) {
        auto cr = fincept::WatchlistRepository::instance().create("Default", "#d97706");
        if (cr.is_ok()) {
            watchlists_.append(cr.value());
            // Add some starter symbols
            auto& repo = fincept::WatchlistRepository::instance();
            repo.add_stock(watchlists_.first().id, "AAPL", "Apple Inc");
            repo.add_stock(watchlists_.first().id, "MSFT", "Microsoft Corp");
            repo.add_stock(watchlists_.first().id, "GOOGL", "Alphabet Inc");
            repo.add_stock(watchlists_.first().id, "AMZN", "Amazon.com Inc");
            repo.add_stock(watchlists_.first().id, "NVDA", "NVIDIA Corp");
            repo.add_stock(watchlists_.first().id, "TSLA", "Tesla Inc");
            repo.add_stock(watchlists_.first().id, "META", "Meta Platforms");
            repo.add_stock(watchlists_.first().id, "JPM", "JPMorgan Chase");
        }
    }

    wl_list_->blockSignals(true);
    wl_list_->clear();
    for (const auto& wl : watchlists_) {
        wl_list_->addItem(wl.name);
    }
    wl_list_->blockSignals(false);

    wl_count_->setText(QString("%1 lists").arg(watchlists_.size()));

    // Select first watchlist
    if (!watchlists_.isEmpty()) {
        wl_list_->setCurrentRow(0);
    }
}

void WatchlistScreen::load_stocks() {
    if (current_wl_id_.isEmpty())
        return;

    auto r = fincept::WatchlistRepository::instance().get_stocks(current_wl_id_);
    if (r.is_err()) {
        stocks_.clear();
    } else {
        stocks_ = r.value();
    }

    stock_count_->setText(QString("%1 symbols").arg(stocks_.size()));
    fetch_quotes();
}

void WatchlistScreen::fetch_quotes() {
    if (stocks_.isEmpty()) {
        table_->clear_data();
        return;
    }

    QStringList symbols;
    for (const auto& s : stocks_) {
        symbols << s.symbol;
    }

    services::MarketDataService::instance().fetch_quotes(symbols, [this](bool ok, QVector<services::QuoteData> quotes) {
        if (!ok || quotes.isEmpty()) {
            // Show symbols without price data
            table_->clear_data();
            for (const auto& s : stocks_) {
                table_->add_row({s.symbol, s.name, "--", "--", "--", "--", "--", "--"});
            }
            return;
        }
        populate_table(quotes);
    });
}

void WatchlistScreen::populate_table(const QVector<services::QuoteData>& quotes) {
    table_->clear_data();

    // Build a map for quick lookup
    QMap<QString, services::QuoteData> quote_map;
    for (const auto& q : quotes) {
        quote_map[q.symbol] = q;
    }

    for (const auto& s : stocks_) {
        auto it = quote_map.find(s.symbol);
        if (it != quote_map.end()) {
            const auto& q = it.value();
            table_->add_row({q.symbol, q.name.isEmpty() ? s.name : q.name, QString("$%1").arg(q.price, 0, 'f', 2),
                             QString("%1%2").arg(q.change >= 0 ? "+" : "").arg(q.change, 0, 'f', 2),
                             QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2),
                             QString("$%1").arg(q.high, 0, 'f', 2), QString("$%1").arg(q.low, 0, 'f', 2),
                             QString::number(static_cast<qint64>(q.volume))});

            int row = table_->rowCount() - 1;
            // §8 Rule 6: Green = good, Red = bad
            QString chg_color = q.change_pct >= 0 ? POSITIVE : NEGATIVE;
            table_->set_cell_color(row, 3, chg_color);
            table_->set_cell_color(row, 4, chg_color);
        } else {
            table_->add_row({s.symbol, s.name, "--", "--", "--", "--", "--", "--"});
        }
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void WatchlistScreen::on_watchlist_selected(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    current_wl_id_ = watchlists_[row].id;
    panel_title_->setText(watchlists_[row].name.toUpper());
    load_stocks();
}

void WatchlistScreen::on_add_watchlist() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "New Watchlist", "Name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    auto r = fincept::WatchlistRepository::instance().create(name.trimmed());
    if (r.is_ok()) {
        load_watchlists();
        // Select the new one (last in list)
        wl_list_->setCurrentRow(watchlists_.size() - 1);
    }
}

void WatchlistScreen::on_delete_watchlist() {
    if (current_wl_id_.isEmpty())
        return;

    auto reply = QMessageBox::question(this, "Delete Watchlist",
                                       "Are you sure you want to delete this watchlist and all its stocks?",
                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    fincept::WatchlistRepository::instance().remove(current_wl_id_);
    current_wl_id_.clear();
    table_->clear_data();
    panel_title_->setText("Select a watchlist");
    stock_count_->clear();
    load_watchlists();
}

void WatchlistScreen::on_add_stock() {
    if (current_wl_id_.isEmpty())
        return;

    QString text = add_input_->text().trimmed().toUpper();
    if (text.isEmpty())
        return;

    auto& repo = fincept::WatchlistRepository::instance();
    for (auto& s : text.split(",")) {
        QString symbol = s.trimmed();
        if (!symbol.isEmpty()) {
            repo.add_stock(current_wl_id_, symbol);
        }
    }

    add_input_->clear();
    load_stocks();
}

void WatchlistScreen::on_remove_stock() {
    if (current_wl_id_.isEmpty())
        return;

    int row = table_->currentRow();
    if (row < 0 || row >= stocks_.size())
        return;

    QString symbol = stocks_[row].symbol;
    fincept::WatchlistRepository::instance().remove_stock(current_wl_id_, symbol);
    load_stocks();
}

void WatchlistScreen::on_refresh() {
    if (!current_wl_id_.isEmpty()) {
        fetch_quotes();
    }
}

} // namespace fincept::screens
