// src/screens/portfolio/PortfolioInsightsPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHash>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QVBoxLayout;

namespace fincept::screens {

/// Unified right-hand dock that exposes both AI analysis and Agent Runner in
/// one coherent surface. Replaces the old PortfolioAiPanel / PortfolioAgentPanel
/// floating overlays, which clashed with the charts and each other.
///
/// The panel is positioned by PortfolioScreen via open_tab(); it fills the
/// available vertical space below the command bar and anchors to the right
/// edge. A scrim beneath it dims the rest of the screen so focus is obvious.
class PortfolioInsightsPanel : public QWidget {
    Q_OBJECT
  public:
    enum class Tab { AI, Agent };

    explicit PortfolioInsightsPanel(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void open_tab(Tab tab);
    void reload_agents();

  signals:
    void close_requested();

  protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();
    QWidget* build_ai_page();
    QWidget* build_agent_page();
    void switch_tab(Tab tab);
    void set_ai_type(const QString& type);
    void run_ai(bool force);
    void run_agent(bool force);
    void render_result(QTextBrowser* target, const QString& markdown);
    void render_error(QTextBrowser* target, const QString& message);
    void render_empty(QTextBrowser* target, const QString& hint);
    QString build_portfolio_context() const;

    // Layout
    QPushButton* tab_ai_btn_ = nullptr;
    QPushButton* tab_agent_btn_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLabel* header_status_ = nullptr;

    // AI page
    QPushButton* ai_full_ = nullptr;
    QPushButton* ai_risk_ = nullptr;
    QPushButton* ai_rebal_ = nullptr;
    QPushButton* ai_opport_ = nullptr;
    QPushButton* ai_run_ = nullptr;
    QLabel* ai_meta_ = nullptr;
    QTextBrowser* ai_content_ = nullptr;

    // Agent page
    QComboBox* agent_cb_ = nullptr;
    QLabel* agent_desc_ = nullptr;
    QPushButton* agent_run_ = nullptr;
    QLabel* agent_meta_ = nullptr;
    QTextBrowser* agent_content_ = nullptr;

    // State
    Tab current_tab_ = Tab::AI;
    portfolio::PortfolioSummary summary_;
    QString last_portfolio_id_;
    QString ai_type_ = "full";
    bool ai_busy_ = false;
    bool agent_busy_ = false;
    QString ai_pending_type_;       // analysis_type in flight (for result routing)
    QString agent_pending_req_id_;  // run_agent_streaming request id in flight
    QString agent_pending_id_;      // agent_id matching that request
    QString agent_streaming_text_;  // accumulated tokens for live rendering
    QHash<QString, QString> ai_cache_;    // type -> markdown
    QHash<QString, QString> agent_cache_; // agent_id -> markdown
};

} // namespace fincept::screens
