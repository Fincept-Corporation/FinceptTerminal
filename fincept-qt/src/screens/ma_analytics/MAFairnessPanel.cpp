// MAFairnessPanel.cpp — the Valuation Range Check module (range check, premium analysis, process quality).
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace fincept::services::ma;

namespace fincept::screens {

using namespace fincept::services::ma;

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 5: VALUATION RANGE CHECK (module key "fairness" kept for saved state)
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_fairness_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Start from the target: fills the premium analysis reference prices and share count (MA-4).
    vl->addWidget(build_company_strip(w));

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();

    // ── Range Check (formerly "Fairness Analysis"; same analysis id, so saved results stay) ──
    // Where the offer sits against each method's valuation range. Not a fairness opinion: that is a formal
    // letter from an adviser after due diligence, and the result says so.
    add_lazy_sub_tab(QT_TR_NOOP("Range Check"), [this](QWidget* fa) {
        auto* fa_vl = new QVBoxLayout(fa);
        fa_vl->setContentsMargins(12, 12, 12, 12);
        fa_vl->setSpacing(8);

        auto* note = new QLabel(tr("Checks the offer against the valuation range of each method you enter (for "
                                   "example from the Valuation module's football field). A valuation range check, "
                                   "not a fairness opinion."),
                                fa);
        note->setWordWrap(true);
        note->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY()).arg(ui::fonts::px(-2)));
        fa_vl->addWidget(note);

        auto* offer = make_double_spin(0, 1e6, 50, 2, "", fa);
        double_inputs_["fo_offer_price"] = offer;
        fa_vl->addWidget(build_input_row(tr("Offer Price") + " ($)", offer, fa));

        struct MaRcMethod {
            const char* key;
            const char* label;
            double low;
            double high;
        };
        // Sample ranges per share (replace with your own); LBO is optional (0 = not used).
        const MaRcMethod methods[] = {{"dcf", QT_TR_NOOP("DCF"), 44, 52},
                                      {"comps", QT_TR_NOOP("Trading Comps"), 41, 49},
                                      {"prec", QT_TR_NOOP("Precedent Txns"), 48, 56},
                                      {"lbo", QT_TR_NOOP("LBO"), 0, 0}};
        for (const MaRcMethod& m : methods) {
            auto* lo = make_double_spin(0, 1e6, m.low, 2, "", fa);
            auto* hi = make_double_spin(0, 1e6, m.high, 2, "", fa);
            lo->setSpecialValueText(tr("not used"));
            hi->setSpecialValueText(tr("not used"));
            double_inputs_[QStringLiteral("fo_%1_low").arg(QLatin1String(m.key))] = lo;
            double_inputs_[QStringLiteral("fo_%1_high").arg(QLatin1String(m.key))] = hi;
            fa_vl->addWidget(build_input_row(tr(m.label) + QStringLiteral(" ") + tr("Low") + " ($)", lo, fa));
            fa_vl->addWidget(build_input_row(tr(m.label) + QStringLiteral(" ") + tr("High") + " ($)", hi, fa));
        }

        auto* fa_run = make_run_button(tr("RUN RANGE CHECK"), fa);
        connect(fa_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Checking the offer against the ranges..."));
            QJsonObject params;
            params["offer_price"] = double_inputs_["fo_offer_price"]->value();
            QJsonArray list;
            const QPair<const char*, const char*> keys[] = {
                {"dcf", "DCF"}, {"comps", "Trading Comps"}, {"prec", "Precedent Txns"}, {"lbo", "LBO Analysis"}};
            for (const auto& k : keys) {
                const double lo = double_inputs_[QStringLiteral("fo_%1_low").arg(QLatin1String(k.first))]->value();
                const double hi = double_inputs_[QStringLiteral("fo_%1_high").arg(QLatin1String(k.first))]->value();
                if (lo <= 0 && hi <= 0)
                    continue; // not used
                QJsonObject m;
                m["method"] = QString::fromLatin1(k.second);
                m["low"] = std::min(lo, hi);
                m["high"] = std::max(lo, hi);
                list.append(m);
            }
            if (list.isEmpty()) {
                display_error(tr("Enter the low and high value of at least one method."));
                return;
            }
            params["methods"] = list;
            MAAnalyticsService::instance().generate_fairness_opinion(params);
        });
        fa_vl->addWidget(fa_run);
        fa_vl->addStretch();
    });

    // ── Premium Analysis ──
    add_lazy_sub_tab(QT_TR_NOOP("Premium Analysis"), [this](QWidget* pa) {
        auto* pa_vl = new QVBoxLayout(pa);
        pa_vl->setContentsMargins(12, 12, 12, 12);
        pa_vl->setSpacing(8);

        auto* pa_offer = make_double_spin(0, 1e6, 50, 2, "", pa);
        double_inputs_["pa_offer"] = pa_offer;
        pa_vl->addWidget(build_input_row(tr("Offer Price") + " ($)", pa_offer, pa));

        auto* pa_1d = make_double_spin(0, 1e6, 42, 2, "", pa);
        double_inputs_["pa_price_1d"] = pa_1d;
        pa_vl->addWidget(build_input_row(tr("1-Day Prior Price") + " ($)", pa_1d, pa));

        auto* pa_4w = make_double_spin(0, 1e6, 38, 2, "", pa);
        double_inputs_["pa_price_4w"] = pa_4w;
        pa_vl->addWidget(build_input_row(tr("4-Week Avg Price") + " ($)", pa_4w, pa));

        auto* pa_52w = make_double_spin(0, 1e6, 35, 2, "", pa);
        double_inputs_["pa_price_52w"] = pa_52w;
        pa_vl->addWidget(build_input_row(tr("52-Week High") + " ($)", pa_52w, pa));

        // Optional: only with the target's real share count is the total offer value shown (no invented default).
        auto* pa_shares = make_double_spin(0, 1e13, 0, 0, "", pa);
        pa_shares->setSpecialValueText(tr("not set"));
        pa_shares->setToolTip(tr("Target shares outstanding - needed for the total offer value"));
        double_inputs_["pa_shares"] = pa_shares;
        pa_vl->addWidget(build_input_row(tr("Target Shares Outstanding"), pa_shares, pa));

        auto* pa_run = make_run_button(tr("ANALYZE PREMIUM"), pa);
        connect(pa_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Analyzing Premium..."));
            QJsonObject params;
            params["offer_price"] = double_inputs_["pa_offer"]->value();
            params["price_1d"] = double_inputs_["pa_price_1d"]->value();
            params["price_4w"] = double_inputs_["pa_price_4w"]->value();
            params["price_52w"] = double_inputs_["pa_price_52w"]->value();
            if (double_inputs_["pa_shares"]->value() > 0)
                params["shares_outstanding"] = double_inputs_["pa_shares"]->value();
            MAAnalyticsService::instance().analyze_premium(params);
        });
        pa_vl->addWidget(pa_run);
        pa_vl->addStretch();
    });

    // ── Process Quality ──
    add_lazy_sub_tab(QT_TR_NOOP("Process Quality"), [this](QWidget* pq) {
        auto* pq_vl = new QVBoxLayout(pq);
        pq_vl->setContentsMargins(12, 12, 12, 12);
        pq_vl->setSpacing(8);

        QStringList pq_factors = {tr("Board Independence"), tr("Special Committee"),   tr("Independent Advisor"),
                                  tr("Market Check"),       tr("Negotiation Process"), tr("Due Diligence"),
                                  tr("Disclosure Quality"), tr("Timing Adequacy")};
        for (int i = 0; i < pq_factors.size(); ++i) {
            auto* spin = make_int_spin(1, 5, 3, pq);
            int_inputs_[QString("pq_%1").arg(i)] = spin;
            pq_vl->addWidget(build_input_row(pq_factors[i] + " (1-5)", spin, pq));
        }

        auto* pq_run = make_run_button(tr("ASSESS PROCESS QUALITY"), pq);
        connect(pq_run, &QPushButton::clicked, this, [this]() {
            status_label_->setText(tr("Assessing Process Quality..."));
            QJsonObject params;
            QJsonArray factors;
            for (int i = 0; i < 8; ++i)
                factors.append(int_inputs_[QString("pq_%1").arg(i)]->value());
            params["factors"] = factors;
            MAAnalyticsService::instance().assess_process_quality(params);
        });
        pq_vl->addWidget(pq_run);
        pq_vl->addStretch();
    });

    vl->addWidget(sub_tabs_);

    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);

    return w;
}

} // namespace fincept::screens
