#pragma once
// BasketOrdersDialog — Zerodha-style order baskets, broker-agnostic.
// Create/edit named baskets of order legs, then execute the whole basket on one
// or several accounts at once. Execution goes through
// UnifiedTrading::place_basket_orders per selected account, so each account
// keeps its own paper/live routing and broker mapping.

#include "storage/repositories/OrderBasketRepository.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QVBoxLayout;

namespace fincept::screens::equity {

class BasketOrdersDialog : public QDialog {
    Q_OBJECT
  public:
    explicit BasketOrdersDialog(QWidget* parent = nullptr);

  private slots:
    void on_basket_selected(int row);
    void on_new_basket();
    void on_delete_basket();
    void on_save_basket();
    void on_add_leg();
    void on_execute();

  private:
    void reload_baskets(const QString& select_id = {});
    void refresh_legs_table();
    void build_accounts_box(QVBoxLayout* into);
    OrderBasket* current_basket();

    QVector<OrderBasket> baskets_;
    int current_row_ = -1;

    // Left: basket list
    QListWidget* basket_list_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;

    // Right: editor
    QLineEdit* name_edit_ = nullptr;
    QTableWidget* legs_table_ = nullptr;
    QLineEdit* leg_symbol_ = nullptr;
    QComboBox* leg_exchange_ = nullptr;
    QComboBox* leg_side_ = nullptr;
    QSpinBox* leg_qty_ = nullptr;
    QComboBox* leg_type_ = nullptr;
    QDoubleSpinBox* leg_price_ = nullptr;
    QComboBox* leg_product_ = nullptr;
    QPushButton* save_btn_ = nullptr;

    // Execute
    QVector<QCheckBox*> account_cbs_;
    QVector<QString> account_ids_;
    QPushButton* execute_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens::equity
