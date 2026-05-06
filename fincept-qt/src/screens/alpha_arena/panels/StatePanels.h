#pragma once
// StatePanels — four light-weight read-only panels grouped in one file.
//
//   PositionsPanel : open positions per agent (coin, qty, entry, mark, uPnL,
//                    leverage, liq distance).
//   HitlPanel      : pending HITL approvals; Accept / Reject buttons emit a
//                    signal the screen forwards to AlphaArenaEngine::resume_after_hitl.
//   RiskPanel      : per-agent kill-switch state (parse fails, risk rejects,
//                    drawdown) + average leverage. Reads aa_agents +
//                    aa_pnl_snapshots.
//   AuditPanel     : append-only event stream from aa_events; tails the latest
//                    seq seen, polls AlphaArenaRepo::events_since.
//
// Each panel owns its own refresh() and is bound via set_competition().
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §11 (Frontend C++ surface).

#include <QLabel>
#include <QListWidget>
#include <QString>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::alpha_arena {

class PositionsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PositionsPanel(QWidget* parent = nullptr);
    void set_competition(const QString& competition_id);
  public slots:
    void refresh();
  private:
    QString competition_id_;
    QTableWidget* table_;
};

class HitlPanel : public QWidget {
    Q_OBJECT
  public:
    explicit HitlPanel(QWidget* parent = nullptr);
    void set_competition(const QString& competition_id);
  public slots:
    void refresh();
    /// Surface a freshly-requested HITL approval (engine signal).
    void on_hitl_requested(QString approval_id, QString agent_id, QString summary);
  signals:
    void approval_resolved(QString approval_id, bool approved);
  private:
    void rebuild_buttons_for(const QString& approval_id, const QString& agent_id,
                             const QString& summary);
    QString competition_id_;
    QListWidget* list_;
};

class RiskPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RiskPanel(QWidget* parent = nullptr);
    void set_competition(const QString& competition_id);
  public slots:
    void refresh();
  private:
    QString competition_id_;
    QTableWidget* table_;
    QLabel* telemetry_label_ = nullptr;  // p50/p99 + rates summary
};

class AuditPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AuditPanel(QWidget* parent = nullptr);
    void set_competition(const QString& competition_id);
  public slots:
    void refresh();
  private:
    QString competition_id_;
    QListWidget* list_;
    qint64 last_seq_ = 0;
};

} // namespace fincept::screens::alpha_arena
