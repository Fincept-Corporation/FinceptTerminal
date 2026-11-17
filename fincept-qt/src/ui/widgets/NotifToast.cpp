#include "ui/widgets/NotifToast.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fincept::ui {

using namespace fincept::notifications;

NotifToast::NotifToast(QWidget* parent) : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(metrics::width(TOAST_W), metrics::width(TOAST_H));
    hide();

    // ── Layout ────────────────────────────────────────────────────────────────
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(12, 10, 12, 10);
    outer->setSpacing(10);

    level_dot_ = new QLabel("●");
    level_dot_->setFixedSize(metrics::width(10), metrics::width(10));
    level_dot_->setStyleSheet("background: transparent; font-size: " + fincept::ui::fonts::px(-4) + "px;");
    outer->addWidget(level_dot_, 0, Qt::AlignTop | Qt::AlignHCenter);

    auto* text_col = new QWidget;
    text_col->setStyleSheet("background: transparent;");
    auto* tvl = new QVBoxLayout(text_col);
    tvl->setContentsMargins(0, 0, 0, 0);
    tvl->setSpacing(2);

    title_lbl_ = new QLabel;
    title_lbl_->setStyleSheet(QString("color: %1; font-size: " + fincept::ui::fonts::px(-1) + "px; font-weight: bold; background: transparent;")
                                  .arg(colors::TEXT_PRIMARY.get()));
    title_lbl_->setWordWrap(false);
    tvl->addWidget(title_lbl_);

    msg_lbl_ = new QLabel;
    msg_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-2) + "px; background: transparent;").arg(colors::TEXT_SECONDARY.get()));
    msg_lbl_->setWordWrap(true);
    tvl->addWidget(msg_lbl_);

    outer->addWidget(text_col, 1);

    time_lbl_ = new QLabel;
    time_lbl_->setStyleSheet(
        QString("color: %1; font-size: " + fincept::ui::fonts::px(-4) + "px; background: transparent;").arg(colors::TEXT_TERTIARY.get()));
    time_lbl_->setAlignment(Qt::AlignTop | Qt::AlignRight);
    outer->addWidget(time_lbl_, 0, Qt::AlignTop);

    // ── Timers & animation (P3: start only in showEvent) ─────────────────────
    auto_dismiss_ = new QTimer(this);
    auto_dismiss_->setSingleShot(true);
    auto_dismiss_->setInterval(AUTO_DISMISS_MS);
    connect(auto_dismiss_, &QTimer::timeout, this, &NotifToast::dismiss);

    slide_anim_ = new QPropertyAnimation(this, "pos", this);
    slide_anim_->setDuration(ANIM_MS);
    slide_anim_->setEasingCurve(QEasingCurve::OutCubic);
}

// ── Slot ──────────────────────────────────────────────────────────────────────

void NotifToast::show_notification(const NotificationRecord& record) {
    current_level_ = record.request.level;
    apply_size(); // the font may have changed since the last toast

    const QString dot_color = [&]() -> QString {
        switch (current_level_) {
            case NotifLevel::Warning:
                return colors::WARNING.get();
            case NotifLevel::Alert:
                return colors::AMBER.get();
            case NotifLevel::Critical:
                return colors::DANGER.get();
            default:
                return colors::CYAN.get();
        }
    }();

    level_dot_->setStyleSheet(QString("color: %1; background: transparent; font-size: " + fincept::ui::fonts::px(-4) + "px;").arg(dot_color));

    title_lbl_->setText(record.request.title.left(50));
    msg_lbl_->setText(record.request.message.left(80));
    time_lbl_->setText(record.request.timestamp.toString("hh:mm:ss"));

    reposition();
    show();
    raise();
}

// ── Events ────────────────────────────────────────────────────────────────────

void NotifToast::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Slide in from right
    if (parentWidget()) {
        const QPoint hidden_pos(parentWidget()->width(), y());
        const QPoint shown_pos(parentWidget()->width() - width() - MARGIN, y());
        slide_anim_->setStartValue(hidden_pos);
        slide_anim_->setEndValue(shown_pos);
        slide_anim_->start();
    }
    auto_dismiss_->start();
}

void NotifToast::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    auto_dismiss_->stop();
    slide_anim_->stop();
}

void NotifToast::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    reposition();
}

void NotifToast::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QString border_color = [&]() -> QString {
        switch (current_level_) {
            case NotifLevel::Warning:
                return colors::WARNING.get();
            case NotifLevel::Alert:
                return colors::AMBER.get();
            case NotifLevel::Critical:
                return colors::DANGER.get();
            default:
                return colors::CYAN.get();
        }
    }();

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(colors::BG_SURFACE.get()));
    p.drawRoundedRect(rect(), 6, 6);

    // Left accent bar
    p.setBrush(QColor(border_color));
    p.drawRoundedRect(0, 0, 3, height(), 2, 2);

    // Border
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(colors::BORDER_MED.get()), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
}

// ── Private ───────────────────────────────────────────────────────────────────

void NotifToast::apply_size() {
    setFixedSize(metrics::width(TOAST_W), metrics::width(TOAST_H));
    if (level_dot_)
        level_dot_->setFixedSize(metrics::width(10), metrics::width(10));
}

void NotifToast::reposition() {
    if (!parentWidget())
        return;
    const int x = parentWidget()->width() - width() - MARGIN;
    const int y = MARGIN;
    move(x, y);
}

void NotifToast::dismiss() {
    if (!parentWidget()) {
        hide();
        return;
    }
    // Slide back out to the right
    const QPoint shown_pos(parentWidget()->width() - width() - MARGIN, y());
    const QPoint hidden_pos(parentWidget()->width() + 10, y());
    slide_anim_->setStartValue(shown_pos);
    slide_anim_->setEndValue(hidden_pos);
    connect(slide_anim_, &QPropertyAnimation::finished, this, &NotifToast::hide, Qt::SingleShotConnection);
    slide_anim_->start();
}

} // namespace fincept::ui
