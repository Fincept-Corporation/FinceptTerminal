// Unit tests for src/ui/formatting/FormatWidgets.{h,cpp}: how screens repaint
// after a currency / number format change without re-fetching — models get one
// full-range dataChanged + headerDataChanged (no reset), cached-text widgets
// re-render once per event-loop turn (or once on the next show when hidden),
// bound labels and spin boxes follow the unit. Also the cur:: facade
// (core/currency/Currency.h) that forwards to them. A widget test: runs on the
// offscreen platform.

#include "core/currency/Currency.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>

namespace fmt = fincept::ui::formatting;

class TstFormatRefresh : public QObject {
    Q_OBJECT

  private slots:
    void init();
    void model_gets_one_full_range_refresh();
    void hidden_widget_rerenders_once_on_show();
    void visible_widget_rerenders_once_per_turn();
    void bound_label_follows_symbol_style();
    void bound_spin_box_follows_position();
    void rebinding_replaces_the_earlier_binding();
    void currency_manager_facade();
};

void TstFormatRefresh::init() {
    auto& s = fmt::FormatSettings::instance();
    s.set_values(fmt::FormatSettings::Values{});
    s.set_locale(QLocale(QLocale::English, QLocale::UnitedStates));
    s.set_auto_display_currency(QStringLiteral("USD"));
    QCoreApplication::processEvents(); // flush hooks of the previous test
}

void TstFormatRefresh::model_gets_one_full_range_refresh() {
    QStandardItemModel model(40, 6);
    fmt::refresh_model_on_format_change(&model);
    QSignalSpy data(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy header(&model, &QAbstractItemModel::headerDataChanged);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Code); // #139
    QCOMPARE(data.count(), 0); // deferred, not in the signal handler
    QCoreApplication::processEvents();
    QCOMPARE(data.count(), 1);
    QCOMPARE(header.count(), 1);
    QCOMPARE(reset.count(), 0);
    const auto args = data.takeFirst();
    QCOMPARE(args.at(0).value<QModelIndex>(), model.index(0, 0));
    QCOMPARE(args.at(1).value<QModelIndex>(), model.index(39, 5));
    const auto h = header.takeFirst();
    QCOMPARE(h.at(0).value<Qt::Orientation>(), Qt::Horizontal);
    QCOMPARE(h.at(1).toInt(), 0);
    QCOMPARE(h.at(2).toInt(), 5);

    // Several changes in one turn coalesce.
    fmt::FormatSettings::instance().set_negative_style(fmt::NegativeStyle::Parentheses);
    fmt::FormatSettings::instance().set_grouping(fmt::Grouping::None);
    QCoreApplication::processEvents();
    QCOMPARE(data.count(), 1);
    QCOMPARE(header.count(), 1);
}

void TstFormatRefresh::hidden_widget_rerenders_once_on_show() {
    QWidget w;
    int calls = 0;
    fmt::on_format_changed(&w, [&calls]() { ++calls; });
    auto& s = fmt::FormatSettings::instance();
    s.set_symbol_style(fmt::SymbolStyle::Code); // #140
    s.set_symbol_style(fmt::SymbolStyle::Disambiguated);
    s.set_symbol_style(fmt::SymbolStyle::Symbol);
    QCoreApplication::processEvents();
    QCOMPARE(calls, 0);
    w.show();
    QCOMPARE(calls, 1);
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
    // Hidden again: nothing until the next show; no change -> nothing on show.
    w.hide();
    w.show();
    QCOMPARE(calls, 1);
}

void TstFormatRefresh::visible_widget_rerenders_once_per_turn() {
    QWidget w;
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w) || w.isVisible());
    int calls = 0;
    fmt::on_format_changed(&w, [&calls]() { ++calls; });
    auto& s = fmt::FormatSettings::instance();
    s.set_money_decimals(fmt::DecimalsPolicy::Fixed, 3); // #141
    s.set_minor_units(fmt::MinorUnits::Major);
    s.set_code_when_mixed(false);
    QCOMPARE(calls, 0);
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
    // A setter that changes nothing does not re-render.
    s.set_code_when_mixed(false);
    QCoreApplication::processEvents();
    QCOMPARE(calls, 1);
}

void TstFormatRefresh::bound_label_follows_symbol_style() {
    QLabel lbl;
    fmt::bind_currency_label(&lbl, QStringLiteral("Rev (%1)")); // #142
    QCOMPARE(lbl.text(), QStringLiteral("Rev ($)"));
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(lbl.text(), QStringLiteral("Rev (USD)"));
    QLabel cad;
    fmt::bind_currency_label(&cad, QStringLiteral("Value (%1)"), QStringLiteral("CAD"));
    QCOMPARE(cad.text(), QStringLiteral("Value (CAD)"));
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Symbol);
    QCOMPARE(cad.text(), QStringLiteral("Value (C$)"));
    QVERIFY(fmt::FormatSettings::instance().set_display_currency(QStringLiteral("EUR")));
    QCOMPARE(lbl.text(), QStringLiteral("Rev (\u20AC)"));
}

void TstFormatRefresh::bound_spin_box_follows_position() {
    QDoubleSpinBox spin;
    fmt::bind_currency_affix(&spin); // #143
    QCOMPARE(spin.prefix(), QStringLiteral("$ "));
    QCOMPARE(spin.suffix(), QString());
    fmt::FormatSettings::instance().set_symbol_position(fmt::SymbolPosition::After);
    QCOMPARE(spin.prefix(), QString());
    QCOMPARE(spin.suffix(), QStringLiteral("\u00A0$"));
    // A locale that writes the unit after the number.
    fmt::FormatSettings::instance().set_symbol_position(fmt::SymbolPosition::Locale);
    fmt::FormatSettings::instance().set_locale(QLocale(QLocale::German, QLocale::Germany));
    QSpinBox eur;
    fmt::bind_currency_affix(&eur, QStringLiteral("EUR"));
    QCOMPARE(eur.prefix(), QString());
    QCOMPARE(eur.suffix(), QStringLiteral("\u00A0\u20AC"));
    // Pence are always a suffix.
    QDoubleSpinBox gbp;
    fmt::bind_currency_affix(&gbp, QStringLiteral("GBp"));
    QCOMPARE(gbp.suffix(), QStringLiteral("\u00A0p"));
}

void TstFormatRefresh::rebinding_replaces_the_earlier_binding() {
    // A screen re-binds a unit label / money spin box when its currency changes:
    // the earlier binding must go, not keep a second connection alive.
    const QString label_binding = QStringLiteral("fincept_fmt_currency_label_binding");
    const QString affix_binding = QStringLiteral("fincept_fmt_currency_affix_binding");
    QLabel lbl;
    fmt::bind_currency_label(&lbl, QStringLiteral("A (%1)"), QStringLiteral("USD"));
    fmt::bind_currency_label(&lbl, QStringLiteral("B (%1)"), QStringLiteral("EUR"));
    fmt::bind_currency_label(&lbl, QStringLiteral("C (%1)"), QStringLiteral("JPY"));
    QCOMPARE(lbl.text(), QStringLiteral("C (\u00A5)"));
    QCoreApplication::processEvents(); // replaced bindings are deleted on the next turn
    QCOMPARE(lbl.findChildren<QObject*>(label_binding, Qt::FindDirectChildrenOnly).size(), 1);
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(lbl.text(), QStringLiteral("C (JPY)"));

    QDoubleSpinBox spin;
    fmt::bind_currency_affix(&spin, QStringLiteral("USD"));
    fmt::bind_currency_affix(&spin, QStringLiteral("GBp"));
    QCoreApplication::processEvents();
    QCOMPARE(spin.findChildren<QObject*>(affix_binding, Qt::FindDirectChildrenOnly).size(), 1);
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Symbol);
    QCOMPARE(spin.prefix(), QString());
    QCOMPARE(spin.suffix(), QStringLiteral("\u00A0p"));

    // unbind_currency(): the unit is left alone and no longer follows the settings.
    fmt::unbind_currency(&spin);
    spin.setSuffix(QString());
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Code);
    QCoreApplication::processEvents();
    QCOMPARE(spin.suffix(), QString());
    QCOMPARE(spin.findChildren<QObject*>(affix_binding, Qt::FindDirectChildrenOnly).size(), 0);
    fmt::unbind_currency(&spin); // no binding: no-op
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Symbol);

    // Re-binding from inside a format-change handler (a screen re-rendering) is safe.
    QWidget host;
    host.show();
    QLabel inner(&host);
    fmt::bind_currency_label(&inner, QStringLiteral("X (%1)"), QStringLiteral("USD"));
    QObject::connect(&fmt::FormatSettings::instance(), &fmt::FormatSettings::changed, &inner, [&inner]() {
        fmt::bind_currency_label(&inner, QStringLiteral("Y (%1)"), QStringLiteral("CHF"));
    });
    fmt::FormatSettings::instance().set_symbol_style(fmt::SymbolStyle::Code);
    QCoreApplication::processEvents();
    QCOMPARE(inner.text(), QStringLiteral("Y (CHF)"));
    QCOMPARE(inner.findChildren<QObject*>(label_binding, Qt::FindDirectChildrenOnly).size(), 1);
}
void TstFormatRefresh::currency_manager_facade() {
    using fincept::currency::CurrencyManager;
    auto& cm = CurrencyManager::instance();
    cm.initialize();
    QSignalSpy spy(&cm, &CurrencyManager::currency_changed);
    QCOMPARE(cm.code(), QStringLiteral("USD"));
    QCOMPARE(cm.symbol(), QStringLiteral("$"));
    QCOMPARE(CurrencyManager::symbol_for(QStringLiteral("CAD")), QStringLiteral("C$")); // one table, not "$"
    QCOMPARE(CurrencyManager::symbol_for(QStringLiteral("CHF")), QStringLiteral("CHF")); // not "Fr"
    QVERIFY(CurrencyManager::available().size() > 100);
    cm.set_currency(QStringLiteral("EUR"));
    QCOMPARE(fmt::FormatSettings::instance().display_currency(), QStringLiteral("EUR"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("EUR"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("\u20AC"));
    cm.set_currency(QStringLiteral("NOPE"));
    QCOMPARE(spy.count(), 1);
    // A change that alters neither code nor unit does not re-emit.
    fmt::FormatSettings::instance().set_grouping(fmt::Grouping::None);
    QCOMPARE(spy.count(), 1);
    // The cur:: helpers.
    QCOMPARE(cur::symbol(), QStringLiteral("\u20AC"));
    QCOMPARE(cur::money(1234.5), QStringLiteral("\u20AC1234.50"));
    QCOMPARE(cur::money(1234.5, false, QStringLiteral("USD")), QStringLiteral("$1234.50"));
    fmt::FormatSettings::instance().set_grouping(fmt::Grouping::Always);
    QCOMPARE(cur::money(1.5e9, true, QStringLiteral("USD")), QStringLiteral("$1.50B"));
    QCOMPARE(cur::money(std::numeric_limits<double>::quiet_NaN()), fmt::placeholder());
    QLabel lbl;
    cur::bindLabel(&lbl, QStringLiteral("Capital (%1)"));
    QCOMPARE(lbl.text(), QStringLiteral("Capital (\u20AC)"));
    cm.set_currency(QStringLiteral("INR"));
    QCOMPARE(lbl.text(), QStringLiteral("Capital (\u20B9)"));
}

QTEST_MAIN(TstFormatRefresh)
#include "tst_format_refresh.moc"
