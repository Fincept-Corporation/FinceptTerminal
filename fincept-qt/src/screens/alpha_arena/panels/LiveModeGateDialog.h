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
// `live_mode_accepted` event into arena_events (ArenaStore) with timestamp +
// hostname. The wizard passes competition_id="pending" (the competition does
// not exist yet); ArenaEngine::start() re-keys the wallet handle to the real id.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 7.

#include <QCheckBox>
#include <QDialog>
#include <QEvent>
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

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void update_confirm_state();
    void on_confirm();

  private:
    void retranslateUi();

    QString competition_id_;
    QString agent_key_handle_;

    QLabel* warn_label_ = nullptr;
    QLabel* phrase_prompt_ = nullptr;
    QLineEdit* phrase_input_;
    QCheckBox* age_check_;
    QCheckBox* jurisdiction_check_;
    QLabel* key_prompt_ = nullptr;
    QLineEdit* private_key_input_;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* confirm_btn_;
    QLabel* status_label_;
};

} // namespace fincept::screens::alpha_arena
