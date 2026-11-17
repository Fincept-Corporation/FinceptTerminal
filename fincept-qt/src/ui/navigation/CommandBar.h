#pragma once
// CommandBar — the header's universal search box. No syntax: typing goes straight to
// results (symbols, screens and their tabs, actions, settings, the user's saved items,
// data sources, help), grouped in a panel under the box; Enter does the obvious thing,
// Tab / → offers the row's other actions, Ctrl+Enter opens beside the current panel,
// Esc closes and gives focus back. Ctrl+K (the user's Search key) focuses it from
// anywhere; "/" too when focus is not in a text field.
//
// Pieces (src/ui/navigation/search/): SearchField (the box), SearchPopup + SearchModel +
// SearchDelegate (the panel), SearchController (query -> rows -> run), SearchActions /
// SearchInstruments (what a row does), SearchSources (seed, ActionRegistry bridge, saved
// items, data sources, usage). Rows any screen can add: core/command/CommandRegistry.h.
//
// Kept from the old bar: navigate_to / dock_command signals (ToolBar -> WindowFrame), every
// old alias ("bt", "wl", "mkts" …), "<screen> add|replace|remove <screen>", the /stock …
// /economic searches (now optional), the symbol hand-off (nav.open_symbol), recent picks.

#include <QPointer>
#include <QWidget>

// Kept from the old header: files that include ToolBar.h (WindowFrame*) may rely on these.
#include "services/markets/MarketSearchService.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>

class QShortcut;

namespace fincept::ui {

namespace search {
class SearchController;
class SearchField;
class SearchModel;
class SearchPopup;
struct SearchRow;
} // namespace search

class CommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit CommandBar(QWidget* parent = nullptr);
    ~CommandBar() override;

    /// Focus the box and open the results (Ctrl+K, "/", View > Search). Remembers where focus was.
    void activate();
    /// activate() on the visible search box of `window`; false when it has none showing (focus mode).
    static bool activate_in(QWidget* window);
    /// The Component Browser dialog (every screen by category) of this window.
    void open_component_browser();

  signals:
    void navigate_to(const QString& screen_id);
    /// "markets add news" → ("add", "markets", "news"); "replace"; "remove" (secondary empty).
    void dock_command(const QString& action, const QString& primary, const QString& secondary);
    /// A shell menu action (ToolBar::action_triggered ids: new_window, layout_save_as, check_updates …).
    void toolbar_action(const QString& action);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

  private:
    void on_text_changed(const QString& text);
    void on_rows(const QList<search::SearchRow>& rows, const QString& preselect);
    void move_selection(int step);
    void open_menu_for_current(const QPoint& global_pos = QPoint());
    void open_menu(int row, const QPoint& global_pos);
    void close_popup(bool restore_focus);
    void open_popup();
    QString current_key() const;
    void refit();

    search::SearchField* field_ = nullptr;
    search::SearchModel* model_ = nullptr;
    search::SearchPopup* popup_ = nullptr;
    search::SearchController* controller_ = nullptr;
    QPointer<QWidget> previous_focus_;
    QShortcut* slash_ = nullptr;
    bool user_moved_ = false;
    bool menu_open_ = false;
    bool closing_ = false;
};

} // namespace fincept::ui
