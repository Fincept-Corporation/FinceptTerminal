#pragma once

#include <QPointer>
#include <QPropertyAnimation>
#include <QWidget>

class QGraphicsOpacityEffect;

namespace fincept::screens::widgets {

/// Animated loading state shown on top of a widget's content area while data
/// streams in from the DataHub.
///
/// Two modes:
///   * Indeterminate — caller does not know the expected item count. The
///     overlay shows skeleton rows with a sweeping shimmer and a "LOADING…"
///     label. Use this when a widget receives its data in a single payload.
///   * Determinate — caller passes (loaded, expected). The overlay shows a
///     thin progress bar plus "LOADING X / Y ITEMS". When `loaded >=
///     expected`, the overlay fades out and hides itself. Use this for
///     widgets that fan out per-symbol / per-topic subscriptions where the
///     UI fills up incrementally.
///
/// The overlay reparents itself to a target widget (`attach_to`) and tracks
/// the target's geometry via an event filter so it always covers the
/// content area exactly. It draws its own background and content with
/// QPainter — no stylesheet on the overlay itself, so it composites cleanly
/// over whatever widgets the caller has placed underneath.
class LoadingOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal shimmerPhase READ shimmer_phase WRITE set_shimmer_phase)
    Q_PROPERTY(qreal displayedProgress READ displayed_progress WRITE set_displayed_progress)

  public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    /// Reparent the overlay to `target` and resize-track it so the overlay
    /// always matches the target's geometry. Pass nullptr to detach.
    void attach_to(QWidget* target);

    /// Begin showing the overlay in indeterminate mode (no item count).
    /// Idempotent — safe to call repeatedly.
    void start_indeterminate();

    /// Set determinate progress. Switches the overlay into determinate mode
    /// on first call, and auto-hides when `loaded >= expected` and
    /// `expected > 0`. If `expected <= 0`, behaves like
    /// `start_indeterminate()`.
    void set_progress(int loaded, int expected);

    /// Hide the overlay (with fade-out) and stop animations.
    void finish();

    bool is_active() const { return active_; }

    qreal shimmer_phase() const { return shimmer_phase_; }
    void set_shimmer_phase(qreal v);
    qreal displayed_progress() const { return displayed_progress_; }
    void set_displayed_progress(qreal v);

  protected:
    void paintEvent(QPaintEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

  private:
    void start_shimmer();
    void stop_shimmer();
    void fade_in();
    void fade_out_and_hide();
    void sync_geometry();

    QPointer<QWidget> target_;
    bool active_ = false;
    bool indeterminate_ = true;
    int loaded_ = 0;
    int expected_ = 0;

    qreal shimmer_phase_ = 0.0;        ///< 0..1 — drives the sweeping highlight
    qreal displayed_progress_ = 0.0;   ///< 0..1 — eased toward loaded_/expected_

    QPropertyAnimation* shimmer_anim_ = nullptr;
    QPropertyAnimation* progress_anim_ = nullptr;
    QPropertyAnimation* fade_anim_ = nullptr;
    QGraphicsOpacityEffect* opacity_effect_ = nullptr;
};

} // namespace fincept::screens::widgets
