#pragma once
// OptionChainModel — table model for the F&O Chain sub-tab.
//
// One row per strike; columns mirror Sensibull's chain layout:
//
//   CE side                       │ Strike │ PE side
//   OI · ChgOI · Vol · IV · LTP   │   K    │ LTP · IV · Vol · ChgOI · OI
//
// The model owns a snapshot of the OptionChain payload from DataHub. Per-
// row updates (Phase 3 WS path) mutate the row in place and emit
// dataChanged for that single QModelIndex pair so the table doesn't
// repaint the world on every tick.

#include "services/options/OptionChainTypes.h"

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

namespace fincept::screens::fno {

class OptionChainModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int {
        // CE side
        ColCeOi = 0,
        ColCeOiChange,
        ColCeVolume,
        ColCeIv,
        ColCeLtp,
        // Strike pivot
        ColStrike,
        // PE side
        ColPeLtp,
        ColPeIv,
        ColPeVolume,
        ColPeOiChange,
        ColPeOi,
        ColCount
    };

    enum Role : int {
        // Ratio of this row's CE OI to max CE OI in the chain (for the OI bar
        // delegate). 0..1. Same for PE side.
        CeOiBarRole = Qt::UserRole + 1,
        PeOiBarRole,
        IsAtmRole,         // true on the ATM row
        IsCallItmRole,     // strike < spot — CE is in-the-money
        IsPutItmRole,      // strike > spot — PE is in-the-money
        StrikeRole,        // raw strike value
        CeTokenRole,
        PeTokenRole,
    };

    explicit OptionChainModel(QObject* parent = nullptr);

    // ── QAbstractTableModel ─────────────────────────────────────────────────
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

    // ── Snapshot replacement ────────────────────────────────────────────────

    /// Replace the underlying chain wholesale and reset the model. Triggers
    /// a full table rebuild — used when the user picks a new underlying or
    /// expiry, or for the very first publish.
    void set_chain(const fincept::services::options::OptionChain& chain);

    /// Targeted update for a single token (Phase 3 WS path). Walks the chain
    /// once to find the row carrying this token and emits dataChanged for
    /// just the OI/LTP/IV columns on that side. No-op if token isn't present.
    void update_leg_quote(qint64 token, const fincept::trading::BrokerQuote& q);

    const fincept::services::options::OptionChain& chain() const { return chain_; }

  private:
    fincept::services::options::OptionChain chain_;
    double max_ce_oi_ = 0;
    double max_pe_oi_ = 0;
    void recompute_oi_bounds();
};

} // namespace fincept::screens::fno
