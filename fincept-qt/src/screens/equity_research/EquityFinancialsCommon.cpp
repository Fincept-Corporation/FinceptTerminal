// src/screens/equity_research/EquityFinancialsCommon.cpp
#include "screens/equity_research/EquityFinancialsCommon.h"

#include "screens/equity_research/EquityResearchUi.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"

#include <QCoreApplication>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

namespace fcm_fmt = fincept::ui::formatting;

struct FcmText {
    Q_DECLARE_TR_FUNCTIONS(EquityFinancials)
};

fin_model::Period fcm_period(int basis) {
    return basis == kFinQuarterly ? fin_model::Period::Quarterly
           : basis == kFinTtm     ? fin_model::Period::Ttm
                                  : fin_model::Period::Annual;
}

fin_model::Statement fcm_statement(int st) {
    return st == 1 ? fin_model::Statement::Balance : st == 2 ? fin_model::Statement::CashFlow : fin_model::Statement::Income;
}

} // namespace

FinBundlePtr fin_bundle(const services::equity::FinancialsData& data, const services::equity::StockInfo& info) {
    auto b = std::make_shared<FinBundle>();
    b->symbol = data.symbol;
    b->data = data;
    b->info = info;
    b->fetched_at = data.fetched_at;
    b->stale = data.stale;
    b->has_quarters = !data.q_income.isEmpty() || !data.q_balance.isEmpty() || !data.q_cash_flow.isEmpty();

    fin_an::Context ctx;
    ctx.sector = info.sector;
    ctx.industry = info.industry;
    // The market value enters Altman's X4 only when it is in the reporting currency: the listing
    // trades in the currency it reports in (an ADR in USD reporting TWD does not).
    if (info.market_cap > 0.0) {
        if (info.financial_currency.isEmpty() || info.financial_currency == info.currency)
            ctx.market_cap = info.market_cap;
        else
            ctx.market_cap_note = FcmText::tr("quoted in %1, reports in %2").arg(info.currency, info.financial_currency);
    } else {
        ctx.market_cap_note = FcmText::tr("market value not available");
    }
    for (int basis = 0; basis < 3; ++basis) {
        b->an[basis] = fin_an::analyse(data, fcm_period(basis), ctx);
        for (int st = 0; st < 3; ++st) {
            b->stmt[basis][st] = fin_model::build(data, fcm_statement(st), fcm_period(basis), b->an[0].tmpl);
            b->stmt_periods[basis][st] = fin_an::period_infos(b->stmt[basis][st].columns, fcm_period(basis));
        }
    }
    return b;
}

QString fin_reporting_currency(const services::equity::StockInfo& info) {
    if (!info.financial_currency.isEmpty())
        return fcm_fmt::major_currency(info.financial_currency);
    if (!info.currency.isEmpty())
        return fcm_fmt::major_currency(info.currency);
    return {};
}

void fin_choose_scale(FinViewState& vs, const FinBundle& b) {
    double max_abs = 0.0;
    for (int basis = 0; basis < 3; ++basis)
        for (int st = 0; st < 2; ++st)
            for (const auto& r : b.stmt[basis][st].rows) {
                if (r.kind != fin_model::Kind::Amount)
                    continue;
                for (double v : r.values)
                    if (std::isfinite(v))
                        max_abs = std::max(max_abs, std::fabs(v));
            }
    if (fcm_fmt::resolve_compact(fcm_fmt::Compact::FromSettings, vs.ccy) == fcm_fmt::Compact::LakhCrore && max_abs >= 1e7) {
        vs.scale = 1e7;
        vs.scale_word = FcmText::tr("crore");
        vs.decimals = max_abs / 1e7 >= 1000.0 ? 0 : 1;
    } else if (max_abs >= 1e11) {
        vs.scale = 1e9;
        vs.scale_word = FcmText::tr("billions");
        vs.decimals = 2;
    } else if (max_abs >= 1e8) {
        vs.scale = 1e6;
        vs.scale_word = FcmText::tr("millions");
        vs.decimals = max_abs / 1e6 >= 1000.0 ? 0 : 1;
    } else if (max_abs >= 1e5) {
        vs.scale = 1e3;
        vs.scale_word = FcmText::tr("thousands");
        vs.decimals = 0;
    } else {
        vs.scale = 1.0;
        vs.scale_word = FcmText::tr("units");
        vs.decimals = 0;
    }
}

QString fin_amount(double v, const FinViewState& vs) {
    if (!std::isfinite(v))
        return fcm_fmt::placeholder();
    const double x = v / vs.scale;
    const double rounding = 0.5 * std::pow(10.0, -vs.decimals);
    if (x < 0.0 && std::fabs(x) >= rounding &&
        fcm_fmt::FormatSettings::instance().negative_style() == fcm_fmt::NegativeStyle::Parentheses)
        return QStringLiteral("(%1)").arg(fcm_fmt::number(std::fabs(x), vs.decimals));
    return fcm_fmt::number(x, vs.decimals);
}

QString fin_money(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return fcm_fmt::placeholder();
    return fcm_fmt::money(v, ccy, fcm_fmt::Compact::FromSettings);
}

QString fin_value(double v, fin_an::Unit unit, const QString& ccy) {
    if (!std::isfinite(v))
        return fcm_fmt::placeholder();
    switch (unit) {
        case fin_an::Unit::Money:
            return fin_money(v, ccy);
        case fin_an::Unit::PerShare:
            return fcm_fmt::money(v, ccy, fcm_fmt::Compact::None, 2);
        case fin_an::Unit::Shares:
            return fcm_fmt::compact(v, fcm_fmt::Compact::FromSettings);
        case fin_an::Unit::Percent:
            return fcm_fmt::fraction_pct(v, 1, false);
        case fin_an::Unit::PctPoints:
            return fcm_fmt::pp(v * 100.0, 1, true);
        case fin_an::Unit::Multiple:
            return fcm_fmt::ratio(v, 2);
        case fin_an::Unit::Days:
            return FcmText::tr("%1 d").arg(fcm_fmt::number(v, 0));
        case fin_an::Unit::Score:
        default:
            return fcm_fmt::number(v, 2);
    }
}

QString fin_change(double change, fin_an::Unit unit) {
    if (!std::isfinite(change))
        return fcm_fmt::placeholder();
    switch (unit) {
        case fin_an::Unit::Percent:
        case fin_an::Unit::PctPoints:
            return fcm_fmt::pp(change * 100.0, 1, true);
        case fin_an::Unit::Money:
        case fin_an::Unit::PerShare:
        case fin_an::Unit::Shares:
            return fcm_fmt::fraction_pct(change, 1, true);
        case fin_an::Unit::Multiple:
            return fcm_fmt::signed_number(change, 2) + QStringLiteral("x");
        case fin_an::Unit::Days:
            return FcmText::tr("%1 d").arg(fcm_fmt::signed_number(change, 0));
        case fin_an::Unit::Score:
        default:
            return fcm_fmt::signed_number(change, 2);
    }
}

QString fin_missing_text(const QString& note) {
    return note.startsWith(QLatin1String("NM")) ? QStringLiteral("NM") : fcm_fmt::placeholder();
}

QString fin_pct_signed(double fraction, int decimals) {
    return std::isfinite(fraction) ? fcm_fmt::fraction_pct(fraction, decimals, true) : fcm_fmt::placeholder();
}

QString fin_arrow(double change) {
    if (!std::isfinite(change) || change == 0.0)
        return {};
    return change > 0.0 ? QStringLiteral("▲") : QStringLiteral("▼");
}

QString fin_stylesheet() {
    namespace c = ui::colors;
    QString s;
    s += QStringLiteral("QWidget#finRoot{background:%1;}").arg(c::BG_BASE());
    s += QStringLiteral("QWidget#finHeader{background:%1;border:0;border-bottom:1px solid %2;}")
             .arg(c::BG_SURFACE(), c::BORDER_DIM());
    s += QStringLiteral("QWidget#finSectionBar{background:%1;border:0;border-bottom:1px solid %2;}")
             .arg(c::BG_BASE(), c::BORDER_DIM());
    s += QStringLiteral("QFrame#finPanel{background:%1;border:1px solid %2;}").arg(c::BG_SURFACE(), c::BORDER_DIM());
    s += QStringLiteral("QWidget#finPanelBody{background:transparent;border:0;}");
    s += QStringLiteral("QScrollArea#finScroll{background:transparent;border:0;}");
    s += QStringLiteral("QWidget#finScrollBody{background:%1;}").arg(c::BG_BASE());
    s += QStringLiteral("QLabel#finSymbol{color:%1;background:transparent;font-weight:700;font-size:%2px;}")
             .arg(c::AMBER())
             .arg(ui::fonts::px(1));
    s += QStringLiteral("QLabel#finTitle{color:%1;background:transparent;font-weight:600;font-size:%2px;}")
             .arg(c::TEXT_PRIMARY())
             .arg(ui::fonts::px(-1));
    s += QStringLiteral("QLabel#finChip{color:%1;background:%2;border:1px solid %3;padding:1px 6px;font-weight:600;"
                        "font-size:%4px;}")
             .arg(c::TEXT_PRIMARY(), c::BG_RAISED(), c::BORDER_MED())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finMeta{color:%1;background:transparent;font-size:%2px;}")
             .arg(c::TEXT_TERTIARY())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finCaption{color:%1;background:transparent;font-size:%2px;}")
             .arg(c::TEXT_SECONDARY())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finNote{color:%1;background:transparent;font-size:%2px;}")
             .arg(c::TEXT_TERTIARY())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finNotice{color:%1;background:%2;border:1px solid %3;padding:4px 10px;font-size:%4px;}")
             .arg(c::TEXT_SECONDARY(), c::BG_RAISED(), c::BORDER_DIM())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finNotice[tone=\"warn\"]{color:%1;border-color:%1;}").arg(c::WARNING());
    s += QStringLiteral("QLabel#finSection{color:%1;background:transparent;font-weight:700;letter-spacing:1px;"
                        "font-size:%2px;}")
             .arg(c::TEXT_TERTIARY())
             .arg(ui::fonts::px(-4));
    s += QStringLiteral("QLabel#finKey{color:%1;background:transparent;font-size:%2px;}")
             .arg(c::TEXT_SECONDARY())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finVal{color:%1;background:transparent;font-weight:600;font-size:%2px;}")
             .arg(c::TEXT_PRIMARY())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLabel#finBig{color:%1;background:transparent;font-weight:700;font-size:%2px;}")
             .arg(c::TEXT_PRIMARY())
             .arg(ui::fonts::px(10));
    s += QStringLiteral("QLabel#finBigUnit{color:%1;background:transparent;font-weight:600;font-size:%2px;}")
             .arg(c::TEXT_TERTIARY())
             .arg(ui::fonts::px(0));
    s += QStringLiteral("QLabel[tone=\"ok\"]{color:%1;} QLabel[tone=\"danger\"]{color:%2;} QLabel[tone=\"warn\"]{color:%3;}"
                        "QLabel[tone=\"pos\"]{color:%4;} QLabel[tone=\"neg\"]{color:%5;} QLabel[tone=\"dim\"]{color:%6;}"
                        "QLabel[tone=\"accent\"]{color:%7;}")
             .arg(c::SUCCESS(), c::DANGER(), c::WARNING(), c::POSITIVE(), c::NEGATIVE(), c::TEXT_TERTIARY(), c::AMBER());
    s += QStringLiteral("QLabel#finChip[tone=\"ok\"]{border-color:%1;} QLabel#finChip[tone=\"danger\"]{border-color:%2;}"
                        "QLabel#finChip[tone=\"warn\"]{border-color:%3;}")
             .arg(c::SUCCESS(), c::DANGER(), c::WARNING());
    s += QStringLiteral("QPushButton#finBtn{background:transparent;color:%1;border:1px solid %1;padding:3px 10px;"
                        "font-weight:700;font-size:%2px;}")
             .arg(c::AMBER())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QPushButton#finBtn:hover{background:%1;color:%2;} QPushButton#finBtn:disabled{color:%3;border-color:%4;}"
                        "QPushButton#finBtn::menu-indicator{width:0;}")
             .arg(c::AMBER(), c::TEXT_ON_ACCENT(), c::TEXT_DIM(), c::BORDER_MED());
    s += QStringLiteral("QLineEdit#finFilter{background:%1;color:%2;border:1px solid %3;padding:2px 8px;font-size:%4px;}")
             .arg(c::BG_RAISED(), c::TEXT_PRIMARY(), c::BORDER_DIM())
             .arg(ui::fonts::px(-3));
    s += QStringLiteral("QLineEdit#finFilter:focus{border-color:%1;}").arg(c::AMBER());
    s += QStringLiteral("QTableView#finTable{background:%1;alternate-background-color:%2;gridline-color:%3;color:%4;border:0;"
                        "selection-background-color:%5;selection-color:%4;font-size:%6px;}")
             .arg(c::BG_SURFACE(), c::ROW_ALT(), c::BORDER_DIM(), c::TEXT_PRIMARY(), c::BG_HOVER())
             .arg(ui::fonts::px(-2));
    s += QStringLiteral("QTableView#finTable QHeaderView::section{background:%1;color:%2;padding:4px 6px;border:0;"
                        "border-bottom:1px solid %3;border-right:1px solid %3;font-weight:700;font-size:%4px;}")
             .arg(c::BG_RAISED(), c::TEXT_SECONDARY(), c::BORDER_DIM())
             .arg(ui::fonts::px(-4));
    return s + er_ui::stylesheet();
}

FinPanel fin_panel(const QString& title, QWidget* parent, int margin) {
    FinPanel p;
    p.frame = new QFrame(parent);
    p.frame->setObjectName(QStringLiteral("finPanel"));
    auto* outer = new QVBoxLayout(p.frame);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);
    p.header = new ui::PanelHeader(title, p.frame);
    outer->addWidget(p.header);
    auto* body = new QWidget(p.frame);
    body->setObjectName(QStringLiteral("finPanelBody"));
    p.body = new QVBoxLayout(body);
    p.body->setContentsMargins(margin, margin, margin, margin);
    p.body->setSpacing(6);
    outer->addWidget(body, 1);
    return p;
}

QLabel* fin_caption(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QStringLiteral("finCaption"));
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

} // namespace fincept::screens
