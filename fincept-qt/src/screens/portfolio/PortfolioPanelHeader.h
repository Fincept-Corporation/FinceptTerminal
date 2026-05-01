// src/screens/portfolio/PortfolioPanelHeader.h
#pragma once

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

/// Unified panel header used by every landing-view panel (HOLDINGS,
/// PERFORMANCE, SECTORS, POSITIONS, TRANSACTION HISTORY).
///
/// Visual contract:
///   - 30px tall, BG_SURFACE background, 1px BORDER_DIM bottom rule
///   - Left:  3×14 amber tick + 11px / 700 / 1.2px tracking title (TEXT_PRIMARY)
///   - Right: caller-supplied controls slot (returned via `controls_slot`)
///
/// Returns the header widget. Caller adds it to a vertical layout, then puts
/// panel-specific controls (period buttons, mode toggles, count badges, filter
/// inputs, ⓢ counts, hide chevrons …) into `controls_slot` using its layout.
struct PanelHeaderResult {
    QWidget* header = nullptr;     ///< Add to layout
    QWidget* controls_slot = nullptr; ///< Right-side controls container; has a QHBoxLayout already
    QLabel* title_label = nullptr; ///< In case you need to mutate it (e.g. theme refresh)
};

inline PanelHeaderResult make_panel_header(const QString& title_text, QWidget* parent = nullptr) {
    PanelHeaderResult r;

    r.header = new QWidget(parent);
    r.header->setFixedHeight(30);
    r.header->setObjectName("pfPanelHeader");
    r.header->setStyleSheet(QString("#pfPanelHeader { background:%1; border-bottom:1px solid %2; }")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    auto* hl = new QHBoxLayout(r.header);
    hl->setContentsMargins(12, 0, 10, 0);
    hl->setSpacing(8);

    // ▌ Amber tick — 3×14 vertical bar, square corners (DESIGN_SYSTEM rule 9.1).
    auto* tick = new QLabel(r.header);
    tick->setFixedSize(3, 14);
    tick->setStyleSheet(QString("background:%1;").arg(ui::colors::AMBER()));
    hl->addWidget(tick);

    // Title — 11px / 700 / 1.2px tracking.
    r.title_label = new QLabel(title_text, r.header);
    r.title_label->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700;"
                                         "  letter-spacing:1.2px; background:transparent;")
                                     .arg(ui::colors::TEXT_PRIMARY()));
    hl->addWidget(r.title_label);

    // Right-side controls slot. Layout already attached so caller can just
    // addWidget into it.
    r.controls_slot = new QWidget(r.header);
    r.controls_slot->setStyleSheet("background:transparent;");
    auto* slot_hl = new QHBoxLayout(r.controls_slot);
    slot_hl->setContentsMargins(0, 0, 0, 0);
    slot_hl->setSpacing(6);

    hl->addStretch(1);
    hl->addWidget(r.controls_slot);

    return r;
}

} // namespace fincept::screens
