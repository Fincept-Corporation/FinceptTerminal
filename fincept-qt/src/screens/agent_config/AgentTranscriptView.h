#pragma once
// AgentTranscriptView.h — the live transcript of an agent run.
//
// Virtualized: a list model of AgentEvents, a delegate that paints each row with
// QPainter (no widget per event) and caches its height per width, and a
// QListView that toggles rows open/closed on click (tool calls start collapsed
// to a short preview) and copies selected rows with Ctrl+C.
//
// One TOOL CARD per call: a tool result (or a finished command) whose ref_id
// matches an earlier call is joined into that call's row — input, then the
// result, with a running / ok / failed mark — instead of a second row.
// Streamed text (AgentEvent::partial) is shown in one LIVE row at the end that
// the complete message replaces.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QAbstractListModel>
#include <QFont>
#include <QHash>
#include <QListView>
#include <QStyledItemDelegate>
#include <QVector>

#include <optional>

class QTextLayout;

namespace fincept::screens {

class AgentTranscriptModel : public QAbstractListModel {
    Q_OBJECT
  public:
    /// Rows kept in the view; older rows are dropped (the full transcript stays
    /// in the database — EXPORT writes all of it).
    static constexpr int kMaxRows = 5000;

    struct Row {
        services::coding_agents::AgentEvent ev;
        /// The ToolResult / finished Command joined to this call by ref_id.
        std::optional<services::coding_agents::AgentEvent> result;
        bool expanded = false;
        bool superseded = false; // assistant text repeated verbatim by the final result
        bool live = false;       // streamed text still arriving (never part of the stored transcript)
        mutable int cached_width = -1;
        mutable int cached_height = 0;
        mutable bool cached_more = false;
    };

    explicit AgentTranscriptModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void reset_events(const QVector<services::coding_agents::AgentEvent>& events);
    /// Appends only events newer than the last one shown (safe to call with overlap).
    void append_events(const QVector<services::coding_agents::AgentEvent>& events);
    /// Show / update the streamed text of the message in progress (a partial event).
    void set_live(const services::coding_agents::AgentEvent& partial);
    /// The run ended: drop streamed text that never got its complete message.
    void drop_live() { remove_live(); }
    void clear_events();

    const Row* row_at(int row) const;
    void toggle_expanded(int row);
    void set_all_expanded(bool expanded);
    void invalidate_layout();
    int hidden_count() const { return hidden_; }
    int last_seq() const { return max_seq_; }

    /// Plain-text rendering of one row (copy / export): the call and its result.
    static QString row_text(const Row& row);
    /// Plain-text rendering of one event.
    static QString event_text(const services::coding_agents::AgentEvent& ev);

  private:
    /// Join `ev` to an open call row (index into rows_), or -1.
    int join_target(const services::coding_agents::AgentEvent& ev) const;
    void mark_superseded(const services::coding_agents::AgentEvent& final_ev);
    void remove_live();
    int live_row() const; // index of the live row, or -1

    QVector<Row> rows_;
    int hidden_ = 0;
    int max_seq_ = 0;
};

class AgentTranscriptDelegate : public QStyledItemDelegate {
    Q_OBJECT
  public:
    explicit AgentTranscriptDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Re-read fonts from the theme (call after an appearance change).
    void refresh_fonts();
    /// Notify the view that this row's height changed (after expand/collapse).
    void row_height_changed(const QModelIndex& index) { emit sizeHintChanged(index); }

  private:
    struct BodyLayout {
        int lines_a = 0; // the event's own body (a call's input)
        int lines_b = 0; // a joined result
        bool more = false;
    };
    QString tag_text(const AgentTranscriptModel::Row& row) const;
    static int collapsed_lines(const AgentTranscriptModel::Row& row);
    static QString prepared(const QString& text, int cap, bool tail, bool expanded);
    static QString part_a_text(const AgentTranscriptModel::Row& row);
    BodyLayout layout_body(QTextLayout& a, QTextLayout& b, const AgentTranscriptModel::Row& row, int width) const;
    int lay_out(QTextLayout& layout, const QString& text, int width, int max_lines, bool* more) const;
    int content_width(int row_width) const;

    QFont meta_font_;
    QFont tag_font_;
    QFont title_font_;
    QFont body_font_;
    int body_line_h_ = 14;
    int title_h_ = 16;
    int gutter_time_ = 64; // time column width, sized to the meta font
    int gutter_tag_ = 62;  // kind-tag column width, sized to the tag font
};

class AgentTranscriptView : public QListView {
    Q_OBJECT
  public:
    explicit AgentTranscriptView(QWidget* parent = nullptr);

    void set_transcript_model(AgentTranscriptModel* model);
    AgentTranscriptDelegate* transcript_delegate() const { return delegate_; }
    bool is_at_bottom() const;
    /// Lay out pending rows first, then scroll — scrollToBottom() alone lands on the
    /// pre-insert maximum because QListView lays out new rows lazily.
    void scroll_to_end();
    void copy_selection() const;

  protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

  private:
    void on_clicked(const QModelIndex& index);
    void view_event(const QModelIndex& index);

    AgentTranscriptModel* model_ = nullptr;
    AgentTranscriptDelegate* delegate_ = nullptr;
};

} // namespace fincept::screens
