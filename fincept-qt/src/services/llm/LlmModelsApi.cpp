// Per-provider model-list discovery. Async GET on the GUI thread (the QNAM needs
// it); the answer — OpenRouter's is over 1 MB — is parsed and shaped on a worker
// (ModelListParse), and models_fetched is emitted back on the GUI thread for the
// newest request of that provider only.

#include "auth/AuthManager.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "services/llm/LlmContentExtractors.h"
#include "services/llm/LlmService.h"
#include "services/llm/ModelListParse.h"
#include "services/llm/ProviderCatalog.h"
#include "storage/repositories/SettingsRepository.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace fincept::ai_chat {

static constexpr const char* TAG = "LlmModelsApi";

namespace {
/// Newest fetch_models() request per provider (lower-case name). GUI thread only:
/// fetch_models() and every delivery run there. An answer for an older request
/// of the same provider is dropped — latest request wins.
QHash<QString, quint64>& llm_models_latest_request() {
    static QHash<QString, quint64> latest;
    return latest;
}

quint64 llm_models_next_request_id() {
    static quint64 last = 0;
    return ++last;
}

bool llm_models_is_latest(const QString& provider_key, quint64 request_id) {
    return llm_models_latest_request().value(provider_key) == request_id;
}
} // namespace

QString LlmService::get_models_url(const QString& provider, const QString& api_key, const QString& base_url) {
    Q_UNUSED(api_key) // Auth in headers, not URL.
    const QString p = provider.toLower();

    // Blocked provider (AtlasCloud, removed) — no models URL, so the Fetch button
    // can never reach it even if base_url was hand-pointed at the host.
    if (ProviderCatalog::is_blocked(p, base_url))
        return {};

    // Ollama uses /api/tags, not /v1/models — must short-circuit before the custom-base_url branch
    // or a user with base_url=http://localhost:11434 hits the wrong path and parses a 404 page.
    if (p == "ollama") {
        QString base = base_url.isEmpty() ? "http://localhost:11434" : base_url;
        if (base.endsWith('/'))
            base.chop(1);
        return base + "/api/tags";
    }

    // Custom base_url (except fincept) — assume OpenAI-compatible /v1/models.
    if (!base_url.isEmpty() && p != "fincept") {
        QString base = base_url;
        while (base.endsWith('/'))
            base.chop(1);

        const QString suffix = (p == "anthropic") ? QStringLiteral("/models?limit=1000") : QStringLiteral("/models");

        // Already a full endpoint — use verbatim.
        if (base.contains(QStringLiteral("/models")))
            return base;

        // Base already includes a version segment (e.g. ".../v1", ".../v1beta") —
        // append only the suffix. Otherwise inject the default "/v1".
        static const QRegularExpression re(QStringLiteral("/v\\d+[a-zA-Z]*$"));
        if (re.match(base).hasMatch())
            return base + suffix;
        return base + QStringLiteral("/v1") + suffix;
    }

    if (p == "openai")
        return "https://api.openai.com/v1/models";
    if (p == "anthropic")
        return "https://api.anthropic.com/v1/models?limit=1000";
    if (p == "gemini" || p == "google") {
        return "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100";
    }
    if (p == "groq")
        return "https://api.groq.com/openai/v1/models";
    if (p == "deepseek")
        return "https://api.deepseek.com/models";
    if (p == "openrouter")
        return "https://openrouter.ai/api/v1/models";
    if (p == "xai")
        return "https://api.x.ai/v1/models";
    if (p == "kimi")
        return "https://api.moonshot.ai/v1/models";
    if (p == "aihubmix")
        return "https://aihubmix.com/v1/models"; // fallback if prefilled base_url was cleared
    // fincept publishes no models endpoint — /research/llm/models is a 404, and
    // /research/llm/async takes no `model` field at all (the backend picks).
    // fetch_models() short-circuits to the known list before reaching here.
    // minimax has no public /v1/models — caller falls back to known models.
    return {};
}

QMap<QString, QString> LlmService::get_models_headers(const QString& provider, const QString& api_key) {
    QMap<QString, QString> h;
    const QString p = provider.toLower();

    if (p == "anthropic") {
        if (!api_key.isEmpty())
            h["x-api-key"] = api_key;
        h["anthropic-version"] = "2023-06-01";
    } else if (p == "gemini" || p == "google") {
        if (!api_key.isEmpty())
            h["x-goog-api-key"] = api_key;
    } else if (p == "ollama") {
        // No auth.
    } else if (p == "fincept") {
        // Same fallback as ensure_config — resolve via AuthManager
        // (session → SecureStorage), never the plaintext settings row (CR-08).
        QString resolved_key = api_key;
        if (resolved_key.isEmpty())
            resolved_key = fincept::auth::AuthManager::instance().fincept_api_key();
        if (!resolved_key.isEmpty())
            h["X-API-Key"] = resolved_key;
    } else {
        // OpenAI-compatible.
        if (!api_key.isEmpty())
            h["Authorization"] = "Bearer " + api_key;
    }
    return h;
}

QStringList LlmService::parse_models_response(const QString& provider, const QByteArray& body) {
    // Pure parse + shaping in ModelListParse (any thread; fetch_models runs it on a worker).
    ParsedModelList parsed = parse_model_list(provider, body);
    if (!parsed.parsed)
        LOG_WARN(TAG, logf("Model list response could not be parsed", {{"provider", provider}, {"bytes", body.size()}}));
    return std::move(parsed.models);
}

void LlmService::fetch_models(const QString& provider, const QString& api_key, const QString& base_url) {
    // This call is now the newest for its provider: any answer still on its way for
    // an earlier call (other key / base URL) is dropped when it arrives.
    const QString provider_key = provider.toLower();
    const quint64 request_id = llm_models_next_request_id();
    llm_models_latest_request().insert(provider_key, request_id);

    // No public /models endpoint for fincept — return known list immediately.
    if (provider.toLower() == "fincept") {
        emit models_fetched(provider,
                            {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"}, {});
        return;
    }

    const QString url = get_models_url(provider, api_key, base_url);
    if (url.isEmpty()) {
        emit models_fetched(provider, {}, "Unknown provider: " + provider);
        return;
    }

    // NAM must live on the main thread — its DNS/TLS path needs a pumping event loop.
    if (!models_nam_)
        models_nam_ = new QNetworkAccessManager(this);

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto headers = get_models_headers(provider, api_key);
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    LOG_DEBUG(TAG, logf("Model list fetch started", {{"provider", provider}, {"url", log_url(url)}}));

    QNetworkReply* reply = models_nam_->get(req);

    reply->setProperty("_provider", provider);
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    timer->start(15000);

    connect(reply, &QNetworkReply::finished, this, [this, reply, provider_key, request_id]() {
        reply->deleteLater();
        const QString p = reply->property("_provider").toString();

        // A newer fetch for this provider was started meanwhile: its answer is
        // the one the settings page waits for (and this one is never parsed).
        if (!llm_models_is_latest(provider_key, request_id)) {
            LOG_DEBUG(TAG, logf("Model list answer dropped, a newer fetch is running", {{"provider", p}}));
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            QString error;
            if (reply->error() == QNetworkReply::OperationCanceledError)
                error = "Request timed out";
            else {
                error = reply->errorString();
                if (error.isEmpty())
                    error = QString("HTTP %1").arg(status);
            }
            // Logged before the provider's message is folded in: that text is for the
            // user (it can echo a masked key fragment), not for the log file. Qt's
            // errorString() embeds the request URL: scrubbed for the log.
            LOG_WARN(TAG, logf("Model list fetch failed", {{"provider", p},
                                                           {"status", status},
                                                           {"qt_error", static_cast<int>(reply->error())},
                                                           {"error", log_scrub(error)}}));
            // Qt's "server replied: Unauthorized" says nothing about WHY — the provider's
            // JSON body usually does ("API key not valid", "model access denied", …).
            if (reply->error() != QNetworkReply::OperationCanceledError) {
                const QString server_msg = parse_server_error_message(reply->readAll());
                if (!server_msg.isEmpty())
                    error = status > 0 ? QString("HTTP %1: %2").arg(status).arg(server_msg) : server_msg;
            }
            emit models_fetched(p, {}, error);
            return;
        }

        // Parse and shape on a worker (OpenRouter's list is over 1 MB); the result
        // comes back to this (GUI) thread and is emitted only while this request
        // is still the newest for its provider.
        QPointer<LlmService> self = this;
        QThreadPool::globalInstance()->start([self, p, provider_key, request_id, body = reply->readAll()]() {
            QStringList models = parse_models_response(p, body); // pure + one log line
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self.data(),
                [self, p, provider_key, request_id, models = std::move(models)]() {
                    if (!self)
                        return;
                    if (!llm_models_is_latest(provider_key, request_id)) {
                        LOG_DEBUG(TAG, logf("Model list answer dropped, a newer fetch is running", {{"provider", p}}));
                        return;
                    }
                    const QString error = models.isEmpty() ? QStringLiteral("No models found") : QString();
                    LOG_DEBUG(TAG, logf("Model list fetched", {{"provider", p}, {"count", models.size()}}));
                    emit self->models_fetched(p, models, error);
                },
                Qt::QueuedConnection);
        });
    });
}

} // namespace fincept::ai_chat
