// src/ui/widgets/FlowLayout.h
#pragma once
// ui::FlowLayout — lays its widgets out left to right and wraps to a new line when the
// width runs out (chips, filter buttons, tags, toolbars that must stay readable at every
// font size instead of being clipped). The shared copy of the per-screen flow layouts
// (RvFlowLayout, BtStripFlowLayout, BtResFlowLayout, notes / algo FlowLayout) — a drop-in
// superset:
//   * height-for-width (put it in a QVBoxLayout / QGridLayout and it grows to as many lines
//     as it needs); hidden widgets take no space;
//   * spacing from the font metrics of the parent widget by default (follows the base
//     font size), or fixed pixels like the old copies;
//   * right-to-left aware (mirrors each line), optional centred lines, per-item vertical
//     alignment inside a line via QLayout::setAlignment(widget, Qt::AlignVCenter);
//   * insert_widget(index, w) keeps e.g. an "add" input last;
//   * sizeHint() = everything on one line (the preferred width), or the widest item with
//     set_compact_hint(true) (the old copies' sizeHint == minimumSize).
// Margins default to 0 (setContentsMargins as usual). The geometry itself is the pure
// flow_layout::arrange() in FlowLayoutGeometry.h (unit-tested).
//
//   auto* chips = new QWidget(panel);
//   auto* flow = new fincept::ui::FlowLayout(chips);          // spacing from the font
//   for (const QString& t : tags) flow->addWidget(make_chip(t, chips));

#include <QLayout>
#include <QList>

namespace fincept::ui {

class FlowLayout : public QLayout {
  public:
    /// h_spacing / v_spacing < 0 = from the parent widget's font (default).
    explicit FlowLayout(QWidget* parent = nullptr, int h_spacing = -1, int v_spacing = -1);
    ~FlowLayout() override;

    // ── spacing / look ──────────────────────────────────────────────────────
    /// Pixels between items of a line / between lines; < 0 = from the font.
    void set_spacing(int h_spacing, int v_spacing);
    int horizontal_spacing() const; ///< effective (font-derived when not set)
    int vertical_spacing() const;
    /// QLayout::setSpacing sets both (QLayout::spacing() returns the horizontal one).
    void setSpacing(int spacing) override;
    int spacing() const override;
    /// Centre each line in the available width instead of starting it at the left.
    void set_centered(bool centered);
    bool centered() const { return centered_; }
    /// true: sizeHint() == minimumSize() (widest item); false (default): one line wide.
    void set_compact_hint(bool compact);

    /// Insert `w` before position `index` (clamped); addWidget() appends.
    void insert_widget(int index, QWidget* w);

    // ── QLayout ─────────────────────────────────────────────────────────────
    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void invalidate() override;

  private:
    /// Places the items in `rect` (margins applied); returns the height used incl. margins.
    int arrange(const QRect& rect, bool apply) const;
    bool right_to_left() const;

    QList<QLayoutItem*> items_;
    int h_spacing_ = -1;
    int v_spacing_ = -1;
    bool centered_ = false;
    bool compact_hint_ = false;
    mutable int cached_width_ = -1;
    mutable int cached_height_ = -1;
};

} // namespace fincept::ui
