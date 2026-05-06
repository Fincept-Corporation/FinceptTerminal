#pragma once
// LeaderboardPanel — primary surface of the Alpha Arena tab.
//
// Reads the latest pnl snapshot per agent from AlphaArenaRepo::leaderboard()
// and renders 8 columns: RANK, MODEL, EQUITY, RETURN%, SHARPE, MAX DD, FEES,
// LEVERAGE. Refreshes on AlphaArenaEngine::tick_fired and on demand.
//
// Stable colours: the row's model swatch hashes from `model_id` so the same
// model gets the same colour across competitions. (User's previous prototype
// keyed colours by row-index, so swap-rank → colour-flip — disorienting.)
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §10 (Scoring) and §11
// (Frontend C++ surface).

#include <QString>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::alpha_arena {

class LeaderboardPanel : public QWidget {
    Q_OBJECT
  public:
    explicit LeaderboardPanel(QWidget* parent = nullptr);

    /// Set the competition the panel is bound to. Empty = idle.
    void set_competition(const QString& competition_id);

  public slots:
    /// Refresh the table from AlphaArenaRepo. Cheap to call.
    void refresh();

  signals:
    void agent_double_clicked(QString agent_id);

  private:
    QString stable_colour_for(const QString& model_id) const;

    QString competition_id_;
    QTableWidget* table_;
};

} // namespace fincept::screens::alpha_arena
