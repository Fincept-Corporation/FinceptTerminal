#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QListWidget;
class QScrollArea;
class QGridLayout;
class QLabel;

namespace fincept::ui {

class ComponentCard;

/// Modal dialog letting users browse every registered screen (component)
/// and launch one into the active workspace. Bloomberg Launchpad
/// Component Browser equivalent.
///
/// Layout:
///   Left  — category sidebar ("All" at top, then distinct categories in
///           catalogue order)
///   Top   — search box (matches against title + tags, case-insensitive)
///   Main  — grid of ComponentCards, sorted by popularity desc
///
/// Keyboard: Esc closes, Enter activates the current selection, arrow keys
/// navigate between cards when the list-view equivalent focus is wired up
/// (grid cards don't natively support keyboard focus yet — future polish).
class ComponentBrowserDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ComponentBrowserDialog(QWidget* parent = nullptr);

  signals:
    /// Emitted when the user double-clicks a card or presses Enter.
    /// The caller (WindowFrame) is expected to navigate to the screen.
    /// The dialog closes itself after emitting.
    void component_chosen(const QString& screen_id);

  private:
    void build_ui();
    void rebuild_cards();
    void on_search_changed(const QString& query);
    void on_category_changed(int row);

    QLineEdit* search_ = nullptr;
    QListWidget* category_list_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QGridLayout* grid_layout_ = nullptr;
    QLabel* count_label_ = nullptr;

    QString active_category_ = "All";
    QString search_query_;
    QString selected_id_;
};

} // namespace fincept::ui
