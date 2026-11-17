// ToolResultCard.cpp — a tool call / result card for the transcript and the
// ACTIVITY drawer (WP5). See the header.

#include "screens/chat_mode/ToolResultCard.h"

#include "screens/chat_mode/ArtefactTableModel.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/ArtefactLogic.h"
#include "services/agent_tasks/ArtefactService.h"
#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPointer>
#include <QStyle>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace al = fincept::agent_tasks::artefact_logic;
namespace rv = fincept::chat_mode::run_view;

namespace tool_result_card_detail {

constexpr int kTrcMaxTableRows = 1000;
constexpr int kTrcMaxFields = 60;
constexpr int kTrcVisibleRows = 10;

/// One selector stylesheet, set on the card before its children exist; the theme
/// manager's re-style pass keeps it in step with theme and font changes.
QString trc_style(bool compact) {
    const int base = compact ? -3 : -2;
    return QStringLiteral(
               "QFrame#toolResultCard{background:%1;border:1px solid %2;border-left:2px solid %3;}"
               "QFrame#toolResultCard[tone=\"success\"]{border-left-color:%4;}"
               "QFrame#toolResultCard[tone=\"danger\"]{border-left-color:%5;}"
               "QFrame#toolResultCard QLabel{background:transparent;border:none;color:%6;font-size:%7px;}"
               "QFrame#toolResultCard QLabel#trcName{font-weight:700;}"
               "QFrame#toolResultCard QLabel#trcArgs{color:%8;}"
               "QFrame#toolResultCard QLabel#trcMeta{color:%9;}"
               "QFrame#toolResultCard QLabel#trcState{font-weight:700;color:%3;}"
               "QFrame#toolResultCard QLabel#trcState[tone=\"success\"]{color:%4;}"
               "QFrame#toolResultCard QLabel#trcState[tone=\"danger\"]{color:%5;}"
               "QFrame#toolResultCard QLabel#trcNote{color:%10;font-style:italic;}"
               "QFrame#toolResultCard QLabel#trcError{color:%5;}"
               "QFrame#toolResultCard QLabel#trcKey{color:%9;}")
               .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::INFO(), ui::colors::SUCCESS(),
                    ui::colors::DANGER(), ui::colors::TEXT_PRIMARY(), ui::fonts::px(base), ui::colors::TEXT_TERTIARY(),
                    ui::colors::TEXT_SECONDARY())
               .arg(ui::colors::TEXT_DIM()) +
           QStringLiteral(
               "QFrame#toolResultCard QToolButton{background:transparent;color:%1;border:1px solid %2;padding:1px 6px;"
               "font-size:%3px;font-weight:700;}"
               "QFrame#toolResultCard QToolButton:hover{color:%4;border-color:%5;}"
               "QFrame#toolResultCard QToolButton:focus{border-color:%5;}"
               "QFrame#toolResultCard QToolButton:disabled{color:%6;border-color:%2;}"
               "QFrame#toolResultCard QTableView{background:%7;alternate-background-color:%8;color:%4;gridline-color:%2;"
               "border:1px solid %2;font-size:%3px;selection-background-color:%9;selection-color:%4;}"
               "QFrame#toolResultCard QHeaderView::section{background:%10;color:%1;border:none;border-bottom:1px solid %2;"
               "border-right:1px solid %2;padding:2px 6px;font-size:%3px;font-weight:700;}"
               "QFrame#toolResultCard QPlainTextEdit{background:%7;color:%4;border:1px solid %2;font-size:%3px;}")
               .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::fonts::px(base), ui::colors::TEXT_PRIMARY(),
                    ui::colors::AMBER(), ui::colors::TEXT_DIM(), ui::colors::BG_SURFACE(), ui::colors::ROW_ALT(),
                    ui::colors::BG_HOVER())
               .arg(ui::colors::BG_RAISED());
}

void trc_set_tone(QWidget* w, const char* tone) {
    if (w->property("tone").toString() == QLatin1String(tone))
        return;
    w->setProperty("tone", QString::fromLatin1(tone));
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QLabel* trc_label(QWidget* parent, const char* name, Qt::TextFormat fmt = Qt::PlainText) {
    auto* l = new QLabel(parent);
    l->setObjectName(QLatin1String(name));
    l->setTextFormat(fmt);
    return l;
}

} // namespace tool_result_card_detail

using namespace tool_result_card_detail;

struct ToolResultCard::Impl {
    ToolResultCard* q = nullptr;
    QString tool;
    QString args;
    QString run_id;
    int step = -1;
    QString ref_id;
    bool finished = false;
    bool success = true;
    qint64 duration_ms = -1;
    QString preview;
    QJsonValue full = QJsonValue(QJsonValue::Undefined);
    bool body_built = false;
    bool saved = false;

    QLabel* glyph = nullptr;
    QLabel* name = nullptr;
    QLabel* state = nullptr;
    QLabel* meta = nullptr;
    QLabel* args_lbl = nullptr;
    QToolButton* toggle = nullptr;
    QToolButton* save = nullptr;
    QWidget* body = nullptr;
    QVBoxLayout* body_layout = nullptr;

    void refresh();
    void elide_args();
    void build_body();
    void do_save();
};

void ToolResultCard::Impl::elide_args() {
    const int w = std::max(20, args_lbl->width());
    const QString shown = QFontMetrics(args_lbl->font()).elidedText(args, Qt::ElideRight, w);
    if (args_lbl->text() != shown)
        args_lbl->setText(shown);
}

void ToolResultCard::Impl::refresh() {
    name->setText(tool.isEmpty() ? ToolResultCard::tr("Tool") : tool);
    args_lbl->setToolTip(args);
    args_lbl->setVisible(!args.isEmpty());
    elide_args();
    const char* tone = !finished ? "info" : success ? "success" : "danger";
    glyph->setText(!finished ? QStringLiteral("●") : success ? QStringLiteral("✓") : QStringLiteral("✕"));
    state->setText(!finished ? ToolResultCard::tr("RUNNING") : success ? ToolResultCard::tr("DONE") : ToolResultCard::tr("FAILED"));
    trc_set_tone(glyph, tone);
    trc_set_tone(state, tone);
    trc_set_tone(q, tone);
    meta->setText(duration_ms >= 0 ? rv::ActivityLog::format_duration_precise(duration_ms) : QString());
    meta->setVisible(duration_ms >= 0);
    toggle->setEnabled(finished);
    toggle->setText((body && !body->isHidden() ? QStringLiteral("▾ ") : QStringLiteral("▸ ")) + ToolResultCard::tr("RESULT"));
    toggle->setToolTip(finished ? ToolResultCard::tr("Show or hide what the tool returned")
                                : ToolResultCard::tr("The tool is still running"));
    const bool can_save = finished && success && !run_id.isEmpty() && !saved;
    save->setVisible(!run_id.isEmpty());
    save->setEnabled(can_save);
    save->setText(saved ? ToolResultCard::tr("SAVED") : ToolResultCard::tr("SAVE AS ARTEFACT"));
    save->setToolTip(saved      ? ToolResultCard::tr("Kept in the run's ARTEFACTS")
                     : !finished ? ToolResultCard::tr("Available when the tool has finished")
                     : !success  ? ToolResultCard::tr("A failed call has no result to keep")
                                 : ToolResultCard::tr("Keep this result in the run's ARTEFACTS (table, JSON or text)"));
    q->setAccessibleName(ToolResultCard::tr("Tool %1: %2").arg(name->text(), state->text()));
}

void ToolResultCard::Impl::build_body() {
    if (body_built)
        return;
    body_built = true;
    const al::ToolResultView view = al::classify_tool_result(full, preview, success);
    const bool preview_only = full.isUndefined() || full.isNull();
    switch (view.shape) {
        case al::ResultShape::Table: {
            auto* table = new QTableView(body);
            auto* model = new ArtefactTableModel(table);
            model->set_table(view.table, kTrcMaxTableRows);
            table->setModel(model);
            table->setSortingEnabled(true);
            table->sortByColumn(-1, Qt::AscendingOrder);
            table->setAlternatingRowColors(true);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setStretchLastSection(true);
            table->horizontalHeader()->setHighlightSections(false);
            table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            table->setWordWrap(false);
            const QFontMetrics fm(table->font());
            const int row_h = fm.height() + 6;
            table->verticalHeader()->setDefaultSectionSize(row_h);
            for (int c = 0; c < model->columnCount(); ++c)
                table->setColumnWidth(c, std::clamp(fm.horizontalAdvance(model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()) + 28,
                                                    fm.averageCharWidth() * 8, fm.averageCharWidth() * 28));
            const int rows = std::min(model->rowCount(), kTrcVisibleRows);
            table->setFixedHeight(row_h * (rows + 1) + 6);
            body_layout->addWidget(table);
            QString note;
            if (!view.text.isEmpty())
                note = ToolResultCard::tr("%1 · ").arg(view.text);
            note += model->is_cut() ? ToolResultCard::tr("showing %1 of %2 rows — Save as artefact keeps them all")
                                          .arg(QLocale().toString(model->rowCount()), QLocale().toString(model->total_rows()))
                                    : ToolResultCard::tr("%n row(s) — click a column to sort", nullptr, model->rowCount());
            if (preview_only)
                note += QStringLiteral(" · ") + ToolResultCard::tr("from the server's shortened preview");
            auto* n = trc_label(body, "trcNote");
            n->setText(note);
            n->setWordWrap(true);
            body_layout->addWidget(n);
            break;
        }
        case al::ResultShape::KeyValue: {
            auto* grid_host = new QWidget(body);
            auto* grid = new QGridLayout(grid_host);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setHorizontalSpacing(10);
            grid->setVerticalSpacing(2);
            int row = 0;
            for (const auto& f : view.fields) {
                if (row >= kTrcMaxFields)
                    break;
                auto* k = trc_label(grid_host, "trcKey");
                k->setText(f.first);
                auto* v = trc_label(grid_host, "trcValue");
                v->setText(f.second);
                v->setWordWrap(true);
                v->setTextInteractionFlags(Qt::TextSelectableByMouse);
                grid->addWidget(k, row, 0, Qt::AlignTop);
                grid->addWidget(v, row, 1);
                ++row;
            }
            grid->setColumnStretch(1, 1);
            body_layout->addWidget(grid_host);
            if (view.fields.size() > kTrcMaxFields) {
                auto* n = trc_label(body, "trcNote");
                n->setText(ToolResultCard::tr("%1 more fields — Save as artefact keeps them all")
                               .arg(view.fields.size() - kTrcMaxFields));
                body_layout->addWidget(n);
            }
            break;
        }
        case al::ResultShape::Error: {
            auto* e = trc_label(body, "trcError");
            e->setText(view.text);
            e->setWordWrap(true);
            e->setTextInteractionFlags(Qt::TextSelectableByMouse);
            body_layout->addWidget(e);
            break;
        }
        case al::ResultShape::Text: {
            auto* text = new QPlainTextEdit(body);
            text->setReadOnly(true);
            text->setPlainText(view.text);
            text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
            const QFontMetrics fm(text->font());
            const int lines = std::min<int>(12, std::max<int>(2, static_cast<int>(view.text.count(QLatin1Char('\n'))) + 2));
            text->setFixedHeight(fm.lineSpacing() * lines + 10);
            body_layout->addWidget(text);
            if (preview_only) {
                auto* n = trc_label(body, "trcNote");
                n->setText(ToolResultCard::tr("The server sends a shortened preview of the tool's output."));
                n->setWordWrap(true);
                body_layout->addWidget(n);
            }
            break;
        }
        case al::ResultShape::Empty: {
            auto* n = trc_label(body, "trcNote");
            n->setText(ToolResultCard::tr("The tool returned no output."));
            body_layout->addWidget(n);
            break;
        }
    }
}

void ToolResultCard::Impl::do_save() {
    if (run_id.isEmpty() || saved)
        return;
    emit q->save_requested(tool, full.isUndefined() ? QJsonValue(preview) : full);
    save->setEnabled(false);
    save->setText(ToolResultCard::tr("SAVING…"));
    QJsonObject source;
    if (!ref_id.isEmpty())
        source[QStringLiteral("ref_id")] = ref_id;
    if (!args.isEmpty())
        source[QStringLiteral("arguments")] = args.left(500);
    QPointer<ToolResultCard> self(q);
    agt::ArtefactService::instance().save_tool_result(
        run_id, step, tool, full, preview, source, q, [self](bool ok, const QString& id, const QString& err) {
            if (!self)
                return;
            Impl* d = self->d_.get();
            d->saved = ok;
            d->refresh();
            if (!ok) {
                d->save->setToolTip(err);
                d->state->setToolTip(err);
            } else {
                emit self->saved(id);
            }
        });
}

// ── ToolResultCard ───────────────────────────────────────────────────────────

ToolResultCard::ToolResultCard(QWidget* parent, bool compact) : QFrame(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    setObjectName(QStringLiteral("toolResultCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(trc_style(compact)); // before the children exist
    setProperty("tone", QStringLiteral("info"));

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(8, 4, 8, 4);
    vl->setSpacing(2);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    d_->glyph = trc_label(this, "trcState");
    d_->name = trc_label(this, "trcName");
    d_->state = trc_label(this, "trcState");
    d_->meta = trc_label(this, "trcMeta");
    top->addWidget(d_->glyph);
    top->addWidget(d_->name);
    top->addStretch(1);
    top->addWidget(d_->meta);
    top->addWidget(d_->state);
    vl->addLayout(top);

    auto* second = new QHBoxLayout;
    second->setSpacing(6);
    d_->args_lbl = trc_label(this, "trcArgs");
    d_->args_lbl->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // elided to the room it gets
    d_->toggle = new QToolButton(this);
    d_->toggle->setCursor(Qt::PointingHandCursor);
    d_->save = new QToolButton(this);
    d_->save->setCursor(Qt::PointingHandCursor);
    second->addWidget(d_->args_lbl, 1);
    second->addWidget(d_->toggle);
    second->addWidget(d_->save);
    vl->addLayout(second);

    d_->body = new QWidget(this);
    d_->body_layout = new QVBoxLayout(d_->body);
    d_->body_layout->setContentsMargins(0, 4, 0, 2);
    d_->body_layout->setSpacing(3);
    d_->body->setVisible(false);
    vl->addWidget(d_->body);

    connect(d_->toggle, &QToolButton::clicked, this, [this]() { set_expanded(!is_expanded()); });
    connect(d_->save, &QToolButton::clicked, this, [this]() { d_->do_save(); });
    new rv::RvEventHook(d_->args_lbl, [this](QEvent::Type t) {
        if (t == QEvent::Resize)
            d_->elide_args();
    });
    d_->refresh();
}

ToolResultCard::~ToolResultCard() = default;

bool ToolResultCard::can_render(const QJsonValue& result) {
    if (result.isUndefined() || result.isNull())
        return false;
    const al::ToolResultView v = al::classify_tool_result(result, {}, true);
    return v.shape == al::ResultShape::Table || v.shape == al::ResultShape::KeyValue;
}

void ToolResultCard::set_call(const QString& tool, const QString& args_preview, const QString& run_id, int step,
                              const QString& ref_id) {
    d_->tool = tool;
    d_->args = al::summarize_arguments(args_preview, 400);
    d_->run_id = run_id;
    d_->step = step;
    d_->ref_id = ref_id;
    d_->refresh();
}

void ToolResultCard::set_result(bool success, qint64 duration_ms, const QString& preview, const QJsonValue& full) {
    d_->finished = true;
    d_->success = success;
    d_->duration_ms = duration_ms;
    d_->preview = preview;
    d_->full = full;
    if (d_->body_built) {
        // A late result for an expanded card: rebuild its body.
        while (QLayoutItem* item = d_->body_layout->takeAt(0)) {
            if (QWidget* w = item->widget())
                w->deleteLater();
            delete item;
        }
        d_->body_built = false;
        if (!d_->body->isHidden())
            d_->build_body();
    }
    d_->refresh();
}

void ToolResultCard::set_result(const QString& tool_name, const QJsonValue& result, bool success) {
    if (!tool_name.isEmpty())
        d_->tool = tool_name;
    set_result(success, -1, QString(), result);
}

void ToolResultCard::set_expanded(bool expanded) {
    if (expanded && !d_->finished)
        return;
    if (expanded)
        d_->build_body();
    d_->body->setVisible(expanded);
    d_->refresh();
}

bool ToolResultCard::is_expanded() const {
    return !d_->body->isHidden();
}

QString ToolResultCard::tool_name() const {
    return d_->tool;
}

QString ToolResultCard::ref_id() const {
    return d_->ref_id;
}

} // namespace fincept::chat_mode
