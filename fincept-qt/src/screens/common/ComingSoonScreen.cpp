#include "screens/common/ComingSoonScreen.h"

#include "ui/theme/Theme.h"

#include <QVBoxLayout>

namespace fincept::screens {

ComingSoonScreen::ComingSoonScreen(const QString& tab_name, QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(this);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(12);

    auto* icon = new QLabel("\xe2\x9a\xa1"); // ⚡
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size:48px;background:transparent;");
    vl->addWidget(icon);

    auto* title = new QLabel(tab_name.toUpper());
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color:%1;font-size:20px;font-weight:700;background:transparent;"
                                 "letter-spacing:2px;")
                             .arg(ui::colors::AMBER()));
    vl->addWidget(title);

    sub_label_ = new QLabel(tr("COMING SOON"));
    sub_label_->setAlignment(Qt::AlignCenter);
    sub_label_->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;background:transparent;"
                                      "letter-spacing:3px;")
                                  .arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(sub_label_);

    desc_label_ = new QLabel(tr("This module is under active development.\nIt will be available in a future update."));
    desc_label_->setAlignment(Qt::AlignCenter);
    desc_label_->setStyleSheet(QString("color:%1;font-size:13px;background:transparent;").arg(ui::colors::TEXT_DIM()));
    vl->addWidget(desc_label_);
}

void ComingSoonScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ComingSoonScreen::retranslateUi() {
    if (sub_label_) sub_label_->setText(tr("COMING SOON"));
    if (desc_label_)
        desc_label_->setText(tr("This module is under active development.\nIt will be available in a future update."));
}

} // namespace fincept::screens
