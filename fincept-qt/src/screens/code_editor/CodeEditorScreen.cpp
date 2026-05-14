// src/screens/code_editor/CodeEditorScreen.cpp
// Python Colab — Obsidian design system implementation.
// Responsive cells, markdown rendering, keyboard shortcuts, collapsible output.

// CellWidget + CodeTextEdit lives in CodeEditorScreen_Cells.cpp.
// CellNavigator lives in CodeEditorScreen_Navigator.cpp.

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
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ═════════════════════════════════════════════════════════════════════════════
// CodeTextEdit — editor with keyboard shortcuts

CodeEditorScreen::CodeEditorScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    on_new_notebook();
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
    setStyleSheet(QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE(), colors::TEXT_PRIMARY()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    splitter_ = new QSplitter(Qt::Horizontal, this);
    splitter_->setHandleWidth(1);
    splitter_->setStyleSheet(QString("QSplitter { background:%1; } QSplitter::handle { background:%2; }")
                                 .arg(colors::BG_BASE(), colors::BORDER_DIM()));

    navigator_ = new CellNavigator(splitter_);
    navigator_->setMinimumWidth(180);
    navigator_->setMaximumWidth(300);
    connect(navigator_, &CellNavigator::cell_selected, this, &CodeEditorScreen::on_select_cell);
    connect(navigator_, &CellNavigator::rename_requested, this, &CodeEditorScreen::on_rename_cell);
    splitter_->addWidget(navigator_);

    scroll_area_ = new QScrollArea(splitter_);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setStyleSheet(
        QString("QScrollArea { border:none; background:%1; }"
                "QScrollBar:vertical { background:%2; width:6px; margin:0; }"
                "QScrollBar::handle:vertical { background:%3; min-height:40px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }")
            .arg(colors::BG_BASE(), colors::BG_SURFACE(), colors::BORDER_BRIGHT()));

    cells_container_ = new QWidget(scroll_area_);
    cells_layout_ = new QVBoxLayout(cells_container_);
    cells_layout_->setContentsMargins(8, 4, 12, 40);
    cells_layout_->setSpacing(0);
    cells_layout_->addStretch();

    scroll_area_->setWidget(cells_container_);
    splitter_->addWidget(scroll_area_);

    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setSizes({200, 800});

    root->addWidget(splitter_, 1);
    root->addWidget(build_status_bar());
}

QWidget* CodeEditorScreen::build_toolbar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(36);
    bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(0);

    auto* title = new QLabel("PYTHON NOTEBOOK", bar);
    title->setStyleSheet(QString("color:%1; font-family:%2; font-size:%3px; font-weight:700;"
                                 " letter-spacing:1px; padding-right:16px;")
                             .arg(colors::AMBER(), fonts::DATA_FAMILY)
                             .arg(fonts::SMALL));
    hl->addWidget(title);

    auto add_sep = [&]() {
        auto* sep = new QWidget(bar);
        sep->setFixedSize(1, 18);
        sep->setStyleSheet(QString("background:%1;").arg(colors::BORDER_MED()));
        hl->addWidget(sep);
    };

    auto make_btn = [&](const QString& text, const QString& fg = QString()) -> QPushButton* {
        auto* btn = new QPushButton(text, bar);
        btn->setCursor(Qt::PointingHandCursor);
        QString color = fg.isEmpty() ? colors::TEXT_SECONDARY : fg;
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   " font-family:%2; font-size:%3px; font-weight:600; padding:0 10px;"
                                   " letter-spacing:0.5px; }"
                                   "QPushButton:hover { color:%4; background:%5; }")
                               .arg(color, fonts::DATA_FAMILY)
                               .arg(fonts::TINY)
                               .arg(colors::TEXT_PRIMARY(), colors::BG_HOVER()));
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

    add_sep();

    auto* add_btn = make_btn("+ CELL", colors::CYAN);
    connect(add_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_add_cell);
    hl->addWidget(add_btn);

    auto* clear_btn = make_btn("CLEAR OUT");
    connect(clear_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_clear_outputs);
    hl->addWidget(clear_btn);

    add_sep();

    auto* run_all_btn = new QPushButton("RUN ALL", bar);
    run_all_btn->setCursor(Qt::PointingHandCursor);
    run_all_btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                       " font-family:%3; font-size:%4px; font-weight:700; padding:4px 14px;"
                                       " letter-spacing:0.5px; }"
                                       "QPushButton:hover { background:%5; }")
                                   .arg(colors::AMBER(), colors::BG_BASE(), fonts::DATA_FAMILY)
                                   .arg(fonts::TINY)
                                   .arg(colors::ORANGE()));
    connect(run_all_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_run_all);
    hl->addWidget(run_all_btn);

    hl->addStretch();

    auto* sidebar_btn = make_btn("SIDEBAR");
    connect(sidebar_btn, &QPushButton::clicked, this, &CodeEditorScreen::on_toggle_sidebar);
    hl->addWidget(sidebar_btn);

    add_sep();

    kernel_label_ = new QLabel("KERNEL: IDLE", bar);
    kernel_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                         " letter-spacing:0.5px; padding:0 8px;")
                                     .arg(colors::POSITIVE(), fonts::DATA_FAMILY));
    hl->addWidget(kernel_label_);

    auto* py_label = new QLabel("Python 3.11", bar);
    py_label->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; padding-left:8px;")
                                .arg(colors::TEXT_TERTIARY(), fonts::DATA_FAMILY));
    hl->addWidget(py_label);

    return bar;
}

QWidget* CodeEditorScreen::build_status_bar() {
    auto* bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet(QString("background:%1; border-top:1px solid %2;").arg(colors::BG_RAISED(), colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);

    status_label_ = new QLabel("READY", bar);
    status_label_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:10px; font-weight:600; letter-spacing:0.5px;")
            .arg(colors::TEXT_SECONDARY(), fonts::DATA_FAMILY));
    hl->addWidget(status_label_);
    hl->addStretch();

    auto* shortcuts = new QLabel("Ctrl+Enter: RUN  |  Shift+Enter: RUN & NEXT  |  Tab: 4 SPACES  |  Ctrl+S: SAVE", bar);
    shortcuts->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; letter-spacing:0.3px;")
                                 .arg(colors::TEXT_DIM(), fonts::DATA_FAMILY));
    hl->addWidget(shortcuts);

    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Notebook management
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_new_notebook() {
    cells_.clear();
    execution_counter_ = 0;
    notebook_path_.clear();

    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = "code";
    cell.source = "# Fincept Python Notebook\n"
                  "# Write Python code and press Ctrl+Enter to run\n"
                  "\n"
                  "print(\"Hello from Fincept Terminal!\")";
    cells_.append(cell);

    selected_cell_id_ = cell.id;
    rebuild_cells();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_add_cell() {
    QString after_id = selected_cell_id_;
    if (after_id.isEmpty() && !cells_.isEmpty())
        after_id = cells_.last().id;
    add_cell_after(after_id, "code");
}

void CodeEditorScreen::on_insert_below(const QString& cell_id) {
    add_cell_after(cell_id, "code");
}

void CodeEditorScreen::add_cell_after(const QString& after_id, const QString& type) {
    NotebookCell cell;
    cell.id = new_cell_id();
    cell.cell_type = type;

    int idx = find_cell_index(after_id);
    if (idx >= 0)
        cells_.insert(idx + 1, cell);
    else
        cells_.append(cell);

    selected_cell_id_ = cell.id;
    rebuild_cells();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_delete_cell(const QString& cell_id) {
    if (cells_.size() <= 1)
        return;

    int idx = find_cell_index(cell_id);
    if (idx >= 0) {
        cells_.removeAt(idx);
        if (idx < cells_.size())
            selected_cell_id_ = cells_[idx].id;
        else if (!cells_.isEmpty())
            selected_cell_id_ = cells_.last().id;

        rebuild_cells();
        update_status();
        update_navigator();
    }
}

void CodeEditorScreen::on_move_cell_up(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx <= 0)
        return;

    auto* cw = find_cell_widget(cell_id);
    if (cw)
        cells_[idx] = cw->cell_data();

    cells_.swapItemsAt(idx, idx - 1);
    rebuild_cells();
    update_navigator();
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
    update_navigator();
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
    update_navigator();
}

void CodeEditorScreen::on_select_cell(const QString& cell_id) {
    if (cell_id == selected_cell_id_)
        return;

    auto* old_cw = find_cell_widget(selected_cell_id_);
    if (old_cw)
        old_cw->set_selected(false);

    selected_cell_id_ = cell_id;

    auto* new_cw = find_cell_widget(cell_id);
    if (new_cw) {
        new_cw->set_selected(true);
        scroll_area_->ensureWidgetVisible(new_cw, 50, 50);
    }
    update_navigator();
}

void CodeEditorScreen::on_rename_cell(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

    QString current_name = cells_[idx].title.trimmed();
    if (current_name.isEmpty()) {
        current_name = cells_[idx].source.split('\n').first().trimmed();
        if (current_name.isEmpty())
            current_name = QString("Cell %1").arg(idx + 1);
    }

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, "Rename Cell", "Cell name:", QLineEdit::Normal, current_name, &ok).trimmed();
    if (!ok)
        return;

    // Empty name means "use auto preview from source"
    cells_[idx].title = name;
    update_navigator();
}

void CodeEditorScreen::on_clear_outputs() {
    for (auto& cell : cells_) {
        cell.outputs.clear();
        cell.execution_count = 0;
    }
    execution_counter_ = 0;
    rebuild_cells();
    update_status();
    update_navigator();
}

void CodeEditorScreen::on_toggle_sidebar() {
    sidebar_visible_ = !sidebar_visible_;
    navigator_->setVisible(sidebar_visible_);
}

void CodeEditorScreen::advance_to_next(const QString& current_id) {
    int idx = find_cell_index(current_id);
    if (idx < 0)
        return;

    if (idx + 1 < cells_.size()) {
        on_select_cell(cells_[idx + 1].id);
    } else {
        // Add a new cell at the end and select it
        add_cell_after(current_id, "code");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cell execution
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::on_run_cell(const QString& cell_id) {
    int idx = find_cell_index(cell_id);
    if (idx < 0)
        return;

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

    kernel_label_->setText("KERNEL: BUSY");
    kernel_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                         " letter-spacing:0.5px; padding:0 8px;")
                                     .arg(colors::WARNING(), fonts::DATA_FAMILY));

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

        self->kernel_label_->setText("KERNEL: IDLE");
        self->kernel_label_->setStyleSheet(QString("color:%1; font-family:%2; font-size:10px; font-weight:600;"
                                                   " letter-spacing:0.5px; padding:0 8px;")
                                               .arg(colors::POSITIVE(), fonts::DATA_FAMILY));

        self->update_status();
        self->update_navigator();
    });
}

void CodeEditorScreen::on_run_and_advance(const QString& cell_id) {
    on_run_cell(cell_id);
    advance_to_next(cell_id);
}

void CodeEditorScreen::on_run_all() {
    for (const auto& cell : cells_) {
        if (cell.cell_type == "code")
            on_run_cell(cell.id);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// File I/O
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

    QJsonArray json_cells = doc.object()["cells"].toArray();

    cells_.clear();
    execution_counter_ = 0;

    for (const auto& jc : json_cells) {
        QJsonObject co = jc.toObject();
        NotebookCell cell;
        cell.id = new_cell_id();
        cell.cell_type = co["cell_type"].toString("code");

        QJsonValue src = co["source"];
        if (src.isArray()) {
            QStringList lines;
            for (const auto& line : src.toArray())
                lines << line.toString();
            cell.source = lines.join("");
        } else {
            cell.source = src.toString();
        }

        QJsonObject cell_metadata = co["metadata"].toObject();
        cell.title = cell_metadata["fincept_title"].toString().trimmed();
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
    selected_cell_id_ = cells_.first().id;
    rebuild_cells();
    update_status();
    update_navigator();
    LOG_INFO("CodeEditor", "Opened notebook: " + path);
    ScreenStateManager::instance().notify_changed(this);
}

void CodeEditorScreen::on_save_notebook() {
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

    QJsonArray json_cells;
    for (const auto& cell : cells_) {
        QJsonObject co;
        co["cell_type"] = cell.cell_type;

        QJsonArray src_lines;
        for (const auto& line : cell.source.split('\n'))
            src_lines.append(line + "\n");
        co["source"] = src_lines;
        QJsonObject cell_metadata;
        if (!cell.title.trimmed().isEmpty())
            cell_metadata["fincept_title"] = cell.title.trimmed();
        co["metadata"] = cell_metadata;

        if (cell.cell_type == "code") {
            co["execution_count"] = cell.execution_count > 0 ? QJsonValue(cell.execution_count) : QJsonValue();
            co["outputs"] = QJsonArray();
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
        services::FileManagerService::instance().import_file(path, "code_editor");
    }
    update_status();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void CodeEditorScreen::rebuild_cells() {
    while (cells_layout_->count() > 1) {
        auto* item = cells_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < cells_.size(); ++i) {
        auto* cw = new CellWidget(cells_[i], i, cells_container_);
        cw->set_selected(cells_[i].id == selected_cell_id_);

        connect(cw, &CellWidget::run_requested, this, &CodeEditorScreen::on_run_cell);
        connect(cw, &CellWidget::run_and_advance, this, &CodeEditorScreen::on_run_and_advance);
        connect(cw, &CellWidget::delete_requested, this, &CodeEditorScreen::on_delete_cell);
        connect(cw, &CellWidget::move_up_requested, this, &CodeEditorScreen::on_move_cell_up);
        connect(cw, &CellWidget::move_down_requested, this, &CodeEditorScreen::on_move_cell_down);
        connect(cw, &CellWidget::toggle_type_requested, this, &CodeEditorScreen::on_toggle_cell_type);
        connect(cw, &CellWidget::cell_clicked, this, &CodeEditorScreen::on_select_cell);
        connect(cw, &CellWidget::insert_below_requested, this, &CodeEditorScreen::on_insert_below);

        cells_layout_->insertWidget(i, cw);
    }
}

int CodeEditorScreen::find_cell_index(const QString& cell_id) const {
    for (int i = 0; i < cells_.size(); ++i)
        if (cells_[i].id == cell_id)
            return i;
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
    int code_count = 0, md_count = 0, executed = 0;
    for (const auto& c : cells_) {
        if (c.cell_type == "code")
            ++code_count;
        else
            ++md_count;
        if (c.execution_count > 0)
            ++executed;
    }

    QString info = QString("CELLS: %1 CODE  %2 MD  |  EXECUTED: %3").arg(code_count).arg(md_count).arg(executed);
    if (!notebook_path_.isEmpty())
        info += "  |  " + QFileInfo(notebook_path_).fileName();
    status_label_->setText(info);
}

void CodeEditorScreen::update_navigator() {
    if (navigator_)
        navigator_->rebuild(cells_, selected_cell_id_);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap CodeEditorScreen::save_state() const {
    return {{"notebook_path", notebook_path_}};
}

void CodeEditorScreen::restore_state(const QVariantMap& state) {
    const QString path = state.value("notebook_path").toString();
    if (!path.isEmpty() && notebook_path_ != path) {
        QFile f(path);
        if (f.exists()) {
            notebook_path_ = path; // pre-set so on_open_notebook picks it up
            // simulate the open flow directly
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                if (!doc.isNull()) {
                    // Delegate to on_open_notebook by setting the path and triggering
                    // the full load — but on_open_notebook opens a dialog so we
                    // reconstruct just enough to restore the path label/status.
                    // Full re-open would require extracting the load logic to a helper.
                    // For now: restore the path so the title bar shows the last file.
                    update_status();
                }
            }
        }
    }
}

} // namespace fincept::screens
