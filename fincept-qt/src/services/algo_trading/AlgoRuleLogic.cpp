// src/services/algo_trading/AlgoRuleLogic.cpp
#include "services/algo_trading/AlgoRuleLogic.h"

#include "services/algo_trading/AlgoTradingTypes.h"

#include <QCoreApplication>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::algo::rules {

namespace {

QString arl_tr(const char* text, const char* disambiguation = nullptr, int n = -1) {
    return QCoreApplication::translate("fincept::services::algo::rules", text, disambiguation, n);
}

const IndicatorDef* arl_find(const QString& id) {
    static const QVector<IndicatorDef> kCatalog = algo_indicators();
    for (const IndicatorDef& d : kCatalog)
        if (d.id == id)
            return &d;
    return nullptr;
}

/// A threshold as people write it: 30, 0.05, 1250.5 (C locale, no grouping, no trailing zeros).
QString arl_number(double v) {
    if (!std::isfinite(v))
        return QStringLiteral("—");
    QString s = QString::number(v, 'f', 6);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s == QLatin1String("-0") ? QStringLiteral("0") : s;
}

double arl_param(const QJsonObject& params, const ParamSpec& spec) {
    const QJsonValue v = params.value(spec.name);
    return v.isDouble() ? v.toDouble() : spec.def;
}

QString arl_field_text(const IndicatorDef* def, const QString& field) {
    if (!def || def->fields.size() <= 1 || field.isEmpty() || field == QLatin1String("value"))
        return {};
    if (def->id == QLatin1String("MACD") && field == QLatin1String("line"))
        return {};
    QString f = field;
    f.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (f == QLatin1String("pct b"))
        f = QStringLiteral("%B");
    return f;
}

QString arl_op_text(const QString& op) {
    if (op == QLatin1String("crosses_above"))
        return arl_tr("crosses above");
    if (op == QLatin1String("crosses_below"))
        return arl_tr("crosses below");
    if (op == QLatin1String("=="))
        return arl_tr("touches");
    return op; // > < >= <=
}

bool arl_is_group(const QJsonObject& node) {
    return node.contains(QStringLiteral("children")) || node.value(QStringLiteral("type")).toString() == QLatin1String("group");
}

// ── Value ranges of a left operand ──────────────────────────────────────────

enum class ArlScale { Unknown, Price, NonNegative, Bounded };

struct ArlRange {
    ArlScale scale = ArlScale::Unknown;
    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();
};

ArlRange arl_range(const QString& ind, const QString& field) {
    static const QStringList kPrice = {QStringLiteral("CLOSE"), QStringLiteral("OPEN"),     QStringLiteral("HIGH"),
                                       QStringLiteral("LOW"),   QStringLiteral("VWAP"),     QStringLiteral("SMA"),
                                       QStringLiteral("EMA"),   QStringLiteral("WMA"),      QStringLiteral("DEMA"),
                                       QStringLiteral("TEMA"),  QStringLiteral("KELTNER"),  QStringLiteral("DONCHIAN"),
                                       QStringLiteral("ICHIMOKU")};
    ArlRange r;
    if (kPrice.contains(ind) ||
        (ind == QLatin1String("BOLLINGER") &&
         (field == QLatin1String("upper") || field == QLatin1String("middle") || field == QLatin1String("lower"))) ||
        (ind == QLatin1String("SUPERTREND") && field != QLatin1String("direction"))) {
        r.scale = ArlScale::Price;
        r.lo = 0;
        return r;
    }
    if (ind == QLatin1String("VOLUME") || ind == QLatin1String("ATR") ||
        (ind == QLatin1String("BOLLINGER") && field == QLatin1String("width"))) {
        r.scale = ArlScale::NonNegative;
        r.lo = 0;
        return r;
    }
    if (ind == QLatin1String("RSI") || ind == QLatin1String("MFI") || ind == QLatin1String("STOCHASTIC") ||
        ind == QLatin1String("ADX") || ind == QLatin1String("AROON")) {
        r.scale = ArlScale::Bounded;
        r.lo = 0;
        r.hi = 100;
        return r;
    }
    if (ind == QLatin1String("WILLIAMS_R")) {
        r.scale = ArlScale::Bounded;
        r.lo = -100;
        r.hi = 0;
        return r;
    }
    return r;
}

enum class ArlTruth { Depends, Always, Never };

/// Whether `lhs op value` can change over time, given the operand's range.
ArlTruth arl_truth(const ArlRange& r, const QString& op, double v) {
    if (r.scale == ArlScale::Unknown)
        return ArlTruth::Depends;
    // Prices are strictly positive; every other range is inclusive.
    const bool price = r.scale == ArlScale::Price;
    if (op == QLatin1String(">"))
        return v >= r.hi ? ArlTruth::Never : ((price ? v <= r.lo : v < r.lo) ? ArlTruth::Always : ArlTruth::Depends);
    if (op == QLatin1String(">="))
        return v > r.hi ? ArlTruth::Never : (v <= r.lo ? ArlTruth::Always : ArlTruth::Depends);
    if (op == QLatin1String("<"))
        return v <= r.lo ? ArlTruth::Never : (v > r.hi ? ArlTruth::Always : ArlTruth::Depends);
    if (op == QLatin1String("<="))
        return (price ? v <= r.lo : v < r.lo) ? ArlTruth::Never : (v >= r.hi ? ArlTruth::Always : ArlTruth::Depends);
    if (op == QLatin1String("crosses_above"))
        return (v >= r.hi || (price && v <= r.lo)) ? ArlTruth::Never : ArlTruth::Depends;
    if (op == QLatin1String("crosses_below"))
        return v <= r.lo ? ArlTruth::Never : ArlTruth::Depends;
    return ArlTruth::Depends;
}

QString arl_range_text(const ArlRange& r) {
    return arl_tr("%1 to %2").arg(arl_number(r.lo), arl_number(r.hi));
}

bool arl_known_op(const QString& op) {
    return algo_operators().contains(op);
}

/// The problem of one leaf, empty when it is fine.
QString arl_leaf_problem(const QJsonObject& c) {
    const QString ind = c.value(QStringLiteral("indicator")).toString();
    if (ind.isEmpty())
        return arl_tr("Pick an indicator.");
    if (!arl_find(ind))
        return arl_tr("Unknown indicator “%1”.").arg(ind);
    const QString op = c.value(QStringLiteral("operator")).toString();
    if (!arl_known_op(op))
        return arl_tr("Pick an operator.");
    if (op == QLatin1String("rising") || op == QLatin1String("falling"))
        return {};
    const QString field = c.value(QStringLiteral("field")).toString(QStringLiteral("value"));
    const QString lhs = operand_text(ind, c.value(QStringLiteral("params")).toObject(), field,
                                     c.value(QStringLiteral("offset")).toInt(0));
    if (op == QLatin1String("between")) {
        const double lo = c.value(QStringLiteral("value")).toDouble(0);
        const double hi = c.value(QStringLiteral("value2")).toDouble(0);
        if (lo > hi)
            return arl_tr("The lower bound (%1) is above the upper bound (%2).").arg(arl_number(lo), arl_number(hi));
        const ArlRange r = arl_range(ind, field);
        if (r.scale != ArlScale::Unknown && (hi < r.lo || lo > r.hi || (r.scale == ArlScale::Price && hi <= 0)))
            return arl_tr("%1 can never be between %2 and %3.").arg(lhs, arl_number(lo), arl_number(hi));
        return {};
    }
    if (c.value(QStringLiteral("compare_mode")).toString(QStringLiteral("value")) == QLatin1String("indicator")) {
        const QString cind = c.value(QStringLiteral("compare_indicator")).toString();
        if (cind.isEmpty())
            return arl_tr("Pick the indicator to compare with.");
        if (!arl_find(cind))
            return arl_tr("Unknown indicator “%1”.").arg(cind);
        const QJsonObject p1 = c.value(QStringLiteral("params")).toObject();
        const QJsonObject p2 = c.value(QStringLiteral("compare_params")).toObject();
        const QString f2 = c.value(QStringLiteral("compare_field")).toString(QStringLiteral("value"));
        const QString rhs = operand_text(cind, p2, f2, c.value(QStringLiteral("compare_offset")).toInt(0));
        if (lhs == rhs)
            return arl_tr("%1 is compared with itself — the rule can never change.").arg(lhs);
        return {};
    }
    const double v = c.value(QStringLiteral("value")).toDouble(0);
    const ArlRange r = arl_range(ind, field);
    switch (arl_truth(r, op, v)) {
        case ArlTruth::Always:
            return arl_tr("%1 %2 %3 is always true — it would fire on every bar. Enter a level the indicator "
                          "really crosses, or compare with another indicator.")
                .arg(lhs, arl_op_text(op), arl_number(v));
        case ArlTruth::Never:
            if (r.scale == ArlScale::Price)
                return arl_tr("%1 %2 %3 can never be true: prices stay above 0. Enter a price level or compare with "
                              "another indicator.")
                    .arg(lhs, arl_op_text(op), arl_number(v));
            return arl_tr("%1 %2 %3 can never be true (%4 ranges from %5).")
                .arg(lhs, arl_op_text(op), arl_number(v), lhs, arl_range_text(r));
        case ArlTruth::Depends:
            break;
    }
    return {};
}

QString arl_node_text(const QJsonObject& node, bool nested) {
    if (!arl_is_group(node))
        return condition_text(node);
    const QJsonArray kids = node.value(QStringLiteral("children")).toArray();
    const QString logic = node.value(QStringLiteral("logic")).toString(node.value(QStringLiteral("op")).toString(QStringLiteral("AND")));
    const QString joiner = logic.compare(QLatin1String("OR"), Qt::CaseInsensitive) == 0 ? arl_tr(" or ") : arl_tr(" and ");
    QStringList parts;
    for (const auto& k : kids)
        parts << arl_node_text(k.toObject(), true);
    QString s = parts.isEmpty() ? arl_tr("(empty group)") : parts.join(joiner);
    if (parts.size() > 1 && nested)
        s = QLatin1Char('(') + s + QLatin1Char(')');
    if (node.value(QStringLiteral("negate")).toBool(false))
        s = arl_tr("not %1").arg(parts.size() > 1 && !nested ? QLatin1Char('(') + s + QLatin1Char(')') : s);
    return s;
}

int arl_operand_lookback(const QString& ind, const QJsonObject& params) {
    const IndicatorDef* def = arl_find(ind);
    if (!def)
        return 1;
    auto p = [&](const char* name, double fallback) {
        for (const ParamSpec& s : def->params)
            if (s.name == QLatin1String(name))
                return std::max(1, static_cast<int>(std::ceil(arl_param(params, s))));
        return static_cast<int>(fallback);
    };
    if (ind == QLatin1String("SMA") || ind == QLatin1String("WMA") || ind == QLatin1String("BOLLINGER") ||
        ind == QLatin1String("DONCHIAN") || ind == QLatin1String("CCI") || ind == QLatin1String("WILLIAMS_R") ||
        ind == QLatin1String("CMF"))
        return p("period", 20);
    if (ind == QLatin1String("EMA") || ind == QLatin1String("DEMA") || ind == QLatin1String("TEMA") ||
        ind == QLatin1String("ATR") || ind == QLatin1String("ADX") || ind == QLatin1String("KELTNER"))
        return 3 * p("period", 20);
    if (ind == QLatin1String("SUPERTREND"))
        return 3 * p("period", 10);
    if (ind == QLatin1String("RSI"))
        return 3 * p("period", 14) + 1;
    if (ind == QLatin1String("MFI") || ind == QLatin1String("ROC") || ind == QLatin1String("AROON"))
        return p("period", 14) + 1;
    if (ind == QLatin1String("MACD"))
        return 3 * p("slow", 26) + p("signal", 9);
    if (ind == QLatin1String("STOCHASTIC"))
        return p("k_period", 14) + p("d_period", 3);
    if (ind == QLatin1String("ICHIMOKU"))
        return std::max({p("tenkan", 9), p("kijun", 26), p("senkou", 52)}) + p("kijun", 26);
    if (ind == QLatin1String("VOL_WIN_CHG"))
        return p("window", 10) + 1;
    if (ind == QLatin1String("OBV"))
        return 2;
    return 1; // prices, volume, VWAP
}

int arl_block_lookback(const QJsonArray& block) {
    int need = 0;
    for (const auto& v : block) {
        const QJsonObject n = v.toObject();
        if (arl_is_group(n)) {
            need = std::max(need, arl_block_lookback(n.value(QStringLiteral("children")).toArray()));
            continue;
        }
        // +1: crosses / rising / falling read the previous bar too.
        need = std::max(need, arl_operand_lookback(n.value(QStringLiteral("indicator")).toString(),
                                                   n.value(QStringLiteral("params")).toObject()) +
                                  std::max(0, n.value(QStringLiteral("offset")).toInt(0)) + 1);
        if (n.value(QStringLiteral("compare_mode")).toString() == QLatin1String("indicator"))
            need = std::max(need, arl_operand_lookback(n.value(QStringLiteral("compare_indicator")).toString(),
                                                       n.value(QStringLiteral("compare_params")).toObject()) +
                                      std::max(0, n.value(QStringLiteral("compare_offset")).toInt(0)) + 1);
    }
    return need;
}

void arl_collect(const QJsonArray& block, QVector<RuleIssue>& out) {
    for (int i = 0; i < block.size(); ++i) {
        const QJsonObject n = block.at(i).toObject();
        if (!arl_is_group(n)) {
            const QString p = arl_leaf_problem(n);
            if (!p.isEmpty())
                out.append({i, -1, p, true});
            continue;
        }
        const QJsonArray kids = n.value(QStringLiteral("children")).toArray();
        if (kids.isEmpty()) {
            out.append({i, -1, arl_tr("Empty group — add a rule to it or remove it."), true});
            continue;
        }
        for (int k = 0; k < kids.size(); ++k) {
            const QJsonObject kid = kids.at(k).toObject();
            const QString p = arl_is_group(kid) ? QString() : arl_leaf_problem(kid);
            if (!p.isEmpty())
                out.append({i, k, p, true});
        }
    }
}

} // namespace

QString operand_text(const QString& indicator, const QJsonObject& params, const QString& field, int offset) {
    const IndicatorDef* def = arl_find(indicator);
    QString s;
    if (!def) {
        s = indicator.isEmpty() ? arl_tr("(indicator)") : indicator;
    } else {
        QString label = def->label;
        if (def->category == QLatin1String("stock") && label.endsWith(QLatin1String(" Price")))
            label.chop(6); // "Close Price" -> "Close"
        s = label;
        if (!def->params.isEmpty()) {
            QStringList vals;
            for (const ParamSpec& spec : def->params)
                vals << arl_number(arl_param(params, spec));
            s += QLatin1Char('(') + vals.join(QStringLiteral(", ")) + QLatin1Char(')');
        }
        const QString f = arl_field_text(def, field);
        if (!f.isEmpty())
            s += QLatin1Char(' ') + f;
    }
    if (offset > 0)
        s += QLatin1Char(' ') + arl_tr("%n bar(s) ago", nullptr, offset);
    return s;
}

QString condition_text(const QJsonObject& c) {
    const QString lhs = operand_text(c.value(QStringLiteral("indicator")).toString(),
                                     c.value(QStringLiteral("params")).toObject(),
                                     c.value(QStringLiteral("field")).toString(QStringLiteral("value")),
                                     c.value(QStringLiteral("offset")).toInt(0));
    const QString op = c.value(QStringLiteral("operator")).toString(QStringLiteral(">"));
    if (op == QLatin1String("rising"))
        return arl_tr("%1 is rising").arg(lhs);
    if (op == QLatin1String("falling"))
        return arl_tr("%1 is falling").arg(lhs);
    if (op == QLatin1String("between"))
        return arl_tr("%1 is between %2 and %3")
            .arg(lhs, arl_number(c.value(QStringLiteral("value")).toDouble(0)),
                 arl_number(c.value(QStringLiteral("value2")).toDouble(0)));
    QString rhs;
    if (c.value(QStringLiteral("compare_mode")).toString(QStringLiteral("value")) == QLatin1String("indicator"))
        rhs = operand_text(c.value(QStringLiteral("compare_indicator")).toString(),
                           c.value(QStringLiteral("compare_params")).toObject(),
                           c.value(QStringLiteral("compare_field")).toString(QStringLiteral("value")),
                           c.value(QStringLiteral("compare_offset")).toInt(0));
    else
        rhs = arl_number(c.value(QStringLiteral("value")).toDouble(0));
    return QStringLiteral("%1 %2 %3").arg(lhs, arl_op_text(op), rhs);
}

QString rules_sentence(const QJsonArray& conditions, const QString& logic, Side side) {
    if (conditions.isEmpty()) {
        switch (side) {
            case Side::Entry:
                return arl_tr("No entry rule yet.");
            case Side::Exit:
                return arl_tr("No exit rule — the position closes only by stop-loss, take-profit or trailing stop.");
            case Side::Scan:
            case Side::Alert:
                break;
        }
        return arl_tr("No rule yet.");
    }
    const QString joiner = logic.compare(QLatin1String("OR"), Qt::CaseInsensitive) == 0 ? arl_tr(" or ") : arl_tr(" and ");
    QStringList parts;
    for (const auto& v : conditions)
        parts << arl_node_text(v.toObject(), conditions.size() > 1);
    const QString body = parts.join(joiner);
    switch (side) {
        case Side::Entry:
            return arl_tr("Enter when %1").arg(body);
        case Side::Exit:
            return arl_tr("Exit when %1").arg(body);
        case Side::Scan:
            return arl_tr("Match when %1").arg(body);
        case Side::Alert:
            return arl_tr("Alert when %1").arg(body);
    }
    return body;
}

QVector<RuleIssue> check_conditions(const QJsonArray& conditions) {
    QVector<RuleIssue> out;
    arl_collect(conditions, out);
    return out;
}

QString validate_strategy(const QJsonArray& entry, const QJsonArray& exit, double stop_loss_pct,
                          double take_profit_pct, double trailing_stop_pct) {
    if (entry.isEmpty())
        return arl_tr("Add at least one entry rule.");
    auto where = [](const RuleIssue& i) {
        return i.child >= 0 ? QStringLiteral("%1.%2").arg(i.index + 1).arg(i.child + 1) : QString::number(i.index + 1);
    };
    for (const RuleIssue& i : check_conditions(entry))
        if (i.blocking)
            return arl_tr("ENTRY rule %1: %2").arg(where(i), i.reason);
    for (const RuleIssue& i : check_conditions(exit))
        if (i.blocking)
            return arl_tr("EXIT rule %1: %2").arg(where(i), i.reason);
    if (exit.isEmpty() && !(stop_loss_pct > 0) && !(take_profit_pct > 0) && !(trailing_stop_pct > 0))
        return arl_tr("The strategy never exits: add an exit rule, a stop-loss, a take-profit or a trailing stop.");
    return {};
}

bool is_placeholder_condition(const QJsonObject& c) {
    if (arl_is_group(c))
        return false;
    return c.value(QStringLiteral("indicator")).toString() == QLatin1String("CLOSE") &&
           c.value(QStringLiteral("operator")).toString(QStringLiteral(">")) == QLatin1String(">") &&
           c.value(QStringLiteral("compare_mode")).toString(QStringLiteral("value")) == QLatin1String("value") &&
           c.value(QStringLiteral("value")).toDouble(0) == 0.0 && c.value(QStringLiteral("offset")).toInt(0) == 0;
}

bool is_blank_rules(const QJsonArray& entry, const QJsonArray& exit) {
    auto blank = [](const QJsonArray& a) {
        return a.isEmpty() || (a.size() == 1 && is_placeholder_condition(a.first().toObject()));
    };
    return blank(entry) && blank(exit);
}

int warmup_bars(const QJsonArray& entry, const QJsonArray& exit, int minimum, int maximum) {
    const int need = std::max(arl_block_lookback(entry), arl_block_lookback(exit));
    return std::clamp(need, minimum, std::max(minimum, maximum));
}

} // namespace fincept::services::algo::rules
