// SettingsScreen.cpp — full-featured settings (Credentials, Appearance,
// Notifications, Storage & Cache, Data Sources, LLM Config, MCP Servers)

#include "screens/settings/SettingsScreen.h"

#include "ai_chat/LlmService.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "services/notifications/NotificationService.h"
#include "screens/settings/LlmConfigSection.h"
#include "screens/settings/McpServersSection.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/DataSourceRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Token-based style builders (live — reflect active theme) ─────────────────

// All helpers read live ThemeManager tokens — no font-size overrides (global QSS handles font)
static QString section_title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::AMBER());
}
static QString sub_title_ss() {
    return QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::TEXT_SECONDARY());
}
static QString label_ss() {
    return QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY());
}
static QString nav_btn_ss() {
    return QString("QPushButton{background:transparent;color:%1;border:none;text-align:left;padding:0 12px;}"
                   "QPushButton:hover{background:%2;color:%3;}"
                   "QPushButton:checked{color:%4;background:%2;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_PRIMARY(),   ui::colors::AMBER());
}
static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER());
}
static QString btn_primary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:none;font-weight:700;padding:0 16px;height:32px;}"
                   "QPushButton:hover{background:%3;}")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM());
}
static QString btn_secondary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;height:32px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_BRIGHT(), ui::colors::BG_HOVER());
}
static QString btn_danger_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE(),
             ui::colors::BORDER_DIM(), ui::colors::BG_HOVER());
}
static QString combo_ss() {
    return QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 8px;min-width:160px;}"
                   "QComboBox:focus{border:1px solid %4;}"
                   "QComboBox::drop-down{border:none;width:20px;}"
                   "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%5;border:1px solid %3;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::BG_HOVER());
}
static QString check_ss() {
    return QString("QCheckBox{color:%1;background:transparent;}"
                   "QCheckBox::indicator{width:14px;height:14px;}"
                   "QCheckBox::indicator:unchecked{border:1px solid %2;background:%3;}"
                   "QCheckBox::indicator:checked{border:1px solid %4;background:%4;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_BRIGHT(),
             ui::colors::BG_RAISED(),      ui::colors::AMBER());
}

// ── Credential key definitions ────────────────────────────────────────────────

using CredDef = QPair<QString, QString>; // {env_key, display_name}
static const QList<CredDef> CRED_KEYS = {
    {"ALPHA_VANTAGE_API_KEY", "Alpha Vantage"},
    {"POLYGON_API_KEY", "Polygon.io"},
    {"DATABENTO_API_KEY", "Databento"},
    {"FRED_API_KEY", "FRED (Federal Reserve)"},
    {"NEWSAPI_KEY", "NewsAPI"},
    {"BINANCE_API_KEY", "Binance API Key"},
    {"BINANCE_SECRET_KEY", "Binance Secret Key"},
    {"KRAKEN_API_KEY", "Kraken API Key"},
    {"KRAKEN_SECRET_KEY", "Kraken Secret Key"},
    {"IEX_CLOUD_TOKEN", "IEX Cloud Token"},
    {"FINNHUB_API_KEY", "Finnhub API Key"},
    {"TIINGO_API_KEY", "Tiingo API Key"},
    {"QUANDL_API_KEY", "Quandl"},
    {"POLYMARKET_API_KEY", "Polymarket API Key"},
    {"POLYMARKET_SECRET", "Polymarket Secret"},
    {"POLYMARKET_PASSPHRASE", "Polymarket Passphrase"},
    {"POLYMARKET_WALLET", "Polymarket Wallet Address"},
};

// ── Construction ──────────────────────────────────────────────────────────────

SettingsScreen::SettingsScreen(QWidget* parent) : QWidget(parent) {

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left nav panel
    nav_ = new QWidget;
    auto* nav = nav_;
    nav->setFixedWidth(200);
    auto* nvl = new QVBoxLayout(nav);
    nvl->setContentsMargins(8, 16, 8, 8);
    nvl->setSpacing(2);

    auto* title = new QLabel("SETTINGS");
    title->setStyleSheet(section_title_ss());
    nvl->addWidget(title);
    nvl->addSpacing(12);

    // Sections (order matters — indices used in showEvent)
    sections_ = new QStackedWidget;
    sections_->addWidget(build_credentials());   // 0
    sections_->addWidget(build_appearance());    // 1
    sections_->addWidget(build_notifications()); // 2
    sections_->addWidget(build_storage());       // 3
    sections_->addWidget(build_data_sources());  // 4
    sections_->addWidget(build_llm_config());    // 5
    sections_->addWidget(build_mcp_servers());   // 6
    sections_->addWidget(build_logging());       // 7

    QList<QPushButton*> nav_btns;
    auto make_btn = [&](const QString& text, int idx) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(32);
        btn->setCheckable(true);
        btn->setStyleSheet(nav_btn_ss());
        connect(btn, &QPushButton::clicked, this, [this, btn, idx]() {
            // Uncheck all sibling buttons in the parent layout
            if (auto* parent = btn->parentWidget()) {
                for (auto* sibling : parent->findChildren<QPushButton*>()) {
                    sibling->setChecked(false);
                }
            }
            btn->setChecked(true);
            sections_->setCurrentIndex(idx);
        });
        nvl->addWidget(btn);
        nav_btns.append(btn);
        return btn;
    };

    auto* first = make_btn("Credentials", 0);
    make_btn("Appearance", 1);
    make_btn("Notifications", 2);
    make_btn("Storage & Cache", 3);
    make_btn("Data Sources", 4);
    make_btn("LLM Config", 5);
    make_btn("MCP Servers", 6);
    make_btn("Logging", 7);

    first->setChecked(true);

    nvl->addStretch();
    root->addWidget(nav);
    root->addWidget(sections_, 1);

    // Wire LLM config changes → reload AI chat service
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5))) {
        connect(llm, &LlmConfigSection::config_changed, this,
                []() { ai_chat::LlmService::instance().reload_config(); });
    }

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void SettingsScreen::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (nav_)
        nav_->setStyleSheet(
            QString("background:%1;border-right:1px solid %2;")
            .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    // Re-apply nav button styles
    if (nav_) {
        for (auto* btn : nav_->findChildren<QPushButton*>())
            btn->setStyleSheet(nav_btn_ss());
    }
}

void SettingsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    load_credentials();
    load_appearance();
    load_notifications();
    refresh_storage_stats();
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5)))
        llm->reload();
}

// ── Shared helpers ────────────────────────────────────────────────────────────

QWidget* SettingsScreen::make_row(const QString& label, QWidget* control, const QString& description) {
    auto* row = new QWidget;
    row->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(row);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(3);

    auto* hl = new QHBoxLayout;
    hl->setContentsMargins(0, 4, 0, 4);
    auto* lbl = new QLabel(label);
    lbl->setStyleSheet(label_ss());
    hl->addWidget(lbl);
    hl->addStretch();
    hl->addWidget(control);
    vl->addLayout(hl);

    if (!description.isEmpty()) {
        auto* desc = new QLabel(description);
        desc->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        desc->setWordWrap(true);
        vl->addWidget(desc);
    }
    return row;
}

QFrame* SettingsScreen::make_sep() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    return sep;
}

// ── Credentials ───────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_credentials() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    // Title
    auto* t = new QLabel("API CREDENTIALS");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info =
        new QLabel("Store API keys securely in the OS keychain. Keys are never written to disk in plain text.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(16);

    // One card per credential
    for (const auto& def : CRED_KEYS) {
        const QString& key = def.first;
        const QString& name = def.second;

        // Card frame
        auto* card = new QFrame;
        card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:4px;}").arg(ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // Card header
        auto* hdr = new QWidget;
        hdr->setFixedHeight(34);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(),ui::colors::BORDER_DIM()));
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        hhl->addWidget(name_lbl);
        hhl->addStretch();

        // Status label — persisted across load_credentials calls
        auto* status_lbl = new QLabel("Not set");
        status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        hhl->addWidget(status_lbl);
        cred_status_[key] = status_lbl;

        cvl->addWidget(hdr);

        // Card body: input + save button
        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* bhl = new QHBoxLayout(body);
        bhl->setContentsMargins(12, 10, 12, 10);
        bhl->setSpacing(8);

        auto* field = new QLineEdit;
        field->setEchoMode(QLineEdit::Password);
        field->setPlaceholderText("Not configured");
        field->setStyleSheet(input_ss());
        cred_fields_[key] = field;
        bhl->addWidget(field, 1);

        auto* save_btn = new QPushButton("Save");
        save_btn->setFixedHeight(30);
        save_btn->setFixedWidth(70);
        save_btn->setStyleSheet(btn_primary_ss());
        bhl->addWidget(save_btn);

        connect(save_btn, &QPushButton::clicked, this, [this, key, field, status_lbl]() {
            QString val = field->text().trimmed();
            if (val.isEmpty()) {
                SecureStorage::instance().remove(key);
                field->setPlaceholderText("Not configured");
                status_lbl->setText("Cleared");
                status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
                LOG_INFO("Credentials", "Cleared key: " + key);
            } else {
                auto r = SecureStorage::instance().store(key, val);
                if (r.is_ok()) {
                    field->clear();
                    field->setPlaceholderText("•••••••• (saved)");
                    status_lbl->setText("Saved ✓");
                    status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                    LOG_INFO("Credentials", "Stored key: " + key);
                } else {
                    status_lbl->setText("Save failed");
                    status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("Credentials", "Failed to store " + key);
                }
            }
        });

        cvl->addWidget(body);
        vl->addWidget(card);
        vl->addSpacing(8);
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::load_credentials() {
    for (const auto& def : CRED_KEYS) {
        const QString& key = def.first;
        auto* field = cred_fields_.value(key, nullptr);
        auto* status = cred_status_.value(key, nullptr);
        if (!field || !status)
            continue;

        auto r = SecureStorage::instance().retrieve(key);
        if (r.is_ok() && !r.value().isEmpty()) {
            field->clear();
            field->setPlaceholderText("•••••••• (saved)");
            status->setText("Saved ✓");
            status->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
        } else {
            field->clear();
            field->setPlaceholderText("Not configured");
            status->setText("Not set");
            status->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        }
    }
}

// ── Appearance ────────────────────────────────────────────────────────────────

// Single source of truth for appearance defaults
namespace {
constexpr const char* kDefaultFontSize   = "14px";
constexpr const char* kDefaultFontFamily = "Consolas";
constexpr const char* kDefaultTheme      = "Obsidian";
constexpr const char* kDefaultDensity    = "Default";
} // namespace

QWidget* SettingsScreen::build_appearance() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { background: %1; width: 6px; }"
                "QScrollBar::handle:vertical { background: %2; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(fincept::ui::colors::BG_SURFACE, fincept::ui::colors::BORDER_MED));

    auto* page = new QWidget;
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
        auto& tm = fincept::ui::ThemeManager::instance();
        int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
        if (px > 0) tm.apply_font(app_font_family_->currentText(), px);
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

    app_theme_ = new QComboBox;
    app_theme_->addItems(fincept::ui::ThemeManager::available_themes());
    app_theme_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Color Theme", app_theme_, "Applied immediately as a live preview."));

    // Live preview: apply theme immediately on change
    connect(app_theme_, &QComboBox::currentTextChanged, this, [](const QString& name) {
        fincept::ui::ThemeManager::instance().apply_theme(name);
    });

    // Accent color override
    accent_color_btn_ = new QPushButton("Pick Color");
    accent_color_btn_->setFixedWidth(120);
    accent_color_btn_->setStyleSheet(btn_secondary_ss());
    connect(accent_color_btn_, &QPushButton::clicked, this, [this]() {
        QColor initial = custom_accent_color_.isEmpty()
            ? QColor(fincept::ui::ThemeManager::instance().tokens().accent)
            : QColor(custom_accent_color_);
        QColor chosen = QColorDialog::getColor(initial, this, "Choose Accent Color");
        if (chosen.isValid()) {
            custom_accent_color_ = chosen.name();
            accent_color_btn_->setStyleSheet(
                QString("QPushButton { background: %1; color: %2; border: none; "
                        "padding: 0 12px; height: 32px; }")
                .arg(custom_accent_color_,
                     QString(fincept::ui::ThemeManager::instance().tokens().bg_base)));
            fincept::ui::ThemeManager::instance().apply_accent(custom_accent_color_);
        }
    });
    vl->addWidget(make_row("Accent Color", accent_color_btn_, "Overrides the theme's default accent color."));

    app_density_ = new QComboBox;
    app_density_->addItems(fincept::ui::ThemeManager::available_densities());
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
    vl->addWidget(make_row("AI Chat Bubble", chat_bubble_toggle_,
                            "Floating chat assistant in the bottom-right corner."));

    ticker_bar_toggle_ = new QCheckBox("Show Ticker Bar");
    ticker_bar_toggle_->setChecked(true);
    ticker_bar_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Ticker Bar", ticker_bar_toggle_,
                            "Live price ticker at the bottom of the screen."));

    animations_toggle_ = new QCheckBox("Enable Animations");
    animations_toggle_->setChecked(true);
    animations_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Animations", animations_toggle_,
                            "Fade and transition effects throughout the UI."));

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    auto* apply_btn = new QPushButton("Save Settings");
    apply_btn->setFixedWidth(160);
    apply_btn->setStyleSheet(btn_primary_ss());
    connect(apply_btn, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto& tm   = fincept::ui::ThemeManager::instance();

        // Persist all values
        repo.set("appearance.font_size",       app_font_size_->currentText(),   "appearance");
        repo.set("appearance.font_family",     app_font_family_->currentText(), "appearance");
        repo.set("appearance.theme",           app_theme_->currentText(),       "appearance");
        repo.set("appearance.density",         app_density_->currentText(),     "appearance");
        repo.set("appearance.show_chat_bubble",
                 chat_bubble_toggle_->isChecked() ? "true" : "false", "appearance");
        repo.set("appearance.show_ticker_bar",
                 ticker_bar_toggle_->isChecked() ? "true" : "false", "appearance");
        repo.set("appearance.animations",
                 animations_toggle_->isChecked() ? "true" : "false", "appearance");
        if (!custom_accent_color_.isEmpty())
            repo.set("appearance.accent_color", custom_accent_color_, "appearance");

        // Flush any pending debounce immediately on save
        if (appearance_debounce_->isActive()) {
            appearance_debounce_->stop();
            int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
            if (px <= 0) px = 14;
            tm.apply_font(app_font_family_->currentText(), px);
            tm.apply_density(app_density_->currentText());
        }
        if (!custom_accent_color_.isEmpty())
            tm.apply_accent(custom_accent_color_);

        LOG_INFO("Settings", "Appearance saved and applied");
    });
    vl->addWidget(apply_btn);
    vl->addStretch();

    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::load_appearance() {
    if (!app_font_size_)
        return;
    auto& repo = SettingsRepository::instance();

    // Block signals during load so live-preview slots don't fire for every
    // setCurrentIndex() call — prevents 4+ ThemeManager apply_* calls and
    // the associated theme_changed signal re-rendering every widget on screen.
    const QSignalBlocker b1(app_font_size_);
    const QSignalBlocker b2(app_font_family_);
    const QSignalBlocker b3(app_theme_);
    const QSignalBlocker b4(app_density_);

    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        QString val = r.is_ok() ? r.value() : def;
        int idx = cb->findText(val);
        if (idx >= 0)
            cb->setCurrentIndex(idx);
    };

    auto load_check = [&](QCheckBox* cb, const QString& key, bool def) {
        if (!cb) return;
        auto r = repo.get(key);
        cb->setChecked(!r.is_ok() ? def : r.value() != "false");
    };

    load_combo(app_font_size_,   "appearance.font_size",   kDefaultFontSize);
    load_combo(app_font_family_, "appearance.font_family", kDefaultFontFamily);
    load_combo(app_theme_,       "appearance.theme",       kDefaultTheme);
    load_combo(app_density_,     "appearance.density",     kDefaultDensity);

    load_check(chat_bubble_toggle_, "appearance.show_chat_bubble", true);
    load_check(ticker_bar_toggle_,  "appearance.show_ticker_bar",  true);
    load_check(animations_toggle_,  "appearance.animations",       true);

    // Restore custom accent color if set
    auto r_accent = repo.get("appearance.accent_color");
    if (r_accent.is_ok() && !r_accent.value().isEmpty()) {
        custom_accent_color_ = r_accent.value();
        if (accent_color_btn_) {
            accent_color_btn_->setStyleSheet(
                QString("QPushButton { background: %1; color: %2; border: none; "
                        "padding: 0 12px; height: 32px; }")
                .arg(custom_accent_color_,
                     QString(fincept::ui::ThemeManager::instance().tokens().bg_base)));
        }
    }
}

// ── Notifications ─────────────────────────────────────────────────────────────
//
// Full per-provider accordion UI.  Each provider gets a collapsible card with
// its credential fields, an enable toggle, and a "Test Send" button.
// Below the provider list: alert trigger checkboxes.

// Provider-specific field definitions ──────────────────────────────────────────
namespace {

struct FieldDef {
    QString key;
    QString label;
    QString placeholder;
    bool    is_password{false};
};

struct ProviderDef {
    QString            id;
    QString            name;
    QString            icon;
    QVector<FieldDef>  fields;
};

const QVector<ProviderDef>& provider_defs() {
    static const QVector<ProviderDef> defs = {
        { "telegram",   "Telegram",     "✈",  {
            {"bot_token", "Bot Token",   "Enter bot token from @BotFather", true},
            {"chat_id",   "Chat ID",     "e.g. 123456789"},
        }},
        { "discord",    "Discord",      "🎮", {
            {"webhook_url", "Webhook URL", "https://discord.com/api/webhooks/..."},
        }},
        { "slack",      "Slack",        "💬", {
            {"webhook_url", "Webhook URL", "https://hooks.slack.com/services/..."},
            {"channel",     "Channel",     "#alerts (optional)"},
        }},
        { "email",      "Email",        "📧", {
            {"smtp_host",  "SMTP Host",    "smtp.gmail.com"},
            {"smtp_port",  "Port",         "587"},
            {"smtp_user",  "Username",     "you@gmail.com"},
            {"smtp_pass",  "Password",     "App password", true},
            {"to_addr",    "To Address",   "recipient@example.com"},
            {"from_addr",  "From Address", "you@gmail.com (optional)"},
        }},
        { "whatsapp",   "WhatsApp",     "📱", {
            {"account_sid",  "Account SID",  "Twilio Account SID"},
            {"auth_token",   "Auth Token",   "Twilio Auth Token", true},
            {"from_number",  "From Number",  "+14155238886"},
            {"to_number",    "To Number",    "+1XXXXXXXXXX"},
        }},
        { "pushover",   "Pushover",     "🔔", {
            {"api_token", "API Token",  "Your Pushover app token", true},
            {"user_key",  "User Key",   "Your Pushover user key",  true},
        }},
        { "ntfy",       "ntfy",         "📢", {
            {"server_url", "Server URL", "https://ntfy.sh (leave empty for default)"},
            {"topic",      "Topic",      "your-topic-name"},
            {"token",      "Auth Token", "Optional auth token", true},
        }},
        { "pushbullet", "Pushbullet",   "🔵", {
            {"api_key",     "API Key",     "Your Pushbullet API key", true},
            {"channel_tag", "Channel Tag", "Optional channel tag"},
        }},
        { "gotify",     "Gotify",       "🔊", {
            {"server_url", "Server URL", "https://your-gotify-server.com"},
            {"app_token",  "App Token",  "Application token", true},
        }},
        { "mattermost", "Mattermost",   "🟦", {
            {"webhook_url", "Webhook URL", "https://your-mattermost.com/hooks/..."},
            {"channel",     "Channel",     "#town-square (optional)"},
            {"username",    "Username",    "Fincept (optional)"},
        }},
        { "teams",      "MS Teams",     "🟪", {
            {"webhook_url", "Webhook URL", "https://outlook.office.com/webhook/..."},
        }},
        { "webhook",    "Webhook",      "🌐", {
            {"url",    "URL",    "https://your-endpoint.com/notify"},
            {"method", "Method", "POST"},
        }},
        { "pagerduty",  "PagerDuty",    "🚨", {
            {"routing_key", "Routing Key", "32-character integration key", true},
        }},
        { "opsgenie",   "Opsgenie",     "🔴", {
            {"api_key", "API Key", "Your Opsgenie API key", true},
        }},
        { "sms",        "SMS (Twilio)", "💬", {
            {"account_sid",  "Account SID",  "Twilio Account SID"},
            {"auth_token",   "Auth Token",   "Twilio Auth Token", true},
            {"from_number",  "From Number",  "+15005550006"},
            {"to_number",    "To Number",    "+1XXXXXXXXXX"},
        }},
    };
    return defs;
}

} // anonymous namespace

QWidget* SettingsScreen::build_notifications() {
    using namespace fincept::notifications;

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea { border: none; background: transparent; }"
                "QScrollBar:vertical { background: %1; width: 6px; }"
                "QScrollBar::handle:vertical { background: %2; border-radius: 3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_MED()));

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(6);

    auto* hdr_lbl = new QLabel("NOTIFICATION PROVIDERS");
    hdr_lbl->setStyleSheet(section_title_ss());
    vl->addWidget(hdr_lbl);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Per-provider accordion cards ──────────────────────────────────────────
    for (const auto& def : provider_defs()) {
        ProviderWidgets pw;

        // Card outer frame
        auto* card = new QFrame;
        card->setStyleSheet(
            QString("QFrame { background: %1; border: 1px solid %2; border-radius: 3px; }")
                .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // ── Header row (icon + name + toggle) ─────────────────────────────────
        auto* head = new QWidget;
        head->setFixedHeight(36);
        head->setStyleSheet("background: transparent;");
        head->setCursor(Qt::PointingHandCursor);
        auto* hl = new QHBoxLayout(head);
        hl->setContentsMargins(10, 0, 10, 0);
        hl->setSpacing(8);

        auto* arrow_lbl = new QLabel("▶");
        arrow_lbl->setStyleSheet(
            QString("color: %1; background: transparent;")
                .arg(fincept::ui::colors::TEXT_SECONDARY()));
        hl->addWidget(arrow_lbl);

        auto* icon_lbl = new QLabel(def.icon + "  " + def.name.toUpper());
        icon_lbl->setStyleSheet(
            QString("color: %1; font-weight: bold; background: transparent;")
                .arg(fincept::ui::colors::TEXT_PRIMARY()));
        hl->addWidget(icon_lbl, 1);

        pw.enabled = new QCheckBox;
        pw.enabled->setStyleSheet(check_ss());
        hl->addWidget(pw.enabled);

        cvl->addWidget(head);

        // ── Body (credential fields + test button) ─────────────────────────────
        pw.body_frame = new QFrame;
        pw.body_frame->setStyleSheet(
            QString("QFrame { background: %1; border-top: 1px solid %2; }")
                .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_DIM()));
        auto* bvl = new QVBoxLayout(pw.body_frame);
        bvl->setContentsMargins(12, 10, 12, 10);
        bvl->setSpacing(6);

        const QString pid = def.id;

        for (const auto& fd : def.fields) {
            auto* field = new QLineEdit;
            field->setPlaceholderText(fd.placeholder);
            field->setStyleSheet(input_ss());
            if (fd.is_password)
                field->setEchoMode(QLineEdit::Password);
            pw.fields[fd.key] = field;
            bvl->addWidget(make_row(fd.label, field));
        }

        // Test button + status
        auto* test_row = new QWidget;
        test_row->setStyleSheet("background: transparent;");
        auto* thl = new QHBoxLayout(test_row);
        thl->setContentsMargins(0, 4, 0, 0);
        thl->setSpacing(8);

        pw.test_btn = new QPushButton("Test Send");
        pw.test_btn->setFixedWidth(100);
        pw.test_btn->setStyleSheet(btn_secondary_ss());

        pw.status_lbl = new QLabel;
        pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

        thl->addWidget(pw.test_btn);
        thl->addWidget(pw.status_lbl, 1);
        thl->addStretch();
        bvl->addWidget(test_row);

        cvl->addWidget(pw.body_frame);
        pw.body_frame->hide(); // collapsed by default

        // ── Expand/collapse on header click ───────────────────────────────────
        connect(head, &QWidget::destroyed, this, [](){}); // keep lifetime
        // Install an event filter to catch mouse press on the header widget
        // (QWidget doesn't have a clicked signal — use a lambda on a QPushButton overlay)
        auto* head_btn = new QPushButton(head);
        head_btn->setGeometry(0, 0, 9999, 36);  // covers full header
        head_btn->setFlat(true);
        head_btn->setStyleSheet("QPushButton { background: transparent; border: none; }");
        head_btn->raise();

        connect(head_btn, &QPushButton::clicked, this,
                [card, pw, arrow_lbl]() mutable {
                    const bool expanded = pw.body_frame->isVisible();
                    pw.body_frame->setVisible(!expanded);
                    arrow_lbl->setText(expanded ? "▶" : "▼");
                    Q_UNUSED(card);
                });

        // ── Test Send wiring ──────────────────────────────────────────────────
        connect(pw.test_btn, &QPushButton::clicked, this,
                [this, pid, pw]() mutable {
                    // Save fields first so provider has latest credentials
                    save_provider_fields(pid, pw);
                    NotificationService::instance().reload_all_configs();

                    pw.status_lbl->setText("Sending...");
                    pw.status_lbl->setStyleSheet(
                        QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

                    NotificationRequest req;
                    req.title   = "Fincept Test";
                    req.message = "This is a test notification from Fincept Terminal.";
                    req.trigger = NotifTrigger::Manual;

                    QPointer<QLabel> status_ptr = pw.status_lbl;
                    NotificationService::instance().send_to(pid, req,
                        [status_ptr](bool ok, const QString& err) {
                            if (!status_ptr) return;
                            if (ok) {
                                status_ptr->setText("✓ Sent successfully");
                                status_ptr->setStyleSheet(
                                    QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                            } else {
                                status_ptr->setText("✗ " + err.left(60));
                                status_ptr->setStyleSheet(
                                    QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                            }
                        });
                });

        provider_widgets_[def.id] = pw;
        vl->addWidget(card);
    }

    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Alert triggers ────────────────────────────────────────────────────────
    auto* trig_hdr = new QLabel("ALERT TRIGGERS");
    trig_hdr->setStyleSheet(sub_title_ss());
    vl->addWidget(trig_hdr);
    vl->addSpacing(4);

    trigger_inapp_  = new QCheckBox; trigger_inapp_->setStyleSheet(check_ss());
    trigger_price_  = new QCheckBox; trigger_price_->setStyleSheet(check_ss());
    trigger_news_   = new QCheckBox; trigger_news_->setStyleSheet(check_ss());
    trigger_orders_ = new QCheckBox; trigger_orders_->setStyleSheet(check_ss());

    vl->addWidget(make_row("In-App Alerts (toast + bell)", trigger_inapp_,
                           "Show slide-in toasts and update bell badge."));
    vl->addWidget(make_row("Price Alerts", trigger_price_,
                           "Notify when price alert thresholds are crossed."));
    vl->addWidget(make_row("News Alerts", trigger_news_,
                           "Enable news notifications (configure which types below)."));

    // ── News alert sub-options ────────────────────────────────────────────────
    news_subopts_frame_ = new QFrame;
    news_subopts_frame_->setObjectName("newsSuboptsFrame");
    news_subopts_frame_->setStyleSheet(
        "#newsSuboptsFrame { border-left: 2px solid #374151; margin-left: 24px; padding-left: 8px; }");
    auto* sub_vl = new QVBoxLayout(news_subopts_frame_);
    sub_vl->setContentsMargins(0, 4, 0, 4);
    sub_vl->setSpacing(4);

    news_breaking_   = new QCheckBox; news_breaking_->setStyleSheet(check_ss());
    news_monitors_   = new QCheckBox; news_monitors_->setStyleSheet(check_ss());
    news_deviations_ = new QCheckBox; news_deviations_->setStyleSheet(check_ss());
    news_flash_      = new QCheckBox; news_flash_->setStyleSheet(check_ss());

    sub_vl->addWidget(make_row("Breaking News", news_breaking_,
                               "Notify on FLASH/BREAKING/URGENT priority clusters."));
    sub_vl->addWidget(make_row("Monitor Keyword Matches", news_monitors_,
                               "Notify when a news monitor watch list gets new matches."));
    sub_vl->addWidget(make_row("Category Volume Spikes", news_deviations_,
                               "Notify when a category has abnormally high article volume (z-score ≥ 3)."));
    sub_vl->addWidget(make_row("FLASH + High-Impact Articles", news_flash_,
                               "Notify on individual articles that are both FLASH priority and high market impact."));

    vl->addWidget(news_subopts_frame_);

    // Show/hide sub-options based on master toggle
    connect(trigger_news_, &QCheckBox::toggled, this, [this](bool on) {
        news_subopts_frame_->setVisible(on);
    });

    vl->addWidget(make_row("Order Fill Alerts", trigger_orders_,
                           "Notify when orders are filled or rejected."));

    vl->addSpacing(16);

    // ── Save ──────────────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Save All Providers");
    save_btn->setFixedWidth(200);
    save_btn->setStyleSheet(btn_primary_ss());
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        // Save triggers
        auto& repo = SettingsRepository::instance();
        auto b = [](bool v) { return v ? "1" : "0"; };
        repo.set("notifications.inapp",           b(trigger_inapp_->isChecked()),   "notifications");
        repo.set("notifications.price_alerts",    b(trigger_price_->isChecked()),   "notifications");
        repo.set("notifications.news_alerts",     b(trigger_news_->isChecked()),    "notifications");
        repo.set("notifications.order_fills",     b(trigger_orders_->isChecked()),  "notifications");
        repo.set("notifications.news_breaking",   b(news_breaking_->isChecked()),   "notifications");
        repo.set("notifications.news_monitors",   b(news_monitors_->isChecked()),   "notifications");
        repo.set("notifications.news_deviations", b(news_deviations_->isChecked()), "notifications");
        repo.set("notifications.news_flash",      b(news_flash_->isChecked()),      "notifications");

        // Save every provider
        for (const auto& def : provider_defs())
            save_provider_fields(def.id, provider_widgets_.value(def.id));

        NotificationService::instance().reload_all_configs();
        LOG_INFO("Settings", "All notification providers saved");
    });
    vl->addWidget(save_btn);
    vl->addStretch();

    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::load_notifications() {
    using namespace fincept::notifications;

    if (provider_widgets_.isEmpty()) return;

    auto& repo = SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    // Load trigger checkboxes
    if (trigger_inapp_)  trigger_inapp_->setChecked(get_bool("notifications.inapp",        true));
    if (trigger_price_)  trigger_price_->setChecked(get_bool("notifications.price_alerts", true));
    if (trigger_orders_) trigger_orders_->setChecked(get_bool("notifications.order_fills", true));

    const bool news_on = get_bool("notifications.news_alerts", false);
    if (trigger_news_) trigger_news_->setChecked(news_on);

    // News sub-options — default all on, gate on master toggle
    if (news_breaking_)   news_breaking_->setChecked(get_bool("notifications.news_breaking",   true));
    if (news_monitors_)   news_monitors_->setChecked(get_bool("notifications.news_monitors",   true));
    if (news_deviations_) news_deviations_->setChecked(get_bool("notifications.news_deviations", true));
    if (news_flash_)      news_flash_->setChecked(get_bool("notifications.news_flash",          true));
    if (news_subopts_frame_) news_subopts_frame_->setVisible(news_on);

    // Load per-provider fields
    for (const auto& def : provider_defs()) {
        if (!provider_widgets_.contains(def.id)) continue;
        const auto& pw  = provider_widgets_[def.id];
        const QString cat = QString("notif_%1").arg(def.id);

        // Enable toggle
        if (pw.enabled) {
            auto r = repo.get(cat + ".enabled");
            pw.enabled->setChecked(r.is_ok() && r.value() == "1");
        }

        // Credential fields
        for (const auto& fd : def.fields) {
            if (!pw.fields.contains(fd.key)) continue;
            auto r = repo.get(cat + "." + fd.key);
            if (r.is_ok() && pw.fields[fd.key])
                pw.fields[fd.key]->setText(r.value());
        }
    }
}

// Helper: persist a single provider's fields from its widget set
void SettingsScreen::save_provider_fields(const QString& provider_id,
                                          const ProviderWidgets& pw) {
    auto& repo = SettingsRepository::instance();
    const QString cat = QString("notif_%1").arg(provider_id);

    if (pw.enabled)
        repo.set(cat + ".enabled", pw.enabled->isChecked() ? "1" : "0", cat);

    for (auto it = pw.fields.constBegin(); it != pw.fields.constEnd(); ++it) {
        if (it.value())
            repo.set(cat + "." + it.key(), it.value()->text().trimmed(), cat);
    }
}

// ── Storage & Cache ───────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_storage() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(10);

    auto* t = new QLabel("STORAGE & CACHE");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Cache overview card ───────────────────────────────────────────────────
    auto* t2 = new QLabel("CACHE OVERVIEW");
    t2->setStyleSheet(sub_title_ss());
    vl->addWidget(t2);
    vl->addSpacing(4);

    auto* card = new QFrame;
    card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:4px;}").arg(ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
    auto* cvl = new QVBoxLayout(card);
    cvl->setContentsMargins(16, 12, 16, 12);
    cvl->setSpacing(8);

    storage_count_ = new QLabel("—");
    storage_count_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    cvl->addWidget(make_row("Cache Entries", storage_count_));

    auto* refresh_btn = new QPushButton("Refresh");
    refresh_btn->setFixedWidth(90);
    refresh_btn->setStyleSheet(btn_secondary_ss());
    connect(refresh_btn, &QPushButton::clicked, this, [this]() { refresh_storage_stats(); });
    cvl->addWidget(refresh_btn);
    vl->addWidget(card);

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Clear all ─────────────────────────────────────────────────────────────
    auto* t3 = new QLabel("CLEAR CACHE");
    t3->setStyleSheet(sub_title_ss());
    vl->addWidget(t3);
    vl->addSpacing(4);

    auto* clear_all = new QPushButton("Clear All Cache");
    clear_all->setStyleSheet(btn_danger_ss());
    connect(clear_all, &QPushButton::clicked, this, [this]() {
        CacheManager::instance().clear();
        refresh_storage_stats();
        LOG_INFO("Settings", "All cache cleared");
    });
    vl->addWidget(clear_all);

    auto* note = new QLabel("Clears all cached market data, quotes, and news. "
                            "Live data will be re-fetched on next access.");
    note->setWordWrap(true);
    note->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(note);

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Clear by category ─────────────────────────────────────────────────────
    auto* t4 = new QLabel("CLEAR BY CATEGORY");
    t4->setStyleSheet(sub_title_ss());
    vl->addWidget(t4);
    vl->addSpacing(4);

    static const QStringList CACHE_CATS = {"market_data", "news", "quotes", "charts", "general"};
    for (const QString& cat : CACHE_CATS) {
        auto* btn = new QPushButton("Clear  " + cat);
        btn->setStyleSheet(btn_secondary_ss());
        btn->setFixedWidth(200);
        connect(btn, &QPushButton::clicked, this, [this, cat]() {
            CacheManager::instance().clear_category(cat);
            refresh_storage_stats();
            LOG_INFO("Settings", "Cache cleared: " + cat);
        });
        vl->addWidget(btn);
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::refresh_storage_stats() {
    if (storage_count_)
        storage_count_->setText(QString::number(CacheManager::instance().entry_count()) + " entries");
}

// ── Data Sources ──────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_data_sources() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        QString("QScrollArea{border:none;background:transparent;}"
                "QScrollBar:vertical{width:6px;background:transparent;}"
                "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
        .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    auto* t = new QLabel("DATA SOURCES");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info = new QLabel("Toggle which external data providers are active. "
                            "Changes take effect immediately.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(16);
    vl->addWidget(make_sep());
    vl->addSpacing(16);

    // Load from repository
    auto result = DataSourceRepository::instance().list_all();
    if (!result.is_ok() || result.value().isEmpty()) {
        auto* empty = new QLabel("No data sources configured.\n"
                                 "Sources are seeded automatically at application startup.");
        empty->setWordWrap(true);
        empty->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        vl->addWidget(empty);
        vl->addStretch();
        scroll->setWidget(page);
        return scroll;
    }

    // Group by category
    QMap<QString, QVector<DataSource>> grouped;
    for (const auto& ds : result.value())
        grouped[ds.category.isEmpty() ? "General" : ds.category].append(ds);

    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        // Category header
        auto* cat_lbl = new QLabel(it.key().toUpper());
        cat_lbl->setStyleSheet(sub_title_ss());
        vl->addWidget(cat_lbl);
        vl->addSpacing(6);

        for (const auto& ds : it.value()) {
            // Source card
            auto* card = new QFrame;
            card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:4px;}").arg(ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
            auto* cvl2 = new QVBoxLayout(card);
            cvl2->setContentsMargins(0, 0, 0, 0);
            cvl2->setSpacing(0);

            // Card header row
            auto* hdr = new QWidget;
            hdr->setFixedHeight(40);
            hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(),ui::colors::BORDER_DIM()));
            auto* hhl = new QHBoxLayout(hdr);
            hhl->setContentsMargins(12, 0, 12, 0);

            auto* name_lbl = new QLabel(ds.display_name.isEmpty() ? ds.alias : ds.display_name);
            name_lbl->setStyleSheet(QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
            hhl->addWidget(name_lbl);

            // Type badge
            auto* type_badge = new QLabel(ds.type == "websocket" ? "WS" : "REST");
            type_badge->setStyleSheet(
                QString("color:%1;font-weight:700;background:%2;border:1px solid %3;border-radius:2px;padding:1px 5px;")
                .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
            hhl->addWidget(type_badge);
            hhl->addStretch();

            // Enable toggle
            auto* toggle = new QCheckBox;
            toggle->setChecked(ds.enabled);
            toggle->setStyleSheet(check_ss());
            hhl->addWidget(toggle);

            // Wire toggle → repo
            QString source_id = ds.id;
            connect(toggle, &QCheckBox::toggled, this, [source_id](bool checked) {
                DataSourceRepository::instance().set_enabled(source_id, checked);
                LOG_INFO("DataSources", (checked ? "Enabled: " : "Disabled: ") + source_id);
            });

            cvl2->addWidget(hdr);

            // Description (if any)
            if (!ds.description.isEmpty()) {
                auto* desc = new QLabel(ds.description);
                desc->setWordWrap(true);
                desc->setStyleSheet(QString("color:%1;padding:8px 12px;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
                cvl2->addWidget(desc);
            }

            vl->addWidget(card);
            vl->addSpacing(8);
        }
        vl->addSpacing(8);
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

// ── LLM Config / MCP Servers ──────────────────────────────────────────────────

QWidget* SettingsScreen::build_llm_config() {
    return new LlmConfigSection;
}

QWidget* SettingsScreen::build_mcp_servers() {
    return new McpServersSection;
}

// ── Logging ───────────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_logging() {
    static const QStringList LEVEL_NAMES = {"Debug", "Info", "Warn", "Error"};

    auto level_to_idx = [](const QString& s) -> int {
        const QStringList names = {"Debug", "Info", "Warn", "Error"};
        int i = names.indexOf(s);
        return i >= 0 ? i : 1; // default Info
    };

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget;
    auto* vl   = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(16);

    // Title
    auto* t = new QLabel("LOGGING");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());

    // ── Global level ─────────────────────────────────────────────────────────
    auto* global_lbl = new QLabel("Global Log Level");
    global_lbl->setStyleSheet(sub_title_ss());
    vl->addWidget(global_lbl);

    auto* global_desc = new QLabel("Minimum level for all tags unless overridden.");
    global_desc->setStyleSheet(label_ss());
    global_desc->setWordWrap(true);
    vl->addWidget(global_desc);

    log_global_level_ = new QComboBox;
    log_global_level_->addItems(LEVEL_NAMES);
    log_global_level_->setStyleSheet(combo_ss());
    log_global_level_->setFixedWidth(160);

    const QString saved_global = AppConfig::instance().get("log/global_level", "Info").toString();
    log_global_level_->setCurrentIndex(level_to_idx(saved_global));

    vl->addWidget(log_global_level_);
    vl->addWidget(make_sep());

    // ── Per-tag overrides ────────────────────────────────────────────────────
    auto* tag_title = new QLabel("Per-Tag Overrides");
    tag_title->setStyleSheet(sub_title_ss());
    vl->addWidget(tag_title);

    auto* tag_desc = new QLabel("Override the log level for a specific tag (e.g. ExchangeService, AgentService).");
    tag_desc->setStyleSheet(label_ss());
    tag_desc->setWordWrap(true);
    vl->addWidget(tag_desc);

    // Container for tag rows
    log_tag_list_ = new QWidget;
    log_tag_layout_ = new QVBoxLayout(log_tag_list_);
    log_tag_layout_->setContentsMargins(0, 0, 0, 0);
    log_tag_layout_->setSpacing(6);

    // Load persisted tag overrides
    auto& cfg = AppConfig::instance();
    const int count = cfg.get("log/tag_count", 0).toInt();
    auto add_tag_row = [&](const QString& tag, const QString& level) {
        auto* row  = new QWidget;
        auto* rl   = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* tag_edit = new QLineEdit(tag);
        tag_edit->setPlaceholderText("Tag name");
        tag_edit->setFixedWidth(220);
        tag_edit->setStyleSheet(input_ss());

        auto* lvl_combo = new QComboBox;
        lvl_combo->addItems(LEVEL_NAMES);
        lvl_combo->setStyleSheet(combo_ss());
        lvl_combo->setFixedWidth(120);
        lvl_combo->setCurrentIndex(level_to_idx(level));

        auto* del_btn = new QPushButton("Remove");
        del_btn->setStyleSheet(btn_danger_ss());
        del_btn->setFixedHeight(28);
        connect(del_btn, &QPushButton::clicked, this, [row]() {
            row->deleteLater();
        });

        rl->addWidget(tag_edit);
        rl->addWidget(lvl_combo);
        rl->addWidget(del_btn);
        rl->addStretch();
        log_tag_layout_->addWidget(row);
    };

    for (int i = 0; i < count; ++i) {
        const QString tag   = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
        const QString level = cfg.get(QString("log/tag_%1_level").arg(i)).toString();
        if (!tag.isEmpty())
            add_tag_row(tag, level);
    }

    vl->addWidget(log_tag_list_);

    // Add row button
    auto* add_btn = new QPushButton("+ Add Tag Override");
    add_btn->setStyleSheet(btn_secondary_ss());
    add_btn->setFixedHeight(30);
    add_btn->setFixedWidth(180);
    connect(add_btn, &QPushButton::clicked, this, [this, add_tag_row]() mutable {
        add_tag_row({}, "Info");
    });
    vl->addWidget(add_btn);
    vl->addWidget(make_sep());

    // ── Save button ──────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Apply & Save");
    save_btn->setStyleSheet(btn_primary_ss());
    save_btn->setFixedHeight(34);
    save_btn->setFixedWidth(140);

    connect(save_btn, &QPushButton::clicked, this, [this]() {
        auto& cfg = AppConfig::instance();
        auto& log = Logger::instance();

        // Save + apply global level
        const QString gl = log_global_level_->currentText();
        cfg.set("log/global_level", gl);
        const QHash<QString, LogLevel> lvl_map = {
            {"Debug", LogLevel::Debug}, {"Info",  LogLevel::Info},
            {"Warn",  LogLevel::Warn},  {"Error", LogLevel::Error}
        };
        log.set_level(lvl_map.value(gl, LogLevel::Info));

        // Save + apply per-tag overrides
        log.clear_all_tag_levels();
        const auto rows = log_tag_list_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        int saved = 0;
        for (auto* row : rows) {
            auto* tag_edit  = row->findChild<QLineEdit*>();
            auto* lvl_combo = row->findChild<QComboBox*>();
            if (!tag_edit || !lvl_combo) continue;
            const QString tag   = tag_edit->text().trimmed();
            const QString level = lvl_combo->currentText();
            if (tag.isEmpty()) continue;
            cfg.set(QString("log/tag_%1_name").arg(saved),  tag);
            cfg.set(QString("log/tag_%1_level").arg(saved), level);
            log.set_tag_level(tag, lvl_map.value(level, LogLevel::Info));
            ++saved;
        }
        cfg.set("log/tag_count", saved);

        LOG_INFO("Settings", QString("Logging config saved: global=%1, tags=%2").arg(gl).arg(saved));
    });

    vl->addWidget(save_btn);
    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

} // namespace fincept::screens
