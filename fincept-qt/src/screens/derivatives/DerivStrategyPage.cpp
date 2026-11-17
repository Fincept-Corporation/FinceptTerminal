#include "screens/derivatives/DerivStrategyPage.h"

#include "services/options/DerivBookStore.h"
#include "services/options/OptionModels.h"
#include "services/options/StrategyMath.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace st = services::options::strategy;
namespace mk = services::options::market;
namespace md = services::options::models;
namespace fmt = fincept::ui::formatting;
namespace bk = services::options::book;
using services::options::DerivBookStore;
using services::options::FreeOptionChainService;

namespace {

constexpr double kMultiplier = 100.0;

bool dsp_broad_index(const QString& sym) {
    static const QStringList kIndex = {"SPX", "XSP", "NDX", "RUT", "MRUT", "VIX", "DJX", "OEX", "XEO"};
    return kIndex.contains(FreeOptionChainService::key_of(sym));
}

/// Translated template names, by id (StrategyMath keeps English ids and names).
QString dsp_template_name(const std::string& id, const std::string& fallback) {
    static const QHash<QString, const char*> kNames = {
        {"long_call", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Long call")},
        {"long_put", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Long put")},
        {"bull_call_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Bull call spread (debit vertical)")},
        {"bear_put_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Bear put spread (debit vertical)")},
        {"bull_put_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Bull put spread (credit vertical)")},
        {"bear_call_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Bear call spread (credit vertical)")},
        {"long_straddle", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Long straddle")},
        {"short_straddle", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Short straddle")},
        {"long_strangle", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Long strangle")},
        {"short_strangle", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Short strangle")},
        {"long_call_butterfly", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Long call butterfly")},
        {"iron_butterfly", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Iron butterfly")},
        {"iron_condor", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Iron condor")},
        {"call_calendar", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Call calendar")},
        {"put_calendar", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Put calendar")},
        {"call_diagonal", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Call diagonal")},
        {"call_ratio_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Call ratio spread (1x2)")},
        {"put_ratio_spread", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Put ratio spread (1x2)")},
        {"collar", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Collar")},
        {"covered_call", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Covered call")},
        {"protective_put", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Protective put")},
        {"cash_secured_put", QT_TRANSLATE_NOOP("fincept::screens::deriv::DerivStrategyPage", "Cash-secured put")},
    };
    const auto it = kNames.constFind(QString::fromStdString(id));
    return it == kNames.constEnd() ? QString::fromStdString(fallback)
                                   : QCoreApplication::translate("fincept::screens::deriv::DerivStrategyPage", it.value());
}

} // namespace

// ── Legs model ───────────────────────────────────────────────────────────────

void DerivLegsModel::set_legs(const QVector<LegView>& legs) {
    beginResetModel();
    legs_ = legs;
    endResetModel();
}

void DerivLegsModel::append(const QVector<LegView>& legs) {
    if (legs.isEmpty())
        return;
    beginInsertRows({}, legs_.size(), legs_.size() + legs.size() - 1);
    legs_ += legs;
    endInsertRows();
}

void DerivLegsModel::remove_row(int row) {
    if (row < 0 || row >= legs_.size())
        return;
    beginRemoveRows({}, row, row);
    legs_.removeAt(row);
    endRemoveRows();
}

void DerivLegsModel::refresh_view() {
    if (!legs_.isEmpty())
        emit dataChanged(index(0, 0), index(legs_.size() - 1, kCols - 1));
}

int DerivLegsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : legs_.size();
}

int DerivLegsModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : kCols;
}

Qt::ItemFlags DerivLegsModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (!index.isValid() || index.row() >= legs_.size())
        return f;
    const bool option = legs_[index.row()].leg.kind != st::LegKind::Underlying;
    switch (index.column()) {
        case kActive:
            return f | Qt::ItemIsUserCheckable;
        case kQty:
        case kEntry:
            return f | Qt::ItemIsEditable;
        case kStrike:
        case kIv:
            return option ? (f | Qt::ItemIsEditable) : f;
        default:
            return f;
    }
}

QVariant DerivLegsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= legs_.size())
        return {};
    const LegView& v = legs_[index.row()];
    const st::Leg& l = v.leg;
    const bool option = l.kind != st::LegKind::Underlying;
    if (role == Qt::CheckStateRole && index.column() == kActive)
        return l.active ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue(index.column() == kType || index.column() == kExpiry ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                                                         : (Qt::AlignRight | Qt::AlignVCenter));
    if (role == Qt::EditRole) {
        switch (index.column()) {
            case kQty: return l.quantity;
            case kStrike: return l.strike;
            case kEntry: return l.entry_price;
            case kIv: return l.iv * 100.0;
            default: return {};
        }
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (index.column()) {
        case kQty:
            return (l.quantity >= 0 ? tr("Buy %1") : tr("Sell %1")).arg(fmt::quantity(std::fabs(l.quantity)));
        case kType:
            return l.kind == st::LegKind::Call ? tr("Call") : l.kind == st::LegKind::Put ? tr("Put") : tr("Stock");
        case kStrike:
            return option ? fmt::number(l.strike, 2) : QString();
        case kExpiry:
            return option ? v.expiry_label : QStringLiteral("—");
        case kEntry:
            return fmt::number(l.entry_price, 2);
        case kIv:
            return option ? fmt::fraction_pct(l.iv, 1, false) : QString();
        case kMidNow: {
            if (!option)
                return spot_ > 0 ? fmt::number(spot_, 2) : QString();
            double mid = 0, iv = 0;
            if (chain_ && mk::find_quote(*chain_, l.expiry_index, l.strike, l.kind == st::LegKind::Call, &mid, &iv) && mid > 0)
                return fmt::number(mid, 2);
            return QString();
        }
        case kDelta: {
            const double units = st::leg_units(l, kMultiplier);
            if (!option)
                return fmt::number(units, 0);
            if (!(spot_ > 0) || !(l.expiry_years > 0))
                return QString();
            const md::Greeks g = md::bsm_full(l.kind == st::LegKind::Call, spot_, l.strike, l.expiry_years, r_, q_,
                                              std::max(l.iv, 1e-4));
            return g.valid ? fmt::number(units * g.delta, 1) : QString();
        }
        default:
            return {};
    }
}

bool DerivLegsModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.row() >= legs_.size())
        return false;
    st::Leg& l = legs_[index.row()].leg;
    bool ok = true;
    if (role == Qt::CheckStateRole && index.column() == kActive) {
        l.active = value.toInt() == Qt::Checked;
    } else if (role == Qt::EditRole) {
        const double d = value.toDouble(&ok);
        if (!ok || !std::isfinite(d))
            return false;
        switch (index.column()) {
            case kQty:
                if (d == 0)
                    return false;
                l.quantity = d;
                break;
            case kStrike:
                if (d <= 0)
                    return false;
                l.strike = d;
                break;
            case kEntry:
                if (d < 0)
                    return false;
                l.entry_price = d;
                break;
            case kIv:
                if (d <= 0 || d > 1000)
                    return false;
                l.iv = d / 100.0;
                break;
            default:
                return false;
        }
    } else {
        return false;
    }
    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), kCols - 1));
    emit legs_edited();
    return true;
}

QVariant DerivLegsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::ToolTipRole) {
        switch (section) {
            case kQty: return tr("Contracts (shares for stock). Edit it: a negative number sells.");
            case kEntry: return tr("Price paid or received per share. Edit it to test a fill.");
            case kIv: return tr("Volatility that values the leg before its expiry (percent).");
            case kDelta: return tr("Position delta of the leg in shares.");
            default: return {};
        }
    }
    if (role != Qt::DisplayRole)
        return {};
    switch (section) {
        case kActive: return tr("ON");
        case kQty: return tr("SIDE / QTY");
        case kType: return tr("TYPE");
        case kStrike: return tr("STRIKE");
        case kExpiry: return tr("EXPIRY");
        case kEntry: return tr("ENTRY");
        case kIv: return tr("IV");
        case kMidNow: return tr("MID NOW");
        case kDelta: return tr("DELTA");
        default: return {};
    }
}

// ── Page ─────────────────────────────────────────────────────────────────────

DerivStrategyPage::DerivStrategyPage(DerivContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx), book_("fincept::screens::deriv::DerivStrategyPage") {
    build();
}

DerivStrategyPage::~DerivStrategyPage() {
    for (quint64 sid : service_ids_)
        FreeOptionChainService::instance().cancel(sid);
}

void DerivStrategyPage::build() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(8);
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setChildrenCollapsible(false);
    root->addWidget(split);
    split->addWidget(build_controls(split));
    split->addWidget(build_analysis(split));
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);

    memory_ = new ui::InputMemory(QStringLiteral("derivatives"), QStringLiteral("strategy"), this);
    memory_->track(QStringLiteral("symbol"), symbol_memory_);
    memory_->track(QStringLiteral("template"), template_memory_);
    memory_->track(QStringLiteral("width"), width_);
    memory_->track(QStringLiteral("qty"), qty_);
    memory_->track(QStringLiteral("legs"), legs_memory_);
    memory_->track(QStringLiteral("name"), name_memory_);
    memory_->restore();
    if (!symbol_memory_->text().trimmed().isEmpty())
        symbol_ = symbol_memory_->text().trimmed();
    else if (ctx_ && ctx_->chain())
        symbol_ = ctx_->chain()->symbol;
    symbol_memory_->setText(symbol_);
    sym_box_->set_text(symbol_);
    const QString tid = template_memory_->text().isEmpty() ? QStringLiteral("iron_condor") : template_memory_->text();
    for (int i = 0; i < template_->count(); ++i)
        if (template_->itemData(i).toString() == tid)
            template_->setCurrentIndex(i);
    if (ctx_)
        connect(ctx_, &DerivContext::chain_changed, this, [this] {
            if (ctx_->chain() && FreeOptionChainService::key_of(ctx_->chain()->symbol) == FreeOptionChainService::key_of(symbol_)) {
                on_chain(ctx_->chain());
                recompute();
            }
        });
    // Saved strategies: read once off the GUI thread, again whenever one is saved or deleted.
    connect(&DerivBookStore::instance(), &DerivBookStore::strategies_changed, this, &DerivStrategyPage::reload_saved);
    reload_saved();
    retranslate();
    pane_->set_view(FreeOptionChainService::key_of(symbol_));
}

QWidget* DerivStrategyPage::build_controls(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(8);
    Card card = make_card(w, QStringLiteral("STRAT"), tr("POSITION"));
    book_.label(card.title, QT_TR_NOOP("POSITION"));
    vl->addWidget(card.frame, 1);
    auto* gw = new QWidget(card.body->parentWidget());
    auto* g = new QGridLayout(gw);
    g->setContentsMargins(0, 0, 0, 0);
    g->setHorizontalSpacing(8);
    g->setVerticalSpacing(4);
    card.body->addWidget(gw);

    sym_box_ = new ui::SymbolSearchBox(gw);
    sym_box_->set_compact(true);
    sym_box_->set_clear_on_pick(false);
    services::ResolveOptions ro;
    ro.ctx.allowed_classes = {core::instrument::AssetClass::Equity, core::instrument::AssetClass::Etf,
                              core::instrument::AssetClass::Index};
    sym_box_->set_options(ro);
    sym_box_->set_recent_key(QStringLiteral("derivatives.underlying"));
    connect(sym_box_, &ui::SymbolSearchBox::instrument_picked, this, [this](const core::instrument::Instrument& inst) {
        const QString s = inst.symbol.isEmpty() ? inst.display : inst.symbol;
        if (s.isEmpty() || FreeOptionChainService::key_of(s) == FreeOptionChainService::key_of(symbol_))
            return;
        symbol_ = s;
        symbol_memory_->setText(s);
        name_memory_->clear();
        legs_->set_legs({});
        save_legs();
        chain_.reset();
        pane_->set_view(FreeOptionChainService::key_of(s));
        pane_->run();
    });
    QLabel* l = add_field(g, 0, 0, tr("UNDERLYING"), sym_box_, 2);
    book_.label(l, QT_TR_NOOP("UNDERLYING"));
    chain_status_ = new QLabel(gw);
    chain_status_->setObjectName(QStringLiteral("derivStatusLine"));
    chain_status_->setWordWrap(true);
    g->addWidget(chain_status_, 2, 0, 1, 2);

    template_ = new QComboBox(gw);
    for (const st::Template& t : st::templates())
        template_->addItem(dsp_template_name(t.id, t.name), QString::fromStdString(t.id));
    expiry_ = new QComboBox(gw);
    width_ = make_int_spin(gw, 1, 1, 20);
    width_->setToolTip(tr("Distance between the strikes of the template, in listed strike steps."));
    qty_ = make_int_spin(gw, 1, 1, 1000);
    l = add_field(g, 2, 0, tr("TEMPLATE"), template_, 2);
    book_.label(l, QT_TR_NOOP("TEMPLATE"));
    l = add_field(g, 3, 0, tr("EXPIRY"), expiry_);
    book_.label(l, QT_TR_NOOP("EXPIRY"));
    l = add_field(g, 3, 1, tr("WIDTH (STRIKE STEPS)"), width_);
    book_.label(l, QT_TR_NOOP("WIDTH (STRIKE STEPS)"));
    l = add_field(g, 4, 0, tr("QUANTITY (×)"), qty_);
    book_.label(l, QT_TR_NOOP("QUANTITY (×)"));
    apply_ = make_button(gw, tr("APPLY TEMPLATE"), true);
    book_.button(apply_, QT_TR_NOOP("APPLY TEMPLATE"));
    g->addWidget(apply_, 9, 1); // beside the quantity box, at its natural width
    compact_grid(g);            // the fields keep their width; the spare width stays at the right
    connect(apply_, &QPushButton::clicked, this, &DerivStrategyPage::apply_template);
    connect(template_, &QComboBox::currentIndexChanged, this, [this] {
        template_memory_->setText(template_->currentData().toString());
        const auto& all = st::templates();
        const int i = template_->currentIndex();
        if (i >= 0 && i < static_cast<int>(all.size()))
            template_->setToolTip(QString::fromStdString(all[i].outlook));
    });

    auto* sec = make_section(card.frame, tr("LEGS"));
    book_.label(sec, QT_TR_NOOP("LEGS"));
    card.body->addWidget(sec);
    legs_view_ = new QTableView(card.frame);
    legs_ = new DerivLegsModel(legs_view_);
    legs_view_->setModel(legs_);
    dense_table(legs_view_);
    legs_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    legs_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    legs_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    legs_view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                                QAbstractItemView::AnyKeyPressed);
    connect(legs_, &DerivLegsModel::legs_edited, this, [this] {
        save_legs();
        recompute();
    });
    card.body->addWidget(legs_view_, 1);
    auto* btns = new QWidget(card.frame);
    auto* bl = new QHBoxLayout(btns);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(6);
    static const char* const kAdd[] = {QT_TR_NOOP("+ CALL"), QT_TR_NOOP("+ PUT"), QT_TR_NOOP("+ STOCK")};
    for (int k = 0; k < 3; ++k) {
        auto* b = make_button(btns, tr(kAdd[k]), false);
        book_.button(b, kAdd[k]);
        connect(b, &QPushButton::clicked, this, [this, k] { add_leg(k); });
        bl->addWidget(b);
    }
    auto* rm = make_button(btns, tr("REMOVE"), false);
    book_.button(rm, QT_TR_NOOP("REMOVE"));
    connect(rm, &QPushButton::clicked, this, [this] {
        const QModelIndex i = legs_view_->currentIndex();
        if (i.isValid()) {
            legs_->remove_row(i.row());
            save_legs();
            recompute();
        }
    });
    bl->addWidget(rm);
    auto* mids = make_button(btns, tr("USE MIDS"), false);
    book_.button(mids, QT_TR_NOOP("USE MIDS"));
    mids->setToolTip(tr("Set every entry price and IV to the chain's current mid"));
    connect(mids, &QPushButton::clicked, this, &DerivStrategyPage::reprice_entries);
    bl->addWidget(mids);
    bl->addStretch(1);
    card.body->addWidget(btns);

    // Saved strategies and the book.
    auto* keep = new QWidget(card.frame);
    auto* kl = new QHBoxLayout(keep);
    kl->setContentsMargins(0, 0, 0, 0);
    kl->setSpacing(6);
    auto* save = make_button(keep, tr("SAVE AS…"), false);
    book_.button(save, QT_TR_NOOP("SAVE AS…"));
    book_.tooltip(save, QT_TR_NOOP("Save these legs under a name (saving under an existing name replaces it)"));
    connect(save, &QPushButton::clicked, this, &DerivStrategyPage::save_as);
    kl->addWidget(save);
    saved_btn_ = make_button(keep, tr("SAVED STRATEGIES"), false);
    book_.button(saved_btn_, QT_TR_NOOP("SAVED STRATEGIES"));
    book_.tooltip(saved_btn_, QT_TR_NOOP("Open or delete a saved strategy"));
    auto* saved_menu = new QMenu(saved_btn_);
    connect(saved_menu, &QMenu::aboutToShow, this, &DerivStrategyPage::fill_saved_menu);
    saved_btn_->setMenu(saved_menu);
    kl->addWidget(saved_btn_);
    auto* to_book = make_button(keep, tr("ADD TO BOOK"), false);
    book_.button(to_book, QT_TR_NOOP("ADD TO BOOK"));
    book_.tooltip(to_book, QT_TR_NOOP("Save this position in the BOOK to follow its marks, P&L and Greeks"));
    connect(to_book, &QPushButton::clicked, this, &DerivStrategyPage::add_to_book);
    kl->addWidget(to_book);
    kl->addStretch(1);
    card.body->addWidget(keep);

    // Hidden carriers of remembered values.
    legs_memory_ = new QLineEdit(w);
    legs_memory_->hide();
    symbol_memory_ = new QLineEdit(w);
    symbol_memory_->hide();
    template_memory_ = new QLineEdit(w);
    template_memory_->hide();
    name_memory_ = new QLineEdit(w);
    name_memory_->hide();
    return w;
}

QWidget* DerivStrategyPage::build_analysis(QWidget* parent) {
    // A panel like the POSITION card beside it: a one-line header (title, the pane's status,
    // RUN SCENARIOS and the exports), then the KPIs, the target-date slider and the P&L chart.
    auto* w = new QWidget(parent);
    w->setObjectName(QStringLiteral("derivPanel"));
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(1, 1, 1, 1);
    vl->setSpacing(0);
    auto* top = new ui::PanelHeader(tr("PROFIT / LOSS"), w);
    book_.label(top->title_label(), QT_TR_NOOP("PROFIT / LOSS"));
    status_ = new QLabel(top->controls());
    status_->setObjectName(QStringLiteral("derivStatusLine"));
    status_->setWordWrap(true); // wraps rather than widening the panel past a narrow window
    top->add_control(status_, 1);
    auto* scen = make_button(top->controls(), tr("RUN SCENARIOS"), false);
    book_.button(scen, QT_TR_NOOP("RUN SCENARIOS"));
    scen->setToolTip(tr("Stress this position: spot × volatility grid, time decay, presets"));
    connect(scen, &QPushButton::clicked, this, [this] {
        if (ctx_)
            emit ctx_->scenarios_requested();
    });
    top->add_control(scen);
    result_bar_ = new DerivResultBar(QStringLiteral("option_strategy"), top->controls());
    result_bar_->set_table([this] { return export_table(); });
    result_bar_->set_image([this] { return chart_ ? chart_->render_image() : QPixmap(); });
    result_bar_->set_payload([this] { return report_payload(); });
    top->add_control(result_bar_);
    vl->addWidget(top);

    pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, w);
    auto* content = new QWidget(pane_->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(10, 8, 10, 8);
    cl->setSpacing(6);
    kpis_ = new ui::KpiStrip(content);
    cl->addWidget(kpis_);
    greek_kpis_ = new ui::KpiStrip(content);
    cl->addWidget(greek_kpis_);
    auto* trow = new QWidget(content);
    auto* trl = new QHBoxLayout(trow);
    trl->setContentsMargins(0, 0, 0, 0);
    auto* tcap = new QLabel(tr("TARGET DATE"), trow);
    tcap->setObjectName(QStringLiteral("derivLabel"));
    book_.label(tcap, QT_TR_NOOP("TARGET DATE"));
    target_ = new QSlider(Qt::Horizontal, trow);
    target_->setRange(0, 0);
    target_->setToolTip(tr("Days from today for the dashed curve (valued with the model)"));
    tcap->setBuddy(target_);
    target_label_ = new QLabel(trow);
    target_label_->setObjectName(QStringLiteral("derivStatusLine"));
    connect(target_, &QSlider::valueChanged, this, [this] { recompute(); });
    trl->addWidget(tcap);
    trl->addWidget(target_, 1);
    trl->addWidget(target_label_);
    cl->addWidget(trow);
    chart_ = make_xy_chart(content, tr("PROFIT / LOSS"), QString(), {});
    cl->addWidget(chart_, 1);
    note_ = make_note(content, QString());
    cl->addWidget(note_);
    pane_->set_content(content);
    pane_->set_requirements({ui::Requirement::python()});
    pane_->set_persist_limit(0);
    pane_->set_max_age(5 * 60);
    pane_->set_timeout(100);
    pane_->set_loading_text(tr("Loading the delayed option chain…"));
    pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);
    pane_->set_status_label(status_);
    pane_->set_start([this](quint64 id) {
        const QString sym = symbol_;
        const quint64 sid = FreeOptionChainService::instance().fetch(
            sym, QStringLiteral("auto"), false, this, [this, id, sym](const FreeOptionChainService::Reply& r) {
                if (!pane_->is_current(id))
                    return;
                if (r.stage == FreeOptionChainService::Stage::Failed) {
                    service_ids_.remove(id);
                    pane_->fail(id, r.message, r.status == QLatin1String("no_options") ? tr("No listed options") : QString());
                    return;
                }
                on_chain(r.chain);
                const QJsonObject payload{{QStringLiteral("symbol"), FreeOptionChainService::key_of(sym)}};
                if (r.stage == FreeOptionChainService::Stage::Saved) {
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
    pane_->set_render([this](const QJsonValue&, const QDateTime&, bool) {
        if (!chain_) {
            if (auto c = FreeOptionChainService::instance().cached(symbol_))
                on_chain(c);
        }
        recompute();
    });
    vl->addWidget(pane_, 1);
    return w;
}

// ── Chain / legs ─────────────────────────────────────────────────────────────

void DerivStrategyPage::on_chain(const DerivContext::ChainPtr& chain) {
    if (!chain || !chain->ok())
        return;
    const bool first = !chain_;
    chain_ = chain;
    const mk::FreeExpiry* x0 = chain->expiries.isEmpty() ? nullptr : &chain->expiries.first();
    legs_->set_chain(chain);
    legs_->set_market(chain->spot, x0 ? x0->rate : 0.04, x0 ? x0->carry_q : 0.0);
    chain_status_->setText(tr("%1 %2 · %3 · delayed").arg(chain->symbol, num(chain->spot, 2), chain->source_label));
    fill_expiries();
    if (first) {
        restore_legs();
        if (legs_->legs().isEmpty())
            apply_template();
    } else {
        // A newer chain: refresh each leg's time to expiry; prices stay as entered.
        QVector<LegView> legs = legs_->legs();
        for (LegView& v : legs) {
            if (v.leg.kind == st::LegKind::Underlying)
                continue;
            for (int e = 0; e < chain->expiries.size(); ++e)
                if (chain->expiries[e].date.toString(Qt::ISODate) == v.expiry_label) {
                    v.leg.expiry_index = e;
                    v.leg.expiry_years = chain->expiries[e].years;
                }
        }
        legs_->set_legs(legs);
    }
    legs_->refresh_view();
}

void DerivStrategyPage::fill_expiries() {
    if (!chain_)
        return;
    const QString keep = expiry_->currentText().left(10);
    QSignalBlocker block(expiry_);
    expiry_->clear();
    int pick = -1, default_pick = 0;
    for (int e = 0; e < chain_->expiries.size(); ++e) {
        const mk::FreeExpiry& x = chain_->expiries[e];
        expiry_->addItem(tr("%1 (%2d)").arg(x.date.toString(Qt::ISODate)).arg(x.days), e);
        if (x.date.toString(Qt::ISODate) == keep)
            pick = e;
        if (default_pick == 0 && x.days >= 25)
            default_pick = e;
    }
    expiry_->setCurrentIndex(pick >= 0 ? pick : default_pick);
}

void DerivStrategyPage::apply_template() {
    if (!chain_) {
        status_->setText(tr("Load a chain first."));
        return;
    }
    const auto& all = st::templates();
    const int ti = template_->currentIndex();
    const int e = expiry_->currentData().toInt();
    if (ti < 0 || ti >= static_cast<int>(all.size()))
        return;
    st::ChainAccess acc;
    const DerivContext::ChainPtr c = chain_;
    acc.expiry_count = c->expiries.size();
    acc.strikes = [c](int ei) {
        const QVector<double> v = mk::strikes(*c, ei);
        return std::vector<double>(v.begin(), v.end());
    };
    acc.years = [c](int ei) { return c->expiries[ei].years; };
    acc.quote = [c](int ei, double k, bool call, double* mid, double* iv) { return mk::find_quote(*c, ei, k, call, mid, iv); };
    std::string err;
    const std::vector<st::Leg> legs =
        st::instantiate(all[ti], acc, e, c->spot, width_->value(), qty_->value(), kMultiplier, &err);
    if (legs.empty()) {
        status_->setText(tr("Cannot build this template here: %1.").arg(QString::fromStdString(err)));
        return;
    }
    QVector<LegView> views;
    for (const st::Leg& l : legs) {
        LegView v;
        v.leg = l;
        if (l.kind != st::LegKind::Underlying && l.expiry_index >= 0)
            v.expiry_label = c->expiries[l.expiry_index].date.toString(Qt::ISODate);
        views.append(v);
    }
    name_memory_->clear(); // a template's position is named after the template
    legs_->set_legs(views);
    save_legs();
    recompute();
}

void DerivStrategyPage::add_leg(int kind) {
    if (!chain_) {
        status_->setText(tr("Load a chain first."));
        return;
    }
    LegView v;
    if (kind == 2) {
        v.leg.kind = st::LegKind::Underlying;
        v.leg.quantity = kMultiplier;
        v.leg.entry_price = chain_->spot;
    } else {
        const int e = std::max(0, expiry_->currentData().toInt());
        const mk::FreeExpiry& x = chain_->expiries[e];
        v.leg.kind = kind == 0 ? st::LegKind::Call : st::LegKind::Put;
        v.leg.quantity = 1;
        v.leg.strike = x.atm_strike;
        v.leg.expiry_years = x.years;
        v.leg.expiry_index = e;
        double mid = 0, iv = 0;
        mk::find_quote(*chain_, e, x.atm_strike, kind == 0, &mid, &iv);
        v.leg.entry_price = mid;
        v.leg.iv = iv > 0 ? iv : (x.smile.atm_iv > 0 ? x.smile.atm_iv : 0.2);
        v.expiry_label = x.date.toString(Qt::ISODate);
    }
    legs_->append({v});
    save_legs();
    recompute();
}

void DerivStrategyPage::add_legs(const QVector<LegView>& legs) {
    if (legs.isEmpty())
        return;
    if (ctx_ && ctx_->chain() &&
        FreeOptionChainService::key_of(ctx_->chain()->symbol) != FreeOptionChainService::key_of(symbol_)) {
        // Another underlying: start a new position on it.
        symbol_ = ctx_->chain()->symbol;
        symbol_memory_->setText(symbol_);
        sym_box_->set_text(symbol_);
        legs_->set_legs({});
        chain_.reset();
        pane_->set_view(FreeOptionChainService::key_of(symbol_));
        on_chain(ctx_->chain());
        legs_->set_legs({});
    } else if (!chain_ && ctx_ && ctx_->chain()) {
        on_chain(ctx_->chain());
    }
    legs_->append(legs);
    save_legs();
    recompute();
}

void DerivStrategyPage::reprice_entries() {
    if (!chain_)
        return;
    QVector<LegView> legs = legs_->legs();
    for (LegView& v : legs) {
        if (v.leg.kind == st::LegKind::Underlying) {
            v.leg.entry_price = chain_->spot;
            continue;
        }
        double mid = 0, iv = 0;
        if (mk::find_quote(*chain_, v.leg.expiry_index, v.leg.strike, v.leg.kind == st::LegKind::Call, &mid, &iv)) {
            if (mid > 0)
                v.leg.entry_price = mid;
            if (iv > 0)
                v.leg.iv = iv;
        }
    }
    legs_->set_legs(legs);
    save_legs();
    recompute();
}

void DerivStrategyPage::save_legs() {
    if (restoring_)
        return;
    QJsonArray a;
    for (const LegView& v : legs_->legs()) {
        const st::Leg& l = v.leg;
        a.append(QJsonObject{{QStringLiteral("k"), l.kind == st::LegKind::Call   ? QStringLiteral("C")
                                                   : l.kind == st::LegKind::Put ? QStringLiteral("P")
                                                                                : QStringLiteral("U")},
                             {QStringLiteral("q"), l.quantity},
                             {QStringLiteral("K"), l.strike},
                             {QStringLiteral("e"), v.expiry_label},
                             {QStringLiteral("p"), l.entry_price},
                             {QStringLiteral("v"), l.iv},
                             {QStringLiteral("a"), l.active}});
    }
    legs_memory_->setText(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)));
}

void DerivStrategyPage::restore_legs() {
    if (!chain_)
        return;
    const QJsonArray a = QJsonDocument::fromJson(legs_memory_->text().toUtf8()).array();
    QVector<LegView> legs;
    int dropped = 0;
    for (const QJsonValue& jv : a) {
        const QJsonObject o = jv.toObject();
        LegView v;
        const QString k = o.value(QStringLiteral("k")).toString();
        v.leg.kind = k == QLatin1String("C") ? st::LegKind::Call : k == QLatin1String("P") ? st::LegKind::Put : st::LegKind::Underlying;
        v.leg.quantity = o.value(QStringLiteral("q")).toDouble(1);
        v.leg.strike = o.value(QStringLiteral("K")).toDouble();
        v.leg.entry_price = o.value(QStringLiteral("p")).toDouble();
        v.leg.iv = o.value(QStringLiteral("v")).toDouble(0.2);
        v.leg.active = o.value(QStringLiteral("a")).toBool(true);
        v.expiry_label = o.value(QStringLiteral("e")).toString();
        if (v.leg.kind != st::LegKind::Underlying) {
            bool found = false;
            for (int e = 0; e < chain_->expiries.size(); ++e)
                if (chain_->expiries[e].date.toString(Qt::ISODate) == v.expiry_label) {
                    v.leg.expiry_index = e;
                    v.leg.expiry_years = chain_->expiries[e].years;
                    found = true;
                }
            if (!found) {
                ++dropped;
                continue;
            }
        }
        legs.append(v);
    }
    restoring_ = true;
    legs_->set_legs(legs);
    restoring_ = false;
    if (dropped > 0)
        status_->setText(tr("%1 saved leg(s) have expired and were left out.").arg(dropped));
}

// ── Saved strategies / book ──────────────────────────────────────────────────

QVector<bk::BookLeg> DerivStrategyPage::book_legs() const {
    QVector<bk::BookLeg> out;
    for (const LegView& v : legs_->legs()) {
        if (!v.leg.active)
            continue;
        bk::BookLeg l;
        l.type = v.leg.kind == st::LegKind::Call  ? bk::LegType::Call
                 : v.leg.kind == st::LegKind::Put ? bk::LegType::Put
                                                  : bk::LegType::Stock;
        l.quantity = v.leg.quantity;
        l.strike = v.leg.strike;
        l.expiry = QDate::fromString(v.expiry_label, Qt::ISODate);
        l.entry_price = v.leg.entry_price;
        l.entry_iv = l.is_option() ? v.leg.iv : 0.0;
        out.append(l);
    }
    return out;
}

void DerivStrategyPage::load_legs(const QString& symbol, const QString& name, const QVector<bk::BookLeg>& legs) {
    QJsonArray a;
    for (const bk::BookLeg& l : legs) {
        if (!l.is_option() && l.type != bk::LegType::Stock)
            continue; // futures and swaps are not strategy legs
        a.append(QJsonObject{{QStringLiteral("k"), l.type == bk::LegType::Call  ? QStringLiteral("C")
                                                   : l.type == bk::LegType::Put ? QStringLiteral("P")
                                                                                : QStringLiteral("U")},
                             {QStringLiteral("q"), l.quantity},
                             {QStringLiteral("K"), l.strike},
                             {QStringLiteral("e"), l.expiry.toString(Qt::ISODate)},
                             {QStringLiteral("p"), l.entry_price},
                             {QStringLiteral("v"), l.entry_iv > 0 ? l.entry_iv : 0.2},
                             {QStringLiteral("a"), true}});
    }
    legs_memory_->setText(QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact)));
    name_memory_->setText(name);
    if (chain_ && FreeOptionChainService::key_of(chain_->symbol) == FreeOptionChainService::key_of(symbol)) {
        restore_legs(); // same underlying: the chain in hand maps the expiries
        recompute();
        return;
    }
    symbol_ = symbol;
    symbol_memory_->setText(symbol);
    sym_box_->set_text(symbol);
    legs_->set_legs({});
    chain_.reset();
    pane_->set_view(FreeOptionChainService::key_of(symbol));
    if (auto c = FreeOptionChainService::instance().cached(symbol); c && c->ok()) {
        on_chain(c); // restores the legs from legs_memory_
        recompute();
        return;
    }
    status_->setText(tr("Loading the %1 chain to place “%2”…").arg(symbol, name));
    pane_->run(); // the chain arrives → on_chain restores the legs
}

void DerivStrategyPage::save_as() {
    const QVector<bk::BookLeg> legs = book_legs();
    if (legs.isEmpty()) {
        status_->setText(tr("Nothing to save: add legs first."));
        return;
    }
    const QString suggestion = name_memory_->text().isEmpty()
                                   ? tr("%1 %2").arg(chain_ ? chain_->symbol : symbol_, template_->currentText())
                                   : name_memory_->text();
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save strategy"), tr("Name (an existing name is replaced):"),
                                               QLineEdit::Normal, suggestion, &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    bk::SavedStrategy s;
    s.name = name;
    s.underlying = chain_ ? chain_->symbol : symbol_;
    s.template_id = template_->currentData().toString();
    s.currency = ccy();
    s.multiplier = kMultiplier;
    s.legs = legs;
    status_->setText(tr("Saving…"));
    DerivBookStore::instance().save_strategy(s, this, [this](const bk::SavedStrategy& saved, const QString& error) {
        if (!error.isEmpty()) {
            status_->setText(error);
            return;
        }
        name_memory_->setText(saved.name);
        status_->setText(tr("Saved “%1”.").arg(saved.name));
        recompute();
    });
}

void DerivStrategyPage::reload_saved() {
    DerivBookStore::instance().load_strategies(this, [this](const QVector<bk::SavedStrategy>& rows, const QString& error) {
        if (error.isEmpty())
            saved_ = rows;
    });
}

void DerivStrategyPage::fill_saved_menu() {
    QMenu* menu = saved_btn_->menu();
    menu->clear();
    if (saved_.isEmpty()) {
        QAction* none = menu->addAction(tr("No saved strategies yet - use SAVE AS…"));
        none->setEnabled(false);
        return;
    }
    for (const bk::SavedStrategy& s : saved_) {
        QString label = tr("%1 (%2, %3 legs)").arg(s.name, s.underlying).arg(s.legs.size());
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        menu->addAction(label, this, [this, s] {
            for (int i = 0; i < template_->count(); ++i)
                if (template_->itemData(i).toString() == s.template_id) {
                    QSignalBlocker block(template_);
                    template_->setCurrentIndex(i);
                }
            load_legs(s.underlying, s.name, s.legs);
        });
    }
    menu->addSeparator();
    QMenu* del = menu->addMenu(tr("Delete"));
    for (const bk::SavedStrategy& s : saved_) {
        QString label = s.name;
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        del->addAction(label, this, [this, s] {
            DerivBookStore::instance().remove_strategy(s.id, this, [this, name = s.name](const QString& error) {
                status_->setText(error.isEmpty() ? tr("Deleted “%1”.").arg(name) : error);
            });
        });
    }
}

void DerivStrategyPage::add_to_book() {
    const QVector<bk::BookLeg> legs = book_legs();
    if (legs.isEmpty()) {
        status_->setText(tr("Nothing to add: add legs first."));
        return;
    }
    if (!ctx_)
        return;
    bk::BookPosition p;
    p.kind = legs.size() == 1 ? (legs.first().is_option() ? QStringLiteral("option") : QStringLiteral("stock"))
                              : QStringLiteral("strategy");
    p.name = name_memory_->text().isEmpty() ? template_->currentText() : name_memory_->text();
    p.underlying = chain_ ? chain_->symbol : symbol_;
    p.currency = ccy();
    p.multiplier = p.kind == QLatin1String("stock") ? 1.0 : kMultiplier;
    p.entry_date = QDate::currentDate();
    p.legs = legs;
    emit ctx_->book_add_requested(p);
}

// ── Analysis ─────────────────────────────────────────────────────────────────

st::Spec DerivStrategyPage::spec() const {
    st::Spec s;
    s.multiplier = kMultiplier;
    for (const LegView& v : legs_->legs())
        s.legs.push_back(v.leg);
    return s;
}

st::Market DerivStrategyPage::market() const {
    st::Market m;
    if (!chain_ || chain_->expiries.isEmpty())
        return m;
    // Rate and carry of the position's first expiry.
    const double h = st::first_expiry(spec());
    const mk::FreeExpiry* best = &chain_->expiries.first();
    for (const mk::FreeExpiry& x : chain_->expiries)
        if (std::fabs(x.years - h) < std::fabs(best->years - h))
            best = &x;
    m.spot = chain_->spot;
    m.r = best->rate;
    m.q = best->carry_q;
    return m;
}

double DerivStrategyPage::horizon_vol() const {
    if (!chain_ || chain_->expiries.isEmpty())
        return 0.2;
    const double h = st::first_expiry(spec());
    const mk::FreeExpiry* best = &chain_->expiries.first();
    for (const mk::FreeExpiry& x : chain_->expiries)
        if (std::fabs(x.years - h) < std::fabs(best->years - h))
            best = &x;
    return best->smile.atm_iv > 0 ? best->smile.atm_iv : 0.2;
}

QString DerivStrategyPage::ccy() const {
    return chain_ ? chain_->currency : QStringLiteral("USD");
}

void DerivStrategyPage::recompute() {
    const st::Spec s = spec();
    const st::Market m = market();
    kpis_->clear();
    greek_kpis_->clear();
    last_rows_.clear();
    chart_->clear();
    bool any = false;
    for (const st::Leg& l : s.legs)
        any = any || l.active;
    if (!chain_ || !any) {
        chart_->set_state(ui::ChartState::Empty, tr("Apply a template or add legs to see the payoff."));
        note_->clear();
        return;
    }
    const double h = st::first_expiry(s);
    const int days = static_cast<int>(std::floor(h * 365.0));
    {
        QSignalBlocker block(target_);
        target_->setRange(0, std::max(0, days));
    }
    const double t_target = std::min(target_->value() / 365.0, h);
    target_label_->setText(tr("in %1 days (%2)").arg(target_->value()).arg(
        QDate::currentDate().addDays(target_->value()).toString(QStringLiteral("d MMM"))));
    const double sig = horizon_vol();
    const double span = std::max(0.15, 3.0 * sig * std::sqrt(std::max(h, 1.0 / 365.0)));
    double lo = m.spot * std::exp(-span), hi = m.spot * std::exp(span);
    for (const st::Leg& l : s.legs)
        if (l.active && l.kind != st::LegKind::Underlying) {
            lo = std::min(lo, l.strike * 0.9);
            hi = std::max(hi, l.strike * 1.1);
        }
    const auto curve = st::payoff_curve(s, m, lo, hi, 241, t_target);
    const auto bes = st::breakevens(s, m, lo, hi);
    const st::Extremes ex = st::extremes(s, m);
    const st::Probability pop = st::probability_of_profit(s, m, sig);
    const st::PositionGreeks g = st::greeks(s, m);
    const st::MarginEstimate mg = st::margin_estimate(s, m, dsp_broad_index(symbol_));
    const double net = st::net_premium(s);
    const QString c = ccy();

    auto tile = [this](ui::KpiStrip* strip, const QString& label, const QString& value, const QString& sub,
                       bool compact) {
        ui::StatTile* t = strip->add_tile(label);
        t->set_value(value);
        t->set_compact(compact);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        last_rows_.append({label, value + (sub.isEmpty() ? QString() : QStringLiteral(" (") + sub + QLatin1Char(')'))});
        return t;
    };
    tile(kpis_, net >= 0 ? tr("NET DEBIT") : tr("NET CREDIT"), money(std::fabs(net), c),
         net >= 0 ? tr("paid at entry") : tr("received at entry"), false);
    ui::StatTile* mp = tile(kpis_, tr("MAX PROFIT"), ex.profit_unbounded ? tr("Unlimited") : money(ex.max_profit, c),
                            tr("at the first expiry"), false);
    mp->set_tone(ui::StatTile::Tone::Positive);
    ui::StatTile* ml = tile(kpis_, tr("MAX LOSS"), ex.loss_unbounded ? tr("Unlimited") : money(ex.max_loss, c),
                            tr("at the first expiry"), false);
    ml->set_tone(ui::StatTile::Tone::Negative);
    QStringList bt;
    for (double b : bes)
        bt << num(b, 2);
    tile(kpis_, tr("BREAKEVENS"), bt.isEmpty() ? tr("none") : bt.join(QStringLiteral(" · ")), tr("at the first expiry"), false);
    tile(kpis_, tr("PROBABILITY OF PROFIT"), pop.valid ? fmt::fraction_pct(pop.pop, 1, false) : QStringLiteral("—"),
         tr("lognormal, IV %1").arg(vol_pct(sig, 1)), false);
    tile(kpis_, tr("EXPECTED P&L"), pop.valid ? signed_money(pop.expected_pnl, c) : QStringLiteral("—"),
         tr("same measure"), true);
    ui::StatTile* mt = tile(kpis_, tr("MARGIN (ESTIMATE)"), money(mg.requirement, c), tr("rule-based, not your broker's"), true);
    mt->set_tooltip_text(tr("Estimate from Reg-T / Cboe strategy-based rules (%1). Your broker's or the exchange's "
                            "margin can differ.")
                             .arg(QString::fromStdString(mg.method)));
    tile(greek_kpis_, tr("DELTA"), num(g.delta, 1), tr("shares"), true);
    tile(greek_kpis_, tr("DOLLAR DELTA"), money(g.dollar_delta, c), QString(), true);
    tile(greek_kpis_, tr("GAMMA"), num(g.gamma, 3), tr("shares per 1.0 spot"), true);
    tile(greek_kpis_, tr("THETA"), signed_money(g.theta_day, c), tr("per day"), true);
    tile(greek_kpis_, tr("VEGA"), signed_money(g.vega_pt, c), tr("per vol point"), true);
    tile(greek_kpis_, tr("RHO"), signed_money(g.rho_pct, c), tr("per 1% rate"), true);

    QVector<QPointF> exp_pts, tgt_pts;
    for (const st::PayoffPoint& p : curve) {
        exp_pts.append({p.spot, p.pnl_horizon});
        tgt_pts.append({p.spot, p.pnl_target});
    }
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return fmt::compact(v); });
    chart_->set_time_formatter([](double v) { return DerivStrategyPage::tr("Spot %1").arg(num(v, 2)); });
    chart_->set_tick_time_formatter([](double v) { return num(v, 0); });
    ui::TimeSeriesChart::SeriesSpec a;
    a.name = tr("At the first expiry");
    a.points = exp_pts;
    a.rebase = false;
    a.style = ui::TimeSeriesChart::Style::Baseline;
    a.baseline = 0.0;
    chart_->add_series(a);
    if (t_target < h - 1e-9) {
        ui::TimeSeriesChart::SeriesSpec b;
        b.name = tr("At the target date");
        b.points = tgt_pts;
        b.dashed = true;
        b.rebase = false;
        chart_->add_series(b);
    }
    chart_->add_hline(0.0, QString(), ui::ChartColorRole::Muted);
    chart_->add_price_marker(m.spot, st::pnl_at(s, m, m.spot, t_target), ui::ChartMarkerKind::Custom, tr("Spot"),
                             tr("Spot %1").arg(num(m.spot, 2)));
    for (double b : bes)
        chart_->add_price_marker(b, 0.0, ui::ChartMarkerKind::Custom, tr("BE"), tr("Breakeven %1").arg(num(b, 2)));
    chart_->set_state(ui::ChartState::Ready);
    note_->setText(tr("Model: Black-Scholes-Merton per leg at its own IV (European approximation of American "
                      "contracts), rate %1, carry %2, multiplier %3. \"At the first expiry\" values later legs with "
                      "the model. Probability of profit: the underlying lognormal under the risk-neutral drift with "
                      "the expiry's ATM IV. Margin: %4 — an estimate, not a broker's figure.")
                       .arg(vol_pct(m.r), vol_pct(m.q), num(kMultiplier, 0), QString::fromStdString(mg.method)));
    // Share the position with SCENARIOS.
    if (ctx_) {
        DerivContext::Position p;
        p.name = name_memory_->text().isEmpty() ? template_->currentText() : name_memory_->text();
        p.symbol = chain_->symbol;
        p.currency = c;
        p.legs = legs_->legs();
        p.market = m;
        p.multiplier = kMultiplier;
        p.atm_iv = sig;
        p.valid = true;
        ctx_->set_position(p);
    }
}

void DerivStrategyPage::refresh() {
    if (pane_)
        pane_->run();
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivStrategyPage::export_table() const {
    ui::ChartExport::Table t;
    for (int c = 0; c < DerivLegsModel::kCols; ++c)
        t.headers << legs_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    for (int r = 0; r < legs_->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < DerivLegsModel::kCols; ++c)
            row << (c == DerivLegsModel::kActive ? (legs_->legs()[r].leg.active ? tr("yes") : tr("no"))
                                                 : legs_->data(legs_->index(r, c), Qt::DisplayRole).toString());
        t.rows.append(row);
    }
    t.rows.append(QStringList());
    for (const auto& [k, v] : last_rows_)
        t.rows.append({k, v});
    return t;
}

report::ReportPayload DerivStrategyPage::report_payload() const {
    report::ReportPayload p;
    p.source = QStringLiteral("Derivatives");
    p.title = tr("Option strategy: %1 on %2")
                  .arg(name_memory_->text().isEmpty() ? template_->currentText() : name_memory_->text(),
                       chain_ ? chain_->symbol : symbol_);
    p.as_of = chain_ && chain_->quote_time.isValid() ? chain_->quote_time.toLocalTime() : QDateTime::currentDateTime();
    p.kpi_title = tr("Position at a glance");
    for (const auto& [k, v] : last_rows_) {
        report::PayloadKpi kpi;
        kpi.label = k;
        kpi.value = v;
        p.kpis.append(kpi);
    }
    report::PayloadTable legs;
    legs.title = tr("Legs");
    for (int c = 1; c < DerivLegsModel::kCols; ++c)
        legs.columns << legs_->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    for (int r = 0; r < legs_->rowCount(); ++r) {
        if (!legs_->legs()[r].leg.active)
            continue;
        QStringList row;
        for (int c = 1; c < DerivLegsModel::kCols; ++c)
            row << legs_->data(legs_->index(r, c), Qt::DisplayRole).toString();
        legs.rows.append(row);
    }
    p.tables.append(legs);
    if (chart_) {
        report::PayloadChart ch;
        ch.title = tr("Profit / loss");
        ch.image = png_bytes(chart_->render_image());
        if (!ch.image.isEmpty())
            p.charts.append(ch);
    }
    p.inputs = {{tr("Underlying"), chain_ ? chain_->symbol : symbol_},
                {tr("Spot"), chain_ ? num(chain_->spot, 2) : QStringLiteral("—")},
                {tr("Source"), chain_ ? chain_->source_label + tr(" (delayed)") : QString()},
                {tr("Multiplier"), num(kMultiplier, 0)}};
    p.notes = note_ ? note_->text() : QString();
    return p;
}

// ── Language ─────────────────────────────────────────────────────────────────

void DerivStrategyPage::changeEvent(QEvent* e) {
    if (e->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(e);
}

void DerivStrategyPage::retranslate() {
    book_.apply();
    const auto& all = st::templates();
    for (int i = 0; i < template_->count() && i < static_cast<int>(all.size()); ++i)
        template_->setItemText(i, dsp_template_name(all[i].id, all[i].name));
    sym_box_->set_placeholder(tr("Underlying: US stock, ETF or index"));
    if (chain_) {
        fill_expiries();
        recompute();
    }
}

} // namespace fincept::screens::deriv
