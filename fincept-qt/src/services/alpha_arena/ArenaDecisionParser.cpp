#include "services/alpha_arena/ArenaDecisionParser.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace fincept::arena {
namespace {
QJsonObject extract_object(const QString& raw) {
    auto doc = QJsonDocument::fromJson(raw.trimmed().toUtf8());
    if (doc.isObject()) return doc.object();
    static const QRegularExpression fence(QStringLiteral("```(?:json)?\\s*(\\{.*?\\})\\s*```"),
                                          QRegularExpression::DotMatchesEverythingOption);
    if (auto m = fence.match(raw); m.hasMatch()) {
        doc = QJsonDocument::fromJson(m.captured(1).toUtf8());
        if (doc.isObject()) return doc.object();
    }
    // First balanced top-level {...} scan. String-aware: braces inside JSON
    // string values (e.g. a reason of "unmatched } here") must not count.
    const qsizetype start = raw.indexOf('{');
    if (start >= 0) {
        int depth = 0;
        bool in_str = false, esc = false;
        for (qsizetype i = start; i < raw.size(); ++i) {
            const QChar ch = raw[i];
            if (in_str) {
                if (esc) esc = false;
                else if (ch == QLatin1Char('\\')) esc = true;
                else if (ch == QLatin1Char('"')) in_str = false;
                continue;
            }
            if (ch == QLatin1Char('"')) in_str = true;
            else if (ch == QLatin1Char('{')) ++depth;
            else if (ch == QLatin1Char('}') && --depth == 0) {
                doc = QJsonDocument::fromJson(raw.mid(start, i - start + 1).toUtf8());
                if (doc.isObject()) return doc.object();
                break;
            }
        }
    }
    return {};
}
} // namespace

ParsedDecision parse_decision(const QString& raw, const QStringList& instruments, double max_leverage) {
    ParsedDecision out;
    const QJsonObject obj = extract_object(raw);
    if (obj.isEmpty()) { out.error = QStringLiteral("no JSON object found in response"); return out; }
    const QJsonArray arr = obj.value(QStringLiteral("actions")).toArray();
    if (arr.isEmpty()) { out.error = QStringLiteral("\"actions\" missing or empty"); return out; }
    for (const auto& v : arr) {
        const QJsonObject a = v.toObject();
        AgentAction act;
        const QString kind = a.value(QStringLiteral("action")).toString().toLower();
        act.reason = a.value(QStringLiteral("reason")).toString();
        act.coin = a.value(QStringLiteral("coin")).toString().toUpper();
        if (kind == QLatin1String("hold")) { act.kind = ActionKind::Hold; out.actions.append(act); continue; }
        if (kind != QLatin1String("open") && kind != QLatin1String("close")) {
            out.error = QStringLiteral("unknown action \"%1\"").arg(kind); out.actions.clear(); return out;
        }
        if (!instruments.contains(act.coin)) {
            out.error = QStringLiteral("coin \"%1\" not in competition instruments").arg(act.coin);
            out.actions.clear(); return out;
        }
        if (kind == QLatin1String("close")) {
            act.kind = ActionKind::Close;
            act.size_usd = a.value(QStringLiteral("size_usd")).toDouble(0);
            out.actions.append(act); continue;
        }
        act.kind = ActionKind::Open;
        act.side = a.value(QStringLiteral("side")).toString().toLower();
        act.size_usd = a.value(QStringLiteral("size_usd")).toDouble();
        act.leverage = a.value(QStringLiteral("leverage")).toDouble(1);
        if (act.side != QLatin1String("long") && act.side != QLatin1String("short")) {
            out.error = QStringLiteral("invalid side \"%1\"").arg(act.side); out.actions.clear(); return out;
        }
        if (act.size_usd <= 0) { out.error = QStringLiteral("size_usd must be > 0"); out.actions.clear(); return out; }
        if (act.leverage < 1 || act.leverage > max_leverage) {
            out.error = QStringLiteral("leverage %1 outside [1, %2]").arg(act.leverage).arg(max_leverage);
            out.actions.clear(); return out;
        }
        out.actions.append(act);
    }
    return out;
}

QString ParsedDecision::actions_json() const {
    QJsonArray arr;
    for (const auto& a : actions) {
        QJsonObject o;
        o["action"] = a.kind == ActionKind::Open ? "open" : a.kind == ActionKind::Close ? "close" : "hold";
        if (!a.coin.isEmpty()) o["coin"] = a.coin;
        if (a.kind == ActionKind::Open) { o["side"] = a.side; o["size_usd"] = a.size_usd; o["leverage"] = a.leverage; }
        if (a.kind == ActionKind::Close && a.size_usd > 0) o["size_usd"] = a.size_usd;
        o["reason"] = a.reason;
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}
} // namespace fincept::arena
