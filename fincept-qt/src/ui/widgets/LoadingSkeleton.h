#pragma once
// LoadingSkeleton — placeholder blocks in the shape of the content that is on
// its way (KPI tiles, a chart, table rows, cards, text), with a soft shimmer,
// so a pane that is loading is never blank.
//
//   auto* sk = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Result, pane);
//   sk->set_message(tr("Pricing the bond…"));
//   stack->addWidget(sk);            // show it while the work runs,
//   stack->setCurrentWidget(sk);     // swap to the real content when it lands
//
// Fully custom-painted from the live theme (nothing cached, no stylesheet), so
// theme and font changes need no handling. The animation timer runs only
// while the widget is visible and motion is enabled ("Enable Animations" in
// Settings > Appearance, see motion_enabled()); it costs one small repaint
// every ~60 ms. With motion off the blocks are painted still.
//
// Ghost mode (set_ghost) paints a faint, still copy of the shape with no
// caption — the illustration behind an EmptyState preview.

#include <QString>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

/// Loading placeholders (LoadingSkeleton, LoadingOverlay) animate only when
/// this is true: the "Enable Animations" setting (appearance.animations), read
/// once, kept current by Settings > Appearance through set_motion_enabled().
bool motion_enabled();
void set_motion_enabled(bool on);

class LoadingSkeleton : public QWidget {
    Q_OBJECT
  public:
    /// Result: a KPI row, a chart and a few table rows (analytics results).
    /// Table: a header and rows. Chart: one plot area. Cards: a grid of cards.
    /// Text: paragraph lines. Form: label / field pairs. List (since 2026-11-17
    /// round 4): list items — a leading mark, a title over a sub-line, a short
    /// value at the right (news, watch lists, search results, series pickers).
    enum class Shape { Result, Table, Chart, Cards, Text, Form, List };

    explicit LoadingSkeleton(QWidget* parent = nullptr);
    explicit LoadingSkeleton(Shape shape, QWidget* parent = nullptr);

    void set_shape(Shape shape);
    Shape shape() const { return shape_; }
    /// Caption above the blocks ("COMPUTING DCF…"). Empty hides it.
    void set_message(const QString& message);
    QString message() const { return message_; }
    /// Number of KPI tiles in Result (default 4) and rows in Result/Table (default 6).
    void set_kpi_count(int n);
    void set_row_count(int n);
    /// Paint a background fill (default) or let the parent show through.
    void set_opaque(bool opaque);
    /// Animate while visible (default, subject to motion_enabled()) or paint still.
    void set_animated(bool animated);
    /// Faint, still, caption-less copy of the shape (an illustration). Implies
    /// set_animated(false) and a transparent background.
    void set_ghost(bool ghost);
    /// Progress under the caption (since 2026-11-17 round 6): `fraction` 0..1
    /// draws a determinate bar in place of the moving segment — also with motion
    /// off (it is not an animation); a negative / NaN fraction keeps the
    /// indeterminate line. `text` ("Downloading 3 of 12 series") is drawn under
    /// the bar; empty = none. clear_progress() = back to indeterminate, no text.
    void set_progress(double fraction, const QString& text = QString());
    void clear_progress();
    double progress() const { return progress_; } ///< < 0 = indeterminate
    QString progress_text() const { return progress_text_; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

  protected:
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    QTimer timer_;
    Shape shape_ = Shape::Result;
    QString message_;
    int kpis_ = 4;
    int rows_ = 6;
    int phase_ = 0;
    bool opaque_ = true;
    bool animated_ = true;
    bool ghost_ = false;
    // Since round 6.
    double progress_ = -1;
    QString progress_text_;
    void update_accessible_description();
};

} // namespace fincept::ui
