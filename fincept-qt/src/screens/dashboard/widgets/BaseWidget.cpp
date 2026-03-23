#include "screens/dashboard/widgets/BaseWidget.h"

#include "ui/theme/Theme.h"

namespace fincept::screens::widgets {

BaseWidget::BaseWidget(const QString& title, QWidget* parent, const QString& accent_color)
    : QFrame(parent), accent_color_(accent_color.isEmpty() ? ui::colors::AMBER : accent_color) {
    setStyleSheet(QString("BaseWidget { background: %1; border: 1px solid %2; border-radius: 2px; }")
                      .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_BRIGHT));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Title bar ──
    title_bar_ = new QWidget;
    title_bar_->setFixedHeight(26);
    title_bar_->setStyleSheet(
        QString("background: %1; border-bottom: 1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

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
        QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::AMBER));
    loading_label_->setVisible(false);
    hl->addWidget(loading_label_);

    hl->addStretch();

    // Refresh button
    refresh_btn_ = new QPushButton(QChar(0x21BB)); // ↻
    refresh_btn_->setFixedSize(20, 20);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-size: 11px; }"
                "QPushButton:hover { color: %2; }")
            .arg(ui::colors::TEXT_TERTIARY, ui::colors::TEXT_PRIMARY));
    connect(refresh_btn_, &QPushButton::clicked, this, &BaseWidget::refresh_requested);
    hl->addWidget(refresh_btn_);

    // Close button
    auto* close_btn = new QPushButton(QChar(0x2715)); // ✕
    close_btn->setFixedSize(20, 20);
    close_btn->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-size: 10px; }"
                "QPushButton:hover { color: %2; }")
            .arg(ui::colors::TEXT_TERTIARY, ui::colors::NEGATIVE));
    connect(close_btn, &QPushButton::clicked, this, &BaseWidget::close_requested);
    hl->addWidget(close_btn);

    vl->addWidget(title_bar_);

    // ── Content ──
    content_ = new QWidget;
    content_->setStyleSheet("background: transparent;");
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    vl->addWidget(content_, 1);
}

void BaseWidget::set_loading(bool loading) {
    loading_label_->setVisible(loading);
    loading_label_->setText(loading ? "LOADING..." : "");
}

void BaseWidget::set_error(const QString& error) {
    if (error.isEmpty())
        return;

    // Clear content and show error
    QLayoutItem* item;
    while ((item = content_layout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto* err_label = new QLabel(error);
    err_label->setAlignment(Qt::AlignCenter);
    err_label->setWordWrap(true);
    err_label->setStyleSheet(
        QString("color: %1; font-size: 9px; padding: 16px; background: transparent;").arg(ui::colors::NEGATIVE));
    content_layout_->addWidget(err_label);
}

void BaseWidget::set_title(const QString& title) {
    title_label_->setText(title);
}

} // namespace fincept::screens::widgets
