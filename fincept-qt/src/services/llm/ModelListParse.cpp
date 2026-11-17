#include "services/llm/ModelListParse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>

namespace fincept::ai_chat {

ParsedModelList parse_model_list(const QString& provider, const QByteArray& body) {
    ParsedModelList out;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isNull() || !doc.isObject())
        return out;
    out.parsed = true;
    const QJsonObject root = doc.object();
    const QString p = provider.toLower();
    QStringList& models = out.models;

    if (p == QLatin1String("gemini") || p == QLatin1String("google")) {
        const QJsonArray arr = root.value(QLatin1String("models")).toArray();
        for (const QJsonValue& v : arr) {
            const QJsonObject m = v.toObject();
            // Only models that can answer a chat (generateContent).
            const QJsonArray methods = m.value(QLatin1String("supportedGenerationMethods")).toArray();
            bool can_generate = false;
            for (const QJsonValue& method : methods) {
                if (method.toString() == QLatin1String("generateContent")) {
                    can_generate = true;
                    break;
                }
            }
            if (!can_generate)
                continue;
            QString name = m.value(QLatin1String("name")).toString();
            if (name.startsWith(QLatin1String("models/")))
                name = name.mid(7);
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == QLatin1String("ollama")) {
        const QJsonArray arr = root.value(QLatin1String("models")).toArray();
        for (const QJsonValue& v : arr) {
            const QString name = v.toObject().value(QLatin1String("name")).toString();
            if (!name.isEmpty())
                models.append(name);
        }
    } else if (p == QLatin1String("fincept")) {
        // data may be either {"models":[...]} or a direct array.
        const QJsonValue data_val = root.value(QLatin1String("data"));
        QJsonArray arr;
        if (data_val.isObject())
            arr = data_val.toObject().value(QLatin1String("models")).toArray();
        else if (data_val.isArray())
            arr = data_val.toArray();
        for (const QJsonValue& v : arr) {
            const QString id = v.isString() ? v.toString() : v.toObject().value(QLatin1String("id")).toString();
            if (!id.isEmpty())
                models.append(id);
        }
        if (models.isEmpty())
            models = {QStringLiteral("MiniMax-M2.7"), QStringLiteral("MiniMax-M2.7-highspeed"),
                      QStringLiteral("MiniMax-M2.5")};
    } else {
        // OpenAI-compatible: {"data": [{"id": ...}]}.
        const QJsonArray arr = root.value(QLatin1String("data")).toArray();
        models.reserve(arr.size());
        for (const QJsonValue& v : arr) {
            const QString id = v.toObject().value(QLatin1String("id")).toString();
            if (!id.isEmpty())
                models.append(id);
        }
    }

    models.sort(Qt::CaseInsensitive);
    return out;
}

} // namespace fincept::ai_chat
