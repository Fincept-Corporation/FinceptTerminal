#pragma once
#include "core/events/EventBus.h"
#include "screens/common/IStatefulScreen.h"
#include "storage/repositories/NotesRepository.h"

#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
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
    void changeEvent(QEvent* event) override;

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
    /// A symbol link in a note (ticker chip or $CASHTAG) was clicked — offer "Open in…".
    void on_symbol_link(const QUrl& url);

  private:
    void build_ui();
    QWidget* build_category_sidebar();
    QWidget* build_notes_list_panel();
    QWidget* build_editor_panel();
    void retranslateUi();
    void load_notes();
    void update_notes_list();
    void show_note(const fincept::FinancialNote& note);
    void clear_editor();
    void enter_edit_mode();
    void enter_view_mode();

    /// Snapshot the editor fields into one comparable string. Used to detect
    /// unsaved edits without tracking every widget's changed() signal.
    QString editor_fingerprint() const;
    /// True when the editor is open and its contents differ from what was
    /// loaded into it.
    bool has_unsaved_edits() const;
    /// Ask before throwing away an in-progress edit. Returns true if the caller
    /// may proceed (nothing pending, or the user chose to discard/save).
    bool confirm_leave_editor();

    // ── Autosave ─────────────────────────────────────────────────────────────
    /// A field changed in the editor: (re)arm the autosave debounce.
    void on_editor_edited();
    /// Write the editor to the database without leaving it. Returns true when the
    /// note is saved (or nothing needed saving); false when it could not be (no title,
    /// database error).
    bool autosave_now();
    /// Build the note to write from the editor, keeping fields the editor does not
    /// show (favourite, archived, colour, reminder) from the stored note.
    fincept::FinancialNote note_from_editor() const;
    /// Undo everything autosave wrote since the editor was opened (CANCEL).
    void revert_edit_session();
    /// Markdown body of a note for the viewer, with $TICKER cashtags as links.
    static QString markdown_with_symbol_links(const QString& content);

    // Data
    QVector<fincept::FinancialNote> notes_;
    QVector<fincept::FinancialNote> filtered_notes_;
    QString current_category_ = "ALL";
    int selected_note_id_ = -1;
    bool is_editing_ = false;
    /// editor_fingerprint() as of the last load/save into the editor.
    QString editor_baseline_;

    // Category sidebar
    QListWidget* category_list_ = nullptr;
    QLabel* stats_label_ = nullptr;
    QLabel* category_header_ = nullptr; // "CATEGORIES" (cached for retranslateUi)

    // Notes list panel
    QListWidget* notes_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLabel* count_label_ = nullptr;
    QPushButton* new_btn_ = nullptr; // "+ NEW" (cached for retranslateUi)

    // Editor / Viewer panel
    QStackedWidget* right_stack_ = nullptr;
    QLabel* empty_label_ = nullptr; // empty-state placeholder (cached for retranslateUi)
    // View mode
    QLabel* view_title_ = nullptr;
    QLabel* view_meta_ = nullptr;
    QTextBrowser* view_content_ = nullptr; // markdown rendering, symbol links
    // View-mode toolbar buttons (cached for retranslateUi)
    QPushButton* view_edit_btn_ = nullptr;
    QPushButton* view_fav_btn_ = nullptr;
    QPushButton* view_archive_btn_ = nullptr;
    QPushButton* view_delete_btn_ = nullptr;
    // Edit mode
    QLineEdit* edit_title_ = nullptr;
    QTextEdit* edit_content_ = nullptr;
    QComboBox* edit_category_ = nullptr;
    QComboBox* edit_priority_ = nullptr;
    QComboBox* edit_sentiment_ = nullptr;
    QLineEdit* edit_tags_ = nullptr;
    QLineEdit* edit_tickers_ = nullptr;
    // Edit-mode toolbar buttons (cached for retranslateUi)
    QPushButton* edit_save_btn_ = nullptr;
    QPushButton* edit_cancel_btn_ = nullptr;
    QPushButton* edit_export_btn_ = nullptr;
    QLabel* autosave_label_ = nullptr; // "AUTOSAVED 14:32:05" / "AUTOSAVE: needs a title"
    // Edit-mode metadata field labels (cached for retranslateUi)
    QLabel* lbl_cat_ = nullptr;
    QLabel* lbl_pri_ = nullptr;
    QLabel* lbl_sent_ = nullptr;
    QLabel* lbl_tags_ = nullptr;
    QLabel* lbl_tickers_ = nullptr;

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP notes tools publish notes.created / notes.deleted when the LLM
    // mutates notes via Finagent or AI Chat. Each handler reloads the
    // notes list so the screen reflects the change immediately.
    QList<EventBus::HandlerId> mcp_event_subs_;

    QTimer* autosave_timer_ = nullptr;  // 2.5 s after the last keystroke
    QTimer* search_timer_ = nullptr;    // debounces the search box
    fincept::FinancialNote session_original_; // the note as it was when editing began
    bool has_session_original_ = false;
    bool autosaved_in_session_ = false; // autosave wrote something since editing began
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
};

} // namespace fincept::screens
