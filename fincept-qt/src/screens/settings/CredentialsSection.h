#pragma once
// CredentialsSection.h — API key credentials panel for SettingsScreen.
// Stores keys in SecureStorage (AES-256-GCM encrypted SQLite); never written to disk plain.

#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QWidget>

class QPushButton;
class QVBoxLayout;

namespace fincept::screens {

class CredentialsSection : public QWidget {
    Q_OBJECT
  public:
    explicit CredentialsSection(QWidget* parent = nullptr);

    /// Refresh status labels and placeholders from SecureStorage.
    void reload();

    /// Narrow the provider cards to `text` (same as typing in the section's own
    /// filter box). Used by the Settings search to land on one provider.
    void set_filter(const QString& text);

    /// Every provider's display name and env-var key, for the Settings search
    /// ("tiingo" or "POLYGON_API_KEY" should find this section).
    static QStringList search_terms();

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();

    /// Build the card of provider `index` (into CRED_KEYS) at the end of the list.
    void build_card(int index);
    /// Build the next few cards and schedule the ones after them.
    void build_more_cards();
    /// Status label and placeholder of one built card, from SecureStorage.
    void load_card_state(const QString& key);

    /// Show only the provider cards whose name or env-var key contains `text`.
    void apply_filter(const QString& text);
    static bool matches_filter(const QPair<QString, QString>& def, const QString& needle);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QHash<QString, QLineEdit*> cred_fields_;      // key → password field
    QHash<QString, QLabel*> cred_status_;         // key → status label
    QHash<QString, QPushButton*> cred_save_btns_; // key → Save button
    QHash<QString, QWidget*> cred_cards_;         // key → card wrapper (shown/hidden by the filter)

    // Static text widgets cached for retranslateUi.
    QLabel* title_ = nullptr;
    QLabel* info_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QLabel* no_match_lbl_ = nullptr;

    // Cards are built in batches so the section opens at once (see build_ui()).
    static constexpr int kFirstCards = 12;  // with the section: more than a screenful
    static constexpr int kCardsPerTurn = 6; // then per turn of the event loop
    QWidget* page_ = nullptr;               // parent of the card wrappers
    QVBoxLayout* cards_layout_ = nullptr;   // the cards, above the page's stretch
    int next_card_ = 0;                     // index into CRED_KEYS of the next card to build
};

} // namespace fincept::screens
