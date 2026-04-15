#include "screens/dashboard/DashboardScreen.h"

#include "screens/dashboard/canvas/AddWidgetDialog.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/TemplatePicker.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "services/markets/MarketDataService.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/NotifToast.h"

#include <QDataStream>
#include <QEvent>
#include <QHideEvent>
#include <QPalette>
#include <QPointer>
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

} // namespace

namespace fincept::screens {

DashboardScreen::DashboardScreen(QWidget* parent) : QWidget(parent) {
    apply_solid_background(this, QColor(ui::colors::BG_BASE()));
    refresh_theme();

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Top Toolbar ──
    toolbar_ = new DashboardToolBar;
    vl->addWidget(toolbar_);

    // ── Scrolling Ticker Bar ──
    ticker_bar_ = new TickerBar;
    vl->addWidget(ticker_bar_);
    // When user saves a new symbol list, re-fetch quotes immediately.
    connect(ticker_bar_, &TickerBar::symbols_changed, this, &DashboardScreen::refresh_ticker);

    // ── Main Content: Canvas (in scroll) + Market Pulse ──
    content_split_ = new QSplitter(Qt::Horizontal);
    content_split_->setHandleWidth(1);
    content_split_->setStyleSheet(QString("QSplitter::handle{background:%1;}QSplitter::handle:hover{background:%2;}")
                                      .arg(ui::colors::BORDER_DIM(), ui::colors::BORDER_MED()));

    canvas_ = new DashboardCanvas;

    // Scroll area wraps canvas — provides vertical scroll when content exceeds viewport
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(false); // canvas controls its own size
    scroll_area_->setMinimumWidth(0);        // allow ADS to shrink this panel freely
    scroll_area_->setWidget(canvas_);
    scroll_area_->setStyleSheet(QString("QScrollArea{border:none;background:%1;}"
                                        "QScrollBar:vertical{width:6px;background:transparent;}"
                                        "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                                        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                                    .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
    scroll_area_->viewport()->setStyleSheet(
        QString("background:%1;").arg(ui::colors::BG_BASE()));

    // Sync canvas width to scroll viewport via event filter
    scroll_area_->viewport()->installEventFilter(this);

    market_pulse_ = new MarketPulsePanel;

    content_split_->addWidget(scroll_area_);
    content_split_->addWidget(market_pulse_);
    content_split_->setStretchFactor(0, 3);
    content_split_->setStretchFactor(1, 1);
    content_split_->setCollapsible(0, false);
    content_split_->setCollapsible(1, false);
    market_pulse_->setMinimumWidth(180);

    vl->addWidget(content_split_, 1);

    // ── Bottom Status Bar ──
    status_bar_ = new DashboardStatusBar;
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

    // ── Ticker refresh timer: 5-minute interval, started in showEvent (P3) ──
    ticker_refresh_timer_ = new QTimer(this);
    ticker_refresh_timer_->setInterval(5 * 60 * 1000);
    connect(ticker_refresh_timer_, &QTimer::timeout, this, &DashboardScreen::refresh_ticker);

    // ── Canvas signals → toolbar/statusbar ──
    connect(canvas_, &DashboardCanvas::widget_count_changed, toolbar_, &DashboardToolBar::set_widget_count);
    connect(canvas_, &DashboardCanvas::widget_count_changed, status_bar_, &DashboardStatusBar::set_widget_count);
    connect(canvas_, &DashboardCanvas::layout_changed, this, [this](const GridLayout&) { save_timer_->start(); });

    // ── Toolbar buttons ──
    connect(toolbar_, &DashboardToolBar::toggle_pulse_clicked, this, [this]() {
        pulse_visible_ = !pulse_visible_;
        market_pulse_->setVisible(pulse_visible_);
    });

    connect(toolbar_, &DashboardToolBar::add_widget_clicked, this, [this]() {
        auto* dlg = new AddWidgetDialog(this);
        connect(dlg, &AddWidgetDialog::widget_selected, canvas_, &DashboardCanvas::add_widget);
        dlg->exec();
        dlg->deleteLater();
    });

    connect(toolbar_, &DashboardToolBar::reset_layout_clicked, this, [this]() {
        auto* dlg = new TemplatePicker(this);
        connect(dlg, &TemplatePicker::template_selected, this, [this](const QString& tid) {
            // Clear saved state so fresh template is used
            QtConcurrent::run([]() { fincept::SettingsRepository::instance().remove("dashboard_canvas_layout"); });
            canvas_->apply_template(tid);
        });
        dlg->exec();
        dlg->deleteLater();
    });

    connect(toolbar_, &DashboardToolBar::save_layout_clicked, this, &DashboardScreen::save_layout);

    connect(toolbar_, &DashboardToolBar::toggle_compact_clicked, this, [this]() {
        static bool compact = false;
        compact = !compact;
        canvas_->set_row_height(compact ? 40 : 60);
    });

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void DashboardScreen::refresh_theme() {
    const QColor bg(ui::colors::BG_BASE());
    apply_solid_background(this, bg);

    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (content_split_) {
        apply_solid_background(content_split_, bg);
        content_split_->setStyleSheet(
            QString("QSplitter{background:%1;}QSplitter::handle{background:%2;}QSplitter::handle:hover{background:%3;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM(), ui::colors::BORDER_MED()));
    }
    if (scroll_area_) {
        apply_solid_background(scroll_area_, bg);
        scroll_area_->setStyleSheet(
            QString("QScrollArea{border:none;background:%1;}"
                    "QScrollBar:vertical{width:6px;background:transparent;}"
                    "QScrollBar::handle:vertical{background:%2;border-radius:3px;min-height:20px;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                .arg(ui::colors::BG_BASE(), ui::colors::BORDER_MED()));
        // Ensure viewport doesn't paint its own opaque background over the canvas
        apply_solid_background(scroll_area_->viewport(), bg);
        scroll_area_->viewport()->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    }
}

// ── Show/Hide ─────────────────────────────────────────────────────────────────

void DashboardScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh_theme();

    if (ticker_bar_)
        ticker_bar_->resume();

    // Fetch live data for the ticker on first show, then every 5 minutes.
    refresh_ticker();
    if (ticker_refresh_timer_)
        ticker_refresh_timer_->start();

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
    if (ticker_refresh_timer_)
        ticker_refresh_timer_->stop();
}

// ── Ticker refresh ────────────────────────────────────────────────────────────

void DashboardScreen::refresh_ticker() {
    if (!ticker_bar_)
        return;

    const QStringList symbols = ticker_bar_->symbols();
    if (symbols.isEmpty())
        return;

    QPointer<DashboardScreen> self = this;
    services::MarketDataService::instance().fetch_quotes(symbols,
        [self](bool ok, QVector<services::QuoteData> quotes) {
            if (!self || !ok || quotes.isEmpty())
                return;
            QVector<TickerBar::Entry> entries;
            entries.reserve(quotes.size());
            for (const auto& q : quotes)
                entries.append({q.symbol, q.price, q.change});
            self->ticker_bar_->set_data(entries);
        });
}

// ── Event filter: sync canvas width to scroll viewport ────────────────────────

bool DashboardScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == scroll_area_->viewport() && event->type() == QEvent::Resize) {
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

// ── Save / Restore layout via SettingsRepository ──────────────────────────────

void DashboardScreen::save_layout() {
    save_timer_->stop();

    GridLayout layout = canvas_->current_layout();

    // Serialize: cols, row_h, margin, item count, then each item
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << layout.cols << layout.row_h << layout.margin;
    stream << static_cast<int>(layout.items.size());
    for (const auto& item : layout.items) {
        stream << item.id << item.instance_id;
        stream << item.cell.x << item.cell.y << item.cell.w << item.cell.h;
        stream << item.cell.min_w << item.cell.min_h;
    }

    QString encoded = data.toBase64();
    QPointer<DashboardScreen> self = this;
    QtConcurrent::run(
        [encoded]() { fincept::SettingsRepository::instance().set("dashboard_canvas_layout", encoded, "dashboard"); });
}

void DashboardScreen::restore_layout() {
    QPointer<DashboardScreen> self = this;
    QtConcurrent::run([self]() {
        auto result = fincept::SettingsRepository::instance().get("dashboard_canvas_layout");

        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;

                if (result.is_err() || result.value().isEmpty()) {
                    self->build_default_layout();
                    return;
                }

                QByteArray data = QByteArray::fromBase64(result.value().toUtf8());
                QDataStream stream(&data, QIODevice::ReadOnly);

                GridLayout layout;
                int count = 0;
                stream >> layout.cols >> layout.row_h >> layout.margin >> count;

                if (count <= 0 || count > 100) {
                    self->build_default_layout();
                    return;
                }

                for (int i = 0; i < count; ++i) {
                    GridItem item;
                    stream >> item.id >> item.instance_id;
                    stream >> item.cell.x >> item.cell.y >> item.cell.w >> item.cell.h;
                    stream >> item.cell.min_w >> item.cell.min_h;
                    layout.items.append(item);
                }

                self->canvas_->load_layout(layout);
            },
            Qt::QueuedConnection);
    });
}

void DashboardScreen::build_default_layout() {
    canvas_->apply_template("portfolio_manager");
}

} // namespace fincept::screens
