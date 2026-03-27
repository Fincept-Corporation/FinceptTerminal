// src/screens/code_editor/CodeEditorScreen.cpp
#include "screens/code_editor/CodeEditorScreen.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/file_manager/FileManagerService.h"
#include "ui/theme/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// Colors matching the Tauri version's FINCEPT theme
static const QString kOrange = "#FF8800";
static const QString kGreen = "#00D66F";
static const QString kRed = "#FF3B3B";
static const QString kYellow = "#FFD700";
static const QString kBlue = "#0088FF";
static const QString kPurple = "#9D4EDD";
static const QString kCyan = "#00E5FF";

// ═════════════════════════════════════════════════════════════════════════════
// CellWidget
// ═════════════════════════════════════════════════════════════════════════════

CellWidget::CellWidget(const NotebookCell& cell, QWidget* parent)
    : QWidget(parent), cell_id_(cell.id), cell_type_(cell.cell_type), execution_count_(cell.execution_count) {
    build_ui();
    set_cell_data(cell);
}

void CellWidget::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 4, 0, 4);
    root->setSpacing(0);

    // Header bar
    auto* header = new QWidget(this);
    header->setFixedHeight(26);
    auto* hhl = new QHBoxLayout(header);
    hhl->setContentsMargins(12, 0, 12, 0);
    hhl->setSpacing(6);

    header_label_ = new QLabel("In [ ]:", header);
    header_label_->setFixedWidth(70);
    header_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; font-weight:700;").arg(kOrange, fonts::DATA_FAMILY));
    hhl->addWidget(header_label_);

    type_badge_ = new QLabel("PYTHON", header);
    type_badge_->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px; font-weight:700;"
                                       " background:%3; padding:1px 6px;")
                                   .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY, kBlue));
    hhl->addWidget(type_badge_);

    hhl->addStretch();

    // Action buttons (compact)
    auto make_btn = [&](const QString& text, const QString& color) -> QPushButton* {
        auto* btn = new QPushButton(text, header);
        btn->setFixedHeight(20);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                   " font-family:%3; font-size:9px; padding:0 6px; }"
                                   "QPushButton:hover { background:%4; }")
                               .arg(color, colors::BORDER_DIM, fonts::DATA_FAMILY, colors::BG_HOVER));
        return btn;
    };

    auto* run_btn = make_btn("RUN", kGreen);
    connect(run_btn, &QPushButton::clicked, this, [this]() { emit run_requested(cell_id_); });
    hhl->addWidget(run_btn);

    auto* type_btn = make_btn("TYPE", kCyan);
    connect(type_btn, &QPushButton::clicked, this, [this]() { emit toggle_type_requested(cell_id_); });
    hhl->addWidget(type_btn);

    auto* up_btn = make_btn("UP", colors::TEXT_SECONDARY);
    connect(up_btn, &QPushButton::clicked, this, [this]() { emit move_up_requested(cell_id_); });
    hhl->addWidget(up_btn);

    auto* dn_btn = make_btn("DN", colors::TEXT_SECONDARY);
    connect(dn_btn, &QPushButton::clicked, this, [this]() { emit move_down_requested(cell_id_); });
    hhl->addWidget(dn_btn);

    auto* del_btn = make_btn("DEL", kRed);
    connect(del_btn, &QPushButton::clicked, this, [this]() { emit delete_requested(cell_id_); });
    hhl->addWidget(del_btn);

    header->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;").arg(colors::BG_RAISED, colors::BORDER_DIM));
    root->addWidget(header);

    // Editor
    editor_ = new QTextEdit(this);
    editor_->setMinimumHeight(60);
    editor_->setMaximumHeight(300);
    editor_->setAcceptRichText(false);
    editor_->setStyleSheet(QString("QTextEdit { background:#0F0F0F; color:%1; border:none; border-left:3px solid %2;"
                                   " font-family:%3; font-size:12px; padding:8px; }")
                               .arg(colors::TEXT_PRIMARY, kOrange, fonts::DATA_FAMILY));

    // Ctrl+Enter to run
    connect(editor_, &QTextEdit::textChanged, this, [this]() {
        // Auto-resize editor height
        QTextDocument* doc = editor_->document();
        int h = static_cast<int>(doc->size().height()) + 16;
        editor_->setFixedHeight(qBound(60, h, 300));
    });

    root->addWidget(editor_);

    // Output area
    output_area_ = new QWidget(this);
    output_layout_ = new QVBoxLayout(output_area_);
    output_layout_->setContentsMargins(12, 4, 12, 4);
    output_layout_->setSpacing(4);
    output_area_->setStyleSheet(QString("background:%1;").arg(colors::BG_BASE));
    output_area_->setVisible(false);
    root->addWidget(output_area_);

    // Click to select
    setMouseTracking(true);
}

void CellWidget::set_cell_data(const NotebookCell& cell) {
    cell_id_ = cell.id;
    cell_type_ = cell.cell_type;
    execution_count_ = cell.execution_count;
    running_ = cell.running;

    editor_->blockSignals(true);
    editor_->setPlainText(cell.source);
    editor_->blockSignals(false);

    update_header();

    if (!cell.outputs.isEmpty()) {
        set_outputs(cell.outputs, cell.execution_count);
    }
}

NotebookCell CellWidget::cell_data() const {
    NotebookCell cell;
    cell.id = cell_id_;
    cell.cell_type = cell_type_;
    cell.source = editor_->toPlainText();
    cell.execution_count = execution_count_;
    return cell;
}

void CellWidget::set_running(bool running) {
    running_ = running;
    update_header();
}

void CellWidget::set_outputs(const QVector<CellOutput>& outputs, int exec_count) {
    execution_count_ = exec_count;
    update_header();
    render_outputs();

    // Clear old output widgets
    while (output_layout_->count() > 0) {
        auto* item = output_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    if (outputs.isEmpty()) {
        output_area_->setVisible(false);
        return;
    }

    output_area_->setVisible(true);

    // Out label
    auto* out_label = new QLabel(QString("Out [%1]:").arg(exec_count), output_area_);
    out_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; font-weight:700;").arg(kGreen, fonts::DATA_FAMILY));
    output_layout_->addWidget(out_label);

    for (const auto& out : outputs) {
        if (out.type == "stream" || out.type == "execute_result") {
            auto* text_lbl = new QLabel(out.text, output_area_);
            text_lbl->setWordWrap(true);
            text_lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            text_lbl->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; padding:4px 0;")
                                        .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
            output_layout_->addWidget(text_lbl);
        } else if (out.type == "error") {
            auto* err_widget = new QWidget(output_area_);
            err_widget->setStyleSheet(QString("background:#1a0000; border-left:2px solid %1; padding:8px;").arg(kRed));
            auto* err_layout = new QVBoxLayout(err_widget);
            err_layout->setContentsMargins(8, 4, 8, 4);
            err_layout->setSpacing(2);

            auto* err_name = new QLabel(QString("<b>%1</b>: %2").arg(out.error_name, out.error_value), err_widget);
            err_name->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px;").arg(kRed, fonts::DATA_FAMILY));
            err_layout->addWidget(err_name);

            if (!out.traceback.isEmpty()) {
                auto* tb = new QLabel(out.traceback.join("\n"), err_widget);
                tb->setWordWrap(true);
                tb->setTextInteractionFlags(Qt::TextSelectableByMouse);
                tb->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px;")
                                      .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
                err_layout->addWidget(tb);
            }
            output_layout_->addWidget(err_widget);
        }
    }
}

void CellWidget::set_selected(bool selected) {
    QString border_color = selected ? kOrange : colors::BORDER_DIM;
    editor_->setStyleSheet(QString("QTextEdit { background:#0F0F0F; color:%1; border:none; border-left:3px solid %2;"
                                   " font-family:%3; font-size:12px; padding:8px; }")
                               .arg(colors::TEXT_PRIMARY, border_color, fonts::DATA_FAMILY));
}

void CellWidget::update_header() {
    if (running_) {
        header_label_->setText("In [*]:");
        header_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; font-weight:700;").arg(kYellow, fonts::DATA_FAMILY));
    } else if (execution_count_ > 0) {
        header_label_->setText(QString("In [%1]:").arg(execution_count_));
        header_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; font-weight:700;").arg(kGreen, fonts::DATA_FAMILY));
    } else {
        header_label_->setText("In [ ]:");
        header_label_->setStyleSheet(
            QString("color:%1; font-family:%2; font-size:10px; font-weight:700;").arg(kOrange, fonts::DATA_FAMILY));
    }

    if (cell_type_ == "markdown") {
        type_badge_->setText("MARKDOWN");
        type_badge_->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px; font-weight:700;"
                                           " background:%3; padding:1px 6px;")
                                       .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY, kPurple));
    } else {
        type_badge_->setText("PYTHON");
        type_badge_->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px; font-weight:700;"
                                           " background:%3; padding:1px 6px;")
                                       .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY, kBlue));
    }
}

void CellWidget::render_outputs() {
    // Handled in set_outputs
}

// ═════════════════════════════════════════════════════════════════════════════
// CodeEditorScreen
// ═════════════════════════════════════════════════════════════════════════════

CodeEditorScreen::CodeEditorScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    on_new_notebook(); // Start with a fresh notebook
}

void CodeEditorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_INFO("CodeEditor", "Screen shown");
}

void CodeEditorScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    LOG_INFO("CodeEditor", "Screen hidden");
}

void CodeEditorScreen::build_ui() {
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg("#000000", colors::TEXT_PRIMARY));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Scrollable cell area
    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setStyleSheet("QScrollArea { border:none; background:#000000; }"
                                "QScrollBar:vertical { background:#0F0F0F; width:8px; }"
                                "QScrollBar::handle:vertical { background:#2A2A2A; min-height:30px; }"
                                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }");

    cells_container_ = new QWidget(scroll_area_);
    cells_layout_ = new QVBoxLayout(cells_container_);
    cells_layout_->setContentsMargins(40, 8, 40, 8);
    cells_layout_->setSpacing(0);
    cells_layout_->addStretch();

    scroll_area_->setWidget(cells_container_);
    root->addWidget(scroll_area_, 1);

    // Status bar
    auto* status_bar = new QWidget(this);
    status_bar->setFixedHeight(24);
    status_bar->setStyleSheet(QString("background:#0F0F0F; border-top:1px solid %1;").arg(colors::BORDER_DIM));
    auto* shl = new QHBoxLayout(status_bar);
    shl->setContentsMargins(12, 0, 12, 0);
    status_label_ = new QLabel("Ready", status_bar);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;").arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    shl->addWidget(status_label_);
    shl->addStretch();

    auto* shortcut_label = new QLabel("Ctrl+Enter: Run Cell  |  Shift+Enter: Run & Next", status_bar);
    shortcut_label->setStyleSheet(status_label_->styleSheet());
    shl->addWidget(shortcut_label);

    root->addWidget(status_bar);
}

QWidget* CodeEditorScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(40);
    bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg("#1A1A1A", colors::BORDER_DIM));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(4);

    auto* title = new QLabel("PYTHON COLAB", bar);
    title->setStyleSheet(QString("color:%1; font-family:%2; font-size:11px; font-weight:700; margin-right:12px;")
                             .arg(kOrange, fonts::DATA_FAMILY));
    hl->addWidget(title);

    auto make_btn = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, bar);
        btn->setStyleSheet(QString("QPushButton { background:#2A2A2A; color:%1; border:none;"
                                   " font-family:%2; font-size:10px; font-weight:600; padding:6px 12px; }"
                                   "QPushButton:hover { background:#3A3A3A; }")
                               .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
        return btn;
    };

    auto* new_btn = make_btn("NEW");
    connect(new_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_new_notebook);
    hl->addWidget(new_btn);

    auto* open_btn = make_btn("OPEN");
    connect(open_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_open_notebook);
    hl->addWidget(open_btn);

    auto* save_btn = make_btn("SAVE");
    connect(save_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_save_notebook);
    hl->addWidget(save_btn);

    auto* sep = new QWidget(bar);
    sep->setFixedSize(1, 20);
    sep->setStyleSheet("background:#2A2A2A;");
    hl->addWidget(sep);

    auto* add_btn = make_btn("+ CELL");
    connect(add_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_add_cell);
    hl->addWidget(add_btn);

    auto* run_all_btn = new QPushButton("RUN ALL", bar);
    run_all_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       " font-family:%3; font-size:10px; font-weight:700; padding:6px 14px; }"
                                       "QPushButton:hover { background:#cc6600; }")
                                   .arg(kOrange, "#000000", fonts::DATA_FAMILY));
    connect(run_all_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_run_all);
    hl->addWidget(run_all_btn);

    hl->addStretch();

    // Python version label
    auto* py_label = new QLabel("Python 3.12", bar);
    py_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:9px;").arg(kBlue, fonts::DATA_FAMILY));
    hl->addWidget(py_label);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Notebook management
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_new_notebook() {
    cells_.clear();
    execution_counter_ = 0;
    notebook_path_.clear();

    // Add initial code cell
    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = "code";
    cell.source = "# Welcome to Fincept Python Colab\n# Write Python code and press Ctrl+Enter to run\n\nprint(\"Hello "
                  "from Fincept Terminal!\")";
    cells_.append(cell);

    rebuild_cells();
    update_status();
}

void CodeEditorScreen::on_add_cell() {
    QString after_id = selected_cell_id_;
    if (after_id.isEmpty() && !cells_.isEmpty()) {
        after_id = cells_.last().id;
    }
    add_cell_after(after_id, "code");
}

void CodeEditorScreen::add_cell_after(const QString& after_id, const QString& type) {
    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = type;

    int idx = find_cell_index(after_id);
    if (idx >= 0) {
        cells_.insert(idx + 1, cell);
    } else {
        cells_.append(cell);
    }

    rebuild_cells();
    update_status();
}

void CodeEditorScreen::on_delete_cell(const QString& cell_id) {
    if (cells_.size() <= 1)
        return;

    // Save current cell data first
    auto* cw = find_cell_widget(cell_id);
    int idx = find_cell_index(cell_id);
    if (idx >= 0) {
        cells_.removeAt(idx);
        rebuild_cells();
        update_status();
    }
}

void CodeEditorScreen::on_move_cell_up(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx <= 0)
        return;

    // Save current text from widget
    auto* cw = find_cell_widget(cell_id);
    if (cw)
        cells_[idx] = cw->cell_data();

    cells_.swapItemsAt(idx, idx - 1);
    rebuild_cells();
}

void CodeEditorScreen::on_move_cell_down(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0 || idx >= cells_.size() - 1)
        return;

    auto* cw = find_cell_widget(cell_id);
    if (cw)
        cells_[idx] = cw->cell_data();

    cells_.swapItemsAt(idx, idx + 1);
    rebuild_cells();
}

void CodeEditorScreen::on_toggle_cell_type(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

    auto* cw = find_cell_widget(cell_id);
    if (cw)
        cells_[idx] = cw->cell_data();

    cells_[idx].cell_type = (cells_[idx].cell_type == "code") ? "markdown" : "code";
    cells_[idx].outputs.clear();
    cells_[idx].execution_count = 0;

    rebuild_cells();
}

// ─────────────────────────────────────────────────────────────────────────────
// Cell execution
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_run_cell(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

    // Save current text from widget
    auto* cw = find_cell_widget(cell_id);
    if (cw) {
        cells_[idx] = cw->cell_data();
        cw->set_running(true);
    }

    if (cells_[idx].cell_type != "code")
        return;

    QString code = cells_[idx].source;
    if (code.trimmed().isEmpty())
        return;

    ++execution_counter_;
    int exec_num = execution_counter_;

    QPointer<CodeEditorScreen> self = this;
    QString cid = cell_id;

    python::PythonRunner::instance().run_code(code, [self, cid, exec_num](python::PythonResult result) {
        if (!self)
            return;

        int idx = self->find_cell_index(cid);
        if (idx < 0)
            return;

        QVector<CellOutput> outputs;

        if (result.success) {
            if (!result.output.isEmpty()) {
                CellOutput out;
                out.type = "stream";
                out.name = "stdout";
                out.text = result.output;
                outputs.append(out);
            }
        } else {
            CellOutput out;
            out.type = "error";
            out.error_name = "ExecutionError";
            out.error_value = result.error.isEmpty() ? QString("Process exited with code %1").arg(result.exit_code)
                                                     : result.error.split('\n').last();
            out.traceback = result.error.split('\n');
            out.text = result.error;
            outputs.append(out);

            // Also include stdout if any
            if (!result.output.isEmpty()) {
                CellOutput stdout_out;
                stdout_out.type = "stream";
                stdout_out.name = "stdout";
                stdout_out.text = result.output;
                outputs.prepend(stdout_out);
            }
        }

        self->cells_[idx].outputs = outputs;
        self->cells_[idx].execution_count = exec_num;
        self->cells_[idx].running = false;

        auto* widget = self->find_cell_widget(cid);
        if (widget) {
            widget->set_outputs(outputs, exec_num);
            widget->set_running(false);
        }

        self->update_status();
    });
}

void CodeEditorScreen::on_run_all() {
    // Run all code cells sequentially (queue them all)
    for (const auto& cell : cells_) {
        if (cell.cell_type == "code") {
            on_run_cell(cell.id);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// File I/O (.ipynb)
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_open_notebook() {
    QString path =
        QFileDialog::getOpenFileName(this, "Open Notebook", {}, "Jupyter Notebooks (*.ipynb);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
        return;

    QJsonObject root = doc.object();
    QJsonArray json_cells = root["cells"].toArray();

    cells_.clear();
    execution_counter_ = 0;

    for (const auto& jc : json_cells) {
        QJsonObject co = jc.toObject();
        NotebookCell cell;
        cell.id = new_cell_id();
        cell.cell_type = co["cell_type"].toString("code");

        // Source can be string or array of strings
        QJsonValue src = co["source"];
        if (src.isArray()) {
            QStringList lines;
            for (const auto& line : src.toArray()) {
                lines << line.toString();
            }
            cell.source = lines.join("");
        } else {
            cell.source = src.toString();
        }

        cell.execution_count = co["execution_count"].toInt(0);
        cells_.append(cell);
    }

    if (cells_.isEmpty()) {
        NotebookCell cell;
        cell.id = new_cell_id();
        cell.cell_type = "code";
        cells_.append(cell);
    }

    notebook_path_ = path;
    rebuild_cells();
    update_status();
    LOG_INFO("CodeEditor", "Opened notebook: " + path);
}

void CodeEditorScreen::on_save_notebook() {
    // Sync cell data from widgets
    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = find_cell_widget(cells_[i].id);
        if (cw)
            cells_[i] = cw->cell_data();
    }

    QString path = notebook_path_;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save Notebook", "notebook.ipynb", "Jupyter Notebooks (*.ipynb)");
        if (path.isEmpty())
            return;
    }

    // Build ipynb JSON
    QJsonArray json_cells;
    for (const auto& cell : cells_) {
        QJsonObject co;
        co["cell_type"] = cell.cell_type;

        // Split source into lines (ipynb format)
        QJsonArray src_lines;
        for (const auto& line : cell.source.split('\n')) {
            src_lines.append(line + "\n");
        }
        co["source"] = src_lines;
        co["metadata"] = QJsonObject();

        if (cell.cell_type == "code") {
            co["execution_count"] = cell.execution_count > 0 ? QJsonValue(cell.execution_count) : QJsonValue();
            co["outputs"] = QJsonArray(); // Don't save outputs
        }

        json_cells.append(co);
    }

    QJsonObject root;
    root["cells"] = json_cells;
    root["nbformat"] = 4;
    root["nbformat_minor"] = 5;

    QJsonObject metadata;
    QJsonObject kernelspec;
    kernelspec["display_name"] = "Python 3";
    kernelspec["language"] = "python";
    kernelspec["name"] = "python3";
    metadata["kernelspec"] = kernelspec;
    root["metadata"] = metadata;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        notebook_path_ = path;
        LOG_INFO("CodeEditor", "Saved notebook: " + path);

        // Register with File Manager so it appears in the Files tab
        services::FileManagerService::instance().import_file(path, "code_editor");
    }

    update_status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::rebuild_cells() {
    // Remove old widgets
    while (cells_layout_->count() > 1) { // keep the stretch
        auto* item = cells_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    // Add cell widgets
    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = new CellWidget(cells_[i], cells_container_);
        connect(cw, &CellWidget::run_requested, this, &CodeEditorScreen::on_run_cell);
        connect(cw, &CellWidget::delete_requested, this, &CodeEditorScreen::on_delete_cell);
        connect(cw, &CellWidget::move_up_requested, this, &CodeEditorScreen::on_move_cell_up);
        connect(cw, &CellWidget::move_down_requested, this, &CodeEditorScreen::on_move_cell_down);
        connect(cw, &CellWidget::toggle_type_requested, this, &CodeEditorScreen::on_toggle_cell_type);
        cells_layout_->insertWidget(i, cw);
    }
}

int CodeEditorScreen::find_cell_index(const QString& cell_id) const {
    for (int i = 0; i < cells_.size(); ++i) {
        if (cells_[i].id == cell_id)
            return i;
    }
    return -1;
}

CellWidget* CodeEditorScreen::find_cell_widget(const QString& cell_id) const {
    for (int i = 0; i < cells_layout_->count(); ++i) {
        auto* item = cells_layout_->itemAt(i);
        if (!item || !item->widget())
            continue;
        auto* cw = qobject_cast<CellWidget*>(item->widget());
        if (cw && cw->cell_id() == cell_id)
            return cw;
    }
    return nullptr;
}

QString CodeEditorScreen::new_cell_id() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void CodeEditorScreen::update_status() {
    int code_count = 0;
    int md_count = 0;
    int executed = 0;
    for (const auto& c : cells_) {
        if (c.cell_type == "code")
            ++code_count;
        else
            ++md_count;
        if (c.execution_count > 0)
            ++executed;
    }

    QString info = QString("%1 CODE  |  %2 MD  |  EXECUTED: %3").arg(code_count).arg(md_count).arg(executed);
    if (!notebook_path_.isEmpty()) {
        info += "  |  " + QFileInfo(notebook_path_).fileName();
    }
    status_label_->setText(info);
}

} // namespace fincept::screens
