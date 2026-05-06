#pragma once
// CredentialsSection.h — API key credentials panel for SettingsScreen.
// Stores keys in OS keychain via SecureStorage; never written to disk plain.

#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

namespace fincept::screens {

class CredentialsSection : public QWidget {
    Q_OBJECT
  public:
    explicit CredentialsSection(QWidget* parent = nullptr);

    /// Refresh status labels and placeholders from SecureStorage.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();

    QHash<QString, QLineEdit*> cred_fields_; // key → password field
    QHash<QString, QLabel*>    cred_status_; // key → status label
};

} // namespace fincept::screens
