#include "screens/derivatives/DerivChainModel.h"

#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QFont>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens::deriv {

namespace fmt = fincept::ui::formatting;
namespace mk = services::options::market;

namespace {

QString dcm_num(double v, int d) {
    return std::isfinite(v) ? fmt::number(v, d) : QString();
}

QString dcm_iv(double v) {
    return std::isfinite(v) && v > 0 ? fmt::fraction_pct(v, 1, false) : QString();
}

QString dcm_int(qint64 v) {
    return v > 0 ? fmt::number(static_cast<double>(v), 0) : QString();
}

} // namespace

DerivChainModel::DerivChainModel(QObject* parent) : QAbstractTableModel(parent) {}

void DerivChainModel::set_chain(const ChainPtr& chain) {
    beginResetModel();
    chain_ = chain;
    if (!chain_ || expiry_ >= chain_->expiries.size())
        expiry_ = 0;
    rebuild();
    endResetModel();
}

void DerivChainModel::set_mode(Mode mode) {
    if (mode == mode_)
        return;
    beginResetModel();
    mode_ = mode;
    sort_col_ = -1;
    rebuild();
    endResetModel();
}

void DerivChainModel::set_expiry(int expiry) {
    if (expiry == expiry_)
        return;
    beginResetModel();
    expiry_ = expiry;
    rebuild();
    endResetModel();
}

void DerivChainModel::set_window(int n) {
    if (n == window_)
        return;
    beginResetModel();
    window_ = std::max(0, n);
    rebuild();
    endResetModel();
}

void DerivChainModel::rebuild() {
    strikes_.clear();
    call_idx_.clear();
    put_idx_.clear();
    flat_.clear();
    atm_row_ = -1;
    if (!chain_ || !chain_->ok())
        return;
    if (mode_ == Mode::Flat) {
        flat_.resize(chain_->quotes.size());
        std::iota(flat_.begin(), flat_.end(), 0);
        if (sort_col_ >= 0)
            sort_flat();
        return;
    }
    if (expiry_ < 0 || expiry_ >= chain_->expiries.size())
        return;
    const mk::FreeExpiry& x = chain_->expiries[expiry_];
    QVector<double> all;
    QVector<int> calls, puts;
    for (int i = x.begin; i < x.end; ++i) {
        const mk::FreeOptionQuote& q = chain_->quotes[i];
        if (all.isEmpty() || q.strike != all.last()) {
            all.append(q.strike);
            calls.append(-1);
            puts.append(-1);
        }
        (q.is_call ? calls.last() : puts.last()) = i;
    }
    int atm = 0;
    for (int i = 1; i < all.size(); ++i)
        if (std::fabs(all[i] - x.atm_strike) < std::fabs(all[atm] - x.atm_strike))
            atm = i;
    int lo = 0, hi = all.size();
    if (window_ > 0) {
        lo = std::max(0, atm - window_);
        hi = std::min(static_cast<int>(all.size()), atm + window_ + 1);
    }
    strikes_ = all.mid(lo, hi - lo);
    call_idx_ = calls.mid(lo, hi - lo);
    put_idx_ = puts.mid(lo, hi - lo);
    atm_row_ = atm - lo;
}

int DerivChainModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return mode_ == Mode::Flat ? flat_.size() : strikes_.size();
}

int DerivChainModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return mode_ == Mode::Flat ? int(kFlatCols) : int(kExpiryCols);
}

int DerivChainModel::quote_at(int row, int column) const {
    if (!chain_ || row < 0)
        return -1;
    if (mode_ == Mode::Flat)
        return row < flat_.size() ? flat_[row] : -1;
    if (row >= strikes_.size())
        return -1;
    return column <= kStrike ? call_idx_[row] : put_idx_[row];
}

double DerivChainModel::number(int row, int column) const {
    const double nan = std::nan("");
    if (!chain_)
        return nan;
    if (mode_ == Mode::ByExpiry) {
        if (row < 0 || row >= strikes_.size())
            return nan;
        if (column == kStrike)
            return strikes_[row];
        const int qi = column < kStrike ? call_idx_[row] : put_idx_[row];
        if (qi < 0)
            return nan;
        const mk::FreeOptionQuote& q = chain_->quotes[qi];
        switch (column) {
            case kCOi: case kPOi: return static_cast<double>(q.open_interest);
            case kCVol: case kPVol: return static_cast<double>(q.volume);
            case kCIv: case kPIv: return std::isfinite(q.iv) ? q.iv : q.feed_iv;
            case kCDelta: case kPDelta: return q.delta;
            case kCBid: case kPBid: return q.bid;
            case kCAsk: case kPAsk: return q.ask;
            case kCMid: case kPMid: return q.mid;
            default: return nan;
        }
    }
    if (row < 0 || row >= flat_.size())
        return nan;
    const mk::FreeOptionQuote& q = chain_->quotes[flat_[row]];
    switch (column) {
        case kFExpiry: return static_cast<double>(q.expiry);
        case kFDays: return chain_->expiries[q.expiry].days;
        case kFType: return q.is_call ? 1.0 : 0.0;
        case kFStrike: return q.strike;
        case kFBid: return q.bid;
        case kFAsk: return q.ask;
        case kFMid: return q.mid;
        case kFLast: return q.last;
        case kFIv: return std::isfinite(q.iv) ? q.iv : q.feed_iv;
        case kFDelta: return q.delta;
        case kFGamma: return q.gamma;
        case kFTheta: return q.theta;
        case kFVega: return q.vega;
        case kFVol: return static_cast<double>(q.volume);
        case kFOi: return static_cast<double>(q.open_interest);
        default: return nan;
    }
}

QVariant DerivChainModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !chain_)
        return {};
    const int row = index.row(), col = index.column();
    if (role == kNumberRole)
        return number(row, col);
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
    if (mode_ == Mode::ByExpiry) {
        if (role == Qt::BackgroundRole) {
            if (row == atm_row_)
                return QColor(fincept::ui::colors::ACCENT_BG());
            if (col == kStrike)
                return QColor(fincept::ui::colors::BG_RAISED());
            return {};
        }
        if (role == Qt::FontRole && (col == kStrike || row == atm_row_)) {
            // The table's own font (the screen sheet gives #derivTable base-4 px) in bold — a
            // default QFont would draw these cells in the application's size, not the table's.
            return fincept::ui::fonts::font(-4, QFont::Bold);
        }
        if (role == Qt::ToolTipRole) {
            const int qi = quote_at(row, col);
            if (qi < 0)
                return {};
            const mk::FreeOptionQuote& q = chain_->quotes[qi];
            return tr("%1 %2 · IV from the mid %3 · source IV %4 · last %5")
                .arg(q.is_call ? tr("Call") : tr("Put"), fmt::number(q.strike, 2), dcm_iv(q.iv), dcm_iv(q.feed_iv),
                     dcm_num(q.last, 2));
        }
        if (role != Qt::DisplayRole)
            return {};
        const double v = number(row, col);
        switch (col) {
            case kStrike: return fmt::number(v, 2);
            case kCOi: case kPOi: case kCVol: case kPVol: return dcm_int(static_cast<qint64>(std::isfinite(v) ? v : 0));
            case kCIv: case kPIv: return dcm_iv(v);
            case kCDelta: case kPDelta: return dcm_num(v, 3);
            default: return dcm_num(v, 2);
        }
    }
    if (role == Qt::ForegroundRole && col == kFType)
        return QColor(number(row, col) > 0.5 ? fincept::ui::colors::CYAN() : fincept::ui::colors::TEXT_SECONDARY());
    if (role != Qt::DisplayRole)
        return {};
    const mk::FreeOptionQuote& q = chain_->quotes[flat_[row]];
    const double v = number(row, col);
    switch (col) {
        case kFExpiry: return chain_->expiries[q.expiry].date.toString(Qt::ISODate);
        case kFDays: return QString::number(chain_->expiries[q.expiry].days);
        case kFType: return q.is_call ? tr("Call") : tr("Put");
        case kFStrike: return fmt::number(v, 2);
        case kFIv: return dcm_iv(v);
        case kFDelta: return dcm_num(v, 3);
        case kFGamma: return dcm_num(v, 4);
        case kFTheta: case kFVega: return dcm_num(v, 3);
        case kFVol: case kFOi: return dcm_int(static_cast<qint64>(std::isfinite(v) ? v : 0));
        default: return dcm_num(v, 2);
    }
}

QVariant DerivChainModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::ToolTipRole) {
        if (mode_ == Mode::ByExpiry && section == kStrike)
            return tr("Strike. The highlighted row is the strike nearest the forward (at the money).");
        return tr("IV and Greeks are computed locally from the bid/ask mid (Black-Scholes-Merton on the implied "
                  "forward). Theta per day, vega per vol point.");
    }
    if (role != Qt::DisplayRole)
        return {};
    if (mode_ == Mode::ByExpiry) {
        switch (section) {
            case kCOi: return tr("C OI");
            case kCVol: return tr("C VOL");
            case kCIv: return tr("C IV");
            case kCDelta: return tr("C DELTA");
            case kCBid: return tr("C BID");
            case kCAsk: return tr("C ASK");
            case kCMid: return tr("C MID");
            case kStrike: return tr("STRIKE");
            case kPMid: return tr("P MID");
            case kPBid: return tr("P BID");
            case kPAsk: return tr("P ASK");
            case kPDelta: return tr("P DELTA");
            case kPIv: return tr("P IV");
            case kPVol: return tr("P VOL");
            case kPOi: return tr("P OI");
            default: return {};
        }
    }
    switch (section) {
        case kFExpiry: return tr("EXPIRY");
        case kFDays: return tr("DAYS");
        case kFType: return tr("TYPE");
        case kFStrike: return tr("STRIKE");
        case kFBid: return tr("BID");
        case kFAsk: return tr("ASK");
        case kFMid: return tr("MID");
        case kFLast: return tr("LAST");
        case kFIv: return tr("IV");
        case kFDelta: return tr("DELTA");
        case kFGamma: return tr("GAMMA");
        case kFTheta: return tr("THETA");
        case kFVega: return tr("VEGA");
        case kFVol: return tr("VOLUME");
        case kFOi: return tr("OPEN INT.");
        default: return {};
    }
}

void DerivChainModel::sort(int column, Qt::SortOrder order) {
    if (mode_ != Mode::Flat || !chain_ || column < 0 || column >= kFlatCols)
        return;
    sort_col_ = column;
    sort_order_ = order;
    beginResetModel();
    sort_flat();
    endResetModel();
}

void DerivChainModel::sort_flat() {
    const int column = sort_col_;
    const Qt::SortOrder order = sort_order_;
    // Numeric keys; missing values last whatever the order. Ties keep the chain order.
    QVector<double> key(chain_->quotes.size(), std::nan(""));
    for (int qi = 0; qi < chain_->quotes.size(); ++qi) {
        const mk::FreeOptionQuote& q = chain_->quotes[qi];
        switch (column) {
            case kFExpiry: case kFDays: key[qi] = q.expiry; break;
            case kFType: key[qi] = q.is_call ? 1 : 0; break;
            case kFStrike: key[qi] = q.strike; break;
            case kFBid: key[qi] = q.bid; break;
            case kFAsk: key[qi] = q.ask; break;
            case kFMid: key[qi] = q.mid; break;
            case kFLast: key[qi] = q.last; break;
            case kFIv: key[qi] = std::isfinite(q.iv) ? q.iv : q.feed_iv; break;
            case kFDelta: key[qi] = q.delta; break;
            case kFGamma: key[qi] = q.gamma; break;
            case kFTheta: key[qi] = q.theta; break;
            case kFVega: key[qi] = q.vega; break;
            case kFVol: key[qi] = static_cast<double>(q.volume); break;
            case kFOi: key[qi] = static_cast<double>(q.open_interest); break;
            default: break;
        }
    }
    const bool asc = order == Qt::AscendingOrder;
    std::stable_sort(flat_.begin(), flat_.end(), [&key, asc](int a, int b) {
        const double ka = key[a], kb = key[b];
        const bool na = !std::isfinite(ka), nb = !std::isfinite(kb);
        if (na || nb)
            return !na && nb;
        return asc ? ka < kb : ka > kb;
    });
}

} // namespace fincept::screens::deriv
