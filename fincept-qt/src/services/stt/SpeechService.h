#pragma once
// SpeechService.h — Speech-to-text with pluggable provider backend.
//
// Architecture:
//   • Public API unchanged: start_listening() / stop_listening() / signals.
//   • Internally selects a provider (Google or Deepgram) based on AppConfig
//     key "voice/provider" (default: "google"). Each provider spawns its own
//     Python script as a long-running QProcess using the same JSON-lines
//     stdout protocol ({status|text|error|fatal}).
//   • AiChatBubble never sees the provider — it only sees the three signals.
//
// Complies with P1 (never block UI), P6 (service layer), P15 (thread safety).
// Uses its own QProcess (not PythonRunner) because PythonRunner is designed
// for short-lived scripts with a single callback — this is a long-running
// streaming process, same pattern as ExchangeService's WebSocket process.

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>

#include <atomic>
#include <memory>

namespace fincept::services {

/// Abstract base for STT providers. Both providers share the Python-QProcess
/// + JSON-lines parsing plumbing; subclasses only contribute the script path
/// and any extra environment variables (e.g. API keys, model).
class SttProvider : public QObject {
    Q_OBJECT
  public:
    explicit SttProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~SttProvider() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_active() const noexcept = 0;
    [[nodiscard]] virtual QString name() const = 0;

  signals:
    void transcription(const QString& text);
    void error(const QString& message);
    void fatal_error(const QString& message);
    void active_changed(bool active);
};

class SpeechService : public QObject {
    Q_OBJECT

  public:
    static SpeechService& instance();

    SpeechService(const SpeechService&) = delete;
    SpeechService& operator=(const SpeechService&) = delete;

    void start_listening();
    void stop_listening();

    /// Picks up a new provider selection or Deepgram settings immediately.
    /// If currently listening, stops the active session and restarts with
    /// the new provider. If idle, just swaps the provider so the next
    /// start_listening() uses it. Safe to call at any time.
    void reload_config();

    [[nodiscard]] bool is_listening() const noexcept;

    /// Returns the currently active provider id ("google" or "deepgram").
    /// Reads AppConfig; does not reflect in-flight state.
    [[nodiscard]] static QString configured_provider();

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

    void install_provider();
    void teardown_provider();

    std::unique_ptr<SttProvider> provider_;
    std::atomic<bool> listening_{false};
};

} // namespace fincept::services
