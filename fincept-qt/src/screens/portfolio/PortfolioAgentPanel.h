// src/screens/portfolio/PortfolioAgentPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QWidget>

namespace fincept::screens {

/// Floating agent runner panel — selects a saved agent config and runs it on portfolio data.
class PortfolioAgentPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioAgentPanel(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void show_panel();
    void reload_agents();

  signals:
    void close_requested();

  private:
    void build_ui();
    void run_agent(bool force = false);

    QPushButton*  close_btn_    = nullptr;
    QLabel*       status_lbl_   = nullptr;
    QComboBox*    agent_cb_     = nullptr;
    QPushButton*  run_btn_      = nullptr;
    QTextBrowser* content_      = nullptr;

    portfolio::PortfolioSummary summary_;
    bool running_ = false;
    QHash<QString, QString> result_cache_; // agent_id -> result
    QString last_portfolio_id_;
};

} // namespace fincept::screens
