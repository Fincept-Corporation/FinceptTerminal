#pragma once
// FormatWidgets — keep formatted text in step with FormatSettings without
// re-fetching anything. QtWidgets, app target only (the formatters themselves
// are Qt Core).
//
// A format change (currency, symbol style, locale, decimals...) re-formats
// data the screen already holds. Three mechanisms, pick by how the screen
// holds its text:
//
//   A  a model that formats in data():
//        fmt::refresh_model_on_format_change(model_);
//   B  a widget that keeps formatted text (QTableWidget items, labels, tiles,
//      scene items): keep the last payload and re-render from it
//        fmt::on_format_changed(this, [this] { render_from_cache(); });
//   C  a unit label or a money spin box:
//        fmt::bind_currency_label(capital_label_, tr("Capital (%1)"), ccy);
//        fmt::bind_currency_affix(capital_spin_, ccy);
//
// Rules: never fetch, re-subscribe, reset a model, rebuild a layout or set a
// stylesheet because of a format change. The handlers here do O(1) work in
// the signal and defer the rest to the next event-loop turn (coalesced); a
// hidden widget re-renders once when it is next shown.

#include <QString>

#include <functional>

class QAbstractItemModel;
class QAbstractSpinBox;
class QLabel;
class QWidget;

namespace fincept::ui::formatting {

/// Sets lbl's text to tmpl.arg(currency_unit(ccy)) now and on every format
/// change ("Revenue (%1)" -> "Revenue ($)" -> "Revenue (USD)"). Empty `ccy` =
/// the display currency. Disconnects when the label dies. Calling it again for
/// the same label replaces the earlier binding (re-bind when the currency changes).
void bind_currency_label(QLabel* lbl, const QString& tmpl, const QString& ccy = {});

/// Puts currency_unit(ccy) on a QDoubleSpinBox / QSpinBox as a prefix
/// (unit + " ") or a suffix (no-break space + unit), following where money()
/// places the unit for the user's settings ("$ 100.00" en_US, "100,00 €"
/// de_DE; "p" is always a suffix), and keeps it in sync. Empty `ccy` = the
/// display currency. Calling it again for the same spin box replaces the
/// earlier binding.
void bind_currency_affix(QAbstractSpinBox* spin, const QString& ccy = {});

/// Ends the bind_currency_label() / bind_currency_affix() binding of `widget`
/// (e.g. its currency became unknown). The text, prefix and suffix are left as
/// they are — clear them yourself. No-op for a widget without a binding.
void unbind_currency(QWidget* widget);

/// Calls `rerender` after a format change: on the next event-loop turn if
/// `owner` is visible (several changes in one turn -> one call), otherwise
/// once when `owner` is next shown. Disconnects when `owner` dies.
void on_format_changed(QWidget* owner, std::function<void()> rerender);

/// For models that format in data(): after a format change (coalesced, next
/// event-loop turn) emits dataChanged over every top-level cell for the
/// display / tooltip / accessible-text roles and headerDataChanged over every
/// horizontal section. No reset: selection and scroll position are kept.
/// Disconnects when the model dies.
void refresh_model_on_format_change(QAbstractItemModel* model);

} // namespace fincept::ui::formatting
