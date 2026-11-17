// src/screens/equity_research/EquityFinancialsOverview.cpp
#include "screens/equity_research/EquityFinancialsOverview.h"

#include "screens/equity_research/EquityFinancialsCharts.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/Sparkline.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

namespace fo_fmt = fincept::ui::formatting;
using fin_charts::ComboBars;
using fin_charts::ComboLine;

bool fo_ok(double v) {
    return std::isfinite(v);
}

/// Chart heights at the default font, growing in proportion with larger fonts (a chart's text —
/// axis labels, legend, the DuPont boxes — grows with the font, so its plot must too).
constexpr int kFoChartPx = 230;
constexpr int kFoDupontPx = 210;
constexpr int kFoRoicPx = 150;

ui::PaintedChart::Formatter fo_pct_formatter() {
    return [](double v) { return fo_fmt::fraction_pct(v, 1, false); };
}

ui::PaintedChart::Formatter fo_money_formatter(const QString& ccy) {
    return [ccy](double v) { return fo_fmt::axis_money(v, ccy); };
}

QVector<fin_charts::FinWaterfallChart::Step> fo_steps(const QVector<fin_an::Step>& in) {
    QVector<fin_charts::FinWaterfallChart::Step> out;
    for (const auto& s : in) {
        fin_charts::FinWaterfallChart::Step w;
        w.label = s.label;
        w.value = s.value;
        w.tip = s.note;
        switch (s.kind) {
            case fin_an::Step::Kind::Start:
                w.kind = fin_charts::FinWaterfallChart::Kind::Start;
                break;
            case fin_an::Step::Kind::Subtotal:
                w.kind = fin_charts::FinWaterfallChart::Kind::Subtotal;
                break;
            case fin_an::Step::Kind::End:
                w.kind = fin_charts::FinWaterfallChart::Kind::End;
                break;
            case fin_an::Step::Kind::Delta:
            default:
                w.kind = fin_charts::FinWaterfallChart::Kind::Delta;
                break;
        }
        out.append(w);
    }
    return out;
}

/// The last `n` readings up to and including `upto`.
QVector<double> fo_tail(const fin_an::Series& s, int upto, int n) {
    QVector<double> out;
    for (int i = std::max(0, upto - n + 1); i <= upto && i < s.size(); ++i)
        out.append(s.at(i));
    return out;
}

} // namespace

FinOverviewPage::FinOverviewPage(QWidget* parent) : QWidget(parent) {
    build_ui();
    // Chart heights follow the font: re-fit once an appearance change has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        for (auto* c : findChildren<ui::PaintedChart*>()) {
            const QVariant px = c->property("finHeightPx");
            if (px.isValid())
                c->setMinimumHeight(ui::metrics::width(px.toInt()));
        }
        relayout(true);
    });
}

FinOverviewPage::Cell& FinOverviewPage::add_cell(const QString& key) {
    Cell c;
    c.key = key;
    c.panel = fin_panel(QString(), body_);
    c.panel.frame->setObjectName(QStringLiteral("finPanel"));
    cells_.append(c);
    return cells_.last();
}

void FinOverviewPage::build_ui() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("finScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body_ = new QWidget(scroll_);
    body_->setObjectName(QStringLiteral("finScrollBody"));
    grid_ = new QGridLayout(body_);
    grid_->setContentsMargins(8, 8, 8, 8);
    grid_->setHorizontalSpacing(8);
    grid_->setVerticalSpacing(8);
    scroll_->setWidget(body_);
    outer->addWidget(scroll_);

    auto sized = [](QWidget* w, int px) {
        w->setProperty("finHeightPx", px);
        w->setMinimumHeight(ui::metrics::width(px)); // proportional to the font (never below px)
    };
    auto wire = [this](fin_charts::FinChartBase* c) {
        connect(c, &fin_charts::FinChartBase::category_clicked, this, &FinOverviewPage::period_selected);
    };
    auto combo = [&](const QString& key) {
        Cell& cell = add_cell(key);
        auto* chart = new fin_charts::FinComboChart(cell.panel.frame);
        sized(chart, kFoChartPx);
        wire(chart);
        cell.panel.body->addWidget(chart, 1);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
        return chart;
    };
    rev_chart_ = combo(QStringLiteral("rev"));
    {
        Cell& cell = add_cell(QStringLiteral("wf"));
        wf_chart_ = new fin_charts::FinWaterfallChart(cell.panel.frame);
        sized(wf_chart_, kFoChartPx);
        cell.panel.body->addWidget(wf_chart_, 1);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
    }
    margin_chart_ = combo(QStringLiteral("margins"));
    {
        Cell& cell = add_cell(QStringLiteral("structure"));
        struct_chart_ = new fin_charts::FinStackChart(cell.panel.frame);
        sized(struct_chart_, kFoChartPx);
        wire(struct_chart_);
        cell.panel.body->addWidget(struct_chart_, 1);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
    }
    cash_chart_ = combo(QStringLiteral("cash"));
    {
        Cell& cell = add_cell(QStringLiteral("bridge"));
        bridge_chart_ = new fin_charts::FinWaterfallChart(cell.panel.frame);
        sized(bridge_chart_, kFoChartPx);
        cell.panel.body->addWidget(bridge_chart_, 1);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
    }
    {
        Cell& cell = add_cell(QStringLiteral("returns"));
        dupont_ = new fin_charts::FinDupontView(cell.panel.frame);
        sized(dupont_, kFoDupontPx);
        cell.panel.body->addWidget(dupont_, 3);
        roic_chart_ = new fin_charts::FinComboChart(cell.panel.frame);
        sized(roic_chart_, kFoRoicPx);
        wire(roic_chart_);
        cell.panel.body->addWidget(roic_chart_, 2);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
    }
    {
        Cell& cell = add_cell(QStringLiteral("growth"));
        growth_ = new ui::MatrixHeatmap(cell.panel.frame);
        sized(growth_, kFoChartPx);
        growth_->set_show_legend(false);
        growth_->set_diverging_scale(-0.5, 0.5, 0.0);
        growth_->set_value_formatter([](double v) { return fo_fmt::fraction_pct(v, 1, true); });
        connect(growth_, &ui::MatrixHeatmap::cell_clicked, this, [this](int, int col) {
            const QVector<int> cols = growth_->property("finCols").value<QVector<int>>();
            if (col >= 0 && col < cols.size())
                emit period_selected(cols[col]);
        });
        cell.panel.body->addWidget(growth_, 1);
        cell.caption = fin_caption(cell.panel.frame);
        cell.panel.body->addWidget(cell.caption);
    }
    wc_chart_ = combo(QStringLiteral("wc"));
    capret_chart_ = combo(QStringLiteral("capret"));
    {
        // Share-count trend under the capital-return chart.
        Cell& cell = cells_.last();
        auto* row = new QWidget(cell.panel.frame);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(8);
        shares_label_ = new QLabel(row);
        shares_label_->setObjectName(QStringLiteral("finKey"));
        hl->addWidget(shares_label_);
        shares_spark_ = new ui::Sparkline(row);
        ui::SparklineStyle st;
        st.color_mode = ui::SparklineStyle::ColorMode::Accent;
        st.last_dot = true;
        st.min_max_dots = false;
        shares_spark_->set_spark_style(st);
        shares_spark_->setMinimumWidth(ui::metrics::width(120));
        hl->addWidget(shares_spark_, 1);
        cell.panel.body->insertWidget(cell.panel.body->count() - 1, row);
    }
    {
        Cell& cell = add_cell(QStringLiteral("banknote"));
        bank_note_ = new QLabel(cell.panel.frame);
        bank_note_->setObjectName(QStringLiteral("finCaption"));
        bank_note_->setWordWrap(true);
        cell.panel.body->addWidget(bank_note_);
        cell.panel.body->addStretch(1);
    }
    for (auto& c : cells_)
        c.panel.frame->hide();
    retranslate();
}

void FinOverviewPage::retranslate() {
    for (auto& c : cells_) {
        QString title;
        const bool fin = bundle_ && bundle_->an[vs_.basis].financial();
        const bool insurer = bundle_ && bundle_->an[vs_.basis].tmpl == fin_model::Template::Insurer;
        if (c.key == QLatin1String("rev"))
            title = tr("REVENUE & EARNINGS");
        else if (c.key == QLatin1String("wf"))
            title = tr("INCOME WATERFALL");
        else if (c.key == QLatin1String("margins"))
            title = fin ? tr("MARGINS & EFFICIENCY") : tr("MARGIN TREND");
        else if (c.key == QLatin1String("structure"))
            title = tr("BALANCE SHEET STRUCTURE");
        else if (c.key == QLatin1String("cash"))
            title = fin ? tr("CAPITAL STRENGTH") : tr("CASH FLOW");
        else if (c.key == QLatin1String("bridge"))
            title = tr("CASH BRIDGE");
        else if (c.key == QLatin1String("returns"))
            title = fin ? tr("RETURNS · DUPONT · ROE vs ROTE") : tr("RETURNS · DUPONT · ROIC");
        else if (c.key == QLatin1String("growth"))
            title = tr("GROWTH HEAT MAP (YOY)");
        else if (c.key == QLatin1String("wc"))
            title = fin ? (insurer ? tr("UNDERWRITING") : tr("BANKING RATIOS")) : tr("WORKING CAPITAL & EFFICIENCY");
        else if (c.key == QLatin1String("capret"))
            title = tr("CAPITAL RETURN");
        else if (c.key == QLatin1String("banknote"))
            title = tr("CASH FLOW");
        c.panel.header->set_title(title);
        c.panel.frame->setAccessibleName(title);
    }
    if (bank_note_)
        bank_note_->setText(
            tr("Cash-flow charts are not shown for banks and insurers: their operating cash flow moves with trading, "
               "lending and deposit balances rather than with earnings power. Capital return is measured against net "
               "income instead, and the cash-flow statement is on the CASH FLOW tab."));
}

void FinOverviewPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        if (bundle_) {
            fill_trends();
            fill_selected();
        }
    }
    QWidget::changeEvent(e);
}

void FinOverviewPage::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    relayout();
}

void FinOverviewPage::relayout(bool force) {
    const int w = scroll_ ? scroll_->viewport()->width() : width();
    const int unit = ui::metrics::width(470);
    const int cols = w < unit * 2 ? 1 : w < unit * 3 + 40 ? 2 : 3;
    if (cols == columns_ && !force)
        return;
    columns_ = cols;
    while (grid_->count() > 0)
        delete grid_->takeAt(0); // the layout item only; the panel stays
    QVector<Cell*> shown;
    for (auto& c : cells_) {
        const bool on = shown_keys_.contains(c.key);
        if (on)
            shown.append(&c);
        else if (c.panel.frame->isVisibleTo(this))
            c.panel.frame->hide();
    }
    std::sort(shown.begin(), shown.end(), [this](const Cell* a, const Cell* b) {
        return shown_keys_.indexOf(a->key) < shown_keys_.indexOf(b->key);
    });
    for (int i = 0; i < shown.size(); ++i) {
        const int r = i / cols, c = i % cols;
        const bool last = i == shown.size() - 1;
        const int span = last ? cols - c : 1;
        grid_->addWidget(shown[i]->panel.frame, r, c, 1, span);
        if (!shown[i]->panel.frame->isVisibleTo(this))
            shown[i]->panel.frame->show();
    }
    for (int c = 0; c < 3; ++c)
        grid_->setColumnStretch(c, c < cols ? 1 : 0);
}

QString FinOverviewPage::period_label(int i) const {
    if (!bundle_)
        return {};
    const auto& a = bundle_->an[vs_.basis];
    return i >= 0 && i < a.count() ? a.periods[i].label : QString();
}

void FinOverviewPage::set_data(const FinBundlePtr& bundle, const FinViewState& vs) {
    bundle_ = bundle;
    vs_ = vs;
    if (!bundle_)
        return;
    const auto& a = bundle_->an[vs_.basis];
    const bool fin = a.financial();
    shown_keys_ = fin ? QStringList{QStringLiteral("rev"),     QStringLiteral("wf"),       QStringLiteral("margins"),
                                    QStringLiteral("structure"), QStringLiteral("cash"),   QStringLiteral("returns"),
                                    QStringLiteral("growth"),  QStringLiteral("wc"),       QStringLiteral("capret"),
                                    QStringLiteral("banknote")}
                      : QStringList{QStringLiteral("rev"),       QStringLiteral("wf"),     QStringLiteral("margins"),
                                    QStringLiteral("structure"), QStringLiteral("cash"),   QStringLiteral("bridge"),
                                    QStringLiteral("returns"),   QStringLiteral("growth"), QStringLiteral("wc"),
                                    QStringLiteral("capret")};
    retranslate();
    fill_trends();
    fill_selected();
    relayout(true);
}

void FinOverviewPage::set_selected(int index) {
    vs_.selected = index;
    if (!bundle_)
        return;
    fill_selected();
}

void FinOverviewPage::fill_trends() {
    const auto& a = bundle_->an[vs_.basis];
    const int n = a.count();
    const bool fin = a.financial();
    const bool insurer = a.tmpl == fin_model::Template::Insurer;
    QStringList cats, tips;
    for (const auto& p : a.periods) {
        cats << p.label;
        tips << p.tooltip;
    }
    const QString few = n < 3 ? tr("Only %n period(s) on file — a trend needs at least three.", nullptr, n) : QString();
    auto line = [&a](const char* id) { return a.line(QLatin1String(id)).v; };
    auto metric = [&a](const char* id) { return a.metric_series(QLatin1String(id)).v; };
    auto ready = [&few](ui::PaintedChart* c) {
        if (few.isEmpty())
            c->set_state(ui::PaintedChart::Ready);
        else
            c->set_state(ui::PaintedChart::Empty, few);
    };
    const QString ccy = vs_.ccy;

    // 1 — Revenue & earnings: bars + margins on the right axis.
    rev_chart_->set_categories(cats, tips);
    if (fin) {
        rev_chart_->set_bars({{tr("Revenue"), line("revenue"), 0, {}},
                              insurer ? ComboBars{tr("Total expenses"), line("total_expenses"), 1, {}}
                                      : ComboBars{tr("Net interest income"), line("nii"), 1, {}},
                              {tr("Net income"), line("net_income"), 2, {}}});
        rev_chart_->set_lines({{tr("Net margin"), metric("net_margin"), 3, true, false, true},
                               {tr("Pretax margin"), metric("pretax_margin"), 4, true, true, true}});
    } else {
        rev_chart_->set_bars({{tr("Revenue"), line("revenue"), 0, {}},
                              {tr("Operating income"), line("operating_income"), 1, {}},
                              {tr("Net income"), line("net_income"), 2, {}}});
        rev_chart_->set_lines({{tr("Operating margin"), metric("operating_margin"), 3, true, false, true},
                               {tr("Net margin"), metric("net_margin"), 4, true, false, true}});
    }
    rev_chart_->set_primary_formatter(fo_money_formatter(ccy));
    rev_chart_->set_secondary_formatter(fo_pct_formatter());
    ready(rev_chart_);

    // 3 — Margins.
    margin_chart_->set_categories(cats, tips);
    margin_chart_->set_bars({});
    if (fin) {
        QVector<ComboLine> l{{tr("Net margin"), metric("net_margin"), 0, false, false, true},
                             {tr("Pretax margin"), metric("pretax_margin"), 1, false, false, true}};
        if (insurer)
            l << ComboLine{tr("Expenses ÷ revenue"), metric("expense_ratio"), 2, false, true, true};
        else
            l << ComboLine{tr("Efficiency ratio"), metric("efficiency_ratio"), 2, false, true, true}
              << ComboLine{tr("NII share"), metric("nii_share"), 3, false, false, true};
        margin_chart_->set_lines(l);
    } else {
        margin_chart_->set_lines({{tr("Gross"), metric("gross_margin"), 0, false, false, true},
                                  {tr("Operating"), metric("operating_margin"), 1, false, false, true},
                                  {tr("Net"), metric("net_margin"), 2, false, false, true},
                                  {tr("FCF"), metric("fcf_margin"), 3, false, true, true}});
    }
    margin_chart_->set_zero_based(false);
    margin_chart_->set_primary_formatter(fo_pct_formatter());
    ready(margin_chart_);

    // 4 — Balance-sheet structure (any number of periods: a single one is still a picture).
    struct_chart_->set_categories(cats, tips);
    QVector<fin_charts::FinStackChart::Segment> left, right;
    int slot = 0;
    for (const auto& s : a.assets_mix)
        left.append({s.label, s.value.v, slot++});
    slot = 6;
    for (const auto& s : a.funding_mix)
        right.append({s.label, s.value.v, slot++});
    struct_chart_->set_sides(tr("ASSETS"), left, tr("LIABILITIES + EQUITY"), right);
    struct_chart_->set_amount_formatter([ccy](double v) { return fin_money(v, ccy); });
    struct_chart_->set_state(left.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                             left.isEmpty() ? tr("No balance sheet on file for this basis.") : QString());

    // 5 — Cash flow (industrial) / capital strength (financial).
    cash_chart_->set_categories(cats, tips);
    if (fin) {
        cash_chart_->set_bars({});
        cash_chart_->set_lines({{tr("Equity ÷ assets"), metric("equity_to_assets"), 0, false, false, true},
                                {tr("Debt ÷ assets"), metric("debt_to_assets"), 1, false, false, true}});
        cash_chart_->set_zero_based(true);
        cash_chart_->set_primary_formatter(fo_pct_formatter());
        cash_chart_->set_secondary_formatter({});
    } else {
        const QVector<double> fcf = line("fcf");
        QVector<double> cum(fcf.size(), fin_an::kNaN);
        double run = 0.0;
        bool any = false;
        for (int i = 0; i < fcf.size(); ++i)
            if (fo_ok(fcf[i])) {
                run += fcf[i];
                cum[i] = run;
                any = true;
            } else if (any) {
                cum[i] = run;
            }
        cash_chart_->set_bars({{tr("Operating cash flow"), line("cfo"), 0, {}},
                               {tr("Capex"), line("capex"), 5, {}},
                               {tr("Free cash flow"), fcf, 3, {}}});
        cash_chart_->set_lines({{tr("Cumulative FCF"), cum, 2, false, true, true}});
        cash_chart_->set_zero_based(true);
        cash_chart_->set_primary_formatter(fo_money_formatter(ccy));
    }
    ready(cash_chart_);

    // 7 — Returns vs a hurdle.
    roic_chart_->set_categories(cats, tips);
    roic_chart_->set_bars({});
    if (fin)
        roic_chart_->set_lines({{tr("ROE"), metric("roe"), 0, false, false, true},
                                {tr("ROTE"), metric("rote"), 1, false, false, true}});
    else
        roic_chart_->set_lines({{tr("ROIC"), metric("roic"), 0, false, false, true},
                                {tr("ROE"), metric("roe"), 1, false, false, true}});
    roic_chart_->set_zero_based(true);
    roic_chart_->set_primary_formatter(fo_pct_formatter());
    roic_chart_->set_reference_line(fin_an::kHurdleRate, tr("%1 hurdle").arg(fo_fmt::fraction_pct(fin_an::kHurdleRate, 0, false)));
    ready(roic_chart_);

    // 8 — Growth heat map: the periods with a year-earlier comparison.
    {
        QVector<int> cols;
        for (int i = 0; i < n; ++i) {
            bool any = false;
            for (const auto& g : a.growth)
                any = any || fo_ok(g.yoy.at(i));
            if (any)
                cols.append(i);
        }
        QStringList rows, col_labels;
        QVector<QVector<double>> values;
        for (const auto& g : a.growth) {
            rows << g.label;
            QVector<double> r;
            for (int c : cols)
                r.append(g.yoy.at(c));
            values.append(r);
        }
        for (int c : cols)
            col_labels << a.periods[c].label;
        growth_->setProperty("finCols", QVariant::fromValue(cols));
        if (cols.isEmpty()) {
            growth_->clear();
            growth_->set_state(ui::PaintedChart::Empty, tr("No period a year earlier on file — growth needs two."));
        } else {
            growth_->set_state(ui::PaintedChart::Ready);
            growth_->set_matrix(rows, col_labels, values);
        }
    }

    // 9 — Working capital (industrial) / banking or underwriting ratios.
    wc_chart_->set_categories(cats, tips);
    wc_chart_->set_bars({});
    if (fin) {
        if (insurer)
            wc_chart_->set_lines({{tr("Expenses ÷ revenue"), metric("expense_ratio"), 0, false, false, true},
                                  {tr("ROA"), metric("roa"), 1, false, false, true}});
        else
            wc_chart_->set_lines({{tr("Efficiency ratio"), metric("efficiency_ratio"), 0, false, false, true},
                                  {tr("NII ÷ avg assets"), metric("nii_to_assets"), 1, false, false, true},
                                  {tr("ROA"), metric("roa"), 2, false, true, true}});
        wc_chart_->set_zero_based(true);
        wc_chart_->set_primary_formatter(fo_pct_formatter());
    } else {
        wc_chart_->set_lines({{tr("DSO"), metric("dso"), 0, false, false, true},
                              {tr("DIO"), metric("dio"), 1, false, false, true},
                              {tr("DPO"), metric("dpo"), 2, false, false, true},
                              {tr("Cash conversion cycle"), metric("ccc"), 3, false, true, true}});
        wc_chart_->set_zero_based(true);
        wc_chart_->set_primary_formatter([](double v) { return tr("%1 d").arg(fo_fmt::number(v, 0)); });
    }
    ready(wc_chart_);

    // 10 — Capital return: dividends + buybacks (stacked) vs FCF (or net income), payout line.
    capret_chart_->set_categories(cats, tips);
    capret_chart_->set_bars({{tr("Dividends"), metric("dividends_paid"), 0, QStringLiteral("ret")},
                             {tr("Buybacks"), metric("buybacks_paid"), 1, QStringLiteral("ret")},
                             fin ? ComboBars{tr("Net income"), line("net_income"), 2, {}}
                                 : ComboBars{tr("Free cash flow"), line("fcf"), 3, {}}});
    capret_chart_->set_lines({fin ? ComboLine{tr("Payout ÷ net income"), metric("total_payout_ni"), 4, true, false, true}
                                  : ComboLine{tr("Payout ÷ FCF"), metric("total_payout_fcf"), 4, true, false, true}});
    capret_chart_->set_primary_formatter(fo_money_formatter(ccy));
    capret_chart_->set_secondary_formatter(fo_pct_formatter());
    ready(capret_chart_);
    shares_spark_->set_data(metric("shares_diluted"));
}

void FinOverviewPage::fill_selected() {
    const auto& a = bundle_->an[vs_.basis];
    const int n = a.count();
    int sel = vs_.selected;
    if (sel < 0 || sel >= n)
        sel = a.latest();
    const bool fin = a.financial();
    const QString per = period_label(sel);
    const QString ccy = vs_.ccy;
    for (auto* c : {static_cast<fin_charts::FinChartBase*>(rev_chart_), static_cast<fin_charts::FinChartBase*>(margin_chart_),
                    static_cast<fin_charts::FinChartBase*>(struct_chart_), static_cast<fin_charts::FinChartBase*>(cash_chart_),
                    static_cast<fin_charts::FinChartBase*>(roic_chart_), static_cast<fin_charts::FinChartBase*>(wc_chart_),
                    static_cast<fin_charts::FinChartBase*>(capret_chart_)})
        c->set_selected(sel);
    auto L = [&a, sel](const char* id) { return a.line(QLatin1String(id)).at(sel); };
    auto M = [&a, sel](const char* id) { return a.metric_series(QLatin1String(id)).at(sel); };
    auto Mchange = [&a, sel](const char* id) {
        const int ya = sel >= 0 && sel < a.count() ? a.periods[sel].year_ago : -1;
        const fin_an::Series s = a.metric_series(QLatin1String(id));
        return fo_ok(s.at(sel)) && fo_ok(s.at(ya)) ? s.at(sel) - s.at(ya) : fin_an::kNaN;
    };
    auto pct = [](double v) { return fin_value(v, fin_an::Unit::Percent, {}); };
    auto Cell_ = [this](const char* key) -> Cell* {
        for (auto& c : cells_)
            if (c.key == QLatin1String(key))
                return &c;
        return nullptr;
    };
    auto set_caption = [&](const char* key, const QString& text, QWidget* chart) {
        if (Cell* c = Cell_(key)) {
            c->caption->setText(text);
            c->panel.header->set_badge(per);
            if (chart)
                chart->setAccessibleDescription(text);
        }
    };

    // Revenue & earnings.
    {
        const double rev = L("revenue");
        QString t = tr("%1 revenue %2").arg(per, fin_money(rev, ccy));
        const double g = M("revenue_growth");
        if (fo_ok(g))
            t += QStringLiteral(" (%1 YoY)").arg(fin_pct_signed(g));
        if (fin)
            t += tr(" · net margin %1 · pretax margin %2").arg(pct(M("net_margin")), pct(M("pretax_margin")));
        else
            t += tr(" · operating margin %1 (%2) · net margin %3")
                     .arg(pct(M("operating_margin")), fin_change(Mchange("operating_margin"), fin_an::Unit::Percent),
                          pct(M("net_margin")));
        set_caption("rev", t, rev_chart_);
        rev_chart_->set_summary(t);
    }
    // Income waterfall.
    {
        const auto steps = fin_an::income_waterfall(a, sel);
        wf_chart_->set_steps(fo_steps(steps));
        wf_chart_->set_caption(per);
        wf_chart_->set_value_formatter(fo_money_formatter(ccy));
        if (steps.isEmpty()) {
            wf_chart_->set_state(ui::PaintedChart::Empty, tr("Revenue or net income not reported for %1.").arg(per));
            set_caption("wf", QString(), wf_chart_);
        } else {
            wf_chart_->set_state(ui::PaintedChart::Ready);
            QString t = tr("Net income %1 = %2 of revenue").arg(fin_money(L("net_income"), ccy), pct(M("net_margin")));
            if (fo_ok(M("tax_rate")))
                t += tr(" · effective tax rate %1").arg(pct(M("tax_rate")));
            set_caption("wf", t, wf_chart_);
            wf_chart_->set_summary(t);
        }
    }
    // Margins.
    {
        QString t = fin ? tr("%1: net %2 · pretax %3 · efficiency ratio %4")
                              .arg(per, pct(M("net_margin")), pct(M("pretax_margin")), pct(M("efficiency_ratio")))
                        : tr("%1: gross %2 · operating %3 · net %4 · FCF %5")
                              .arg(per, pct(M("gross_margin")), pct(M("operating_margin")), pct(M("net_margin")),
                                   pct(M("fcf_margin")));
        set_caption("margins", t, margin_chart_);
        margin_chart_->set_summary(t);
    }
    // Structure.
    {
        const double ta = L("total_assets");
        auto share = [ta](double v) { return fo_ok(v) && fo_ok(ta) && ta > 0 ? v / ta : fin_an::kNaN; };
        QString t = tr("%1: total assets %2 · equity %3 of assets · debt %4")
                        .arg(per, fin_money(ta, ccy), pct(share(L("total_equity"))), pct(share(L("total_debt"))));
        if (!fin)
            t += tr(" · cash & ST investments %1").arg(pct(share(L("cash_sti"))));
        for (const auto& s : a.funding_mix)
            if (!s.value.note_at(sel).isEmpty())
                t += QStringLiteral(" · ") + s.value.note_at(sel);
        set_caption("structure", t, struct_chart_);
        struct_chart_->set_summary(t);
    }
    // Cash flow / capital strength.
    {
        QString t;
        if (fin) {
            t = tr("%1: equity %2 of assets · debt %3 of assets").arg(per, pct(M("equity_to_assets")), pct(M("debt_to_assets")));
        } else {
            double cum = 0.0;
            int k = 0;
            const fin_an::Series fcf = a.line(QStringLiteral("fcf"));
            for (int i = 0; i <= sel && i < fcf.size(); ++i)
                if (fo_ok(fcf.at(i))) {
                    cum += fcf.at(i);
                    ++k;
                }
            t = tr("%1: operating cash flow %2 · capex %3 · FCF %4 (%5 of revenue) · cumulative FCF %6 over %n period(s)",
                   nullptr, k)
                    .arg(per, fin_money(L("cfo"), ccy), fin_money(L("capex"), ccy), fin_money(L("fcf"), ccy),
                         pct(M("fcf_margin")), fin_money(cum, ccy));
        }
        set_caption("cash", t, cash_chart_);
        cash_chart_->set_summary(t);
    }
    // Cash bridge.
    if (!fin) {
        const auto steps = fin_an::cash_bridge(a, sel);
        bridge_chart_->set_steps(fo_steps(steps));
        bridge_chart_->set_caption(per);
        bridge_chart_->set_value_formatter(fo_money_formatter(ccy));
        if (steps.isEmpty()) {
            bridge_chart_->set_state(ui::PaintedChart::Empty, tr("Opening / closing cash not reported for %1.").arg(per));
            set_caption("bridge", QString(), bridge_chart_);
        } else {
            bridge_chart_->set_state(ui::PaintedChart::Ready);
            const double ret = std::fabs(fo_ok(L("dividends")) ? L("dividends") : 0.0) +
                               std::fabs(fo_ok(L("buybacks")) ? L("buybacks") : 0.0);
            const double cfo = L("cfo");
            QString t = tr("Cash %1 → %2").arg(fin_money(L("begin_cash"), ccy), fin_money(L("end_cash"), ccy));
            if (fo_ok(cfo) && cfo > 0)
                t += tr(" · dividends + buybacks used %1 of operating cash flow").arg(pct(ret / cfo));
            set_caption("bridge", t, bridge_chart_);
            bridge_chart_->set_summary(t);
        }
    }
    // Returns: DuPont tree for the selected period.
    {
        auto node = [&](const char* id, const QString& label, fin_an::Unit unit) {
            fin_charts::FinDupontView::Node nd;
            nd.label = label;
            const fin_an::Metric* m = a.metric(QLatin1String(id));
            const double v = M(id);
            nd.value_text = fo_ok(v) ? fin_value(v, unit, ccy) : fin_missing_text(a.metric_series(QLatin1String(id)).note_at(sel));
            const double ch = Mchange(id);
            if (fo_ok(ch)) {
                nd.change_text = tr("%1 %2 YoY").arg(fin_arrow(ch), fin_change(ch, unit));
                nd.change_sign = ch > 0 ? 1 : -1;
                if (m && !m->higher_is_better)
                    nd.change_sign = -nd.change_sign;
            }
            nd.trend = fo_tail(a.metric_series(QLatin1String(id)), sel, 6);
            nd.tip = m ? m->formula : QString();
            const QString note = a.metric_series(QLatin1String(id)).note_at(sel);
            if (!note.isEmpty())
                nd.tip += QStringLiteral("\n") + note;
            return nd;
        };
        auto root = node("roe", tr("RETURN ON EQUITY"), fin_an::Unit::Percent);
        root.tip = tr("Net income ÷ average equity = net margin × asset turnover × equity multiplier");
        dupont_->set_tree(root, {node("net_margin", tr("NET MARGIN"), fin_an::Unit::Percent),
                                 node("asset_turnover", tr("ASSET TURNOVER"), fin_an::Unit::Multiple),
                                 node("equity_multiplier", tr("EQUITY MULTIPLIER"), fin_an::Unit::Multiple)});
        const QString t = fin ? tr("%1: ROE %2 · ROTE %3 vs a %4 hurdle")
                                    .arg(per, pct(M("roe")), pct(M("rote")), pct(fin_an::kHurdleRate))
                              : tr("%1: ROE %2 = %3 × %4 × %5 · ROIC %6 vs a %7 hurdle")
                                    .arg(per, pct(M("roe")), pct(M("net_margin")),
                                         fin_value(M("asset_turnover"), fin_an::Unit::Multiple, {}),
                                         fin_value(M("equity_multiplier"), fin_an::Unit::Multiple, {}), pct(M("roic")),
                                         pct(fin_an::kHurdleRate));
        set_caption("returns", t, dupont_);
        dupont_->set_summary(t);
        roic_chart_->set_summary(t);
    }
    // Growth.
    {
        QStringList parts;
        for (const auto& g : a.growth) {
            if (parts.size() >= 4)
                break;
            const double v = g.yoy.at(sel);
            if (fo_ok(v))
                parts << QStringLiteral("%1 %2").arg(g.label, fin_pct_signed(v));
        }
        const QString t = parts.isEmpty() ? tr("%1: no year-earlier period on file · blank cell = not comparable (NM)").arg(per)
                                          : tr("%1: %2 · blank cell = not comparable").arg(per, parts.join(QStringLiteral(" · ")));
        set_caption("growth", t, growth_);
        growth_->setAccessibleDescription(t);
    }
    // Working capital / banking ratios.
    {
        QString t;
        if (fin) {
            t = tr("%1: efficiency ratio %2 · NII ÷ assets %3 · ROA %4")
                    .arg(per, pct(M("efficiency_ratio")), pct(M("nii_to_assets")), pct(M("roa")));
        } else {
            const double ccc = M("ccc");
            auto days = [](double v) { return fin_value(v, fin_an::Unit::Days, {}); };
            t = tr("%1: cash conversion cycle %2 = DSO %3 + DIO %4 − DPO %5").arg(per, days(ccc), days(M("dso")),
                                                                                    days(M("dio")), days(M("dpo")));
            if (fo_ok(ccc) && ccc < 0)
                t += tr(" · suppliers fund the working capital");
        }
        set_caption("wc", t, wc_chart_);
        wc_chart_->set_summary(t);
    }
    // Capital return.
    {
        const double div = M("dividends_paid"), buy = M("buybacks_paid");
        const double payout = fin ? M("total_payout_ni") : M("total_payout_fcf");
        QString t = tr("%1: dividends %2 · buybacks %3 · %4 of %5").arg(per, fin_money(div, ccy), fin_money(buy, ccy),
                                                                         pct(payout), fin ? tr("net income") : tr("FCF"));
        set_caption("capret", t, capret_chart_);
        capret_chart_->set_summary(t);
        const double sh = M("shares_diluted"), chg = M("share_change");
        QString s = tr("Diluted shares %1").arg(fin_value(sh, fin_an::Unit::Shares, {}));
        if (fo_ok(chg))
            s += QStringLiteral(" · %1 %2 ").arg(fin_arrow(chg), fin_pct_signed(chg)) + tr("YoY");
        shares_label_->setText(s);
        shares_spark_->setAccessibleName(s);
    }
    if (Cell* c = Cell_("banknote"))
        c->panel.header->set_badge(QString());
}

} // namespace fincept::screens
