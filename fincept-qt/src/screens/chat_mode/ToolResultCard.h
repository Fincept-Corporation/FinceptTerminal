#pragma once
// ToolResultCard.h — one tool call and its result as a card (WP5): the tool's
// name, a one-line summary of its arguments, its status (running / done /
// failed) and duration, and a collapsible result rendered by its shape — a
// table (model / view, sortable), key / value fields, text, or the error — with
// "Save as artefact".
//
// Used by the transcript (ChatMessagePanel, one card per tool call of the turn)
// and by the ACTIVITY tab's detail drawer (RunActivityView). The result body is
// built only when the card is first expanded. The cloud agent sends a shortened
// output preview; terminal tools carry the full JSON (RunEvent::data["result"]).

#include <QFrame>
#include <QJsonValue>
#include <QString>

#include <memory>

namespace fincept::chat_mode {

class ToolResultCard : public QFrame {
    Q_OBJECT
  public:
    /// `compact`: denser fonts for the transcript (the ACTIVITY drawer uses the default).
    explicit ToolResultCard(QWidget* parent = nullptr, bool compact = false);
    ~ToolResultCard() override;

    /// Whether `result` has a shape this card renders as a table or fields (rows of
    /// objects, a {columns, rows} object, a flat object). Cheap.
    static bool can_render(const QJsonValue& result);

    /// The call: tool name, its argument preview, and the run / step / event it
    /// belongs to (for "Save as artefact"; an empty run id hides the button).
    void set_call(const QString& tool, const QString& args_preview, const QString& run_id, int step,
                  const QString& ref_id);
    /// The result: `full` is the whole JSON when known (terminal tools), else
    /// Undefined and `preview` (the server's shortened output) is shown.
    void set_result(bool success, qint64 duration_ms, const QString& preview, const QJsonValue& full);
    /// Shorthand of the two above for a finished call without run context.
    void set_result(const QString& tool_name, const QJsonValue& result, bool success);

    void set_expanded(bool expanded);
    bool is_expanded() const;
    QString tool_name() const;
    QString ref_id() const;

  signals:
    /// The user asked to keep this result as a run artefact.
    void save_requested(const QString& tool_name, const QJsonValue& result);
    /// …and it was saved.
    void saved(const QString& artefact_id);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
