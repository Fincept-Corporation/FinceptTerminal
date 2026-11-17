#pragma once
// RunViewWidgets.h — small widget-side helpers shared by Chat Mode's run views
// (WP2): tone colours from the theme, a painted status pill, the executor of a
// run (to ask whether it can pause / is available), an event hook that lets a
// panel react to LanguageChange / Show / Hide without overriding its (fixed)
// header, and a wrapping flow layout for chips.
//
// Header-only, no Q_OBJECT (nothing for moc). Colours and fonts come from
// ui/theme only.

#include "screens/chat_mode/RunViewLogic.h"
#include "services/agent_tasks/IRunExecutor.h"
#include "services/agent_tasks/TaskRunService.h"
#include "services/llm/ModelRouter.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLayout>
#include <QList>
#include <QPainter>
#include <QPointer>
#include <QRect>
#include <QStyle>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <utility>

namespace fincept::chat_mode::run_view {

// ── Tones → theme colours ────────────────────────────────────────────────────

inline QColor tone_color(Tone t) {
    switch (t) {
        case Tone::Info:
            return QColor(ui::colors::INFO());
        case Tone::Success:
            return QColor(ui::colors::SUCCESS());
        case Tone::Warning:
            return QColor(ui::colors::WARNING());
        case Tone::Danger:
            return QColor(ui::colors::DANGER());
        case Tone::Neutral:
            break;
    }
    return QColor(ui::colors::TEXT_SECONDARY());
}

/// An opaque fill washed with the tone (dark-tinted on dark themes, pastel on light).
inline QColor tone_fill(Tone t, double strength = 0.16) {
    return ui::colors::tint_color(tone_color(t), strength);
}

// ── Pills ────────────────────────────────────────────────────────────────────

/// Horizontal padding of a pill on each side of its text.
inline int pill_padding(const QFontMetrics& fm) {
    return std::max(4, fm.averageCharWidth());
}

inline int pill_width(const QFontMetrics& fm, const QString& text) {
    return fm.horizontalAdvance(text) + 2 * pill_padding(fm);
}

inline int pill_height(const QFontMetrics& fm) {
    return fm.height() + 2;
}

/// Paints a status pill (tinted fill, tone border and text) with its top-left at
/// `at`; returns its width.
inline int paint_pill(QPainter* p, const QPoint& at, const QString& text, Tone tone, const QFont& font) {
    const QFontMetrics fm(font);
    const QRect r(at, QSize(pill_width(fm, text), pill_height(fm)));
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    const QColor c = tone_color(tone);
    p->setPen(QPen(c, 1));
    p->setBrush(tone_fill(tone));
    p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
    p->setFont(font);
    p->setPen(c);
    p->drawText(r, Qt::AlignCenter, text);
    p->restore();
    return r.width();
}

/// A pill as a QLabel stylesheet (status bar, plan header).
inline QString pill_css(Tone tone, const QString& font_px) {
    const QColor c = tone_color(tone);
    return QStringLiteral("QLabel{color:%1;background:%2;border:1px solid %1;border-radius:2px;padding:0 6px;"
                          "font-size:%3px;font-weight:700;}")
        .arg(c.name(), tone_fill(tone).name(), font_px);
}

// ── Executors ────────────────────────────────────────────────────────────────

/// The registered executor with engine_id() `executor_id` (TaskRunService owns
/// one per engine as its children), or null.
inline agent_tasks::IRunExecutor* find_executor(const QString& executor_id) {
    const auto list =
        agent_tasks::TaskRunService::instance().findChildren<agent_tasks::IRunExecutor*>(Qt::FindDirectChildrenOnly);
    for (agent_tasks::IRunExecutor* ex : list)
        if (ex && ex->engine_id() == executor_id)
            return ex;
    return nullptr;
}

/// IRunExecutor::can_pause() of the run's executor (false when it has none).
inline bool executor_can_pause(const agent_tasks::TaskRun& r) {
    return agent_tasks::TaskRunService::instance().can_pause(r.id);
}

/// Dynamic property an executor may set to false to say it is not implemented in
/// this build (a skeleton that ends every run with "not available in this build
/// yet"). TaskRunService has no such query; until it has one, a registered
/// executor without the property counts as available.
inline constexpr const char* kExecutorAvailableProperty = "available";

/// Whether runs of `executor_id` can run in this build: the executor is
/// registered with TaskRunService and does not set kExecutorAvailableProperty to
/// false. `why` gets the reason when not.
inline bool executor_available(const QString& executor_id, QString* why = nullptr) {
    agent_tasks::IRunExecutor* ex =
        agent_tasks::TaskRunService::instance().has_executor(executor_id) ? find_executor(executor_id) : nullptr;
    bool ok = false;
    if (ex) {
        const QVariant flag = ex->property(kExecutorAvailableProperty);
        ok = !flag.isValid() || flag.toBool();
    }
    if (!ok && why) {
        if (executor_id == QLatin1String(agent_tasks::kExecutorCloudPlan))
            *why = rv_tr("Multi-step cloud runs are not available in this build yet.");
        else if (executor_id == QLatin1String(agent_tasks::kExecutorLocal))
            *why = rv_tr("Local agent runs are not available in this build yet.");
        else
            *why = rv_tr("This engine is not available in this build.");
    }
    return ok;
}

/// The "Harness" engine choice as it is now: the harness executor is registered and
/// ModelRouter's route for Chat Mode harness tasks holds a harness. Cheap (the
/// router answers from its snapshot); re-ask on ModelRouter::routes_changed.
inline HarnessEngineOption harness_engine_option_now() {
    return harness_engine_option(
        agent_tasks::TaskRunService::instance().has_executor(QString::fromLatin1(kHarnessEngineId)),
        ai_chat::ModelRouter::instance().route(QString::fromLatin1(ai_chat::kFeatureChatModeHarness)));
}

/// "0 credits · billed to your Anthropic API key" for an enabled harness choice.
inline QString harness_billing_note(const HarnessEngineOption& o) {
    return o.enabled ? ai_chat::ModelRouter::billing_note(o.choice) : QString();
}

// ── Event hook ───────────────────────────────────────────────────────────────

/// Calls `fn` with LanguageChange / Show / Hide / Resize events of `target`
/// (child object of the target; removed with it). Lets a panel react to them
/// without overriding changeEvent / showEvent in its header.
class RvEventHook : public QObject {
  public:
    RvEventHook(QWidget* target, std::function<void(QEvent::Type)> fn) : QObject(target), fn_(std::move(fn)) {
        target->installEventFilter(this);
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* e) override {
        if (watched == parent() && fn_) {
            const QEvent::Type t = e->type();
            if (t == QEvent::LanguageChange || t == QEvent::Show || t == QEvent::Hide || t == QEvent::Resize)
                fn_(t);
        }
        return QObject::eventFilter(watched, e);
    }

  private:
    std::function<void(QEvent::Type)> fn_;
};

/// Keys for `target` handled in an event filter instead of QShortcuts: a
/// window-wide shortcut on the same key elsewhere (the composer's Esc = Detach)
/// would make a QShortcut ambiguous and silence both. `fn(event, apply)` says
/// whether the key is ours; with apply = true it also acts. A key that is ours
/// is accepted at ShortcutOverride, so window shortcuts do not fire for it.
class RvKeyHook : public QObject {
  public:
    using Fn = std::function<bool(const QKeyEvent* event, bool apply)>;
    RvKeyHook(QWidget* target, Fn fn) : QObject(target), fn_(std::move(fn)) { target->installEventFilter(this); }

  protected:
    bool eventFilter(QObject* watched, QEvent* e) override {
        if (watched == parent() && fn_ && (e->type() == QEvent::ShortcutOverride || e->type() == QEvent::KeyPress)) {
            const auto* ke = static_cast<QKeyEvent*>(e);
            if (fn_(ke, e->type() == QEvent::KeyPress)) {
                e->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, e);
    }

  private:
    Fn fn_;
};

// ── Flow layout ──────────────────────────────────────────────────────────────

/// Lays its items out left to right and wraps to a new line when the width runs
/// out — chips and filter buttons stay readable at every font size instead of
/// being clipped. Hidden widgets take no space.
class RvFlowLayout : public QLayout {
  public:
    explicit RvFlowLayout(QWidget* parent = nullptr, int h_spacing = 6, int v_spacing = 4)
        : QLayout(parent), h_spacing_(h_spacing), v_spacing_(v_spacing) {
        setContentsMargins(0, 0, 0, 0);
    }
    ~RvFlowLayout() override {
        while (QLayoutItem* item = takeAt(0))
            delete item;
    }

    /// Centre each line in the available width instead of starting it at the left.
    void set_centered(bool centered) {
        centered_ = centered;
        invalidate();
    }

    void addItem(QLayoutItem* item) override { items_.append(item); }
    int count() const override { return static_cast<int>(items_.size()); }
    QLayoutItem* itemAt(int index) const override { return items_.value(index); }
    QLayoutItem* takeAt(int index) override {
        return (index >= 0 && index < items_.size()) ? items_.takeAt(index) : nullptr;
    }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return arrange(QRect(0, 0, width, 0), true); }
    void setGeometry(const QRect& rect) override {
        QLayout::setGeometry(rect);
        arrange(rect, false);
    }
    QSize sizeHint() const override {
        // One line wide; the parent layout asks heightForWidth for the real height.
        int w = 0;
        int h = 0;
        for (QLayoutItem* item : items_) {
            if (item->isEmpty())
                continue;
            const QSize s = item->sizeHint();
            w += s.width() + (w > 0 ? h_spacing_ : 0);
            h = std::max(h, s.height());
        }
        const QMargins m = contentsMargins();
        return QSize(w + m.left() + m.right(), h + m.top() + m.bottom());
    }
    QSize minimumSize() const override {
        QSize s;
        for (QLayoutItem* item : items_)
            if (!item->isEmpty())
                s = s.expandedTo(item->minimumSize());
        const QMargins m = contentsMargins();
        return s + QSize(m.left() + m.right(), m.top() + m.bottom());
    }

  private:
    int arrange(const QRect& rect, bool test_only) const {
        const QMargins m = contentsMargins();
        const QRect area = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
        int x = area.x();
        int y = area.y();
        int line_h = 0;
        QList<std::pair<QLayoutItem*, QRect>> line; // the line being filled (placed when it is complete)
        const auto place_line = [&]() {
            if (test_only || line.isEmpty())
                return;
            // x is one spacing past the last item of the line.
            const int shift = centered_ ? std::max(0, (area.right() + 1 - (x - h_spacing_)) / 2) : 0;
            for (const auto& entry : std::as_const(line))
                entry.first->setGeometry(entry.second.translated(shift, 0));
            line.clear();
        };
        for (QLayoutItem* item : items_) {
            if (item->isEmpty())
                continue;
            const QSize s = item->sizeHint();
            int next_x = x + s.width() + h_spacing_;
            if (next_x - h_spacing_ > area.right() + 1 && line_h > 0) {
                place_line();
                x = area.x();
                y += line_h + v_spacing_;
                next_x = x + s.width() + h_spacing_;
                line_h = 0;
            }
            if (!test_only)
                line.append({item, QRect(QPoint(x, y), QSize(std::min(s.width(), std::max(1, area.width())), s.height()))});
            x = next_x;
            line_h = std::max(line_h, s.height());
        }
        place_line();
        return y + line_h - rect.y() + m.bottom();
    }

    QList<QLayoutItem*> items_;
    int h_spacing_;
    int v_spacing_;
    bool centered_ = false;
};

} // namespace fincept::chat_mode::run_view
