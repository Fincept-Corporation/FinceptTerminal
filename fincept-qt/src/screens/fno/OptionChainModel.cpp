#include "screens/fno/OptionChainModel.h"

#include "screens/fno/FnoFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QColor>

#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using fincept::trading::BrokerQuote;

namespace {

/// OI / volume (contract counts, not money), abbreviated per the user's compact
/// scheme — lakh / crore on an NSE chain while it is on Automatic.
QString ocm_fmt_count(qint64 v, const QString& ccy) {
    if (v == 0)
        return QStringLiteral("--");
    return fno_fmt_oi(v, ccy);
}

/// A premium in the chain's currency; the column header names the unit.
QString ocm_fmt_price(double v, const QString& ccy) {
    if (v <= 0)
        return QStringLiteral("--");
    return fno_price(v, ccy);
}

/// `v` is already a percentage (14.2 means 14.2%).
QString ocm_fmt_pct(double v) {
    if (std::abs(v) < 1e-6)
        return QStringLiteral("--");
    return fincept::ui::formatting::pct(v, 2, false);
}

QString ocm_fmt_signed_pct(double v) {
    if (std::abs(v) < 1e-6)
        return QStringLiteral("--");
    return fincept::ui::formatting::pct(v, 2, true);
}

} // namespace

OptionChainModel::OptionChainModel(QObject* parent) : QAbstractTableModel(parent) {
    // The model formats in data(): a currency / number-format change repaints the
    // cells and headers from the chain it holds (no reset, no re-fetch).
    fincept::ui::formatting::refresh_model_on_format_change(this);
}

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
        using namespace fincept::ui;
        if (col == ColCeLtp && r.ce_quote.change_pct != 0)
            return QColor(r.ce_quote.change_pct > 0 ? colors::POSITIVE() : colors::NEGATIVE());
        if (col == ColPeLtp && r.pe_quote.change_pct != 0)
            return QColor(r.pe_quote.change_pct > 0 ? colors::POSITIVE() : colors::NEGATIVE());
    }

    // The LTP tint is the day's direction: say it in words too, never by colour alone.
    if (role == Qt::ToolTipRole) {
        if (col == ColCeLtp && r.ce_quote.change_pct != 0)
            return tr("Day change %1").arg(ocm_fmt_signed_pct(r.ce_quote.change_pct));
        if (col == ColPeLtp && r.pe_quote.change_pct != 0)
            return tr("Day change %1").arg(ocm_fmt_signed_pct(r.pe_quote.change_pct));
        return {};
    }

    if (role != Qt::DisplayRole)
        return {};

    const QString ccy = fno_chain_ccy(chain_.broker_id);
    switch (col) {
        case ColCeOi:
            return ocm_fmt_count(r.ce_quote.oi, ccy);
        case ColCeOiChange:
            return ocm_fmt_signed_pct(r.ce_quote.oi_change_pct);
        case ColCeVolume:
            return ocm_fmt_count(qint64(r.ce_quote.volume), ccy);
        case ColCeIv:
            return ocm_fmt_pct(r.ce_iv * 100.0);
        case ColCeLtp:
            return ocm_fmt_price(r.ce_quote.ltp, ccy);
        case ColStrike:
            return fincept::services::options::format_strike(r.strike);
        case ColPeLtp:
            return ocm_fmt_price(r.pe_quote.ltp, ccy);
        case ColPeIv:
            return ocm_fmt_pct(r.pe_iv * 100.0);
        case ColPeVolume:
            return ocm_fmt_count(qint64(r.pe_quote.volume), ccy);
        case ColPeOiChange:
            return ocm_fmt_signed_pct(r.pe_quote.oi_change_pct);
        case ColPeOi:
            return ocm_fmt_count(r.pe_quote.oi, ccy);
    }
    return {};
}

QVariant OptionChainModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    // tr() per-call so the live header row reflects the current language.
    // The owning QHeaderView re-polls headerData on QEvent::LanguageChange.
    // The premium columns name the chain's currency (rupees on NSE, dollars on
    // Databento's US options) as the user's settings print it.
    const QString ltp = fincept::ui::formatting::header(tr("LTP"), fno_chain_ccy(chain_.broker_id));
    const QString headers[ColCount] = {
        tr("OI"), tr("Chg OI"), tr("Volume"), tr("IV"),     ltp, tr("Strike"),
        ltp,      tr("IV"),     tr("Volume"), tr("Chg OI"), tr("OI"),
    };
    if (section < 0 || section >= ColCount)
        return {};
    return headers[section];
}

void OptionChainModel::set_chain(const OptionChain& chain) {
    // When the strike/token structure is unchanged (same expiry republished —
    // e.g. a WS-driven live re-publish or a periodic reconcile), merge in place
    // and repaint instead of resetting the model. A full reset would drop the
    // user's scroll position and selection on every refresh; this keeps them.
    //
    // SAFETY: the strike ladder is compared too, not just the tokens. Some
    // providers (Databento, Fyers' chain-v3) publish rows with token == 0 for
    // every strike — comparing tokens alone would then call a *shifted* ladder
    // "the same structure", leaving the user's selected row pointing at a
    // different strike after a refresh. Row identity for this table is
    // (strike, ce_token, pe_token); any change to any of them forces a reset.
    bool same_structure = chain.rows.size() == chain_.rows.size() && !chain_.rows.isEmpty();
    if (same_structure) {
        for (int i = 0; i < chain.rows.size(); ++i) {
            if (chain.rows[i].ce_token != chain_.rows[i].ce_token ||
                chain.rows[i].pe_token != chain_.rows[i].pe_token ||
                std::abs(chain.rows[i].strike - chain_.rows[i].strike) > 1e-6) {
                same_structure = false;
                break;
            }
        }
    }
    if (same_structure) {
        const bool ccy_changed = fno_chain_ccy(chain.broker_id) != fno_chain_ccy(chain_.broker_id);
        chain_ = chain;
        recompute_oi_bounds();
        emit dataChanged(index(0, 0), index(chain_.rows.size() - 1, ColCount - 1));
        if (ccy_changed)
            emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1); // the LTP columns' unit
        return;
    }
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
            emit dataChanged(index(i, ColCeOi), index(i, ColCeLtp), {Qt::DisplayRole, Qt::ForegroundRole, CeOiBarRole});
            return;
        }
        if (r.pe_token == token) {
            r.pe_quote = q;
            if (q.oi >= max_pe_oi_)
                recompute_oi_bounds();
            emit dataChanged(index(i, ColPeLtp), index(i, ColPeOi), {Qt::DisplayRole, Qt::ForegroundRole, PeOiBarRole});
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
