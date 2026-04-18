#include "screens/dashboard/widgets/AgentErrorsWidget.h"

#include "datahub/DataHub.h"
#include "ui/theme/Theme.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QSpinBox>

namespace fincept::screens::widgets {

AgentErrorsWidget::AgentErrorsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget("AGENT ERRORS", parent) {
    auto* vl = content_layout();
    vl->setContentsMargins(8, 6, 8, 6);
    vl->setSpacing(4);

    table_ = new QTableWidget(0, 3, this);
    table_->setHorizontalHeaderLabels({"Time", "Context", "Message"});
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setShowGrid(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    vl->addWidget(table_, 1);

    set_configurable(true);
    apply_styles();
    apply_config(cfg);
}

QJsonObject AgentErrorsWidget::config() const {
    QJsonObject o;
    o.insert("max_rows", max_rows_);
    return o;
}

void AgentErrorsWidget::apply_config(const QJsonObject& cfg) {
    max_rows_ = qBound(3, cfg.value("max_rows").toInt(10), 100);
    while (entries_.size() > max_rows_)
        entries_.removeLast();
    render();

    if (isVisible() && !hub_active_)
        hub_resubscribe();
}

void AgentErrorsWidget::hub_resubscribe() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    hub.subscribe_pattern(this, QStringLiteral("agent:error:*"),
                          [this](const QString& topic, const QVariant& v) { on_error(topic, v); });
    hub_active_ = true;
}

void AgentErrorsWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void AgentErrorsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_resubscribe();
}

void AgentErrorsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void AgentErrorsWidget::on_error(const QString& /*topic*/, const QVariant& value) {
    const QJsonObject obj = value.toJsonObject();
    ErrorEntry e;
    e.when = QDateTime::currentDateTime();
    e.context = obj.value("context").toString();
    e.message = obj.value("message").toString();
    entries_.prepend(e);
    while (entries_.size() > max_rows_)
        entries_.removeLast();
    render();
}

void AgentErrorsWidget::render() {
    table_->setRowCount(entries_.size());
    for (int i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];
        auto* tm = new QTableWidgetItem(e.when.toString("HH:mm:ss"));
        auto* ctx = new QTableWidgetItem(e.context);
        auto* msg = new QTableWidgetItem(e.message);
        msg->setForeground(QColor(ui::colors::NEGATIVE()));
        msg->setToolTip(e.message);
        table_->setItem(i, 0, tm);
        table_->setItem(i, 1, ctx);
        table_->setItem(i, 2, msg);
    }
    set_loading(false);
}

QDialog* AgentErrorsWidget::make_config_dialog(QWidget* parent) {
    auto* dlg = new QDialog(parent);
    dlg->setWindowTitle("Configure — Agent Errors");
    auto* form = new QFormLayout(dlg);

    auto* spin = new QSpinBox(dlg);
    spin->setRange(3, 100);
    spin->setValue(max_rows_);
    form->addRow("Max rows", spin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg, spin]() {
        QJsonObject cfg;
        cfg.insert("max_rows", spin->value());
        apply_config(cfg);
        emit config_changed(cfg);
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return dlg;
}

void AgentErrorsWidget::on_theme_changed() {
    apply_styles();
}

void AgentErrorsWidget::apply_styles() {
    table_->setStyleSheet(QString(
        "QTableWidget{background:transparent;color:%1;gridline-color:%2;font-size:10px;border:none;}"
        "QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %2;"
        "padding:2px 4px;font-size:9px;font-weight:bold;}"
        "QTableWidget::item{padding:2px 4px;}")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
             ui::colors::TEXT_TERTIARY()));
}

} // namespace fincept::screens::widgets
