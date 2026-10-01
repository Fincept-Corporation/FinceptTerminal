#pragma once
// LegEditorTable — model + view pair for the F&O Builder leg list.
//
// Columns
// ───────
//   0  Active     checkbox — toggles leg.is_active
//   1  B/S        derived from sign(leg.lots)
//   2  Type       CE / PE — editable (double-click, type CE or PE)
//   3  Strike     editable — re-resolves the contract from the live chain
//   4  Lots       editable signed int (negative = sell)
//   5  Entry      editable double — entry premium per share
//   6  IV         read-only % (from leg.iv_at_entry × 100)
//   7  ✕          delete row — handled by the view's mousePressEvent
//
// Editing any cell that mutates the leg emits `legs_changed()`. The
// orchestrator (BuilderSubTab) reacts by recomputing analytics + payoff.

#include "services/options/OptionChainTypes.h"

#include <QAbstractTableModel>
#include <QTableView>
#include <QVector>

namespace fincept::screens::fno {

class LegEditorModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Column : int {
        ColActive = 0,
        ColBuySell,
        ColType,
        ColStrike,
        ColLots,
        ColEntry,
        ColIv,
        ColLtp,
        ColDelta,
        ColPnl,
        ColDelete,
        ColCount,
    };

    explicit LegEditorModel(QObject* parent = nullptr);

    // ── QAbstractTableModel ─────────────────────────────────────────────────
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

    // ── Mutation API ────────────────────────────────────────────────────────
    void set_legs(const QVector<fincept::services::options::StrategyLeg>& legs);
    const QVector<fincept::services::options::StrategyLeg>& legs() const { return legs_; }
    /// Append a single leg (used by the chain "leg_clicked" hookup).
    void append_leg(const fincept::services::options::StrategyLeg& leg);
    /// Append a +1-lot call at the chain's ATM strike, fully resolved (contract,
    /// lot size, premium, IV) from the live chain — the "+ ADD LEG" action. Without a
    /// chain it appends an unresolved scratch row the user must complete.
    void append_blank_leg();
    void remove_row(int row);
    /// Push latest chain so LTP/Delta/PnL columns can resolve live values.
    void set_chain(const fincept::services::options::OptionChain& chain);

  signals:
    void legs_changed();

  private:
    /// Re-resolve contract identity + live premium/IV/lot size for `leg`'s current
    /// (strike, type) from `chain_`. A strike the chain doesn't carry leaves the leg
    /// as an unresolved what-if row (no symbol -> Trade All refuses it).
    void resolve_leg_from_chain(fincept::services::options::StrategyLeg& leg) const;

    double resolve_ltp(const fincept::services::options::StrategyLeg& leg) const;
    double resolve_delta(const fincept::services::options::StrategyLeg& leg) const;
    double resolve_pnl(const fincept::services::options::StrategyLeg& leg) const;

    QVector<fincept::services::options::StrategyLeg> legs_;
    fincept::services::options::OptionChain chain_;
};

class LegEditorTable : public QTableView {
    Q_OBJECT
  public:
    explicit LegEditorTable(QWidget* parent = nullptr);

    LegEditorModel* leg_model() const { return model_; }

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private:
    LegEditorModel* model_ = nullptr;
};

} // namespace fincept::screens::fno
