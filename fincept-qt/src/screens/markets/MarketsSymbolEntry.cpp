#include "screens/markets/MarketsSymbolEntry.h"

// ── The one switch ──────────────────────────────────────────────────────────
// 1 = the shared ui::SymbolSearchBox + services::InstrumentResolver (in the build:
// SymbolSearchBox.cpp, SymbolPickerPopup.cpp, InstrumentResolver*.cpp). 0 = the
// fallback line edit that resolves committed text through core::instrument +
// services::InstrumentIndex (kept for builds without the shared box).
#ifndef MARKETS_USE_SYMBOL_SEARCH_BOX
#define MARKETS_USE_SYMBOL_SEARCH_BOX 1
#endif

#include "core/instrument/Instrument.h"
#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "screens/markets/MarketsStyle.h"
#include "services/instruments/InstrumentIndex.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ThemeHelpers.h"

#if MARKETS_USE_SYMBOL_SEARCH_BOX
#include "services/instruments/InstrumentResolver.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"
#endif

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QStandardItemModel>
#include <QTimer>

#include <memory>

namespace fincept::screens {

namespace inst = fincept::core::instrument;

namespace {

MarketsSymbolPick mse_pick(const inst::Instrument& i) {
    MarketsSymbolPick p;
    p.symbol = i.symbol;
    p.display = i.display.isEmpty() ? inst::display_symbol(i.symbol) : i.display;
    if (p.display.isEmpty())
        p.display = i.symbol;
    p.name = i.name;
    p.exchange = i.venue;
    p.asset_class = inst::asset_class_slug(i.asset_class != inst::AssetClass::Unknown ? i.asset_class
                                                                                      : inst::asset_class_of(i.symbol));
    return p;
}

[[maybe_unused]] MarketsSymbolPick mse_pick_symbol(const QString& symbol, const QString& name = {}) {
    inst::Instrument i = inst::instrument_skeleton(symbol);
    if (i.symbol.isEmpty())
        i.symbol = symbol;
    if (!name.isEmpty())
        i.name = name;
    return mse_pick(i);
}

[[maybe_unused]] QString mse_edit_sheet(bool compact) {
    return QStringLiteral("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:%4px 8px;font-family:%5;"
                          "font-size:%6px;selection-background-color:%7;}"
                          "QLineEdit:focus{border-color:%8;}")
        .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
        .arg(compact ? 2 : 4)
        .arg(QString(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::px(-2))
        .arg(ui::colors::BG_HOVER(), ui::colors::AMBER());
}

[[maybe_unused]] QString mse_popup_sheet() {
    return QStringLiteral("QListView{background:%1;color:%2;border:1px solid %3;font-family:%4;font-size:%5px;"
                          "outline:0;}"
                          "QListView::item{padding:3px 8px;}"
                          "QListView::item:selected{background:%6;color:%2;}")
        .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
             QString(ui::fonts::DATA_FAMILY))
        .arg(ui::fonts::px(-2))
        .arg(markets_selection_rgba());
}

#if !MARKETS_USE_SYMBOL_SEARCH_BOX

/// Fallback entry until the shared box is in the build: a line edit with as-you-type
/// suggestions from the local symbol index, and committed text resolved through the
/// symbol normaliser (canonical forms) and the index.
class MarketsFallbackEntry : public QLineEdit {
  public:
    MarketsFallbackEntry(QWidget* parent, const MarketsSymbolEntryOptions& o,
                         std::function<void(const QVector<MarketsSymbolPick>&)> on_picked,
                         std::function<void(const QStringList&, const QString&)> on_unresolved)
        : QLineEdit(parent), opts_(o), on_picked_(std::move(on_picked)), on_unresolved_(std::move(on_unresolved)) {
        setObjectName(QStringLiteral("marketsSymbolEntry"));
        markets_set_sheet(this, mse_edit_sheet(o.compact));
        setPlaceholderText(o.placeholder);
        setClearButtonEnabled(true);
        setMinimumHeight(ui::metrics::height(o.compact ? 24 : 30));
        model_ = new QStandardItemModel(this);
        completer_ = new QCompleter(model_, this);
        completer_->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        completer_->setCaseSensitivity(Qt::CaseInsensitive);
        completer_->setMaxVisibleItems(12);
        setCompleter(completer_);
        markets_set_sheet(completer_->popup(), mse_popup_sheet());
        QObject::connect(completer_, QOverload<const QModelIndex&>::of(&QCompleter::activated), this,
                         [this](const QModelIndex& idx) {
                             const QString sym = idx.data(Qt::UserRole).toString();
                             const QString name = idx.data(Qt::UserRole + 1).toString();
                             if (sym.isEmpty())
                                 return;
                             picked_from_list_ = true;
                             finish({mse_pick_symbol(sym, name)}, {}, {});
                         });
        timer_ = new QTimer(this);
        timer_->setSingleShot(true);
        timer_->setInterval(250);
        QObject::connect(timer_, &QTimer::timeout, this, [this]() { suggest(); });
        QObject::connect(this, &QLineEdit::textEdited, this, [this](const QString&) { timer_->start(); });
        QObject::connect(this, &QLineEdit::returnPressed, this, [this]() {
            if (picked_from_list_) { // the completer already committed this Enter
                picked_from_list_ = false;
                return;
            }
            commit();
        });
    }

  protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape && !(completer_->popup() && completer_->popup()->isVisible())) {
            clear();
            clearFocus(); // the screen returns focus to the last table
            return;
        }
        picked_from_list_ = false;
        QLineEdit::keyPressEvent(e);
    }

  private:
    void suggest() {
        const QString text = this->text().trimmed();
        if (text.size() < 1 || opts_.multi) {
            model_->clear();
            return;
        }
        services::InstrumentIndex::SearchRequest req;
        req.text = text;
        req.limit = 12;
        req.channel = QStringLiteral("markets-entry-%1").arg(reinterpret_cast<quintptr>(this), 0, 16);
        QPointer<MarketsFallbackEntry> self = this;
        services::InstrumentIndex::instance().search(req, this, [self, text](const services::InstrumentIndex::SearchResult& r) {
            if (!self || self->text().trimmed() != text)
                return;
            self->model_->clear();
            for (const auto& hit : r.hits) {
                const inst::Instrument& i = hit.instrument;
                if (i.symbol.isEmpty() || !i.is_active())
                    continue;
                const QString display = i.display.isEmpty() ? inst::display_symbol(i.symbol) : i.display;
                QStringList parts{display.isEmpty() ? i.symbol : display};
                if (!i.name.isEmpty())
                    parts << i.name;
                if (!i.venue.isEmpty())
                    parts << i.venue;
                auto* item = new QStandardItem(parts.join(QStringLiteral("  ·  ")));
                item->setData(i.symbol, Qt::UserRole);
                item->setData(i.name, Qt::UserRole + 1);
                self->model_->appendRow(item);
            }
            if (self->model_->rowCount() > 0 && self->hasFocus())
                self->completer_->complete();
        });
    }

    void commit() {
        const QString raw = text();
        QStringList items = opts_.multi ? inst::split_symbol_list(raw) : QStringList{inst::clean_symbol_text(raw)};
        items.removeAll(QString());
        if (items.isEmpty())
            return;
        // Resolve every item; report once when all have answered.
        struct Batch {
            int pending = 0;
            QVector<MarketsSymbolPick> picks;
            QStringList unresolved;
        };
        auto batch = std::make_shared<Batch>();
        batch->pending = static_cast<int>(items.size());
        QPointer<MarketsFallbackEntry> self = this;
        auto done = [self, batch]() {
            if (--batch->pending > 0 || !self)
                return;
            self->finish(batch->picks, batch->unresolved,
                         batch->unresolved.isEmpty()
                             ? QString()
                             : QCoreApplication::translate("MarketsSymbolEntry",
                                                           "No instrument found for: %1. Check the spelling or add an "
                                                           "exchange (e.g. RELIANCE.NS, 7203.T).")
                                   .arg(batch->unresolved.join(QStringLiteral(", "))));
        };
        for (const QString& item : std::as_const(items)) {
            const inst::ParsedInput parsed = inst::parse_symbol_input(item);
            if (!parsed.canonical.isEmpty()) { // fully specified: RELIANCE.NS, USDJPY, ^NSEI, BTC/USD
                batch->picks.append(mse_pick_symbol(parsed.canonical));
                done();
                continue;
            }
            if (parsed.kind == inst::InputKind::Empty || parsed.kind == inst::InputKind::Invalid) {
                batch->unresolved << item;
                done();
                continue;
            }
            services::InstrumentIndex::SearchRequest req;
            req.text = item;
            req.limit = 3;
            services::InstrumentIndex::instance().search(
                req, this, [batch, item, parsed, done](const services::InstrumentIndex::SearchResult& r) {
                    QString sym;
                    QString name;
                    for (const auto& hit : r.hits)
                        if (!hit.instrument.symbol.isEmpty() && hit.instrument.is_active()) {
                            sym = hit.instrument.symbol;
                            name = hit.instrument.name;
                            break;
                        }
                    if (sym.isEmpty() && !parsed.candidates.isEmpty())
                        sym = parsed.candidates.first(); // the normaliser's best Yahoo form
                    if (sym.isEmpty())
                        batch->unresolved << item;
                    else
                        batch->picks.append(mse_pick_symbol(sym, name));
                    done();
                });
        }
    }

    void finish(const QVector<MarketsSymbolPick>& picks, const QStringList& unresolved, const QString& message) {
        if (!picks.isEmpty() && opts_.clear_on_pick)
            clear();
        model_->clear();
        if (!picks.isEmpty() && on_picked_)
            on_picked_(picks);
        if (!unresolved.isEmpty() && on_unresolved_)
            on_unresolved_(unresolved, message);
    }

    MarketsSymbolEntryOptions opts_;
    std::function<void(const QVector<MarketsSymbolPick>&)> on_picked_;
    std::function<void(const QStringList&, const QString&)> on_unresolved_;
    QStandardItemModel* model_ = nullptr;
    QCompleter* completer_ = nullptr;
    QTimer* timer_ = nullptr;
    bool picked_from_list_ = false;
};

#endif

} // namespace

QWidget* markets_make_symbol_entry(QWidget* parent, const MarketsSymbolEntryOptions& options,
                                   std::function<void(const QVector<MarketsSymbolPick>&)> on_picked,
                                   std::function<void(const QStringList& texts, const QString& message)> on_unresolved) {
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    auto* box = new ui::SymbolSearchBox(parent);
    box->set_placeholder(options.placeholder);
    box->set_options(services::ResolveOptions{}); // defaults: every class a user can find must be addable
    box->set_multi(options.multi);
    box->set_clear_on_pick(options.clear_on_pick);
    box->set_compact(options.compact);
    if (!options.recent_key.isEmpty())
        box->set_recent_key(options.recent_key);
    QObject::connect(box, &ui::SymbolSearchBox::instrument_picked, box,
                     [on_picked](const inst::Instrument& i) {
                         if (on_picked && !i.symbol.isEmpty())
                             on_picked({mse_pick(i)});
                     });
    QObject::connect(box, &ui::SymbolSearchBox::instruments_picked, box,
                     [on_picked](const QVector<inst::Instrument>& list) {
                         QVector<MarketsSymbolPick> picks;
                         for (const auto& i : list)
                             if (!i.symbol.isEmpty())
                                 picks.append(mse_pick(i));
                         if (on_picked && !picks.isEmpty())
                             on_picked(picks);
                     });
    QObject::connect(box, &ui::SymbolSearchBox::unresolved, box,
                     [on_unresolved](const QStringList& texts, const QString& message) {
                         if (on_unresolved)
                             on_unresolved(texts, message);
                     });
    return box;
#else
    return new MarketsFallbackEntry(parent, options, std::move(on_picked), std::move(on_unresolved));
#endif
}

void markets_symbol_entry_focus(QWidget* entry) {
    if (!entry)
        return;
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    if (auto* box = qobject_cast<ui::SymbolSearchBox*>(entry)) {
        box->focus_and_select();
        return;
    }
#endif
    if (auto* edit = qobject_cast<QLineEdit*>(entry)) {
        edit->setFocus(Qt::ShortcutFocusReason);
        edit->selectAll();
        return;
    }
    entry->setFocus(Qt::ShortcutFocusReason);
}

void markets_symbol_entry_set_text(QWidget* entry, const QString& text) {
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    if (auto* box = qobject_cast<ui::SymbolSearchBox*>(entry)) {
        box->set_text(text);
        return;
    }
#endif
    if (auto* edit = qobject_cast<QLineEdit*>(entry))
        edit->setText(text);
}

void markets_symbol_entry_set_placeholder(QWidget* entry, const QString& text) {
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    if (auto* box = qobject_cast<ui::SymbolSearchBox*>(entry)) {
        box->set_placeholder(text);
        return;
    }
#endif
    if (auto* edit = qobject_cast<QLineEdit*>(entry))
        edit->setPlaceholderText(text);
}

bool markets_symbol_entry_popup_visible(QWidget* entry) {
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    if (auto* box = qobject_cast<ui::SymbolSearchBox*>(entry))
        return box->is_popup_visible();
#endif
    if (auto* edit = qobject_cast<QLineEdit*>(entry))
        return edit->completer() && edit->completer()->popup() && edit->completer()->popup()->isVisible();
    return false;
}

bool markets_symbol_entry_has_focus(QWidget* entry) {
    if (!entry)
        return false;
    QWidget* f = QApplication::focusWidget();
    return f && (f == entry || entry->isAncestorOf(f));
}

void markets_resolve_stored(const QStringList& symbols, QObject* context,
                            std::function<void(const QString& stored, const MarketsSymbolPick& now, const QString& note,
                                               bool renamed, bool inactive)>
                                per_symbol) {
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    for (const QString& stored : symbols) {
        services::InstrumentResolver::instance().resolve_stored(
            stored, QString(), context, [stored, per_symbol](const services::ResolveResult& res) {
                if (!per_symbol || res.matches.isEmpty())
                    return;
                const inst::Instrument& cur = res.matches.first();
                const bool renamed = cur.status == inst::InstrumentStatus::Renamed && res.matches.size() > 1;
                const bool inactive = cur.status == inst::InstrumentStatus::Delisted;
                per_symbol(stored, mse_pick(renamed ? res.matches.at(1) : cur), res.note, renamed, inactive);
            });
    }
#else
    Q_UNUSED(symbols);
    Q_UNUSED(context);
    Q_UNUSED(per_symbol);
#endif
}

void markets_learn(const MarketsSymbolPick& pick) {
    if (pick.symbol.isEmpty())
        return;
    inst::Instrument i = inst::instrument_skeleton(pick.symbol);
    if (i.symbol.isEmpty())
        i.symbol = pick.symbol;
    if (!pick.name.isEmpty())
        i.name = pick.name;
#if MARKETS_USE_SYMBOL_SEARCH_BOX
    services::InstrumentResolver::instance().learn(i);
#else
    services::InstrumentIndex::instance().learn(i);
#endif
}

} // namespace fincept::screens
