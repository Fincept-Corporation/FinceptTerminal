// src/ui/widgets/LoadingOverlay.cpp
#include "ui/widgets/LoadingOverlay.h"

#include "ui/theme/Theme.h"

#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace fincept::ui {

static const char* SPIN_FRAMES[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
static constexpr int SPIN_FRAME_COUNT = 10;

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    auto* vl = new QVBoxLayout(this);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(14);

    spinner_label_ = new QLabel(SPIN_FRAMES[0]);
    spinner_label_->setAlignment(Qt::AlignCenter);
    spinner_label_->setStyleSheet(
        QString("color:%1; font-size:32px; background:transparent; border:0;").arg(ui::colors::AMBER));

    message_label_ = new QLabel("LOADING…");
    message_label_->setAlignment(Qt::AlignCenter);
    message_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:2px; "
                                          "background:transparent; border:0;")
                                      .arg(ui::colors::AMBER));

    vl->addWidget(spinner_label_);
    vl->addWidget(message_label_);

    spin_timer_ = new QTimer(this);
    spin_timer_->setInterval(80);
    connect(spin_timer_, &QTimer::timeout, this, [this]() {
        spin_idx_ = (spin_idx_ + 1) % SPIN_FRAME_COUNT;
        spinner_label_->setText(SPIN_FRAMES[spin_idx_]);
    });

    // Fill parent immediately
    if (parent) {
        setGeometry(parent->rect());
        raise();
    }
}

void LoadingOverlay::show_loading(const QString& message) {
    message_label_->setText(message);
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    raise();
    show();
    spin_idx_ = 0;
    spin_timer_->start();
}

void LoadingOverlay::hide_loading() {
    spin_timer_->stop();
    hide();
}

void LoadingOverlay::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    // Dark semi-transparent background
    QColor bg(15, 17, 21, 210); // ~#0f1115 at 82% opacity
    p.fillRect(rect(), bg);
}

} // namespace fincept::ui
