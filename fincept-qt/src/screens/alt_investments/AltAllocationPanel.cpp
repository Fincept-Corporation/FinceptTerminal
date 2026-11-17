// src/screens/alt_investments/AltAllocationPanel.cpp — A9: the alternatives sleeve inside a
// portfolio. See AltAllocationPanel.h.

#include "screens/alt_investments/AltAllocationPanel.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/report/ReportBlocks.h"
#include "screens/alt_investments/AltAllocationMath.h"
#include "screens/alt_investments/AltPanels.h"
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioClassify.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/DataGridEditor.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/report/AddToReportButton.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::alt {

using fincept::services::alt::AltHolding;
using fincept::services::alt::AltHoldingStore;
namespace fmt = fincept::ui::formatting;

namespace {

const QStringList& alt_alloc_classes() {
    static const QStringList k = {QStringLiteral("private_equity"), QStringLiteral("venture"),
                                  QStringLiteral("private_credit"), QStringLiteral("real_estate"),
                                  QStringLiteral("infrastructure"), QStringLiteral("hedge_fund"),
                                  QStringLiteral("commodities"),    QStringLiteral("collectibles"),
                                  QStringLiteral("other")};
    return k;
}

QString alt_alloc_class_label(const QString& key) {
    static const QHash<QString, const char*> k = {
        {QStringLiteral("private_equity"), QT_TRANSLATE_NOOP("AltAllocation", "Private equity")},
        {QStringLiteral("venture"), QT_TRANSLATE_NOOP("AltAllocation", "Venture")},
        {QStringLiteral("private_credit"), QT_TRANSLATE_NOOP("AltAllocation", "Private credit")},
        {QStringLiteral("real_estate"), QT_TRANSLATE_NOOP("AltAllocation", "Real estate")},
        {QStringLiteral("infrastructure"), QT_TRANSLATE_NOOP("AltAllocation", "Infrastructure")},
        {QStringLiteral("hedge_fund"), QT_TRANSLATE_NOOP("AltAllocation", "Hedge funds")},
        {QStringLiteral("commodities"), QT_TRANSLATE_NOOP("AltAllocation", "Commodities")},
        {QStringLiteral("collectibles"), QT_TRANSLATE_NOOP("AltAllocation", "Collectibles")},
        {QStringLiteral("other"), QT_TRANSLATE_NOOP("AltAllocation", "Other")},
        {QStringLiteral("equity"), QT_TRANSLATE_NOOP("AltAllocation", "Listed equity")},
        {QStringLiteral("bond"), QT_TRANSLATE_NOOP("AltAllocation", "Bonds")},
        {QStringLiteral("commodity"), QT_TRANSLATE_NOOP("AltAllocation", "Listed commodities")},
        {QStringLiteral("crypto"), QT_TRANSLATE_NOOP("AltAllocation", "Crypto")},
        {QStringLiteral("cash"), QT_TRANSLATE_NOOP("AltAllocation", "Cash")},
        {QStringLiteral("listed_real_estate"), QT_TRANSLATE_NOOP("AltAllocation", "Listed real estate")},
        {QStringLiteral("funds"), QT_TRANSLATE_NOOP("AltAllocation", "Private-market funds")},
    };
    const char* l = k.value(key, nullptr);
    return l ? QCoreApplication::translate("AltAllocation", l) : key;
}

QString alt_alloc_tier_label(int tier) {
    static const char* const k[] = {QT_TRANSLATE_NOOP("AltAllocation", "Within a day"),
                                    QT_TRANSLATE_NOOP("AltAllocation", "Within a month"),
                                    QT_TRANSLATE_NOOP("AltAllocation", "Within a quarter"),
                                    QT_TRANSLATE_NOOP("AltAllocation", "Within a year"),
                                    QT_TRANSLATE_NOOP("AltAllocation", "Locked (over a year)")};
    return QCoreApplication::translate("AltAllocation", k[std::clamp(tier, 0, alt_alloc::kTierCount - 1)]);
}

} // namespace

QWidget* make_allocation_panel(QWidget* parent) {
    return new AltAllocationPanel(parent);
}

AltAllocationPanel::AltAllocationPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("altAllocPanel"));
    build_ui();
    auto& ps = services::PortfolioService::instance();
    connect(&ps, &services::PortfolioService::portfolios_loaded, this,
            [this](const QVector<portfolio::Portfolio>& list) {
                const QString keep = portfolio_combo_->currentData().toString();
                const QSignalBlocker block(portfolio_combo_);
                portfolio_combo_->clear();
                portfolio_combo_->addItem(tr("No portfolio (alternatives only)"), QString());
                for (const auto& p : list)
                    portfolio_combo_->addItem(p.name + QStringLiteral("  ·  ") + p.currency, p.id);
                int at = portfolio_combo_->findData(keep);
                if (at < 0 && portfolio_combo_->count() > 1)
                    at = 1; // the first portfolio
                portfolio_combo_->setCurrentIndex(std::max(0, at));
                on_portfolio_changed();
            });
    connect(&ps, &services::PortfolioService::summary_loaded, this, [this](const portfolio::PortfolioSummary& s) {
        if (s.portfolio.id != portfolio_id_ || portfolio_id_.isEmpty())
            return; // another portfolio's (or another screen's) summary
        liquid_positions_.clear();
        unpriced_ = 0;
        base_ccy_ = s.base_currency.isEmpty() ? s.portfolio.currency : s.base_currency;
        grid_->set_column_currency(QStringLiteral("value"), base_ccy_);
        grid_->set_column_currency(QStringLiteral("unfunded"), base_ccy_);
        for (const auto& h : s.holdings) {
            if (!std::isfinite(h.market_value)) {
                ++unpriced_;
                continue;
            }
            const auto info = portfolio::classify_asset(h.symbol, h.quote_type, h.name);
            QString cls = portfolio::asset_class_key(info.cls);
            if (cls == QLatin1String("real_estate"))
                cls = QStringLiteral("listed_real_estate"); // REITs trade daily: not a private holding
            liquid_positions_ << Liquid{h.symbol, cls, h.market_value};
        }
        have_summary_ = true;
        recompute();
    });
    connect(&ps, &services::PortfolioService::summary_error, this, [this](const QString& id, const QString& err) {
        if (id != portfolio_id_)
            return;
        source_note_->setText(tr("The portfolio could not be valued (%1). Alternatives are shown on their own.").arg(err));
        have_summary_ = true;
        liquid_positions_.clear();
        recompute();
    });
    fmt::on_format_changed(this, [this]() { recompute(); });
}

AltAllocationPanel::~AltAllocationPanel() {
    if (save_timer_ && save_timer_->isActive())
        save_holdings();
}

void AltAllocationPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("altRightBar"));
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 6, 8, 6);
    bl->setSpacing(6);
    auto* lbl = new QLabel(tr("PORTFOLIO"), bar);
    lbl->setObjectName(QStringLiteral("altRightTitle"));
    portfolio_combo_ = new QComboBox(bar);
    portfolio_combo_->setMinimumContentsLength(22);
    portfolio_combo_->addItem(tr("Loading portfolios…"), QString());
    portfolio_combo_->setAccessibleName(tr("Portfolio"));
    connect(portfolio_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { on_portfolio_changed(); });
    auto* refresh = new QPushButton(tr("REFRESH"), bar);
    refresh->setObjectName(QStringLiteral("altSecondaryBtn"));
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, this, [this]() { load_sources(); });
    auto* open_pf = new QPushButton(tr("OPEN PORTFOLIO"), bar);
    open_pf->setObjectName(QStringLiteral("altSecondaryBtn"));
    open_pf->setCursor(Qt::PointingHandCursor);
    connect(open_pf, &QPushButton::clicked, this, []() {
        fincept::EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                              {{QStringLiteral("screen_id"), QStringLiteral("portfolio")}});
    });
    source_note_ = new QLabel(bar);
    source_note_->setObjectName(QStringLiteral("altStatusText"));
    source_note_->setWordWrap(true);
    auto* report = new ui::AddToReportButton(QStringLiteral("Alternative Investments"), bar);
    report->set_payload_provider([this]() { return report_payload(); });
    report->set_show_result(false);
    bl->addWidget(lbl);
    bl->addWidget(portfolio_combo_);
    bl->addWidget(refresh);
    bl->addWidget(open_pf);
    bl->addWidget(source_note_, 1);
    bl->addWidget(report);
    root->addWidget(bar);

    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    auto* left = new QWidget(split);
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(12, 10, 12, 10);
    lvl->setSpacing(6);
    auto* gt = new QLabel(tr("OTHER ALTERNATIVE HOLDINGS"), left);
    gt->setObjectName(QStringLiteral("altFieldLabel"));
    lvl->addWidget(gt);
    using ui::DataGridColumn;
    QStringList class_labels;
    for (const QString& c : alt_alloc_classes())
        class_labels << alt_alloc_class_label(c);
    auto liq = DataGridColumn::choice(QStringLiteral("liquidity"), tr("LIQUIDITY"),
                                      {QStringLiteral("daily"), QStringLiteral("monthly"), QStringLiteral("quarterly"),
                                       QStringLiteral("annual"), QStringLiteral("locked")},
                                      {tr("Daily"), tr("Monthly"), tr("Quarterly"), tr("Annual"), tr("Locked")});
    liq.tooltip = tr("How soon the holding can be turned into cash (notice and redemption terms, lock-ups)");
    // Money columns in the portfolio's base currency (set whenever base_ccy_ changes): read like every
    // money field ("2.5M", "1,250,000"); an amount typed in another currency is refused, never converted.
    auto unfunded = DataGridColumn::amount(QStringLiteral("unfunded"), tr("UNFUNDED"), QString(), 0, /*required=*/false);
    unfunded.tooltip = tr("Commitment not yet called (blank = none)");
    grid_ = new ui::DataGridEditor({DataGridColumn::text(QStringLiteral("name"), tr("NAME"), /*required=*/true),
                                    DataGridColumn::choice(QStringLiteral("class"), tr("CLASS"), alt_alloc_classes(),
                                                           class_labels),
                                    DataGridColumn::amount(QStringLiteral("value"), tr("VALUE"), QString(), 0), liq,
                                    unfunded},
                                   left);
    grid_->set_hint(tr("holdings the Portfolio tab does not price: direct property, a hedge fund, art; private-market "
                       "funds come from PRIVATE MARKETS"));
    grid_->set_visible_rows(8);
    lvl->addWidget(grid_, 1);
    grid_note_ = new QLabel(left);
    grid_note_->setObjectName(QStringLiteral("altMethodNote"));
    grid_note_->setWordWrap(true);
    lvl->addWidget(grid_note_);
    save_timer_ = new QTimer(this);
    save_timer_->setSingleShot(true);
    save_timer_->setInterval(800);
    connect(save_timer_, &QTimer::timeout, this, &AltAllocationPanel::save_holdings);
    connect(grid_, &ui::DataGridEditor::changed, this, [this]() {
        if (filling_grid_)
            return;
        save_timer_->start();
        recompute();
    });
    split->addWidget(left);

    view_ = new ui::ResultView(split);
    view_->show_loading(tr("Reading the portfolio and the fund book…"));
    split->addWidget(view_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    const int lw = std::max(380, QFontMetrics(ui::fonts::font(-1)).averageCharWidth() * 62);
    left->setMinimumWidth(lw);
    split->setSizes({lw, 4000});
    root->addWidget(split, 1);
}

void AltAllocationPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!portfolios_requested_) {
        portfolios_requested_ = true; // the list arrives on portfolios_loaded (shared with the Portfolio tab)
        services::PortfolioService::instance().load_portfolios();
    }
}

void AltAllocationPanel::on_portfolio_changed() {
    if (save_timer_->isActive()) {
        save_timer_->stop();
        save_holdings(); // the edits belong to the portfolio that was on screen
    }
    portfolio_id_ = portfolio_combo_->currentData().toString();
    load_sources();
}

void AltAllocationPanel::load_sources() {
    const quint64 gen = ++generation_;
    have_summary_ = portfolio_id_.isEmpty(); // nothing to value without a portfolio
    have_funds_ = false;
    have_holdings_ = false;
    liquid_positions_.clear();
    unpriced_ = 0;
    base_ccy_ = fmt::display_currency();
    grid_->set_column_currency(QStringLiteral("value"), base_ccy_);
    grid_->set_column_currency(QStringLiteral("unfunded"), base_ccy_);
    view_->show_loading(tr("Reading the portfolio and the fund book…"));
    if (!portfolio_id_.isEmpty())
        services::PortfolioService::instance().load_summary(portfolio_id_);
    AltHoldingStore::list_fund_positions(this, [this, gen](const auto& funds, const QString&) {
        if (gen != generation_)
            return;
        funds_ = funds;
        have_funds_ = true;
        recompute();
    });
    AltHoldingStore::list_holdings(portfolio_id_, this, [this, gen](const QVector<AltHolding>& rows, const QString&) {
        if (gen != generation_)
            return;
        QJsonArray arr;
        for (const AltHolding& h : rows)
            arr.append(QJsonObject{{QStringLiteral("name"), h.name},
                                   {QStringLiteral("class"), h.asset_class},
                                   {QStringLiteral("value"), h.value},
                                   {QStringLiteral("liquidity"), h.liquidity},
                                   {QStringLiteral("unfunded"), h.unfunded}});
        filling_grid_ = true;
        grid_->from_json(arr);
        filling_grid_ = false;
        have_holdings_ = true;
        recompute();
    });
}

void AltAllocationPanel::save_holdings() {
    QVector<AltHolding> rows;
    for (const QJsonValue& v : grid_->to_json()) {
        const QJsonObject o = v.toObject();
        AltHolding h;
        h.name = o.value(QStringLiteral("name")).toString();
        h.asset_class = o.value(QStringLiteral("class")).toString();
        h.value = o.value(QStringLiteral("value")).toDouble();
        h.liquidity = o.value(QStringLiteral("liquidity")).toString();
        h.unfunded = o.value(QStringLiteral("unfunded")).toDouble();
        h.currency = base_ccy_;
        h.portfolio_id = portfolio_id_;
        rows << h;
    }
    QPointer<QLabel> note = grid_note_;
    AltHoldingStore::replace_holdings(portfolio_id_, rows, this, [note](bool ok, const QString&) {
        if (note && !ok)
            note->setText(AltAllocationPanel::tr("Could not save the holdings - your edits stay on screen."));
    });
}

void AltAllocationPanel::recompute() {
    if (!have_summary_ || !have_funds_ || !have_holdings_)
        return;
    using namespace alt_alloc;
    QVector<Position> ps;
    for (const Liquid& l : liquid_positions_)
        ps << Position{l.name, l.asset_class, l.value, Daily, 0, false};
    QStringList left_out;
    QStringList no_nav;
    for (const auto& f : funds_) {
        if (!f.currency.isEmpty() && f.currency.compare(base_ccy_, Qt::CaseInsensitive) != 0) {
            left_out << QStringLiteral("%1 (%2)").arg(f.name, f.currency);
            continue; // amounts are not converted here
        }
        if (f.nav_date.isEmpty())
            no_nav << f.name;
        ps << Position{f.name, QStringLiteral("funds"), f.nav, Locked, f.unfunded(), true};
    }
    for (const QJsonValue& v : grid_->to_json()) {
        const QJsonObject o = v.toObject();
        ps << Position{o.value(QStringLiteral("name")).toString(), o.value(QStringLiteral("class")).toString(),
                       o.value(QStringLiteral("value")).toDouble(), tier_from_key(o.value(QStringLiteral("liquidity")).toString()),
                       o.value(QStringLiteral("unfunded")).toDouble(), true};
    }

    const QString ccy = base_ccy_;
    QJsonObject out;
    const double total = total_value(ps);
    out.insert(QStringLiteral("total_value"), total);
    out.insert(QStringLiteral("alternatives_value"), alternative_value(ps));
    out.insert(QStringLiteral("alternatives_weight_pct"), total > 0 ? alternative_value(ps) / total * 100.0 : 0.0);
    out.insert(QStringLiteral("unfunded_commitments"), total_unfunded(ps));
    out.insert(QStringLiteral("liquid_within_90_days"), liquid_within(ps, 92));
    const double cover = unfunded_cover(ps);
    if (std::isfinite(cover))
        out.insert(QStringLiteral("unfunded_cover_multiple"), cover);

    QJsonArray classes;
    const auto cls = by_class(ps);
    for (auto it = cls.constBegin(); it != cls.constEnd(); ++it)
        if (it.value() > 0)
            classes.append(QJsonObject{{QStringLiteral("asset_class"), alt_alloc_class_label(it.key())},
                                       {QStringLiteral("value"), it.value()},
                                       {QStringLiteral("weight_pct"), total > 0 ? it.value() / total * 100.0 : 0.0}});
    out.insert(QStringLiteral("allocation"), classes);

    QJsonArray lad;
    const QVector<double> l = ladder(ps);
    for (int i = 0; i < kTierCount; ++i)
        lad.append(QJsonObject{{QStringLiteral("horizon"), alt_alloc_tier_label(i)},
                               {QStringLiteral("cumulative_value"), l.at(i)},
                               {QStringLiteral("cumulative_pct"), total > 0 ? l.at(i) / total * 100.0 : 0.0}});
    out.insert(QStringLiteral("liquidity_ladder"), lad);

    QJsonArray stresses;
    struct Case {
        const char* name;
        double shock;
        double calls;
    };
    for (const Case& c : {Case{QT_TRANSLATE_NOOP("AltAllocation", "Today"), 0, 0},
                          Case{QT_TRANSLATE_NOOP("AltAllocation", "Listed -20%, 25% of unfunded called"), -20, 25},
                          Case{QT_TRANSLATE_NOOP("AltAllocation", "Listed -40%, 50% of unfunded called"), -40, 50}}) {
        const StressResult r = stress(ps, c.shock, c.calls);
        QJsonObject row{{QStringLiteral("scenario"), QCoreApplication::translate("AltAllocation", c.name)},
                        {QStringLiteral("total_value"), r.total},
                        {QStringLiteral("alternatives_weight_pct"), r.alt_weight_pct},
                        {QStringLiteral("liquid_within_90_days"), r.liquid_90d},
                        {QStringLiteral("unfunded_commitments"), r.unfunded}};
        if (std::isfinite(r.cover))
            row.insert(QStringLiteral("cover_multiple"), r.cover);
        stresses.append(row);
    }
    out.insert(QStringLiteral("stress_denominator_effect"), stresses);
    last_ = out;

    QStringList notes;
    if (portfolio_id_.isEmpty())
        notes << tr("No portfolio chosen: alternatives only.");
    if (unpriced_ > 0)
        notes << tr("%1 portfolio holding(s) without a price are left out.").arg(unpriced_);
    if (!left_out.isEmpty())
        notes << tr("Funds in another currency are left out (no conversion here): %1.").arg(left_out.join(QStringLiteral(", ")));
    if (!no_nav.isEmpty())
        notes << tr("No NAV statement yet, counted at zero value: %1.").arg(no_nav.join(QStringLiteral(", ")));
    notes << tr("Amounts in %1. Stress: listed assets move, private NAVs are held (they lag), calls are paid from "
                "listed assets, no distributions.")
                 .arg(ccy);
    source_note_->setText(notes.join(QLatin1Char(' ')));
    grid_note_->setText(tr("Values in %1, the portfolio's base currency. %2 row(s) used.")
                            .arg(ccy)
                            .arg(grid_->valid_row_count()));

    ui::result::Options o;
    o.kpi_paths = {QStringLiteral("total_value"), QStringLiteral("alternatives_value"),
                   QStringLiteral("alternatives_weight_pct"), QStringLiteral("unfunded_commitments"),
                   QStringLiteral("liquid_within_90_days"), QStringLiteral("unfunded_cover_multiple")};
    o.money_format = [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); };
    o.unit_for = [](const QString&, const QString& key, double) {
        if (key.endsWith(QLatin1String("_pct")))
            return ui::result::Unit::Percent;
        if (key.contains(QLatin1String("multiple")))
            return ui::result::Unit::Multiple;
        if (key.contains(QLatin1String("value")) || key.contains(QLatin1String("unfunded")) ||
            key.contains(QLatin1String("liquid")))
            return ui::result::Unit::Money;
        return ui::result::Unit::Auto;
    };
    o.bar_columns.insert(QStringLiteral("allocation"), QStringLiteral("value"));
    o.bar_columns.insert(QStringLiteral("liquidity_ladder"), QStringLiteral("cumulative_value"));
    view_->set_options(o);
    view_->show_result(out);
}

fincept::report::ReportPayload AltAllocationPanel::report_payload() const {
    fincept::report::ReportPayload p;
    if (last_.isEmpty())
        return p;
    p.title = tr("Alternatives allocation and liquidity");
    p.source = QStringLiteral("Alternative Investments");
    p.as_of = QDateTime::currentDateTime();
    const QString ccy = base_ccy_;
    auto money = [ccy](double v) { return fmt::money(v, ccy, fmt::Compact::FromSettings); };
    p.kpis.append({tr("Total"), money(last_.value(QStringLiteral("total_value")).toDouble()), QString(), QString()});
    p.kpis.append({tr("Alternatives"), money(last_.value(QStringLiteral("alternatives_value")).toDouble()), QString(), QString()});
    p.kpis.append({tr("Alternatives weight"),
                   QLocale().toString(last_.value(QStringLiteral("alternatives_weight_pct")).toDouble(), 'f', 1),
                   QStringLiteral("%"), QString()});
    p.kpis.append({tr("Unfunded"), money(last_.value(QStringLiteral("unfunded_commitments")).toDouble()), QString(), QString()});
    if (last_.contains(QStringLiteral("unfunded_cover_multiple")))
        p.kpis.append({tr("Cover by 90-day liquidity"),
                       QLocale().toString(last_.value(QStringLiteral("unfunded_cover_multiple")).toDouble(), 'f', 2),
                       QStringLiteral("x"), QString()});
    fincept::report::PayloadTable ladder_t;
    ladder_t.title = tr("Liquidity ladder (cumulative)");
    ladder_t.columns = {tr("Horizon"), fmt::header(tr("Value"), ccy), tr("% of total")};
    for (const QJsonValue& v : last_.value(QStringLiteral("liquidity_ladder")).toArray()) {
        const QJsonObject o = v.toObject();
        ladder_t.rows << QStringList{o.value(QStringLiteral("horizon")).toString(),
                                     money(o.value(QStringLiteral("cumulative_value")).toDouble()),
                                     QLocale().toString(o.value(QStringLiteral("cumulative_pct")).toDouble(), 'f', 1)};
    }
    p.tables << ladder_t;
    fincept::report::PayloadTable st;
    st.title = tr("Stress: the denominator effect");
    st.columns = {tr("Scenario"), tr("Alternatives weight %"), fmt::header(tr("Liquid within 90 days"), ccy),
                  fmt::header(tr("Unfunded"), ccy), tr("Cover (x)")};
    for (const QJsonValue& v : last_.value(QStringLiteral("stress_denominator_effect")).toArray()) {
        const QJsonObject o = v.toObject();
        st.rows << QStringList{o.value(QStringLiteral("scenario")).toString(),
                               QLocale().toString(o.value(QStringLiteral("alternatives_weight_pct")).toDouble(), 'f', 1),
                               money(o.value(QStringLiteral("liquid_within_90_days")).toDouble()),
                               money(o.value(QStringLiteral("unfunded_commitments")).toDouble()),
                               o.contains(QStringLiteral("cover_multiple"))
                                   ? QLocale().toString(o.value(QStringLiteral("cover_multiple")).toDouble(), 'f', 2)
                                   : QStringLiteral("-")};
    }
    p.tables << st;
    p.notes = source_note_->text();
    return p;
}

} // namespace fincept::screens::alt
