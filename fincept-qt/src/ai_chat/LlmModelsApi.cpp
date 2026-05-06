// LlmModelsApi.cpp — dynamic model discovery for each supported provider.
// Builds the right URL + headers per provider, then runs an async GET on a
// worker thread and emits models_fetched on completion.

#include "ai_chat/LlmService.h"

#include "storage/repositories/SettingsRepository.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

namespace fincept::ai_chat {

QString LlmService::get_models_url(const QString& provider, const QString& api_key, const QString& base_url) {
    Q_UNUSED(api_key) // Auth goes via headers (see get_models_headers()), URL is provider-based.
    const QString p = provider.toLower();

    // Custom base_url (except fincept)
    if (!base_url.isEmpty() && p != "fincept") {
        QString base = base_url;
        if (base.endsWith('/'))
            base.chop(1);
        if (p == "anthropic")
            return base + "/v1/models?limit=1000";
        return base + "/v1/models";
    }

    if (p == "openai")     return "https://api.openai.com/v1/models";
    if (p == "anthropic")  return "https://api.anthropic.com/v1/models?limit=1000";
    if (p == "gemini" || p == "google") {
        // Auth goes via x-goog-api-key header (set by get_models_headers()).
        return "https://generativelanguage.googleapis.com/v1beta/models?pageSize=100";
    }
    if (p == "groq")       return "https://api.groq.com/openai/v1/models";
    if (p == "deepseek")   return "https://api.deepseek.com/models";
    if (p == "openrouter") return "https://openrouter.ai/api/v1/models";
    if (p == "ollama") {
        QString base = base_url.isEmpty() ? "http://localhost:11434" : base_url;
        if (base.endsWith('/'))
            base.chop(1);
        return base + "/api/tags";
    }
    if (p == "xai")     return "https://api.x.ai/v1/models";
    if (p == "kimi")    return "https://api.moonshot.ai/v1/models";
    if (p == "fincept") return "https://api.fincept.in/research/llm/models";
    // minimax: no public /v1/models endpoint — fallback models used instead
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
        // Key is in URL query param OR header
        if (!api_key.isEmpty())
            h["x-goog-api-key"] = api_key;
    } else if (p == "ollama") {
        // No auth needed
    } else if (p == "fincept") {
        // Resolve API key from session (same logic as ensure_config)
        QString resolved_key = api_key;
        if (resolved_key.isEmpty()) {
            auto stored = SettingsRepository::instance().get("fincept_api_key");
            if (stored.is_ok() && !stored.value().isEmpty())
                resolved_key = stored.value();
        }
        if (!resolved_key.isEmpty())
            h["X-API-Key"] = resolved_key;
    } else {
        // OpenAI-compatible: OpenAI, Groq, DeepSeek, OpenRouter
        if (!api_key.isEmpty())
            h["Authorization"] = "Bearer " + api_key;
    }
    return h;
}

QStringList LlmService::parse_models_response(const QString& provider, const QByteArray& body) {
    QStringList models;
    auto doc = QJsonDocument::fromJson(body);
    if (doc.isNull() || !doc.isObject())
        return models;
    QJsonObject root = doc.object();
    const QString p = provider.toLower();

    if (p == "gemini" || p == "google") {
        // {"models": [{"name": "models/gemini-...", "supportedGenerationMethods": [...]}]}
        QJsonArray arr = root["models"].toArray();
        for (const auto& v : arr) {
            QJsonObject m = v.toObject();
            // Only include models that support generateContent
            QJsonArray methods = m["supportedGenerationMethods"].toArray();
            bool can_generate = false;
            for (const auto& method : methods) {
                if (method.toString() == "generateContent") {
                    can_generate = true;
                    break;
                }
            }
            if (!can_generate)
                continue;

            QString name = m["name"].toString();
            // Strip "models/" prefix
            if (name.startsWith("models/"))
                name = name.mid(7);
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == "ollama") {
        // {"models": [{"name": "llama3:8b", ...}]}
        QJsonArray arr = root["models"].toArray();
        for (const auto& v : arr) {
            QString name = v.toObject()["name"].toString();
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == "fincept") {
        // {"success":true,"data":{"models":["MiniMax-M2.7",...]}}
        // or {"success":true,"data":["model1","model2",...]}
        QJsonValue data_val = root["data"];
        QJsonArray arr;
        if (data_val.isObject())
            arr = data_val.toObject()["models"].toArray();
        else if (data_val.isArray())
            arr = data_val.toArray();
        for (const auto& v : arr) {
            QString id = v.isString() ? v.toString() : v.toObject()["id"].toString();
            if (!id.isEmpty())
                models.append(id);
        }
        // If API doesn't expose a models endpoint yet, fall back to known models
        if (models.isEmpty())
            models = {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5"};
    } else {
        // OpenAI-compatible: OpenAI, Anthropic, Groq, DeepSeek, OpenRouter
        // {"data": [{"id": "model-id", ...}]}
        QJsonArray arr = root["data"].toArray();
        for (const auto& v : arr) {
            QString id = v.toObject()["id"].toString();
            if (!id.isEmpty())
                models.append(id);
        }
    }

    models.sort(Qt::CaseInsensitive);
    return models;
}

void LlmService::fetch_models(const QString& provider, const QString& api_key, const QString& base_url) {
    // Fincept has no public /models listing endpoint — return known models immediately.
    if (provider.toLower() == "fincept") {
        emit models_fetched(provider,
                            {"MiniMax-M2.7", "MiniMax-M2.7-highspeed", "MiniMax-M2.5", "MiniMax-M2.5-highspeed"}, {});
        return;
    }

    QString url = get_models_url(provider, api_key, base_url);
    if (url.isEmpty()) {
        emit models_fetched(provider, {}, "Unknown provider: " + provider);
        return;
    }

    auto headers = get_models_headers(provider, api_key);

    QPointer<LlmService> self = this;
    (void)QtConcurrent::run([self, provider, url, headers]() {
        // Blocking GET (reuse blocking_post pattern but with GET)
        QNetworkAccessManager nam;
        QNetworkRequest req{QUrl(url)};
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
            req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

        QNetworkReply* reply = nam.get(req);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(15000);
        loop.exec();

        QString error;
        QStringList models;

        if (!timer.isActive()) {
            reply->abort();
            error = "Request timed out";
        } else {
            timer.stop();
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                error = reply->errorString();
                if (error.isEmpty())
                    error = "HTTP " + QString::number(status);
            } else {
                models = parse_models_response(provider, reply->readAll());
                if (models.isEmpty())
                    error = "No models found";
            }
        }

        reply->deleteLater();

        if (self) {
            QMetaObject::invokeMethod(
                self,
                [self, provider, models, error]() {
                    if (self)
                        emit self->models_fetched(provider, models, error);
                },
                Qt::QueuedConnection);
        }
    });
}

} // namespace fincept::ai_chat
