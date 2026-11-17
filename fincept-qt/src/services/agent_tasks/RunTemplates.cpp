// RunTemplates.cpp — the built-in finance task templates, placeholder filling,
// parameter validation and the JSON of user templates. Qt Core only.

#include "services/agent_tasks/RunTemplates.h"

#include <QCoreApplication>
#include <QDate>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace fincept::agent_tasks {

namespace run_templates_detail {

QString rt_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::RunTemplates", text);
}

/// Placeholder name → the context key it reads: lower case, spaces as underscores.
QString rt_norm(const QString& name) {
    QString n = name.trimmed().toLower();
    n.replace(QLatin1Char(' '), QLatin1Char('_'));
    return n;
}

const QRegularExpression& rt_placeholder_re() {
    static const QRegularExpression re(QStringLiteral("<([A-Za-z][A-Za-z0-9 _]{0,40})>"));
    return re;
}

/// The context value as prompt text; `found` = the key exists (even when empty).
QString rt_value(const QJsonObject& ctx, const QString& norm_key, bool* found) {
    for (auto it = ctx.constBegin(); it != ctx.constEnd(); ++it) {
        if (rt_norm(it.key()) != norm_key)
            continue;
        *found = true;
        const QJsonValue v = it.value();
        if (v.isString())
            return v.toString().trimmed();
        if (v.isDouble())
            return QString::number(v.toDouble(), 'g', 12);
        if (v.isBool())
            return v.toBool() ? rt_tr("yes") : rt_tr("no");
        if (v.isArray()) {
            QStringList parts;
            for (const QJsonValue& e : v.toArray())
                if (!e.toString().trimmed().isEmpty())
                    parts << e.toString().trimmed();
            return parts.join(QStringLiteral(", "));
        }
        return {};
    }
    *found = false;
    return {};
}

RunTemplateParam rt_param(const QString& key, const QString& label, TemplateParamType type, bool required,
                          const QString& def = {}, const QString& hint = {}) {
    RunTemplateParam p;
    p.key = key;
    p.label = label;
    p.type = type;
    p.required = required;
    p.default_value = def;
    p.hint = hint;
    return p;
}

RunTemplateParam rt_symbol(bool required = true) {
    return rt_param(QStringLiteral("symbol"), rt_tr("Symbol"), TemplateParamType::Symbol, required, {},
                    rt_tr("Ticker, name or ISIN"));
}

RunTemplateParam rt_portfolio() {
    return rt_param(QStringLiteral("portfolio"), rt_tr("Portfolio"), TemplateParamType::Portfolio, true, {},
                    rt_tr("Its holdings are added to the task when it starts"));
}

RunTemplateParam rt_period(const QString& def) {
    return rt_param(QStringLiteral("period"), rt_tr("Period"), TemplateParamType::DateRange, true, def,
                    rt_tr("Relative periods move with the day a schedule runs"));
}

RunTemplate rt_base(const QString& id, const QString& category, const QString& title, const QString& description,
                    double credits, int steps, int minutes) {
    RunTemplate t;
    t.id = id;
    t.category = category;
    t.title = title;
    t.description = description;
    t.engine = QString::fromLatin1(kEngineCloud);
    t.mode = QStringLiteral("deep");
    t.plan_first = true;
    t.budget.max_credits = credits;
    t.budget.max_steps = steps;
    t.budget.max_minutes = minutes;
    return t;
}

void rt_finish(RunTemplate& t) {
    t.context_keys.clear();
    for (const RunTemplateParam& p : std::as_const(t.params))
        t.context_keys << param_context_keys(p);
}

} // namespace run_templates_detail

using namespace run_templates_detail;

// ── Parameter types ──────────────────────────────────────────────────────────

QString param_type_id(TemplateParamType t) {
    switch (t) {
        case TemplateParamType::Symbol:
            return QStringLiteral("symbol");
        case TemplateParamType::Symbols:
            return QStringLiteral("symbols");
        case TemplateParamType::Portfolio:
            return QStringLiteral("portfolio");
        case TemplateParamType::Watchlist:
            return QStringLiteral("watchlist");
        case TemplateParamType::DateRange:
            return QStringLiteral("date_range");
        case TemplateParamType::Text:
            return QStringLiteral("text");
        case TemplateParamType::LongText:
            return QStringLiteral("long_text");
        case TemplateParamType::Choice:
            return QStringLiteral("choice");
    }
    return QStringLiteral("text");
}

std::optional<TemplateParamType> param_type_from_id(const QString& id) {
    for (TemplateParamType t : {TemplateParamType::Symbol, TemplateParamType::Symbols, TemplateParamType::Portfolio,
                                TemplateParamType::Watchlist, TemplateParamType::DateRange, TemplateParamType::Text,
                                TemplateParamType::LongText, TemplateParamType::Choice})
        if (param_type_id(t) == id)
            return t;
    return std::nullopt;
}

QStringList param_context_keys(const RunTemplateParam& p) {
    switch (p.type) {
        case TemplateParamType::Portfolio:
            return {QStringLiteral("portfolio_id"), QStringLiteral("portfolio_name")};
        case TemplateParamType::Watchlist:
            return {QStringLiteral("watchlist_id"), QStringLiteral("watchlist_name")};
        case TemplateParamType::DateRange:
            return {QStringLiteral("date_range"), QStringLiteral("date_from"), QStringLiteral("date_to"),
                    QStringLiteral("period")};
        default:
            break;
    }
    return {p.key};
}

QJsonObject template_default_context(const RunTemplate& t) {
    QJsonObject ctx;
    for (const RunTemplateParam& p : t.params) {
        switch (p.type) {
            case TemplateParamType::DateRange:
                ctx[QStringLiteral("date_range")] = p.default_value.isEmpty() ? QStringLiteral("30d") : p.default_value;
                break;
            case TemplateParamType::Choice:
                ctx[p.key] = p.default_value.isEmpty() && !p.choices.isEmpty() ? p.choices.first() : p.default_value;
                break;
            case TemplateParamType::Portfolio:
            case TemplateParamType::Watchlist:
                break; // picked by the user
            default:
                if (!p.default_value.isEmpty())
                    ctx[p.key] = p.default_value;
                break;
        }
    }
    return ctx;
}

// ── Built-in templates ───────────────────────────────────────────────────────

QVector<RunTemplate> run_templates() {
    QVector<RunTemplate> out;
    const QString equities = rt_tr("Equities");
    const QString portfolio = rt_tr("Portfolio");
    const QString markets = rt_tr("Markets");

    {
        RunTemplate t = rt_base(QStringLiteral("equity_deep_dive"), equities, rt_tr("Equity deep-dive"),
                                rt_tr("A full research note on one stock: business, results, financial trends, balance "
                                      "sheet, valuation against peers, risks and a view."),
                                50, 6, 30);
        t.params = {rt_symbol(),
                    rt_param(QStringLiteral("focus"), rt_tr("Extra focus"), TemplateParamType::Text, false, {},
                             rt_tr("Optional, e.g. margins, China exposure, capital allocation"))};
        t.title_pattern = rt_tr("Deep-dive: <SYMBOL>");
        t.prompt = rt_tr("Write an equity research note on <SYMBOL> for an investor deciding whether to own it.\n"
                         "Cover: the business model and segments; the latest results and guidance; revenue, margin and "
                         "free-cash-flow trends over the last 3-5 years; the balance sheet and capital allocation; "
                         "valuation against its own history and its closest peers (P/E, EV/EBITDA, FCF yield); the "
                         "main risks and upcoming catalysts. Use figures with their dates and sources.\n"
                         "Extra focus: <FOCUS>.\n"
                         "End with a clear view (own / watch / avoid) and what would change it.");
        t.plan_hint = rt_tr("1) business and latest results, 2) multi-year financial trends as a table, 3) balance "
                            "sheet and capital allocation, 4) valuation versus history and peers as a table, 5) risks, "
                            "catalysts and the view.");
        t.expected_artefacts = {rt_tr("Research note (Markdown) with a view"),
                                rt_tr("Table: multi-year financial summary (CSV)"),
                                rt_tr("Table: valuation multiples versus peers (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("earnings_preview_review"), equities, rt_tr("Earnings preview / review"),
                                rt_tr("Before results: what the market expects and what to watch. After results: what "
                                      "was reported against expectations and what it changes."),
                                40, 5, 25);
        RunTemplateParam phase = rt_param(QStringLiteral("phase"), rt_tr("Type"), TemplateParamType::Choice, true,
                                          QStringLiteral("preview"));
        phase.choices = {QStringLiteral("preview"), QStringLiteral("review")};
        phase.choice_labels = {rt_tr("Preview (before the report)"), rt_tr("Review (after the report)")};
        t.params = {rt_symbol(), phase,
                    rt_param(QStringLiteral("quarter"), rt_tr("Quarter"), TemplateParamType::Text, false, {},
                             rt_tr("Optional, e.g. Q3 FY2026 — empty means the latest"))};
        t.title_pattern = rt_tr("Earnings <PHASE>: <SYMBOL>");
        t.prompt = rt_tr("Prepare an earnings <PHASE> for <SYMBOL>.\n"
                         "Quarter: <QUARTER>.\n"
                         "For a preview: consensus for revenue, EPS and guidance; the key metrics and segments to "
                         "watch; recent trends, estimate revisions and positioning; the options-implied move if "
                         "available; what would be a positive or negative surprise.\n"
                         "For a review: reported figures against consensus (a table); guidance changes; management's "
                         "commentary; segment details; the market's reaction; and what changed in the investment "
                         "case.");
        t.plan_hint = rt_tr("1) the expectations or the reported figures as a table, 2) segments and key metrics, "
                            "3) guidance and commentary, 4) market reaction / positioning, 5) conclusions.");
        t.expected_artefacts = {rt_tr("Earnings note (Markdown)"),
                                rt_tr("Table: consensus versus actual / expected figures (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("peer_comparison"), equities, rt_tr("Sector / peer comparison"),
                                rt_tr("A company against its closest listed peers: growth, profitability, balance sheet "
                                      "and valuation side by side."),
                                40, 5, 25);
        t.params = {rt_param(QStringLiteral("symbol"), rt_tr("Company"), TemplateParamType::Symbol, true, {},
                             rt_tr("Ticker, name or ISIN")),
                    rt_param(QStringLiteral("peers"), rt_tr("Peers"), TemplateParamType::Symbols, false, {},
                             rt_tr("Optional — empty lets the agent pick 4-6 peers")),
                    rt_param(QStringLiteral("metrics"), rt_tr("Metrics"), TemplateParamType::Text, false,
                             rt_tr("revenue growth, EBITDA margin, ROIC, net debt / EBITDA, P/E, EV/EBITDA, FCF yield"))};
        t.title_pattern = rt_tr("Peers: <SYMBOL>");
        t.prompt = rt_tr("Compare <SYMBOL> with its closest listed peers.\n"
                         "Peers to include: <PEERS>.\n"
                         "Metrics: <METRICS>.\n"
                         "Put the figures in one comparison table (latest fiscal year and trailing twelve months, with "
                         "dates), explain where <SYMBOL> is better or worse and why, and say whether its valuation "
                         "gap to the peers is justified.");
        t.plan_hint = rt_tr("1) pick and justify the peer set, 2) collect the metrics into one table, 3) explain the "
                            "differences, 4) valuation verdict.");
        t.expected_artefacts = {rt_tr("Comparison report (Markdown)"), rt_tr("Table: peer metrics (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("portfolio_review"), portfolio, rt_tr("Portfolio review"),
                                rt_tr("How a portfolio did and why: drivers by holding, concentration, risk and "
                                      "rebalancing ideas. Its holdings are read from the terminal."),
                                50, 6, 30);
        t.params = {rt_portfolio(), rt_period(QStringLiteral("30d")),
                    rt_param(QStringLiteral("focus"), rt_tr("Extra focus"), TemplateParamType::Text, false, {},
                             rt_tr("Optional, e.g. dividend income, tech exposure"))};
        t.title_pattern = rt_tr("Portfolio review: <PORTFOLIO_NAME>");
        t.prompt = rt_tr("Review my portfolio \"<PORTFOLIO_NAME>\" (its holdings are listed below) over <PERIOD>.\n"
                         "Cover: the performance of each holding and what drove it; concentration by position, sector "
                         "and currency; risk (volatility, drawdown, correlation between holdings); holdings whose "
                         "investment case changed; and concrete rebalancing ideas with their reasons.\n"
                         "Extra focus: <FOCUS>.\n"
                         "This is analysis only: do not place or propose orders.");
        t.plan_hint = rt_tr("1) performance by holding as a table, 2) exposures by sector and currency as a table, "
                            "3) risk, 4) holdings to review, 5) rebalancing ideas.");
        t.expected_artefacts = {rt_tr("Portfolio review (Markdown)"), rt_tr("Table: performance by holding (CSV)"),
                                rt_tr("Table: sector / currency exposure (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("risk_check"), portfolio, rt_tr("Portfolio risk check"),
                                rt_tr("Concentration, volatility, drawdowns, correlation, an approximate value-at-risk "
                                      "and a stress scenario, with hedging ideas."),
                                50, 6, 30);
        RunTemplateParam horizon = rt_param(QStringLiteral("horizon"), rt_tr("Horizon"), TemplateParamType::Choice, true,
                                            QStringLiteral("3 months"));
        horizon.choices = {QStringLiteral("1 month"), QStringLiteral("3 months"), QStringLiteral("12 months")};
        horizon.choice_labels = {rt_tr("1 month"), rt_tr("3 months"), rt_tr("12 months")};
        t.params = {rt_portfolio(), horizon,
                    rt_param(QStringLiteral("scenario"), rt_tr("Stress scenario"), TemplateParamType::Text, false,
                             rt_tr("rates +100 bp and equities -15%"),
                             rt_tr("e.g. oil +20%, recession, USD +10%"))};
        t.title_pattern = rt_tr("Risk check: <PORTFOLIO_NAME>");
        t.prompt = rt_tr("Run a risk check on my portfolio \"<PORTFOLIO_NAME>\" (its holdings are listed below) for a "
                         "<HORIZON> horizon.\n"
                         "Cover: concentration (single name, sector, country, currency); volatility and beta; historical "
                         "drawdowns; correlation clusters; liquidity; and an approximate value-at-risk with its "
                         "method.\n"
                         "Stress-test it against: <SCENARIO>.\n"
                         "Suggest hedges or position-size changes with reasons. This is analysis only: do not place or "
                         "propose orders.");
        t.plan_hint = rt_tr("1) exposures table, 2) volatility, beta and drawdowns, 3) correlation and liquidity, 4) "
                            "value-at-risk, 5) the stress scenario, 6) hedging ideas.");
        t.expected_artefacts = {rt_tr("Risk report (Markdown)"), rt_tr("Table: exposures (CSV)"),
                                rt_tr("Table: stress-test impact by holding (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("holdings_news_digest"), portfolio, rt_tr("News digest for holdings"),
                                rt_tr("The material news of every holding of a portfolio in a period, with its likely "
                                      "impact."),
                                40, 5, 25);
        t.params = {rt_portfolio(), rt_period(QStringLiteral("7d"))};
        t.title_pattern = rt_tr("News digest: <PORTFOLIO_NAME>");
        t.prompt = rt_tr("Build a news digest for the holdings of my portfolio \"<PORTFOLIO_NAME>\" (listed below) "
                         "covering <PERIOD>.\n"
                         "For each holding with material news: 1-3 bullets with the date and source, and its likely "
                         "impact (positive / negative / neutral) with a reason. Skip holdings without material news "
                         "and say which ones they were. End with the five most important items overall.");
        t.plan_hint = rt_tr("1) collect news per holding, 2) judge materiality and impact, 3) the top five.");
        t.expected_artefacts = {rt_tr("News digest (Markdown)"), rt_tr("Table: news by holding with impact (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("watchlist_screen"), markets, rt_tr("Watchlist screen"),
                                rt_tr("Rank the symbols of a watchlist on your criteria and flag the most interesting "
                                      "ones. The symbols are read from the terminal."),
                                50, 6, 30);
        t.params = {rt_param(QStringLiteral("watchlist"), rt_tr("Watchlist"), TemplateParamType::Watchlist, true, {},
                             rt_tr("Its symbols are added to the task when it starts")),
                    rt_param(QStringLiteral("criteria"), rt_tr("Criteria"), TemplateParamType::LongText, true,
                             rt_tr("momentum (1-month and 3-month returns), valuation against the 5-year average, "
                                   "upcoming earnings and news catalysts"))};
        t.title_pattern = rt_tr("Screen: <WATCHLIST_NAME>");
        t.prompt = rt_tr("Screen the symbols of my watchlist \"<WATCHLIST_NAME>\" (listed below) on: <CRITERIA>.\n"
                         "Rank them in one table with the figures behind the ranking (with dates), then flag the three "
                         "most interesting names with a short paragraph each on why.");
        t.plan_hint = rt_tr("1) collect the figures for every symbol, 2) score and rank them as a table, 3) the top "
                            "three with reasons.");
        t.expected_artefacts = {rt_tr("Screen report (Markdown)"), rt_tr("Table: ranked watchlist (CSV)")};
        out.append(t);
    }
    {
        RunTemplate t = rt_base(QStringLiteral("market_wrap"), markets, rt_tr("Macro / market wrap"),
                                rt_tr("Indices, sectors, rates, FX, commodities, macro data and central banks for a "
                                      "region and period, and what to watch next."),
                                30, 4, 20);
        RunTemplateParam region = rt_param(QStringLiteral("region"), rt_tr("Region"), TemplateParamType::Choice, true,
                                           QStringLiteral("Global"));
        region.choices = {QStringLiteral("Global"), QStringLiteral("US"), QStringLiteral("Europe"), QStringLiteral("India"),
                          QStringLiteral("Asia-Pacific")};
        region.choice_labels = {rt_tr("Global"), rt_tr("United States"), rt_tr("Europe"), rt_tr("India"),
                                rt_tr("Asia-Pacific")};
        t.params = {region, rt_period(QStringLiteral("1d")),
                    rt_param(QStringLiteral("focus"), rt_tr("Extra focus"), TemplateParamType::Text, false, {},
                             rt_tr("Optional, e.g. semiconductors, the yen"))};
        t.title_pattern = rt_tr("Market wrap: <REGION>");
        t.prompt = rt_tr("Write a market wrap for <REGION> markets covering <PERIOD>.\n"
                         "Cover: the main index moves; sector leaders and laggards; rates and the yield curve; FX; "
                         "commodities; notable macro data releases and central-bank news; and what to watch in the "
                         "coming days. Be factual and give the numbers with their dates.\n"
                         "Extra focus: <FOCUS>.");
        t.plan_hint = rt_tr("1) indices and sectors as a table, 2) rates, FX and commodities, 3) macro and central "
                            "banks, 4) the week ahead.");
        t.expected_artefacts = {rt_tr("Market wrap (Markdown)"), rt_tr("Table: index and asset moves (CSV)")};
        out.append(t);
    }
    for (RunTemplate& t : out)
        rt_finish(t);
    return out;
}

std::optional<RunTemplate> find_run_template(const QString& id) {
    for (const RunTemplate& t : run_templates())
        if (t.id == id)
            return t;
    return std::nullopt;
}

// ── Placeholders ─────────────────────────────────────────────────────────────

QString fill_run_template(const QString& prompt, const QJsonObject& context) {
    const QStringList lines = prompt.split(QLatin1Char('\n'));
    QStringList out;
    out.reserve(lines.size());
    for (const QString& line : lines) {
        QString result;
        int last = 0;
        bool drop = false;
        auto it = rt_placeholder_re().globalMatch(line);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            bool found = false;
            const QString value = rt_value(context, rt_norm(m.captured(1)), &found);
            result += line.mid(last, m.capturedStart() - last);
            if (!found) {
                result += m.captured(0); // nothing fills it: left for the user to see
            } else if (value.isEmpty()) {
                drop = true;             // an optional parameter left empty: the line goes
                break;
            } else {
                result += value;
            }
            last = static_cast<int>(m.capturedEnd());
        }
        if (drop)
            continue;
        result += line.mid(last);
        out << result;
    }
    return out.join(QLatin1Char('\n'));
}

QStringList run_template_placeholders(const QString& prompt) {
    QStringList out;
    auto it = rt_placeholder_re().globalMatch(prompt);
    while (it.hasNext()) {
        const QString name = it.next().captured(1).trimmed();
        if (!out.contains(name, Qt::CaseInsensitive))
            out << name;
    }
    return out;
}

RunRequest request_from_template(const RunTemplate& t, const QJsonObject& context) {
    RunRequest r;
    // A parameter the user left out counts as empty: its optional line is dropped
    // instead of reaching the agent as "<FOCUS>".
    QJsonObject fill = context;
    for (const RunTemplateParam& p : t.params)
        for (const QString& k : param_context_keys(p))
            if (!fill.contains(k))
                fill.insert(k, QString());
    r.title = fill_run_template(t.title_pattern.isEmpty() ? t.title : t.title_pattern, fill).simplified();
    if (r.title.isEmpty() || !run_template_placeholders(r.title).isEmpty())
        r.title = t.title;
    QString prompt = fill_run_template(t.prompt, fill).trimmed();
    if (!t.plan_hint.trimmed().isEmpty())
        prompt += QStringLiteral("\n\n") + rt_tr("Suggested approach: %1").arg(t.plan_hint.trimmed());
    if (!t.expected_artefacts.isEmpty())
        prompt += QStringLiteral("\n\n") + rt_tr("Expected output: %1. Put tabular figures in Markdown tables.")
                                               .arg(t.expected_artefacts.join(QStringLiteral("; ")));
    r.prompt = prompt;
    r.engine = t.engine;
    r.mode = t.mode;
    r.template_id = t.id;
    r.budget = t.budget;
    r.plan_first = t.plan_first;
    r.origin = QString::fromLatin1(kOriginUser);
    // The parameters only (the plan request lists the context to the agent).
    QJsonObject ctx;
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        if (it.value().isString() && it.value().toString().trimmed().isEmpty())
            continue;
        ctx.insert(it.key(), it.value());
    }
    r.context = ctx;
    return r;
}

QStringList validate_template_params(const RunTemplate& t, const QJsonObject& context) {
    QStringList errors;
    const auto text = [&context](const QString& key) { return context.value(key).toString().trimmed(); };
    for (const RunTemplateParam& p : t.params) {
        switch (p.type) {
            case TemplateParamType::Symbol: {
                const QString v = text(p.key);
                if (p.required && v.isEmpty())
                    errors << rt_tr("Pick a symbol for “%1”.").arg(p.label);
                else if (v.size() > 32)
                    errors << rt_tr("“%1” is not a symbol.").arg(v.left(40));
                break;
            }
            case TemplateParamType::Symbols:
                if (p.required && text(p.key).isEmpty())
                    errors << rt_tr("Add at least one symbol to “%1”.").arg(p.label);
                break;
            case TemplateParamType::Portfolio:
                if (p.required && text(QStringLiteral("portfolio_id")).isEmpty())
                    errors << rt_tr("Choose a portfolio.");
                break;
            case TemplateParamType::Watchlist:
                if (p.required && text(QStringLiteral("watchlist_id")).isEmpty())
                    errors << rt_tr("Choose a watchlist.");
                break;
            case TemplateParamType::DateRange: {
                const QString code = text(QStringLiteral("date_range"));
                if (p.required && code.isEmpty()) {
                    errors << rt_tr("Choose a period.");
                } else if (code == QLatin1String("custom")) {
                    const QDate from = QDate::fromString(text(QStringLiteral("date_from")), Qt::ISODate);
                    const QDate to = QDate::fromString(text(QStringLiteral("date_to")), Qt::ISODate);
                    if (!from.isValid() || !to.isValid())
                        errors << rt_tr("Enter both dates of the period.");
                    else if (from > to)
                        errors << rt_tr("The period starts after it ends.");
                }
                break;
            }
            case TemplateParamType::Choice: {
                const QString v = text(p.key);
                if (p.required && v.isEmpty())
                    errors << rt_tr("Choose “%1”.").arg(p.label);
                else if (!v.isEmpty() && !p.choices.isEmpty() && !p.choices.contains(v))
                    errors << rt_tr("“%1” is not one of the choices for “%2”.").arg(v, p.label);
                break;
            }
            case TemplateParamType::Text:
            case TemplateParamType::LongText: {
                const QString v = text(p.key);
                if (p.required && v.isEmpty())
                    errors << rt_tr("Fill in “%1”.").arg(p.label);
                else if (v.size() > (p.type == TemplateParamType::Text ? 500 : 4000))
                    errors << rt_tr("“%1” is too long.").arg(p.label);
                break;
            }
        }
    }
    // What the filled prompt still asks for (a user template's own placeholders).
    QJsonObject probe = context;
    for (const RunTemplateParam& p : t.params)
        for (const QString& k : param_context_keys(p))
            if (!probe.contains(k))
                probe.insert(k, QString()); // a parameter's key: empty is fine (line dropped)
    const QStringList left = run_template_placeholders(fill_run_template(t.prompt, probe));
    if (!left.isEmpty())
        errors << rt_tr("The task text has placeholders nothing fills: %1. Replace them with text.")
                      .arg(QLatin1Char('<') + left.join(QStringLiteral(">, <")) + QLatin1Char('>'));
    return errors;
}

// ── JSON ─────────────────────────────────────────────────────────────────────

QJsonObject run_template_to_json(const RunTemplate& t) {
    QJsonObject o;
    o["v"] = 1;
    o["id"] = t.id;
    o["title"] = t.title;
    o["description"] = t.description;
    o["prompt"] = t.prompt;
    o["engine"] = t.engine;
    o["mode"] = t.mode;
    o["plan_first"] = t.plan_first;
    QJsonObject b;
    b["max_credits"] = t.budget.max_credits;
    b["max_steps"] = t.budget.max_steps;
    b["max_minutes"] = t.budget.max_minutes;
    o["budget"] = b;
    QJsonArray params;
    for (const RunTemplateParam& p : t.params) {
        QJsonObject po;
        po["key"] = p.key;
        po["label"] = p.label;
        po["type"] = param_type_id(p.type);
        po["required"] = p.required;
        if (!p.default_value.isEmpty())
            po["default"] = p.default_value;
        if (!p.hint.isEmpty())
            po["hint"] = p.hint;
        if (!p.choices.isEmpty()) {
            po["choices"] = QJsonArray::fromStringList(p.choices);
            po["choice_labels"] = QJsonArray::fromStringList(p.choice_labels);
        }
        params.append(po);
    }
    o["params"] = params;
    o["title_pattern"] = t.title_pattern;
    o["plan_hint"] = t.plan_hint;
    o["expected_artefacts"] = QJsonArray::fromStringList(t.expected_artefacts);
    o["category"] = t.category;
    o["based_on"] = t.based_on;
    return o;
}

RunTemplate run_template_from_json(const QJsonObject& o) {
    RunTemplate t;
    t.id = o.value("id").toString();
    t.title = o.value("title").toString();
    t.description = o.value("description").toString();
    t.prompt = o.value("prompt").toString();
    t.engine = o.value("engine").toString(QString::fromLatin1(kEngineCloud));
    t.mode = o.value("mode").toString(QStringLiteral("deep"));
    t.plan_first = o.value("plan_first").toBool(true);
    const QJsonObject b = o.value("budget").toObject();
    t.budget.max_credits = b.value("max_credits").toDouble();
    t.budget.max_steps = b.value("max_steps").toInt();
    t.budget.max_minutes = b.value("max_minutes").toInt();
    for (const QJsonValue& v : o.value("params").toArray()) {
        const QJsonObject po = v.toObject();
        RunTemplateParam p;
        p.key = po.value("key").toString();
        p.label = po.value("label").toString(p.key);
        p.type = param_type_from_id(po.value("type").toString()).value_or(TemplateParamType::Text);
        p.required = po.value("required").toBool();
        p.default_value = po.value("default").toString();
        p.hint = po.value("hint").toString();
        for (const QJsonValue& c : po.value("choices").toArray())
            p.choices << c.toString();
        for (const QJsonValue& c : po.value("choice_labels").toArray())
            p.choice_labels << c.toString();
        if (p.key.isEmpty() && p.type != TemplateParamType::Portfolio && p.type != TemplateParamType::Watchlist &&
            p.type != TemplateParamType::DateRange)
            continue;
        t.params.append(p);
    }
    t.title_pattern = o.value("title_pattern").toString();
    t.plan_hint = o.value("plan_hint").toString();
    for (const QJsonValue& v : o.value("expected_artefacts").toArray())
        t.expected_artefacts << v.toString();
    t.category = o.value("category").toString();
    t.based_on = o.value("based_on").toString();
    t.user_defined = true;
    rt_finish(t);
    return t;
}

} // namespace fincept::agent_tasks
