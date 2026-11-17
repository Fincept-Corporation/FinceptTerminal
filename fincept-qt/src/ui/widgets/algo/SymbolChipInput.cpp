// src/ui/widgets/algo/SymbolChipInput.cpp
#include "ui/widgets/algo/SymbolChipInput.h"

#include "services/markets/MarketDataService.h"
#include "trading/AccountManager.h"
#include "trading/instruments/InstrumentNormalize.h"
#include "trading/instruments/InstrumentService.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/widgets/FlowLayout.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringListModel>
#include <QTimer>

namespace fincept::ui::algo {

namespace {
QStringList sym_chip_loaded_broker_ids() {
    QStringList ids;
    for (const auto& a : fincept::trading::AccountManager::instance().list_accounts())
        if (!ids.contains(a.broker_id) && fincept::trading::InstrumentService::instance().is_loaded(a.broker_id))
            ids << a.broker_id;
    return ids;
}

// Instrument search runs once the user pauses typing, not on every keystroke.
constexpr int kSymChipSearchDebounceMs = 150;
constexpr const char* kSymChipDebounceName = "symChipSearchDebounce";

QTimer* sym_chip_debounce_timer(const QObject* owner) {
    return owner->findChild<QTimer*>(QLatin1String(kSymChipDebounceName), Qt::FindDirectChildrenOnly);
}

// A chip's "SYMBOL  price" text from the quote kept on its label (properties "px",
// "pxCcy", "pxDigits", "pxMoney"): the price in the instrument's own currency; an index
// level or FX rate has no unit. Nothing before the quote has arrived.
void sym_chip_render_price(QLabel* lbl, const QString& symbol) {
    namespace fmt = fincept::ui::formatting;
    const QVariant px = lbl->property("px");
    if (!px.isValid())
        return;
    fmt::InstrumentFmt inst = fmt::instrument_for_symbol(symbol, lbl->property("pxCcy").toString());
    const int digits = lbl->property("pxDigits").toInt();
    if (digits >= 0)
        inst.decimals = digits;
    const double v = px.toDouble();
    const QString price = lbl->property("pxMoney").toBool() ? fmt::price_with_symbol(v, inst) : fmt::price(v, inst);
    lbl->setText(QStringLiteral("%1  %2").arg(symbol, price));
}

// Show only canonical alphabetic exchanges (NSE/BSE/NFO…); hide broker numeric
// segment codes that would otherwise render as a meaningless "· 1".
QString sym_chip_pretty_exchange(const QString& ex) {
    const QString e = ex.trimmed().toUpper();
    if (e.endsWith(QStringLiteral("_INDEX")))
        return e.left(e.size() - 6) + QStringLiteral(" IDX");
    static const QStringList known = {"NSE", "BSE", "NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX"};
    return known.contains(e) ? e : QString();
}
} // namespace

SymbolChipInput::SymbolChipInput(QWidget* parent) : QFrame(parent) {
    // Styled by the Algo kit's root stylesheet (a bordered #algoPanel box): no own sheet.
    setObjectName(QStringLiteral("algoPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAccessibleName(tr("Symbols"));
    flow_ = new fincept::ui::FlowLayout(this);
    flow_->setContentsMargins(6, 4, 6, 4);
    flow_->set_compact_hint(true); // sizeHint == minimum: the box grows by lines, never wider than its host

    edit_ = new QLineEdit(this);
    edit_->setFrame(false);
    edit_->setPlaceholderText(tr("type a ticker…"));
    edit_->setAccessibleName(tr("Add symbol"));
    edit_->setToolTip(tr("Type a ticker or name; Enter / Tab / comma adds it, Backspace in an empty field removes "
                         "the last symbol"));
    edit_->setMinimumWidth(QFontMetrics(edit_->font()).averageCharWidth() * 16);
    edit_->installEventFilter(this);
    flow_->addWidget(edit_);

    model_ = new QStringListModel(this);
    completer_ = new QCompleter(model_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    completer_->setMaxVisibleItems(8);
    edit_->setCompleter(completer_);

    // The completer popup is a child window of the line edit, so the screen's root
    // stylesheet reaches it by object name (#algoList): opaque, themed, readable rows.
    if (auto* popup = completer_->popup()) {
        popup->setObjectName(QStringLiteral("algoList"));
        popup->setTextElideMode(Qt::ElideNone); // show full label, never "…"
        popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        connect(popup, &QAbstractItemView::clicked, this,
                [this](const QModelIndex& idx) { commit_label(idx.data().toString()); });
    }

    // Debounced search: one instrument search ~150 ms after the last keystroke
    // instead of one per keystroke. Clearing the field still empties the list at
    // once, and every commit path flushes a pending search first, so a pick
    // always resolves against suggestions for the text actually typed.
    auto* debounce = new QTimer(this);
    debounce->setObjectName(QLatin1String(kSymChipDebounceName));
    debounce->setSingleShot(true);
    debounce->setInterval(kSymChipSearchDebounceMs);
    connect(debounce, &QTimer::timeout, this, [this]() {
        on_text_changed(edit_->text());
        // The line edit re-shows its popup only on a keystroke; the model was
        // refreshed after that, so show it against the new suggestions now.
        if (edit_->hasFocus() && model_->rowCount() > 0)
            completer_->complete();
    });
    connect(edit_, &QLineEdit::textEdited, this, [this, debounce](const QString& text) {
        if (text.trimmed().isEmpty()) {
            debounce->stop();
            on_text_changed(text);
        } else {
            debounce->start();
        }
    });
    connect(edit_, &QLineEdit::returnPressed, this, [this]() { commit_text(edit_->text()); });
    connect(completer_, QOverload<const QString&>::of(&QCompleter::activated), this,
            [this](const QString& label) { commit_label(label); });
    // Currency / number format changed: re-render the chip prices from their kept quotes.
    fincept::ui::formatting::on_format_changed(this, [this]() {
        for (int i = 0; i < chips_.size() && i < symbols_.size(); ++i)
            if (auto* lbl = chips_[i]->findChild<QLabel*>())
                sym_chip_render_price(lbl, symbols_[i]);
    });
}

void SymbolChipInput::on_text_changed(const QString& text) {
    const QString q = text.trimmed();
    if (q.isEmpty()) {
        model_->setStringList({});
        sugg_map_.clear();
        return;
    }
    const auto results =
        fincept::trading::InstrumentService::instance().search_all(q, "", sym_chip_loaded_broker_ids(), 16);
    QStringList labels;
    sugg_map_.clear();
    for (const auto& r : results) {
        // Friendly label (clean F&O / underlying name) instead of the raw canonical
        // symbol; remember the real symbol so a pick commits the right ticker.
        const QString friendly =
            fincept::trading::norm::display_name(r.name, r.instrument_type, r.expiry, r.strike, r.symbol);
        const QString ex = sym_chip_pretty_exchange(r.exchange);
        const QString label = ex.isEmpty() ? friendly : QStringLiteral("%1   ·   %2").arg(friendly, ex);
        if (sugg_map_.contains(label))
            continue; // collapse identical labels across brokers
        sugg_map_.insert(label, r.symbol);
        labels << label;
    }
    model_->setStringList(labels);

    // Widen the popup to fit the longest label so nothing is truncated (the
    // popup otherwise inherits the narrow line-edit width); bounds from the font.
    if (auto* popup = completer_->popup()) {
        const QFontMetrics fm(edit_->font());
        int w = 0;
        for (const auto& l : labels)
            w = qMax(w, fm.horizontalAdvance(l));
        const int avg = qMax(5, fm.averageCharWidth());
        popup->setMinimumWidth(qBound(avg * 28, w + avg * 7, avg * 70));
    }
}

bool SymbolChipInput::eventFilter(QObject* obj, QEvent* ev) {
    if (obj == edit_ && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Tab) {
            // A search still pending in the debounce window would leave the popup
            // showing the previous keystroke's suggestions — run it now.
            if (QTimer* t = sym_chip_debounce_timer(this); t && t->isActive()) {
                t->stop();
                on_text_changed(edit_->text());
                if (model_->rowCount() > 0)
                    completer_->complete();
            }
            // accept the highlighted/first suggestion if the popup is up, else raw text
            if (completer_->popup() && completer_->popup()->isVisible()) {
                const auto idx = completer_->popup()->currentIndex();
                const QString label = idx.isValid() ? idx.data().toString() : completer_->currentCompletion();
                if (!label.trimmed().isEmpty()) {
                    commit_label(label);
                    return true;
                }
            }
            if (!edit_->text().trimmed().isEmpty()) {
                commit_text(edit_->text());
                return true;
            }
        } else if (ke->key() == Qt::Key_Backspace && edit_->text().isEmpty() && !symbols_.isEmpty()) {
            remove_chip(symbols_.last());
            return true; // backspace on empty edit removes last chip
        } else if (ke->key() == Qt::Key_Comma) {
            commit_text(edit_->text());
            return true;
        }
    }
    return QFrame::eventFilter(obj, ev);
}

void SymbolChipInput::commit_text(const QString& raw) {
    // Resolve against suggestions for the current text, not a debounced older one.
    if (QTimer* t = sym_chip_debounce_timer(this); t && t->isActive()) {
        t->stop();
        on_text_changed(edit_->text());
    }
    // If the whole text is a picked suggestion label (e.g. "HINDUNILVR · NSE"),
    // resolve it to its single real symbol instead of fragmenting it.
    const QString whole = raw.trimmed();
    if (sugg_map_.contains(whole)) {
        commit_label(whole);
        return;
    }
    // Split ONLY on comma/newline (never space) so a "SYMBOL · EXCH" label is
    // never broken into pieces; a bare ticker has no spaces.
    const auto parts = raw.split(QRegularExpression("[,\\n\\r]+"), Qt::SkipEmptyParts);
    for (const auto& p : parts) {
        const QString t = p.trimmed();
        if (t.isEmpty())
            continue;
        add_chip(sugg_map_.contains(t) ? sugg_map_.value(t) : t.toUpper());
    }
    edit_->clear();
    model_->setStringList({});
    sugg_map_.clear();
    if (completer_->popup())
        completer_->popup()->hide();
}

void SymbolChipInput::commit_label(const QString& label) {
    // The label was picked from the suggestions on screen, which sugg_map_ still
    // describes — drop any pending re-search rather than running it.
    if (QTimer* t = sym_chip_debounce_timer(this))
        t->stop();
    // Resolve the friendly suggestion label back to its real canonical symbol.
    QString sym = sugg_map_.value(label);
    if (sym.isEmpty())
        sym = label.section(QStringLiteral("   ·   "), 0, 0).trimmed().toUpper();
    add_chip(sym);
    edit_->clear();
    model_->setStringList({});
    sugg_map_.clear();
    if (completer_->popup())
        completer_->popup()->hide();
}

void SymbolChipInput::add_chip(const QString& symbol) {
    if (!insert_chip(symbol))
        return;
    emit symbols_changed();
    fetch_price(symbol);
    edit_->setFocus();
}

bool SymbolChipInput::insert_chip(const QString& symbol) {
    if (symbol.isEmpty() || symbols_.contains(symbol))
        return false;
    symbols_ << symbol;

    // A transparent holder: the framed chip label (#algoChip) and its remove link.
    auto* chip = new QWidget(this);
    auto* hl = new QHBoxLayout(chip);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    // The holder's only label (quotes find it with findChild<QLabel*>()).
    auto* lbl = new QLabel(symbol, chip);
    lbl->setObjectName(QStringLiteral("algoChip"));
    lbl->setAccessibleName(symbol);
    auto* x = new QPushButton(QStringLiteral("✕"), chip);
    x->setObjectName(QStringLiteral("algoLink"));
    x->setCursor(Qt::PointingHandCursor);
    x->setFocusPolicy(Qt::NoFocus); // Backspace in the field removes chips from the keyboard
    x->setToolTip(tr("Remove %1").arg(symbol));
    x->setAccessibleName(tr("Remove %1").arg(symbol));
    connect(x, &QPushButton::clicked, this, [this, symbol]() { remove_chip(symbol); });
    hl->addWidget(lbl);
    hl->addWidget(x);

    // insert the chip BEFORE the trailing edit_
    flow_->insert_widget(flow_->indexOf(edit_), chip);
    chips_ << chip;
    return true;
}

void SymbolChipInput::remove_chip(const QString& symbol) {
    const int i = symbols_.indexOf(symbol);
    if (i < 0)
        return;
    symbols_.removeAt(i);
    QWidget* chip = chips_.takeAt(i);
    flow_->removeWidget(chip);
    chip->deleteLater();
    emit symbols_changed();
}

void SymbolChipInput::fetch_price(const QString& symbol) {
    fetch_prices({symbol});
}

void SymbolChipInput::fetch_prices(const QStringList& symbols) {
    if (symbols.isEmpty())
        return;
    // Namespace is fincept::services (not fincept::services::markets).
    // QuoteCallback = std::function<void(bool, QVector<QuoteData>)> — by value, not const-ref.
    QPointer<SymbolChipInput> self(this); // the quotes can land after the input is gone
    fincept::services::MarketDataService::instance().fetch_quotes(
        symbols, [self, symbols](bool ok, QVector<fincept::services::QuoteData> quotes) {
            if (!self || !ok || quotes.isEmpty())
                return;
            for (int k = 0; k < quotes.size(); ++k) {
                const auto& q = quotes[k];
                // A single request answers for its symbol whatever the feed calls it.
                const QString symbol = symbols.size() == 1 ? symbols.first() : q.symbol.trimmed().toUpper();
                const double px = q.price;
                const int i = self->symbols_.indexOf(symbol);
                if (i < 0)
                    continue;
                if (i < self->chips_.size())
                    if (auto* lbl = self->chips_[i]->findChild<QLabel*>()) {
                        // Kept on the label so a format change can re-render it.
                        lbl->setProperty("px", px);
                        lbl->setProperty("pxCcy", q.currency);
                        lbl->setProperty("pxDigits", q.price_digits);
                        lbl->setProperty("pxMoney", !q.currency.isEmpty() &&
                                                        fincept::ui::formatting::is_money_quote(q.quote_type, symbol));
                        sym_chip_render_price(lbl, symbol);
                    }
                emit self->price_resolved(symbol, px);
            }
        });
}

QStringList SymbolChipInput::symbols() const {
    return symbols_;
}

void SymbolChipInput::set_symbols(const QStringList& syms) {
    // One symbols_changed() and one quote request for the whole list, and no focus
    // grab (a list loaded on a tab's first show must not move the keyboard focus).
    for (auto* c : chips_) {
        flow_->removeWidget(c);
        c->deleteLater();
    }
    chips_.clear();
    symbols_.clear();
    QStringList added;
    for (const auto& s : syms) {
        const QString sym = s.trimmed().toUpper();
        if (insert_chip(sym))
            added << sym;
    }
    emit symbols_changed();
    fetch_prices(added);
}

void SymbolChipInput::clear() {
    for (auto* c : chips_) {
        flow_->removeWidget(c);
        c->deleteLater();
    }
    chips_.clear();
    symbols_.clear();
    emit symbols_changed();
}

} // namespace fincept::ui::algo
