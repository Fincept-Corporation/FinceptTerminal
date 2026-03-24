#pragma once
// Equity Credentials — dialog for entering broker API keys

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>

namespace fincept::screens::equity {

class EquityCredentials : public QDialog {
    Q_OBJECT
  public:
    explicit EquityCredentials(const QString& broker_id, QWidget* parent = nullptr);

  signals:
    void credentials_saved(const QString& broker_id, const QString& api_key, const QString& api_secret,
                           const QString& auth_code);

  private slots:
    void on_save();
    void on_clear();

  private:
    QLineEdit* key_edit_ = nullptr;
    QLineEdit* secret_edit_ = nullptr;
    QLineEdit* auth_code_edit_ = nullptr;
    QLabel* status_label_ = nullptr;
    QString broker_id_;
};

} // namespace fincept::screens::equity
