// PaperBlotterPanel.cpp — shared paper-trading blotter (positions/orders/trades).
#include "screens/common/PaperBlotterPanel.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::common {

using namespace fincept::trading;
namespace colors = fincept::ui::colors;

namespace {

// Coalesce live mark-to-market refreshes — a burst of ticks should trigger at
// most one table rebuild per this window.
constexpr int kRefreshThrottleMs = 400;

QString currency_glyph(const QString& code) {
    if (code.compare(QLatin1String("INR"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("₹"); // ₹
    if (code.compare(QLatin1String("USD"), Qt::CaseInsensitive) == 0 || code.isEmpty())
        return QStringLiteral("$");
    return code + QLatin1Char(' ');
}

QTableWidget* make_table(const QStringList& headers) {
    auto* t = new QTableWidget;
    t->setColumnCount(headers.size());
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setAlternatingRowColors(false);
    t->setShowGrid(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return t;
}

// Write one cell, REUSING the existing QTableWidgetItem when the row survived
// the refresh. The old code allocated ~10 fresh QTableWidgetItems per row on
// every tick (2.5 s poll + every mark signal); on a 200-position blotter that
// was 2000 allocations + 2000 destructions per refresh. Reuse also avoids the
// model reset that killed the user's scroll position.
void set_cell(QTableWidget* t, int row, int col, const QString& text, const QColor& fg = QColor()) {
    QTableWidgetItem* it = t->item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        t->setItem(row, col, it);
    }
    if (it->text() != text)
        it->setText(text);
    if (fg.isValid()) {
        if (it->foreground().color() != fg)
            it->setForeground(fg);
    } else if (it->data(Qt::ForegroundRole).isValid()) {
        it->setData(Qt::ForegroundRole, QVariant());
    }
}

// Save/restore the vertical scroll offset around a table rebuild so a live
// blotter doesn't yank itself back to the top on every mark tick.
struct ScrollKeeper {
    QTableWidget* t;
    int v;
    explicit ScrollKeeper(QTableWidget* table) : t(table), v(table->verticalScrollBar()->value()) {
        t->setUpdatesEnabled(false);
    }
    ~ScrollKeeper() {
        t->verticalScrollBar()->setValue(v);
        t->setUpdatesEnabled(true);
    }
};

} // namespace

PaperBlotterPanel::PaperBlotterPanel(QWidget* parent) : QWidget(parent) {
    build_ui();

    // Live updates: the centralized mark service tells us when prices were marked
    // or a fill changed positions/orders. Coalesce bursts into one rebuild.
    connect(&PaperMarkService::instance(), &PaperMarkService::prices_marked, this,
            [this](const QString&) { schedule_refresh(); });
    connect(&PaperMarkService::instance(), &PaperMarkService::portfolio_changed, this,
            [this](const QString&) { schedule_refresh(); });

    // Safety-net poll so the view stays fresh even if no marks arrive (e.g. a
    // closed market). Per CLAUDE.md P3 the timer is NOT started here — it is
    // started in showEvent() and stopped in hideEvent(), so a hidden blotter
    // costs zero DB queries instead of polling pt_get_* every 2.5 s forever.
    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(2500);
    connect(poll_timer_, &QTimer::timeout, this, &PaperBlotterPanel::refresh);
}

void PaperBlotterPanel::build_ui() {
    setObjectName("paperBlotter");
    setStyleSheet(QStringLiteral("#paperBlotter { background:%1; } "
                                 "QTableWidget { background:%1; color:%2; border:none; "
                                 "  gridline-color:%3; font-size:11px; } "
                                 "QHeaderView::section { background:%1; color:%4; border:none; "
                                 "  border-bottom:1px solid %3; padding:5px 8px; font-size:10px; "
                                 "  font-weight:700; } "
                                 "QTabWidget::pane { border:none; } "
                                 "QTabBar::tab { background:transparent; color:%4; padding:6px 14px; "
                                 "  font-size:10px; font-weight:700; } "
                                 "QTabBar::tab:selected { color:%5; border-bottom:2px solid %6; } "
                                 // Per-row Exit button styling lives HERE, applied once, instead of
                                 // a setStyleSheet() (== full CSS reparse) per button per refresh.
                                 "QPushButton#blotterExitBtn { background:transparent; color:%7; "
                                 "  border:1px solid %7; padding:2px 10px; font-size:10px; font-weight:700; } "
                                 "QPushButton#blotterExitBtn:hover { background:%7; color:%5; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(),
                           colors::TEXT_PRIMARY(), colors::AMBER(), colors::NEGATIVE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar: summary + square-off-all.
    auto* bar = new QWidget(this);
    auto* bar_lay = new QHBoxLayout(bar);
    bar_lay->setContentsMargins(10, 6, 10, 6);
    summary_ = new QLabel(tr("No open paper positions"), bar);
    summary_->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                .arg(colors::TEXT_SECONDARY()));
    summary_state_ = 0;
    summary_->setAccessibleName(tr("Paper trading summary"));
    bar_lay->addWidget(summary_);
    bar_lay->addStretch(1);
    square_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"), bar);
    square_all_btn_->setCursor(Qt::PointingHandCursor);
    square_all_btn_->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:#fff; border:none; "
                                                  "  padding:5px 14px; font-size:10px; font-weight:700; } "
                                                  "QPushButton:disabled { background:%2; color:%3; }")
                                       .arg(colors::NEGATIVE(), colors::BG_HOVER(), colors::TEXT_DIM()));
    square_all_btn_->setAccessibleName(tr("Square off all open paper positions"));
    connect(square_all_btn_, &QPushButton::clicked, this, &PaperBlotterPanel::square_off_all);
    bar_lay->addWidget(square_all_btn_);
    root->addWidget(bar);

    tabs_ = new QTabWidget(this);
    positions_ = make_table({tr("Account"), tr("Symbol"), tr("Product"), tr("Side"), tr("Qty"), tr("Avg"), tr("LTP"),
                             tr("P&L"), tr("P&L %"), QString()});
    orders_ = make_table({tr("Account"), tr("Symbol"), tr("Product"), tr("Side"), tr("Type"), tr("Qty"), tr("Price"),
                          tr("Status"), tr("Time")});
    trades_ = make_table({tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("P&L"), tr("Time")});
    positions_->setAccessibleName(tr("Open paper positions"));
    orders_->setAccessibleName(tr("Working paper orders"));
    trades_->setAccessibleName(tr("Executed paper trades"));
    tabs_->addTab(positions_, tr("Positions"));
    tabs_->addTab(orders_, tr("Orders"));
    tabs_->addTab(trades_, tr("Trades"));
    root->addWidget(tabs_, 1);
}

void PaperBlotterPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh();
    if (poll_timer_)
        poll_timer_->start();
}

void PaperBlotterPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (poll_timer_)
        poll_timer_->stop();
    if (refresh_timer_)
        refresh_timer_->stop();
    refresh_armed_ = false;
}

void PaperBlotterPanel::schedule_refresh() {
    if (refresh_armed_ || !isVisible())
        return;
    refresh_armed_ = true;
    if (!refresh_timer_) {
        refresh_timer_ = new QTimer(this);
        refresh_timer_->setSingleShot(true);
        connect(refresh_timer_, &QTimer::timeout, this, [this]() {
            refresh_armed_ = false;
            refresh();
        });
    }
    refresh_timer_->start(kRefreshThrottleMs);
}

void PaperBlotterPanel::refresh() {
    const QColor pos_color(colors::POSITIVE());
    const QColor neg_color(colors::NEGATIVE());

    struct Acct {
        QString id;
        QString name;
        QString pid;
        QString glyph;
    };
    QVector<Acct> accts;
    for (const auto& a : AccountManager::instance().active_accounts()) {
        if (a.trading_mode != QLatin1String("paper") || a.paper_portfolio_id.isEmpty())
            continue;
        const QString glyph = currency_glyph(pt_get_portfolio(a.paper_portfolio_id).currency);
        accts.push_back(
            {a.account_id, a.display_name.isEmpty() ? a.account_id : a.display_name, a.paper_portfolio_id, glyph});
    }

    // ── Positions ───────────────────────────────────────────────────────────
    // Rows are REUSED in place (setRowCount to the new size, then overwrite) so
    // a live blotter no longer reallocates every item + every Exit button on
    // every tick, and the user's scroll offset survives the refresh.
    {
        ScrollKeeper keep(positions_);
        int r = 0;
        double total_pnl = 0.0;
        for (const auto& acct : accts) {
            for (const auto& p : pt_get_positions(acct.pid)) {
                if (p.quantity == 0.0)
                    continue;
                if (positions_->rowCount() <= r)
                    positions_->setRowCount(r + 1);

                const QColor pc = p.unrealized_pnl >= 0 ? pos_color : neg_color;
                set_cell(positions_, r, 0, acct.name);
                set_cell(positions_, r, 1, p.symbol);
                set_cell(positions_, r, 2, p.product.isEmpty() ? QStringLiteral("MIS") : p.product.toUpper());
                set_cell(positions_, r, 3, p.side.toUpper(),
                         p.side.compare(QLatin1String("long"), Qt::CaseInsensitive) == 0 ? pos_color : neg_color);
                set_cell(positions_, r, 4, QString::number(p.quantity, 'f', 0));
                set_cell(positions_, r, 5, QString::number(p.entry_price, 'f', 2));
                set_cell(positions_, r, 6, QString::number(p.current_price, 'f', 2));
                set_cell(positions_, r, 7, acct.glyph + QString::number(p.unrealized_pnl, 'f', 2), pc);
                const double notional = p.entry_price * p.quantity;
                const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
                set_cell(positions_, r, 8, QString::number(pct, 'f', 2) + QLatin1Char('%'), pc);

                // Reuse the row's existing Exit button; only its bound
                // account/symbol/product change. The click handler reads them
                // from dynamic properties so the connection is made once.
                auto* exit_btn = qobject_cast<QPushButton*>(positions_->cellWidget(r, 9));
                if (!exit_btn) {
                    exit_btn = new QPushButton(tr("Exit"));
                    exit_btn->setObjectName(QStringLiteral("blotterExitBtn"));
                    exit_btn->setCursor(Qt::PointingHandCursor);
                    connect(exit_btn, &QPushButton::clicked, this, [this, exit_btn]() {
                        square_off_one(exit_btn->property("fnAccountId").toString(),
                                       exit_btn->property("fnSymbol").toString(),
                                       exit_btn->property("fnProduct").toString());
                    });
                    positions_->setCellWidget(r, 9, exit_btn);
                }
                exit_btn->setProperty("fnAccountId", acct.id);
                exit_btn->setProperty("fnSymbol", p.symbol);
                exit_btn->setProperty("fnProduct", p.product);
                exit_btn->setAccessibleName(tr("Exit %1 position").arg(p.symbol));
                exit_btn->setToolTip(tr("Close %1 at market").arg(p.symbol));

                total_pnl += p.unrealized_pnl;
                ++r;
            }
        }
        if (positions_->rowCount() != r)
            positions_->setRowCount(r); // drops surplus rows (items + widgets)

        // Column auto-sizing measures every cell; only re-run it when the row
        // shape actually changed, not on every price tick.
        if (last_positions_rows_ != r) {
            positions_->resizeColumnsToContents();
            last_positions_rows_ = r;
        }

        square_all_btn_->setEnabled(r > 0);
        update_summary(r, total_pnl, accts.isEmpty() ? QStringLiteral("$") : accts.first().glyph);
    }

    // ── Working orders (pending only) ─────────────────────────────────────────
    {
        ScrollKeeper keep(orders_);
        int r = 0;
        for (const auto& acct : accts) {
            for (const auto& o : pt_get_orders(acct.pid, QStringLiteral("pending"))) {
                if (orders_->rowCount() <= r)
                    orders_->setRowCount(r + 1);
                set_cell(orders_, r, 0, acct.name);
                set_cell(orders_, r, 1, o.symbol);
                set_cell(orders_, r, 2, o.product.isEmpty() ? QStringLiteral("MIS") : o.product.toUpper());
                set_cell(orders_, r, 3, o.side.toUpper());
                set_cell(orders_, r, 4, o.order_type.toUpper());
                set_cell(orders_, r, 5, QString::number(o.quantity, 'f', 0));
                set_cell(orders_, r, 6, o.price ? QString::number(*o.price, 'f', 2) : tr("MKT"));
                set_cell(orders_, r, 7, o.status.toUpper());
                const QDateTime dt = QDateTime::fromString(o.created_at, Qt::ISODate).toLocalTime();
                set_cell(orders_, r, 8, dt.isValid() ? dt.toString("HH:mm:ss") : o.created_at);
                ++r;
            }
        }
        if (orders_->rowCount() != r)
            orders_->setRowCount(r);
        if (last_orders_rows_ != r) {
            orders_->resizeColumnsToContents();
            last_orders_rows_ = r;
        }
    }

    // ── Executed trades (recent) ──────────────────────────────────────────────
    {
        ScrollKeeper keep(trades_);
        int r = 0;
        for (const auto& acct : accts) {
            for (const auto& t : pt_get_trades(acct.pid, 50)) {
                if (trades_->rowCount() <= r)
                    trades_->setRowCount(r + 1);
                set_cell(trades_, r, 0, t.symbol);
                set_cell(trades_, r, 1, t.side.toUpper());
                set_cell(trades_, r, 2, QString::number(t.quantity, 'f', 0));
                set_cell(trades_, r, 3, QString::number(t.price, 'f', 2));
                set_cell(trades_, r, 4, acct.glyph + QString::number(t.pnl, 'f', 2),
                         t.pnl >= 0 ? pos_color : neg_color);
                const QDateTime dt = QDateTime::fromString(t.timestamp, Qt::ISODate).toLocalTime();
                set_cell(trades_, r, 5, dt.isValid() ? dt.toString("MM-dd HH:mm") : t.timestamp);
                ++r;
            }
        }
        if (trades_->rowCount() != r)
            trades_->setRowCount(r);
        if (last_trades_rows_ != r) {
            trades_->resizeColumnsToContents();
            last_trades_rows_ = r;
        }
    }
}

void PaperBlotterPanel::update_summary(int open_count, double total_pnl, const QString& glyph) {
    // Only reparse the label CSS when the semantic state flips — the text can
    // change every tick, the stylesheet almost never does.
    const int want = open_count == 0 ? 0 : (total_pnl >= 0 ? 1 : 2);
    if (open_count == 0)
        summary_->setText(tr("No open paper positions"));
    else
        summary_->setText(tr("%1 open · unrealized %2%3").arg(open_count).arg(glyph).arg(total_pnl, 0, 'f', 2));

    if (want == summary_state_)
        return;
    summary_state_ = want;
    if (want == 0)
        summary_->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                    .arg(colors::TEXT_SECONDARY()));
    else
        summary_->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                    .arg(want == 1 ? colors::POSITIVE() : colors::NEGATIVE()));
}

void PaperBlotterPanel::square_off_one(const QString& account_id, const QString& symbol, const QString& product) {
    // Paper close is a fast local DB op (UnifiedTrading routes paper accounts to
    // the pt_* engine, no broker round-trip). Exchange is left empty so the engine
    // matches by the stored bare symbol.
    auto resp = UnifiedTrading::instance().close_position(account_id, symbol, QString(), product);
    if (!resp.success)
        LOG_WARN("PaperBlotter", QString("Square-off %1 failed: %2").arg(symbol, resp.error));
    refresh();
}

void PaperBlotterPanel::square_off_all() {
    // Collect every open paper position currently shown, then close each.
    struct Target {
        QString account_id;
        QString symbol;
        QString product;
    };
    QVector<Target> targets;
    for (const auto& a : AccountManager::instance().active_accounts()) {
        if (a.trading_mode != QLatin1String("paper") || a.paper_portfolio_id.isEmpty())
            continue;
        for (const auto& p : pt_get_positions(a.paper_portfolio_id))
            if (p.quantity != 0.0)
                targets.push_back({a.account_id, p.symbol, p.product});
    }
    if (targets.isEmpty())
        return;

    if (QMessageBox::question(this, tr("Square off all"),
                              tr("Close all %1 open paper position(s) at market?").arg(targets.size())) !=
        QMessageBox::Yes)
        return;

    int ok = 0;
    for (const auto& t : targets) {
        auto resp = UnifiedTrading::instance().close_position(t.account_id, t.symbol, QString(), t.product);
        if (resp.success)
            ++ok;
    }
    LOG_INFO("PaperBlotter", QString("Square-off all: closed %1/%2").arg(ok).arg(targets.size()));
    refresh();
}

} // namespace fincept::screens::common
