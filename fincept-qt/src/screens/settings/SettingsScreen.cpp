// SettingsScreen.cpp — full-featured settings (Credentials, Appearance,
// Notifications, Storage & Cache, Data Sources, LLM Config, MCP Servers)

#include "screens/settings/SettingsScreen.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "screens/settings/LlmConfigSection.h"
#include "screens/settings/McpServersSection.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/DataSourceRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
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

// ── File-scope style constants ────────────────────────────────────────────────

static const char* SECTION_TITLE = "color: #FF6600; font-size: 13px; font-weight: bold; letter-spacing: 0.5px; "
                                   "background: transparent;";

static const char* SUB_TITLE = "color: #808080; font-size: 11px; font-weight: 700; letter-spacing: 0.5px; "
                               "background: transparent;";

static const char* LABEL = "color: #888888; font-size: 13px; background: transparent;";

static const char* NAV_BTN = "QPushButton { background: transparent; color: #888; border: none; "
                             "text-align: left; padding: 0 12px; font-size: 13px; }"
                             "QPushButton:hover { background: #111; color: #e0e0e0; }"
                             "QPushButton:checked { color: #FF6600; background: #111; }";

static const char* INPUT_SS = "QLineEdit { background: #111; color: #e0e0e0; border: 1px solid #2a2a2a; "
                              "border-radius: 3px; padding: 6px; font-size: 12px; }"
                              "QLineEdit:focus { border: 1px solid #FF6600; }";

static const char* BTN_PRIMARY = "QPushButton { background: #FF6600; color: #000; border: none; border-radius: 4px; "
                                 "font-size: 12px; font-weight: 700; padding: 0 16px; height: 32px; }"
                                 "QPushButton:hover { background: #FF8800; }";

static const char* BTN_SECONDARY = "QPushButton { background: #1a1a1a; color: #e0e0e0; border: 1px solid #333; "
                                   "border-radius: 4px; font-size: 12px; padding: 0 12px; height: 32px; }"
                                   "QPushButton:hover { background: #222; }";

static const char* BTN_DANGER = "QPushButton { background: #1a0a0a; color: #cc3300; border: 1px solid #330000; "
                                "border-radius: 3px; font-size: 12px; padding: 4px 12px; }"
                                "QPushButton:hover { background: #2a1010; }";

static const char* COMBO_SS = "QComboBox { background: #111; color: #e0e0e0; border: 1px solid #2a2a2a; "
                              "border-radius: 3px; padding: 5px 8px; font-size: 12px; min-width: 160px; }"
                              "QComboBox:focus { border: 1px solid #FF6600; }"
                              "QComboBox::drop-down { border: none; width: 20px; }"
                              "QComboBox QAbstractItemView { background: #111; color: #e0e0e0; "
                              "selection-background-color: #1a1a1a; border: 1px solid #333; }";

static const char* CHECK_SS = "QCheckBox { color: #888; font-size: 13px; background: transparent; }"
                              "QCheckBox::indicator { width: 14px; height: 14px; }"
                              "QCheckBox::indicator:unchecked { border: 1px solid #333; background: #111; }"
                              "QCheckBox::indicator:checked   { border: 1px solid #FF6600; background: #FF6600; }";

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
    setStyleSheet(QString("background: %1;").arg(ui::colors::DARK));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left nav panel
    auto* nav = new QWidget;
    nav->setFixedWidth(200);
    nav->setStyleSheet(
        QString("background: %1; border-right: 1px solid %2;").arg(ui::colors::PANEL, ui::colors::BORDER));
    auto* nvl = new QVBoxLayout(nav);
    nvl->setContentsMargins(8, 16, 8, 8);
    nvl->setSpacing(2);

    auto* title = new QLabel("SETTINGS");
    title->setStyleSheet(SECTION_TITLE);
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

    QList<QPushButton*> nav_btns;
    auto make_btn = [&](const QString& text, int idx) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(32);
        btn->setCheckable(true);
        btn->setStyleSheet(NAV_BTN);
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

    first->setChecked(true);

    nvl->addStretch();
    root->addWidget(nav);
    root->addWidget(sections_, 1);

    // Wire LLM config changes → reload AI chat service
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5))) {
        connect(llm, &LlmConfigSection::config_changed, this,
                []() { ai_chat::LlmService::instance().reload_config(); });
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
    lbl->setStyleSheet(LABEL);
    hl->addWidget(lbl);
    hl->addStretch();
    hl->addWidget(control);
    vl->addLayout(hl);

    if (!description.isEmpty()) {
        auto* desc = new QLabel(description);
        desc->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
        desc->setWordWrap(true);
        vl->addWidget(desc);
    }
    return row;
}

QFrame* SettingsScreen::make_sep() {
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #1e1e1e;");
    return sep;
}

// ── Credentials ───────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_credentials() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: #0a0a0a; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 3px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    // Title
    auto* t = new QLabel("API CREDENTIALS");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info =
        new QLabel("Store API keys securely in the OS keychain. Keys are never written to disk in plain text.");
    info->setWordWrap(true);
    info->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
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
        card->setStyleSheet("QFrame { background: #0d0d0d; border: 1px solid #1a1a1a; border-radius: 4px; }");
        auto* cvl = new QVBoxLayout(card);
        cvl->setContentsMargins(0, 0, 0, 0);
        cvl->setSpacing(0);

        // Card header
        auto* hdr = new QWidget;
        hdr->setFixedHeight(34);
        hdr->setStyleSheet("background: #111; border-bottom: 1px solid #1a1a1a;");
        auto* hhl = new QHBoxLayout(hdr);
        hhl->setContentsMargins(12, 0, 12, 0);
        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet("color: #e0e0e0; font-size: 12px; font-weight: 600; background: transparent;");
        hhl->addWidget(name_lbl);
        hhl->addStretch();

        // Status label — persisted across load_credentials calls
        auto* status_lbl = new QLabel("Not set");
        status_lbl->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
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
        field->setStyleSheet(INPUT_SS);
        cred_fields_[key] = field;
        bhl->addWidget(field, 1);

        auto* save_btn = new QPushButton("Save");
        save_btn->setFixedHeight(30);
        save_btn->setFixedWidth(70);
        save_btn->setStyleSheet(BTN_PRIMARY);
        bhl->addWidget(save_btn);

        connect(save_btn, &QPushButton::clicked, this, [this, key, field, status_lbl]() {
            QString val = field->text().trimmed();
            if (val.isEmpty()) {
                SecureStorage::instance().remove(key);
                field->setPlaceholderText("Not configured");
                status_lbl->setText("Cleared");
                status_lbl->setStyleSheet("color: #888; font-size: 11px; background: transparent;");
                LOG_INFO("Credentials", "Cleared key: " + key);
            } else {
                auto r = SecureStorage::instance().store(key, val);
                if (r.is_ok()) {
                    field->clear();
                    field->setPlaceholderText("•••••••• (saved)");
                    status_lbl->setText("Saved ✓");
                    status_lbl->setStyleSheet("color: #44aa44; font-size: 11px; background: transparent;");
                    LOG_INFO("Credentials", "Stored key: " + key);
                } else {
                    status_lbl->setText("Save failed");
                    status_lbl->setStyleSheet("color: #cc3300; font-size: 11px; background: transparent;");
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
            status->setStyleSheet("color: #44aa44; font-size: 11px; background: transparent;");
        } else {
            field->clear();
            field->setPlaceholderText("Not configured");
            status->setText("Not set");
            status->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
        }
    }
}

// ── Appearance ────────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_appearance() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: #0a0a0a; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 3px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    auto* t = new QLabel("TERMINAL APPEARANCE");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // Font size
    app_font_size_ = new QComboBox;
    app_font_size_->addItems({"12px", "13px", "14px", "15px", "16px", "17px", "18px"});
    app_font_size_->setCurrentText("14px");
    app_font_size_->setStyleSheet(COMBO_SS);
    vl->addWidget(make_row("Font Size", app_font_size_));

    // Font family
    app_font_family_ = new QComboBox;
    app_font_family_->addItems({"Consolas", "Courier New", "JetBrains Mono", "Fira Code", "Cascadia Code"});
    app_font_family_->setStyleSheet(COMBO_SS);
    vl->addWidget(make_row("Font Family", app_font_family_, "Requires restart to take full effect."));

    vl->addSpacing(8);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    auto* t2 = new QLabel("THEME");
    t2->setStyleSheet(SUB_TITLE);
    vl->addWidget(t2);
    vl->addSpacing(4);

    // Color theme
    app_theme_ = new QComboBox;
    app_theme_->addItems({"Bloomberg Dark", "Dark Terminal", "Matrix Green", "Obsidian"});
    app_theme_->setStyleSheet(COMBO_SS);
    vl->addWidget(make_row("Color Theme", app_theme_));

    // Content density
    app_density_ = new QComboBox;
    app_density_->addItems({"Compact", "Default", "Comfortable"});
    app_density_->setCurrentText("Default");
    app_density_->setStyleSheet(COMBO_SS);
    vl->addWidget(make_row("Content Density", app_density_, "Controls padding and spacing throughout the UI."));

    vl->addSpacing(16);

    // Apply button
    auto* apply_btn = new QPushButton("Apply & Save");
    apply_btn->setFixedWidth(160);
    apply_btn->setStyleSheet(BTN_PRIMARY);
    connect(apply_btn, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        repo.set("appearance.font_size", app_font_size_->currentText(), "appearance");
        repo.set("appearance.font_family", app_font_family_->currentText(), "appearance");
        repo.set("appearance.theme", app_theme_->currentText(), "appearance");
        repo.set("appearance.density", app_density_->currentText(), "appearance");
        LOG_INFO("Settings", "Appearance saved");
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
    auto load_combo = [&](QComboBox* cb, const QString& key, const QString& def) {
        auto r = repo.get(key);
        QString val = r.is_ok() ? r.value() : def;
        int idx = cb->findText(val);
        if (idx >= 0)
            cb->setCurrentIndex(idx);
    };
    load_combo(app_font_size_, "appearance.font_size", "14px");
    load_combo(app_font_family_, "appearance.font_family", "Consolas");
    load_combo(app_theme_, "appearance.theme", "Bloomberg Dark");
    load_combo(app_density_, "appearance.density", "Default");
}

// ── Notifications ─────────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_notifications() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: #0a0a0a; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 3px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    auto* t = new QLabel("NOTIFICATIONS");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Email ─────────────────────────────────────────────────────────────────
    auto* email_hdr = new QLabel("EMAIL");
    email_hdr->setStyleSheet(SUB_TITLE);
    vl->addWidget(email_hdr);

    notif_email_ = new QCheckBox;
    notif_email_->setStyleSheet(CHECK_SS);

    notif_email_addr_ = new QLineEdit;
    notif_email_addr_->setPlaceholderText("you@example.com");
    notif_email_addr_->setStyleSheet(INPUT_SS);
    notif_email_addr_->setFixedWidth(240);

    auto* email_row = make_row("Email Address", notif_email_addr_);

    vl->addWidget(make_row("Email Alerts", notif_email_));
    vl->addWidget(email_row);

    // Show/hide email field based on toggle
    connect(notif_email_, &QCheckBox::toggled, email_row, &QWidget::setVisible);
    email_row->setVisible(false); // default hidden until loaded

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── In-App ────────────────────────────────────────────────────────────────
    auto* inapp_hdr = new QLabel("IN-APP ALERTS");
    inapp_hdr->setStyleSheet(SUB_TITLE);
    vl->addWidget(inapp_hdr);

    notif_inapp_ = new QCheckBox;
    notif_inapp_->setStyleSheet(CHECK_SS);
    notif_price_ = new QCheckBox;
    notif_price_->setStyleSheet(CHECK_SS);
    notif_news_ = new QCheckBox;
    notif_news_->setStyleSheet(CHECK_SS);
    notif_orders_ = new QCheckBox;
    notif_orders_->setStyleSheet(CHECK_SS);

    vl->addWidget(make_row("In-App Notifications", notif_inapp_));
    vl->addWidget(make_row("Price Alerts", notif_price_));
    vl->addWidget(make_row("News Alerts", notif_news_));
    vl->addWidget(make_row("Order Fill Alerts", notif_orders_));

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Sound ────────────────────────────────────────────────────────────────
    auto* sound_hdr = new QLabel("SOUND");
    sound_hdr->setStyleSheet(SUB_TITLE);
    vl->addWidget(sound_hdr);

    notif_sound_ = new QCheckBox;
    notif_sound_->setStyleSheet(CHECK_SS);
    vl->addWidget(make_row("Sound Alerts", notif_sound_, "Plays a chime on price alerts and order fills."));

    vl->addSpacing(16);

    auto* save_btn = new QPushButton("Save Preferences");
    save_btn->setFixedWidth(180);
    save_btn->setStyleSheet(BTN_PRIMARY);
    connect(save_btn, &QPushButton::clicked, this, [this]() {
        auto& repo = SettingsRepository::instance();
        auto b = [](bool v) { return v ? "1" : "0"; };
        repo.set("notifications.email", b(notif_email_->isChecked()), "notifications");
        repo.set("notifications.email_addr", notif_email_addr_->text().trimmed(), "notifications");
        repo.set("notifications.inapp", b(notif_inapp_->isChecked()), "notifications");
        repo.set("notifications.price_alerts", b(notif_price_->isChecked()), "notifications");
        repo.set("notifications.news_alerts", b(notif_news_->isChecked()), "notifications");
        repo.set("notifications.order_fills", b(notif_orders_->isChecked()), "notifications");
        repo.set("notifications.sound", b(notif_sound_->isChecked()), "notifications");
        LOG_INFO("Settings", "Notification preferences saved");
    });
    vl->addWidget(save_btn);
    vl->addStretch();

    scroll->setWidget(page);
    return scroll;
}

void SettingsScreen::load_notifications() {
    if (!notif_email_)
        return;
    auto& repo = SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() ? (r.value() == "1") : def;
    };

    notif_email_->setChecked(get_bool("notifications.email", true));
    auto addr_r = repo.get("notifications.email_addr");
    notif_email_addr_->setText(addr_r.is_ok() ? addr_r.value() : "");
    notif_inapp_->setChecked(get_bool("notifications.inapp", true));
    notif_price_->setChecked(get_bool("notifications.price_alerts", false));
    notif_news_->setChecked(get_bool("notifications.news_alerts", false));
    notif_orders_->setChecked(get_bool("notifications.order_fills", true));
    notif_sound_->setChecked(get_bool("notifications.sound", false));

    // Sync email address row visibility
    if (auto* email_row = notif_email_addr_->parentWidget())
        email_row->setVisible(notif_email_->isChecked());
}

// ── Storage & Cache ───────────────────────────────────────────────────────────

QWidget* SettingsScreen::build_storage() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: #0a0a0a; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 3px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(10);

    auto* t = new QLabel("STORAGE & CACHE");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Cache overview card ───────────────────────────────────────────────────
    auto* t2 = new QLabel("CACHE OVERVIEW");
    t2->setStyleSheet(SUB_TITLE);
    vl->addWidget(t2);
    vl->addSpacing(4);

    auto* card = new QFrame;
    card->setStyleSheet("QFrame { background: #0d0d0d; border: 1px solid #1a1a1a; border-radius: 4px; }");
    auto* cvl = new QVBoxLayout(card);
    cvl->setContentsMargins(16, 12, 16, 12);
    cvl->setSpacing(8);

    storage_count_ = new QLabel("—");
    storage_count_->setStyleSheet("color: #e0e0e0; font-size: 13px; background: transparent;");
    cvl->addWidget(make_row("Cache Entries", storage_count_));

    auto* refresh_btn = new QPushButton("Refresh");
    refresh_btn->setFixedWidth(90);
    refresh_btn->setStyleSheet(BTN_SECONDARY);
    connect(refresh_btn, &QPushButton::clicked, this, [this]() { refresh_storage_stats(); });
    cvl->addWidget(refresh_btn);
    vl->addWidget(card);

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Clear all ─────────────────────────────────────────────────────────────
    auto* t3 = new QLabel("CLEAR CACHE");
    t3->setStyleSheet(SUB_TITLE);
    vl->addWidget(t3);
    vl->addSpacing(4);

    auto* clear_all = new QPushButton("Clear All Cache");
    clear_all->setStyleSheet(BTN_DANGER);
    connect(clear_all, &QPushButton::clicked, this, [this]() {
        CacheManager::instance().clear();
        refresh_storage_stats();
        LOG_INFO("Settings", "All cache cleared");
    });
    vl->addWidget(clear_all);

    auto* note = new QLabel("Clears all cached market data, quotes, and news. "
                            "Live data will be re-fetched on next access.");
    note->setWordWrap(true);
    note->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
    vl->addWidget(note);

    vl->addSpacing(12);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    // ── Clear by category ─────────────────────────────────────────────────────
    auto* t4 = new QLabel("CLEAR BY CATEGORY");
    t4->setStyleSheet(SUB_TITLE);
    vl->addWidget(t4);
    vl->addSpacing(4);

    static const QStringList CACHE_CATS = {"market_data", "news", "quotes", "charts", "general"};
    for (const QString& cat : CACHE_CATS) {
        auto* btn = new QPushButton("Clear  " + cat);
        btn->setStyleSheet(BTN_SECONDARY);
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
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }"
                          "QScrollBar:vertical { background: #0a0a0a; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #2a2a2a; border-radius: 3px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(0);

    auto* t = new QLabel("DATA SOURCES");
    t->setStyleSheet(SECTION_TITLE);
    vl->addWidget(t);
    vl->addSpacing(4);

    auto* info = new QLabel("Toggle which external data providers are active. "
                            "Changes take effect immediately.");
    info->setWordWrap(true);
    info->setStyleSheet("color: #555; font-size: 11px; background: transparent;");
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
        empty->setStyleSheet("color: #555; font-size: 12px; background: transparent;");
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
        cat_lbl->setStyleSheet(SUB_TITLE);
        vl->addWidget(cat_lbl);
        vl->addSpacing(6);

        for (const auto& ds : it.value()) {
            // Source card
            auto* card = new QFrame;
            card->setStyleSheet("QFrame { background: #0d0d0d; border: 1px solid #1a1a1a; border-radius: 4px; }");
            auto* cvl2 = new QVBoxLayout(card);
            cvl2->setContentsMargins(0, 0, 0, 0);
            cvl2->setSpacing(0);

            // Card header row
            auto* hdr = new QWidget;
            hdr->setFixedHeight(40);
            hdr->setStyleSheet("background: #111; border-bottom: 1px solid #1a1a1a;");
            auto* hhl = new QHBoxLayout(hdr);
            hhl->setContentsMargins(12, 0, 12, 0);

            auto* name_lbl = new QLabel(ds.display_name.isEmpty() ? ds.alias : ds.display_name);
            name_lbl->setStyleSheet("color: #e0e0e0; font-size: 12px; font-weight: 600; background: transparent;");
            hhl->addWidget(name_lbl);

            // Type badge
            auto* type_badge = new QLabel(ds.type == "websocket" ? "WS" : "REST");
            type_badge->setStyleSheet("color: #555; font-size: 10px; font-weight: 700; "
                                      "background: #1a1a1a; border: 1px solid #2a2a2a; "
                                      "border-radius: 2px; padding: 1px 5px;");
            hhl->addWidget(type_badge);
            hhl->addStretch();

            // Enable toggle
            auto* toggle = new QCheckBox;
            toggle->setChecked(ds.enabled);
            toggle->setStyleSheet(CHECK_SS);
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
                desc->setStyleSheet("color: #555; font-size: 11px; padding: 8px 12px; background: transparent;");
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

} // namespace fincept::screens
