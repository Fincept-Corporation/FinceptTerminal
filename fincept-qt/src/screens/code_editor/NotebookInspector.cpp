// NotebookInspector.cpp — see NotebookInspector.h. Styled by the screen sheet.

#include "screens/code_editor/NotebookInspector.h"

#include "screens/code_editor/NotebookOutputs.h"
#include "screens/code_editor/NotebookStyle.h"
#include "ui/widgets/EmptyState.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

namespace colors = ui::colors;

namespace {

constexpr int kMaxRuns = 500;

QString esc(const QString& s) {
    return s.toHtmlEscaped();
}

QString fmt_ms(qint64 ms) {
    if (ms < 1000)
        return NotebookInspector::tr("%1 ms").arg(ms);
    return NotebookInspector::tr("%1 s").arg(ms / 1000.0, 0, 'f', ms < 10000 ? 2 : 1);
}

} // namespace

NotebookInspector::NotebookInspector(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("nbInspector"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("nbPanelHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setMinimumHeight(30);
    auto* h = new QHBoxLayout(head);
    h->setContentsMargins(6, 0, 4, 0);
    h->setSpacing(2);
    for (int i = 0; i < 3; ++i) {
        tabs_[i] = new QPushButton(head);
        tabs_[i]->setObjectName(QStringLiteral("nbTab"));
        tabs_[i]->setCursor(Qt::PointingHandCursor);
        connect(tabs_[i], &QPushButton::clicked, this, [this, i]() { show_tab(static_cast<Tab>(i)); });
        h->addWidget(tabs_[i]);
    }
    h->addStretch(1);
    auto* close = new QToolButton(head);
    close->setObjectName(QStringLiteral("nbBtn"));
    close->setIcon(nb_icon(NbGlyph::Close));
    close->setIconSize(QSize(12, 12));
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Hide the inspector (Ctrl+Shift+I)"));
    close->setAccessibleName(close->toolTip());
    connect(close, &QToolButton::clicked, this, &NotebookInspector::close_requested);
    h->addWidget(close);
    v->addWidget(head);

    stack_ = new QStackedWidget(this);
    v->addWidget(stack_, 1);

    help_ = new QTextBrowser(stack_);
    help_->setObjectName(QStringLiteral("nbTraceback"));
    help_->setOpenLinks(false);
    help_->setFrameShape(QFrame::NoFrame);
    help_->setAccessibleName(tr("Help for the name under the cursor"));
    connect(help_, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        if (url.toString() == QLatin1String("#keys"))
            show_shortcuts();
    });
    stack_->addWidget(help_);

    output_page_ = new QWidget(stack_);
    output_layout_ = new QVBoxLayout(output_page_);
    output_layout_->setContentsMargins(10, 8, 10, 8);
    output_layout_->setSpacing(6);
    output_caption_ = new QLabel(output_page_);
    output_caption_->setObjectName(QStringLiteral("nbDim"));
    output_caption_->setWordWrap(true);
    output_caption_->setVisible(false);
    output_layout_->addWidget(output_caption_);
    output_empty_ = new ui::EmptyState(output_page_);
    output_empty_->set_compact(true);
    output_layout_->addWidget(output_empty_, 1);
    stack_->addWidget(output_page_);

    history_stack_ = new QStackedWidget(stack_);
    history_empty_ = new ui::EmptyState(history_stack_);
    history_empty_->set_compact(true);
    history_stack_->addWidget(history_empty_);
    history_ = new QTreeWidget(history_stack_);
    history_->setObjectName(QStringLiteral("nbTree"));
    history_->setColumnCount(4);
    history_->setRootIsDecorated(false);
    history_->setUniformRowHeights(true);
    history_->header()->setStretchLastSection(false);
    history_->header()->setSectionResizeMode(QHeaderView::Interactive);
    history_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    const int avg = QFontMetrics(ui::fonts::font(-2)).averageCharWidth();
    history_->header()->resizeSection(0, avg * 9 + 12);
    history_->header()->resizeSection(2, avg * 8 + 12);
    history_->header()->resizeSection(3, avg * 10 + 12);
    connect(history_, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* it, int) { emit run_activated(it->data(0, Qt::UserRole).toString()); });
    history_stack_->addWidget(history_);
    stack_->addWidget(history_stack_);

    retranslate();
    show_shortcuts();
    rebuild_history();
    show_tab(Tab::Help);
}

void NotebookInspector::show_tab(Tab tab) {
    tab_ = tab;
    stack_->setCurrentIndex(static_cast<int>(tab));
    for (int i = 0; i < 3; ++i)
        nb_set_prop(tabs_[i], "active", i == static_cast<int>(tab) ? QStringLiteral("true") : QStringLiteral("false"));
}

void NotebookInspector::show_shortcuts() {
    help_mode_ = HelpMode::Shortcuts;
    render_help();
}

void NotebookInspector::set_help_pending(const QString& symbol) {
    help_mode_ = HelpMode::Pending;
    help_symbol_ = symbol;
    render_help();
}

void NotebookInspector::set_help(const QJsonObject& reply) {
    help_mode_ = HelpMode::Reply;
    help_reply_ = reply;
    help_symbol_ = reply.value(QStringLiteral("name")).toString();
    render_help();
}

void NotebookInspector::set_help_unavailable(const QString& symbol, const QString& why) {
    help_mode_ = HelpMode::Unavailable;
    help_symbol_ = symbol;
    help_why_ = why;
    render_help();
}

void NotebookInspector::render_help() {
    const notebook::HtmlPalette pal = nb_html_palette();
    QString html = QStringLiteral("<style>body{color:%1;} .dim{color:%2;} .acc{color:%3;} pre{white-space:pre-wrap;}"
                                  "td{padding:2px 10px 2px 0;} h3{margin:0 0 4px 0;}</style>")
                       .arg(pal.text, pal.dim, pal.accent);
    const QString keys_link = QStringLiteral("<p><a href=\"#keys\" style=\"color:%1;\">%2</a></p>")
                                  .arg(pal.link, esc(tr("Keyboard shortcuts")));
    switch (help_mode_) {
        case HelpMode::Shortcuts: {
            html += QStringLiteral("<h3>%1</h3><p class=\"dim\">%2</p><table>")
                        .arg(esc(tr("KEYBOARD")), esc(tr("Put the cursor on a name and press Ctrl+I for its help — it "
                                                         "comes from the live kernel, so run the cell that imports it first.")));
            const QList<QPair<QString, QString>> keys = {
                {tr("Ctrl+Enter"), tr("Run the cell")},
                {tr("Shift+Enter"), tr("Run and select the next cell")},
                {tr("Alt+Enter"), tr("Run and insert a cell below")},
                {tr("Ctrl+Shift+Enter"), tr("Run all")},
                {tr("Esc / Enter"), tr("Command mode / edit the selected cell")},
                {tr("Up / Down, K / J"), tr("Select the cell above / below (command mode)")},
                {tr("A / B"), tr("Insert a cell above / below")},
                {tr("D, D"), tr("Delete the cell (Z or UNDO brings it back)")},
                {tr("X / C / V"), tr("Cut / copy / paste cells")},
                {tr("M / Y"), tr("Make the cell text (markdown) / code")},
                {tr("O"), tr("Hide or show the cell's output")},
                {tr("L"), tr("Line numbers on / off")},
                {tr("Alt+Up / Alt+Down"), tr("Move the cell")},
                {tr("I, I"), tr("Interrupt the kernel")},
                {tr("0, 0"), tr("Restart the kernel")},
                {tr("F2"), tr("Rename the cell")},
                {tr("Tab / Shift+Tab"), tr("Complete a name, indent / dedent")},
                {tr("Ctrl+Space"), tr("Complete the name before the cursor")},
                {tr("Ctrl+/"), tr("Comment / uncomment lines")},
                {tr("Ctrl+I"), tr("Help for the name under the cursor")},
                {tr("Ctrl+F, Enter, Shift+Enter"), tr("Find in the notebook, next, previous")},
                {tr("Ctrl+S / Ctrl+Shift+S"), tr("Save / save as")},
                {tr("Ctrl+B / Ctrl+Shift+I"), tr("Rail / inspector")},
            };
            for (const auto& [k, d] : keys)
                html += QStringLiteral("<tr><td class=\"acc\"><b>%1</b></td><td>%2</td></tr>").arg(esc(k), esc(d));
            html += QStringLiteral("</table>");
            break;
        }
        case HelpMode::Pending:
            html += QStringLiteral("<h3>%1</h3><p class=\"dim\">%2</p>")
                        .arg(esc(help_symbol_), esc(tr("Asking the kernel…")));
            html += keys_link;
            break;
        case HelpMode::Unavailable:
            html += QStringLiteral("<h3>%1</h3><p class=\"dim\">%2</p>")
                        .arg(esc(help_symbol_.isEmpty() ? tr("Help") : help_symbol_), esc(help_why_));
            html += keys_link;
            break;
        case HelpMode::Reply: {
            const QJsonObject& r = help_reply_;
            if (!r.value(QStringLiteral("found")).toBool()) {
                html += QStringLiteral("<h3>%1</h3><p class=\"dim\">%2</p>")
                            .arg(esc(help_symbol_),
                                 esc(tr("Not defined in the kernel. Run the cell that defines or imports it, then ask again.")));
                html += keys_link;
                break;
            }
            QStringList meta;
            if (!r.value(QStringLiteral("kind")).toString().isEmpty())
                meta << r.value(QStringLiteral("kind")).toString();
            if (!r.value(QStringLiteral("module")).toString().isEmpty())
                meta << r.value(QStringLiteral("module")).toString();
            if (!r.value(QStringLiteral("shape")).toString().isEmpty())
                meta << tr("shape %1").arg(r.value(QStringLiteral("shape")).toString());
            html += QStringLiteral("<h3>%1</h3><p class=\"dim\">%2</p>")
                        .arg(esc(help_symbol_), esc(meta.join(QStringLiteral("  ·  "))));
            const QString sig = r.value(QStringLiteral("signature")).toString();
            if (!sig.isEmpty())
                html += QStringLiteral("<pre class=\"acc\">%1</pre>").arg(esc(sig));
            const QString value = r.value(QStringLiteral("value")).toString();
            if (!value.isEmpty())
                html += QStringLiteral("<pre>%1</pre>").arg(esc(value));
            const QString doc = r.value(QStringLiteral("doc")).toString();
            html += doc.isEmpty() ? QStringLiteral("<p class=\"dim\">%1</p>").arg(esc(tr("No documentation.")))
                                  : QStringLiteral("<pre>%1</pre>").arg(esc(doc));
            html += keys_link;
            break;
        }
    }
    help_->setHtml(html);
}

void NotebookInspector::set_output_view(NbOutputView* view, const QString& caption) {
    if (output_view_) {
        output_layout_->removeWidget(output_view_);
        output_view_->deleteLater();
        output_view_ = nullptr;
    }
    output_view_ = view;
    output_caption_->setText(caption);
    output_caption_->setVisible(!caption.isEmpty());
    output_empty_->setVisible(!view);
    if (view)
        output_layout_->addWidget(view, 1);
    show_tab(Tab::Output);
}

void NotebookInspector::show_output(const notebook::CellOutput& out, const QString& caption) {
    set_output_view(make_output_view(out, true, output_page_), caption);
}

void NotebookInspector::show_preview(const QJsonObject& reply) {
    const QString name = reply.value(QStringLiteral("name")).toString();
    if (!reply.value(QStringLiteral("found")).toBool()) {
        set_output_view(nullptr, QString());
        output_empty_->set_content(QStringLiteral("?"), tr("%1 is not defined").arg(name),
                                   tr("The kernel no longer has this variable."));
        return;
    }
    notebook::CellOutput out;
    if (reply.contains(QStringLiteral("table"))) {
        out.kind = notebook::CellOutput::Kind::Table;
        out.table = reply.value(QStringLiteral("table")).toObject();
    } else {
        out.kind = notebook::CellOutput::Kind::Result;
        out.text = reply.value(QStringLiteral("text")).toString();
    }
    set_output_view(make_output_view(out, true, output_page_), tr("Variable  %1").arg(name));
}

void NotebookInspector::add_run(const NotebookRunRecord& rec) {
    runs_.prepend(rec);
    if (runs_.size() > kMaxRuns)
        runs_.removeLast();
    rebuild_history();
}

void NotebookInspector::clear_runs() {
    runs_.clear();
    rebuild_history();
}

void NotebookInspector::rebuild_history() {
    if (runs_.isEmpty()) {
        history_stack_->setCurrentWidget(history_empty_);
        return;
    }
    history_->setUpdatesEnabled(false);
    history_->clear();
    for (const NotebookRunRecord& r : runs_) {
        QString status;
        QColor color(colors::TEXT_SECONDARY());
        if (r.status == QLatin1String("ok")) {
            status = tr("✓ done");
        } else if (r.status == QLatin1String("error")) {
            status = tr("✕ error");
            color = QColor(colors::DANGER());
        } else if (r.status == QLatin1String("timeout")) {
            status = tr("■ time limit");
            color = QColor(colors::WARNING());
        } else {
            status = tr("■ stopped");
            color = QColor(colors::WARNING());
        }
        auto* it = new QTreeWidgetItem(history_, {QLocale().toString(r.when.time(), QStringLiteral("HH:mm:ss")),
                                                  QStringLiteral("[%1] %2").arg(r.exec_count).arg(r.label), fmt_ms(r.ms),
                                                  status});
        it->setData(0, Qt::UserRole, r.cell_id);
        it->setForeground(3, color);
        it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        it->setToolTip(1, r.label);
    }
    history_->setUpdatesEnabled(true);
    history_stack_->setCurrentWidget(history_);
}

void NotebookInspector::refresh_theme() {
    render_help();
    rebuild_history();
    if (output_view_)
        output_view_->refresh_theme();
}

void NotebookInspector::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        render_help();
        rebuild_history();
    }
    QWidget::changeEvent(e);
}

void NotebookInspector::retranslate() {
    tabs_[0]->setText(tr("HELP"));
    tabs_[1]->setText(tr("OUTPUT"));
    tabs_[2]->setText(tr("HISTORY"));
    tabs_[0]->setToolTip(tr("Help for the name under the cursor (Ctrl+I) and the keyboard reference"));
    tabs_[1]->setToolTip(tr("A table, figure or value shown large"));
    tabs_[2]->setToolTip(tr("Every run of this session"));
    history_->setHeaderLabels({tr("TIME"), tr("CELL"), tr("TOOK"), tr("RESULT")});
    output_empty_->set_content(QStringLiteral("⤢"), tr("Nothing shown large yet"),
                               tr("Click EXPAND on a table or figure, or double-click a variable in VARIABLES."));
    history_empty_->set_content(QStringLiteral("⏱"), tr("No runs yet"),
                                tr("Every cell you run in this session is listed here with its time and outcome."));
}

} // namespace fincept::screens
