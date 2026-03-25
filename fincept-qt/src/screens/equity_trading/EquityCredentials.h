#pragma once
// EquityCredentials — broker credential dialog driven by BrokerProfile
// For brokers with has_native_paper=true, shows separate LIVE and PAPER sections.

#include "trading/BrokerInterface.h"

#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>

namespace fincept::screens::equity {

class EquityCredentials : public QDialog {
    Q_OBJECT
  public:
    explicit EquityCredentials(const QString& broker_id,
                               const trading::BrokerProfile& profile,
                               QWidget* parent = nullptr);

    // Called by EquityTradingScreen after successful auth to update status labels
    void mark_connected(const QString& env, const QString& account_id);
    void mark_error(const QString& error);

  signals:
    // auth_code == "live" / "paper" (for has_native_paper brokers) or auth code string
    void credentials_saved(const QString& broker_id, const QString& api_key,
                           const QString& api_secret, const QString& auth_code);

  private:
    void on_connect(const QString& env); // env="live"/"paper"/"" (regular)
    void on_clear(const QString& env);

    QString broker_id_;
    trading::BrokerProfile profile_;

    // Native-paper broker fields (e.g. Alpaca) — live side
    QLineEdit* field_live_key_     = nullptr;
    QLineEdit* field_live_secret_  = nullptr;
    QLabel*    live_status_        = nullptr;

    // Native-paper broker fields — paper side
    QLineEdit* field_paper_key_    = nullptr;
    QLineEdit* field_paper_secret_ = nullptr;
    QLabel*    paper_status_       = nullptr;

    // Regular broker fields
    QLineEdit* field_client_code_ = nullptr;  // AngelOne: separate client/login ID
    QLineEdit* field_key_    = nullptr;
    QLineEdit* field_secret_ = nullptr;
    QLineEdit* field_auth_   = nullptr;
    QComboBox* field_env_    = nullptr;

    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens::equity
