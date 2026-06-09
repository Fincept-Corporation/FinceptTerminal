// src/ui/widgets/algo/FnoLegRuleEditor.cpp
#include "ui/widgets/algo/FnoLegRuleEditor.h"

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
constexpr int kColSide   = 0;
constexpr int kColType   = 1;
constexpr int kColStrike = 2;
constexpr int kColValue  = 3;
constexpr int kColLots   = 4;
constexpr int kColDelete = 5;
constexpr int kNumCols   = 6;

const char* kStyle = R"(
QWidget#fnoLegRuleEditor {
    background: #1a1a2e;
    color: #e0e0e0;
}
QTableWidget {
    background: #12121f;
    color: #e0e0e0;
    gridline-color: #2a2a4a;
    border: 1px solid #2a2a4a;
    selection-background-color: #2a2a6a;
    selection-color: #ffffff;
    font-size: 12px;
}
QTableWidget::item {
    padding: 2px 4px;
}
QHeaderView::section {
    background: #1a1a3a;
    color: #a0a0c0;
    border: none;
    border-bottom: 1px solid #2a2a4a;
    padding: 3px 6px;
    font-size: 11px;
    font-weight: bold;
}
QComboBox {
    background: #1e1e38;
    color: #e0e0e0;
    border: 1px solid #3a3a5a;
    border-radius: 3px;
    padding: 1px 4px;
    font-size: 12px;
}
QComboBox QAbstractItemView {
    background: #1a1a30;
    color: #e0e0e0;
    selection-background-color: #3a3a7a;
}
QDoubleSpinBox, QSpinBox {
    background: #1e1e38;
    color: #e0e0e0;
    border: 1px solid #3a3a5a;
    border-radius: 3px;
    padding: 1px 4px;
    font-size: 12px;
}
QPushButton#addLegBtn {
    background: #1e3a5a;
    color: #a0d0ff;
    border: 1px solid #2a5a8a;
    border-radius: 4px;
    padding: 4px 12px;
    font-size: 12px;
}
QPushButton#addLegBtn:hover {
    background: #2a4a7a;
}
QPushButton#deleteLegBtn {
    background: #3a1a1a;
    color: #ff8080;
    border: 1px solid #6a2a2a;
    border-radius: 3px;
    padding: 1px 6px;
    font-size: 11px;
}
QPushButton#deleteLegBtn:hover {
    background: #5a2a2a;
}
)";

} // namespace

FnoLegRuleEditor::FnoLegRuleEditor(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("fnoLegRuleEditor"));
    setStyleSheet(QString::fromLatin1(kStyle));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // ── Table ────────────────────────────────────────────────────────────────
    table_ = new QTableWidget(0, kNumCols, this);
    table_->setObjectName(QStringLiteral("fnoLegTable"));
    table_->setHorizontalHeaderLabels(
        {tr("B/S"), tr("Type"), tr("Strike"), tr("Value"), tr("Lots"), tr("")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setDefaultSectionSize(90);
    table_->setColumnWidth(kColSide,   70);
    table_->setColumnWidth(kColType,   60);
    table_->setColumnWidth(kColStrike, 100);
    table_->setColumnWidth(kColValue,  90);
    table_->setColumnWidth(kColLots,   70);
    table_->setColumnWidth(kColDelete, 32);
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
    add_btn->setFixedHeight(26);
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
    table_->insertRow(row);
    table_->setRowHeight(row, 28);

    // B/S combo
    auto* side_cb = new QComboBox(this);
    side_cb->addItems({QStringLiteral("BUY"), QStringLiteral("SELL")});
    side_cb->setCurrentText(leg.side.isEmpty() ? QStringLiteral("BUY") : leg.side);
    table_->setCellWidget(row, kColSide, side_cb);

    // Type combo
    auto* type_cb = new QComboBox(this);
    type_cb->addItems({QStringLiteral("CE"), QStringLiteral("PE"), QStringLiteral("FUT")});
    type_cb->setCurrentText(leg.kind.isEmpty() ? QStringLiteral("CE") : leg.kind);
    table_->setCellWidget(row, kColType, type_cb);

    // Strike mode combo
    auto* strike_cb = new QComboBox(this);
    strike_cb->addItems({QStringLiteral("ATM"), QStringLiteral("ATM_OFFSET"),
                         QStringLiteral("DELTA"), QStringLiteral("ABSOLUTE")});
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
    del_btn->setFixedSize(26, 22);
    table_->setCellWidget(row, kColDelete, del_btn);

    // Connect change signals — use this->table_ capture to scan for current row
    // at click time (avoids stale index after row removal).
    connect(side_cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit legs_changed(); });
    connect(type_cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit legs_changed(); });
    connect(strike_cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { emit legs_changed(); });
    connect(val_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { emit legs_changed(); });
    connect(lots_spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { emit legs_changed(); });

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
