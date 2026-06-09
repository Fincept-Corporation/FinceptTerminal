// PaperBlotterPanel.cpp — shared paper-trading blotter (positions/orders/trades).
#include "screens/common/PaperBlotterPanel.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/PaperTrading.h"
#include "trading/PaperMarkService.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QTableWidget>
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

QTableWidgetItem* cell(const QString& text, const QColor& fg = QColor()) {
    auto* it = new QTableWidgetItem(text);
    if (fg.isValid())
        it->setForeground(fg);
    return it;
}

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
    // closed market), but only while actually visible.
    auto* poll = new QTimer(this);
    poll->setInterval(2500);
    connect(poll, &QTimer::timeout, this, [this]() {
        if (isVisible())
            refresh();
    });
    poll->start();
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
                                 "QTabBar::tab:selected { color:%5; border-bottom:2px solid %6; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                           colors::TEXT_SECONDARY(), colors::TEXT_PRIMARY(), colors::AMBER()));

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
    bar_lay->addWidget(summary_);
    bar_lay->addStretch(1);
    square_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"), bar);
    square_all_btn_->setCursor(Qt::PointingHandCursor);
    square_all_btn_->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:#fff; border:none; "
                                                  "  padding:5px 14px; font-size:10px; font-weight:700; } "
                                                  "QPushButton:disabled { background:%2; color:%3; }")
                                       .arg(colors::NEGATIVE(), colors::BG_HOVER(), colors::TEXT_DIM()));
    connect(square_all_btn_, &QPushButton::clicked, this, &PaperBlotterPanel::square_off_all);
    bar_lay->addWidget(square_all_btn_);
    root->addWidget(bar);

    tabs_ = new QTabWidget(this);
    positions_ = make_table({tr("Account"), tr("Symbol"), tr("Product"), tr("Side"), tr("Qty"), tr("Avg"),
                             tr("LTP"), tr("P&L"), tr("P&L %"), QString()});
    orders_ = make_table({tr("Account"), tr("Symbol"), tr("Product"), tr("Side"), tr("Type"), tr("Qty"),
                          tr("Price"), tr("Status"), tr("Time")});
    trades_ = make_table({tr("Symbol"), tr("Side"), tr("Qty"), tr("Price"), tr("P&L"), tr("Time")});
    tabs_->addTab(positions_, tr("Positions"));
    tabs_->addTab(orders_, tr("Orders"));
    tabs_->addTab(trades_, tr("Trades"));
    root->addWidget(tabs_, 1);
}

void PaperBlotterPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    refresh();
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
        accts.push_back({a.account_id, a.display_name.isEmpty() ? a.account_id : a.display_name,
                         a.paper_portfolio_id, glyph});
    }

    // ── Positions ───────────────────────────────────────────────────────────
    positions_->setRowCount(0);
    double total_pnl = 0.0;
    int open_count = 0;
    for (const auto& acct : accts) {
        for (const auto& p : pt_get_positions(acct.pid)) {
            if (p.quantity == 0.0)
                continue;
            const int r = positions_->rowCount();
            positions_->insertRow(r);
            const QColor pc = p.unrealized_pnl >= 0 ? pos_color : neg_color;
            positions_->setItem(r, 0, cell(acct.name));
            positions_->setItem(r, 1, cell(p.symbol));
            positions_->setItem(r, 2, cell(p.product.isEmpty() ? QStringLiteral("MIS") : p.product.toUpper()));
            positions_->setItem(r, 3, cell(p.side.toUpper(),
                                           p.side.compare(QLatin1String("long"), Qt::CaseInsensitive) == 0
                                               ? pos_color
                                               : neg_color));
            positions_->setItem(r, 4, cell(QString::number(p.quantity, 'f', 0)));
            positions_->setItem(r, 5, cell(QString::number(p.entry_price, 'f', 2)));
            positions_->setItem(r, 6, cell(QString::number(p.current_price, 'f', 2)));
            positions_->setItem(r, 7, cell(acct.glyph + QString::number(p.unrealized_pnl, 'f', 2), pc));
            const double notional = p.entry_price * p.quantity;
            const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
            positions_->setItem(r, 8, cell(QString::number(pct, 'f', 2) + QLatin1Char('%'), pc));

            auto* exit_btn = new QPushButton(tr("Exit"));
            exit_btn->setCursor(Qt::PointingHandCursor);
            exit_btn->setStyleSheet(QStringLiteral("QPushButton { background:transparent; color:%1; "
                                                   "  border:1px solid %1; padding:2px 10px; font-size:10px; "
                                                   "  font-weight:700; } QPushButton:hover { background:%1; color:#fff; }")
                                        .arg(colors::NEGATIVE()));
            const QString aid = acct.id, sym = p.symbol, prod = p.product;
            connect(exit_btn, &QPushButton::clicked, this,
                    [this, aid, sym, prod]() { square_off_one(aid, sym, prod); });
            positions_->setCellWidget(r, 9, exit_btn);

            total_pnl += p.unrealized_pnl;
            ++open_count;
        }
    }
    positions_->resizeColumnsToContents();
    square_all_btn_->setEnabled(open_count > 0);

    if (open_count == 0) {
        summary_->setText(tr("No open paper positions"));
        summary_->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:600;background:transparent;")
                                    .arg(colors::TEXT_SECONDARY()));
    } else {
        const QString glyph = accts.isEmpty() ? QStringLiteral("$") : accts.first().glyph;
        summary_->setText(tr("%1 open · unrealized %2%3")
                              .arg(open_count)
                              .arg(glyph)
                              .arg(total_pnl, 0, 'f', 2));
        summary_->setStyleSheet(QStringLiteral("color:%1;font-size:11px;font-weight:700;background:transparent;")
                                    .arg(total_pnl >= 0 ? colors::POSITIVE() : colors::NEGATIVE()));
    }

    // ── Working orders (pending only) ─────────────────────────────────────────
    orders_->setRowCount(0);
    for (const auto& acct : accts) {
        for (const auto& o : pt_get_orders(acct.pid, QStringLiteral("pending"))) {
            const int r = orders_->rowCount();
            orders_->insertRow(r);
            orders_->setItem(r, 0, cell(acct.name));
            orders_->setItem(r, 1, cell(o.symbol));
            orders_->setItem(r, 2, cell(o.product.isEmpty() ? QStringLiteral("MIS") : o.product.toUpper()));
            orders_->setItem(r, 3, cell(o.side.toUpper()));
            orders_->setItem(r, 4, cell(o.order_type.toUpper()));
            orders_->setItem(r, 5, cell(QString::number(o.quantity, 'f', 0)));
            orders_->setItem(r, 6, cell(o.price ? QString::number(*o.price, 'f', 2) : tr("MKT")));
            orders_->setItem(r, 7, cell(o.status.toUpper()));
            const QDateTime dt = QDateTime::fromString(o.created_at, Qt::ISODate).toLocalTime();
            orders_->setItem(r, 8, cell(dt.isValid() ? dt.toString("HH:mm:ss") : o.created_at));
        }
    }
    orders_->resizeColumnsToContents();

    // ── Executed trades (recent) ──────────────────────────────────────────────
    trades_->setRowCount(0);
    for (const auto& acct : accts) {
        for (const auto& t : pt_get_trades(acct.pid, 50)) {
            const int r = trades_->rowCount();
            trades_->insertRow(r);
            trades_->setItem(r, 0, cell(t.symbol));
            trades_->setItem(r, 1, cell(t.side.toUpper()));
            trades_->setItem(r, 2, cell(QString::number(t.quantity, 'f', 0)));
            trades_->setItem(r, 3, cell(QString::number(t.price, 'f', 2)));
            const QColor tc = t.pnl >= 0 ? pos_color : neg_color;
            trades_->setItem(r, 4, cell(acct.glyph + QString::number(t.pnl, 'f', 2), tc));
            const QDateTime dt = QDateTime::fromString(t.timestamp, Qt::ISODate).toLocalTime();
            trades_->setItem(r, 5, cell(dt.isValid() ? dt.toString("MM-dd HH:mm") : t.timestamp));
        }
    }
    trades_->resizeColumnsToContents();
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
                              tr("Close all %1 open paper position(s) at market?").arg(targets.size()))
        != QMessageBox::Yes)
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
