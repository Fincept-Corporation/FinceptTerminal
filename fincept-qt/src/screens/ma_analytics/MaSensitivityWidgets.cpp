// MaSensitivityWidgets.cpp — MA-5 / MA-7: the sensitivity grids (ui::SensitivityTable) under the DCF, LBO
// sensitivity and accretion / dilution results, and the merger model's EPS bridge. The numbers come in the same
// script reply as the result above them (one run, one model), so the base cell of every grid equals the headline.
#include "screens/ma_analytics/MAModulePanel.h"

#include "screens/ma_analytics/MaResultFormat.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/SensitivityTable.h"

#include <QJsonArray>

#include <cmath>

namespace fincept::screens {

namespace {

QVector<double> ma_sw_doubles(const QJsonArray& a) {
    QVector<double> out;
    out.reserve(a.size());
    for (const QJsonValue& v : a)
        out << (v.isDouble() ? v.toDouble() : std::nan(""));
    return out;
}

QVector<QVector<double>> ma_sw_matrix(const QJsonArray& m) {
    QVector<QVector<double>> out;
    out.reserve(m.size());
    for (const QJsonValue& row : m)
        out << ma_sw_doubles(row.toArray());
    return out;
}

QString ma_sw_pct(double v, int decimals = 1, bool sign = false) {
    return fincept::ui::formatting::pct(v, decimals, sign);
}

} // namespace

void MAModulePanel::add_sensitivity_widgets(const QString& context, const QJsonObject& payload) {
    if (!result_view_)
        return;
    const QString intrinsic = module_.id == services::ma::ModuleId::Deals ? QStringLiteral("USD") : QString();
    auto money = [intrinsic](double v) { return ma_fmt::ma_money_text(v, intrinsic); };
    auto add = [this](ui::SensitivityTable* t) {
        shown_grids_.append(t);
        result_view_->add_custom_widget(t);
    };

    if (context == QLatin1String("dcf")) {
        const QJsonObject s = payload.value(QStringLiteral("sensitivity")).toObject();
        const QVector<double> w = ma_sw_doubles(s.value(QStringLiteral("wacc_values")).toArray());
        const QVector<double> g = ma_sw_doubles(s.value(QStringLiteral("terminal_growth_values")).toArray());
        if (w.isEmpty() || g.isEmpty())
            return;
        auto* t = new ui::SensitivityTable;
        t->set_title(tr("DCF sensitivity"), tr("WACC x terminal growth"));
        t->set_export_name(QStringLiteral("dcf_sensitivity"));
        t->set_axes(tr("WACC"), w, tr("Terminal growth"), g, [](double v) { return ma_sw_pct(v, 2); },
                    [](double v) { return ma_sw_pct(v, 2); });
        t->add_metric(tr("Value per share"), ma_sw_matrix(s.value(QStringLiteral("price_per_share")).toArray()), money);
        t->add_metric(tr("Enterprise value"), ma_sw_matrix(s.value(QStringLiteral("enterprise_value")).toArray()), money);
        t->add_metric(tr("Equity value"), ma_sw_matrix(s.value(QStringLiteral("equity_value")).toArray()), money);
        t->set_base_cell(s.value(QStringLiteral("base_row")).toInt(-1), s.value(QStringLiteral("base_col")).toInt(-1));
        t->set_note(tr("Each cell re-runs this DCF with that WACC and terminal growth. An empty cell: WACC at or below "
                       "the growth rate, or growth outside the model's 0-5%."));
        add(t);
        return;
    }

    if (context == QLatin1String("lbo_sensitivity")) {
        const QVector<double> entry = ma_sw_doubles(payload.value(QStringLiteral("entry_multiple_range")).toArray());
        const QVector<double> exit = ma_sw_doubles(payload.value(QStringLiteral("exit_multiple_range")).toArray());
        if (entry.isEmpty() || exit.isEmpty())
            return;
        auto mult = [](double v) { return fincept::ui::formatting::number(v, 1) + QLatin1Char('x'); };
        auto* t = new ui::SensitivityTable;
        t->set_title(tr("LBO returns sensitivity"), tr("entry x exit multiple"));
        t->set_export_name(QStringLiteral("lbo_sensitivity"));
        t->set_axes(tr("Entry multiple"), entry, tr("Exit multiple"), exit, mult, mult);
        t->add_metric(tr("IRR"), ma_sw_matrix(payload.value(QStringLiteral("irr_pct_matrix")).toArray()),
                      [](double v) { return ma_sw_pct(v, 1); });
        t->add_metric(tr("MOIC"), ma_sw_matrix(payload.value(QStringLiteral("moic_matrix")).toArray()),
                      [](double v) { return fincept::ui::formatting::number(v, 2) + QLatin1Char('x'); });
        t->set_base_cell(static_cast<int>((entry.size() - 1) / 2), static_cast<int>((exit.size() - 1) / 2));
        t->set_note(tr("Rows: the multiple paid at entry; columns: the multiple at exit. Base case = the LBO Model "
                       "tab's operating case and debt."));
        add(t);
        return;
    }

    if (context == QLatin1String("accretion_dilution")) {
        const QJsonObject s = payload.value(QStringLiteral("sensitivity")).toObject();
        const QVector<double> rows = ma_sw_doubles(s.value(QStringLiteral("row_values")).toArray());
        const QVector<double> syn = ma_sw_doubles(s.value(QStringLiteral("synergy_values")).toArray());
        const QVector<double> cash = ma_sw_doubles(s.value(QStringLiteral("cash_pct_values")).toArray());
        if (rows.isEmpty())
            return;
        const bool by_premium = s.value(QStringLiteral("row_axis")).toString() == QLatin1String("premium_pct");
        const QString row_title = by_premium ? tr("Premium") : tr("Purchase price");
        auto row_fmt = [by_premium, money](double v) { return by_premium ? ma_sw_pct(v, 0) : money(v); };
        const int base_row = s.value(QStringLiteral("base_row")).toInt(-1);
        const double standalone =
            payload.value(QStringLiteral("accretion_dilution")).toObject().value(QStringLiteral("standalone_eps")).toDouble(
                std::nan(""));

        if (!syn.isEmpty()) {
            auto* t = new ui::SensitivityTable;
            t->set_title(tr("Year-1 accretion / dilution"), row_title + tr(" x run-rate synergies"));
            t->set_export_name(QStringLiteral("accretion_premium_synergies"));
            t->set_axes(row_title, rows, tr("Run-rate synergies (pre-tax)"), syn, row_fmt, money);
            ui::SensitivityMetric acc;
            acc.name = tr("Accretion %");
            acc.values = ma_sw_matrix(s.value(QStringLiteral("accretion_premium_synergies")).toArray());
            acc.format = [](double v) { return ma_sw_pct(v, 1, true); };
            acc.midpoint = 0.0; // accretive cells read as good, dilutive as bad, whatever the base case
            t->add_metric(acc);
            ui::SensitivityMetric eps;
            eps.name = tr("Pro forma EPS");
            eps.values = ma_sw_matrix(s.value(QStringLiteral("eps_premium_synergies")).toArray());
            eps.format = [](double v) { return fincept::ui::formatting::number(v, 2); };
            eps.midpoint = standalone; // centred on the acquirer's standalone EPS
            t->add_metric(eps);
            t->set_base_cell(base_row, s.value(QStringLiteral("base_col")).toInt(-1));
            add(t);
        }
        if (!cash.isEmpty()) {
            auto* t = new ui::SensitivityTable;
            t->set_title(tr("Year-1 accretion / dilution"), row_title + tr(" x cash share of the price"));
            t->set_export_name(QStringLiteral("accretion_premium_cash"));
            t->set_axes(row_title, rows, tr("Cash share"), cash, row_fmt, [](double v) { return ma_sw_pct(v, 0); });
            ui::SensitivityMetric acc;
            acc.name = tr("Accretion %");
            acc.values = ma_sw_matrix(s.value(QStringLiteral("accretion_premium_cash")).toArray());
            acc.format = [](double v) { return ma_sw_pct(v, 1, true); };
            acc.midpoint = 0.0;
            t->add_metric(acc);
            t->set_base_cell(base_row, s.value(QStringLiteral("base_col_cash")).toInt(-1));
            t->set_note(tr("The cash part is paid from the acquirer cash entered, the rest with new debt at the cost of "
                           "debt; the stock part is issued at the acquirer share price."));
            add(t);
        }
    }
}

void MAModulePanel::add_merger_widgets(const QJsonObject& payload) {
    if (!result_view_)
        return;
    // EPS bridge: standalone EPS -> each effect -> pro forma EPS (the steps add up exactly).
    const QJsonArray steps = payload.value(QStringLiteral("eps_bridge")).toArray();
    if (steps.size() >= 3) {
        QStringList cats;
        QVector<double> vals;
        for (int i = 0; i + 1 < steps.size(); ++i) {
            const QJsonObject o = steps.at(i).toObject();
            cats << o.value(QStringLiteral("step")).toString();
            vals << o.value(QStringLiteral("value")).toDouble();
        }
        auto* chart = new ui::BarChart;
        chart->set_categories(cats);
        ui::BarSeries sr;
        sr.name = tr("EPS");
        sr.values = vals;
        chart->set_series({sr});
        chart->set_mode(ui::BarChart::Mode::Waterfall);
        chart->set_waterfall_total(true, tr("Pro forma EPS"));
        chart->set_show_values(true);
        chart->set_value_formatter([](double v) { return fincept::ui::formatting::number(v, 3); });
        chart->setMinimumHeight(260);
        shown_charts_.append({tr("EPS bridge (year 1)"), chart});
        result_view_->add_custom_widget(ui::ResultView::make_card(
            tr("EPS bridge (year 1)"), chart, tr("per acquirer share; the last step is the effect of the new shares")));
    }
    // Accretion by year (synergy phase-in, integration costs and earnings growth play out).
    const QJsonArray years = payload.value(QStringLiteral("years")).toArray();
    if (years.size() >= 2) {
        QStringList cats;
        QVector<double> acc;
        bool any = false;
        for (const QJsonValue& y : years) {
            const QJsonObject o = y.toObject();
            cats << tr("Year %1").arg(o.value(QStringLiteral("year")).toInt());
            const QJsonValue a = o.value(QStringLiteral("accretion_dilution_pct"));
            acc << (a.isDouble() ? a.toDouble() : std::nan(""));
            any = any || a.isDouble();
        }
        if (any) {
            auto* chart = new ui::BarChart;
            chart->set_data(cats, acc, tr("Accretion / dilution"));
            chart->set_signed_colors(true);
            chart->set_show_values(true);
            chart->set_value_formatter([](double v) { return ma_sw_pct(v, 1, true); });
            chart->setMinimumHeight(200);
            shown_charts_.append({tr("Accretion / dilution by year"), chart});
            result_view_->add_custom_widget(
                ui::ResultView::make_card(tr("Accretion / dilution by year"), chart, tr("EPS change vs standalone")));
        }
    }
}

} // namespace fincept::screens
