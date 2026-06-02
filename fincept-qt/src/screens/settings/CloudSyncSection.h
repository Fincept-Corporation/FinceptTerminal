#pragma once
#include <QCheckBox>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QShowEvent>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

/// Settings > Cloud Sync — master switch + advanced per-domain opt-out + live
/// status. Drives CloudSyncEngine; the flags themselves are device-local.
///
/// The per-domain rows are generated from a single list (see cloud_domains() in
/// the .cpp) — adding a synced domain is a one-line entry there.
class CloudSyncSection : public QWidget {
    Q_OBJECT
  public:
    explicit CloudSyncSection(QWidget* parent = nullptr);

    void reload();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void update_enabled_state();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QCheckBox* master_toggle_ = nullptr;
    QLabel* sign_in_hint_ = nullptr;
    QLabel* credits_banner_ = nullptr;
    QHash<QString, QCheckBox*> domain_toggles_; // entity -> "sync this domain" checkbox
    QHash<QString, QLabel*> domain_status_;     // entity -> status label

    // Static text widgets cached for retranslateUi.
    QLabel* title_ = nullptr;
    QLabel* blurb_ = nullptr;
    QLabel* adv_title_ = nullptr;
    QLabel* cloud_sync_label_ = nullptr;
    QLabel* cloud_sync_desc_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // Per-domain row label + description, keyed by entity (for retranslateUi).
    QHash<QString, QLabel*> domain_row_label_;
    QHash<QString, QLabel*> domain_row_desc_;
};

} // namespace fincept::screens
