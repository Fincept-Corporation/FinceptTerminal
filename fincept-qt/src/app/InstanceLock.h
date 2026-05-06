#pragma once

// InstanceLock — single-instance enforcement + cross-instance IPC.
//
// Replaces the SingleApplication library, which used QSharedMemory +
// QSystemSemaphore. That combo leaks on macOS Qt 6.6+ (QTBUG-111855), causing
// the app to silently exit on Finder/`open` launch — see issues #234 and #252.
//
// Design: one QLocalServer per profile key. Filesystem-addressable (Unix
// socket on POSIX, named pipe on Windows), so stale locks from a crashed
// previous primary can be detected and removed — something the semaphore
// approach can't do portably. No path-length limits, no sandbox quirks, no
// per-platform IPC restrictions.
//
// Wire format: 4-byte big-endian length prefix, followed by a JSON object
// `{"args": [...]}` carrying the secondary's argv. Length-prefixing handles
// short reads on slow connections; JSON keeps the format extensible without
// breaking older builds.

#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fincept {

class InstanceLock : public QObject {
    Q_OBJECT
  public:
    enum class Status { Primary, Secondary };

    explicit InstanceLock(QObject* parent = nullptr);
    ~InstanceLock() override;

    /// Try to become the primary instance for `key`. If another primary is
    /// already running, send `args` to it and return Secondary — the caller
    /// should exit cleanly. Otherwise begin listening for secondaries and
    /// return Primary.
    ///
    /// Failure mode: if listen() fails twice (once normally, once after
    /// removing a stale socket), the call still returns Primary. Better to
    /// run as an unsynchronised primary than to refuse to start. Logged.
    Status acquire(const QString& key, const QStringList& args = {});

    /// Map an arbitrary key into a stable, length-bounded local-socket name.
    /// Static so tests can verify the mapping. Uses SHA-1 truncation for the
    /// uniqueness component — long enough to avoid collisions, short enough
    /// to fit inside macOS POSIX semaphore-style namespaces (the original
    /// bug at the heart of #252).
    static QString socket_name_for(const QString& key);

  signals:
    /// Emitted on the primary when a secondary connects and sends its argv.
    /// `args` is the literal argv vector the secondary passed to `acquire()`.
    void message_received(const QStringList& args);

  private slots:
    void on_new_connection();

  private:
    QLocalServer* server_ = nullptr;
    QString name_;

    bool try_listen(const QString& name);
    bool send_to_existing(const QString& name, const QStringList& args);
};

} // namespace fincept
