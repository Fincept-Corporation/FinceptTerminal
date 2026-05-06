// ProfilesSection.cpp — profile selector for multi-account / multi-env workflows.

#include "screens/settings/ProfilesSection.h"

#include "core/config/ProfileManager.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>

namespace fincept::screens {

ProfilesSection::ProfilesSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ProfilesSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 20);
    vl->setSpacing(16);

    auto* title = new QLabel("Profiles");
    title->setStyleSheet(section_title_ss());
    vl->addWidget(title);

    auto* desc = new QLabel("Each profile has its own isolated database, credentials, logs and workspaces.\n"
                            "Launch the terminal with  --profile <name>  to open a specific profile.\n"
                            "Different profiles can run simultaneously — useful for separate trading accounts.");
    desc->setWordWrap(true);
    desc->setStyleSheet(label_ss());
    vl->addWidget(desc);
    vl->addWidget(make_sep());

    auto& pm = ProfileManager::instance();
    auto* active_lbl = new QLabel(QString("Active profile:  <b>%1</b>").arg(pm.active()));
    active_lbl->setStyleSheet(label_ss());
    vl->addWidget(active_lbl);

    auto* list_title = new QLabel("All profiles");
    list_title->setStyleSheet(sub_title_ss());
    vl->addWidget(list_title);

    auto* list_widget = new QWidget(this);
    auto* list_vl = new QVBoxLayout(list_widget);
    list_vl->setContentsMargins(0, 0, 0, 0);
    list_vl->setSpacing(6);

    const QStringList profiles = pm.list_profiles();
    for (const QString& name : profiles) {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);

        auto* name_lbl = new QLabel(name);
        name_lbl->setStyleSheet(label_ss());
        hl->addWidget(name_lbl, 1);

        if (name == pm.active()) {
            auto* badge = new QLabel("ACTIVE");
            badge->setStyleSheet(QString("color:%1;font-weight:700;font-size:10px;background:transparent;")
                                     .arg(ui::colors::AMBER()));
            hl->addWidget(badge);
        } else {
            auto* switch_btn = new QPushButton("Switch");
            switch_btn->setFixedWidth(72);
            switch_btn->setStyleSheet(btn_secondary_ss());
            connect(switch_btn, &QPushButton::clicked, this, [name]() {
                const QString exe = QCoreApplication::applicationFilePath();
                QProcess::startDetached(exe, {"--profile", name});
                QCoreApplication::quit();
            });
            hl->addWidget(switch_btn);
        }
        list_vl->addWidget(row);
    }
    vl->addWidget(list_widget);
    vl->addWidget(make_sep());

    auto* new_title = new QLabel("Create new profile");
    new_title->setStyleSheet(sub_title_ss());
    vl->addWidget(new_title);

    auto* new_row = new QWidget(this);
    auto* new_hl = new QHBoxLayout(new_row);
    new_hl->setContentsMargins(0, 0, 0, 0);
    new_hl->setSpacing(8);

    auto* name_input = new QLineEdit;
    name_input->setPlaceholderText("profile-name  (alphanumeric, - and _ only)");
    name_input->setStyleSheet(input_ss());
    new_hl->addWidget(name_input, 1);

    auto* create_btn = new QPushButton("Create & Switch");
    create_btn->setStyleSheet(btn_primary_ss());
    connect(create_btn, &QPushButton::clicked, this, [name_input]() {
        const QString name = name_input->text().trimmed().toLower();
        if (name.isEmpty()) return;
        ProfileManager::instance().create_profile(name);
        const QString exe = QCoreApplication::applicationFilePath();
        QProcess::startDetached(exe, {"--profile", name});
        QCoreApplication::quit();
    });
    new_hl->addWidget(create_btn);
    vl->addWidget(new_row);

    auto* hint = new QLabel("Creating a profile sets up a fresh data directory. "
                            "The app will restart with the new profile active.");
    hint->setWordWrap(true);
    hint->setStyleSheet(label_ss());
    vl->addWidget(hint);

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll);
}

} // namespace fincept::screens
