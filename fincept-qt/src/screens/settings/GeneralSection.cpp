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
[[maybe_unused]] constexpr const char* kKeyLanguage      = "general.language";
constexpr const char* kDefaultLanguage  = "en";

// make_row() builds the label (and optional description) QLabel internally and
// returns only the row widget. To re-translate those at runtime we grab them
// back from the row's direct child QLabels. Construction order is deterministic:
// title label first, description label (when present) second.
void capture_row_labels(QWidget* row, QLabel** label_out, QLabel** desc_out = nullptr) {
    const auto labels = row->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    if (!labels.isEmpty() && label_out)
        *label_out = labels.at(0);
    if (labels.size() > 1 && desc_out)
        *desc_out = labels.at(1);
}
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
    window_title_ = new QLabel(tr("WINDOW BEHAVIOUR"));
    window_title_->setStyleSheet(section_title_ss());
    vl->addWidget(window_title_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    on_close_combo_ = new QComboBox;
    on_close_combo_->addItem(tr("Quit application"),   QStringLiteral("quit"));
    on_close_combo_->addItem(tr("Show Launchpad"),     QStringLiteral("show_launchpad"));
    on_close_combo_->setStyleSheet(combo_ss());
    auto* on_close_row = make_row(
        tr("On last window close"),
        on_close_combo_,
        tr("Default is Quit. Choose 'Show Launchpad' if you want a small portal "
           "window to stay open after closing your last terminal window."));
    capture_row_labels(on_close_row, &on_close_label_, &on_close_desc_);
    vl->addWidget(on_close_row);

    connect(on_close_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString val = on_close_combo_->itemData(idx).toString();
                SettingsRepository::instance().set(
                    QString::fromLatin1(kKeyOnClose), val, QStringLiteral("general"));
                LOG_INFO("Settings", QString("on_last_window_close → %1").arg(val));
            });

    vl->addSpacing(20);

    // ── LANGUAGE ──────────────────────────────────────────────────────────────
    lang_title_ = new QLabel(tr("LANGUAGE"));
    lang_title_->setStyleSheet(section_title_ss());
    vl->addWidget(lang_title_);
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
    auto* language_row = make_row(
        tr("Interface language"),
        language_combo_,
        tr("Changes apply immediately. English is the source language; all other "
           "translations are embedded with the build."));
    capture_row_labels(language_row, &language_label_, &language_desc_);
    vl->addWidget(language_row);

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
    currency_title_ = new QLabel(tr("CURRENCY"));
    currency_title_->setStyleSheet(section_title_ss());
    vl->addWidget(currency_title_);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    currency_combo_ = new QComboBox;
    for (const auto& c : currency::CurrencyManager::available())
        currency_combo_->addItem(QString("%1  %2  (%3)").arg(c.symbol, c.name, c.code), c.code);
    currency_combo_->setStyleSheet(combo_ss());
    auto* currency_row = make_row(
        tr("Display currency"),
        currency_combo_,
        tr("Changes the currency symbol on calculators and analytics. Live market "
           "data keeps its own currency — values are not converted."));
    capture_row_labels(currency_row, &currency_label_, &currency_desc_);
    vl->addWidget(currency_row);

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

void GeneralSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void GeneralSection::retranslateUi() {
    // Section titles.
    if (window_title_)   window_title_->setText(tr("WINDOW BEHAVIOUR"));
    if (lang_title_)     lang_title_->setText(tr("LANGUAGE"));
    if (currency_title_) currency_title_->setText(tr("CURRENCY"));

    // Window-behaviour combo items are fixed UI labels (data stays the same).
    if (on_close_combo_) {
        const QSignalBlocker block(on_close_combo_);
        const int quit_idx = on_close_combo_->findData(QStringLiteral("quit"));
        if (quit_idx >= 0) on_close_combo_->setItemText(quit_idx, tr("Quit application"));
        const int launch_idx = on_close_combo_->findData(QStringLiteral("show_launchpad"));
        if (launch_idx >= 0) on_close_combo_->setItemText(launch_idx, tr("Show Launchpad"));
    }

    // Row labels + descriptions.
    if (on_close_label_) on_close_label_->setText(tr("On last window close"));
    if (on_close_desc_)
        on_close_desc_->setText(tr("Default is Quit. Choose 'Show Launchpad' if you want a small portal "
                                   "window to stay open after closing your last terminal window."));

    if (language_label_) language_label_->setText(tr("Interface language"));
    if (language_desc_)
        language_desc_->setText(tr("Changes apply immediately. English is the source language; all other "
                                   "translations are embedded with the build."));

    if (currency_label_) currency_label_->setText(tr("Display currency"));
    if (currency_desc_)
        currency_desc_->setText(tr("Changes the currency symbol on calculators and analytics. Live market "
                                   "data keeps its own currency — values are not converted."));
}

} // namespace fincept::screens
