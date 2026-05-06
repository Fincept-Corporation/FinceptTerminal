// AppearanceSection.cpp — typography / density / interface toggles.

#include "screens/settings/AppearanceSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr const char* kDefaultFontSize   = "14px";
constexpr const char* kDefaultFontFamily = "Consolas";
constexpr const char* kDefaultDensity    = "Default";
} // namespace

AppearanceSection::AppearanceSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void AppearanceSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void AppearanceSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    // ── TYPOGRAPHY ────────────────────────────────────────────────────────────
    auto* t = new QLabel("TYPOGRAPHY");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    app_font_size_ = new QComboBox;
    for (int px = 8; px <= 24; ++px)
        app_font_size_->addItem(QString("%1px").arg(px));
    app_font_size_->setCurrentText(kDefaultFontSize);
    app_font_size_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Font Size", app_font_size_));

    app_font_family_ = new QComboBox;
    app_font_family_->addItems(QFontDatabase::families());
    app_font_family_->setCurrentText(kDefaultFontFamily);
    app_font_family_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Font Family", app_font_family_));

    // Debounced live preview — coalesce rapid changes into one apply after 300ms idle
    appearance_debounce_ = new QTimer(this);
    appearance_debounce_->setSingleShot(true);
    appearance_debounce_->setInterval(300);
    connect(appearance_debounce_, &QTimer::timeout, this, [this]() {
        auto& tm = ui::ThemeManager::instance();
        int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
        if (px > 0)
            tm.apply_font(app_font_family_->currentText(), px);
        tm.apply_density(app_density_->currentText());
    });

    auto restart_debounce = [this]() { appearance_debounce_->start(); };
    connect(app_font_size_,   &QComboBox::currentTextChanged, this, restart_debounce);
    connect(app_font_family_, &QComboBox::currentTextChanged, this, restart_debounce);

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── THEME ─────────────────────────────────────────────────────────────────
    auto* t2 = new QLabel("THEME");
    t2->setStyleSheet(sub_title_ss());
    vl->addWidget(t2);
    vl->addSpacing(4);

    app_density_ = new QComboBox;
    app_density_->addItems(ui::ThemeManager::available_densities());
    app_density_->setCurrentText("Default");
    app_density_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Content Density", app_density_, "Controls padding and spacing throughout the UI."));

    connect(app_density_, &QComboBox::currentTextChanged, this, restart_debounce);

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── INTERFACE ─────────────────────────────────────────────────────────────
    auto* t3 = new QLabel("INTERFACE");
    t3->setStyleSheet(sub_title_ss());
    vl->addWidget(t3);
    vl->addSpacing(4);

    chat_bubble_toggle_ = new QCheckBox("Show AI Chat Bubble");
    chat_bubble_toggle_->setChecked(true);
    chat_bubble_toggle_->setStyleSheet(check_ss());
    vl->addWidget(
        make_row("AI Chat Bubble", chat_bubble_toggle_, "Floating chat assistant in the bottom-right corner."));

    ticker_bar_toggle_ = new QCheckBox("Show Ticker Bar");
    ticker_bar_toggle_->setChecked(true);
    ticker_bar_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Ticker Bar", ticker_bar_toggle_, "Live price ticker at the bottom of the screen."));

    animations_toggle_ = new QCheckBox("Enable Animations");
    animations_toggle_->setChecked(true);
    animations_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Animations", animations_toggle_, "Fade and transition effects throughout the UI."));

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    auto* apply_btn = new QPushButton("Save Settings");
    apply_btn->setFixedWidth(160);
    apply_btn->setStyleSheet(btn_primary_ss());
    connect(apply_btn, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto& tm = ui::ThemeManager::instance();

        repo.set("appearance.font_size",         app_font_size_->currentText(),                            "appearance");
        repo.set("appearance.font_family",       app_font_family_->currentText(),                          "appearance");
        repo.set("appearance.density",           app_density_->currentText(),                              "appearance");
        repo.set("appearance.show_chat_bubble",  chat_bubble_toggle_->isChecked() ? "true" : "false",      "appearance");
        repo.set("appearance.show_ticker_bar",   ticker_bar_toggle_->isChecked()  ? "true" : "false",      "appearance");
        repo.set("appearance.animations",        animations_toggle_->isChecked()  ? "true" : "false",      "appearance");

        // Flush any pending debounce immediately on save
        if (appearance_debounce_->isActive()) {
            appearance_debounce_->stop();
            int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
            if (px <= 0) px = 14;
            tm.apply_font(app_font_family_->currentText(), px);
            tm.apply_density(app_density_->currentText());
        }

        LOG_INFO("Settings", "Appearance saved and applied");
    });
    vl->addWidget(apply_btn);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void AppearanceSection::reload() {
    if (!app_font_size_) return;
    auto& repo = SettingsRepository::instance();

    // Block signals during load so live-preview slots don't fire for every
    // setCurrentIndex() call — prevents 4+ ThemeManager apply_* calls and
    // the associated theme_changed signal re-rendering every widget on screen.
    const QSignalBlocker b1(app_font_size_);
    const QSignalBlocker b2(app_font_family_);
    const QSignalBlocker b4(app_density_);

    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        QString val = r.is_ok() ? r.value() : def;
        int idx = cb->findText(val);
        if (idx >= 0) cb->setCurrentIndex(idx);
    };

    auto load_check = [&](QCheckBox* cb, const QString& key, bool def) {
        if (!cb) return;
        auto r = repo.get(key);
        cb->setChecked(!r.is_ok() ? def : r.value() != "false");
    };

    load_combo(app_font_size_,   "appearance.font_size",   kDefaultFontSize);
    load_combo(app_font_family_, "appearance.font_family", kDefaultFontFamily);
    load_combo(app_density_,     "appearance.density",     kDefaultDensity);

    load_check(chat_bubble_toggle_, "appearance.show_chat_bubble", true);
    load_check(ticker_bar_toggle_,  "appearance.show_ticker_bar",  true);
    load_check(animations_toggle_,  "appearance.animations",       true);
}

} // namespace fincept::screens
