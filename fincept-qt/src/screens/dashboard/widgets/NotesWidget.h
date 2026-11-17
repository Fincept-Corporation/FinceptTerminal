#pragma once
// NotesWidget — recent / favorite financial notes from NotesRepository.
// Refreshes on EventBus `notes.created` / `notes.updated` / `notes.deleted`
// (and on show / manual refresh). Click a row to
// jump to the Notes screen via `nav.switch_screen`.
//
// Config (persisted): { "filter": "recent"|"favorites", "max_rows": <int> }

#include "core/events/EventBus.h"
#include "screens/dashboard/widgets/BaseWidget.h"

#include <QList>
#include <QScrollArea>
#include <QVBoxLayout>

class QPlainTextEdit;
class QTextBrowser;

namespace fincept::screens::widgets {

class NotesWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit NotesWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    QDialog* make_config_dialog(QWidget* parent) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void apply_styles();
    void refresh_data();
    void clear_rows();
    void subscribe_events();
    void unsubscribe_events();

    QString filter_ = "recent"; // "recent" or "favorites"
    int max_rows_ = 8;

    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QList<EventBus::HandlerId> event_subs_;
};

// DeskNoteWidget (type id "desk_note") — free text kept in the TILE'S OWN CONFIG,
// not in NotesRepository: it never appears in the Notes screen and the Notes
// screen's database is never touched.
//
// Config: {"seed": "<template id>", "text": "<the user's text>"}.
//   * "seed" names the template whose note this is. While "text" is absent the
//     tile shows that template's seed note (DashboardTemplates.h,
//     template_note_seed()), translated and as shipped by the running version,
//     so an unedited note picks up an improved seed after an update.
//   * "text" is present once the user saved an edit; it always wins over the seed.
//     Saving the seed text unchanged removes it again (still "unedited").
//   * A seeded note's edits are also recorded per template id in the settings
//     key "dashboard.template_notes" (template_note_save_text()), which
//     build_template_layout() reads, so re-applying the template brings the
//     edited note back. RESTORE (with confirmation) is the only path back to the
//     seed: it clears "text" and that record.
//   * Without "seed" it is a plain note pad (empty until written).
// The note renders as Markdown; EDIT (or a double-click) switches to the source.
class DeskNoteWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit DeskNoteWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void retranslateUi() override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    /// What the note shows: the user's text, else the seed.
    QString shown_text() const;
    void render();
    void begin_edit();
    void finish_edit(bool save);
    void restore_seed();
    void apply_styles();
    void update_title();

    QString seed_;        ///< template id ("" = plain note pad)
    QString text_;        ///< the user's text (valid when edited_)
    bool edited_ = false; ///< config carries "text": the user's text wins over the seed
    bool editing_ = false;

    QTextBrowser* view_ = nullptr;
    QPlainTextEdit* editor_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* edit_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;
    QPushButton* restore_btn_ = nullptr;
};

} // namespace fincept::screens::widgets
