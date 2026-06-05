// src/ui/widgets/algo/SymbolSearchCombo.cpp
#include "ui/widgets/algo/SymbolSearchCombo.h"
#include "services/algo_trading/AlgoTradingTypes.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>

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

    // The drop-down view and the completer popup are separate top-level lists the
    // global stylesheet never reaches; style both so their rows stay readable in
    // the dark theme instead of rendering near-invisible, clipped text.
    const QString list_qss =
        QString("QAbstractItemView { background:%1; color:%2; border:1px solid %3; "
                "outline:0; selection-background-color:%4; selection-color:%5; }"
                "QAbstractItemView::item { min-height:22px; padding:3px 8px; border:0; }"
                "QAbstractItemView::item:hover { background:%6; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
            .arg(ui::colors::ACCENT_BG(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER());
    if (auto* v = view())
        v->setStyleSheet(list_qss);
    if (auto* popup = completer_->popup())
        popup->setStyleSheet(list_qss);

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
