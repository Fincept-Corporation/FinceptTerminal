// src/screens/docs/DocsScreen_internal.h
//
// Private header — included only by DocsScreen*.cpp translation units.
// Exposes the scroll-area QSS so the page-builder TUs can apply the same
// look without duplicating the string. `inline` so the header is safe to
// include from multiple TUs.

#pragma once

#include "ui/theme/Theme.h"

#include <QString>

namespace fincept::screens::docs_internal {

inline QString SCROLL_SS() {
    return QString("QScrollArea { border: none; background: transparent; }"
                   "QScrollBar:vertical { width: 5px; background: transparent; }"
                   "QScrollBar::handle:vertical { background: %1; }"
                   "QScrollBar::handle:vertical:hover { background: %2; }"
                   "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
        .arg(fincept::ui::colors::BORDER_MED(), fincept::ui::colors::BORDER_BRIGHT());
}

} // namespace fincept::screens::docs_internal
