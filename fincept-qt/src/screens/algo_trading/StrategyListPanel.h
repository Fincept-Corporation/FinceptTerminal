// src/screens/algo_trading/StrategyListPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Strategy list — compact table with pagination, loads from registry_index.json in <50ms.
class StrategyListPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategyListPanel(QWidget* parent = nullptr);

  private slots:
    void on_strategies_loaded(QVector<fincept::services::algo::AlgoStrategy> strategies);
    void on_sort_changed(int index);
    void on_error(const QString& context, const QString& msg);
    void on_filter_changed(const QString& text);
    void go_to_page(int page);

  private:
    void build_ui();
    void connect_service();
    void render_page();
    void update_pagination_controls();

    static constexpr int kPageSize = 50;

    QLineEdit*    search_edit_   = nullptr;
    QLabel*       count_label_   = nullptr;
    QComboBox*    sort_combo_    = nullptr;
    QComboBox*    cat_combo_     = nullptr;
    QTableWidget* table_         = nullptr;
    QLabel*       page_label_    = nullptr;
    QPushButton*  prev_btn_      = nullptr;
    QPushButton*  next_btn_      = nullptr;

    QVector<fincept::services::algo::AlgoStrategy> strategies_;  // full list
    QVector<fincept::services::algo::AlgoStrategy> filtered_;    // after search/sort
    int current_page_ = 0; // 0-based
};

} // namespace fincept::screens
