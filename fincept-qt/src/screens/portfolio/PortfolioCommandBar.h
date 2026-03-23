// src/screens/portfolio/PortfolioCommandBar.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class PortfolioCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioCommandBar(QWidget* parent = nullptr);

    void set_portfolios(const QVector<portfolio::Portfolio>& portfolios);
    void set_selected_portfolio(const portfolio::Portfolio& p);
    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_refreshing(bool refreshing);
    void set_detail_view(std::optional<portfolio::DetailView> view);
    void set_has_selection(bool has_selection);
    void set_has_portfolios(bool has_portfolios);

  signals:
    void portfolio_selected(QString id);
    void create_requested();
    void delete_requested(QString id);
    void buy_requested();
    void sell_requested();
    void refresh_requested();
    void export_csv_requested();
    void export_json_requested();
    void import_requested();
    void refresh_interval_changed(int ms);
    void ffn_toggled();
    void detail_view_selected(portfolio::DetailView view);
    void ai_analyze_requested();
    void agent_run_requested();

  private:
    void build_ui();
    void build_portfolio_selector(QHBoxLayout* layout);
    void build_action_buttons(QHBoxLayout* layout);
    void build_detail_buttons(QHBoxLayout* layout);
    void toggle_dropdown();
    void update_selector_label();

    // Portfolio selector
    QPushButton* selector_btn_   = nullptr;
    QWidget*     dropdown_       = nullptr;
    QLineEdit*   search_edit_    = nullptr;
    QListWidget* portfolio_list_ = nullptr;

    // Stats in the bar
    QLabel* nav_label_     = nullptr;
    QLabel* pnl_label_     = nullptr;
    QLabel* day_label_     = nullptr;
    QLabel* pos_label_     = nullptr;

    // Action buttons
    QPushButton* buy_btn_     = nullptr;
    QPushButton* sell_btn_    = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QComboBox*   interval_cb_ = nullptr;
    QPushButton* ffn_btn_     = nullptr;

    // Detail view buttons
    QVector<QPushButton*> detail_btns_;

    // Containers for show/hide
    QWidget* actions_container_ = nullptr; // BUY/SELL/REFRESH/interval/exports/FFN
    QWidget* details_container_ = nullptr; // 9 detail view buttons + AI/AGENT
    QWidget* stats_container_   = nullptr; // inline NAV/PNL/DAY/POS

    // State
    QVector<portfolio::Portfolio> portfolios_;
    QString selected_id_;
    std::optional<portfolio::DetailView> active_detail_;
    bool dropdown_visible_ = false;
};

} // namespace fincept::screens
