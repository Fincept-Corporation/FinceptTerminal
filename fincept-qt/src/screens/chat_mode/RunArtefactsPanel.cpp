// RunArtefactsPanel.cpp — the ARTEFACTS tab (WP5). See the header.
//
//   [THIS RUN] [PINNED · 3]                          4 artefacts · 86 KB
//   ┌ list ──────────────────────────────────────────────────────────────┐
//   │ MD   Final report — Deep-dive: AAPL               final · 14:32 · 12 KB │
//   │ CSV  Step 2 — Valuation multiples                 step 2 · 14:20 · 2 KB  │
//   └────────────────────────────────────────────────────────────────────┘
//   ┌ preview (Markdown / table / chart / image) ────────────────────────┐
//   [OPEN] [SAVE AS…] [COPY] [ADD TO REPORT] [SEND TO NOTES] [PIN] [DELETE]
//   (inline delete confirmation, status line)

#include "screens/chat_mode/RunArtefactsPanel.h"

#include "screens/chat_mode/ArtefactTableModel.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/ArtefactService.h"
#include "services/agent_tasks/ReportHandoff.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace al = fincept::agent_tasks::artefact_logic;
namespace rv = fincept::chat_mode::run_view;

namespace run_artefacts_panel_detail {

constexpr int kRapIdRole = Qt::UserRole + 1;
constexpr int kRapPreviewMaxChars = 400000;
constexpr int kRapStatusMs = 8000;

agt::ArtefactService& rap_svc() {
    return agt::ArtefactService::instance();
}

QString rap_size(qint64 bytes) {
    if (bytes <= 0)
        return QString();
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

/// Short tag drawn in the kind box.
QString rap_kind_tag(const QString& kind) {
    if (kind == QLatin1String(agt::kArtefactReport))
        return QStringLiteral("MD");
    if (kind == QLatin1String(agt::kArtefactTable))
        return QStringLiteral("CSV");
    if (kind == QLatin1String(agt::kArtefactSeries))
        return QStringLiteral("CHART");
    if (kind == QLatin1String(agt::kArtefactJson))
        return QStringLiteral("JSON");
    if (kind == QLatin1String(agt::kArtefactImage))
        return QStringLiteral("IMG");
    if (kind == QLatin1String(agt::kArtefactText))
        return QStringLiteral("TXT");
    return QStringLiteral("FILE");
}

QColor rap_kind_color(const QString& kind) {
    if (kind == QLatin1String(agt::kArtefactReport))
        return QColor(ui::colors::AMBER());
    if (kind == QLatin1String(agt::kArtefactTable) || kind == QLatin1String(agt::kArtefactSeries))
        return QColor(ui::colors::INFO());
    if (kind == QLatin1String(agt::kArtefactJson))
        return QColor(ui::colors::CYAN());
    return QColor(ui::colors::TEXT_SECONDARY());
}

/// "final · 14:32 · 12 KB" / "step 2 · tool get_quote · 14:20".
QString rap_meta(const agt::RunArtefact& a, bool with_run) {
    QStringList parts;
    if (with_run)
        parts << (a.run_title.isEmpty() ? RunArtefactsPanel::tr("deleted run") : a.run_title.left(40));
    const QString origin = a.source.value(QStringLiteral("origin")).toString();
    if (origin == QLatin1String("final"))
        parts << RunArtefactsPanel::tr("final");
    else if (a.step_idx >= 0)
        parts << RunArtefactsPanel::tr("step %1").arg(a.step_idx + 1);
    const QString tool = a.source.value(QStringLiteral("tool")).toString();
    if (!tool.isEmpty())
        parts << RunArtefactsPanel::tr("tool %1").arg(tool);
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(a.created_at_ms);
    parts << (when.date() == QDate::currentDate() ? QLocale().toString(when.time(), QLocale::ShortFormat)
                                                  : QLocale().toString(when, QLocale::ShortFormat));
    const QString size = rap_size(a.bytes);
    if (!size.isEmpty())
        parts << size;
    return parts.join(QStringLiteral(" · "));
}

/// Order: the final report first, then by step, then by time.
bool rap_before(const agt::RunArtefact& a, const agt::RunArtefact& b) {
    const bool af = a.source.value(QStringLiteral("origin")).toString() == QLatin1String("final");
    const bool bf = b.source.value(QStringLiteral("origin")).toString() == QLatin1String("final");
    if (af != bf)
        return af;
    if (a.step_idx != b.step_idx)
        return a.step_idx < b.step_idx;
    if (a.created_at_ms != b.created_at_ms)
        return a.created_at_ms < b.created_at_ms;
    return a.id < b.id;
}

class RapModel : public QAbstractListModel {
  public:
    explicit RapModel(QObject* parent) : QAbstractListModel(parent) {}

    void set_items(const QVector<agt::RunArtefact>& items, bool with_run) {
        beginResetModel();
        items_ = items;
        with_run_ = with_run;
        endResetModel();
    }
    void relayout() {
        beginResetModel();
        endResetModel();
    }
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : static_cast<int>(items_.size()); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= items_.size())
            return {};
        const agt::RunArtefact& a = items_.at(index.row());
        switch (role) {
            case Qt::DisplayRole:
            case Qt::AccessibleTextRole:
                return a.title;
            case Qt::ToolTipRole:
                return a.title + QLatin1Char('\n') + rap_meta(a, true);
            case kRapIdRole:
                return a.id;
            default:
                return {};
        }
    }
    const agt::RunArtefact* at(int row) const { return row >= 0 && row < items_.size() ? &items_.at(row) : nullptr; }
    int row_of(const QString& id) const {
        for (int i = 0; i < items_.size(); ++i)
            if (items_.at(i).id == id)
                return i;
        return -1;
    }
    bool with_run() const { return with_run_; }

  private:
    QVector<agt::RunArtefact> items_;
    bool with_run_ = false;
};

/// Two lines per artefact: a kind box, the title (bold) and its meta line.
class RapDelegate : public QStyledItemDelegate {
  public:
    explicit RapDelegate(RapModel* model, QObject* parent) : QStyledItemDelegate(parent), model_(model) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        const QFontMetrics t(ui::fonts::font(-2, QFont::Bold));
        const QFontMetrics m(ui::fonts::font(-3));
        return QSize(option.rect.width(), t.height() + m.height() + 12);
    }

    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const agt::RunArtefact* a = model_->at(index.row());
        if (!a)
            return;
        p->save();
        const QRect r = option.rect;
        const bool selected = option.state & QStyle::State_Selected;
        const bool hover = option.state & QStyle::State_MouseOver;
        if (selected)
            p->fillRect(r, QColor(ui::colors::BG_HOVER()));
        else if (hover)
            p->fillRect(r, ui::colors::overlay_color(0.04));
        p->setPen(QColor(ui::colors::BORDER_DIM()));
        p->drawLine(r.bottomLeft(), r.bottomRight());
        if (selected) {
            p->fillRect(QRect(r.left(), r.top(), 2, r.height()), QColor(ui::colors::AMBER()));
        }
        const QFont tag_font = ui::fonts::font(-5, QFont::Bold);
        const QFontMetrics tfm(tag_font);
        const QString tag = rap_kind_tag(a->kind);
        const int box_w = std::max(tfm.horizontalAdvance(QStringLiteral("CHART")) + 8, 30);
        const QRect box(r.left() + 6, r.top() + 6, box_w, tfm.height() + 6);
        const QColor kc = rap_kind_color(a->kind);
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(QPen(kc, 1));
        p->setBrush(ui::colors::tint_color(kc, 0.14));
        p->drawRoundedRect(QRectF(box).adjusted(0.5, 0.5, -0.5, -0.5), 2, 2);
        p->setFont(tag_font);
        p->drawText(box, Qt::AlignCenter, tag);
        const int x = box.right() + 8;
        const int w = std::max(10, r.right() - x - 6);
        const QFont title_font = ui::fonts::font(-2, QFont::Bold);
        const QFontMetrics title_fm(title_font);
        p->setFont(title_font);
        p->setPen(QColor(ui::colors::TEXT_PRIMARY()));
        QString title = a->title;
        if (a->pinned)
            title = QStringLiteral("◆ ") + title; // pinned (a glyph every terminal font has)
        p->drawText(QRect(x, r.top() + 5, w, title_fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    title_fm.elidedText(title, Qt::ElideRight, w));
        const QFont meta_font = ui::fonts::font(-3);
        const QFontMetrics meta_fm(meta_font);
        p->setFont(meta_font);
        p->setPen(QColor(ui::colors::TEXT_TERTIARY()));
        p->drawText(QRect(x, r.top() + 6 + title_fm.height(), w, meta_fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    meta_fm.elidedText(rap_meta(*a, model_->with_run()), Qt::ElideRight, w));
        p->restore();
    }

  private:
    RapModel* model_;
};

/// A series artefact drawn as lines: the first column is the x axis (shown as
/// labels), each numeric column one line. Colours from the theme's chart palette.
class RapSeriesChart : public QWidget {
  public:
    explicit RapSeriesChart(QWidget* parent) : QWidget(parent) { setMinimumHeight(120); }

    void set_table(const al::ArtefactTable& t) {
        table_ = t;
        update();
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), QColor(ui::colors::BG_SURFACE()));
        const QFont f = ui::fonts::font(-4);
        const QFontMetrics fm(f);
        p.setFont(f);
        const QRect plot = rect().adjusted(8, 8, -8, -(fm.height() + 8));
        if (table_.rows.size() < 2 || table_.columns.size() < 2 || plot.width() < 20 || plot.height() < 20) {
            p.setPen(QColor(ui::colors::TEXT_DIM()));
            p.drawText(rect(), Qt::AlignCenter, RunArtefactsPanel::tr("Not enough points to draw."));
            return;
        }
        QVector<QVector<double>> series;
        QStringList names;
        double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest();
        for (int c = 1; c < table_.columns.size() && series.size() < 4; ++c) {
            QVector<double> ys;
            bool ok_all = true;
            for (const QStringList& row : table_.rows) {
                bool ok = false;
                QString v = row.value(c);
                v.remove(QLatin1Char(','));
                v.remove(QLatin1Char('%'));
                const double y = QLocale::c().toDouble(v.trimmed(), &ok);
                if (!ok) {
                    ok_all = false;
                    break;
                }
                ys.append(y);
                lo = std::min(lo, y);
                hi = std::max(hi, y);
            }
            if (ok_all) {
                series.append(ys);
                names << table_.columns.at(c);
            }
        }
        if (series.isEmpty()) {
            p.setPen(QColor(ui::colors::TEXT_DIM()));
            p.drawText(rect(), Qt::AlignCenter, RunArtefactsPanel::tr("No numeric column to draw."));
            return;
        }
        if (hi - lo < 1e-12)
            hi = lo + 1;
        p.setPen(QPen(QColor(ui::colors::BORDER_DIM()), 1));
        p.drawRect(plot);
        const int n = static_cast<int>(table_.rows.size());
        for (int s = 0; s < series.size(); ++s) {
            QPainterPath path;
            for (int i = 0; i < n; ++i) {
                const double x = plot.left() + plot.width() * (n == 1 ? 0.5 : double(i) / (n - 1));
                const double y = plot.bottom() - plot.height() * (series[s][i] - lo) / (hi - lo);
                if (i == 0)
                    path.moveTo(x, y);
                else
                    path.lineTo(x, y);
            }
            p.setPen(QPen(ui::colors::chart_color(s), 1.6));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
        p.setPen(QColor(ui::colors::TEXT_TERTIARY()));
        const int label_y = plot.bottom() + 4;
        p.drawText(QRect(plot.left(), label_y, plot.width() / 2, fm.height()), Qt::AlignLeft,
                   table_.rows.first().value(0));
        p.drawText(QRect(plot.center().x(), label_y, plot.width() / 2, fm.height()), Qt::AlignRight,
                   table_.rows.last().value(0));
        p.drawText(plot.adjusted(4, 2, -4, -2), Qt::AlignTop | Qt::AlignLeft,
                   QLocale().toString(hi, 'g', 6) + QStringLiteral("  ") + names.join(QStringLiteral(" / ")));
        p.drawText(plot.adjusted(4, 2, -4, -2), Qt::AlignBottom | Qt::AlignLeft, QLocale().toString(lo, 'g', 6));
    }

  private:
    al::ArtefactTable table_;
};

QString rap_style() {
    return QStringLiteral(
               "#runArtefactsPanel{background:%1;}"
               "#runArtefactsPanel QLabel{background:transparent;border:none;color:%2;font-size:%3px;}"
               "#runArtefactsPanel QLabel#rapCount{color:%4;}"
               "#runArtefactsPanel QLabel#rapStatus[error=\"true\"]{color:%5;}"
               "#runArtefactsPanel QLabel#rapPreviewNote{color:%4;font-style:italic;}"
               "#runArtefactsPanel QPushButton{background:%6;color:%2;border:1px solid %7;padding:3px 8px;"
               "font-size:%3px;font-weight:700;}"
               "#runArtefactsPanel QPushButton:hover{color:%8;border-color:%9;}"
               "#runArtefactsPanel QPushButton:focus{border-color:%9;}"
               "#runArtefactsPanel QPushButton:disabled{color:%10;background:transparent;}")
               .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-3), ui::colors::TEXT_TERTIARY(),
                    ui::colors::DANGER(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
                    ui::colors::AMBER())
               .arg(ui::colors::TEXT_DIM()) +
           QStringLiteral(
               "#runArtefactsPanel QPushButton:checked{background:%1;color:%2;border-color:%1;}"
               "#runArtefactsPanel QPushButton#rapDanger{color:%3;border-color:%3;}"
               "#runArtefactsPanel QFrame#rapConfirm{background:%4;border:1px solid %3;}"
               "#runArtefactsPanel QListView{background:%5;border:1px solid %6;outline:none;}"
               "#runArtefactsPanel QTextBrowser{background:%5;color:%7;border:1px solid %6;font-size:%8px;}"
               "#runArtefactsPanel QTableView{background:%5;alternate-background-color:%9;color:%7;gridline-color:%6;"
               "border:1px solid %6;font-size:%8px;selection-background-color:%10;selection-color:%7;}")
               .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::DANGER(),
                    ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::colors::BG_BASE(), ui::colors::BORDER_DIM(),
                    ui::colors::TEXT_PRIMARY(), ui::fonts::px(-2), ui::colors::ROW_ALT())
               .arg(ui::colors::BG_HOVER()) +
           QStringLiteral(
               "#runArtefactsPanel QHeaderView::section{background:%1;color:%2;border:none;border-bottom:1px solid %3;"
               "border-right:1px solid %3;padding:2px 6px;font-size:%4px;font-weight:700;}"
               "#runArtefactsPanel QSplitter::handle{background:%3;}")
               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::fonts::px(-3));
}

void rap_set_error(QLabel* l, bool error) {
    if (l->property("error").toBool() == error)
        return;
    l->setProperty("error", error);
    l->style()->unpolish(l);
    l->style()->polish(l);
}

} // namespace run_artefacts_panel_detail

using namespace run_artefacts_panel_detail;

// ── Impl ─────────────────────────────────────────────────────────────────────

struct RunArtefactsPanel::Impl {
    RunArtefactsPanel* q = nullptr;
    QString run_id;
    bool pinned_scope = false;
    QString selected_id;
    quint64 preview_request = 0; ///< a load answering an older selection is dropped
    bool preview_dirty = false;  ///< the theme changed: re-render on appearance_settled
    QString preview_markdown;    ///< what the text preview shows (re-rendered for a theme change)
    bool preview_is_markdown = false;
    agt::ArtefactContent content;

    QPushButton* run_btn = nullptr;
    QPushButton* pinned_btn = nullptr;
    QLabel* count = nullptr;
    QStackedWidget* stack = nullptr; // 0 empty state, 1 content
    ui::EmptyState* empty = nullptr;
    QSplitter* split = nullptr;
    QListView* list = nullptr;
    RapModel* model = nullptr;
    QStackedWidget* preview = nullptr; // 0 message, 1 text, 2 table, 3 chart+table, 4 image
    QLabel* preview_msg = nullptr;
    QTextBrowser* text = nullptr;
    QTableView* table = nullptr;
    ArtefactTableModel* table_model = nullptr;
    QWidget* chart_page = nullptr;
    RapSeriesChart* chart = nullptr;
    QTableView* chart_table = nullptr;
    ArtefactTableModel* chart_model = nullptr;
    QLabel* image = nullptr;
    QLabel* preview_note = nullptr;
    QPushButton* open_btn = nullptr;
    QPushButton* save_btn = nullptr;
    QPushButton* copy_btn = nullptr;
    ui::AddToReportButton* report_btn = nullptr; ///< target menu: open report / recent / new
    QPushButton* notes_btn = nullptr;
    QPushButton* pin_btn = nullptr;
    QPushButton* delete_btn = nullptr;
    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QPushButton* confirm_yes = nullptr;
    QPushButton* confirm_no = nullptr;
    QLabel* status = nullptr;
    QPushButton* status_action = nullptr;
    QString status_screen; ///< what status_action opens
    QTimer* status_timer = nullptr;

    void build();
    void wire();
    void retranslate();
    void reload();
    void update_empty();
    void select(const QString& id);
    void load_preview();
    void show_content(const agt::ArtefactContent& c);
    void render_text();
    void update_actions();
    void show_status(const QString& text, bool error, const QString& action_text = {}, const QString& screen = {});
    void save_as();
    void copy();
    /// What "Add to report" sends: the selected artefact as loaded for the preview.
    report::ReportPayload report_payload() const;
    void send_to_notes();
    void ask_delete();
    const agt::RunArtefact* current() const { return model->at(model->row_of(selected_id)); }
};

void RunArtefactsPanel::Impl::build() {
    q->setObjectName(QStringLiteral("runArtefactsPanel"));
    q->setAttribute(Qt::WA_StyledBackground, true);
    q->setStyleSheet(rap_style()); // before the children exist

    auto* vl = new QVBoxLayout(q);
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(6);

    auto* bar = new QWidget(q);
    auto* flow = new rv::RvFlowLayout(bar, 4, 4);
    run_btn = new QPushButton(bar);
    pinned_btn = new QPushButton(bar);
    for (QPushButton* b : {run_btn, pinned_btn}) {
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        flow->addWidget(b);
    }
    run_btn->setChecked(true);
    count = new QLabel(bar);
    count->setObjectName(QStringLiteral("rapCount"));
    flow->addWidget(count);
    vl->addWidget(bar);

    stack = new QStackedWidget(q);
    empty = new ui::EmptyState(stack);
    empty->set_compact(true);
    stack->addWidget(empty);

    auto* content_page = new QWidget(stack);
    auto* cl = new QVBoxLayout(content_page);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    split = new QSplitter(Qt::Vertical, content_page);
    split->setChildrenCollapsible(false);
    list = new QListView(split);
    list->setProperty("fincept_agent_risk_keys", "Delete=delete"); // agents: Delete deletes the selected artefact
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list->setMouseTracking(true);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    model = new RapModel(list);
    list->setModel(model);
    list->setItemDelegate(new RapDelegate(model, list));
    split->addWidget(list);

    preview = new QStackedWidget(split);
    preview_msg = new QLabel(preview);
    preview_msg->setAlignment(Qt::AlignCenter);
    preview_msg->setWordWrap(true);
    preview->addWidget(preview_msg);
    text = new QTextBrowser(preview);
    text->setOpenExternalLinks(true);
    text->setReadOnly(true);
    preview->addWidget(text);
    table = new QTableView(preview);
    table_model = new ArtefactTableModel(table);
    table->setModel(table_model);
    preview->addWidget(table);
    chart_page = new QWidget(preview);
    auto* chart_vl = new QVBoxLayout(chart_page);
    chart_vl->setContentsMargins(0, 0, 0, 0);
    chart_vl->setSpacing(4);
    chart = new RapSeriesChart(chart_page);
    chart_table = new QTableView(chart_page);
    chart_model = new ArtefactTableModel(chart_table);
    chart_table->setModel(chart_model);
    chart_vl->addWidget(chart, 1);
    chart_vl->addWidget(chart_table, 1);
    preview->addWidget(chart_page);
    image = new QLabel(preview);
    image->setAlignment(Qt::AlignCenter);
    image->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    preview->addWidget(image);
    for (QTableView* t : {table, chart_table}) {
        t->setSortingEnabled(true);
        t->setAlternatingRowColors(true);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setSelectionBehavior(QAbstractItemView::SelectRows);
        t->verticalHeader()->setVisible(false);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setWordWrap(false);
    }
    split->addWidget(preview);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    cl->addWidget(split, 1);

    preview_note = new QLabel(content_page);
    preview_note->setObjectName(QStringLiteral("rapPreviewNote"));
    preview_note->setWordWrap(true);
    preview_note->setVisible(false);
    cl->addWidget(preview_note);

    auto* actions = new QWidget(content_page);
    auto* af = new rv::RvFlowLayout(actions, 4, 4);
    open_btn = new QPushButton(actions);
    save_btn = new QPushButton(actions);
    copy_btn = new QPushButton(actions);
    report_btn = new ui::AddToReportButton(QString::fromLatin1(agt::kReportSourceLabel), actions);
    report_btn->set_payload_provider([this]() { return report_payload(); });
    notes_btn = new QPushButton(actions);
    pin_btn = new QPushButton(actions);
    delete_btn = new QPushButton(actions);
    delete_btn->setProperty("fincept_agent_risk", "delete"); // agents: deletes the artefact
    delete_btn->setObjectName(QStringLiteral("rapDanger"));
    for (QPushButton* b : {open_btn, save_btn, copy_btn}) {
        b->setCursor(Qt::PointingHandCursor);
        af->addWidget(b);
    }
    af->addWidget(report_btn);
    for (QPushButton* b : {notes_btn, pin_btn, delete_btn}) {
        b->setCursor(Qt::PointingHandCursor);
        af->addWidget(b);
    }
    cl->addWidget(actions);

    confirm = new QFrame(content_page);
    confirm->setObjectName(QStringLiteral("rapConfirm"));
    auto* cvl = new QVBoxLayout(confirm);
    cvl->setContentsMargins(8, 6, 8, 6);
    cvl->setSpacing(4);
    confirm_text = new QLabel(confirm);
    confirm_text->setWordWrap(true);
    cvl->addWidget(confirm_text);
    auto* chl = new QHBoxLayout;
    confirm_yes = new QPushButton(confirm);
    confirm_yes->setObjectName(QStringLiteral("rapDanger"));
    confirm_no = new QPushButton(confirm);
    chl->addWidget(confirm_yes);
    chl->addWidget(confirm_no);
    chl->addStretch();
    cvl->addLayout(chl);
    confirm->setVisible(false);
    cl->addWidget(confirm);

    auto* status_row = new QWidget(content_page);
    auto* sl = new QHBoxLayout(status_row);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(6);
    status = new QLabel(status_row);
    status->setObjectName(QStringLiteral("rapStatus"));
    status->setWordWrap(true);
    status_action = new QPushButton(status_row);
    status_action->setCursor(Qt::PointingHandCursor);
    status_action->setVisible(false);
    sl->addWidget(status, 1);
    sl->addWidget(status_action);
    cl->addWidget(status_row);
    status->setVisible(false);

    stack->addWidget(content_page);
    vl->addWidget(stack, 1);

    status_timer = new QTimer(q);
    status_timer->setSingleShot(true);
    status_timer->setInterval(kRapStatusMs);
}

void RunArtefactsPanel::Impl::wire() {
    auto& svc = rap_svc();
    QObject::connect(&svc, &agt::ArtefactService::artefacts_changed, q, [this](const QString& run) {
        if (pinned_scope || run.isEmpty() || run == run_id)
            reload();
    });
    QObject::connect(&agt::TaskRunService::instance(), &agt::TaskRunService::run_updated, q, [this](const QString& id) {
        if (id == run_id && !pinned_scope)
            update_empty(); // a running run ended, or was deleted
    });
    QObject::connect(run_btn, &QPushButton::clicked, q, [this]() {
        pinned_scope = false;
        run_btn->setChecked(true);
        pinned_btn->setChecked(false);
        reload();
    });
    QObject::connect(pinned_btn, &QPushButton::clicked, q, [this]() {
        pinned_scope = true;
        run_btn->setChecked(false);
        pinned_btn->setChecked(true);
        reload();
    });
    QObject::connect(list->selectionModel(), &QItemSelectionModel::currentChanged, q,
                     [this](const QModelIndex& index) { select(index.data(kRapIdRole).toString()); });
    QObject::connect(list, &QListView::doubleClicked, q, [this]() { open_btn->click(); });
    QObject::connect(empty, &ui::EmptyState::primary_clicked, q, [this]() {
        const QString action = empty->property("rapAction").toString();
        if (action == QLatin1String("pinned")) {
            pinned_btn->click();
        } else if (action == QLatin1String("save_reply")) {
            QPointer<RunArtefactsPanel> self(q);
            rap_svc().save_final_report(run_id, q, [self](bool ok, const QString& id, const QString& err) {
                if (!self)
                    return;
                Impl* d = self->d_.get();
                if (ok)
                    d->select(id);
                else
                    d->show_status(err, true);
                d->update_empty();
            });
        } else if (action == QLatin1String("new_run")) {
            emit q->new_run_requested();
        }
    });
    QObject::connect(empty, &ui::EmptyState::secondary_clicked, q, [this]() { emit q->new_run_requested(); });

    QObject::connect(open_btn, &QPushButton::clicked, q, [this]() {
        const agt::RunArtefact* a = current();
        const QString path = a ? agt::ArtefactService::file_path(*a) : QString();
        if (path.isEmpty())
            return;
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            show_status(RunArtefactsPanel::tr("No application could open %1.").arg(QFileInfo(path).fileName()), true);
    });
    QObject::connect(save_btn, &QPushButton::clicked, q, [this]() { save_as(); });
    QObject::connect(copy_btn, &QPushButton::clicked, q, [this]() { copy(); });
    QObject::connect(notes_btn, &QPushButton::clicked, q, [this]() { send_to_notes(); });
    QObject::connect(pin_btn, &QPushButton::clicked, q, [this]() {
        if (const agt::RunArtefact* a = current())
            rap_svc().set_pinned(a->id, !a->pinned);
    });
    QObject::connect(delete_btn, &QPushButton::clicked, q, [this]() { ask_delete(); });
    QObject::connect(confirm_no, &QPushButton::clicked, q, [this]() {
        confirm->setVisible(false);
        list->setFocus();
    });
    QObject::connect(status_action, &QPushButton::clicked, q, [this]() {
        if (!status_screen.isEmpty())
            emit q->open_screen_requested(status_screen);
    });
    QObject::connect(status_timer, &QTimer::timeout, q, [this]() {
        if (!status->property("error").toBool()) {
            status->setVisible(false);
            status_action->setVisible(false);
        }
    });
    new rv::RvKeyHook(list, [this](const QKeyEvent* ke, bool apply) {
        if (ke->key() == Qt::Key_Delete && delete_btn->isEnabled()) {
            if (apply)
                ask_delete();
            return true;
        }
        if (ke->matches(QKeySequence::Copy) && copy_btn->isEnabled()) {
            if (apply)
                copy();
            return true;
        }
        return false;
    });
    for (QPushButton* b : {confirm_yes, confirm_no})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                confirm->setVisible(false);
            return true;
        });
    // Rendered Markdown carries the theme's colours: render again once the new
    // theme has settled (never inside theme_changed), and re-measure the rows.
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, q,
                     [this](const ui::ThemeTokens&) { preview_dirty = true; });
    QObject::connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, q, [this]() {
        const QString keep = selected_id;
        model->relayout();
        if (const int row = model->row_of(keep); row >= 0)
            list->setCurrentIndex(model->index(row));
        if (preview_dirty && preview_is_markdown)
            render_text();
        preview_dirty = false;
        chart->update();
    });
    new rv::RvEventHook(image, [this](QEvent::Type t) {
        if (t == QEvent::Resize && !content.image.isNull() && preview->currentWidget() == image)
            image->setPixmap(QPixmap::fromImage(content.image.scaled(image->size(), Qt::KeepAspectRatio,
                                                                      Qt::SmoothTransformation)));
    });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::LanguageChange) {
            retranslate();
            reload();
        }
    });
}

void RunArtefactsPanel::Impl::retranslate() {
    run_btn->setText(RunArtefactsPanel::tr("THIS RUN"));
    run_btn->setToolTip(RunArtefactsPanel::tr("Reports, tables and results of the selected run"));
    const int pinned = static_cast<int>(rap_svc().pinned_artefacts().size());
    pinned_btn->setText(pinned > 0 ? RunArtefactsPanel::tr("PINNED · %1").arg(pinned) : RunArtefactsPanel::tr("PINNED"));
    pinned_btn->setToolTip(RunArtefactsPanel::tr("Pinned artefacts of every run — kept when their run is deleted"));
    open_btn->setText(RunArtefactsPanel::tr("OPEN"));
    save_btn->setText(RunArtefactsPanel::tr("SAVE AS…"));
    copy_btn->setText(RunArtefactsPanel::tr("COPY"));
    notes_btn->setText(RunArtefactsPanel::tr("SEND TO NOTES"));
    delete_btn->setText(RunArtefactsPanel::tr("DELETE"));
    confirm_yes->setText(RunArtefactsPanel::tr("DELETE"));
    confirm_no->setText(RunArtefactsPanel::tr("KEEP"));
    list->setAccessibleName(RunArtefactsPanel::tr("Run artefacts"));
    text->setAccessibleName(RunArtefactsPanel::tr("Artefact preview"));
    update_actions();
}

void RunArtefactsPanel::Impl::reload() {
    QVector<agt::RunArtefact> items = pinned_scope ? rap_svc().pinned_artefacts() : rap_svc().artefacts(run_id);
    if (!pinned_scope)
        std::sort(items.begin(), items.end(), rap_before);
    model->set_items(items, pinned_scope);
    qint64 bytes = 0;
    for (const agt::RunArtefact& a : std::as_const(items))
        bytes += std::max<qint64>(0, a.bytes);
    count->setText(items.isEmpty() ? QString()
                                   : RunArtefactsPanel::tr("%n artefact(s)", nullptr, static_cast<int>(items.size())) +
                                         (bytes > 0 ? QStringLiteral(" · ") + rap_size(bytes) : QString()));
    const int pinned = static_cast<int>(rap_svc().pinned_artefacts().size());
    pinned_btn->setText(pinned > 0 ? RunArtefactsPanel::tr("PINNED · %1").arg(pinned) : RunArtefactsPanel::tr("PINNED"));
    update_empty();
    if (items.isEmpty()) {
        selected_id.clear();
        return;
    }
    int row = model->row_of(selected_id);
    if (row < 0)
        row = 0;
    const QString id = model->at(row)->id;
    list->setCurrentIndex(model->index(row));
    if (id != selected_id || preview->currentIndex() == 0)
        select(id);
    else
        update_actions();
}

void RunArtefactsPanel::Impl::update_empty() {
    if (model->rowCount() > 0) {
        stack->setCurrentIndex(1);
        return;
    }
    stack->setCurrentIndex(0);
    const int pinned = static_cast<int>(rap_svc().pinned_artefacts().size());
    ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("▤");
    spec.glyph_tone = ui::EmptyState::Tone::Accent;
    QString action;
    const agt::TaskRun* r = run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(run_id);
    if (pinned_scope) {
        spec.title = RunArtefactsPanel::tr("No pinned artefacts");
        spec.message = RunArtefactsPanel::tr("Pin a report or table to keep it when its run is deleted. Pinned artefacts "
                                             "of every run are listed here.");
        spec.primary = RunArtefactsPanel::tr("NEW RUN");
        action = QStringLiteral("new_run");
    } else if (!r) {
        spec.title = RunArtefactsPanel::tr("Artefacts");
        spec.message = RunArtefactsPanel::tr("Reports, tables and results a run produces are kept here — select a run in "
                                             "RUNS, or start a task.");
        if (pinned > 0) {
            spec.primary = RunArtefactsPanel::tr("SHOW PINNED (%1)").arg(pinned);
            spec.secondary = RunArtefactsPanel::tr("NEW RUN");
            action = QStringLiteral("pinned");
        } else {
            spec.primary = RunArtefactsPanel::tr("NEW RUN");
            action = QStringLiteral("new_run");
        }
    } else if (!agt::is_terminal(r->status)) {
        spec.title = RunArtefactsPanel::tr("Nothing yet");
        spec.message = r->plan_first || r->engine == QLatin1String(agt::kEngineLocal)
                           ? RunArtefactsPanel::tr("Each step's deliverable and its tables appear here as the step "
                                                   "finishes; the final report when the run ends.")
                           : RunArtefactsPanel::tr("When the reply is complete you can keep it here as a report.");
        spec.note = rv::run_status_pill(r->status);
        spec.note_tone = ui::EmptyState::Tone::Info;
        if (pinned > 0) {
            spec.primary = RunArtefactsPanel::tr("SHOW PINNED (%1)").arg(pinned);
            action = QStringLiteral("pinned");
        }
    } else if (!r->final_text.trimmed().isEmpty()) {
        spec.title = RunArtefactsPanel::tr("No artefacts");
        spec.message = RunArtefactsPanel::tr("This reply's answer is in the conversation. Keep it as a Markdown report "
                                             "to export it, add it to a report or send it to Notes.");
        spec.primary = RunArtefactsPanel::tr("SAVE REPLY AS ARTEFACT");
        action = QStringLiteral("save_reply");
    } else {
        spec.title = RunArtefactsPanel::tr("No artefacts");
        spec.message = RunArtefactsPanel::tr("This run ended without a result to keep.");
        spec.primary = RunArtefactsPanel::tr("NEW RUN");
        action = QStringLiteral("new_run");
    }
    empty->apply(spec);
    empty->setProperty("rapAction", action);
}

void RunArtefactsPanel::Impl::select(const QString& id) {
    if (id.isEmpty())
        return;
    selected_id = id;
    confirm->setVisible(false);
    load_preview();
    update_actions();
}

void RunArtefactsPanel::Impl::load_preview() {
    const quint64 request = ++preview_request;
    preview_msg->setText(RunArtefactsPanel::tr("Loading…"));
    preview->setCurrentIndex(0);
    preview_note->setVisible(false);
    content = {};
    QPointer<RunArtefactsPanel> self(q);
    rap_svc().load_content(selected_id, q, [self, request](const agt::ArtefactContent& c) {
        if (!self || request != self->d_->preview_request)
            return; // another artefact was selected meanwhile
        self->d_->show_content(c);
    });
}

void RunArtefactsPanel::Impl::show_content(const agt::ArtefactContent& c) {
    content = c;
    preview_is_markdown = false;
    preview_note->setVisible(false);
    if (!c.ok) {
        preview_msg->setText(c.error.isEmpty() ? RunArtefactsPanel::tr("This artefact cannot be shown.") : c.error);
        preview->setCurrentIndex(0);
        update_actions();
        return;
    }
    const agt::RunArtefact* a = current();
    const QString kind = a ? a->kind : c.kind;
    if (!c.image.isNull()) {
        image->setPixmap(QPixmap::fromImage(c.image.scaled(image->size().expandedTo(QSize(64, 64)), Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation)));
        preview->setCurrentWidget(image);
    } else if (kind == QLatin1String(agt::kArtefactSeries) && !c.table.isEmpty()) {
        chart->set_table(c.table);
        chart_model->set_table(c.table);
        preview->setCurrentWidget(chart_page);
    } else if (!c.table.isEmpty() && kind != QLatin1String(agt::kArtefactReport)) {
        table_model->set_table(c.table);
        const QFontMetrics fm(table->font());
        table->verticalHeader()->setDefaultSectionSize(fm.height() + 6);
        for (int col = 0; col < table_model->columnCount(); ++col)
            table->setColumnWidth(col, std::clamp(fm.horizontalAdvance(table_model->headerData(col, Qt::Horizontal,
                                                                                               Qt::DisplayRole).toString()) + 28,
                                                  fm.averageCharWidth() * 8, fm.averageCharWidth() * 30));
        preview->setCurrentWidget(table);
        preview_note->setText(RunArtefactsPanel::tr("%1 rows × %2 columns — click a column to sort")
                                  .arg(QLocale().toString(table_model->rowCount()))
                                  .arg(table_model->columnCount()));
        preview_note->setVisible(true);
    } else if (!c.text.isEmpty()) {
        preview_markdown = c.text.left(kRapPreviewMaxChars);
        preview_is_markdown = kind == QLatin1String(agt::kArtefactReport);
        render_text();
        preview->setCurrentWidget(text);
        if (c.text.size() > kRapPreviewMaxChars) {
            preview_note->setText(RunArtefactsPanel::tr("The preview shows the beginning; open or save the file for all of it."));
            preview_note->setVisible(true);
        }
    } else {
        preview_msg->setText(c.file_path.isEmpty() ? RunArtefactsPanel::tr("Nothing to preview.")
                                                   : RunArtefactsPanel::tr("No preview for this file type — OPEN shows it "
                                                                           "in its application."));
        preview->setCurrentIndex(0);
    }
    update_actions();
}

void RunArtefactsPanel::Impl::render_text() {
    if (preview_is_markdown)
        text->setHtml(ui::MarkdownRenderer::render(preview_markdown));
    else
        text->setPlainText(preview_markdown);
}

void RunArtefactsPanel::Impl::update_actions() {
    const agt::RunArtefact* a = current();
    const bool has = a != nullptr;
    const QString path = has ? agt::ArtefactService::file_path(*a) : QString();
    const bool loaded = has && content.ok;
    open_btn->setEnabled(!path.isEmpty());
    open_btn->setToolTip(path.isEmpty() ? RunArtefactsPanel::tr("This artefact has no file of its own")
                                        : RunArtefactsPanel::tr("Open %1 in its application").arg(QFileInfo(path).fileName()));
    save_btn->setEnabled(has);
    save_btn->setToolTip(RunArtefactsPanel::tr("Save a copy (tables as CSV or Excel, reports as Markdown)"));
    copy_btn->setEnabled(loaded);
    copy_btn->setToolTip(loaded ? RunArtefactsPanel::tr("Copy to the clipboard (tables tab-separated, for spreadsheets)")
                                : RunArtefactsPanel::tr("Wait for the preview to load"));
    // The payload is built from the loaded preview: nothing to send until it is there.
    const bool reportable = loaded && !report_payload().is_empty();
    report_btn->setEnabled(reportable);
    report_btn->setToolTip(!has      ? QString()
                           : !loaded ? RunArtefactsPanel::tr("Wait for the preview to load")
                           : !reportable
                               ? RunArtefactsPanel::tr("This file cannot go into a report — OPEN or SAVE AS… instead")
                               : RunArtefactsPanel::tr("Add it to the report open in the Report Builder; the arrow "
                                                       "picks another report or a new one"));
    notes_btn->setEnabled(has && a->kind != QLatin1String(agt::kArtefactImage));
    notes_btn->setToolTip(has && a->kind == QLatin1String(agt::kArtefactImage)
                              ? RunArtefactsPanel::tr("Images cannot be sent to Notes")
                              : RunArtefactsPanel::tr("Create a note with this content and a link back to the run"));
    pin_btn->setEnabled(has);
    pin_btn->setText(has && a->pinned ? RunArtefactsPanel::tr("UNPIN") : RunArtefactsPanel::tr("PIN"));
    pin_btn->setToolTip(has && a->pinned ? RunArtefactsPanel::tr("Unpinned, it is deleted with its run")
                                         : RunArtefactsPanel::tr("Keep it when its run is deleted and when old artefacts "
                                                                 "are cleaned up"));
    delete_btn->setEnabled(has);
    delete_btn->setToolTip(RunArtefactsPanel::tr("Delete this artefact and its file (Del)"));
}

void RunArtefactsPanel::Impl::show_status(const QString& message, bool error, const QString& action_text,
                                          const QString& screen) {
    status->setText(message);
    status->setVisible(!message.isEmpty());
    rap_set_error(status, error);
    status_screen = screen;
    status_action->setText(action_text);
    status_action->setVisible(!action_text.isEmpty());
    status_timer->start();
}

void RunArtefactsPanel::Impl::save_as() {
    const agt::RunArtefact* a = current();
    if (!a)
        return;
    QStringList filters;
    QString ext;
    if (a->kind == QLatin1String(agt::kArtefactTable) || a->kind == QLatin1String(agt::kArtefactSeries)) {
        filters << RunArtefactsPanel::tr("CSV (*.csv)");
#ifdef FINCEPT_HAS_QXLSX
        filters << RunArtefactsPanel::tr("Excel workbook (*.xlsx)");
#endif
        filters << RunArtefactsPanel::tr("Markdown table (*.md)") << RunArtefactsPanel::tr("JSON (*.json)");
        ext = QStringLiteral("csv");
    } else if (a->kind == QLatin1String(agt::kArtefactReport)) {
        filters << RunArtefactsPanel::tr("Markdown (*.md)") << RunArtefactsPanel::tr("Text (*.txt)");
        ext = QStringLiteral("md");
    } else if (a->kind == QLatin1String(agt::kArtefactJson)) {
        filters << RunArtefactsPanel::tr("JSON (*.json)");
        ext = QStringLiteral("json");
        if (!content.table.isEmpty()) {
            filters << RunArtefactsPanel::tr("CSV (*.csv)");
#ifdef FINCEPT_HAS_QXLSX
            filters << RunArtefactsPanel::tr("Excel workbook (*.xlsx)");
#endif
        }
    } else {
        ext = QFileInfo(a->stored_name).suffix();
        filters << RunArtefactsPanel::tr("All files (*)");
    }
    const QString suggested = al::safe_file_stem(a->title) + (ext.isEmpty() ? QString() : QLatin1Char('.') + ext);
    // Non-modal: open() returns at once; the choice arrives through fileSelected.
    auto* dlg = new QFileDialog(q, RunArtefactsPanel::tr("Save artefact"), suggested);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    dlg->setNameFilters(filters);
    if (!ext.isEmpty())
        dlg->setDefaultSuffix(ext);
    const QString id = a->id;
    const QString run = a->run_id;
    QPointer<RunArtefactsPanel> self(q);
    QObject::connect(dlg, &QFileDialog::fileSelected, q, [self, id, run](const QString& path) {
        if (!self || path.isEmpty())
            return;
        self->d_->show_status(RunArtefactsPanel::tr("Saving…"), false);
        rap_svc().export_artefact(run, id, path, self, [self, path](bool ok, const QString& err) {
            if (!self)
                return;
            self->d_->show_status(ok ? RunArtefactsPanel::tr("Saved to %1").arg(QDir::toNativeSeparators(path)) : err, !ok);
        });
    });
    dlg->open();
}

void RunArtefactsPanel::Impl::copy() {
    QClipboard* cb = QApplication::clipboard();
    if (!cb || !content.ok)
        return;
    if (!content.image.isNull()) {
        cb->setImage(content.image);
    } else if (!content.table.isEmpty() && (current() && current()->kind != QLatin1String(agt::kArtefactReport))) {
        // Tab-separated: pastes as cells into a spreadsheet.
        QStringList lines;
        lines << content.table.columns.join(QLatin1Char('\t'));
        for (const QStringList& r : std::as_const(content.table.rows))
            lines << r.join(QLatin1Char('\t'));
        cb->setText(lines.join(QLatin1Char('\n')));
    } else {
        cb->setText(content.text);
    }
    show_status(RunArtefactsPanel::tr("Copied to the clipboard."), false);
}

report::ReportPayload RunArtefactsPanel::Impl::report_payload() const {
    const agt::RunArtefact* a = current();
    if (!a || !content.ok)
        return {};
    agt::ArtefactReportContent c;
    c.text = content.text;
    c.table = content.table;
    c.image = content.image_bytes;
    c.image_format = content.image_format;
    return agt::artefact_report_payload(*a, c);
}

void RunArtefactsPanel::Impl::send_to_notes() {
    const agt::RunArtefact* a = current();
    if (!a)
        return;
    notes_btn->setEnabled(false);
    QPointer<RunArtefactsPanel> self(q);
    rap_svc().send_to_notes(a->id, q, [self](bool ok, int, const QString& err) {
        if (!self)
            return;
        Impl* d = self->d_.get();
        d->update_actions();
        if (ok)
            d->show_status(RunArtefactsPanel::tr("Saved as a note (tag agent-run)."), false,
                           RunArtefactsPanel::tr("OPEN NOTES"), QStringLiteral("notes"));
        else
            d->show_status(err, true);
    });
}

void RunArtefactsPanel::Impl::ask_delete() {
    const agt::RunArtefact* a = current();
    if (!a)
        return;
    confirm_text->setText(a->pinned ? RunArtefactsPanel::tr("Delete the pinned artefact “%1” and its file? This cannot "
                                                            "be undone.")
                                          .arg(a->title)
                                    : RunArtefactsPanel::tr("Delete “%1” and its file? This cannot be undone.").arg(a->title));
    const QString id = a->id;
    QObject::disconnect(confirm_yes, &QPushButton::clicked, nullptr, nullptr);
    QPointer<RunArtefactsPanel> self(q);
    QObject::connect(confirm_yes, &QPushButton::clicked, q, [self, id]() {
        if (!self)
            return;
        self->d_->confirm->setVisible(false);
        rap_svc().delete_artefact(id, self, [self](bool ok, const QString& err) {
            if (self)
                self->d_->show_status(ok ? RunArtefactsPanel::tr("Artefact deleted.") : err, !ok);
        });
    });
    confirm->setVisible(true);
    confirm_yes->setFocus();
}

// ── RunArtefactsPanel ────────────────────────────────────────────────────────

RunArtefactsPanel::RunArtefactsPanel(QWidget* parent) : QWidget(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    d_->retranslate();
    d_->reload();
}

RunArtefactsPanel::~RunArtefactsPanel() = default;

void RunArtefactsPanel::set_run(const QString& run_id) {
    const bool changed = run_id != d_->run_id;
    d_->run_id = run_id;
    // An older task run (finished before artefacts existed): make its artefacts now.
    if (const agt::TaskRun* r = run_id.isEmpty() ? nullptr : agt::TaskRunService::instance().find_run(run_id);
        r && agt::is_terminal(r->status))
        rap_svc().ensure_extracted(run_id);
    if (changed && d_->pinned_scope) {
        // A run picked in RUNS: show its artefacts.
        d_->pinned_scope = false;
        d_->run_btn->setChecked(true);
        d_->pinned_btn->setChecked(false);
    }
    if (changed)
        d_->selected_id.clear();
    d_->reload();
}

QString RunArtefactsPanel::run_id() const {
    return d_->run_id;
}

} // namespace fincept::chat_mode
