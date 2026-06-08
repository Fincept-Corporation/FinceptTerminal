#include "app/MonitorPickerDialog.h"

#include "ui/theme/Theme.h"

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace fincept {

namespace {

// Internal widget that paints all connected screens as a proportional map.
// Hovering highlights a monitor; clicking invokes the callback with the
// chosen QScreen*. No signals — kept Q_OBJECT-free so MOC doesn't need to
// see this anonymous-namespace type.
class MonitorMapWidget : public QWidget {
  public:
    using ClickHandler = std::function<void(QScreen*)>;

    MonitorMapWidget(QScreen* current, ClickHandler on_click, QWidget* parent)
        : QWidget(parent), current_(current), on_click_(std::move(on_click)) {
        setMouseTracking(true);
        setMinimumSize(560, 320);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        const auto screens = QGuiApplication::screens();
        if (screens.isEmpty())
            return;

        // Virtual desktop bounds — union of every screen's geometry.
        QRect bounds = screens.first()->geometry();
        for (QScreen* s : screens)
            bounds = bounds.united(s->geometry());

        const int margin = 20;
        const QRect content = rect().adjusted(margin, margin, -margin, -margin);
        if (content.width() <= 0 || content.height() <= 0 || bounds.isEmpty())
            return;

        const qreal scale = std::min(static_cast<qreal>(content.width()) / bounds.width(),
                                     static_cast<qreal>(content.height()) / bounds.height());
        const qreal off_x = content.left() + (content.width() - bounds.width() * scale) / 2.0
                            - bounds.left() * scale;
        const qreal off_y = content.top() + (content.height() - bounds.height() * scale) / 2.0
                            - bounds.top() * scale;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.fillRect(rect(), QColor(ui::colors::BG_SURFACE()));

        screen_rects_.clear();
        screen_rects_.reserve(screens.size());

        const QScreen* primary = QGuiApplication::primaryScreen();

        for (int i = 0; i < screens.size(); ++i) {
            QScreen* s = screens[i];
            const QRect g = s->geometry();
            const QRectF mapped(g.left() * scale + off_x, g.top() * scale + off_y,
                                g.width() * scale, g.height() * scale);
            const QRect mapped_int = mapped.toRect();
            screen_rects_.append(mapped_int);

            const bool is_hover = (i == hover_idx_);
            const bool is_current = (s == current_);
            const bool is_primary = (s == primary);

            QColor fill = QColor(ui::colors::BG_BASE());
            QColor border = QColor(ui::colors::BORDER_DIM());
            int border_w = 1;
            if (is_hover) {
                fill = QColor(ui::colors::AMBER_DIM());
                border = QColor(ui::colors::AMBER());
                border_w = 2;
            } else if (is_current) {
                border = QColor(ui::colors::AMBER());
                border_w = 2;
            }

            p.setPen(QPen(border, border_w));
            p.setBrush(fill);
            const QRectF body = mapped.adjusted(border_w * 0.5, border_w * 0.5,
                                                -border_w * 0.5, -border_w * 0.5);
            p.drawRect(body);

            QFont label_font = font();
            label_font.setPixelSize(28);
            label_font.setBold(true);
            p.setFont(label_font);
            p.setPen(is_hover ? QColor(ui::colors::TEXT_PRIMARY())
                              : QColor(ui::colors::TEXT_PRIMARY()));
            const QString label =
                QString::number(i + 1) + (is_primary ? QStringLiteral(" ★") : QString());
            p.drawText(mapped_int, Qt::AlignCenter, label);

            QFont sub_font = font();
            sub_font.setPixelSize(10);
            p.setFont(sub_font);
            p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
            const QString sub = QStringLiteral("%1×%2").arg(g.width()).arg(g.height());
            p.drawText(mapped_int.adjusted(4, 4, -4, -4),
                       Qt::AlignHCenter | Qt::AlignBottom, sub);

            if (is_current) {
                QFont tag_font = sub_font;
                tag_font.setBold(true);
                p.setFont(tag_font);
                p.setPen(QColor(ui::colors::AMBER()));
                p.drawText(mapped_int.adjusted(4, 4, -4, -4),
                           Qt::AlignHCenter | Qt::AlignTop, tr("CURRENT"));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        int new_idx = -1;
        for (int i = 0; i < screen_rects_.size(); ++i) {
            if (screen_rects_[i].contains(e->pos())) {
                new_idx = i;
                break;
            }
        }
        if (new_idx != hover_idx_) {
            hover_idx_ = new_idx;
            setCursor(new_idx >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override {
        if (hover_idx_ != -1) {
            hover_idx_ = -1;
            unsetCursor();
            update();
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton)
            return;
        const auto screens = QGuiApplication::screens();
        for (int i = 0; i < screen_rects_.size() && i < screens.size(); ++i) {
            if (screen_rects_[i].contains(e->pos())) {
                if (on_click_)
                    on_click_(screens[i]);
                return;
            }
        }
    }

  private:
    QScreen* current_ = nullptr;
    ClickHandler on_click_;
    int hover_idx_ = -1;
    QList<QRect> screen_rects_;
};

} // namespace

MonitorPickerDialog::MonitorPickerDialog(QWidget* parent, QScreen* current) : QDialog(parent) {
    setWindowTitle(tr("Choose Monitor"));
    setModal(true);
    setMinimumSize(620, 420);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 14, 16, 14);
    vl->setSpacing(10);

    title_label_ = new QLabel(tr("Open new window on:"), this);
    title_label_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px; font-weight: 600;")
                             .arg(QString(ui::colors::TEXT_PRIMARY())));
    vl->addWidget(title_label_);

    auto* map = new MonitorMapWidget(
        current,
        [this](QScreen* s) {
            picked_ = s;
            accept();
        },
        this);
    map_widget_ = map;
    vl->addWidget(map, /*stretch=*/1);

    hint_label_ = new QLabel(
        tr("★ = primary monitor.  Click a monitor to open the new window there."), this);
    hint_label_->setStyleSheet(QStringLiteral("color: %1; font-size: 10px;")
                            .arg(QString(ui::colors::TEXT_SECONDARY())));
    vl->addWidget(hint_label_);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    vl->addWidget(bb);
}

void MonitorPickerDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void MonitorPickerDialog::retranslateUi() {
    setWindowTitle(tr("Choose Monitor"));
    if (title_label_) title_label_->setText(tr("Open new window on:"));
    if (hint_label_)
        hint_label_->setText(
            tr("★ = primary monitor.  Click a monitor to open the new window there."));
    // Force a repaint so the per-screen "CURRENT" overlay (painted via tr() in
    // MonitorMapWidget::paintEvent) re-evaluates under the new language.
    if (map_widget_) map_widget_->update();
}

QScreen* MonitorPickerDialog::pick(QWidget* parent, QScreen* current) {
    const auto screens = QGuiApplication::screens();
    if (screens.size() <= 1)
        return screens.isEmpty() ? nullptr : screens.first();

    // Re-entrancy guard. exec() below spins a NESTED event loop, and the
    // secondary-instance relaunch path (InstanceLock::message_received →
    // new_window_on_next_monitor → here) is a QUEUED signal, so it gets
    // delivered *by that nested loop* while a picker is already open. Without
    // this guard, every relaunch stacks another modal picker on top of the
    // current one; the user then dismisses them one by one and each dismissal
    // spawns a window — looking like "the dialog won't close and keeps opening
    // windows". exec() is GUI-thread only, so a function-static pointer is the
    // right scope: surface the existing picker and drop the duplicate request
    // instead of stacking. (The InstanceLock deferred-emit only fixed the
    // crash this same re-entrancy used to cause, not the stacking.)
    static MonitorPickerDialog* s_active = nullptr;
    if (s_active) {
        s_active->raise();
        s_active->activateWindow();
        return nullptr;
    }

    MonitorPickerDialog dlg(parent, current);
    s_active = &dlg;
    const int rc = dlg.exec();
    s_active = nullptr;
    if (rc == QDialog::Accepted)
        return dlg.picked_screen();
    return nullptr;
}

} // namespace fincept
