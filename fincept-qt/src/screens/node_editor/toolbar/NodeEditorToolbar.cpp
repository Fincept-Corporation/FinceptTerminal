#include "screens/node_editor/toolbar/NodeEditorToolbar.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>

namespace fincept::workflow {

static QString btn_style() {
    return QString("QPushButton {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  font-family: Consolas; font-size: 11px; font-weight: bold;"
                   "  padding: 0 10px; min-height: 22px;"
                   "}"
                   "QPushButton:hover { color: %4; background: %5; }"
                   "QPushButton:disabled { color: %6; }")
        .arg(ui::colors::BG_HOVER, ui::colors::TEXT_SECONDARY,
             ui::colors::BORDER_MED, ui::colors::TEXT_PRIMARY,
             ui::colors::BG_HOVER, ui::colors::TEXT_DIM);
}

static QString accent_btn_style() {
    return QString("QPushButton {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3; font-family: Consolas;"
                   "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
                   "}"
                   "QPushButton:hover { background: %2; color: %4; }")
        .arg(ui::colors::ACCENT_BG, ui::colors::AMBER,
             ui::colors::AMBER_DIM, ui::colors::BG_BASE);
}

NodeEditorToolbar::NodeEditorToolbar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(34);
    setObjectName("nodeEditorToolbar");
    build_ui();
}

void NodeEditorToolbar::set_workflow_name(const QString& name) {
    name_edit_->setText(name);
}

QString NodeEditorToolbar::workflow_name() const {
    return name_edit_->text();
}

void NodeEditorToolbar::set_can_undo(bool can) {
    undo_btn_->setEnabled(can);
}
void NodeEditorToolbar::set_can_redo(bool can) {
    redo_btn_->setEnabled(can);
}

void NodeEditorToolbar::set_executing(bool running) {
    QString stop_style = QString("QPushButton {"
                                   "  background: %1; color: %2;"
                                   "  border: 1px solid rgba(220,38,38,0.4); font-family: Consolas;"
                                   "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
                                   "}"
                                   "QPushButton:hover { background: %2; color: %3; }")
                              .arg(ui::colors::NEGATIVE_BG, ui::colors::NEGATIVE, ui::colors::TEXT_PRIMARY);

    execute_btn_->setText(running ? "STOP" : "EXECUTE");
    execute_btn_->setStyleSheet(running ? stop_style : accent_btn_style());
}

void NodeEditorToolbar::build_ui() {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_HOVER));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(6);

    // ── Workflow name ──────────────────────────────────────────────
    name_edit_ = new QLineEdit("Untitled Workflow");
    name_edit_->setFixedWidth(200);
    name_edit_->setStyleSheet(
        QString("QLineEdit {"
                "  background: transparent; color: %1; border: none;"
                "  border-bottom: 1px solid %2; font-family: Consolas;"
                "  font-size: 13px; font-weight: bold; padding: 2px 4px;"
                "}"
                "QLineEdit:focus { border-bottom: 1px solid %3; }")
            .arg(ui::colors::TEXT_PRIMARY, ui::colors::TEXT_DIM, ui::colors::AMBER));
    layout->addWidget(name_edit_);

    connect(name_edit_, &QLineEdit::textChanged, this, &NodeEditorToolbar::name_changed);

    // ── Status badge ───────────────────────────────────────────────
    auto* status = new QLabel("DRAFT");
    status->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                 "font-weight: bold; letter-spacing: 0.5px; padding: 0 6px;")
                              .arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(status);

    layout->addStretch();

    // ── Undo / Redo ────────────────────────────────────────────────
    undo_btn_ = new QPushButton("UNDO");
    undo_btn_->setStyleSheet(btn_style());
    undo_btn_->setEnabled(false);
    undo_btn_->setToolTip("Undo last action (Ctrl+Z)");
    layout->addWidget(undo_btn_);
    connect(undo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::undo_clicked);

    redo_btn_ = new QPushButton("REDO");
    redo_btn_->setStyleSheet(btn_style());
    redo_btn_->setEnabled(false);
    redo_btn_->setToolTip("Redo last action (Ctrl+Y)");
    layout->addWidget(redo_btn_);
    connect(redo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::redo_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep1 = new QFrame;
    sep1->setFixedSize(1, 18);
    sep1->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM));
    layout->addWidget(sep1);

    // ── Save / Load / Clear ────────────────────────────────────────
    auto* save_btn = new QPushButton("SAVE");
    save_btn->setStyleSheet(btn_style());
    save_btn->setToolTip("Save workflow to database");
    layout->addWidget(save_btn);
    connect(save_btn, &QPushButton::clicked, this, &NodeEditorToolbar::save_clicked);

    auto* load_btn = new QPushButton("LOAD");
    load_btn->setStyleSheet(btn_style());
    load_btn->setToolTip("Load a saved workflow");
    layout->addWidget(load_btn);
    connect(load_btn, &QPushButton::clicked, this, &NodeEditorToolbar::load_clicked);

    auto* clear_btn = new QPushButton("CLEAR");
    clear_btn->setStyleSheet(btn_style());
    layout->addWidget(clear_btn);
    connect(clear_btn, &QPushButton::clicked, this, &NodeEditorToolbar::clear_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep2 = new QFrame;
    sep2->setFixedSize(1, 18);
    sep2->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM));
    layout->addWidget(sep2);

    // ── Import / Export ────────────────────────────────────────────
    auto* import_btn = new QPushButton("IMPORT");
    import_btn->setStyleSheet(btn_style());
    layout->addWidget(import_btn);
    connect(import_btn, &QPushButton::clicked, this, &NodeEditorToolbar::import_clicked);

    auto* export_btn = new QPushButton("EXPORT");
    export_btn->setStyleSheet(btn_style());
    layout->addWidget(export_btn);
    connect(export_btn, &QPushButton::clicked, this, &NodeEditorToolbar::export_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep3 = new QFrame;
    sep3->setFixedSize(1, 18);
    sep3->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM));
    layout->addWidget(sep3);

    // ── Templates ──────────────────────────────────────────────────
    auto* templates_btn = new QPushButton("TEMPLATES");
    templates_btn->setStyleSheet(btn_style());
    layout->addWidget(templates_btn);
    connect(templates_btn, &QPushButton::clicked, this, &NodeEditorToolbar::templates_clicked);

    // ── Deploy ─────────────────────────────────────────────────────
    auto* deploy_btn = new QPushButton("DEPLOY");
    deploy_btn->setStyleSheet(btn_style());
    layout->addWidget(deploy_btn);
    connect(deploy_btn, &QPushButton::clicked, this, &NodeEditorToolbar::deploy_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep4 = new QFrame;
    sep4->setFixedSize(1, 18);
    sep4->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM));
    layout->addWidget(sep4);

    // ── Execute ────────────────────────────────────────────────────
    execute_btn_ = new QPushButton("EXECUTE");
    execute_btn_->setStyleSheet(accent_btn_style());
    layout->addWidget(execute_btn_);
    connect(execute_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::execute_clicked);
}

} // namespace fincept::workflow
