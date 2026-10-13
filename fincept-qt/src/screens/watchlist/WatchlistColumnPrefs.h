#pragma once
// WatchlistColumnPrefs — column chooser + persisted column layout for a
// QTableWidget (built for the Watchlist screen's quote table).
//
// Right-click the table header → tick/untick columns, or "Reset columns".
// Hidden columns, column widths and the sort column/order are saved (debounced)
// to SettingsRepository under one key and restored when attached, so the
// table comes back the way the user left it.
//
// Usage (one line, after the table's headers are set):
//     WatchlistColumnPrefs::attach(table_, QStringLiteral("watchlist.table_columns"));

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QTableWidget;
class QPoint;

namespace fincept::screens {

class WatchlistColumnPrefs : public QObject {
    Q_OBJECT
  public:
    /// Attach to `table` (the helper is parented to it). `locked_column` can
    /// never be hidden (the symbol column other code reads rows from).
    static WatchlistColumnPrefs* attach(QTableWidget* table, const QString& settings_key, int locked_column = 0);

  private:
    WatchlistColumnPrefs(QTableWidget* table, QString settings_key, int locked_column);

    void show_menu(const QPoint& pos);
    void restore_async();
    void apply(const QJsonObject& o);
    void schedule_save();
    void save();
    void reset();

    QPointer<QTableWidget> table_;
    QString key_;
    int locked_column_ = 0;
    QJsonObject defaults_; ///< widths at attach time, for "Reset columns"
    QTimer save_timer_;
    bool restoring_ = false;
    /// False until the stored layout has been read and applied: a resize
    /// during startup must not save the defaults over the user's layout.
    bool ready_ = false;
};

} // namespace fincept::screens
