#pragma once
// ClapDetectorService.h — Background "clap to start" mic listener.
//
// Spawns scripts/voice/clap_detector.py as a long-running QProcess that
// monitors the default microphone for a clap (or double-clap), and emits
// `clap_detected()` on the UI thread when one is heard.
//
// Privacy: the Python script analyses raw PCM in-process and never sends
// audio anywhere. This service just observes the JSON-line event stream.
//
// Lifecycle (driven by AiChatBubble):
//   • Bubble constructor reads `voice/clap_to_start/enabled`. If true,
//     calls start().
//   • Before AiChatBubble starts STT, it calls stop() to free the mic and
//     avoid the clap detector reacting to the user's own speech.
//   • After STT finishes, it calls start() again if the feature is still
//     enabled.

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QString>

#include <atomic>

namespace fincept::services {

class ClapDetectorService : public QObject {
    Q_OBJECT

  public:
    static ClapDetectorService& instance();

    ClapDetectorService(const ClapDetectorService&) = delete;
    ClapDetectorService& operator=(const ClapDetectorService&) = delete;

    /// Start the background listener (no-op if already running). Reads
    /// AppConfig keys for sensitivity / mode.
    void start();

    /// Stop the listener and close the mic stream. Idempotent.
    void stop();

    [[nodiscard]] bool is_active() const noexcept;

    /// Convenience helper for callers — reads AppConfig key
    /// "voice/clap_to_start/enabled" (default false).
    [[nodiscard]] static bool is_enabled_in_config();

  signals:
    /// Fired on the UI thread when a clap event is detected. The bubble
    /// connects this to start_listening().
    void clap_detected();

    /// Recoverable / fatal errors — logged + surfaced if a UI listener wants
    /// to display them. Most users will never see this.
    void error_occurred(const QString& message);

    /// Mic is open and the script is actively analysing audio.
    void listening_changed(bool active);

  private:
    explicit ClapDetectorService(QObject* parent = nullptr);
    ~ClapDetectorService() override;

    void on_stdout_ready();
    void on_stderr_ready();
    void on_process_finished(int exit_code, QProcess::ExitStatus status);
    void parse_line(const QByteArray& line);

    QPointer<QProcess> process_;
    QByteArray         stdout_buffer_;
    std::atomic<bool>  active_{false};
};

} // namespace fincept::services
