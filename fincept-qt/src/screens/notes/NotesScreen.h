#pragma once
#include "core/events/EventBus.h"
#include "screens/IStatefulScreen.h"
#include "storage/repositories/NotesRepository.h"

#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextEdit>
#include <QWidget>

namespace fincept::screens {

/// Financial Notes Screen — 3-panel layout:
/// Category Sidebar | Notes List | Editor / Viewer
class NotesScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit NotesScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "notes"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_category_selected(int row);
    void on_note_selected(int row);
    void on_new_note();
    void on_save_note();
    void on_delete_note();
    void on_toggle_favorite();
    void on_toggle_archive();
    void on_search_changed(const QString& text);
    void on_export_note();

  private:
    void build_ui();
    QWidget* build_category_sidebar();
    QWidget* build_notes_list_panel();
    QWidget* build_editor_panel();
    void load_notes();
    void update_notes_list();
    void show_note(const fincept::FinancialNote& note);
    void clear_editor();
    void enter_edit_mode();
    void enter_view_mode();

    // Data
    QVector<fincept::FinancialNote> notes_;
    QVector<fincept::FinancialNote> filtered_notes_;
    QString current_category_ = "ALL";
    int selected_note_id_ = -1;
    bool is_editing_ = false;

    // Category sidebar
    QListWidget* category_list_ = nullptr;
    QLabel* stats_label_ = nullptr;

    // Notes list panel
    QListWidget* notes_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLabel* count_label_ = nullptr;

    // Editor / Viewer panel
    QStackedWidget* right_stack_ = nullptr;
    // View mode
    QLabel* view_title_ = nullptr;
    QLabel* view_meta_ = nullptr;
    QLabel* view_content_ = nullptr;
    // Edit mode
    QLineEdit* edit_title_ = nullptr;
    QTextEdit* edit_content_ = nullptr;
    QComboBox* edit_category_ = nullptr;
    QComboBox* edit_priority_ = nullptr;
    QComboBox* edit_sentiment_ = nullptr;
    QLineEdit* edit_tags_ = nullptr;
    QLineEdit* edit_tickers_ = nullptr;

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP notes tools publish notes.created / notes.deleted when the LLM
    // mutates notes via Finagent or AI Chat. Each handler reloads the
    // notes list so the screen reflects the change immediately.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
};

} // namespace fincept::screens
