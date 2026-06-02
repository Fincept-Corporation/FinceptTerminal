// VoiceConfigSection.cpp — Voice / STT + TTS provider configuration panel.
//
// Two independent provider selectors share a single Deepgram credentials
// block. The credentials block is hidden entirely when neither provider is
// using Deepgram. STT/TTS-specific tunables are only shown when their
// respective provider is set to Deepgram.

#include "screens/settings/VoiceConfigSection.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QDir>
#include <QFileInfo>
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

// ── Style helpers ────────────────────────────────────────────────────────────

static QString label_ss() {
    return QString("color:%1;background:transparent;").arg(ui::colors::TEXT_SECONDARY());
}
static QString title_ss() {
    return QString("color:%1;font-weight:bold;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::AMBER());
}
static QString subtitle_ss() {
    return QString("color:%1;font-weight:700;letter-spacing:0.5px;background:transparent;")
        .arg(ui::colors::TEXT_SECONDARY());
}
static QString input_ss() {
    return QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QLineEdit:focus{border:1px solid %4;}"
                   "QDoubleSpinBox{background:%1;color:%2;border:1px solid %3;padding:6px;}"
                   "QDoubleSpinBox:focus{border:1px solid %4;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER());
}
static QString combo_ss() {
    return QString(
               "QComboBox{background:%1;color:%2;border:1px solid %3;padding:5px 8px;min-width:180px;}"
               "QComboBox:focus{border:1px solid %4;}"
               "QComboBox::drop-down{border:none;width:20px;}"
               "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%5;border:1px solid %3;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_MED(), ui::colors::AMBER(),
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
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BORDER_BRIGHT(), ui::colors::BG_HOVER());
}

// Helper to build a labelled row [label | stretch | control]. When
// label_out is non-null it receives the inner QLabel so its text can be
// re-applied on a live language switch.
static QWidget* labelled_row(const QString& label_text, QWidget* control, QLabel** label_out = nullptr) {
    auto* row = new QWidget;
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    auto* lbl = new QLabel(label_text);
    lbl->setStyleSheet(label_ss());
    hl->addWidget(lbl);
    hl->addStretch();
    hl->addWidget(control);
    if (label_out)
        *label_out = lbl;
    return row;
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
    title_lbl_ = new QLabel(tr("VOICE — SPEECH-TO-TEXT & TEXT-TO-SPEECH"));
    title_lbl_->setStyleSheet(title_ss());
    vl->addWidget(title_lbl_);

    blurb_lbl_ = new QLabel(
        tr("Pick a provider for each direction independently. Free options work "
           "out of the box; Deepgram requires an API key but offers higher accuracy "
           "(STT) and natural-sounding voices (TTS)."));
    blurb_lbl_->setStyleSheet(label_ss());
    blurb_lbl_->setWordWrap(true);
    vl->addWidget(blurb_lbl_);

    // ── STT provider ─────────────────────────────────────────────────────────
    stt_section_lbl_ = new QLabel(tr("SPEECH-TO-TEXT (microphone -> text)"));
    stt_section_lbl_->setStyleSheet(subtitle_ss());
    vl->addWidget(stt_section_lbl_);

    stt_provider_combo_ = new QComboBox;
    stt_provider_combo_->setStyleSheet(combo_ss());
    stt_provider_combo_->addItem(tr("Google (free, default)"), "google");
    stt_provider_combo_->addItem(tr("Deepgram (API key required)"), "deepgram");
    vl->addWidget(labelled_row(tr("Provider"), stt_provider_combo_, &stt_provider_row_lbl_));

    // STT Deepgram-only rows
    stt_dg_group_ = new QWidget;
    auto* stt_dg_vl = new QVBoxLayout(stt_dg_group_);
    stt_dg_vl->setContentsMargins(0, 0, 0, 0);
    stt_dg_vl->setSpacing(10);

    stt_model_combo_ = new QComboBox;
    stt_model_combo_->setStyleSheet(combo_ss());
    stt_model_combo_->addItem(tr("nova-3 (recommended)"), "nova-3");
    stt_model_combo_->addItem("nova-2", "nova-2");
    stt_model_combo_->addItem(tr("enhanced"), "enhanced");
    stt_model_combo_->addItem(tr("base"), "base");
    stt_dg_vl->addWidget(labelled_row(tr("STT Model"), stt_model_combo_, &stt_model_row_lbl_));

    stt_language_combo_ = new QComboBox;
    stt_language_combo_->setStyleSheet(combo_ss());
    stt_language_combo_->addItem(tr("English (auto)"), "en");
    stt_language_combo_->addItem(tr("English (US)"), "en-US");
    stt_language_combo_->addItem(tr("English (UK)"), "en-GB");
    stt_language_combo_->addItem(tr("Multilingual (nova-3 only)"), "multi");
    stt_dg_vl->addWidget(labelled_row(tr("Language"), stt_language_combo_, &stt_language_row_lbl_));

    keyterms_edit_ = new QLineEdit;
    keyterms_edit_->setPlaceholderText(tr("AAPL, BTCUSD, Nifty (comma-separated)"));
    keyterms_edit_->setStyleSheet(input_ss());
    keyterms_edit_->setMinimumWidth(320);
    stt_dg_vl->addWidget(labelled_row(tr("Key terms"), keyterms_edit_, &keyterms_row_lbl_));

    gain_spin_ = new QDoubleSpinBox;
    gain_spin_->setRange(1.0, 10.0);
    gain_spin_->setSingleStep(0.5);
    gain_spin_->setDecimals(1);
    gain_spin_->setSuffix("x");
    gain_spin_->setStyleSheet(input_ss());
    gain_spin_->setMinimumWidth(120);
    stt_dg_vl->addWidget(labelled_row(tr("Mic gain (raise if mic is quiet)"), gain_spin_, &gain_row_lbl_));

    device_edit_ = new QLineEdit;
    device_edit_->setPlaceholderText(tr("e.g. 'Headset' (substring match, blank = system default)"));
    device_edit_->setStyleSheet(input_ss());
    device_edit_->setMinimumWidth(320);
    stt_dg_vl->addWidget(labelled_row(tr("Mic device"), device_edit_, &device_row_lbl_));

    vl->addWidget(stt_dg_group_);

    // ── TTS provider ─────────────────────────────────────────────────────────
    tts_section_lbl_ = new QLabel(tr("TEXT-TO-SPEECH (assistant reply -> spoken audio)"));
    tts_section_lbl_->setStyleSheet(subtitle_ss());
    vl->addWidget(tts_section_lbl_);

    tts_provider_combo_ = new QComboBox;
    tts_provider_combo_->setStyleSheet(combo_ss());
    tts_provider_combo_->addItem(tr("Free / pyttsx3 (offline, default)"), "pyttsx3");
    tts_provider_combo_->addItem(tr("Deepgram Aura-2 (API key required)"), "deepgram");
    vl->addWidget(labelled_row(tr("Provider"), tts_provider_combo_, &tts_provider_row_lbl_));

    // TTS Deepgram-only rows
    tts_dg_group_ = new QWidget;
    auto* tts_dg_vl = new QVBoxLayout(tts_dg_group_);
    tts_dg_vl->setContentsMargins(0, 0, 0, 0);
    tts_dg_vl->setSpacing(10);

    tts_model_combo_ = new QComboBox;
    tts_model_combo_->setStyleSheet(combo_ss());
    // Aura-2 voice catalogue (subset — most popular). Format the label as
    // "Display name (gender)" so users know what they're picking.
    tts_model_combo_->addItem(tr("Thalia — female, American"),     "aura-2-thalia-en");
    tts_model_combo_->addItem(tr("Helena — female, American"),     "aura-2-helena-en");
    tts_model_combo_->addItem(tr("Andromeda — female, American"),  "aura-2-andromeda-en");
    tts_model_combo_->addItem(tr("Apollo — male, American"),       "aura-2-apollo-en");
    tts_model_combo_->addItem(tr("Orion — male, American"),        "aura-2-orion-en");
    tts_model_combo_->addItem(tr("Arcas — male, American"),        "aura-2-arcas-en");
    tts_model_combo_->addItem(tr("Aurora — female, American"),     "aura-2-aurora-en");
    tts_model_combo_->addItem(tr("Luna — female, American"),       "aura-2-luna-en");
    tts_model_combo_->addItem(tr("Zeus — male, American"),         "aura-2-zeus-en");
    tts_dg_vl->addWidget(labelled_row(tr("Voice"), tts_model_combo_, &tts_voice_row_lbl_));

    vl->addWidget(tts_dg_group_);

    // ── Shared Deepgram credentials block ────────────────────────────────────
    deepgram_group_ = new QWidget;
    auto* dg_vl = new QVBoxLayout(deepgram_group_);
    dg_vl->setContentsMargins(0, 8, 0, 0);
    dg_vl->setSpacing(10);

    dg_title_lbl_ = new QLabel(tr("DEEPGRAM CREDENTIALS (shared by STT + TTS)"));
    dg_title_lbl_->setStyleSheet(subtitle_ss());
    dg_vl->addWidget(dg_title_lbl_);

    // API key row with show/hide toggle
    auto* key_row = new QWidget;
    auto* key_hl = new QHBoxLayout(key_row);
    key_hl->setContentsMargins(0, 0, 0, 0);
    key_lbl_ = new QLabel(tr("API Key"));
    key_lbl_->setStyleSheet(label_ss());
    api_key_edit_ = new QLineEdit;
    api_key_edit_->setEchoMode(QLineEdit::Password);
    api_key_edit_->setPlaceholderText(tr("Paste your Deepgram API key"));
    api_key_edit_->setStyleSheet(input_ss());
    api_key_edit_->setMinimumWidth(320);
    show_key_btn_ = new QPushButton(tr("Show"));
    show_key_btn_->setStyleSheet(btn_secondary_ss());
    show_key_btn_->setCheckable(true);
    key_hl->addWidget(key_lbl_);
    key_hl->addStretch();
    key_hl->addWidget(api_key_edit_);
    key_hl->addWidget(show_key_btn_);
    dg_vl->addWidget(key_row);

    vl->addWidget(deepgram_group_);

    // ── Clap-to-start (independent — works with Google or Deepgram) ──────────
    clap_section_lbl_ = new QLabel(tr("WAKE TRIGGER"));
    clap_section_lbl_->setStyleSheet(subtitle_ss());
    vl->addWidget(clap_section_lbl_);

    clap_enabled_cb_ = new QCheckBox(tr("Clap to open mic"));
    clap_enabled_cb_->setStyleSheet(QString("QCheckBox{color:%1;background:transparent;}"
                                            "QCheckBox::indicator{width:14px;height:14px;}"
                                            "QCheckBox::indicator:unchecked{border:1px solid %2;background:%3;}"
                                            "QCheckBox::indicator:checked{border:1px solid %4;background:%4;}")
                                        .arg(ui::colors::TEXT_PRIMARY(),
                                             ui::colors::BORDER_MED(),
                                             ui::colors::BG_RAISED(),
                                             ui::colors::AMBER()));
    vl->addWidget(clap_enabled_cb_);

    clap_blurb_lbl_ = new QLabel(
        tr("Listens to the mic in the background and opens the chat bubble when "
           "you clap. Audio is analysed locally and never sent over the network."));
    clap_blurb_lbl_->setStyleSheet(label_ss());
    clap_blurb_lbl_->setWordWrap(true);
    vl->addWidget(clap_blurb_lbl_);

    clap_mode_combo_ = new QComboBox;
    clap_mode_combo_->setStyleSheet(combo_ss());
    clap_mode_combo_->addItem(tr("Double clap (recommended)"), "double");
    clap_mode_combo_->addItem(tr("Single clap (more reactive, more false positives)"), "single");
    vl->addWidget(labelled_row(tr("Trigger"), clap_mode_combo_, &clap_trigger_row_lbl_));

    // ── Save / Test row ──────────────────────────────────────────────────────
    auto* btn_row = new QWidget;
    auto* btn_hl = new QHBoxLayout(btn_row);
    btn_hl->setContentsMargins(0, 8, 0, 0);
    save_btn_ = new QPushButton(tr("Save"));
    save_btn_->setStyleSheet(btn_primary_ss());
    test_btn_ = new QPushButton(tr("Test Deepgram key"));
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

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect(stt_provider_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &VoiceConfigSection::on_provider_changed);
    connect(tts_provider_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &VoiceConfigSection::on_provider_changed);
    connect(show_key_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_show_hide_key);
    connect(save_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_save);
    connect(test_btn_, &QPushButton::clicked, this, &VoiceConfigSection::on_test);
}

// ── Load / Save ──────────────────────────────────────────────────────────────

void VoiceConfigSection::reload() {
    auto& cfg = AppConfig::instance();

    // STT provider — read canonical key, fall back to legacy "voice/provider".
    QString stt = cfg.get("voice/stt/provider", "").toString().toLower();
    if (stt.isEmpty())
        stt = cfg.get("voice/provider", "google").toString().toLower();
    int idx = stt_provider_combo_->findData(stt);
    stt_provider_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    // TTS provider
    const QString tts = cfg.get("voice/tts/provider", "pyttsx3").toString().toLower();
    idx = tts_provider_combo_->findData(tts);
    tts_provider_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    // Shared credentials
    auto key_res = SecureStorage::instance().retrieve(kSecureKey);
    api_key_edit_->setText(key_res.is_ok() ? key_res.value() : QString());

    // STT-Deepgram tunables
    const QString stt_model = cfg.get("voice/deepgram/model", "nova-3").toString();
    idx = stt_model_combo_->findData(stt_model);
    stt_model_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    const QString lang = cfg.get("voice/deepgram/language", "en").toString();
    idx = stt_language_combo_->findData(lang);
    stt_language_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    keyterms_edit_->setText(cfg.get("voice/deepgram/keyterms", "").toString());
    gain_spin_->setValue(cfg.get("voice/deepgram/gain", 3.0).toDouble());
    device_edit_->setText(cfg.get("voice/deepgram/device", "").toString());

    // TTS-Deepgram tunables
    const QString tts_model = cfg.get("voice/deepgram/tts_model", "aura-2-thalia-en").toString();
    idx = tts_model_combo_->findData(tts_model);
    tts_model_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    // Clap-to-start
    clap_enabled_cb_->setChecked(cfg.get("voice/clap_to_start/enabled", false).toBool());
    const QString clap_mode = cfg.get("voice/clap_to_start/mode", "double").toString();
    idx = clap_mode_combo_->findData(clap_mode);
    clap_mode_combo_->setCurrentIndex(idx >= 0 ? idx : 0);

    apply_provider_visibility();
    set_status({}, false);
}

void VoiceConfigSection::apply_provider_visibility() {
    const QString stt = stt_provider_combo_->currentData().toString();
    const QString tts = tts_provider_combo_->currentData().toString();
    const bool stt_dg = (stt == "deepgram");
    const bool tts_dg = (tts == "deepgram");
    const bool any_dg = stt_dg || tts_dg;

    stt_dg_group_->setVisible(stt_dg);
    tts_dg_group_->setVisible(tts_dg);
    deepgram_group_->setVisible(any_dg);
    test_btn_->setVisible(any_dg);
}

void VoiceConfigSection::on_provider_changed() {
    apply_provider_visibility();
}

void VoiceConfigSection::on_show_hide_key() {
    if (show_key_btn_->isChecked()) {
        api_key_edit_->setEchoMode(QLineEdit::Normal);
        show_key_btn_->setText(tr("Hide"));
    } else {
        api_key_edit_->setEchoMode(QLineEdit::Password);
        show_key_btn_->setText(tr("Show"));
    }
}

void VoiceConfigSection::on_save() {
    auto& cfg = AppConfig::instance();
    const QString stt = stt_provider_combo_->currentData().toString();
    const QString tts = tts_provider_combo_->currentData().toString();

    cfg.set("voice/stt/provider", stt);
    cfg.set("voice/tts/provider", tts);
    // Clear legacy key so it doesn't shadow future reads.
    cfg.set("voice/provider", stt);

    cfg.set("voice/deepgram/model",     stt_model_combo_->currentData().toString());
    cfg.set("voice/deepgram/language",  stt_language_combo_->currentData().toString());
    cfg.set("voice/deepgram/keyterms",  keyterms_edit_->text().trimmed());
    cfg.set("voice/deepgram/gain",      QString::number(gain_spin_->value(), 'f', 1));
    cfg.set("voice/deepgram/device",    device_edit_->text().trimmed());
    cfg.set("voice/deepgram/tts_model", tts_model_combo_->currentData().toString());

    cfg.set("voice/clap_to_start/enabled", clap_enabled_cb_->isChecked());
    cfg.set("voice/clap_to_start/mode",    clap_mode_combo_->currentData().toString());

    const QString api_key = api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        auto r = SecureStorage::instance().remove(kSecureKey);
        if (r.is_err())
            LOG_WARN(TAG, QString("Failed to clear Deepgram key: %1").arg(QString::fromStdString(r.error())));
    } else {
        auto r = SecureStorage::instance().store(kSecureKey, api_key);
        if (r.is_err()) {
            set_status(tr("Failed to save API key: ") + QString::fromStdString(r.error()), true);
            return;
        }
    }

    LOG_INFO(TAG, QString("Voice config saved (stt=%1 tts=%2)").arg(stt, tts));
    set_status(tr("Saved. Changes apply on next voice session."), false);
    emit config_changed();
}

// ── Test connection ──────────────────────────────────────────────────────────

void VoiceConfigSection::on_test() {
    const QString api_key = api_key_edit_->text().trimmed();
    if (api_key.isEmpty()) {
        set_status(tr("Enter an API key first."), true);
        return;
    }

    // Step 1 — verify the Python SDK is installed in venv-numpy2 (only
    // matters for STT; TTS uses requests + sounddevice, which are already
    // present). Done via a filesystem probe of the venv site-packages.
    const QString stt = stt_provider_combo_->currentData().toString();
    if (stt == "deepgram") {
        const QString venv_root =
            python::PythonSetupManager::instance().install_dir() + "/venv-numpy2";
#ifdef _WIN32
        const QString sp = venv_root + "/Lib/site-packages/deepgram";
#else
        QString sp;
        QDir lib_dir(venv_root + "/lib");
        const auto py_dirs = lib_dir.entryList({"python3.*"}, QDir::Dirs);
        if (!py_dirs.isEmpty())
            sp = lib_dir.filePath(py_dirs.first()) + "/site-packages/deepgram";
#endif
        if (sp.isEmpty() || !QFileInfo::exists(sp)) {
            set_status(
                tr("Deepgram Python SDK is missing from venv-numpy2. "
                   "Open Settings -> Python Env -> Reinstall packages, then test again."),
                true);
            return;
        }
    }

    set_status(tr("Testing Deepgram key…"), false);
    test_btn_->setEnabled(false);

    // Lightweight verification: GET /v1/projects — succeeds iff the key is
    // valid. No audio sent, no billing impact. Same key authorises both
    // /v1/listen (STT) and /v1/speak (TTS).
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
            self->set_status(tr("Deepgram key is valid (STT + TTS available)."), false);
        } else if (status == 401) {
            self->set_status(tr("Invalid API key (401)."), true);
        } else if (status == 402) {
            self->set_status(tr("Deepgram credits exhausted (402)."), true);
        } else {
            self->set_status(tr("Test failed: HTTP %1 — %2").arg(status).arg(reply->errorString()), true);
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

void VoiceConfigSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void VoiceConfigSection::retranslateUi() {
    // Section titles + blurbs.
    if (title_lbl_) title_lbl_->setText(tr("VOICE — SPEECH-TO-TEXT & TEXT-TO-SPEECH"));
    if (blurb_lbl_)
        blurb_lbl_->setText(tr("Pick a provider for each direction independently. Free options work "
                               "out of the box; Deepgram requires an API key but offers higher accuracy "
                               "(STT) and natural-sounding voices (TTS)."));

    // STT section.
    if (stt_section_lbl_)      stt_section_lbl_->setText(tr("SPEECH-TO-TEXT (microphone -> text)"));
    if (stt_provider_row_lbl_) stt_provider_row_lbl_->setText(tr("Provider"));
    if (stt_model_row_lbl_)    stt_model_row_lbl_->setText(tr("STT Model"));
    if (stt_language_row_lbl_) stt_language_row_lbl_->setText(tr("Language"));
    if (keyterms_row_lbl_)     keyterms_row_lbl_->setText(tr("Key terms"));
    if (gain_row_lbl_)         gain_row_lbl_->setText(tr("Mic gain (raise if mic is quiet)"));
    if (device_row_lbl_)       device_row_lbl_->setText(tr("Mic device"));

    if (keyterms_edit_) keyterms_edit_->setPlaceholderText(tr("AAPL, BTCUSD, Nifty (comma-separated)"));
    if (device_edit_)   device_edit_->setPlaceholderText(tr("e.g. 'Headset' (substring match, blank = system default)"));

    if (stt_provider_combo_) {
        stt_provider_combo_->setItemText(0, tr("Google (free, default)"));
        stt_provider_combo_->setItemText(1, tr("Deepgram (API key required)"));
    }
    if (stt_model_combo_) {
        stt_model_combo_->setItemText(0, tr("nova-3 (recommended)"));
        stt_model_combo_->setItemText(2, tr("enhanced"));
        stt_model_combo_->setItemText(3, tr("base"));
    }
    if (stt_language_combo_) {
        stt_language_combo_->setItemText(0, tr("English (auto)"));
        stt_language_combo_->setItemText(1, tr("English (US)"));
        stt_language_combo_->setItemText(2, tr("English (UK)"));
        stt_language_combo_->setItemText(3, tr("Multilingual (nova-3 only)"));
    }

    // TTS section.
    if (tts_section_lbl_)      tts_section_lbl_->setText(tr("TEXT-TO-SPEECH (assistant reply -> spoken audio)"));
    if (tts_provider_row_lbl_) tts_provider_row_lbl_->setText(tr("Provider"));
    if (tts_voice_row_lbl_)    tts_voice_row_lbl_->setText(tr("Voice"));

    if (tts_provider_combo_) {
        tts_provider_combo_->setItemText(0, tr("Free / pyttsx3 (offline, default)"));
        tts_provider_combo_->setItemText(1, tr("Deepgram Aura-2 (API key required)"));
    }
    if (tts_model_combo_) {
        tts_model_combo_->setItemText(0, tr("Thalia — female, American"));
        tts_model_combo_->setItemText(1, tr("Helena — female, American"));
        tts_model_combo_->setItemText(2, tr("Andromeda — female, American"));
        tts_model_combo_->setItemText(3, tr("Apollo — male, American"));
        tts_model_combo_->setItemText(4, tr("Orion — male, American"));
        tts_model_combo_->setItemText(5, tr("Arcas — male, American"));
        tts_model_combo_->setItemText(6, tr("Aurora — female, American"));
        tts_model_combo_->setItemText(7, tr("Luna — female, American"));
        tts_model_combo_->setItemText(8, tr("Zeus — male, American"));
    }

    // Deepgram credentials block.
    if (dg_title_lbl_) dg_title_lbl_->setText(tr("DEEPGRAM CREDENTIALS (shared by STT + TTS)"));
    if (key_lbl_)      key_lbl_->setText(tr("API Key"));
    if (api_key_edit_) api_key_edit_->setPlaceholderText(tr("Paste your Deepgram API key"));
    // Show/Hide button reflects current echo mode — keep state, not a stale label.
    if (show_key_btn_)
        show_key_btn_->setText(show_key_btn_->isChecked() ? tr("Hide") : tr("Show"));

    // Wake trigger / clap.
    if (clap_section_lbl_) clap_section_lbl_->setText(tr("WAKE TRIGGER"));
    if (clap_enabled_cb_)  clap_enabled_cb_->setText(tr("Clap to open mic"));
    if (clap_blurb_lbl_)
        clap_blurb_lbl_->setText(tr("Listens to the mic in the background and opens the chat bubble when "
                                    "you clap. Audio is analysed locally and never sent over the network."));
    if (clap_trigger_row_lbl_) clap_trigger_row_lbl_->setText(tr("Trigger"));
    if (clap_mode_combo_) {
        clap_mode_combo_->setItemText(0, tr("Double clap (recommended)"));
        clap_mode_combo_->setItemText(1, tr("Single clap (more reactive, more false positives)"));
    }

    // Action buttons.
    if (save_btn_) save_btn_->setText(tr("Save"));
    if (test_btn_) test_btn_->setText(tr("Test Deepgram key"));
}

} // namespace fincept::screens
