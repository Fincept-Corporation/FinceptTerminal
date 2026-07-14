#pragma once

#include <QTableWidgetItem>

namespace fincept::ui {

// A QTableWidgetItem that SORTS by a numeric value while DISPLAYING arbitrary
// (formatted) text. Plain QTableWidgetItem::operator< compares the DisplayRole
// as a string, so "10" sorts before "9" and "$1,000" before "$200" — wrong for
// any numeric column. Use this for price / quantity / ratio / percent cells so
// header-click sorting orders by magnitude.
//
//   table->setItem(row, col, new ui::NumericTableWidgetItem(QString::number(v,'f',2), v));
class NumericTableWidgetItem : public QTableWidgetItem {
  public:
    NumericTableWidgetItem(const QString& display_text, double value)
        : QTableWidgetItem(display_text), value_(value) {}

    bool operator<(const QTableWidgetItem& other) const override {
        if (const auto* o = dynamic_cast<const NumericTableWidgetItem*>(&other))
            return value_ < o->value_;
        return QTableWidgetItem::operator<(other);
    }

    double numeric_value() const { return value_; }

  private:
    double value_ = 0.0;
};

} // namespace fincept::ui
