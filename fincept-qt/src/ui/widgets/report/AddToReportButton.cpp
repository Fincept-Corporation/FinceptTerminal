// AddToReportButton.cpp — see AddToReportButton.h.

#include "ui/widgets/report/AddToReportButton.h"

#include "ui/notifications/NotificationService.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

namespace fincept::ui {

using fincept::services::AppendResult;
using fincept::services::ReportInbox;
using fincept::services::ReportTarget;

namespace {

QString atr_escape(QString s) {
    return s.replace(QLatin1Char('&'), QStringLiteral("&&")); // a menu / button label, not a mnemonic
}

void atr_toast(const AppendResult& r) {
    if (r.ok)
        ToastService::instance().post(
            ToastService::Severity::Success,
            AddToReportButton::tr("Added %n block(s) to “%1”", nullptr, static_cast<int>(r.block_ids.size()))
                .arg(r.report_title),
            QStringLiteral("report_builder"));
    else
        ToastService::instance().post(ToastService::Severity::Error,
                                      AddToReportButton::tr("Could not add to the report: %1").arg(r.error),
                                      QStringLiteral("report_builder"));
}

} // namespace

AddToReportButton::AddToReportButton(const QString& source_label, QWidget* parent)
    : QWidget(parent), source_label_(source_label) {
    options_.source_label = source_label;
    setObjectName(QStringLiteral("rbAddToReport"));
    // One selector-scoped sheet, set before the children exist.
    setStyleSheet(QStringLiteral("#rbAddToReport QToolButton { background: %1; color: %2; border: 1px solid %3;"
                                 " padding: 2px 8px; font-size: %4px; font-weight: bold; }"
                                 "#rbAddToReport QToolButton:hover { border-color: %5; color: %5; }"
                                 "#rbAddToReport QToolButton:disabled { color: %6; }"
                                 "#rbAddToReport QLabel { background: transparent; color: %7; font-size: %4px; }"
                                 "#rbAddToReport QLabel[rbTone=\"bad\"] { color: %8; }")
                      .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                           ui::fonts::px(-2), ui::colors::AMBER(), ui::colors::TEXT_TERTIARY(),
                           ui::colors::TEXT_SECONDARY(), ui::colors::DANGER()));
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    button_ = new QToolButton(this);
    button_->setPopupMode(QToolButton::MenuButtonPopup);
    button_->setCursor(Qt::PointingHandCursor);
    menu_ = new QMenu(button_);
    button_->setMenu(menu_);
    hl->addWidget(button_);
    result_ = new QLabel(this);
    result_->setTextFormat(Qt::RichText);
    result_->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    result_->hide();
    hl->addWidget(result_);
    result_timer_ = new QTimer(this);
    result_timer_->setSingleShot(true);
    result_timer_->setInterval(7000);
    connect(result_timer_, &QTimer::timeout, result_, &QLabel::hide);
    connect(button_, &QToolButton::clicked, this, [this]() { send(ReportTarget::current()); });
    connect(menu_, &QMenu::aboutToShow, this, &AddToReportButton::populate_menu);
    connect(result_, &QLabel::linkActivated, this, [](const QString& link) {
        // "open:<report id>:<block id>"
        const QStringList parts = link.split(QLatin1Char(':'));
        if (parts.size() == 3)
            ReportInbox::open_report(parts[1].toLongLong(), parts[2].toInt());
    });
    retranslate();
}

void AddToReportButton::set_payload_provider(PayloadProvider provider) {
    provider_ = std::move(provider);
}

void AddToReportButton::set_options(const fincept::services::AppendOptions& options) {
    options_ = options;
    if (options_.source_label.isEmpty())
        options_.source_label = source_label_;
}

void AddToReportButton::set_show_result(bool on) {
    show_result_ = on;
    if (!on)
        result_->hide();
}

void AddToReportButton::retranslate() {
    button_->setText(tr("ADD TO REPORT"));
    button_->setToolTip(tr("Add this result to the report open in the Report Builder (arrow: choose another report)"));
}

void AddToReportButton::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void AddToReportButton::populate_menu() {
    menu_->clear();
    auto* loading = menu_->addAction(tr("Loading reports…"));
    loading->setEnabled(false);
    const quint64 req = ++menu_request_;
    QPointer<AddToReportButton> self(this);
    ReportInbox::instance().list_targets(5, this, [self, req](const QVector<fincept::services::ReportTargetInfo>& targets) {
        if (!self || req != self->menu_request_)
            return;
        QMenu* m = self->menu_;
        m->clear();
        for (const auto& t : targets) {
            const QString text = t.is_current ? tr("Open report: %1").arg(t.title) : t.title;
            QAction* a = m->addAction(atr_escape(text));
            a->setToolTip(t.detail);
            a->setStatusTip(t.detail);
            const ReportTarget target = t.target;
            QObject::connect(a, &QAction::triggered, self, [self, target]() {
                if (self)
                    self->send(target);
            });
            if (t.is_current && targets.size() > 1)
                m->addSeparator();
        }
        m->addSeparator();
        QAction* fresh = m->addAction(tr("New report"));
        QObject::connect(fresh, &QAction::triggered, self, [self]() {
            if (self)
                self->send(ReportTarget::new_blank());
        });
    });
}

void AddToReportButton::send(const ReportTarget& target) {
    if (busy_)
        return;
    if (!provider_) {
        AppendResult r;
        r.error = tr("nothing to add here yet");
        show_result(r);
        return;
    }
    const fincept::report::ReportPayload payload = provider_();
    if (payload.is_empty()) {
        AppendResult r;
        r.error = tr("there is no result to add yet — run it first");
        show_result(r);
        return;
    }
    busy_ = true;
    button_->setEnabled(false);
    ReportInbox::instance().append_blocks(target, payload, options_, this, [this](const AppendResult& r) {
        busy_ = false;
        button_->setEnabled(true);
        show_result(r);
        emit appended(r);
    });
}

void AddToReportButton::show_result(const AppendResult& r) {
    atr_toast(r);
    if (!show_result_)
        return;
    result_->setProperty("rbTone", r.ok ? QString() : QStringLiteral("bad"));
    result_->style()->unpolish(result_);
    result_->style()->polish(result_);
    if (r.ok)
        result_->setText(tr("Added to “%1” · <a href=\"open:%2:%3\" style=\"color:%4;\">Open</a>")
                             .arg(r.report_title.toHtmlEscaped())
                             .arg(r.report_id)
                             .arg(r.block_ids.value(0))
                             .arg(QString::fromLatin1(ui::colors::AMBER())));
    else
        result_->setText(tr("Not added: %1").arg(r.error.toHtmlEscaped()));
    result_->show();
    result_timer_->start();
}

void add_send_to_report_action(QMenu* menu, const QString& source_label, AddToReportButton::PayloadProvider provider) {
    if (!menu)
        return;
    auto send = [source_label, provider](const ReportTarget& target) {
        if (!provider)
            return;
        const fincept::report::ReportPayload payload = provider();
        if (payload.is_empty())
            return;
        fincept::services::AppendOptions o;
        o.source_label = source_label;
        ReportInbox::instance().append_blocks(target, payload, o, nullptr, [](const AppendResult& r) { atr_toast(r); });
    };
    QAction* a = menu->addAction(AddToReportButton::tr("Add to report"));
    QObject::connect(a, &QAction::triggered, menu, [send]() { send(ReportTarget::current()); });
    QAction* b = menu->addAction(AddToReportButton::tr("Add to a new report"));
    QObject::connect(b, &QAction::triggered, menu, [send]() { send(ReportTarget::new_blank()); });
}

} // namespace fincept::ui
