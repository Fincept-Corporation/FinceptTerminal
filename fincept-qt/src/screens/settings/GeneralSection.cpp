// GeneralSection.cpp — application-wide behaviour toggles.

#include "screens/settings/GeneralSection.h"

#include "core/currency/CurrencyManager.h"
#include "core/i18n/LanguageManager.h"
#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr const char* kKeyOnClose       = "general.on_last_window_close";
constexpr const char* kDefaultOnClose   = "quit";
constexpr const char* kKeyLanguage      = "general.language";
constexpr const char* kDefaultLanguage  = "en";
} // namespace

GeneralSection::GeneralSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void GeneralSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void GeneralSection::build_ui() {
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

    // ── WINDOW BEHAVIOUR ──────────────────────────────────────────────────────
    auto* t = new QLabel(tr("WINDOW BEHAVIOUR"));
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    on_close_combo_ = new QComboBox;
    on_close_combo_->addItem(tr("Quit application"),   QStringLiteral("quit"));
    on_close_combo_->addItem(tr("Show Launchpad"),     QStringLiteral("show_launchpad"));
    on_close_combo_->setStyleSheet(combo_ss());
    vl->addWidget(make_row(
        tr("On last window close"),
        on_close_combo_,
        tr("Default is Quit. Choose 'Show Launchpad' if you want a small portal "
           "window to stay open after closing your last terminal window.")));

    connect(on_close_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString val = on_close_combo_->itemData(idx).toString();
                SettingsRepository::instance().set(
                    QString::fromLatin1(kKeyOnClose), val, QStringLiteral("general"));
                LOG_INFO("Settings", QString("on_last_window_close → %1").arg(val));
            });

    vl->addSpacing(20);

    // ── LANGUAGE ──────────────────────────────────────────────────────────────
    auto* lang_title = new QLabel(tr("LANGUAGE"));
    lang_title->setStyleSheet(section_title_ss());
    vl->addWidget(lang_title);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    language_combo_ = new QComboBox;
    // Populate from LanguageManager so the canonical list lives in one place.
    // Native names are intentionally NOT translated — each language is always
    // shown in its own script so a user can recognise their own locale even
    // when the current UI language is unfamiliar to them.
    for (const QString& code : i18n::LanguageManager::supported_languages()) {
        language_combo_->addItem(i18n::LanguageManager::native_name(code), code);
    }
    language_combo_->setStyleSheet(combo_ss());
    vl->addWidget(make_row(
        tr("Interface language"),
        language_combo_,
        tr("Changes apply immediately. English is the source language; all other "
           "translations are embedded with the build.")));

    connect(language_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString code = language_combo_->itemData(idx).toString();
                if (code.isEmpty())
                    return;
                LOG_INFO("Settings", QString("language → %1").arg(code));
                // LanguageManager handles persistence + translator swap + the
                // QEvent::LanguageChange that retranslates every screen.
                i18n::LanguageManager::instance().set_language(code);
            });

    vl->addSpacing(20);

    // ── CURRENCY ────────────────────────────────────────────────────────────
    auto* cur_title = new QLabel(tr("CURRENCY"));
    cur_title->setStyleSheet(section_title_ss());
    vl->addWidget(cur_title);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    currency_combo_ = new QComboBox;
    for (const auto& c : currency::CurrencyManager::available())
        currency_combo_->addItem(QString("%1  %2  (%3)").arg(c.symbol, c.name, c.code), c.code);
    currency_combo_->setStyleSheet(combo_ss());
    vl->addWidget(make_row(
        tr("Display currency"),
        currency_combo_,
        tr("Changes the currency symbol on calculators and analytics. Live market "
           "data keeps its own currency — values are not converted.")));

    connect(currency_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString code = currency_combo_->itemData(idx).toString();
                if (code.isEmpty())
                    return;
                LOG_INFO("Settings", QString("currency → %1").arg(code));
                currency::CurrencyManager::instance().set_currency(code);
            });

    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void GeneralSection::reload() {
    if (on_close_combo_) {
        const auto r = SettingsRepository::instance().get(
            QString::fromLatin1(kKeyOnClose), QString::fromLatin1(kDefaultOnClose));
        const QString cur = r.is_ok() ? r.value() : QString::fromLatin1(kDefaultOnClose);
        const int idx = on_close_combo_->findData(cur);
        QSignalBlocker block(on_close_combo_);
        on_close_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (language_combo_) {
        // Trust LanguageManager over the raw DB read — they should agree, but
        // LanguageManager is the live source after a startup-time install.
        const QString cur = i18n::LanguageManager::instance().current_language();
        const QString effective = cur.isEmpty() ? QString::fromLatin1(kDefaultLanguage) : cur;
        const int idx = language_combo_->findData(effective);
        QSignalBlocker block(language_combo_);
        language_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    if (currency_combo_) {
        const QString cur = currency::CurrencyManager::instance().code();
        const int idx = currency_combo_->findData(cur);
        QSignalBlocker block(currency_combo_);
        currency_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    }
}

} // namespace fincept::screens
