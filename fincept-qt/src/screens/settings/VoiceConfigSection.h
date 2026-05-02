#pragma once
// VoiceConfigSection.h — Voice / speech configuration panel.
//
// Two independent provider selectors:
//   • Speech-to-Text (STT)  — Google (free, default) | Deepgram
//   • Text-to-Speech (TTS)  — Pyttsx3 (free, default) | Deepgram (Aura-2)
//
// One shared Deepgram credentials block (API key + tunables) only shown when
// at least one of the two providers is set to Deepgram.

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class VoiceConfigSection : public QWidget {
    Q_OBJECT
  public:
    explicit VoiceConfigSection(QWidget* parent = nullptr);

    /// Reload values from AppConfig and SecureStorage.
    void reload();

  signals:
    /// Emitted after Save — parent wires this to refresh both SpeechService
    /// and TtsService.
    void config_changed();

  private slots:
    void on_provider_changed();
    void on_show_hide_key();
    void on_save();
    void on_test();

  private:
    void build_ui();
    void apply_provider_visibility();
    void set_status(const QString& msg, bool error = false);

    // Provider selectors (independent)
    QComboBox*   stt_provider_combo_ = nullptr;
    QComboBox*   tts_provider_combo_ = nullptr;

    // Deepgram-shared credentials
    QLineEdit*   api_key_edit_      = nullptr;
    QPushButton* show_key_btn_      = nullptr;

    // Deepgram STT tunables
    QComboBox*       stt_model_combo_    = nullptr;
    QComboBox*       stt_language_combo_ = nullptr;
    QLineEdit*       keyterms_edit_      = nullptr;
    QDoubleSpinBox*  gain_spin_          = nullptr;
    QLineEdit*       device_edit_        = nullptr;

    // Deepgram TTS tunables
    QComboBox*   tts_model_combo_   = nullptr;

    // Clap-to-start (independent of STT/TTS provider — works with any)
    QCheckBox*   clap_enabled_cb_   = nullptr;
    QComboBox*   clap_mode_combo_   = nullptr;

    // Action buttons
    QPushButton* save_btn_          = nullptr;
    QPushButton* test_btn_          = nullptr;
    QLabel*      status_lbl_        = nullptr;

    // Container groups for visibility toggling
    QWidget*     deepgram_group_    = nullptr;   // shared key + buttons
    QWidget*     stt_dg_group_      = nullptr;   // Deepgram STT-only rows
    QWidget*     tts_dg_group_      = nullptr;   // Deepgram TTS-only rows
};

} // namespace fincept::screens
