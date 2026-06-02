// src/screens/economics/panels/FinceptMacroPanel.cpp
// Fincept Macro — proprietary macro data source.
// The script fincept_macro.py does not yet exist.
// This panel shows a Coming Soon state with description of planned data.
// When fincept_macro.py is ready, implement on_fetch() and on_result() here.
#include "screens/economics/panels/FinceptMacroPanel.h"

#include "core/logging/Logger.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kFinceptMacroSourceId = "fincept";
static constexpr const char* kFinceptMacroColor = "#d97706"; // amber
} // namespace

FinceptMacroPanel::FinceptMacroPanel(QWidget* parent)
    : EconPanelBase(kFinceptMacroSourceId, kFinceptMacroColor, parent) {
    build_base_ui(this);
    // No service connection — Coming Soon panel
}

void FinceptMacroPanel::activate() {
    show_empty(tr("Fincept Macro — Coming Soon\n\n"
                  "Planned data:\n"
                  "  · Central bank rates (40+ countries)\n"
                  "  · Sovereign debt metrics\n"
                  "  · Fincept proprietary macro indices\n"
                  "  · Global inflation dashboard\n"
                  "  · Emerging market indicators\n\n"
                  "Requires Fincept subscription + API key\n"
                  "Check back in a future release"));
}

void FinceptMacroPanel::build_controls(QHBoxLayout* thl) {
    coming_soon_lbl_ = new QLabel(tr("FINCEPT MACRO — COMING SOON"));
    coming_soon_lbl_->setStyleSheet(ctrl_label_style() + "letter-spacing:1px;");
    thl->addWidget(coming_soon_lbl_);
}

void FinceptMacroPanel::on_fetch() {
    show_empty(tr("Fincept Macro data script is not yet available.\n"
                  "This panel will be enabled in a future release."));
}

void FinceptMacroPanel::on_result(const QString& /*request_id*/, const services::EconomicsResult& /*result*/) {
    // No-op until fincept_macro.py is implemented
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void FinceptMacroPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void FinceptMacroPanel::retranslateUi() {
    if (coming_soon_lbl_)
        coming_soon_lbl_->setText(tr("FINCEPT MACRO — COMING SOON"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
