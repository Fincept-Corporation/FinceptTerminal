// PortfolioMonitorScreen.cpp — unified all-accounts positions & holdings.
#include "screens/portfolio_monitor/PortfolioMonitorScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"
#include "trading/TradingTypes.h"
#include "trading/UnifiedPortfolioService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"

#include <QClipboard>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QHideEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTextStream>
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

/// Dynamic-property guard set around the showEvent() fallback that routes the
/// screen to PAPER after a *failed* read of portfolio_monitor.mode. While it is
/// set, on_mode_toggled() applies the mode but does not persist it, so a
/// transient DB error cannot demote a LIVE user to paper on disk.
constexpr const char* kModePersistSuppressedProp = "fincept_mode_persist_suppressed";

/// Currency of every amount and price on this screen: UnifiedPortfolioService merges
/// only the broker accounts of one currency (no FX), and names it. Formatting follows
/// the user's settings (FormatSettings) — it used to force Indian lakh grouping on
/// everyone regardless of them.
QString monitor_ccy() {
    return UnifiedPortfolioService::aggregate_currency();
}

QString fmt_money(double v, bool signed_pfx = false) {
    return signed_pfx ? ui::formatting::signed_money(v, monitor_ccy()) : ui::formatting::money(v, monitor_ccy());
}

/// A price (average cost, LTP, limit) in the instrument's own currency, at price precision.
QString fmt_price(double v, const QString& symbol) {
    return ui::formatting::price_with_symbol(v, ui::formatting::instrument_for_symbol(symbol, monitor_ccy()));
}

/// P&L as a percentage of the cost, signed ("+1.23%"): the sign, not only the colour.
QString fmt_pct(double pct) {
    return ui::formatting::pct(pct, 2);
}

QString fmt_qty(double v) {
    return ui::formatting::quantity(v, 4);
}

void set_pnl_cell(QTreeWidgetItem* item, int col, double pnl, const QString& text) {
    item->setText(col, text);
    item->setForeground(col, pnl > 0   ? QColor(fincept::ui::colors::POSITIVE())
                             : pnl < 0 ? QColor(fincept::ui::colors::NEGATIVE())
                                       : QColor(fincept::ui::colors::TEXT_SECONDARY()));
}

QString row_key(const QString& symbol, const QString& exchange) {
    return symbol + QLatin1Char('|') + exchange;
}

/// Side of the square float / dock button: 24px at the default font size, growing with it.
int float_button_side() {
    return std::max(24, fincept::ui::fonts::font_px(-1) + 11);
}

/// LIVE: how long an empty tree waits for the first broker refresh after activate()
/// before it says why it is empty instead of showing the skeleton.
constexpr int kAwaitRefreshMs = 20 * 1000;

/// The page stack inside `tabs` (QTabWidget's own child), so a tab page built there is
/// not moved — and re-styled as a whole — by addTab().
QWidget* pm_page_parent(QTabWidget* tabs) {
    QWidget* stack = tabs->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                     Qt::FindDirectChildrenOnly);
    return stack ? stack : static_cast<QWidget*>(tabs);
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

    // Under its final parent from the start, pages built in the tab widget's own
    // page stack: nothing is re-parented (and re-styled as a whole) by addTab().
    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    build_tree_page(pos_page_, /*holdings=*/false, pm_page_parent(tabs_));
    build_tree_page(hold_page_, /*holdings=*/true, pm_page_parent(tabs_));
    positions_tree_ = pos_page_.tree;
    holdings_tree_ = hold_page_.tree;
    tabs_->addTab(pos_page_.stack, tr("POSITIONS"));
    tabs_->addTab(hold_page_.stack, tr("HOLDINGS"));
    layout->addWidget(tabs_, 1);

    await_timer_ = new QTimer(this);
    await_timer_->setSingleShot(true);
    connect(await_timer_, &QTimer::timeout, this, [this]() {
        // The broker did not answer in time: say why the tree is empty instead of loading forever.
        pos_page_.awaiting = false;
        hold_page_.awaiting = false;
        update_tree_state(false);
        update_tree_state(true);
    });

    auto& svc = UnifiedPortfolioService::instance();
    // A structural change is also "the refresh arrived" for the page awaiting it.
    connect(&svc, &UnifiedPortfolioService::positions_changed, this, [this]() {
        pos_page_.awaiting = false;
        rebuild_positions();
    });
    connect(&svc, &UnifiedPortfolioService::holdings_changed, this, [this]() {
        hold_page_.awaiting = false;
        rebuild_holdings();
    });
    connect(&svc, &UnifiedPortfolioService::position_patched, this, &PortfolioMonitorScreen::patch_position);
    connect(&svc, &UnifiedPortfolioService::holding_patched, this, &PortfolioMonitorScreen::patch_holding);
    connect(&svc, &UnifiedPortfolioService::summary_changed, this, &PortfolioMonitorScreen::update_summary);
    connect(&svc, &UnifiedPortfolioService::accounts_changed, this, &PortfolioMonitorScreen::update_summary);
    connect(&svc, &UnifiedPortfolioService::action_finished, this, &PortfolioMonitorScreen::on_action_finished);

    // Theme switch: only the data-driven colours (tree item brushes) are re-applied
    // here; font-derived sizes are re-fitted once the new appearance has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &PortfolioMonitorScreen::fit_to_font);
    // Currency / number format changed: re-format from the service's last aggregated
    // rows (in memory; nothing is fetched) and the summary bar.
    ui::formatting::on_format_changed(this, [this]() {
        if (activated_) {
            rebuild_positions();
            rebuild_holdings();
        }
        update_summary();
    });
}

void PortfolioMonitorScreen::refresh_theme() {
    // Tree cells carry their text colours as item brushes.
    if (activated_) {
        rebuild_positions();
        rebuild_holdings();
    }
}

void PortfolioMonitorScreen::fit_to_font() {
    // Sizes derived from the font, fixed at build time.
    const int side = float_button_side();
    float_btn_->setFixedSize(side, side);
    if (summary_scroll_ && summary_scroll_->widget())
        summary_scroll_->setFixedHeight(summary_scroll_->widget()->sizeHint().height() + 10);
}

void PortfolioMonitorScreen::build_tree_page(TreePage& page, bool holdings, QWidget* parent) {
    page.stack = new QStackedWidget(parent);
    page.tree = make_tree(holdings, page.stack);
    page.stack->addWidget(page.tree);
    page.skeleton = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, page.stack);
    page.skeleton->set_row_count(6);
    page.stack->addWidget(page.skeleton);
    page.empty = new ui::EmptyState(page.stack);
    page.stack->addWidget(page.empty);
    page.stack->setCurrentWidget(page.tree);
    connect(page.empty, &ui::EmptyState::primary_clicked, this,
            [this, holdings]() { on_empty_action(holdings, /*primary=*/true); });
    connect(page.empty, &ui::EmptyState::secondary_clicked, this,
            [this, holdings]() { on_empty_action(holdings, /*primary=*/false); });
}

void PortfolioMonitorScreen::update_tree_state(bool holdings) {
    TreePage& page = holdings ? hold_page_ : pos_page_;
    if (!page.stack)
        return;
    auto& svc = UnifiedPortfolioService::instance();
    const bool has_rows = holdings ? !svc.holdings().isEmpty() : !svc.positions().isEmpty();
    const int n_accounts = static_cast<int>(svc.accounts().size());
    const bool live = svc.mode() == UnifiedPortfolioService::Mode::Live;

    TreeState st = TreeState::Empty;
    if (has_rows)
        st = TreeState::Rows;
    else if (n_accounts == 0)
        st = TreeState::NoAccount;
    else if (page.awaiting)
        st = TreeState::Loading;
    else if (live && svc.summary().accounts_live == 0)
        st = TreeState::Disconnected;
    page.state = st;

    if (st == TreeState::Rows) {
        if (page.stack->currentWidget() != page.tree)
            page.stack->setCurrentWidget(page.tree);
        return;
    }
    if (st == TreeState::Loading) {
        const QString what = holdings ? tr("holdings") : tr("positions");
        page.skeleton->set_message(
            tr("Loading live %1 from %n broker account(s)…", nullptr, n_accounts).arg(what));
        if (page.stack->currentWidget() != page.skeleton)
            page.stack->setCurrentWidget(page.skeleton);
        return;
    }

    const QString key = QStringLiteral("%1|%2|%3").arg(int(st)).arg(live ? 1 : 0).arg(n_accounts);
    if (page.spec_key != key) {
        page.spec_key = key;
        ui::EmptyState::Spec spec;
        spec.illustration = true;
        spec.illustration_shape = ui::LoadingSkeleton::Shape::Table;
        switch (st) {
            case TreeState::NoAccount:
                spec.glyph = QStringLiteral("+");
                spec.glyph_tone = ui::EmptyState::Tone::Accent;
                spec.title = tr("No trading account");
                spec.message = tr("Positions and holdings of every connected %1 broker account — paper trading "
                                  "included — appear here.")
                                   .arg(monitor_ccy());
                spec.primary = tr("OPEN ACCOUNTS");
                break;
            case TreeState::Disconnected:
                spec.glyph = QStringLiteral("!");
                spec.glyph_tone = ui::EmptyState::Tone::Warning;
                spec.title = tr("Broker not connected");
                spec.message = tr("None of your %n account(s) is connected, so live %1 cannot be loaded. "
                                  "Reconnect it in Equity Trading > Accounts, or show the paper book.",
                                  nullptr, n_accounts)
                                   .arg(holdings ? tr("holdings") : tr("positions"));
                spec.primary = tr("OPEN ACCOUNTS");
                spec.secondary = tr("SHOW PAPER");
                break;
            default:
                spec.glyph = QStringLiteral("○");
                spec.glyph_tone = ui::EmptyState::Tone::Neutral;
                spec.title = holdings ? tr("No holdings") : tr("No open positions");
                spec.message = (live ? tr("Nothing is open in your %n live account(s).", nullptr, n_accounts)
                                     : tr("Nothing is open in the paper books of your %n account(s).", nullptr,
                                          n_accounts));
                spec.primary = tr("NEW ORDER");
                spec.secondary = tr("REFRESH");
                break;
        }
        page.empty->apply(spec);
    }
    if (page.stack->currentWidget() != page.empty)
        page.stack->setCurrentWidget(page.empty);
}

void PortfolioMonitorScreen::begin_awaiting_refresh() {
    auto& svc = UnifiedPortfolioService::instance();
    const bool wait = svc.mode() == UnifiedPortfolioService::Mode::Live && !svc.accounts().isEmpty();
    pos_page_.awaiting = wait;
    hold_page_.awaiting = wait;
    if (wait)
        await_timer_->start(kAwaitRefreshMs);
    else
        await_timer_->stop();
    update_tree_state(false);
    update_tree_state(true);
}

void PortfolioMonitorScreen::on_empty_action(bool holdings, bool primary) {
    const TreeState st = (holdings ? hold_page_ : pos_page_).state;
    switch (st) {
        case TreeState::NoAccount:
        case TreeState::Disconnected:
            if (primary) {
                ui::open_setup_link(ui::SetupLink{QStringLiteral("equity_trading"), QStringLiteral("accounts"), {}},
                                    this);
            } else if (mode_btn_->isChecked()) {
                mode_btn_->setChecked(false); // SHOW PAPER (fires on_mode_toggled)
            }
            break;
        case TreeState::Empty:
            if (primary) {
                on_new_order();
            } else {
                UnifiedPortfolioService::instance().refresh_all();
                begin_awaiting_refresh();
            }
            break;
        default:
            break;
    }
}

void PortfolioMonitorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!activated_) {
        // Apply the persisted mode BEFORE the first activate so the initial
        // model is built from the right engine. setChecked only emits toggled
        // on a state change, so style/route the paper default explicitly.
        const auto saved =
            SettingsRepository::instance().get(QStringLiteral("portfolio_monitor.mode"), QStringLiteral("paper"));
        if (saved.is_err()) {
            LOG_ERROR("PortfolioMonitor",
                      QString("settings read failed for 'portfolio_monitor.mode' — showing PAPER for this session, "
                              "leaving the stored mode untouched: %1")
                          .arg(QString::fromStdString(saved.error())));
            // The stored mode is unknown, not "paper". Apply paper in memory
            // (styles + routes the service) with the persist suppressed, and
            // block mode_btn_ so an already-checked button can't re-enter
            // on_mode_toggled() via toggled().
            const QSignalBlocker block(mode_btn_);
            setProperty(kModePersistSuppressedProp, true);
            on_mode_toggled(false);
            setProperty(kModePersistSuppressedProp, false);
        } else if (saved.value() == QLatin1String("live")) {
            mode_btn_->setChecked(true); // fires on_mode_toggled(true)
        } else {
            on_mode_toggled(false);
        }
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
    // LIVE: activate() asked every broker for a refresh — an empty tree shows the
    // skeleton until it lands (paper data is read synchronously above).
    begin_awaiting_refresh();
}

void PortfolioMonitorScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    await_timer_->stop();
    // P3/D3: stop the service's summary timer and per-tick patching while nobody can
    // see it. The account streams keep running (they're shared); the next showEvent's
    // activate() re-seeds from their caches and refreshes.
    UnifiedPortfolioService::instance().deactivate();
}

// ── UI construction ──────────────────────────────────────────────────────────

QWidget* PortfolioMonitorScreen::build_summary_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("pmSummaryBar");
    // Value labels are coloured by a "sign" property (pos/neg/flat) matched
    // here, instead of a setStyleSheet() per label on every summary tick. The
    // PAPER/LIVE button ("live") and the action toast ("ok") follow the same
    // pattern: one sheet, set before the children exist, switched by property.
    bar->setStyleSheet(QString("#pmSummaryBar{background:%3;border:1px solid %4;}"
                               "QLabel#pmStatValue{color:%5;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:600;}"
                               "QLabel#pmStatValue[sign=\"pos\"]{color:%1;}"
                               "QLabel#pmStatValue[sign=\"neg\"]{color:%2;}"
                               "QPushButton#pmModeBtn{background:%6;border:1px solid %7;color:%7;padding:4px 14px;"
                               "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;}"
                               "QPushButton#pmModeBtn[live=\"true\"]{border-color:%8;color:%8;}"
                               "QLabel#pmToast{color:%9;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                               "QLabel#pmToast[ok=\"true\"]{color:%10;}"
                               "QLabel#pmToast[ok=\"false\"]{color:%8;}")
                           .arg(fincept::ui::colors::POSITIVE(), fincept::ui::colors::NEGATIVE(),
                                fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM(),
                                fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::BG_RAISED(),
                                fincept::ui::colors::AMBER(), fincept::ui::colors::DANGER(),
                                fincept::ui::colors::TEXT_SECONDARY())
                           .arg(fincept::ui::colors::SUCCESS()));
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(10, 6, 10, 6);
    h->setSpacing(14);

    // ⧉ detach / ⤓ dock back (feeds pattern). The equity screen owns the
    // float window and the reparenting; this button only signals intent.
    // ↗ / ↙ (Arrows block): the app font has no glyph for ⧉ / ⤓, which drew as an empty box.
    float_btn_ = new QPushButton(QStringLiteral("↗"));
    float_btn_->setCursor(Qt::PointingHandCursor);
    float_btn_->setFixedSize(float_button_side(), float_button_side());
    float_btn_->setToolTip(tr("Detach into its own window"));
    float_btn_->setStyleSheet(QString("QPushButton{background:transparent;border:1px solid %1;color:%2;"
                                      "font-size:" + fincept::ui::fonts::px(-1) + "px;}"
                                      "QPushButton:hover{color:%3;border-color:%4;}")
                                  .arg(fincept::ui::colors::BORDER_MED(), fincept::ui::colors::TEXT_SECONDARY(),
                                       fincept::ui::colors::TEXT_PRIMARY(), fincept::ui::colors::AMBER()));
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
    mode_btn_->setObjectName(QStringLiteral("pmModeBtn"));
    mode_btn_->setCheckable(true); // checked == LIVE
    mode_btn_->setProperty("fincept_agent_risk", "account"); // paper <-> live switch
    mode_btn_->setCursor(Qt::PointingHandCursor);
    mode_btn_->setMinimumHeight(24);
    connect(mode_btn_, &QPushButton::toggled, this, &PortfolioMonitorScreen::on_mode_toggled);
    h->addWidget(mode_btn_);

    auto add_stat = [&](const QString& title) {
        auto* box = new QVBoxLayout;
        box->setSpacing(1);
        auto* cap = new QLabel(title);
        cap->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;")
                               .arg(fincept::ui::colors::TEXT_SECONDARY()));
        auto* val = new QLabel(QStringLiteral("—"));
        val->setObjectName("pmStatValue");
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
    toast_->setObjectName(QStringLiteral("pmToast")); // coloured by the bar's sheet ("ok" property)
    h->addWidget(toast_);

    auto* refresh = new QPushButton(tr("REFRESH"));
    auto* new_order = new QPushButton(tr("+ NEW ORDER"));
    auto* square_all = new QPushButton(tr("SQUARE OFF ALL"));
    for (auto* b : {refresh, new_order}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton{background:%1;border:1px solid %2;color:%3;"
                                 "padding:5px 12px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;}"
                                 "QPushButton:hover{border-color:%3;}")
                             .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_MED(),
                                  fincept::ui::colors::AMBER()));
    }
    square_all->setCursor(Qt::PointingHandCursor);
    square_all->setProperty("fincept_agent_risk", "order");
    square_all->setStyleSheet(QString("QPushButton{background:%1;border:1px solid %2;color:%3;"
                                      "padding:5px 12px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:600;}"
                                      "QPushButton:hover{border-color:%3;}")
                                  .arg(fincept::ui::colors::NEGATIVE_BG(), fincept::ui::colors::NEGATIVE_DIM(),
                                       fincept::ui::colors::NEGATIVE()));
    connect(refresh, &QPushButton::clicked, this, []() { UnifiedPortfolioService::instance().refresh_all(); });
    connect(new_order, &QPushButton::clicked, this, &PortfolioMonitorScreen::on_new_order);
    connect(square_all, &QPushButton::clicked, this, [this]() {
        const auto ret = QMessageBox::question(this, tr("Square Off All"),
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
    summary_scroll_ = scroller;
    return scroller;
}

void PortfolioMonitorScreen::set_floating(bool floating) {
    floating_ = floating;
    float_btn_->setText(floating ? QStringLiteral("↙") : QStringLiteral("↗"));
    float_btn_->setToolTip(floating ? tr("Dock back into the equity tab") : tr("Detach into its own window"));
}

void PortfolioMonitorScreen::on_mode_toggled(bool live) {
    mode_btn_->setText(live ? tr("LIVE") : tr("PAPER"));
    // Colour from the summary bar's sheet (DANGER outline = real money); re-polish only on a change.
    if (mode_btn_->property("live").toBool() != live) {
        mode_btn_->setProperty("live", live);
        mode_btn_->style()->unpolish(mode_btn_);
        mode_btn_->style()->polish(mode_btn_);
    }
    mode_btn_->setToolTip(live ? tr("Showing REAL broker positions — click for paper")
                               : tr("Showing paper portfolios — click for live"));
    UnifiedPortfolioService::instance().set_mode(live ? UnifiedPortfolioService::Mode::Live
                                                      : UnifiedPortfolioService::Mode::Paper);
    // Suppressed only by the showEvent() fallback after a failed read — writing
    // "paper" there would overwrite a stored "live".
    if (!property(kModePersistSuppressedProp).toBool())
        SettingsRepository::instance().set(QStringLiteral("portfolio_monitor.mode"),
                                           live ? QStringLiteral("live") : QStringLiteral("paper"));
    rebuild_positions();
    rebuild_holdings();
    update_summary();
    // A switch to LIVE asks the brokers for fresh books (the model holds only what the
    // streams cached meanwhile); the empty trees show the skeleton until they land.
    // The first show does this itself, after activate().
    if (activated_ && isVisible()) {
        if (live)
            UnifiedPortfolioService::instance().refresh_all();
        begin_awaiting_refresh();
    }
}

QTreeWidget* PortfolioMonitorScreen::make_tree(bool holdings, QWidget* parent) {
    auto* tree = new QTreeWidget(parent);
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
    // EXIT / EXIT ALL buttons are styled here once (they are children of the
    // tree's viewport) — rebuild_tree() used to call setStyleSheet per button,
    // i.e. a CSS parse per row on every positions refresh.
    tree->setStyleSheet(QString("QTreeWidget{background:%1;border:1px solid %2;font-size:" + fincept::ui::fonts::px(-2) +
                                "px;}"
                                "QTreeWidget::item{min-height:26px;}"
                                "QPushButton#pmExitBtn{background:transparent;border:1px solid %3;"
                                "color:%4;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 8px;}"
                                "QPushButton#pmExitBtn:hover{background:%5;}")
                            .arg(fincept::ui::colors::BG_BASE(), fincept::ui::colors::BORDER_DIM(),
                                 fincept::ui::colors::NEGATIVE_DIM(), fincept::ui::colors::NEGATIVE(),
                                 fincept::ui::colors::NEGATIVE_BG()));
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeWidget::customContextMenuRequested, this,
            [this, tree, holdings](const QPoint& pos) { show_row_menu(tree, holdings, pos); });
    return tree;
}

void PortfolioMonitorScreen::fill_parent(QTreeWidgetItem* item, const AggRow& row, bool holdings) {
    item->setText(ColSymbol, row.exchange.isEmpty() ? row.symbol : QString("%1  ·  %2").arg(row.symbol, row.exchange));
    if (holdings) {
        set_pnl_cell(item, ColProduct, row.day_pnl, fmt_money(row.day_pnl, true));
        item->setToolTip(ColSymbol,
                         tr("Invested %1   Current %2").arg(fmt_money(row.invested), fmt_money(row.current)));
    } else {
        item->setText(ColProduct, QString::number(row.children.size()) + tr(" acct"));
    }
    item->setText(ColQty, fmt_qty(row.total_qty));
    item->setText(ColAvg, row.w_avg_price > 0 ? fmt_price(row.w_avg_price, row.symbol) : QStringLiteral("—"));
    item->setText(ColLtp, row.ltp > 0 ? fmt_price(row.ltp, row.symbol) : QStringLiteral("—"));
    set_pnl_cell(item, ColPnl, row.pnl, fmt_money(row.pnl, true));
    const double base = row.w_avg_price * std::fabs(row.total_qty);
    const double pct = base > 0 ? row.pnl / base * 100.0 : 0.0;
    set_pnl_cell(item, ColPnlPct, row.pnl, fmt_pct(pct));
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
    // An empty book is not drawn in the tree: update_tree_state() shows the
    // skeleton (refresh on its way) or an empty state that says why.

    for (const AggRow& row : rows) {
        auto* parent = new QTreeWidgetItem(tree);
        fill_parent(parent, row, holdings);
        parent->setData(ColSymbol, Qt::UserRole, row.symbol);
        parent->setData(ColSymbol, Qt::UserRole + 1, row.exchange);
        const QString key = row_key(row.symbol, row.exchange);
        index.insert(key, parent);

        // Parent-level EXIT ALL (only meaningful with >1 child; single-child
        // symbols get their action on the child row).
        if (row.children.size() > 1) {
            auto* exit_all = new QPushButton(tr("EXIT ALL"));
            exit_all->setCursor(Qt::PointingHandCursor);
            exit_all->setObjectName("pmExitBtn");
            exit_all->setProperty("fincept_agent_risk", "order");
            const QString sym = row.symbol, exch = row.exchange;
            const int n = int(row.children.size());
            connect(exit_all, &QPushButton::clicked, this,
                    [this, sym, exch, holdings, n]() { confirm_exit_symbol(sym, exch, holdings, n); });
            tree->setItemWidget(parent, ColAction, exit_all);
        }

        for (const AggChild& c : row.children) {
            auto* child = new QTreeWidgetItem(parent);
            child->setText(ColSymbol, c.broker_label);
            child->setForeground(ColSymbol, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            if (holdings)
                set_pnl_cell(child, ColProduct, c.day_pnl, fmt_money(c.day_pnl, true));
            else
                child->setText(ColProduct, c.product + (c.side.isEmpty() ? QString() : "·" + c.side));
            child->setText(ColQty, fmt_qty(c.qty));
            child->setText(ColAvg, fmt_price(c.avg_price, row.symbol));
            child->setText(ColLtp, c.ltp > 0 ? fmt_price(c.ltp, row.symbol) : QStringLiteral("—"));
            set_pnl_cell(child, ColPnl, c.pnl, fmt_money(c.pnl, true));
            const double notional = c.avg_price * std::fabs(c.qty);
            set_pnl_cell(child, ColPnlPct, c.pnl, fmt_pct(notional > 0 ? c.pnl / notional * 100.0 : 0.0));

            auto* exit_btn = new QPushButton(tr("EXIT"));
            exit_btn->setCursor(Qt::PointingHandCursor);
            exit_btn->setObjectName("pmExitBtn");
            exit_btn->setProperty("fincept_agent_risk", "order");
            const QString acct = c.account_id, label = c.broker_label, sym = row.symbol, exch = row.exchange,
                          product = c.product;
            const double qty = c.qty;
            connect(exit_btn, &QPushButton::clicked, this, [this, acct, label, sym, exch, product, qty, holdings]() {
                if (holdings && UnifiedPortfolioService::instance().mode() == UnifiedPortfolioService::Mode::Live)
                    confirm_sell_holding(acct, label, sym, exch, qty);
                else
                    confirm_exit_child(acct, label, sym, exch, product, qty);
            });
            tree->setItemWidget(child, ColAction, exit_btn);
        }

        parent->setExpanded(expanded.contains(key) || rows.size() <= 4);
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void PortfolioMonitorScreen::rebuild_positions() {
    rebuild_tree(positions_tree_, UnifiedPortfolioService::instance().positions(), /*holdings=*/false, pos_index_);
    update_tree_state(/*holdings=*/false);
}

void PortfolioMonitorScreen::rebuild_holdings() {
    rebuild_tree(holdings_tree_, UnifiedPortfolioService::instance().holdings(), /*holdings=*/true, hold_index_);
    update_tree_state(/*holdings=*/true);
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
        child->setText(ColLtp, c.ltp > 0 ? fmt_price(c.ltp, row.symbol) : QStringLiteral("—"));
        set_pnl_cell(child, ColPnl, c.pnl, fmt_money(c.pnl, true));
        const double notional = c.avg_price * std::fabs(c.qty);
        set_pnl_cell(child, ColPnlPct, c.pnl, fmt_pct(notional > 0 ? c.pnl / notional * 100.0 : 0.0));
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
    // Re-polish only when the sign flips; the colour rules live on #pmSummaryBar.
    auto set_sign = [](QLabel* l, double v) {
        const QString sign = v > 0 ? QStringLiteral("pos") : v < 0 ? QStringLiteral("neg") : QStringLiteral("flat");
        if (l->property("sign").toString() == sign)
            return;
        l->setProperty("sign", sign);
        l->style()->unpolish(l);
        l->style()->polish(l);
    };
    auto set = [&set_sign](QLabel* l, double v, bool color, bool signed_pfx = true) {
        l->setText(fmt_money(v, signed_pfx));
        if (color)
            set_sign(l, v);
    };
    set(sum_positions_pnl_, s.positions_pnl, true);
    set(sum_day_pnl_, s.positions_day_pnl + s.holdings_day_pnl, true);
    set(sum_holdings_pnl_, s.holdings_pnl, true);
    set(sum_invested_, s.invested, false, false);
    set(sum_current_, s.current, false, false);
    sum_return_->setText(fmt_pct(s.return_pct));
    set_sign(sum_return_, s.return_pct);
    sum_accounts_->setText(tr("%1/%2 LIVE").arg(s.accounts_live).arg(s.accounts_total));
    // The account set / live count decides between "no account", "broker not
    // connected" and "nothing open" for an empty tree.
    update_tree_state(/*holdings=*/false);
    update_tree_state(/*holdings=*/true);
}

void PortfolioMonitorScreen::on_action_finished(bool ok, const QString& message) {
    toast_->setText(message);
    // State colour (done / failed) from the summary bar's sheet; re-polish only on a change.
    const QString state = ok ? QStringLiteral("true") : QStringLiteral("false");
    if (toast_->property("ok").toString() != state) {
        toast_->setProperty("ok", state);
        toast_->style()->unpolish(toast_);
        toast_->style()->polish(toast_);
    }
    QTimer::singleShot(8000, toast_, [t = toast_]() { t->clear(); });
}

// ── Row menu ─────────────────────────────────────────────────────────────────

void PortfolioMonitorScreen::show_row_menu(QTreeWidget* tree, bool holdings, const QPoint& pos) {
    QTreeWidgetItem* item = tree->itemAt(pos);
    while (item && item->parent())
        item = item->parent(); // account rows act on their symbol
    const QString symbol = item ? item->data(ColSymbol, Qt::UserRole).toString() : QString();
    const QString exchange = item ? item->data(ColSymbol, Qt::UserRole + 1).toString() : QString();

    QMenu menu(this);
    if (!symbol.isEmpty()) {
        // Equity Research / News / Watchlist / Backtesting key on Yahoo tickers;
        // the broker symbol needs its exchange suffix for those.
        const QString ex = exchange.toUpper();
        const QString yahoo = ex == QLatin1String("NSE")   ? symbol + QStringLiteral(".NS")
                              : ex == QLatin1String("BSE") ? symbol + QStringLiteral(".BO")
                                                           : symbol;
        QMenu* open_in = menu.addMenu(tr("Open %1 in").arg(symbol));
        const struct {
            const char* screen;
            const char* label;
            bool broker_symbol;
        } kTargets[] = {
            {"equity_trading", QT_TR_NOOP("Equity Trading"), true},
            {"equity_research", QT_TR_NOOP("Equity Research"), false},
            {"news", QT_TR_NOOP("News"), false},
            {"watchlist", QT_TR_NOOP("Watchlist"), false},
        };
        for (const auto& t : kTargets) {
            const QString screen = QString::fromLatin1(t.screen);
            const QString sym = t.broker_symbol ? symbol : yahoo;
            const QString exch = t.broker_symbol ? exchange : QString();
            connect(open_in->addAction(tr(t.label)), &QAction::triggered, this, [screen, sym, exch]() {
                QVariantMap payload{{"screen_id", screen}, {"symbol", sym}};
                if (!exch.isEmpty())
                    payload.insert("exchange", exch);
                fincept::EventBus::instance().publish("nav.open_symbol", payload);
            });
        }
        connect(menu.addAction(tr("Copy Symbol")), &QAction::triggered, this,
                [symbol]() { QGuiApplication::clipboard()->setText(symbol); });
        menu.addSeparator();
    }
    connect(menu.addAction(holdings ? tr("Export Holdings (CSV)…") : tr("Export Positions (CSV)…")),
            &QAction::triggered, this, [this, holdings]() { export_csv(holdings); });
    menu.exec(tree->viewport()->mapToGlobal(pos));
}

void PortfolioMonitorScreen::export_csv(bool holdings) {
    const auto rows_src =
        holdings ? UnifiedPortfolioService::instance().holdings() : UnifiedPortfolioService::instance().positions();
    if (rows_src.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("Nothing to export."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export CSV"),
        QStringLiteral("%1_%2.csv")
            .arg(holdings ? QStringLiteral("holdings") : QStringLiteral("positions"),
                 QDate::currentDate().toString(Qt::ISODate)),
        tr("CSV Files (*.csv)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write %1:\n%2").arg(path, f.errorString()));
        return;
    }
    auto esc = [](QString v) {
        if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))) {
            v.replace(QLatin1String("\""), QLatin1String("\"\""));
            v = QLatin1Char('"') + v + QLatin1Char('"');
        }
        return v;
    };
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    // Machine-readable numbers (C locale, no grouping); every price and amount is in
    // the aggregate currency, named in the appended ISO code column.
    namespace fmt = ui::formatting;
    const QString ccy = monitor_ccy();
    out << "symbol,exchange,account,product,side,quantity,avg_price,ltp,pnl,day_pnl,currency\n";
    for (const AggRow& row : rows_src) {
        for (const AggChild& c : row.children) {
            out << esc(row.symbol) << ',' << esc(row.exchange) << ',' << esc(c.broker_label) << ','
                << esc(c.product) << ',' << esc(c.side) << ',' << fmt::export_number(c.qty, 4) << ','
                << fmt::export_number(c.avg_price, 4) << ',' << fmt::export_number(c.ltp, 4) << ','
                << fmt::export_number(c.pnl, 2) << ',' << fmt::export_number(c.day_pnl, 2) << ',' << ccy << '\n';
        }
    }
    out.flush();
    on_action_finished(out.status() == QTextStream::Ok,
                       out.status() == QTextStream::Ok ? tr("Exported to %1").arg(path) : tr("Write error: %1").arg(path));
}

// ── Actions ──────────────────────────────────────────────────────────────────

void PortfolioMonitorScreen::confirm_exit_child(const QString& account_id, const QString& broker_label,
                                                const QString& symbol, const QString& exchange, const QString& product,
                                                double qty) {
    const auto ret =
        QMessageBox::question(this, tr("Exit Position"),
                              tr("Square off %1 × %2 on %3 (%4)?").arg(fmt_qty(qty), symbol, broker_label, product),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        UnifiedPortfolioService::instance().exit_child(account_id, symbol, exchange, product);
}

void PortfolioMonitorScreen::confirm_sell_holding(const QString& account_id, const QString& broker_label,
                                                  const QString& symbol, const QString& exchange, double qty) {
    if (!(qty > 0.0))
        return;
    const auto ret = QMessageBox::question(
        this, tr("Sell Holding"),
        tr("Place a REAL delivery SELL of %1 × %2 (%3) at MARKET on %4?").arg(fmt_qty(qty), symbol, exchange, broker_label),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;
    if (UnifiedPortfolioService::instance().mode() != UnifiedPortfolioService::Mode::Live)
        return; // mode changed meanwhile — the confirmation named a REAL order
    trading::UnifiedOrder order;
    order.symbol = symbol;
    order.exchange = exchange;
    order.side = trading::OrderSide::Sell;
    order.order_type = trading::OrderType::Market;
    order.quantity = qty;
    order.product_type = trading::ProductType::Delivery;
    UnifiedPortfolioService::instance().place_new_order(account_id, order);
}

void PortfolioMonitorScreen::confirm_exit_symbol(const QString& symbol, const QString& exchange, bool holdings,
                                                 int n_accounts) {
    if (holdings && UnifiedPortfolioService::instance().mode() == UnifiedPortfolioService::Mode::Live) {
        // Same reason as confirm_sell_holding(): close_position() only searches the
        // positions book, so EXIT ALL on a LIVE holding failed for every account.
        QVector<AggChild> children;
        for (const AggRow& r : UnifiedPortfolioService::instance().holdings())
            if (r.symbol == symbol && r.exchange == exchange)
                children = r.children;
        if (children.isEmpty())
            return;
        const auto sell_ret = QMessageBox::question(
            this, tr("Sell Holding"),
            tr("Place REAL delivery SELL orders at MARKET for the whole %1 holding across %2 account(s)?")
                .arg(symbol)
                .arg(children.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (sell_ret != QMessageBox::Yes ||
            UnifiedPortfolioService::instance().mode() != UnifiedPortfolioService::Mode::Live)
            return;
        for (const AggChild& c : children) {
            if (!(c.qty > 0.0))
                continue;
            trading::UnifiedOrder order;
            order.symbol = symbol;
            order.exchange = exchange;
            order.side = trading::OrderSide::Sell;
            order.order_type = trading::OrderType::Market;
            order.quantity = c.qty;
            order.product_type = trading::ProductType::Delivery;
            UnifiedPortfolioService::instance().place_new_order(c.account_id, order);
        }
        return;
    }
    const auto ret = QMessageBox::question(this, tr("Exit All"),
                                           tr("Square off %1 across %2 account(s)?").arg(symbol).arg(n_accounts),
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        UnifiedPortfolioService::instance().exit_symbol(symbol, exchange, holdings);
}

void PortfolioMonitorScreen::on_new_order() {
    const auto accounts = UnifiedPortfolioService::instance().accounts();
    if (accounts.isEmpty()) {
        QMessageBox::information(this, tr("New Order"),
                                 tr("No active broker account in %1. The monitor aggregates connected broker accounts "
                                    "that trade in %1.")
                                     .arg(monitor_ccy()));
        return;
    }

    // Which engine the order hits is the service's mode (the PAPER/LIVE switch). It was
    // never shown here, and a LIVE order went out with no confirmation step.
    const bool live = UnifiedPortfolioService::instance().mode() == UnifiedPortfolioService::Mode::Live;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("New Order - %1").arg(live ? tr("LIVE") : tr("PAPER")));
    auto* form = new QFormLayout(&dlg);

    auto* mode_lbl = new QLabel(live ? tr("LIVE - places a REAL order with the selected broker account.")
                                     : tr("PAPER - simulated order on the account's paper portfolio; nothing is "
                                          "sent to the broker."));
    mode_lbl->setWordWrap(true);
    mode_lbl->setStyleSheet(QString("color:%1;font-weight:700;")
                                .arg(live ? fincept::ui::colors::NEGATIVE() : fincept::ui::colors::AMBER()));
    form->addRow(mode_lbl);

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
    ui::formatting::bind_currency_affix(price_spin, monitor_ccy()); // a price in the accounts' currency
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

    // Validation message shown inside the dialog, which stays open until the input is valid.
    auto* error_lbl = new QLabel;
    error_lbl->setWordWrap(true);
    error_lbl->setStyleSheet(QString("color:%1;").arg(fincept::ui::colors::NEGATIVE()));
    error_lbl->hide();
    form->addRow(error_lbl);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("PLACE ORDER"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("fincept_agent_risk", "order");
    // An empty symbol used to close the dialog and do nothing; a LIMIT left at 0.00 was
    // sent as a limit order at price 0.
    connect(buttons, &QDialogButtonBox::accepted, &dlg,
            [&dlg, symbol_edit, type_combo, price_spin, error_lbl]() {
                QString why;
                if (symbol_edit->text().trimmed().isEmpty())
                    why = tr("Enter a symbol.");
                else if (type_combo->currentIndex() == 1 && !(price_spin->value() > 0.0))
                    why = tr("Enter a limit price greater than 0.");
                if (!why.isEmpty()) {
                    error_lbl->setText(why);
                    error_lbl->show();
                    return;
                }
                dlg.accept();
            });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString symbol = symbol_edit->text().trimmed().toUpper();
    const bool is_limit = type_combo->currentIndex() == 1;
    if (symbol.isEmpty() || (is_limit && !(price_spin->value() > 0.0)))
        return; // validated in the dialog; never send an order without a symbol or a limit price

    // The mode could only change from outside while the dialog was open; never send to an
    // engine the user did not see named.
    if ((UnifiedPortfolioService::instance().mode() == UnifiedPortfolioService::Mode::Live) != live) {
        QMessageBox::warning(this, tr("New Order"),
                             tr("The PAPER/LIVE mode changed while the order was being entered. Nothing was sent."));
        return;
    }
    if (live) {
        const QString type_text =
            is_limit ? tr("LIMIT @ %1").arg(fmt_price(price_spin->value(), symbol)) : tr("MARKET");
        const auto ret = QMessageBox::question(
            this, tr("Confirm LIVE Order"),
            tr("Place a REAL %1 order for %2 × %3 (%4, %5, %6) on %7?")
                .arg(side_combo->currentText(), QString::number(qty_spin->value()), symbol, exch_combo->currentText(),
                     type_text, product_combo->currentText(), acct_combo->currentText()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;
    }

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
