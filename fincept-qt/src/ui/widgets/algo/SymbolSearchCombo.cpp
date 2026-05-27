// src/ui/widgets/algo/SymbolSearchCombo.cpp
#include "ui/widgets/algo/SymbolSearchCombo.h"
#include "services/algo_trading/AlgoTradingTypes.h"

namespace fincept::ui::algo {

SymbolSearchCombo::SymbolSearchCombo(QWidget* parent)
    : QComboBox(parent) {
    setObjectName(QStringLiteral("symbolSearchCombo"));
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);

    model_ = new QStringListModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setFilterMode(Qt::MatchContains);
    setCompleter(completer_);

    QStringList defaults;
    defaults << services::algo::nifty50_symbols() << services::algo::bank_nifty_symbols();
    defaults.removeDuplicates();
    defaults.sort();
    set_symbols(defaults);
}

void SymbolSearchCombo::set_symbols(const QStringList& symbols) {
    clear();
    addItems(symbols);
    model_->setStringList(symbols);
}

} // namespace fincept::ui::algo
