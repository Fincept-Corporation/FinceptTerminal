#pragma once
// Crypto Order Entry — buy/sell form with SL/TP, percentage buttons

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace fincept::screens::crypto {

class CryptoOrderEntry : public QWidget {
    Q_OBJECT
public:
    explicit CryptoOrderEntry(QWidget* parent = nullptr);

    void set_balance(double balance);
    void set_current_price(double price);
    void set_mode(bool is_paper);

signals:
    void order_submitted(const QString& side, const QString& order_type,
                         double quantity, double price, double stop_price,
                         double sl_price, double tp_price);

private slots:
    void on_submit();
    void on_pct_clicked(int pct);

private:
    void update_cost_preview();

    QComboBox* side_combo_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QLineEdit* qty_edit_ = nullptr;
    QLineEdit* price_edit_ = nullptr;
    QLineEdit* stop_price_edit_ = nullptr;
    QLineEdit* sl_edit_ = nullptr;
    QLineEdit* tp_edit_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QLabel* balance_label_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* mode_label_ = nullptr;

    double balance_ = 0;
    double current_price_ = 0;
    bool is_paper_ = true;
};

} // namespace fincept::screens::crypto
