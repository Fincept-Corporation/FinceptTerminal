#include "screens/crypto_center/tabs/MarketsTab.h"

#include "screens/crypto_center/panels/MarketsListPanel.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

MarketsTab::MarketsTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("marketsTab"));
    build_ui();
    apply_theme();
}

MarketsTab::~MarketsTab() = default;

void MarketsTab::build_ui() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("marketsTabScroll"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("marketsTabContent"));
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto* host = new QFrame(content);
    host->setObjectName(QStringLiteral("marketsTabPanelHost"));
    auto* hl = new QVBoxLayout(host);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    list_panel_ = new panels::MarketsListPanel(host);
    hl->addWidget(list_panel_);
    root->addWidget(host);

    root->addStretch(1);
    scroll->setWidget(content);
}

void MarketsTab::apply_theme() {
    using namespace ui::colors;
    const QString ss = QStringLiteral(
        "QWidget#marketsTab { background:%1; }"
        "QScrollArea#marketsTabScroll { background:%1; border:none; }"
        "QWidget#marketsTabContent { background:%1; }"
        "QFrame#marketsTabPanelHost { background:%2; border:1px solid %3; }"
    )
        .arg(BG_BASE(),
             BG_SURFACE(),
             BORDER_DIM());
    setStyleSheet(ss);
}

} // namespace fincept::screens
