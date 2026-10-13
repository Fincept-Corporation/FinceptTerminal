// SettingsScreen.cpp — thin shell hosting nav + a QStackedWidget of section
// widgets. All per-section state and rendering lives in *Section.h/.cpp.

#include "screens/settings/SettingsScreen.h"

#include "core/events/EventBus.h"
#include "core/i18n/LanguageManager.h"
#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/settings/AppearanceSection.h"
#include "screens/settings/CloudSyncSection.h"
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
#include "services/llm/LlmService.h"
#include "services/llm/ProviderCatalog.h"
#include "services/stt/SpeechService.h"
#include "services/tts/TtsService.h"
#include "services/voice_trigger/ClapDetectorService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EnterprisePromo.h"

#include <QEvent>
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

    nav_title_ = new QLabel;
    nav_title_->setStyleSheet(section_title_ss());
    nvl->addWidget(nav_title_);
    nvl->addSpacing(6);

    nav_filter_ = new QLineEdit;
    nav_filter_->setFixedHeight(26);
    nav_filter_->setClearButtonEnabled(true);
    nav_filter_->setStyleSheet(input_ss());
    nav_filter_->setAccessibleName(tr("Search settings"));
    connect(nav_filter_, &QLineEdit::textChanged, this, &SettingsScreen::apply_nav_filter);
    // Enter jumps to the first section that survived the filter — otherwise the
    // search box narrows the list but still needs a mouse click to act on it.
    connect(nav_filter_, &QLineEdit::returnPressed, this, [this]() {
        for (const auto& nb : nav_buttons_) {
            if (nb.btn && !nb.btn->isHidden()) {
                nb.btn->click();
                return;
            }
        }
    });
    nvl->addWidget(nav_filter_);
    nvl->addSpacing(6);

    // ── Section factories ────────────────────────────────────────────────────
    // One factory per stack index. Used at construction AND by the language-
    // change rebuild path so we don't hardcode the type list twice.
    section_factories_.clear();
    section_factories_.resize(16);
    section_factories_[0] = [] { return new CredentialsSection; };
    section_factories_[1] = [] { return new AppearanceSection; };
    section_factories_[2] = [] { return new NotificationsSection; };
    section_factories_[3] = [] { return new StorageSection; };
    section_factories_[4] = [] { return new DataSourcesSection; };
    section_factories_[5] = [] { return new LlmConfigSection; };
    section_factories_[6] = [] { return new McpServersSection; };
    section_factories_[7] = [] { return new LoggingSection; };
    section_factories_[8] = [] { return new SecuritySection; };
    section_factories_[9] = [] { return new ProfilesSection; };
    section_factories_[10] = [] { return new KeybindingsSection; };
    section_factories_[11] = [] { return new PythonEnvSection; };
    section_factories_[12] = [] { return new DeveloperSection; };
    section_factories_[13] = [] { return new VoiceConfigSection; };
    section_factories_[14] = [] { return new GeneralSection; };
    section_factories_[15] = [] { return new CloudSyncSection; };

    // Empty placeholders; the real section replaces its placeholder on first
    // visit (ensure_section_built).
    sections_ = new QStackedWidget;
    section_built_.fill(false, section_factories_.size());
    for (int i = 0; i < section_factories_.size(); ++i)
        sections_->addWidget(new QWidget);

    auto make_btn = [&](const QString& source_key, int idx, const QString& keywords = {}) {
        auto* btn = new QPushButton;
        btn->setFixedHeight(32);
        btn->setCheckable(true);
        btn->setStyleSheet(nav_btn_ss());
        btn->setProperty("settingsSectionIndex", idx);
        connect(btn, &QPushButton::clicked, this, [this, btn, idx]() {
            if (auto* parent = btn->parentWidget()) {
                for (auto* sibling : parent->findChildren<QPushButton*>())
                    sibling->setChecked(false);
            }
            btn->setChecked(true);
            ensure_section_built(idx);
            sections_->setCurrentIndex(idx);
            forward_search_to_section(idx);
            ScreenStateManager::instance().notify_changed(this);
        });
        nvl->addWidget(btn);
        nav_buttons_.append({btn, source_key, keywords});
        return btn;
    };

    auto add_scope_header = [&](const QString& key) {
        auto* h = new QLabel;
        h->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;"
                                 "letter-spacing:1.2px;padding:8px 6px 4px 6px;")
                             .arg(ui::colors::TEXT_DIM()));
        nvl->addWidget(h);
        scope_headers_.append(h);
        scope_header_keys_.append(key);
    };

    // The keyword strings are search aliases only — never displayed, so they
    // stay in English (the filter also matches the translated label text).
    // Sections whose content is data-driven (credential providers, LLM
    // providers, keyboard actions) add those names too, so e.g. "tiingo",
    // "deepseek" or "screenshot" lands on the right section.
    QStringList llm_terms;
    for (const QString& id : ai_chat::ProviderCatalog::known_providers())
        llm_terms << id << ai_chat::ProviderCatalog::display_name(id);
    QStringList key_terms;
    for (KeyAction a : KeyConfigManager::instance().all_actions())
        key_terms << KeyConfigManager::instance().display_name(a);

    add_scope_header(QStringLiteral("SHELL"));
    auto* first = make_btn(QStringLiteral("General"), 14,
                           QStringLiteral("language locale translation currency symbol window close last window "
                                          "launchpad quit interface"));
    make_btn(QStringLiteral("Appearance"), 1,
             QStringLiteral("theme font size family density spacing padding ticker bar chat bubble animation "
                            "typography interface dashboard"));
    make_btn(QStringLiteral("Notifications"), 2,
             QStringLiteral("telegram discord slack email smtp whatsapp twilio pushover ntfy pushbullet gotify "
                            "mattermost teams webhook pagerduty opsgenie sms alerts price news order fill breaking "
                            "monitors deviations flash trigger test send provider"));
    make_btn(QStringLiteral("Keybindings"), 10,
             QStringLiteral("shortcut hotkey keyboard rebind keys keymap accelerator ") + key_terms.join(' '));
    make_btn(QStringLiteral("Voice"), 13,
             QStringLiteral("speech stt tts deepgram microphone mic clap wake trigger aura pyttsx3 voice dictation "
                            "text to speech"));
    make_btn(QStringLiteral("Logging"), 7,
             QStringLiteral("log level debug trace warn error verbose json log file folder path tags override "
                            "diagnostics"));
    make_btn(QStringLiteral("Developer"), 12,
             QStringLiteral("datahub inspector agentic mode autonomous tasks schedules background pubsub topics "
                            "experimental devtools"));

    add_scope_header(QStringLiteral("PROFILE"));
    make_btn(QStringLiteral("Profiles"), 9,
             QStringLiteral("account switch multi profile workspace isolation create restart"));
    make_btn(QStringLiteral("Credentials"), 0,
             QStringLiteral("api key secret token encrypted secure storage ") +
                 CredentialsSection::search_terms().join(' '));
    make_btn(QStringLiteral("Security"), 8,
             QStringLiteral("pin lock auto-lock timeout inactivity audit log attempts lockout minimize destructive "
                            "ai tools permissions"));
    make_btn(QStringLiteral("Data Sources"), 4,
             QStringLiteral("connections connectors websocket rest sql providers enable disable delete bulk"));
    make_btn(QStringLiteral("LLM Config"), 5,
             QStringLiteral("ai model llm provider profile temperature max tokens system prompt tool rounds base url "
                            "fetch test connection mcp tools ") +
                 llm_terms.join(' '));
    make_btn(QStringLiteral("MCP Servers"), 6,
             QStringLiteral("model context protocol tools external server stdio command env"));
    make_btn(QStringLiteral("Python Env"), 11,
             QStringLiteral("packages venv pip uv numpy libraries install upgrade python interpreter environment"));
    make_btn(QStringLiteral("Storage & Cache"), 3,
             QStringLiteral("disk database sqlite sql console query cache clear delete data danger zone workspaces "
                            "logs reset ui state wipe"));
    make_btn(QStringLiteral("Cloud Sync"), 15,
             QStringLiteral("backup sync account devices domains credits cloud watchlists notes portfolios reports "
                            "workflows dashboard preferences notebooks upload merge"));

    nav_no_match_ = new QLabel;
    nav_no_match_->setWordWrap(true);
    nav_no_match_->setVisible(false);
    nvl->addWidget(nav_no_match_);

    first->setChecked(true);
    ensure_section_built(14);
    sections_->setCurrentIndex(14);

    nvl->addStretch();
    // Sidebar footer: a quiet pointer to the private edition. Self-styled, not
    // in nav_buttons_ (so the section filter and checked-state logic skip it);
    // text is set in retranslateUi().
    auto* ent_link = ui::enterprise::make_cta_button(
        QString(), ui::enterprise::url_with_ref(ui::enterprise::site_url(), QStringLiteral("settings")),
        ui::enterprise::CtaStyle::Quiet, nav_);
    ent_link->setObjectName("settingsEnterpriseLink");
    nvl->addWidget(ent_link);
    root->addWidget(nav_);
    root->addWidget(sections_, 1);

    retranslateUi();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
    refresh_theme();

    // Language changes mean every section needs fresh tr() lookups. We rebuild
    // each section by constructing a new instance via its factory — simpler
    // and more reliable than threading retranslateUi() through every section.
    connect(&i18n::LanguageManager::instance(), &i18n::LanguageManager::language_changed, this,
            [this](const QString&) { rebuild_sections_for_language_change(); });
}

void SettingsScreen::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(e);
}

void SettingsScreen::apply_nav_filter(const QString& text) {
    // Every word must match somewhere in the label / keywords, so "telegram
    // alerts" or "api key polygon" work, not just one contiguous phrase.
    const QStringList terms = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const bool filtering = !terms.isEmpty();

    int shown = 0;
    for (const auto& nb : nav_buttons_) {
        if (!nb.btn)
            continue;
        bool match = true;
        if (filtering) {
            const QString hay = nb.btn->text() + QLatin1Char(' ') + nb.source_key + QLatin1Char(' ') + nb.keywords;
            for (const QString& t : terms) {
                if (!hay.contains(t, Qt::CaseInsensitive)) {
                    match = false;
                    break;
                }
            }
        }
        nb.btn->setVisible(match);
        if (match)
            ++shown;
    }
    for (auto* h : scope_headers_) {
        if (h)
            h->setVisible(!filtering);
    }
    if (nav_no_match_) {
        nav_no_match_->setText(tr("No settings match \"%1\".").arg(text.simplified()));
        nav_no_match_->setVisible(filtering && shown == 0);
    }
}

void SettingsScreen::forward_search_to_section(int idx) {
    // Deep-link the search into sections that have their own filter: "tiingo"
    // opens Credentials already narrowed to the Tiingo card. Only when the query
    // names something inside the section — "keys" should not filter the cards.
    const QString q = nav_filter_ ? nav_filter_->text().simplified() : QString();
    if (q.isEmpty() || !sections_)
        return;
    auto names_contain = [&q](const QStringList& names) {
        for (const QString& n : names) {
            if (n.contains(q, Qt::CaseInsensitive))
                return true;
        }
        return false;
    };
    if (idx == 0) {
        if (auto* creds = qobject_cast<CredentialsSection*>(sections_->widget(0)))
            creds->set_filter(names_contain(CredentialsSection::search_terms()) ? q : QString());
    } else if (idx == 10) {
        QStringList actions;
        for (KeyAction a : KeyConfigManager::instance().all_actions())
            actions << KeyConfigManager::instance().display_name(a);
        if (auto* keys = qobject_cast<KeybindingsSection*>(sections_->widget(10)))
            keys->set_search(names_contain(actions) ? q : QString());
    }
}

void SettingsScreen::retranslateUi() {
    if (nav_title_)
        nav_title_->setText(tr("SETTINGS"));
    if (nav_filter_)
        nav_filter_->setPlaceholderText(tr("Search settings…"));
    for (const auto& nb : nav_buttons_) {
        if (nb.btn)
            nb.btn->setText(tr(nb.source_key.toUtf8().constData()));
    }
    for (int i = 0; i < scope_headers_.size() && i < scope_header_keys_.size(); ++i) {
        if (scope_headers_[i])
            scope_headers_[i]->setText(tr(scope_header_keys_[i].toUtf8().constData()));
    }
    if (auto* ent_link = findChild<QPushButton*>(QStringLiteral("settingsEnterpriseLink")))
        ent_link->setText(tr("Fincept Terminal Enterprise ↗"));
}

void SettingsScreen::rebuild_sections_for_language_change() {
    if (!sections_)
        return;
    const int current = sections_->currentIndex();
    // Replace each already-built widget in-place. We insert at the same index
    // first, then remove the old widget — preserves index ordering for nav
    // buttons. Only the visible section is rebuilt right away; the others go
    // back to placeholders and are built (in the new language) the next time
    // they are visited, instead of rebuilding up to 16 sections on a language
    // switch.
    for (int i = 0; i < section_factories_.size(); ++i) {
        if (!section_factories_[i] || !section_built_.value(i))
            continue;
        const bool visible_now = (i == current);
        QWidget* old = sections_->widget(i);
        QWidget* fresh = visible_now ? section_factories_[i]() : new QWidget;
        sections_->insertWidget(i, fresh);
        sections_->removeWidget(old);
        if (old)
            old->deleteLater();
        if (visible_now)
            wire_section_signals(i); // signals must be re-wired against the fresh instance
        else
            section_built_[i] = false;
    }
    sections_->setCurrentIndex(current);
}

void SettingsScreen::ensure_section_built(int idx) {
    if (!sections_ || idx < 0 || idx >= section_factories_.size() || section_built_.value(idx) ||
        !section_factories_[idx])
        return;
    QWidget* placeholder = sections_->widget(idx);
    const bool was_current = (sections_->currentWidget() == placeholder);
    QWidget* fresh = section_factories_[idx]();
    sections_->insertWidget(idx, fresh);
    sections_->removeWidget(placeholder);
    if (placeholder)
        placeholder->deleteLater();
    if (was_current)
        sections_->setCurrentWidget(fresh);
    section_built_[idx] = true;
    wire_section_signals(idx);
}

void SettingsScreen::wire_section_signals(int idx) {
    if (!sections_)
        return;
    // LLM config changes → reload AI chat service.
    if (auto* llm = qobject_cast<LlmConfigSection*>(idx == 5 ? sections_->widget(idx) : nullptr)) {
        connect(llm, &LlmConfigSection::config_changed, this,
                []() { ai_chat::LlmService::instance().reload_config(); });
    }
    // Voice config changes → reload BOTH STT and TTS services and restart
    // the clap detector so the user's new provider / key / voice / wake-trigger
    // picks take effect on the next session.
    if (auto* voice = qobject_cast<VoiceConfigSection*>(idx == 13 ? sections_->widget(idx) : nullptr)) {
        connect(voice, &VoiceConfigSection::config_changed, this, []() {
            fincept::services::SpeechService::instance().reload_config();
            fincept::services::TtsService::instance().reload_config();

            auto& clap = fincept::services::ClapDetectorService::instance();
            clap.stop();
            if (fincept::services::ClapDetectorService::is_enabled_in_config())
                clap.start();
        });
    }
}

void SettingsScreen::refresh_theme() {
    using namespace settings_styles;
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    if (nav_) {
        nav_->setStyleSheet(QString("background:%1;border-right:1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
        // Walk our own list rather than every QPushButton descendant, so the
        // nav style is never applied to embedded controls (e.g. the filter
        // box's clear button).
        for (const auto& nb : nav_buttons_) {
            if (nb.btn)
                nb.btn->setStyleSheet(nav_btn_ss());
        }
        if (nav_filter_)
            nav_filter_->setStyleSheet(input_ss());
        // Re-apply scope-header colour. Walk our explicit list rather than
        // text-matching, so the headers' style survives translation to
        // languages where the source label no longer literally reads "SHELL".
        if (nav_no_match_)
            nav_no_match_->setStyleSheet(QString("color:%1;padding:6px;").arg(ui::colors::TEXT_DIM()));
        for (auto* lbl : scope_headers_) {
            if (!lbl)
                continue;
            lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;"
                                       "letter-spacing:1.2px;padding:8px 6px 4px 6px;")
                                   .arg(ui::colors::TEXT_DIM()));
        }
    }
}

void SettingsScreen::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    subscribe_mcp_events();
}

void SettingsScreen::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    unsubscribe_mcp_events();
}

void SettingsScreen::reload_visible_section() {
    if (!sections_)
        return;
    auto* w = sections_->currentWidget();
    if (!w)
        return;
    w->hide();
    w->show();
}

// ── MCP-driven UI sync ──────────────────────────────────────────────────────

void SettingsScreen::subscribe_mcp_events() {
    if (!mcp_event_subs_.isEmpty())
        return; // idempotent

    QPointer<SettingsScreen> self = this;
    auto on_settings_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self)
                    return;
                self->reload_visible_section();
            },
            Qt::QueuedConnection);
    };
    auto on_provider_changed = [self](const QVariantMap&) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self.data(),
            [self]() {
                if (!self)
                    return;
                self->reload_visible_section();
                ai_chat::LlmService::instance().reload_config();
            },
            Qt::QueuedConnection);
    };

    auto& bus = EventBus::instance();
    mcp_event_subs_.append(bus.subscribe(this, "settings.changed", on_settings_changed));
    mcp_event_subs_.append(bus.subscribe(this, "llm.provider_changed", on_provider_changed));
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
    if (!sections_)
        return;
    const int idx = state.value("section", 14).toInt();
    if (idx < 0 || idx >= sections_->count())
        return;
    ensure_section_built(idx);
    sections_->setCurrentIndex(idx);

    if (!nav_)
        return;
    for (auto* btn : nav_->findChildren<QPushButton*>()) {
        const QVariant tag = btn->property("settingsSectionIndex");
        btn->setChecked(tag.isValid() && tag.toInt() == idx);
    }
}

} // namespace fincept::screens
