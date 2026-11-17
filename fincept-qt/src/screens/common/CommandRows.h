#pragma once
// CommandRows.h — header-only helpers for screens that put their own rows into the header
// search box (core/command/CommandRegistry.h). Qt Core only, no Q_OBJECT.
//
// A screen's .cpp registers its rows ONCE per process through a file-scope registrar, so the
// rows exist before the screen is ever built (the box navigates first, then the screen gets
// the section through ISectionLinked::open_section):
//
//   namespace {
//   QList<fincept::command::CommandItem> cal_command_rows() {
//       namespace rows = fincept::screens::command_rows;
//       const QString where = CalendarsScreen::tr("Calendars");
//       return {rows::section("calendars", "earnings", CalendarsScreen::tr("Earnings calendar"),
//                             {"earnings", "eps", "results"}, where)};
//   }
//   void cal_install_commands() {
//       fincept::screens::command_rows::install(QStringLiteral("calendars"), &cal_command_rows);
//   }
//   } // namespace
//   Q_COREAPP_STARTUP_FUNCTION(cal_install_commands)
//
// Q_COREAPP_STARTUP_FUNCTION runs the function inside the QCoreApplication constructor (before
// any GUI exists): install() registers the rows at once and again on every
// LanguageManager::language_changed — the first one comes from LanguageManager::initialize()
// at start-up, after the translator is installed — so tr() titles follow the language.
// A row whose id equals a seeded one ("<screen>.<section>", ':' and '/' -> '.') replaces it:
// the seed never overwrites a row a screen registered first.

#include "core/command/CommandRank.h"
#include "core/command/CommandRegistry.h"
#include "core/i18n/LanguageManager.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace fincept::screens::command_rows {

/// The row id of a section link: "<screen>.<section>" with ':' and '/' turned into '.'
/// ("economics.fred", "backtesting.engine.vectorbt", "ai_quant_lab.module.garch.view").
inline QString row_id(const QString& screen, const QString& section) {
    QString part = section;
    part.replace(QLatin1Char(':'), QLatin1Char('.'));
    part.replace(QLatin1Char('/'), QLatin1Char('.'));
    return screen + QLatin1Char('.') + part;
}

/// "Economics › FRED" — where a row lands, for its subtitle.
inline QString where(const QString& context, const QString& title) {
    return QStringLiteral("%1 › %2").arg(context, title);
}

/// A kScreen row that opens `section` (and optionally `focus`) of `screen`. `context` is the
/// screen's own (translated) name; the subtitle defaults to "<context> › <title>".
inline command::CommandItem section(const QString& screen, const QString& section, const QString& title,
                                    const QStringList& keywords, const QString& context,
                                    const QString& subtitle = QString(), const QString& focus = QString()) {
    command::CommandItem it;
    it.id = row_id(screen, section);
    it.title = title;
    it.keywords = keywords;
    it.group = QString::fromLatin1(command::groups::kScreen);
    it.subtitle = subtitle.isEmpty() ? where(context, title) : subtitle;
    it.screen_id = screen;
    it.section = section;
    it.focus = focus;
    it.owner = screen;
    it.context = context;
    return it;
}

using RowsFn = QList<command::CommandItem> (*)();

/// Registers `rows()` under `owner` now and again after every language change (see the top of
/// this file). Call once per process, from a Q_COREAPP_STARTUP_FUNCTION registrar.
inline void install(const QString& owner, RowsFn rows) {
    if (!rows)
        return;
    auto apply = [owner, rows]() {
        QList<command::CommandItem> list = rows();
        for (command::CommandItem& it : list)
            if (it.owner.isEmpty())
                it.owner = owner;
        command::CommandRegistry::instance().register_items(list);
    };
    apply();
    QObject::connect(&i18n::LanguageManager::instance(), &i18n::LanguageManager::language_changed,
                     &command::CommandRegistry::instance(), [apply](const QString&) { apply(); });
}

/// The rows of `rows` that `query` finds (the box's own matching rules: title, aliases, keywords,
/// context), best first, at most `cap`. Cheap — for providers over in-memory lists.
inline QList<command::CommandItem> best_matches(const QList<command::CommandItem>& rows, const QString& query,
                                                int cap) {
    namespace rk = command::rank;
    const rk::Query q = rk::prepare_query(query);
    if (q.empty() || cap <= 0)
        return {};
    struct Hit {
        qint64 score;
        qsizetype index;
    };
    QList<Hit> hits;
    for (qsizetype i = 0; i < rows.size(); ++i) {
        const command::CommandItem& r = rows.at(i);
        rk::Fields f;
        f.title = r.title;
        f.codes = r.aliases;
        f.keywords = r.keywords;
        if (!r.context.isEmpty())
            f.keywords.append(r.context);
        const rk::Match m = rk::match(q, rk::prepare(f));
        if (m.tier >= rk::Tier::Fuzzy)
            hits.append({rk::score(m, 0, r.priority), i});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score > b.score; });
    QList<command::CommandItem> out;
    for (const Hit& h : hits) {
        if (out.size() >= cap)
            break;
        out.append(rows.at(h.index));
    }
    return out;
}

/// True when `key` is "<kind>:<rest>" (kind compared case-insensitively); `rest` gets the part after
/// the colon, case kept (ids such as file names or UUIDs are case-sensitive).
inline bool split_kind(const QString& key, const char* kind, QString* rest) {
    const QString prefix = QString::fromLatin1(kind) + QLatin1Char(':');
    if (!key.startsWith(prefix, Qt::CaseInsensitive))
        return false;
    if (rest)
        *rest = key.mid(prefix.size()).trimmed();
    return true;
}

} // namespace fincept::screens::command_rows
