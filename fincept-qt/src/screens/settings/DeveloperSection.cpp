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
    auto* agentic_title = new QLabel("Agentic Mode (Experimental)");
    agentic_title->setStyleSheet(section_title_ss());
    vl->addWidget(agentic_title);

    auto* agentic_desc = new QLabel(
        "Enable durable, long-running autonomous tasks. When on, AGENT STUDIO shows "
        "an extra AGENTIC tab listing in-flight tasks (plan, step log, pause/resume/"
        "cancel) and the chat panel offers a \"Run as background task\" checkbox. "
        "All state is checkpointed to SQLite so tasks survive process restarts. "
        "Leave off for standard chatbot behavior.");
    agentic_desc->setWordWrap(true);
    agentic_desc->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(agentic_desc);

    auto* agentic_toggle = new QCheckBox("Enable Agentic Mode");
    agentic_toggle->setStyleSheet(QString("color:%1;font-size:12px;padding:4px 0;")
                                      .arg(ui::colors::TEXT_PRIMARY()));
    {
        auto r = SettingsRepository::instance().get(
            QStringLiteral("agentic_mode_enabled"), QStringLiteral("false"));
        agentic_toggle->setChecked(r.is_ok() && r.value() == QStringLiteral("true"));
    }
    connect(agentic_toggle, &QCheckBox::toggled, this, [](bool checked) {
        const QString v = checked ? QStringLiteral("true") : QStringLiteral("false");
        SettingsRepository::instance().set(
            QStringLiteral("agentic_mode_enabled"), v, QStringLiteral("features"));
        QVariantMap payload;
        payload["enabled"] = checked;
        EventBus::instance().publish(QStringLiteral("settings.agentic_mode_changed"), payload);
    });
    vl->addWidget(agentic_toggle);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_DIM()));
    sep->setFixedHeight(1);
    vl->addWidget(sep);

    // ── DataHub Inspector ────────────────────────────────────────────────────
    auto* title = new QLabel("DataHub Inspector");
    title->setStyleSheet(section_title_ss());
    vl->addWidget(title);

    auto* desc = new QLabel(
        "Live view over the in-process pub/sub layer. Shows every active topic, its "
        "subscriber count, total publishes, and time since last publish. Refreshes "
        "once per second while this tab is visible.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(desc);

    vl->addWidget(new devtools::DataHubInspector(this), 1);
}

} // namespace fincept::screens
