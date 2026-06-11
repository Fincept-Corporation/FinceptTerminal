// PortfolioMonitorScreen.cpp — unified all-accounts positions & holdings.
#include "screens/portfolio_monitor/PortfolioMonitorScreen.h"

#include "storage/repositories/SettingsRepository.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedPortfolioService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

using trading::AggChild;
using trading::AggRow;
using trading::UnifiedPortfolioService;

namespace {

// Columns shared by both trees. Holdings reuses DayPnl for "Today's P&L" and
// shows invested/current in the parent tooltip.
enum Col { ColSymbol = 0, ColProduct, ColQty, ColAvg, ColLtp, ColPnl, ColPnlPct, ColAction, ColCount };

const QString kRupee = QString::fromUtf8("₹");

QString fmt_money(double v, bool signed_pfx = false) {
    const QLocale loc(QLocale::English, QLocale::India); // lakh grouping for ₹
    const QString mag = loc.toString(std::fabs(v), 'f', 2);
    const QString prefix = v < 0 ? QStringLiteral("-") : (signed_pfx ? QStringLiteral("+") : QString());
    return prefix + kRupee + mag;
}

QString fmt_qty(double v) {
    return qFuzzyCompare(v, std::round(v)) ? QString::number(qint64(std::llround(v)))
                                           : QString::number(v, 'f', 2);
}

void set_pnl_cell(QTreeWidgetItem* item, int col, double pnl, const QString& text) {
    item->setText(col, text);
    item->setForeground(col, pnl > 0   ? QColor(fincept::ui::colors::POSITIVE())
                             : pnl < 0 ? QColor(fincept::ui::colors::NEGATIVE())
                                       : QColor("#808080"));
}

QString row_key(const QString& symbol, const QString& exchange) {
    return symbol + QLatin1Char('|') + exchange;
}

} // namespace

PortfolioMonitorScreen::PortfolioMonitorScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("portfolioMonitorScreen");
    // Embedded as a splitter column — must be free to shrink. Children (scroll
    // areas + interactive tree columns) all tolerate narrow widths, so the
    // panel never forces the parent window wider than the screen.
    setMinimumWidth(280);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    layout->addWidget(build_summary_bar());

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    positions_tree_ = make_tree(/*holdings=*/false);
    holdings_tree_ = make_tree(/*holdings=*/true);
    tabs_->addTab(positions_tree_, tr("POSITIONS"));
    tabs_->addTab(holdings_tree_, tr("HOLDINGS"));
    layout->addWidget(tabs_, 1);

    auto& svc = UnifiedPortfolioService::instance();
    connect(&svc, &UnifiedPortfolioService::positions_changed, this, &PortfolioMonitorScreen::rebuild_positions);
    connect(&svc, &UnifiedPortfolioService::holdings_changed, this, &PortfolioMonitorScreen::rebuild_holdings);
    connect(&svc, &UnifiedPortfolioService::position_patched, this, &PortfolioMonitorScreen::patch_position);
    connect(&svc, &UnifiedPortfolioService::holding_patched, this, &PortfolioMonitorScreen::patch_holding);
    connect(&svc, &UnifiedPortfolioService::summary_changed, this, &PortfolioMonitorScreen::update_summary);
    connect(&svc, &UnifiedPortfolioService::accounts_changed, this, &PortfolioMonitorScreen::update_summary);
    connect(&svc, &UnifiedPortfolioService::action_finished, this, &PortfolioMonitorScreen::on_action_finished);
}

void PortfolioMonitorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!activated_) {
        // Apply the persisted mode BEFORE the first activate so the initial
        // model is built from the right engine. setChecked only emits toggled
        // on a state change, so style/route the paper default explicitly.
        const auto saved = SettingsRepository::instance().get(QStringLiteral("portfolio_monitor.mode"),
                                                              QStringLiteral("paper"));
        const bool live = saved.is_ok() && saved.value() == QLatin1String("live");
        if (live)
            mode_btn_->setChecked(true); // fires on_mode_toggled(true)
        else
            on_mode_toggled(false);
    }
    // activate() is idempotent: picks up newly-added accounts and forces a
    // portfolio refresh, so reopening the panel always shows fresh data.
    UnifiedPortfolioService::instance().activate();
    if (!activated_) {
        activated_ = true;
        rebuild_positions();
        rebuild_holdings();
        update_summary();
    }
}

// ── UI construction ──────────────────────────────────────────────────────────

QWidget* PortfolioMonitorScreen::build_summary_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("pmSummaryBar");
    bar->setStyleSheet("#pmSummaryBar{background:#0a0a0a;border:1px solid #1a1a1a;}");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(14);

    // ⧉ detach / ⤓ dock back (feeds pattern). The equity screen owns the
    // float window and the reparenting; this button only signals intent.
    float_btn_ = new QPushButton(QStringLiteral("⧉"));
    float_btn_->setCursor(Qt::PointingHandCursor);
    float_btn_->setFixedSize(24, 24);
    float_btn_->setToolTip(tr("Detach into its own window"));
    float_btn_->setStyleSheet("QPushButton{background:transparent;border:1px solid #2a2a2a;color:#808080;"
                              "font-size:13px;}QPushButton:hover{color:#e5e5e5;border-color:#d97706;}");
    connect(float_btn_, &QPushButton::clicked, this, [this]() {
        if (floating_)
            emit dock_requested();
        else
            emit float_requested();
    });
    h->addWidget(float_btn_);

    // PAPER ⇄ LIVE switch. The service keeps the two models fully separate;
    // this just selects which one is displayed (and which engine actions hit).
    // Default and persistence: settings key portfolio_monitor.mode.
    mode_btn_ = new QPushButton(tr("PAPER"));
    mode_btn_->setCheckable(true); // checked == LIVE
    mode_btn_->setCursor(Qt::PointingHandCursor);
    mode_btn_->setFixedHeight(24);
    connect(mode_btn_, &QPushButton::toggled, this, &PortfolioMonitorScreen::on_mode_toggled);
    h->addWidget(mode_btn_);

    auto add_stat = [&](const QString& title) {
        auto* box = new QVBoxLayout;
        box->setSpacing(1);
        auto* cap = new QLabel(title);
        cap->setStyleSheet("color:#808080;font-size:10px;");
        auto* val = new QLabel(QStringLiteral("—"));
        val->setStyleSheet("color:#e5e5e5;font-size:13px;font-weight:600;");
        box->addWidget(cap);
        box->addWidget(val);
        h->addLayout(box);
        return val;
    };

    sum_positions_pnl_ = add_stat(tr("POSITIONS P&L"));
    sum_day_pnl_ = add_stat(tr("DAY P&L"));
    sum_holdings_pnl_ = add_stat(tr("HOLDINGS P&L"));
    sum_invested_ = add_stat(tr("INVESTED"));
    sum_current_ = add_stat(tr("CURRENT"));
    sum_return_ = add_stat(tr("RETURN %"));
    sum_accounts_ = add_stat(tr("ACCOUNTS"));

    h->addStretch(1);

    toast_ = new QLabel;
    toast_->setStyleSheet("color:#808080;font-size:11px;");
    h->addWidget(toast_);

    auto* refresh = new QPushButton(tr("REFRESH"));
    auto* new_order = new QPushButton(tr("+ NEW ORDER"));
    auto* square_all = new QPushButton(tr("SQUARE OFF ALL"));
    for (auto* b : {refresh, new_order}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton{background:#111;border:1px solid #2a2a2a;color:#d97706;"
                         "padding:5px 12px;font-size:11px;font-weight:600;}"
                         "QPushButton:hover{border-color:#d97706;}");
    }
    square_all->setCursor(Qt::PointingHandCursor);
    square_all->setStyleSheet("QPushButton{background:#1a0a0a;border:1px solid #5c1f1f;color:#ef4444;"
                              "padding:5px 12px;font-size:11px;font-weight:600;}"
                              "QPushButton:hover{border-color:#ef4444;}");
    connect(refresh, &QPushButton::clicked, this, []() { UnifiedPortfolioService::instance().refresh_all(); });
    connect(new_order, &QPushButton::clicked, this, &PortfolioMonitorScreen::on_new_order);
    connect(square_all, &QPushButton::clicked, this, [this]() {
        const auto ret = QMessageBox::question(
            this, tr("Square Off All"),
            tr("Square off ALL intraday positions across EVERY connected account?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret == QMessageBox::Yes)
            UnifiedPortfolioService::instance().square_off_all_positions();
    });
    h->addWidget(refresh);
    h->addWidget(new_order);
    h->addWidget(square_all);

    // The stat row is wider than a narrow splitter column — scroll horizontally
    // instead of forcing a minimum width on the whole panel (which would push
    // the main window past the screen edge).
    auto* scroller = new QScrollArea;
    scroller->setWidget(bar);
    scroller->setWidgetResizable(true);
    scroller->setFrameShape(QFrame::NoFrame);
    scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroller->setFixedHeight(bar->sizeHint().height() + 10); // room for the h-scrollbar
    scroller->setMinimumWidth(0);
    return scroller;
}

void PortfolioMonitorScreen::set_floating(bool floating) {
    floating_ = floating;
    float_btn_->setText(floating ? QStringLiteral("⤓") : QStringLiteral("⧉"));
    float_btn_->setToolTip(floating ? tr("Dock back into the equity tab")
                                    : tr("Detach into its own window"));
}

void PortfolioMonitorScreen::on_mode_toggled(bool live) {
    mode_btn_->setText(live ? tr("LIVE") : tr("PAPER"));
    mode_btn_->setStyleSheet(
        live ? "QPushButton{background:#1a0a0a;border:1px solid #ef4444;color:#ef4444;"
               "padding:4px 14px;font-size:11px;font-weight:700;}"
             : "QPushButton{background:#111;border:1px solid #d97706;color:#d97706;"
               "padding:4px 14px;font-size:11px;font-weight:700;}");
    mode_btn_->setToolTip(live ? tr("Showing REAL broker positions — click for paper")
                               : tr("Showing paper portfolios — click for live"));
    UnifiedPortfolioService::instance().set_mode(live ? UnifiedPortfolioService::Mode::Live
                                                      : UnifiedPortfolioService::Mode::Paper);
    SettingsRepository::instance().set(QStringLiteral("portfolio_monitor.mode"),
                                       live ? QStringLiteral("live") : QStringLiteral("paper"));
    rebuild_positions();
    rebuild_holdings();
    update_summary();
}

QTreeWidget* PortfolioMonitorScreen::make_tree(bool holdings) {
    auto* tree = new QTreeWidget;
    tree->setColumnCount(ColCount);
    tree->setHeaderLabels({tr("Symbol / Account"), holdings ? tr("Today's P&L") : tr("Product"), tr("Qty"),
                           tr("Avg Price"), tr("LTP"), tr("P&L"), tr("P&L %"), tr("Action")});
    tree->setRootIsDecorated(true);
    tree->setAlternatingRowColors(false);
    tree->setUniformRowHeights(true);
    tree->setSelectionMode(QAbstractItemView::NoSelection);
    // Interactive (user-draggable) columns — Stretch mode forbids resizing, so
    // names/numbers truncated with no way to widen them. Sensible defaults +
    // a horizontal scrollbar when the column is narrower than the content.
    tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    tree->header()->setStretchLastSection(false);
    tree->setColumnWidth(ColSymbol, 190);
    tree->setColumnWidth(ColProduct, holdings ? 100 : 90);
    tree->setColumnWidth(ColQty, 64);
    tree->setColumnWidth(ColAvg, 92);
    tree->setColumnWidth(ColLtp, 92);
    tree->setColumnWidth(ColPnl, 96);
    tree->setColumnWidth(ColPnlPct, 70);
    tree->setColumnWidth(ColAction, 76);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tree->setMinimumWidth(0);
    tree->setStyleSheet("QTreeWidget{background:#080808;border:1px solid #1a1a1a;font-size:12px;}"
                        "QTreeWidget::item{height:26px;}");
    return tree;
}

void PortfolioMonitorScreen::fill_parent(QTreeWidgetItem* item, const AggRow& row, bool holdings) {
    item->setText(ColSymbol, row.exchange.isEmpty() ? row.symbol
                                                    : QString("%1  ·  %2").arg(row.symbol, row.exchange));
    if (holdings) {
        set_pnl_cell(item, ColProduct, row.day_pnl, fmt_money(row.day_pnl, true));
        item->setToolTip(ColSymbol, tr("Invested %1   Current %2")
                                        .arg(fmt_money(row.invested), fmt_money(row.current)));
    } else {
        item->setText(ColProduct, QString::number(row.children.size()) + tr(" acct"));
    }
    item->setText(ColQty, fmt_qty(row.total_qty));
    item->setText(ColAvg, row.w_avg_price > 0 ? fmt_money(row.w_avg_price) : QStringLiteral("—"));
    item->setText(ColLtp, row.ltp > 0 ? fmt_money(row.ltp) : QStringLiteral("—"));
    set_pnl_cell(item, ColPnl, row.pnl, fmt_money(row.pnl, true));
    const double base = row.w_avg_price * std::fabs(row.total_qty);
    const double pct = base > 0 ? row.pnl / base * 100.0 : 0.0;
    set_pnl_cell(item, ColPnlPct, row.pnl, QString::number(pct, 'f', 2) + "%");
}

void PortfolioMonitorScreen::rebuild_tree(QTreeWidget* tree, const QVector<AggRow>& rows, bool holdings,
                                          QHash<QString, QTreeWidgetItem*>& index) {
    // Preserve which symbols were expanded across rebuilds.
    QSet<QString> expanded;
    for (auto it = index.constBegin(); it != index.constEnd(); ++it)
        if (it.value()->isExpanded())
            expanded.insert(it.key());

    tree->clear();
    index.clear();

    for (const AggRow& row : rows) {
        auto* parent = new QTreeWidgetItem(tree);
        fill_parent(parent, row, holdings);
        const QString key = row_key(row.symbol, row.exchange);
        index.insert(key, parent);

        // Parent-level EXIT ALL (only meaningful with >1 child; single-child
        // symbols get their action on the child row).
        if (row.children.size() > 1) {
            auto* exit_all = new QPushButton(tr("EXIT ALL"));
            exit_all->setCursor(Qt::PointingHandCursor);
            exit_all->setStyleSheet("QPushButton{background:transparent;border:1px solid #5c1f1f;"
                                    "color:#ef4444;font-size:10px;padding:2px 8px;}"
                                    "QPushButton:hover{background:#1a0a0a;}");
            const QString sym = row.symbol, exch = row.exchange;
            const int n = int(row.children.size());
            connect(exit_all, &QPushButton::clicked, this,
                    [this, sym, exch, holdings, n]() { confirm_exit_symbol(sym, exch, holdings, n); });
            tree->setItemWidget(parent, ColAction, exit_all);
        }

        for (const AggChild& c : row.children) {
            auto* child = new QTreeWidgetItem(parent);
            child->setText(ColSymbol, c.broker_label);
            child->setForeground(ColSymbol, QColor("#9ca3af"));
            if (holdings)
                set_pnl_cell(child, ColProduct, c.day_pnl, fmt_money(c.day_pnl, true));
            else
                child->setText(ColProduct, c.product + (c.side.isEmpty() ? QString() : "·" + c.side));
            child->setText(ColQty, fmt_qty(c.qty));
            child->setText(ColAvg, fmt_money(c.avg_price));
            child->setText(ColLtp, c.ltp > 0 ? fmt_money(c.ltp) : QStringLiteral("—"));
            set_pnl_cell(child, ColPnl, c.pnl, fmt_money(c.pnl, true));
            const double notional = c.avg_price * std::fabs(c.qty);
            set_pnl_cell(child, ColPnlPct, c.pnl,
                         QString::number(notional > 0 ? c.pnl / notional * 100.0 : 0.0, 'f', 2) + "%");

            auto* exit_btn = new QPushButton(tr("EXIT"));
            exit_btn->setCursor(Qt::PointingHandCursor);
            exit_btn->setStyleSheet("QPushButton{background:transparent;border:1px solid #5c1f1f;"
                                    "color:#ef4444;font-size:10px;padding:2px 8px;}"
                                    "QPushButton:hover{background:#1a0a0a;}");
            const QString acct = c.account_id, label = c.broker_label, sym = row.symbol,
                          exch = row.exchange, product = c.product;
            const double qty = c.qty;
            connect(exit_btn, &QPushButton::clicked, this, [this, acct, label, sym, exch, product, qty]() {
                confirm_exit_child(acct, label, sym, exch, product, qty);
            });
            tree->setItemWidget(child, ColAction, exit_btn);
        }

        parent->setExpanded(expanded.contains(key) || rows.size() <= 4);
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void PortfolioMonitorScreen::rebuild_positions() {
    rebuild_tree(positions_tree_, UnifiedPortfolioService::instance().positions(), /*holdings=*/false,
                 pos_index_);
}

void PortfolioMonitorScreen::rebuild_holdings() {
    rebuild_tree(holdings_tree_, UnifiedPortfolioService::instance().holdings(), /*holdings=*/true,
                 hold_index_);
}

// Tick patch: parent sums AND child cells, in place. Children must repaint with
// the parent — patching only the parent leaves child rows at older marks, so the
// parent P&L stops equalling the visible child sum (looks like a calc bug).
// Child items align 1:1 with row.children (both built in rebuild_tree; ticks
// never reorder), so index-addressing is safe. Work stays O(children of one
// symbol) per tick, within the quote hot-path constraint.
namespace {
void patch_row_items(QTreeWidgetItem* parent, const AggRow& row, bool holdings) {
    const int n = std::min(parent->childCount(), int(row.children.size()));
    for (int i = 0; i < n; ++i) {
        QTreeWidgetItem* child = parent->child(i);
        const AggChild& c = row.children[i];
        if (holdings)
            set_pnl_cell(child, ColProduct, c.day_pnl, fmt_money(c.day_pnl, true));
        child->setText(ColLtp, c.ltp > 0 ? fmt_money(c.ltp) : QStringLiteral("—"));
        set_pnl_cell(child, ColPnl, c.pnl, fmt_money(c.pnl, true));
        const double notional = c.avg_price * std::fabs(c.qty);
        set_pnl_cell(child, ColPnlPct, c.pnl,
                     QString::number(notional > 0 ? c.pnl / notional * 100.0 : 0.0, 'f', 2) + "%");
    }
}
} // namespace

void PortfolioMonitorScreen::patch_position(const QString& symbol) {
    for (const AggRow& row : UnifiedPortfolioService::instance().positions()) {
        if (row.symbol != symbol)
            continue;
        if (auto* item = pos_index_.value(row_key(row.symbol, row.exchange))) {
            fill_parent(item, row, /*holdings=*/false);
            patch_row_items(item, row, /*holdings=*/false);
        }
    }
}

void PortfolioMonitorScreen::patch_holding(const QString& symbol) {
    for (const AggRow& row : UnifiedPortfolioService::instance().holdings()) {
        if (row.symbol != symbol)
            continue;
        if (auto* item = hold_index_.value(row_key(row.symbol, row.exchange))) {
            fill_parent(item, row, /*holdings=*/true);
            patch_row_items(item, row, /*holdings=*/true);
        }
    }
}

void PortfolioMonitorScreen::update_summary() {
    const auto s = UnifiedPortfolioService::instance().summary();
    auto set = [](QLabel* l, double v, bool color, bool signed_pfx = true) {
        l->setText(fmt_money(v, signed_pfx));
        if (color)
            l->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;")
                                 .arg(v > 0   ? fincept::ui::colors::POSITIVE()
                                      : v < 0 ? fincept::ui::colors::NEGATIVE()
                                              : QStringLiteral("#e5e5e5")));
    };
    set(sum_positions_pnl_, s.positions_pnl, true);
    set(sum_day_pnl_, s.positions_day_pnl + s.holdings_day_pnl, true);
    set(sum_holdings_pnl_, s.holdings_pnl, true);
    set(sum_invested_, s.invested, false, false);
    set(sum_current_, s.current, false, false);
    sum_return_->setText(QString::number(s.return_pct, 'f', 2) + "%");
    sum_return_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;")
                                   .arg(s.return_pct > 0   ? fincept::ui::colors::POSITIVE()
                                        : s.return_pct < 0 ? fincept::ui::colors::NEGATIVE()
                                                           : QStringLiteral("#e5e5e5")));
    sum_accounts_->setText(QString("%1/%2 LIVE").arg(s.accounts_live).arg(s.accounts_total));
}

void PortfolioMonitorScreen::on_action_finished(bool ok, const QString& message) {
    toast_->setText(message);
    toast_->setStyleSheet(QString("color:%1;font-size:11px;")
                              .arg(ok ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    QTimer::singleShot(8000, toast_, [t = toast_]() { t->clear(); });
}

// ── Actions ──────────────────────────────────────────────────────────────────

void PortfolioMonitorScreen::confirm_exit_child(const QString& account_id, const QString& broker_label,
                                                const QString& symbol, const QString& exchange,
                                                const QString& product, double qty) {
    const auto ret = QMessageBox::question(
        this, tr("Exit Position"),
        tr("Square off %1 × %2 on %3 (%4)?").arg(fmt_qty(qty), symbol, broker_label, product),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        UnifiedPortfolioService::instance().exit_child(account_id, symbol, exchange, product);
}

void PortfolioMonitorScreen::confirm_exit_symbol(const QString& symbol, const QString& exchange,
                                                 bool holdings, int n_accounts) {
    const auto ret = QMessageBox::question(
        this, tr("Exit All"),
        tr("Square off %1 across %2 account(s)?").arg(symbol).arg(n_accounts),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        UnifiedPortfolioService::instance().exit_symbol(symbol, exchange, holdings);
}

void PortfolioMonitorScreen::on_new_order() {
    const auto accounts = UnifiedPortfolioService::instance().accounts();
    if (accounts.isEmpty()) {
        QMessageBox::information(this, tr("New Order"), tr("No active INR broker accounts."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("New Order — Any Account"));
    auto* form = new QFormLayout(&dlg);

    auto* acct_combo = new QComboBox;
    for (const auto& a : accounts)
        acct_combo->addItem(a.label + (a.live ? "" : tr(" (offline)")), a.account_id);
    auto* symbol_edit = new QLineEdit;
    symbol_edit->setPlaceholderText(tr("e.g. RELIANCE"));
    auto* exch_combo = new QComboBox;
    exch_combo->addItems({"NSE", "BSE", "NFO", "BFO", "MCX", "CDS"});
    auto* side_combo = new QComboBox;
    side_combo->addItems({tr("BUY"), tr("SELL")});
    auto* qty_spin = new QSpinBox;
    qty_spin->setRange(1, 1'000'000);
    auto* type_combo = new QComboBox;
    type_combo->addItems({tr("MARKET"), tr("LIMIT")});
    auto* price_spin = new QDoubleSpinBox;
    price_spin->setRange(0.0, 10'000'000.0);
    price_spin->setDecimals(2);
    price_spin->setEnabled(false);
    connect(type_combo, &QComboBox::currentIndexChanged, price_spin,
            [price_spin](int idx) { price_spin->setEnabled(idx == 1); });
    auto* product_combo = new QComboBox;
    product_combo->addItems({tr("INTRADAY"), tr("DELIVERY"), tr("MARGIN")});

    form->addRow(tr("Account"), acct_combo);
    form->addRow(tr("Symbol"), symbol_edit);
    form->addRow(tr("Exchange"), exch_combo);
    form->addRow(tr("Side"), side_combo);
    form->addRow(tr("Quantity"), qty_spin);
    form->addRow(tr("Type"), type_combo);
    form->addRow(tr("Limit Price"), price_spin);
    form->addRow(tr("Product"), product_combo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("PLACE ORDER"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString symbol = symbol_edit->text().trimmed().toUpper();
    if (symbol.isEmpty())
        return;

    trading::UnifiedOrder order;
    order.symbol = symbol;
    order.exchange = exch_combo->currentText();
    order.side = side_combo->currentIndex() == 0 ? trading::OrderSide::Buy : trading::OrderSide::Sell;
    order.order_type = type_combo->currentIndex() == 0 ? trading::OrderType::Market : trading::OrderType::Limit;
    order.quantity = qty_spin->value();
    order.price = type_combo->currentIndex() == 1 ? price_spin->value() : 0.0;
    order.product_type = product_combo->currentIndex() == 0   ? trading::ProductType::Intraday
                         : product_combo->currentIndex() == 1 ? trading::ProductType::Delivery
                                                              : trading::ProductType::Margin;

    UnifiedPortfolioService::instance().place_new_order(acct_combo->currentData().toString(), order);
}

} // namespace fincept::screens
