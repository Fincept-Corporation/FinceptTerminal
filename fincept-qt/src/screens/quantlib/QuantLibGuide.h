#pragma once
// QuantLibGuide.h — plain-language guide to the QuantLib endpoint catalog: a
// readable title and a one-sentence "what this returns" for each endpoint, and
// a glyph per module. Used by the results pane's preview state (endpoints that
// are not run automatically because they cost credits or need a plan) and its
// loading caption. Text lives in QuantLibGuide.cpp (translation context
// "QuantLibGuide").

#include <QString>

namespace fincept::screens {

/// "pricing/bs/greeks-full" → "Black-Scholes · Greeks (full)".
QString ql_endpoint_title(const QString& endpoint);

/// One or two sentences on what the calculation returns, by endpoint group.
QString ql_endpoint_summary(const QString& endpoint);

/// One character for the module ("pricing" → "Δ").
QString ql_module_glyph(const QString& module_id);

} // namespace fincept::screens
