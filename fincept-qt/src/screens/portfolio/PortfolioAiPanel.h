// src/screens/portfolio/PortfolioAiPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QWidget>

namespace fincept::screens {

/// Floating AI analysis panel overlaying the portfolio screen.
/// Supports 4 analysis types with cached results per portfolio.
class PortfolioAiPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioAiPanel(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void show_panel();

  signals:
    void close_requested();

  private:
    void build_ui();
    void set_analysis_type(const QString& type);
    void run_analysis(bool force = false);
    QString build_portfolio_prompt() const;

    // Header
    QPushButton* close_btn_ = nullptr;
    QLabel* status_lbl_ = nullptr;

    // Type selector buttons
    QPushButton* full_btn_ = nullptr;
    QPushButton* risk_btn_ = nullptr;
    QPushButton* rebal_btn_ = nullptr;
    QPushButton* opport_btn_ = nullptr;

    // Run button
    QPushButton* run_btn_ = nullptr;

    // Content
    QTextBrowser* content_ = nullptr;

    // State
    portfolio::PortfolioSummary summary_;
    QString current_type_ = "full";
    bool analyzing_ = false;
    QHash<QString, QString> result_cache_; // type -> result
    QString last_portfolio_id_;
};

} // namespace fincept::screens
