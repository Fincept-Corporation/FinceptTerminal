#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QListWidget;

namespace fincept::ui {

/// Phase 9: Ctrl+K palette overlay. Fuzzy search over `SuggestionIndex`.
///
/// Modal-ish: blocks input to the underlying frame while open (similar
/// to VSCode's command palette). Esc dismisses. Enter invokes the
/// selected suggestion.
///
/// Each invocation creates a fresh dialog (cheap: it's a QDialog with
/// a QLineEdit + QListWidget) and surfaces it centered on the parent
/// frame.
class CommandPalette : public QDialog {
    Q_OBJECT
  public:
    /// Show the palette over the given frame. Centred on frame, not on
    /// primary screen, so multi-monitor users see it where they're
    /// looking.
    static void show_for(QWidget* parent_frame);

  private:
    explicit CommandPalette(QWidget* parent);

    void on_text_changed(const QString& text);
    void on_accept();

    QLineEdit* input_ = nullptr;
    QListWidget* suggestions_ = nullptr;
};

} // namespace fincept::ui
