// src/screens/settings/KeybindingsSection.h
#pragma once
#include "core/keys/KeyActions.h"

#include <QDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Modal dialog that captures a single key combination from the user.
/// Apply stays disabled while the combination collides with another binding
/// that can fire in the same place (two shortcuts on one key in one window are
/// "ambiguous" to Qt, and then NEITHER fires), or when a global action is given
/// a bare editing key (Enter / Esc / arrows …) that would swallow that key
/// everywhere.
class KeyCaptureDialog : public QDialog {
    Q_OBJECT
  public:
    explicit KeyCaptureDialog(KeyAction action, const QKeySequence& current, QWidget* parent = nullptr);

    /// Returns the captured sequence, or empty if cancelled.
    QKeySequence captured() const { return captured_; }

  protected:
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLabel* current_label_ = nullptr;
    QLabel* scope_label_ = nullptr;
    QLabel* hint_label_ = nullptr;
    QLabel* captured_label_ = nullptr;
    QLabel* conflict_label_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;

    KeyAction action_;
    QKeySequence current_;
    QKeySequence captured_;
};

/// Keybindings settings panel — grouped table of action→key with per-row Reset and Reset All.
/// Rebinding is live: KeyConfigManager updates the shared QAction (every window
/// picks it up) and persists the sequence.
class KeybindingsSection : public QWidget {
    Q_OBJECT
  public:
    explicit KeybindingsSection(QWidget* parent = nullptr);

    /// Put `text` in the section's own search box (Settings search deep-link).
    void set_search(const QString& text);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void apply_theme();
    void rebuild_rows();
    QWidget* build_group(const QString& group_name, const QList<KeyAction>& actions);
    void open_capture(KeyAction a);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QVBoxLayout* groups_layout_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLabel* hint_label_ = nullptr;
    QPushButton* reset_all_btn_ = nullptr;
    bool rebuild_pending_ = false; // a coalesced rebuild_rows() is already queued
};

} // namespace fincept::screens
