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
class KeyCaptureDialog : public QDialog {
    Q_OBJECT
  public:
    explicit KeyCaptureDialog(KeyAction action, const QKeySequence& current, QWidget* parent = nullptr);

    /// Returns the captured sequence, or empty if cancelled.
    QKeySequence captured() const { return captured_; }

  protected:
    void keyPressEvent(QKeyEvent* event) override;

  private:
    QLabel*      hint_label_     = nullptr;
    QLabel*      captured_label_ = nullptr;
    QLabel*      conflict_label_ = nullptr;
    QPushButton* apply_btn_      = nullptr;

    KeyAction    action_;
    QKeySequence captured_;
};

/// Keybindings settings panel — grouped table of action→key with per-row Reset and Reset All.
class KeybindingsSection : public QWidget {
    Q_OBJECT
  public:
    explicit KeybindingsSection(QWidget* parent = nullptr);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    void rebuild_rows();
    QWidget* build_group(const QString& group_name, const QList<KeyAction>& actions);

    QVBoxLayout* groups_layout_ = nullptr;
    QLineEdit*   search_input_  = nullptr;
};

} // namespace fincept::screens
