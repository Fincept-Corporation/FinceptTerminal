#include "trading/instruments/parsers/MotilalInstrumentParser.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/CsvUtil.h"

#include <QRegularExpression>
#include <QStringList>
#include <QStringView>

namespace fincept::trading {
namespace {

// Public scrip-master + index CSVs per exchange (no auth). Each file has its own
// header row, so download joins them into one payload with section markers and
// parse rebuilds a csv::header_index per section.
const QStringList kScripExchanges = {"NSE", "BSE", "NSEFO", "NSECD", "MCX", "BSEFO"};
const QStringList kIndexExchanges = {"NSE", "BSE"};

const QString kScripMarker = QStringLiteral("#SCRIP:");
const QString kIndexMarker = QStringLiteral("#INDEX:");

QByteArray download_motilal(const BrokerCredentials&) {
    QByteArray combined;
    for (const QString& ex : kScripExchanges) {
        const QString url = QString("https://openapi.motilaloswal.com/getscripmastercsv?name=%1").arg(ex);
        const QByteArray body = http_get_blocking(url, {}, 120000);
        if (body.isEmpty())
            continue;
        combined += (kScripMarker + ex + "\n").toUtf8();
        combined += body;
        combined += '\n';
    }
    for (const QString& ex : kIndexExchanges) {
        const QString url = QString("https://openapi.motilaloswal.com/getindexdatacsv?name=%1").arg(ex);
        const QByteArray body = http_get_blocking(url, {}, 120000);
        if (body.isEmpty())
            continue;
        combined += (kIndexMarker + ex + "\n").toUtf8();
        combined += body;
        combined += '\n';
    }
    return combined;
}

// Motilal exchange code → canonical exchange. Unmapped codes are skipped.
QString canon_exchange(const QString& raw) {
    const QString e = raw.trimmed().toUpper();
    if (e == "NSE")
        return "NSE";
    if (e == "BSE")
        return "BSE";
    if (e == "NSEFO")
        return "NFO";
    if (e == "NSECD")
        return "CDS";
    if (e == "MCX")
        return "MCX";
    if (e == "BSEFO")
        return "BFO";
    if (e == "BSECD")
        return "BCD";
    return {};
}

void parse_scrip_row(const QStringList& row, const QHash<QString, int>& idx, QVector<Instrument>& out) {
    const QString token = csv::field(row, idx, "scripcode");
    if (token.isEmpty() || token.toLongLong() == 0)
        return;

    const QString raw_exch = csv::field(row, idx, "exchangename");
    const QString exch = canon_exchange(raw_exch);
    if (exch.isEmpty())
        return;

    const QString brsym = csv::field(row, idx, "scripname");
    const QString underlying = csv::field(row, idx, "scripshortname").toUpper();
    const QString instr = csv::field(row, idx, "instrumentname").toUpper();
    const QString opt = csv::field(row, idx, "optiontype").toUpper();

    const bool is_deriv = (exch == "NFO" || exch == "BFO" || exch == "CDS" || exch == "MCX");

    InstrumentType t;
    if (opt == "CE")
        t = InstrumentType::CE;
    else if (opt == "PE")
        t = InstrumentType::PE;
    else if (opt == "XX")
        t = InstrumentType::FUT;
    else if (instr.contains("IDX") && (exch == "NSE" || exch == "BSE" || exch == "MCX"))
        t = InstrumentType::INDEX;
    else if (is_deriv)
        t = InstrumentType::FUT;
    else
        t = InstrumentType::EQ;

    double strike = csv::field(row, idx, "strikeprice").toDouble();
    if (strike <= 0.0)
        strike = 0.0;
    const double tick = csv::field(row, idx, "ticksize").toDouble();

    // Expiry is embedded in the scrip name string, e.g. "NIFTY 25-Aug-2026 CE".
    static const QRegularExpression kExpRe(QStringLiteral("\\d{1,2}-[A-Za-z]{3}-\\d{4}"));
    QString exp_nd;
    const QRegularExpressionMatch m = kExpRe.match(brsym);
    if (m.hasMatch())
        exp_nd = norm::expiry_to_nodash(m.captured(0)); // "25-Aug-2026"

    Instrument inst;
    inst.broker_id = "motilal";
    inst.instrument_token = token.toLongLong();
    inst.exchange_token = inst.instrument_token;
    inst.brsymbol = brsym;
    inst.name = underlying;
    inst.brexchange = raw_exch.trimmed().toUpper();
    inst.instrument_type = t;
    inst.exchange = norm::normalise_exchange(exch, t);
    inst.strike = strike;
    inst.lot_size = qMax(1, csv::field(row, idx, "marketlot").toInt());
    inst.tick_size = tick > 0.0 ? tick : 0.05;
    inst.expiry = norm::nodash_to_display(exp_nd);

    const QString fallback = (t == InstrumentType::EQ) ? brsym : underlying;
    inst.symbol = norm::synthesize_symbol(underlying, t, exp_nd, strike, fallback);
    if (inst.symbol.isEmpty())
        return;
    out.append(inst);
}

void parse_index_row(const QString& section_ex, const QStringList& row, const QHash<QString, int>& idx,
                     QVector<Instrument>& out) {
    const QString token = csv::field(row, idx, "indexcode");
    const QString raw_exch = csv::field(row, idx, "exchangename");
    const QString index_name = csv::field(row, idx, "indexname");
    if (index_name.isEmpty())
        return;

    Instrument inst;
    inst.broker_id = "motilal";
    inst.symbol = norm::normalise_index_symbol(index_name);
    inst.name = inst.symbol;
    inst.brsymbol = index_name;
    inst.brexchange = raw_exch.trimmed().toUpper();
    inst.exchange = (section_ex == "BSE") ? "BSE_INDEX" : "NSE_INDEX";
    inst.instrument_type = InstrumentType::INDEX;
    inst.lot_size = 1;
    inst.tick_size = 0.05;
    const qint64 tok = token.toLongLong();
    inst.instrument_token = tok != 0 ? tok : norm::stable_token(index_name);
    inst.exchange_token = inst.instrument_token;
    if (inst.symbol.isEmpty())
        return;
    out.append(inst);
}

QVector<Instrument> parse_motilal(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    enum class Kind { None, Scrip, Index };
    Kind kind = Kind::None;
    QString section_ex;
    QHash<QString, int> idx;
    bool want_header = false;

    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kScripMarker)) {
            kind = Kind::Scrip;
            section_ex = line.mid(kScripMarker.size());
            want_header = true; // next line is this file's CSV header
            continue;
        }
        if (line.startsWith(kIndexMarker)) {
            kind = Kind::Index;
            section_ex = line.mid(kIndexMarker.size());
            want_header = true;
            continue;
        }
        if (want_header) {
            idx = csv::header_index(lv);
            want_header = false;
            continue;
        }
        const QStringList row = csv::split_csv_line(lv);
        if (kind == Kind::Scrip)
            parse_scrip_row(row, idx, out);
        else if (kind == Kind::Index)
            parse_index_row(section_ex, row, idx, out);
    }

    LOG_INFO("MotilalSource", QString("Parsed %1 Motilal instruments").arg(out.size()));
    return out;
}

} // namespace

void register_motilal_source() {
    SymbolResolver::instance().register_source({QStringLiteral("motilal"),
                                                [](const BrokerCredentials& c) { return download_motilal(c); },
                                                [](const QByteArray& p) { return parse_motilal(p); }});
}

} // namespace fincept::trading
