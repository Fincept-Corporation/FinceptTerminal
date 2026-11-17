// Unit tests for the shared symbol picker (symbol system WP4):
//   ui/widgets/symbol/SymbolSearchBox.{h,cpp}, SymbolPickerPopup.{h,cpp}
// The keyboard model (Up / Down / Enter / Tab / Esc, Enter without a list, multi entry),
// row states (filter, no quote data), recents, selector mode and sizes — QTest key and
// mouse events against the real widget. The resolver is a private InstrumentResolver whose
// local source ranks the bundled seed in memory (no database, no network), so every answer
// is deterministic.

#include "core/instrument/InstrumentRank.h"
#include "core/instrument/VenueSymbols.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/instruments/SymbolSeed.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QApplication>
#include <QFontMetrics>
#include <QLineEdit>
#include <QListView>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

using namespace fincept::core::instrument;
using fincept::services::InstrumentResolver;
using fincept::services::ResolveOptions;
using fincept::services::ResolverSources;
using fincept::ui::SymbolSearchBox;

Q_DECLARE_METATYPE(fincept::core::instrument::Instrument)

namespace {

struct TsbState {
    QVector<Instrument> pool;
    QHash<QString, QStringList> recent;
    QStringList learned;
    quint64 next = 0;
};

ResolverSources tsb_sources(const std::shared_ptr<TsbState>& st) {
    ResolverSources s;
    s.local_search = [st](const ResolverSources::LocalQuery& q, QObject* ctx,
                          ResolverSources::LocalSearchDone done) -> quint64 {
        RankContext c = q.ctx;
        c.limit = q.limit;
        const auto hits = rank_instruments(st->pool, q.parsed, c);
        QTimer::singleShot(5, ctx, [done, hits]() { done(hits, QString()); });
        return ++st->next;
    };
    s.local_cancel = [](quint64) {};
    s.local_lookup = [st](const QString& key, ResolverSources::LookupMode, QObject* ctx, ResolverSources::LookupDone done) {
        QVector<Instrument> found;
        for (const Instrument& i : st->pool)
            if (i.symbol == key || i.id == key)
                found << i;
        QTimer::singleShot(5, ctx, [done, found]() { done(found, QString()); });
    };
    s.learn = [st](const Instrument& i) { st->learned << i.symbol; };
    s.load_recent = [st](const QString& key) { return st->recent.value(key); };
    s.save_recent = [st](const QString& key, const QStringList& v) { st->recent.insert(key, v); };
    return s;
}

QStringList tsb_symbols(const QVector<Instrument>& v) {
    QStringList out;
    for (const Instrument& i : v)
        out << (i.symbol.isEmpty() ? i.display : i.symbol);
    return out;
}

} // namespace

class TstSymbolSearchBox : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void init();
    void cleanup();

    void type_move_and_pick_with_keys();
    void escape_closes_without_clearing();
    void tab_completes_to_the_row();
    void enter_without_list_resolves_exact();
    void enter_on_ambiguous_opens_the_list();
    void did_you_mean_for_a_typo();
    void row_filter_marks_and_disables();
    void rows_without_quote_data();
    void multi_entry_list();
    void recents_on_focus();
    void selector_mode_keeps_the_pick();
    void mouse_click_picks();
    void sizes_from_font_metrics();
    void describe_format();

  private:
    SymbolSearchBox* make_box();
    void type(const QString& text);

    std::shared_ptr<TsbState> state_;
    std::unique_ptr<InstrumentResolver> resolver_;
    QWidget* window_ = nullptr;
    SymbolSearchBox* box_ = nullptr;
    QLineEdit* other_ = nullptr;
};

void TstSymbolSearchBox::initTestCase() {
    qRegisterMetaType<Instrument>();
    state_ = std::make_shared<TsbState>();
    state_->pool = fincept::services::builtin_symbol_seed();
    Instrument saputo = instrument_skeleton("SAP.TO");
    saputo.name = QStringLiteral("Saputo Inc.");
    state_->pool << saputo;
    Instrument gbtc = instrument_skeleton("BTC");
    gbtc.name = QStringLiteral("Grayscale Bitcoin Mini Trust ETF");
    gbtc.asset_class = AssetClass::Etf;
    gbtc.quote_type = QStringLiteral("ETF");
    state_->pool << gbtc;
    Instrument dhaka; // a listing without a Yahoo symbol
    dhaka.id = QStringLiteral("TV.DSEBD:RELIANCE1");
    dhaka.display = QStringLiteral("RELIANCE1");
    dhaka.name = QStringLiteral("Reliance Insurance Mutual Fund");
    dhaka.asset_class = AssetClass::Fund;
    dhaka.currency = QStringLiteral("BDT");
    dhaka.venue = QStringLiteral("DSEBD");
    state_->pool << dhaka;
    resolver_ = std::make_unique<InstrumentResolver>(tsb_sources(state_));
}

SymbolSearchBox* TstSymbolSearchBox::make_box() {
    auto* box = new SymbolSearchBox(window_);
    box->set_resolver(resolver_.get());
    ResolveOptions o;
    o.home_country = QStringLiteral("IN");
    box->set_options(o);
    return box;
}

void TstSymbolSearchBox::init() {
    state_->recent.clear();
    window_ = new QWidget;
    auto* vl = new QVBoxLayout(window_);
    box_ = make_box();
    other_ = new QLineEdit(window_);
    vl->addWidget(box_);
    vl->addWidget(other_);
    window_->resize(520, 160);
    window_->show();
    window_->activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(window_));
    box_->focus_and_select();
    QTRY_VERIFY(box_->line_edit()->hasFocus());
}

void TstSymbolSearchBox::cleanup() {
    delete window_;
    window_ = nullptr;
    box_ = nullptr;
    other_ = nullptr;
}

void TstSymbolSearchBox::type(const QString& text) {
    QTest::keyClicks(box_->line_edit(), text);
}

void TstSymbolSearchBox::type_move_and_pick_with_keys() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("RELIANCE");
    QTRY_VERIFY(box_->is_popup_visible());
    QTRY_COMPARE(tsb_symbols(box_->listed_instruments()).mid(0, 2), (QStringList{"RELIANCE.NS", "RELIANCE.BO"}));
    QCOMPARE(box_->current_row(), 0); // best match pre-selected
    QTest::keyClick(box_->line_edit(), Qt::Key_Down);
    QCOMPARE(box_->current_row(), 1);
    QTest::keyClick(box_->line_edit(), Qt::Key_Up);
    QCOMPARE(box_->current_row(), 0);
    QTest::keyClick(box_->line_edit(), Qt::Key_Up); // clamps
    QCOMPARE(box_->current_row(), 0);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QCOMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("RELIANCE.NS"));
    QVERIFY(box_->text().isEmpty()); // add-box: cleared after a pick
    QVERIFY(!box_->is_popup_visible());
    QVERIFY(state_->learned.contains(QStringLiteral("RELIANCE.NS"))); // picks are learned
    QVERIFY(box_->line_edit()->hasFocus());                           // focus never left the box
}

void TstSymbolSearchBox::escape_closes_without_clearing() {
    type("REL");
    QTRY_VERIFY(box_->is_popup_visible());
    QTest::keyClick(box_->line_edit(), Qt::Key_Escape);
    QVERIFY(!box_->is_popup_visible());
    QCOMPARE(box_->text(), QStringLiteral("REL"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Escape);
    QCOMPARE(box_->text(), QStringLiteral("REL")); // default: a second Esc keeps the text
    QTest::keyClick(box_->line_edit(), Qt::Key_Down); // reopens the list for the same text
    QTRY_VERIFY(box_->is_popup_visible());
    QTest::keyClick(box_->line_edit(), Qt::Key_Escape);
    box_->set_clear_on_escape(true);
    QTest::keyClick(box_->line_edit(), Qt::Key_Escape);
    QVERIFY(box_->text().isEmpty());
}

void TstSymbolSearchBox::tab_completes_to_the_row() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("relia");
    QTRY_COMPARE(box_->listed_instruments().value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Tab);
    QCOMPARE(box_->text(), QStringLiteral("RELIANCE.NS"));
    QVERIFY(box_->line_edit()->hasFocus()); // Tab did not move the focus
    QCOMPARE(picked.count(), 0);
    QTRY_VERIFY(box_->is_popup_visible());
    QTRY_COMPARE(box_->listed_instruments().value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QTRY_COMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("RELIANCE.NS"));
}

void TstSymbolSearchBox::enter_without_list_resolves_exact() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("aapl");
    QTest::keyClick(box_->line_edit(), Qt::Key_Return); // before any list arrived
    QTRY_COMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("AAPL"));

    // Every form a user types lands on the right listing.
    const QList<QPair<QString, QString>> cases = {
        {"NSE:RELIANCE", "RELIANCE.NS"}, {"reliance.ns", "RELIANCE.NS"}, {"700", "0700.HK"},
        {"0700.HK", "0700.HK"},          {"BTC", "BTC-USD"},             {"BTC/USDT", "BTC-USD"},
        {"btc-usd", "BTC-USD"},          {"EURUSD", "EURUSD=X"},         {"USD/INR", "INR=X"},
        {"^NSEI", "^NSEI"},              {"nifty 50", "^NSEI"},          {"gold", "GC=F"},
        {"GC=F", "GC=F"},                {"US0378331005", "AAPL"},       {"TATAMOTORS", "TMPV.NS"},
    };
    for (const auto& c : cases) {
        picked.clear();
        box_->set_text(QString());
        type(c.first);
        QTest::keyClick(box_->line_edit(), Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(picked.count() == 1, 3000);
        QCOMPARE(picked.first().first().value<Instrument>().symbol, c.second);
    }
}

void TstSymbolSearchBox::enter_on_ambiguous_opens_the_list() {
    ResolveOptions o;
    o.home_country = QStringLiteral("US");
    box_->set_options(o);
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("SAP");
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QTRY_VERIFY(box_->is_popup_visible());
    QTRY_VERIFY(box_->listed_instruments().size() >= 3);
    QCOMPARE(picked.count(), 0); // SAP SE and Saputo: the user chooses
    QVERIFY2(box_->status_text().contains(QStringLiteral("more than one")), qPrintable(box_->status_text()));
    QCOMPARE(box_->listed_instruments().first().symbol, QStringLiteral("SAP"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Down);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return); // the highlighted row
    QCOMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("SAP.DE"));
}

void TstSymbolSearchBox::did_you_mean_for_a_typo() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("relaince");
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QTRY_VERIFY(box_->status_text().contains(QStringLiteral("did you mean"), Qt::CaseInsensitive));
    QCOMPARE(picked.count(), 0);
    QCOMPARE(box_->listed_instruments().value(0).symbol, QStringLiteral("RELIANCE.NS"));
}

void TstSymbolSearchBox::row_filter_marks_and_disables() {
    ResolveOptions o;
    o.home_country = QStringLiteral("US");
    box_->set_options(o);
    box_->set_row_filter([](const Instrument& i) -> SymbolSearchBox::RowCheck {
        if (i.asset_class == AssetClass::Crypto)
            return {SymbolSearchBox::RowState::Disabled, QStringLiteral("not tradeable with Zerodha")};
        if (i.asset_class == AssetClass::Etf)
            return {SymbolSearchBox::RowState::Marked, QStringLiteral("paper trading only")};
        return SymbolSearchBox::RowState::Pickable;
    });
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("BTC");
    QTRY_COMPARE(box_->listed_instruments().value(0).symbol, QStringLiteral("BTC-USD"));
    QCOMPARE(box_->current_row(), 1); // the first PICKABLE row is pre-selected
    QTest::keyClick(box_->line_edit(), Qt::Key_Up);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return); // disabled row: no pick, reason shown
    QCOMPARE(picked.count(), 0);
    QVERIFY2(box_->status_text().contains(QStringLiteral("not tradeable")), qPrintable(box_->status_text()));
    QTest::keyClick(box_->line_edit(), Qt::Key_Down);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return); // marked row: pickable
    QCOMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("BTC"));
}

void TstSymbolSearchBox::rows_without_quote_data() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("RELIANCE1");
    QTRY_COMPARE(box_->listed_instruments().value(0).display, QStringLiteral("RELIANCE1"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Up);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QCOMPARE(picked.count(), 0);
    QVERIFY2(box_->status_text().contains(QStringLiteral("no quote data")), qPrintable(box_->status_text()));
    box_->set_allow_unquoted(true);
    box_->set_text(QString());
    type("RELIANCE1");
    QTRY_COMPARE(box_->listed_instruments().value(0).display, QStringLiteral("RELIANCE1"));
    QTRY_COMPARE(box_->current_row(), 0);
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QCOMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().id, QStringLiteral("TV.DSEBD:RELIANCE1"));
}

void TstSymbolSearchBox::multi_entry_list() {
    box_->set_multi(true);
    QSignalSpy many(box_, &SymbolSearchBox::instruments_picked);
    QSignalSpy bad(box_, &SymbolSearchBox::unresolved);
    type("AAPL, reliance.ns; nosuchthingzz");
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QTRY_COMPARE(many.count(), 1);
    const auto list = many.first().first().value<QVector<Instrument>>();
    QCOMPARE(tsb_symbols(list), (QStringList{"AAPL", "RELIANCE.NS"}));
    QTRY_COMPARE(bad.count(), 1);
    QCOMPARE(bad.first().at(0).toStringList(), QStringList{QStringLiteral("nosuchthingzz")});
    QVERIFY(!bad.first().at(1).toString().isEmpty());
    QCOMPARE(box_->text(), QStringLiteral("nosuchthingzz")); // what failed stays for fixing
}

void TstSymbolSearchBox::recents_on_focus() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    int n = 0;
    for (const char* t : {"AAPL", "TMPV"}) {
        type(QString::fromLatin1(t));
        QTest::keyClick(box_->line_edit(), Qt::Key_Return);
        ++n;
        QTRY_COMPARE(picked.count(), n); // Enter resolves asynchronously
    }
    other_->setFocus();
    QTRY_VERIFY(!box_->is_popup_visible());
    box_->line_edit()->setFocus();
    QTRY_VERIFY(box_->is_popup_visible());
    QCOMPARE(box_->status_text(), QStringLiteral("Recent"));
    QCOMPARE(tsb_symbols(box_->listed_instruments()), (QStringList{"TMPV.NS", "AAPL"}));
    QTest::keyClick(box_->line_edit(), Qt::Key_Return); // picks the highlighted recent
    QCOMPARE(picked.count(), 3);
    QCOMPARE(picked.last().first().value<Instrument>().symbol, QStringLiteral("TMPV.NS"));
}

void TstSymbolSearchBox::selector_mode_keeps_the_pick() {
    box_->set_clear_on_pick(false);
    type("RELIANCE");
    QTRY_COMPARE(box_->listed_instruments().value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QTest::keyClick(box_->line_edit(), Qt::Key_Return);
    QCOMPARE(box_->text(), QStringLiteral("RELIANCE.NS"));
    QCOMPARE(box_->current_instrument().symbol, QStringLiteral("RELIANCE.NS"));
    QCOMPARE(box_->current_instrument().currency, QStringLiteral("INR"));
    type("X");
    QVERIFY(!box_->current_instrument().is_valid());
    box_->set_instrument(instrument_skeleton("0700.HK"));
    QCOMPARE(box_->text(), QStringLiteral("0700.HK"));
    QCOMPARE(box_->current_instrument().id, QStringLiteral("XHKG:0700"));
    QVERIFY(!box_->is_popup_visible());
}

void TstSymbolSearchBox::mouse_click_picks() {
    QSignalSpy picked(box_, &SymbolSearchBox::instrument_picked);
    type("RELIANCE");
    QTRY_COMPARE(tsb_symbols(box_->listed_instruments()).value(1), QStringLiteral("RELIANCE.BO"));
    auto* view = box_->findChild<QListView*>(QStringLiteral("symbolPickerList"));
    QVERIFY(view);
    const QRect r = view->visualRect(view->model()->index(1, 0));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, r.center());
    QCOMPARE(picked.count(), 1);
    QCOMPARE(picked.first().first().value<Instrument>().symbol, QStringLiteral("RELIANCE.BO"));
}

void TstSymbolSearchBox::sizes_from_font_metrics() {
    const int line = QFontMetrics(fincept::ui::fonts::font(0)).height();
    QCOMPARE(box_->height(), line + 11);
    box_->set_compact(true);
    QCOMPARE(box_->height(), line + 5);
    // Compact is a few pixels over one text line whatever the platform's font metrics are
    // (about 24 px with the Linux default font; taller with the Windows one).
    QVERIFY(box_->height() > line && box_->height() <= line + 8);
    QCOMPARE(box_->line_edit()->accessibleName(), QStringLiteral("Symbol search"));
}

void TstSymbolSearchBox::describe_format() {
    Instrument rel = instrument_skeleton("RELIANCE.NS");
    rel.name = QStringLiteral("Reliance Industries");
    QCOMPARE(fincept::ui::describe_instrument(rel),
             QString::fromUtf8("RELIANCE.NS \xc2\xb7 Reliance Industries \xc2\xb7 NSE \xc2\xb7 INR"));
}

QTEST_MAIN(TstSymbolSearchBox)
#include "tst_symbol_search_box.moc"
