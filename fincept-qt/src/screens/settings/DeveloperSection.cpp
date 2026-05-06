// DeveloperSection.cpp — embeds the DataHub Inspector for live pub/sub view.

#include "screens/settings/DeveloperSection.h"

#include "screens/devtools/DataHubInspector.h"
#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"

#include <QLabel>
#include <QString>
#include <QVBoxLayout>

namespace fincept::screens {

DeveloperSection::DeveloperSection(QWidget* parent) : QWidget(parent) {
    using namespace settings_styles;

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

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
