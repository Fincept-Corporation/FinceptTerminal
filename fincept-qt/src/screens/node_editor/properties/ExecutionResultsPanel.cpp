#include "screens/node_editor/properties/ExecutionResultsPanel.h"

#include "ui/theme/Theme.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

namespace fincept::workflow {

// ── Helpers ────────────────────────────────────────────────────────────────

static QString format_duration(int ms) {
    if (ms < 1000)
        return QString("%1ms").arg(ms);
    return QString("%1s").arg(ms / 1000.0, 0, 'f', 1);
}

static QString value_to_string(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 6);
    if (v.isBool())
        return v.toBool() ? "true" : "false";
    if (v.isNull() || v.isUndefined())
        return "—";
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

// Build a result card widget for a single node execution
static QWidget* make_result_card(const NodeExecutionResult& result) {
    bool ok = result.success;

    auto* card = new QWidget(nullptr);
    card->setObjectName("resultCard");
    card->setStyleSheet(QString("QWidget#resultCard {"
                                "  background: %1;"
                                "  border-left: 3px solid %2;"
                                "  border-bottom: 1px solid %3;"
                                "  margin: 0;"
                                "}")
                            .arg(ui::colors::BG_HOVER(), ok ? ui::colors::POSITIVE.get() : ui::colors::NEGATIVE.get(),
                                 ui::colors::BORDER_MED()));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(4);

    // ── Header row ─────────────────────────────────────────────────
    auto* header_row = new QWidget(nullptr);
    auto* hl = new QHBoxLayout(header_row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    // Status badge
    auto* badge = new QLabel(ok ? "✓" : "✗");
    badge->setFixedWidth(16);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px;"
                                 " font-weight: bold;")
                             .arg(ok ? ui::colors::POSITIVE.get() : ui::colors::NEGATIVE.get()));
    hl->addWidget(badge);

    // Node ID (truncated)
    auto* id_lbl = new QLabel(result.node_id.left(8) + "…");
    id_lbl->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
    id_lbl->setToolTip(result.node_id);
    hl->addWidget(id_lbl);

    hl->addStretch();

    // Duration
    auto* dur_lbl = new QLabel(format_duration(result.duration_ms));
    dur_lbl->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(dur_lbl);

    vl->addWidget(header_row);

    // ── Content ────────────────────────────────────────────────────
    if (!ok) {
        // Error message
        auto* err = new QLabel(result.error.isEmpty() ? "Unknown error" : result.error);
        err->setWordWrap(true);
        err->setTextInteractionFlags(Qt::TextSelectableByMouse);
        err->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                   " background: transparent; padding: 2px 0;")
                               .arg(ui::colors::NEGATIVE()));
        vl->addWidget(err);
    } else if (result.output.isObject()) {
        auto obj = result.output.toObject();

        // LLM / Agent response
        if (obj.contains("response")) {
            QString response = obj.value("response").toString();

            // Replace literal \n sequences with actual newlines
            response.replace("\\n", "\n");

            auto* text = new QLabel(response);
            text->setWordWrap(true);
            text->setTextInteractionFlags(Qt::TextSelectableByMouse);
            text->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                        " background: transparent; line-height: 1.4;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
            vl->addWidget(text);

            // Show exec time if present
            if (obj.contains("exec_ms")) {
                auto* meta = new QLabel(QString("Agent ran in %1ms").arg(obj.value("exec_ms").toInt()));
                meta->setStyleSheet(
                    QString("color: %1; font-family: Consolas; font-size: 9px;").arg(ui::colors::TEXT_TERTIARY()));
                vl->addWidget(meta);
            }
        }
        // Price / market data
        else if (obj.contains("price") && obj.contains("symbol")) {
            double price = obj["price"].toDouble();
            auto* price_lbl = new QLabel(QString("%1  $%2").arg(obj["symbol"].toString()).arg(price, 0, 'f', 2));
            price_lbl->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 11px;"
                                             " font-weight: bold;")
                                         .arg(ui::colors::TEXT_PRIMARY()));
            vl->addWidget(price_lbl);
        }
        // Generic key-value pairs
        else {
            auto* grid = new QWidget(nullptr);
            auto* gl = new QVBoxLayout(grid);
            gl->setContentsMargins(0, 0, 0, 0);
            gl->setSpacing(2);

            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                auto* row = new QWidget(nullptr);
                auto* rl = new QHBoxLayout(row);
                rl->setContentsMargins(0, 0, 0, 0);
                rl->setSpacing(8);

                auto* key = new QLabel(it.key() + ":");
                key->setFixedWidth(110);
                key->setStyleSheet(
                    QString("color: %1; font-family: Consolas; font-size: 10px;").arg(ui::colors::TEXT_SECONDARY()));
                rl->addWidget(key);

                QString valStr = value_to_string(it.value());
                auto* val = new QLabel(valStr);
                val->setWordWrap(true);
                val->setTextInteractionFlags(Qt::TextSelectableByMouse);
                val->setStyleSheet(
                    QString("color: %1; font-family: Consolas; font-size: 10px;").arg(ui::colors::TEXT_PRIMARY()));
                rl->addWidget(val, 1);

                gl->addWidget(row);
            }
            vl->addWidget(grid);
        }
    } else if (!result.output.isNull() && !result.output.isUndefined()) {
        // Scalar output
        auto* val = new QLabel(value_to_string(result.output));
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        val->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;").arg(ui::colors::TEXT_PRIMARY()));
        vl->addWidget(val);
    }

    return card;
}

// ── ExecutionResultsPanel ──────────────────────────────────────────────────

ExecutionResultsPanel::ExecutionResultsPanel(QWidget* parent) : QWidget(parent) {
    setFixedHeight(34); // header-only until execution starts (collapsed by default)
    setObjectName("executionResultsPanel");
    build_ui();
    hide();
}

void ExecutionResultsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top separator ──────────────────────────────────────────────
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED()));
    root->addWidget(sep);

    // ── Header bar ────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(8);

    // Collapse toggle
    collapse_btn_ = new QPushButton("▸");
    collapse_btn_->setFixedSize(18, 18);
    collapse_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                         "  border: none; font-size: 10px; }"
                                         "QPushButton:hover { color: %2; }")
                                     .arg(ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_PRIMARY()));
    collapse_btn_->setToolTip("Expand / Collapse");
    connect(collapse_btn_, &QPushButton::clicked, this, [this]() { set_collapsed(!collapsed_); });
    hl->addWidget(collapse_btn_);

    auto* title = new QLabel("EXECUTION RESULTS");
    title->setStyleSheet(QString("color: %1; font-family: Consolas; font-size: 10px;"
                                 " font-weight: bold; letter-spacing: 0.5px;")
                             .arg(ui::colors::AMBER()));
    hl->addWidget(title);

    // Node counter badge
    node_counter_ = new QLabel("");
    node_counter_->setStyleSheet(
        QString("color: %1; font-family: Consolas; font-size: 10px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(node_counter_);

    hl->addStretch();

    // Status label
    status_label_ = new QLabel("IDLE");
    status_label_->setStyleSheet(QString("color: %1; font-family: Consolas;"
                                         " font-size: 10px; font-weight: bold;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(status_label_);

    // Separator
    auto* vsep = new QFrame;
    vsep->setFixedSize(1, 16);
    vsep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_MED()));
    hl->addWidget(vsep);

    // Clear button
    clear_btn_ = new QPushButton("CLEAR");
    clear_btn_->setFixedHeight(20);
    clear_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                      "  border: none; font-family: Consolas; font-size: 10px; }"
                                      "QPushButton:hover { color: %2; }")
                                  .arg(ui::colors::TEXT_TERTIARY(), ui::colors::TEXT_PRIMARY()));
    connect(clear_btn_, &QPushButton::clicked, this, &ExecutionResultsPanel::clear);
    hl->addWidget(clear_btn_);

    // Copy button
    copy_btn_ = new QPushButton("COPY");
    copy_btn_->setFixedHeight(20);
    copy_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                     "  border: none; font-family: Consolas; font-size: 10px; }"
                                     "QPushButton:hover { color: %2; }")
                                 .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER()));
    copy_btn_->setToolTip("Copy all results to clipboard");
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(copy_buffer_);
        copy_btn_->setText("COPIED!");
        copy_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                         "  border: none; font-family: Consolas; font-size: 10px; }")
                                     .arg(ui::colors::POSITIVE()));
        QTimer::singleShot(1500, copy_btn_, [this]() {
            copy_btn_->setText("COPY");
            copy_btn_->setStyleSheet(QString("QPushButton { background: transparent; color: %1;"
                                             "  border: none; font-family: Consolas; font-size: 10px; }"
                                             "QPushButton:hover { color: %2; }")
                                         .arg(ui::colors::TEXT_TERTIARY(), ui::colors::AMBER()));
        });
    });
    hl->addWidget(copy_btn_);

    root->addWidget(header);

    // ── Scrollable results area ────────────────────────────────────
    scroll_area_ = new QScrollArea;
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area_->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical { background: %1; width: 5px; border: none; }"
                "QScrollBar::handle:vertical { background: %2; min-height: 20px; border-radius: 2px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_BRIGHT()));

    results_container_ = new QWidget(this);
    results_container_->setStyleSheet(QString("background: %1;").arg(ui::colors::BG_RAISED()));
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(0);
    results_layout_->addStretch();

    scroll_area_->setWidget(results_container_);
    scroll_area_->hide(); // starts collapsed
    root->addWidget(scroll_area_, 1);
}

void ExecutionResultsPanel::set_collapsed(bool collapsed) {
    collapsed_ = collapsed;
    collapse_btn_->setText(collapsed_ ? "▸" : "▾");
    scroll_area_->setVisible(!collapsed_);
    setFixedHeight(collapsed_ ? 34 : 280);
}

void ExecutionResultsPanel::clear() {
    while (results_layout_->count() > 1) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    node_count_ = 0;
    error_count_ = 0;
    copy_buffer_.clear();
    node_counter_->setText("");
    status_label_->setText("IDLE");
    status_label_->setStyleSheet(QString("color: %1; font-family: Consolas;"
                                         " font-size: 10px; font-weight: bold;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
}

void ExecutionResultsPanel::set_started(const QString& workflow_id) {
    Q_UNUSED(workflow_id);
    clear();
    show();
    set_collapsed(false); // auto-expand when execution starts
    status_label_->setText("RUNNING…");
    status_label_->setStyleSheet(QString("color: %1; font-family: Consolas;"
                                         " font-size: 10px; font-weight: bold;")
                                     .arg(ui::colors::AMBER()));
}

void ExecutionResultsPanel::add_node_result(const NodeExecutionResult& result) {
    ++node_count_;
    if (!result.success)
        ++error_count_;

    // Update counter
    QString counter = QString("%1 node%2").arg(node_count_).arg(node_count_ == 1 ? "" : "s");
    if (error_count_ > 0)
        counter += QString("  %1 error%2").arg(error_count_).arg(error_count_ == 1 ? "" : "s");
    node_counter_->setText(counter);
    node_counter_->setStyleSheet(
        QString("color: %1; font-family: Consolas; font-size: 10px;")
            .arg(error_count_ > 0 ? ui::colors::NEGATIVE.get() : ui::colors::TEXT_TERTIARY.get()));

    // Append plain-text representation to copy buffer
    copy_buffer_ += QString("[%1] %2  %3\n")
                        .arg(result.success ? "OK" : "ERR")
                        .arg(result.node_id.left(8))
                        .arg(format_duration(result.duration_ms));
    if (!result.success) {
        copy_buffer_ += result.error + "\n";
    } else if (result.output.isObject()) {
        auto obj = result.output.toObject();
        if (obj.contains("response")) {
            QString r = obj.value("response").toString();
            r.replace("\\n", "\n");
            copy_buffer_ += r + "\n";
        } else {
            copy_buffer_ += QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)) + "\n";
        }
    } else {
        copy_buffer_ += value_to_string(result.output) + "\n";
    }
    copy_buffer_ += "\n";

    auto* card = make_result_card(result);
    int pos = results_layout_->count() - 1; // before trailing stretch
    results_layout_->insertWidget(pos, card);

    // Auto-scroll to bottom
    QTimer::singleShot(0, scroll_area_, [this]() {
        scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void ExecutionResultsPanel::set_finished(const WorkflowExecutionResult& result) {
    if (result.success) {
        status_label_->setText(QString("DONE  %1").arg(format_duration(result.total_duration_ms)));
        status_label_->setStyleSheet(QString("color: %1; font-family: Consolas;"
                                             " font-size: 10px; font-weight: bold;")
                                         .arg(ui::colors::POSITIVE()));
    } else {
        QString msg =
            result.error.startsWith("Execution stopped") ? "STOPPED" : QString("FAILED  %1").arg(result.error);
        status_label_->setText(msg);
        status_label_->setStyleSheet(QString("color: %1; font-family: Consolas;"
                                             " font-size: 10px; font-weight: bold;")
                                         .arg(ui::colors::NEGATIVE()));
    }
}

} // namespace fincept::workflow
