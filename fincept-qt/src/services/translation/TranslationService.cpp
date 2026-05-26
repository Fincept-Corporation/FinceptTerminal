#include "services/translation/TranslationService.h"

#include "core/i18n/LanguageManager.h"
#include "core/logging/Logger.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/TranslationCacheRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrent>

namespace fincept::services {

namespace {

// Coalesce window for batching requests fired in quick succession by the
// news pipeline. Short enough to feel instant, long enough to merge a full
// page of headlines into one round-trip.
constexpr int kFlushDelayMs = 350;
constexpr int kMaxBatch = 60;

QString lang_to_native_name(const QString& code) {
    // The LLM does better with prose target names than Qt locale codes.
    if (code == QLatin1String("zh_CN")) return QStringLiteral("Simplified Chinese (zh_CN)");
    if (code == QLatin1String("zh_HK")) return QStringLiteral("Traditional Chinese (Hong Kong)");
    if (code == QLatin1String("zh_TW")) return QStringLiteral("Traditional Chinese (Taiwan)");
    if (code == QLatin1String("id_ID")) return QStringLiteral("Indonesian");
    if (code == QLatin1String("vi_VN")) return QStringLiteral("Vietnamese");
    if (code == QLatin1String("tr_TR")) return QStringLiteral("Turkish");
    if (code == QLatin1String("de_DE")) return QStringLiteral("German");
    if (code == QLatin1String("pt_BR")) return QStringLiteral("Portuguese (Brazil)");
    if (code == QLatin1String("es_ES")) return QStringLiteral("Spanish");
    if (code == QLatin1String("fr_FR")) return QStringLiteral("French");
    if (code == QLatin1String("it_IT")) return QStringLiteral("Italian");
    return code;
}

QString build_system_prompt(const QString& target_lang) {
    return QStringLiteral(
        "You are a translator for a professional financial terminal. "
        "Translate every input source string to %1 using terminology a "
        "Bloomberg/Reuters user would expect.\n"
        "Rules:\n"
        "  * Output ONLY a JSON object whose keys are the EXACT source "
        "strings (verbatim, byte-for-byte) and values are translations.\n"
        "  * Preserve tickers, ISO currency codes, numbers, percentages, "
        "and product names ($AAPL, USD, S&P 500, ETF) in their original "
        "form.\n"
        "  * Preserve placeholders (%1, %2, {0}, %s) and HTML/markup tags.\n"
        "  * Keep translations concise — same length class as the source "
        "when possible.\n"
        "  * Do not add commentary, Markdown fences, or extra keys.")
        .arg(lang_to_native_name(target_lang));
}

QString build_user_message(const QStringList& sources) {
    QJsonArray arr;
    for (const auto& s : sources)
        arr.append(s);
    return QStringLiteral("Translate these headlines / strings:\n") +
           QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

} // namespace

TranslationService& TranslationService::instance() {
    static TranslationService s;
    return s;
}

TranslationService::TranslationService() : QObject(nullptr) {
    flush_timer_ = new QTimer(this);
    flush_timer_->setSingleShot(true);
    flush_timer_->setInterval(kFlushDelayMs);
    connect(flush_timer_, &QTimer::timeout, this, [this]() {
        // Run the LLM call off the GUI thread.
        (void)QtConcurrent::run([this]() { flush(); });
    });

    // Track the user's language choice. LanguageManager is initialised
    // before any service in main.cpp, so its signal is safe to wire here.
    const QString init_code = i18n::LanguageManager::instance().current_language();
    on_language_changed(init_code);
    connect(&i18n::LanguageManager::instance(), &i18n::LanguageManager::language_changed, this,
            &TranslationService::on_language_changed);
}

bool TranslationService::is_active() const {
    QMutexLocker lock(&mutex_);
    if (target_lang_.isEmpty())
        return false;
    return ai_chat::LlmService::instance().is_configured();
}

QString TranslationService::target_lang() const {
    QMutexLocker lock(&mutex_);
    return target_lang_;
}

QString TranslationService::cached(const QString& source) const {
    QMutexLocker lock(&mutex_);
    auto it = memory_cache_.constFind(source);
    if (it != memory_cache_.constEnd())
        return it.value();
    return {};
}

QString TranslationService::request_translation(const QString& source, const QString& domain) {
    if (source.trimmed().isEmpty())
        return source;

    QString target;
    {
        QMutexLocker lock(&mutex_);
        target = target_lang_;
    }
    if (target.isEmpty())
        return {};

    // 1. Memory cache hit?
    {
        QMutexLocker lock(&mutex_);
        auto it = memory_cache_.constFind(source);
        if (it != memory_cache_.constEnd())
            return it.value();
    }

    // 2. SQLite cache hit? (slow path — promote to memory.)
    const QString hash = TranslationCacheRepository::hash_source(source);
    if (auto disk = TranslationCacheRepository::instance().lookup(hash, target)) {
        QMutexLocker lock(&mutex_);
        memory_cache_.insert(source, *disk);
        return *disk;
    }

    // 3. Enqueue an LLM request for next batch.
    {
        QMutexLocker lock(&mutex_);
        if (!in_flight_.contains(source))
            pending_.insert(source, domain);
    }
    schedule_flush();
    return {};
}

void TranslationService::request_batch(const QStringList& sources, const QString& domain) {
    for (const auto& s : sources)
        (void)request_translation(s, domain);
}

void TranslationService::invalidate_domain(const QString& domain) {
    auto r = TranslationCacheRepository::instance().clear_domain(domain);
    if (r.is_err()) {
        LOG_WARN("Translation", QStringLiteral("clear_domain(%1) failed: %2")
                                     .arg(domain, QString::fromStdString(r.error())));
    }
    QMutexLocker lock(&mutex_);
    memory_cache_.clear();
}

void TranslationService::on_language_changed(const QString& code) {
    QString new_target;
    if (code != QLatin1String("en") && !code.isEmpty())
        new_target = code;

    {
        QMutexLocker lock(&mutex_);
        if (new_target == target_lang_)
            return;
        target_lang_ = new_target;
        // Translations belong to the previous target — purge to avoid
        // serving stale strings while waiting for fresh ones.
        memory_cache_.clear();
        pending_.clear();
        in_flight_.clear();
    }
    emit target_lang_changed(new_target);
}

void TranslationService::schedule_flush() {
    // Restart timer on every enqueue so we naturally coalesce bursts.
    QMetaObject::invokeMethod(flush_timer_, qOverload<>(&QTimer::start), Qt::QueuedConnection);
}

void TranslationService::flush() {
    QString target;
    QHash<QString, QString> snapshot; // source → domain
    {
        QMutexLocker lock(&mutex_);
        if (flushing_)
            return;
        if (pending_.isEmpty())
            return;
        target = target_lang_;
        if (target.isEmpty()) {
            pending_.clear();
            return;
        }
        if (!ai_chat::LlmService::instance().is_configured()) {
            // Drain so we don't grow unboundedly while the user configures.
            pending_.clear();
            return;
        }
        // Move at most kMaxBatch entries into the snapshot.
        auto it = pending_.begin();
        while (it != pending_.end() && snapshot.size() < kMaxBatch) {
            snapshot.insert(it.key(), it.value());
            in_flight_.insert(it.key());
            it = pending_.erase(it);
        }
        flushing_ = true;
    }

    const QStringList sources = snapshot.keys();
    const QString system_prompt = build_system_prompt(target);
    const QString user_msg = build_user_message(sources);

    // Use the non-streaming, no-tools path — translation is a one-shot call.
    auto& llm = ai_chat::LlmService::instance();
    std::vector<ai_chat::ConversationMessage> history;
    history.push_back({QStringLiteral("system"), system_prompt});

    ai_chat::LlmResponse resp = llm.chat(user_msg, history, /*use_tools=*/false);

    QHash<QString, QString> results;
    if (resp.success && !resp.content.isEmpty()) {
        results = parse_llm_json(resp.content, sources);
    } else if (!resp.error.isEmpty()) {
        LOG_WARN("Translation",
                 QStringLiteral("LLM translate failed (%1 sources): %2").arg(sources.size()).arg(resp.error));
    }

    // Persist + memoize successful translations.
    const QString provider = llm.active_provider();
    const QString model = llm.active_model();
    auto& repo = TranslationCacheRepository::instance();
    {
        QMutexLocker lock(&mutex_);
        for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
            if (it.value().isEmpty())
                continue;
            memory_cache_.insert(it.key(), it.value());
            in_flight_.remove(it.key());
            const QString domain = snapshot.value(it.key());
            auto r = repo.put(it.key(), /*src_lang=*/QString(), target, domain, it.value(), provider, model);
            if (r.is_err()) {
                LOG_WARN("Translation", QStringLiteral("cache put failed: %1")
                                             .arg(QString::fromStdString(r.error())));
            }
        }
        // Anything we didn't get a translation for: leave out of cache but
        // also out of in_flight_ so a future request can retry.
        for (const auto& src : sources) {
            if (!results.contains(src) || results.value(src).isEmpty())
                in_flight_.remove(src);
        }
        flushing_ = false;
    }

    if (!results.isEmpty()) {
        // Marshal back to the GUI thread for downstream model updates.
        QMetaObject::invokeMethod(
            this, [this, results]() { emit translations_ready(results); }, Qt::QueuedConnection);
    }

    // If more entries piled up while we were waiting, keep going.
    {
        QMutexLocker lock(&mutex_);
        if (!pending_.isEmpty()) {
            QMetaObject::invokeMethod(flush_timer_, qOverload<>(&QTimer::start), Qt::QueuedConnection);
        }
    }
}

QHash<QString, QString> TranslationService::parse_llm_json(const QString& body,
                                                           const QStringList& sources) {
    QHash<QString, QString> out;

    // Strip optional Markdown fences (```json ... ```).
    QString text = body.trimmed();
    static const QRegularExpression fence(R"(^```(?:json)?\s*|\s*```$)",
                                          QRegularExpression::MultilineOption);
    text.remove(fence);

    // Some models prefix prose before the JSON. Find the outermost {...}.
    const int open = text.indexOf('{');
    const int close = text.lastIndexOf('}');
    if (open < 0 || close <= open)
        return out;
    const QString json_str = text.mid(open, close - open + 1);

    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(json_str.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return out;

    const QJsonObject obj = doc.object();
    for (const auto& src : sources) {
        if (obj.contains(src)) {
            const QString v = obj.value(src).toString().trimmed();
            if (!v.isEmpty())
                out.insert(src, v);
        }
    }
    return out;
}

} // namespace fincept::services
