// GeneralSection.cpp — application-wide behaviour toggles.

#include "screens/settings/GeneralSection.h"

#include "core/logging/Logger.h"
#include "screens/settings/SettingsRowHelpers.h"
#include "screens/settings/SettingsStyles.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"

#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr const char* kKeyOnClose       = "general.on_last_window_close";
constexpr const char* kDefaultOnClose   = "quit";
} // namespace

GeneralSection::GeneralSection(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void GeneralSection::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    reload();
}

void GeneralSection::build_ui() {
    using namespace settings_styles;
    using namespace settings_helpers;

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QString("QScrollArea { border: none; background: transparent; }"
                                  "QScrollBar:vertical { background: %1; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_MED()));

    auto* page = new QWidget(this);
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(8);

    // ── WINDOW BEHAVIOUR ──────────────────────────────────────────────────────
    auto* t = new QLabel("WINDOW BEHAVIOUR");
    t->setStyleSheet(section_title_ss());
    vl->addWidget(t);
    vl->addWidget(make_sep());
    vl->addSpacing(8);

    on_close_combo_ = new QComboBox;
    on_close_combo_->addItem("Quit application",   QStringLiteral("quit"));
    on_close_combo_->addItem("Show Launchpad",     QStringLiteral("show_launchpad"));
    on_close_combo_->setStyleSheet(combo_ss());
    vl->addWidget(make_row(
        "On last window close",
        on_close_combo_,
        "Default is Quit. Choose 'Show Launchpad' if you want a small portal "
        "window to stay open after closing your last terminal window."));

    connect(on_close_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                const QString val = on_close_combo_->itemData(idx).toString();
                SettingsRepository::instance().set(
                    QString::fromLatin1(kKeyOnClose), val, QStringLiteral("general"));
                LOG_INFO("Settings", QString("on_last_window_close → %1").arg(val));
            });

    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll);
}

void GeneralSection::reload() {
    if (!on_close_combo_) return;
    const auto r = SettingsRepository::instance().get(
        QString::fromLatin1(kKeyOnClose), QString::fromLatin1(kDefaultOnClose));
    const QString cur = r.is_ok() ? r.value() : QString::fromLatin1(kDefaultOnClose);
    const int idx = on_close_combo_->findData(cur);
    QSignalBlocker block(on_close_combo_);
    on_close_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

} // namespace fincept::screens
