#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace fincept::wallet {

class LocalWalletBridge;

/// Modal dialog that owns a LocalWalletBridge, opens the user's default
/// browser to the bridge URL, and emits `connected(pubkey, label)` once a
/// valid signed-message handshake completes.
class ConnectWalletDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ConnectWalletDialog(QWidget* parent = nullptr);
    ~ConnectWalletDialog() override;

  signals:
    void connected(QString pubkey_b58, QString label);
    void failed(QString reason);

  protected:
    void showEvent(QShowEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

  private:
    void start_handshake();
    void on_payload(QString pubkey, QString signature, QString label);
    void on_timed_out();
    void on_bridge_error(QString message);
    void fail_and_close(const QString& reason);

    LocalWalletBridge* bridge_ = nullptr;
    QByteArray nonce_hex_;
    QByteArray expected_message_;
    QLabel* status_label_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QPushButton* reopen_button_ = nullptr;
    QString last_open_url_;
    bool handshake_done_ = false;
};

} // namespace fincept::wallet
