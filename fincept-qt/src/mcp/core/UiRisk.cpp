// UiRisk.cpp — see UiRisk.h. The word lists are the conservative default; screens
// mark their order / delete / spend / account / code controls with fincept_agent_risk.

#include "mcp/core/UiRisk.h"

#include "core/agent/AgentRiskMarkers.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QKeySequence>
#include <QModelIndex>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QWidget>

namespace fincept::mcp::core::ui {

namespace ui_risk_detail {

using namespace ::fincept::core::agent;

QString why_money() {
    return QStringLiteral("places, changes or cancels an order, or moves money");
}

Risk make(bool money, const QString& why, const QString& kind) {
    Risk r;
    r.risky = true;
    r.money = money;
    r.why = why;
    r.kind = kind;
    return r;
}

/// "Ctrl+Return=code;Delete=delete" / {seq: kind} / ["seq=kind", …] → pairs.
QList<QPair<QString, QString>> parse_pairs(const QVariant& v) {
    QList<QPair<QString, QString>> out;
    auto add = [&out](const QString& item) {
        const qsizetype eq = item.lastIndexOf(QLatin1Char('='));
        if (eq > 0)
            out.append({item.left(eq).trimmed(), item.mid(eq + 1).trimmed()});
    };
    if (v.typeId() == QMetaType::QVariantMap) {
        const QVariantMap m = v.toMap();
        for (auto it = m.constBegin(); it != m.constEnd(); ++it)
            out.append({it.key().trimmed(), it.value().toString().trimmed()});
    } else if (v.typeId() == QMetaType::QStringList || v.typeId() == QMetaType::QVariantList) {
        for (const QString& item : v.toStringList())
            add(item);
    } else {
        for (const QString& item : v.toString().split(QLatin1Char(';'), Qt::SkipEmptyParts))
            add(item);
    }
    return out;
}

} // namespace ui_risk_detail

using namespace ui_risk_detail;

Risk risk_for_kind(const QString& raw) {
    const QString k = raw.trimmed().toLower();
    if (k.isEmpty() || k == QLatin1String("none"))
        return {};
    if (k == QLatin1String("order") || k == QLatin1String("trade"))
        return make(true, why_money(), QStringLiteral("order"));
    if (k == QLatin1String("money"))
        return make(true, QStringLiteral("moves money"), QStringLiteral("money"));
    if (k == QLatin1String("spend"))
        return make(true, QStringLiteral("spends money, credits or model quota"), QStringLiteral("spend"));
    if (k == QLatin1String("delete") || k == QLatin1String("destructive"))
        return make(false, QStringLiteral("deletes or resets data"), QStringLiteral("destructive"));
    if (k == QLatin1String("account"))
        return make(false, QStringLiteral("changes the account or its security"), QStringLiteral("account"));
    if (k == QLatin1String("code"))
        return make(false, QStringLiteral("runs code on this computer"), QStringLiteral("code"));
    if (k == QLatin1String("file"))
        return make(false, QStringLiteral("writes a file"), QStringLiteral("file"));
    return make(false, QStringLiteral("is marked as needing the user's approval"), QStringLiteral("generic"));
}

bool is_order_kind(const Risk& r) {
    return r.risky && (r.kind == QLatin1String("order") || r.kind == QLatin1String("money"));
}

Risk classify_words(const QString& words) {
    // Account settings that change what a position risks count as money too: in
    // LIVE mode an agent used to change real leverage / margin mode (Bybit's is
    // account-wide) with no approval. Buttons that act on positions or baskets
    // without the word "order" ("EXIT ALL", "CANCEL ALL", "PLACE STRATEGY",
    // "EXECUTE BASKET", "TRADE ALL") are orders as well.
    static const QRegularExpression money(QStringLiteral(
        R"(\b(buy|sell|short|cover)\b|\b(place|submit|send|execute|confirm|modify|amend)\b.{0,24}\b(orders?|trades?|bets?|transactions?|swaps?)\b|\b(orders?|trades?)\b.{0,16}\b(place|submit|send|execute|confirm)|\bsquare[ -]?off\b|\b(close|exit) (all )?positions?\b|\bclose all\b|\bcancel all\b|\bexit all\b|^\s*exit\b(?!\s+(chat|mode|full|app|application|terminal|focus|edit|preview|search|setup|wizard|tour|demo))|\bflatten\b|\bliquidate\b|\bcancel (all )?orders?\b|\b(place|execute) (strateg(y|ies)|baskets?|brackets?|oco|all)\b|\btrade all\b|\b(withdraw|transfer|deposit|swap|bridge|stake|unstake|redeem|mint|burn)\b|\bsend (funds|crypto|tokens?)\b|\bsign (and send|transaction)\b|\blive (orders?|trades?|trading|mode)\b|\b(set|change|apply|adjust)\b.{0,24}\bleverage\b|\bleverage\b.{0,24}\b(to|apply)\b|\bmargin[ -]?mode\b|\breduce[ -]?only\b|ordersend|orderconfirm|confirmplace|placeorder|submitorder)"));
    static const QRegularExpression destroy(QStringLiteral(
        R"(\b(delete|remove|erase|wipe|purge|drop|uninstall|revoke|regenerate|discard|reset)\b|\bclear (all|history|data|cache|everything)\b|danger|destructive)"));
    static const QRegularExpression benign(QStringLiteral(
        R"(\b(reset|clear|remove|delete)\b.{0,14}\b(zoom|view|filters?|search|selection|chart|columns?|indicators?|stud(y|ies)|overlays?|form|fields?|input|text|sort|highlights?|query)\b)"));
    static const QRegularExpression spend(
        QStringLiteral(R"(\b(credits?|spend|purchase|pay|payment|checkout|subscribe|upgrade|top[ -]?up|recharge)\b)"));
    // Starting an AI agent / harness run spends model quota.
    static const QRegularExpression agent_run(QStringLiteral(
        R"(\b(start|launch|run|execute)\b.{0,16}\b(agents?|harness(es)?|plans?|workflows?)\b)"));
    static const QRegularExpression account(QStringLiteral(R"(\b(log ?out|sign ?out|delete account)\b)"));

    if (money.match(words).hasMatch())
        return make(true, why_money(), QStringLiteral("order"));
    if (destroy.match(words).hasMatch() && !benign.match(words).hasMatch())
        return make(false, QStringLiteral("deletes or resets data"), QStringLiteral("destructive"));
    if (spend.match(words).hasMatch())
        // Refused in a PAPER run, and never covered by a blanket grant.
        return make(true, QStringLiteral("spends money or credits"), QStringLiteral("spend"));
    if (agent_run.match(words).hasMatch())
        return make(false, QStringLiteral("starts an AI agent run (uses model quota)"), QStringLiteral("spend"));
    if (account.match(words).hasMatch())
        return make(false, QStringLiteral("signs out"), QStringLiteral("account"));
    return {};
}

Risk classify_row_action(const QString& action, const QString& context) {
    static const QRegularExpression verb(QStringLiteral(
        R"(\b(cancel|close|exit|square|modify|amend|edit|buy|sell|place|submit|execute|confirm|trade|flatten|liquidate|withdraw|transfer)\b)"));
    static const QRegularExpression book(
        QStringLiteral(R"(\b(orders?|positions?|holdings?|trades?|blotter|fills?|portfolio|basket|legs?)\b)"));
    Risk r = classify_words(action);
    if (r.risky)
        return r;
    if (verb.match(action).hasMatch() && book.match(context).hasMatch())
        return make(true, why_money(), QStringLiteral("order"));
    // The row's own words can say it ("RELIANCE · BUY · OPEN" + "Cancel").
    if (verb.match(action).hasMatch()) {
        const Risk row = classify_words(context);
        if (row.money)
            return row;
    }
    return r;
}

bool is_negative_choice(const QString& name) {
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^\s*(no|cancel|close|back|dismiss|abort|don'?t|keep|skip|not now|later|go back)\s*(\([^)]*\))?\s*[.!]?\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(name).hasMatch();
}

Risk own_marker_risk(const QObject* o) {
    if (!o)
        return {};
    const QVariant v = o->property(kAgentRiskProperty);
    return v.isValid() ? risk_for_kind(v.toString()) : Risk{};
}

Risk marker_risk(const QObject* o) {
    Risk r = own_marker_risk(o);
    if (r.risky || !o)
        return r;
    // An ancestor's marker covers this control only when the ancestor says so.
    if (o->isWidgetType() && static_cast<const QWidget*>(o)->isWindow())
        return {};
    for (const QObject* p = o->parent(); p; p = p->parent()) {
        if (p->property(kAgentRiskInheritProperty).toBool()) {
            const Risk inherited = own_marker_risk(p);
            if (inherited.risky)
                return inherited;
        }
        if (p->isWidgetType() && static_cast<const QWidget*>(p)->isWindow())
            break;
    }
    return {};
}

Risk keys_risk(const QWidget* w, const QKeySequence& seq) {
    if (seq.isEmpty())
        return {};
    for (const QWidget* p = w; p; p = p->parentWidget()) {
        const QVariant v = p->property(kAgentRiskKeysProperty);
        if (v.isValid()) {
            for (const auto& pair : parse_pairs(v)) {
                const QKeySequence want = QKeySequence::fromString(pair.first, QKeySequence::PortableText);
                if (!want.isEmpty() && want[0] == seq[0])
                    return risk_for_kind(pair.second);
            }
        }
        if (p->isWindow())
            break;
    }
    return {};
}

Risk cell_risk(const QAbstractItemView* view, const QModelIndex& index) {
    if (!view || !index.isValid())
        return {};
    Risk r;
    if (const QVariant v = index.data(kAgentRiskRole); v.isValid())
        r = risk_for_kind(v.toString());
    if (r.risky)
        return r;
    const QVariant cols = view->property(kAgentRiskColumnsProperty);
    if (!cols.isValid() || !view->model())
        return {};
    const QString header =
        view->model()->headerData(index.column(), Qt::Horizontal, Qt::DisplayRole).toString().trimmed();
    for (const auto& pair : parse_pairs(cols)) {
        bool is_num = false;
        const int n = pair.first.toInt(&is_num);
        if ((is_num && n == index.column()) ||
            (!is_num && !header.isEmpty() && header.compare(pair.first, Qt::CaseInsensitive) == 0))
            return risk_for_kind(pair.second);
    }
    return {};
}

Risk stronger(const Risk& a, const Risk& b) {
    if (a.money != b.money)
        return a.money ? a : b;
    if (a.risky != b.risky)
        return a.risky ? a : b;
    if (is_order_kind(b) && !is_order_kind(a))
        return b;
    return a;
}

} // namespace fincept::mcp::core::ui
