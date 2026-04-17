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

    // Window geometry — scoped per window_id (0 = primary)
    void save_geometry(int window_id, const QByteArray& geometry, const QByteArray& state);
    QByteArray load_geometry(int window_id) const;
    QByteArray load_state(int window_id) const;

    // Track how many windows were open at last shutdown
    void save_window_count(int count);
    int load_window_count() const;

    // ADS dock layout — scoped per window_id
    void save_dock_layout(int window_id, const QByteArray& layout);
    QByteArray load_dock_layout(int window_id) const;

    // Dock layout version — used to auto-discard stale layouts after code changes
    void set_dock_layout_version(int window_id, int version);
    int dock_layout_version(int window_id) const;

    // Named perspectives (saved layouts)
    void save_perspectives(QSettings& source);
    void load_perspectives(QSettings& target) const;

    // Last active screen
    void set_last_screen(const QString& screen_id);
    QString last_screen() const;

  signals:
    void session_started();

  private:
    SessionManager() = default;
    QElapsedTimer elapsed_;
    mutable QSettings settings_{"Fincept", "FinceptTerminal"};
};

} // namespace fincept
