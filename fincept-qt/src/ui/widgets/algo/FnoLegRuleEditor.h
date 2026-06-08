#pragma once

#include "algo_engine/fno/FnoAlgoTypes.h"

#include <QVector>
#include <QWidget>

class QTableWidget;

namespace fincept::ui::algo {

// Rule-based F&O leg editor. Source of truth for the strategy's AlgoFnoLeg rules
// (kind/side/lots + strike rule). No live-chain dependency — strikes are rules,
// resolved later at entry/preview time.
class FnoLegRuleEditor : public QWidget {
    Q_OBJECT
  public:
    explicit FnoLegRuleEditor(QWidget* parent = nullptr);

    QVector<fincept::algo::fno::AlgoFnoLeg> legs() const;
    void set_legs(const QVector<fincept::algo::fno::AlgoFnoLeg>& legs);
    void clear_legs();

  signals:
    void legs_changed();

  private:
    void add_row(const fincept::algo::fno::AlgoFnoLeg& leg);
    fincept::algo::fno::AlgoFnoLeg row_to_leg(int row) const;

    QTableWidget* table_ = nullptr;
};

} // namespace fincept::ui::algo
