// src/screens/code_editor/CodeEditorScreen_Cells.cpp
//
// CodeTextEdit (lightweight QTextEdit subclass) + CellWidget (per-cell view).
// Both classes are declared in CodeEditorScreen.h and live here as a
// partial-class split — the file in their own folder grouping by widget
// responsibility.

#include "screens/code_editor/CodeEditorScreen.h"

#include "core/keys/KeyConfigManager.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "python/PythonRunner.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ═════════════════════════════════════════════════════════════════════════════

CodeTextEdit::CodeTextEdit(QWidget* parent) : QTextEdit(parent) {}

void CodeTextEdit::keyPressEvent(QKeyEvent* event) {
    auto& km = KeyConfigManager::instance();
    const QKeySequence pressed(event->keyCombination());

    if (pressed == km.key(KeyAction::RunCell)) {
        emit run_shortcut();
        return;
    }
    if (pressed == km.key(KeyAction::RunAndNext)) {
        emit run_and_next();
        return;
    }
    // Tab inserts 4 spaces
    if (event->key() == Qt::Key_Tab && !(event->modifiers() & Qt::ShiftModifier)) {
        insertPlainText("    ");
        return;
    }
    QTextEdit::keyPressEvent(event);
}

// ═════════════════════════════════════════════════════════════════════════════
// CellWidget
// ═════════════════════════════════════════════════════════════════════════════

CellWidget::CellWidget(const NotebookCell& cell, int index, QWidget* parent)
    : QWidget(parent),
      cell_id_(cell.id),
      cell_type_(cell.cell_type),
      execution_count_(cell.execution_count),
      index_(index) {
    build_ui();
    set_cell_data(cell);
}

void CellWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 2, 0, 2);
    root->setSpacing(0);

    // ── Main cell row: [gutter] [editor] ──
    auto* cell_row = new QWidget(this);
    auto* row_layout = new QHBoxLayout(cell_row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(0);

    // ── Gutter ──
    gutter_ = new QWidget(cell_row);
    gutter_->setFixedWidth(56);
    gutter_->setStyleSheet(QString("background:%1;").arg(colors::BG_RAISED()));
    auto* gutter_layout = new QVBoxLayout(gutter_);
    gutter_layout->setContentsMargins(0, 4, 6, 2);
    gutter_layout->setSpacing(2);
    gutter_layout->setAlignment(Qt::AlignTop | Qt::AlignRight);

    gutter_number_ = new QLabel("[ ]", gutter_);
    gutter_number_->setAlignment(Qt::AlignRight | Qt::AlignTop);
    gutter_number_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:%3px; font-weight:700; background:transparent;")
            .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY)
            .arg(fonts::TINY));
    gutter_layout->addWidget(gutter_number_);

    gutter_type_ = new QLabel("PY", gutter_);
    gutter_type_->setAlignment(Qt::AlignRight);
    gutter_type_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                        " letter-spacing:0.5px; background:transparent;")
                                    .arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    gutter_layout->addWidget(gutter_type_);
    gutter_layout->addStretch();

    row_layout->addWidget(gutter_);

    // ── Editor container ──
    editor_container_ = new QWidget(cell_row);
    auto* editor_vbox = new QVBoxLayout(editor_container_);
    editor_vbox->setContentsMargins(0, 0, 0, 0);
    editor_vbox->setSpacing(0);

    // Toolbar — always occupies 24px to prevent layout shift; buttons hide/show
    toolbar_ = new QWidget(editor_container_);
    toolbar_->setFixedHeight(24);
    toolbar_->setStyleSheet(QString("background:%1;").arg(colors::BG_RAISED()));
    auto* tb_layout = new QHBoxLayout(toolbar_);
    tb_layout->setContentsMargins(8, 0, 8, 0);
    tb_layout->setSpacing(2);

    auto make_tool_btn = [&](const QString& text, const QString& fg_color) -> QPushButton* {
        auto* btn = new QPushButton(text, toolbar_);
        btn->setFixedHeight(20);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setVisible(false); // hidden until hover
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   " font-family:%2; font-size:%3px; font-weight:600; padding:0 8px;"
                                   " letter-spacing:0.5px; }"
                                   "QPushButton:hover { background:%4; color:%5; }")
                               .arg(fg_color, fonts::DATA_FAMILY)
                               .arg(fonts::TINY)
                               .arg(colors::BG_HOVER(), colors::TEXT_PRIMARY()));
        return btn;
    };

    auto* run_btn = make_tool_btn("RUN", colors::POSITIVE);
    connect(run_btn, &QPushButton::clicked, this, [this]() { emit run_requested(cell_id_); });
    tb_layout->addWidget(run_btn);

    auto* type_btn = make_tool_btn("TYPE", colors::CYAN);
    connect(type_btn, &QPushButton::clicked, this, [this]() { emit toggle_type_requested(cell_id_); });
    tb_layout->addWidget(type_btn);

    auto* up_btn = make_tool_btn("UP", colors::TEXT_SECONDARY);
    connect(up_btn, &QPushButton::clicked, this, [this]() { emit move_up_requested(cell_id_); });
    tb_layout->addWidget(up_btn);

    auto* dn_btn = make_tool_btn("DN", colors::TEXT_SECONDARY);
    connect(dn_btn, &QPushButton::clicked, this, [this]() { emit move_down_requested(cell_id_); });
    tb_layout->addWidget(dn_btn);

    tb_layout->addStretch();

    auto* del_btn = make_tool_btn("DEL", colors::NEGATIVE);
    connect(del_btn, &QPushButton::clicked, this, [this]() { emit delete_requested(cell_id_); });
    tb_layout->addWidget(del_btn);

    editor_vbox->addWidget(toolbar_);

    // ── Code editor ──
    editor_ = new CodeTextEdit(editor_container_);
    editor_->setAcceptRichText(false);
    editor_->setTabStopDistance(28);
    editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor_->setLineWrapMode(QTextEdit::NoWrap);
    editor_->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:none;"
                                   " font-family:%3; font-size:%4px; padding:2px 6px;"
                                   " selection-background-color:%5; }"
                                   "QScrollBar:vertical { background:%1; width:5px; }"
                                   "QScrollBar::handle:vertical { background:%6; min-height:20px; }"
                                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
                                   "QScrollBar:horizontal { background:%1; height:5px; }"
                                   "QScrollBar::handle:horizontal { background:%6; min-width:20px; }"
                                   "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }")
                               .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), fonts::DATA_FAMILY)
                               .arg(fonts::DATA)
                               .arg(colors::AMBER_DIM(), colors::BORDER_BRIGHT()));
    editor_->document()->setDocumentMargin(2);

    // Keyboard shortcuts
    connect(editor_, &CodeTextEdit::run_shortcut, this, [this]() { emit run_requested(cell_id_); });
    connect(editor_, &CodeTextEdit::run_and_next, this, [this]() { emit run_and_advance(cell_id_); });

    // Responsive height on every content change
    connect(editor_->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
            [this](const QSizeF&) { adjust_editor_height(); });

    editor_vbox->addWidget(editor_);

    // ── Markdown preview (for MD cells, shown when not editing) ──
    md_preview_ = new QTextBrowser(editor_container_);
    md_preview_->setOpenExternalLinks(true);
    md_preview_->setReadOnly(true);
    md_preview_->setVisible(false);
    md_preview_->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:none;"
                                       " font-family:%3; font-size:%4px; padding:10px 12px; }"
                                       "QScrollBar:vertical { background:%1; width:5px; }"
                                       "QScrollBar::handle:vertical { background:%5; min-height:20px; }"
                                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                                   .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), fonts::DATA_FAMILY)
                                   .arg(fonts::DATA)
                                   .arg(colors::BORDER_BRIGHT()));

    // Click on preview to edit
    md_preview_->installEventFilter(this);

    editor_vbox->addWidget(md_preview_);

    row_layout->addWidget(editor_container_, 1);
    root->addWidget(cell_row);

    // ── Output area ──
    auto* output_row = new QWidget(this);
    auto* output_row_layout = new QHBoxLayout(output_row);
    output_row_layout->setContentsMargins(0, 0, 0, 0);
    output_row_layout->setSpacing(0);

    auto* output_gutter = new QWidget(output_row);
    output_gutter->setFixedWidth(56);
    output_row_layout->addWidget(output_gutter);

    output_area_ = new QWidget(output_row);
    auto* output_vbox = new QVBoxLayout(output_area_);
    output_vbox->setContentsMargins(0, 0, 0, 0);
    output_vbox->setSpacing(0);

    // Output header with collapse toggle
    output_toggle_ = new QPushButton("OUTPUT", output_area_);
    output_toggle_->setFixedHeight(22);
    output_toggle_->setCursor(Qt::PointingHandCursor);
    output_toggle_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none; border-left:3px solid %3;"
                " font-family:%4; font-size:10px; font-weight:700; letter-spacing:0.5px;"
                " padding:0 10px; text-align:left; }"
                "QPushButton:hover { background:%5; }")
            .arg(colors::BG_RAISED(), colors::POSITIVE(), colors::BORDER_DIM(), fonts::DATA_FAMILY, colors::BG_HOVER()));
    output_toggle_->setVisible(false);
    connect(output_toggle_, &QPushButton::clicked, this, [this]() {
        output_collapsed_ = !output_collapsed_;
        output_content_->setVisible(!output_collapsed_);
        output_toggle_->setText(output_collapsed_ ? "OUTPUT [collapsed]" : "OUTPUT");
    });
    output_vbox->addWidget(output_toggle_);

    // Scrollable output content
    output_content_ = new QWidget(output_area_);
    output_content_layout_ = new QVBoxLayout(output_content_);
    output_content_layout_->setContentsMargins(12, 6, 12, 8);
    output_content_layout_->setSpacing(4);
    output_content_->setStyleSheet(
        QString("background:%1; border-left:3px solid %2;").arg(colors::BG_BASE(), colors::BORDER_DIM()));
    output_content_->setVisible(false);
    output_vbox->addWidget(output_content_);

    output_area_->setVisible(false);
    output_row_layout->addWidget(output_area_, 1);
    root->addWidget(output_row);

    // ── Insert-below: always-visible dim "+" centered between cells ──
    insert_hint_ = new QWidget(this);
    insert_hint_->setFixedHeight(20);
    auto* insert_layout = new QHBoxLayout(insert_hint_);
    insert_layout->setContentsMargins(56, 0, 0, 0);
    insert_layout->setSpacing(0);

    auto* insert_line = new QWidget(insert_hint_);
    insert_line->setFixedHeight(1);
    insert_line->setStyleSheet(QString("background:%1;").arg(colors::BORDER_DIM()));
    insert_layout->addWidget(insert_line, 1);

    auto* insert_btn = new QPushButton("+", insert_hint_);
    insert_btn->setFixedSize(32, 18);
    insert_btn->setCursor(Qt::PointingHandCursor);
    insert_btn->setObjectName("insertBtn");
    insert_btn->setStyleSheet(QString("QPushButton { color:%1; font-family:%2; font-size:12px; font-weight:700;"
                                      " background:%3; border:1px solid %4; }"
                                      "QPushButton:hover { background:%5; color:%6; border-color:%5; }")
                                  .arg(colors::TEXT_DIM(), fonts::DATA_FAMILY, colors::BG_SURFACE(), colors::BORDER_DIM(),
                                       colors::AMBER_DIM(), colors::AMBER()));
    connect(insert_btn, &QPushButton::clicked, this, [this]() { emit insert_below_requested(cell_id_); });
    insert_layout->addWidget(insert_btn);

    auto* insert_line2 = new QWidget(insert_hint_);
    insert_line2->setFixedHeight(1);
    insert_line2->setStyleSheet(QString("background:%1;").arg(colors::BORDER_DIM()));
    insert_layout->addWidget(insert_line2, 1);

    root->addWidget(insert_hint_);
    setMouseTracking(true);
}

void CellWidget::adjust_editor_height() {
    if (!editor_->isVisible())
        return;
    // Exact content height from the layout engine + CSS padding (2+2)
    int doc_h = static_cast<int>(editor_->document()->size().height());
    int target = doc_h + 6;
    int max_h = (cell_type_ == "code") ? 600 : 300;
    int h = qBound(28, target, max_h);
    editor_->setFixedHeight(h);
}

void CellWidget::set_cell_data(const NotebookCell& cell) {
    cell_id_ = cell.id;
    cell_type_ = cell.cell_type;
    title_ = cell.title;
    execution_count_ = cell.execution_count;
    running_ = cell.running;

    editor_->blockSignals(true);
    editor_->setPlainText(cell.source);
    editor_->blockSignals(false);

    // Force document layout so height measurement is accurate
    editor_->document()->adjustSize();
    adjust_editor_height();
    update_gutter();

    // For markdown cells, show rendered preview by default
    if (cell_type_ == "markdown" && !cell.source.trimmed().isEmpty()) {
        md_editing_ = false;
        editor_->setVisible(false);
        md_preview_->setVisible(true);
        render_markdown();
    } else {
        md_editing_ = true;
        editor_->setVisible(true);
        md_preview_->setVisible(false);
    }

    if (!cell.outputs.isEmpty()) {
        set_outputs(cell.outputs, cell.execution_count);
    }
}

NotebookCell CellWidget::cell_data() const {
    NotebookCell cell;
    cell.id = cell_id_;
    cell.cell_type = cell_type_;
    cell.title = title_;
    cell.source = editor_->toPlainText();
    cell.execution_count = execution_count_;
    return cell;
}

void CellWidget::set_running(bool running) {
    running_ = running;
    update_gutter();
}

void CellWidget::set_outputs(const QVector<CellOutput>& outputs, int exec_count) {
    execution_count_ = exec_count;
    update_gutter();

    // Clear old output content
    while (output_content_layout_->count() > 0) {
        auto* item = output_content_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (outputs.isEmpty()) {
        output_area_->setVisible(false);
        output_toggle_->setVisible(false);
        output_content_->setVisible(false);
        return;
    }

    output_area_->setVisible(true);
    output_toggle_->setVisible(true);
    output_toggle_->setText(QString("OUT [%1]").arg(exec_count));
    output_collapsed_ = false;
    output_content_->setVisible(true);

    for (const auto& out : outputs) {
        if (out.type == "stream" || out.type == "execute_result") {
            // Use QTextEdit in read-only mode for proper scrolling of long output
            auto* output_view = new QTextEdit(output_content_);
            output_view->setReadOnly(true);
            output_view->setPlainText(out.text);
            output_view->setLineWrapMode(QTextEdit::WidgetWidth);
            output_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            output_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

            QString fg = (out.name == "stderr") ? colors::WARNING : colors::TEXT_PRIMARY;
            output_view->setStyleSheet(
                QString("QTextEdit { background:transparent; color:%1; border:none;"
                        " font-family:%2; font-size:%3px; padding:2px 0; }"
                        "QScrollBar:vertical { background:transparent; width:4px; }"
                        "QScrollBar::handle:vertical { background:%4; }"
                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                    .arg(fg, fonts::DATA_FAMILY)
                    .arg(fonts::DATA)
                    .arg(colors::BORDER_BRIGHT()));

            // Size to content, max 300px then scroll
            int lines = out.text.count('\n') + 1;
            int line_h = QFontMetrics(output_view->font()).lineSpacing();
            int content_h = lines * line_h + 10;
            int clamped = qBound(30, content_h, 300);
            output_view->setFixedHeight(clamped);

            output_content_layout_->addWidget(output_view);

        } else if (out.type == "error") {
            auto* err_widget = new QWidget(output_content_);
            err_widget->setStyleSheet(QString("background:#1a0808; border-left:2px solid %1;").arg(colors::NEGATIVE()));
            auto* err_layout = new QVBoxLayout(err_widget);
            err_layout->setContentsMargins(10, 6, 10, 6);
            err_layout->setSpacing(4);

            auto* err_header = new QLabel(QString("%1: %2").arg(out.error_name, out.error_value), err_widget);
            err_header->setWordWrap(true);
            err_header->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700;")
                                          .arg(colors::NEGATIVE(), fonts::DATA_FAMILY)
                                          .arg(fonts::DATA));
            err_layout->addWidget(err_header);

            if (!out.traceback.isEmpty()) {
                auto* tb_view = new QTextEdit(err_widget);
                tb_view->setReadOnly(true);
                tb_view->setPlainText(out.traceback.join("\n"));
                tb_view->setLineWrapMode(QTextEdit::WidgetWidth);
                tb_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                tb_view->setStyleSheet(
                    QString("QTextEdit { background:transparent; color:%1; border:none;"
                            " font-family:%2; font-size:%3px; }"
                            "QScrollBar:vertical { background:transparent; width:4px; }"
                            "QScrollBar::handle:vertical { background:%4; }"
                            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
                        .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY)
                        .arg(fonts::TINY)
                        .arg(colors::BORDER_BRIGHT()));

                int tb_lines = out.traceback.join("\n").count('\n') + 1;
                int tb_h = qBound(40, tb_lines * 16 + 10, 200);
                tb_view->setFixedHeight(tb_h);
                err_layout->addWidget(tb_view);
            }
            output_content_layout_->addWidget(err_widget);
        }
    }
}

void CellWidget::set_selected(bool selected) {
    selected_ = selected;

    // Selected: amber left accent, brighter surface. Unselected: dim border, standard surface.
    QString border = selected ? colors::AMBER : colors::BORDER_MED;
    QString bg = selected ? "#0e0e0e" : colors::BG_SURFACE();
    editor_container_->setStyleSheet(QString("background:%1; border-left:3px solid %2; border-top:1px solid %3;"
                                             " border-bottom:1px solid %3; border-right:1px solid %3;")
                                         .arg(bg, border, colors::BORDER_DIM()));

    // Gutter matches cell background
    QString gutter_bg = selected ? "#0e0e0e" : colors::BG_RAISED();
    gutter_->setStyleSheet(QString("background:%1;").arg(gutter_bg));

    QString num_color = selected ? colors::AMBER : colors::TEXT_TERTIARY;
    gutter_number_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:%3px; font-weight:700; background:transparent;")
            .arg(num_color, fonts::DATA_FAMILY)
            .arg(fonts::TINY));

    // For markdown: if selecting, switch to edit mode; if deselecting, render
    if (cell_type_ == "markdown") {
        if (selected && !md_editing_) {
            md_editing_ = true;
            md_preview_->setVisible(false);
            editor_->setVisible(true);
            editor_->setFocus();
            adjust_editor_height();
        } else if (!selected && md_editing_) {
            md_editing_ = false;
            editor_->setVisible(false);
            md_preview_->setVisible(true);
            render_markdown();
        }
    }
}

void CellWidget::set_index(int index) {
    index_ = index;
    update_gutter();
}

void CellWidget::update_gutter() {
    if (running_) {
        gutter_number_->setText("[*]");
        gutter_number_->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700;")
                                          .arg(colors::WARNING(), fonts::DATA_FAMILY)
                                          .arg(fonts::TINY));
    } else if (execution_count_ > 0) {
        gutter_number_->setText(QString("[%1]").arg(execution_count_));
        gutter_number_->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700;")
                                          .arg(selected_ ? colors::AMBER() : colors::POSITIVE(), fonts::DATA_FAMILY)
                                          .arg(fonts::TINY));
    } else {
        gutter_number_->setText(QString("[%1]").arg(index_ + 1));
        gutter_number_->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700;")
                                          .arg(selected_ ? colors::AMBER() : colors::TEXT_TERTIARY(), fonts::DATA_FAMILY)
                                          .arg(fonts::TINY));
    }

    if (cell_type_ == "markdown") {
        gutter_type_->setText("MD");
        gutter_type_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; font-weight:600; letter-spacing:0.5px;")
                .arg(colors::INFO(), fonts::DATA_FAMILY));
    } else {
        gutter_type_->setText("PY");
        gutter_type_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; font-weight:600; letter-spacing:0.5px;")
                .arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    }
}

void CellWidget::render_markdown() {
    QString src = editor_->toPlainText();
    if (src.trimmed().isEmpty()) {
        md_preview_->setHtml(QString("<p style='color:%1; font-style:italic;'>Empty markdown cell — click to edit</p>")
                                 .arg(colors::TEXT_TERTIARY()));
        md_preview_->setMinimumHeight(48);
        md_preview_->setMaximumHeight(48);
        return;
    }

    // Convert basic markdown to HTML for rendering
    // Handles: headers, bold, italic, code blocks, inline code, lists, links, horizontal rules
    QString html;
    html += QString("<style>"
                    "body { color:%1; font-family:%2; font-size:%3px; }"
                    "h1 { color:%4; font-size:20px; font-weight:700; margin:8px 0 4px; border-bottom:1px solid %5; "
                    "padding-bottom:4px; }"
                    "h2 { color:%4; font-size:17px; font-weight:700; margin:8px 0 4px; }"
                    "h3 { color:%4; font-size:15px; font-weight:700; margin:6px 0 3px; }"
                    "h4 { color:%6; font-size:14px; font-weight:600; margin:4px 0 2px; }"
                    "code { background:%7; color:%4; padding:2px 4px; font-family:%2; }"
                    "pre { background:%7; color:%1; padding:8px; font-family:%2; font-size:13px;"
                    "      border-left:2px solid %5; margin:6px 0; }"
                    "a { color:%8; }"
                    "strong { color:%1; font-weight:700; }"
                    "em { color:%6; font-style:italic; }"
                    "ul, ol { margin:4px 0; padding-left:20px; }"
                    "li { margin:2px 0; }"
                    "hr { border:none; border-top:1px solid %5; margin:8px 0; }"
                    "blockquote { border-left:3px solid %4; padding-left:10px; color:%6; margin:6px 0; }"
                    "</style>")
                .arg(colors::TEXT_PRIMARY(), fonts::DATA_FAMILY)
                .arg(fonts::DATA)
                .arg(colors::AMBER(), colors::BORDER_MED(), colors::TEXT_SECONDARY(), colors::BG_RAISED(), colors::CYAN());

    bool in_code_block = false;
    QString code_block;
    QStringList lines = src.split('\n');

    for (const auto& line : lines) {
        // Fenced code blocks
        if (line.trimmed().startsWith("```")) {
            if (in_code_block) {
                html += "<pre>" + code_block.toHtmlEscaped() + "</pre>";
                code_block.clear();
                in_code_block = false;
            } else {
                in_code_block = true;
            }
            continue;
        }
        if (in_code_block) {
            if (!code_block.isEmpty())
                code_block += "\n";
            code_block += line;
            continue;
        }

        QString trimmed = line.trimmed();

        // Horizontal rule
        if (trimmed == "---" || trimmed == "***" || trimmed == "___") {
            html += "<hr>";
            continue;
        }
        // Headers
        if (trimmed.startsWith("#### ")) {
            html += "<h4>" + trimmed.mid(5).toHtmlEscaped() + "</h4>";
            continue;
        }
        if (trimmed.startsWith("### ")) {
            html += "<h3>" + trimmed.mid(4).toHtmlEscaped() + "</h3>";
            continue;
        }
        if (trimmed.startsWith("## ")) {
            html += "<h2>" + trimmed.mid(3).toHtmlEscaped() + "</h2>";
            continue;
        }
        if (trimmed.startsWith("# ")) {
            html += "<h1>" + trimmed.mid(2).toHtmlEscaped() + "</h1>";
            continue;
        }
        // Blockquote
        if (trimmed.startsWith("> ")) {
            html += "<blockquote>" + trimmed.mid(2).toHtmlEscaped() + "</blockquote>";
            continue;
        }
        // Unordered list
        if (trimmed.startsWith("- ") || trimmed.startsWith("* ")) {
            html += "<ul><li>" + trimmed.mid(2).toHtmlEscaped() + "</li></ul>";
            continue;
        }
        // Ordered list (simple)
        if (trimmed.length() > 2 && trimmed[0].isDigit() && trimmed[1] == '.') {
            html += "<ol><li>" + trimmed.mid(3).toHtmlEscaped() + "</li></ol>";
            continue;
        }
        // Empty line = paragraph break
        if (trimmed.isEmpty()) {
            html += "<br>";
            continue;
        }

        // Inline formatting: **bold**, *italic*, `code`, [link](url)
        QString escaped = line.toHtmlEscaped();
        // Inline code first (to avoid nested processing)
        escaped.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        // Bold
        escaped.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<strong>\\1</strong>");
        // Italic
        escaped.replace(QRegularExpression("\\*([^*]+)\\*"), "<em>\\1</em>");
        // Links
        escaped.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href='\\2'>\\1</a>");

        html += "<p style='margin:2px 0;'>" + escaped + "</p>";
    }

    // Close unclosed code block
    if (in_code_block && !code_block.isEmpty()) {
        html += "<pre>" + code_block.toHtmlEscaped() + "</pre>";
    }

    md_preview_->setHtml(html);

    // Size preview to content
    QTimer::singleShot(0, this, [this]() {
        int h = static_cast<int>(md_preview_->document()->size().height()) + 20;
        int clamped = qBound(48, h, 600);
        md_preview_->setMinimumHeight(clamped);
        md_preview_->setMaximumHeight(clamped);
    });
}

void CellWidget::enterEvent(QEnterEvent* /*event*/) {
    hovered_ = true;
    // Show toolbar buttons
    for (int i = 0; i < toolbar_->layout()->count(); ++i) {
        auto* w = toolbar_->layout()->itemAt(i)->widget();
        if (w)
            w->setVisible(true);
    }
    toolbar_->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));
}

void CellWidget::leaveEvent(QEvent* /*event*/) {
    hovered_ = false;
    // Hide toolbar buttons
    for (int i = 0; i < toolbar_->layout()->count(); ++i) {
        auto* w = toolbar_->layout()->itemAt(i)->widget();
        if (w)
            w->setVisible(false);
    }
    toolbar_->setStyleSheet(QString("background:%1;").arg(colors::BG_RAISED()));
}

void CellWidget::mousePressEvent(QMouseEvent* event) {
    emit cell_clicked(cell_id_);
    QWidget::mousePressEvent(event);
}

// ═════════════════════════════════════════════════════════════════════════════
// CellNavigator (sidebar)
// ═════════════════════════════════════════════════════════════════════════════
} // namespace fincept::screens
