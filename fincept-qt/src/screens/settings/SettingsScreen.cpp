#include "screens/settings/SettingsScreen.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>
#include <QCheckBox>

namespace fincept::screens {

static const char* SECTION_TITLE =
    "color: #FF6600; font-size: 13px; font-weight: bold; letter-spacing: 0.5px; "
    "background: transparent; font-family: 'Inter', sans-serif;";

static const char* LABEL =
    "color: #888888; font-size: 13px; background: transparent; font-family: 'Inter', sans-serif;";

static const char* VALUE =
    "color: #e0e0e0; font-size: 13px; background: transparent; font-family: 'Inter', sans-serif;";

SettingsScreen::SettingsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left nav
    auto* nav = new QWidget;
    nav->setFixedWidth(200);
    nav->setStyleSheet(QString("background: %1; border-right: 1px solid %2;")
        .arg(ui::colors::PANEL, ui::colors::BORDER));
    auto* nvl = new QVBoxLayout(nav);
    nvl->setContentsMargins(8, 16, 8, 8);
    nvl->setSpacing(2);

    auto* title = new QLabel("SETTINGS");
    title->setStyleSheet(SECTION_TITLE);
    nvl->addWidget(title);
    nvl->addSpacing(12);

    sections_ = new QStackedWidget;
    sections_->addWidget(build_appearance());
    sections_->addWidget(build_notifications());
    sections_->addWidget(build_storage());

    auto make_btn = [&](const QString& text, int idx) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(32);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #888; border: none; "
            "text-align: left; padding: 0 12px; font-size: 13px; font-family: 'Inter', sans-serif; }"
            "QPushButton:hover { background: #111; color: #e0e0e0; }");
        connect(btn, &QPushButton::clicked, this, [this, idx]() { sections_->setCurrentIndex(idx); });
        nvl->addWidget(btn);
    };

    make_btn("Appearance", 0);
    make_btn("Notifications", 1);
    make_btn("Storage & Cache", 2);

    nvl->addStretch();
    root->addWidget(nav);
    root->addWidget(sections_, 1);
}

QWidget* SettingsScreen::build_appearance() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* t = new QLabel("TERMINAL APPEARANCE");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);

    auto* sep = new QFrame; sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep);

    // Font size
    auto* fs_row = new QWidget;
    fs_row->setStyleSheet("background: transparent;");
    auto* frl = new QHBoxLayout(fs_row);
    frl->setContentsMargins(0, 4, 0, 4);
    auto* fs_lbl = new QLabel("Font Size");
    fs_lbl->setStyleSheet(LABEL);
    frl->addWidget(fs_lbl);
    frl->addStretch();
    auto* fs_combo = new QComboBox;
    fs_combo->addItems({"12px", "13px", "14px", "15px", "16px"});
    fs_combo->setCurrentText("14px");
    frl->addWidget(fs_combo);
    vl->addWidget(fs_row);

    // Theme
    auto* th_row = new QWidget;
    th_row->setStyleSheet("background: transparent;");
    auto* trl = new QHBoxLayout(th_row);
    trl->setContentsMargins(0, 4, 0, 4);
    auto* th_lbl = new QLabel("Theme");
    th_lbl->setStyleSheet(LABEL);
    trl->addWidget(th_lbl);
    trl->addStretch();
    auto* th_val = new QLabel("Bloomberg Dark");
    th_val->setStyleSheet(VALUE);
    trl->addWidget(th_val);
    vl->addWidget(th_row);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* SettingsScreen::build_notifications() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* t = new QLabel("NOTIFICATIONS");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);

    auto* sep = new QFrame; sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep);

    auto add_toggle = [&](const QString& label, bool default_on) {
        auto* row = new QWidget;
        row->setStyleSheet("background: transparent;");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 4, 0, 4);
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(LABEL);
        rl->addWidget(lbl);
        rl->addStretch();
        auto* cb = new QCheckBox;
        cb->setChecked(default_on);
        rl->addWidget(cb);
        vl->addWidget(row);
    };

    add_toggle("Email Notifications", true);
    add_toggle("In-App Notifications", true);
    add_toggle("Price Alerts", false);
    add_toggle("News Alerts", false);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QWidget* SettingsScreen::build_storage() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    auto* t = new QLabel("STORAGE & CACHE");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);

    auto* sep = new QFrame; sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER));
    vl->addWidget(sep);

    auto* clear_btn = new QPushButton("Clear Cache");
    clear_btn->setFixedHeight(32);
    clear_btn->setStyleSheet(
        "QPushButton { background: #1a1a1a; color: #e0e0e0; border: 1px solid #333; "
        "border-radius: 4px; font-size: 13px; font-family: 'Inter', sans-serif; }"
        "QPushButton:hover { background: #222; }");
    vl->addWidget(clear_btn);

    auto* note = new QLabel("Clearing cache will remove locally stored market data and preferences.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #666; font-size: 12px; background: transparent;");
    vl->addWidget(note);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

} // namespace fincept::screens
