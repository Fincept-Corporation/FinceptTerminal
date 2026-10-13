// src/screens/equity_research/EquityPeersTab.cpp
#include "screens/equity_research/EquityPeersTab.h"

#include "screens/equity_research/EquityResearchFormat.h"
#include "screens/equity_research/EquityResearchLinks.h"
#include "services/equity/EquityResearchService.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace peers_tab_detail {

constexpr int kSymbolRole = Qt::UserRole + 1;

/// "—" when the source sent nothing (yfinance reports a missing ratio as 0).
QString peers_fmt(double v, int dec, const QString& suffix = {}) {
    return v != 0.0 && std::isfinite(v) ? QString::number(v, 'f', dec) + suffix : equity_fmt::dash();
}

} // namespace peers_tab_detail

using namespace peers_tab_detail;

EquityPeersTab::EquityPeersTab(QWidget* parent) : QWidget(parent) {
    build_ui();
    auto& svc = services::equity::EquityResearchService::instance();
    connect(&svc, &services::equity::EquityResearchService::peers_loaded, this, &EquityPeersTab::on_peers_loaded);
    // Dismiss the "LOADING PEERS…" overlay on failure — it's hidden only on success.
    connect(&svc, &services::equity::EquityResearchService::error_occurred, this,
            [this](const QString& ctx, const QString&) {
                if (ctx != "Peers")
                    return;
                if (loading_overlay_)
                    loading_overlay_->hide_loading();
                status_label_->setText(tr("Could not load peer data — press LOAD to try again."));
                status_label_->show();
            });
}

void EquityPeersTab::set_symbol(const QString& symbol) {
    if (symbol == current_symbol_)
        return;
    current_symbol_ = symbol;
    // The table still holds the previous symbol's peers; clear it so a failed load leaves
    // an empty grid + the error line instead of another company's comparison.
    peer_table_->setRowCount(0);
    peers_loaded_ = false;
    cached_peers_.clear();
    // A peer list restored from the saved session wins once over the curated defaults.
    peers_edit_->setText(pending_peers_.isEmpty() ? default_peers(symbol).join(", ") : pending_peers_);
    pending_peers_.clear();
    loading_overlay_->show_loading(tr("LOADING PEERS…"));
    on_load_clicked();
}

void EquityPeersTab::build_ui() {
    setObjectName(QStringLiteral("equityPeersTab"));
    // One stylesheet for the tab; children are styled by objectName.
    setStyleSheet(
        QString("#equityPeersTab { background:%1; }"
                "QFrame#peersCtrl { background:%2; border:1px solid %3; }"
                "QLabel { background:transparent; border:0; }"
                "QLabel#peersCaption { color:%4; font-size:10px; font-weight:700; letter-spacing:1px; }"
                "QLineEdit#peersEdit { background:%5; color:%6; border:1px solid %3; padding:4px 10px; font-size:11px; }"
                "QLineEdit#peersEdit:focus { border-color:%7; }"
                "QPushButton#peersLoad { background:%7; color:%1; border:0; padding:5px 18px; font-size:10px;"
                " font-weight:700; }"
                "QPushButton#peersLoad:hover { background:%8; }"
                "QLabel#peersStatus { color:%4; font-size:11px; padding:4px; }"
                "QLabel#peersLegend, QLabel#peersUnits { color:%9; font-size:10px; }"
                "QLabel#peersDotGood { color:%10; font-size:10px; } QLabel#peersDotBad { color:%11; font-size:10px; }"
                "QLabel#peersDotNeutral { color:%12; font-size:10px; } QLabel#peersDotInfo { color:%13; font-size:10px; }"
                "QTableWidget { background:%2; alternate-background-color:%1; gridline-color:%3; color:%6; border:0;"
                " font-size:11px; }"
                "QHeaderView::section { background:%5; color:%4; font-size:9px; font-weight:700; padding:5px 4px;"
                " border:0; border-bottom:1px solid %3; letter-spacing:1px; }"
                "QTableWidget::item { padding:2px 6px; }")
            .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(), ui::colors::AMBER_DIM(),
                 ui::colors::TEXT_TERTIARY())
            .arg(ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::WARNING(), ui::colors::CYAN()));

    loading_overlay_ = new ui::LoadingOverlay(this);
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(10);

    // ── Controls row ──────────────────────────────────────────────────────────
    auto* ctrl = new QFrame(this);
    ctrl->setObjectName(QStringLiteral("peersCtrl"));
    auto* hl = new QHBoxLayout(ctrl);
    hl->setContentsMargins(12, 8, 12, 8);
    hl->setSpacing(10);

    peers_caption_ = new QLabel(tr("PEERS (comma-separated):"), ctrl);
    peers_caption_->setObjectName(QStringLiteral("peersCaption"));
    hl->addWidget(peers_caption_);

    peers_edit_ = new QLineEdit(ctrl);
    peers_edit_->setObjectName(QStringLiteral("peersEdit"));
    peers_edit_->setPlaceholderText(tr("MSFT, GOOGL, META — then Enter"));
    connect(peers_edit_, &QLineEdit::returnPressed, this, &EquityPeersTab::on_load_clicked);
    hl->addWidget(peers_edit_, 1);

    load_btn_ = new QPushButton(tr("LOAD"), ctrl);
    load_btn_->setObjectName(QStringLiteral("peersLoad"));
    load_btn_->setCursor(Qt::PointingHandCursor);
    connect(load_btn_, &QPushButton::clicked, this, &EquityPeersTab::on_load_clicked);
    hl->addWidget(load_btn_);
    vl->addWidget(ctrl);

    // ── Status ────────────────────────────────────────────────────────────────
    status_label_ = new QLabel(tr("Loading peer data…"), this);
    status_label_->setObjectName(QStringLiteral("peersStatus"));
    status_label_->hide();
    vl->addWidget(status_label_);

    // ── Table ─────────────────────────────────────────────────────────────────
    peer_table_ = new QTableWidget(this);
    peer_table_->setAlternatingRowColors(true);
    peer_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peer_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    peer_table_->verticalHeader()->setDefaultSectionSize(26);
    peer_table_->verticalHeader()->hide();
    peer_table_->horizontalHeader()->setStretchLastSection(false);
    peer_table_->setSortingEnabled(true);
    peer_table_->setContextMenuPolicy(Qt::CustomContextMenu);
    peer_table_->setToolTip(tr("Double-click a row to research that company · right-click for more"));
    connect(peer_table_, &QWidget::customContextMenuRequested, this, &EquityPeersTab::show_row_menu);
    connect(peer_table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const QString sym = symbol_at_row(row);
        if (!sym.isEmpty() && sym != current_symbol_)
            emit research_requested(sym, false);
    });
    vl->addWidget(peer_table_, 1);

    // ── Legend + units ────────────────────────────────────────────────────────
    auto* legend = new QWidget(this);
    auto* leg_hl = new QHBoxLayout(legend);
    leg_hl->setContentsMargins(0, 4, 0, 0);
    leg_hl->setSpacing(16);
    auto make_leg = [&](const char* dot_name, const QString& text) {
        auto* dot = new QLabel(QStringLiteral("●"), legend);
        dot->setObjectName(QLatin1String(dot_name));
        auto* t = new QLabel(text, legend);
        t->setObjectName(QStringLiteral("peersLegend"));
        leg_hl->addWidget(dot);
        leg_hl->addWidget(t);
        legend_text_lbls_.append(t);
    };
    make_leg("peersDotGood", tr("Positive / Good"));
    make_leg("peersDotBad", tr("Negative / High Risk"));
    make_leg("peersDotNeutral", tr("Neutral"));
    make_leg("peersDotInfo", tr("Symbol / Info"));
    leg_hl->addStretch();
    vl->addWidget(legend);

    units_lbl_ = new QLabel(this);
    units_lbl_->setObjectName(QStringLiteral("peersUnits"));
    units_lbl_->setWordWrap(true);
    vl->addWidget(units_lbl_);
    retranslateUi();
}

void EquityPeersTab::on_load_clicked() {
    if (current_symbol_.isEmpty())
        return;
    QStringList peers;
    for (const auto& p : peers_edit_->text().split(','))
        if (!p.trimmed().isEmpty())
            peers.append(p.trimmed().toUpper());
    peers.removeDuplicates();
    peers.removeAll(current_symbol_.toUpper());
    if (peers.isEmpty()) {
        loading_overlay_->hide_loading();
        status_label_->setText(tr("Enter at least one peer ticker, then press LOAD."));
        status_label_->show();
        return;
    }
    requested_symbols_ = QStringList{current_symbol_} + peers;
    status_label_->setText(tr("Loading peer data…"));
    status_label_->show();
    services::equity::EquityResearchService::instance().fetch_peers(current_symbol_, peers);
}

void EquityPeersTab::on_peers_loaded(QVector<services::equity::PeerData> peers) {
    // The signal has no symbol/request id: a slow response for the previous symbol (or an MCP
    // peers call) must not overwrite the table. Every returned row must belong to our request.
    for (const auto& p : peers) {
        if (!requested_symbols_.contains(p.symbol, Qt::CaseInsensitive))
            return;
    }
    loading_overlay_->hide_loading();
    cached_peers_ = peers;
    peers_loaded_ = true;
    populate_table(peers);
    if (peers.isEmpty()) {
        status_label_->setText(tr("No data came back for %1 — check the tickers (yfinance form, e.g. TCS.NS).")
                                   .arg(requested_symbols_.join(QStringLiteral(", "))));
        status_label_->show();
    } else {
        QStringList missing;
        for (const auto& s : requested_symbols_) {
            bool found = false;
            for (const auto& p : peers)
                found = found || p.symbol.compare(s, Qt::CaseInsensitive) == 0;
            if (!found)
                missing << s;
        }
        status_label_->setText(missing.isEmpty() ? QString() : tr("No data for: %1").arg(missing.join(QStringLiteral(", "))));
        status_label_->setVisible(!missing.isEmpty());
    }
}

void EquityPeersTab::populate_table(const QVector<services::equity::PeerData>& peers) {
    const QStringList headers = {tr("SYMBOL"),    tr("MKT CAP"), tr("PRICE"),   tr("P/E"),     tr("FWD P/E"),
                                 tr("P/B"),       tr("P/S"),     tr("PEG"),     tr("ROE"),     tr("ROA"),
                                 tr("GROSS MGN"), tr("NET MGN"), tr("OP MGN"),  tr("D/E"),     tr("DIV YIELD"),
                                 tr("BETA")};

    peer_table_->setSortingEnabled(false);
    peer_table_->setColumnCount(static_cast<int>(headers.size()));
    peer_table_->setRowCount(static_cast<int>(peers.size()));
    peer_table_->setHorizontalHeaderLabels(headers);

    const QColor kGray(ui::colors::TEXT_TERTIARY());
    // Lower is better: green ≤ good, amber ≤ warn, red above; missing (≤ 0) is grey.
    auto color_ratio = [&kGray](double v, double good, double warn) -> QColor {
        if (v <= 0.0)
            return kGray;
        if (v <= good)
            return QColor(ui::colors::POSITIVE());
        if (v <= warn)
            return QColor(ui::colors::WARNING());
        return QColor(ui::colors::NEGATIVE());
    };
    auto color_sign = [&kGray](double v) -> QColor {
        if (v == 0.0)
            return kGray;
        return QColor(v > 0.0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
    };
    // Sort by the raw figure, never by the formatted text ("3.95T" vs "890.20B").
    auto set_cell = [&](int row, int col, const QString& text, double sort_value, const QColor& fg) {
        auto* item = new ui::NumericTableWidgetItem(text, sort_value);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (fg.isValid())
            item->setForeground(fg);
        peer_table_->setItem(row, col, item);
    };
    auto pct = [](double fraction) {
        return fraction != 0.0 ? equity_fmt::pct(fraction, 1) : equity_fmt::dash();
    };

    QFont bold;
    bold.setBold(true);
    for (int r = 0; r < peers.size(); ++r) {
        const auto& p = peers[r];
        auto* sym_item = new QTableWidgetItem(p.symbol);
        sym_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        sym_item->setForeground(p.symbol == current_symbol_ ? QColor(ui::colors::AMBER()) : QColor(ui::colors::CYAN()));
        sym_item->setFont(bold);
        sym_item->setData(kSymbolRole, p.symbol);
        if (!p.name.isEmpty())
            sym_item->setToolTip(p.sector.isEmpty() ? p.name : QStringLiteral("%1 · %2").arg(p.name, p.sector));
        peer_table_->setItem(r, 0, sym_item);

        set_cell(r, 1, p.market_cap > 0.0 ? equity_fmt::large(p.market_cap) : equity_fmt::dash(), p.market_cap,
                 QColor(ui::colors::CYAN()));
        set_cell(r, 2, peers_fmt(p.price, 2), p.price, QColor(ui::colors::CYAN()));
        set_cell(r, 3, peers_fmt(p.pe_ratio, 1, QStringLiteral("x")), p.pe_ratio, color_ratio(p.pe_ratio, 15, 30));
        set_cell(r, 4, peers_fmt(p.forward_pe, 1, QStringLiteral("x")), p.forward_pe, color_ratio(p.forward_pe, 15, 30));
        set_cell(r, 5, peers_fmt(p.price_to_book, 2, QStringLiteral("x")), p.price_to_book,
                 color_ratio(p.price_to_book, 2, 5));
        set_cell(r, 6, peers_fmt(p.price_to_sales, 2, QStringLiteral("x")), p.price_to_sales,
                 color_ratio(p.price_to_sales, 3, 8));
        set_cell(r, 7, peers_fmt(p.peg_ratio, 2), p.peg_ratio, color_ratio(p.peg_ratio, 1, 2));
        set_cell(r, 8, pct(p.roe), p.roe, color_sign(p.roe));
        set_cell(r, 9, pct(p.roa), p.roa, color_sign(p.roa));
        set_cell(r, 10, pct(p.gross_margin), p.gross_margin, color_sign(p.gross_margin));
        set_cell(r, 11, pct(p.profit_margin), p.profit_margin, color_sign(p.profit_margin));
        set_cell(r, 12, pct(p.operating_margin), p.operating_margin, color_sign(p.operating_margin));
        // yfinance reports debtToEquity and dividendYield in PERCENT (D/E 152 = 1.52x, yield
        // 0.33 = 0.33%) — normalise to the ratio / fraction the thresholds and pct() expect.
        const double de = p.debt_to_equity / 100.0;
        set_cell(r, 13, peers_fmt(de, 2, QStringLiteral("x")), de, color_ratio(de, 0.5, 2.0));
        const double dy = p.dividend_yield / 100.0;
        set_cell(r, 14, pct(dy), dy, dy > 0.0 ? QColor(ui::colors::POSITIVE()) : kGray);
        set_cell(r, 15, peers_fmt(p.beta, 2), p.beta,
                 p.beta == 0.0 ? kGray
                               : (p.beta > 0.0 && p.beta <= 1.5 ? QColor(ui::colors::POSITIVE())
                                                                : QColor(ui::colors::NEGATIVE())));
    }

    peer_table_->resizeColumnsToContents();
    peer_table_->setSortingEnabled(true);
}

QString EquityPeersTab::symbol_at_row(int row) const {
    auto* it = peer_table_->item(row, 0);
    return it ? it->data(kSymbolRole).toString() : QString();
}

void EquityPeersTab::show_row_menu(const QPoint& pos) {
    auto* item = peer_table_->itemAt(pos);
    if (!item)
        return;
    const QString sym = symbol_at_row(item->row());
    if (sym.isEmpty())
        return;
    QMenu menu(this);
    if (sym != current_symbol_) {
        connect(menu.addAction(tr("Research %1 here").arg(sym)), &QAction::triggered, this,
                [this, sym]() { emit research_requested(sym, false); });
        connect(menu.addAction(tr("Price chart for %1").arg(sym)), &QAction::triggered, this,
                [this, sym]() { emit research_requested(sym, true); });
        menu.addSeparator();
    }
    equity_links::add_open_in_actions(&menu, this, sym);
    menu.addSeparator();
    connect(menu.addAction(tr("Copy symbol")), &QAction::triggered, this,
            [sym]() { QApplication::clipboard()->setText(sym); });
    menu.exec(peer_table_->viewport()->mapToGlobal(pos));
}

QStringList EquityPeersTab::default_peers(const QString& symbol) const {
    static const QHash<QString, QStringList> kPeers = {
        {"AAPL", {"MSFT", "GOOGL", "META", "AMZN", "NVDA"}},
        {"MSFT", {"AAPL", "GOOGL", "AMZN", "ORCL", "CRM"}},
        {"GOOGL", {"MSFT", "AAPL", "META", "AMZN", "NFLX"}},
        {"AMZN", {"MSFT", "GOOGL", "AAPL", "WMT", "TGT"}},
        {"NVDA", {"AMD", "INTC", "QCOM", "TSM", "AVGO"}},
        {"META", {"GOOGL", "SNAP", "PINS", "MSFT", "AMZN"}},
        {"TSLA", {"F", "GM", "NIO", "RIVN", "LCID"}},
        {"JPM", {"BAC", "WFC", "GS", "MS", "C"}},
        {"JNJ", {"PFE", "ABT", "MRK", "BMY", "ABBV"}},
        {"XOM", {"CVX", "BP", "SHEL", "COP", "EOG"}},
        {"RELIANCE.NS", {"TCS.NS", "INFY.NS", "HDFCBANK.NS", "ITC.NS", "HINDUNILVR.NS"}},
        {"TCS.NS", {"INFY.NS", "WIPRO.NS", "HCLTECH.NS", "TECHM.NS", "RELIANCE.NS"}},
    };
    // No curated group: compare against the broad market ETFs until the user types peers.
    return kPeers.value(symbol.toUpper(), {"SPY", "QQQ", "DIA"});
}

// ── Re-translation ───────────────────────────────────────────────────────────

void EquityPeersTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EquityPeersTab::retranslateUi() {
    if (peers_caption_)
        peers_caption_->setText(tr("PEERS (comma-separated):"));
    if (load_btn_)
        load_btn_->setText(tr("LOAD"));
    if (peers_edit_)
        peers_edit_->setPlaceholderText(tr("MSFT, GOOGL, META — then Enter"));
    const QStringList legend = {tr("Positive / Good"), tr("Negative / High Risk"), tr("Neutral"), tr("Symbol / Info")};
    for (int i = 0; i < legend_text_lbls_.size() && i < legend.size(); ++i)
        legend_text_lbls_[i]->setText(legend[i]);
    if (units_lbl_)
        units_lbl_->setText(tr("Trailing ratios from Yahoo Finance. Multiples in x; margins, returns and dividend "
                               "yield in %; D/E = total debt ÷ equity (x). Market cap and price are in each "
                               "listing's own currency."));
    // Re-render from cached data so headers and dashes pick up the new locale.
    if (peers_loaded_)
        populate_table(cached_peers_);
}

QString EquityPeersTab::peers_text() const {
    return peers_edit_ ? peers_edit_->text() : QString();
}

void EquityPeersTab::set_peers_text(const QString& text) {
    // Tabs load lazily: keep the restored list until set_symbol() runs for the first time.
    pending_peers_ = text.trimmed();
    if (peers_edit_)
        peers_edit_->setText(text);
}

} // namespace fincept::screens
