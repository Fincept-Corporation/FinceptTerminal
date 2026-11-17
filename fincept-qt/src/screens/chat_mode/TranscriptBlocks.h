#pragma once
// TranscriptBlocks.h — the widgets of Chat Mode's transcript blocks (2026-11-17).
//
// One small widget per transcript::Item kind, created by TranscriptView only while the
// item is on (or near) the screen and destroyed when it scrolls far away. A block
// reads everything from its Item and writes its state back there (expanded, show
// all, …), so it can be rebuilt at any time. Plain QWidgets (no Q_OBJECT): buttons are
// wired with lambdas and talk to the view through BlockHost. Styling: the view's ONE
// stylesheet (transcript_sheet(), object-name selectors) plus rich-text CSS
// (rich_css()) inside the labels, rebuilt by restyle() after the appearance settled.

#include "screens/chat_mode/TranscriptModel.h"

#include <QWidget>

#include <functional>

namespace fincept::chat_mode::transcript {

/// What blocks need from their view (owned by the view; blocks never outlive it).
struct BlockHost {
    std::function<void(const Action&)> act;          ///< a request for the panel
    std::function<void(quint64)> relayout;           ///< the item's height changed (expanded, show all)
    std::function<void(quint64)> toggle_group;       ///< fold / unfold an ActivityGroup
    std::function<void(quint64)> classify;           ///< classify an Activity result on a worker
    std::function<const Turn*(int)> turn;
    std::function<bool()> approvals_enabled;
    std::function<int()> hovered_turn;
    std::function<void(const QString&)> open_link;   ///< allow-list + symbol chips
};

class Block : public QWidget {
  public:
    Block(const ItemPtr& item, const BlockHost* host, QWidget* parent);

    const ItemPtr& item() const { return item_; }
    /// Height at `width` (the block is laid out at that width).
    virtual int height_for(int width);
    /// The item's data or state changed.
    virtual void sync() {}
    /// The appearance settled: rich text carries inline colours — rebuild it.
    virtual void restyle() { sync(); }
    /// The mouse entered / left the block's turn (headers brighten their actions).
    virtual void set_hot(bool hot) { (void)hot; }

  protected:
    ItemPtr item_;
    const BlockHost* host_ = nullptr;
};

/// The widget for `item` (never null; an unknown kind becomes a note).
Block* create_block(const ItemPtr& item, const BlockHost* host, QWidget* parent);

/// Height of `item` at `width` from font metrics only (items never realised).
int estimate_height(Item& item, int width);
/// Forget the cached font metrics (the font changed).
void reset_metrics();

/// The view's stylesheet (object-name selectors for every block part).
QString transcript_sheet();
/// CSS for rich-text labels (links, ticker chips, code, nested tables).
QString rich_css();

/// The text of an Activity result as copied (CSV for tables).
QString result_text(const Item& item);

} // namespace fincept::chat_mode::transcript
