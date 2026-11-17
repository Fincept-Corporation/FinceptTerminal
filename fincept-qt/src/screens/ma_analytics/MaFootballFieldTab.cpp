// MaFootballFieldTab.cpp — MA-6: the Valuation module's Football Field sub-tab. Built locally (no script) from the
// user's own latest runs of this module — DCF, Trading Comps, Precedent Txns, LBO Sensitivity — plus the loaded
// company's 52-week range and an optional range typed here, on one axis with the current price and the offer.
// Methods not run yet are listed; a range that cannot be put in the chosen basis is left out with the reason.
#include "screens/ma_analytics/MAModulePanel.h"

#include "core/logging/Logger.h"
#include "screens/ma_analytics/MaFootballField.h"
#include "screens/ma_analytics/MaFootballFieldMath.h"
#include "screens/ma_analytics/MaResultFormat.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/ResultView.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

double ma_fft_input(const QHash<QString, QDoubleSpinBox*>& inputs, const char* name) {
    const QDoubleSpinBox* w = inputs.value(QString::fromLatin1(name), nullptr);
    return (w && w->value() > 0) ? w->value() : std::nan("");
}

QJsonObject ma_fft_row(const ma_ff::Range& r, const QString& as_of) {
    QJsonObject o{{QStringLiteral("method"), r.label}, {QStringLiteral("low"), r.low}, {QStringLiteral("high"), r.high}};
    if (std::isfinite(r.mid))
        o.insert(QStringLiteral("mid"), r.mid);
    if (!r.note.isEmpty())
        o.insert(QStringLiteral("basis_note"), r.note);
    if (!as_of.isEmpty())
        o.insert(QStringLiteral("run"), as_of.left(16).replace(QLatin1Char('T'), QLatin1Char(' ')));
    return o;
}

} // namespace

void MAModulePanel::build_football_field_tab(QWidget* page) {
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(8);

    auto* hint = new QLabel(tr("Built from your latest DCF, Trading Comps, Precedent Txns and LBO Sensitivity runs and "
                               "the loaded company's 52-week range. Per-share values use the shares, debt and cash "
                               "of the DCF tab."),
                            page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; font-size:%2px;").arg(ui::colors::TEXT_SECONDARY()).arg(ui::fonts::px(-2)));
    vl->addWidget(hint);

    auto* basis = new QComboBox(page);
    basis->addItem(tr("Value per share"), QStringLiteral("per_share"));
    basis->addItem(tr("Enterprise value"), QStringLiteral("enterprise_value"));
    basis->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:4px 6px; }")
                             .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
    combo_inputs_["ff_basis"] = basis;
    vl->addWidget(build_input_row(tr("Show values as"), basis, page));

    auto* price = make_double_spin(0, 1e7, 0, 2, "", page);
    price->setSpecialValueText(tr("not set"));
    price->setToolTip(tr("Current share price (filled when a company is loaded)"));
    double_inputs_["ff_price"] = price;
    vl->addWidget(build_input_row(tr("Current Share Price") + " ($)", price, page));

    auto* offer = make_double_spin(0, 1e7, 0, 2, "", page);
    offer->setSpecialValueText(tr("not set"));
    double_inputs_["ff_offer"] = offer;
    vl->addWidget(build_input_row(tr("Offer per Share") + " ($)", offer, page));

    auto* irr_lo = make_double_spin(0, 100, 20, 1, "%", page);
    double_inputs_["ff_irr_low"] = irr_lo;
    vl->addWidget(build_input_row(tr("LBO target IRR from"), irr_lo, page));
    auto* irr_hi = make_double_spin(0, 100, 25, 1, "%", page);
    double_inputs_["ff_irr_high"] = irr_hi;
    vl->addWidget(build_input_row(tr("LBO target IRR to"), irr_hi, page));

    auto* man_lo = make_double_spin(0, 1e15, 0, 2, "", page);
    man_lo->setSpecialValueText(tr("not set"));
    man_lo->setToolTip(tr("An extra range of your own (analyst targets, a bid), in the basis chosen above"));
    double_inputs_["ff_manual_low"] = man_lo;
    vl->addWidget(build_input_row(tr("Other Range Low") + " ($)", man_lo, page));
    auto* man_hi = make_double_spin(0, 1e15, 0, 2, "", page);
    man_hi->setSpecialValueText(tr("not set"));
    double_inputs_["ff_manual_high"] = man_hi;
    vl->addWidget(build_input_row(tr("Other Range High") + " ($)", man_hi, page));

    auto* run = make_run_button(tr("BUILD FOOTBALL FIELD"), page);
    connect(run, &QPushButton::clicked, this, [this]() {
        const quint64 id = run_ids_.value(QStringLiteral("football_field"));
        if (!pane_ || id == 0)
            return;
        const QJsonObject p = football_field_payload();
        pane_->deliver(id, p);
        status_label_->setText(p.contains(QStringLiteral("needs_data")) ? tr("Nothing to draw yet") : tr("Ready"));
    });
    vl->addWidget(run);
    vl->addStretch();
}

QJsonObject MAModulePanel::football_field_payload() const {
    const QComboBox* basis_box = combo_inputs_.value(QStringLiteral("ff_basis"), nullptr);
    const bool per_share = !basis_box || basis_box->currentData().toString() == QLatin1String("per_share");
    const ma_ff::Basis basis = per_share ? ma_ff::Basis::PerShare : ma_ff::Basis::EnterpriseValue;
    ma_ff::Bridge bridge;
    bridge.shares = ma_fft_input(double_inputs_, "dcf_shares");
    const QDoubleSpinBox* debt = double_inputs_.value(QStringLiteral("dcf_debt"), nullptr);
    const QDoubleSpinBox* cash = double_inputs_.value(QStringLiteral("dcf_cash"), nullptr);
    bridge.debt = debt ? debt->value() : std::nan("");
    bridge.cash = cash ? cash->value() : std::nan("");

    QJsonArray methods, not_run, left_out;
    QVector<ma_ff::Range> rows;
    auto take = [&](ma_ff::Range r, const QString& as_of) {
        if (!r.valid()) {
            left_out.append(QJsonObject{{QStringLiteral("method"), r.label}, {QStringLiteral("reason"), r.note}});
            return;
        }
        if (!ma_ff::to_basis(&r, basis, bridge)) {
            left_out.append(QJsonObject{{QStringLiteral("method"), r.label}, {QStringLiteral("reason"), r.note}});
            return;
        }
        rows << r;
        methods.append(ma_fft_row(r, as_of));
    };

    const QJsonObject dcf = last_run(QStringLiteral("dcf"));
    if (dcf.isEmpty())
        not_run.append(tr("DCF"));
    else
        take(ma_ff::dcf_range(dcf, basis), dcf.value(QStringLiteral("_as_of")).toString());

    const QJsonObject comps = last_run(QStringLiteral("trading_comps"));
    if (comps.isEmpty())
        not_run.append(tr("Trading Comps"));
    else
        for (const ma_ff::Range& r : ma_ff::multiple_ranges(comps, QStringLiteral("comps"), tr("Trading comps")))
            take(r, comps.value(QStringLiteral("_as_of")).toString());

    const QJsonObject prec = last_run(QStringLiteral("precedent_txns"));
    if (prec.isEmpty())
        not_run.append(tr("Precedent Txns"));
    else
        for (const ma_ff::Range& r : ma_ff::multiple_ranges(prec, QStringLiteral("precedents"), tr("Precedents")))
            take(r, prec.value(QStringLiteral("_as_of")).toString());

    const QJsonObject lbo = last_run(QStringLiteral("lbo_sensitivity"));
    if (lbo.isEmpty()) {
        not_run.append(tr("LBO Sensitivity"));
    } else {
        const QDoubleSpinBox* irr_lo = double_inputs_.value(QStringLiteral("ff_irr_low"), nullptr);
        const QDoubleSpinBox* irr_hi = double_inputs_.value(QStringLiteral("ff_irr_high"), nullptr);
        const double ebitda = ma_fft_input(double_inputs_, "lbof_ebitda");
        take(ma_ff::lbo_range(lbo, ebitda, irr_lo ? irr_lo->value() : 20.0, irr_hi ? irr_hi->value() : 25.0),
             lbo.value(QStringLiteral("_as_of")).toString());
    }

    auto snap = snapshots_.value(QStringLiteral("target"));
    if (!snap.ok) {
        const QVariant cached = fincept::CacheManager::instance().get(
            QStringLiteral("ma_analytics:snapshot:%1:target").arg(module_.key));
        if (!cached.isNull())
            snap = services::CompanySnapshot::from_json(QJsonDocument::fromJson(cached.toString().toUtf8()).object());
    }
    if (snap.ok && std::isfinite(snap.week52_low) && std::isfinite(snap.week52_high)) {
        ma_ff::Range r;
        r.key = QStringLiteral("week52");
        r.label = tr("52-week trading range");
        r.low = snap.week52_low;
        r.high = snap.week52_high;
        r.basis_ev = false;
        r.note = snap.caption();
        take(r, snap.as_of.toString(Qt::ISODate));
    }

    const double man_lo = ma_fft_input(double_inputs_, "ff_manual_low");
    const double man_hi = ma_fft_input(double_inputs_, "ff_manual_high");
    if (std::isfinite(man_lo) && std::isfinite(man_hi) && man_hi >= man_lo) {
        ma_ff::Range r;
        r.key = QStringLiteral("manual");
        r.label = tr("Other (entered)");
        r.low = man_lo;
        r.high = man_hi;
        r.basis_ev = !per_share;
        r.note = tr("typed on this tab");
        take(r, QString());
    }

    QJsonObject out;
    const double price = ma_fft_input(double_inputs_, "ff_price");
    const double offer = ma_fft_input(double_inputs_, "ff_offer");
    const double price_b = per_share ? price : ma_ff::per_share_to_ev(price, bridge);
    const double offer_b = per_share ? offer : ma_ff::per_share_to_ev(offer, bridge);
    if (rows.isEmpty()) {
        QJsonArray steps;
        steps.append(tr("Run DCF, Trading Comps, Precedent Txns or LBO Sensitivity on this module (each opens with "
                        "your last inputs), then come back here."));
        steps.append(tr("Or load a company above for its 52-week range, or type a range of your own here."));
        out.insert(QStringLiteral("needs_data"),
                   QJsonObject{{QStringLiteral("title"), tr("Nothing to draw yet")},
                               {QStringLiteral("message"),
                                tr("The football field is built from your own valuation runs. Not run yet: %1.")
                                    .arg(not_run.isEmpty() ? tr("none") : [&]() {
                                        QStringList l;
                                        for (const QJsonValue& v : not_run)
                                            l << v.toString();
                                        return l.join(QStringLiteral(", "));
                                    }())},
                               {QStringLiteral("steps"), steps}});
        return out;
    }
    double lo = 0, hi = 0;
    ma_ff::axis(rows, {price_b, offer_b}, 0.06, &lo, &hi);
    out.insert(QStringLiteral("basis"), per_share ? QStringLiteral("per_share") : QStringLiteral("enterprise_value"));
    out.insert(QStringLiteral("methods"), methods);
    if (!not_run.isEmpty())
        out.insert(QStringLiteral("not_run"), not_run);
    if (!left_out.isEmpty())
        out.insert(QStringLiteral("left_out"), left_out);
    if (std::isfinite(price_b))
        out.insert(QStringLiteral("current_price"), price_b);
    if (std::isfinite(offer_b))
        out.insert(QStringLiteral("offer_price"), offer_b);
    out.insert(QStringLiteral("markers"), QJsonObject{{QStringLiteral("axis_low"), lo}, {QStringLiteral("axis_high"), hi}});
    return out;
}

void MAModulePanel::add_football_field_widget(const QJsonObject& payload) {
    if (!result_view_)
        return;
    QVector<ma_ff::Range> rows;
    for (const QJsonValue& v : payload.value(QStringLiteral("methods")).toArray()) {
        const QJsonObject o = v.toObject();
        ma_ff::Range r;
        r.label = o.value(QStringLiteral("method")).toString();
        r.low = ma_fmt::ma_num(o, "low");
        r.high = ma_fmt::ma_num(o, "high");
        r.mid = ma_fmt::ma_num(o, "mid");
        r.note = o.value(QStringLiteral("basis_note")).toString();
        if (r.valid())
            rows << r;
    }
    if (rows.isEmpty())
        return;
    const QJsonObject axis = payload.value(QStringLiteral("markers")).toObject();
    double lo = ma_fmt::ma_num(axis, "axis_low"), hi = ma_fmt::ma_num(axis, "axis_high");
    if (!std::isfinite(lo) || !std::isfinite(hi))
        ma_ff::axis(rows, {}, 0.06, &lo, &hi);
    QVector<ui::RangeMarker> markers;
    const double price = ma_fmt::ma_num(payload, "current_price");
    if (std::isfinite(price)) {
        ui::RangeMarker m;
        m.value = price;
        m.label = tr("Price");
        m.role = ui::RangeMarker::Role::Current;
        markers << m;
    }
    const double offer = ma_fmt::ma_num(payload, "offer_price");
    if (std::isfinite(offer)) {
        ui::RangeMarker m;
        m.value = offer;
        m.label = tr("Offer");
        m.role = ui::RangeMarker::Role::Target;
        markers << m;
    }
    const QString intrinsic;
    auto* field = new MaFootballField;
    field->set_rows(rows, lo, hi, markers, [intrinsic](double v) { return ma_fmt::ma_money_text(v, intrinsic); });
    const bool per_share = payload.value(QStringLiteral("basis")).toString() == QLatin1String("per_share");
    shown_charts_.append({tr("Football field"), field});
    result_view_->add_custom_widget(ui::ResultView::make_card(
        tr("Football field"), field, per_share ? tr("value per share by method") : tr("enterprise value by method")));
}

} // namespace fincept::screens
