#include "screens/dashboard/DashboardScreen.h"

#include "screens/dashboard/canvas/AddWidgetDialog.h"
#include "screens/dashboard/canvas/DashboardTemplates.h"
#include "screens/dashboard/canvas/TemplatePicker.h"
#include "screens/dashboard/canvas/WidgetRegistry.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QDataStream>
#include <QEvent>
#include <QHideEvent>
#include <QPointer>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace fincept::screens {

DashboardScreen::DashboardScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Top Toolbar ──
    toolbar_ = new DashboardToolBar;
    vl->addWidget(toolbar_);

    // ── Scrolling Ticker Bar ──
    ticker_bar_ = new TickerBar;
    vl->addWidget(ticker_bar_);

    // ── Main Content: Canvas (in scroll) + Market Pulse ──
    content_split_ = new QSplitter(Qt::Horizontal);
    content_split_->setHandleWidth(1);
    content_split_->setStyleSheet("QSplitter::handle { background: #1a1a1a; }"
                                  "QSplitter::handle:hover { background: #333333; }");

    canvas_ = new DashboardCanvas;

    // Scroll area wraps canvas — provides vertical scroll when content exceeds viewport
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(false); // canvas controls its own size
    scroll_area_->setWidget(canvas_);
    scroll_area_->setStyleSheet(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #222; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    // Sync canvas width to scroll viewport via event filter
    scroll_area_->viewport()->installEventFilter(this);

    market_pulse_ = new MarketPulsePanel;

    content_split_->addWidget(scroll_area_);
    content_split_->addWidget(market_pulse_);
    content_split_->setStretchFactor(0, 3);
    content_split_->setStretchFactor(1, 1);
    content_split_->setSizes({900, 280});

    vl->addWidget(content_split_, 1);

    // ── Bottom Status Bar ──
    status_bar_ = new DashboardStatusBar;
    vl->addWidget(status_bar_);

    // ── Debounced save timer (P3: not started in constructor) ──
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(800);
    connect(save_timer_, &QTimer::timeout, this, &DashboardScreen::save_layout);

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
}

// ── Show/Hide ─────────────────────────────────────────────────────────────────

void DashboardScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (ticker_bar_)
        ticker_bar_->resume();

    if (!layout_restored_) {
        layout_restored_ = true;
        restore_layout();
    }
}

void DashboardScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (ticker_bar_)
        ticker_bar_->pause();
}

// ── Event filter: sync canvas width to scroll viewport ────────────────────────

bool DashboardScreen::eventFilter(QObject* obj, QEvent* event) {
    if (obj == scroll_area_->viewport() && event->type() == QEvent::Resize) {
        int vp_w = scroll_area_->viewport()->width();
        // Only update if width actually changed — avoids triggering
        // resizeEvent → rebuild_grid_cache → reflow_tiles on every scroll tick
        if (vp_w > 0 && vp_w != canvas_->width())
            canvas_->setFixedWidth(vp_w);
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
