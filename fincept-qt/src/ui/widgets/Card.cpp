#include "ui/widgets/Card.h"
#include "ui/theme/Theme.h"
#include "ui/theme/StyleSheets.h"

namespace fincept::ui {

Card::Card(const QString& title, QWidget* parent) : QFrame(parent) {
    setStyleSheet(styles::card_frame());

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Title bar
    auto* title_bar = new QWidget;
    title_bar->setFixedHeight(28);
    title_bar->setStyleSheet("background: transparent;");

    auto* hl = new QHBoxLayout(title_bar);
    hl->setContentsMargins(8, 0, 4, 0);
    hl->setSpacing(4);

    title_label_ = new QLabel(title);
    title_label_->setStyleSheet(styles::card_title());
    hl->addWidget(title_label_);

    hl->addStretch();

    auto* close_btn = new QPushButton("x");
    close_btn->setFixedSize(20, 20);
    close_btn->setStyleSheet(styles::card_close_button());
    connect(close_btn, &QPushButton::clicked, this, &Card::close_requested);
    hl->addWidget(close_btn);

    vl->addWidget(title_bar);

    // Separator
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(colors::BORDER));
    vl->addWidget(sep);

    // Content
    content_ = new QWidget;
    content_->setStyleSheet("background: transparent;");
    content_layout_ = new QVBoxLayout(content_);
    content_layout_->setContentsMargins(8, 4, 8, 8);
    content_layout_->setSpacing(4);

    vl->addWidget(content_, 1);
}

void Card::set_title(const QString& title) {
    title_label_->setText(title);
}

} // namespace fincept::ui
