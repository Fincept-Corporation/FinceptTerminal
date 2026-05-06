#include "services/alpha_arena/AlphaArenaJson.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>

#include <algorithm>

namespace fincept::services::alpha_arena {

namespace {

/// Find the outermost balanced {...} block in `text`. The model is told to
/// emit JSON; in practice it sometimes wraps the JSON in markdown fences,
/// prose, or trailing commentary. We scan for the first `{` and walk forward
/// counting brace depth (skipping strings and escapes) to find the match.
/// Returns empty QString on failure.
QString extract_outer_json_object(const QString& text) {
    int start = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QChar('{')) { start = i; break; }
    }
    if (start < 0) return {};

    int depth = 0;
    bool in_str = false;
    bool escape = false;
    for (int i = start; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (in_str) {
            if (escape)        { escape = false; }
            else if (c == '\\') { escape = true; }
            else if (c == '"')  { in_str = false; }
            continue;
        }
        if (c == '"') { in_str = true; continue; }
        if (c == '{') { ++depth; }
        else if (c == '}') {
            --depth;
            if (depth == 0) {
                return text.mid(start, i - start + 1);
            }
        }
    }
    return {};
}

bool is_known_coin(const QString& coin_upper) {
    return kPerpUniverse().contains(coin_upper);
}

double clamp_unit(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

int clamp_leverage(int v) {
    return std::clamp(v, limits::kMinLeverage, limits::kMaxLeverage);
}

QString truncate(QString s, int max_chars) {
    if (s.size() > max_chars) s.truncate(max_chars);
    return s;
}

/// Try to convert a value to its action representation. Returns nullopt if
/// the action is irrecoverably malformed (caller appends a per-action note
/// to the response's parse_error).
std::optional<ProposedAction> action_from_json(const QJsonObject& obj, QString* defect) {
    ProposedAction a;

    const auto sig_opt = signal_from_wire(obj.value("signal").toString());
    if (!sig_opt) {
        if (defect) *defect = QStringLiteral("unknown signal");
        return std::nullopt;
    }
    a.signal = *sig_opt;

    a.coin = obj.value("coin").toString().toUpper();
    if (!is_known_coin(a.coin)) {
        if (defect) *defect = QStringLiteral("coin not in perp universe: ") + a.coin;
        return std::nullopt;
    }

    a.quantity = obj.value("quantity").toDouble(0.0);
    a.leverage = clamp_leverage(obj.value("leverage").toInt(1));
    a.profit_target = obj.value("profit_target").toDouble(0.0);
    a.stop_loss = obj.value("stop_loss").toDouble(0.0);
    a.invalidation_condition =
        truncate(obj.value("invalidation_condition").toString(),
                 limits::kInvalidationConditionMaxChars);
    a.confidence = clamp_unit(obj.value("confidence").toDouble(0.0));
    a.risk_usd = obj.value("risk_usd").toDouble(0.0);
    a.justification =
        truncate(obj.value("justification").toString(), limits::kJustificationMaxChars);

    // Entry-signal-specific validation.
    const bool is_entry = (a.signal == Signal::BuyToEnter || a.signal == Signal::SellToEnter);
    if (is_entry) {
        if (a.quantity <= 0.0) {
            if (defect) *defect = QStringLiteral("entry signal requires quantity > 0");
            return std::nullopt;
        }
        if (a.profit_target <= 0.0 || a.stop_loss <= 0.0) {
            if (defect) *defect = QStringLiteral("entry signal requires profit_target and stop_loss > 0");
            return std::nullopt;
        }
    }

    return a;
}

} // namespace

ModelResponse parse_model_response(const QString& raw_text) {
    ModelResponse out;
    out.raw_text = raw_text;

    const QString json_text = extract_outer_json_object(raw_text);
    if (json_text.isEmpty()) {
        out.parse_error = QStringLiteral("no JSON object found in model output");
        return out;
    }

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json_text.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) {
        out.parse_error = QStringLiteral("invalid JSON: ") + err.errorString();
        return out;
    }

    const QJsonObject root = doc.object();
    const QJsonValue actions_val = root.value("actions");
    if (!actions_val.isArray()) {
        out.parse_error = QStringLiteral("missing or non-array \"actions\" field");
        return out;
    }

    QStringList per_action_defects;
    for (const auto& v : actions_val.toArray()) {
        if (!v.isObject()) {
            per_action_defects << QStringLiteral("non-object action entry");
            continue;
        }
        QString defect;
        if (auto a = action_from_json(v.toObject(), &defect)) {
            out.actions.append(*a);
        } else if (!defect.isEmpty()) {
            per_action_defects << defect;
        }
    }

    if (out.actions.isEmpty() && !per_action_defects.isEmpty()) {
        out.parse_error = QStringLiteral("no valid actions: ") + per_action_defects.join(QStringLiteral("; "));
    } else if (!per_action_defects.isEmpty()) {
        // Some survived — surface the partial defect set so the audit trail
        // shows which actions were dropped, but still return the survivors.
        out.parse_error =
            QStringLiteral("partial: ") + per_action_defects.join(QStringLiteral("; "));
    }

    return out;
}

QJsonObject action_to_json(const ProposedAction& a) {
    QJsonObject o;
    o["signal"] = signal_to_wire(a.signal);
    o["coin"] = a.coin;
    o["quantity"] = a.quantity;
    o["leverage"] = a.leverage;
    o["profit_target"] = a.profit_target;
    o["stop_loss"] = a.stop_loss;
    o["invalidation_condition"] = a.invalidation_condition;
    o["confidence"] = a.confidence;
    o["risk_usd"] = a.risk_usd;
    o["justification"] = a.justification;
    return o;
}

} // namespace fincept::services::alpha_arena
