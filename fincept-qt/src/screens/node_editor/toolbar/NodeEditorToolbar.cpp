#include "screens/node_editor/toolbar/NodeEditorToolbar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

namespace {

static const char* kBtnStyle =
    "QPushButton {"
    "  background: #1e1e1e; color: #808080; border: 1px solid #2a2a2a;"
    "  font-family: Consolas; font-size: 11px; font-weight: bold;"
    "  padding: 0 10px; min-height: 22px;"
    "}"
    "QPushButton:hover { color: #e5e5e5; background: #252525; }"
    "QPushButton:disabled { color: #4a4a4a; }";

static const char* kAccentBtnStyle =
    "QPushButton {"
    "  background: rgba(217,119,6,0.1); color: #d97706;"
    "  border: 1px solid #78350f; font-family: Consolas;"
    "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
    "}"
    "QPushButton:hover { background: #d97706; color: #080808; }";

} // namespace

namespace fincept::workflow {

NodeEditorToolbar::NodeEditorToolbar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(34);
    setObjectName("nodeEditorToolbar");
    build_ui();
}

void NodeEditorToolbar::set_workflow_name(const QString& name)
{
    name_edit_->setText(name);
}

QString NodeEditorToolbar::workflow_name() const
{
    return name_edit_->text();
}

void NodeEditorToolbar::set_can_undo(bool can) { undo_btn_->setEnabled(can); }
void NodeEditorToolbar::set_can_redo(bool can) { redo_btn_->setEnabled(can); }

void NodeEditorToolbar::set_executing(bool running)
{
    static const char* kStopStyle =
        "QPushButton {"
        "  background: rgba(220,38,38,0.15); color: #dc2626;"
        "  border: 1px solid rgba(220,38,38,0.4); font-family: Consolas;"
        "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
        "}"
        "QPushButton:hover { background: #dc2626; color: #e5e5e5; }";

    static const char* kExecStyle =
        "QPushButton {"
        "  background: rgba(217,119,6,0.1); color: #d97706;"
        "  border: 1px solid #78350f; font-family: Consolas;"
        "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
        "}"
        "QPushButton:hover { background: #d97706; color: #080808; }";

    execute_btn_->setText(running ? "STOP" : "EXECUTE");
    execute_btn_->setStyleSheet(running ? kStopStyle : kExecStyle);
}

void NodeEditorToolbar::build_ui()
{
    setStyleSheet("background: #1e1e1e;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(6);

    // ── Workflow name ──────────────────────────────────────────────
    name_edit_ = new QLineEdit("Untitled Workflow");
    name_edit_->setFixedWidth(200);
    name_edit_->setStyleSheet(
        "QLineEdit {"
        "  background: transparent; color: #e5e5e5; border: none;"
        "  border-bottom: 1px solid #4a4a4a; font-family: Consolas;"
        "  font-size: 13px; font-weight: bold; padding: 2px 4px;"
        "}"
        "QLineEdit:focus { border-bottom: 1px solid #d97706; }");
    layout->addWidget(name_edit_);

    connect(name_edit_, &QLineEdit::textChanged, this, &NodeEditorToolbar::name_changed);

    // ── Status badge ───────────────────────────────────────────────
    auto* status = new QLabel("DRAFT");
    status->setStyleSheet(
        "color: #525252; font-family: Consolas; font-size: 10px;"
        "font-weight: bold; letter-spacing: 0.5px; padding: 0 6px;");
    layout->addWidget(status);

    layout->addStretch();

    // ── Undo / Redo ────────────────────────────────────────────────
    undo_btn_ = new QPushButton("UNDO");
    undo_btn_->setStyleSheet(kBtnStyle);
    undo_btn_->setEnabled(false);
    undo_btn_->setToolTip("Undo last action (Ctrl+Z)");
    layout->addWidget(undo_btn_);
    connect(undo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::undo_clicked);

    redo_btn_ = new QPushButton("REDO");
    redo_btn_->setStyleSheet(kBtnStyle);
    redo_btn_->setEnabled(false);
    redo_btn_->setToolTip("Redo last action (Ctrl+Y)");
    layout->addWidget(redo_btn_);
    connect(redo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::redo_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep1 = new QFrame;
    sep1->setFixedSize(1, 18);
    sep1->setStyleSheet("background: #4a4a4a; border: none;");
    layout->addWidget(sep1);

    // ── Save / Load / Clear ────────────────────────────────────────
    auto* save_btn = new QPushButton("SAVE");
    save_btn->setStyleSheet(kBtnStyle);
    save_btn->setToolTip("Save workflow to database");
    layout->addWidget(save_btn);
    connect(save_btn, &QPushButton::clicked, this, &NodeEditorToolbar::save_clicked);

    auto* load_btn = new QPushButton("LOAD");
    load_btn->setStyleSheet(kBtnStyle);
    load_btn->setToolTip("Load a saved workflow");
    layout->addWidget(load_btn);
    connect(load_btn, &QPushButton::clicked, this, &NodeEditorToolbar::load_clicked);

    auto* clear_btn = new QPushButton("CLEAR");
    clear_btn->setStyleSheet(kBtnStyle);
    layout->addWidget(clear_btn);
    connect(clear_btn, &QPushButton::clicked, this, &NodeEditorToolbar::clear_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep2 = new QFrame;
    sep2->setFixedSize(1, 18);
    sep2->setStyleSheet("background: #4a4a4a; border: none;");
    layout->addWidget(sep2);

    // ── Import / Export ────────────────────────────────────────────
    auto* import_btn = new QPushButton("IMPORT");
    import_btn->setStyleSheet(kBtnStyle);
    layout->addWidget(import_btn);
    connect(import_btn, &QPushButton::clicked, this, &NodeEditorToolbar::import_clicked);

    auto* export_btn = new QPushButton("EXPORT");
    export_btn->setStyleSheet(kBtnStyle);
    layout->addWidget(export_btn);
    connect(export_btn, &QPushButton::clicked, this, &NodeEditorToolbar::export_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep3 = new QFrame;
    sep3->setFixedSize(1, 18);
    sep3->setStyleSheet("background: #4a4a4a; border: none;");
    layout->addWidget(sep3);

    // ── Templates ──────────────────────────────────────────────────
    auto* templates_btn = new QPushButton("TEMPLATES");
    templates_btn->setStyleSheet(kBtnStyle);
    layout->addWidget(templates_btn);
    connect(templates_btn, &QPushButton::clicked, this, &NodeEditorToolbar::templates_clicked);

    // ── Deploy ─────────────────────────────────────────────────────
    auto* deploy_btn = new QPushButton("DEPLOY");
    deploy_btn->setStyleSheet(kBtnStyle);
    layout->addWidget(deploy_btn);
    connect(deploy_btn, &QPushButton::clicked, this, &NodeEditorToolbar::deploy_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep4 = new QFrame;
    sep4->setFixedSize(1, 18);
    sep4->setStyleSheet("background: #4a4a4a; border: none;");
    layout->addWidget(sep4);

    // ── Execute ────────────────────────────────────────────────────
    execute_btn_ = new QPushButton("EXECUTE");
    execute_btn_->setStyleSheet(kAccentBtnStyle);
    layout->addWidget(execute_btn_);
    connect(execute_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::execute_clicked);
}

} // namespace fincept::workflow
