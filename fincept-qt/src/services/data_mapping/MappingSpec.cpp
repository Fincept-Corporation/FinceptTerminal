// MappingSpec.cpp — see MappingSpec.h.

#include "services/data_mapping/MappingSpec.h"

#include <QCoreApplication>
#include <QHash>

namespace fincept::services::datamapping {

namespace {

QString ms_tr(const char* text) {
    return QCoreApplication::translate("MappingSpec", text);
}

/// "$.a.b" / "a.b" / "$[0].x" → "a.b" / "[0].x".
QString ms_strip_root(QString p) {
    p = p.trimmed();
    if (p.startsWith(QLatin1Char('$')))
        p = p.mid(1);
    while (p.startsWith(QLatin1Char('.')))
        p = p.mid(1);
    return p;
}

} // namespace

QString target_type_name(TargetType t) {
    switch (t) {
        case TargetType::String:
            return QStringLiteral("string");
        case TargetType::Integer:
            return QStringLiteral("integer");
        case TargetType::Number:
            return QStringLiteral("number");
        case TargetType::Bool:
            return QStringLiteral("bool");
        case TargetType::Date:
            return QStringLiteral("date");
        case TargetType::DateTime:
            return QStringLiteral("datetime");
        case TargetType::Period:
            return QStringLiteral("period");
        case TargetType::Json:
            return QStringLiteral("json");
    }
    return QStringLiteral("string");
}

TargetType target_type_from_name(const QString& s) {
    const QString n = s.trimmed().toLower();
    if (n == QLatin1String("integer") || n == QLatin1String("int"))
        return TargetType::Integer;
    if (n == QLatin1String("number") || n == QLatin1String("float") || n == QLatin1String("double") ||
        n == QLatin1String("decimal"))
        return TargetType::Number;
    if (n == QLatin1String("bool") || n == QLatin1String("boolean"))
        return TargetType::Bool;
    if (n == QLatin1String("date"))
        return TargetType::Date;
    if (n == QLatin1String("datetime") || n == QLatin1String("timestamp"))
        return TargetType::DateTime;
    if (n == QLatin1String("period"))
        return TargetType::Period;
    if (n == QLatin1String("json") || n == QLatin1String("object"))
        return TargetType::Json;
    return TargetType::String;
}

QJsonObject SourceRef::to_json() const {
    QJsonObject o{{"kind", kind}, {"ref", ref}};
    if (!entity.isEmpty())
        o.insert(QStringLiteral("entity"), entity);
    if (!args.isEmpty())
        o.insert(QStringLiteral("args"), QJsonArray::fromStringList(args));
    if (!params.isEmpty())
        o.insert(QStringLiteral("params"), params);
    if (!table.isEmpty())
        o.insert(QStringLiteral("table"), table);
    if (!http.isEmpty())
        o.insert(QStringLiteral("http"), http);
    return o;
}

SourceRef SourceRef::from_json(const QJsonObject& o) {
    SourceRef s;
    s.kind = o.value(QStringLiteral("kind")).toString();
    s.ref = o.value(QStringLiteral("ref")).toString();
    s.entity = o.value(QStringLiteral("entity")).toString();
    for (const QJsonValue& a : o.value(QStringLiteral("args")).toArray())
        s.args << a.toString();
    s.params = o.value(QStringLiteral("params")).toObject();
    s.table = o.value(QStringLiteral("table")).toString();
    s.http = o.value(QStringLiteral("http")).toObject();
    return s;
}

QString SourceRef::describe() const {
    if (kind == QLatin1String("connection"))
        return entity.isEmpty() ? ms_tr("connection") : ms_tr("connection · %1").arg(entity);
    if (kind == QLatin1String("connector"))
        return QStringLiteral("%1 · %2 %3").arg(ref, entity, args.join(QLatin1Char(' '))).trimmed();
    if (kind == QLatin1String("file"))
        return ms_tr("file · %1").arg(ref);
    if (kind == QLatin1String("http")) {
        const QString base = http.value(QStringLiteral("base_url")).toString();
        const QString ep = http.value(QStringLiteral("endpoint")).toString();
        return ms_tr("HTTP %1 %2%3").arg(http.value(QStringLiteral("method")).toString(QStringLiteral("GET")), base, ep);
    }
    return ms_tr("no source");
}

QJsonObject FieldMapping::to_json() const {
    QJsonObject o{{"target", target}, {"type", target_type_name(type)}, {"source", source}};
    if (!path.isEmpty())
        o.insert(QStringLiteral("path"), path);
    if (!format.isEmpty())
        o.insert(QStringLiteral("format"), format);
    if (scale != 1.0)
        o.insert(QStringLiteral("scale"), scale);
    if (!default_value.isEmpty())
        o.insert(QStringLiteral("default"), default_value);
    if (required)
        o.insert(QStringLiteral("required"), true);
    if (!transform.isEmpty())
        o.insert(QStringLiteral("transform"), transform);
    return o;
}

FieldMapping FieldMapping::from_json(const QJsonObject& o) {
    FieldMapping f;
    f.target = o.value(QStringLiteral("target")).toString();
    f.type = target_type_from_name(o.value(QStringLiteral("type")).toString());
    f.source = o.value(QStringLiteral("source")).toString();
    f.path = o.value(QStringLiteral("path")).toString();
    f.format = o.value(QStringLiteral("format")).toString();
    const double sc = o.value(QStringLiteral("scale")).toDouble(1.0);
    f.scale = sc == 0.0 ? 1.0 : sc;
    f.default_value = o.value(QStringLiteral("default")).toString();
    f.required = o.value(QStringLiteral("required")).toBool();
    f.transform = o.value(QStringLiteral("transform")).toString();
    return f;
}

QJsonObject MappingOptions::to_json() const {
    return QJsonObject{{"decimal", decimal},
                       {"null_tokens", QJsonArray::fromStringList(null_tokens)},
                       {"key_fields", QJsonArray::fromStringList(key_fields)},
                       {"duplicates", duplicates},
                       {"drop_invalid_rows", drop_invalid_rows},
                       {"trim_text", trim_text}};
}

MappingOptions MappingOptions::from_json(const QJsonObject& o) {
    MappingOptions m;
    if (o.contains(QStringLiteral("decimal")))
        m.decimal = o.value(QStringLiteral("decimal")).toString(QStringLiteral("auto"));
    if (o.contains(QStringLiteral("null_tokens"))) {
        m.null_tokens.clear();
        for (const QJsonValue& v : o.value(QStringLiteral("null_tokens")).toArray())
            m.null_tokens << v.toString();
    }
    for (const QJsonValue& v : o.value(QStringLiteral("key_fields")).toArray())
        m.key_fields << v.toString();
    m.duplicates = o.value(QStringLiteral("duplicates")).toString(QStringLiteral("keep"));
    m.drop_invalid_rows = o.value(QStringLiteral("drop_invalid_rows")).toBool();
    m.trim_text = o.value(QStringLiteral("trim_text")).toBool(true);
    return m;
}

QJsonObject MappingSpec::to_json() const {
    QJsonArray f;
    for (const FieldMapping& m : fields)
        f.append(m.to_json());
    return QJsonObject{{"version", version},
                       {"target_schema", target_schema},
                       {"source", source.to_json()},
                       {"fields", f},
                       {"options", options.to_json()}};
}

MappingSpec MappingSpec::from_json(const QJsonObject& o) {
    MappingSpec s;
    s.version = o.value(QStringLiteral("version")).toInt(2);
    s.target_schema = o.value(QStringLiteral("target_schema")).toString();
    s.source = SourceRef::from_json(o.value(QStringLiteral("source")).toObject());
    for (const QJsonValue& v : o.value(QStringLiteral("fields")).toArray())
        s.fields << FieldMapping::from_json(v.toObject());
    s.options = MappingOptions::from_json(o.value(QStringLiteral("options")).toObject());
    return s;
}

int MappingSpec::index_of(const QString& target) const {
    for (int i = 0; i < fields.size(); ++i)
        if (fields[i].target == target)
            return i;
    return -1;
}

MappingSpec spec_from_legacy(const LegacyMapping& legacy) {
    MappingSpec spec;
    spec.target_schema = legacy.schema_name.isEmpty() ? QStringLiteral("CUSTOM") : legacy.schema_name;
    spec.source.kind = QStringLiteral("http");
    spec.source.ref = legacy.base_url;
    QJsonObject http{{"base_url", legacy.base_url},
                     {"endpoint", legacy.endpoint},
                     {"method", legacy.method.isEmpty() ? QStringLiteral("GET") : legacy.method},
                     {"auth_type", legacy.auth_type.isEmpty() ? QStringLiteral("None") : legacy.auth_type},
                     {"headers", legacy.headers},
                     {"body", legacy.body}};

    // Do all non-empty expressions project the same array ("$.data[*].x")?
    QString common_prefix;
    bool all_projected = true;
    int with_expr = 0;
    for (const QJsonValue& v : legacy.field_mappings) {
        const QString expr = ms_strip_root(v.toObject().value(QStringLiteral("expression")).toString());
        if (expr.isEmpty())
            continue;
        ++with_expr;
        const int star = int(expr.indexOf(QStringLiteral("[*]")));
        if (star < 0) {
            all_projected = false;
            break;
        }
        const QString prefix = expr.left(star);
        if (common_prefix.isNull())
            common_prefix = prefix;
        else if (prefix != common_prefix) {
            all_projected = false;
            break;
        }
    }
    const bool rows_mode = all_projected && with_expr > 0;
    // Rows mode: the records live at the common prefix ("" = the response itself is the array).
    // Document mode: the whole response is one row (column "$"), each expression a path inside it.
    http.insert(QStringLiteral("rows_path"), rows_mode ? common_prefix : QString());
    http.insert(QStringLiteral("document_mode"), !rows_mode);
    spec.source.http = http;

    const TargetSchema* schema = find_target_schema(legacy.schema_name);
    for (const QJsonValue& v : legacy.field_mappings) {
        const QJsonObject fm = v.toObject();
        FieldMapping f;
        f.target = fm.value(QStringLiteral("target")).toString();
        if (f.target.isEmpty())
            continue;
        if (schema) {
            for (const SchemaField& sf : schema->fields) {
                if (sf.name == f.target) {
                    f.type = sf.type;
                    f.required = sf.required;
                    break;
                }
            }
        }
        const QString expr = ms_strip_root(fm.value(QStringLiteral("expression")).toString());
        if (!expr.isEmpty()) {
            if (rows_mode) {
                QString rest = expr.mid(int(expr.indexOf(QStringLiteral("[*]"))) + 3);
                while (rest.startsWith(QLatin1Char('.')))
                    rest = rest.mid(1);
                if (rest.isEmpty()) {
                    f.source = QStringLiteral("value");
                } else {
                    const int dot = int(rest.indexOf(QLatin1Char('.')));
                    const int br = int(rest.indexOf(QLatin1Char('[')));
                    int cut = -1;
                    if (dot >= 0 && br >= 0)
                        cut = qMin(dot, br);
                    else
                        cut = qMax(dot, br);
                    f.source = cut < 0 ? rest : rest.left(cut);
                    f.path = cut < 0 ? QString() : rest.mid(cut).startsWith(QLatin1Char('.')) ? rest.mid(cut + 1)
                                                                                              : rest.mid(cut);
                }
            } else {
                f.source = QStringLiteral("$");
                f.path = expr;
            }
        }
        const QString tr = fm.value(QStringLiteral("transform")).toString().trimmed();
        if (tr == QLatin1String("to_number")) {
            f.type = f.type == TargetType::Integer ? TargetType::Integer : TargetType::Number;
        } else if (tr == QLatin1String("to_string")) {
            f.type = TargetType::String;
        } else if (tr == QLatin1String("unix_ms_to_iso")) {
            f.type = TargetType::DateTime;
            f.format = QStringLiteral("epoch_ms");
        } else if (tr == QLatin1String("unix_s_to_iso")) {
            f.type = TargetType::DateTime;
            f.format = QStringLiteral("epoch_s");
        } else if (tr == QLatin1String("unix_to_iso")) {
            f.type = TargetType::DateTime;
            f.format = QStringLiteral("epoch");
        } else if (!tr.isEmpty()) {
            f.transform = tr; // upper / lower / abs_value / …
        }
        f.default_value = fm.contains(QStringLiteral("default_val")) ? fm.value(QStringLiteral("default_val")).toString()
                                                                     : fm.value(QStringLiteral("default")).toString();
        spec.fields << f;
    }
    return spec;
}

const QVector<TargetSchema>& target_schemas() {
    static const QVector<TargetSchema> kSchemas = [] {
        QVector<TargetSchema> s;
        s.append({QStringLiteral("OHLCV"),
                  ms_tr("Price bars: open, high, low, close and volume per period"),
                  {{QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("timestamp"), TargetType::DateTime, true, ms_tr("Bar time")},
                   {QStringLiteral("open"), TargetType::Number, true, ms_tr("Opening price")},
                   {QStringLiteral("high"), TargetType::Number, true, ms_tr("Highest price")},
                   {QStringLiteral("low"), TargetType::Number, true, ms_tr("Lowest price")},
                   {QStringLiteral("close"), TargetType::Number, true, ms_tr("Closing price")},
                   {QStringLiteral("volume"), TargetType::Number, true, ms_tr("Traded volume")},
                   {QStringLiteral("vwap"), TargetType::Number, false, ms_tr("Volume weighted average price")},
                   {QStringLiteral("trades"), TargetType::Integer, false, ms_tr("Number of trades")}}});
        s.append({QStringLiteral("QUOTE"),
                  ms_tr("Latest quote per symbol"),
                  {{QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("timestamp"), TargetType::DateTime, true, ms_tr("Quote time")},
                   {QStringLiteral("price"), TargetType::Number, true, ms_tr("Last traded price")},
                   {QStringLiteral("bid"), TargetType::Number, false, ms_tr("Best bid")},
                   {QStringLiteral("ask"), TargetType::Number, false, ms_tr("Best ask")},
                   {QStringLiteral("bidSize"), TargetType::Number, false, ms_tr("Bid size")},
                   {QStringLiteral("askSize"), TargetType::Number, false, ms_tr("Ask size")},
                   {QStringLiteral("volume"), TargetType::Number, false, ms_tr("Volume")},
                   {QStringLiteral("change"), TargetType::Number, false, ms_tr("Change from previous close")},
                   {QStringLiteral("changePercent"), TargetType::Number, false, ms_tr("Change in percent")},
                   {QStringLiteral("open"), TargetType::Number, false, ms_tr("Day open")},
                   {QStringLiteral("high"), TargetType::Number, false, ms_tr("Day high")},
                   {QStringLiteral("low"), TargetType::Number, false, ms_tr("Day low")},
                   {QStringLiteral("previousClose"), TargetType::Number, false, ms_tr("Previous close")}}});
        s.append({QStringLiteral("TICK"),
                  ms_tr("Individual trades"),
                  {{QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("timestamp"), TargetType::DateTime, true, ms_tr("Trade time")},
                   {QStringLiteral("price"), TargetType::Number, true, ms_tr("Trade price")},
                   {QStringLiteral("quantity"), TargetType::Number, true, ms_tr("Trade quantity")},
                   {QStringLiteral("side"), TargetType::String, false, ms_tr("BUY / SELL / UNKNOWN")},
                   {QStringLiteral("tradeId"), TargetType::String, false, ms_tr("Trade id")}}});
        s.append({QStringLiteral("ORDER"),
                  ms_tr("Orders"),
                  {{QStringLiteral("orderId"), TargetType::String, true, ms_tr("Order id")},
                   {QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("side"), TargetType::String, true, ms_tr("BUY / SELL")},
                   {QStringLiteral("type"), TargetType::String, true, ms_tr("MARKET / LIMIT / STOP / STOP_LIMIT")},
                   {QStringLiteral("quantity"), TargetType::Number, true, ms_tr("Quantity")},
                   {QStringLiteral("filledQuantity"), TargetType::Number, false, ms_tr("Filled quantity")},
                   {QStringLiteral("price"), TargetType::Number, false, ms_tr("Limit price")},
                   {QStringLiteral("averagePrice"), TargetType::Number, false, ms_tr("Average fill price")},
                   {QStringLiteral("status"), TargetType::String, true, ms_tr("PENDING / OPEN / FILLED / CANCELLED")},
                   {QStringLiteral("timestamp"), TargetType::DateTime, true, ms_tr("Placement time")},
                   {QStringLiteral("exchange"), TargetType::String, false, ms_tr("Exchange")}}});
        s.append({QStringLiteral("POSITION"),
                  ms_tr("Open positions"),
                  {{QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("quantity"), TargetType::Number, true, ms_tr("Quantity")},
                   {QStringLiteral("averagePrice"), TargetType::Number, true, ms_tr("Average entry price")},
                   {QStringLiteral("currentPrice"), TargetType::Number, false, ms_tr("Current price")},
                   {QStringLiteral("marketValue"), TargetType::Number, false, ms_tr("Market value")},
                   {QStringLiteral("costBasis"), TargetType::Number, false, ms_tr("Cost basis")},
                   {QStringLiteral("pnl"), TargetType::Number, false, ms_tr("Unrealised P&L")},
                   {QStringLiteral("pnlPercent"), TargetType::Number, false, ms_tr("P&L percent")},
                   {QStringLiteral("exchange"), TargetType::String, false, ms_tr("Exchange")}}});
        s.append({QStringLiteral("PORTFOLIO"),
                  ms_tr("Portfolio snapshots"),
                  {{QStringLiteral("timestamp"), TargetType::DateTime, true, ms_tr("Snapshot time")},
                   {QStringLiteral("totalValue"), TargetType::Number, true, ms_tr("Total value")},
                   {QStringLiteral("cash"), TargetType::Number, false, ms_tr("Cash")},
                   {QStringLiteral("invested"), TargetType::Number, false, ms_tr("Invested")},
                   {QStringLiteral("marketValue"), TargetType::Number, false, ms_tr("Market value of holdings")},
                   {QStringLiteral("pnl"), TargetType::Number, false, ms_tr("Unrealised P&L")},
                   {QStringLiteral("pnlPercent"), TargetType::Number, false, ms_tr("P&L percent")},
                   {QStringLiteral("dayPnl"), TargetType::Number, false, ms_tr("Today's P&L")}}});
        s.append({QStringLiteral("INSTRUMENT"),
                  ms_tr("Instrument master"),
                  {{QStringLiteral("symbol"), TargetType::String, true, ms_tr("Trading symbol")},
                   {QStringLiteral("name"), TargetType::String, true, ms_tr("Instrument name")},
                   {QStringLiteral("exchange"), TargetType::String, false, ms_tr("Exchange")},
                   {QStringLiteral("instrumentType"), TargetType::String, false, ms_tr("EQUITY / FUTURES / OPTIONS / …")},
                   {QStringLiteral("currency"), TargetType::String, false, ms_tr("Trading currency")},
                   {QStringLiteral("isin"), TargetType::String, false, ms_tr("ISIN")},
                   {QStringLiteral("lotSize"), TargetType::Number, false, ms_tr("Lot size")},
                   {QStringLiteral("tickSize"), TargetType::Number, false, ms_tr("Tick size")}}});
        s.append({QStringLiteral("TIMESERIES"),
                  ms_tr("One value per date (macro series, indicators, rates)"),
                  {{QStringLiteral("series"), TargetType::String, false, ms_tr("Series id / name")},
                   {QStringLiteral("date"), TargetType::Date, true, ms_tr("Observation date")},
                   {QStringLiteral("value"), TargetType::Number, true, ms_tr("Observation value")},
                   {QStringLiteral("unit"), TargetType::String, false, ms_tr("Unit")}}});
        s.append({QStringLiteral("MACRO"),
                  ms_tr("Indicator values by country and period (2024, 2024-Q1, 2024-03)"),
                  {{QStringLiteral("country"), TargetType::String, true, ms_tr("Country code or name")},
                   {QStringLiteral("indicator"), TargetType::String, true, ms_tr("Indicator id")},
                   {QStringLiteral("period"), TargetType::Period, true, ms_tr("Year, quarter or month")},
                   {QStringLiteral("value"), TargetType::Number, true, ms_tr("Value")},
                   {QStringLiteral("unit"), TargetType::String, false, ms_tr("Unit")}}});
        return s;
    }();
    return kSchemas;
}

const TargetSchema* find_target_schema(const QString& name) {
    for (const TargetSchema& s : target_schemas())
        if (s.name.compare(name, Qt::CaseInsensitive) == 0)
            return &s;
    return nullptr;
}

QString suggest_source_column(const QString& target, const QStringList& source_columns) {
    if (target.isEmpty() || source_columns.isEmpty())
        return QString();
    for (const QString& c : source_columns)
        if (c == target)
            return c;
    auto norm = [](QString s) {
        s = s.toLower();
        s.remove(QLatin1Char('_'));
        s.remove(QLatin1Char('-'));
        s.remove(QLatin1Char(' '));
        s.remove(QLatin1Char('.'));
        return s;
    };
    const QString t = norm(target);
    for (const QString& c : source_columns)
        if (norm(c) == t)
            return c;
    static const QHash<QString, QStringList> kSynonyms = {
        {QStringLiteral("timestamp"), {"date", "datetime", "time", "ts", "t", "period", "asof", "tradedate"}},
        {QStringLiteral("date"), {"timestamp", "datetime", "time", "period", "asof", "obsdate", "refdate", "day"}},
        {QStringLiteral("period"), {"date", "year", "quarter", "month", "time", "timeperiod"}},
        {QStringLiteral("close"), {"adjclose", "closeprice", "last", "price", "c", "lastprice"}},
        {QStringLiteral("open"), {"openprice", "o"}},
        {QStringLiteral("high"), {"highprice", "h"}},
        {QStringLiteral("low"), {"lowprice", "l"}},
        {QStringLiteral("volume"), {"vol", "v", "qty", "quantity", "totalvolume"}},
        {QStringLiteral("price"), {"last", "lastprice", "close", "value", "rate", "p"}},
        {QStringLiteral("symbol"), {"ticker", "code", "instrument", "sym", "s", "seriesid"}},
        {QStringLiteral("name"), {"title", "label", "description", "longname", "shortname"}},
        {QStringLiteral("value"), {"obsvalue", "observation", "val", "amount", "rate", "price", "close"}},
        {QStringLiteral("country"), {"countrycode", "iso", "iso2code", "iso3", "region", "economy", "key"}},
        {QStringLiteral("indicator"), {"indicatorid", "series", "seriesid", "code", "concept"}},
        {QStringLiteral("series"), {"seriesid", "indicator", "code", "name"}},
        {QStringLiteral("quantity"), {"qty", "size", "amount", "shares", "volume"}},
        {QStringLiteral("changepercent"), {"pctchange", "changepct", "percentchange", "dp"}},
        {QStringLiteral("change"), {"chg", "d", "netchange"}},
        {QStringLiteral("previousclose"), {"prevclose", "pc", "priorclose"}},
        {QStringLiteral("currency"), {"ccy", "curr"}},
    };
    const QStringList syn = kSynonyms.value(t);
    for (const QString& s : syn)
        for (const QString& c : source_columns)
            if (norm(c) == s)
                return c;
    return QString();
}

} // namespace fincept::services::datamapping
