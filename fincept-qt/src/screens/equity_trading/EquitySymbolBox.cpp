// EquitySymbolBox.cpp — see header. The local fallback for ui::SymbolSearchBox plus the
// factory and instrument helpers the Equity Trading screen routes all symbol entry
// through.
#include "screens/equity_trading/EquitySymbolBox.h"

#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "services/instruments/InstrumentIndex.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>

namespace fincept::screens::equity {

namespace ci = fincept::core::instrument;

namespace {

// File-local helpers carry an eqsb_ prefix (unity-build safety).
constexpr int eqsb_kSearchDelayMs = 150;
constexpr int eqsb_kRole = Qt::UserRole + 1; // row index into rows_

QString eqsb_class_label(ci::AssetClass c) {
    switch (c) {
        case ci::AssetClass::Equity:
            return QCoreApplication::translate("EquitySymbolBox", "Stock");
        case ci::AssetClass::Etf:
            return QCoreApplication::translate("EquitySymbolBox", "ETF");
        case ci::AssetClass::Fund:
            return QCoreApplication::translate("EquitySymbolBox", "Fund");
        case ci::AssetClass::Index:
            return QCoreApplication::translate("EquitySymbolBox", "Index");
        case ci::AssetClass::Fx:
            return QCoreApplication::translate("EquitySymbolBox", "FX");
        case ci::AssetClass::Crypto:
            return QCoreApplication::translate("EquitySymbolBox", "Crypto");
        case ci::AssetClass::Future:
            return QCoreApplication::translate("EquitySymbolBox", "Future");
        case ci::AssetClass::Option:
            return QCoreApplication::translate("EquitySymbolBox", "Option");
        case ci::AssetClass::Bond:
            return QCoreApplication::translate("EquitySymbolBox", "Bond");
        case ci::AssetClass::Commodity:
            return QCoreApplication::translate("EquitySymbolBox", "Commodity");
        case ci::AssetClass::Unknown:
            break;
    }
    return {};
}

// "RELIANCE  ·  NSE  ·  Stock  ·  Reliance Industries"
QString eqsb_row_text(const ci::Instrument& inst) {
    QStringList parts;
    parts << (inst.display.isEmpty() ? inst.symbol : inst.display);
    if (!inst.venue.isEmpty())
        parts << inst.venue;
    else if (!inst.country.isEmpty())
        parts << inst.country;
    const QString cls = eqsb_class_label(inst.asset_class);
    if (!cls.isEmpty())
        parts << cls;
    if (!inst.name.isEmpty())
        parts << inst.name;
    return parts.join(QStringLiteral("  \xC2\xB7  "));
}

[[maybe_unused]] bool eqsb_lot_rule(const QString& country, double* lot) {
    static const QStringList kOne = {"US", "IN", "GB", "DE", "FR", "NL", "BE", "ES", "IT", "PT", "IE", "CH", "AT",
                                     "SE", "NO", "DK", "FI", "CA", "AU", "NZ", "KR", "BR", "MX", "ZA"};
    static const QStringList kHundred = {"JP", "CN", "SG", "MY", "TH", "ID"};
    if (kOne.contains(country)) {
        *lot = 1.0;
        return true;
    }
    if (kHundred.contains(country)) {
        *lot = 100.0;
        return true;
    }
    if (country == QLatin1String("TW")) {
        *lot = 1000.0;
        return true;
    }
    return false; // HK: board lot per stock — unknown here
}

} // namespace

// ============================================================================
// Instrument helpers
// ============================================================================

ci::Instrument eq_complete(const ci::Instrument& inst) {
#if FINCEPT_EQ_SHARED_SYMBOL_BOX
    return fincept::services::complete_instrument(inst);
#else
    if (inst.symbol.isEmpty())
        return inst;
    ci::Instrument out = inst;
    const ci::Instrument sk = ci::instrument_skeleton(inst.symbol);
    if (!sk.is_valid())
        return out;
    auto fill = [](QString& field, const QString& value) {
        if (field.isEmpty())
            field = value;
    };
    fill(out.id, sk.id);
    fill(out.display, sk.display);
    fill(out.name, sk.name);
    fill(out.venue, sk.venue);
    fill(out.mic, sk.mic);
    fill(out.country, sk.country);
    fill(out.currency, sk.currency);
    if (out.asset_class == ci::AssetClass::Unknown)
        out.asset_class = sk.asset_class;
    if (out.aliases.isEmpty())
        out.aliases = sk.aliases;
    return out;
#endif
}

fincept::services::TradingMeta eq_trading_meta(const ci::Instrument& inst) {
#if FINCEPT_EQ_SHARED_SYMBOL_BOX
    return fincept::services::trading_meta_for(inst);
#else
    fincept::services::TradingMeta m;
    namespace mk = fincept::core::market;
    if (const mk::Venue* v = mk::venue_for_symbol(inst.symbol, inst.venue)) {
        m.venue = v->id;
        m.time_zone = v->tz_id;
    }
    const ci::Instrument full = eq_complete(inst);
    if (full.asset_class == ci::AssetClass::Equity || full.asset_class == ci::AssetClass::Etf) {
        double lot = 1.0;
        if (eqsb_lot_rule(full.country, &lot)) {
            m.lot_size = lot;
            m.lot_known = true;
        }
    }
    return m;
#endif
}

ci::Instrument eq_instrument_for_stored(const QString& stored_symbol, const QString& exchange_hint, QString* note) {
    const QString raw = stored_symbol.trimmed();
    // The hint only applies to a bare code: "RELIANCE" + NSE -> RELIANCE.NS. A stored
    // symbol that already names its market (VOD.L, ^NSEI, EURUSD=X, BTC-USD) wins.
    QString canonical = ci::canonical_symbol(raw, exchange_hint);
    if (canonical.isEmpty() && !exchange_hint.isEmpty())
        canonical = ci::canonical_symbol(raw); // an unknown / Yahoo-less hint: try the text alone
    ci::Instrument inst;
    if (canonical.isEmpty()) {
        // Not a symbol the public data path can quote (an F&O contract, a name):
        // keep the row, identified by its stored text.
        inst.id = raw;
        inst.display = raw;
        inst.venue = exchange_hint;
        if (note)
            *note = QCoreApplication::translate("EquitySymbolBox", "%1 has no public market-data symbol").arg(raw);
        return inst;
    }
    inst.symbol = canonical;
    inst = eq_complete(inst);
    if (note && canonical != raw)
        *note = QCoreApplication::translate("EquitySymbolBox", "%1 (%2) is now %3").arg(raw, exchange_hint, canonical);
    return inst;
}

SymbolBox* make_symbol_box(SymbolBoxRole role, QWidget* parent) {
    auto* box = new SymbolBox(parent);
    fincept::services::ResolveOptions opts;
    opts.limit = 12;
    opts.ctx.include_derivatives = false;
    // What the screen can quote, chart and paper-trade (the shared component's advice
    // for paper trading); a broker account marks what it cannot trade (row filter).
    opts.ctx.allowed_classes = {ci::AssetClass::Equity, ci::AssetClass::Etf, ci::AssetClass::Index,
                                ci::AssetClass::Fx,     ci::AssetClass::Crypto, ci::AssetClass::Future};
    box->set_options(opts);
    box->set_compact(true); // ~24 px: command-bar and panel-header rows
    if (role == SymbolBoxRole::Selector) {
        box->set_clear_on_pick(false);
        box->set_placeholder(QCoreApplication::translate("EquitySymbolBox", "Symbol, name or ISIN…"));
        box->set_recent_key(QStringLiteral("equity_trading.selector"));
    } else {
        box->set_clear_on_pick(true);
        box->set_multi(true); // "AAPL, VOD.L, BTC" or a pasted column
        box->set_placeholder(QCoreApplication::translate("EquitySymbolBox", "Add: ticker, name or ISIN…"));
        box->set_recent_key(QStringLiteral("equity_trading.watchlist"));
    }
    return box;
}

// ============================================================================
// EquitySymbolBox (fallback)
// ============================================================================

EquitySymbolBox::EquitySymbolBox(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("eqSymbolBox"));
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    edit_ = new QLineEdit(this);
    edit_->setObjectName(QStringLiteral("eqSymbolBoxEdit"));
    edit_->setClearButtonEnabled(false);
    lay->addWidget(edit_);
    setFocusProxy(edit_);

    // Ownership: completer under the edit, model under the completer, so teardown
    // frees the completer's internal model before the source model (see the
    // watchlist's note on the QCompletionModel teardown crash).
    completer_ = new QCompleter(edit_);
    model_ = new QStandardItemModel(completer_);
    completer_->setModel(model_);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer_->setMaxVisibleItems(12);
    edit_->setCompleter(completer_);
    if (auto* popup = completer_->popup())
        popup->setObjectName(QStringLiteral("eqSymbolBoxPopup"));
    refresh_popup_style();

    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(eqsb_kSearchDelayMs);
    connect(search_timer_, &QTimer::timeout, this, &EquitySymbolBox::run_search);

    connect(edit_, &QLineEdit::textEdited, this, &EquitySymbolBox::on_text_edited);
    connect(edit_, &QLineEdit::returnPressed, this, &EquitySymbolBox::commit);
    // After setCompleter(): runs after the completer's own text insertion.
    connect(completer_, qOverload<const QModelIndex&>(&QCompleter::activated), this,
            [this](const QModelIndex& idx) { pick_row(idx.data(eqsb_kRole).toInt()); });
    if (auto* popup = completer_->popup()) {
        // A single click must pick (activated is unreliable on a model rebuilt per keystroke).
        connect(popup, &QAbstractItemView::clicked, this,
                [this](const QModelIndex& idx) { pick_row(idx.data(eqsb_kRole).toInt()); });
    }
    edit_->installEventFilter(this);

    connect(&fincept::ui::ThemeManager::instance(), &fincept::ui::ThemeManager::appearance_settled, this, [this]() {
        refresh_popup_style();
        apply_height();
    });
    apply_height();
}

EquitySymbolBox::~EquitySymbolBox() {
    if (search_id_ != 0)
        fincept::services::InstrumentIndex::instance().cancel(search_id_);
}

void EquitySymbolBox::refresh_popup_style() {
    auto* popup = completer_ ? completer_->popup() : nullptr;
    if (!popup)
        return;
    namespace c = fincept::ui::colors;
    const QString sheet =
        QString("QAbstractItemView{background:%1;color:%2;border:1px solid %3;outline:0;padding:2px;"
                "font-family:%5;font-size:" +
                fincept::ui::fonts::px(-2) +
                "px;}"
                "QAbstractItemView::item{padding:4px 8px;}"
                "QAbstractItemView::item:selected,QAbstractItemView::item:hover{background:%4;color:%2;}"
                "QAbstractItemView::item:disabled{color:%6;}")
            .arg(c::BG_SURFACE(), c::TEXT_PRIMARY(), c::BORDER_MED(), c::BG_HOVER(),
                 QString(fincept::ui::fonts::DATA_FAMILY), c::TEXT_TERTIARY());
    if (popup->styleSheet() != sheet)
        popup->setStyleSheet(sheet);
}

void EquitySymbolBox::apply_height() {
    const int h = QFontMetrics(fincept::ui::fonts::font()).height() + (compact_ ? 8 : 12);
    edit_->setMinimumHeight(h);
}

void EquitySymbolBox::set_compact(bool on) {
    compact_ = on;
    apply_height();
}

void EquitySymbolBox::set_placeholder(const QString& text) {
    edit_->setPlaceholderText(text);
}

void EquitySymbolBox::set_options(const fincept::services::ResolveOptions& opts) {
    opts_ = opts;
}

void EquitySymbolBox::set_text(const QString& text) {
    edit_->setText(text);
}

QString EquitySymbolBox::text() const {
    return edit_->text();
}

void EquitySymbolBox::focus_and_select() {
    edit_->setFocus(Qt::ShortcutFocusReason);
    edit_->selectAll();
}

void EquitySymbolBox::set_instrument(const ci::Instrument& inst) {
    picked_ = inst;
    const QString shown = inst.display.isEmpty() ? (inst.symbol.isEmpty() ? inst.id : inst.symbol) : inst.display;
    if (edit_->text() != shown)
        edit_->setText(shown); // setText does not emit textEdited: no search
    edit_->setToolTip(eqsb_row_text(inst));
}

QString EquitySymbolBox::home_country() const {
    if (!opts_.home_country.isEmpty())
        return opts_.home_country;
    if (!opts_.ctx.home_country.isEmpty())
        return opts_.ctx.home_country;
    const auto& prof = fincept::core::market::MarketProfileStore::instance().current();
    return prof.countries.value(0);
}

void EquitySymbolBox::on_text_edited(const QString& text) {
    if (text.trimmed().isEmpty()) {
        rows_.clear();
        row_checks_.clear();
        model_->clear();
        rows_text_.clear();
        return;
    }
    search_timer_->start();
}

QVector<ci::Instrument> EquitySymbolBox::parser_rows(const QString& text) const {
    QVector<ci::Instrument> out;
    const ci::ParsedInput parsed = ci::parse_symbol_input(text, opts_.venue_hint, home_country());
    QStringList syms = parsed.candidates;
    if (!parsed.canonical.isEmpty() && !syms.contains(parsed.canonical))
        syms.prepend(parsed.canonical);
    for (const QString& s : syms) {
        ci::Instrument inst;
        inst.symbol = s;
        inst = eq_complete(inst);
        if (inst.is_valid())
            out.append(inst);
    }
    return out;
}

void EquitySymbolBox::run_search() {
    const QString text = edit_->text().trimmed();
    if (text.isEmpty())
        return;
    const QVector<ci::Instrument> parsed = parser_rows(text);
    show_rows(parsed); // instant: what the text itself denotes

    fincept::services::InstrumentIndex::SearchRequest req;
    req.text = text;
    req.venue_hint = opts_.venue_hint;
    req.ctx = opts_.ctx;
    req.ctx.home_country = home_country();
    req.limit = opts_.limit > 0 ? opts_.limit : 12;
    req.channel = QStringLiteral("eqSymbolBox:%1").arg(quintptr(this), 0, 16);
    auto& index = fincept::services::InstrumentIndex::instance();
    if (search_id_ != 0)
        index.cancel(search_id_);
    QPointer<EquitySymbolBox> self = this;
    search_id_ = index.search(req, this, [self, text, parsed](const fincept::services::InstrumentIndex::SearchResult& r) {
        if (!self)
            return;
        self->search_id_ = 0;
        if (self->edit_->text().trimmed() != text)
            return; // the user typed on
        QVector<ci::Instrument> merged;
        for (const auto& hit : r.hits) {
            if (hit.instrument.symbol.isEmpty())
                continue; // no market data: nothing this screen can quote or chart
            merged.append(eq_complete(hit.instrument));
        }
        for (const auto& p : parsed) {
            bool dup = false;
            for (const auto& m : merged)
                dup = dup || m.symbol == p.symbol;
            if (!dup)
                merged.append(p);
        }
        self->show_rows(merged);
    });
}

void EquitySymbolBox::show_rows(const QVector<ci::Instrument>& rows) {
    rows_ = rows;
    rows_text_ = edit_->text().trimmed();
    row_checks_.clear();
    model_->clear();
    for (int i = 0; i < rows_.size(); ++i) {
        const RowCheck check = row_filter_ ? row_filter_(rows_[i]) : RowCheck{};
        row_checks_.append(check);
        QString text = eqsb_row_text(rows_[i]);
        if (!check.reason.isEmpty())
            text += QStringLiteral("  \xE2\x80\x94  ") + check.reason;
        auto* item = new QStandardItem(text);
        item->setData(i, eqsb_kRole);
        item->setToolTip(rows_[i].symbol);
        if (check.state == RowState::Disabled)
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        model_->appendRow(item);
    }
    if (!rows_.isEmpty() && edit_->hasFocus() && !picking_) {
        completer_->complete();
        if (auto* popup = completer_->popup()) {
            // Pre-select the best pickable row so Enter takes it.
            for (int i = 0; i < row_checks_.size(); ++i) {
                if (row_checks_[i].state != RowState::Disabled) {
                    popup->setCurrentIndex(model_->index(i, 0));
                    break;
                }
            }
        }
    }
}

bool EquitySymbolBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == edit_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape && completer_->popup() && completer_->popup()->isVisible()) {
            completer_->popup()->hide(); // close the list, keep the text
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EquitySymbolBox::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
}

void EquitySymbolBox::pick_row(int row) {
    if (row < 0 || row >= rows_.size())
        return;
    if (row < row_checks_.size() && row_checks_[row].state == RowState::Disabled) {
        emit unresolved({rows_[row].symbol}, row_checks_[row].reason);
        return;
    }
    pick(rows_[row]);
}

void EquitySymbolBox::pick(const ci::Instrument& inst) {
    if (!inst.is_valid())
        return;
    if (row_filter_) {
        const RowCheck check = row_filter_(inst);
        if (check.state == RowState::Disabled) {
            emit unresolved({inst.symbol}, check.reason);
            return;
        }
    }
    picking_ = true;
    search_timer_->stop();
    if (completer_->popup())
        completer_->popup()->hide();
    picked_ = inst;
    if (clear_on_pick_) {
        // Deferred: the completer writes the row text into the edit after activated().
        QTimer::singleShot(0, edit_, [this]() {
            edit_->clear();
            picking_ = false;
        });
    } else {
        set_instrument(inst);
        QTimer::singleShot(0, edit_, [this, inst]() {
            set_instrument(inst); // undo the completer's insertion of the row text
            picking_ = false;
        });
    }
    emit instrument_picked(inst);
}

void EquitySymbolBox::commit() {
    if (picking_)
        return; // the Enter that picked a row from the list
    const QString text = edit_->text().trimmed();
    if (text.isEmpty())
        return;
    // Selector already showing its pick: nothing to do.
    if (!clear_on_pick_ && picked_.is_valid() &&
        (text == picked_.display || text == picked_.symbol || text == picked_.id))
        return;
    auto* popup = completer_->popup();
    if (popup && popup->isVisible() && popup->currentIndex().isValid()) {
        pick_row(popup->currentIndex().data(eqsb_kRole).toInt());
        return;
    }
    const ci::ParsedInput parsed = ci::parse_symbol_input(text, opts_.venue_hint, home_country());
    auto richer = [this](const QString& sym) {
        for (const auto& r : rows_)
            if (r.symbol == sym)
                return r;
        ci::Instrument inst;
        inst.symbol = sym;
        return eq_complete(inst);
    };
    if (!parsed.canonical.isEmpty()) {
        pick(richer(parsed.canonical)); // fully specified: VOD.L, ^NSEI, EURUSD=X, NSE:RELIANCE
        return;
    }
    // An exact listing-code / alias hit from the index wins when it is the only one.
    if (rows_text_ == text && !rows_.isEmpty()) {
        int exact = -1, n_exact = 0;
        for (int i = 0; i < rows_.size(); ++i) {
            const QString disp = rows_[i].display.isEmpty() ? rows_[i].symbol : rows_[i].display;
            if (disp.compare(text, Qt::CaseInsensitive) == 0 || rows_[i].symbol.compare(text, Qt::CaseInsensitive) == 0) {
                if (exact < 0)
                    exact = i;
                ++n_exact;
            }
        }
        if (n_exact == 1) {
            pick_row(exact);
            return;
        }
    }
    if (parsed.candidates.size() == 1) {
        pick(richer(parsed.candidates.first()));
        return;
    }
    if (parsed.candidates.size() > 1 || (rows_text_ == text && !rows_.isEmpty())) {
        // Ambiguous ("did you mean"): open the list instead of guessing.
        if (rows_text_ != text)
            show_rows(parser_rows(text));
        edit_->setFocus();
        completer_->complete();
        return;
    }
    if (raw_allowed_)
        emit text_committed_raw(text);
    emit unresolved({text}, tr("No instrument matches “%1”. Try a ticker (AAPL, VOD.L, RELIANCE.NS), "
                               "a name, an index (NIFTY 50) or a pair (EUR/USD, BTC-USD).")
                                .arg(text));
}

} // namespace fincept::screens::equity
