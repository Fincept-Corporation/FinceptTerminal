// SettingsScreen.cpp — full-featured settings (Credentials, Appearance,
// Notifications, Storage & Cache, Data Sources, LLM Config, MCP Servers)

#include "screens/settings/SettingsScreen.h"

#include "ai_chat/LlmService.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/config/ProfileManager.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/devtools/DataHubInspector.h"
#include "screens/settings/KeybindingsSection.h"
#include "screens/settings/LlmConfigSection.h"
#include "screens/settings/McpServersSection.h"
#include "screens/settings/PythonEnvSection.h"
#include "services/notifications/NotificationService.h"
#include "storage/StorageManager.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/DataSourceRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "storage/sqlite/CacheDatabase.h"
#include "storage/sqlite/Database.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Token-based style builders (live — reflect active theme) ─────────────────

// All helpers read live ThemeManager tokens — no font-size overrides (global QSS handles font)
static QString section_title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;").arg(ui::colors::AMBER());
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
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER());
}
static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER());
}
static QString btn_primary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:none;font-weight:700;padding:0 16px;height:32px;}"
                   "QPushButton:hover{background:%3;}")
        .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::AMBER_DIM());
}
static QString btn_secondary_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 12px;height:32px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_BRIGHT(), ui::colors::BG_HOVER());
}
static QString btn_danger_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;}"
                   "QPushButton:hover{background:%4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE(), ui::colors::BORDER_DIM(), ui::colors::BG_HOVER());
}
static QString combo_ss() {
    return QString(
               "QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 8px;min-width:160px;}"
               "QComboBox:focus{border:1px solid %4;}"
               "QComboBox::drop-down{border:none;width:20px;}"
               "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%5;border:1px solid %3;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER(),
             ui::colors::BG_HOVER());
}
static QString check_ss() {
    return QString("QCheckBox{color:%1;background:transparent;}"
                   "QCheckBox::indicator{width:14px;height:14px;}"
                   "QCheckBox::indicator:unchecked{border:1px solid %2;background:%3;}"
                   "QCheckBox::indicator:checked{border:1px solid %4;background:%4;}")
        .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_BRIGHT(), ui::colors::BG_RAISED(), ui::colors::AMBER());
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
    nav_ = new QWidget(this);
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
    sections_->addWidget(build_security());      // 8
    sections_->addWidget(build_profiles());      // 9
    sections_->addWidget(build_keybindings());   // 10
    sections_->addWidget(build_python_env());    // 11
    sections_->addWidget(build_developer());     // 12

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
            ScreenStateManager::instance().notify_changed(this);
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
    make_btn("Security", 8);
    make_btn("Profiles", 9);
    make_btn("Keybindings", 10);
    make_btn("Python Env", 11);
    make_btn("Developer", 12);

    first->setChecked(true);

    nvl->addStretch();
    root->addWidget(nav);
    root->addWidget(sections_, 1);

    // Wire LLM config changes → reload AI chat service
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5))) {
        connect(llm, &LlmConfigSection::config_changed, this,
                []() { ai_chat::LlmService::instance().reload_config(); });
    }

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void SettingsScreen::refresh_theme() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (nav_)
        nav_->setStyleSheet(QString("background:%1;border-right:1px solid %2;")
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
    load_security();
    refresh_storage_stats();
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5)))
        llm->reload();
}

// ── Shared helpers ────────────────────────────────────────────────────────────

QWidget* SettingsScreen::make_row(const QString& label, QWidget* control, const QString& description) {
    auto* row = new QWidget(nullptr);
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
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
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
        card->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;border-radius:4px;}")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // Card header
        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(34);
        hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(
            QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        hhl->addWidget(name_lbl);
        hhl->addStretch();

        // Status label — persisted across load_credentials calls
        auto* status_lbl = new QLabel("Not set");
        status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        hhl->addWidget(status_lbl);
        cred_status_[key] = status_lbl;

        cvl->addWidget(hdr);

        // Card body: input + save button
        auto* body = new QWidget(this);
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
                status_lbl->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
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
constexpr const char* kDefaultFontSize = "14px";
constexpr const char* kDefaultFontFamily = "Consolas";
constexpr const char* kDefaultDensity = "Default";
} // namespace

QWidget* SettingsScreen::build_appearance() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_MED()));

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
        auto& tm = fincept::ui::ThemeManager::instance();
        int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
        if (px > 0)
            tm.apply_font(app_font_family_->currentText(), px);
        tm.apply_density(app_density_->currentText());
    });

    auto restart_debounce = [this]() { appearance_debounce_->start(); };
    connect(app_font_size_, &QComboBox::currentTextChanged, this, restart_debounce);
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
        auto& tm = fincept::ui::ThemeManager::instance();

        // Persist all values
        repo.set("appearance.font_size", app_font_size_->currentText(), "appearance");
        repo.set("appearance.font_family", app_font_family_->currentText(), "appearance");
        repo.set("appearance.density", app_density_->currentText(), "appearance");
        repo.set("appearance.show_chat_bubble", chat_bubble_toggle_->isChecked() ? "true" : "false", "appearance");
        repo.set("appearance.show_ticker_bar", ticker_bar_toggle_->isChecked() ? "true" : "false", "appearance");
        repo.set("appearance.animations", animations_toggle_->isChecked() ? "true" : "false", "appearance");

        // Flush any pending debounce immediately on save
        if (appearance_debounce_->isActive()) {
            appearance_debounce_->stop();
            int px = QString(app_font_size_->currentText()).replace("px", "").toInt();
            if (px <= 0)
                px = 14;
            tm.apply_font(app_font_family_->currentText(), px);
            tm.apply_density(app_density_->currentText());
        }

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
    const QSignalBlocker b4(app_density_);

    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        QString val = r.is_ok() ? r.value() : def;
        int idx = cb->findText(val);
        if (idx >= 0)
            cb->setCurrentIndex(idx);
    };

    auto load_check = [&](QCheckBox* cb, const QString& key, bool def) {
        if (!cb)
            return;
        auto r = repo.get(key);
        cb->setChecked(!r.is_ok() ? def : r.value() != "false");
    };

    load_combo(app_font_size_, "appearance.font_size", kDefaultFontSize);
    load_combo(app_font_family_, "appearance.font_family", kDefaultFontFamily);
    load_combo(app_density_, "appearance.density", kDefaultDensity);

    load_check(chat_bubble_toggle_, "appearance.show_chat_bubble", true);
    load_check(ticker_bar_toggle_, "appearance.show_ticker_bar", true);
    load_check(animations_toggle_, "appearance.animations", true);
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
    bool is_password{false};
};

struct ProviderDef {
    QString id;
    QString name;
    QString icon;
    QVector<FieldDef> fields;
};

const QVector<ProviderDef>& provider_defs() {
    static const QVector<ProviderDef> defs = {
        {"telegram",
         "Telegram",
         "✈",
         {
             {"bot_token", "Bot Token", "Enter bot token from @BotFather", true},
             {"chat_id", "Chat ID", "e.g. 123456789"},
         }},
        {"discord",
         "Discord",
         "🎮",
         {
             {"webhook_url", "Webhook URL", "https://discord.com/api/webhooks/..."},
         }},
        {"slack",
         "Slack",
         "💬",
         {
             {"webhook_url", "Webhook URL", "https://hooks.slack.com/services/..."},
             {"channel", "Channel", "#alerts (optional)"},
         }},
        {"email",
         "Email",
         "📧",
         {
             {"smtp_host", "SMTP Host", "smtp.gmail.com"},
             {"smtp_port", "Port", "587"},
             {"smtp_user", "Username", "you@gmail.com"},
             {"smtp_pass", "Password", "App password", true},
             {"to_addr", "To Address", "recipient@example.com"},
             {"from_addr", "From Address", "you@gmail.com (optional)"},
         }},
        {"whatsapp",
         "WhatsApp",
         "📱",
         {
             {"account_sid", "Account SID", "Twilio Account SID"},
             {"auth_token", "Auth Token", "Twilio Auth Token", true},
             {"from_number", "From Number", "+14155238886"},
             {"to_number", "To Number", "+1XXXXXXXXXX"},
         }},
        {"pushover",
         "Pushover",
         "🔔",
         {
             {"api_token", "API Token", "Your Pushover app token", true},
             {"user_key", "User Key", "Your Pushover user key", true},
         }},
        {"ntfy",
         "ntfy",
         "📢",
         {
             {"server_url", "Server URL", "https://ntfy.sh (leave empty for default)"},
             {"topic", "Topic", "your-topic-name"},
             {"token", "Auth Token", "Optional auth token", true},
         }},
        {"pushbullet",
         "Pushbullet",
         "🔵",
         {
             {"api_key", "API Key", "Your Pushbullet API key", true},
             {"channel_tag", "Channel Tag", "Optional channel tag"},
         }},
        {"gotify",
         "Gotify",
         "🔊",
         {
             {"server_url", "Server URL", "https://your-gotify-server.com"},
             {"app_token", "App Token", "Application token", true},
         }},
        {"mattermost",
         "Mattermost",
         "🟦",
         {
             {"webhook_url", "Webhook URL", "https://your-mattermost.com/hooks/..."},
             {"channel", "Channel", "#town-square (optional)"},
             {"username", "Username", "Fincept (optional)"},
         }},
        {"teams",
         "MS Teams",
         "🟪",
         {
             {"webhook_url", "Webhook URL", "https://outlook.office.com/webhook/..."},
         }},
        {"webhook",
         "Webhook",
         "🌐",
         {
             {"url", "URL", "https://your-endpoint.com/notify"},
             {"method", "Method", "POST"},
         }},
        {"pagerduty",
         "PagerDuty",
         "🚨",
         {
             {"routing_key", "Routing Key", "32-character integration key", true},
         }},
        {"opsgenie",
         "Opsgenie",
         "🔴",
         {
             {"api_key", "API Key", "Your Opsgenie API key", true},
         }},
        {"sms",
         "SMS (Twilio)",
         "💬",
         {
             {"account_sid", "Account SID", "Twilio Account SID"},
             {"auth_token", "Auth Token", "Twilio Auth Token", true},
             {"from_number", "From Number", "+15005550006"},
             {"to_number", "To Number", "+1XXXXXXXXXX"},
         }},
    };
    return defs;
}

} // anonymous namespace

QWidget* SettingsScreen::build_notifications() {
    using namespace fincept::notifications;

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; border-radius: 3px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(fincept::ui::colors::BG_SURFACE(), fincept::ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
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
        card->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 3px; }")
                                .arg(fincept::ui::colors::BG_RAISED(), fincept::ui::colors::BORDER_DIM()));
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // ── Header row (icon + name + toggle) ─────────────────────────────────
        auto* head = new QWidget(this);
        head->setFixedHeight(36);
        head->setStyleSheet("background: transparent;");
        head->setCursor(Qt::PointingHandCursor);
        auto* hl = new QHBoxLayout(head);
        hl->setContentsMargins(10, 0, 10, 0);
        hl->setSpacing(8);

        auto* arrow_lbl = new QLabel("▶");
        arrow_lbl->setStyleSheet(
            QString("color: %1; background: transparent;").arg(fincept::ui::colors::TEXT_SECONDARY()));
        hl->addWidget(arrow_lbl);

        auto* icon_lbl = new QLabel(def.icon + "  " + def.name.toUpper());
        icon_lbl->setStyleSheet(
            QString("color: %1; font-weight: bold; background: transparent;").arg(fincept::ui::colors::TEXT_PRIMARY()));
        hl->addWidget(icon_lbl, 1);

        pw.enabled = new QCheckBox;
        pw.enabled->setStyleSheet(check_ss());
        hl->addWidget(pw.enabled);

        cvl->addWidget(head);

        // ── Body (credential fields + test button) ─────────────────────────────
        pw.body_frame = new QFrame;
        pw.body_frame->setStyleSheet(QString("QFrame { background: %1; border-top: 1px solid %2; }")
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
        auto* test_row = new QWidget(this);
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
        connect(head, &QWidget::destroyed, this, []() {}); // keep lifetime
        // Install an event filter to catch mouse press on the header widget
        // (QWidget doesn't have a clicked signal — use a lambda on a QPushButton overlay)
        auto* head_btn = new QPushButton(head);
        head_btn->setGeometry(0, 0, 9999, 36); // covers full header
        head_btn->setFlat(true);
        head_btn->setStyleSheet("QPushButton { background: transparent; border: none; }");
        head_btn->raise();

        connect(head_btn, &QPushButton::clicked, this, [card, pw, arrow_lbl]() mutable {
            const bool expanded = pw.body_frame->isVisible();
            pw.body_frame->setVisible(!expanded);
            arrow_lbl->setText(expanded ? "▶" : "▼");
            Q_UNUSED(card);
        });

        // ── Test Send wiring ──────────────────────────────────────────────────
        connect(pw.test_btn, &QPushButton::clicked, this, [this, pid, pw]() mutable {
            // Save fields first so provider has latest credentials
            save_provider_fields(pid, pw);
            NotificationService::instance().reload_all_configs();

            pw.status_lbl->setText("Sending...");
            pw.status_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));

            NotificationRequest req;
            req.title = "Fincept Test";
            req.message = "This is a test notification from Fincept Terminal.";
            req.trigger = NotifTrigger::Manual;

            QPointer<QLabel> status_ptr = pw.status_lbl;
            NotificationService::instance().send_to(pid, req, [status_ptr](bool ok, const QString& err) {
                if (!status_ptr)
                    return;
                if (ok) {
                    status_ptr->setText("✓ Sent successfully");
                    status_ptr->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                } else {
                    status_ptr->setText("✗ " + err.left(60));
                    status_ptr->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
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

    trigger_inapp_ = new QCheckBox;
    trigger_inapp_->setStyleSheet(check_ss());
    trigger_price_ = new QCheckBox;
    trigger_price_->setStyleSheet(check_ss());
    trigger_news_ = new QCheckBox;
    trigger_news_->setStyleSheet(check_ss());
    trigger_orders_ = new QCheckBox;
    trigger_orders_->setStyleSheet(check_ss());

    vl->addWidget(
        make_row("In-App Alerts (toast + bell)", trigger_inapp_, "Show slide-in toasts and update bell badge."));
    vl->addWidget(make_row("Price Alerts", trigger_price_, "Notify when price alert thresholds are crossed."));
    vl->addWidget(make_row("News Alerts", trigger_news_, "Enable news notifications (configure which types below)."));

    // ── News alert sub-options ────────────────────────────────────────────────
    news_subopts_frame_ = new QFrame;
    news_subopts_frame_->setObjectName("newsSuboptsFrame");
    news_subopts_frame_->setStyleSheet(
        QString("#newsSuboptsFrame { border-left: 2px solid %1; margin-left: 24px; padding-left: 8px; }")
            .arg(ui::colors::BORDER_BRIGHT()));
    auto* sub_vl = new QVBoxLayout(news_subopts_frame_);
    sub_vl->setContentsMargins(0, 4, 0, 4);
    sub_vl->setSpacing(4);

    news_breaking_ = new QCheckBox;
    news_breaking_->setStyleSheet(check_ss());
    news_monitors_ = new QCheckBox;
    news_monitors_->setStyleSheet(check_ss());
    news_deviations_ = new QCheckBox;
    news_deviations_->setStyleSheet(check_ss());
    news_flash_ = new QCheckBox;
    news_flash_->setStyleSheet(check_ss());

    sub_vl->addWidget(make_row("Breaking News", news_breaking_, "Notify on FLASH/BREAKING/URGENT priority clusters."));
    sub_vl->addWidget(
        make_row("Monitor Keyword Matches", news_monitors_, "Notify when a news monitor watch list gets new matches."));
    sub_vl->addWidget(make_row("Category Volume Spikes", news_deviations_,
                               "Notify when a category has abnormally high article volume (z-score ≥ 3)."));
    sub_vl->addWidget(make_row("FLASH + High-Impact Articles", news_flash_,
                               "Notify on individual articles that are both FLASH priority and high market impact."));

    vl->addWidget(news_subopts_frame_);

    // Show/hide sub-options based on master toggle
    connect(trigger_news_, &QCheckBox::toggled, this, [this](bool on) { news_subopts_frame_->setVisible(on); });

    vl->addWidget(make_row("Order Fill Alerts", trigger_orders_, "Notify when orders are filled or rejected."));

    vl->addSpacing(16);

    // ── Save ──────────────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Save All Providers");
    save_btn->setFixedWidth(200);
    save_btn->setStyleSheet(btn_primary_ss());
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        // Save triggers
        auto& repo = SettingsRepository::instance();
        auto b = [](bool v) { return v ? "1" : "0"; };
        repo.set("notifications.inapp", b(trigger_inapp_->isChecked()), "notifications");
        repo.set("notifications.price_alerts", b(trigger_price_->isChecked()), "notifications");
        repo.set("notifications.news_alerts", b(trigger_news_->isChecked()), "notifications");
        repo.set("notifications.order_fills", b(trigger_orders_->isChecked()), "notifications");
        repo.set("notifications.news_breaking", b(news_breaking_->isChecked()), "notifications");
        repo.set("notifications.news_monitors", b(news_monitors_->isChecked()), "notifications");
        repo.set("notifications.news_deviations", b(news_deviations_->isChecked()), "notifications");
        repo.set("notifications.news_flash", b(news_flash_->isChecked()), "notifications");

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

    if (provider_widgets_.isEmpty())
        return;

    auto& repo = SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() && !r.value().isEmpty() ? (r.value() == "1") : def;
    };

    // Load trigger checkboxes
    if (trigger_inapp_)
        trigger_inapp_->setChecked(get_bool("notifications.inapp", true));
    if (trigger_price_)
        trigger_price_->setChecked(get_bool("notifications.price_alerts", true));
    if (trigger_orders_)
        trigger_orders_->setChecked(get_bool("notifications.order_fills", true));

    const bool news_on = get_bool("notifications.news_alerts", false);
    if (trigger_news_)
        trigger_news_->setChecked(news_on);

    // News sub-options — default all on, gate on master toggle
    if (news_breaking_)
        news_breaking_->setChecked(get_bool("notifications.news_breaking", true));
    if (news_monitors_)
        news_monitors_->setChecked(get_bool("notifications.news_monitors", true));
    if (news_deviations_)
        news_deviations_->setChecked(get_bool("notifications.news_deviations", true));
    if (news_flash_)
        news_flash_->setChecked(get_bool("notifications.news_flash", true));
    if (news_subopts_frame_)
        news_subopts_frame_->setVisible(news_on);

    // Load per-provider fields
    for (const auto& def : provider_defs()) {
        if (!provider_widgets_.contains(def.id))
            continue;
        const auto& pw = provider_widgets_[def.id];
        const QString cat = QString("notif_%1").arg(def.id);

        // Enable toggle
        if (pw.enabled) {
            auto r = repo.get(cat + ".enabled");
            pw.enabled->setChecked(r.is_ok() && r.value() == "1");
        }

        // Credential fields
        for (const auto& fd : def.fields) {
            if (!pw.fields.contains(fd.key))
                continue;
            auto r = repo.get(cat + "." + fd.key);
            if (r.is_ok() && pw.fields[fd.key])
                pw.fields[fd.key]->setText(r.value());
        }
    }
}

// Helper: persist a single provider's fields from its widget set
void SettingsScreen::save_provider_fields(const QString& provider_id, const ProviderWidgets& pw) {
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

static QString format_bytes(qint64 bytes) {
    if (bytes < 1024)
        return QString::number(bytes) + " B";
    if (bytes < 1048576)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1073741824)
        return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    return QString::number(bytes / 1073741824.0, 'f', 2) + " GB";
}

// Obsidian-standard panel with 34px header bar
static QFrame* make_panel(const QString& title, QWidget* status_widget = nullptr) {
    auto* panel = new QFrame;
    panel->setStyleSheet(
        QString("QFrame{background:%1;border:1px solid %2;}").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(0, 0, 0, 0);
    pvl->setSpacing(0);

    auto* hdr = new QWidget(nullptr);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 12, 0);

    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;").arg(ui::colors::AMBER()));
    hhl->addWidget(lbl);
    hhl->addStretch();
    if (status_widget) {
        status_widget->setStyleSheet(status_widget->styleSheet() + "background:transparent;");
        hhl->addWidget(status_widget);
    }
    pvl->addWidget(hdr);

    return panel;
}

// Obsidian-standard data row: LABEL ······ VALUE, 26px, bottom border
static QWidget* make_data_row(const QString& label_text, QLabel* value_lbl, bool alt_bg = false) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt_bg ? ui::colors::ROW_ALT() : "transparent", ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(0);

    auto* lbl = new QLabel(label_text);
    lbl->setStyleSheet(QString("color:%1;font-weight:600;letter-spacing:0.5px;background:transparent;")
                           .arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(lbl);
    hl->addStretch();

    value_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_lbl->setStyleSheet(
        QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(value_lbl);

    return row;
}

// Category row: NAME ··· COUNT ··· [CLEAR]  — 26px, terminal table style
static QWidget* make_category_row(const QString& name, QLabel* count_lbl, QPushButton* clear_btn, bool alt) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(lbl, 1);

    count_lbl->setObjectName("cat_count");
    count_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    count_lbl->setFixedWidth(70);
    count_lbl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::CYAN()));
    hl->addWidget(count_lbl);

    clear_btn->setFixedSize(56, 20);
    clear_btn->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                "QPushButton:hover{background:%1;color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
    hl->addWidget(clear_btn);

    return row;
}

// Group header row for category tables — amber dim label
static QWidget* make_group_header(const QString& title) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    auto* lbl = new QLabel(title);
    lbl->setStyleSheet(
        QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;").arg(ui::colors::AMBER_DIM()));
    hl->addWidget(lbl);
    return row;
}

// File action row: LABEL ··· SIZE ··· [BUTTON]
static QWidget* make_file_action_row(const QString& name, QLabel* size_lbl, QPushButton* btn, bool alt) {
    auto* row = new QWidget(nullptr);
    row->setFixedHeight(26);
    row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                           .arg(alt ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);

    auto* lbl = new QLabel(name);
    lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(lbl, 1);

    size_lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    size_lbl->setFixedWidth(80);
    size_lbl->setStyleSheet(
        QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(size_lbl);

    btn->setFixedSize(56, 20);
    btn->setStyleSheet(
        QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                "QPushButton:hover{background:%1;color:%2;}")
            .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
    hl->addWidget(btn);

    return row;
}

QWidget* SettingsScreen::build_storage() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // Title
    auto* t = new QLabel("STORAGE & DATA MANAGEMENT");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info = new QLabel("Manage all persistent data, databases, and files. "
                            "Execute SQL queries directly against terminal databases.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(10);

    // ═══════════════════════════════════════════════════════════════════════════
    // SECTION 1: DISK USAGE — stat boxes + data rows
    // ═══════════════════════════════════════════════════════════════════════════
    {
        auto* refresh_lbl = new QLabel("REFRESH");
        refresh_lbl->setStyleSheet(QString("color:%1;font-weight:600;cursor:pointer;").arg(ui::colors::AMBER()));
        refresh_lbl->setCursor(Qt::PointingHandCursor);
        refresh_lbl->installEventFilter(this); // for click

        auto* panel = make_panel("DISK USAGE", refresh_lbl);
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        // Stat boxes row
        auto* stat_row = new QWidget(this);
        stat_row->setStyleSheet("background:transparent;");
        auto* shl = new QHBoxLayout(stat_row);
        shl->setContentsMargins(10, 10, 10, 10);
        shl->setSpacing(10);

        auto make_stat_box = [&](const QString& label, QLabel*& val_out) {
            auto* box = new QFrame;
            box->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* bx = new QVBoxLayout(box);
            bx->setContentsMargins(12, 12, 12, 14);
            bx->setSpacing(4);
            bx->setAlignment(Qt::AlignCenter);

            val_out = new QLabel("—");
            val_out->setAlignment(Qt::AlignCenter);
            val_out->setStyleSheet(
                QString("color:%1;font-size:18px;font-weight:700;background:transparent;").arg(ui::colors::CYAN()));
            bx->addWidget(val_out);

            auto* ll = new QLabel(label);
            ll->setAlignment(Qt::AlignCenter);
            ll->setStyleSheet(
                QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
            bx->addWidget(ll);

            shl->addWidget(box, 1);
        };

        make_stat_box("MAIN DB", storage_main_db_);
        make_stat_box("CACHE DB", storage_cache_db_);
        make_stat_box("LOG FILES", storage_log_size_);
        make_stat_box("WORKSPACES", storage_ws_size_);
        make_stat_box("TOTAL", storage_total_size_);

        bvl->addWidget(stat_row);

        // Detail rows
        storage_count_ = new QLabel("—");
        bvl->addWidget(make_data_row("Cache Entries", storage_count_, false));

        // Connect refresh click
        connect(refresh_lbl, &QLabel::linkActivated, this, [this]() { refresh_storage_stats(); });
        // Since QLabel doesn't emit linkActivated for plain text, use eventFilter
        // Actually, let's just use a proper clickable mechanism:
        auto* refresh_btn = new QPushButton("Refresh");
        refresh_btn->setFixedSize(70, 22);
        refresh_btn->setStyleSheet(
            QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%3;}")
                .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));

        connect(refresh_btn, &QPushButton::clicked, this, [this]() { refresh_storage_stats(); });

        // Replace the header status widget
        if (auto* hdr_layout = panel->findChild<QWidget*>()->findChild<QHBoxLayout*>()) {
            hdr_layout->removeWidget(refresh_lbl);
            refresh_lbl->deleteLater();
            hdr_layout->addWidget(refresh_btn);
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ═══════════════════════════════════════════════════════════════════════════
    // SECTION 2: DATA CATEGORIES — grouped table with counts + clear
    // ═══════════════════════════════════════════════════════════════════════════
    {
        auto* panel = make_panel("DATA CATEGORIES");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");

        storage_categories_ = new QVBoxLayout(body);
        storage_categories_->setContentsMargins(0, 0, 0, 0);
        storage_categories_->setSpacing(0);

        // Table header
        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(8);
        auto* th1 = new QLabel("CATEGORY");
        th1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th1, 1);
        auto* th2 = new QLabel("ENTRIES");
        th2->setFixedWidth(70);
        th2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        th2->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th2);
        auto* th3 = new QLabel("ACTION");
        th3->setFixedWidth(56);
        th3->setAlignment(Qt::AlignCenter);
        th3->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th3);
        storage_categories_->addWidget(th);

        auto& sm = StorageManager::instance();
        auto stats = sm.all_stats();

        QString current_group;
        int row_idx = 0;
        for (const auto& cat : stats) {
            if (cat.group != current_group) {
                current_group = cat.group;
                storage_categories_->addWidget(make_group_header(cat.group.toUpper()));
                row_idx = 0;
            }

            auto* count_lbl = new QLabel(QString::number(cat.count));
            auto* clear_btn = new QPushButton("CLR");

            QString cat_id = cat.id;
            QString cat_label = cat.label;
            connect(clear_btn, &QPushButton::clicked, this, [this, cat_id, cat_label, count_lbl]() {
                auto answer = QMessageBox::warning(this, "Clear " + cat_label,
                                                   "Permanently delete all " + cat_label.toLower() +
                                                       "?\n\n"
                                                       "This cannot be undone.",
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                    return;

                auto r = StorageManager::instance().clear_category(cat_id);
                if (r.is_ok()) {
                    count_lbl->setText("0");
                    LOG_INFO("Settings", "Cleared: " + cat_label);
                } else {
                    QMessageBox::critical(this, "Error",
                                          "Failed to clear " + cat_label + ":\n" + QString::fromStdString(r.error()));
                }
                refresh_storage_stats();
            });

            storage_categories_->addWidget(make_category_row(cat.label, count_lbl, clear_btn, (row_idx % 2) == 1));
            ++row_idx;
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ═══════════════════════════════════════════════════════════════════════════
    // SECTION 3: FILE & STATE MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    {
        auto* panel = make_panel("FILE & STATE MANAGEMENT");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        // Table header
        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(8);
        auto* th1 = new QLabel("STORE");
        th1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th1, 1);
        auto* th2 = new QLabel("SIZE");
        th2->setFixedWidth(80);
        th2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        th2->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th2);
        auto* th3 = new QLabel("ACTION");
        th3->setFixedWidth(56);
        th3->setAlignment(Qt::AlignCenter);
        th3->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
        thl->addWidget(th3);
        bvl->addWidget(th);

        // Helper to create file rows with confirmation
        auto add_file_row = [&](const QString& name, QLabel* size_lbl, const QString& confirm_title,
                                const QString& confirm_msg, std::function<void()> action, bool alt) {
            auto* btn = new QPushButton("CLR");
            connect(btn, &QPushButton::clicked, this, [this, confirm_title, confirm_msg, action]() {
                auto answer = QMessageBox::warning(this, confirm_title, confirm_msg,
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes)
                    return;
                action();
                refresh_storage_stats();
            });
            bvl->addWidget(make_file_action_row(name, size_lbl, btn, alt));
        };

        auto* log_sz = new QLabel("—");
        auto* ws_sz = new QLabel("—");
        auto* qs_lbl = new QLabel("Registry");

        add_file_row(
            "Log Files", log_sz, "Clear Logs", "Clear all application log files?\nCurrent log data will be lost.",
            [this]() {
                StorageManager::instance().clear_log_files();
                LOG_INFO("Settings", "Logs cleared");
            },
            false);

        add_file_row(
            "Workspace Files (.fwsp)", ws_sz, "Delete Workspaces",
            "Delete all saved workspace files?\nThis cannot be undone.",
            [this]() {
                StorageManager::instance().clear_workspace_files();
                LOG_INFO("Settings", "Workspaces deleted");
            },
            true);

        add_file_row(
            "Window & UI State", qs_lbl, "Reset UI State",
            "Reset all window positions, dock layouts, and perspectives?\n"
            "Takes effect on next restart.",
            [this]() {
                StorageManager::instance().clear_qsettings();
                LOG_INFO("Settings", "QSettings cleared");
            },
            false);

        // Cache subcategories as inline buttons
        auto* cache_row = new QWidget(this);
        cache_row->setFixedHeight(30);
        cache_row->setStyleSheet(
            QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::ROW_ALT(), ui::colors::BORDER_DIM()));
        auto* chl = new QHBoxLayout(cache_row);
        chl->setContentsMargins(12, 0, 12, 0);
        chl->setSpacing(4);
        auto* cache_label = new QLabel("Cache:");
        cache_label->setStyleSheet(
            QString("color:%1;font-weight:600;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        chl->addWidget(cache_label);

        static const QStringList CACHE_CATS = {"market_data", "news", "quotes", "charts", "general"};
        for (const QString& cat : CACHE_CATS) {
            auto* btn = new QPushButton(cat);
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 6px;}"
                                       "QPushButton:hover{color:%4;border-color:%4;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                                        ui::colors::AMBER()));
            connect(btn, &QPushButton::clicked, this, [this, cat]() {
                CacheManager::instance().clear_category(cat);
                refresh_storage_stats();
                LOG_INFO("Settings", "Cache cleared: " + cat);
            });
            chl->addWidget(btn);
        }
        chl->addStretch();
        bvl->addWidget(cache_row);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ═══════════════════════════════════════════════════════════════════════════
    // SECTION 4: SQL CONSOLE — direct database access
    // ═══════════════════════════════════════════════════════════════════════════
    {
        auto* panel = make_panel("SQL CONSOLE");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(6);

        auto* hint = new QLabel("Execute SQL queries directly against terminal databases. "
                                "Use SELECT to inspect data, or INSERT/UPDATE/DELETE to modify.");
        hint->setWordWrap(true);
        hint->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(hint);

        // DB selector + input + execute
        auto* input_row = new QWidget(this);
        input_row->setStyleSheet("background:transparent;");
        auto* irl = new QHBoxLayout(input_row);
        irl->setContentsMargins(0, 0, 0, 0);
        irl->setSpacing(6);

        sql_db_selector_ = new QComboBox;
        sql_db_selector_->addItem("fincept.db", "main");
        sql_db_selector_->addItem("cache.db", "cache");
        sql_db_selector_->setFixedWidth(110);
        sql_db_selector_->setStyleSheet(combo_ss());
        irl->addWidget(sql_db_selector_);

        sql_input_ = new QLineEdit;
        sql_input_->setPlaceholderText("SELECT * FROM chat_sessions LIMIT 10");
        sql_input_->setStyleSheet(input_ss());
        irl->addWidget(sql_input_, 1);

        auto* exec_btn = new QPushButton("EXEC");
        exec_btn->setFixedSize(56, 28);
        exec_btn->setStyleSheet(
            QString("QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%3;}")
                .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
        irl->addWidget(exec_btn);

        bvl->addWidget(input_row);

        // Status line
        sql_status_ = new QLabel("Ready");
        sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(sql_status_);

        // Results area — scrollable table
        auto* results_scroll = new QScrollArea;
        results_scroll->setWidgetResizable(true);
        results_scroll->setFixedHeight(200);
        results_scroll->setStyleSheet(
            QString("QScrollArea{border:1px solid %1;background:%2;}"
                    "QScrollBar:vertical{width:5px;background:transparent;}"
                    "QScrollBar::handle:vertical{background:%3;}"
                    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
                    "QScrollBar:horizontal{height:5px;background:transparent;}"
                    "QScrollBar::handle:horizontal{background:%3;}"
                    "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}")
                .arg(ui::colors::BORDER_DIM(), ui::colors::BG_BASE(), ui::colors::BORDER_MED()));

        auto* results_widget = new QWidget(this);
        results_widget->setStyleSheet("background:transparent;");
        sql_results_layout_ = new QVBoxLayout(results_widget);
        sql_results_layout_->setContentsMargins(0, 0, 0, 0);
        sql_results_layout_->setSpacing(0);
        sql_results_layout_->addStretch();
        results_scroll->setWidget(results_widget);
        bvl->addWidget(results_scroll);

        // Quick-access table list buttons
        auto* tables_row = new QWidget(this);
        tables_row->setStyleSheet("background:transparent;");
        auto* trhl = new QHBoxLayout(tables_row);
        trhl->setContentsMargins(0, 0, 0, 0);
        trhl->setSpacing(4);
        auto* trl = new QLabel("Quick:");
        trl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        trhl->addWidget(trl);

        static const QStringList QUICK_TABLES = {"chat_sessions", "news_articles", "financial_notes", "portfolios",
                                                 "watchlists",    "pt_portfolios", "workflows",       "settings"};
        for (const QString& tbl : QUICK_TABLES) {
            auto* btn = new QPushButton(tbl);
            btn->setFixedHeight(18);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:0 4px;}"
                                       "QPushButton:hover{color:%4;border-color:%4;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                                        ui::colors::AMBER()));
            connect(btn, &QPushButton::clicked, this, [this, tbl]() {
                sql_input_->setText("SELECT * FROM " + tbl + " LIMIT 20");
                sql_db_selector_->setCurrentIndex(0); // main db
            });
            trhl->addWidget(btn);
        }
        trhl->addStretch();
        bvl->addWidget(tables_row);

        // Execute handler
        auto execute_sql = [this]() {
            QString sql = sql_input_->text().trimmed();
            if (sql.isEmpty())
                return;

            // Clear previous results
            while (sql_results_layout_->count() > 0) {
                auto* item = sql_results_layout_->takeAt(0);
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }

            bool use_cache = (sql_db_selector_->currentData().toString() == "cache");

            // Determine if this is a write operation
            QString upper = sql.toUpper().trimmed();
            bool is_write = upper.startsWith("INSERT") || upper.startsWith("UPDATE") || upper.startsWith("DELETE") ||
                            upper.startsWith("DROP") || upper.startsWith("ALTER") || upper.startsWith("CREATE");

            if (is_write) {
                // Confirm write operations
                auto answer = QMessageBox::warning(this, "Execute Write Query",
                                                   "This will modify the database:\n\n" + sql + "\n\nContinue?",
                                                   QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                if (answer != QMessageBox::Yes) {
                    sql_status_->setText("Cancelled");
                    sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::WARNING()));
                    return;
                }
            }

            // Execute
            QSqlQuery query(use_cache ? CacheDatabase::instance().raw_db() : Database::instance().raw_db());
            if (!query.exec(sql)) {
                sql_status_->setText("Error: " + query.lastError().text());
                sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::NEGATIVE()));
                LOG_ERROR("SQL Console", "Query failed: " + query.lastError().text());
                return;
            }

            if (is_write) {
                int affected = query.numRowsAffected();
                sql_status_->setText(QString("OK — %1 row(s) affected").arg(affected));
                sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
                refresh_storage_stats();
                LOG_INFO("SQL Console", QString("Write query: %1 rows affected").arg(affected));
                return;
            }

            // Build results table for SELECT
            auto rec = query.record();
            int cols = rec.count();
            if (cols == 0) {
                sql_status_->setText("OK — no columns returned");
                sql_status_->setStyleSheet(
                    QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
                return;
            }

            // Column header
            auto* hdr_row = new QWidget(this);
            hdr_row->setFixedHeight(22);
            hdr_row->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* hhl = new QHBoxLayout(hdr_row);
            hhl->setContentsMargins(6, 0, 6, 0);
            hhl->setSpacing(4);
            for (int c = 0; c < cols; ++c) {
                auto* cl = new QLabel(rec.fieldName(c));
                cl->setStyleSheet(
                    QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
                hhl->addWidget(cl, 1);
            }
            sql_results_layout_->addWidget(hdr_row);

            // Data rows (limit 100)
            int row_count = 0;
            while (query.next() && row_count < 100) {
                auto* dr = new QWidget(this);
                dr->setFixedHeight(20);
                dr->setStyleSheet(
                    QString("background:%1;border-bottom:1px solid %2;")
                        .arg((row_count % 2) ? ui::colors::ROW_ALT() : "transparent", ui::colors::BORDER_DIM()));
                auto* dhl = new QHBoxLayout(dr);
                dhl->setContentsMargins(6, 0, 6, 0);
                dhl->setSpacing(4);
                for (int c = 0; c < cols; ++c) {
                    QString val = query.value(c).toString();
                    if (val.length() > 60)
                        val = val.left(57) + "...";
                    auto* vl2 = new QLabel(val);
                    vl2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
                    dhl->addWidget(vl2, 1);
                }
                sql_results_layout_->addWidget(dr);
                ++row_count;
            }

            sql_status_->setText(
                QString("OK — %1 row(s) returned%2").arg(row_count).arg(row_count >= 100 ? " (limited to 100)" : ""));
            sql_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::POSITIVE()));
        };

        connect(exec_btn, &QPushButton::clicked, this, execute_sql);
        connect(sql_input_, &QLineEdit::returnPressed, this, execute_sql);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ═══════════════════════════════════════════════════════════════════════════
    // SECTION 5: DANGER ZONE
    // ═══════════════════════════════════════════════════════════════════════════
    {
        auto* panel = new QFrame;
        panel->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                 .arg(ui::colors::BG_SURFACE(), ui::colors::NEGATIVE_DIM()));
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        // Red header
        auto* hdr = new QWidget(this);
        hdr->setFixedHeight(34);
        hdr->setStyleSheet(
            QString("background:rgba(220,38,38,0.08);border-bottom:1px solid %1;").arg(ui::colors::NEGATIVE_DIM()));
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* hlbl = new QLabel("DANGER ZONE");
        hlbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                                .arg(ui::colors::NEGATIVE()));
        hhl->addWidget(hlbl);
        pvl->addWidget(hdr);

        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(12, 10, 12, 10);
        bvl->setSpacing(8);

        // Clear all cache
        auto* cache_row = new QWidget(this);
        cache_row->setStyleSheet("background:transparent;");
        auto* cr_hl = new QHBoxLayout(cache_row);
        cr_hl->setContentsMargins(0, 0, 0, 0);
        cr_hl->setSpacing(8);
        auto* cache_desc = new QWidget(this);
        auto* cd_vl = new QVBoxLayout(cache_desc);
        cd_vl->setContentsMargins(0, 0, 0, 0);
        cd_vl->setSpacing(2);
        auto* cd1 = new QLabel("Clear All Cache");
        cd1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
        cd_vl->addWidget(cd1);
        auto* cd2 = new QLabel("Delete all temporary cached data. Will be re-fetched on next access.");
        cd2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        cd_vl->addWidget(cd2);
        cr_hl->addWidget(cache_desc, 1);
        auto* cache_btn = new QPushButton("CLEAR CACHE");
        cache_btn->setFixedSize(110, 26);
        cache_btn->setStyleSheet(
            QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%2;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
        connect(cache_btn, &QPushButton::clicked, this, [this]() {
            auto answer = QMessageBox::warning(
                this, "Clear All Cache", "Delete all temporary cached data?\nData will be re-fetched on next access.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes)
                return;
            CacheManager::instance().clear();
            refresh_storage_stats();
            LOG_INFO("Settings", "All cache cleared");
        });
        cr_hl->addWidget(cache_btn);
        bvl->addWidget(cache_row);

        bvl->addWidget(make_sep());

        // Nuclear: clear ALL data
        auto* nuke_row = new QWidget(this);
        nuke_row->setStyleSheet("background:transparent;");
        auto* nr_hl = new QHBoxLayout(nuke_row);
        nr_hl->setContentsMargins(0, 0, 0, 0);
        nr_hl->setSpacing(8);
        auto* nuke_desc = new QWidget(this);
        auto* nd_vl = new QVBoxLayout(nuke_desc);
        nd_vl->setContentsMargins(0, 0, 0, 0);
        nd_vl->setSpacing(2);
        auto* nd1 = new QLabel("Clear ALL User Data");
        nd1->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::NEGATIVE()));
        nd_vl->addWidget(nd1);
        auto* nd2 =
            new QLabel("Permanently delete all databases, files, cache, and UI state. OS keychain is preserved.");
        nd2->setWordWrap(true);
        nd2->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        nd_vl->addWidget(nd2);
        nr_hl->addWidget(nuke_desc, 1);
        auto* nuke_btn = new QPushButton("DELETE ALL");
        nuke_btn->setFixedSize(110, 26);
        nuke_btn->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:2px solid %1;font-weight:700;}"
                                        "QPushButton:hover{background:%2;color:%3;}")
                                    .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE()));
        connect(nuke_btn, &QPushButton::clicked, this, [this]() {
            auto a1 = QMessageBox::critical(this, "Clear ALL User Data",
                                            "WARNING: This will permanently delete ALL data:\n\n"
                                            "  Chat history, notes, reports, watchlists\n"
                                            "  Portfolios, transactions, paper trades\n"
                                            "  Workflows, dashboard layouts\n"
                                            "  News articles, RSS feeds, monitors\n"
                                            "  Data sources, MCP servers\n"
                                            "  Agent configs, LLM configs & profiles\n"
                                            "  App settings, credentials, key-value storage\n"
                                            "  All cache, log files, workspaces, UI state\n\n"
                                            "OS keychain credentials are NOT affected.\n"
                                            "This action CANNOT be undone.",
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a1 != QMessageBox::Yes)
                return;

            auto a2 = QMessageBox::critical(this, "Final Confirmation",
                                            "ALL data will be permanently deleted.\nAre you absolutely sure?",
                                            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (a2 != QMessageBox::Yes)
                return;

            auto& sm = StorageManager::instance();
            for (const auto& info : sm.all_stats())
                sm.clear_category(info.id);
            sm.clear_log_files();
            sm.clear_workspace_files();
            sm.clear_qsettings();
            refresh_storage_stats();
            LOG_INFO("Settings", "ALL user data cleared");
        });
        nr_hl->addWidget(nuke_btn);
        bvl->addWidget(nuke_row);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::refresh_storage_stats() {
    auto& sm = StorageManager::instance();

    // Stat boxes
    if (storage_main_db_)
        storage_main_db_->setText(format_bytes(sm.main_db_size()));
    if (storage_cache_db_)
        storage_cache_db_->setText(format_bytes(sm.cache_db_size()));
    if (storage_log_size_)
        storage_log_size_->setText(format_bytes(sm.log_files_size()));
    if (storage_ws_size_)
        storage_ws_size_->setText(format_bytes(sm.workspace_files_size()));
    if (storage_total_size_)
        storage_total_size_->setText(
            format_bytes(sm.main_db_size() + sm.cache_db_size() + sm.log_files_size() + sm.workspace_files_size()));

    // Cache entry count
    if (storage_count_)
        storage_count_->setText(QString::number(CacheManager::instance().entry_count()));

    // Update per-category counts
    if (storage_categories_) {
        auto stats = sm.all_stats();
        int stat_idx = 0;
        for (int i = 0; i < storage_categories_->count() && stat_idx < stats.size(); ++i) {
            auto* item = storage_categories_->itemAt(i);
            if (!item || !item->widget())
                continue;
            // Find count label by object name
            auto* count_lbl = item->widget()->findChild<QLabel*>("cat_count");
            if (!count_lbl)
                continue;
            count_lbl->setText(QString::number(stats[stat_idx].count));
            ++stat_idx;
        }
    }
}

// ── Data Sources ──────────────────────────────────────────────────────────────

// Transport type badge text from data source type string
static QString ds_transport_badge(const QString& type) {
    if (type == "websocket")
        return "WS";
    if (type == "rest_api")
        return "REST";
    if (type == "sql")
        return "SQL";
    return type.left(4).toUpper();
}

QWidget* SettingsScreen::build_data_sources() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // Title + Open Full Screen button
    auto* title_row = new QWidget(this);
    title_row->setStyleSheet("background:transparent;");
    auto* trl = new QHBoxLayout(title_row);
    trl->setContentsMargins(0, 0, 0, 0);
    auto* t = new QLabel("DATA SOURCES");
    t->setStyleSheet(section_title_ss());
    trl->addWidget(t);
    trl->addStretch();

    auto* open_full = new QPushButton("OPEN FULL SCREEN");
    open_full->setFixedHeight(24);
    open_full->setCursor(Qt::PointingHandCursor);
    open_full->setStyleSheet(
        QString(
            "QPushButton{background:rgba(217,119,6,0.1);color:%1;border:1px solid %2;font-weight:700;padding:0 10px;}"
            "QPushButton:hover{background:%1;color:%3;}")
            .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE()));
    connect(open_full, &QPushButton::clicked, this,
            []() { EventBus::instance().publish("nav.switch_screen", {{"screen", "data_sources"}}); });
    trl->addWidget(open_full);
    vl->addWidget(title_row);

    auto* info = new QLabel("Quick management of configured connections. "
                            "For full browsing, adding, testing, and import/export use the full screen.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(info);
    vl->addSpacing(4);

    // ── Stats strip ──────────────────────────────────────────────────────────
    auto result = DataSourceRepository::instance().list_all();
    auto connections = result.is_ok() ? result.value() : QVector<DataSource>{};

    int total = connections.size();
    int active = 0;
    QSet<QString> providers;
    QMap<QString, int> cat_counts;
    for (const auto& ds : connections) {
        if (ds.enabled)
            ++active;
        providers.insert(ds.provider);
        QString cat = ds.category.isEmpty() ? "other" : ds.category;
        cat_counts[cat]++;
    }

    {
        auto* stat_row = new QWidget(this);
        stat_row->setStyleSheet("background:transparent;");
        auto* shl = new QHBoxLayout(stat_row);
        shl->setContentsMargins(0, 0, 0, 0);
        shl->setSpacing(8);

        auto make_stat = [&](const QString& label, int value, const QString& color) {
            auto* box = new QFrame;
            box->setStyleSheet(QString("QFrame{background:%1;border:1px solid %2;}")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* bx = new QVBoxLayout(box);
            bx->setContentsMargins(10, 8, 10, 10);
            bx->setSpacing(2);
            bx->setAlignment(Qt::AlignCenter);

            auto* val = new QLabel(QString::number(value));
            val->setAlignment(Qt::AlignCenter);
            val->setStyleSheet(QString("color:%1;font-size:18px;font-weight:700;background:transparent;").arg(color));
            bx->addWidget(val);

            auto* ll = new QLabel(label);
            ll->setAlignment(Qt::AlignCenter);
            ll->setStyleSheet(
                QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
            bx->addWidget(ll);

            shl->addWidget(box, 1);
        };

        make_stat("TOTAL", total, ui::colors::CYAN());
        make_stat("ACTIVE", active, ui::colors::POSITIVE());
        make_stat("INACTIVE", total - active, ui::colors::TEXT_DIM());
        make_stat("PROVIDERS", providers.size(), ui::colors::AMBER());
        vl->addWidget(stat_row);
    }

    vl->addSpacing(6);

    // ── Connections table ────────────────────────────────────────────────────
    if (connections.isEmpty()) {
        auto* empty_panel = make_panel("CONNECTIONS");
        auto* empty_body = new QWidget(this);
        empty_body->setStyleSheet("background:transparent;");
        auto* evl = new QVBoxLayout(empty_body);
        evl->setContentsMargins(12, 16, 12, 16);
        auto* elbl =
            new QLabel("No data sources configured. Open the full Data Sources screen to browse and add connectors.");
        elbl->setWordWrap(true);
        elbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
        evl->addWidget(elbl);
        static_cast<QVBoxLayout*>(empty_panel->layout())->addWidget(empty_body);
        vl->addWidget(empty_panel);
    } else {
        // Group by category
        QMap<QString, QVector<DataSource>> grouped;
        for (const auto& ds : connections) {
            QString cat = ds.category.isEmpty() ? "other" : ds.category;
            grouped[cat].append(ds);
        }

        auto* panel = make_panel("CONNECTIONS");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(0, 0, 0, 0);
        bvl->setSpacing(0);

        // Table header
        auto* th = new QWidget(this);
        th->setFixedHeight(26);
        th->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                              .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* thl = new QHBoxLayout(th);
        thl->setContentsMargins(12, 0, 12, 0);
        thl->setSpacing(6);

        auto add_th = [&](const QString& text, int width = 0) {
            auto* lbl = new QLabel(text);
            lbl->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;").arg(ui::colors::TEXT_DIM()));
            if (width > 0) {
                lbl->setFixedWidth(width);
                lbl->setAlignment(Qt::AlignCenter);
            }
            thl->addWidget(lbl, width > 0 ? 0 : 1);
        };
        add_th("SOURCE");
        add_th("TYPE", 44);
        add_th("CATEGORY", 80);
        add_th("ON", 30);
        add_th("DEL", 30);
        bvl->addWidget(th);

        int row_idx = 0;
        for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
            // Category group header
            auto* grp = new QWidget(this);
            grp->setFixedHeight(24);
            grp->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
            auto* ghl = new QHBoxLayout(grp);
            ghl->setContentsMargins(12, 0, 12, 0);
            auto* glbl = new QLabel(it.key().toUpper() + QString("  (%1)").arg(it.value().size()));
            glbl->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                                    .arg(ui::colors::AMBER_DIM()));
            ghl->addWidget(glbl);
            bvl->addWidget(grp);

            for (const auto& ds : it.value()) {
                auto* row = new QWidget(this);
                row->setFixedHeight(26);
                row->setStyleSheet(
                    QString("background:%1;border-bottom:1px solid %2;")
                        .arg((row_idx % 2) ? ui::colors::ROW_ALT() : ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));

                auto* rhl = new QHBoxLayout(row);
                rhl->setContentsMargins(12, 0, 12, 0);
                rhl->setSpacing(6);

                // Name
                QString name = ds.display_name.isEmpty() ? ds.alias : ds.display_name;
                auto* name_lbl = new QLabel(name);
                name_lbl->setStyleSheet(
                    QString("color:%1;background:transparent;")
                        .arg(ds.enabled ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
                rhl->addWidget(name_lbl, 1);

                // Type badge
                auto* badge = new QLabel(ds_transport_badge(ds.type));
                badge->setFixedWidth(44);
                badge->setAlignment(Qt::AlignCenter);
                badge->setStyleSheet(
                    QString("color:%1;font-weight:700;background:%2;border:1px solid %3;")
                        .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
                rhl->addWidget(badge);

                // Category
                auto* cat_lbl = new QLabel(ds.category.isEmpty() ? "—" : ds.category);
                cat_lbl->setFixedWidth(80);
                cat_lbl->setAlignment(Qt::AlignCenter);
                cat_lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_TERTIARY()));
                rhl->addWidget(cat_lbl);

                // Enable toggle
                auto* toggle = new QCheckBox;
                toggle->setChecked(ds.enabled);
                toggle->setFixedWidth(30);
                toggle->setStyleSheet(check_ss());
                QString source_id = ds.id;
                connect(toggle, &QCheckBox::toggled, this, [source_id, name_lbl](bool checked) {
                    DataSourceRepository::instance().set_enabled(source_id, checked);
                    name_lbl->setStyleSheet(
                        QString("color:%1;background:transparent;")
                            .arg(checked ? ui::colors::TEXT_PRIMARY() : ui::colors::TEXT_TERTIARY()));
                    LOG_INFO("DataSources", (checked ? "Enabled: " : "Disabled: ") + source_id);
                });
                rhl->addWidget(toggle);

                // Delete button
                auto* del_btn = new QPushButton("X");
                del_btn->setFixedSize(22, 18);
                del_btn->setCursor(Qt::PointingHandCursor);
                del_btn->setStyleSheet(
                    QString("QPushButton{background:transparent;color:%1;border:none;font-weight:700;}"
                            "QPushButton:hover{color:%2;}")
                        .arg(ui::colors::TEXT_DIM(), ui::colors::NEGATIVE()));
                connect(del_btn, &QPushButton::clicked, this, [this, source_id, name, row]() {
                    auto answer = QMessageBox::warning(this, "Delete Connection",
                                                       "Delete connection \"" + name + "\"?\n\nThis cannot be undone.",
                                                       QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
                    if (answer != QMessageBox::Yes)
                        return;

                    DataSourceRepository::instance().remove(source_id);
                    row->hide();
                    LOG_INFO("DataSources", "Deleted: " + source_id);
                });
                rhl->addWidget(del_btn);

                bvl->addWidget(row);
                ++row_idx;
            }
        }

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addSpacing(10);

    // ── Bulk actions ─────────────────────────────────────────────────────────
    {
        auto* panel = make_panel("BULK ACTIONS");
        auto* body = new QWidget(this);
        body->setStyleSheet("background:transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(10, 8, 10, 8);
        bvl->setSpacing(6);

        auto* btn_row = new QWidget(this);
        btn_row->setStyleSheet("background:transparent;");
        auto* brhl = new QHBoxLayout(btn_row);
        brhl->setContentsMargins(0, 0, 0, 0);
        brhl->setSpacing(8);

        // Enable all
        auto* enable_all = new QPushButton("ENABLE ALL");
        enable_all->setFixedHeight(24);
        enable_all->setStyleSheet(QString("QPushButton{background:rgba(22,163,74,0.1);color:%1;border:1px solid "
                                          "%1;font-weight:700;padding:0 10px;}"
                                          "QPushButton:hover{background:%1;color:%2;}")
                                      .arg(ui::colors::POSITIVE(), ui::colors::BG_BASE()));
        connect(enable_all, &QPushButton::clicked, this, [this]() {
            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok())
                return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().set_enabled(ds.id, true);
            LOG_INFO("DataSources", "All sources enabled");
            // Rebuild section to reflect changes
            if (sections_) {
                auto* old = sections_->widget(4);
                auto* fresh = build_data_sources();
                sections_->insertWidget(4, fresh);
                sections_->removeWidget(old);
                old->deleteLater();
                sections_->setCurrentIndex(4);
            }
        });
        brhl->addWidget(enable_all);

        // Disable all
        auto* disable_all = new QPushButton("DISABLE ALL");
        disable_all->setFixedHeight(24);
        disable_all->setStyleSheet(
            QString("QPushButton{background:rgba(220,38,38,0.1);color:%1;border:1px solid %3;font-weight:700;padding:0 "
                    "10px;}"
                    "QPushButton:hover{background:%1;color:%2;}")
                .arg(ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE_DIM()));
        connect(disable_all, &QPushButton::clicked, this, [this]() {
            auto answer = QMessageBox::warning(this, "Disable All", "Disable all data source connections?",
                                               QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes)
                return;
            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok())
                return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().set_enabled(ds.id, false);
            LOG_INFO("DataSources", "All sources disabled");
            if (sections_) {
                auto* old = sections_->widget(4);
                auto* fresh = build_data_sources();
                sections_->insertWidget(4, fresh);
                sections_->removeWidget(old);
                old->deleteLater();
                sections_->setCurrentIndex(4);
            }
        });
        brhl->addWidget(disable_all);

        // Delete all
        auto* delete_all = new QPushButton("DELETE ALL");
        delete_all->setFixedHeight(24);
        delete_all->setStyleSheet(
            QString("QPushButton{background:%1;color:%2;border:2px solid %2;font-weight:700;padding:0 10px;}"
                    "QPushButton:hover{background:%2;color:%3;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY()));
        connect(delete_all, &QPushButton::clicked, this, [this]() {
            auto answer =
                QMessageBox::critical(this, "Delete All Connections",
                                      "Permanently delete ALL data source connections?\n\n"
                                      "This cannot be undone. You can re-add them from the full Data Sources screen.",
                                      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes)
                return;

            auto r = DataSourceRepository::instance().list_all();
            if (!r.is_ok())
                return;
            for (const auto& ds : r.value())
                DataSourceRepository::instance().remove(ds.id);
            LOG_INFO("DataSources", "All connections deleted");
            if (sections_) {
                auto* old = sections_->widget(4);
                auto* fresh = build_data_sources();
                sections_->insertWidget(4, fresh);
                sections_->removeWidget(old);
                old->deleteLater();
                sections_->setCurrentIndex(4);
            }
        });
        brhl->addWidget(delete_all);

        brhl->addStretch();
        bvl->addWidget(btn_row);

        auto* note = new QLabel("For adding new connections, testing connectivity, and import/export, "
                                "use the full Data Sources screen.");
        note->setWordWrap(true);
        note->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_DIM()));
        bvl->addWidget(note);

        static_cast<QVBoxLayout*>(panel->layout())->addWidget(body);
        vl->addWidget(panel);
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
    // Full level range — matches LogLevel enum in core/logging/Logger.h.
    static const QStringList LEVEL_NAMES = {"Trace", "Debug", "Info", "Warn", "Error", "Fatal"};

    auto level_to_idx = [](const QString& s) -> int {
        int i = LEVEL_NAMES.indexOf(s);
        return i >= 0 ? i : LEVEL_NAMES.indexOf("Info"); // default Info
    };

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
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

    // ── Output format ────────────────────────────────────────────────────────
    auto* fmt_title = new QLabel("Output Format");
    fmt_title->setStyleSheet(sub_title_ss());
    vl->addWidget(fmt_title);

    auto* fmt_desc = new QLabel(
        "Plain text is human-readable; JSON emits one structured object per line (easier to parse with tooling).");
    fmt_desc->setStyleSheet(label_ss());
    fmt_desc->setWordWrap(true);
    vl->addWidget(fmt_desc);

    log_json_mode_ = new QCheckBox("Emit structured JSON lines");
    log_json_mode_->setChecked(AppConfig::instance().get("log/json_mode", false).toBool());
    vl->addWidget(log_json_mode_);
    vl->addWidget(make_sep());

    // ── Log file location ────────────────────────────────────────────────────
    auto* path_title = new QLabel("Log File");
    path_title->setStyleSheet(sub_title_ss());
    vl->addWidget(path_title);

    const QString log_path = AppPaths::logs() + "/fincept.log";
    log_path_label_ = new QLabel(QDir::toNativeSeparators(log_path));
    log_path_label_->setStyleSheet(label_ss());
    log_path_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    log_path_label_->setWordWrap(true);
    vl->addWidget(log_path_label_);

    auto* path_row = new QWidget(this);
    auto* path_rl = new QHBoxLayout(path_row);
    path_rl->setContentsMargins(0, 0, 0, 0);
    path_rl->setSpacing(8);

    auto* open_folder_btn = new QPushButton("Open Log Folder");
    open_folder_btn->setStyleSheet(btn_secondary_ss());
    open_folder_btn->setFixedHeight(30);
    open_folder_btn->setFixedWidth(160);
    connect(open_folder_btn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::logs()));
    });

    auto* copy_path_btn = new QPushButton("Copy Path");
    copy_path_btn->setStyleSheet(btn_secondary_ss());
    copy_path_btn->setFixedHeight(30);
    copy_path_btn->setFixedWidth(120);
    connect(copy_path_btn, &QPushButton::clicked, this, [this]() {
        if (log_path_label_)
            QApplication::clipboard()->setText(log_path_label_->text());
    });

    path_rl->addWidget(open_folder_btn);
    path_rl->addWidget(copy_path_btn);
    path_rl->addStretch();
    vl->addWidget(path_row);
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
    log_tag_list_ = new QWidget(this);
    log_tag_layout_ = new QVBoxLayout(log_tag_list_);
    log_tag_layout_->setContentsMargins(0, 0, 0, 0);
    log_tag_layout_->setSpacing(6);

    // Load persisted tag overrides
    auto& cfg = AppConfig::instance();
    const int count = cfg.get("log/tag_count", 0).toInt();
    auto add_tag_row = [&](const QString& tag, const QString& level) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
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
        connect(del_btn, &QPushButton::clicked, this, [row]() { row->deleteLater(); });

        rl->addWidget(tag_edit);
        rl->addWidget(lvl_combo);
        rl->addWidget(del_btn);
        rl->addStretch();
        log_tag_layout_->addWidget(row);
    };

    for (int i = 0; i < count; ++i) {
        const QString tag = cfg.get(QString("log/tag_%1_name").arg(i)).toString();
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
    connect(add_btn, &QPushButton::clicked, this, [this, add_tag_row]() mutable { add_tag_row({}, "Info"); });
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
        const QHash<QString, LogLevel> lvl_map = {{"Trace", LogLevel::Trace}, {"Debug", LogLevel::Debug},
                                                  {"Info", LogLevel::Info},   {"Warn", LogLevel::Warn},
                                                  {"Error", LogLevel::Error}, {"Fatal", LogLevel::Fatal}};
        log.set_level(lvl_map.value(gl, LogLevel::Info));

        // Save + apply JSON output mode
        const bool json_on = log_json_mode_ && log_json_mode_->isChecked();
        cfg.set("log/json_mode", json_on);
        log.set_json_mode(json_on);

        // Save + apply per-tag overrides
        log.clear_all_tag_levels();
        const auto rows = log_tag_list_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        int saved = 0;
        for (auto* row : rows) {
            auto* tag_edit = row->findChild<QLineEdit*>();
            auto* lvl_combo = row->findChild<QComboBox*>();
            if (!tag_edit || !lvl_combo)
                continue;
            const QString tag = tag_edit->text().trimmed();
            const QString level = lvl_combo->currentText();
            if (tag.isEmpty())
                continue;
            cfg.set(QString("log/tag_%1_name").arg(saved), tag);
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

// ── Security ─────────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_security() {
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

    // ── PIN STATUS ────────────────────────────────────────────────────────────
    auto* t1 = new QLabel("PIN AUTHENTICATION");
    t1->setStyleSheet(section_title_ss());
    vl->addWidget(t1);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    sec_pin_status_ = new QLabel;
    sec_pin_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_PRIMARY()));
    vl->addWidget(make_row("PIN Status", sec_pin_status_, "A 6-digit PIN is required to unlock the terminal."));

    sec_lockout_status_ = new QLabel;
    sec_lockout_status_->setStyleSheet(QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(
        make_row("Failed Attempts", sec_lockout_status_, "PIN lockout engages after 5 consecutive failures."));

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── CHANGE PIN ────────────────────────────────────────────────────────────
    auto* t2 = new QLabel("CHANGE PIN");
    t2->setStyleSheet(sub_title_ss());
    vl->addWidget(t2);
    vl->addSpacing(4);

    sec_change_pin_btn_ = new QPushButton("Change PIN");
    sec_change_pin_btn_->setFixedWidth(140);
    sec_change_pin_btn_->setStyleSheet(btn_secondary_ss());
    vl->addWidget(sec_change_pin_btn_);

    // Expandable change-PIN form (hidden by default)
    sec_change_pin_form_ = new QWidget(this);
    sec_change_pin_form_->setStyleSheet("background: transparent;");
    auto* cpfl = new QVBoxLayout(sec_change_pin_form_);
    cpfl->setContentsMargins(0, 8, 0, 0);
    cpfl->setSpacing(6);

    auto make_pin_field = [&](const QString& placeholder) {
        auto* input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setMaxLength(6);
        input->setEchoMode(QLineEdit::Password);
        input->setFixedWidth(200);
        input->setStyleSheet(input_ss());
        return input;
    };

    sec_current_pin_ = make_pin_field("Current PIN");
    cpfl->addWidget(make_row("Current PIN", sec_current_pin_));

    sec_new_pin_ = make_pin_field("New PIN");
    cpfl->addWidget(make_row("New PIN", sec_new_pin_));

    sec_confirm_pin_ = make_pin_field("Confirm PIN");
    cpfl->addWidget(make_row("Confirm PIN", sec_confirm_pin_));

    sec_pin_error_ = new QLabel;
    sec_pin_error_->setWordWrap(true);
    sec_pin_error_->setStyleSheet(
        QString("color:%1;background:rgba(220,38,38,0.08);border:1px solid %2;padding:6px 8px;")
            .arg(ui::colors::NEGATIVE(), ui::colors::NEGATIVE_DIM()));
    sec_pin_error_->hide();
    cpfl->addWidget(sec_pin_error_);

    sec_pin_success_ = new QLabel;
    sec_pin_success_->setWordWrap(true);
    sec_pin_success_->setStyleSheet(
        QString("color:%1;background:rgba(22,163,74,0.08);border:1px solid %2;padding:6px 8px;")
            .arg(ui::colors::POSITIVE(), ui::colors::POSITIVE_DIM()));
    sec_pin_success_->hide();
    cpfl->addWidget(sec_pin_success_);

    auto* save_pin_btn = new QPushButton("Update PIN");
    save_pin_btn->setFixedWidth(140);
    save_pin_btn->setStyleSheet(btn_primary_ss());
    cpfl->addWidget(save_pin_btn);

    sec_change_pin_form_->hide();
    vl->addWidget(sec_change_pin_form_);

    // Toggle form visibility
    connect(sec_change_pin_btn_, &QPushButton::clicked, this, [this]() {
        bool showing = sec_change_pin_form_->isVisible();
        sec_change_pin_form_->setVisible(!showing);
        sec_change_pin_btn_->setText(showing ? "Change PIN" : "Cancel");
        if (!showing) {
            sec_current_pin_->clear();
            sec_new_pin_->clear();
            sec_confirm_pin_->clear();
            sec_pin_error_->hide();
            sec_pin_success_->hide();
            sec_current_pin_->setFocus();
        }
    });

    // Save PIN handler
    connect(save_pin_btn, &QPushButton::clicked, this, [this]() {
        sec_pin_error_->hide();
        sec_pin_success_->hide();

        auto& pm = auth::PinManager::instance();
        const QString current = sec_current_pin_->text();
        const QString new_pin = sec_new_pin_->text();
        const QString confirm = sec_confirm_pin_->text();

        // Verify current PIN
        if (!pm.verify_pin(current)) {
            sec_pin_error_->setText("Current PIN is incorrect");
            sec_pin_error_->show();
            sec_current_pin_->clear();
            sec_current_pin_->setFocus();
            return;
        }

        // Validate new PIN
        if (new_pin.length() != 6) {
            sec_pin_error_->setText("New PIN must be exactly 6 digits");
            sec_pin_error_->show();
            return;
        }
        for (const QChar& c : new_pin) {
            if (!c.isDigit()) {
                sec_pin_error_->setText("PIN must contain only digits");
                sec_pin_error_->show();
                return;
            }
        }
        if (new_pin != confirm) {
            sec_pin_error_->setText("New PINs do not match");
            sec_pin_error_->show();
            sec_confirm_pin_->clear();
            sec_confirm_pin_->setFocus();
            return;
        }
        if (new_pin == current) {
            sec_pin_error_->setText("New PIN must be different from current PIN");
            sec_pin_error_->show();
            return;
        }

        auto result = pm.set_pin(new_pin);
        if (result.is_err()) {
            sec_pin_error_->setText(QString::fromStdString(result.error()));
            sec_pin_error_->show();
            return;
        }

        sec_pin_success_->setText("PIN updated successfully");
        sec_pin_success_->show();
        sec_current_pin_->clear();
        sec_new_pin_->clear();
        sec_confirm_pin_->clear();
        load_security();

        LOG_INFO("Settings", "PIN changed via Security settings");
    });

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── AUTO-LOCK ─────────────────────────────────────────────────────────────
    auto* t3 = new QLabel("AUTO-LOCK");
    t3->setStyleSheet(sub_title_ss());
    vl->addWidget(t3);
    vl->addSpacing(4);

    sec_autolock_toggle_ = new QCheckBox("Enable auto-lock on inactivity");
    sec_autolock_toggle_->setChecked(true);
    sec_autolock_toggle_->setStyleSheet(check_ss());
    vl->addWidget(make_row("Auto-Lock", sec_autolock_toggle_, "Locks the terminal after a period of inactivity."));

    sec_lock_timeout_ = new QComboBox;
    sec_lock_timeout_->addItem("1 min", 1);
    sec_lock_timeout_->addItem("2 min", 2);
    sec_lock_timeout_->addItem("5 min", 5);
    sec_lock_timeout_->addItem("10 min", 10);
    sec_lock_timeout_->addItem("15 min", 15);
    sec_lock_timeout_->addItem("30 min", 30);
    sec_lock_timeout_->addItem("60 min", 60);
    sec_lock_timeout_->setCurrentIndex(3); // default 10 min
    sec_lock_timeout_->setStyleSheet(combo_ss());
    vl->addWidget(make_row("Lock Timeout", sec_lock_timeout_, "Time of inactivity before the terminal locks."));

    // Enable/disable timeout combo based on toggle
    connect(sec_autolock_toggle_, &QCheckBox::toggled, this,
            [this](bool checked) { sec_lock_timeout_->setEnabled(checked); });

    vl->addSpacing(16);

    // ── SAVE ──────────────────────────────────────────────────────────────────
    auto* save_btn = new QPushButton("Save Security Settings");
    save_btn->setFixedWidth(200);
    save_btn->setStyleSheet(btn_primary_ss());
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto& guard = auth::InactivityGuard::instance();

        bool autolock = sec_autolock_toggle_->isChecked();
        int minutes = sec_lock_timeout_->currentData().toInt();

        repo.set("security.autolock_enabled", autolock ? "true" : "false", "security");
        repo.set("security.lock_timeout_minutes", QString::number(minutes), "security");

        guard.set_timeout_minutes(minutes);
        guard.set_enabled(autolock);

        LOG_INFO("Settings", QString("Security settings saved: autolock=%1, timeout=%2min").arg(autolock).arg(minutes));
    });
    vl->addWidget(save_btn);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::load_security() {
    auto& pm = auth::PinManager::instance();
    auto& repo = SettingsRepository::instance();

    // PIN status
    if (sec_pin_status_) {
        if (pm.has_pin())
            sec_pin_status_->setText("CONFIGURED");
        else
            sec_pin_status_->setText("NOT SET");
        sec_pin_status_->setStyleSheet(QString("color:%1;font-weight:700;background:transparent;")
                                           .arg(pm.has_pin() ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    }

    // Lockout status
    if (sec_lockout_status_) {
        int attempts = pm.failed_attempts();
        if (attempts == 0) {
            sec_lockout_status_->setText("0 / 5");
        } else {
            sec_lockout_status_->setText(QString("%1 / 5").arg(attempts));
        }
        sec_lockout_status_->setStyleSheet(
            QString("color:%1;font-weight:700;background:transparent;")
                .arg(attempts > 0 ? ui::colors::WARNING() : ui::colors::TEXT_SECONDARY()));
    }

    // Change PIN button visibility (only if PIN exists)
    if (sec_change_pin_btn_)
        sec_change_pin_btn_->setEnabled(pm.has_pin());

    // Auto-lock settings
    if (sec_autolock_toggle_ && sec_lock_timeout_) {
        const QSignalBlocker b1(sec_autolock_toggle_);
        const QSignalBlocker b2(sec_lock_timeout_);

        auto r_enabled = repo.get("security.autolock_enabled");
        bool enabled = !r_enabled.is_ok() || r_enabled.value() != "false"; // default true
        sec_autolock_toggle_->setChecked(enabled);
        sec_lock_timeout_->setEnabled(enabled);

        auto r_timeout = repo.get("security.lock_timeout_minutes");
        int minutes = r_timeout.is_ok() ? r_timeout.value().toInt() : 10;
        // Find matching combo index by data
        for (int i = 0; i < sec_lock_timeout_->count(); ++i) {
            if (sec_lock_timeout_->itemData(i).toInt() == minutes) {
                sec_lock_timeout_->setCurrentIndex(i);
                break;
            }
        }
    }
}

// ── Profiles ──────────────────────────────────────────────────────────────────
QWidget* SettingsScreen::build_profiles() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(16);

    auto* title = new QLabel("Profiles");
    title->setStyleSheet(section_title_ss());
    vl->addWidget(title);

    auto* desc = new QLabel("Each profile has its own isolated database, credentials, logs and workspaces.\n"
                            "Launch the terminal with  --profile <name>  to open a specific profile.\n"
                            "Different profiles can run simultaneously — useful for separate trading accounts.");
    desc->setWordWrap(true);
    desc->setStyleSheet(label_ss());
    vl->addWidget(desc);
    vl->addWidget(make_sep());

    // Active profile display
    auto& pm = ProfileManager::instance();
    auto* active_lbl = new QLabel(QString("Active profile:  <b>%1</b>").arg(pm.active()));
    active_lbl->setStyleSheet(label_ss());
    vl->addWidget(active_lbl);

    // Profile list
    auto* list_title = new QLabel("All profiles");
    list_title->setStyleSheet(sub_title_ss());
    vl->addWidget(list_title);

    auto* list_widget = new QWidget(this);
    auto* list_vl = new QVBoxLayout(list_widget);
    list_vl->setContentsMargins(0, 0, 0, 0);
    list_vl->setSpacing(6);

    const QStringList profiles = pm.list_profiles();
    for (const QString& name : profiles) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(label_ss());
        hl->addWidget(name_lbl, 1);

        if (name == pm.active()) {
            auto* badge = new QLabel("ACTIVE");
            badge->setStyleSheet(QString("color:%1;font-weight:700;font-size:10px;"
                                         "background:transparent;")
                                     .arg(ui::colors::AMBER()));
            hl->addWidget(badge);
        } else {
            // "Switch" launches a new process with --profile <name> then quits this one
            auto* switch_btn = new QPushButton("Switch");
            switch_btn->setFixedWidth(72);
            switch_btn->setStyleSheet(btn_secondary_ss());
            connect(switch_btn, &QPushButton::clicked, this, [name]() {
                const QString exe = QCoreApplication::applicationFilePath();
                QProcess::startDetached(exe, {"--profile", name});
                QCoreApplication::quit();
            });
            hl->addWidget(switch_btn);
        }
        list_vl->addWidget(row);
    }
    vl->addWidget(list_widget);
    vl->addWidget(make_sep());

    // Create new profile
    auto* new_title = new QLabel("Create new profile");
    new_title->setStyleSheet(sub_title_ss());
    vl->addWidget(new_title);

    auto* new_row = new QWidget(this);
    auto* new_hl = new QHBoxLayout(new_row);
    new_hl->setContentsMargins(0, 0, 0, 0);
    new_hl->setSpacing(8);

    auto* name_input = new QLineEdit;
    name_input->setPlaceholderText("profile-name  (alphanumeric, - and _ only)");
    name_input->setStyleSheet(input_ss());
    new_hl->addWidget(name_input, 1);

    auto* create_btn = new QPushButton("Create & Switch");
    create_btn->setStyleSheet(btn_primary_ss());
    connect(create_btn, &QPushButton::clicked, this, [name_input]() {
        const QString name = name_input->text().trimmed().toLower();
        if (name.isEmpty())
            return;
        ProfileManager::instance().create_profile(name);
        const QString exe = QCoreApplication::applicationFilePath();
        QProcess::startDetached(exe, {"--profile", name});
        QCoreApplication::quit();
    });
    new_hl->addWidget(create_btn);
    vl->addWidget(new_row);

    auto* hint = new QLabel("Creating a profile sets up a fresh data directory. "
                            "The app will restart with the new profile active.");
    hint->setWordWrap(true);
    hint->setStyleSheet(label_ss());
    vl->addWidget(hint);

    vl->addStretch();
    scroll->setWidget(page);
    return scroll;
}

QVariantMap SettingsScreen::save_state() const {
    return {{"section", sections_ ? sections_->currentIndex() : 0}};
}

void SettingsScreen::restore_state(const QVariantMap& state) {
    if (!sections_)
        return;
    const int idx = state.value("section", 0).toInt();
    if (idx >= 0 && idx < sections_->count())
        sections_->setCurrentIndex(idx);
}

QWidget* SettingsScreen::build_keybindings() {
    return new KeybindingsSection(this);
}

// ── Python Env ────────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_python_env() {
    return new PythonEnvSection(this);
}

// ── Developer ─────────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_developer() {
    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    auto* title = new QLabel("DataHub Inspector");
    title->setStyleSheet(section_title_ss());
    vl->addWidget(title);

    auto* desc = new QLabel(
        "Live view over the in-process pub/sub layer. Shows every active topic, its "
        "subscriber count, total publishes, and time since last publish. Refreshes "
        "once per second while this tab is visible.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1;font-size:11px;")
                            .arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(desc);

    vl->addWidget(new devtools::DataHubInspector(page), 1);
    return page;
}

} // namespace fincept::screens
