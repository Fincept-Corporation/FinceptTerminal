#include "trading/instruments/IciciInstrumentParser.h"

#include "trading/instruments/InstrumentNormalize.h"

#include "core/logging/Logger.h"

#include <QString>
#include <QTextStream>

namespace fincept::trading {
namespace {

// Format a strike for use inside a normalised option symbol: drop the decimal
// when it is a whole number ("22750"), keep two places otherwise ("22750.50").
// Uniquely named so it cannot collide with the other parsers' helpers when the
// instrument parsers are concatenated under a unity build.
QString icici_strike_str(double strike) {
    // Shared strike formatting — identical across every broker.
    return norm::format_strike(strike);
}

// "30-Mar-2026" → {dd:"30", mon:"MAR", yy:"26"}. ICICI always ships the
// "DD-Mon-YYYY" form, so a split on '-' is both correct and locale-independent
// (avoids QDate's locale-sensitive MMM parsing).
struct IciciExpiry {
    QString display; // "30-MAR-2026"
    QString ddmmmyy; // "30MAR26"
    bool valid = false;
};

IciciExpiry icici_parse_expiry(const QString& raw) {
    IciciExpiry e;
    const QStringList p = raw.split('-');
    if (p.size() != 3)
        return e;
    const QString dd = p[0].trimmed();
    const QString mon = p[1].trimmed().toUpper();
    const QString yyyy = p[2].trimmed();
    if (dd.isEmpty() || mon.isEmpty() || yyyy.size() < 2)
        return e;
    e.display = dd + "-" + mon + "-" + yyyy;
    e.ddmmmyy = dd + mon + yyyy.right(2);
    e.valid = true;
    return e;
}

} // namespace

Instrument IciciInstrumentParser::parse_row(const QStringList& cols) {
    const int n = cols.size();
    if (n < 13)
        return {};

    // The only free-text field is SN (the company name, col 1), which could in
    // theory contain a comma in this unquoted CSV. Anchor the structured fields:
    // SC from the front, everything else from the end so a comma in the name
    // never shifts the columns we rely on.
    const QString sc = cols[0].trimmed();          // ICICI stock code
    const QString ec = cols[n - 11].trimmed();     // exchange (NSE/BSE/NFO/BFO/MCX)
    const QString sm = cols[n - 10].trimmed();     // short mnemonic / contract encoding
    const QString sg = cols[n - 9].trimmed();      // segment
    const QString tk = cols[n - 8].trimmed();      // numeric token
    const QString ls = cols[n - 7].trimmed();      // lot size
    const QString ns = cols[n - 5].trimmed();      // exchange ticker symbol
    const QString ts = cols[n - 4].trimmed();      // tick size

    if (sc.isEmpty() || ec.isEmpty())
        return {};

    Instrument inst;
    inst.broker_id = QStringLiteral("icicidirect");
    inst.brexchange = ec.toUpper();
    inst.exchange = ec.toUpper();

    bool token_ok = false;
    const qint64 token = tk.toLongLong(&token_ok);
    inst.instrument_token = token_ok ? token : 0;
    inst.exchange_token = inst.instrument_token;

    const double lot = ls.toDouble();
    inst.lot_size = lot > 0 ? static_cast<int>(lot) : 1;
    const double tick = ts.toDouble();
    inst.tick_size = tick > 0 ? tick : 0.05;

    const QString underlying_ticker = ns.isEmpty() ? sc.toUpper() : ns.toUpper();

    // Derivatives encode the contract in SM as "<UNDER>~F:…" or "<UNDER>~O:…".
    // Equities have no '~'.
    const int tilde = sm.indexOf('~');
    const bool is_derivative = tilde >= 0 && (sg.compare("EQUITY", Qt::CaseInsensitive) != 0);

    if (!is_derivative) {
        // Cash equity / ETF.
        inst.instrument_type = InstrumentType::EQ;
        inst.brsymbol = sc;
        inst.symbol = underlying_ticker;
        inst.name = underlying_ticker;
        return inst;
    }

    // Derivative: decode the part after '~'. "F:DD-Mon-YYYY" or
    // "O:DD-Mon-YYYY:CE|PE:<strike×100>".
    const QString rest = sm.mid(tilde + 1);
    // brsymbol carries the underlying ICICI code — Breeze's `stock_code` on F&O
    // orders — with expiry/strike/right supplied separately from these fields.
    inst.brsymbol = sc;
    inst.name = underlying_ticker;

    if (rest.startsWith("F:")) {
        const IciciExpiry exp = icici_parse_expiry(rest.mid(2));
        if (!exp.valid)
            return {};
        inst.instrument_type = InstrumentType::FUT;
        inst.expiry = exp.display;
        inst.strike = 0.0;
        inst.symbol = underlying_ticker + exp.ddmmmyy + "FUT";
        return inst;
    }

    if (rest.startsWith("O:")) {
        const QStringList parts = rest.mid(2).split(':'); // [expiry, CE|PE, strike×100]
        if (parts.size() < 3)
            return {};
        const IciciExpiry exp = icici_parse_expiry(parts[0]);
        if (!exp.valid)
            return {};
        const QString right = parts[1].trimmed().toUpper();
        const double strike = parts[2].trimmed().toDouble() / 100.0;
        inst.instrument_type = (right == "CE") ? InstrumentType::CE
                               : (right == "PE") ? InstrumentType::PE
                                                 : InstrumentType::UNKNOWN;
        if (inst.instrument_type == InstrumentType::UNKNOWN)
            return {};
        inst.expiry = exp.display;
        inst.strike = strike;
        inst.symbol = underlying_ticker + exp.ddmmmyy + icici_strike_str(strike) + right;
        return inst;
    }

    return {};
}

QVector<Instrument> IciciInstrumentParser::parse(const QByteArray& csv_data) {
    QVector<Instrument> results;
    if (csv_data.isEmpty()) {
        LOG_WARN("IciciParser", "Empty CSV data received");
        return results;
    }

    QTextStream stream(csv_data);
    const QString header = stream.readLine();
    Q_UNUSED(header)

    int skipped = 0;
    results.reserve(35000); // ICICI ships ~33k rows (equity + F&O + commodity)
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.isEmpty())
            continue;

        const QStringList cols = line.split(',');
        if (cols.size() < 13) {
            ++skipped;
            continue;
        }

        Instrument inst = parse_row(cols);
        if (inst.symbol.isEmpty() || inst.exchange.isEmpty() || inst.brsymbol.isEmpty()) {
            ++skipped;
            continue;
        }
        results.append(inst);
    }

    LOG_INFO("IciciParser",
             QString("Parsed %1 instruments (skipped %2 malformed rows)").arg(results.size()).arg(skipped));
    return results;
}

} // namespace fincept::trading
