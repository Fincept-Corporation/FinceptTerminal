// PaperBlotterPanel.cpp — shared paper-trading blotter (positions/orders/trades).
#include "screens/common/PaperBlotterPanel.h"

#include "core/logging/Logger.h"
#include "trading/AccountManager.h"
#include "trading/PaperMarkService.h"
#include "trading/PaperTrading.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedTrading.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
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
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::common {

using namespace fincept::trading;
namespace colors = fincept::ui::colors;
namespace fmt = fincept::ui::formatting;

namespace {

// Coalesce live mark-to-market refreshes — a burst of ticks should trigger at
// most one table rebuild per this window.
constexpr int kRefreshThrottleMs = 400;
constexpr qint64 kActionErrorShowMs = 8000; // how long a failed square-off stays in the summary

// The paper portfolio's currency code for money cells (fmt::signed_money places
// the symbol and the sign: "-₹1,234.50", not "₹-1234.50").
QString portfolio_ccy(const QString& code) {
    return code.trimmed().isEmpty() ? QStringLiteral("USD") : code.trimmed().toUpper();
}

// Gain / loss / flat colour: a zero P&L (an opening fill, a flat position) is
// neither green nor red.
QColor pnl_color(double v) {
    if (v > 0)
        return QColor(colors::POSITIVE());
    if (v < 0)
        return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

// A stored ISO timestamp (naive = UTC, as the paper engine writes it) in the
// display zone; unparseable text is shown unchanged.
QString paper_time(const QString& iso, bool with_date) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso;
    if (dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC);
    return with_date ? fmt::date_time(dt, fmt::DateStyle::Short, false) : fmt::time(dt, true);
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
// `numeric` right-aligns the cell (quantities, prices, P&L).
void set_cell(QTableWidget* t, int row, int col, const QString& text, const QColor& fg = QColor(),
              bool numeric = false) {
    QTableWidgetItem* it = t->item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        t->setItem(row, col, it);
    }
    if (it->text() != text)
        it->setText(text);
    const Qt::Alignment align = numeric ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter);
    if (Qt::Alignment(it->textAlignment()) != align)
        it->setTextAlignment(align);
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

    // Gain / loss cell colours are item brushes the re-style pass cannot reach;
    // a refresh re-applies them (theme or direction-palette change). Hidden
    // blotters pick them up on their next showEvent() refresh.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) {
                if (isVisible())
                    refresh();
            });
}

void PaperBlotterPanel::build_ui() {
    setObjectName("paperBlotter");
    setStyleSheet(QString("#paperBlotter { background:%1; } "
                                 "QTableWidget { background:%1; color:%2; border:none; "
                                 "  gridline-color:%3; font-size:" + fincept::ui::fonts::px(-3) + "px; } "
                                 "QHeaderView::section { background:%1; color:%4; border:none; "
                                 "  border-bottom:1px solid %3; padding:5px 8px; font-size:" + fincept::ui::fonts::px(-4) + "px; "
                                 "  font-weight:700; } "
                                 "QTabWidget::pane { border:none; } "
                                 "QTabBar::tab { background:transparent; color:%4; padding:6px 14px; "
                                 "  font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; } "
                                 "QTabBar::tab:selected { color:%5; border-bottom:2px solid %6; } "
                                 // Per-row Exit button styling lives HERE, applied once, instead of
                                 // a setStyleSheet() (== full CSS reparse) per button per refresh.
                                 // Exit closes a position whichever its side — a destructive action,
                                 // so the status red, not the price-direction colour.
                                 "QPushButton#blotterExitBtn { background:transparent; color:%7; "
                                 "  border:1px solid %7; padding:2px 10px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; } "
                                 "QPushButton#blotterExitBtn:hover { background:%7; color:%8; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::TEXT_SECONDARY(),
                           colors::TEXT_PRIMARY(), colors::AMBER(), colors::DANGER(), colors::TEXT_ON_ACCENT()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar: summary + square-off-all.
    auto* bar = new QWidget(this);
    auto* bar_lay = new QHBoxLayout(bar);
    bar_lay->setContentsMargins(10, 6, 10, 6);
    summary_ = new QLabel(tr("No open paper positions"), bar);
    summary_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;")
                                .arg(colors::TEXT_SECONDARY()));
    summary_state_ = 0;
    summary_->setAccessibleName(tr("Paper trading summary"));
    bar_lay->addWidget(summary_);
    bar_lay->addStretch(1);
    square_all_btn_ = new QPushButton(tr("SQUARE OFF ALL"), bar);
    square_all_btn_->setProperty("fincept_agent_risk", "order");
    square_all_btn_->setCursor(Qt::PointingHandCursor);
    square_all_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%4; border:none; "
                                                  "  padding:5px 14px; font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:700; } "
                                                  "QPushButton:disabled { background:%2; color:%3; }")
                                       .arg(colors::DANGER(), colors::BG_SURFACE(), colors::TEXT_TERTIARY(),
                                            colors::TEXT_ON_ACCENT()));
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
        QString ccy;
    };
    QVector<Acct> accts;
    for (const auto& a : AccountManager::instance().active_accounts()) {
        if (a.trading_mode != QLatin1String("paper") || a.paper_portfolio_id.isEmpty())
            continue;
        const QString ccy = portfolio_ccy(pt_get_portfolio(a.paper_portfolio_id).currency);
        accts.push_back(
            {a.account_id, a.display_name.isEmpty() ? a.account_id : a.display_name, a.paper_portfolio_id, ccy});
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

                const QColor pc = pnl_color(p.unrealized_pnl);
                set_cell(positions_, r, 0, acct.name);
                set_cell(positions_, r, 1, p.symbol);
                set_cell(positions_, r, 2, p.product.isEmpty() ? QStringLiteral("MIS") : p.product.toUpper());
                set_cell(positions_, r, 3, p.side.toUpper(),
                         p.side.compare(QLatin1String("long"), Qt::CaseInsensitive) == 0 ? pos_color : neg_color);
                set_cell(positions_, r, 4, fmt::quantity(p.quantity), QColor(), true);
                set_cell(positions_, r, 5, fmt::number(p.entry_price, 2), QColor(), true);
                set_cell(positions_, r, 6, fmt::number(p.current_price, 2), QColor(), true);
                set_cell(positions_, r, 7, fmt::signed_money(p.unrealized_pnl, acct.ccy), pc, true);
                const double notional = p.entry_price * p.quantity;
                const double pct = notional != 0.0 ? (p.unrealized_pnl / notional) * 100.0 : 0.0;
                set_cell(positions_, r, 8, fmt::pct(pct, 2), pc, true);

                // Reuse the row's existing Exit button; only its bound
                // account/symbol/product change. The click handler reads them
                // from dynamic properties so the connection is made once.
                auto* exit_btn = qobject_cast<QPushButton*>(positions_->cellWidget(r, 9));
                if (!exit_btn) {
                    exit_btn = new QPushButton(tr("Exit"));
                    exit_btn->setObjectName(QStringLiteral("blotterExitBtn"));
                    exit_btn->setProperty("fincept_agent_risk", "order");
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
        update_summary(r, total_pnl, accts.isEmpty() ? QStringLiteral("USD") : accts.first().ccy);
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
                set_cell(orders_, r, 3, o.side.toUpper(),
                         o.side.compare(QLatin1String("buy"), Qt::CaseInsensitive) == 0 ? pos_color : neg_color);
                set_cell(orders_, r, 4, o.order_type.toUpper());
                set_cell(orders_, r, 5, fmt::quantity(o.quantity), QColor(), true);
                set_cell(orders_, r, 6, o.price ? fmt::number(*o.price, 2) : tr("MKT"), QColor(), true);
                set_cell(orders_, r, 7, o.status.toUpper());
                set_cell(orders_, r, 8, paper_time(o.created_at, false));
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
                set_cell(trades_, r, 1, t.side.toUpper(),
                         t.side.compare(QLatin1String("buy"), Qt::CaseInsensitive) == 0 ? pos_color : neg_color);
                set_cell(trades_, r, 2, fmt::quantity(t.quantity), QColor(), true);
                set_cell(trades_, r, 3, fmt::number(t.price, 2), QColor(), true);
                // An opening fill realises nothing: "—", not a green 0.00.
                set_cell(trades_, r, 4, t.pnl == 0.0 ? fmt::placeholder() : fmt::signed_money(t.pnl, acct.ccy),
                         pnl_color(t.pnl), true);
                set_cell(trades_, r, 5, paper_time(t.timestamp, true));
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

void PaperBlotterPanel::update_summary(int open_count, double total_pnl, const QString& ccy) {
    // A failed square-off is shown here — the row simply staying put said nothing.
    // It stays long enough to read (a few poll cycles), then the next refresh clears it.
    if (!action_error_.isEmpty() && QDateTime::currentMSecsSinceEpoch() > action_error_until_ms_)
        action_error_.clear();

    // Only reparse the label CSS when the semantic state flips — the text can
    // change every tick, the stylesheet almost never does.
    const int want = !action_error_.isEmpty() ? 3 : open_count == 0 ? 0 : (total_pnl >= 0 ? 1 : 2);
    if (want == 3)
        summary_->setText(action_error_);
    else if (open_count == 0)
        summary_->setText(tr("No open paper positions"));
    else
        summary_->setText(tr("%1 open · unrealized %2").arg(open_count).arg(fmt::signed_money(total_pnl, ccy)));

    if (want == summary_state_)
        return;
    summary_state_ = want;
    if (want == 3)
        summary_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;background:transparent;")
                                    .arg(colors::NEGATIVE()));
    else if (want == 0)
        summary_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;background:transparent;")
                                    .arg(colors::TEXT_SECONDARY()));
    else
        summary_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;background:transparent;")
                                    .arg(want == 1 ? colors::POSITIVE() : colors::NEGATIVE()));
}

void PaperBlotterPanel::square_off_one(const QString& account_id, const QString& symbol, const QString& product) {
    // Paper close is a fast local DB op (UnifiedTrading routes paper accounts to
    // the pt_* engine, no broker round-trip). Exchange is left empty so the engine
    // matches by the stored bare symbol.
    auto resp = UnifiedTrading::instance().close_position(account_id, symbol, QString(), product);
    if (!resp.success) {
        LOG_WARN("PaperBlotter", QString("Square-off %1 failed: %2").arg(symbol, resp.error));
        action_error_ = tr("Exit %1 failed: %2")
                            .arg(symbol, resp.error.isEmpty() ? tr("no reason given") : resp.error);
        action_error_until_ms_ = QDateTime::currentMSecsSinceEpoch() + kActionErrorShowMs;
    } else {
        action_error_.clear();
    }
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
    QString first_error;
    for (const auto& t : targets) {
        auto resp = UnifiedTrading::instance().close_position(t.account_id, t.symbol, QString(), t.product);
        if (resp.success)
            ++ok;
        else if (first_error.isEmpty())
            first_error = QStringLiteral("%1: %2").arg(t.symbol, resp.error.isEmpty() ? tr("no reason given") : resp.error);
    }
    LOG_INFO("PaperBlotter", QString("Square-off all: closed %1/%2").arg(ok).arg(targets.size()));
    if (ok < targets.size()) {
        action_error_ = tr("Closed %1 of %2 — %3").arg(ok).arg(targets.size()).arg(first_error);
        action_error_until_ms_ = QDateTime::currentMSecsSinceEpoch() + kActionErrorShowMs;
    } else {
        action_error_.clear();
    }
    refresh();
}

} // namespace fincept::screens::common
