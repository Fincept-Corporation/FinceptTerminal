#include "screens/fno/ScreenerSubTab.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QColor>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QShowEvent>
#include <QSpinBox>
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

QString fmt_int_compact(qint64 v) {
    if (v == 0)
        return QStringLiteral("—");
    const double a = std::abs(double(v));
    if (a >= 1e7) return QString::number(v / 1.0e7, 'f', 2) + "Cr";
    if (a >= 1e5) return QString::number(v / 1.0e5, 'f', 2) + "L";
    if (a >= 1e3) return QString::number(v / 1.0e3, 'f', 1) + "k";
    return QLocale(QLocale::English).toString(v);
}

QString fmt_pct(double v) {
    if (std::abs(v) < kEps)
        return QStringLiteral("—");
    return QString::number(v, 'f', 2) + "%";
}

QString fmt_signed_pct(double v) {
    if (std::abs(v) < kEps)
        return QStringLiteral("—");
    return (v > 0 ? "+" : "") + QString::number(v, 'f', 2) + "%";
}

QColor color_for_pct(double v) {
    if (v > kEps)  return QColor(colors::POSITIVE());
    if (v < -kEps) return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

}  // namespace

// ── Model ──────────────────────────────────────────────────────────────────

ScreenedChainModel::ScreenedChainModel(QObject* parent) : QAbstractTableModel(parent) {}

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

    if (role == Qt::TextAlignmentRole) {
        if (col == ColStrike)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole) {
        switch (col) {
            case ColCeChg: return color_for_pct(r.ce_quote.change_pct);
            case ColPeChg: return color_for_pct(r.pe_quote.change_pct);
        }
        if (col == ColStrike && r.is_atm)
            return QColor(colors::AMBER());
    }
    if (role != Qt::DisplayRole)
        return {};

    switch (col) {
        case ColStrike: return QString::number(r.strike, 'f', r.strike < 100 ? 2 : 0);
        case ColCeIv:   return fmt_pct(r.ce_iv * 100.0);
        case ColCeOi:   return fmt_int_compact(r.ce_quote.oi);
        case ColCeChg:  return fmt_signed_pct(r.ce_quote.change_pct);
        case ColPeChg:  return fmt_signed_pct(r.pe_quote.change_pct);
        case ColPeOi:   return fmt_int_compact(r.pe_quote.oi);
        case ColPeIv:   return fmt_pct(r.pe_iv * 100.0);
    }
    return {};
}

QVariant ScreenedChainModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* kHeaders[ColCount] = {
        "Strike", "CE IV", "CE OI", "CE Δ%", "PE Δ%", "PE OI", "PE IV",
    };
    if (section < 0 || section >= ColCount)
        return {};
    return QString::fromLatin1(kHeaders[section]);
}

void ScreenedChainModel::set_rows(const QVector<OptionChainRow>& rows) {
    beginResetModel();
    rows_ = rows;
    endResetModel();
}

// ── SubTab ─────────────────────────────────────────────────────────────────

ScreenerSubTab::ScreenerSubTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fnoScreenerTab");
    setStyleSheet(
        QString("#fnoScreenerTab { background:%1; }"
                "#fnoScreenerHeader { background:%2; border-bottom:1px solid %3; }"
                "#fnoScreenerLabel { color:%4; font-size:9px; font-weight:700; "
                "                     letter-spacing:0.4px; background:transparent; }"
                "#fnoScreenerCount { color:%4; font-size:10px; background:transparent; padding:6px 12px; }"
                "QDoubleSpinBox, QSpinBox { background:%2; color:%5; border:1px solid %3; "
                "                            padding:2px 4px; font-size:11px; min-width:70px; }"
                "QTableView { background:%1; color:%5; border:1px solid %3; "
                "              gridline-color:%3; selection-background-color:%6; }"
                "QHeaderView::section { background:%2; color:%4; border:none; "
                "                       border-bottom:1px solid %3; padding:5px 8px; "
                "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }")
            .arg(colors::BG_BASE(),         // %1
                 colors::BG_RAISED(),       // %2
                 colors::BORDER_DIM(),      // %3
                 colors::TEXT_SECONDARY(),  // %4
                 colors::TEXT_PRIMARY(),    // %5
                 colors::BG_HOVER()));      // %6

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

    auto add_kv = [&](const QString& label, QWidget* control) {
        auto* l = new QLabel(label.toUpper(), header);
        l->setObjectName("fnoScreenerLabel");
        hlay->addWidget(l);
        hlay->addWidget(control);
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
    add_kv("IV Min", iv_min_);
    add_kv("IV Max", iv_max_);

    distance_ = new QSpinBox(header);
    distance_->setRange(1, 100);
    distance_->setValue(20);
    add_kv("± Strikes", distance_);

    ce_oi_min_ = new QSpinBox(header);
    ce_oi_min_->setRange(0, 1000);
    ce_oi_min_->setSuffix(" L");
    ce_oi_min_->setSingleStep(1);
    ce_oi_min_->setValue(0);
    add_kv("CE OI ≥", ce_oi_min_);

    pe_oi_min_ = new QSpinBox(header);
    pe_oi_min_->setRange(0, 1000);
    pe_oi_min_->setSuffix(" L");
    pe_oi_min_->setSingleStep(1);
    pe_oi_min_->setValue(0);
    add_kv("PE OI ≥", pe_oi_min_);

    hlay->addStretch(1);
    root->addWidget(header);

    // ── Body: table ───────────────────────────────────────────────────────
    table_ = new QTableView(this);
    model_ = new ScreenedChainModel(this);
    table_->setModel(model_);
    table_->setShowGrid(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->setVisible(false);
    table_->verticalHeader()->setDefaultSectionSize(22);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    root->addWidget(table_, 1);

    // ── Footer: count label ───────────────────────────────────────────────
    count_label_ = new QLabel(QStringLiteral("0 of 0 strikes match"), this);
    count_label_->setObjectName("fnoScreenerCount");
    root->addWidget(count_label_);
}

QVariantMap ScreenerSubTab::save_state() const {
    QVariantMap m;
    m["iv_min"] = iv_min_ ? iv_min_->value() : 0.0;
    m["iv_max"] = iv_max_ ? iv_max_->value() : 200.0;
    m["distance"] = distance_ ? distance_->value() : 20;
    m["ce_oi"] = ce_oi_min_ ? ce_oi_min_->value() : 0;
    m["pe_oi"] = pe_oi_min_ ? pe_oi_min_->value() : 0;
    return m;
}

void ScreenerSubTab::restore_state(const QVariantMap& state) {
    if (state.contains("iv_min"))  iv_min_->setValue(state.value("iv_min").toDouble());
    if (state.contains("iv_max"))  iv_max_->setValue(state.value("iv_max").toDouble());
    if (state.contains("distance")) distance_->setValue(state.value("distance").toInt());
    if (state.contains("ce_oi"))   ce_oi_min_->setValue(state.value("ce_oi").toInt());
    if (state.contains("pe_oi"))   pe_oi_min_->setValue(state.value("pe_oi").toInt());
}

void ScreenerSubTab::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (subscribed_)
        return;
    QPointer<ScreenerSubTab> self = this;
    fincept::datahub::DataHub::instance().subscribe_pattern(
        this, QStringLiteral("option:chain:*"),
        [self](const QString& /*topic*/, const QVariant& v) {
            if (!self) return;
            self->on_chain_published(v);
        });
    subscribed_ = true;
}

void ScreenerSubTab::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (!subscribed_)
        return;
    fincept::datahub::DataHub::instance().unsubscribe_pattern(this, QStringLiteral("option:chain:*"));
    subscribed_ = false;
}

void ScreenerSubTab::on_chain_published(const QVariant& v) {
    if (!v.canConvert<OptionChain>())
        return;
    last_chain_ = v.value<OptionChain>();
    apply_filters();
}

void ScreenerSubTab::on_filter_changed() {
    apply_filters();
}

void ScreenerSubTab::apply_filters() {
    if (last_chain_.rows.isEmpty()) {
        model_->set_rows({});
        count_label_->setText(QStringLiteral("0 of 0 strikes match"));
        return;
    }
    int atm = 0;
    for (int i = 0; i < last_chain_.rows.size(); ++i)
        if (last_chain_.rows[i].is_atm) { atm = i; break; }

    const double iv_lo = iv_min_->value() / 100.0;
    const double iv_hi = iv_max_->value() / 100.0;
    const int dist = distance_->value();
    const qint64 ce_oi_floor = qint64(ce_oi_min_->value()) * 100'000;
    const qint64 pe_oi_floor = qint64(pe_oi_min_->value()) * 100'000;

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
    model_->set_rows(filtered);
    count_label_->setText(QString("%1 of %2 strikes match")
                              .arg(filtered.size())
                              .arg(last_chain_.rows.size()));
}

} // namespace fincept::screens::fno
