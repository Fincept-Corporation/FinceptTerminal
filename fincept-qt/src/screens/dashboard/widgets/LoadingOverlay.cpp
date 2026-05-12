#include "screens/dashboard/widgets/LoadingOverlay.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include <algorithm>

namespace fincept::screens::widgets {

namespace {

constexpr int kShimmerPeriodMs = 1200;
constexpr int kFadeMs = 220;
constexpr int kProgressEaseMs = 380;

QColor with_alpha(const QString& css, int alpha) {
    QColor c(css);
    c.setAlpha(alpha);
    return c;
}

} // namespace

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);

    // QGraphicsOpacityEffect was previously used here. Removed: on Qt6 the
    // effect forces an offscreen render pass + composite on every paint even
    // when opacity is 1.0, which adds ~ms per frame per overlay. With ~30
    // dashboard widgets each owning an overlay this dominated paint time on
    // low-end GPUs. We now apply opacity directly via QPainter::setOpacity
    // in paintEvent and drive it through the `fadeOpacity` Q_PROPERTY.

    shimmer_anim_ = new QPropertyAnimation(this, "shimmerPhase", this);
    shimmer_anim_->setDuration(kShimmerPeriodMs);
    shimmer_anim_->setStartValue(0.0);
    shimmer_anim_->setEndValue(1.0);
    shimmer_anim_->setLoopCount(-1);

    progress_anim_ = new QPropertyAnimation(this, "displayedProgress", this);
    progress_anim_->setDuration(kProgressEaseMs);
    progress_anim_->setEasingCurve(QEasingCurve::OutCubic);

    fade_anim_ = new QPropertyAnimation(this, "fadeOpacity", this);
    fade_anim_->setDuration(kFadeMs);

    // setVisible(false) dispatches a QHideEvent → hideEvent() → stop_shimmer(),
    // so the animations must exist before this call. Keep it (we need
    // WA_WState_ExplicitShowHide set so reparenting onto a visible target
    // doesn't auto-show the overlay).
    setVisible(false);

    connect(fade_anim_, &QPropertyAnimation::finished, this, [this]() {
        // If we faded out, actually hide so we stop drawing on top.
        if (fade_opacity_ <= 0.001) {
            setVisible(false);
            stop_shimmer();
        }
    });

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

void LoadingOverlay::attach_to(QWidget* target) {
    if (target_ == target)
        return;
    if (target_)
        target_->removeEventFilter(this);
    target_ = target;
    if (!target) {
        setParent(nullptr);
        return;
    }
    setParent(target);
    target->installEventFilter(this);
    sync_geometry();
    raise();
}

void LoadingOverlay::set_shimmer_phase(qreal v) {
    shimmer_phase_ = v;
    update();
}

void LoadingOverlay::set_displayed_progress(qreal v) {
    displayed_progress_ = v;
    update();
}

void LoadingOverlay::set_fade_opacity(qreal v) {
    fade_opacity_ = v;
    update();
}

void LoadingOverlay::start_indeterminate() {
    indeterminate_ = true;
    loaded_ = 0;
    expected_ = 0;
    error_mode_ = false;
    error_text_.clear();
    progress_anim_->stop();
    displayed_progress_ = 0.0;

    if (!active_) {
        active_ = true;
        sync_geometry();
        setVisible(true);
        raise();
        fade_in();
    }
    start_shimmer();
    update();
}

void LoadingOverlay::set_progress(int loaded, int expected) {
    if (expected <= 0) {
        start_indeterminate();
        return;
    }

    indeterminate_ = false;
    error_mode_ = false;
    error_text_.clear();
    loaded_ = std::clamp(loaded, 0, expected);
    expected_ = expected;

    if (!active_ && loaded_ < expected_) {
        active_ = true;
        sync_geometry();
        setVisible(true);
        raise();
        fade_in();
        start_shimmer();
    }

    const qreal target = static_cast<qreal>(loaded_) / static_cast<qreal>(expected_);
    progress_anim_->stop();
    progress_anim_->setStartValue(displayed_progress_);
    progress_anim_->setEndValue(target);
    progress_anim_->start();

    if (loaded_ >= expected_)
        finish();
    else
        update();
}

void LoadingOverlay::finish() {
    error_mode_ = false;
    error_text_.clear();
    if (!active_)
        return;
    active_ = false;
    fade_out_and_hide();
}

void LoadingOverlay::set_error(const QString& message) {
    error_mode_ = true;
    error_text_ = message;
    stop_shimmer();
    progress_anim_->stop();
    if (!active_) {
        active_ = true;
        sync_geometry();
        setVisible(true);
        raise();
        fade_in();
    }
    update();
}

void LoadingOverlay::start_shimmer() {
    if (shimmer_anim_->state() != QAbstractAnimation::Running)
        shimmer_anim_->start();
}

void LoadingOverlay::stop_shimmer() {
    shimmer_anim_->stop();
}

void LoadingOverlay::fade_in() {
    fade_anim_->stop();
    fade_anim_->setStartValue(fade_opacity_);
    fade_anim_->setEndValue(1.0);
    fade_anim_->start();
}

void LoadingOverlay::fade_out_and_hide() {
    fade_anim_->stop();
    fade_anim_->setStartValue(fade_opacity_);
    fade_anim_->setEndValue(0.0);
    fade_anim_->start();
}

void LoadingOverlay::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Resume the shimmer animation when our parent widget tree becomes
    // visible again (e.g. user switches back to the dashboard tab). Without
    // this the QPropertyAnimation central timer would keep ticking ~60Hz on
    // every overlay regardless of visibility — 30+ overlays × 60Hz is
    // measurable on weak hardware.
    if (active_ && !error_mode_)
        start_shimmer();
}

void LoadingOverlay::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    stop_shimmer();
}

void LoadingOverlay::sync_geometry() {
    if (target_)
        setGeometry(target_->rect());
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* e) {
    if (obj == target_.data()) {
        if (e->type() == QEvent::Resize || e->type() == QEvent::Show) {
            sync_geometry();
            if (active_)
                raise();
        }
    }
    return QWidget::eventFilter(obj, e);
}

void LoadingOverlay::paintEvent(QPaintEvent* /*e*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    // Apply our fade opacity directly via the painter (was previously a
    // QGraphicsOpacityEffect — see comment in the constructor).
    p.setOpacity(fade_opacity_);

    const QRect r = rect();
    if (r.isEmpty())
        return;

    // ── Backdrop ──
    // Mostly opaque so the underlying half-rendered widget doesn't bleed
    // through and look like a glitch — but tinted with the surface color so
    // the loading state still feels like part of the widget.
    p.fillRect(r, with_alpha(ui::colors::BG_SURFACE(), 235));

    // ── Error mode ──
    if (error_mode_) {
        QFont err_font;
        err_font.setFamily(ui::fonts::DATA_FAMILY);
        err_font.setPixelSize(11);
        err_font.setBold(true);
        err_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
        p.setFont(err_font);
        p.setPen(QColor(ui::colors::NEGATIVE()));
        const QRect text_rect = r.adjusted(12, 12, -12, -12);
        p.drawText(text_rect, Qt::AlignCenter | Qt::TextWordWrap, error_text_);
        return;
    }

    // ── Skeleton bars ──
    // Three rows of varying widths simulate where content will appear. The
    // shimmer sweeps across these, not across the full widget background.
    const QColor skel_bg = with_alpha(ui::colors::BORDER_DIM(), 170);
    const QColor accent = QColor(ui::colors::AMBER());
    const QColor accent_dim = with_alpha(ui::colors::AMBER(), 70);

    const int margin_x = 12;
    const int bar_h = 8;
    const int row_gap = 10;
    const int center_y = r.center().y();
    const int progress_block_h = 56;  // text + progress bar reserved
    const int top = std::max(r.top() + 12, center_y - progress_block_h / 2 - bar_h * 3 - row_gap * 2);

    struct SkelRow {
        qreal width_pct;
    };
    const SkelRow rows[] = {{1.0}, {0.85}, {0.62}};
    for (int i = 0; i < 3; ++i) {
        const int row_w = static_cast<int>((r.width() - 2 * margin_x) * rows[i].width_pct);
        const QRect row(margin_x, top + i * (bar_h + row_gap), row_w, bar_h);
        QPainterPath path;
        path.addRoundedRect(row, bar_h / 2.0, bar_h / 2.0);
        p.fillPath(path, skel_bg);

        // Shimmer — a soft moving highlight on each row, offset so they
        // don't all flash in lockstep.
        const qreal phase = std::fmod(shimmer_phase_ + i * 0.15, 1.0);
        const qreal sweep_w = row.width() * 0.35;
        const qreal sweep_x = row.left() - sweep_w + (row.width() + sweep_w) * phase;

        QLinearGradient grad(sweep_x, 0, sweep_x + sweep_w, 0);
        grad.setColorAt(0.0, QColor(0, 0, 0, 0));
        grad.setColorAt(0.5, accent_dim);
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.save();
        p.setClipPath(path);
        p.fillRect(row.adjusted(-int(sweep_w), 0, int(sweep_w), 0), grad);
        p.restore();
    }

    // ── Status text + progress bar ──
    const int text_y = top + 3 * (bar_h + row_gap) + 8;
    QFont label_font;
    label_font.setFamily(ui::fonts::DATA_FAMILY);
    label_font.setPixelSize(11);
    label_font.setBold(true);
    label_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    p.setFont(label_font);

    QString status_text;
    if (indeterminate_ || expected_ <= 0) {
        // Animate "LOADING" with a pulse on the trailing dots so the user
        // sees motion even before any item count is known.
        const int dots = static_cast<int>(shimmer_phase_ * 4) % 4;
        QString trail(dots, QChar('.'));
        status_text = QStringLiteral("LOADING%1").arg(trail);
    } else {
        // Display the *animated* count rather than the raw one — this is
        // why displayed_progress_ exists: a watchlist filling 8 of 8
        // symbols animates 0→1→2→…→8 instead of jumping.
        const int shown = static_cast<int>(std::round(displayed_progress_ * expected_));
        status_text = QStringLiteral("LOADING %1 / %2 ITEMS").arg(shown).arg(expected_);
    }

    p.setPen(QColor(ui::colors::TEXT_SECONDARY()));
    QFontMetrics fm(label_font);
    const int text_w = fm.horizontalAdvance(status_text);
    const int text_h = fm.height();
    p.drawText(QRect((r.width() - text_w) / 2, text_y, text_w, text_h), Qt::AlignCenter, status_text);

    // Progress bar — drawn underneath the text. In indeterminate mode it's
    // a moving block; in determinate mode it fills based on
    // displayed_progress_.
    const int bar_y = text_y + text_h + 6;
    const int bar_full_w = r.width() - 2 * margin_x;
    const int bar_track_h = 3;
    const QRect track(margin_x, bar_y, bar_full_w, bar_track_h);
    QPainterPath track_path;
    track_path.addRoundedRect(track, bar_track_h / 2.0, bar_track_h / 2.0);
    p.fillPath(track_path, with_alpha(ui::colors::BORDER_DIM(), 200));

    p.save();
    p.setClipPath(track_path);
    if (indeterminate_ || expected_ <= 0) {
        const qreal block_w = bar_full_w * 0.30;
        const qreal x = track.left() - block_w + (track.width() + block_w) * shimmer_phase_;
        QLinearGradient grad(x, 0, x + block_w, 0);
        grad.setColorAt(0.0, QColor(0, 0, 0, 0));
        grad.setColorAt(0.5, accent);
        grad.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(QRectF(x, track.top(), block_w, track.height()), grad);
    } else {
        const int fill_w = static_cast<int>(bar_full_w * displayed_progress_);
        QLinearGradient grad(track.left(), 0, track.left() + fill_w, 0);
        grad.setColorAt(0.0, accent_dim);
        grad.setColorAt(1.0, accent);
        p.fillRect(QRect(track.left(), track.top(), fill_w, track.height()), grad);
    }
    p.restore();
}

} // namespace fincept::screens::widgets
