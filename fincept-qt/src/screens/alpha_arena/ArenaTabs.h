#pragma once
// ArenaPanelGrid — always-visible detail panel grid under the dashboard
// (formerly a tab stack). Pure viewer: every refresh re-reads ArenaStore /
// ArenaEngine. Layout: [MODEL CHAT] | [POSITIONS / TRADES] | [RISK / AUDIT],
// plus a high-visibility HITL approval banner above the grid (live mode).
#include <QWidget>

class QHBoxLayout; class QLabel; class QListWidget; class QTableWidget;
class QTextBrowser;

namespace fincept::screens::alpha_arena {

class ArenaPanelGrid : public QWidget {
    Q_OBJECT
  public:
    explicit ArenaPanelGrid(QWidget* parent = nullptr);
    void set_competition(const QString& id, bool live_mode);
    void set_agent_filter(const QString& agent_id);            // empty = all agents
    void refresh();                                            // all panels (all visible)
    void refresh_positions_only();                             // cheap; every marks tick
    void add_hitl(const QString& approval_id, const QString& agent_label, const QString& summary);
  signals:
    void hitl_resolved(QString approval_id, bool approved);
  private:
    void refresh_chat(); void refresh_positions(); void refresh_trades();
    void refresh_risk(); void refresh_audit();
    void update_hitl_banner_visibility();

    QString competition_id_, agent_filter_;
    QTextBrowser* chat_ = nullptr;
    QTableWidget* positions_ = nullptr;
    QTableWidget* trades_ = nullptr;
    QTableWidget* risk_ = nullptr;
    QListWidget* audit_ = nullptr;
    QLabel* chat_hdr_ = nullptr;
    QLabel* positions_hdr_ = nullptr;
    QLabel* trades_hdr_ = nullptr;
    QLabel* risk_hdr_ = nullptr;
    QLabel* audit_hdr_ = nullptr;
    QWidget* hitl_banner_ = nullptr;          // hidden when no pending approvals
    QHBoxLayout* hitl_row_ = nullptr;
};

} // namespace fincept::screens::alpha_arena
