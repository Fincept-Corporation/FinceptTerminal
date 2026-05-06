#pragma once
// LiveModeGateDialog — modal that user must clear before live mode is engaged.
//
// User must:
//   * type the exact phrase "I UNDERSTAND THIS IS REAL MONEY" (case-sensitive)
//   * tick "I am ≥ 18"
//   * tick "I am not in a sanctioned jurisdiction" (US/NK/IR/etc — geofence
//     enforced separately when GEOFENCE is on)
//   * paste an agent-wallet private key (separate from any master wallet)
//
// On accept, the dialog stores the key in SecureStorage under the key
// `alpha_arena/agent_key/<competition_id>` and writes a single immutable
// `live_mode_accepted` event into aa_events with timestamp + hostname.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 7.

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>

namespace fincept::screens::alpha_arena {

class LiveModeGateDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LiveModeGateDialog(const QString& competition_id, QWidget* parent = nullptr);

    /// Available after accept() returned QDialog::Accepted.
    QString agent_key_handle() const { return agent_key_handle_; }

  private slots:
    void update_confirm_state();
    void on_confirm();

  private:
    QString competition_id_;
    QString agent_key_handle_;

    QLineEdit* phrase_input_;
    QCheckBox* age_check_;
    QCheckBox* jurisdiction_check_;
    QLineEdit* private_key_input_;
    QPushButton* confirm_btn_;
    QLabel* status_label_;
};

} // namespace fincept::screens::alpha_arena
