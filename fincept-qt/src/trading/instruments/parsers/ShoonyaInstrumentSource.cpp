#include "trading/instruments/parsers/ShoonyaInstrumentSource.h"

#include "core/logging/Logger.h"
#include "trading/TradingTypes.h"
#include "trading/instruments/InstrumentDecompress.h"
#include "trading/instruments/InstrumentDownload.h"
#include "trading/instruments/InstrumentSource.h"
#include "trading/instruments/SymbolResolver.h"
#include "trading/instruments/parsers/NorenSymbolParser.h"

#include <QStringList>

namespace fincept::trading {
namespace {

// Per-exchange "<EX>_symbols.txt.zip" masters. Column layout differs per file.
const QStringList kExchanges = {"NSE", "NFO", "CDS", "MCX", "BSE", "BFO"};

// Combined-payload section marker (download joins all exchanges into one blob;
// parse splits on it because each exchange has a different column layout).
const QString kMarker = QStringLiteral("#EX:");

QByteArray download_shoonya(const BrokerCredentials&) {
    QByteArray combined;
    for (const QString& ex : kExchanges) {
        const QString url = QString("https://api.shoonya.com/%1_symbols.txt.zip").arg(ex);
        const QByteArray zip = http_get_blocking(url);
        if (zip.isEmpty())
            continue;
        const QByteArray txt = decompress::unzip_first_entry(zip);
        if (txt.isEmpty()) {
            LOG_WARN("ShoonyaSource", QString("Could not unzip %1").arg(url));
            continue;
        }
        combined += (kMarker + ex + "\n").toUtf8();
        combined += txt;
        if (!txt.endsWith('\n'))
            combined += '\n';
    }
    return combined;
}

// Extract a NorenRow from one comma-split line, per the exchange's column layout.
bool extract_row(const QString& ex, const QStringList& c, NorenRow& out) {
    out.brexchange = ex;
    if (ex == "NSE" || ex == "BSE") {
        // Exchange, Token, LotSize, Symbol, TradingSymbol, Instrument, TickSize
        if (c.size() < 6)
            return false;
        out.token = c[1];
        out.lot_size = c[2].toInt();
        out.name = c[3];
        out.brsymbol = c[4];
        out.raw_instrument = c[5];
        out.tick_size = (c.size() > 6 ? c[6].toDouble() : 5.0) / 100.0; // paise → rupees
        return true;
    }
    if (ex == "NFO" || ex == "BFO") {
        // Exchange, Token, LotSize, Symbol, TradingSymbol, Expiry, Instrument, OptionType, StrikePrice, TickSize
        if (c.size() < 10)
            return false;
        out.token = c[1];
        out.lot_size = c[2].toInt();
        out.name = c[3];
        out.brsymbol = c[4];
        out.expiry_raw = c[5];
        out.raw_instrument = c[6];
        out.option_type = c[7];
        out.strike = c[8].toDouble();
        out.tick_size = c[9].toDouble();
        return true;
    }
    if (ex == "CDS") {
        // Exchange, Token, LotSize, Precision, Multiplier, Symbol, TradingSymbol, Expiry, Instrument, OptionType, StrikePrice, TickSize
        if (c.size() < 12)
            return false;
        out.token = c[1];
        out.lot_size = c[2].toInt();
        out.name = c[5];
        out.brsymbol = c[6];
        out.expiry_raw = c[7];
        out.raw_instrument = c[8];
        out.option_type = c[9];
        out.strike = c[10].toDouble();
        out.tick_size = c[11].toDouble();
        return true;
    }
    if (ex == "MCX") {
        // Exchange, Token, LotSize, GNGD, Symbol, TradingSymbol, Expiry, Instrument, OptionType, StrikePrice, TickSize
        if (c.size() < 11)
            return false;
        out.token = c[1];
        out.lot_size = c[2].toInt();
        out.name = c[4];
        out.brsymbol = c[5];
        out.expiry_raw = c[6];
        out.raw_instrument = c[7];
        out.option_type = c[8];
        out.strike = c[9].toDouble();
        out.tick_size = c[10].toDouble();
        return true;
    }
    return false;
}

QVector<Instrument> parse_shoonya(const QByteArray& payload) {
    QVector<Instrument> out;
    if (payload.isEmpty())
        return out;

    const QString text = QString::fromUtf8(payload);
    const QList<QStringView> lines = QStringView(text).split(u'\n');
    QString ex;
    bool skip_header = false;
    for (const QStringView lv : lines) {
        const QString line = lv.trimmed().toString();
        if (line.isEmpty())
            continue;
        if (line.startsWith(kMarker)) {
            ex = line.mid(kMarker.size());
            skip_header = true; // next line is the file's own header row
            continue;
        }
        if (skip_header) {
            skip_header = false;
            continue;
        }
        const QStringList c = line.split(',');
        NorenRow r;
        if (!extract_row(ex, c, r))
            continue;
        if (r.token.toLongLong() == 0)
            continue;
        out.append(noren_build(QStringLiteral("shoonya"), r));
    }

    // BSE indices are not present in the master — Shoonya documents fixed tokens.
    for (const auto& idx : {qMakePair(QStringLiteral("SENSEX"), QStringLiteral("1")),
                            qMakePair(QStringLiteral("BANKEX"), QStringLiteral("12"))}) {
        NorenRow r;
        r.brexchange = "BSE";
        r.token = idx.second;
        r.name = idx.first;
        r.brsymbol = idx.first;
        r.raw_instrument = "INDEX";
        r.is_bse_index = true;
        out.append(noren_build(QStringLiteral("shoonya"), r));
    }

    LOG_INFO("ShoonyaSource", QString("Parsed %1 Shoonya instruments").arg(out.size()));
    return out;
}

} // namespace

void register_shoonya_source() {
    SymbolResolver::instance().register_source({QStringLiteral("shoonya"),
                                                [](const BrokerCredentials& c) { return download_shoonya(c); },
                                                [](const QByteArray& p) { return parse_shoonya(p); }});
}

} // namespace fincept::trading
