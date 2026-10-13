#include "screens/node_editor/properties/ExecutionResultsPanel.h"

#include "ui/theme/Theme.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>

#include <cmath>

namespace fincept::workflow {

// ── Helpers ────────────────────────────────────────────────────────────────

namespace {

QString erp_duration(int ms) {
    if (ms < 1000)
        return QString("%1ms").arg(ms);
    return QString("%1s").arg(ms / 1000.0, 0, 'f', 1);
}

// Full precision (6 significant digits turned 1234.5678 into 1234.57 and an order
// quantity of 1234567 into 1.23457e+06).
QString erp_number(double v) {
    if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 1e15)
        return QString::number(static_cast<qint64>(v));
    return QString::number(v, 'g', 12);
}

QString erp_value(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble())
        return erp_number(v.toDouble());
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

QString erp_tr(const char* text) {
    return QCoreApplication::translate("ExecutionResultsPanel", text);
}

QLabel* erp_label(const QString& text, const char* object_name) {
    auto* l = new QLabel(text);
    l->setObjectName(object_name);
    return l;
}

// One run-log row. Styling comes from the panel stylesheet (objectName + "state"
// property) — no per-widget setStyleSheet, these are created once per node per run.
QWidget* erp_make_card(const QString& state, const QString& badge_text, const QString& title,
                       const QString& title_tip, const QString& right_text) {
    auto* card = new QWidget(nullptr);
    card->setObjectName("resultCard");
    card->setProperty("state", state);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(10, 6, 10, 6);
    vl->setSpacing(4);

    auto* header_row = new QWidget(card);
    auto* hl = new QHBoxLayout(header_row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    auto* badge = erp_label(badge_text, "resultBadge");
    badge->setProperty("state", state);
    badge->setFixedWidth(56);
    hl->addWidget(badge);

    auto* name = erp_label(title, "resultName");
    name->setToolTip(title_tip);
    hl->addWidget(name);
    hl->addStretch();
    hl->addWidget(erp_label(right_text, "resultDim"));
    vl->addWidget(header_row);
    return card;
}

QWidget* erp_result_card(const NodeExecutionResult& result, const QString& node_label) {
    const bool ok = result.success;
    // The executor marks a node that never ran (branch not taken / gate not passed) with _skipped.
    const bool skipped = ok && result.output.isObject() && result.output.toObject().value("_skipped").toBool(false);
    const QString state = !ok ? QStringLiteral("error") : skipped ? QStringLiteral("skipped") : QStringLiteral("ok");
    const QString badge = !ok ? erp_tr("FAILED") : skipped ? erp_tr("SKIPPED") : erp_tr("OK");
    const QString title = node_label.isEmpty() ? result.node_id.left(8) + "…" : node_label;

    auto* card = erp_make_card(state, badge, title, result.node_id, skipped ? QString() : erp_duration(result.duration_ms));
    auto* vl = qobject_cast<QVBoxLayout*>(card->layout());

    if (!ok) {
        auto* err = erp_label(result.error.isEmpty() ? erp_tr("Unknown error") : result.error, "resultError");
        err->setWordWrap(true);
        err->setTextInteractionFlags(Qt::TextSelectableByMouse);
        vl->addWidget(err);
        return card;
    }

    if (skipped) {
        // A node may say why it stopped the flow (a Price Alert whose condition is not met).
        const QString reason = result.output.toObject().value("reason").toString();
        auto* val = erp_label(reason.isEmpty() ? erp_tr("Did not run — an upstream branch or gate did not let data "
                                                        "through")
                                               : erp_tr("Not triggered — %1").arg(reason),
                              "resultDim");
        val->setWordWrap(true);
        vl->addWidget(val);
        return card;
    }

    if (result.output.isObject()) {
        const auto obj = result.output.toObject();
        if (obj.contains("response")) {
            // LLM / Agent response
            QString response = obj.value("response").toString();
            response.replace("\\n", "\n");
            auto* text = erp_label(response, "resultText");
            text->setWordWrap(true);
            text->setTextInteractionFlags(Qt::TextSelectableByMouse);
            vl->addWidget(text);
            if (obj.contains("exec_ms"))
                vl->addWidget(
                    erp_label(erp_tr("Agent ran in %1").arg(erp_duration(obj.value("exec_ms").toInt())), "resultDim"));
            return card;
        }

        // Generic key-value rows (order results, quotes, ...). A trading result names its
        // mode first so a paper fill can never be mistaken for a live one.
        auto* grid = new QWidget(card);
        auto* gl = new QVBoxLayout(grid);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setSpacing(2);
        QStringList keys = obj.keys();
        if (keys.removeOne(QStringLiteral("mode")))
            keys.prepend(QStringLiteral("mode"));
        for (const QString& k : keys) {
            auto* row = new QWidget(grid);
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(8);
            auto* key = erp_label(k + ":", "resultKey");
            key->setFixedWidth(110);
            rl->addWidget(key);
            auto* val = erp_label(erp_value(obj.value(k)), "resultValue");
            val->setWordWrap(true);
            val->setTextInteractionFlags(Qt::TextSelectableByMouse);
            rl->addWidget(val, 1);
            gl->addWidget(row);
        }
        vl->addWidget(grid);
        return card;
    }

    if (!result.output.isNull() && !result.output.isUndefined()) {
        auto* val = erp_label(erp_value(result.output), "resultValue");
        val->setWordWrap(true);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        vl->addWidget(val);
    }
    return card;
}

void erp_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ── ExecutionResultsPanel ──────────────────────────────────────────────────

ExecutionResultsPanel::ExecutionResultsPanel(QWidget* parent) : QWidget(parent) {
    setFixedHeight(34); // header-only until execution starts (collapsed by default)
    setObjectName("executionResultsPanel");
    build_ui();
    hide();
}

void ExecutionResultsPanel::build_ui() {
    // Every row and label below is styled from this one sheet.
    setStyleSheet(
        QString("QWidget#erpHeader { background: %1; }"
                "QFrame#erpSep { background: %2; border: none; }"
                "QLabel { font-family: Consolas; font-size: 10px; background: transparent; }"
                "QLabel#erpTitle { color: %3; font-weight: bold; letter-spacing: 0.5px; }"
                "QLabel#erpCounter { color: %4; }"
                "QLabel#erpCounter[errors=\"true\"] { color: %5; }"
                "QLabel#erpStatus { color: %4; font-weight: bold; }"
                "QLabel#erpStatus[state=\"running\"] { color: %3; }"
                "QLabel#erpStatus[state=\"done\"] { color: %6; }"
                "QLabel#erpStatus[state=\"failed\"] { color: %5; }"
                "QLabel#erpBanner { color: %7; padding: 4px 10px; font-weight: bold; border-bottom: 1px solid %2; }"
                "QLabel#erpBanner[live=\"true\"] { color: %5; }"
                "QPushButton#erpLinkBtn { background: transparent; color: %4; border: none; font-family: Consolas;"
                "  font-size: 10px; }"
                "QPushButton#erpLinkBtn:hover { color: %8; }"
                "QWidget#erpResults { background: %9; }"
                "QWidget#resultCard { background: %10; border-left: 3px solid %2; border-bottom: 1px solid %2; }")
            .arg(ui::colors::BORDER_DIM(), ui::colors::BORDER_MED(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::NEGATIVE(), ui::colors::POSITIVE(), ui::colors::TEXT_SECONDARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED())
            .arg(ui::colors::BG_HOVER()) +
        QString("QWidget#resultCard[state=\"ok\"] { border-left: 3px solid %1; }"
                "QWidget#resultCard[state=\"error\"] { border-left: 3px solid %2; }"
                "QWidget#resultCard[state=\"running\"] { border-left: 3px solid %3; }"
                "QLabel#resultBadge { font-weight: bold; color: %4; }"
                "QLabel#resultBadge[state=\"ok\"] { color: %1; }"
                "QLabel#resultBadge[state=\"error\"] { color: %2; }"
                "QLabel#resultBadge[state=\"running\"] { color: %3; }"
                "QLabel#resultName { color: %5; font-size: 11px; font-weight: bold; }"
                "QLabel#resultDim { color: %4; font-size: 9px; }"
                "QLabel#resultError { color: %2; padding: 2px 0; }"
                "QLabel#resultText, QLabel#resultValue { color: %5; }"
                "QLabel#resultKey { color: %6; }"
                "QScrollArea#erpScroll { background: %7; border: none; }"
                "QScrollArea#erpScroll QScrollBar:vertical { background: %7; width: 5px; border: none; }"
                "QScrollArea#erpScroll QScrollBar::handle:vertical { background: %8; min-height: 20px; }"
                "QScrollArea#erpScroll QScrollBar::add-line:vertical,"
                "QScrollArea#erpScroll QScrollBar::sub-line:vertical { height: 0; }")
            .arg(ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(),
                 ui::colors::BORDER_BRIGHT()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sep = new QFrame;
    sep->setObjectName("erpSep");
    sep->setFixedHeight(1);
    root->addWidget(sep);

    // ── Header bar ────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("erpHeader");
    header->setFixedHeight(32);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 8, 0);
    hl->setSpacing(8);

    collapse_btn_ = new QPushButton("▸");
    collapse_btn_->setObjectName("erpLinkBtn");
    collapse_btn_->setFixedSize(18, 18);
    connect(collapse_btn_, &QPushButton::clicked, this, [this]() { set_collapsed(!collapsed_); });
    hl->addWidget(collapse_btn_);

    title_label_ = erp_label(QString(), "erpTitle");
    hl->addWidget(title_label_);
    node_counter_ = erp_label(QString(), "erpCounter");
    hl->addWidget(node_counter_);
    hl->addStretch();
    status_label_ = erp_label(QString(), "erpStatus");
    hl->addWidget(status_label_);

    auto* vsep = new QFrame;
    vsep->setObjectName("erpSep");
    vsep->setFixedSize(1, 16);
    hl->addWidget(vsep);

    clear_btn_ = new QPushButton;
    clear_btn_->setObjectName("erpLinkBtn");
    clear_btn_->setFixedHeight(20);
    connect(clear_btn_, &QPushButton::clicked, this, &ExecutionResultsPanel::clear);
    hl->addWidget(clear_btn_);

    copy_btn_ = new QPushButton;
    copy_btn_->setObjectName("erpLinkBtn");
    copy_btn_->setFixedHeight(20);
    connect(copy_btn_, &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(copy_buffer_);
        copy_btn_->setText(tr("COPIED!"));
        QTimer::singleShot(1500, copy_btn_, [this]() { copy_btn_->setText(tr("COPY")); });
    });
    hl->addWidget(copy_btn_);

    root->addWidget(header);

    mode_banner_ = erp_label(QString(), "erpBanner");
    mode_banner_->setWordWrap(true);
    mode_banner_->hide();
    root->addWidget(mode_banner_);

    // ── Scrollable results area ────────────────────────────────────
    scroll_area_ = new QScrollArea;
    scroll_area_->setObjectName("erpScroll");
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    results_container_ = new QWidget(this);
    results_container_->setObjectName("erpResults");
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(0);
    results_layout_->addStretch();

    scroll_area_->setWidget(results_container_);
    scroll_area_->hide(); // starts collapsed
    root->addWidget(scroll_area_, 1);

    retranslateUi();
}

void ExecutionResultsPanel::set_collapsed(bool collapsed) {
    collapsed_ = collapsed;
    collapse_btn_->setText(collapsed_ ? "▸" : "▾");
    scroll_area_->setVisible(!collapsed_);
    const int banner_h = mode_banner_->isVisible() ? mode_banner_->sizeHint().height() : 0;
    setFixedHeight(collapsed_ ? 34 + banner_h : 300);
}

void ExecutionResultsPanel::clear() {
    while (results_layout_->count() > 1) {
        auto* item = results_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    running_cards_.clear();
    node_count_ = 0;
    error_count_ = 0;
    skipped_count_ = 0;
    copy_buffer_.clear();
    run_state_ = RunState::Idle;
    refresh_node_counter();
    refresh_status_label();
}

void ExecutionResultsPanel::set_started(const QString& workflow_id) {
    Q_UNUSED(workflow_id);
    clear();
    show();
    set_collapsed(false); // auto-expand when execution starts
    run_state_ = RunState::Running;
    refresh_status_label();
}

void ExecutionResultsPanel::set_mode_banner(const QString& text, bool live) {
    mode_banner_->setText(text);
    mode_banner_->setVisible(!text.isEmpty());
    mode_banner_->setProperty("live", live);
    erp_repolish(mode_banner_);
    if (!text.isEmpty())
        copy_buffer_ += text + "\n\n";
}

void ExecutionResultsPanel::insert_card(QWidget* card, int pos) {
    if (pos < 0 || pos > results_layout_->count() - 1)
        pos = results_layout_->count() - 1; // before trailing stretch
    results_layout_->insertWidget(pos, card);
}

void ExecutionResultsPanel::scroll_to_bottom() {
    QTimer::singleShot(0, scroll_area_, [this]() {
        scroll_area_->verticalScrollBar()->setValue(scroll_area_->verticalScrollBar()->maximum());
    });
}

void ExecutionResultsPanel::add_node_running(const QString& node_id, const QString& node_label) {
    if (running_cards_.contains(node_id))
        return;
    auto* card = erp_make_card(QStringLiteral("running"), tr("RUNNING"),
                               node_label.isEmpty() ? node_id.left(8) + "…" : node_label, node_id, QString());
    insert_card(card);
    running_cards_.insert(node_id, card);
    scroll_to_bottom();
}

void ExecutionResultsPanel::add_node_result(const NodeExecutionResult& result, const QString& node_label) {
    ++node_count_;
    const bool skipped =
        result.success && result.output.isObject() && result.output.toObject().value("_skipped").toBool(false);
    if (!result.success)
        ++error_count_;
    else if (skipped)
        ++skipped_count_;
    refresh_node_counter();

    // Plain-text copy of the log
    copy_buffer_ += QString("[%1] %2  %3\n")
                        .arg(!result.success ? "FAILED" : skipped ? "SKIPPED" : "OK")
                        .arg(node_label.isEmpty() ? result.node_id.left(8) : node_label)
                        .arg(erp_duration(result.duration_ms));
    if (!result.success) {
        copy_buffer_ += result.error + "\n";
    } else if (result.output.isObject() && !skipped) {
        auto obj = result.output.toObject();
        if (obj.contains("response")) {
            QString r = obj.value("response").toString();
            r.replace("\\n", "\n");
            copy_buffer_ += r + "\n";
        } else {
            copy_buffer_ += QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)) + "\n";
        }
    } else if (!skipped) {
        copy_buffer_ += erp_value(result.output) + "\n";
    }
    copy_buffer_ += "\n";

    // Replace the node's RUNNING row in place, so the log keeps launch order.
    int pos = -1;
    if (QWidget* running = running_cards_.take(result.node_id)) {
        pos = results_layout_->indexOf(running);
        results_layout_->removeWidget(running);
        running->deleteLater();
    }
    insert_card(erp_result_card(result, node_label), pos);
    scroll_to_bottom();
}

void ExecutionResultsPanel::set_finished(const WorkflowExecutionResult& result) {
    last_total_ms_ = result.total_duration_ms;
    if (result.success) {
        run_state_ = RunState::Done;
    } else if (result.error.startsWith("Execution stopped")) {
        run_state_ = RunState::Stopped;
        last_failure_.clear();
    } else {
        run_state_ = RunState::Failed;
        last_failure_ = result.error;
    }
    // A run that ended before anything launched (cycle, refused) explains itself in the log.
    if (!result.success && node_count_ == 0 && running_cards_.isEmpty() && !result.error.isEmpty()) {
        NodeExecutionResult nr;
        nr.success = false;
        nr.error = result.error;
        insert_card(erp_result_card(nr, tr("Workflow")));
    }
    refresh_status_label();
    copy_buffer_ += tr("Result: %1 in %2").arg(result.success ? tr("completed") : tr("failed: %1").arg(result.error),
                                                erp_duration(result.total_duration_ms)) +
                    "\n";
}

// Re-render the node/error counter for the current counts.
void ExecutionResultsPanel::refresh_node_counter() {
    if (!node_counter_)
        return;
    if (node_count_ == 0) {
        node_counter_->setText("");
    } else {
        QString counter = tr("%n node(s)", "", node_count_);
        if (error_count_ > 0)
            counter += "  " + tr("%n failed", "", error_count_);
        if (skipped_count_ > 0)
            counter += "  " + tr("%n skipped", "", skipped_count_);
        node_counter_->setText(counter);
    }
    node_counter_->setProperty("errors", error_count_ > 0);
    erp_repolish(node_counter_);
}

// Re-render the status badge for the current run state.
void ExecutionResultsPanel::refresh_status_label() {
    if (!status_label_)
        return;
    QString text;
    QString state;
    switch (run_state_) {
        case RunState::Idle:
            text = tr("IDLE");
            break;
        case RunState::Running:
            text = tr("RUNNING…");
            state = QStringLiteral("running");
            break;
        case RunState::Done:
            text = tr("DONE  %1").arg(erp_duration(last_total_ms_));
            state = QStringLiteral("done");
            break;
        case RunState::Stopped:
            text = tr("STOPPED");
            state = QStringLiteral("failed");
            break;
        case RunState::Failed:
            text = tr("FAILED  %1").arg(last_failure_.left(160));
            state = QStringLiteral("failed");
            break;
    }
    status_label_->setText(text);
    status_label_->setToolTip(run_state_ == RunState::Failed ? last_failure_ : QString());
    status_label_->setProperty("state", state);
    erp_repolish(status_label_);
}

void ExecutionResultsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ExecutionResultsPanel::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("RUN LOG"));
    if (collapse_btn_)
        collapse_btn_->setToolTip(tr("Expand / Collapse"));
    if (clear_btn_)
        clear_btn_->setText(tr("CLEAR"));
    if (copy_btn_) {
        copy_btn_->setText(tr("COPY"));
        copy_btn_->setToolTip(tr("Copy the run log to the clipboard"));
    }
    refresh_status_label();
    refresh_node_counter();
    // Result cards are data-driven snapshots of past executions — left as rendered.
}

} // namespace fincept::workflow
