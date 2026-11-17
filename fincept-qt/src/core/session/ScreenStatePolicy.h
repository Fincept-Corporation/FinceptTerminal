#pragma once
// Pure decision logic behind ScreenStateManager: which stored copy of a panel's
// UI state a restore uses, and the order gate that stops an older write from
// landing on top of a newer one. Qt Core only and header-only, so
// tests/tst_screen_state_policy.cpp exercises it without the application.
//
// Background. A panel's state lives in two rows of cache.db's screen_state:
//   * the INSTANCE row ("uuid:<panel instance id>"), the panel's own copy;
//   * the SCREEN-TYPE row (the screen's state_key()), the latest state any
//     panel of that type wrote. A panel opened for the first time starts from
//     it.
// Builds before this fix wrote the debounced saves (every edit) to the
// screen-type row only and the instance row only on hide/quit. After an
// abnormal exit the instance row was therefore OLDER than the screen-type row,
// yet the restore always preferred it — the newest edits were lost, and the
// next save wrote the old state back over them.

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QtGlobal>

namespace fincept::session {

/// One stored copy of a screen's state (the payload itself is not needed for
/// the decision).
struct StoredScreenState {
    bool found = false;  ///< a row exists AND its state_version matches the screen's
    QString updated_at;  ///< the row's updated_at column, as SQLite wrote it (UTC)
    QString writer_uuid; ///< screen-type row only: the panel instance that wrote it
                         ///< (empty: an unbound writer or a build that did not record it)
};

enum class RestoreSource {
    None,       ///< nothing stored: the screen keeps its defaults
    Instance,   ///< the panel's own row
    ScreenType, ///< the screen-type row
};

/// Parses an updated_at value. SQLite wrote either "YYYY-MM-DDTHH:MM:SS.sssZ"
/// (strftime, used by every screen_state write) or "YYYY-MM-DD HH:MM:SS"
/// (datetime('now'), older rows); both are UTC. Returns a null QDateTime for an
/// empty or unparseable value, which sorts as "oldest".
inline QDateTime parse_state_timestamp(const QString& text) {
    QString s = text.trimmed();
    if (s.isEmpty())
        return {};
    s.replace(QLatin1Char(' '), QLatin1Char('T'));
    const int t = s.indexOf(QLatin1Char('T'));
    const bool has_zone = s.endsWith(QLatin1Char('Z')) ||
                          (t > 0 && (s.indexOf(QLatin1Char('+'), t) > 0 || s.indexOf(QLatin1Char('-'), t) > 0));
    if (!has_zone)
        s += QLatin1Char('Z');
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(s, Qt::ISODate);
    return dt.isValid() ? dt.toUTC() : QDateTime{};
}

/// Which row a restore of panel `instance_uuid` uses. The newest copy wins:
///   * only one row exists        -> that row (the screen-type row is the
///                                   first-open / migration fallback);
///   * both exist                 -> the instance row, unless the screen-type
///                                   row is strictly newer AND was written by
///                                   this panel (or by an unknown writer, i.e.
///                                   a build that recorded none) — a row another
///                                   panel of the same type wrote must not
///                                   replace this panel's own state.
/// Ties and unparseable timestamps keep the instance row.
inline RestoreSource choose_restore_source(const StoredScreenState& instance_row, const StoredScreenState& type_row,
                                           const QString& instance_uuid) {
    if (!instance_row.found)
        return type_row.found ? RestoreSource::ScreenType : RestoreSource::None;
    if (!type_row.found)
        return RestoreSource::Instance;
    if (!type_row.writer_uuid.isEmpty() && type_row.writer_uuid != instance_uuid)
        return RestoreSource::Instance;
    const QDateTime t_type = parse_state_timestamp(type_row.updated_at);
    if (!t_type.isValid())
        return RestoreSource::Instance;
    const QDateTime t_inst = parse_state_timestamp(instance_row.updated_at);
    if (!t_inst.isValid() || t_type > t_inst)
        return RestoreSource::ScreenType;
    return RestoreSource::Instance;
}

/// Orders writes to the same row. Every write gets a sequence number on the
/// GUI thread when it is dispatched (dispatch order = the order the user made
/// the changes); writes then run on pool threads in any order. A write is
/// applied only if no write dispatched later has already landed on its row, so
/// a slow, older write can never overwrite a newer one (and an older write
/// queued before a delete cannot resurrect the row). Not thread-safe by itself:
/// callers hold one mutex around admit() + the write + mark_applied().
class WriteOrderGate {
  public:
    /// True when a write with sequence `seq` may be applied to `row_key`.
    bool admit(const QString& row_key, quint64 seq) const { return seq > applied_.value(row_key, 0); }

    /// Records that the write `seq` landed on `row_key` (call only on success).
    void mark_applied(const QString& row_key, quint64 seq) {
        quint64& v = applied_[row_key];
        if (seq > v)
            v = seq;
    }

    quint64 last_applied(const QString& row_key) const { return applied_.value(row_key, 0); }

  private:
    QHash<QString, quint64> applied_;
};

} // namespace fincept::session
