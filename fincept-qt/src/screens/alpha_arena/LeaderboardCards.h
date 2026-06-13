#pragma once
// LeaderboardCards — dense terminal-style agent board (QTableWidget rows, one
// per agent). Click a row to select (filters the panel grid); per-row action
// icons: halt/resume toggle and kill (close positions + halt).
#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

class QTableWidget;

namespace fincept::screens::alpha_arena {

struct AgentCardData {
    QString agent_id, name, status;   // status: active|halted_user|halted_circuit
    QColor color;
    double equity = 0, pnl_pct = 0, upnl = 0;
    int open_positions = 0;
    qint64 tokens = 0;
};

class LeaderboardCards : public QWidget {
    Q_OBJECT
  public:
    explicit LeaderboardCards(QWidget* parent = nullptr);
    void set_data(QVector<AgentCardData> cards);   // caller sorts by equity desc
    QString selected_agent() const { return selected_; }
  signals:
    void agent_selected(QString agent_id);         // empty = deselected
    void halt_requested(QString agent_id);
    void resume_requested(QString agent_id);
    void kill_requested(QString agent_id);         // close positions & halt one model
  private:
    void rebuild();
    QVector<AgentCardData> cards_;
    QString selected_;
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens::alpha_arena
