#include "screens/fno/OptionChainModel.h"

#include <QColor>
#include <QLocale>

#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using fincept::trading::BrokerQuote;

namespace {

QString fmt_int_compact(qint64 v) {
    // Compact integer formatting: 1.2k, 3.4M, 5.6L (lakhs for IN context).
    // We avoid lakh/crore in pure code path — the chain values are integers
    // up to ~10^8 (e.g. NIFTY total OI in lakhs of contracts).
    if (v == 0)
        return QStringLiteral("--");
    const double a = std::abs(double(v));
    if (a >= 1e7)
        return QString::number(v / 1.0e7, 'f', 2) + "Cr";
    if (a >= 1e5)
        return QString::number(v / 1.0e5, 'f', 2) + "L";
    if (a >= 1e3)
        return QString::number(v / 1.0e3, 'f', 1) + "k";
    return QLocale(QLocale::English).toString(v);
}

QString fmt_price(double v) {
    if (v <= 0)
        return QStringLiteral("--");
    return QString::number(v, 'f', 2);
}

QString fmt_pct(double v) {
    // v is already a percentage (e.g. 14.2 means 14.2%)
    if (std::abs(v) < 1e-6)
        return QStringLiteral("--");
    return QString::number(v, 'f', 2) + "%";
}

QString fmt_signed_pct(double v) {
    if (std::abs(v) < 1e-6)
        return QStringLiteral("--");
    return (v > 0 ? "+" : "") + QString::number(v, 'f', 2) + "%";
}

}  // namespace

OptionChainModel::OptionChainModel(QObject* parent) : QAbstractTableModel(parent) {}

int OptionChainModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return chain_.rows.size();
}

int OptionChainModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant OptionChainModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= chain_.rows.size())
        return {};
    const OptionChainRow& r = chain_.rows.at(index.row());
    const int col = index.column();

    if (role == StrikeRole)
        return r.strike;
    if (role == IsAtmRole)
        return r.is_atm;
    if (role == IsCallItmRole)
        return chain_.spot > 0 && r.strike < chain_.spot;
    if (role == IsPutItmRole)
        return chain_.spot > 0 && r.strike > chain_.spot;
    if (role == CeTokenRole)
        return QVariant::fromValue(r.ce_token);
    if (role == PeTokenRole)
        return QVariant::fromValue(r.pe_token);
    if (role == CeOiBarRole)
        return max_ce_oi_ > 0 ? double(r.ce_quote.oi) / max_ce_oi_ : 0.0;
    if (role == PeOiBarRole)
        return max_pe_oi_ > 0 ? double(r.pe_quote.oi) / max_pe_oi_ : 0.0;

    if (role == Qt::TextAlignmentRole) {
        if (col == ColStrike)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        // Net change colouring for LTP cells uses BrokerQuote.change_pct
        if (col == ColCeLtp && r.ce_quote.change_pct != 0)
            return r.ce_quote.change_pct > 0 ? QColor(0x16, 0xa3, 0x4a) : QColor(0xdc, 0x26, 0x26);
        if (col == ColPeLtp && r.pe_quote.change_pct != 0)
            return r.pe_quote.change_pct > 0 ? QColor(0x16, 0xa3, 0x4a) : QColor(0xdc, 0x26, 0x26);
    }

    if (role != Qt::DisplayRole)
        return {};

    switch (col) {
        case ColCeOi:        return fmt_int_compact(r.ce_quote.oi);
        case ColCeOiChange:  return fmt_signed_pct(r.ce_quote.oi_change_pct);
        case ColCeVolume:    return fmt_int_compact(qint64(r.ce_quote.volume));
        case ColCeIv:        return fmt_pct(r.ce_iv * 100.0);
        case ColCeLtp:       return fmt_price(r.ce_quote.ltp);
        case ColStrike:      return QString::number(r.strike, 'f', r.strike < 100 ? 2 : 0);
        case ColPeLtp:       return fmt_price(r.pe_quote.ltp);
        case ColPeIv:        return fmt_pct(r.pe_iv * 100.0);
        case ColPeVolume:    return fmt_int_compact(qint64(r.pe_quote.volume));
        case ColPeOiChange:  return fmt_signed_pct(r.pe_quote.oi_change_pct);
        case ColPeOi:        return fmt_int_compact(r.pe_quote.oi);
    }
    return {};
}

QVariant OptionChainModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* kHeaders[ColCount] = {
        "OI",   "Chg OI", "Volume", "IV",   "LTP",
        "Strike",
        "LTP",  "IV",     "Volume", "Chg OI", "OI",
    };
    if (section < 0 || section >= ColCount)
        return {};
    return QString::fromLatin1(kHeaders[section]);
}

void OptionChainModel::set_chain(const OptionChain& chain) {
    beginResetModel();
    chain_ = chain;
    recompute_oi_bounds();
    endResetModel();
}

void OptionChainModel::update_leg_quote(qint64 token, const BrokerQuote& q) {
    for (int i = 0; i < chain_.rows.size(); ++i) {
        OptionChainRow& r = chain_.rows[i];
        if (r.ce_token == token) {
            r.ce_quote = q;
            // Recompute bounds only when the touched OI could shift the max.
            if (q.oi >= max_ce_oi_)
                recompute_oi_bounds();
            emit dataChanged(index(i, ColCeOi), index(i, ColCeLtp),
                             {Qt::DisplayRole, Qt::ForegroundRole, CeOiBarRole});
            return;
        }
        if (r.pe_token == token) {
            r.pe_quote = q;
            if (q.oi >= max_pe_oi_)
                recompute_oi_bounds();
            emit dataChanged(index(i, ColPeLtp), index(i, ColPeOi),
                             {Qt::DisplayRole, Qt::ForegroundRole, PeOiBarRole});
            return;
        }
    }
}

void OptionChainModel::recompute_oi_bounds() {
    max_ce_oi_ = 0;
    max_pe_oi_ = 0;
    for (const auto& r : chain_.rows) {
        if (double(r.ce_quote.oi) > max_ce_oi_)
            max_ce_oi_ = double(r.ce_quote.oi);
        if (double(r.pe_quote.oi) > max_pe_oi_)
            max_pe_oi_ = double(r.pe_quote.oi);
    }
}

} // namespace fincept::screens::fno
