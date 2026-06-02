// DeveloperSection.cpp — DataHub Inspector + Agentic Mode toggle.

#include "screens/settings/DeveloperSection.h"

#include "core/events/EventBus.h"
#include "screens/devtools/DataHubInspector.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QString>
#include <QVariantMap>
#include <QVBoxLayout>

namespace fincept::screens {

DeveloperSection::DeveloperSection(QWidget* parent) : QWidget(parent) {
    using namespace settings_styles;

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // ── Agentic Mode toggle ──────────────────────────────────────────────────
    agentic_title_ = new QLabel(tr("Agentic Mode (Experimental)"));
    agentic_title_->setStyleSheet(section_title_ss());
    vl->addWidget(agentic_title_);

    agentic_desc_ = new QLabel(
        tr("Enable durable, long-running autonomous tasks. When on, AGENT STUDIO shows "
           "an extra AGENTIC tab listing in-flight tasks (plan, step log, pause/resume/"
           "cancel) and the chat panel offers a \"Run as background task\" checkbox. "
           "All state is checkpointed to SQLite so tasks survive process restarts. "
           "Leave off for standard chatbot behavior."));
    agentic_desc_->setWordWrap(true);
    agentic_desc_->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(agentic_desc_);

    agentic_toggle_ = new QCheckBox(tr("Enable Agentic Mode"));
    agentic_toggle_->setStyleSheet(QString("color:%1;font-size:12px;padding:4px 0;")
                                       .arg(ui::colors::TEXT_PRIMARY()));
    {
        auto r = SettingsRepository::instance().get(
            QStringLiteral("agentic_mode_enabled"), QStringLiteral("false"));
        agentic_toggle_->setChecked(r.is_ok() && r.value() == QStringLiteral("true"));
    }
    connect(agentic_toggle_, &QCheckBox::toggled, this, [](bool checked) {
        const QString v = checked ? QStringLiteral("true") : QStringLiteral("false");
        SettingsRepository::instance().set(
            QStringLiteral("agentic_mode_enabled"), v, QStringLiteral("features"));
        QVariantMap payload;
        payload["enabled"] = checked;
        EventBus::instance().publish(QStringLiteral("settings.agentic_mode_changed"), payload);
    });
    vl->addWidget(agentic_toggle_);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_DIM()));
    sep->setFixedHeight(1);
    vl->addWidget(sep);

    // ── DataHub Inspector ────────────────────────────────────────────────────
    inspector_title_ = new QLabel(tr("DataHub Inspector"));
    inspector_title_->setStyleSheet(section_title_ss());
    vl->addWidget(inspector_title_);

    inspector_desc_ = new QLabel(
        tr("Live view over the in-process pub/sub layer. Shows every active topic, its "
           "subscriber count, total publishes, and time since last publish. Refreshes "
           "once per second while this tab is visible."));
    inspector_desc_->setWordWrap(true);
    inspector_desc_->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(inspector_desc_);

    vl->addWidget(new devtools::DataHubInspector(this), 1);
}

void DeveloperSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void DeveloperSection::retranslateUi() {
    if (agentic_title_) agentic_title_->setText(tr("Agentic Mode (Experimental)"));
    if (agentic_desc_)
        agentic_desc_->setText(
            tr("Enable durable, long-running autonomous tasks. When on, AGENT STUDIO shows "
               "an extra AGENTIC tab listing in-flight tasks (plan, step log, pause/resume/"
               "cancel) and the chat panel offers a \"Run as background task\" checkbox. "
               "All state is checkpointed to SQLite so tasks survive process restarts. "
               "Leave off for standard chatbot behavior."));
    if (agentic_toggle_) agentic_toggle_->setText(tr("Enable Agentic Mode"));
    if (inspector_title_) inspector_title_->setText(tr("DataHub Inspector"));
    if (inspector_desc_)
        inspector_desc_->setText(
            tr("Live view over the in-process pub/sub layer. Shows every active topic, its "
               "subscriber count, total publishes, and time since last publish. Refreshes "
               "once per second while this tab is visible."));
}

} // namespace fincept::screens
