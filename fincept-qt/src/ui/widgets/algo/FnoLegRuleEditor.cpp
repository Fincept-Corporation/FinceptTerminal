// src/ui/widgets/algo/FnoLegRuleEditor.cpp
#include "ui/widgets/algo/FnoLegRuleEditor.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QStringLiteral>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::ui::algo {

namespace {

// Column indices
constexpr int kColSide = 0;
constexpr int kColType = 1;
constexpr int kColStrike = 2;
constexpr int kColValue = 3;
constexpr int kColLots = 4;
constexpr int kColDelete = 5;
constexpr int kNumCols = 6;

QString build_style() {
    using namespace fincept::ui;
    return QString(R"(
QWidget#fnoLegRuleEditor {
    background: %1;
    color: %2;
}
QTableWidget {
    background: %3;
    color: %2;
    gridline-color: %4;
    border: 1px solid %5;
    selection-background-color: %6;
    selection-color: %2;
    font-size: )" + fonts::px(-2) + R"(px;
}
QTableWidget::item {
    padding: 2px 4px;
}
QHeaderView::section {
    background: %7;
    color: %8;
    border: none;
    border-bottom: 1px solid %5;
    padding: 3px 6px;
    font-size: )" + fonts::px(-3) + R"(px;
    font-weight: bold;
}
QComboBox {
    background: %7;
    color: %2;
    border: 1px solid %5;
    border-radius: 3px;
    padding: 1px 4px;
    font-size: )" + fonts::px(-2) + R"(px;
}
QComboBox QAbstractItemView {
    background: %1;
    color: %2;
    selection-background-color: %6;
    selection-color: %2;
}
QDoubleSpinBox, QSpinBox {
    background: %7;
    color: %2;
    border: 1px solid %5;
    border-radius: 3px;
    padding: 1px 4px;
    font-size: )" + fonts::px(-2) + R"(px;
}
QPushButton#addLegBtn {
    background: %9;
    color: %10;
    border: 1px solid %11;
    border-radius: 4px;
    padding: 4px 12px;
    font-size: )" + fonts::px(-2) + R"(px;
}
QPushButton#addLegBtn:hover {
    background: %12;
}
QPushButton#deleteLegBtn {
    background: %13;
    color: %14;
    border: 1px solid %15;
    border-radius: 3px;
    padding: 1px 6px;
    font-size: )" + fonts::px(-3) + R"(px;
}
QPushButton#deleteLegBtn:hover {
    background: %16;
}
)")
        .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BG_BASE(), colors::BORDER_DIM(),
             colors::BORDER_MED(), colors::BG_HOVER(), colors::BG_RAISED(), colors::TEXT_SECONDARY(),
             colors::tint(colors::INFO, 0.16))
        .arg(colors::INFO(), colors::alpha(colors::INFO, 0.5), colors::tint(colors::INFO, 0.28),
             colors::tint(colors::DANGER, 0.14), colors::DANGER(), colors::alpha(colors::DANGER, 0.5),
             colors::tint(colors::DANGER, 0.28));
}

} // namespace

FnoLegRuleEditor::FnoLegRuleEditor(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("fnoLegRuleEditor"));
    setStyleSheet(build_style());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // ── Table ────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(0, kNumCols, this);
    table_->setObjectName(QStringLiteral("fnoLegTable"));
    table_->setHorizontalHeaderLabels({tr("B/S"), tr("Type"), tr("Strike"), tr("Value"), tr("Lots"), tr("")});
    table_->horizontalHeader()->setStretchLastSection(false);
    // Column widths hold the cell editors' text: sized for the default font
    // and widened with the user's font size.
    const auto apply_column_widths = [this]() {
        table_->horizontalHeader()->setDefaultSectionSize(ui::metrics::width(90));
        table_->setColumnWidth(kColSide, ui::metrics::width(70));
        table_->setColumnWidth(kColType, ui::metrics::width(60));
        table_->setColumnWidth(kColStrike, ui::metrics::width(100));
        table_->setColumnWidth(kColValue, ui::metrics::width(90));
        table_->setColumnWidth(kColLots, ui::metrics::width(70));
        table_->setColumnWidth(kColDelete, ui::metrics::width(32));
    };
    apply_column_widths();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [apply_column_widths](const ui::ThemeTokens&) { apply_column_widths(); });
    ui::metrics::follow_row_height(table_, 28);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(false);
    table_->setShowGrid(true);
    layout->addWidget(table_);

    // ── Add Leg button ───────────────────────────────────────────────────────
    auto* btn_row = new QHBoxLayout();
    btn_row->setContentsMargins(0, 2, 0, 0);
    auto* add_btn = new QPushButton(tr("Add Leg"), this);
    add_btn->setObjectName(QStringLiteral("addLegBtn"));
    add_btn->setMinimumHeight(26);
    btn_row->addWidget(add_btn);
    btn_row->addStretch();
    layout->addLayout(btn_row);

    connect(add_btn, &QPushButton::clicked, this, [this]() {
        fincept::algo::fno::AlgoFnoLeg def;
        def.kind = QStringLiteral("CE");
        def.side = QStringLiteral("BUY");
        def.lots = 1;
        def.strike_mode = QStringLiteral("ATM");
        def.strike_value = 0;
        add_row(def);
        emit legs_changed();
    });
}

void FnoLegRuleEditor::add_row(const fincept::algo::fno::AlgoFnoLeg& leg) {
    const int row = table_->rowCount();
    table_->insertRow(row); // height from metrics::follow_row_height (constructor)

    // B/S combo
    auto* side_cb = new QComboBox(this);
    side_cb->addItems({QStringLiteral("BUY"), QStringLiteral("SELL")});
    side_cb->setCurrentText(leg.side.isEmpty() ? QStringLiteral("BUY") : leg.side);
    table_->setCellWidget(row, kColSide, side_cb);

    // Type combo
    auto* type_cb = new QComboBox(this);
    // Only option legs can be resolved from the chain and traded. FUT is listed
    // solely when a saved rule already uses it, so the rule is shown truthfully
    // (deploy refuses it) instead of being silently rewritten to CE.
    type_cb->addItems({QStringLiteral("CE"), QStringLiteral("PE")});
    if (leg.kind == QLatin1String("FUT")) {
        type_cb->addItem(QStringLiteral("FUT"));
        type_cb->setItemData(type_cb->count() - 1, tr("Futures legs are not supported yet — change this leg to CE or PE"),
                             Qt::ToolTipRole);
    }
    type_cb->setCurrentText(leg.kind.isEmpty() ? QStringLiteral("CE") : leg.kind);
    table_->setCellWidget(row, kColType, type_cb);

    // Strike mode combo
    auto* strike_cb = new QComboBox(this);
    strike_cb->addItems(
        {QStringLiteral("ATM"), QStringLiteral("ATM_OFFSET"), QStringLiteral("DELTA"), QStringLiteral("ABSOLUTE")});
    strike_cb->setCurrentText(leg.strike_mode.isEmpty() ? QStringLiteral("ATM") : leg.strike_mode);
    table_->setCellWidget(row, kColStrike, strike_cb);

    // Strike value double-spin
    auto* val_spin = new QDoubleSpinBox(this);
    val_spin->setRange(-1e6, 1e6);
    val_spin->setDecimals(2);
    val_spin->setSingleStep(1.0);
    val_spin->setValue(leg.strike_value);
    table_->setCellWidget(row, kColValue, val_spin);

    // Lots spin
    auto* lots_spin = new QSpinBox(this);
    lots_spin->setRange(1, 10000);
    lots_spin->setValue(leg.lots < 1 ? 1 : leg.lots);
    table_->setCellWidget(row, kColLots, lots_spin);

    // Delete button
    auto* del_btn = new QPushButton(QStringLiteral("✕"), this);
    del_btn->setObjectName(QStringLiteral("deleteLegBtn"));
    ui::metrics::follow_fixed_size(del_btn, 26, 22);
    table_->setCellWidget(row, kColDelete, del_btn);

    // Connect change signals — use this->table_ capture to scan for current row
    // at click time (avoids stale index after row removal).
    connect(side_cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit legs_changed(); });
    connect(type_cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit legs_changed(); });
    connect(strike_cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { emit legs_changed(); });
    connect(val_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit legs_changed(); });
    connect(lots_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { emit legs_changed(); });

    // Delete: find the row by scanning cellWidget at click time
    connect(del_btn, &QPushButton::clicked, this, [this, del_btn]() {
        for (int r = 0; r < table_->rowCount(); ++r) {
            if (table_->cellWidget(r, kColDelete) == del_btn) {
                table_->removeRow(r);
                emit legs_changed();
                return;
            }
        }
    });
}

fincept::algo::fno::AlgoFnoLeg FnoLegRuleEditor::row_to_leg(int row) const {
    fincept::algo::fno::AlgoFnoLeg leg;

    if (auto* w = qobject_cast<QComboBox*>(table_->cellWidget(row, kColSide)))
        leg.side = w->currentText();
    if (auto* w = qobject_cast<QComboBox*>(table_->cellWidget(row, kColType)))
        leg.kind = w->currentText();
    if (auto* w = qobject_cast<QComboBox*>(table_->cellWidget(row, kColStrike)))
        leg.strike_mode = w->currentText();
    if (auto* w = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, kColValue)))
        leg.strike_value = w->value();
    if (auto* w = qobject_cast<QSpinBox*>(table_->cellWidget(row, kColLots)))
        leg.lots = w->value();

    return leg;
}

QVector<fincept::algo::fno::AlgoFnoLeg> FnoLegRuleEditor::legs() const {
    QVector<fincept::algo::fno::AlgoFnoLeg> result;
    result.reserve(table_->rowCount());
    for (int r = 0; r < table_->rowCount(); ++r)
        result.append(row_to_leg(r));
    return result;
}

void FnoLegRuleEditor::set_legs(const QVector<fincept::algo::fno::AlgoFnoLeg>& legs) {
    table_->setRowCount(0); // clears all rows and their cell widgets
    for (const auto& leg : legs)
        add_row(leg);
    // Intentionally NOT emitting legs_changed() — programmatic load.
}

void FnoLegRuleEditor::clear_legs() {
    table_->setRowCount(0);
}

} // namespace fincept::ui::algo
