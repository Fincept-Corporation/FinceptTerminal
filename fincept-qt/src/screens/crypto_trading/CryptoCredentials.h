#pragma once
// Crypto Credentials — dialog for entering exchange API keys

#include <QDialog>
#include <QLineEdit>
#include <QLabel>

namespace fincept::screens::crypto {

class CryptoCredentials : public QDialog {
    Q_OBJECT
public:
    explicit CryptoCredentials(const QString& exchange_id, QWidget* parent = nullptr);

    QString api_key() const;
    QString api_secret() const;
    QString password() const;

    void set_values(const QString& key, const QString& secret, const QString& password);

signals:
    void credentials_saved(const QString& api_key, const QString& api_secret, const QString& password);

private slots:
    void on_save();
    void on_clear();

private:
    QLineEdit* key_edit_ = nullptr;
    QLineEdit* secret_edit_ = nullptr;
    QLineEdit* password_edit_ = nullptr;
    QLabel* status_label_ = nullptr;
    QString exchange_id_;
};

} // namespace fincept::screens::crypto
