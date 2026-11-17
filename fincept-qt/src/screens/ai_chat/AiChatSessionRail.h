#pragma once
// AiChatSessionRail.h — the sessions rail of the AI Chat tab (left zone; a drawer < 1100 px).
//
//   ▌SESSIONS 42                                   [+]
//   [ search sessions …                          ]
//   PINNED
//   ▌Fed path after CPI                 14:32  ●
//    "Rates priced for two cuts by…"   GPT-4O
//   TODAY / YESTERDAY / THIS WEEK / OLDER
//   …
//   ─────────────────────────────────────────────────
//   RENAME  PIN  EXPORT                      DELETE
//
// Rows are painted by a delegate (title, last-message snippet, time in tabular figures,
// model chip, running dot / unread dot, pin tick); group headers are not selectable. Hover
// shows painted PIN / RENAME / DELETE glyphs on the row (clicks hit-tested in the
// delegate); the context menu has Rename, Pin / Unpin, Export as Markdown…, Copy as
// Markdown, Delete…  (the Delete action carries fincept_agent_risk "delete"). The footer
// acts on the selected session; its DELETE button carries "delete" too. The list view has
// fincept_agent_risk_keys "Delete=delete" (Del deletes, F2 renames, Enter opens).
//
// Search filters titles and snippets (case-insensitive, every word must match). Deleting
// is the host's job (it confirms first) — this widget only asks.

#include "screens/ai_chat/AiChatTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QLineEdit;
class QListView;
class QPushButton;
class QStyledItemDelegate;

namespace fincept::ui {
class PanelHeader;
}

namespace fincept::screens::aichat {

class AicSessionModel; // AiChatSessionRail.cpp

class AicSessionRail : public QWidget {
    Q_OBJECT
  public:
    explicit AicSessionRail(QWidget* parent = nullptr);
    ~AicSessionRail() override;

    /// Every session (any order — grouped Pinned / Today / Yesterday / This week / Older,
    /// newest first inside a group). Keeps the selection by id and the search text.
    void set_sessions(const QVector<SessionSummary>& sessions);
    /// Highlight `id` (no signal); "" = none.
    void set_active(const QString& id);
    QString active() const { return active_; }
    void set_running(const QString& id, bool running);
    void set_unread(const QString& id, bool unread);
    void set_snippet(const QString& id, const QString& snippet);
    void set_search_text(const QString& text);
    QString search_text() const;
    /// False while an answer streams (switching would abandon it): rows and actions are
    /// disabled with a tooltip saying why; the running row stays highlighted.
    void set_interactive(bool interactive);

    void focus_list();
    void focus_search();
    QWidget* list_widget() const;
    QWidget* search_widget() const;
    /// Width that fits ~26 average characters at the current font.
    int preferred_width() const;

  signals:
    void session_activated(const QString& id);
    void new_requested();
    void rename_requested(const QString& id);
    void pin_toggled(const QString& id, bool pinned);
    void export_requested(const QString& id); ///< save as Markdown…
    void copy_requested(const QString& id);   ///< copy as Markdown
    void delete_requested(const QString& id); ///< the host confirms
    void search_changed(const QString& text);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    QLineEdit* search_ = nullptr;
    QListView* list_ = nullptr;
    AicSessionModel* model_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* rename_btn_ = nullptr;
    QPushButton* pin_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QString active_;
    bool interactive_ = true;

    void retranslate();
    void update_footer();
    void show_menu(const QPoint& pos);

    // ── private (AiChatSessionRail.cpp) ──────────────────────────────────────
    ui::PanelHeader* header_ = nullptr;
    QWidget* footer_ = nullptr;
    QStyledItemDelegate* delegate_ = nullptr; ///< the row painter (AiChatSessionRail.cpp)
    bool footer_compact_ = false;             ///< glyph labels: the full ones do not fit
    bool programmatic_search_ = false;        ///< set_search_text(): no search_changed

    void apply_search(const QString& text, bool user);
    QString selected_id() const;
    QString target_id() const; ///< the selected session, else the active one
    void select_id(const QString& id, bool scroll);
    void activate_row(int row);
    void run_glyph(int glyph, const QString& id);
    void open_export_menu(const QString& id);
    void update_badge();
    void fit_metrics();
    void fit_footer();
    int footer_full_width() const;
};

} // namespace fincept::screens::aichat
