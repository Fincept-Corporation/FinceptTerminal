#include "screens/derivatives/DerivChainPage.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "core/market/MarketSessions.h"
#include "screens/derivatives/DerivChainModel.h"
#include "services/options/VolSurface.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/InputMemory.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/ThemeIcon.h"
#include "ui/widgets/ProAnalytics.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace mk = services::options::market;
namespace vol = services::options::vol;
using services::options::FreeOptionChainService;

namespace {

/// Undiscounted call delta of a point of the smile (puts mapped to their call delta).
double dcp_call_delta(double k, double iv, double T) {
    if (!(iv > 0) || !(T > 0))
        return std::nan("");
    const double st = iv * std::sqrt(T);
    const double d1 = (-k + 0.5 * st * st) / st;
    return 0.5 * std::erfc(-d1 / std::sqrt(2.0));
}

QString dcp_pct_fmt(double v) {
    return ui::formatting::number(v, 1) + QLatin1Char('%');
}

} // namespace

DerivChainPage::DerivChainPage(DerivContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx), book_("fincept::screens::deriv::DerivChainPage") {
    build();
}

DerivChainPage::~DerivChainPage() {
    for (quint64 sid : service_ids_)
        FreeOptionChainService::instance().cancel(sid);
}

// ── Construction ─────────────────────────────────────────────────────────────

void DerivChainPage::build() {
    // A one-line toolbar strip across the page, then the content with a narrow margin.
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(build_toolbar());
    auto* body = new QVBoxLayout;
    body->setContentsMargins(10, 8, 10, 6);
    body->setSpacing(6);
    root->addLayout(body, 1);

    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, this);
    QWidget* content = build_content(pane_->content_parent());
    pane_->set_content(content);
    pane_->set_requirements({ui::Requirement::python()});
    pane_->set_persist_limit(0); // the chain itself is kept by FreeOptionChainService (memory + disk)
    pane_->set_max_age(5 * 60);  // the feed is ~15 min delayed: refresh on a visit after 5 minutes
    pane_->set_timeout(100);
    pane_->set_auto_delay(150);
    pane_->set_status_label(status_);
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);
    pane_->skeleton()->set_row_count(12);
    pane_->set_start([this](quint64 id) {
        auto& svc = FreeOptionChainService::instance();
        const QString sym = symbol_;
        const bool force = pane_->property("derivForce").toBool();
        pane_->setProperty("derivForce", false);
        static const char* const kSources[] = {"auto", "cboe", "yahoo"};
        const QString src = QString::fromLatin1(kSources[std::clamp(source_->currentIndex(), 0, 2)]);
        const quint64 sid = svc.fetch(sym, src, force, this, [this, id, sym](const FreeOptionChainService::Reply& r) {
            if (!pane_->is_current(id) && r.stage != FreeOptionChainService::Stage::Saved)
                return;
            if (r.stage == FreeOptionChainService::Stage::Failed) {
                service_ids_.remove(id);
                failure_text_ = r.message;
                QString title = tr("Could not load the option chain");
                QString msg = r.message;
                if (r.status == QLatin1String("no_options"))
                    title = tr("No listed options");
                else if (r.status == QLatin1String("bad_symbol"))
                    title = tr("Not an optionable symbol");
                else if (r.status == QLatin1String("feed_unavailable"))
                    msg = tr("The delayed option feeds (Cboe, then Yahoo Finance) did not answer: %1").arg(r.message);
                pane_->fail(id, msg, title);
                return;
            }
            chain_ = r.chain;
            showing_saved_ = r.stage == FreeOptionChainService::Stage::Saved;
            QJsonObject payload{{QStringLiteral("symbol"), FreeOptionChainService::key_of(sym)}};
            if (showing_saved_) {
                pane_->deliver_partial(id, payload);
            } else {
                service_ids_.remove(id);
                pane_->deliver(id, payload);
            }
        });
        service_ids_.insert(id, sid);
    });
    pane_->set_cancel([this](quint64 id) {
        if (service_ids_.contains(id))
            FreeOptionChainService::instance().cancel(service_ids_.take(id));
    });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) {
        const QString key = payload.toObject().value(QStringLiteral("symbol")).toString();
        DerivContext::ChainPtr c = (chain_ && FreeOptionChainService::key_of(chain_->symbol) == key)
                                       ? chain_
                                       : FreeOptionChainService::instance().cached(key);
        if (!c) {
            pane_->invalidate(); // remembered in this session but evicted: load again
            return;
        }
        show_chain(c);
    });
    connect(pane_, &ui::AutoLoadPane::running_changed, this, [this](bool running) {
        refresh_->setEnabled(!running || pane_->state() == ui::AutoLoadPane::State::Content);
    });
    body->addWidget(pane_, 1);

    footer_ = make_note(this, QString());
    body->addWidget(footer_);

    // Inputs remembered across sessions.
    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("chain"), this);
    memory_->track(QStringLiteral("symbol"), symbol_memory_);
    memory_->track(QStringLiteral("source"), source_);
    memory_->track(QStringLiteral("window"), window_);
    memory_->track(QStringLiteral("table_mode"), table_mode_);
    memory_->track(QStringLiteral("expiry"), expiry_memory_);
    memory_->track(QStringLiteral("analysis"), tab_memory_);
    memory_->track(QStringLiteral("smile_x"), smile_x_);
    memory_->track(QStringLiteral("oi_metric"), oi_metric_);
    memory_->restore();
    if (!symbol_memory_->text().trimmed().isEmpty())
        symbol_ = symbol_memory_->text().trimmed();
    symbol_memory_->setText(symbol_);
    sym_box_->set_text(symbol_);
    model_->set_window(window_->value());
    model_->set_mode(table_mode_->currentIndex() == 1 ? DerivChainModel::Mode::Flat : DerivChainModel::Mode::ByExpiry);
    table_->setSortingEnabled(table_mode_->currentIndex() == 1);
    set_analysis(tab_memory_->value());
    retranslate();
    pane_->set_view(FreeOptionChainService::key_of(symbol_));
    // Connected after the restore: a remembered source must not start a fetch while building.
    connect(source_, &QComboBox::currentIndexChanged, this, &DerivChainPage::refresh);
}

QWidget* DerivChainPage::build_toolbar() {
    QHBoxLayout* hl = nullptr;
    QWidget* bar = make_strip(this, &hl);
    hl->setContentsMargins(10, 3, 10, 3);
    sym_box_ = new ui::SymbolSearchBox(bar);
    sym_box_->set_compact(true);
    sym_box_->set_clear_on_pick(false);
    services::ResolveOptions ro;
    ro.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf,
                              core::instrument::AssetClass::Index};
    sym_box_->set_options(ro);
    sym_box_->set_recent_key(QStringLiteral("derivatives.underlying"));
    sym_box_->setMinimumWidth(QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')) * 16);
    connect(sym_box_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& inst) {
        const QString s = inst.symbol.isEmpty() ? inst.display : inst.symbol;
        if (!s.isEmpty())
            set_symbol(s, true);
    });
    connect(sym_box_, &ui::SymbolSearchBox::unresolved, this,
            [this](const QStringList&, const QString& msg) { status_->setText(msg); });
    hl->addWidget(sym_box_);

    source_ = make_combo(bar, {tr("Cboe, Yahoo fallback"), tr("Cboe only"), tr("Yahoo only")});
    book_.combo(source_, {QT_TR_NOOP("Cboe, Yahoo fallback"), QT_TR_NOOP("Cboe only"), QT_TR_NOOP("Yahoo only")});
    source_->setToolTip(tr("Delayed quote source. Cboe's public delayed feed covers every expiry in one request; "
                           "Yahoo Finance needs one request per expiry (the nearest 12 are loaded)."));
    hl->addWidget(source_);
    refresh_ = make_button(bar, tr("REFRESH"), false);
    refresh_->setIcon(ui::theme_icon(ui::ThemeGlyph::Refresh));
    refresh_->setToolTip(tr("Load the delayed chain again (F5)"));
    book_.button(refresh_, QT_TR_NOOP("REFRESH"));
    connect(refresh_, &QPushButton::clicked, this, &DerivChainPage::refresh);
    hl->addWidget(refresh_);
    spot_label_ = new QLabel(bar);
    spot_label_->setObjectName(QStringLiteral("derivPanelTitle"));
    hl->addWidget(spot_label_);
    source_chip_ = new QLabel(bar);
    source_chip_->setObjectName(QStringLiteral("derivChip"));
    source_chip_->setProperty("tone", QStringLiteral("info"));
    hl->addWidget(source_chip_);
    status_ = new QLabel(bar);
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true); // wraps rather than widening the strip past a narrow window
    hl->addWidget(status_, 1);
    // Hand the underlying to Surface Analytics (it takes a symbol through nav.open_symbol and
    // shows its sample surfaces around it; nothing is fetched there until the user asks).
    auto* surface = make_button(bar, tr("SURFACE ANALYTICS"), false);
    book_.button(surface, QT_TR_NOOP("SURFACE ANALYTICS"));
    book_.tooltip(surface, QT_TR_NOOP("Open this underlying in the Surface Analytics tab (its surfaces are samples "
                                      "until you load data there)"));
    connect(surface, &QPushButton::clicked, this, [this] {
        EventBus::instance().publish(QStringLiteral("nav.open_symbol"),
                                     {{QStringLiteral("screen_id"), QStringLiteral("surface_analytics")},
                                      {QStringLiteral("symbol"), FreeOptionChainService::key_of(symbol_)},
                                      {QStringLiteral("asset_class"), QStringLiteral("equity")}});
    });
    hl->addWidget(surface);
    result_bar_ = new DerivResultBar(QStringLiteral("option_chain"), bar);
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return export_image(); });
    result_bar_->set_payload([this] { return report_payload(); });
    hl->addWidget(result_bar_);

    // Hidden inputs that carry remembered values through InputMemory.
    symbol_memory_ = new QLineEdit(bar);
    symbol_memory_->hide();
    expiry_memory_ = new QLineEdit(bar);
    expiry_memory_->hide();
    tab_memory_ = new QSpinBox(bar);
    tab_memory_->setRange(0, 3);
    tab_memory_->hide();
    return bar;
}

QWidget* DerivChainPage::build_content(QWidget* parent) {
    auto* content = new QWidget(parent);
    auto* hl = new QHBoxLayout(content);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(10);

    // Left column: expiries and table options.
    auto* left = new QWidget(content);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(6);
    auto* ecap = make_section(left, tr("EXPIRIES"));
    book_.label(ecap, QT_TR_NOOP("EXPIRIES"));
    ll->addWidget(ecap);
    expiries_ = new QListWidget(left);
    expiries_->setObjectName(QStringLiteral("derivList"));
    expiries_->setUniformItemSizes(true);
    ecap->setBuddy(expiries_);
    connect(expiries_, &QListWidget::currentRowChanged, this, &DerivChainPage::on_expiry_changed);
    ll->addWidget(expiries_, 1);
    auto* wcap = new QLabel(tr("STRIKES EACH SIDE OF ATM"), left);
    wcap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(wcap, QT_TR_NOOP("STRIKES EACH SIDE OF ATM"));
    window_ = make_int_spin(left, 20, 0, 500);
    window_->setSpecialValueText(tr("all"));
    window_->setToolTip(tr("0 = every strike"));
    wcap->setBuddy(window_);
    connect(window_, &QSpinBox::valueChanged, this, [this](int v) {
        model_->set_window(v);
        update_oi();
    });
    ll->addWidget(wcap);
    ll->addWidget(window_);
    auto* tcap = new QLabel(tr("TABLE"), left);
    tcap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(tcap, QT_TR_NOOP("TABLE"));
    table_mode_ = make_combo(left, {tr("Calls | strike | puts"), tr("All contracts (sortable)")});
    book_.combo(table_mode_, {QT_TR_NOOP("Calls | strike | puts"), QT_TR_NOOP("All contracts (sortable)")});
    tcap->setBuddy(table_mode_);
    connect(table_mode_, &QComboBox::currentIndexChanged, this, [this](int i) {
        model_->set_mode(i == 1 ? DerivChainModel::Mode::Flat : DerivChainModel::Mode::ByExpiry);
        table_->setSortingEnabled(i == 1);
        if (i == 0 && model_->atm_row() >= 0)
            table_->scrollTo(model_->index(model_->atm_row(), DerivChainModel::kStrike), QAbstractItemView::PositionAtCenter);
    });
    ll->addWidget(tcap);
    ll->addWidget(table_mode_);
    auto* hint = make_note(left, tr("Double-click a contract to price it; right-click to add it to a strategy."));
    book_.label(hint, QT_TR_NOOP("Double-click a contract to price it; right-click to add it to a strategy."));
    ll->addWidget(hint);
    left->setMaximumWidth(QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')) * 16);
    hl->addWidget(left);

    // Right: the table over the analysis views.
    auto* split = new QSplitter(Qt::Vertical, content);
    split->setChildrenCollapsible(false);
    table_ = new QTableView(split);
    model_ = new DerivChainModel(table_);
    table_->setModel(model_);
    dense_table(table_); // zebra, row height from the table font (re-fitted on a font change)
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setMinimumSectionSize(
        QFontMetrics(ui::fonts::font(-4)).horizontalAdvance(QStringLiteral("00000.00")) + 12);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QTableView::customContextMenuRequested, this, &DerivChainPage::table_menu);
    connect(table_, &QTableView::activated, this, &DerivChainPage::table_activated);
    split->addWidget(table_);

    auto* lower = new QWidget(split);
    auto* lv = new QVBoxLayout(lower);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(4);
    // The analysis views' tab strip (one line, hairline under it).
    QHBoxLayout* tl = nullptr;
    QWidget* tabs = make_strip(lower, &tl);
    tl->setContentsMargins(0, 0, 4, 0);
    tl->setSpacing(2);
    static const char* const kTabs[] = {QT_TR_NOOP("SMILE"), QT_TR_NOOP("TERM & EXPECTED MOVE"), QT_TR_NOOP("SURFACE"),
                                        QT_TR_NOOP("OPEN INTEREST")};
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(ui::literal_ampersands(tr(kTabs[i])), tabs); // '&' shown, not a mnemonic
        b->setObjectName(QStringLiteral("derivSubBtn"));
        b->setCursor(Qt::PointingHandCursor);
        book_.button(b, kTabs[i]);
        connect(b, &QPushButton::clicked, this, [this, i] { set_analysis(i); });
        tl->addWidget(b);
        tab_btns_.append(b);
    }
    tl->addStretch(1);
    // Pro analytics: the server's positioning report for the underlying on screen (the
    // confirmed symbol_, as the page's own actions use it), in its own window. Opens on
    // click only; hidden while the server does not offer Pro analytics.
    tl->addWidget(ui::pro::make_button(ui::pro::Kind::OptionsPositioning, [this]() {
        ui::pro::Request r;
        r.symbol = symbol_;
        return r;
    }, tabs));
    lv->addWidget(tabs);
    analysis_ = new QStackedWidget(lower);
    analysis_->addWidget(build_smile(analysis_));
    analysis_->addWidget(build_term(analysis_));
    analysis_->addWidget(build_surface(analysis_));
    analysis_->addWidget(build_oi(analysis_));
    lv->addWidget(analysis_, 1);
    split->addWidget(lower);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    hl->addWidget(split, 1);
    return content;
}

QWidget* DerivChainPage::build_smile(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    smile_kpis_ = new ui::KpiStrip(w);
    vl->addWidget(smile_kpis_);
    auto* row = new QWidget(w);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    auto* cap = new QLabel(tr("X AXIS"), row);
    cap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(cap, QT_TR_NOOP("X AXIS"));
    smile_x_ = make_combo(row, {tr("Strike"), tr("Log-moneyness ln(K/F)"), tr("Call delta")});
    book_.combo(smile_x_, {QT_TR_NOOP("Strike"), QT_TR_NOOP("Log-moneyness ln(K/F)"), QT_TR_NOOP("Call delta")});
    cap->setBuddy(smile_x_);
    connect(smile_x_, &QComboBox::currentIndexChanged, this, [this] { update_smile(); });
    rl->addWidget(cap);
    rl->addWidget(smile_x_);
    rl->addStretch(1);
    vl->addWidget(row);
    smile_chart_ = make_xy_chart(w, tr("IMPLIED VOLATILITY SMILE"), QString(), {});
    vl->addWidget(smile_chart_, 1);
    smile_note_ = make_note(w, QString());
    vl->addWidget(smile_note_);
    return w;
}

QWidget* DerivChainPage::build_term(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    term_kpis_ = new ui::KpiStrip(w);
    vl->addWidget(term_kpis_);
    auto* split = new QSplitter(Qt::Horizontal, w);
    term_chart_ = make_xy_chart(split, tr("ATM IMPLIED VOLATILITY TERM STRUCTURE"), QString(), {});
    split->addWidget(term_chart_);
    move_table_ = new QTableView(split);
    move_model_ = new QStandardItemModel(move_table_);
    move_table_->setModel(move_model_);
    dense_table(move_table_);
    move_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    move_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    split->addWidget(move_table_);
    vl->addWidget(split, 1);
    return w;
}

QWidget* DerivChainPage::build_surface(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    surface_ = new ui::MatrixHeatmap(w);
    surface_->set_sequential_scale();
    surface_->set_value_formatter([](double v) { return dcp_pct_fmt(v); });
    vl->addWidget(surface_, 1);
    surface_note_ = make_note(w, QString());
    vl->addWidget(surface_note_);
    return w;
}

QWidget* DerivChainPage::build_oi(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);
    oi_kpis_ = new ui::KpiStrip(w);
    vl->addWidget(oi_kpis_);
    auto* row = new QWidget(w);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    auto* cap = new QLabel(tr("SHOW"), row);
    cap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(cap, QT_TR_NOOP("SHOW"));
    oi_metric_ = make_combo(row, {tr("Open interest"), tr("Volume")});
    book_.combo(oi_metric_, {QT_TR_NOOP("Open interest"), QT_TR_NOOP("Volume")});
    cap->setBuddy(oi_metric_);
    connect(oi_metric_, &QComboBox::currentIndexChanged, this, [this] { update_oi(); });
    rl->addWidget(cap);
    rl->addWidget(oi_metric_);
    rl->addStretch(1);
    vl->addWidget(row);
    oi_chart_ = new ui::BarChart(w);
    oi_chart_->set_value_formatter([](double v) { return ui::formatting::compact(v); });
    vl->addWidget(oi_chart_, 1);
    return w;
}

// ── Actions ──────────────────────────────────────────────────────────────────

void DerivChainPage::refresh() {
    if (!pane_)
        return;
    pane_->setProperty("derivForce", true);
    pane_->run();
}

void DerivChainPage::open_symbol(const QString& symbol) {
    if (!symbol.isEmpty() && FreeOptionChainService::key_of(symbol) != FreeOptionChainService::key_of(symbol_))
        set_symbol(symbol, true);
}

void DerivChainPage::set_symbol(const QString& symbol, bool run) {
    symbol_ = symbol.trimmed();
    symbol_memory_->setText(symbol_);
    memory_->save_now();
    expiry_memory_->clear();
    sym_box_->set_text(symbol_);
    pane_->set_view(FreeOptionChainService::key_of(symbol_));
    if (run)
        pane_->run();
}

void DerivChainPage::set_analysis(int index) {
    index = std::clamp(index, 0, 3);
    tab_memory_->setValue(index);
    analysis_->setCurrentIndex(index);
    for (int i = 0; i < tab_btns_.size(); ++i) {
        tab_btns_[i]->setProperty("active", i == index);
        tab_btns_[i]->style()->unpolish(tab_btns_[i]);
        tab_btns_[i]->style()->polish(tab_btns_[i]);
    }
}

void DerivChainPage::show_chain(const DerivContext::ChainPtr& chain) {
    if (!chain || !chain->ok())
        return;
    chain_ = chain;
    if (ctx_)
        ctx_->set_chain(chain);
    // Expiries (keep the remembered date when it is still listed).
    const QString want = expiry_memory_->text();
    int pick = -1;
    {
        QSignalBlocker block(expiries_);
        expiries_->clear();
        for (int e = 0; e < chain->expiries.size(); ++e) {
            const mk::FreeExpiry& x = chain->expiries[e];
            auto* item = new QListWidgetItem(tr("%1   %2d").arg(x.date.toString(Qt::ISODate)).arg(x.days), expiries_);
            item->setData(Qt::UserRole, x.date.toString(Qt::ISODate));
            item->setToolTip(tr("ATM IV %1 · forward %2").arg(vol_pct(x.smile.atm_iv), num(x.forward, 2)));
            if (x.date.toString(Qt::ISODate) == want)
                pick = e;
        }
        if (pick < 0) { // default: the first expiry at least ~3 weeks out (a meaningful smile)
            pick = 0;
            for (int e = 0; e < chain->expiries.size(); ++e)
                if (chain->expiries[e].days >= 20) {
                    pick = e;
                    break;
                }
        }
        expiries_->setCurrentRow(pick);
    }
    model_->set_chain(chain);
    on_expiry_changed(pick);
    update_term();
    update_surface();
    update_header();
    update_notice();
}

void DerivChainPage::on_expiry_changed(int row) {
    if (!chain_ || row < 0 || row >= chain_->expiries.size())
        return;
    expiry_ = row;
    expiry_memory_->setText(chain_->expiries[row].date.toString(Qt::ISODate));
    model_->set_expiry(row);
    if (model_->mode() == DerivChainModel::Mode::ByExpiry && model_->atm_row() >= 0)
        table_->scrollTo(model_->index(model_->atm_row(), DerivChainModel::kStrike), QAbstractItemView::PositionAtCenter);
    update_smile();
    update_oi();
}

void DerivChainPage::update_header() {
    if (!chain_)
        return;
    QString spot = tr("%1 %2").arg(chain_->symbol, num(chain_->spot, 2));
    if (std::isfinite(chain_->change_pct))
        spot += QStringLiteral(" (%1)").arg(ui::formatting::pct(chain_->change_pct));
    spot_label_->setText(spot);
    static const QTimeZone ny("America/New_York");
    const QDateTime qt = chain_->quote_time.isValid() ? chain_->quote_time : chain_->fetched_at;
    const QString when = qt.isValid() ? (ny.isValid() ? qt.toTimeZone(ny) : qt).toString(QStringLiteral("d MMM HH:mm"))
                                      : QString();
    source_chip_->setText(tr("DELAYED · %1 · AS OF %2 NEW YORK").arg(chain_->source_label.toUpper(), when));
    source_chip_->setToolTip(tr("Quotes are delayed (about 15 minutes for Cboe). Fetched %1.")
                                 .arg(chain_->fetched_at.toLocalTime().toString(QStringLiteral("d MMM HH:mm"))));
}

void DerivChainPage::update_notice() {
    if (!chain_)
        return;
    QStringList parts;
    ui::EmptyState::Tone tone = ui::EmptyState::Tone::Info;
    if (chain_->source == QLatin1String("yahoo") && !chain_->fallback_reason.isEmpty()) {
        parts << tr("Cboe was unavailable (%1): showing Yahoo Finance delayed quotes.").arg(chain_->fallback_reason);
        tone = ui::EmptyState::Tone::Warning;
    }
    if (chain_->partial_total > chain_->partial_loaded && chain_->partial_loaded > 0)
        parts << tr("Yahoo Finance: %1 of %2 expiries loaded (one request per expiry).")
                     .arg(chain_->partial_loaded)
                     .arg(chain_->partial_total);
    if (const auto* venue = core::market::venue_by_code(QStringLiteral("NYSE"))) {
        const auto st = core::market::session_status(*venue, QDateTime::currentDateTimeUtc()).state;
        if (st != core::market::SessionState::Open)
            parts << tr("US market closed: quotes are from the last session.");
    }
    if (showing_saved_)
        parts << tr("Showing the chain saved earlier while a fresh one loads.");
    if (parts.isEmpty())
        pane_->clear_notice();
    else
        pane_->set_notice(parts.join(QLatin1Char(' ')), tone);
    footer_->setText(tr("Delayed quotes (about 15 minutes) · source: %1 · IV and Greeks computed locally from the "
                        "bid/ask mid (Black-Scholes-Merton on the forward implied by put-call parity; a European "
                        "approximation of American contracts). %2")
                         .arg(chain_->source_label, chain_->notes.join(QLatin1Char(' '))));
}

void DerivChainPage::update_smile() {
    if (!chain_ || expiry_ >= chain_->expiries.size())
        return;
    const mk::FreeExpiry& x = chain_->expiries[expiry_];
    const int mode = smile_x_->currentIndex();
    const QVector<vol::SmileQuote> pts = mk::smile_quotes(*chain_, expiry_);
    auto xval = [&](double k, double iv) {
        if (mode == 0)
            return x.forward * std::exp(k);
        if (mode == 1)
            return k;
        return dcp_call_delta(k, iv, x.years);
    };
    QVector<QPointF> mkt, fit;
    for (const vol::SmileQuote& s : pts)
        mkt.append({xval(s.k, s.iv), s.iv * 100.0});
    if (x.svi.ok) {
        const double lo = x.k_lo - 0.05, hi = x.k_hi + 0.05;
        for (int i = 0; i <= 120; ++i) {
            const double k = lo + (hi - lo) * i / 120.0;
            const double v = vol::svi_vol(x.svi.params, k, x.years);
            if (v > 0)
                fit.append({xval(k, v), v * 100.0});
        }
    }
    auto by_x = [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); };
    std::sort(mkt.begin(), mkt.end(), by_x);
    std::sort(fit.begin(), fit.end(), by_x);
    smile_chart_->clear();
    smile_chart_->set_title(tr("IMPLIED VOLATILITY SMILE · %1 (%2 DAYS)").arg(x.date.toString(Qt::ISODate)).arg(x.days));
    smile_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, dcp_pct_fmt);
    static const char* const kAxis[] = {QT_TR_NOOP("Strike"), QT_TR_NOOP("k"), QT_TR_NOOP("Call delta")};
    const QString axis = tr(kAxis[std::clamp(mode, 0, 2)]);
    const int dec = mode == 0 ? 2 : 3;
    smile_chart_->set_time_formatter([axis, dec](double v) { return axis + QLatin1Char(' ') + num(v, dec); });
    smile_chart_->set_tick_time_formatter([dec](double v) { return num(v, dec); });
    if (mkt.size() >= 2) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("Market (out-of-the-money mids)");
        s.points = mkt;
        s.rebase = false;
        smile_chart_->add_series(s);
    }
    if (fit.size() >= 2) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("SVI fit");
        s.points = fit;
        s.dashed = true;
        s.rebase = false;
        smile_chart_->add_series(s);
    }
    if (x.smile.atm_iv > 0)
        smile_chart_->add_price_marker(xval(0.0, x.smile.atm_iv), x.smile.atm_iv * 100.0, ui::ChartMarkerKind::Custom,
                                       tr("ATM"), tr("Forward %1, ATM IV %2").arg(num(x.forward, 2), vol_pct(x.smile.atm_iv)));
    smile_chart_->set_state(mkt.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty,
                            tr("Too few two-sided quotes for a smile on this expiry."));

    smile_kpis_->clear();
    auto tile = [this](const QString& label, const QString& value, const QString& sub = QString()) {
        ui::StatTile* t = smile_kpis_->add_tile(label);
        t->set_value(value);
        t->set_compact(true);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        return t;
    };
    tile(tr("ATM IV"), vol_pct(x.smile.atm_iv), tr("at the forward"));
    tile(tr("25Δ RISK REVERSAL"), x.smile.has_25d ? ui::formatting::pp(x.smile.rr25 * 100.0) : QStringLiteral("—"),
         tr("call − put vol"));
    tile(tr("25Δ BUTTERFLY"), x.smile.has_25d ? ui::formatting::pp(x.smile.bf25 * 100.0) : QStringLiteral("—"),
         tr("wings − ATM"));
    if (std::isfinite(x.straddle))
        tile(tr("EXPECTED MOVE"), QStringLiteral("± %1").arg(num(x.straddle, 2)),
             tr("ATM straddle · %1 of spot").arg(pct_points(100.0 * x.straddle / chain_->spot)));
    tile(tr("1σ MOVE"), QStringLiteral("± %1").arg(num(x.smile.move_1sd, 2)), tr("spot × ATM IV × √T"));
    ui::StatTile* fit_tile = tile(tr("SVI FIT"), x.svi.ok ? ui::formatting::pp(x.svi.rmse_vol * 100.0, 2, false)
                                                         : tr("no fit"),
                                  x.svi.ok ? tr("RMSE, %1 quotes").arg(x.svi.points)
                                           : QString::fromStdString(x.svi.message));
    if (!x.svi.ok || x.svi.rmse_vol > 0.02)
        fit_tile->set_tone(ui::StatTile::Tone::Warning);
    ui::StatTile* bfly = tile(tr("BUTTERFLY"), !x.svi.ok ? QStringLiteral("—") : x.butterfly.ok ? tr("none") : tr("ARBITRAGE"),
                              !x.svi.ok ? tr("needs a fit")
                              : x.butterfly.ok ? tr("density ≥ 0 on the quoted range")
                                               : tr("negative density near k = %1").arg(num(x.butterfly.worst_k, 2)));
    if (x.svi.ok && !x.butterfly.ok)
        bfly->set_tone(ui::StatTile::Tone::Warning);
    ui::StatTile* cal = tile(tr("CALENDAR"), !x.calendar_checked ? QStringLiteral("—") : x.calendar.ok ? tr("none") : tr("ARBITRAGE"),
                             !x.calendar_checked ? tr("vs the previous expiry")
                             : x.calendar.ok ? tr("total variance rises with time")
                                             : tr("variance falls vs the previous expiry"));
    if (x.calendar_checked && !x.calendar.ok)
        cal->set_tone(ui::StatTile::Tone::Warning);
    smile_note_->setText(x.svi.ok ? tr("Raw SVI w(k) = a + b(ρ(k − m) + √((k − m)² + σ²)): a %1, b %2, ρ %3, m %4, σ %5. "
                                       "Forward %6 (%7), rate %8, implied carry %9.")
                                        .arg(num(x.svi.params.a, 4), num(x.svi.params.b, 4), num(x.svi.params.rho, 3),
                                             num(x.svi.params.m, 3), num(x.svi.params.sigma, 3), num(x.forward, 2),
                                             x.forward_from_parity ? tr("put-call parity") : tr("spot at the risk-free rate"),
                                             vol_pct(x.rate), vol_pct(x.carry_q))
                                  : tr("No SVI fit for this expiry: %1.").arg(QString::fromStdString(x.svi.message)));
}

void DerivChainPage::update_term() {
    if (!chain_)
        return;
    QVector<QPointF> atm, fwd;
    for (const mk::FreeExpiry& x : chain_->expiries) {
        if (x.smile.atm_iv > 0)
            atm.append({static_cast<double>(x.days), x.smile.atm_iv * 100.0});
        if (std::isfinite(x.forward_vol) && x.forward_vol > 0)
            fwd.append({static_cast<double>(x.days), x.forward_vol * 100.0});
    }
    term_chart_->clear();
    term_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, dcp_pct_fmt);
    term_chart_->set_time_formatter([](double v) { return DerivChainPage::tr("%1 days").arg(num(v, 0)); });
    term_chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    if (atm.size() >= 2) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("ATM implied vol");
        s.points = atm;
        s.rebase = false;
        term_chart_->add_series(s);
    }
    if (fwd.size() >= 2) {
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = tr("Forward vol between expiries");
        s.points = fwd;
        s.dashed = true;
        s.rebase = false;
        term_chart_->add_series(s);
    }
    term_chart_->set_state(atm.size() >= 2 ? ui::ChartState::Ready : ui::ChartState::Empty,
                           tr("Too few expiries with an ATM volatility."));

    auto at_days = [&atm](double d) {
        std::vector<double> xs, ys;
        for (const QPointF& p : atm) {
            xs.push_back(p.x());
            ys.push_back(p.y());
        }
        return vol::interp_linear(xs, ys, d, false);
    };
    int calendar_flags = 0;
    for (const mk::FreeExpiry& x : chain_->expiries)
        if ((x.calendar_checked && !x.calendar.ok) || x.forward_vol == -1.0)
            ++calendar_flags;
    term_kpis_->clear();
    auto tile = [this](const QString& label, const QString& value, const QString& sub = QString()) {
        ui::StatTile* t = term_kpis_->add_tile(label);
        t->set_value(value);
        t->set_compact(true);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        return t;
    };
    if (std::isfinite(chain_->iv30_pct))
        tile(tr("CBOE IV30"), pct_points(chain_->iv30_pct), tr("30-day implied vol of %1").arg(chain_->symbol));
    const double v30 = at_days(30), v90 = at_days(90);
    tile(tr("ATM IV 30D"), std::isfinite(v30) ? dcp_pct_fmt(v30) : QStringLiteral("—"), tr("interpolated"));
    tile(tr("ATM IV 90D"), std::isfinite(v90) ? dcp_pct_fmt(v90) : QStringLiteral("—"), tr("interpolated"));
    tile(tr("TERM SLOPE"), std::isfinite(v30) && std::isfinite(v90) ? ui::formatting::pp(v90 - v30) : QStringLiteral("—"),
         tr("90D − 30D"));
    ui::StatTile* ct = tile(tr("CALENDAR FLAGS"), QString::number(calendar_flags), tr("expiries with falling variance"));
    if (calendar_flags > 0)
        ct->set_tone(ui::StatTile::Tone::Warning);

    move_model_->clear();
    move_model_->setHorizontalHeaderLabels({tr("EXPIRY"), tr("DAYS"), tr("ATM IV"), tr("STRADDLE"), tr("MOVE %"),
                                            tr("1σ MOVE"), tr("RANGE (STRADDLE)")});
    for (const mk::FreeExpiry& x : chain_->expiries) {
        QList<QStandardItem*> row;
        auto cell = [](const QString& t) {
            auto* it = new QStandardItem(t);
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return it;
        };
        row << cell(x.date.toString(Qt::ISODate)) << cell(QString::number(x.days)) << cell(vol_pct(x.smile.atm_iv));
        const bool st = std::isfinite(x.straddle);
        row << cell(st ? num(x.straddle, 2) : QStringLiteral("—"))
            << cell(st ? pct_points(100.0 * x.straddle / chain_->spot) : QStringLiteral("—"))
            << cell(num(x.smile.move_1sd, 2))
            << cell(st ? QStringLiteral("%1 – %2").arg(num(chain_->spot - x.straddle, 2), num(chain_->spot + x.straddle, 2))
                       : QStringLiteral("—"));
        move_model_->appendRow(row);
    }
}

void DerivChainPage::update_surface() {
    if (!chain_)
        return;
    QStringList rows, cols;
    for (double m : chain_->surface_moneyness)
        rows << QStringLiteral("%1%").arg(num(m * 100.0, 1));
    for (const mk::FreeExpiry& x : chain_->expiries)
        cols << QStringLiteral("%1 (%2d)").arg(x.date.toString(QStringLiteral("d MMM yy"))).arg(x.days);
    QVector<QVector<double>> vals;
    for (const QVector<double>& r : chain_->surface_iv) {
        QVector<double> v;
        for (double d : r)
            v.append(std::isfinite(d) ? d * 100.0 : std::nan(""));
        vals.append(v);
    }
    surface_->set_matrix(rows, cols, vals);
    surface_note_->setText(tr("Rows: strike as a share of each expiry's forward (K/F). Cells: SVI where it fits within "
                              "3 vol points, otherwise the market smile; blank where nothing is quoted."));
}

void DerivChainPage::update_oi() {
    if (!chain_ || expiry_ >= chain_->expiries.size())
        return;
    const mk::FreeExpiry& x = chain_->expiries[expiry_];
    const bool volume = oi_metric_->currentIndex() == 1;
    const QVector<double> ks = mk::strikes(*chain_, expiry_);
    int atm = 0;
    for (int i = 1; i < ks.size(); ++i)
        if (std::fabs(ks[i] - x.atm_strike) < std::fabs(ks[atm] - x.atm_strike))
            atm = i;
    const int w = std::min(window_->value() > 0 ? window_->value() : 20, 25);
    const int lo = std::max(0, atm - w), hi = std::min(static_cast<int>(ks.size()), atm + w + 1);
    QStringList cats;
    QVector<double> calls, puts;
    for (int i = lo; i < hi; ++i) {
        cats << num(ks[i], 2);
        calls.append(0);
        puts.append(0);
    }
    for (int i = x.begin; i < x.end; ++i) {
        const mk::FreeOptionQuote& q = chain_->quotes[i];
        const auto it = std::lower_bound(ks.begin() + lo, ks.begin() + hi, q.strike);
        if (it == ks.begin() + hi || *it != q.strike)
            continue;
        const int j = static_cast<int>(it - (ks.begin() + lo));
        const double v = static_cast<double>(volume ? q.volume : q.open_interest);
        (q.is_call ? calls[j] : puts[j]) += v;
    }
    oi_chart_->set_categories(cats);
    oi_chart_->set_series({{tr("Calls"), calls, QColor()}, {tr("Puts"), puts, QColor()}});
    oi_kpis_->clear();
    auto tile = [this](const QString& label, const QString& value, const QString& sub = QString()) {
        ui::StatTile* t = oi_kpis_->add_tile(label);
        t->set_value(value);
        t->set_compact(true);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        return t;
    };
    tile(tr("PUT/CALL OI"), x.call_oi > 0 ? ui::formatting::ratio(x.put_oi / x.call_oi) : QStringLiteral("—"), tr("this expiry"));
    tile(tr("PUT/CALL VOLUME"), x.call_volume > 0 ? ui::formatting::ratio(x.put_volume / x.call_volume) : QStringLiteral("—"),
         tr("this expiry"));
    tile(tr("MAX PAIN"), std::isfinite(x.max_pain) ? num(x.max_pain, 2) : QStringLiteral("—"),
         tr("least paid out to holders at expiry"));
    tile(tr("OPEN INTEREST"), ui::formatting::compact(x.call_oi + x.put_oi), tr("contracts, this expiry"));
    double c_all = 0, p_all = 0;
    for (const mk::FreeExpiry& e : chain_->expiries) {
        c_all += e.call_oi;
        p_all += e.put_oi;
    }
    tile(tr("PUT/CALL OI (ALL)"), c_all > 0 ? ui::formatting::ratio(p_all / c_all) : QStringLiteral("—"), tr("every expiry"));
}

// ── Table actions ────────────────────────────────────────────────────────────

ContractRef DerivChainPage::contract_ref(int qi) const {
    ContractRef c;
    if (!chain_ || qi < 0 || qi >= chain_->quotes.size())
        return c;
    const mk::FreeOptionQuote& q = chain_->quotes[qi];
    const mk::FreeExpiry& x = chain_->expiries[q.expiry];
    c.symbol = chain_->symbol;
    c.expiry_label = x.date.toString(Qt::ISODate);
    c.spot = chain_->spot;
    c.strike = q.strike;
    c.years = x.years;
    c.is_call = q.is_call;
    c.iv = std::isfinite(q.iv) ? q.iv : (std::isfinite(q.feed_iv) ? q.feed_iv : x.smile.atm_iv);
    c.rate = x.rate;
    c.q = x.carry_q;
    c.mid = std::isfinite(q.mid) ? q.mid : (std::isfinite(q.last) ? q.last : 0.0);
    return c;
}

LegView DerivChainPage::leg_view(int qi, double quantity) const {
    LegView v;
    const ContractRef c = contract_ref(qi);
    v.leg.kind = c.is_call ? services::options::strategy::LegKind::Call : services::options::strategy::LegKind::Put;
    v.leg.quantity = quantity;
    v.leg.strike = c.strike;
    v.leg.expiry_years = c.years;
    v.leg.entry_price = c.mid;
    v.leg.iv = c.iv > 0 ? c.iv : 0.2;
    v.leg.expiry_index = chain_ ? chain_->quotes[qi].expiry : -1;
    v.expiry_label = c.expiry_label;
    return v;
}

void DerivChainPage::table_activated(const QModelIndex& index) {
    const int qi = model_->quote_at(index.row(), index.column());
    if (qi >= 0 && ctx_)
        emit ctx_->price_contract_requested(contract_ref(qi));
}

void DerivChainPage::table_menu(const QPoint& pos) {
    const QModelIndex index = table_->indexAt(pos);
    QMenu menu(this);
    if (index.isValid() && chain_ && ctx_) {
        const int row = index.row();
        int call_q = -1, put_q = -1;
        if (model_->mode() == DerivChainModel::Mode::ByExpiry) {
            call_q = model_->quote_at(row, DerivChainModel::kCMid);
            put_q = model_->quote_at(row, DerivChainModel::kPMid);
        } else {
            const int qi = model_->quote_at(row, 0);
            (chain_->quotes[qi].is_call ? call_q : put_q) = qi;
        }
        auto add = [&](int qi, const QString& kind) {
            if (qi < 0)
                return;
            const QString k = num(chain_->quotes[qi].strike, 2);
            menu.addAction(tr("Price the %1 %2 in the pricer").arg(k, kind), this,
                           [this, qi] { emit ctx_->price_contract_requested(contract_ref(qi)); });
            menu.addAction(tr("Buy the %1 %2 → strategy").arg(k, kind), this,
                           [this, qi] { emit ctx_->legs_requested({leg_view(qi, 1)}); });
            menu.addAction(tr("Sell the %1 %2 → strategy").arg(k, kind), this,
                           [this, qi] { emit ctx_->legs_requested({leg_view(qi, -1)}); });
            menu.addAction(tr("Scenarios on the %1 %2").arg(k, kind), this,
                           [this, qi] { emit ctx_->scenarios_contract_requested(contract_ref(qi)); });
            menu.addAction(tr("Add the %1 %2 to the book…").arg(k, kind), this, [this, qi] {
                const ContractRef c = contract_ref(qi);
                services::options::book::BookPosition p;
                p.kind = QStringLiteral("option");
                p.underlying = c.symbol;
                p.currency = chain_ ? chain_->currency : QStringLiteral("USD");
                p.multiplier = 100;
                p.entry_date = QDate::currentDate();
                services::options::book::BookLeg l;
                l.type = c.is_call ? services::options::book::LegType::Call : services::options::book::LegType::Put;
                l.quantity = 1;
                l.strike = c.strike;
                l.expiry = QDate::fromString(c.expiry_label, Qt::ISODate);
                l.entry_price = c.mid;
                l.entry_iv = c.iv;
                p.legs = {l};
                emit ctx_->book_add_requested(p);
            });
        };
        add(call_q, tr("call"));
        if (call_q >= 0 && put_q >= 0)
            menu.addSeparator();
        add(put_q, tr("put"));
        menu.addSeparator();
    }
    ui::ChartExport::MenuSpec spec;
    spec.suggested_name = QStringLiteral("option_chain");
    spec.csv = [this] { return export_table(); };
    spec.offer_image = false;
    ui::ChartExport::populate_menu(&menu, table_, spec);
    menu.exec(table_->viewport()->mapToGlobal(pos));
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivChainPage::export_table() const {
    ui::ChartExport::Table t;
    for (int c = 0; c < model_->columnCount(); ++c)
        t.headers << model_->headerData(c, Qt::Horizontal).toString();
    const int rows = model_->rowCount();
    t.rows.reserve(rows);
    for (int r = 0; r < rows; ++r) {
        QStringList row;
        row.reserve(model_->columnCount());
        for (int c = 0; c < model_->columnCount(); ++c)
            row << model_->data(model_->index(r, c)).toString();
        t.rows.append(row);
    }
    return t;
}

QPixmap DerivChainPage::export_image() const {
    switch (analysis_ ? analysis_->currentIndex() : 0) {
        case 1:
            return term_chart_->render_image();
        case 2:
            return ui::ChartExport::grab(surface_);
        case 3:
            return ui::ChartExport::grab(oi_chart_);
        default:
            return smile_chart_->render_image();
    }
}

report::ReportPayload DerivChainPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    if (!chain_ || expiry_ >= chain_->expiries.size()) {
        p.title = tr("Option chain");
        p.notes = tr("No chain loaded.");
        return p;
    }
    const mk::FreeExpiry& x = chain_->expiries[expiry_];
    p.title = tr("Options on %1 · %2 expiry").arg(chain_->symbol, x.date.toString(Qt::ISODate));
    p.as_of = chain_->quote_time.isValid() ? chain_->quote_time.toLocalTime() : chain_->fetched_at.toLocalTime();
    p.kpi_title = tr("Volatility and positioning (%1, delayed)").arg(chain_->source_label);
    auto kpi = [&p](const QString& l, const QString& v) {
        report::PayloadKpi k;
        k.label = l;
        k.value = v;
        p.kpis.append(k);
    };
    kpi(tr("Spot"), num(chain_->spot, 2));
    kpi(tr("ATM IV"), vol_pct(x.smile.atm_iv));
    if (x.smile.has_25d) {
        kpi(tr("25Δ risk reversal"), ui::formatting::pp(x.smile.rr25 * 100.0));
        kpi(tr("25Δ butterfly"), ui::formatting::pp(x.smile.bf25 * 100.0));
    }
    if (std::isfinite(x.straddle))
        kpi(tr("Expected move (straddle)"), QStringLiteral("± %1").arg(num(x.straddle, 2)));
    kpi(tr("Put/call OI"), x.call_oi > 0 ? ui::formatting::ratio(x.put_oi / x.call_oi) : QStringLiteral("—"));
    if (std::isfinite(x.max_pain))
        kpi(tr("Max pain"), num(x.max_pain, 2));
    if (x.svi.ok)
        kpi(tr("SVI fit RMSE"), ui::formatting::pp(x.svi.rmse_vol * 100.0, 2, false));
    report::PayloadTable smile;
    smile.title = tr("Smile (out-of-the-money mids)");
    smile.columns = {tr("Strike"), tr("ln(K/F)"), tr("Implied vol")};
    for (const vol::SmileQuote& s : mk::smile_quotes(*chain_, expiry_))
        smile.rows.append({num(s.strike, 2), num(s.k, 4), vol_pct(s.iv)});
    p.tables.append(smile);
    report::PayloadTable term;
    term.title = tr("Term structure and expected move");
    term.columns = {tr("Expiry"), tr("Days"), tr("ATM IV"), tr("Straddle"), tr("1σ move")};
    for (const mk::FreeExpiry& e : chain_->expiries)
        term.rows.append({e.date.toString(Qt::ISODate), QString::number(e.days), vol_pct(e.smile.atm_iv),
                          std::isfinite(e.straddle) ? num(e.straddle, 2) : QStringLiteral("—"), num(e.smile.move_1sd, 2)});
    p.tables.append(term);
    report::PayloadChart c;
    c.title = tr("Implied volatility");
    c.image = png_bytes(export_image());
    if (!c.image.isEmpty())
        p.charts.append(c);
    p.inputs = {{tr("Symbol"), chain_->symbol}, {tr("Source"), chain_->source_label + tr(" (delayed)")},
                {tr("Expiry"), x.date.toString(Qt::ISODate)}};
    p.notes = tr("Delayed quotes. IV and Greeks computed locally from bid/ask mids (Black-Scholes-Merton on the "
                 "forward implied by put-call parity). SVI = Gatheral's raw parameterisation, fitted per expiry.");
    return p;
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivChainPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivChainPage::retranslate() {
    book_.apply();
    sym_box_->set_placeholder(tr("Underlying: US stock, ETF or index"));
    if (chain_) {
        update_header();
        update_notice();
        update_smile();
        update_term();
        update_surface();
        update_oi();
    }
}

} // namespace fincept::screens::deriv
