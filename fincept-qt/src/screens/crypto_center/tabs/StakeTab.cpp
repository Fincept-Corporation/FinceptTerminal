#include "screens/crypto_center/tabs/StakeTab.h"

#include "screens/crypto_center/panels/ActiveLocksPanel.h"
#include "screens/crypto_center/panels/LockPanel.h"
#include "screens/crypto_center/panels/TierPanel.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

StakeTab::StakeTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("stakeTab"));
    build_ui();
    apply_theme();
}

StakeTab::~StakeTab() = default;

void StakeTab::build_ui() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("stakeTabScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("stakeTabContent"));
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto wrap_panel = [content, root](QWidget* inner) {
        auto* host = new QFrame(content);
        host->setObjectName(QStringLiteral("stakeTabPanelHost"));
        auto* hl = new QVBoxLayout(host);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        hl->addWidget(inner);
        root->addWidget(host);
    };

    lock_panel_ = new panels::LockPanel(content);
    wrap_panel(lock_panel_);

    active_locks_ = new panels::ActiveLocksPanel(content);
    wrap_panel(active_locks_);

    tier_panel_ = new panels::TierPanel(content);
    wrap_panel(tier_panel_);

    root->addStretch(1);
    scroll->setWidget(content);
}

void StakeTab::apply_theme() {
    using namespace ui::colors;
    const QString ss = QStringLiteral(
        "QWidget#stakeTab { background:%1; }"
        "QScrollArea#stakeTabScroll { background:%1; border:none; }"
        "QWidget#stakeTabContent { background:%1; }"
        "QFrame#stakeTabPanelHost { background:%2; border:1px solid %3; }"
    )
        .arg(BG_BASE(),
             BG_SURFACE(),
             BORDER_DIM());
    setStyleSheet(ss);
}

} // namespace fincept::screens
