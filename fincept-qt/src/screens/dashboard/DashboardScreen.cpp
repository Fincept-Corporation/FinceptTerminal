#include "screens/dashboard/DashboardScreen.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/events/EventBus.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/common/CommandRows.h"
#include "screens/dashboard/canvas/AddWidgetDialog.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/TemplatePicker.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/NotifToast.h"

#include "core/market/MarketProfile.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QCursor>
#include <QDataStream>
#include <QLocale>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QUuid>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QKeySequence>
#include <QPalette>
#include <QPointer>
#include <QShortcut>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {

void apply_solid_background(QWidget* widget, const QColor& color) {
    if (!widget)
        return;

    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setAutoFillBackground(true);

    QPalette pal = widget->palette();
    pal.setColor(QPalette::Window, color);
    pal.setColor(QPalette::Base, color);
    pal.setColor(QPalette::Button, color);
    widget->setPalette(pal);
}

// QWidget::setStyleSheet() re-styles the widget and everything inside it even
// when the text is the one already in place. refresh_theme() runs on every
// show of the dashboard, so an unchanged sheet must not be set again — with
// the tiles built that re-style froze the UI for 2-3 s on each visit.
void set_sheet_if_changed(QWidget* widget, const QString& sheet) {
    if (widget && widget->styleSheet() != sheet)
        widget->setStyleSheet(sheet);
}

// The ticker strip paints nothing while it holds no quotes, so a failed fetch
// left it silently blank. This label — a child of the strip, below its inline
// editor and transparent to the mouse so the strip's own double-click and
// right-click menu still work — shows the hub's reason in that empty space.
constexpr const char* kDashTickerStatusName = "dashTickerStatus";

QLabel* dash_ticker_status_label(QWidget* ticker, bool create) {
    if (!ticker)
        return nullptr;
    auto* lbl = ticker->findChild<QLabel*>(QString::fromLatin1(kDashTickerStatusName), Qt::FindDirectChildrenOnly);
    if (!lbl && create) {
        lbl = new QLabel(ticker);
        lbl->setObjectName(QString::fromLatin1(kDashTickerStatusName));
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        lbl->hide();
    }
    return lbl;
}

void dash_ticker_show_status(QWidget* ticker, const QString& text, const QString& reason) {
    auto* lbl = dash_ticker_status_label(ticker, true);
    if (!lbl)
        return;
    lbl->setStyleSheet(QString("color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; padding-left:10px; background:transparent;")
                           .arg(fincept::ui::colors::TEXT_TERTIARY()));
    lbl->setText(text);
    lbl->setToolTip(reason);
    lbl->setGeometry(ticker->rect());
    lbl->lower();
    lbl->show();
}

void dash_ticker_clear_status(QWidget* ticker) {
    if (auto* lbl = dash_ticker_status_label(ticker, false))
        lbl->hide();
}

} // namespace

namespace fincept::screens {

DashboardScreen::DashboardScreen(QWidget* parent) : QWidget(parent) {
    apply_solid_background(this, QColor(ui::colors::BG_BASE()));
    refresh_theme();

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Every part is created under its final parent. Built parentless and then moved
    // into the screen, each subtree (the Market Pulse panel above all) was re-styled
    // on the move — most of a ~1.6 s GUI stall on the first open of the dashboard.

    // ── Top Toolbar ──
    toolbar_ = new DashboardToolBar(this);
    vl->addWidget(toolbar_);

    // ── Scrolling Ticker Bar ──
    ticker_bar_ = new TickerBar(this);
    vl->addWidget(ticker_bar_);
    ticker_bar_->installEventFilter(this); // keeps the failure label (see hub_resubscribe_ticker) sized
    // When user saves a new symbol list, re-fetch quotes immediately.
    connect(ticker_bar_, &TickerBar::symbols_changed, this, &DashboardScreen::refresh_ticker);
    // Double-click a symbol in the strip → open it in Equity Research.
    connect(ticker_bar_, &TickerBar::symbol_activated, this, [](const QString& sym) {
        EventBus::instance().publish("nav.open_symbol",
                                     QVariantMap{{"screen_id", "equity_research"}, {"symbol", sym}});
    });

    // ── Enterprise strip (dismissible until the next release) ──
    if (!ui::enterprise::placement_dismissed(QStringLiteral("dashboard"))) {
        ent_strip_ = build_enterprise_strip();
        vl->addWidget(ent_strip_);
    }

    // ── Main Content: Canvas (in scroll) + Market Pulse ──
    content_split_ = new QSplitter(Qt::Horizontal, this);
    content_split_->setHandleWidth(1);
    content_split_->setStyleSheet(QString("QSplitter::handle{background:%1;}QSplitter::handle:hover{background:%2;}")
                                      .arg(ui::colors::BORDER_DIM(), ui::colors::BORDER_MED()));

    canvas_ = new DashboardCanvas; // empty here (tiles stream in later): moving it is cheap

    // Scroll area wraps canvas — provides vertical scroll when content exceeds viewport
    scroll_area_ = new QScrollArea(content_split_);
    scroll_area_->setWidgetResizable(false); // canvas controls its own size
    scroll_area_->setMinimumWidth(0);        // allow ADS to shrink this panel freely
    scroll_area_->setWidget(canvas_);
    scroll_area_->setStyleSheet(QString("QScrollArea{border:none;background:%1;}"
                                        "QScrollBar:vertical{width:6px;background:transparent;}"
                                        "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
    scroll_area_->viewport()->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));

    // Sync canvas width to scroll viewport via event filter
    scroll_area_->viewport()->installEventFilter(this);

    market_pulse_ = new MarketPulsePanel(content_split_);

    content_split_->addWidget(scroll_area_);
    content_split_->addWidget(market_pulse_);
    content_split_->setStretchFactor(0, 3);
    content_split_->setStretchFactor(1, 1);
    content_split_->setCollapsible(0, false);
    content_split_->setCollapsible(1, false);
    market_pulse_->setMinimumWidth(180);

    vl->addWidget(content_split_, 1);

    // ── Bottom Status Bar ──
    status_bar_ = new DashboardStatusBar(this);
    vl->addWidget(status_bar_);

    // ── In-app notification toast (overlay, not in layout) ────────────────────
    notif_toast_ = new fincept::ui::NotifToast(this);
    connect(&fincept::notifications::NotificationService::instance(),
            &fincept::notifications::NotificationService::notification_received, notif_toast_,
            &fincept::ui::NotifToast::show_notification);

    // ── Debounced save timer (P3: not started in constructor) ──
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(800);
    connect(save_timer_, &QTimer::timeout, this, &DashboardScreen::save_layout);

    // ── Canvas signals → toolbar/statusbar ──
    connect(canvas_, &DashboardCanvas::widget_count_changed, toolbar_, &DashboardToolBar::set_widget_count);
    connect(canvas_, &DashboardCanvas::widget_count_changed, status_bar_, &DashboardStatusBar::set_widget_count);
    // The toolbar's LIVE badge was hardcoded — set_connected() had no caller
    // anywhere in the codebase. Drive it from the status bar's API health probe.
    connect(status_bar_, &DashboardStatusBar::connectivity_changed, toolbar_, &DashboardToolBar::set_connected);
    connect(canvas_, &DashboardCanvas::layout_changed, this, [this](const GridLayout&) { save_timer_->start(); });
    // Every tile closed: say so and offer the two ways to fill the canvas.
    connect(canvas_, &DashboardCanvas::widget_count_changed, this, [this](int) { update_canvas_empty_state(); });

    // ── Toolbar buttons ──
    // Kept as named lambdas so the keyboard shortcuts below drive exactly the
    // same code path as the toolbar buttons.
    auto toggle_pulse = [this]() {
        pulse_visible_ = !pulse_visible_;
        market_pulse_->setVisible(pulse_visible_);
    };
    auto open_add_widget = [this]() { open_add_widget_dialog(); };

    connect(toolbar_, &DashboardToolBar::toggle_pulse_clicked, this, toggle_pulse);

    connect(toolbar_, &DashboardToolBar::add_widget_clicked, this, open_add_widget);

    connect(toolbar_, &DashboardToolBar::reset_layout_clicked, this, [this]() { open_template_picker(); });

    connect(toolbar_, &DashboardToolBar::save_layout_clicked, this, &DashboardScreen::save_layout);
    connect(toolbar_, &DashboardToolBar::layouts_menu_requested, this, &DashboardScreen::show_layouts_menu);

    connect(toolbar_, &DashboardToolBar::refresh_clicked, this, &DashboardScreen::on_refresh_clicked);

    connect(toolbar_, &DashboardToolBar::toggle_compact_clicked, this, [this]() {
        // Derive the current mode from the canvas, not from a flag: the row
        // height is persisted with the layout (templates load compact).
        const bool to_compact = canvas_->row_height() > kGridRowCompact;
        canvas_->set_row_height(to_compact ? kGridRowCompact : kGridRowComfortable);
        sync_compact_button();
    });
    connect(canvas_, &DashboardCanvas::layout_changed, this, [this](const GridLayout&) { sync_compact_button(); });

    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled; refresh_theme() sets each sheet only when it changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    // The dashboard had zero keyboard affordances; every action needed a mouse
    // trip to the toolbar. WindowShortcut scope so they fire while any child
    // of the dashboard has focus, but not while another screen is active.
    auto add_shortcut = [this](QKeySequence seq, auto&& handler) {
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, handler);
    };
    // No local F5 / Ctrl+P: those are the global Refresh and Screenshot actions in
    // the same window, and two window-scoped shortcuts on one key are ambiguous to
    // Qt, so neither fired. F5 still refreshes the dashboard — the frame's Refresh
    // action invokes the focused panel's on_refresh_clicked() slot.
    add_shortcut(QKeySequence(QKeySequence::Save), [this]() { save_layout(); });
    add_shortcut(QKeySequence(Qt::CTRL | Qt::Key_N), open_add_widget);

    // Agents: the frame's Refresh key (F5 by default) runs on_refresh_clicked(), which also
    // reloads the billed tiles — the same kind as the toolbar REFRESH button ("spend").
    // The key is handled outside this screen, so it is named for the ancestor walk.
    auto sync_refresh_key = [this]() {
        const QString key =
            KeyConfigManager::instance().key(KeyAction::Refresh).toString(QKeySequence::PortableText);
        setProperty(fincept::core::agent::kAgentRiskKeysProperty,
                    key.isEmpty() ? QVariant() : QVariant(key + QStringLiteral("=spend")));
    };
    sync_refresh_key();
    connect(&KeyConfigManager::instance(), &KeyConfigManager::key_changed, this,
            [sync_refresh_key](KeyAction a, const QKeySequence&) {
                if (a == KeyAction::Refresh)
                    sync_refresh_key();
            });
}

void DashboardScreen::open_add_widget_dialog() {
    // open() (window-modal, no nested event loop) + delete-on-close instead of
    // exec(): the selection arrives through the dialog's signal anyway.
    auto* dlg = new AddWidgetDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &AddWidgetDialog::widget_selected, canvas_, &DashboardCanvas::add_widget);
    dlg->open();
}

void DashboardScreen::open_template_picker() {
    auto* dlg = new TemplatePicker(canvas_->template_id(), canvas_->is_pristine() || canvas_->tile_count() == 0, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // Queued: the picker closes (and repaints away) before anything else runs.
    connect(dlg, &TemplatePicker::template_selected, this, &DashboardScreen::request_template, Qt::QueuedConnection);
    dlg->open();
}

void DashboardScreen::update_canvas_empty_state() {
    if (!canvas_ || !scroll_area_)
        return;
    const bool empty = canvas_->is_empty();
    if (!empty) {
        if (canvas_empty_)
            canvas_empty_->hide();
        return;
    }
    if (!canvas_empty_) {
        // A child of the scroll viewport (beside the canvas, not in it): centred in
        // what the user sees, sized by eventFilter() with the viewport.
        ui::EmptyState::Spec spec = ui::EmptyState::no_data_spec(
            tr("Dashboard is empty"),
            tr("Add widgets one by one — quotes, charts, news, portfolio, broker and AI tiles — or start from a "
               "ready-made template."),
            tr("ADD WIDGET"));
        spec.glyph = QStringLiteral("▦"); // ▦
        spec.glyph_tone = ui::EmptyState::Tone::Accent;
        spec.secondary = tr("TEMPLATES");
        canvas_empty_ = ui::EmptyState::create(spec, scroll_area_->viewport());
        connect(canvas_empty_, &ui::EmptyState::primary_clicked, this, &DashboardScreen::open_add_widget_dialog);
        connect(canvas_empty_, &ui::EmptyState::secondary_clicked, this, &DashboardScreen::open_template_picker);
    }
    canvas_empty_->setGeometry(scroll_area_->viewport()->rect());
    canvas_empty_->raise();
    canvas_empty_->show();
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void DashboardScreen::refresh_theme() {
    const QColor bg(ui::colors::BG_BASE());
    apply_solid_background(this, bg);

    // `*` is the blanket background this sheet always applied (a bare
    // declaration list is parsed as `* { … }`); the #dashEntStrip rules style
    // the Enterprise strip without per-widget sheets.
    set_sheet_if_changed(this, QString("*{background:%1;}"
                          "QFrame#dashEntStrip{background:%2;border:none;border-bottom:1px solid %3;}"
                          "QLabel#dashEntStripHead{background:transparent;color:%4;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;"
                          "  letter-spacing:1px;}"
                          "QLabel#dashEntStripText{background:transparent;color:%5;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                          "QPushButton#dashEntStripCta{background:transparent;color:%4;border:1px solid %6;"
                          "  padding:2px 10px;font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:0.5px;}"
                          "QPushButton#dashEntStripCta:hover{color:%7;border-color:%4;}"
                          "QPushButton#dashEntStripClose{background:transparent;color:%8;border:none;padding:0 6px;"
                          "  font-size:" + fincept::ui::fonts::px(0) + "px;}"
                          "QPushButton#dashEntStripClose:hover{color:%7;}")
                      .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
                           ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER_DIM(),
                           ui::colors::TEXT_PRIMARY(), ui::colors::ICON_DIM()));
    if (content_split_) {
        apply_solid_background(content_split_, bg);
        set_sheet_if_changed(content_split_,
            QString("QSplitter{background:%1;}QSplitter::handle{background:%2;}QSplitter::handle:hover{background:%3;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::BORDER_MED()));
    }
    if (scroll_area_) {
        apply_solid_background(scroll_area_, bg);
        set_sheet_if_changed(scroll_area_,
            QString("QScrollArea{border:none;background:%1;}"
                    "QScrollBar:vertical{width:6px;background:transparent;}"
                    "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
        // Ensure viewport doesn't paint its own opaque background over the canvas
        apply_solid_background(scroll_area_->viewport(), bg);
        set_sheet_if_changed(scroll_area_->viewport(), QString("background:%1;").arg(ui::colors::BG_BASE()));
    }
}

// ── Show/Hide ─────────────────────────────────────────────────────────────────

void DashboardScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_theme();

    if (ticker_bar_) {
        // Honour the appearance settings that were previously saved but never
        // read: "Show Ticker Bar" toggles visibility; "Enable Animations" toggles
        // the scroll (off → the bar shows its symbols but doesn't animate).
        auto& repo = SettingsRepository::instance();
        const auto tr = repo.get("appearance.show_ticker_bar");
        const bool show_ticker = !tr.is_ok() ? true : tr.value() != "false";
        const auto ar = repo.get("appearance.animations");
        const bool animations = !ar.is_ok() ? true : ar.value() != "false";
        ticker_bar_->setVisible(show_ticker);
        if (show_ticker && animations)
            ticker_bar_->resume();
        else
            ticker_bar_->pause();
    }

    // Subscribe to current ticker symbols — hub schedules refreshes per TopicPolicy.
    hub_resubscribe_ticker(/*force_fetch=*/false);

    // Set splitter sizes on first show using actual pixel width.
    // Must be done here (not in constructor) because the widget has no size yet
    // during construction. MarketPulsePanel's large sizeHint otherwise causes it
    // to claim ~70% of the splitter before the user sees it.
    if (!split_sized_) {
        split_sized_ = true;
        const int total = content_split_->width();
        if (total > 400) {
            const int pulse_w = qBound(180, total / 4, 320);
            content_split_->setSizes({total - pulse_w, pulse_w});
        }
    }

    if (!layout_restored_) {
        layout_restored_ = true;
        restore_layout();
    }
}

void DashboardScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_bar_)
        ticker_bar_->pause();
    hub_unsubscribe_ticker();
}

// ── Ticker refresh ────────────────────────────────────────────────────────────

void DashboardScreen::refresh_ticker() {
    if (!ticker_bar_)
        return;
    // TickerBar emits symbols_changed when its saved list finishes loading from
    // the settings DB (async, started in its constructor). That can land while
    // the dashboard is not on screen; subscribing then would leave the hub
    // subscription alive past hideEvent(). showEvent() re-reads
    // ticker_bar_->symbols() and subscribes, so nothing is lost by skipping.
    if (!isVisible())
        return;
    // Hub path: user edited symbols → drop old subs, attach to new set,
    // kick the hub so consumers see data immediately.
    //
    // Note: this used to early-return on an empty symbol list, which left the
    // *previous* symbols subscribed and still scrolling after the user cleared
    // the bar. hub_resubscribe_ticker() handles the empty case correctly
    // (unsubscribe, then no-op).
    hub_resubscribe_ticker(/*force_fetch=*/true);
}

void DashboardScreen::on_refresh_clicked() {
    // Toolbar REFRESH button — force-refresh every live data source on the
    // dashboard. The hub's per-producer rate limit is still honoured, so
    // rage-clicking can't hammer upstream APIs.
    if (ticker_bar_ && !ticker_subscribed_.isEmpty()) {
        QStringList topics;
        topics.reserve(ticker_subscribed_.size());
        for (const QString& sym : ticker_subscribed_)
            topics.append(QStringLiteral("market:quote:") + sym);
        datahub::DataHub::instance().request(topics, /*force=*/true);
    }
    if (market_pulse_)
        market_pulse_->refresh_data();
    // Fan refresh out to every visible BaseWidget tile on the canvas.
    // Hidden tiles (collapsed, off-screen, on a non-visible workspace) are
    // skipped — refreshing them burns the producer's rate limit on data
    // the user can't see, and the visibility-driven subscribe/unsubscribe
    // (P3) means hidden widgets aren't subscribed anyway.
    if (canvas_) {
        const auto widgets = canvas_->findChildren<widgets::BaseWidget*>();
        for (auto* w : widgets) {
            if (w && w->isVisible())
                w->request_refresh();
        }
    }
}

void DashboardScreen::rebuild_ticker_from_cache() {
    if (!ticker_bar_)
        return;
    QVector<TickerBar::Entry> entries;
    entries.reserve(ticker_subscribed_.size());
    for (const auto& sym : ticker_subscribed_) {
        if (!ticker_cache_.contains(sym))
            continue;
        const auto& q = ticker_cache_.value(sym);
        // TickerBar renders this value with a trailing "%", so it must be the
        // percent change — q.change is the absolute price move (e.g. +2.35 on a
        // 190 stock showed as "+2.35%" instead of "+1.25%").
        entries.append({q.symbol, q.price, q.change_pct});
    }
    if (!entries.isEmpty())
        ticker_bar_->set_data(entries);
}

void DashboardScreen::hub_resubscribe_ticker(bool force_fetch) {
    if (!ticker_bar_)
        return;

    auto& hub = datahub::DataHub::instance();
    // Drop any prior subscriptions owned by this screen, since the symbol
    // set may have changed (user edited ticker symbols).
    hub.unsubscribe(this);
    hub_active_ = false;
    ticker_cache_.clear();
    dash_ticker_clear_status(ticker_bar_);

    ticker_subscribed_ = ticker_bar_->symbols();
    if (ticker_subscribed_.isEmpty()) {
        ticker_bar_->set_data({}); // clear stale entries so nothing misleading scrolls
        return;
    }

    QStringList topics;
    topics.reserve(ticker_subscribed_.size());
    for (const QString& sym : ticker_subscribed_) {
        const QString topic = services::MarketDataService::quote_topic(sym);
        topics.append(topic);
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            ticker_cache_.insert(sym, v.value<services::QuoteData>());
            dash_ticker_clear_status(ticker_bar_);
            rebuild_ticker_from_cache();
        });
        // Error channel: same owner, so the unsubscribe(this) calls above and in
        // hub_unsubscribe_ticker() tear it down with the value subscription. The
        // reason is shown only while the strip has nothing to scroll; the next
        // value clears it.
        hub.subscribe_errors(this, topic, [this](const QString& error) {
            if (ticker_cache_.isEmpty())
                dash_ticker_show_status(ticker_bar_, tr("Ticker quotes unavailable — %1").arg(error), error);
        });
    }
    // Force only after a user edit of the symbol list. On a plain tab show the
    // subscribe() above already delivers fresh cached quotes and cold-starts a
    // fetch for any stale/missing topic, so a forced request here just re-hit
    // the upstream API on every tab flip.
    hub.request(topics, force_fetch);
    hub_active_ = true;
}

void DashboardScreen::hub_unsubscribe_ticker() {
    if (!hub_active_)
        return;
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

// ── Event filter: sync canvas width to scroll viewport ────────────────────────

bool DashboardScreen::eventFilter(QObject* obj, QEvent* event) {
    if (ticker_bar_ && obj == ticker_bar_ && event->type() == QEvent::Resize) {
        if (auto* lbl = dash_ticker_status_label(ticker_bar_, false))
            lbl->setGeometry(ticker_bar_->rect());
    }
    // Narrow dashboards: drop the strip's sentence rather than clip it
    // mid-word; the heading and SEE ENTERPRISE stay. Height never changes.
    if (ent_strip_ && obj == ent_strip_ && event->type() == QEvent::Resize && ent_strip_text_) {
        const int needed = ent_strip_text_->sizeHint().width() + 360;
        ent_strip_text_->setVisible(ent_strip_->width() >= needed);
    }
    if (obj == scroll_area_->viewport() && event->type() == QEvent::Resize) {
        if (canvas_empty_)
            canvas_empty_->setGeometry(scroll_area_->viewport()->rect());
        int vp_w = scroll_area_->viewport()->width();
        // Only update if width actually changed — avoids triggering
        // resizeEvent → rebuild_grid_cache → reflow_tiles on every scroll tick.
        // Use setMaximumWidth + resize instead of setFixedWidth so the canvas
        // never imposes a minimum-width constraint that fights ADS splitters
        // when two screens are placed side-by-side.
        if (vp_w > 0 && vp_w != canvas_->width()) {
            canvas_->setMinimumWidth(0);
            canvas_->setMaximumWidth(QWIDGETSIZE_MAX);
            canvas_->resize(vp_w, canvas_->height());
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Enterprise strip ──────────────────────────────────────────────────────────

QWidget* DashboardScreen::build_enterprise_strip() {
    auto* strip = new QFrame(this);
    strip->setObjectName("dashEntStrip");
    strip->setMinimumHeight(30); // one line; taller only when a large font needs it
    auto* hl = new QHBoxLayout(strip);
    hl->setContentsMargins(12, 0, 6, 0);
    hl->setSpacing(10);

    auto* head = new QLabel(tr("FINCEPT TERMINAL ENTERPRISE"), strip);
    head->setObjectName("dashEntStripHead");
    hl->addWidget(head);

    ent_strip_text_ = new QLabel(tr("— the private edition for funds and research desks: private datasets, "
                                    "multi-agent research, SSO and SLA."),
                                 strip);
    ent_strip_text_->setObjectName("dashEntStripText");
    // Size hint ignored: the sentence must never impose a minimum width on the
    // dashboard panel (ADS splits). eventFilter() hides it when it won't fit.
    ent_strip_text_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    hl->addWidget(ent_strip_text_, 1);

    auto* cta = new QPushButton(tr("SEE ENTERPRISE ↗"), strip);
    cta->setObjectName("dashEntStripCta");
    cta->setCursor(Qt::PointingHandCursor);
    cta->setToolTip(ui::enterprise::site_url());
    connect(cta, &QPushButton::clicked, this, []() {
        ui::enterprise::open_url(
            ui::enterprise::url_with_ref(ui::enterprise::site_url(), QStringLiteral("dashboard")));
    });
    hl->addWidget(cta);

    auto* close = new QPushButton(QString::fromUtf8("\xc3\x97"), strip); // U+00D7 multiplication sign
    close->setObjectName("dashEntStripClose");
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Hide until the next release"));
    close->setAccessibleName(tr("Hide the Enterprise strip"));
    connect(close, &QPushButton::clicked, this, [this]() {
        ui::enterprise::dismiss_placement(QStringLiteral("dashboard"));
        if (ent_strip_) {
            ent_strip_->hide();
            ent_strip_->deleteLater();
        }
        ent_strip_ = nullptr;
        ent_strip_text_ = nullptr;
    });
    hl->addWidget(close);

    strip->installEventFilter(this);
    return strip;
}

// ── Save / Restore layout via SettingsRepository ──────────────────────────────

namespace {
// Magic + version prefix for the serialized dashboard layout blob. Distinguishes
// the current config-carrying format from the legacy one (which packed items
// with no per-item config). `cols` (a small int) never collides with the magic,
// so legacy blobs are still decoded (with empty per-item config).
constexpr quint32 kDashLayoutMagic = 0xDA58A101u;
// v2 appends the template id and the "pristine" flag after the items. A v1
// reader stops after the items, so older builds still load a v2 blob.
constexpr quint32 kDashLayoutVersion = 2;

/// Dynamic-property guard: set when restore_layout() could not *read* the
/// stored blob (as opposed to reading a genuinely absent/corrupt one). While
/// it is set, save_layout() refuses to write, because the layout on screen is
/// the built-in default rather than anything the user arranged — persisting it
/// would replace their real dashboard. Cleared by the next successful load.
constexpr const char* kDashLayoutLoadFailedProp = "fincept_dash_layout_load_failed";
/// The read error behind kDashLayoutLoadFailedProp, for the skipped-save log line.
constexpr const char* kDashLayoutLoadErrorProp = "fincept_dash_layout_load_error";
/// SettingsRepository key of the current dashboard layout.
constexpr const char* kDashLayoutKey = "dashboard_canvas_layout";
} // namespace

void DashboardScreen::save_layout() {
    save_timer_->stop();

    if (property(kDashLayoutLoadFailedProp).toBool()) {
        LOG_WARN("Dashboard", logf("Skipping layout save — the stored layout could not be read this session, so the "
                                   "on-screen default must not overwrite it",
                                   {{"key", kDashLayoutKey},
                                    {"error", property(kDashLayoutLoadErrorProp).toString()}}));
        return;
    }

    GridLayout layout = canvas_->current_layout();

    // Serialize: magic+version, cols, row_h, margin, item count, then each item
    // (including its per-instance config — dropping it reverted every configured
    // widget to defaults on restart).
    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream << kDashLayoutMagic << kDashLayoutVersion;
    stream << layout.cols << layout.row_h << layout.margin;
    stream << static_cast<int>(layout.items.size());
    for (const auto& item : layout.items) {
        stream << item.id << item.instance_id;
        stream << item.cell.x << item.cell.y << item.cell.w << item.cell.h;
        stream << item.cell.min_w << item.cell.min_h;
        stream << QJsonDocument(item.config).toJson(QJsonDocument::Compact);
    }
    stream << layout.template_id << layout.pristine;

    QString encoded = buf.toBase64();
    (void)QtConcurrent::run(
        [encoded]() { fincept::SettingsRepository::instance().set("dashboard_canvas_layout", encoded, "dashboard"); });
}

void DashboardScreen::restore_layout() {
    QPointer<DashboardScreen> self = this;
    (void)QtConcurrent::run([self]() {
        auto result = fincept::SettingsRepository::instance().get("dashboard_canvas_layout");

        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;

                if (result.is_err()) {
                    LOG_ERROR("Dashboard",
                              QString("settings read failed for 'dashboard_canvas_layout' — rendering the default "
                                      "layout read-only, leaving the stored layout untouched: %1")
                                  .arg(QString::fromStdString(result.error())));
                    // Degrade read-only: the user still gets a usable dashboard,
                    // but the layout_changed → save_timer_ → set() chain that
                    // apply_template() is about to trigger is disarmed.
                    self->setProperty(kDashLayoutLoadFailedProp, true);
                    self->setProperty(kDashLayoutLoadErrorProp, QString::fromStdString(result.error()));
                    self->build_default_layout();

                    // Say so. Suppressing the write stops the saved layout being
                    // destroyed, but on its own it just swaps one silent failure
                    // for another: the user sees a default dashboard, rearranges
                    // it, and none of it persists — with only a LOG_WARN to
                    // explain why. Reuse the toast this screen already owns.
                    if (self->notif_toast_) {
                        fincept::notifications::NotificationRecord rec;
                        rec.request.title = tr("Dashboard layout unavailable");
                        rec.request.message = tr("Your saved layout could not be read, so a default is shown. "
                                                 "Changes will not be saved this session — restart to retry.");
                        rec.request.level = fincept::notifications::NotifLevel::Warning;
                        self->notif_toast_->show_notification(rec);
                    }
                    return;
                }

                // The read succeeded, so anything below this point is a genuine
                // "unset or corrupt" case where writing the default back is the
                // correct behaviour.
                self->setProperty(kDashLayoutLoadFailedProp, false);

                if (result.value().isEmpty()) {
                    self->build_default_layout();
                    return;
                }

                QByteArray payload = QByteArray::fromBase64(result.value().toUtf8());
                QDataStream stream(&payload, QIODevice::ReadOnly);

                GridLayout layout;
                int count = 0;
                // Detect the format: a leading magic means the config-carrying
                // blob; otherwise the 4 bytes we read were the legacy `cols`.
                quint32 lead = 0;
                stream >> lead;
                const bool has_config = (lead == kDashLayoutMagic);
                quint32 version = 0;
                if (has_config) {
                    stream >> version;
                    stream >> layout.cols >> layout.row_h >> layout.margin >> count;
                } else {
                    layout.cols = static_cast<int>(lead);
                    stream >> layout.row_h >> layout.margin >> count;
                }

                if (count <= 0 || count > 100) {
                    self->build_default_layout();
                    return;
                }

                // Clamp the grid geometry before anything downstream uses it.
                // A truncated / corrupted blob previously produced items with
                // cols == 0 or w == 0, which grid_to_rect() turns into
                // zero-width tiles and compact_vertical() spins over.
                layout.cols = qBound(1, layout.cols, 48);
                layout.row_h = qBound(10, layout.row_h, 400);
                layout.margin = qBound(0, layout.margin, 64);

                for (int i = 0; i < count; ++i) {
                    GridItem item;
                    stream >> item.id >> item.instance_id;
                    stream >> item.cell.x >> item.cell.y >> item.cell.w >> item.cell.h;
                    stream >> item.cell.min_w >> item.cell.min_h;
                    if (has_config) {
                        QByteArray cfg_bytes;
                        stream >> cfg_bytes;
                        const auto doc = QJsonDocument::fromJson(cfg_bytes);
                        if (doc.isObject())
                            item.config = doc.object();
                    }
                    // Stop at the first short read rather than appending
                    // default-constructed garbage for the remaining items.
                    if (stream.status() != QDataStream::Ok)
                        break;
                    if (item.id.isEmpty() || item.instance_id.isEmpty())
                        continue;
                    item.cell.min_w = qBound(1, item.cell.min_w, layout.cols);
                    item.cell.min_h = qBound(1, item.cell.min_h, 64);
                    item.cell.w = qBound(item.cell.min_w, item.cell.w, layout.cols);
                    item.cell.h = qBound(item.cell.min_h, item.cell.h, 64);
                    item.cell.x = qBound(0, item.cell.x, layout.cols - item.cell.w);
                    item.cell.y = qBound(0, item.cell.y, 999);
                    layout.items.append(item);
                }

                if (layout.items.isEmpty()) {
                    self->build_default_layout();
                    return;
                }
                // v2: which template the layout came from, and whether it is
                // still exactly that (or a saved named layout).
                if (version >= 2 && stream.status() == QDataStream::Ok) {
                    QString tid;
                    bool pristine = false;
                    stream >> tid >> pristine;
                    if (stream.status() == QDataStream::Ok) {
                        layout.template_id = tid.left(64);
                        layout.pristine = pristine;
                    }
                }

                // Tiles stream in one per event-loop turn (visible ones first)
                // instead of blocking the UI thread for the whole dashboard.
                self->canvas_->load_layout(layout, DashboardCanvas::TileBuild::Progressive);
                self->sync_compact_button();
            },
            Qt::QueuedConnection);
    });
}

// ── Named layouts, export / import ────────────────────────────────────────────

namespace {
constexpr const char* kDashNamedLayoutsKey = "dashboard.named_layouts";
constexpr const char* kDashLayoutFileFormat = "fincept.dashboard.layout";

QJsonObject dash_layout_to_json(const GridLayout& l) {
    QJsonArray items;
    for (const auto& it : l.items)
        items.append(QJsonObject{{"id", it.id},
                                 {"instance_id", it.instance_id},
                                 {"x", it.cell.x},
                                 {"y", it.cell.y},
                                 {"w", it.cell.w},
                                 {"h", it.cell.h},
                                 {"min_w", it.cell.min_w},
                                 {"min_h", it.cell.min_h},
                                 {"config", it.config}});
    QJsonObject o{{"cols", l.cols}, {"row_h", l.row_h}, {"margin", l.margin}, {"items", items}};
    if (!l.template_id.isEmpty())
        o.insert("template", l.template_id);
    return o;
}

/// Parse + clamp exactly like restore_layout(); unknown widget ids are dropped
/// by the canvas. `fresh_ids`: give every tile a new instance id (imports).
bool dash_layout_from_json(const QJsonObject& o, bool fresh_ids, GridLayout* out) {
    GridLayout l;
    l.cols = qBound(1, o.value("cols").toInt(12), 48);
    l.row_h = qBound(10, o.value("row_h").toInt(60), 400);
    l.margin = qBound(0, o.value("margin").toInt(4), 64);
    l.template_id = o.value("template").toString().left(64);
    for (const auto& v : o.value("items").toArray()) {
        const QJsonObject j = v.toObject();
        GridItem it;
        it.id = j.value("id").toString();
        it.instance_id = fresh_ids ? QUuid::createUuid().toString(QUuid::WithoutBraces) : j.value("instance_id").toString();
        if (it.id.isEmpty() || it.instance_id.isEmpty())
            continue;
        it.cell.min_w = qBound(1, j.value("min_w").toInt(2), l.cols);
        it.cell.min_h = qBound(1, j.value("min_h").toInt(3), 64);
        it.cell.w = qBound(it.cell.min_w, j.value("w").toInt(4), l.cols);
        it.cell.h = qBound(it.cell.min_h, j.value("h").toInt(4), 64);
        it.cell.x = qBound(0, j.value("x").toInt(0), l.cols - it.cell.w);
        it.cell.y = qBound(0, j.value("y").toInt(0), 999);
        it.config = j.value("config").toObject();
        l.items.append(it);
        if (l.items.size() >= 100)
            break;
    }
    if (l.items.isEmpty())
        return false;
    *out = l;
    return true;
}

QJsonObject dash_read_named_layouts() {
    const auto r = fincept::SettingsRepository::instance().get(QString::fromLatin1(kDashNamedLayoutsKey));
    if (r.is_err() || r.value().isEmpty())
        return {};
    return QJsonDocument::fromJson(r.value().toUtf8()).object();
}
} // namespace

bool DashboardScreen::save_named_layout(const QString& name) {
    QJsonObject all = dash_read_named_layouts();
    QJsonObject entry = dash_layout_to_json(canvas_->current_layout());
    entry.insert("saved_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    all.insert(name, entry);
    const auto w = fincept::SettingsRepository::instance().set(
        QString::fromLatin1(kDashNamedLayoutsKey), QString::fromUtf8(QJsonDocument(all).toJson(QJsonDocument::Compact)),
        "dashboard");
    if (w.is_err()) {
        LOG_WARN("Dashboard", QString("could not save named layout: %1").arg(QString::fromStdString(w.error())));
        QMessageBox::warning(this, tr("Save failed"),
                             tr("The layout could not be saved: %1").arg(QString::fromStdString(w.error())));
        return false;
    }
    // The current layout now exists under that name: replacing it loses nothing.
    canvas_->mark_saved();
    save_timer_->start();
    return true;
}

void DashboardScreen::prompt_layout_name(const QString& suggestion, std::function<void(const QString&)> on_name) {
    // A heap dialog opened with open(): no nested event loop around the screen.
    auto* dlg = new QInputDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Save layout"));
    dlg->setLabelText(tr("Layout name:"));
    dlg->setTextValue(suggestion);
    dlg->setMinimumWidth(380); // the suggested name fits without scrolling
    connect(dlg, &QInputDialog::textValueSelected, this, [on_name = std::move(on_name)](const QString& v) {
        const QString name = v.trimmed();
        if (!name.isEmpty() && on_name)
            on_name(name.left(80));
    });
    dlg->open();
}

void DashboardScreen::confirm_replace(const QString& what, std::function<void()> replace) {
    // Nothing of the user's own would be lost: an untouched template, a layout
    // that is saved under a name, or an empty dashboard.
    if (canvas_->is_pristine() || canvas_->tile_count() == 0) {
        replace();
        return;
    }
    auto* box = new QMessageBox(QMessageBox::Question, tr("Replace your layout?"),
                                tr("Your current dashboard has changes of its own. %1 replaces it.\n\n"
                                   "Save it as a named layout first? You can switch back to it any time from "
                                   "LAYOUTS.")
                                    .arg(what),
                                QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    auto* save_btn = box->addButton(tr("Save it, then continue"), QMessageBox::AcceptRole);
    auto* replace_btn = box->addButton(tr("Replace without saving"), QMessageBox::DestructiveRole);
    replace_btn->setProperty("fincept_agent_risk", "destructive"); // discards the unsaved layout
    box->addButton(QMessageBox::Cancel);
    box->setDefaultButton(save_btn);
    connect(box, &QMessageBox::buttonClicked, this, [this, box, save_btn, replace_btn, replace](QAbstractButton* b) {
        if (b == replace_btn) {
            replace();
        } else if (b == save_btn) {
            const QString suggestion =
                tr("My layout %1").arg(QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
            prompt_layout_name(suggestion, [this, replace](const QString& name) {
                if (save_named_layout(name))
                    replace();
            });
        }
        Q_UNUSED(box);
    });
    box->open();
}

void DashboardScreen::request_template(const QString& template_id) {
    confirm_replace(tr("Applying the template"), [this, template_id]() {
        // apply_template() emits layout_changed, and the debounced save then
        // writes the template (with its template id, pristine) as the current
        // layout. Tiles are built one per event-loop turn.
        if (canvas_->apply_template(template_id, DashboardCanvas::TileBuild::Progressive))
            sync_compact_button();
    });
}

void DashboardScreen::sync_compact_button() {
    if (toolbar_ && canvas_)
        toolbar_->set_compact(canvas_->row_height() <= kGridRowCompact);
}

void DashboardScreen::show_layouts_menu(QWidget* anchor) {
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QJsonObject named = dash_read_named_layouts();

    connect(menu->addAction(tr("Save current layout as…")), &QAction::triggered, this, [this]() {
        QString suggestion = template_display_name_for(canvas_->template_id());
        prompt_layout_name(suggestion, [this](const QString& name) { save_named_layout(name); });
    });

    if (!named.isEmpty()) {
        menu->addSection(tr("Switch to"));
        for (auto it = named.constBegin(); it != named.constEnd(); ++it) {
            const QString name = it.key();
            const QJsonObject entry = it.value().toObject();
            connect(menu->addAction(name), &QAction::triggered, this, [this, entry, name]() {
                GridLayout l;
                if (!dash_layout_from_json(entry, /*fresh_ids=*/false, &l))
                    return;
                confirm_replace(tr("Switching to “%1”").arg(name), [this, l]() mutable {
                    l.pristine = true; // it is saved under its name
                    canvas_->load_layout(l, DashboardCanvas::TileBuild::Progressive);
                    sync_compact_button();
                    save_timer_->start(); // it is now the current layout
                });
            });
        }
        auto* del = menu->addMenu(tr("Delete saved layout"));
        for (auto it = named.constBegin(); it != named.constEnd(); ++it) {
            const QString name = it.key();
            connect(del->addAction(name), &QAction::triggered, this, [name]() {
                QJsonObject all = dash_read_named_layouts();
                all.remove(name);
                (void)fincept::SettingsRepository::instance().set(
                    QString::fromLatin1(kDashNamedLayoutsKey),
                    QString::fromUtf8(QJsonDocument(all).toJson(QJsonDocument::Compact)), "dashboard");
            });
            del->actions().constLast()->setProperty("fincept_agent_risk", "delete");
        }
    }

    menu->addSeparator();
    connect(menu->addAction(tr("Export current layout…")), &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, tr("Export dashboard layout"),
                                                          QStringLiteral("dashboard-layout.json"),
                                                          tr("Dashboard layout (*.json)"));
        if (path.isEmpty())
            return;
        QJsonObject doc{{"format", QString::fromLatin1(kDashLayoutFileFormat)},
                        {"version", 1},
                        {"region", core::market::MarketProfileStore::instance().current_id()},
                        {"layout", dash_layout_to_json(canvas_->current_layout())}};
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
            f.write(QJsonDocument(doc).toJson(QJsonDocument::Indented)) < 0) {
            QMessageBox::warning(this, tr("Export failed"), tr("Could not write %1: %2").arg(path, f.errorString()));
        }
    });
    connect(menu->addAction(tr("Import layout…")), &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Import dashboard layout"), QString(),
                                                          tr("Dashboard layout (*.json)"));
        if (path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.size() > 4 * 1024 * 1024) {
            QMessageBox::warning(this, tr("Import failed"), tr("Could not read %1").arg(path));
            return;
        }
        const QJsonObject doc = QJsonDocument::fromJson(f.readAll()).object();
        GridLayout l;
        if (doc.value("format").toString() != QLatin1String(kDashLayoutFileFormat) ||
            !dash_layout_from_json(doc.value("layout").toObject(), /*fresh_ids=*/true, &l)) {
            QMessageBox::warning(this, tr("Import failed"), tr("%1 is not a Fincept dashboard layout.").arg(path));
            return;
        }
        confirm_replace(tr("Importing the layout"), [this, l]() {
            canvas_->load_layout(l, DashboardCanvas::TileBuild::Progressive);
            sync_compact_button();
            save_timer_->start(); // it is now the current layout
        });
    });

    menu->popup(anchor ? anchor->mapToGlobal(QPoint(0, anchor->height())) : QCursor::pos());
}

void DashboardScreen::build_default_layout() {
    // The region-aware market overview, in compact density like every
    // template: its tiles follow the user's market region (MarketProfile,
    // inferred from the locale until the user picks one).
    canvas_->apply_template("market_overview", DashboardCanvas::TileBuild::Progressive);
    sync_compact_button();
}

bool DashboardScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus);
    const QString key = section.trimmed().toLower();
    if (key.isEmpty())
        return true;
    // Both pickers are window-modal: opened after the link has landed, as the toolbar buttons open them.
    if (key == QLatin1String("add_widget")) {
        QMetaObject::invokeMethod(this, [this]() { open_add_widget_dialog(); }, Qt::QueuedConnection);
        return true;
    }
    if (key == QLatin1String("templates")) {
        QMetaObject::invokeMethod(this, [this]() { open_template_picker(); }, Qt::QueuedConnection);
        return true;
    }
    return false;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> dash_command_rows() {
    namespace rows = command_rows;
    const QString where = DashboardScreen::tr("Dashboard");
    // "add_widget" is the seeded intent.add_widget row.
    return {
        rows::section(QStringLiteral("dashboard"), QStringLiteral("templates"), DashboardScreen::tr("Dashboard templates"),
                      {QStringLiteral("templates"), QStringLiteral("dashboard layout"), QStringLiteral("preset"),
                       QStringLiteral("reset dashboard")},
                      where),
    };
}

void dash_install_commands() {
    command_rows::install(QStringLiteral("dashboard"), &dash_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(dash_install_commands)

} // namespace fincept::screens
