#include "screens/fno/ScreenerSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/fno/FnoFormat.h"
#include "services/options/OptionChainService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QColor>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

constexpr double kEps = 1e-6;

/// OI (contract counts, not money), abbreviated per the user's compact scheme.
QString scr_fmt_count(qint64 v, const QString& ccy) {
    if (v == 0)
        return QStringLiteral("—");
    return fno_fmt_oi(v, ccy);
}

/// `v` is already a percentage (14.2 means 14.2%).
QString scr_fmt_pct(double v) {
    if (std::abs(v) < kEps)
        return QStringLiteral("—");
    return fincept::ui::formatting::pct(v, 2, false);
}

QString scr_fmt_signed_pct(double v) {
    if (std::abs(v) < kEps)
        return QStringLiteral("—");
    return fincept::ui::formatting::pct(v, 2, true);
}

QColor color_for_pct(double v) {
    if (v > kEps)
        return QColor(colors::POSITIVE());
    if (v < -kEps)
        return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

} // namespace

// ── Model ──────────────────────────────────────────────────────────────────

ScreenedChainModel::ScreenedChainModel(QObject* parent) : QAbstractTableModel(parent), ccy_(fno_ccy()) {
    // Formats in data(): a number-format change repaints from the rows held.
    fincept::ui::formatting::refresh_model_on_format_change(this);
}

int ScreenedChainModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return rows_.size();
}

int ScreenedChainModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant ScreenedChainModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        return {};
    const OptionChainRow& r = rows_.at(index.row());
    const int col = index.column();

    if (role == StrikeRole)
        return r.strike;
    if (role == SortRole) {
        switch (col) {
            case ColStrike:
                return r.strike;
            case ColCeIv:
                return r.ce_iv;
            case ColCeOi:
                return double(r.ce_quote.oi);
            case ColCeChg:
                return r.ce_quote.change_pct;
            case ColPeChg:
                return r.pe_quote.change_pct;
            case ColPeOi:
                return double(r.pe_quote.oi);
            case ColPeIv:
                return r.pe_iv;
        }
        return {};
    }

    if (role == Qt::TextAlignmentRole) {
        if (col == ColStrike)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole) {
        switch (col) {
            case ColCeChg:
                return color_for_pct(r.ce_quote.change_pct);
            case ColPeChg:
                return color_for_pct(r.pe_quote.change_pct);
        }
        if (col == ColStrike && r.is_atm)
            return QColor(colors::AMBER());
    }
    if (role != Qt::DisplayRole)
        return {};

    switch (col) {
        case ColStrike:
            return fincept::services::options::format_strike(r.strike);
        case ColCeIv:
            return scr_fmt_pct(r.ce_iv * 100.0);
        case ColCeOi:
            return scr_fmt_count(r.ce_quote.oi, ccy_);
        case ColCeChg:
            return scr_fmt_signed_pct(r.ce_quote.change_pct);
        case ColPeChg:
            return scr_fmt_signed_pct(r.pe_quote.change_pct);
        case ColPeOi:
            return scr_fmt_count(r.pe_quote.oi, ccy_);
        case ColPeIv:
            return scr_fmt_pct(r.pe_iv * 100.0);
    }
    return {};
}

QVariant ScreenedChainModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    // tr() per-call — owning QHeaderView re-polls on QEvent::LanguageChange.
    const QString headers[ColCount] = {
        tr("Strike"), tr("CE IV"), tr("CE OI"), tr("CE Δ%"), tr("PE Δ%"), tr("PE OI"), tr("PE IV"),
    };
    if (section < 0 || section >= ColCount)
        return {};
    return headers[section];
}

void ScreenedChainModel::set_rows(const QVector<OptionChainRow>& rows, const QString& chain_ccy) {
    ccy_ = chain_ccy.isEmpty() ? fno_ccy() : chain_ccy;
    // Fast path: same strike set (the overwhelmingly common case — a chain
    // republish with unchanged filters). A full model reset on every publish
    // yanks the user's scroll position back to the top and drops the selected
    // row several times a minute, which makes the screener unusable as a live
    // monitor. Merge in place and repaint instead.
    bool same_rows = rows.size() == rows_.size() && !rows_.isEmpty();
    if (same_rows) {
        for (int i = 0; i < rows.size(); ++i) {
            if (std::abs(rows[i].strike - rows_[i].strike) > 1e-6) {
                same_rows = false;
                break;
            }
        }
    }
    if (same_rows) {
        rows_ = rows;
        emit dataChanged(index(0, 0), index(rows_.size() - 1, ColCount - 1));
        return;
    }
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

// ── SubTab ─────────────────────────────────────────────────────────────────

ScreenerSubTab::ScreenerSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoScreenerTab");
    setStyleSheet(QString("#fnoScreenerTab { background:%1; }"
                          "#fnoScreenerHeader { background:%2; border-bottom:1px solid %3; }"
                          "#fnoScreenerLabel { color:%4; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; "
                          "                     letter-spacing:0.4px; background:transparent; }"
                          "#fnoScreenerCount { color:%4; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; padding:6px 12px; }"
                          "QDoubleSpinBox, QSpinBox { background:%2; color:%5; border:1px solid %3; "
                          "                            padding:2px 4px; font-size:" + fincept::ui::fonts::px(-3) + "px; min-width:70px; }"
                          "QTableView { background:%1; color:%5; border:1px solid %3; "
                          "              gridline-color:%3; selection-background-color:%6; selection-color:%5; }"
                          "QHeaderView::section { background:%2; color:%4; border:none; "
                          "                       border-bottom:1px solid %3; padding:5px 8px; "
                          "                       font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:0.4px; }")
                      .arg(colors::BG_BASE(),        // %1
                           colors::BG_RAISED(),      // %2
                           colors::BORDER_DIM(),     // %3
                           colors::TEXT_SECONDARY(), // %4
                           colors::TEXT_PRIMARY(),   // %5
                           colors::BG_HOVER()));     // %6

    setup_ui();
    connect(iv_min_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ScreenerSubTab::on_filter_changed);
    connect(iv_max_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ScreenerSubTab::on_filter_changed);
    connect(distance_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScreenerSubTab::on_filter_changed);
    connect(ce_oi_min_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScreenerSubTab::on_filter_changed);
    connect(pe_oi_min_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScreenerSubTab::on_filter_changed);
}

ScreenerSubTab::~ScreenerSubTab() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void ScreenerSubTab::setup_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Filter strip ──────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName("fnoScreenerHeader");
    auto* hlay = new QHBoxLayout(header);
    hlay->setContentsMargins(12, 8, 12, 8);
    hlay->setSpacing(8);

    auto add_kv = [&](const QString& label, QLabel*& label_out, QWidget* control) {
        auto* l = new QLabel(label.toUpper(), header);
        l->setObjectName("fnoScreenerLabel");
        hlay->addWidget(l);
        hlay->addWidget(control);
        label_out = l;
    };

    iv_min_ = new QDoubleSpinBox(header);
    iv_min_->setRange(0, 200);
    iv_min_->setSuffix("%");
    iv_min_->setSingleStep(1.0);
    iv_min_->setValue(0);
    iv_max_ = new QDoubleSpinBox(header);
    iv_max_->setRange(0, 200);
    iv_max_->setSuffix("%");
    iv_max_->setSingleStep(1.0);
    iv_max_->setValue(200);
    add_kv(tr("IV Min"), iv_min_label_, iv_min_);
    add_kv(tr("IV Max"), iv_max_label_, iv_max_);

    distance_ = new QSpinBox(header);
    distance_->setRange(1, 100);
    distance_->setValue(20);
    add_kv(tr("± Strikes"), distance_label_, distance_);

    ce_oi_min_ = new QSpinBox(header);
    ce_oi_min_->setRange(0, 1000);
    ce_oi_min_->setSuffix(" L");
    ce_oi_min_->setSingleStep(1);
    ce_oi_min_->setValue(0);
    add_kv(tr("CE OI ≥"), ce_oi_label_, ce_oi_min_);

    pe_oi_min_ = new QSpinBox(header);
    pe_oi_min_->setRange(0, 1000);
    pe_oi_min_->setSuffix(" L");
    pe_oi_min_->setSingleStep(1);
    pe_oi_min_->setValue(0);
    add_kv(tr("PE OI ≥"), pe_oi_label_, pe_oi_min_);

    iv_min_->setAccessibleName(tr("Minimum implied volatility"));
    iv_max_->setAccessibleName(tr("Maximum implied volatility"));
    distance_->setAccessibleName(tr("Maximum strikes away from at-the-money"));
    label_oi_unit(); // lakhs until a US chain arrives (apply_oi_unit)
    setTabOrder(iv_min_, iv_max_);
    setTabOrder(iv_max_, distance_);
    setTabOrder(distance_, ce_oi_min_);
    setTabOrder(ce_oi_min_, pe_oi_min_);

    hlay->addStretch(1);
    root->addWidget(header);

    // ── Body: table / skeleton while the chain loads / "needs a chain" page ─
    body_ = new QStackedWidget(this);
    table_ = new QTableView(body_);
    table_->setAccessibleName(tr("Screened strikes"));
    model_ = new ScreenedChainModel(this);
    // A screener ranks: click a header to order by IV / OI / move (numeric sort
    // role). The model's in-place dataChanged path keeps working through the proxy.
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(ScreenedChainModel::SortRole);
    table_->setModel(proxy_);
    table_->setSortingEnabled(true);
    table_->sortByColumn(ScreenedChainModel::ColStrike, Qt::AscendingOrder);
    table_->setShowGrid(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    // 22px rows at the default font size, growing with the user's font.
    const auto row_height = [] { return qMax(22, QFontMetrics(fonts::font()).height() + 6); };
    table_->verticalHeader()->setDefaultSectionSize(row_height());
    // Row height follows the font: re-fit once the appearance change has settled
    // (never inside a theme_changed handler).
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, table_,
            [this, row_height]() { table_->verticalHeader()->setDefaultSectionSize(row_height()); });
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    body_->addWidget(table_);
    skeleton_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Table, body_);
    skeleton_->set_message(fno_chain_loading_caption());
    body_->addWidget(skeleton_);
    no_chain_ = new fincept::ui::EmptyState(body_);
    connect(no_chain_, &fincept::ui::EmptyState::primary_clicked, this, &ScreenerSubTab::open_chain_requested);
    body_->addWidget(no_chain_);
    root->addWidget(body_, 1);

    // ── Footer: count label ───────────────────────────────────────────────
    count_label_ = new QLabel(tr("0 of 0 strikes match"), this);
    count_label_->setObjectName("fnoScreenerCount");
    root->addWidget(count_label_);

    // No chain_published connection: it fired for every chain assembly for the
    // widget's lifetime (hidden or not) and double-handled each publish once the
    // hub subscription in showEvent() was armed.
    const auto& cached = fincept::services::options::OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty())
        on_chain_published(QVariant::fromValue(cached));
    update_body();
}

QVariantMap ScreenerSubTab::save_state() const {
    QVariantMap m;
    m["iv_min"] = iv_min_ ? iv_min_->value() : 0.0;
    m["iv_max"] = iv_max_ ? iv_max_->value() : 200.0;
    m["distance"] = distance_ ? distance_->value() : 20;
    // OI floors per unit: the boxes hold the current unit, the other one is kept aside.
    const int ce = ce_oi_min_ ? ce_oi_min_->value() : 0;
    const int pe = pe_oi_min_ ? pe_oi_min_->value() : 0;
    m["ce_oi"] = oi_us_ ? oi_floor_lakh_[0] : ce; // lakhs (Indian chains)
    m["pe_oi"] = oi_us_ ? oi_floor_lakh_[1] : pe;
    m["ce_oi_us"] = oi_us_ ? ce : oi_floor_us_[0]; // contracts (US chains)
    m["pe_oi_us"] = oi_us_ ? pe : oi_floor_us_[1];
    return m;
}

void ScreenerSubTab::restore_state(const QVariantMap& state) {
    if (state.contains("iv_min"))
        iv_min_->setValue(state.value("iv_min").toDouble());
    if (state.contains("iv_max"))
        iv_max_->setValue(state.value("iv_max").toDouble());
    if (state.contains("distance"))
        distance_->setValue(state.value("distance").toInt());
    if (state.contains("ce_oi"))
        oi_floor_lakh_[0] = std::max(0, state.value("ce_oi").toInt());
    if (state.contains("pe_oi"))
        oi_floor_lakh_[1] = std::max(0, state.value("pe_oi").toInt());
    if (state.contains("ce_oi_us"))
        oi_floor_us_[0] = std::max(0, state.value("ce_oi_us").toInt());
    if (state.contains("pe_oi_us"))
        oi_floor_us_[1] = std::max(0, state.value("pe_oi_us").toInt());
    // The boxes show the unit of the chain on screen (both set before one re-filter).
    const int* floors = oi_us_ ? oi_floor_us_ : oi_floor_lakh_;
    {
        const QSignalBlocker block_ce(ce_oi_min_);
        const QSignalBlocker block_pe(pe_oi_min_);
        ce_oi_min_->setValue(floors[0]);
        pe_oi_min_->setValue(floors[1]);
    }
    apply_filters();
}

void ScreenerSubTab::apply_oi_unit(bool us) {
    if (!ce_oi_min_ || !pe_oi_min_ || us == oi_us_)
        return;
    // Keep the floors of the unit being left; they come back with it.
    int* leaving = oi_us_ ? oi_floor_us_ : oi_floor_lakh_;
    leaving[0] = ce_oi_min_->value();
    leaving[1] = pe_oi_min_->value();
    oi_us_ = us;
    const int* floors = oi_us_ ? oi_floor_us_ : oi_floor_lakh_;
    int i = 0;
    for (QSpinBox* box : {ce_oi_min_, pe_oi_min_}) {
        const QSignalBlocker block(box); // apply_filters() is running: no re-entry
        if (oi_us_) {
            box->setRange(0, 1'000'000);
            box->setSingleStep(50);
        } else {
            box->setRange(0, 1000);
            box->setSingleStep(1);
        }
        box->setValue(floors[i++]);
    }
    label_oi_unit();
}

void ScreenerSubTab::label_oi_unit() {
    if (!ce_oi_min_ || !pe_oi_min_)
        return;
    // US listed options are counted in contracts (a lakh — 100,000 — would filter out
    // nearly every US strike); NSE / BSE OI reads in lakhs of contracts.
    const QString suffix = oi_us_ ? QStringLiteral(" ") + tr("contracts") : QStringLiteral(" L");
    const QString tip = oi_us_ ? tr("Open-interest floor in contracts (US listed options).")
                               : tr("Open-interest floor in lakhs of contracts (1 L = 100,000).");
    for (QSpinBox* box : {ce_oi_min_, pe_oi_min_}) {
        box->setSuffix(suffix);
        box->setToolTip(tip);
    }
    ce_oi_min_->setAccessibleName(oi_us_ ? tr("Minimum call open interest in contracts")
                                         : tr("Minimum call open interest in lakhs"));
    pe_oi_min_->setAccessibleName(oi_us_ ? tr("Minimum put open interest in contracts")
                                         : tr("Minimum put open interest in lakhs"));
}

void ScreenerSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    QPointer<ScreenerSubTab> self = this;
    fincept::datahub::DataHub::instance().subscribe_pattern(this, QStringLiteral("option:chain:*"),
                                                            [self](const QString& /*topic*/, const QVariant& v) {
                                                                if (!self)
                                                                    return;
                                                                self->on_chain_published(v);
                                                            });
    subscribed_ = true;
    // Catch up on a snapshot published while this tab was hidden.
    const auto& cached = fincept::services::options::OptionChainService::instance().last_chain();
    if (!cached.rows.isEmpty() && cached.timestamp_ms != last_chain_.timestamp_ms)
        on_chain_published(QVariant::fromValue(cached));
}

void ScreenerSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // §D3: pair the showEvent subscribe. The `subscribed_` latch never reset
    // and on_chain_published() has no visibility gate, so a hidden tab ran a
    // full apply_filters() rebuild on every option-chain publish for the rest
    // of the process.
    //
    // unsubscribe_pattern() rather than the blanket unsubscribe(this) so this
    // stays symmetric with the subscribe_pattern() above and can't collaterally
    // drop any concrete-topic subscription added here later.
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    subscribed_ = false;
}

void ScreenerSubTab::on_chain_published(const QVariant& v) {
    if (!v.canConvert<OptionChain>())
        return;
    const OptionChain incoming = v.value<OptionChain>();
    // Every option:chain:* publish arrives here — screen only the Chain tab's series.
    if (!fincept::services::options::OptionChainService::instance().is_focus_chain(incoming))
        return;
    last_chain_ = incoming;
    apply_filters();
}

void ScreenerSubTab::set_external_chain(const OptionChain& chain) {
    last_chain_ = chain;
    apply_filters();
}

void ScreenerSubTab::set_chain_status(FnoChainStatus status, const QString& reason) {
    chain_status_ = status;
    chain_reason_ = reason;
    update_body();
}

void ScreenerSubTab::update_body() {
    if (!body_)
        return;
    if (!last_chain_.rows.isEmpty()) {
        body_->setCurrentWidget(table_);
        return;
    }
    // Loading — or the Chain tab already shows a chain this tab is about to receive.
    if (chain_status_ != FnoChainStatus::Unavailable) {
        body_->setCurrentWidget(skeleton_);
        return;
    }
    no_chain_->apply(fno_no_chain_spec(
        tr("Chain screener"),
        tr("Filters the chain on the Chain tab by implied volatility, distance from the money and open interest."),
        chain_reason_, fincept::ui::LoadingSkeleton::Shape::Table));
    body_->setCurrentWidget(no_chain_);
}

void ScreenerSubTab::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ScreenerSubTab::retranslateUi() {
    if (iv_min_label_)
        iv_min_label_->setText(tr("IV Min").toUpper());
    if (iv_max_label_)
        iv_max_label_->setText(tr("IV Max").toUpper());
    if (distance_label_)
        distance_label_->setText(tr("± Strikes").toUpper());
    if (ce_oi_label_)
        ce_oi_label_->setText(tr("CE OI ≥").toUpper());
    if (pe_oi_label_)
        pe_oi_label_->setText(tr("PE OI ≥").toUpper());
    label_oi_unit();
    // Refresh the count label string in the new language.
    apply_filters();
}

void ScreenerSubTab::on_filter_changed() {
    apply_filters();
}

void ScreenerSubTab::apply_filters() {
    update_body();
    if (last_chain_.rows.isEmpty()) {
        model_->set_rows({});
        // Distinguish "no chain loaded yet" (the page above says what to do) from
        // "chain loaded, nothing passes your filters".
        count_label_->setText(tr("No chain loaded yet."));
        return;
    }
    int atm = 0;
    for (int i = 0; i < last_chain_.rows.size(); ++i)
        if (last_chain_.rows[i].is_atm) {
            atm = i;
            break;
        }

    const double iv_lo = iv_min_->value() / 100.0;
    const double iv_hi = iv_max_->value() / 100.0;
    const int dist = distance_->value();
    // OI floors in the chain's unit: contracts on a US chain, lakhs on an Indian one.
    apply_oi_unit(fno_is_us_source(last_chain_.broker_id));
    const qint64 oi_scale = oi_us_ ? 1 : 100'000;
    const qint64 ce_oi_floor = qint64(ce_oi_min_->value()) * oi_scale;
    const qint64 pe_oi_floor = qint64(pe_oi_min_->value()) * oi_scale;

    QVector<OptionChainRow> filtered;
    for (int i = 0; i < last_chain_.rows.size(); ++i) {
        if (std::abs(i - atm) > dist)
            continue;
        const auto& r = last_chain_.rows.at(i);
        // IV gate — pass when EITHER side has IV in band; rows with both
        // IVs zero are rejected unless the band starts at 0 (i.e. user
        // explicitly opted in).
        const double max_iv = std::max(r.ce_iv, r.pe_iv);
        if (iv_lo > 0 || iv_hi < 2.0) {
            if (max_iv < iv_lo - kEps || max_iv > iv_hi + kEps)
                continue;
        }
        if (r.ce_quote.oi < ce_oi_floor)
            continue;
        if (r.pe_quote.oi < pe_oi_floor)
            continue;
        filtered.append(r);
    }

    // Preserve the user's place across a filter change (which does force a
    // model reset). Selection is remembered by STRIKE, not row index — the
    // filtered set is a different length every time the thresholds move.
    double keep_strike = 0;
    if (const QModelIndex cur = table_->currentIndex(); cur.isValid() && cur.row() < model_->rowCount())
        keep_strike = cur.data(ScreenedChainModel::StrikeRole).toDouble();
    const int keep_scroll = table_->verticalScrollBar() ? table_->verticalScrollBar()->value() : 0;

    model_->set_rows(filtered, fno_chain_ccy(last_chain_.broker_id));

    bool reselected = false;
    if (keep_strike > 0) {
        for (int i = 0; i < filtered.size(); ++i) {
            if (std::abs(filtered[i].strike - keep_strike) < 1e-6) {
                // Source row -> the row the (possibly sorted) view shows.
                const QModelIndex at = proxy_->mapFromSource(model_->index(i, 0));
                table_->selectRow(at.row());
                table_->scrollTo(at, QAbstractItemView::PositionAtCenter);
                reselected = true;
                break;
            }
        }
    }
    // Nothing to re-centre on — hold the scroll position instead of snapping
    // back to the top on every republish.
    if (!reselected && table_->verticalScrollBar())
        table_->verticalScrollBar()->setValue(qMin(keep_scroll, table_->verticalScrollBar()->maximum()));

    if (filtered.isEmpty())
        count_label_->setText(tr("No strikes match — widen the IV band, the ± strike range, "
                                 "or lower the OI floors. (%1 strikes in the chain.)")
                                  .arg(last_chain_.rows.size()));
    else
        count_label_->setText(tr("%1 of %2 strikes match").arg(filtered.size()).arg(last_chain_.rows.size()));
}

} // namespace fincept::screens::fno
