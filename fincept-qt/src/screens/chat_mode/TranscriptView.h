#pragma once
// TranscriptView.h — Chat Mode's transcript as a continuous document (2026-11-17).
//
// A block-document view: the transcript is a flat list of transcript::Items (see
// TranscriptModel.h) in a QAbstractScrollArea; only the items within (and a little
// around) the viewport have widgets. Heights of items never shown are estimated from
// font metrics; a realised item is measured (height-for-width) and the layout is
// corrected around a scroll anchor so the text being read does not move. Thousands
// of turns cost one prefix-sum walk, not thousands of widgets.
//
// Follows the tail while the reader is at the bottom; once they scroll up it stays
// put and a "↓ NEW" pill offers the way back. Links open only through the allow-list
// (http / https / mailto -> QDesktopServices); ticker chips and every button become
// action_requested() for the panel. Theme tokens only; one stylesheet on the view,
// rich-text CSS rebuilt for the realised blocks on ThemeManager::appearance_settled.

#include "screens/chat_mode/TranscriptModel.h"

#include <QAbstractScrollArea>

#include <memory>

namespace fincept::chat_mode {

class TranscriptView : public QAbstractScrollArea {
    Q_OBJECT
  public:
    explicit TranscriptView(QWidget* parent = nullptr);
    ~TranscriptView() override;

    // ── Turns ───────────────────────────────────────────────────────────────
    /// Append a turn and its header item; returns the turn index.
    int add_turn(const transcript::Turn& turn);
    transcript::Turn* turn(int index);
    int turn_count() const;
    /// The turn's header shows its new data (tokens, duration, actions).
    void update_turn(int index);

    // ── Items ───────────────────────────────────────────────────────────────
    /// Append (ids are assigned here; an item's `turn` is kept as given).
    quint64 append(const transcript::ItemPtr& item);
    /// Insert before `before` (appends when it is unknown).
    quint64 insert_before(quint64 before, const transcript::ItemPtr& item);
    /// A whole conversation at once (history): `turns` are appended and every item's
    /// `turn` (an index into `turns`) is remapped to the view's numbering.
    void append_history(const QVector<transcript::Turn>& turns, const QVector<transcript::ItemPtr>& items);
    transcript::Item* item(quint64 id) const;
    transcript::ItemPtr item_ptr(quint64 id) const;
    /// The item's content or state changed: its widget is refreshed and re-measured.
    void changed(quint64 id);
    void remove(quint64 id);
    void clear();
    bool is_empty() const;
    /// Text of the whole transcript as Markdown (export).
    QString to_markdown() const;

    // ── Approvals (disabled while a turn runs) ──────────────────────────────
    void set_approvals_enabled(bool enabled);
    bool approvals_enabled() const;

    // ── Scrolling ───────────────────────────────────────────────────────────
    bool following() const;
    /// Follow the tail again (and scroll there).
    void scroll_to_bottom();
    int scroll_value() const;
    void set_scroll_value(int value);
    /// Keyboard focus into the transcript (Alt+2).
    void focus_document();

  signals:
    void action_requested(const fincept::chat_mode::transcript::Action& action);
    void scroll_changed();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void scrollContentsBy(int dx, int dy) override;
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
