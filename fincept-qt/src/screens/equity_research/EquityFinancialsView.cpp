// src/screens/equity_research/EquityFinancialsView.cpp
#include "screens/equity_research/EquityFinancialsView.h"

#include "screens/equity_research/EquityFinancialsOverview.h"
#include "screens/equity_research/EquityFinancialsRatios.h"
#include "screens/equity_research/EquityFinancialsStatements.h"
#include "screens/equity_research/EquityResearchUi.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace {

namespace fv_fmt = fincept::ui::formatting;

const char* const kFvSections[] = {"overview", "income", "balance", "cashflow", "ratios", "peers"};

int fv_statement_of(const QString& key) {
    return key == QLatin1String("income") ? 0 : key == QLatin1String("balance") ? 1 : key == QLatin1String("cashflow") ? 2 : -1;
}

/// Show `page` in `stack`; hidden pages stop counting towards the stack's size hint, so a tall
/// page never leaves a blank band under a short one.
void fv_stack_show(QStackedWidget* stack, QWidget* page) {
    for (int i = 0; i < stack->count(); ++i) {
        QWidget* w = stack->widget(i);
        const QSizePolicy::Policy pol = w == page ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        if (w->sizePolicy().horizontalPolicy() != pol || w->sizePolicy().verticalPolicy() != pol)
            w->setSizePolicy(pol, pol);
    }
    if (stack->currentWidget() != page)
        stack->setCurrentWidget(page);
}

QString fv_unit_key(fin_an::Unit u, const QString& ccy) {
    switch (u) {
        case fin_an::Unit::Money:
        case fin_an::Unit::PerShare:
            return ccy;
        case fin_an::Unit::Shares:
            return QStringLiteral("shares");
        case fin_an::Unit::Multiple:
            return QStringLiteral("x");
        case fin_an::Unit::Days:
            return QStringLiteral("days");
        case fin_an::Unit::Score:
            return QStringLiteral("score");
        case fin_an::Unit::Percent:
        case fin_an::Unit::PctPoints:
        default:
            return QStringLiteral("fraction");
    }
}

} // namespace

EquityFinancialsView::EquityFinancialsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void EquityFinancialsView::build_ui() {
    setObjectName(QStringLiteral("finRoot"));
    setAttribute(Qt::WA_StyledBackground, true);
    er_ui::apply_style(this, []() { return fin_stylesheet(); });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header strip ─────────────────────────────────────────────────────────────────────────
    header_ = new QWidget(this);
    header_->setObjectName(QStringLiteral("finHeader"));
    header_->setAttribute(Qt::WA_StyledBackground, true);
    header_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* hf = new ui::FlowLayout(header_);
    hf->setContentsMargins(10, 6, 10, 6);
    symbol_lbl_ = new QLabel(header_);
    symbol_lbl_->setObjectName(QStringLiteral("finSymbol"));
    name_lbl_ = new QLabel(header_);
    name_lbl_->setObjectName(QStringLiteral("finTitle"));
    unit_chip_ = new QLabel(header_);
    unit_chip_->setObjectName(QStringLiteral("finChip"));
    basis_seg_ = new er_ui::Segmented(header_);
    basis_seg_->add(QStringLiteral("annual"), QString());
    basis_seg_->add(QStringLiteral("quarterly"), QString());
    basis_seg_->add(QStringLiteral("ttm"), QString());
    units_seg_ = new er_ui::Segmented(header_);
    units_seg_->add(QStringLiteral("reported"), QString());
    units_seg_->add(QStringLiteral("common"), QString());
    units_seg_->add(QStringLiteral("growth"), QString());
    meta_lbl_ = new QLabel(header_);
    meta_lbl_->setObjectName(QStringLiteral("finMeta"));
    export_btn_ = new QPushButton(header_);
    export_btn_->setObjectName(QStringLiteral("finBtn"));
    export_btn_->setCursor(Qt::PointingHandCursor);
    auto* menu = new QMenu(export_btn_);
    QAction* save = menu->addAction(QString());
    QAction* copy = menu->addAction(QString());
    save->setObjectName(QStringLiteral("finExportSave"));
    copy->setObjectName(QStringLiteral("finExportCopy"));
    connect(save, &QAction::triggered, this, [this]() { export_to(false); });
    connect(copy, &QAction::triggered, this, [this]() { export_to(true); });
    export_btn_->setMenu(menu);
    for (QWidget* w : {static_cast<QWidget*>(symbol_lbl_), static_cast<QWidget*>(name_lbl_), static_cast<QWidget*>(unit_chip_),
                       static_cast<QWidget*>(basis_seg_), static_cast<QWidget*>(units_seg_), static_cast<QWidget*>(meta_lbl_),
                       static_cast<QWidget*>(export_btn_)})
        hf->addWidget(w);
    hf->setAlignment(symbol_lbl_, Qt::AlignVCenter);
    root->addWidget(header_);
    connect(basis_seg_, &er_ui::Segmented::changed, this, [this](const QString& key) {
        set_basis(key == QLatin1String("quarterly") ? kFinQuarterly : key == QLatin1String("ttm") ? kFinTtm : kFinAnnual);
    });
    connect(units_seg_, &er_ui::Segmented::changed, this, [this](const QString& key) {
        set_units(key == QLatin1String("common") ? kFinCommonSize : key == QLatin1String("growth") ? kFinGrowth : kFinReported);
    });

    // ── Section bar ──────────────────────────────────────────────────────────────────────────
    section_bar_ = new QWidget(this);
    section_bar_->setObjectName(QStringLiteral("finSectionBar"));
    section_bar_->setAttribute(Qt::WA_StyledBackground, true);
    section_bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* sf = new ui::FlowLayout(section_bar_);
    sf->setContentsMargins(10, 6, 10, 6);
    section_seg_ = new er_ui::Segmented(section_bar_);
    for (const char* k : kFvSections)
        section_seg_->add(QLatin1String(k), QString());
    sf->addWidget(section_seg_);
    period_chip_ = new QLabel(section_bar_);
    period_chip_->setObjectName(QStringLiteral("finChip"));
    sf->addWidget(period_chip_);
    latest_btn_ = new QPushButton(section_bar_);
    latest_btn_->setObjectName(QStringLiteral("finBtn"));
    latest_btn_->setCursor(Qt::PointingHandCursor);
    latest_btn_->hide();
    sf->addWidget(latest_btn_);
    root->addWidget(section_bar_);
    connect(section_seg_, &er_ui::Segmented::changed, this, [this](const QString& key) { show_section(key); });
    connect(latest_btn_, &QPushButton::clicked, this, [this]() { select_period(latest()); });

    // ── Notice (refresh failed) ──────────────────────────────────────────────────────────────
    notice_ = new QWidget(this);
    notice_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* nl = new QHBoxLayout(notice_);
    nl->setContentsMargins(8, 6, 8, 0);
    nl->setSpacing(8);
    notice_lbl_ = new QLabel(notice_);
    notice_lbl_->setObjectName(QStringLiteral("finNotice"));
    notice_lbl_->setWordWrap(true);
    notice_lbl_->setProperty("tone", QStringLiteral("warn"));
    nl->addWidget(notice_lbl_, 1);
    notice_btn_ = new QPushButton(notice_);
    notice_btn_->setObjectName(QStringLiteral("finBtn"));
    notice_btn_->setCursor(Qt::PointingHandCursor);
    nl->addWidget(notice_btn_);
    connect(notice_btn_, &QPushButton::clicked, this, &EquityFinancialsView::retry_requested);
    notice_->hide();
    root->addWidget(notice_);

    // ── Body ─────────────────────────────────────────────────────────────────────────────────
    body_ = new QStackedWidget(this);
    skeleton_ = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Result, body_);
    skeleton_->set_kpi_count(6);
    skeleton_->set_row_count(8);
    body_->addWidget(skeleton_);
    empty_ = new ui::EmptyState(body_);
    body_->addWidget(empty_);
    connect(empty_, &ui::EmptyState::primary_clicked, this, &EquityFinancialsView::retry_requested);
    content_ = new QWidget(body_);
    auto* cl = new QVBoxLayout(content_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    kpis_ = new ui::KpiStrip(content_);
    kpis_->set_max_columns(12);
    kpis_->set_min_tile_width(ui::metrics::width(150));
    auto* kpi_wrap = new QWidget(content_);
    auto* kw = new QVBoxLayout(kpi_wrap);
    kw->setContentsMargins(8, 8, 8, 0);
    kw->addWidget(kpis_);
    kpi_wrap->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    cl->addWidget(kpi_wrap);
    sections_ = new QStackedWidget(content_);
    cl->addWidget(sections_, 1);
    body_->addWidget(content_);
    root->addWidget(body_, 1);

    retranslate();
    show_idle();
}

void EquityFinancialsView::retranslate() {
    basis_seg_->set_text(QStringLiteral("annual"), tr("ANNUAL"), tr("Fiscal years"));
    const bool has_q = !bundle_ || bundle_->has_quarters;
    basis_seg_->set_text(QStringLiteral("quarterly"), tr("QUARTERLY"),
                         has_q ? tr("Fiscal quarters (returns and turnover annualised)")
                               : tr("No quarterly statements — the company reports half-yearly, or the source has none"));
    basis_seg_->set_text(QStringLiteral("ttm"), tr("TTM"),
                         tr("Trailing twelve months: each fiscal year, then rolling four-quarter sums (balances at the "
                            "latest quarter)"));
    units_seg_->set_text(QStringLiteral("reported"), tr("REPORTED"), tr("Statement figures as filed, in the scale shown"));
    units_seg_->set_text(QStringLiteral("common"), tr("COMMON SIZE %"),
                         tr("Income and cash flow as % of revenue; balance sheet as % of total assets"));
    units_seg_->set_text(QStringLiteral("growth"), tr("YOY %"), tr("Change vs the same period a year earlier"));
    section_seg_->set_text(QStringLiteral("overview"), tr("OVERVIEW"), tr("Charts: earnings, margins, structure, cash, returns, growth"));
    section_seg_->set_text(QStringLiteral("income"), tr("INCOME STATEMENT"), tr("Income statement"));
    section_seg_->set_text(QStringLiteral("balance"), tr("BALANCE SHEET"), tr("Balance sheet (point in time)"));
    section_seg_->set_text(QStringLiteral("cashflow"), tr("CASH FLOW"), tr("Cash-flow statement"));
    section_seg_->set_text(QStringLiteral("ratios"), tr("RATIOS && QUALITY"),
                           tr("Ratio families, earnings quality, Piotroski, Altman, Beneish"));
    section_seg_->set_text(QStringLiteral("peers"), tr("PEERS"), tr("Margins, growth, returns and leverage vs the peer median"));
    export_btn_->setText(tr("EXPORT ▾"));
    export_btn_->setToolTip(tr("Plain numbers in full units with an ISO currency column"));
    const auto actions = export_btn_->menu()->actions();
    for (QAction* a : actions) {
        if (a->objectName() == QLatin1String("finExportSave"))
            a->setText(tr("Save as CSV…"));
        else if (a->objectName() == QLatin1String("finExportCopy"))
            a->setText(tr("Copy to clipboard (tab-separated)"));
    }
    latest_btn_->setText(tr("LATEST"));
    latest_btn_->setToolTip(tr("Select the latest period"));
    notice_btn_->setText(tr("RETRY"));
}

void EquityFinancialsView::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange) {
        retranslate();
        if (bundle_) {
            render_header();
            render_kpis();
        }
    }
    QWidget::changeEvent(e);
}

void EquityFinancialsView::set_body(QWidget* page) {
    fv_stack_show(body_, page);
    const bool content = page == content_;
    section_bar_->setVisible(content);
    for (QWidget* w : {static_cast<QWidget*>(unit_chip_), static_cast<QWidget*>(basis_seg_), static_cast<QWidget*>(meta_lbl_),
                       static_cast<QWidget*>(export_btn_)})
        w->setVisible(content);
    units_seg_->setVisible(content && fv_statement_of(section_) >= 0);
}

void EquityFinancialsView::show_idle() {
    bundle_.reset();
    notice_->hide();
    symbol_lbl_->setText(QString());
    name_lbl_->setText(tr("FINANCIALS"));
    empty_->apply(ui::EmptyState::no_data_spec(tr("No symbol selected"),
                                               tr("Enter a ticker above to load its income statement, balance sheet and "
                                                  "cash flow with ratios, quality scores and charts.")));
    set_body(empty_);
}

void EquityFinancialsView::show_loading(const QString& symbol) {
    bundle_.reset();
    peers_.clear();
    peers_state_ = 0;
    notice_->hide();
    symbol_lbl_->setText(symbol);
    name_lbl_->setText(QString());
    skeleton_->set_message(tr("Loading statements for %1…").arg(symbol));
    set_body(skeleton_);
}

void EquityFinancialsView::show_error(const QString& symbol, const QString& message, const QString& code) {
    bundle_.reset();
    notice_->hide();
    symbol_lbl_->setText(symbol);
    if (code == QLatin1String("no_data")) {
        empty_->apply(ui::EmptyState::no_data_spec(
            tr("No financial statements for %1").arg(symbol),
            tr("ETFs, indices, currencies and crypto file no income statement, balance sheet or cash flow. For a "
               "company, the source may not cover this listing — try its home-market ticker.")));
    } else {
        empty_->apply(ui::EmptyState::error_spec(message.left(300), tr("Statements could not be loaded"), tr("RETRY")));
    }
    set_body(empty_);
}

void EquityFinancialsView::show_refresh_failed(const QString& message) {
    if (!bundle_)
        return;
    notice_lbl_->setText(tr("Refresh failed — showing the statements as of %1. %2")
                             .arg(bundle_->fetched_at > 0
                                      ? QLocale().toString(QDateTime::fromSecsSinceEpoch(bundle_->fetched_at), QLocale::ShortFormat)
                                      : tr("the last download"),
                                  message.left(200)));
    notice_lbl_->setToolTip(message);
    notice_->show();
}

int EquityFinancialsView::latest() const {
    return bundle_ ? bundle_->an[vs_.basis].latest() : -1;
}

void EquityFinancialsView::set_bundle(const FinBundlePtr& bundle) {
    if (!bundle)
        return;
    const bool same_symbol = bundle_ && bundle_->symbol == bundle->symbol;
    if (!same_symbol) {
        peers_.clear();
        peers_state_ = 0;
    }
    bundle_ = bundle;
    vs_.ccy = fin_reporting_currency(bundle_->info);
    fin_choose_scale(vs_, *bundle_);
    if (!bundle_->has_quarters && vs_.basis == kFinQuarterly)
        vs_.basis = kFinAnnual;
    basis_seg_->set_enabled(QStringLiteral("quarterly"), bundle_->has_quarters);
    basis_seg_->set_current(vs_.basis == kFinQuarterly ? QStringLiteral("quarterly")
                            : vs_.basis == kFinTtm     ? QStringLiteral("ttm")
                                                       : QStringLiteral("annual"));
    const int n = bundle_->an[vs_.basis].count();
    if (!same_symbol || vs_.selected < 0 || vs_.selected >= n)
        vs_.selected = latest();
    ++data_gen_;
    ++sel_gen_;
    if (!bundle_->stale)
        notice_->hide();
    retranslate();
    render_header();
    render_kpis();
    set_body(content_);
    refresh_current(true);
}

void EquityFinancialsView::refresh_format() {
    if (!bundle_)
        return;
    fin_choose_scale(vs_, *bundle_);
    ++data_gen_;
    render_header();
    render_kpis();
    refresh_current(true);
}

void EquityFinancialsView::set_basis(int basis) {
    if (basis == vs_.basis || !bundle_)
        return;
    vs_.basis = basis;
    vs_.selected = latest();
    ++data_gen_;
    ++sel_gen_;
    render_header();
    render_kpis();
    refresh_current(true);
}

void EquityFinancialsView::set_units(int units) {
    if (units == vs_.units)
        return;
    vs_.units = units;
    ++data_gen_;
    if (bundle_)
        refresh_current(true);
}

void EquityFinancialsView::select_period(int index) {
    if (!bundle_ || index < 0 || index >= bundle_->an[vs_.basis].count() || index == vs_.selected)
        return;
    vs_.selected = index;
    ++sel_gen_;
    render_header();
    render_kpis();
    refresh_current(false);
}

void EquityFinancialsView::show_section(const QString& key) {
    bool known = false;
    for (const char* k : kFvSections)
        known = known || key == QLatin1String(k);
    if (!known)
        return;
    section_ = key;
    if (section_seg_->current() != key)
        section_seg_->set_current(key);
    units_seg_->setVisible(bundle_ && fv_statement_of(key) >= 0);
    if (bundle_)
        refresh_current(false);
}

void EquityFinancialsView::render_header() {
    if (!bundle_)
        return;
    const auto& info = bundle_->info;
    const auto& a = bundle_->an[vs_.basis];
    symbol_lbl_->setText(bundle_->symbol);
    name_lbl_->setText(info.company_name);
    const QString ccy = vs_.ccy.isEmpty() ? tr("reporting currency") : vs_.ccy;
    unit_chip_->setText(tr("%1 · %2").arg(ccy, vs_.scale_word.toUpper()));
    QString unit_tip = tr("Statement figures are in %1 %2 (the company's reporting currency); per-share figures in %1.")
                           .arg(ccy, vs_.scale_word);
    if (!info.currency.isEmpty() && !vs_.ccy.isEmpty() && fv_fmt::major_currency(info.currency) != vs_.ccy)
        unit_tip += QStringLiteral(" ") + tr("The shares trade in %1, so per-share figures are not in the quote's currency.")
                                              .arg(info.currency);
    unit_chip_->setToolTip(unit_tip);
    QStringList meta;
    meta << tr("FY ends %1").arg(QLocale().monthName(a.fy_end_month));
    meta << (a.tmpl == fin_model::Template::Bank      ? tr("bank layout")
             : a.tmpl == fin_model::Template::Insurer ? tr("insurer layout")
                                                      : tr("industrial layout"));
    QString asof = tr("Yahoo Finance");
    if (bundle_->fetched_at > 0)
        asof += QStringLiteral(" · ") +
                tr("as of %1").arg(QLocale().toString(QDateTime::fromSecsSinceEpoch(bundle_->fetched_at), QLocale::ShortFormat));
    if (bundle_->stale)
        asof += QStringLiteral(" · ") + tr("saved copy, refreshing…");
    meta << asof;
    meta_lbl_->setText(meta.join(QStringLiteral(" · ")));
    meta_lbl_->setToolTip(tr("Free statements from Yahoo Finance (about four fiscal years and five quarters). TTM = sum "
                             "of the last four quarters for flows, the latest quarter for balances."));
    const int sel = vs_.selected;
    if (sel >= 0 && sel < a.count()) {
        period_chip_->setText(tr("PERIOD  %1").arg(a.periods[sel].label));
        period_chip_->setToolTip(a.periods[sel].tooltip + QStringLiteral("\n") +
                                 tr("Click a bar, a heat-map cell or a column header to select another period"));
        period_chip_->show();
    } else {
        period_chip_->hide();
    }
    latest_btn_->setVisible(sel != latest());
}

void EquityFinancialsView::render_kpis() {
    kpis_->clear();
    if (!bundle_)
        return;
    const auto& a = bundle_->an[vs_.basis];
    const int sel = vs_.selected;
    if (sel < 0 || sel >= a.count())
        return;
    const int ya = a.periods[sel].year_ago;
    const QString vs_label = ya >= 0 ? tr("%1 vs %2").arg(a.periods[sel].label, a.periods[ya].label) : a.periods[sel].label;
    for (const auto& k : fin_an::kpis(a, sel)) {
        ui::StatTile* t = kpis_->add_tile(k.id, k.label);
        t->set_compact(true);
        t->set_value(std::isfinite(k.value) ? fin_value(k.value, k.unit, vs_.ccy) : fin_missing_text(k.note));
        if (std::isfinite(k.change)) {
            const QString text = QStringLiteral("%1 %2").arg(fin_arrow(k.change), fin_change(k.change, k.unit));
            t->set_delta(k.change, text);
            t->set_delta_inverted(k.lower_is_better);
        } else {
            t->clear_delta();
        }
        t->set_sub_label(vs_label);
        t->set_sparkline(k.spark);
        t->set_sparkline_color(ui::StatTile::SparkColor::Accent);
        QString tip = k.tooltip;
        if (std::isfinite(k.prior))
            tip += QStringLiteral("\n") + tr("A year earlier: %1").arg(fin_value(k.prior, k.unit, vs_.ccy));
        if (!k.note.isEmpty())
            tip += QStringLiteral("\n") + k.note;
        t->set_tooltip_text(tip);
    }
}

QWidget* EquityFinancialsView::ensure_page(const QString& key) {
    if (QWidget* p = pages_.value(key))
        return p;
    QWidget* page = nullptr;
    if (key == QLatin1String("overview")) {
        auto* p = new FinOverviewPage(sections_);
        connect(p, &FinOverviewPage::period_selected, this, &EquityFinancialsView::select_period);
        page = p;
    } else if (fv_statement_of(key) >= 0) {
        auto* p = new FinStatementPage(fv_statement_of(key), sections_);
        connect(p, &FinStatementPage::period_selected, this, &EquityFinancialsView::select_period);
        page = p;
    } else if (key == QLatin1String("ratios")) {
        auto* p = new FinRatiosPage(sections_);
        connect(p, &FinRatiosPage::period_selected, this, &EquityFinancialsView::select_period);
        page = p;
    } else {
        auto* p = new FinPeersPage(sections_);
        connect(p, &FinPeersPage::peers_requested, this, &EquityFinancialsView::peers_requested);
        page = p;
    }
    sections_->addWidget(page);
    pages_.insert(key, page);
    return page;
}

void EquityFinancialsView::refresh_current(bool data_changed) {
    Q_UNUSED(data_changed);
    if (!bundle_)
        return;
    QWidget* page = ensure_page(section_);
    fv_stack_show(sections_, page);
    const bool fresh = page_data_gen_.value(section_) != data_gen_;
    const bool moved = page_sel_gen_.value(section_) != sel_gen_;
    if (auto* o = qobject_cast<FinOverviewPage*>(page)) {
        if (fresh)
            o->set_data(bundle_, vs_);
        else if (moved)
            o->set_selected(vs_.selected);
    } else if (auto* s = qobject_cast<FinStatementPage*>(page)) {
        if (fresh)
            s->set_data(bundle_, vs_);
        else if (moved)
            s->set_selected(vs_.selected);
    } else if (auto* r = qobject_cast<FinRatiosPage*>(page)) {
        if (fresh)
            r->set_data(bundle_, vs_);
        else if (moved)
            r->set_selected(vs_.selected);
    } else if (auto* pp = qobject_cast<FinPeersPage*>(page)) {
        if (fresh) {
            pp->set_data(bundle_, vs_);
            pp->set_peers(peers_);
            if (peers_state_ != 0)
                pp->set_state(peers_state_ == 1 ? FinPeersPage::PeerState::Loading : FinPeersPage::PeerState::Failed,
                              peers_message_);
        }
    }
    page_data_gen_.insert(section_, data_gen_);
    page_sel_gen_.insert(section_, sel_gen_);
}

void EquityFinancialsView::set_peers(const QVector<services::equity::PeerData>& peers) {
    peers_ = peers;
    peers_state_ = 0;
    peers_message_.clear();
    if (auto* p = qobject_cast<FinPeersPage*>(pages_.value(QStringLiteral("peers"))))
        p->set_peers(peers_);
}

void EquityFinancialsView::set_peers_state(int state, const QString& message) {
    peers_state_ = state;
    peers_message_ = message;
    if (auto* p = qobject_cast<FinPeersPage*>(pages_.value(QStringLiteral("peers"))))
        p->set_state(state == 1   ? FinPeersPage::PeerState::Loading
                     : state == 2 ? FinPeersPage::PeerState::Failed
                                  : FinPeersPage::PeerState::None,
                     message);
}

void EquityFinancialsView::collect_export(QStringList& headers, QVector<QStringList>& rows) {
    headers.clear();
    rows.clear();
    if (!bundle_)
        return;
    const int st = fv_statement_of(section_);
    if (st >= 0) {
        QWidget* page = ensure_page(section_);
        refresh_current(false);
        if (auto* s = qobject_cast<FinStatementPage*>(page))
            s->export_rows(headers, rows);
        return;
    }
    // Everything of the basis on one period axis: the three statements as reported, then ratios.
    const auto& a = bundle_->an[vs_.basis];
    headers << tr("Statement") << tr("Line item") << tr("Source line") << tr("Unit");
    for (const auto& p : a.periods)
        headers << QStringLiteral("%1 (%2)").arg(p.label, p.end.toString(Qt::ISODate));
    const QString names[] = {tr("income statement"), tr("balance sheet"), tr("cash-flow statement")};
    for (int s = 0; s < 3; ++s) {
        const fin_model::Table& t = bundle_->stmt[vs_.basis][s];
        QVector<int> col(a.count(), -1);
        for (int i = 0; i < a.count(); ++i)
            for (int c = 0; c < t.columns.size(); ++c)
                if (qAbs(t.columns[c].end.daysTo(a.periods[i].end)) <= 12)
                    col[i] = c;
        for (const auto& r : t.rows) {
            if (r.kind == fin_model::Kind::Heading)
                continue;
            QStringList row;
            row << names[s]
                << (r.templated ? QCoreApplication::translate("EquityFinancials", r.label.toUtf8().constData()) : r.label)
                << r.id
                << (r.kind == fin_model::Kind::Shares  ? QStringLiteral("shares")
                    : r.kind == fin_model::Kind::Ratio ? QStringLiteral("fraction")
                                                       : vs_.ccy);
            for (int i = 0; i < a.count(); ++i)
                row << (col[i] >= 0 ? fv_fmt::export_number(r.values.value(col[i], fin_an::kNaN)) : QString());
            rows << row;
        }
    }
    for (const auto& m : a.metrics) {
        if (!m.s.any())
            continue;
        QStringList row;
        row << tr("ratios") << m.label << m.id << fv_unit_key(m.unit, vs_.ccy);
        for (int i = 0; i < a.count(); ++i)
            row << fv_fmt::export_number(m.s.at(i));
        rows << row;
    }
}

void EquityFinancialsView::export_to(bool clipboard) {
    QStringList headers;
    QVector<QStringList> rows;
    collect_export(headers, rows);
    if (headers.isEmpty())
        return;
    const QString basis = vs_.basis == kFinQuarterly ? QStringLiteral("quarterly")
                          : vs_.basis == kFinTtm     ? QStringLiteral("ttm")
                                                     : QStringLiteral("annual");
    if (!clipboard) {
        er_ui::export_csv(this, QStringLiteral("%1_financials_%2_%3").arg(bundle_->symbol, section_, basis), headers, rows);
        return;
    }
    QStringList lines;
    lines << headers.join(QLatin1Char('\t'));
    for (const auto& r : rows) {
        QStringList cells;
        for (QString c : r)
            cells << c.replace(QLatin1Char('\t'), QLatin1Char(' '));
        lines << cells.join(QLatin1Char('\t'));
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

} // namespace fincept::screens
