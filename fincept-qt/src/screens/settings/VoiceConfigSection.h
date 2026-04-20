#pragma once
// VoiceConfigSection.h — Voice / speech-to-text provider configuration panel.
//
// Provider switch (Google free vs Deepgram API) + Deepgram credentials &
// tuning. Lives alongside LlmConfigSection in the Settings screen.

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class VoiceConfigSection : public QWidget {
    Q_OBJECT
  public:
    explicit VoiceConfigSection(QWidget* parent = nullptr);

    /// Called when section becomes visible — reloads values from
    /// AppConfig and SecureStorage.
    void reload();

  signals:
    /// Emitted after user saves — SpeechService picks up new config on
    /// its next start_listening() call (no live restart needed).
    void config_changed();

  private slots:
    void on_provider_changed(int index);
    void on_show_hide_key();
    void on_save();
    void on_test();

  private:
    void build_ui();
    void apply_provider_visibility(const QString& provider);
    void set_status(const QString& msg, bool error = false);

    QComboBox*   provider_combo_    = nullptr;
    QLineEdit*   api_key_edit_      = nullptr;
    QPushButton* show_key_btn_      = nullptr;
    QComboBox*   model_combo_       = nullptr;
    QComboBox*   language_combo_    = nullptr;
    QLineEdit*   keyterms_edit_     = nullptr;
    QPushButton* save_btn_          = nullptr;
    QPushButton* test_btn_          = nullptr;
    QLabel*      status_lbl_        = nullptr;

    // Deepgram-only rows, hidden when provider is Google
    QWidget*     deepgram_group_    = nullptr;
};

} // namespace fincept::screens
