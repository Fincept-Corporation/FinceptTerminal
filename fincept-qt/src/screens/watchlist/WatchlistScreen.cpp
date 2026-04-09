#include "screens/watchlist/WatchlistScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ── Style builders (read live theme tokens) ─────────────────────────────────

static QString accent_btn_style() {
    QColor acc(colors::AMBER());
    auto rgb = QString("%1,%2,%3").arg(acc.red()).arg(acc.green()).arg(acc.blue());
    return QString("QPushButton { background:rgba(%1,0.1); color:%2; "
                   "border:1px solid %3; padding:0 12px; height:24px; "
                   "font-size:%4px; font-weight:700; font-family:%5; }"
                   "QPushButton:hover { background:%2; color:%6; }")
        .arg(rgb)
        .arg(colors::AMBER())
        .arg(colors::AMBER_DIM())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::BG_BASE());
}

static QString std_btn_style() {
    return QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                   "padding:0 10px; height:24px; "
                   "font-size:%4px; font-weight:700; font-family:%5; }"
                   "QPushButton:hover { color:%6; background:%7; }")
        .arg(colors::BG_RAISED())
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_DIM())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BG_HOVER());
}

static QString danger_btn_style() {
    QColor neg(colors::NEGATIVE());
    auto rgb = QString("%1,%2,%3").arg(neg.red()).arg(neg.green()).arg(neg.blue());
    return QString("QPushButton { background:rgba(%1,0.1); color:%2; "
                   "border:1px solid rgba(%1,0.3); padding:0 10px; height:24px; "
                   "font-size:%3px; font-weight:700; font-family:%4; }"
                   "QPushButton:hover { background:%2; color:%5; }")
        .arg(rgb)
        .arg(colors::NEGATIVE())
        .arg(fonts::TINY)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::TEXT_PRIMARY());
}

static QString input_style() {
    QColor acc(colors::AMBER());
    auto acc_rgb = QString("%1,%2,%3").arg(acc.red()).arg(acc.green()).arg(acc.blue());
    return QString("QLineEdit { background:%1; color:%2; border:1px solid %3; "
                   "padding:3px 6px; font-size:%4px; font-family:%5; height:28px; }"
                   "QLineEdit:focus { border-color:%6; }"
                   "QLineEdit::selection { background:%7; color:%8; }")
        .arg(colors::BG_BASE())
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_DIM())
        .arg(fonts::SMALL)
        .arg(fonts::DATA_FAMILY())
        .arg(colors::BORDER_BRIGHT())
        .arg(colors::AMBER())
        .arg(colors::BG_BASE());
}

static QString list_style() {
    return QString("QListWidget { background:%1; border:none; font-family:%2; font-size:%3px; }"
                   "QListWidget::item { padding:6px 12px; color:%4; border-bottom:1px solid %5; height:26px; }"
                   "QListWidget::item:selected { background:%6; }"
                   "QListWidget::item:hover { background:%6; }")
        .arg(colors::BG_SURFACE())
        .arg(fonts::DATA_FAMILY())
        .arg(fonts::TINY)
        .arg(colors::TEXT_PRIMARY())
        .arg(colors::BORDER_DIM())
        .arg(colors::BG_HOVER());
}

// ── Constructor ──────────────────────────────────────────────────────────────

WatchlistScreen::WatchlistScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    load_watchlists();

    // Auto-refresh every 30 seconds — starts via showEvent, not eagerly
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, &QTimer::timeout, this, &WatchlistScreen::on_refresh);
    refresh_timer_->setInterval(30000);

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
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

    splitter_ = new QSplitter(Qt::Horizontal);

    splitter_->addWidget(build_sidebar());
    splitter_->addWidget(build_main_panel());
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);

    root->addWidget(splitter_);
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_sidebar() {
    sidebar_ = new QWidget(this);
    sidebar_->setMinimumWidth(180);
    sidebar_->setMaximumWidth(280);

    auto* lay = new QVBoxLayout(sidebar_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Panel header
    sidebar_header_ = new QWidget(this);
    sidebar_header_->setFixedHeight(34);
    auto* hl = new QHBoxLayout(sidebar_header_);
    hl->setContentsMargins(12, 0, 8, 0);
    hl->setSpacing(6);

    sidebar_title_ = new QLabel("WATCHLISTS");
    hl->addWidget(sidebar_title_);
    hl->addStretch();

    add_wl_btn_ = new QPushButton("+");
    add_wl_btn_->setFixedSize(24, 24);
    connect(add_wl_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_watchlist);
    hl->addWidget(add_wl_btn_);

    lay->addWidget(sidebar_header_);

    // Watchlist list
    wl_list_ = new QListWidget;
    connect(wl_list_, &QListWidget::currentRowChanged, this, &WatchlistScreen::on_watchlist_selected);
    lay->addWidget(wl_list_);

    // Footer count
    wl_count_ = new QLabel("0 lists");
    wl_count_->setFixedHeight(26);
    wl_count_->setAlignment(Qt::AlignCenter);
    lay->addWidget(wl_count_);

    return sidebar_;
}

// ── Main Panel ───────────────────────────────────────────────────────────────

QWidget* WatchlistScreen::build_main_panel() {
    main_panel_ = new QWidget(this);

    auto* lay = new QVBoxLayout(main_panel_);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Top bar: title + controls
    top_bar_ = new QWidget(this);
    top_bar_->setFixedHeight(34);
    auto* tl = new QHBoxLayout(top_bar_);
    tl->setContentsMargins(14, 0, 14, 0);
    tl->setSpacing(8);

    panel_title_ = new QLabel("Select a watchlist");
    tl->addWidget(panel_title_);

    tl->addStretch();

    stock_count_ = new QLabel;
    tl->addWidget(stock_count_);

    refresh_btn_ = new QPushButton("REFRESH");
    connect(refresh_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_refresh);
    tl->addWidget(refresh_btn_);

    del_wl_btn_ = new QPushButton("DELETE LIST");
    connect(del_wl_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_delete_watchlist);
    tl->addWidget(del_wl_btn_);

    lay->addWidget(top_bar_);

    // Add stock bar
    add_bar_ = new QWidget(this);
    add_bar_->setFixedHeight(34);
    auto* al = new QHBoxLayout(add_bar_);
    al->setContentsMargins(14, 0, 14, 0);
    al->setSpacing(6);

    add_label_ = new QLabel("ADD:");
    al->addWidget(add_label_);

    add_input_ = new QLineEdit;
    add_input_->setPlaceholderText("AAPL, MSFT, TSLA...");
    add_input_->setFixedHeight(28);
    al->addWidget(add_input_, 1);

    add_btn_ = new QPushButton("ADD");
    connect(add_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_add_stock);
    connect(add_input_, &QLineEdit::returnPressed, this, &WatchlistScreen::on_add_stock);
    al->addWidget(add_btn_);

    remove_btn_ = new QPushButton("REMOVE SELECTED");
    connect(remove_btn_, &QPushButton::clicked, this, &WatchlistScreen::on_remove_stock);
    al->addWidget(remove_btn_);

    lay->addWidget(add_bar_);

    // Table — the main data area
    table_ = new ui::DataTable;
    table_->set_headers({"SYMBOL", "NAME", "PRICE", "CHANGE", "CHG %", "HIGH", "LOW", "VOLUME"});
    table_->set_column_widths({100, 160, 100, 90, 80, 90, 90, 110});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    lay->addWidget(table_, 1);

    return main_panel_;
}

// ── Theme refresh ───────────────────────────────────────────────────────────

void WatchlistScreen::refresh_theme() {
    // Root
    setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));

    // Splitter
    if (splitter_)
        splitter_->setStyleSheet(QString("QSplitter::handle { background:%1; width:1px; }").arg(colors::BORDER_DIM()));

    // Sidebar
    if (sidebar_)
        sidebar_->setStyleSheet(
            QString("background:%1; border-right:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));

    if (sidebar_header_)
        sidebar_header_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    if (sidebar_title_)
        sidebar_title_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.5px; "
                                              "font-family:%3; background:transparent;")
                                          .arg(colors::AMBER())
                                          .arg(fonts::TINY)
                                          .arg(fonts::DATA_FAMILY()));

    if (add_wl_btn_)
        add_wl_btn_->setStyleSheet(accent_btn_style());

    if (wl_list_)
        wl_list_->setStyleSheet(list_style());

    if (wl_count_)
        wl_count_->setStyleSheet(
            QString("background:%1; color:%2; font-size:%3px; border-top:1px solid %4; font-family:%5;")
                .arg(colors::BG_SURFACE())
                .arg(colors::TEXT_TERTIARY())
                .arg(fonts::TINY)
                .arg(colors::BORDER_DIM())
                .arg(fonts::DATA_FAMILY()));

    // Main panel
    if (main_panel_)
        main_panel_->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE()));

    if (top_bar_)
        top_bar_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    if (panel_title_)
        panel_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; background:transparent;")
                .arg(colors::TEXT_PRIMARY())
                .arg(fonts::SMALL)
                .arg(fonts::DATA_FAMILY()));

    if (stock_count_)
        stock_count_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3; background:transparent;")
                                        .arg(colors::TEXT_TERTIARY())
                                        .arg(fonts::TINY)
                                        .arg(fonts::DATA_FAMILY()));

    if (refresh_btn_)
        refresh_btn_->setStyleSheet(std_btn_style());

    if (del_wl_btn_)
        del_wl_btn_->setStyleSheet(danger_btn_style());

    // Add bar
    if (add_bar_)
        add_bar_->setStyleSheet(
            QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));

    if (add_label_)
        add_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:700; letter-spacing:0.5px; "
                                          "font-family:%3; background:transparent;")
                                      .arg(colors::TEXT_SECONDARY())
                                      .arg(fonts::TINY)
                                      .arg(fonts::DATA_FAMILY()));

    if (add_input_)
        add_input_->setStyleSheet(input_style());

    if (add_btn_)
        add_btn_->setStyleSheet(accent_btn_style());

    if (remove_btn_)
        remove_btn_->setStyleSheet(danger_btn_style());
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
        QColor acc(colors::AMBER());
        auto cr = fincept::WatchlistRepository::instance().create("Default", acc.name());
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
            // Green = good, Red = bad
            QString chg_color = q.change_pct >= 0 ? colors::POSITIVE : colors::NEGATIVE;
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
    ScreenStateManager::instance().notify_changed(this);
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

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap WatchlistScreen::save_state() const {
    return {
        {"watchlist_id", current_wl_id_},
    };
}

void WatchlistScreen::restore_state(const QVariantMap& state) {
    const QString wl_id = state.value("watchlist_id").toString();
    if (wl_id.isEmpty())
        return;

    // Find and select the matching watchlist row
    for (int i = 0; i < watchlists_.size(); ++i) {
        if (watchlists_[i].id == wl_id) {
            wl_list_->setCurrentRow(i);
            on_watchlist_selected(i);
            return;
        }
    }
    // Watchlist not found (may have been deleted) — leave default selection
}

} // namespace fincept::screens
