#pragma once
// ReportAiPanel — the Report Builder's AI drafting panel (RB-7): what would be sent, the
// streaming draft, and the PROPOSAL the user accepts (all or part) or discards.
//
//   Ready       the action, the model that answers (ui::ModelBadge "report_draft") and the
//               cost, the notice of exactly what leaves the machine (with the figures list),
//               Send / Cancel. Nothing is sent before Send.
//   Streaming   the draft filling in a read-only view (never the document), elapsed time, Stop.
//   Proposal    the change as a diff (insertions, deletions, numbers that are not in the
//               report's data marked), the list of changes for "accept part", Accept all /
//               Accept selected / Retry / Discard.
//   Titles      three titles to pick from, Use title / Retry / Discard.
//   Failed, Cancelled, needs setup   one ui::EmptyState with the way forward (Retry, Open
//               Settings).
// Accepting emits apply_requested() once (the screen applies it as one undo step), then
// closed(). Model output is shown as text, never as HTML.
//
// Appearance: one selector-scoped stylesheet (#rbAiPanel) set before any child exists;
// colours and sizes from ui/theme only. A theme change re-renders the diff HTML (it carries
// colours) and nothing else; sizes are re-fitted on ThemeManager::appearance_settled.

#include "services/report_builder/ReportAiSession.h"

#include <QWidget>

class QButtonGroup;
class QLabel;
class QListView;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QStandardItemModel;
class QTextBrowser;
class QTimer;

namespace fincept::ui {
class EmptyState;
class ModelBadge;
} // namespace fincept::ui

namespace fincept::screens {

class ReportAiPanel : public QWidget {
    Q_OBJECT
  public:
    /// `backend` nullptr = the panel's own ReportAiLlmBackend (LlmService + ModelRouter);
    /// a given backend is not owned and must outlive the panel.
    explicit ReportAiPanel(QWidget* parent, services::ReportAiBackend* backend = nullptr);
    ~ReportAiPanel() override;

    /// Shows what `request` would send (Ready). Sends nothing.
    void open(const services::AiRequest& request);
    /// A request is in flight.
    bool busy() const;
    services::ReportAiSession* session() const { return session_; }

    /// Width the panel reads well at (font metrics; the screen may give it 300–420 px).
    int preferred_width() const;
    QSize sizeHint() const override;

    /// "Rewrite", "Executive summary" … (the panel's header).
    static QString action_title(report::ai::Action action, const QString& tone = {});

  signals:
    void apply_requested(const fincept::services::ApplyResult& result);
    void closed();

  protected:
    void showEvent(QShowEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    enum class Message { Idle, NeedsSetup, Failed, Cancelled };

    void build_ready_page();
    void build_stream_page();
    void build_proposal_page();
    void build_titles_page();
    QWidget* button_row(QWidget* parent, const QList<QPushButton*>& buttons);
    QLabel* make_label(QWidget* parent, const char* role, bool wrap = true);
    QPushButton* make_button(QWidget* parent, const QString& text, const char* role = nullptr);

    void sync_to_state();
    void refresh_ready();
    void render_ready(const services::AiModelInfo& info);
    void render_stream_status();
    void schedule_stream_flush();
    void flush_stream();
    void render_proposal();
    void render_diff_html();
    void render_titles();
    void render_flags(QLabel* label);
    void show_message(Message m);
    void on_message_primary();
    void on_message_secondary();
    void on_send();
    void on_accept_selected();
    void update_accept_selected();
    void close_panel();
    void refit();

    services::ReportAiBackend* backend_ = nullptr;
    services::ReportAiSession* session_ = nullptr;

    QLabel* title_label_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    // Ready
    QWidget* ready_page_ = nullptr;
    ui::ModelBadge* badge_ = nullptr;
    QLabel* cost_label_ = nullptr;
    QLabel* notice_label_ = nullptr;
    QPushButton* facts_toggle_ = nullptr;
    QListView* facts_view_ = nullptr;
    QStandardItemModel* facts_model_ = nullptr;
    QPushButton* send_btn_ = nullptr;
    QPushButton* cancel_btn_ = nullptr;

    // Streaming
    QWidget* stream_page_ = nullptr;
    QLabel* stream_status_ = nullptr;
    QPlainTextEdit* stream_view_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QTimer* tick_ = nullptr;
    QTimer* stream_flush_ = nullptr;
    QString stream_pending_;
    int stream_rendered_ = 0;
    QString stream_model_;

    // Proposal
    QWidget* proposal_page_ = nullptr;
    QLabel* meta_label_ = nullptr;
    QLabel* fallback_label_ = nullptr;
    QTextBrowser* diff_view_ = nullptr;
    QLabel* flags_label_ = nullptr;
    QLabel* nochange_label_ = nullptr;
    QLabel* hunks_label_ = nullptr;
    QListView* hunk_view_ = nullptr;
    QStandardItemModel* hunk_model_ = nullptr;
    QPushButton* accept_all_btn_ = nullptr;
    QPushButton* accept_sel_btn_ = nullptr;
    QPushButton* retry_btn_ = nullptr;
    QPushButton* discard_btn_ = nullptr;

    // Titles
    QWidget* titles_page_ = nullptr;
    QButtonGroup* title_group_ = nullptr;
    QList<QRadioButton*> title_radios_;
    QList<QLabel*> title_labels_;
    QLabel* title_flags_label_ = nullptr;
    QPushButton* use_title_btn_ = nullptr;
    QPushButton* title_retry_btn_ = nullptr;
    QPushButton* title_discard_btn_ = nullptr;

    // Failed / Cancelled / needs setup / idle
    ui::EmptyState* message_ = nullptr;
    Message message_mode_ = Message::Idle;
};

} // namespace fincept::screens
