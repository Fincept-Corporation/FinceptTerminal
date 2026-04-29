#include "screens/crypto_center/tabs/TradeTab.h"

#include "screens/crypto_center/panels/FeeDiscountPanel.h"
#include "screens/crypto_center/panels/SwapPanel.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString trade_font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

} // namespace

TradeTab::TradeTab(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("tradeTab"));
    build_ui();
    apply_theme();
}

TradeTab::~TradeTab() = default;

void TradeTab::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    auto* top_row = new QHBoxLayout;
    top_row->setSpacing(10);

    // SwapPanel — host inside a styled outer frame so it matches the rest of
    // the screens' panel chrome.
    {
        auto* host = new QFrame(this);
        host->setObjectName(QStringLiteral("tradeTabPanelHost"));
        auto* hl = new QVBoxLayout(host);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(0);
        swap_panel_ = new panels::SwapPanel(host);
        hl->addWidget(swap_panel_);
        top_row->addWidget(host, 2);
    }

    // FeeDiscount panel — Phase 2 §2C, replaces the earlier placeholder.
    {
        fee_discount_placeholder_ = new QFrame(this);
        fee_discount_placeholder_->setObjectName(QStringLiteral("tradeTabPanelHost"));
        auto* outer = new QVBoxLayout(fee_discount_placeholder_);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        fee_discount_panel_ = new panels::FeeDiscountPanel(fee_discount_placeholder_);
        outer->addWidget(fee_discount_panel_);
        top_row->addWidget(fee_discount_placeholder_, 1);
    }
    root->addLayout(top_row, 1);

    // Burn placeholder (Stage 2.3)
    {
        burn_placeholder_ = new QFrame(this);
        burn_placeholder_->setObjectName(QStringLiteral("tradeTabPanelHost"));
        auto* outer = new QVBoxLayout(burn_placeholder_);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* head = new QWidget(burn_placeholder_);
        head->setObjectName(QStringLiteral("tradeTabPanelHead"));
        head->setFixedHeight(34);
        auto* head_l = new QHBoxLayout(head);
        head_l->setContentsMargins(12, 0, 12, 0);
        head_l->setSpacing(0);
        auto* title = new QLabel(QStringLiteral("BURN $FNCPT"), head);
        title->setObjectName(QStringLiteral("tradeTabPanelTitle"));
        auto* status = new QLabel(QStringLiteral("STAGE 2.3"), head);
        status->setObjectName(QStringLiteral("tradeTabPanelStatus"));
        head_l->addWidget(title);
        head_l->addStretch();
        head_l->addWidget(status);
        outer->addWidget(head);

        auto* body = new QLabel(
            tr("Permanently destroy $FNCPT through the on-chain fincept_burn program. "
               "The terminal builds the burn transaction, you sign in your wallet, "
               "and the receipt lands here. Lands with the Anchor program in Stage 2.3."),
            burn_placeholder_);
        body->setObjectName(QStringLiteral("tradeTabPlaceholderBody"));
        body->setWordWrap(true);
        body->setContentsMargins(14, 12, 14, 14);
        outer->addWidget(body);
        outer->addStretch(1);
        root->addWidget(burn_placeholder_);
    }

    root->addStretch(1);
}

void TradeTab::apply_theme() {
    using namespace ui::colors;
    const QString font = trade_font_stack();

    const QString ss = QStringLiteral(
        "QWidget#tradeTab { background:%1; }"
        "QFrame#tradeTabPanelHost { background:%2; border:1px solid %3; }"
        "QWidget#tradeTabPanelHead { background:%4; border-bottom:1px solid %3; }"
        "QLabel#tradeTabPanelTitle { color:%5; font-family:%6; font-size:11px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#tradeTabPanelStatus { color:%7; font-family:%6; font-size:10px;"
        "  font-weight:700; letter-spacing:1.2px; background:transparent; }"
        "QLabel#tradeTabPlaceholderBody { color:%8; font-family:%6; font-size:11px;"
        "  background:transparent; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             BG_RAISED(),       // %4
             AMBER(),           // %5
             font,              // %6
             TEXT_TERTIARY(),   // %7
             TEXT_SECONDARY()); // %8

    setStyleSheet(ss);
}

} // namespace fincept::screens
