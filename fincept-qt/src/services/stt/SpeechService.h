#pragma once
// SpeechService.h — Speech-to-text via Python speech_recognition + Google API.
//
// Architecture:
//   • start_listening() spawns scripts/voice/speech_to_text.py as a QProcess
//   • Python script captures mic audio, sends to Google STT
//   • Results stream back as JSON lines on stdout
//   • stop_listening() kills the process
//
// Complies with P1 (never block UI), P6 (service layer), P15 (thread safety).
// Uses its own QProcess (not PythonRunner) because PythonRunner is designed
// for short-lived scripts with a single callback — this is a long-running
// streaming process, same pattern as ExchangeService's WebSocket process.

#include <QObject>
#include <QProcess>
#include <QString>

#include <atomic>

namespace fincept::services {

class SpeechService : public QObject {
    Q_OBJECT

  public:
    static SpeechService& instance();

    SpeechService(const SpeechService&) = delete;
    SpeechService& operator=(const SpeechService&) = delete;

    void start_listening();
    void stop_listening();

    [[nodiscard]] bool is_listening() const noexcept;

  signals:
    /// Fired on the UI thread with the transcribed text.
    void transcription_ready(const QString& text);

    /// Fired when listening state changes — UI can update mic button.
    void listening_changed(bool active);

    /// Fired on recoverable or fatal errors.
    void error_occurred(const QString& message);

  private:
    explicit SpeechService(QObject* parent = nullptr);
    ~SpeechService() override;

    void spawn_process();
    void kill_process();
    void on_stdout_ready();
    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    void parse_line(const QByteArray& line);

    QProcess* process_ = nullptr;
    QByteArray stdout_buffer_;
    std::atomic<bool> listening_{false};
};

} // namespace fincept::services
