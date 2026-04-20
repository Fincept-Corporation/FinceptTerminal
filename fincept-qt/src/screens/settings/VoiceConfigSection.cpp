// VoiceConfigSection.cpp — Voice / STT provider configuration panel.

#include "screens/settings/VoiceConfigSection.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

static constexpr const char* TAG = "VoiceConfigSection";
static constexpr const char* kSecureKey = "voice.deepgram.api_key";

// ── Style helpers (match SettingsScreen / LlmConfigSection look) ─────────────

static QString label_ss() {
    return QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY());
}
static QString title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::AMBER());
}
static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QLineEdit:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER());
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

// ── Construction ─────────────────────────────────────────────────────────────

VoiceConfigSection::VoiceConfigSection(QWidget* parent) : QWidget(parent) {
    build_ui();
    reload();
}

void VoiceConfigSection::build_ui() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea{border:none;background:transparent;}"
                                  "QScrollBar:vertical{width:6px;background:transparent;}"
                                  "QScrollBar::handle:vertical{background:%1;border-radius:3px;}"
                                  "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
                              .arg(ui::colors::BORDER_MED()));

    auto* page = new QWidget;
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(14);

    // ── Title + blurb ────────────────────────────────────────────────────────
    auto* title = new QLabel("VOICE / SPEECH-TO-TEXT");
    title->setStyleSheet(title_ss());
    vl->addWidget(title);

    auto* blurb = new QLabel(
        "Configure the speech recognition engine used by the AI chat mic button.\n"
        "Google is free but unauthenticated and often rate-limited. Deepgram offers "
        "significantly better accuracy and supports financial keyword boosting.");
    blurb->setStyleSheet(label_ss());
    blurb->setWordWrap(true);
    vl->addWidget(blurb);

    // ── Provider selector ───────────────────────────────────────────────────
    auto* prov_row = new QWidget;
    auto* prov_hl = new QHBoxLayout(prov_row);
    prov_hl->setContentsMargins(0, 0, 0, 0);
    auto* prov_lbl = new QLabel("Provider");
    prov_lbl->setStyleSheet(label_ss());
    provider_combo_ = new QComboBox;
    provider_combo_->setStyleSheet(combo_ss());
    provider_combo_->addItem("Google (free, default)", "google");
    provider_combo_->addItem("Deepgram (API key required)", "deepgram");
    prov_hl->addWidget(prov_lbl);
    prov_hl->addStretch();
    prov_hl->addWidget(provider_combo_);
    vl->addWidget(prov_row);

    // ── Deepgram group (hidden for Google) ───────────────────────────────────
    deepgram_group_ = new QWidget;
    auto* dg_vl = new QVBoxLayout(deepgram_group_);
    dg_vl->setContentsMargins(0, 8, 0, 0);
    dg_vl->setSpacing(10);

    auto* dg_title = new QLabel("DEEPGRAM SETTINGS");
    dg_title->setStyleSheet(QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
                                .arg(ui::colors::TEXT_SECONDARY()));
    dg_vl->addWidget(dg_title);

    // API key row with show/hide toggle
    auto* key_row = new QWidget;
    auto* key_hl = new QHBoxLayout(key_row);
    key_hl->setContentsMargins(0, 0, 0, 0);
    auto* key_lbl = new QLabel("API Key");
    key_lbl->setStyleSheet(label_ss());
    api_key_edit_ = new QLineEdit;
    api_key_edit_->setEchoMode(QLineEdit::Password);
    api_key_edit_->setPlaceholderText("Paste your Deepgram API key");
    api_key_edit_->setStyleSheet(input_ss());
    api_key_edit_->setMinimumWidth(320);
    show_key_btn_ = new QPushButton("Show");
    show_key_btn_->setStyleSheet(btn_secondary_ss());
    show_key_btn_->setCheckable(true);
    key_hl->addWidget(key_lbl);
    key_hl->addStretch();
    key_hl->addWidget(api_key_edit_);
    key_hl->addWidget(show_key_btn_);
    dg_vl->addWidget(key_row);

    // Model dropdown
    auto* model_row = new QWidget;
    auto* model_hl = new QHBoxLayout(model_row);
    model_hl->setContentsMargins(0, 0, 0, 0);
    auto* model_lbl = new QLabel("Model");
    model_lbl->setStyleSheet(label_ss());
    model_combo_ = new QComboBox;
    model_combo_->setStyleSheet(combo_ss());
    model_combo_->addItem("nova-3 (recommended)", "nova-3");
    model_combo_->addItem("nova-2", "nova-2");
    model_combo_->addItem("enhanced", "enhanced");
    model_combo_->addItem("base", "base");
    model_hl->addWidget(model_lbl);
    model_hl->addStretch();
    model_hl->addWidget(model_combo_);
    dg_vl->addWidget(model_row);

    // Language dropdown
    auto* lang_row = new QWidget;
    auto* lang_hl = new QHBoxLayout(lang_row);
    lang_hl->setContentsMargins(0, 0, 0, 0);
    auto* lang_lbl = new QLabel("Language");
    lang_lbl->setStyleSheet(label_ss());
    language_combo_ = new QComboBox;
    language_combo_->setStyleSheet(combo_ss());
    language_combo_->addItem("English (auto)", "en");
    language_combo_->addItem("English (US)", "en-US");
    language_combo_->addItem("English (UK)", "en-GB");
    language_combo_->addItem("Multilingual (nova-3 only)", "multi");
    lang_hl->addWidget(lang_lbl);
    lang_hl->addStretch();
    lang_hl->addWidget(language_combo_);
    dg_vl->addWidget(lang_row);

    // Keyterms
    auto* kt_row = new QWidget;
    auto* kt_hl = new QHBoxLayout(kt_row);
    kt_hl->setContentsMargins(0, 0, 0, 0);
    auto* kt_lbl = new QLabel("Key terms");
    kt_lbl->setStyleSheet(label_ss());
    keyterms_edit_ = new QLineEdit;
    keyterms_edit_->setPlaceholderText("AAPL, BTCUSD, Nifty (comma-separated)");
    keyterms_edit_->setStyleSheet(input_ss());
    keyterms_edit_->setMinimumWidth(320);
    kt_hl->addWidget(kt_lbl);
    kt_hl->addStretch();
    kt_hl->addWidget(keyterms_edit_);
    dg_vl->addWidget(kt_row);

    auto* kt_hint = new QLabel("Boosts recognition of financial symbols and proper nouns.");
    kt_hint->setStyleSheet(label_ss());
    kt_hint->setWordWrap(true);
    dg_vl->addWidget(kt_hint);

    vl->addWidget(deepgram_group_);

    // ── Save / Test row ─────────────────────────────────────────────────────
    auto* btn_row = new QWidget;
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 8, 0, 0);
    save_btn_ = new QPushButton("Save");
    save_btn_->setStyleSheet(btn_primary_ss());
    test_btn_ = new QPushButton("Test Deepgram key");
    test_btn_->setStyleSheet(btn_secondary_ss());
    btn_hl->addWidget(save_btn_);
    btn_hl->addWidget(test_btn_);
    btn_hl->addStretch();
    vl->addWidget(btn_row);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet(label_ss());
    status_lbl_->setWordWrap(true);
    vl->addWidget(status_lbl_);

    vl->addStretch();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(page);
    outer->addWidget(scroll);

    // ── Wiring ──────────────────────────────────────────────────────────────
    connect(provider_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &VoiceConfigSection::on_provider_changed);
    connect(show_key_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_show_hide_key);
    connect(save_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_save);
    connect(test_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_test);
}

// ── Load / Save ──────────────────────────────────────────────────────────────

void VoiceConfigSection::reload() {
    auto& cfg = AppConfig::instance();

    const QString provider = cfg.get("voice/provider", "google").toString().toLower();
    const int idx = provider_combo_->findData(provider);
    provider_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    // API key from SecureStorage
    auto key_res = SecureStorage::instance().retrieve(kSecureKey);
    api_key_edit_->setText(key_res.is_ok() ? key_res.value() : QString());

    const QString model = cfg.get("voice/deepgram/model", "nova-3").toString();
    const int midx = model_combo_->findData(model);
    model_combo_->setCurrentIndex(midx >= 0 ? midx : 0);

    const QString lang = cfg.get("voice/deepgram/language", "en").toString();
    const int lidx = language_combo_->findData(lang);
    language_combo_->setCurrentIndex(lidx >= 0 ? lidx : 0);

    keyterms_edit_->setText(cfg.get("voice/deepgram/keyterms", "").toString());

    apply_provider_visibility(provider);
    set_status({}, false);
}

void VoiceConfigSection::apply_provider_visibility(const QString& provider) {
    const bool is_dg = (provider == "deepgram");
    deepgram_group_->setVisible(is_dg);
    test_btn_->setVisible(is_dg);
}

void VoiceConfigSection::on_provider_changed(int /*index*/) {
    apply_provider_visibility(provider_combo_->currentData().toString());
}

void VoiceConfigSection::on_show_hide_key() {
    if (show_key_btn_->isChecked()) {
        api_key_edit_->setEchoMode(QLineEdit::Normal);
        show_key_btn_->setText("Hide");
    } else {
        api_key_edit_->setEchoMode(QLineEdit::Password);
        show_key_btn_->setText("Show");
    }
}

void VoiceConfigSection::on_save() {
    auto& cfg = AppConfig::instance();
    const QString provider = provider_combo_->currentData().toString();
    cfg.set("voice/provider", provider);
    cfg.set("voice/deepgram/model", model_combo_->currentData().toString());
    cfg.set("voice/deepgram/language", language_combo_->currentData().toString());
    cfg.set("voice/deepgram/keyterms", keyterms_edit_->text().trimmed());

    const QString api_key = api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        auto r = SecureStorage::instance().remove(kSecureKey);
        if (r.is_err())
            LOG_WARN(TAG, QString("Failed to clear Deepgram key: %1").arg(QString::fromStdString(r.error())));
    } else {
        auto r = SecureStorage::instance().store(kSecureKey, api_key);
        if (r.is_err()) {
            set_status(QStringLiteral("Failed to save API key: ") + QString::fromStdString(r.error()), true);
            return;
        }
    }

    LOG_INFO(TAG, QString("Voice config saved (provider=%1)").arg(provider));
    set_status(QStringLiteral("Saved. Changes apply on next voice session."), false);
    emit config_changed();
}

// ── Test connection ──────────────────────────────────────────────────────────

void VoiceConfigSection::on_test() {
    const QString api_key = api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        set_status("Enter an API key first.", true);
        return;
    }

    set_status("Testing Deepgram key…", false);
    test_btn_->setEnabled(false);

    // Lightweight verification: GET /v1/projects — succeeds iff the key is
    // valid. No audio sent, no billing impact. QNetworkAccessManager scoped
    // to this widget so it auto-cleans on destroy.
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.deepgram.com/v1/projects"));
    req.setRawHeader("Authorization", ("Token " + api_key).toUtf8());
    req.setRawHeader("Accept", "application/json");

    QPointer<VoiceConfigSection> self = this;
    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [self, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (!self)
            return;
        self->test_btn_->setEnabled(true);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            self->set_status("Deepgram key is valid.", false);
        } else if (status == 401) {
            self->set_status("Invalid API key (401).", true);
        } else if (status == 402) {
            self->set_status("Deepgram credits exhausted (402).", true);
        } else {
            self->set_status(QString("Test failed: HTTP %1 — %2").arg(status).arg(reply->errorString()), true);
        }
    });
}

void VoiceConfigSection::set_status(const QString& msg, bool error) {
    if (msg.isEmpty()) {
        status_lbl_->clear();
        return;
    }
    status_lbl_->setText(msg);
    status_lbl_->setStyleSheet(QString("color:%1;background:transparent;")
                                   .arg(error ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
}

} // namespace fincept::screens
