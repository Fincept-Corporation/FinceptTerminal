#pragma once

#include "services/prediction/PredictionCredentialStore.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTextEdit;

namespace fincept::screens::polymarket {

/// Credential setup dialog for the Prediction Markets tab.
///
/// Two tabs — one per supported exchange — each with the provider-specific
/// credential fields. "Save" writes to SecureStorage via
/// PredictionCredentialStore; "Clear" removes the stored entry; "Test
/// Connection" is a Phase 6/7 hook (emits test_requested() so the caller
/// can invoke the relevant adapter's fetch_balance()).
class PredictionAccountDialog : public QDialog {
    Q_OBJECT
  public:
    explicit PredictionAccountDialog(QWidget* parent = nullptr);

    /// Select which tab is shown initially.
    void set_active_exchange(const QString& exchange_id);

  signals:
    /// Emitted after the user clicks Save and credentials are persisted.
    /// `exchange_id` is "polymarket" or "kalshi". Screens should reload
    /// their adapter's credentials in response.
    void credentials_saved(const QString& exchange_id);
    /// User clicked Test Connection for the given exchange.
    void test_requested(const QString& exchange_id);

  private slots:
    void on_save_polymarket();
    void on_clear_polymarket();
    void on_save_kalshi();
    void on_clear_kalshi();
    void on_load_kalshi_pem();

  private:
    void build_ui();
    void build_polymarket_tab();
    void build_kalshi_tab();
    void load_existing();

    QTabWidget* tabs_ = nullptr;

    // Polymarket tab
    QLineEdit* pm_private_key_ = nullptr;
    QLineEdit* pm_funder_ = nullptr;
    QComboBox* pm_signature_type_ = nullptr;
    QLabel*    pm_derived_status_ = nullptr;
    QLabel*    pm_status_ = nullptr;
    QPushButton* pm_save_btn_ = nullptr;
    QPushButton* pm_test_btn_ = nullptr;
    QPushButton* pm_clear_btn_ = nullptr;

    // Kalshi tab
    QLineEdit* ks_api_key_id_ = nullptr;
    QTextEdit* ks_private_key_pem_ = nullptr;
    QPushButton* ks_load_pem_btn_ = nullptr;
    QCheckBox* ks_use_demo_ = nullptr;
    QLabel*    ks_status_ = nullptr;
    QPushButton* ks_save_btn_ = nullptr;
    QPushButton* ks_test_btn_ = nullptr;
    QPushButton* ks_clear_btn_ = nullptr;
};

} // namespace fincept::screens::polymarket
