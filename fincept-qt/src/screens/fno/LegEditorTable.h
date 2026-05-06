#pragma once
// LegEditorTable — model + view pair for the F&O Builder leg list.
//
// Columns
// ───────
//   0  Active     checkbox — toggles leg.is_active
//   1  B/S        derived from sign(leg.lots)
//   2  Type       CE / PE
//   3  Strike
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
    void remove_row(int row);

  signals:
    void legs_changed();

  private:
    QVector<fincept::services::options::StrategyLeg> legs_;
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
