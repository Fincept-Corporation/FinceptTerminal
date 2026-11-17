// WatchlistScreen_Actions.cpp — what the user does: add (resolved, with "did
// you mean" suggestions), remove (with undo), import / export, list management
// (create, rename, describe, colour, default, duplicate, merge, order, delete),
// row and list menus, notes, custom groups, alerts, manual ordering and the
// ADD box's autocomplete. Feedback is non-modal (the message bar); only list
// deletion asks for confirmation.

#include "screens/watchlist/WatchlistScreen.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "screens/watchlist/WatchlistAlertDialog.h"
#include "screens/watchlist/WatchlistAnalyticsViews.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "screens/watchlist/WatchlistDetailPane.h"
#include "screens/watchlist/WatchlistImportExport.h"
#include "screens/watchlist/WatchlistScreenInternal.h"
#include "screens/watchlist/WatchlistView.h"
#include "services/backtesting/BacktestingService.h"
#include "services/markets/MarketSearchService.h"
#include "ui/charts/ChartExport.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStringListModel>
#include <QTimer>

#include <algorithm>

namespace fincept::screens {

namespace colors = fincept::ui::colors;
namespace fonts = fincept::ui::fonts;
using namespace wl_internal;
using services::equity::EquityResearchService;
using services::equity::SymbolCandidate;
using services::equity::SymbolResolution;

namespace {

constexpr qint64 kWlsaMaxImportBytes = 5 * 1024 * 1024;

QString wlsa_lookups_text(const QSet<QString>& in_flight) {
    QStringList syms;
    for (const QString& key : in_flight)
        syms << key.section(QLatin1Char('|'), -1);
    syms.sort();
    if (syms.size() > 6)
        return syms.mid(0, 6).join(QStringLiteral(", ")) + QStringLiteral(" …");
    return syms.join(QStringLiteral(", "));
}

QString wlsa_short_list(const QStringList& items, int max = 5) {
    if (items.size() <= max)
        return items.join(QStringLiteral(", "));
    return items.mid(0, max).join(QStringLiteral(", ")) + QStringLiteral(" … (+%1)").arg(items.size() - max);
}

QPushButton* wlsa_action_button(QWidget* parent, const QString& text) {
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:%2; border:1px solid %3; padding:1px 8px; "
                                    "font-size:%4px; font-weight:700; }"
                                    "QPushButton:hover { color:%5; border-color:%5; }")
                         .arg(colors::BG_RAISED(), colors::TEXT_PRIMARY(), colors::BORDER_MED())
                         .arg(fonts::px(-2))
                         .arg(colors::AMBER()));
    return b;
}

QIcon wlsa_swatch(const QColor& c) {
    QPixmap pm(12, 12);
    pm.fill(colors::legible_color(c));
    return QIcon(pm);
}

} // namespace

// ── Add ──────────────────────────────────────────────────────────────────────

void WatchlistScreen::on_add_stock() {
    if (current_wl_id_.isEmpty())
        return;
    const QStringList parts = watchlist_split_symbols(add_input_->text());
    if (parts.isEmpty())
        return;
    // Drop any autocomplete still pending for this text, so its results do not pop up over
    // the emptied box after the add.
    autocomplete_timer_->stop();
    autocomplete_request_.clear();
    if (completer_->popup())
        completer_->popup()->hide();

    QSet<QString> existing;
    for (const QString& s : model_->symbols())
        existing.insert(s.toUpper());

    auto batch = std::make_shared<WatchlistAddBatch>();
    batch->wl_id = current_wl_id_;
    batch->source = WatchlistAddBatch::Source::Typed;
    QSet<QString> seen;
    for (const QString& typed : parts) {
        if (!watchlist_is_ticker_shaped(typed)) {
            // A company name ("Reliance Industries") is several parts; the resolver takes names too,
            // but only one at a time — use the autocomplete for those.
            batch->invalid << typed;
            continue;
        }
        if (existing.contains(typed)) {
            batch->duplicates << typed;
            continue;
        }
        if (seen.contains(typed) || add_lookups_in_flight_.contains(batch->wl_id + QLatin1Char('|') + typed))
            continue; // typed twice, or an earlier ADD of it is still being checked
        seen.insert(typed);
        WatchlistAddBatch::Entry e;
        e.typed = typed;
        batch->entries.append(e);
    }
    add_input_->clear();
    start_add_batch(batch);
}

void WatchlistScreen::start_add_batch(const std::shared_ptr<WatchlistAddBatch>& batch) {
    for (const auto& e : batch->entries)
        add_lookups_in_flight_.insert(batch->wl_id + QLatin1Char('|') + e.typed);
    batch->pending = static_cast<int>(batch->entries.size());
    if (batch->pending == 0) {
        finish_add_batch(batch);
        return;
    }
    add_input_->setPlaceholderText(tr("Checking %1…").arg(wlsa_lookups_text(add_lookups_in_flight_)));
    if (batch->source == WatchlistAddBatch::Source::Import)
        show_feedback(tr("Checking %n symbol(s) against the market…", "", batch->pending), FeedbackTone::Info, 0);
    launch_lookups(batch);
}

void WatchlistScreen::launch_lookups(const std::shared_ptr<WatchlistAddBatch>& batch) {
    QPointer<WatchlistScreen> self = this;
    while (batch->running < kMaxParallelLookups && batch->next < batch->entries.size()) {
        const qsizetype i = batch->next++;
        ++batch->running;
        EquityResearchService::instance().resolve_symbol(
            batch->entries[i].typed, [self, batch, i](bool ok, SymbolResolution res, QString error) {
                auto& e = batch->entries[i];
                if (!ok) {
                    // The lookup itself failed (offline, Python not ready): keep the
                    // feature working and store what was typed.
                    e.outcome = WatchlistAddBatch::Outcome::LookupFailed;
                    e.symbol = e.typed;
                    e.message = error;
                } else if (res.found) {
                    e.outcome = WatchlistAddBatch::Outcome::Found;
                    e.symbol = res.resolved_symbol.trimmed().toUpper();
                    if (!res.name.isEmpty())
                        e.name = res.name;
                    if (!res.exchange.isEmpty())
                        e.exchange = res.exchange;
                } else {
                    e.outcome = WatchlistAddBatch::Outcome::NotFound;
                    e.message = res.message;
                    e.candidates = res.candidates;
                }
                --batch->running;
                --batch->pending;
                if (!self)
                    return;
                if (batch->pending == 0) {
                    self->finish_add_batch(batch);
                    return;
                }
                const int done = static_cast<int>(batch->entries.size()) - batch->pending;
                if (batch->source == WatchlistAddBatch::Source::Import && done % 5 == 0)
                    self->show_feedback(self->tr("Checking symbols: %1 of %2…").arg(done).arg(batch->entries.size()),
                                        FeedbackTone::Info, 0);
                self->launch_lookups(batch);
            });
    }
}

void WatchlistScreen::finish_add_batch(const std::shared_ptr<WatchlistAddBatch>& batch_ref) {
    using Outcome = WatchlistAddBatch::Outcome;
    const std::shared_ptr<WatchlistAddBatch> batch = batch_ref;
    for (const auto& e : batch->entries)
        add_lookups_in_flight_.remove(batch->wl_id + QLatin1Char('|') + e.typed);
    add_input_->setPlaceholderText(add_lookups_in_flight_.isEmpty()
                                       ? tr("Tickers, e.g. AAPL, RELIANCE, VOD.L, EURUSD=X — or type a name and pick a match (Ctrl+N)")
                                       : tr("Checking %1…").arg(wlsa_lookups_text(add_lookups_in_flight_)));

    // The target list as stored now — it may not be on screen any more, and it
    // may have changed while the lookups ran.
    auto& repo = WatchlistRepository::instance();
    QSet<QString> existing;
    if (auto r = repo.get_stocks(batch->wl_id); r.is_ok()) {
        for (const auto& s : r.value())
            existing.insert(s.symbol.toUpper());
    }
    const bool groups_ok = repo.has_groups_column();

    QStringList added, renamed, duplicates = batch->duplicates, not_found, unverified, failed_save;
    QStringList leftover = batch->source == WatchlistAddBatch::Source::Typed ? batch->invalid : QStringList();

    const auto add_one = [&](const QString& symbol, const QString& name, const QString& exchange, const QString& typed,
                             const QString& notes, const QString& group) -> bool {
        const QString sym = symbol.toUpper();
        if (existing.contains(sym)) {
            duplicates << (sym.compare(typed, Qt::CaseInsensitive) == 0 ? sym
                                                                        : QStringLiteral("%1 (%2)").arg(sym, typed));
            return false;
        }
        auto r = repo.add_stock(batch->wl_id, sym, name, exchange);
        if (r.is_err()) {
            LOG_WARN("Watchlist", QString("add_stock(%1) failed: %2").arg(sym, QString::fromStdString(r.error())));
            failed_save << typed;
            leftover << typed;
            return false;
        }
        if (!notes.isEmpty())
            repo.set_stock_notes(batch->wl_id, sym, notes);
        if (!group.isEmpty() && groups_ok)
            repo.set_stock_group(batch->wl_id, {sym}, group);
        existing.insert(sym);
        added << sym;
        return true;
    };

    QVector<Suggestion> fresh;
    for (const auto& e : batch->entries) {
        switch (e.outcome) {
            case Outcome::NotFound:
                if (e.candidates.isEmpty()) {
                    not_found << e.typed;
                    leftover << e.typed;
                } else {
                    Suggestion s;
                    s.wl_id = batch->wl_id;
                    s.typed = e.typed;
                    s.notes = e.notes;
                    s.group = e.group;
                    for (const SymbolCandidate& c : e.candidates) {
                        QString label = c.symbol;
                        if (!c.name.isEmpty())
                            label += QStringLiteral("  —  ") + c.name;
                        if (!c.exchange.isEmpty())
                            label += QStringLiteral("  (") + c.exchange + QLatin1Char(')');
                        s.symbols << c.symbol.toUpper();
                        s.labels << label;
                        s.names << c.name;
                        s.exchanges << c.exchange;
                    }
                    fresh.append(s);
                }
                break;
            case Outcome::Found:
            case Outcome::LookupFailed:
            case Outcome::Pending: { // (Pending cannot happen: every lookup has answered)
                const QString symbol = e.symbol.isEmpty() ? e.typed : e.symbol;
                if (e.outcome == Outcome::LookupFailed) {
                    LOG_INFO("Watchlist", QString("Could not verify %1 (%2) — added as typed").arg(e.typed, e.message));
                    unverified << e.typed;
                }
                if (add_one(symbol, e.name, e.exchange, e.typed, e.notes, e.group) && e.outcome == Outcome::Found &&
                    symbol.compare(e.typed, Qt::CaseInsensitive) != 0)
                    renamed << QStringLiteral("%1 → %2").arg(e.typed, symbol);
                break;
            }
        }
    }
    if (batch->wl_id == current_wl_id_) {
        load_stocks();
        // Show where the new rows landed (a long or grouped list can put them off screen).
        if (batch->source == WatchlistAddBatch::Source::Typed)
            reveal_added(added);
    } else if (!added.isEmpty()) {
        counts_.insert(batch->wl_id, static_cast<int>(existing.size()));
        rebuild_sidebar();
    }

    // What could not be added goes back into the box — unless the user has typed meanwhile.
    if (batch->source == WatchlistAddBatch::Source::Typed && add_input_->text().trimmed().isEmpty())
        add_input_->setText(leftover.join(QStringLiteral(", ")));

    // One summary for the whole batch (no dialog per symbol).
    QStringList parts;
    QStringList details;
    if (!added.isEmpty()) {
        parts << tr("Added %1").arg(wlsa_short_list(added));
        details << tr("Added: %1").arg(added.join(QStringLiteral(", ")));
    }
    if (!renamed.isEmpty())
        details << tr("Added under the listed symbol: %1").arg(renamed.join(QStringLiteral(", ")));
    if (!duplicates.isEmpty()) {
        parts << tr("already listed: %1").arg(wlsa_short_list(duplicates));
        details << tr("Already in this watchlist: %1").arg(duplicates.join(QStringLiteral(", ")));
    }
    if (!not_found.isEmpty()) {
        parts << tr("not found: %1").arg(wlsa_short_list(not_found));
        details << tr("No such symbol, not added: %1").arg(not_found.join(QStringLiteral(", ")));
    }
    if (!batch->invalid.isEmpty()) {
        parts << tr("not a ticker: %1").arg(wlsa_short_list(batch->invalid));
        details << tr("Not a valid ticker: %1").arg(batch->invalid.join(QStringLiteral(", ")));
    }
    if (!unverified.isEmpty()) {
        parts << tr("%n added unverified (lookup unavailable)", "", static_cast<int>(unverified.size()));
        details << tr("Could not be checked (lookup unavailable), added as typed: %1")
                       .arg(unverified.join(QStringLiteral(", ")));
    }
    if (!failed_save.isEmpty()) {
        parts << tr("could not save: %1").arg(wlsa_short_list(failed_save));
        details << tr("Could not be saved: %1").arg(failed_save.join(QStringLiteral(", ")));
    }
    if (batch->file_duplicates > 0)
        details << tr("%n repeated in the file", "", batch->file_duplicates);

    const bool problems = !duplicates.isEmpty() || !not_found.isEmpty() || !batch->invalid.isEmpty() ||
                          !failed_save.isEmpty() || !unverified.isEmpty() || !fresh.isEmpty();
    QString text = parts.join(QStringLiteral("; "));
    if (!text.isEmpty()) {
        text[0] = text[0].toUpper();
        text += QLatin1Char('.');
    }
    if (added.isEmpty() && parts.isEmpty() && fresh.isEmpty() && batch->source == WatchlistAddBatch::Source::Import)
        text = tr("Nothing to import.");

    suggestions_ += fresh;
    if (!fresh.isEmpty()) {
        show_next_suggestion(text);
        return;
    }
    if (text.isEmpty())
        return;
    const FeedbackTone tone = !problems ? FeedbackTone::Success
                              : added.isEmpty() ? FeedbackTone::Error
                                                : FeedbackTone::Warning;
    show_feedback(text, tone, tone == FeedbackTone::Success ? 5000 : 15000, details.join(QLatin1Char('\n')));
}

void WatchlistScreen::show_next_suggestion(const QString& prefix) {
    if (suggestions_.isEmpty())
        return;
    const Suggestion s = suggestions_.first();
    QString text = tr("\"%1\" is not a known symbol — did you mean:").arg(s.typed);
    if (!prefix.isEmpty())
        text = prefix + QStringLiteral("  ") + text;
    if (suggestions_.size() > 1)
        text += QLatin1Char(' ') + tr("(%n more to check)", "", static_cast<int>(suggestions_.size() - 1));
    show_feedback(text, FeedbackTone::Warning, 0, s.labels.join(QLatin1Char('\n')));
    feedback_label_->setProperty("suggestion", true);
    for (int i = 0; i < s.symbols.size() && i < 4; ++i) {
        QPushButton* b = wlsa_action_button(feedback_bar_, s.symbols[i]);
        b->setToolTip(s.labels.value(i));
        const QString sym = s.symbols[i];
        const QString name = s.names.value(i);
        const QString exch = s.exchanges.value(i);
        connect(b, &QPushButton::clicked, this, [this, sym, name, exch]() {
            if (suggestions_.isEmpty())
                return;
            const Suggestion sug = suggestions_.takeFirst();
            feedback_label_->setProperty("suggestion", false);
            auto& repo = WatchlistRepository::instance();
            QString done;
            bool listed = false;
            if (auto r = repo.get_stocks(sug.wl_id); r.is_ok()) {
                for (const auto& st : r.value())
                    listed = listed || st.symbol.compare(sym, Qt::CaseInsensitive) == 0;
            }
            if (listed) {
                done = tr("%1 is already in the list.").arg(sym);
            } else if (repo.add_stock(sug.wl_id, sym, name, exch).is_ok()) {
                if (!sug.notes.isEmpty())
                    repo.set_stock_notes(sug.wl_id, sym, sug.notes);
                if (!sug.group.isEmpty() && repo.has_groups_column())
                    repo.set_stock_group(sug.wl_id, {sym}, sug.group);
                done = tr("Added %1 (for \"%2\").").arg(sym, sug.typed);
            } else {
                done = tr("Could not add %1.").arg(sym);
            }
            if (sug.wl_id == current_wl_id_) {
                load_stocks();
                reveal_added({sym});
            }
            if (!suggestions_.isEmpty())
                show_next_suggestion(done);
            else
                show_feedback(done, FeedbackTone::Success, 5000);
        });
        feedback_actions_->addWidget(b);
    }
    QPushButton* skip = wlsa_action_button(feedback_bar_, tr("Skip"));
    skip->setToolTip(tr("Do not add \"%1\"").arg(s.typed));
    connect(skip, &QPushButton::clicked, this, [this]() { clear_feedback(); }); // drops it, shows the next
    feedback_actions_->addWidget(skip);
}

void WatchlistScreen::add_symbols_direct(const QList<SymbolRef>& refs) {
    if (current_wl_id_.isEmpty() || refs.isEmpty())
        return;
    QSet<QString> existing;
    for (const QString& s : model_->symbols())
        existing.insert(s.toUpper());
    auto& repo = WatchlistRepository::instance();
    QStringList added, dups, invalid;
    for (const SymbolRef& ref : refs) {
        const QString sym = ref.symbol.trimmed().toUpper();
        if (sym.isEmpty())
            continue;
        // Symbols dragged from other panels are already in their data-source form;
        // only the shape is checked (no lookup, as before).
        if (!watchlist_is_ticker_shaped(sym)) {
            invalid << sym;
            continue;
        }
        if (existing.contains(sym)) {
            dups << sym;
            continue;
        }
        if (repo.add_stock(current_wl_id_, sym, QString(), ref.exchange).is_ok()) {
            existing.insert(sym);
            added << sym;
        }
    }
    if (!added.isEmpty()) {
        load_stocks();
        reveal_added(added);
        show_feedback(tr("Added %1.").arg(wlsa_short_list(added)), FeedbackTone::Success, 4000);
    } else if (!dups.isEmpty()) {
        show_feedback(tr("%1 is already in this list.").arg(wlsa_short_list(dups)), FeedbackTone::Info, 4000);
    } else if (!invalid.isEmpty()) {
        show_feedback(tr("Not a valid ticker: %1").arg(wlsa_short_list(invalid)), FeedbackTone::Warning, 6000);
    }
}

void WatchlistScreen::reveal_added(const QStringList& symbols) {
    if (symbols.isEmpty())
        return;
    // load_stocks() reset the model; its modelReset handler re-applies the selection saved
    // before the add on the next turn. Queued after it, so the new rows win.
    const QString list_id = current_wl_id_;
    QTimer::singleShot(0, this, [this, symbols, list_id]() {
        if (current_wl_id_ != list_id)
            return;
        view_->select_symbols(symbols, symbols.first());
        const QModelIndex idx =
            view_->proxy_model()->mapFromSource(view_->source_model()->index_of_symbol(symbols.first(), 0));
        if (idx.isValid())
            view_->scrollTo(idx); // also expands a collapsed group holding it
        on_current_row_changed(); // select_symbols() is silent: show it in the detail pane
    });
}

// ── Remove (with undo) ───────────────────────────────────────────────────────

void WatchlistScreen::on_remove_stock() {
    if (current_wl_id_.isEmpty())
        return;
    const QStringList syms = view_->selected_symbols();
    if (syms.isEmpty()) {
        show_feedback(tr("Select one or more rows first."), FeedbackTone::Info, 4000);
        return;
    }
    remove_symbols(syms);
}

void WatchlistScreen::remove_symbols(const QStringList& symbols) {
    if (current_wl_id_.isEmpty() || symbols.isEmpty())
        return;
    const QString wl = current_wl_id_;
    const QStringList prev_order = model_->symbols();
    QVector<WatchlistStock> removed;
    for (const auto& s : stocks_) {
        if (symbols.contains(s.symbol))
            removed.append(s);
    }
    auto& repo = WatchlistRepository::instance();
    QStringList gone;
    for (const auto& s : removed) {
        if (repo.remove_stock(wl, s.symbol).is_ok())
            gone << s.symbol;
    }
    load_stocks();
    if (gone.isEmpty()) {
        show_feedback(tr("Could not remove the selected symbols."), FeedbackTone::Error, 8000);
        return;
    }
    show_feedback(tr("Removed %1.").arg(wlsa_short_list(gone)), FeedbackTone::Info, 12000,
                  gone.join(QStringLiteral(", ")));
    QPushButton* undo = wlsa_action_button(feedback_bar_, tr("UNDO"));
    connect(undo, &QPushButton::clicked, this, [this, wl, removed, prev_order]() {
        auto& r = WatchlistRepository::instance();
        const bool groups_ok = r.has_groups_column();
        for (const auto& s : removed) {
            r.add_stock(wl, s.symbol, s.name, s.exchange);
            if (!s.notes.isEmpty())
                r.set_stock_notes(wl, s.symbol, s.notes);
            if (!s.group_name.isEmpty() && groups_ok)
                r.set_stock_group(wl, {s.symbol}, s.group_name);
        }
        r.set_stock_order(wl, prev_order); // back to their places
        if (wl == current_wl_id_)
            load_stocks();
        show_feedback(tr("Restored %n symbol(s).", "", static_cast<int>(removed.size())), FeedbackTone::Success, 4000);
    });
    feedback_actions_->addWidget(undo);
}

// ── Import / export ──────────────────────────────────────────────────────────

void WatchlistScreen::on_import_csv() {
    if (current_wl_id_.isEmpty())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Import symbols into \"%1\"").arg(current_list_name()),
                                                      QString(),
                                                      tr("Watchlist files (*.csv *.tsv *.txt *.json);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (QFileInfo(path).size() > kWlsaMaxImportBytes) {
        show_feedback(tr("The file is too large to be a watchlist (over 5 MB)."), FeedbackTone::Error, 10000);
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        show_feedback(tr("Could not open %1: %2").arg(QFileInfo(path).fileName(), f.errorString()),
                      FeedbackTone::Error, 10000);
        return;
    }
    const WatchlistImportParse parse = watchlist_parse_import(f.readAll(), path);
    if (!parse.error.isEmpty()) {
        show_feedback(tr("Import failed: %1").arg(parse.error), FeedbackTone::Error, 12000);
        return;
    }
    import_entries(parse.entries, parse.invalid, parse.duplicates);
}

void WatchlistScreen::import_entries(const QVector<WatchlistImportEntry>& entries, const QStringList& invalid,
                                     int file_duplicates) {
    if (current_wl_id_.isEmpty())
        return;
    QSet<QString> existing;
    for (const QString& s : model_->symbols())
        existing.insert(s.toUpper());
    auto batch = std::make_shared<WatchlistAddBatch>();
    batch->wl_id = current_wl_id_;
    batch->source = WatchlistAddBatch::Source::Import;
    batch->invalid = invalid;
    batch->file_duplicates = file_duplicates;
    for (const auto& e : entries) {
        if (existing.contains(e.symbol)) {
            batch->duplicates << e.symbol;
            continue;
        }
        if (add_lookups_in_flight_.contains(batch->wl_id + QLatin1Char('|') + e.symbol))
            continue;
        WatchlistAddBatch::Entry en;
        en.typed = e.symbol;
        en.name = e.name;
        en.exchange = e.exchange;
        en.notes = e.notes;
        en.group = e.group;
        batch->entries.append(en);
    }
    start_add_batch(batch);
}

void WatchlistScreen::on_export_csv() {
    if (current_wl_id_.isEmpty())
        return;
    if (stocks_.isEmpty()) {
        show_feedback(tr("The list is empty — nothing to export."), FeedbackTone::Info, 4000);
        return;
    }
    // The columns on screen, in their order, at full precision; the rows in
    // their current order (filter applied).
    QStringList headers;
    QVector<int> cols;
    for (const QString& id : view_->visible_column_ids()) {
        const int c = watchlist_columns::index_of(id);
        if (c < 0)
            continue;
        cols << c;
        headers << WatchlistModel::export_headers(c);
    }
    QVector<QStringList> rows;
    for (const QString& sym : view_->display_order()) {
        const auto* r = model_->state(sym);
        if (!r)
            continue;
        QStringList row;
        for (int c : cols)
            row << model_->export_cells(*r, c);
        rows.append(row);
    }
    QString base = current_list_name();
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (ui::ChartExport::save_csv(headers, rows, base.isEmpty() ? QStringLiteral("watchlist") : base, this))
        show_feedback(tr("Exported %n row(s) with %1 columns.", "", static_cast<int>(rows.size())).arg(headers.size()),
                      FeedbackTone::Success, 5000);
}

void WatchlistScreen::export_json() {
    const int row = row_of_list(current_wl_id_);
    if (row < 0)
        return;
    QString base = watchlists_[row].name;
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export list"), base + QStringLiteral(".json"),
                                                      tr("Watchlist JSON (*.json)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        show_feedback(tr("Could not write %1: %2").arg(QFileInfo(path).fileName(), f.errorString()),
                      FeedbackTone::Error, 10000);
        return;
    }
    const QByteArray data = watchlist_export_json(watchlists_[row], stocks_);
    const bool ok = f.write(data) == data.size();
    f.close();
    if (ok)
        show_feedback(tr("Exported %n symbol(s) to %1.", "", static_cast<int>(stocks_.size()))
                          .arg(QFileInfo(path).fileName()),
                      FeedbackTone::Success, 5000);
    else
        show_feedback(tr("Writing %1 failed.").arg(QFileInfo(path).fileName()), FeedbackTone::Error, 10000);
}

// ── Lists ────────────────────────────────────────────────────────────────────

void WatchlistScreen::on_add_watchlist() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("New Watchlist"), tr("Name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    // A distinct identity colour per list (stored user data, drawn legible on any theme).
    const QString color = colors::chart_color(static_cast<int>(watchlists_.size())).name();
    auto r = WatchlistRepository::instance().create(name, color);
    if (r.is_err()) {
        LOG_ERROR("Watchlist", QString("Failed to create watchlist: %1").arg(QString::fromStdString(r.error())));
        show_feedback(tr("Could not create the watchlist."), FeedbackTone::Error, 8000);
        return;
    }
    const QString id = r.value().id;
    load_watchlists();
    const int row = row_of_list(id);
    if (row >= 0)
        wl_list_->setCurrentRow(row);
    add_input_->setFocus();
}

void WatchlistScreen::rename_watchlist(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    fincept::Watchlist wl = watchlists_[row];
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Rename Watchlist"), tr("Name:"), QLineEdit::Normal, wl.name, &ok).trimmed();
    if (!ok || name.isEmpty() || name == wl.name)
        return;
    wl.name = name;
    if (WatchlistRepository::instance().update(wl).is_err()) {
        show_feedback(tr("Could not rename the watchlist."), FeedbackTone::Error, 8000);
        return;
    }
    load_watchlists();
}

void WatchlistScreen::edit_list_description(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    fincept::Watchlist wl = watchlists_[row];
    bool ok = false;
    const QString desc = QInputDialog::getMultiLineText(this, tr("Description — %1").arg(wl.name),
                                                        tr("Description:"), wl.description, &ok);
    if (!ok || desc == wl.description)
        return;
    wl.description = desc.trimmed();
    if (WatchlistRepository::instance().update(wl).is_err()) {
        show_feedback(tr("Could not save the description."), FeedbackTone::Error, 8000);
        return;
    }
    load_watchlists();
}

void WatchlistScreen::set_list_color(int row, const QString& color) {
    if (row < 0 || row >= watchlists_.size())
        return;
    fincept::Watchlist wl = watchlists_[row];
    QString chosen = color;
    if (chosen.isEmpty()) {
        const QColor c = QColorDialog::getColor(QColor(wl.color), this, tr("List colour — %1").arg(wl.name));
        if (!c.isValid())
            return;
        chosen = c.name();
    }
    wl.color = chosen;
    if (WatchlistRepository::instance().update(wl).is_ok()) {
        watchlists_[row].color = chosen;
        rebuild_sidebar();
    }
}

void WatchlistScreen::set_default_list(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    if (WatchlistRepository::instance().set_default(watchlists_[row].id).is_ok())
        load_watchlists();
}

void WatchlistScreen::duplicate_list(int row) {
    if (row < 0 || row >= watchlists_.size())
        return;
    const fincept::Watchlist src = watchlists_[row];
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Duplicate Watchlist"), tr("Name of the copy:"),
                                               QLineEdit::Normal, tr("%1 copy").arg(src.name), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    auto r = WatchlistRepository::instance().duplicate(src.id, name);
    if (r.is_err()) {
        show_feedback(tr("Could not duplicate the list: %1").arg(QString::fromStdString(r.error())),
                      FeedbackTone::Error, 10000);
        return;
    }
    const QString id = r.value().id;
    if (src.id == current_wl_id_)
        save_layout_now();
    WatchlistColumnPrefs::save(id, WatchlistColumnPrefs::load(src.id)); // the copy looks the same
    load_watchlists();
    const int nrow = row_of_list(id);
    if (nrow >= 0)
        wl_list_->setCurrentRow(nrow);
    show_feedback(tr("Created \"%1\" with %n symbol(s).", "", counts_.value(id)).arg(name), FeedbackTone::Success, 5000);
}

void WatchlistScreen::merge_list(int row, const QString& target_id, bool delete_source) {
    if (row < 0 || row >= watchlists_.size())
        return;
    const fincept::Watchlist src = watchlists_[row];
    const int trow = row_of_list(target_id);
    if (trow < 0 || target_id == src.id)
        return;
    const QString target_name = watchlists_[trow].name;
    auto r = WatchlistRepository::instance().merge_into(src.id, target_id);
    if (r.is_err()) {
        show_feedback(tr("Could not merge: %1").arg(QString::fromStdString(r.error())), FeedbackTone::Error, 10000);
        return;
    }
    load_watchlists();
    if (delete_source) {
        // (only reached from an explicit, confirmed action)
        WatchlistRepository::instance().remove(src.id);
        WatchlistColumnPrefs::forget(src.id);
        if (current_wl_id_ == src.id)
            current_wl_id_.clear();
        load_watchlists();
        return;
    }
    show_feedback(tr("Merged %n new symbol(s) from \"%1\" into \"%2\".", "", r.value()).arg(src.name, target_name),
                  FeedbackTone::Success, 12000);
    QPushButton* del = wlsa_action_button(feedback_bar_, tr("Delete \"%1\"").arg(src.name));
    const QString src_id = src.id;
    connect(del, &QPushButton::clicked, this, [this, src_id, target_id]() {
        const int srow = row_of_list(src_id);
        if (srow < 0)
            return;
        if (QMessageBox::question(this, tr("Delete Watchlist"),
                                  tr("Delete \"%1\"? Its symbols are now also in the merged list.")
                                      .arg(watchlists_[srow].name),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        merge_list(srow, target_id, true);
        clear_feedback();
    });
    feedback_actions_->addWidget(del);
}

void WatchlistScreen::move_list(int row, int delta) {
    const int to = row + delta;
    if (row < 0 || row >= watchlists_.size() || to < 0 || to >= watchlists_.size())
        return;
    watchlists_.move(row, to);
    QStringList ids;
    for (const auto& wl : watchlists_)
        ids << wl.id;
    WatchlistRepository::instance().set_watchlist_order(ids);
    rebuild_sidebar();
}

void WatchlistScreen::persist_sidebar_order() {
    QStringList ids;
    for (int i = 0; i < wl_list_->count(); ++i)
        ids << wl_list_->item(i)->data(kListIdRole).toString();
    if (ids.size() != watchlists_.size())
        return;
    QVector<fincept::Watchlist> reordered;
    for (const QString& id : ids) {
        const int r = row_of_list(id);
        if (r >= 0)
            reordered.append(watchlists_[r]);
    }
    if (reordered.size() != watchlists_.size())
        return;
    watchlists_ = reordered;
    if (WatchlistRepository::instance().set_watchlist_order(ids).is_err())
        show_feedback(tr("Could not save the list order."), FeedbackTone::Error, 6000);
}

void WatchlistScreen::on_delete_watchlist() {
    if (current_wl_id_.isEmpty())
        return;
    const QString name = current_list_name();
    const int n = static_cast<int>(stocks_.size());
    // Destructive and not undoable: the one confirmation the screen still asks for.
    if (QMessageBox::question(this, tr("Delete Watchlist"),
                              tr("Delete \"%1\" and its %n symbol(s)?", "", n).arg(name),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    const QString id = current_wl_id_;
    if (WatchlistRepository::instance().remove(id).is_err()) {
        show_feedback(tr("Could not delete the watchlist."), FeedbackTone::Error, 8000);
        return;
    }
    WatchlistColumnPrefs::forget(id);
    layout_save_timer_->stop();
    hub_unsubscribe_all();
    current_wl_id_.clear();
    stocks_.clear();
    model_->set_stocks(QString(), {});
    load_watchlists();
    show_feedback(tr("Deleted \"%1\".").arg(name), FeedbackTone::Info, 5000);
}

// ── Menus ────────────────────────────────────────────────────────────────────

void WatchlistScreen::show_watchlist_menu(const QPoint& pos) {
    QListWidgetItem* item = wl_list_->itemAt(pos);
    if (!item)
        return;
    const int row = wl_list_->row(item);
    wl_list_->setCurrentRow(row);
    if (row < 0 || row >= watchlists_.size())
        return;
    const fincept::Watchlist wl = watchlists_[row];

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Rename…")), &QAction::triggered, this, [this, row]() { rename_watchlist(row); });
    connect(menu->addAction(tr("Description…")), &QAction::triggered, this,
            [this, row]() { edit_list_description(row); });
    QMenu* colour = menu->addMenu(tr("Colour"));
    for (int i = 0; i < 8; ++i) {
        const QColor c = colors::chart_color(i);
        QAction* a = colour->addAction(wlsa_swatch(c), c.name());
        const QString name = c.name();
        connect(a, &QAction::triggered, this, [this, row, name]() { set_list_color(row, name); });
    }
    colour->addSeparator();
    connect(colour->addAction(tr("Custom…")), &QAction::triggered, this, [this, row]() { set_list_color(row, {}); });
    QAction* def = menu->addAction(tr("Set as default"));
    def->setEnabled(!wl.is_default);
    connect(def, &QAction::triggered, this, [this, row]() { set_default_list(row); });
    menu->addSeparator();
    connect(menu->addAction(tr("Duplicate…")), &QAction::triggered, this, [this, row]() { duplicate_list(row); });
    QMenu* merge = menu->addMenu(tr("Merge into"));
    for (const auto& other : watchlists_) {
        if (other.id == wl.id)
            continue;
        const QString tid = other.id;
        connect(merge->addAction(other.name), &QAction::triggered, this,
                [this, row, tid]() { merge_list(row, tid, false); });
    }
    merge->setEnabled(watchlists_.size() > 1);
    QAction* up = menu->addAction(tr("Move up"));
    up->setEnabled(row > 0);
    connect(up, &QAction::triggered, this, [this, row]() { move_list(row, -1); });
    QAction* down = menu->addAction(tr("Move down"));
    down->setEnabled(row + 1 < watchlists_.size());
    connect(down, &QAction::triggered, this, [this, row]() { move_list(row, 1); });
    menu->addSeparator();
    connect(menu->addAction(tr("Export list (JSON)…")), &QAction::triggered, this, &WatchlistScreen::export_json);
    connect(menu->addAction(tr("Delete…")), &QAction::triggered, this, &WatchlistScreen::on_delete_watchlist);
    menu->actions().constLast()->setProperty("fincept_agent_risk", "delete");
    menu->popup(wl_list_->viewport()->mapToGlobal(pos));
}

void WatchlistScreen::show_list_actions_menu() {
    if (current_wl_id_.isEmpty())
        return;
    const int row = row_of_list(current_wl_id_);
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction* imp = menu->addAction(tr("Import symbols (CSV / JSON)…"));
    imp->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(imp, &QAction::triggered, this, &WatchlistScreen::on_import_csv);
    QAction* exp = menu->addAction(tr("Export visible columns (CSV)…"));
    exp->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exp, &QAction::triggered, this, &WatchlistScreen::on_export_csv);
    connect(menu->addAction(tr("Export list (JSON)…")), &QAction::triggered, this, &WatchlistScreen::export_json);
    menu->addSeparator();
    connect(menu->addAction(tr("Duplicate…")), &QAction::triggered, this, [this, row]() { duplicate_list(row); });
    QMenu* merge = menu->addMenu(tr("Merge into"));
    for (const auto& other : watchlists_) {
        if (other.id == current_wl_id_)
            continue;
        const QString tid = other.id;
        connect(merge->addAction(other.name), &QAction::triggered, this,
                [this, row, tid]() { merge_list(row, tid, false); });
    }
    merge->setEnabled(watchlists_.size() > 1);
    QAction* bt = menu->addAction(tr("Backtest this list"));
    bt->setEnabled(!stocks_.isEmpty());
    connect(bt, &QAction::triggered, this, [this]() { backtest_symbols(model_->symbols()); });
    menu->addSeparator();
    QAction* order = menu->addAction(tr("Back to my order"));
    order->setEnabled(view_->is_sorted());
    connect(order, &QAction::triggered, view_, &WatchlistView::clear_sort);
    connect(menu->addAction(tr("Reset this list's layout")), &QAction::triggered, this, [this]() {
        WatchlistColumnPrefs::forget(current_wl_id_);
        layout_save_timer_->stop();
        applying_layout_ = true;
        layout_ = WatchlistColumnPrefs::builtin_default();
        {
            QSignalBlocker block(group_box_);
            group_box_->setCurrentIndex(0);
        }
        model_->set_grouping(WatchlistGrouping::None);
        view_->setRootIsDecorated(false);
        view_->apply_layout(layout_);
        map_view_->set_mode(layout_.map_size, layout_.map_horizon);
        set_view_mode(layout_.view, false);
        set_detail_visible(layout_.detail_visible, false);
        applying_layout_ = false;
        update_data_needs();
        save_layout_now();
    });
    menu->actions().constLast()->setProperty("fincept_agent_risk", "destructive"); // "Reset this list's layout"
    menu->addSeparator();
    connect(menu->addAction(tr("Delete list…")), &QAction::triggered, this, &WatchlistScreen::on_delete_watchlist);
    menu->actions().constLast()->setProperty("fincept_agent_risk", "delete");
    menu->popup(list_menu_btn_->mapToGlobal(QPoint(0, list_menu_btn_->height())));
}

void WatchlistScreen::show_stock_menu(const QPoint& pos) {
    const QModelIndex idx = view_->indexAt(pos);
    if (!idx.isValid())
        return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (idx.data(WatchlistModel::IsGroupRole).toBool()) {
        connect(menu->addAction(tr("Expand all groups")), &QAction::triggered, view_, &QTreeView::expandAll);
        connect(menu->addAction(tr("Collapse all groups")), &QAction::triggered, view_, &QTreeView::collapseAll);
        menu->popup(view_->viewport()->mapToGlobal(pos));
        return;
    }

    // Act on the selection when the clicked row is part of it, else on the clicked row.
    const QString clicked = idx.data(WatchlistModel::SymbolRole).toString();
    QStringList syms = view_->selected_symbols();
    if (!syms.contains(clicked)) {
        view_->select_symbols({clicked}, clicked);
        on_current_row_changed();
        syms = {clicked};
    }
    const QString symbol = clicked;
    const auto* st = model_->state(symbol);
    const QString asset = st ? st->asset_class() : watchlist_asset_class(symbol);

    if (syms.size() == 1) {
        connect(menu->addAction(tr("Open in Equity Research")), &QAction::triggered, this,
                [this, symbol]() { open_symbol_in(QStringLiteral("equity_research"), symbol); });
        // Indices, FX and futures / crypto pairs are not broker-tradable equities.
        if (asset == QLatin1String("equity"))
            connect(menu->addAction(tr("Open in Equity Trading")), &QAction::triggered, this,
                    [this, symbol]() { open_symbol_in(QStringLiteral("equity_trading"), symbol); });
        connect(menu->addAction(tr("Open in News")), &QAction::triggered, this,
                [this, symbol]() { open_symbol_in(QStringLiteral("news"), symbol); });
        menu->addSeparator();
        connect(menu->addAction(tr("Alerts…")), &QAction::triggered, this, [this, symbol]() { open_alerts(symbol); });
        connect(menu->addAction(tr("Edit note…")), &QAction::triggered, this, [this, symbol]() { edit_notes(symbol); });
    } else {
        menu->addSection(tr("%n symbols", "", static_cast<int>(syms.size())));
    }
    QAction* grp = menu->addAction(tr("Set group…"));
    grp->setEnabled(WatchlistRepository::instance().has_groups_column());
    connect(grp, &QAction::triggered, this, [this, syms]() { set_group_for(syms); });
    menu->addSeparator();
    connect(menu->addAction(tr("Move to top")), &QAction::triggered, this, [this]() { move_selection_to_edge(true); });
    connect(menu->addAction(tr("Move to bottom")), &QAction::triggered, this,
            [this]() { move_selection_to_edge(false); });
    QMenu* copy_to = menu->addMenu(tr("Copy to list"));
    QMenu* move_to = menu->addMenu(tr("Move to list"));
    for (const auto& wl : watchlists_) {
        if (wl.id == current_wl_id_)
            continue;
        const QString tid = wl.id;
        connect(copy_to->addAction(wl.name), &QAction::triggered, this,
                [this, syms, tid]() { copy_symbols_to(syms, tid, false); });
        connect(move_to->addAction(wl.name), &QAction::triggered, this,
                [this, syms, tid]() { copy_symbols_to(syms, tid, true); });
    }
    copy_to->setEnabled(watchlists_.size() > 1);
    move_to->setEnabled(watchlists_.size() > 1);
    connect(menu->addAction(syms.size() == 1 ? tr("Copy symbol") : tr("Copy symbols")), &QAction::triggered, this,
            [syms]() { QApplication::clipboard()->setText(syms.join(QLatin1Char('\n'))); });
    connect(menu->addAction(syms.size() == 1 ? tr("Backtest this symbol") : tr("Backtest these symbols")),
            &QAction::triggered, this, [this, syms]() { backtest_symbols(syms); });
    menu->addSeparator();
    QAction* rm = menu->addAction(tr("Remove from watchlist (%1)").arg(syms.size()));
    rm->setProperty("fincept_agent_risk", "delete");
    rm->setShortcut(QKeySequence::Delete);
    connect(rm, &QAction::triggered, this, [this, syms]() { remove_symbols(syms); });
    menu->popup(view_->viewport()->mapToGlobal(pos));
}

void WatchlistScreen::open_symbol_in(const QString& screen_id, const QString& symbol) {
    if (symbol.isEmpty())
        return;
    // nav.open_symbol navigates first (constructing the target if needed) and then
    // delivers the ticker through IGroupLinked.
    EventBus::instance().publish("nav.open_symbol", {{"screen_id", screen_id}, {"symbol", symbol}});
}

void WatchlistScreen::backtest_symbols(const QStringList& symbols) {
    if (symbols.isEmpty())
        return;
    QJsonObject config;
    config["symbols"] = QJsonArray::fromStringList(symbols);
    services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
    EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
}

// ── Notes, groups, alerts, copy / move ───────────────────────────────────────

void WatchlistScreen::edit_notes(const QString& symbol) {
    const auto* st = model_->state(symbol);
    if (!st || current_wl_id_.isEmpty())
        return;
    bool ok = false;
    const QString notes = QInputDialog::getMultiLineText(this, tr("Notes — %1").arg(symbol), tr("Notes:"),
                                                         st->stock.notes, &ok);
    if (!ok || notes == st->stock.notes)
        return;
    if (WatchlistRepository::instance().set_stock_notes(current_wl_id_, symbol, notes).is_err()) {
        show_feedback(tr("Could not save the note."), FeedbackTone::Error, 8000);
        return;
    }
    const QString group = st->stock.group_name;
    for (auto& s : stocks_) {
        if (s.symbol == symbol)
            s.notes = notes;
    }
    model_->set_stock_fields(symbol, notes, group);
    if (detail_->symbol() == symbol) {
        if (const auto* r = model_->state(symbol))
            detail_->update_row(*r);
    }
}

void WatchlistScreen::set_group_for(const QStringList& symbols) {
    if (symbols.isEmpty() || current_wl_id_.isEmpty())
        return;
    auto& repo = WatchlistRepository::instance();
    if (!repo.has_groups_column()) {
        show_feedback(tr("Groups need the database upgrade of this version (migration v054); it runs at the next "
                         "start."),
                      FeedbackTone::Warning, 10000);
        return;
    }
    QStringList groups;
    QString current;
    for (const auto& s : stocks_) {
        if (!s.group_name.isEmpty() && !groups.contains(s.group_name))
            groups << s.group_name;
        if (symbols.size() == 1 && s.symbol == symbols.first())
            current = s.group_name;
    }
    groups.sort(Qt::CaseInsensitive);
    groups.prepend(QString()); // "no group"
    bool ok = false;
    const QString g = QInputDialog::getItem(this, tr("Set group"),
                                            tr("Group for %n symbol(s) (leave empty for none):", "",
                                               static_cast<int>(symbols.size())),
                                            groups, std::max(0, static_cast<int>(groups.indexOf(current))), true, &ok)
                          .trimmed();
    if (!ok)
        return;
    if (repo.set_stock_group(current_wl_id_, symbols, g).is_err()) {
        show_feedback(tr("Could not save the group."), FeedbackTone::Error, 8000);
        return;
    }
    load_stocks();
    if (model_->grouping() != WatchlistGrouping::Custom && !g.isEmpty()) {
        show_feedback(tr("Grouped as \"%1\".").arg(g), FeedbackTone::Success, 8000);
        QPushButton* show = wlsa_action_button(feedback_bar_, tr("Show my groups"));
        connect(show, &QPushButton::clicked, this, [this]() {
            const int i = group_box_->findData(watchlist_grouping_key(WatchlistGrouping::Custom));
            if (i >= 0)
                group_box_->setCurrentIndex(i);
            clear_feedback();
        });
        feedback_actions_->addWidget(show);
    }
}

void WatchlistScreen::open_alerts(const QString& symbol) {
    const auto* st = model_->state(symbol);
    auto* dlg = new WatchlistAlertDialog(symbol, current_wl_id_, st && st->has_quote ? &st->quote : nullptr, this);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void WatchlistScreen::copy_symbols_to(const QStringList& symbols, const QString& target_id, bool move) {
    const int trow = row_of_list(target_id);
    if (trow < 0 || symbols.isEmpty() || current_wl_id_.isEmpty())
        return;
    auto& repo = WatchlistRepository::instance();
    QSet<QString> there;
    if (auto r = repo.get_stocks(target_id); r.is_ok()) {
        for (const auto& s : r.value())
            there.insert(s.symbol.toUpper());
    }
    const bool groups_ok = repo.has_groups_column();
    QStringList copied, skipped;
    for (const QString& sym : symbols) {
        const auto* st = model_->state(sym);
        if (!st)
            continue;
        if (there.contains(sym.toUpper())) {
            skipped << sym;
        } else if (repo.add_stock(target_id, sym, st->stock.name, st->stock.exchange).is_ok()) {
            if (!st->stock.notes.isEmpty())
                repo.set_stock_notes(target_id, sym, st->stock.notes);
            if (!st->stock.group_name.isEmpty() && groups_ok)
                repo.set_stock_group(target_id, {sym}, st->stock.group_name);
            copied << sym;
        } else {
            continue;
        }
        if (move)
            repo.remove_stock(current_wl_id_, sym); // also when it was already there: it moved
    }
    if (auto c = repo.stock_counts(); c.is_ok())
        counts_ = c.value();
    if (move)
        load_stocks();
    rebuild_sidebar();
    const QString target = watchlists_[trow].name;
    QString text = move ? tr("Moved %1 to \"%2\".").arg(wlsa_short_list(copied + skipped), target)
                        : tr("Copied %1 to \"%2\".").arg(wlsa_short_list(copied), target);
    if (!move && !skipped.isEmpty())
        text += QLatin1Char(' ') + tr("Already there: %1.").arg(wlsa_short_list(skipped));
    show_feedback(text, FeedbackTone::Success, 6000);
}

// ── Manual order ─────────────────────────────────────────────────────────────

QStringList WatchlistScreen::base_order_for_move() const {
    QStringList order = model_->symbols();
    if (!view_->is_sorted())
        return order;
    // A column sort is showing: the order on screen becomes the new manual order.
    const int col = view_->header()->sortIndicatorSection();
    const bool asc = view_->header()->sortIndicatorOrder() == Qt::AscendingOrder;
    std::stable_sort(order.begin(), order.end(), [this, col, asc](const QString& a, const QString& b) {
        const QModelIndex ia = model_->index_of_symbol(a, col);
        const QModelIndex ib = model_->index_of_symbol(b, col);
        return asc ? proxy_->less(ia, ib) : proxy_->less(ib, ia);
    });
    return order;
}

void WatchlistScreen::move_rows(const QStringList& moved, const QString& anchor, bool after, const QString& group_key,
                                bool has_group) {
    if (current_wl_id_.isEmpty() || moved.isEmpty())
        return;
    const bool was_sorted = view_->is_sorted();
    const QStringList order = base_order_for_move();
    const QSet<QString> mset(moved.cbegin(), moved.cend());
    QStringList mv;
    QStringList rest;
    for (const QString& s : order)
        (mset.contains(s) ? mv : rest) << s;
    if (mv.isEmpty())
        return;
    qsizetype at = rest.size();
    if (!anchor.isEmpty()) {
        if (mset.contains(anchor)) {
            // Dropped on one of the moved rows: they stay where that row was.
            const qsizetype idx = order.indexOf(anchor);
            at = 0;
            for (qsizetype k = 0; k < idx; ++k)
                if (!mset.contains(order[k]))
                    ++at;
        } else {
            at = rest.indexOf(anchor);
            if (at < 0)
                at = rest.size();
            else if (after)
                ++at;
        }
    }
    QStringList next = rest;
    for (qsizetype i = 0; i < mv.size(); ++i)
        next.insert(at + i, mv[i]);

    auto& repo = WatchlistRepository::instance();
    bool changed = false;
    if (next != model_->symbols()) {
        if (repo.set_stock_order(current_wl_id_, next).is_err()) {
            show_feedback(tr("Could not save the new order."), FeedbackTone::Error, 8000);
            return;
        }
        changed = true;
    }
    if (has_group && model_->grouping() == WatchlistGrouping::Custom) {
        bool differs = false;
        for (const QString& s : mv) {
            const auto* st = model_->state(s);
            differs = differs || (st && st->stock.group_name != group_key);
        }
        if (differs && repo.set_stock_group(current_wl_id_, mv, group_key).is_ok())
            changed = true;
    }
    if (!changed)
        return;
    if (was_sorted)
        view_->clear_sort();
    load_stocks();
    view_->select_symbols(mv, mv.first());
    if (was_sorted)
        show_feedback(tr("Saved as your order — the column sort was turned off to show it."), FeedbackTone::Info,
                      6000);
}

void WatchlistScreen::move_selection(int delta) {
    const QStringList sel = view_->selected_symbols();
    if (sel.isEmpty() || delta == 0 || current_wl_id_.isEmpty())
        return;
    const QSet<QString> selset(sel.cbegin(), sel.cend());
    QStringList order = base_order_for_move();
    const auto same_group = [this](const QString& a, const QString& b) {
        if (!model_->grouped())
            return true;
        const auto* sa = model_->state(a);
        const auto* sb = model_->state(b);
        return sa && sb && model_->group_key_of(*sa) == model_->group_key_of(*sb);
    };
    // Each selected row passes its nearest unselected neighbour (in its own group when grouped).
    if (delta < 0) {
        for (qsizetype i = 0; i < order.size(); ++i) {
            if (!selset.contains(order[i]))
                continue;
            qsizetype j = i - 1;
            while (j >= 0 && !same_group(order[i], order[j]))
                --j;
            if (j >= 0 && !selset.contains(order[j]))
                order.move(i, j);
        }
    } else {
        for (qsizetype i = order.size() - 1; i >= 0; --i) {
            if (!selset.contains(order[i]))
                continue;
            qsizetype j = i + 1;
            while (j < order.size() && !same_group(order[i], order[j]))
                ++j;
            if (j < order.size() && !selset.contains(order[j]))
                order.move(i, j);
        }
    }
    if (order == model_->symbols())
        return;
    if (WatchlistRepository::instance().set_stock_order(current_wl_id_, order).is_err()) {
        show_feedback(tr("Could not save the new order."), FeedbackTone::Error, 8000);
        return;
    }
    if (view_->is_sorted())
        view_->clear_sort();
    const QString cur = view_->current_symbol();
    load_stocks();
    view_->select_symbols(sel, cur.isEmpty() ? sel.first() : cur);
    const QModelIndex idx = proxy_->mapFromSource(model_->index_of_symbol(cur.isEmpty() ? sel.first() : cur, 0));
    if (idx.isValid())
        view_->scrollTo(idx);
}

void WatchlistScreen::move_selection_to_edge(bool top) {
    const QStringList sel = view_->selected_symbols();
    if (sel.isEmpty() || current_wl_id_.isEmpty())
        return;
    const QSet<QString> selset(sel.cbegin(), sel.cend());
    QStringList mv;
    QStringList rest;
    for (const QString& s : base_order_for_move())
        (selset.contains(s) ? mv : rest) << s;
    const QStringList order = top ? mv + rest : rest + mv;
    if (order == model_->symbols())
        return;
    if (WatchlistRepository::instance().set_stock_order(current_wl_id_, order).is_err()) {
        show_feedback(tr("Could not save the new order."), FeedbackTone::Error, 8000);
        return;
    }
    if (view_->is_sorted())
        view_->clear_sort();
    load_stocks();
    view_->select_symbols(sel, sel.first());
}

// ── ADD box autocomplete ─────────────────────────────────────────────────────

void WatchlistScreen::on_add_text_edited(const QString& text) {
    if (text.trimmed().isEmpty()) {
        autocomplete_timer_->stop();
        if (completer_->popup())
            completer_->popup()->hide();
        return;
    }
    autocomplete_timer_->start();
}

void WatchlistScreen::run_autocomplete() {
    static const QRegularExpression kSep(QStringLiteral("[,;\\s]+"));
    const QStringList tokens = add_input_->text().split(kSep, Qt::SkipEmptyParts);
    const QString token = tokens.isEmpty() ? QString() : tokens.last();
    if (token.size() < 2 || add_input_->text().endsWith(QLatin1Char(',')) ||
        add_input_->text().endsWith(QLatin1Char(' '))) {
        if (completer_->popup())
            completer_->popup()->hide();
        return;
    }
    static int request_seq = 0; // request ids only — not appearance
    autocomplete_request_ = QStringLiteral("watchlist-add-%1").arg(++request_seq);
    services::MarketSearchService::instance().search(token, QString(), 10, autocomplete_request_);
}

void WatchlistScreen::on_completion_chosen(const QString& text) {
    const QString sym = text.section(QLatin1Char(' '), 0, 0).trimmed();
    if (sym.isEmpty())
        return;
    // Replace the token being typed with the chosen symbol and add. (A global regex replace of
    // "[^,;\\s]*$" also matched the empty string after the token, so every pick became
    // "AAPLAAPL" and was rejected as not found.)
    QString t = add_input_->text();
    qsizetype start = t.size();
    while (start > 0) {
        const QChar c = t.at(start - 1);
        if (c == QLatin1Char(',') || c == QLatin1Char(';') || c.isSpace())
            break;
        --start;
    }
    t = t.left(start) + sym;
    add_input_->setText(t);
    autocomplete_request_.clear();
    QTimer::singleShot(0, this, &WatchlistScreen::on_add_stock);
}

} // namespace fincept::screens
