// ReportNumberStyle.cpp — the terminal's number / money formatter for formatted report
// tables (ReportRenderer.h: terminal_number_style()). Its own translation unit so the
// renderer's unit tests do not link src/ui/formatting.

#include "screens/report_builder/ReportRenderer.h"
#include "ui/formatting/NumberFormat.h"

namespace fincept::screens::rb {

report::tablefmt::NumberStyle terminal_number_style() {
    namespace fmt = ::fincept::ui::formatting;
    report::tablefmt::NumberStyle s;
    s.number = [](double v, int decimals) { return fmt::number(v, decimals); };
    s.money = [](double v, const QString& ccy, int decimals) {
        return fmt::money(v, ccy, fmt::Compact::None, decimals);
    };
    return s;
}

} // namespace fincept::screens::rb
