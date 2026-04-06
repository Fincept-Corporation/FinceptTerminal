#include "screens/dashboard/widgets/BaseWidget.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QSize>
#include <QStyle>

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
    refresh_btn_ = new QPushButton;
    refresh_btn_->setFixedSize(20, 20);
    refresh_btn_->setText("");
    refresh_btn_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    refresh_btn_->setIconSize(QSize(12, 12));
    refresh_btn_->setToolTip("Refresh widget data");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 2px; "
                "padding: 0px; }"
                "QPushButton:hover { color: %4; border-color: %4; background: %5; }")
            .arg(accent_color_, ui::colors::BG_SURFACE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                 ui::colors::BG_HOVER));
    connect(refresh_btn_, &QPushButton::clicked, this, &BaseWidget::refresh_requested);
    hl->addWidget(refresh_btn_);

    // Close button
    auto* close_btn = new QPushButton;
    close_btn->setFixedSize(20, 20);
    close_btn->setText("");
    close_btn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    close_btn->setIconSize(QSize(11, 11));
    close_btn->setToolTip("Close widget");
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setStyleSheet(
        QString("QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 2px; "
                "padding: 0px; }"
                "QPushButton:hover { color: %4; border-color: %4; background: %5; }")
            .arg(ui::colors::TEXT_TERTIARY, ui::colors::BG_SURFACE, ui::colors::BORDER_DIM, ui::colors::NEGATIVE,
                 ui::colors::BG_HOVER));
    connect(close_btn, &QPushButton::clicked, this, &BaseWidget::close_requested);
    hl->addWidget(close_btn);

    vl->addWidget(title_bar_);

    // Auto-refresh styles on theme/font change — subclasses get this for free
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) {
                refresh_base_theme();
                on_theme_changed();
            });

    // ── Content ──
    content_ = new QWidget;
    content_->setStyleSheet("background: transparent;");
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(0);

    vl->addWidget(content_, 1);
}

void BaseWidget::refresh_base_theme() {
    setStyleSheet(QString("BaseWidget{background:%1;border:1px solid %2;border-radius:2px;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_BRIGHT()));
    if (title_bar_)
        title_bar_->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    if (refresh_btn_)
        refresh_btn_->setStyleSheet(
            QString("QPushButton{color:%1;background:%2;border:1px solid %3;border-radius:2px;padding:0;}"
                    "QPushButton:hover{color:%4;border-color:%4;background:%5;}")
            .arg(accent_color_, ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER()));
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


