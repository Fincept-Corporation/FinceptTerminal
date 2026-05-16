// SettingsScreen.cpp — thin shell hosting nav + a QStackedWidget of section
// widgets. All per-section state and rendering lives in *Section.h/.cpp.

#include "screens/settings/SettingsScreen.h"

#include "services/llm/LlmService.h"
#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/settings/AppearanceSection.h"
#include "screens/settings/CredentialsSection.h"
#include "screens/settings/DataSourcesSection.h"
#include "screens/settings/DeveloperSection.h"
#include "screens/settings/GeneralSection.h"
#include "screens/settings/KeybindingsSection.h"
#include "screens/settings/LlmConfigSection.h"
#include "screens/settings/LoggingSection.h"
#include "screens/settings/McpServersSection.h"
#include "screens/settings/NotificationsSection.h"
#include "screens/settings/ProfilesSection.h"
#include "screens/settings/PythonEnvSection.h"
#include "screens/settings/SecuritySection.h"
#include "screens/settings/SettingsStyles.h"
#include "screens/settings/StorageSection.h"
#include "screens/settings/VoiceConfigSection.h"
#include "services/stt/SpeechService.h"
#include "services/tts/TtsService.h"
#include "services/voice_trigger/ClapDetectorService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {

SettingsScreen::SettingsScreen(QWidget* parent) : QWidget(parent) {
    using namespace settings_styles;

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Left nav panel
    nav_ = new QWidget(this);
    nav_->setFixedWidth(200);
    auto* nvl = new QVBoxLayout(nav_);
    nvl->setContentsMargins(8, 16, 8, 8);
    nvl->setSpacing(2);

    auto* title = new QLabel("SETTINGS");
    title->setStyleSheet(section_title_ss());
    nvl->addWidget(title);
    nvl->addSpacing(12);

    // Sections (order matters — indices used in nav buttons)
    sections_ = new QStackedWidget;
    sections_->addWidget(new CredentialsSection);   // 0
    sections_->addWidget(new AppearanceSection);    // 1
    sections_->addWidget(new NotificationsSection); // 2
    sections_->addWidget(new StorageSection);       // 3
    sections_->addWidget(new DataSourcesSection);   // 4
    sections_->addWidget(new LlmConfigSection);     // 5
    sections_->addWidget(new McpServersSection);    // 6
    sections_->addWidget(new LoggingSection);       // 7
    sections_->addWidget(new SecuritySection);      // 8
    sections_->addWidget(new ProfilesSection);      // 9
    sections_->addWidget(new KeybindingsSection);   // 10
    sections_->addWidget(new PythonEnvSection);     // 11
    sections_->addWidget(new DeveloperSection);     // 12
    sections_->addWidget(new VoiceConfigSection);   // 13
    sections_->addWidget(new GeneralSection);       // 14

    auto make_btn = [&](const QString& text, int idx) {
        auto* btn = new QPushButton(text);
        btn->setFixedHeight(32);
        btn->setCheckable(true);
        btn->setStyleSheet(nav_btn_ss());
        // Tag the button with its target section index so restore_state
        // can re-highlight the right nav row without depending on
        // section count or string-matching button text.
        btn->setProperty("settingsSectionIndex", idx);
        connect(btn, &QPushButton::clicked, this, [this, btn, idx]() {
            if (auto* parent = btn->parentWidget()) {
                for (auto* sibling : parent->findChildren<QPushButton*>())
                    sibling->setChecked(false);
            }
            btn->setChecked(true);
            sections_->setCurrentIndex(idx);
            ScreenStateManager::instance().notify_changed(this);
        });
        nvl->addWidget(btn);
        return btn;
    };

    // Decision 10.9: hierarchical scopes. Each section is tagged by which
    // tier it lives at — Shell-wide settings (theme, hotkeys, telemetry)
    // versus per-Profile settings (credentials, brokers, sync). Frame and
    // Panel scopes are owned by their respective UI surfaces (status bar,
    // panel context menus) and don't appear here.
    auto add_scope_header = [&](const QString& label) {
        auto* h = new QLabel(label);
        h->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;"
                                 "letter-spacing:1.2px;padding:8px 6px 4px 6px;")
                              .arg(ui::colors::TEXT_DIM()));
        nvl->addWidget(h);
    };

    add_scope_header("SHELL");
    auto* first = make_btn("General",       14);
    make_btn("Appearance",      1);
    make_btn("Notifications",   2);
    make_btn("Keybindings",    10);
    make_btn("Voice",          13);
    make_btn("Logging",         7);
    make_btn("Developer",      12);

    add_scope_header("PROFILE");
    make_btn("Profiles",        9);
    make_btn("Credentials",     0);
    make_btn("Security",        8);
    make_btn("Data Sources",    4);
    make_btn("LLM Config",      5);
    make_btn("MCP Servers",     6);
    make_btn("Python Env",     11);
    make_btn("Storage & Cache", 3);

    first->setChecked(true);
    sections_->setCurrentIndex(14);

    nvl->addStretch();
    root->addWidget(nav_);
    root->addWidget(sections_, 1);

    // Wire LLM config changes → reload AI chat service
    if (auto* llm = qobject_cast<LlmConfigSection*>(sections_->widget(5))) {
        connect(llm, &LlmConfigSection::config_changed, this,
                []() { ai_chat::LlmService::instance().reload_config(); });
    }

    // Wire Voice config changes — reload BOTH STT and TTS services and
    // restart the clap detector so the user's new provider / key / voice /
    // wake-trigger picks take effect on the next session.
    if (auto* voice = qobject_cast<VoiceConfigSection*>(sections_->widget(13))) {
        connect(voice, &VoiceConfigSection::config_changed, this, []() {
            fincept::services::SpeechService::instance().reload_config();
            fincept::services::TtsService::instance().reload_config();

            auto& clap = fincept::services::ClapDetectorService::instance();
            clap.stop();
            if (fincept::services::ClapDetectorService::is_enabled_in_config())
                clap.start();
        });
    }

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();
}

void SettingsScreen::refresh_theme() {
    using namespace settings_styles;
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (nav_) {
        nav_->setStyleSheet(QString("background:%1;border-right:1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        for (auto* btn : nav_->findChildren<QPushButton*>())
            btn->setStyleSheet(nav_btn_ss());
        // Re-apply scope-header colour. Headers carry no objectName, so
        // tag them by stylesheet content prefix to avoid clobbering the
        // SETTINGS title at the top of the nav.
        for (auto* lbl : nav_->findChildren<QLabel*>()) {
            const QString text = lbl->text();
            if (text == QStringLiteral("SHELL") || text == QStringLiteral("PROFILE")) {
                lbl->setStyleSheet(
                    QString("color:%1;font-size:10px;font-weight:700;"
                            "letter-spacing:1.2px;padding:8px 6px 4px 6px;")
                        .arg(ui::colors::TEXT_DIM()));
            }
        }
    }
}

void SettingsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    subscribe_mcp_events();
    // Each section's own showEvent fires when it becomes the current page;
    // no orchestration needed here.
}

void SettingsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    unsubscribe_mcp_events();
}

void SettingsScreen::reload_visible_section() {
    // MCP settings change → nudge the currently-visible section so it picks up
    // the mutation. Each section has a public reload()-like surface invoked
    // via showEvent; we fire it by hiding + showing the current widget.
    if (!sections_) return;
    auto* w = sections_->currentWidget();
    if (!w) return;
    w->hide();
    w->show();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────
// MCP settings tools publish settings.changed and llm.provider_changed when
// the LLM mutates config via Finagent or AI Chat. The provider-change handler
// also nudges LlmService so the chat screen subscriber doesn't race.

void SettingsScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty()) return; // idempotent

    QPointer<SettingsScreen> self = this;
    auto on_settings_changed = [self](const QVariantMap&) {
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) return;
            self->reload_visible_section();
        }, Qt::QueuedConnection);
    };
    auto on_provider_changed = [self](const QVariantMap&) {
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self]() {
            if (!self) return;
            self->reload_visible_section();
            ai_chat::LlmService::instance().reload_config();
        }, Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe("settings.changed",     on_settings_changed));
    mcp_event_subs_.append(bus.subscribe("llm.provider_changed", on_provider_changed));
}

void SettingsScreen::unsubscribe_mcp_events() {
    auto& bus = EventBus::instance();
    for (auto id : mcp_event_subs_)
        bus.unsubscribe(id);
    mcp_event_subs_.clear();
}

QVariantMap SettingsScreen::save_state() const {
    return {{"section", sections_ ? sections_->currentIndex() : 0}};
}

void SettingsScreen::restore_state(const QVariantMap& state) {
    if (!sections_) return;
    // Default to General (index 14) — landing page after the hierarchical
    // refactor. Users who had previously visited a different section get
    // it back via the saved index.
    const int idx = state.value("section", 14).toInt();
    if (idx < 0 || idx >= sections_->count())
        return;
    sections_->setCurrentIndex(idx);

    // Sync the nav-button checked-state so the highlight matches the
    // restored section. Buttons are tagged with `settingsSectionIndex`
    // by `make_btn`; un-check everything, then check the matching one.
    if (!nav_)
        return;
    for (auto* btn : nav_->findChildren<QPushButton*>()) {
        const QVariant tag = btn->property("settingsSectionIndex");
        btn->setChecked(tag.isValid() && tag.toInt() == idx);
    }
}

} // namespace fincept::screens
