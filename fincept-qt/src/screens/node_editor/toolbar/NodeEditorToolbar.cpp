#include "screens/node_editor/toolbar/NodeEditorToolbar.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>

namespace fincept::workflow {

static QString btn_style() {
    return QString("QPushButton {"
                   "  background: %1; color: %2; border: 1px solid %3;"
                   "  font-family: Consolas; font-size: 11px; font-weight: bold;"
                   "  padding: 0 10px; min-height: 22px;"
                   "}"
                   "QPushButton:hover { color: %4; background: %5; }"
                   "QPushButton:disabled { color: %6; }")
        .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_MED(), ui::colors::TEXT_PRIMARY(),
             ui::colors::BG_HOVER(), ui::colors::TEXT_DIM());
}

static QString accent_btn_style() {
    return QString("QPushButton {"
                   "  background: %1; color: %2;"
                   "  border: 1px solid %3; font-family: Consolas;"
                   "  font-size: 11px; font-weight: bold; padding: 0 12px; min-height: 22px;"
                   "}"
                   "QPushButton:hover { background: %2; color: %4; }")
        .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER(), ui::colors::AMBER_DIM(), ui::colors::BG_BASE());
}

NodeEditorToolbar::NodeEditorToolbar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(34);
    setObjectName("nodeEditorToolbar");
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_HOVER()));
    setPalette(pal);
    build_ui();
}

void NodeEditorToolbar::apply_background() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(ui::colors::BG_HOVER()));
    setPalette(pal);
}

void NodeEditorToolbar::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    apply_background();
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
                             .arg(ui::colors::NEGATIVE_BG(), ui::colors::NEGATIVE(), ui::colors::TEXT_PRIMARY());

    execute_btn_->setText(running ? tr("STOP") : tr("EXECUTE"));
    execute_btn_->setStyleSheet(running ? stop_style : accent_btn_style());
}

void NodeEditorToolbar::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void NodeEditorToolbar::retranslateUi() {
    if (name_edit_) name_edit_->setPlaceholderText(tr("Untitled Workflow"));
    if (status_badge_) status_badge_->setText(tr("DRAFT"));
    if (undo_btn_) {
        undo_btn_->setText(tr("UNDO"));
        undo_btn_->setToolTip(tr("Undo last action (Ctrl+Z)"));
    }
    if (redo_btn_) {
        redo_btn_->setText(tr("REDO"));
        redo_btn_->setToolTip(tr("Redo last action (Ctrl+Y)"));
    }
    if (save_btn_) {
        save_btn_->setText(tr("SAVE"));
        save_btn_->setToolTip(tr("Save workflow to database"));
    }
    if (load_btn_) {
        load_btn_->setText(tr("LOAD"));
        load_btn_->setToolTip(tr("Load a saved workflow"));
    }
    if (clear_btn_) clear_btn_->setText(tr("CLEAR"));
    if (import_btn_) import_btn_->setText(tr("IMPORT"));
    if (export_btn_) export_btn_->setText(tr("EXPORT"));
    if (templates_btn_) templates_btn_->setText(tr("TEMPLATES"));
    if (deploy_btn_) deploy_btn_->setText(tr("SAVE & RUN"));
    // Execute button text depends on running state; default to EXECUTE.
    if (execute_btn_) execute_btn_->setText(tr("EXECUTE"));
}

void NodeEditorToolbar::build_ui() {
    setStyleSheet(QString("QWidget#nodeEditorToolbar { background: %1; }").arg(ui::colors::BG_HOVER()));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(6);

    // ── Workflow name ──────────────────────────────────────────────
    name_edit_ = new QLineEdit(tr("Untitled Workflow"));
    name_edit_->setFixedWidth(200);
    name_edit_->setStyleSheet(QString("QLineEdit {"
                                      "  background: transparent; color: %1; border: none;"
                                      "  border-bottom: 1px solid %2; font-family: Consolas;"
                                      "  font-size: 13px; font-weight: bold; padding: 2px 4px;"
                                      "}"
                                      "QLineEdit:focus { border-bottom: 1px solid %3; }")
                                  .arg(ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_DIM(), ui::colors::AMBER()));
    layout->addWidget(name_edit_);

    connect(name_edit_, &QLineEdit::textChanged, this, &NodeEditorToolbar::name_changed);

    // ── Status badge ───────────────────────────────────────────────
    status_badge_ = new QLabel(tr("DRAFT"));
    status_badge_->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                         "font-weight: bold; letter-spacing: 0.5px; padding: 0 6px;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(status_badge_);

    layout->addStretch();

    // ── Undo / Redo ────────────────────────────────────────────────
    undo_btn_ = new QPushButton(tr("UNDO"));
    undo_btn_->setStyleSheet(btn_style());
    undo_btn_->setEnabled(false);
    undo_btn_->setToolTip(tr("Undo last action (Ctrl+Z)"));
    layout->addWidget(undo_btn_);
    connect(undo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::undo_clicked);

    redo_btn_ = new QPushButton(tr("REDO"));
    redo_btn_->setStyleSheet(btn_style());
    redo_btn_->setEnabled(false);
    redo_btn_->setToolTip(tr("Redo last action (Ctrl+Y)"));
    layout->addWidget(redo_btn_);
    connect(redo_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::redo_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep1 = new QFrame;
    sep1->setFixedSize(1, 18);
    sep1->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM()));
    layout->addWidget(sep1);

    // ── Save / Load / Clear ────────────────────────────────────────
    save_btn_ = new QPushButton(tr("SAVE"));
    save_btn_->setStyleSheet(btn_style());
    save_btn_->setToolTip(tr("Save workflow to database"));
    layout->addWidget(save_btn_);
    connect(save_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::save_clicked);

    load_btn_ = new QPushButton(tr("LOAD"));
    load_btn_->setStyleSheet(btn_style());
    load_btn_->setToolTip(tr("Load a saved workflow"));
    layout->addWidget(load_btn_);
    connect(load_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::load_clicked);

    clear_btn_ = new QPushButton(tr("CLEAR"));
    clear_btn_->setStyleSheet(btn_style());
    layout->addWidget(clear_btn_);
    connect(clear_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::clear_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep2 = new QFrame;
    sep2->setFixedSize(1, 18);
    sep2->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM()));
    layout->addWidget(sep2);

    // ── Import / Export ────────────────────────────────────────────
    import_btn_ = new QPushButton(tr("IMPORT"));
    import_btn_->setStyleSheet(btn_style());
    layout->addWidget(import_btn_);
    connect(import_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::import_clicked);

    export_btn_ = new QPushButton(tr("EXPORT"));
    export_btn_->setStyleSheet(btn_style());
    layout->addWidget(export_btn_);
    connect(export_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::export_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep3 = new QFrame;
    sep3->setFixedSize(1, 18);
    sep3->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM()));
    layout->addWidget(sep3);

    // ── Templates ──────────────────────────────────────────────────
    templates_btn_ = new QPushButton(tr("TEMPLATES"));
    templates_btn_->setStyleSheet(btn_style());
    layout->addWidget(templates_btn_);
    connect(templates_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::templates_clicked);

    // ── Deploy ─────────────────────────────────────────────────────
    deploy_btn_ = new QPushButton(tr("SAVE & RUN"));
    deploy_btn_->setStyleSheet(btn_style());
    layout->addWidget(deploy_btn_);
    connect(deploy_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::deploy_clicked);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep4 = new QFrame;
    sep4->setFixedSize(1, 18);
    sep4->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::TEXT_DIM()));
    layout->addWidget(sep4);

    // ── Execute ────────────────────────────────────────────────────
    execute_btn_ = new QPushButton(tr("EXECUTE"));
    execute_btn_->setStyleSheet(accent_btn_style());
    layout->addWidget(execute_btn_);
    connect(execute_btn_, &QPushButton::clicked, this, &NodeEditorToolbar::execute_clicked);
}

} // namespace fincept::workflow
