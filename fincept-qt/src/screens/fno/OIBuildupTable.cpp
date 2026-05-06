#include "screens/fno/OIBuildupTable.h"

#include "ui/theme/Theme.h"

#include <QColor>
#include <QHeaderView>

#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

constexpr double kEps = 1e-6;

QString fmt_signed_pct(double v) {
    if (std::abs(v) < kEps)
        return QStringLiteral("—");
    return (v > 0 ? QStringLiteral("+") : QString()) + QString::number(v, 'f', 2) + QStringLiteral("%");
}

QColor color_for_pct(double v) {
    if (v > kEps)
        return QColor(colors::POSITIVE());
    if (v < -kEps)
        return QColor(colors::NEGATIVE());
    return QColor(colors::TEXT_SECONDARY());
}

}  // namespace

// ── Model ──────────────────────────────────────────────────────────────────

OIBuildupModel::OIBuildupModel(QObject* parent) : QAbstractTableModel(parent) {}

int OIBuildupModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return chain_.rows.size();
}

int OIBuildupModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return ColCount;
}

OIBuildupModel::Buildup OIBuildupModel::classify(double price_chg, double oi_chg) {
    if (std::abs(price_chg) < kEps || std::abs(oi_chg) < kEps)
        return Buildup::None;
    if (price_chg > 0 && oi_chg > 0)
        return Buildup::LongBuildup;
    if (price_chg < 0 && oi_chg > 0)
        return Buildup::ShortBuildup;
    if (price_chg > 0 && oi_chg < 0)
        return Buildup::ShortCovering;
    return Buildup::LongUnwinding;
}

QString OIBuildupModel::class_str(Buildup b) {
    switch (b) {
        case Buildup::LongBuildup:    return QStringLiteral("Long Build-up");
        case Buildup::ShortBuildup:   return QStringLiteral("Short Build-up");
        case Buildup::ShortCovering:  return QStringLiteral("Short Covering");
        case Buildup::LongUnwinding:  return QStringLiteral("Long Unwinding");
        default:                      return QStringLiteral("—");
    }
}

QColor OIBuildupModel::class_color(Buildup b) {
    switch (b) {
        case Buildup::LongBuildup:    return QColor(colors::POSITIVE());
        case Buildup::ShortBuildup:   return QColor(colors::NEGATIVE());
        case Buildup::ShortCovering:  return QColor(colors::AMBER());
        case Buildup::LongUnwinding:  return QColor(colors::AMBER());
        default:                      return QColor(colors::TEXT_SECONDARY());
    }
}

QVariant OIBuildupModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= chain_.rows.size())
        return {};
    const OptionChainRow& r = chain_.rows.at(index.row());
    const int col = index.column();

    if (role == Qt::TextAlignmentRole) {
        if (col == ColStrike)
            return int(Qt::AlignCenter | Qt::AlignVCenter);
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole) {
        switch (col) {
            case ColCeClass:
                return class_color(classify(r.ce_quote.change_pct, r.ce_quote.oi_change_pct));
            case ColPeClass:
                return class_color(classify(r.pe_quote.change_pct, r.pe_quote.oi_change_pct));
            case ColCeChgPct:    return color_for_pct(r.ce_quote.change_pct);
            case ColCeOiChgPct:  return color_for_pct(r.ce_quote.oi_change_pct);
            case ColPeChgPct:    return color_for_pct(r.pe_quote.change_pct);
            case ColPeOiChgPct:  return color_for_pct(r.pe_quote.oi_change_pct);
        }
        if (col == ColStrike && r.is_atm)
            return QColor(colors::AMBER());
    }
    if (role != Qt::DisplayRole)
        return {};

    switch (col) {
        case ColStrike:
            return QString::number(r.strike, 'f', r.strike < 100 ? 2 : 0);
        case ColCeClass:
            return class_str(classify(r.ce_quote.change_pct, r.ce_quote.oi_change_pct));
        case ColCeChgPct:    return fmt_signed_pct(r.ce_quote.change_pct);
        case ColCeOiChgPct:  return fmt_signed_pct(r.ce_quote.oi_change_pct);
        case ColPeClass:
            return class_str(classify(r.pe_quote.change_pct, r.pe_quote.oi_change_pct));
        case ColPeChgPct:    return fmt_signed_pct(r.pe_quote.change_pct);
        case ColPeOiChgPct:  return fmt_signed_pct(r.pe_quote.oi_change_pct);
    }
    return {};
}

QVariant OIBuildupModel::headerData(int section, Qt::Orientation orient, int role) const {
    if (orient != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const char* kHeaders[ColCount] = {
        "Strike",
        "CE Action", "CE Δ%", "CE ΔOI%",
        "PE Action", "PE Δ%", "PE ΔOI%",
    };
    if (section < 0 || section >= ColCount)
        return {};
    return QString::fromLatin1(kHeaders[section]);
}

void OIBuildupModel::set_chain(const OptionChain& chain) {
    beginResetModel();
    chain_ = chain;
    endResetModel();
}

// ── View ───────────────────────────────────────────────────────────────────

OIBuildupTable::OIBuildupTable(QWidget* parent) : QTableView(parent) {
    model_ = new OIBuildupModel(this);
    setModel(model_);

    setShowGrid(false);
    setAlternatingRowColors(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(22);

    setStyleSheet(QStringLiteral(
        "QTableView { background:%1; color:%2; border:1px solid %3; gridline-color:%3; selection-background-color:%4; }"
        "QHeaderView::section { background:%5; color:%6; border:none; border-bottom:1px solid %3; padding:4px 6px; "
        "                       font-size:9px; font-weight:700; letter-spacing:0.4px; }"
        "QTableView::item { padding:0 6px; }")
                      .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(),
                           colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY()));
}

} // namespace fincept::screens::fno
