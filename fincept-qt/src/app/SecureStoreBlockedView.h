#pragma once
// SecureStoreBlockedView — what a window shows instead of the login / PIN
// screens when SecureStorage could not be read at start-up (PinManager
// PinState::Unreadable, or AuthManager::saved_session_unreadable()).
//
//   "Could not read secure storage: <reason>."
//   [Retry]  [Sign in again]  [Reset secure storage…]  [Quit]
//
// Which buttons appear follows the failure: Retry always (a transient failure
// is also retried automatically by PinManager); "Sign in again" for a damaged
// PIN record (the PIN is cleared only through that explicit, re-authenticated
// path); "Reset secure storage…" when the key that protects the store cannot be
// unlocked on this account / computer (erases every saved credential, after a
// confirmation). The view only renders and reports clicks; WindowFrame decides
// what each action does. No Q_OBJECT: the actions are plain callbacks.

#include "storage/secure/SecretRead.h"

#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;

namespace fincept {

class SecureStoreBlockedView : public QWidget {
  public:
    struct Actions {
        std::function<void()> retry;
        std::function<void()> sign_in_again;
        std::function<void()> reset_store;
        std::function<void()> quit;
    };

    explicit SecureStoreBlockedView(Actions actions, QWidget* parent = nullptr);

    /// `subject`: what could not be read, in user words ("your PIN", "your saved sign-in").
    void show_failure(const SecretRead& failure, const QString& subject);

    /// The plain-language headline for `failure` — unit-testable.
    static QString headline(const SecretRead& failure);

  private:
    void restyle();

    Actions actions_;
    QLabel* title_ = nullptr;
    QLabel* message_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* retry_ = nullptr;
    QPushButton* sign_in_ = nullptr;
    QPushButton* reset_ = nullptr;
    QPushButton* quit_ = nullptr;
};

} // namespace fincept
