#include "screens/node_editor/properties/ExecutionResultsPanel.h"

#include <QFrame>
#include <QJsonDocument>
#include <QLabel>
#include <QScrollArea>

namespace fincept::workflow {

ExecutionResultsPanel::ExecutionResultsPanel(QWidget* parent) : QWidget(parent) {
    setFixedHeight(180);
    setObjectName("executionResultsPanel");
    build_ui();
    hide(); // hidden by default, shown during execution
}

void ExecutionResultsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Separator ──────────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #2a2a2a; border: none;");
    root->addWidget(sep);

    // ── Header ─────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(28);
    header->setStyleSheet("background: #1e1e1e;");
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 10, 0);

    auto* title = new QLabel("EXECUTION RESULTS");
    title->setStyleSheet("color: #d97706; font-family: Consolas; font-size: 11px;"
                         "font-weight: bold; letter-spacing: 0.5px;");
    hl->addWidget(title);

    hl->addStretch();

    status_label_ = new QLabel("IDLE");
    status_label_->setStyleSheet("color: #525252; font-family: Consolas; font-size: 10px; font-weight: bold;");
    hl->addWidget(status_label_);

    root->addWidget(header);

    // ── Scrollable results ─────────────────────────────────────────
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: #141414; border: none; }"
                          "QScrollBar:vertical { background: #141414; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #4a4a4a; min-height: 20px; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    results_container_ = new QWidget;
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(8, 4, 8, 4);
    results_layout_->setSpacing(2);
    results_layout_->addStretch();

    scroll->setWidget(results_container_);
    root->addWidget(scroll, 1);
}

void ExecutionResultsPanel::clear() {
    while (results_layout_->count() > 1) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    status_label_->setText("IDLE");
    status_label_->setStyleSheet("color: #525252; font-family: Consolas; font-size: 10px; font-weight: bold;");
}

void ExecutionResultsPanel::set_started(const QString& workflow_id) {
    clear();
    show();
    Q_UNUSED(workflow_id);
    status_label_->setText("RUNNING...");
    status_label_->setStyleSheet("color: #d97706; font-family: Consolas; font-size: 10px; font-weight: bold;");
}

void ExecutionResultsPanel::add_node_result(const NodeExecutionResult& result) {
    auto* row = new QWidget;
    row->setFixedHeight(22);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(4, 0, 4, 0);
    rl->setSpacing(8);

    // Status dot
    QString dot_color = result.success ? "#16a34a" : "#dc2626";
    auto* dot = new QLabel(result.success ? "OK" : "ERR");
    dot->setFixedWidth(28);
    dot->setAlignment(Qt::AlignCenter);
    dot->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 9px; font-weight: bold;").arg(dot_color));
    rl->addWidget(dot);

    // Node ID (truncated)
    auto* id_label = new QLabel(result.node_id.left(12));
    id_label->setFixedWidth(90);
    id_label->setStyleSheet("color: #808080; font-family: Consolas; font-size: 10px;");
    rl->addWidget(id_label);

    // Duration
    auto* dur = new QLabel(QString("%1ms").arg(result.duration_ms));
    dur->setFixedWidth(50);
    dur->setStyleSheet("color: #525252; font-family: Consolas; font-size: 10px;");
    rl->addWidget(dur);

    // Output preview or error
    QString preview;
    if (!result.success) {
        preview = result.error;
    } else if (result.output.isObject()) {
        preview = QString::fromUtf8(QJsonDocument(result.output.toObject()).toJson(QJsonDocument::Compact));
    } else if (result.output.isString()) {
        preview = result.output.toString();
    } else {
        preview = "OK";
    }
    auto* output_label = new QLabel(preview);
    output_label->setStyleSheet(
        QString("color: %1; font-family: Consolas; font-size: 10px;").arg(result.success ? "#e5e5e5" : "#dc2626"));
    output_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rl->addWidget(output_label, 1);

    int pos = results_layout_->count() - 1; // before stretch
    results_layout_->insertWidget(pos, row);
}

void ExecutionResultsPanel::set_finished(const WorkflowExecutionResult& result) {
    if (result.success) {
        status_label_->setText(QString("COMPLETED  %1ms").arg(result.total_duration_ms));
        status_label_->setStyleSheet("color: #16a34a; font-family: Consolas; font-size: 10px; font-weight: bold;");
    } else {
        status_label_->setText(QString("FAILED  %1").arg(result.error));
        status_label_->setStyleSheet("color: #dc2626; font-family: Consolas; font-size: 10px; font-weight: bold;");
    }
}

} // namespace fincept::workflow
