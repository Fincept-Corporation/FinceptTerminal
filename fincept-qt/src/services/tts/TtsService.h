#pragma once
// TtsService.h — Pluggable text-to-speech with provider abstraction.
//
// Architecture mirrors SpeechService:
//   TtsService (public facade)
//     └── TtsProvider (abstract — emits speaking_started/finished/error)
//           ├── Pyttsx3TtsProvider   → scripts/voice/tts.py          (free, offline)
//           └── DeepgramTtsProvider  → scripts/voice/deepgram_tts.py (Aura-2)
//
// Provider is selected via AppConfig key "voice/tts/provider"
// (default: "pyttsx3"). The choice is independent of the STT provider —
// users can mix Google STT with Deepgram TTS or vice versa.
//
// One process per utterance. The script reads text from stdin (until EOF),
// emits {"status":"speaking"|"done"} / {"error"|"fatal":...}.

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>

#include <atomic>
#include <memory>

namespace fincept::services {

/// Abstract base for TTS providers. Both providers share the QProcess + JSON
/// stdout parsing plumbing implemented in the .cpp file; subclasses contribute
/// the script path and any extra environment variables (API keys, voices).
class TtsProvider : public QObject {
    Q_OBJECT
  public:
    explicit TtsProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~TtsProvider() override = default;

    virtual void speak(const QString& text) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_active() const noexcept = 0;
    [[nodiscard]] virtual QString name() const = 0;

  signals:
    void speaking_started();
    void speaking_finished();
    void error_occurred(const QString& message);
};

class TtsService : public QObject {
    Q_OBJECT

  public:
    static TtsService& instance();

    TtsService(const TtsService&) = delete;
    TtsService& operator=(const TtsService&) = delete;

    /// Speak `text`. Cancels any in-flight utterance first.
    void speak(const QString& text);

    /// Cancel any in-flight utterance.
    void stop();

    /// Re-read provider/config and rebuild the active provider if needed.
    /// Called by VoiceConfigSection after the user saves new settings.
    void reload_config();

    [[nodiscard]] bool is_speaking() const noexcept;

    /// Returns "pyttsx3" or "deepgram" — current AppConfig setting.
    [[nodiscard]] static QString configured_provider();

  signals:
    void speaking_started();
    void speaking_finished();
    void error_occurred(const QString& message);

  private:
    explicit TtsService(QObject* parent = nullptr);
    ~TtsService() override;

    void install_provider();
    void teardown_provider();

    std::unique_ptr<TtsProvider> provider_;
    std::atomic<bool>            speaking_{false};
};

} // namespace fincept::services
