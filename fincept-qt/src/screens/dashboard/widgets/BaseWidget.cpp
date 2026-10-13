#include "screens/dashboard/widgets/BaseWidget.h"

#include "core/events/EventBus.h"
#include "datahub/DataHub.h"
#include "screens/dashboard/widgets/LoadingOverlay.h"
#include "services/backtesting/BacktestingService.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QJsonArray>
#include <QMenu>
#include <QPalette>
#include <QSize>
#include <QStyle>
#include <QTableWidget>
#include <QToolTip>

namespace fincept::screens::widgets {

namespace {

// Dynamic-property name used by link_symbol() to tag a clickable row/cell.
constexpr const char* kBaseWidgetSymbolProp = "fincept_link_symbol";
// ...and by link_screen() to tag a row that opens a whole screen.
constexpr const char* kBaseWidgetScreenProp = "fincept_link_screen";
// ...and by set_css_cached() to remember the last stylesheet it applied.
constexpr const char* kBaseWidgetCssProp = "fincept_cached_css";

// Index levels (^GSPC), FX/futures (EURUSD=X, GC=F) and Yahoo crypto pairs
// (BTC-USD) are not broker-tradable equities — same rule as the Watchlist menu.
bool base_widget_is_tradable_equity(const QString& symbol) {
    return !symbol.startsWith(QLatin1Char('^')) && !symbol.contains(QLatin1Char('=')) &&
           !symbol.endsWith(QLatin1String("-USD"));
}

// One place for the three title-bar button stylesheets. The constructor used to
// style config/refresh/close inline and refresh_base_theme() only restyled the
// refresh button, so after a theme switch the gear and close buttons kept the
// previous theme's colours.
void apply_title_button_styles(QPushButton* config_btn, QPushButton* refresh_btn, QPushButton* close_btn,
                               const QString& accent) {
    const auto css = [](const QString& fg, const QString& hover) {
        return QString("QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 2px; "
                       "padding: 0px; }"
                       "QPushButton:hover { color: %4; border-color: %4; background: %5; }")
            .arg(fg, ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), hover, ui::colors::BG_HOVER());
    };
    if (config_btn)
        config_btn->setStyleSheet(css(accent, ui::colors::TEXT_PRIMARY()));
    if (refresh_btn)
        refresh_btn->setStyleSheet(css(accent, ui::colors::TEXT_PRIMARY()));
    if (close_btn)
        close_btn->setStyleSheet(css(ui::colors::TEXT_TERTIARY(), ui::colors::NEGATIVE()));
}

} // namespace

BaseWidget::BaseWidget(const QString& title, QWidget* parent, const QString& accent_color)
    : QFrame(parent), accent_color_(accent_color.isEmpty() ? ui::colors::AMBER : accent_color) {
    setObjectName("dashboardBaseWidget");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_SURFACE()));
    setPalette(pal);

    setStyleSheet(QString("#dashboardBaseWidget { background: %1; border: 1px solid %2; border-radius: 2px; }")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_BRIGHT()));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar ──
    title_bar_ = new QWidget(this);
    title_bar_->setFixedHeight(26);
    title_bar_->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(title_bar_);
    hl->setContentsMargins(8, 0, 4, 0);
    hl->setSpacing(6);

    // Accent bar
    accent_bar_ = new QLabel;
    accent_bar_->setFixedSize(2, 10);
    accent_bar_->setStyleSheet(QString("background: %1; border-radius: 1px;").arg(accent_color_));
    hl->addWidget(accent_bar_);

    title_label_ = new QLabel(title);
    title_label_->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; letter-spacing: 0.5px; "
                                        "background: transparent; text-transform: uppercase;")
                                    .arg(accent_color_));
    hl->addWidget(title_label_);

    // Loading indicator
    loading_label_ = new QLabel("...");
    loading_label_->setStyleSheet(
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::AMBER()));
    loading_label_->setVisible(false);
    hl->addWidget(loading_label_);

    hl->addStretch();

    // Config (gear) button — hidden by default until subclass calls set_configurable(true)
    config_btn_ = new QPushButton;
    config_btn_->setFixedSize(20, 20);
    config_btn_->setText("");
    config_btn_->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    config_btn_->setIconSize(QSize(12, 12));
    config_btn_->setToolTip(tr("Configure widget"));
    config_btn_->setAccessibleName(tr("Configure widget"));
    config_btn_->setCursor(Qt::PointingHandCursor);
    config_btn_->setVisible(false);
    connect(config_btn_, &QPushButton::clicked, this, &BaseWidget::on_config_clicked);
    hl->addWidget(config_btn_);

    // Refresh button
    refresh_btn_ = new QPushButton;
    refresh_btn_->setFixedSize(20, 20);
    refresh_btn_->setText("");
    refresh_btn_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refresh_btn_->setIconSize(QSize(12, 12));
    refresh_btn_->setToolTip(tr("Refresh widget data"));
    refresh_btn_->setAccessibleName(tr("Refresh widget data"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, &BaseWidget::refresh_requested);
    hl->addWidget(refresh_btn_);

    // Close button
    auto* close_btn = new QPushButton;
    close_btn_ = close_btn;
    close_btn->setFixedSize(20, 20);
    close_btn->setText("");
    close_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    close_btn->setIconSize(QSize(11, 11));
    close_btn->setToolTip(tr("Close widget"));
    close_btn->setAccessibleName(tr("Close widget"));
    close_btn->setCursor(Qt::PointingHandCursor);
    connect(close_btn, &QPushButton::clicked, this, &BaseWidget::close_requested);
    hl->addWidget(close_btn);

    apply_title_button_styles(config_btn_, refresh_btn_, close_btn_, accent_color_);

    vl->addWidget(title_bar_);

    // Auto-refresh styles on theme/font change — subclasses get this for free
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        refresh_base_theme();
        on_theme_changed();
    });

    // ── Content ──
    content_ = new QWidget(this);
    content_->setStyleSheet("background: transparent;");
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    vl->addWidget(content_, 1);

    // ── Loading overlay ──
    // Lives as a child of the content area so it covers exactly the data
    // region (not the title bar). Sized via an event filter on `content_`.
    loading_overlay_ = new LoadingOverlay(content_);
    loading_overlay_->attach_to(content_);

    // ── Loading watchdog ──
    // Single-shot guard against producers that never publish. If the
    // overlay is still active 20 s after loading started with zero items
    // delivered, swap to a terminal "no data" overlay so the spinner can't
    // spin forever. Per-widget refresh still works to re-arm.
    //
    // 20 s (not 12 s) because dashboard restore fans out 20+ subscribe()
    // calls in one tick; producer-side rate limits then serialise the
    // cold-start fetches and tail widgets routinely take >12 s to see
    // their first payload even on healthy networks.
    loading_watchdog_ = new QTimer(this);
    loading_watchdog_->setSingleShot(true);
    loading_watchdog_->setInterval(20000);
    connect(loading_watchdog_, &QTimer::timeout, this, &BaseWidget::on_watchdog_fired);
}

void BaseWidget::refresh_base_theme() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_SURFACE()));
    setPalette(pal);

    setStyleSheet(QString("#dashboardBaseWidget{background:%1;border:1px solid %2;border-radius:2px;}")
                      .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_BRIGHT()));
    if (title_bar_)
        title_bar_->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                      .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    if (title_label_)
        title_label_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:bold;letter-spacing:0.5px;"
                                            "background:transparent;text-transform:uppercase;")
                                        .arg(accent_color_));
    if (accent_bar_)
        accent_bar_->setStyleSheet(QString("background:%1;border-radius:1px;").arg(accent_color_));
    if (loading_label_)
        loading_label_->setStyleSheet(
            QString("color:%1;font-size:9px;background:transparent;").arg(ui::colors::AMBER()));
    apply_title_button_styles(config_btn_, refresh_btn_, close_btn_, accent_color_);
}

void BaseWidget::set_loading(bool loading) {
    loading_label_->setVisible(loading);
    loading_label_->setText(loading ? tr("LOADING...") : QString());
    if (!loading_overlay_)
        return;
    error_shown_ = false;
    if (loading) {
        loading_overlay_->start_indeterminate();
        last_progress_loaded_ = 0;
        arm_watchdog();
    } else {
        loading_overlay_->finish();
        disarm_watchdog();
    }
}

void BaseWidget::set_loading_progress(int loaded, int expected) {
    if (!loading_overlay_)
        return;
    const bool active = (expected > 0) && (loaded < expected);
    error_shown_ = false;
    loading_label_->setVisible(active);
    loading_label_->setText(active ? tr("LOADING...") : QString());
    loading_overlay_->set_progress(loaded, expected);
    last_progress_loaded_ = loaded;
    if (active)
        arm_watchdog();
    else
        disarm_watchdog();
}

void BaseWidget::set_error(const QString& error) {
    if (error.isEmpty())
        return;

    // Error overrides any in-flight progress. The overlay draws the message
    // over the (intact) content; the next set_loading(true) — e.g. the title
    // bar refresh — switches it back to the loading state.
    if (loading_label_) {
        loading_label_->setVisible(false);
        loading_label_->setText(QString());
    }
    disarm_watchdog();
    error_shown_ = true;
    if (loading_overlay_)
        loading_overlay_->set_error(error);
}

bool BaseWidget::is_waiting_for_first_data() const {
    return loading_overlay_ && loading_overlay_->is_active() && !error_shown_ && last_progress_loaded_ == 0;
}

void BaseWidget::set_title(const QString& title) {
    title_label_->setText(title);
    // Screen readers announce the tile by its title; without this the whole
    // dashboard reads as a wall of unnamed frames.
    setAccessibleName(title);
    title_label_->setAccessibleName(title);
}

void BaseWidget::set_configurable(bool configurable) {
    if (config_btn_)
        config_btn_->setVisible(configurable);
}

void BaseWidget::schedule_render(std::function<void()> fn) {
    pending_render_ = std::move(fn);
    if (render_scheduled_)
        return;
    render_scheduled_ = true;
    // `this` as context object: Qt drops the pending call if the widget dies.
    QTimer::singleShot(0, this, [this]() {
        render_scheduled_ = false;
        // Move out first so a render that itself schedules another one
        // doesn't get clobbered.
        auto fn = std::move(pending_render_);
        pending_render_ = nullptr;
        if (fn)
            fn();
    });
}

void BaseWidget::on_config_clicked() {
    QDialog* dlg = make_config_dialog(this);
    if (!dlg)
        return;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // Subclasses emit config_changed() inside the dialog's accept flow, so
    // nothing here waits for the result: open() is window-modal without the
    // nested event loop exec() ran on the GUI thread (P1).
    dlg->open();
}

void BaseWidget::showEvent(QShowEvent* event) {
    QFrame::showEvent(event);
    // Subclasses commonly call set_loading(true) from their constructor,
    // which arms the watchdog before the widget is mounted. During
    // dashboard layout restore that gap is small but non-zero, and any
    // widget that's constructed but not immediately shown (collapsed
    // workspace, off-screen tile) would otherwise burn its 20 s budget
    // before the subscription is even dispatched. Restart the timer so
    // the budget begins when the user can actually see the spinner.
    // hideEvent() disarms it, so a spinner that was up when the tile was hidden
    // gets a fresh budget here too. Not while an error is on show — the
    // watchdog's generic "no data yet" must not replace a specific error.
    if (loading_overlay_ && loading_overlay_->is_active() && !error_shown_)
        arm_watchdog();
}

void BaseWidget::hideEvent(QHideEvent* event) {
    QFrame::hideEvent(event);
    // See the header: drop every hub subscription this tile owns, whatever
    // the subclass did. Subclasses' own unsubscribe calls become no-ops.
    datahub::DataHub::instance().unsubscribe(this);
    // A watchdog armed for an on-screen spinner must not fire for a tile the
    // user cannot see; showEvent() re-arms it if the spinner is still up.
    disarm_watchdog();
}

void BaseWidget::set_css_cached(QWidget* w, const QString& css) {
    if (!w || w->property(kBaseWidgetCssProp).toString() == css)
        return;
    w->setProperty(kBaseWidgetCssProp, css);
    w->setStyleSheet(css);
}

void BaseWidget::reset_css_cache(QWidget* w) {
    if (w)
        w->setProperty(kBaseWidgetCssProp, QVariant());
}

void BaseWidget::arm_watchdog() {
    if (!loading_watchdog_)
        return;
    loading_watchdog_->start();
}

void BaseWidget::disarm_watchdog() {
    if (!loading_watchdog_)
        return;
    loading_watchdog_->stop();
}

void BaseWidget::on_watchdog_fired() {
    if (!loading_overlay_ || !loading_overlay_->is_active())
        return;
    if (loading_label_) {
        loading_label_->setVisible(false);
        loading_label_->setText(QString());
    }
    // If some data trickled in before the timeout, just hide the spinner so
    // the partial content becomes visible. Otherwise surface a soft error so
    // the user knows the widget hasn't simply hung.
    if (last_progress_loaded_ > 0) {
        loading_overlay_->finish();
    } else {
        loading_overlay_->set_error(tr("No data yet — click refresh to retry"));
    }
}

void BaseWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QFrame::changeEvent(event);
}

void BaseWidget::retranslateUi() {
    if (config_btn_) {
        config_btn_->setToolTip(tr("Configure widget"));
        config_btn_->setAccessibleName(tr("Configure widget"));
    }
    if (refresh_btn_) {
        refresh_btn_->setToolTip(tr("Refresh widget data"));
        refresh_btn_->setAccessibleName(tr("Refresh widget data"));
    }
    if (close_btn_) {
        close_btn_->setToolTip(tr("Close widget"));
        close_btn_->setAccessibleName(tr("Close widget"));
    }
    if (loading_label_ && loading_label_->isVisible())
        loading_label_->setText(tr("LOADING..."));
}

void BaseWidget::open_symbol(const QString& symbol, const QString& screen_id, const QString& asset_class,
                             const QString& exchange) {
    const QString sym = symbol.trimmed();
    if (sym.isEmpty() || screen_id.isEmpty())
        return;
    QVariantMap payload{{"screen_id", screen_id}, {"symbol", sym}, {"asset_class", asset_class}};
    if (!exchange.isEmpty())
        payload.insert("exchange", exchange);
    EventBus::instance().publish("nav.open_symbol", payload);
}

void BaseWidget::link_symbol(QWidget* w, const QString& symbol) {
    if (!w)
        return;
    // Recycled rows call this on every refresh — skip the no-op re-link.
    if (w->property(kBaseWidgetSymbolProp).toString() == symbol)
        return;
    w->setProperty(kBaseWidgetSymbolProp, symbol);
    w->setCursor(Qt::PointingHandCursor);
    w->setToolTip(tr("Double-click to open %1 in Equity Research · right-click for more").arg(symbol));
    // installEventFilter() on an already-installed filter just moves it to the
    // front of the list, so re-linking a recycled row never stacks filters.
    w->installEventFilter(this);
}

void BaseWidget::show_symbol_menu(const QString& symbol_in, const QPoint& global_pos, const QString& asset_class,
                                  const QString& exchange) {
    const QString symbol = symbol_in.trimmed();
    if (symbol.isEmpty())
        return;
    const bool crypto = asset_class == QLatin1String("crypto");

    // Heap + popup(), not a stack QMenu + exec(): exec() spins a nested event
    // loop, and a tile destroyed while its menu is open (layout reload, MCP
    // remove) would then delete a stack object through its parent.
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Open in Equity Research")), &QAction::triggered, this,
            [this, symbol]() { open_symbol(symbol, QStringLiteral("equity_research")); });
    if (crypto) {
        connect(menu->addAction(tr("Open in Crypto Trading")), &QAction::triggered, this,
                [this, symbol, exchange]() {
                    open_symbol(symbol, QStringLiteral("crypto_trading"), QStringLiteral("crypto"), exchange);
                });
    } else if (base_widget_is_tradable_equity(symbol)) {
        connect(menu->addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
                [this, symbol, asset_class, exchange]() {
                    open_symbol(symbol, QStringLiteral("equity_trading"), asset_class, exchange);
                });
    }
    connect(menu->addAction(tr("Open in News")), &QAction::triggered, this,
            [this, symbol]() { open_symbol(symbol, QStringLiteral("news")); });
    if (!crypto) {
        connect(menu->addAction(tr("Backtest This Symbol")), &QAction::triggered, this, [symbol]() {
            QJsonObject config;
            config["symbols"] = QJsonArray{symbol};
            services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", QStringLiteral("backtesting")}});
        });
    }

    // One entry per saved watchlist (same flow as the Screener's row menu).
    const auto lists = fincept::WatchlistRepository::instance().list_all();
    if (lists.is_ok() && !lists.value().isEmpty()) {
        QMenu* wl_menu = menu->addMenu(tr("Add to Watchlist"));
        for (const auto& wl : lists.value()) {
            connect(wl_menu->addAction(wl.name), &QAction::triggered, this,
                    [this, symbol, list_id = wl.id, list_name = wl.name]() {
                        auto& repo = fincept::WatchlistRepository::instance();
                        const auto stocks = repo.get_stocks(list_id);
                        if (stocks.is_ok()) {
                            for (const auto& s : stocks.value()) {
                                if (s.symbol.compare(symbol, Qt::CaseInsensitive) == 0) {
                                    QToolTip::showText(QCursor::pos(), tr("%1 is already in %2").arg(symbol, list_name),
                                                       this);
                                    return;
                                }
                            }
                        }
                        const auto r = repo.add_stock(list_id, symbol, QString());
                        QToolTip::showText(QCursor::pos(),
                                           r.is_ok() ? tr("Added %1 to %2").arg(symbol, list_name)
                                                     : tr("Could not add %1 to %2").arg(symbol, list_name),
                                           this);
                        if (r.is_ok()) // let an open Watchlist panel pick the change up
                            EventBus::instance().publish(
                                "watchlist.updated",
                                QVariantMap{{"watchlist_id", list_id}, {"action", QStringLiteral("add")},
                                            {"symbol", symbol}});
                    });
        }
    }

    menu->addSeparator();
    connect(menu->addAction(tr("Copy Symbol")), &QAction::triggered, this,
            [symbol]() { QApplication::clipboard()->setText(symbol); });
    menu->popup(global_pos);
}

void BaseWidget::install_symbol_menu(QTableWidget* table, int col, int role) {
    if (!table)
        return;
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this, [this, table, col, role](const QPoint& pos) {
        auto* hit = table->itemAt(pos);
        if (!hit)
            return;
        auto* sym_item = table->item(hit->row(), col);
        if (!sym_item)
            return;
        const QString sym = sym_item->data(role).toString();
        if (!sym.isEmpty())
            show_symbol_menu(sym, table->viewport()->mapToGlobal(pos));
    });
}

void BaseWidget::link_screen(QWidget* w, const QString& screen_id, const QString& tooltip) {
    if (!w)
        return;
    w->setProperty(kBaseWidgetScreenProp, screen_id);
    w->setCursor(Qt::PointingHandCursor);
    w->setToolTip(tooltip);
    w->installEventFilter(this); // idempotent, see link_symbol()
}

bool BaseWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::ContextMenu) {
        const QString sym = obj->property(kBaseWidgetSymbolProp).toString();
        if (!sym.isEmpty()) {
            show_symbol_menu(sym, static_cast<QContextMenuEvent*>(event)->globalPos());
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        const QVariant sym = obj->property(kBaseWidgetSymbolProp);
        if (sym.isValid() && !sym.toString().isEmpty()) {
            open_symbol(sym.toString());
            return true;
        }
        const QVariant screen = obj->property(kBaseWidgetScreenProp);
        if (screen.isValid() && !screen.toString().isEmpty()) {
            EventBus::instance().publish("nav.switch_screen", QVariantMap{{"screen_id", screen.toString()}});
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

} // namespace fincept::screens::widgets
