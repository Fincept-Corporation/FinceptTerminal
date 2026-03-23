#include "screens/ComingSoonScreen.h"

#include <QVBoxLayout>

namespace fincept::screens {

ComingSoonScreen::ComingSoonScreen(const QString& tab_name, QWidget* parent) : QWidget(parent) {
    setStyleSheet("background:#080808;");
    auto* vl = new QVBoxLayout(this);
    vl->setAlignment(Qt::AlignCenter);
    vl->setSpacing(12);

    auto* icon = new QLabel("\xe2\x9a\xa1"); // ⚡
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size:48px;background:transparent;");
    vl->addWidget(icon);

    auto* title = new QLabel(tab_name.toUpper());
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#d97706;font-size:20px;font-weight:700;background:transparent;"
                         "letter-spacing:2px;font-family:'Consolas',monospace;");
    vl->addWidget(title);

    auto* sub = new QLabel("COMING SOON");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color:#525252;font-size:14px;font-weight:700;background:transparent;"
                       "letter-spacing:3px;font-family:'Consolas',monospace;");
    vl->addWidget(sub);

    auto* desc = new QLabel("This module is under active development.\nIt will be available in a future update.");
    desc->setAlignment(Qt::AlignCenter);
    desc->setStyleSheet("color:#404040;font-size:13px;background:transparent;"
                        "font-family:'Consolas',monospace;");
    vl->addWidget(desc);
}

} // namespace fincept::screens
