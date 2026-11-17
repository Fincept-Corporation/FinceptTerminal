// RunPlanPanel.cpp — the PLAN tab of Chat Mode's inspector (WP2).
//
// Header (title, status pill, engine, elapsed, live activity), what the run
// waits for (with a way to its conversation), why it stopped, the budget line,
// the controls (pause / resume / cancel / retry from a step — TaskRunService
// calls, never blocking; cancel confirms inline) and the steps: a model / view
// list painted by a delegate (status, title, short summary, credits, duration;
// the running step highlighted). A run that is one cloud reply shows a compact
// "single reply" block instead of a one-row plan.
//
// Safe when the run is deleted or the service resets while it is shown: every
// refresh re-reads the run from TaskRunService and handles its absence.

#include "screens/chat_mode/RunPlanPanel.h"

#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"

#include <QAbstractListModel>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;

namespace run_plan_panel_detail {

constexpr int kRppTickMs = 1000;
constexpr int kRppSummaryChars = 600;
constexpr int kRppSummaryLines = 2;

agt::TaskRunService& rpp_svc() {
    return agt::TaskRunService::instance();
}

qint64 rpp_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString rpp_engine_text(const agt::TaskRun& r) {
    const QString ex = r.executor_id();
    QString text;
    if (ex == QLatin1String(agt::kExecutorCloudTurn))
        text = RunPlanPanel::tr("Cloud · one reply");
    else if (ex == QLatin1String(agt::kExecutorCloudPlan))
        text = RunPlanPanel::tr("Cloud · multi-step");
    else if (ex == QLatin1String(agt::kExecutorLocal))
        text = RunPlanPanel::tr("Local agent");
    else
        text = r.engine;
    if (r.engine == QLatin1String(agt::kEngineCloud) && r.mode == QLatin1String("deep"))
        text += RunPlanPanel::tr(" · deep");
    if (r.is_unattended())
        text += RunPlanPanel::tr(" · scheduled");
    return text;
}

/// Up to `max_lines` lines of `text` wrapped at `width`; the last one elided.
QStringList rpp_wrap(const QString& text, const QFont& font, int width, int max_lines) {
    QStringList out;
    const QString t = text.simplified();
    if (t.isEmpty() || width <= 10 || max_lines <= 0)
        return out;
    QTextLayout layout(t, font);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        if (out.size() == max_lines - 1) {
            out << QFontMetrics(font).elidedText(t.mid(line.textStart()), Qt::ElideRight, width);
            break;
        }
        out << t.mid(line.textStart(), line.textLength()).trimmed();
    }
    layout.endLayout();
    return out;
}

// ── Steps model ──────────────────────────────────────────────────────────────

class RppStepModel : public QAbstractListModel {
  public:
    explicit RppStepModel(QObject* parent) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(steps_.size());
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= steps_.size())
            return {};
        const agt::RunStep& s = steps_.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
                return s.title;
            case Qt::ToolTipRole: {
                QStringList lines{RunPlanPanel::tr("Step %1: %2").arg(s.idx + 1).arg(s.title),
                                  agt::step_status_label(s.status)};
                if (!s.instruction.isEmpty())
                    lines << s.instruction.left(600);
                if (!s.summary.isEmpty())
                    lines << QString() << s.summary.left(1200);
                return lines.join(QLatin1Char('\n'));
            }
            case Qt::AccessibleTextRole:
                return RunPlanPanel::tr("Step %1, %2, %3").arg(s.idx + 1).arg(s.title, agt::step_status_label(s.status));
            default:
                return {};
        }
    }

    const agt::RunStep* step_at(int row) const { return (row >= 0 && row < steps_.size()) ? &steps_.at(row) : nullptr; }

    /// Replace the steps; keeps rows (and the selection) when the count is
    /// unchanged. Returns true when row heights may have changed (rows added or
    /// removed, or a summary changed) — the view then needs a relayout.
    bool set_steps(const QVector<agt::RunStep>& steps) {
        if (steps.size() == steps_.size()) {
            bool heights = false;
            for (int i = 0; i < steps.size() && !heights; ++i)
                heights = steps.at(i).summary != steps_.at(i).summary;
            steps_ = steps;
            if (!steps_.isEmpty())
                emit dataChanged(index(0), index(static_cast<int>(steps_.size()) - 1));
            return heights;
        }
        beginResetModel();
        steps_ = steps;
        endResetModel();
        return true;
    }

    void repaint_all() {
        if (!steps_.isEmpty())
            emit dataChanged(index(0), index(static_cast<int>(steps_.size()) - 1));
    }

  private:
    QVector<agt::RunStep> steps_;
};

class RppStepDelegate : public QStyledItemDelegate {
  public:
    explicit RppStepDelegate(QObject* parent) : QStyledItemDelegate(parent) { refresh_fonts(); }

    void refresh_fonts() {
        title_font_ = ui::fonts::font(-2, QFont::DemiBold);
        body_font_ = ui::fonts::font(-3);
        meta_font_ = ui::fonts::font(-4);
        pill_font_ = ui::fonts::font(-5, QFont::Bold);
        badge_font_ = ui::fonts::font(-4, QFont::Bold);
        title_h_ = std::max(QFontMetrics(title_font_).height(), rv::pill_height(QFontMetrics(pill_font_)));
        body_h_ = QFontMetrics(body_font_).lineSpacing();
        meta_h_ = QFontMetrics(meta_font_).height();
        badge_d_ = QFontMetrics(badge_font_).height() + 4;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* m = static_cast<const RppStepModel*>(index.model());
        const agt::RunStep* s = m ? m->step_at(index.row()) : nullptr;
        int width = option.rect.width();
        if (const auto* view = qobject_cast<const QAbstractItemView*>(parent()))
            width = view->viewport()->width();
        int h = kPad + std::max(title_h_, badge_d_);
        if (s) {
            const int lines = static_cast<int>(
                rpp_wrap(rv::markdown_preview(s->summary.left(kRppSummaryChars)), body_font_, text_width(width),
                         kRppSummaryLines)
                    .size());
            h += lines * body_h_;
        }
        h += 2 + meta_h_ + kPad + 1;
        return QSize(width, h);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto* m = static_cast<const RppStepModel*>(index.model());
        const agt::RunStep* s = m ? m->step_at(index.row()) : nullptr;
        const QRect rect = option.rect;
        p->save();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool running = s && s->status == agt::StepStatus::Running;
        QColor bg(ui::colors::BG_SURFACE());
        if (selected)
            bg = QColor(ui::colors::BG_HOVER());
        else if (running)
            bg = rv::tone_fill(rv::Tone::Info, 0.10);
        else if (hovered)
            bg = QColor(ui::colors::BG_RAISED());
        p->fillRect(rect, bg);
        if (!s) {
            p->restore();
            return;
        }
        if (running)
            p->fillRect(QRect(rect.left(), rect.top(), 3, rect.height()), rv::tone_color(rv::Tone::Info));
        else if (selected)
            p->fillRect(QRect(rect.left(), rect.top(), 2, rect.height()), QColor(ui::colors::AMBER()));

        const rv::Tone tone = rv::step_status_tone(s->status);
        const QColor tc = rv::tone_color(tone);
        const int left = rect.left() + 8;
        const int right = rect.right() - 8;
        const int top = rect.top() + kPad;

        // Badge: the step number, or ✓ / ✗ / – once it ended.
        const QRect badge(left, top + (std::max(title_h_, badge_d_) - badge_d_) / 2, badge_d_, badge_d_);
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(tc, 1));
        p->setBrush(s->status == agt::StepStatus::Pending ? QBrush(Qt::NoBrush) : QBrush(rv::tone_fill(tone, 0.22)));
        p->drawEllipse(QRectF(badge).adjusted(0.5, 0.5, -0.5, -0.5));
        QString glyph = QString::number(s->idx + 1);
        if (s->status == agt::StepStatus::Done)
            glyph = QStringLiteral("✓");
        else if (s->status == agt::StepStatus::Failed)
            glyph = QStringLiteral("✗");
        else if (s->status == agt::StepStatus::Skipped)
            glyph = QStringLiteral("–");
        p->setFont(badge_font_);
        p->setPen(s->status == agt::StepStatus::Pending ? QColor(ui::colors::TEXT_TERTIARY()) : tc);
        p->drawText(badge, Qt::AlignCenter, glyph);
        p->setRenderHint(QPainter::Antialiasing, false);

        // Pill on the right, title between.
        const QFontMetrics pfm(pill_font_);
        const QString pill = rv::step_status_pill(s->status);
        const int pill_w = rv::pill_width(pfm, pill);
        rv::paint_pill(p, QPoint(right - pill_w, top + (std::max(title_h_, badge_d_) - rv::pill_height(pfm)) / 2), pill,
                       tone, pill_font_);
        const int tx = badge.right() + 7;
        const int tw = std::max(10, right - pill_w - 6 - tx);
        p->setFont(title_font_);
        p->setPen(QColor(s->status == agt::StepStatus::Pending || s->status == agt::StepStatus::Skipped
                             ? ui::colors::TEXT_SECONDARY()
                             : ui::colors::TEXT_PRIMARY()));
        const QString title = s->title.isEmpty() ? RunPlanPanel::tr("Step %1").arg(s->idx + 1) : s->title;
        p->drawText(QRect(tx, top, tw, std::max(title_h_, badge_d_)), Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(title_font_).elidedText(title, Qt::ElideRight, tw));

        int y = top + std::max(title_h_, badge_d_);
        const int body_w = text_width(rect.width());
        const QStringList lines =
            rpp_wrap(rv::markdown_preview(s->summary.left(kRppSummaryChars)), body_font_, body_w, kRppSummaryLines);
        p->setFont(body_font_);
        p->setPen(s->status == agt::StepStatus::Failed ? tc : QColor(ui::colors::TEXT_SECONDARY()));
        for (const QString& line : lines) {
            p->drawText(QRect(tx, y, body_w, body_h_), Qt::AlignLeft | Qt::AlignVCenter, line);
            y += body_h_;
        }

        QStringList meta;
        const qint64 ms = rv::step_elapsed_ms(*s, rpp_now_ms());
        if (ms >= 0)
            meta << rv::format_duration(ms);
        if (s->credits > 0)
            meta << RunPlanPanel::tr("%1 cr").arg(rv::format_credits(s->credits));
        if (s->attempts > 1)
            meta << RunPlanPanel::tr("attempt %1").arg(s->attempts);
        if (meta.isEmpty() && s->status == agt::StepStatus::Pending)
            meta << RunPlanPanel::tr("not started");
        p->setFont(meta_font_);
        p->setPen(QColor(selected ? ui::colors::TEXT_SECONDARY() : ui::colors::TEXT_TERTIARY()));
        p->drawText(QRect(tx, y + 2, body_w, meta_h_), Qt::AlignLeft | Qt::AlignVCenter,
                    meta.join(QStringLiteral(" · ")));

        if (option.state & QStyle::State_HasFocus) {
            p->setPen(QColor(ui::colors::BORDER_BRIGHT()));
            p->setBrush(Qt::NoBrush);
            p->drawRect(rect.adjusted(0, 0, -1, -1));
        }
        p->setPen(QColor(ui::colors::BORDER_DIM()));
        p->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
        p->restore();
    }

  private:
    int text_width(int row_width) const { return std::max(20, row_width - 8 - badge_d_ - 7 - 8); }

    static constexpr int kPad = 6;
    QFont title_font_;
    QFont body_font_;
    QFont meta_font_;
    QFont pill_font_;
    QFont badge_font_;
    int title_h_ = 16;
    int body_h_ = 14;
    int meta_h_ = 12;
    int badge_d_ = 18;
};

QString rpp_style() {
    return QStringLiteral(
               "#runPlanPanel{background:%1;}"
               "#runPlanPanel QLabel{background:transparent;border:none;color:%3;font-size:%9px;}"
               "#runPlanPanel QLabel#rppTitle{color:%2;font-size:%10px;font-weight:700;}"
               "#runPlanPanel QLabel#rppSection{color:%4;font-size:%11px;font-weight:700;letter-spacing:1px;}"
               "#runPlanPanel QLabel#rppError{color:%12;}"
               "#runPlanPanel QLabel#rppEmpty{color:%4;}"
               "#runPlanPanel QFrame#rppWait{background:%13;border:1px solid %14;border-left:3px solid %14;}"
               "#runPlanPanel QFrame#rppWait QLabel#rppSection{color:%14;}"
               "#runPlanPanel QFrame#rppStop{background:%15;border:1px solid %12;border-left:3px solid %12;}"
               "#runPlanPanel QFrame#rppStop QLabel#rppSection{color:%12;}"
               "#runPlanPanel QFrame#rppStop[warn=\"true\"]{background:%13;border-color:%14;}"
               "#runPlanPanel QFrame#rppStop[warn=\"true\"] QLabel#rppSection{color:%14;}"
               "#runPlanPanel QFrame#rppSingle{background:%5;border:1px solid %6;}"
               "#runPlanPanel QFrame#rppConfirm{background:%15;border:1px solid %12;}"
               "#runPlanPanel QPushButton{background:%5;color:%3;border:1px solid %6;padding:3px 9px;"
               "font-size:%11px;font-weight:700;}"
               "#runPlanPanel QPushButton:hover{color:%2;border-color:%7;}"
               "#runPlanPanel QPushButton:focus{border-color:%8;}"
               "#runPlanPanel QPushButton:disabled{color:%16;border-color:%6;background:transparent;}"
               "#runPlanPanel QPushButton#rppPrimary{background:%8;color:%17;border-color:%8;}"
               "#runPlanPanel QPushButton#rppPrimary:hover{background:%18;border-color:%18;}"
               "#runPlanPanel QPushButton#rppDanger{background:%15;color:%12;border-color:%12;}"
               "#runPlanPanel QProgressBar{background:%5;border:none;max-height:4px;min-height:4px;}"
               "#runPlanPanel QProgressBar::chunk{background:%19;}"
               "#runPlanPanel QProgressBar[warn=\"true\"]::chunk{background:%14;}"
               "#runPlanPanel QProgressBar[stop=\"true\"]::chunk{background:%12;}"
               "#runPlanPanel QListView#rppSteps{background:%1;border:none;outline:none;}"
               "#runPlanPanel QScrollBar:vertical{background:transparent;width:6px;}"
               "#runPlanPanel QScrollBar::handle:vertical{background:%6;min-height:20px;}"
               "#runPlanPanel QScrollBar::add-line:vertical,#runPlanPanel QScrollBar::sub-line:vertical{height:0;}"
               "#runPlanPanel QPlainTextEdit#rppSingleText{background:transparent;border:none;color:%3;"
               "font-size:%9px;}"
               "#runPlanPanel QMenu{background:%1;color:%2;border:1px solid %6;padding:4px 0;}"
               "#runPlanPanel QMenu::item{padding:4px 18px 4px 12px;}"
               "#runPlanPanel QMenu::item:selected{background:%5;}"
               "#runPlanPanel QMenu::item:disabled{color:%16;}"
               "#runPlanPanel QMenu::separator{background:%6;height:1px;margin:4px 8px;}")
        .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(),
             ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT(),
             ui::colors::AMBER(), ui::fonts::px(-3))
        .arg(ui::fonts::px(-1), ui::fonts::px(-4), ui::colors::DANGER(),
             ui::colors::tint(QColor(ui::colors::WARNING()), 0.12), ui::colors::WARNING(),
             ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::colors::TEXT_DIM(), ui::colors::TEXT_ON_ACCENT(),
             ui::colors::AMBER_DIM(), ui::colors::INFO());
}

} // namespace run_plan_panel_detail

using namespace run_plan_panel_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct RunPlanPanel::Impl {
    RunPlanPanel* q = nullptr;
    QString run_id;
    bool deleted = false; ///< the run shown was deleted (or vanished with a reset)

    QStackedWidget* pages = nullptr; ///< 0 = empty, 1 = run
    QLabel* empty = nullptr;

    QLabel* title = nullptr;
    QLabel* pill = nullptr;
    QLabel* meta = nullptr;
    QLabel* activity = nullptr;

    QFrame* wait_box = nullptr;
    QLabel* wait_title = nullptr;
    QLabel* wait_text = nullptr;
    QPushButton* wait_btn = nullptr;

    QFrame* stop_box = nullptr;
    QLabel* stop_title = nullptr;
    QLabel* stop_text = nullptr;

    QLabel* budget_title = nullptr;
    QLabel* budget_text = nullptr;
    QProgressBar* budget_bar = nullptr;

    QPushButton* pause_btn = nullptr;
    QPushButton* resume_btn = nullptr;
    QPushButton* cancel_btn = nullptr;
    QPushButton* retry_btn = nullptr;

    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QPushButton* confirm_yes = nullptr;
    QPushButton* confirm_no = nullptr;

    QLabel* error = nullptr;
    QTimer* error_timer = nullptr;

    QLabel* steps_title = nullptr;
    QStackedWidget* body = nullptr; ///< 0 = steps, 1 = single reply, 2 = planning
    QListView* steps_view = nullptr;
    RppStepModel* steps_model = nullptr;
    RppStepDelegate* steps_delegate = nullptr;
    QLabel* single_title = nullptr;
    QPlainTextEdit* single_text = nullptr;
    QLabel* planning = nullptr;

    QTimer* tick = nullptr;
    QString last_pill_css;

    void build();
    void wire();
    void retranslate();
    void refresh();
    void refresh_steps();
    void refresh_buttons(const agt::TaskRun* r);
    void update_tick();
    int selected_step() const;
    void show_error(const QString& text);
    void do_pause();
    void do_resume();
    void ask_cancel();
    void do_retry(int step);
};

void RunPlanPanel::Impl::build() {
    q->setObjectName(QStringLiteral("runPlanPanel"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(rpp_style());

    auto* outer = new QVBoxLayout(q);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    pages = new QStackedWidget(q);
    outer->addWidget(pages);

    empty = new QLabel(pages);
    empty->setObjectName(QStringLiteral("rppEmpty"));
    empty->setWordWrap(true);
    empty->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    empty->setContentsMargins(12, 24, 12, 12);
    pages->addWidget(empty);

    auto* page = new QWidget(pages);
    pages->addWidget(page);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(8, 8, 8, 6);
    vl->setSpacing(6);

    title = new QLabel(page);
    title->setObjectName(QStringLiteral("rppTitle"));
    title->setWordWrap(true);
    title->setTextFormat(Qt::PlainText);
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(title);
    auto* head = new QHBoxLayout;
    head->setSpacing(6);
    pill = new QLabel(page);
    pill->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    meta = new QLabel(page);
    meta->setWordWrap(true);
    head->addWidget(pill, 0, Qt::AlignTop);
    head->addWidget(meta, 1);
    vl->addLayout(head);
    activity = new QLabel(page);
    activity->setWordWrap(true);
    activity->setTextFormat(Qt::PlainText);
    vl->addWidget(activity);

    // What the run waits for.
    wait_box = new QFrame(page);
    wait_box->setObjectName(QStringLiteral("rppWait"));
    auto* wl = new QVBoxLayout(wait_box);
    wl->setContentsMargins(8, 6, 8, 6);
    wl->setSpacing(4);
    wait_title = new QLabel(wait_box);
    wait_title->setObjectName(QStringLiteral("rppSection"));
    wait_text = new QLabel(wait_box);
    wait_text->setWordWrap(true);
    wait_text->setTextFormat(Qt::PlainText);
    wait_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    wait_btn = new QPushButton(wait_box);
    wait_btn->setObjectName(QStringLiteral("rppPrimary"));
    wait_btn->setCursor(Qt::PointingHandCursor);
    wl->addWidget(wait_title);
    wl->addWidget(wait_text);
    wl->addWidget(wait_btn, 0, Qt::AlignLeft);
    wait_box->setVisible(false);
    vl->addWidget(wait_box);

    // Why it stopped.
    stop_box = new QFrame(page);
    stop_box->setObjectName(QStringLiteral("rppStop"));
    auto* sl = new QVBoxLayout(stop_box);
    sl->setContentsMargins(8, 6, 8, 6);
    sl->setSpacing(3);
    stop_title = new QLabel(stop_box);
    stop_title->setObjectName(QStringLiteral("rppSection"));
    stop_text = new QLabel(stop_box);
    stop_text->setWordWrap(true);
    stop_text->setTextFormat(Qt::PlainText);
    stop_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(stop_title);
    sl->addWidget(stop_text);
    stop_box->setVisible(false);
    vl->addWidget(stop_box);

    // Budget.
    budget_title = new QLabel(page);
    budget_title->setObjectName(QStringLiteral("rppSection"));
    budget_text = new QLabel(page);
    budget_text->setWordWrap(true);
    budget_bar = new QProgressBar(page);
    budget_bar->setTextVisible(false);
    budget_bar->setRange(0, 1000);
    vl->addWidget(budget_title);
    vl->addWidget(budget_text);
    vl->addWidget(budget_bar);

    // Controls (wrap at large fonts).
    auto* controls = new QWidget(page);
    auto* flow = new rv::RvFlowLayout(controls, 4, 4);
    pause_btn = new QPushButton(controls);
    resume_btn = new QPushButton(controls);
    cancel_btn = new QPushButton(controls);
    retry_btn = new QPushButton(controls);
    retry_btn->setProperty("fincept_agent_risk", "spend"); // agents: re-runs the agent step (model quota)
    for (QPushButton* b : {pause_btn, resume_btn, cancel_btn, retry_btn}) {
        b->setCursor(Qt::PointingHandCursor);
        flow->addWidget(b);
    }
    vl->addWidget(controls);

    confirm = new QFrame(page);
    confirm->setObjectName(QStringLiteral("rppConfirm"));
    auto* cl = new QVBoxLayout(confirm);
    cl->setContentsMargins(8, 6, 8, 6);
    cl->setSpacing(4);
    confirm_text = new QLabel(confirm);
    confirm_text->setWordWrap(true);
    cl->addWidget(confirm_text);
    auto* cb = new QHBoxLayout;
    cb->setSpacing(4);
    confirm_yes = new QPushButton(confirm);
    confirm_yes->setObjectName(QStringLiteral("rppDanger"));
    confirm_no = new QPushButton(confirm);
    cb->addWidget(confirm_yes);
    cb->addWidget(confirm_no);
    cb->addStretch();
    cl->addLayout(cb);
    confirm->setVisible(false);
    vl->addWidget(confirm);

    error = new QLabel(page);
    error->setObjectName(QStringLiteral("rppError"));
    error->setWordWrap(true);
    error->setVisible(false);
    vl->addWidget(error);
    error_timer = new QTimer(q);
    error_timer->setSingleShot(true);
    error_timer->setInterval(10000);

    steps_title = new QLabel(page);
    steps_title->setObjectName(QStringLiteral("rppSection"));
    vl->addWidget(steps_title);

    body = new QStackedWidget(page);
    steps_view = new QListView(body);
    steps_view->setObjectName(QStringLiteral("rppSteps"));
    steps_view->setUniformItemSizes(false);
    steps_view->setResizeMode(QListView::Adjust);
    steps_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    steps_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    steps_view->setSelectionMode(QAbstractItemView::SingleSelection);
    steps_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    steps_view->setMouseTracking(true);
    steps_view->setContextMenuPolicy(Qt::CustomContextMenu);
    steps_model = new RppStepModel(steps_view);
    steps_delegate = new RppStepDelegate(steps_view);
    steps_view->setItemDelegate(steps_delegate);
    steps_view->setModel(steps_model);
    body->addWidget(steps_view);

    auto* single = new QFrame(body);
    single->setObjectName(QStringLiteral("rppSingle"));
    auto* sgl = new QVBoxLayout(single);
    sgl->setContentsMargins(8, 8, 8, 8);
    sgl->setSpacing(4);
    single_title = new QLabel(single);
    single_title->setObjectName(QStringLiteral("rppSection"));
    // A scrolling read-only view: a long reply neither clips at the panel's
    // bottom nor makes the panel taller than the window (a word-wrapped label
    // does both, and is laid out again on every refresh).
    single_text = new QPlainTextEdit(single);
    single_text->setObjectName(QStringLiteral("rppSingleText"));
    single_text->setReadOnly(true);
    single_text->setFrameShape(QFrame::NoFrame);
    single_text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    single_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    single_text->document()->setDocumentMargin(0);
    single_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    sgl->addWidget(single_title);
    sgl->addWidget(single_text, 1);
    body->addWidget(single);

    planning = new QLabel(body);
    planning->setObjectName(QStringLiteral("rppEmpty"));
    planning->setWordWrap(true);
    planning->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    planning->setContentsMargins(8, 16, 8, 8);
    body->addWidget(planning);
    vl->addWidget(body, 1);

    tick = new QTimer(q);
    tick->setInterval(kRppTickMs);
}

void RunPlanPanel::Impl::wire() {
    auto& svc = rpp_svc();
    QObject::connect(&svc, &agt::TaskRunService::run_updated, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::run_finished, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::steps_changed, q, [this](const QString& id) {
        if (id == run_id)
            refresh();
    });
    QObject::connect(&svc, &agt::TaskRunService::runs_reset, q, [this]() { refresh(); });

    QObject::connect(pause_btn, &QPushButton::clicked, q, [this]() { do_pause(); });
    QObject::connect(resume_btn, &QPushButton::clicked, q, [this]() { do_resume(); });
    QObject::connect(cancel_btn, &QPushButton::clicked, q, [this]() { ask_cancel(); });
    QObject::connect(retry_btn, &QPushButton::clicked, q, [this]() { do_retry(std::max(0, selected_step())); });
    QObject::connect(wait_btn, &QPushButton::clicked, q, [this]() {
        if (!run_id.isEmpty())
            emit q->conversation_requested(run_id);
    });
    QObject::connect(confirm_no, &QPushButton::clicked, q, [this]() { confirm->setVisible(false); });
    QObject::connect(confirm_yes, &QPushButton::clicked, q, [this]() {
        confirm->setVisible(false);
        if (!rpp_svc().cancel_run(run_id))
            show_error(RunPlanPanel::tr("The run could not be cancelled: it has already ended."));
    });
    for (QPushButton* b : {confirm_yes, confirm_no})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                confirm->setVisible(false);
            return true;
        });
    QObject::connect(error_timer, &QTimer::timeout, q, [this]() { error->setVisible(false); });
    QObject::connect(steps_view->selectionModel(), &QItemSelectionModel::currentChanged, q,
                     [this](const QModelIndex&, const QModelIndex&) { refresh_buttons(rpp_svc().find_run(run_id)); });
    QObject::connect(steps_view, &QListView::customContextMenuRequested, q, [this](const QPoint& pos) {
        const QModelIndex idx = steps_view->indexAt(pos);
        const agt::RunStep* s = steps_model->step_at(idx.row());
        const agt::TaskRun* r = rpp_svc().find_run(run_id);
        if (!s || !r)
            return;
        const rv::RunActions a = rv::run_actions(*r, rv::executor_can_pause(*r));
        auto* menu = new QMenu(steps_view);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->setToolTipsVisible(true);
        const int step = s->idx;
        QAction* retry = menu->addAction(RunPlanPanel::tr("Retry from this step"));
        retry->setProperty("fincept_agent_risk", "spend"); // agents: re-runs the agent from this step (model quota)
        retry->setEnabled(a.retry.enabled);
        retry->setToolTip(a.retry.reason);
        QObject::connect(retry, &QAction::triggered, q, [this, step]() { do_retry(step); });
        QAction* copy = menu->addAction(RunPlanPanel::tr("Copy step result"));
        copy->setEnabled(!s->summary.isEmpty());
        const QString summary = s->summary;
        QObject::connect(copy, &QAction::triggered, q, [summary]() {
            if (auto* cb = QGuiApplication::clipboard())
                cb->setText(summary);
        });
        menu->popup(steps_view->viewport()->mapToGlobal(pos));
    });
    QObject::connect(tick, &QTimer::timeout, q, [this]() {
        const agt::TaskRun* r = rpp_svc().find_run(run_id);
        if (!r) {
            tick->stop();
            return;
        }
        // Only the time-dependent texts: elapsed, budget minutes, running step duration.
        refresh();
    });
    // Theme / font changes: the panel's sheet follows through the central re-style pass;
    // the painted rows and the per-label pills are refitted once the appearance settled
    // (never inside theme_changed).
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() {
        const QString sheet = rpp_style();
        if (q->styleSheet() != sheet)
            q->setStyleSheet(sheet);
        steps_delegate->refresh_fonts();
        steps_view->doItemsLayout();
        last_pill_css.clear(); // rebuilt from the new tokens
        refresh();
    });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange) {
            retranslate();
        } else if (t == QEvent::Show) {
            refresh();
        } else if (t == QEvent::Hide) {
            tick->stop();
        }
    });
}

void RunPlanPanel::Impl::retranslate() {
    budget_title->setText(RunPlanPanel::tr("BUDGET"));
    confirm_yes->setText(RunPlanPanel::tr("CANCEL RUN"));
    confirm_no->setText(RunPlanPanel::tr("KEEP"));
    pause_btn->setText(RunPlanPanel::tr("PAUSE"));
    resume_btn->setText(RunPlanPanel::tr("RESUME"));
    cancel_btn->setText(RunPlanPanel::tr("CANCEL"));
    pause_btn->setAccessibleName(RunPlanPanel::tr("Pause the run"));
    resume_btn->setAccessibleName(RunPlanPanel::tr("Resume the run"));
    cancel_btn->setAccessibleName(RunPlanPanel::tr("Cancel the run"));
    retry_btn->setAccessibleName(RunPlanPanel::tr("Retry the run from the selected step"));
    steps_view->setAccessibleName(RunPlanPanel::tr("Steps of the run"));
    single_title->setText(RunPlanPanel::tr("SINGLE REPLY"));
    refresh();
}

int RunPlanPanel::Impl::selected_step() const {
    const agt::RunStep* s = steps_model->step_at(steps_view->currentIndex().row());
    return s ? s->idx : -1;
}

void RunPlanPanel::Impl::show_error(const QString& text) {
    error->setText(text);
    error->setVisible(!text.isEmpty());
    error_timer->start();
}

void RunPlanPanel::Impl::update_tick() {
    const agt::TaskRun* r = rpp_svc().find_run(run_id);
    const bool want = q->isVisible() && r && !agt::is_terminal(r->status) && r->started_at_ms > 0;
    if (want && !tick->isActive())
        tick->start();
    else if (!want)
        tick->stop();
}

void RunPlanPanel::Impl::refresh() {
    auto& svc = rpp_svc();
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : svc.find_run(run_id);
    if (!r) {
        if (!run_id.isEmpty() && svc.is_loaded())
            deleted = true;
        empty->setText(run_id.isEmpty()
                           ? RunPlanPanel::tr("No run selected.\n\nSend a message, start a task with NEW RUN, or pick "
                                              "a run in RUNS to follow its plan here.")
                       : deleted ? RunPlanPanel::tr("This run was deleted.")
                                 : RunPlanPanel::tr("Loading the run…"));
        pages->setCurrentIndex(0);
        confirm->setVisible(false);
        tick->stop();
        return;
    }
    deleted = false;
    pages->setCurrentIndex(1);
    const qint64 now = rpp_now_ms();
    const QVector<agt::RunStep> steps = svc.steps(run_id);

    // Header.
    title->setText(r->title);
    const rv::Tone tone = rv::run_status_tone(r->status);
    pill->setText(rv::run_status_pill(r->status));
    pill->setToolTip(agt::run_status_label(r->status));
    const QString css = rv::pill_css(tone, ui::fonts::px(-5));
    if (css != last_pill_css) {
        pill->setStyleSheet(css);
        last_pill_css = css;
    }
    QStringList m{rpp_engine_text(*r)};
    if (r->status == agt::RunStatus::Queued) {
        const int pos = svc.queue_position(run_id);
        m << (pos > 0 ? RunPlanPanel::tr("queued #%1").arg(pos) : RunPlanPanel::tr("queued"));
    } else if (r->started_at_ms > 0) {
        m << rv::format_duration(rv::run_elapsed_ms(*r, now));
    }
    meta->setText(m.join(QStringLiteral(" · ")));
    const QString act = agt::is_terminal(r->status) ? QString() : r->activity;
    activity->setText(act);
    activity->setVisible(!act.isEmpty());

    // Waiting for the user.
    const bool waiting = agt::needs_attention(r->status);
    wait_box->setVisible(waiting);
    if (waiting) {
        const QJsonObject& w = r->waiting_payload;
        if (r->status == agt::RunStatus::WaitingInput) {
            wait_title->setText(RunPlanPanel::tr("THE AGENT ASKS"));
            const QString question = w.value(QStringLiteral("question")).toString().trimmed();
            wait_text->setText(question.isEmpty() ? RunPlanPanel::tr("The agent needs more information to continue.")
                                                  : question);
            wait_btn->setText(RunPlanPanel::tr("ANSWER IN THE CONVERSATION"));
            wait_btn->setToolTip(RunPlanPanel::tr("Opens the run's conversation: your next message answers the "
                                                  "question and the run continues."));
        } else {
            wait_title->setText(RunPlanPanel::tr("APPROVAL NEEDED"));
            QString what = w.value(QStringLiteral("title")).toString().trimmed();
            if (what.isEmpty())
                what = QStringLiteral("%1 %2 %3")
                           .arg(w.value(QStringLiteral("action")).toString().toUpper(),
                                w.value(QStringLiteral("quantity")).toVariant().toString(),
                                w.value(QStringLiteral("symbol")).toString())
                           .simplified();
            wait_text->setText(what.isEmpty() ? RunPlanPanel::tr("An action of the agent waits for your decision.")
                                              : what);
            wait_btn->setText(RunPlanPanel::tr("REVIEW IN THE CONVERSATION"));
            wait_btn->setToolTip(RunPlanPanel::tr("Opens the run's conversation, where the approval card is."));
        }
        wait_btn->setEnabled(!r->session_uuid.isEmpty());
        if (r->session_uuid.isEmpty())
            wait_btn->setToolTip(RunPlanPanel::tr("The run has no conversation yet."));
    }

    // Why it stopped.
    const bool stopped = r->status == agt::RunStatus::Failed || r->status == agt::RunStatus::BudgetStopped ||
                         r->status == agt::RunStatus::Interrupted ||
                         (r->status == agt::RunStatus::Cancelled && !r->error.isEmpty());
    stop_box->setVisible(stopped);
    if (stopped) {
        const bool warn = r->status != agt::RunStatus::Failed;
        if (stop_box->property("warn").toBool() != warn) {
            stop_box->setProperty("warn", warn);
            stop_box->style()->unpolish(stop_box);
            stop_box->style()->polish(stop_box);
            for (QLabel* l : {stop_title, stop_text}) {
                l->style()->unpolish(l);
                l->style()->polish(l);
            }
        }
        switch (r->status) {
            case agt::RunStatus::BudgetStopped:
                stop_title->setText(RunPlanPanel::tr("STOPPED AT ITS BUDGET"));
                break;
            case agt::RunStatus::Interrupted:
                stop_title->setText(RunPlanPanel::tr("INTERRUPTED"));
                break;
            case agt::RunStatus::Cancelled:
                stop_title->setText(RunPlanPanel::tr("CANCELLED"));
                break;
            default:
                stop_title->setText(RunPlanPanel::tr("FAILED"));
                break;
        }
        QString why = r->error.trimmed();
        if (why.isEmpty())
            why = agt::run_status_label(r->status);
        if (r->status == agt::RunStatus::Interrupted)
            why += QStringLiteral("\n") + RunPlanPanel::tr("RESUME continues where it stopped; RETRY starts a new run "
                                                           "from the step you pick.");
        else if (r->status == agt::RunStatus::BudgetStopped)
            why += QStringLiteral("\n") + RunPlanPanel::tr("Finished steps are kept. RETRY FROM STEP starts a new run "
                                                           "with a fresh budget.");
        stop_text->setText(why);
    }

    // Budget.
    const rv::BudgetLine b = rv::budget_line(*r, steps, now);
    QStringList parts{b.credits};
    if (!b.steps.isEmpty())
        parts << b.steps;
    if (!b.time.isEmpty())
        parts << b.time;
    if (r->tokens_used > 0)
        parts << RunPlanPanel::tr("%1 tokens").arg(QLocale().toString(r->tokens_used));
    if (r->engine == QLatin1String(agt::kEngineLocal))
        parts << RunPlanPanel::tr("your own model — no credits");
    else if (r->engine == QLatin1String(rv::kHarnessEngineId))
        parts << RunPlanPanel::tr("harness on your own API key — no credits");
    budget_text->setText(parts.join(QStringLiteral(" · ")));
    budget_title->setText(b.limited ? RunPlanPanel::tr("BUDGET") : RunPlanPanel::tr("USAGE"));
    budget_bar->setVisible(b.credits_fraction >= 0);
    if (b.credits_fraction >= 0) {
        budget_bar->setValue(static_cast<int>(std::clamp(b.credits_fraction, 0.0, 1.0) * 1000));
        const bool warn = !b.stopped && b.credits_fraction >= 0.8;
        if (budget_bar->property("warn").toBool() != warn || budget_bar->property("stop").toBool() != b.stopped) {
            budget_bar->setProperty("warn", warn);
            budget_bar->setProperty("stop", b.stopped);
            budget_bar->style()->unpolish(budget_bar);
            budget_bar->style()->polish(budget_bar);
        }
        budget_bar->setToolTip(b.credits);
    }

    refresh_steps();
    refresh_buttons(r);
    update_tick();
}

void RunPlanPanel::Impl::refresh_steps() {
    const agt::TaskRun* r = rpp_svc().find_run(run_id);
    if (!r)
        return;
    const QVector<agt::RunStep> steps = rpp_svc().steps(run_id);
    if (rv::is_single_reply(*r, steps)) {
        steps_title->setText(RunPlanPanel::tr("RESULT"));
        QString text;
        if (!r->final_text.trimmed().isEmpty())
            text = rv::markdown_preview(r->final_text.trimmed().left(1500));
        else if (!steps.isEmpty() && !steps.first().summary.isEmpty())
            text = rv::markdown_preview(steps.first().summary.left(1500));
        else if (!agt::is_terminal(r->status))
            text = RunPlanPanel::tr("One agent turn in the conversation. Its reply streams into the transcript; "
                                    "the ACTIVITY tab shows every tool call.");
        else
            text = RunPlanPanel::tr("No reply text.");
        if (single_text->toPlainText() != text) // refreshed every second while a run is going
            single_text->setPlainText(text);
        steps_model->set_steps({});
        body->setCurrentIndex(1);
        return;
    }
    if (steps.isEmpty()) {
        steps_title->setText(RunPlanPanel::tr("PLAN"));
        planning->setText(r->status == agt::RunStatus::Planning || r->status == agt::RunStatus::Queued
                              ? RunPlanPanel::tr("The agent is writing the plan…")
                              : RunPlanPanel::tr("No plan was recorded for this run."));
        steps_model->set_steps({});
        body->setCurrentIndex(2);
        return;
    }
    const rv::StepProgress p = rv::step_progress(steps);
    steps_title->setText(RunPlanPanel::tr("PLAN · %1 OF %2 DONE").arg(p.done).arg(p.total));
    const int keep = selected_step();
    if (steps_model->set_steps(steps))
        emit steps_delegate->sizeHintChanged(steps_model->index(0)); // summaries changed: relayout the rows
    body->setCurrentIndex(0);
    if (keep >= 0 && keep < steps.size() && steps_view->currentIndex().row() != keep)
        steps_view->setCurrentIndex(steps_model->index(keep));
    if (p.current >= 0 && keep < 0)
        steps_view->scrollTo(steps_model->index(p.current));
}

void RunPlanPanel::Impl::refresh_buttons(const agt::TaskRun* r) {
    if (!r) {
        for (QPushButton* b : {pause_btn, resume_btn, cancel_btn, retry_btn})
            b->setEnabled(false);
        return;
    }
    const rv::RunActions a = rv::run_actions(*r, rv::executor_can_pause(*r));
    auto apply = [](QPushButton* b, const rv::ActionState& st, const QString& tip) {
        b->setEnabled(st.enabled);
        b->setToolTip(st.enabled ? tip : st.reason);
    };
    apply(pause_btn, a.pause,
          r->status == agt::RunStatus::Queued ? RunPlanPanel::tr("Hold the run back before it starts")
                                              : RunPlanPanel::tr("Pause after the current step"));
    apply(resume_btn, a.resume,
          r->status == agt::RunStatus::Interrupted ? RunPlanPanel::tr("Continue the interrupted run where it stopped")
                                                   : RunPlanPanel::tr("Continue the paused run"));
    apply(cancel_btn, a.cancel, RunPlanPanel::tr("Stop the run"));
    const QVector<agt::RunStep> steps = rpp_svc().steps(run_id);
    const bool single = rv::is_single_reply(*r, steps) || steps.size() <= 1;
    const int sel = selected_step();
    if (single) {
        retry_btn->setText(RunPlanPanel::tr("RETRY"));
        apply(retry_btn, a.retry, RunPlanPanel::tr("Send the same request again as a new run"));
    } else {
        const int from = sel >= 0 ? sel : 0;
        retry_btn->setText(RunPlanPanel::tr("RETRY FROM STEP %1").arg(from + 1));
        apply(retry_btn, a.retry,
              sel >= 0 ? RunPlanPanel::tr("Start a new run that keeps steps 1–%1 and runs again from step %2")
                             .arg(from)
                             .arg(from + 1)
                       : RunPlanPanel::tr("Select a step to retry from it; without one the whole plan runs again"));
    }
}

void RunPlanPanel::Impl::do_pause() {
    if (!rpp_svc().pause_run(run_id))
        show_error(RunPlanPanel::tr("The run could not be paused: it is not running, or its engine cannot pause it."));
}

void RunPlanPanel::Impl::do_resume() {
    QString err;
    if (rpp_svc().resume_run(run_id, &err).isEmpty())
        show_error(err.isEmpty() ? RunPlanPanel::tr("The run could not be resumed.") : err);
}

void RunPlanPanel::Impl::ask_cancel() {
    const agt::TaskRun* r = rpp_svc().find_run(run_id);
    if (!r)
        return;
    const bool cloud_turn = r->executor_id() == QLatin1String(agt::kExecutorCloudTurn) && agt::holds_slot(r->status);
    confirm_text->setText(cloud_turn ? RunPlanPanel::tr("Cancel this run? The server cannot be stopped: it finishes "
                                                        "the reply and charges it; the terminal stops following it "
                                                        "and picks the stored answer up.")
                                     : RunPlanPanel::tr("Cancel this run? Finished steps are kept and you can retry "
                                                        "from any step later."));
    confirm->setVisible(true);
    confirm_yes->setFocus();
}

void RunPlanPanel::Impl::do_retry(int step) {
    QString err;
    const QString child = rpp_svc().retry_from_step(run_id, step, &err);
    if (child.isEmpty()) {
        show_error(err.isEmpty() ? RunPlanPanel::tr("The run could not be retried.") : err);
        return;
    }
    emit q->run_created(child);
}

// ── RunPlanPanel ─────────────────────────────────────────────────────────────

RunPlanPanel::RunPlanPanel(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
}

RunPlanPanel::~RunPlanPanel() = default;

void RunPlanPanel::set_run(const QString& run_id) {
    if (run_id == d_->run_id)
        return;
    d_->run_id = run_id;
    d_->deleted = false;
    d_->confirm->setVisible(false);
    d_->error->setVisible(false);
    d_->steps_model->set_steps({});
    d_->refresh();
}

QString RunPlanPanel::run_id() const {
    return d_->run_id;
}

} // namespace fincept::chat_mode
