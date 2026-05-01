#include "screens/crypto_center/tabs/RoadmapTab.h"

#include "screens/crypto_center/panels/BuybackBurnPanel.h"
#include "screens/crypto_center/panels/SupplyChartPanel.h"
#include "screens/crypto_center/panels/TreasuryPanel.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString roadmap_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

} // namespace

RoadmapTab::RoadmapTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("roadmapTab"));
    build_ui();
    apply_theme();
}

RoadmapTab::~RoadmapTab() = default;

void RoadmapTab::build_ui() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Scroll area — keeps the treasury panel reachable on short windows.
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("roadmapTabScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("roadmapTabContent"));
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // Each panel is wrapped in its own outer frame to inherit the panel
    // chrome the rest of the Crypto Center uses.
    auto wrap_panel = [content, root](QWidget* inner) {
        auto* host = new QFrame(content);
        host->setObjectName(QStringLiteral("roadmapTabPanelHost"));
        auto* hl = new QVBoxLayout(host);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        hl->addWidget(inner);
        root->addWidget(host);
    };

    buyback_burn_ = new panels::BuybackBurnPanel(content);
    wrap_panel(buyback_burn_);

    supply_chart_ = new panels::SupplyChartPanel(content);
    wrap_panel(supply_chart_);

    treasury_ = new panels::TreasuryPanel(content);
    wrap_panel(treasury_);

    root->addStretch(1);
    scroll->setWidget(content);
}

void RoadmapTab::apply_theme() {
    using namespace ui::colors;
    const QString font = roadmap_font_stack();

    const QString ss = QStringLiteral(
        "QWidget#roadmapTab { background:%1; }"
        "QScrollArea#roadmapTabScroll { background:%1; border:none; }"
        "QWidget#roadmapTabContent { background:%1; }"
        "QFrame#roadmapTabPanelHost { background:%2; border:1px solid %3; }"
    )
        .arg(BG_BASE(),
             BG_SURFACE(),
             BORDER_DIM());

    setStyleSheet(ss);
}

} // namespace fincept::screens
