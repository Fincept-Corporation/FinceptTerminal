#pragma once
// Crypto Order Entry — BUY/SELL tabs with toggle type buttons

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoOrderEntry : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoOrderEntry(QWidget* parent = nullptr);

    void set_balance(double balance);
    void set_current_price(double price);
    void set_mode(bool is_paper);
    void set_symbol(const QString& symbol);
    void set_futures_mode(bool is_futures); // show/hide leverage + margin controls

  signals:
    void order_submitted(const QString& side, const QString& order_type, double quantity, double price,
                         double stop_price, double sl_price, double tp_price);
    void leverage_changed(int leverage);
    void margin_mode_changed(const QString& mode); // "cross" or "isolated"

  private slots:
    void on_submit();
    void on_pct_clicked(int pct);

  private:
    void update_cost_preview();
    void set_buy_side(bool is_buy);
    void set_order_type(int idx);

    // Side tabs
    QPushButton* buy_tab_ = nullptr;
    QPushButton* sell_tab_ = nullptr;

    // Type buttons
    QPushButton* type_btns_[4] = {};
    int active_type_ = 0; // 0=Market 1=Limit 2=Stop 3=StopLimit

    // Form
    QLineEdit* qty_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLineEdit* stop_price_edit_ = nullptr;
    QLineEdit* sl_edit_ = nullptr;
    QLineEdit* tp_edit_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QWidget* advanced_section_ = nullptr;
    QPushButton* advanced_toggle_ = nullptr;

    // Labels
    QLabel* balance_label_ = nullptr;
    QLabel* market_price_label_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    // Futures controls
    QWidget* futures_section_ = nullptr;
    QSpinBox* leverage_spin_ = nullptr;
    QComboBox* margin_mode_combo_ = nullptr;

    // State
    double balance_ = 0;
    double current_price_ = 0;
    bool is_paper_ = true;
    bool is_buy_side_ = true;
    bool is_futures_ = false;
    QString current_symbol_ = "BTC/USDT";
};

} // namespace fincept::screens::crypto
