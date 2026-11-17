// src/screens/portfolio/PortfolioInsightsPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/agents/AgentTypes.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QVBoxLayout;

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class KpiStrip;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens {

/// Unified right-hand dock that exposes both AI analysis and Agent Runner in
/// one coherent surface. Replaces the old PortfolioAiPanel / PortfolioAgentPanel
/// floating overlays, which clashed with the charts and each other.
///
/// The panel is positioned by PortfolioScreen via open_tab(); it fills the
/// available vertical space below the command bar and anchors to the right
/// edge. A scrim beneath it dims the rest of the screen so focus is obvious.
///
/// Both result areas are ui::AutoLoadPanes with Policy::Manual (an LLM call may
/// cost credits and sends the holdings to the configured provider — class C):
/// nothing runs by itself. Each analysis type (FULL / RISK / REBALANCE / OPPS)
/// and each agent keeps its last result per portfolio with its "as of" time
/// (SettingsRepository, category "portfolio_insights"); with none, a designed
/// preview says what the run returns, what it sends and offers RUN. A missing LLM
/// shows the "needs setup" page with the Settings > LLM Config link.
class PortfolioInsightsPanel : public QWidget {
    Q_OBJECT
  public:
    enum class Tab { AI, Agent };

    explicit PortfolioInsightsPanel(QWidget* parent = nullptr);
    ~PortfolioInsightsPanel() override;

    void set_summary(const portfolio::PortfolioSummary& summary);
    void open_tab(Tab tab);
    void reload_agents();

  signals:
    void close_requested();

  protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    /// A result kept for one view (analysis type or agent) of one portfolio.
    struct Stored {
        QString markdown;
        QDateTime as_of;  ///< UTC
        qint64 ms = -1;   ///< execution time, -1 unknown
        QJsonObject snapshot; ///< {nav, pnl, pnl_pct, positions, ccy} at run time (empty for old results)
    };

    void build_ui();
    QWidget* build_ai_page();
    QWidget* build_agent_page();
    QWidget* build_result_view(QWidget* parent, ui::KpiStrip** kpis, QTextBrowser** text);
    void switch_tab(Tab tab);
    void set_ai_type(const QString& type);
    /// Points the agent pane at the agent selected in the combo (preview + view key).
    void select_agent_view();
    void configure_ai_preview(const QString& type);
    void configure_agent_preview();
    void start_ai(quint64 request_id);
    void start_agent(quint64 request_id);
    void render_payload(ui::KpiStrip* kpis, QTextBrowser* text, const QJsonValue& payload);
    void render_markdown(QTextBrowser* target, const QString& markdown);
    QJsonObject snapshot() const;
    QString build_portfolio_context() const;
    QString ai_view_key(const QString& type) const;
    QString agent_view_key(const QString& agent_id) const;
    void update_run_buttons();
    void show_agent_page(int page);
    void retranslateUi();
    /// Re-renders the HTML result views and the font-sized close button (appearance_settled).
    void refresh_theme();

    // Durable result persistence (survives tab switches, panel recreation, and
    // app restarts). Keyed by portfolio id so each portfolio keeps its own
    // AI-analysis and agent results.
    // `portfolio_id` is the book the run was STARTED for — the user may have switched since.
    void persist_result(const QString& kind, const QString& portfolio_id, const QString& id, const Stored& s);
    void load_persisted_results(const QString& portfolio_id); // repopulates ai_store_ / agent_store_
    static QJsonObject stored_payload(const Stored& s);

    // Layout
    QLabel* header_title_ = nullptr;
    QPushButton* header_close_btn_ = nullptr;
    QPushButton* tab_ai_btn_ = nullptr;
    QPushButton* tab_agent_btn_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLabel* header_status_ = nullptr;
    QLabel* ai_type_label_ = nullptr;
    QLabel* agent_select_label_ = nullptr;

    // AI page
    QPushButton* ai_full_ = nullptr;
    QPushButton* ai_risk_ = nullptr;
    QPushButton* ai_rebal_ = nullptr;
    QPushButton* ai_opport_ = nullptr;
    QPushButton* ai_run_ = nullptr;
    QLabel* ai_meta_ = nullptr; ///< "as of 14:02" (kept by the pane)
    ui::AutoLoadPane* ai_pane_ = nullptr;
    ui::KpiStrip* ai_kpis_ = nullptr;
    QTextBrowser* ai_content_ = nullptr;

    // Agent page
    QStackedWidget* agent_stack_ = nullptr;      ///< 0 = discovering, 1 = no agents / failed, 2 = ready
    ui::LoadingSkeleton* agent_discovering_ = nullptr;
    ui::EmptyState* agent_none_ = nullptr;
    QComboBox* agent_cb_ = nullptr;
    QLabel* agent_desc_ = nullptr;
    QPushButton* agent_run_ = nullptr;
    QLabel* agent_meta_ = nullptr;
    ui::AutoLoadPane* agent_pane_ = nullptr;
    ui::KpiStrip* agent_kpis_ = nullptr;
    QTextBrowser* agent_content_ = nullptr;
    QTimer* agent_partial_timer_ = nullptr; ///< throttles streamed text into the pane

    // State
    Tab current_tab_ = Tab::AI;
    portfolio::PortfolioSummary summary_;
    QString last_portfolio_id_;
    QString ai_type_ = "full";
    bool discovering_ = false;          ///< agent discovery in flight
    bool discovery_done_ = false;       ///< a discovery answered (agents, none, or an error) — not repeated by itself
    bool discovery_failed_ = false;
    QString discovery_error_;
    QVector<services::AgentInfo> discovered_; ///< last list agents_discovered delivered (when not cached)
    bool in_reload_ = false;            ///< reload_agents() re-entrancy guard (discovery may answer synchronously)
    // AI run in flight (one at a time)
    quint64 ai_pending_pane_id_ = 0;    ///< pane request id
    QString ai_pending_type_;           ///< analysis_type in flight
    QString ai_pending_req_id_;         ///< run_portfolio_analysis request id in flight
    QString ai_pending_portfolio_id_;   ///< portfolio that AI run was started for
    QJsonObject ai_pending_snapshot_;
    // Agent run in flight (one at a time)
    quint64 agent_pending_pane_id_ = 0;
    QString agent_pending_req_id_;      ///< run_agent_streaming request id in flight
    QString agent_pending_id_;          ///< agent_id matching that request
    QString agent_pending_portfolio_id_;
    QJsonObject agent_pending_snapshot_;
    QString agent_streaming_text_;      ///< accumulated tokens for live rendering
    QHash<QString, Stored> ai_store_;    ///< view key ("<pid>/<type>") -> last result
    QHash<QString, Stored> agent_store_; ///< view key ("<pid>/<agent_id>") -> last result
};

} // namespace fincept::screens
