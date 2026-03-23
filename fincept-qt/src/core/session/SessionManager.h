#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QSettings>
#include <QVariantMap>

namespace fincept {

/// Manages session state: uptime, last-used tabs, window geometry, etc.
class SessionManager : public QObject {
    Q_OBJECT
  public:
    static SessionManager& instance();

    void start_session();
    qint64 uptime_seconds() const;
    QString uptime_string() const;

    // Tab session state
    void save_tab_state(const QString& tab_id, const QVariantMap& state);
    QVariantMap load_tab_state(const QString& tab_id) const;

    // Window geometry
    void save_geometry(const QByteArray& geometry, const QByteArray& state);
    QByteArray load_geometry() const;
    QByteArray load_state() const;

    // Last active screen
    void set_last_screen(const QString& screen_id);
    QString last_screen() const;

  signals:
    void session_started();

  private:
    SessionManager() = default;
    QElapsedTimer elapsed_;
    QSettings settings_{"Fincept", "FinceptTerminal"};
};

} // namespace fincept
