#pragma once
// CredentialsSection.h — API key credentials panel for SettingsScreen.
// Stores keys in OS keychain via SecureStorage; never written to disk plain.

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

class QPushButton;

namespace fincept::screens {

class CredentialsSection : public QWidget {
    Q_OBJECT
  public:
    explicit CredentialsSection(QWidget* parent = nullptr);

    /// Refresh status labels and placeholders from SecureStorage.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QHash<QString, QLineEdit*>   cred_fields_; // key → password field
    QHash<QString, QLabel*>      cred_status_; // key → status label
    QHash<QString, QPushButton*> cred_save_btns_; // key → Save button

    // Static text widgets cached for retranslateUi.
    QLabel* title_ = nullptr;
    QLabel* info_  = nullptr;
};

} // namespace fincept::screens
