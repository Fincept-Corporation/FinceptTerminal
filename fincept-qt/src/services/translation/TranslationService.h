#pragma once
// TranslationService — batched, cached, LLM-backed text translation.
//
// Designed for the news pipeline: feed it dozens of English headlines and
// it returns Chinese (or any other target locale) on a Qt signal, with
// repeats served from the SQLite cache so the same headline never round-
// trips to the LLM twice.
//
// Threading model:
//   • request_translation() and request_batch() may be called from the UI
//     thread; they queue work and return immediately.
//   • The actual LLM call runs on a QtConcurrent worker thread (LlmService
//     manages its own QNAM/threading internally).
//   • `translations_ready` is emitted on the UI thread once results have
//     been merged into the cache.
//
// Empty / pre-translated inputs are short-circuited (returned as-is via the
// signal in the same batch). Caller responsibility: re-render the affected
// rows when the signal arrives.

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept::services {

class TranslationService : public QObject {
    Q_OBJECT
  public:
    static TranslationService& instance();

    /// True when an LLM provider is configured AND the active UI language
    /// is non-English. The news code checks this before requesting work.
    bool is_active() const;

    /// Current target language code (Qt locale string, e.g. "zh_CN") — empty
    /// when translation is disabled.
    QString target_lang() const;

    /// Cheap synchronous lookup. Returns the cached translation if present,
    /// or an empty QString. Does NOT enqueue work — pair with request_*.
    QString cached(const QString& source) const;

    /// Synchronous lookup, but also enqueues a background LLM call for
    /// future delivery if the source is not cached. `domain` tags the entry
    /// in the cache (e.g. "news.headline"); used for stats and bulk
    /// invalidation only.
    ///
    /// Returns the cached translation if present, or "" otherwise.
    QString request_translation(const QString& source, const QString& domain);

    /// Batch variant — enqueue many sources at once. The signal fires when
    /// the worker has translated at least one source from this batch (the
    /// signal payload always contains source→translation pairs for the
    /// caller to look up).
    void request_batch(const QStringList& sources, const QString& domain);

    /// Drop the entire in-memory + on-disk cache for a domain. Useful when
    /// the user changes provider or target language and wants a clean slate.
    void invalidate_domain(const QString& domain);

  signals:
    /// Emitted on the UI thread after a batch lands. Keys are the original
    /// source strings; values are translations (may be empty on LLM failure
    /// — caller should treat as "skip, leave source as-is").
    void translations_ready(QHash<QString, QString> results);

    /// Emitted when target language changes (proxied from LanguageManager).
    void target_lang_changed(QString lang);

  private:
    TranslationService();
    Q_DISABLE_COPY(TranslationService)

    void on_language_changed(const QString& code);

    /// Drain pending_ into a snapshot, fire one LLM call, then hand the
    /// results to apply_results() on the GUI thread. Runs on a QtConcurrent
    /// worker thread. MUST NOT touch QSqlDatabase — see apply_results().
    void flush();

    /// Schedules flush() after a short coalescing window so multiple
    /// request_* calls during a single news refresh get batched.
    void schedule_flush();

    /// GUI-thread sequel to flush(). Performs the SQLite cache writes,
    /// updates memory_cache_, and emits translations_ready. Kept on the
    /// main thread because the per-thread QSqlDatabase connection that a
    /// pooled worker would otherwise acquire becomes a use-after-free
    /// hazard during app shutdown.
    void apply_results(const QHash<QString, QString>& results,
                       const QHash<QString, QString>& snapshot, const QStringList& sources,
                       const QString& target, const QString& provider, const QString& model);

    /// Parse `{"src": "trans", ...}` shaped JSON from the model — robust
    /// against Markdown fences and stray prose.
    static QHash<QString, QString> parse_llm_json(const QString& body, const QStringList& sources);

    mutable QMutex mutex_;
    QString target_lang_;       // "" when disabled
    QHash<QString, QString> pending_;  // source → domain
    QHash<QString, QString> memory_cache_; // source → translated; mirrors SQLite
    QSet<QString> in_flight_;   // sources already shipped to LLM, awaiting reply
    QTimer* flush_timer_ = nullptr;
    bool flushing_ = false;
};

} // namespace fincept::services
