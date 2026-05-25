#include "app/PhaseOneServerCli.h"
#include "app/PhaseOneServerPaths.h"
#include "core/config/AppPaths.h"
#include "multiuser/server/PhaseOneServerRuntime.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace fincept;

namespace {

class PhaseOneServerRuntimeTest : public QObject {
    Q_OBJECT

  private slots:
    void fincept_binary_path_is_injected();
    void cli_parses_server_options();
    void cli_requires_explicit_server_host();
    void server_paths_ignore_profile_for_authoritative_db_path();
    void runtime_rejects_missing_host();
    void runtime_starts_server_and_runs_migrations();
    void runtime_accepts_post_json_bodies_over_tcp();
};

void PhaseOneServerRuntimeTest::fincept_binary_path_is_injected() {
    QVERIFY(qEnvironmentVariableIsSet("FINCEPT_TERMINAL_BINARY"));
    QVERIFY(!qEnvironmentVariable("FINCEPT_TERMINAL_BINARY").isEmpty());
}

void PhaseOneServerRuntimeTest::cli_parses_server_options() {
    const char* argv[] = {"fincept",     "--server",      "--server-host", "127.0.0.1", "--server-port",
                          "45450",       "--server-db",   "/tmp/phase1.db", "--profile", "ops",
                          "--",          "--theme",       "light"};

    const PhaseOneServerCliParseResult parsed =
        PhaseOneServerCli::parse(static_cast<int>(sizeof(argv) / sizeof(argv[0])), argv);

    QVERIFY(parsed.ok);
    QCOMPARE(parsed.options.mode, PhaseOneProcessMode::Server);
    QCOMPARE(parsed.options.host, QString("127.0.0.1"));
    QCOMPARE(parsed.options.port, quint16(45450));
    QCOMPARE(parsed.options.db_path, QString("/tmp/phase1.db"));
    QCOMPARE(parsed.options.profile, QString("ops"));
    QCOMPARE(parsed.options.client_passthrough_args, QStringList({"--theme", "light"}));
}

void PhaseOneServerRuntimeTest::cli_requires_explicit_server_host() {
    const char* argv[] = {"fincept", "--server", "--profile", "ops"};

    const PhaseOneServerCliParseResult parsed =
        PhaseOneServerCli::parse(static_cast<int>(sizeof(argv) / sizeof(argv[0])), argv);

    QVERIFY(parsed.ok);
    QCOMPARE(parsed.options.mode, PhaseOneProcessMode::Server);
    QVERIFY(parsed.options.host.isEmpty());
    QCOMPARE(parsed.options.port, quint16(45450));
    QCOMPARE(parsed.options.profile, QString("ops"));
}

void PhaseOneServerRuntimeTest::server_paths_ignore_profile_for_authoritative_db_path() {
    AppPaths::clear_overrides();

    PhaseOneServerCliOptions options;
    options.mode = PhaseOneProcessMode::Server;
    options.profile = QStringLiteral("ops");

    const PhaseOneResolvedServerOptions resolved = PhaseOneServerPaths::resolve(options);

    QCOMPARE(resolved.db_path, AppPaths::root() + QStringLiteral("/server/phase1-server.db"));
    QVERIFY(!resolved.db_path.contains(QStringLiteral("/profiles/ops/")));
}

void PhaseOneServerRuntimeTest::runtime_rejects_missing_host() {
    PhaseOneServerRuntime runtime;

    PhaseOneResolvedServerOptions options;
    options.port = 45450;
    options.db_path = "/tmp/phase1.db";

    const PhaseOneServerStartResult result = runtime.start(options);

    QVERIFY(!result.ok);
    QCOMPARE(result.error_context.code, QString("invalid_server_host"));
    QVERIFY(result.error_context.message.contains("host", Qt::CaseInsensitive));
}

void PhaseOneServerRuntimeTest::runtime_starts_server_and_runs_migrations() {
    AppPaths::clear_overrides();

    QTemporaryDir temp_dir;
    QVERIFY2(temp_dir.isValid(), "temporary directory must be created");

    QTcpServer probe;
    QVERIFY2(probe.listen(QHostAddress::LocalHost, 0), "port probe must listen");
    const quint16 port = probe.serverPort();
    probe.close();

    PhaseOneResolvedServerOptions options;
    options.host = QStringLiteral("127.0.0.1");
    options.port = port;
    options.db_path = temp_dir.filePath(QStringLiteral("phase1-server.db"));

    PhaseOneServerRuntime runtime;
    const PhaseOneServerStartResult result = runtime.start(options);

    QVERIFY2(result.ok, qPrintable(result.error_context.message));
    QVERIFY(QFileInfo::exists(options.db_path));

    QSqlDatabase verification_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("phase_one_runtime_verify"));
    verification_db.setDatabaseName(options.db_path);
    QVERIFY2(verification_db.open(), qPrintable(verification_db.lastError().text()));

    QSqlQuery users_query(verification_db);
    QVERIFY2(users_query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name='users'")),
              qPrintable(users_query.lastError().text()));
    QVERIFY(users_query.next());

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY2(socket.waitForConnected(3000), qPrintable(socket.errorString()));
    socket.write("GET /phase1/health HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    QVERIFY(socket.waitForBytesWritten(3000));

    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (response.isEmpty() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        socket.waitForReadyRead(50);
        response += socket.readAll();
    }

    QVERIFY(!response.isEmpty());
    QVERIFY(response.contains("200"));
    QVERIFY(response.contains("\"status\":\"starting\""));

    verification_db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("phase_one_runtime_verify"));
}

void PhaseOneServerRuntimeTest::runtime_accepts_post_json_bodies_over_tcp() {
    AppPaths::clear_overrides();

    QTemporaryDir temp_dir;
    QVERIFY2(temp_dir.isValid(), "temporary directory must be created");

    QTcpServer probe;
    QVERIFY2(probe.listen(QHostAddress::LocalHost, 0), "port probe must listen");
    const quint16 port = probe.serverPort();
    probe.close();

    PhaseOneResolvedServerOptions options;
    options.host = QStringLiteral("127.0.0.1");
    options.port = port;
    options.db_path = temp_dir.filePath(QStringLiteral("phase1-server.db"));

    PhaseOneServerRuntime runtime;
    const PhaseOneServerStartResult start = runtime.start(options);
    QVERIFY2(start.ok, qPrintable(start.error_context.message));

    const QByteArray body = R"({"username":"admin1","password":"Passw0rd!"})";
    const QByteArray request = QByteArray("POST /phase1/admin/bootstrap HTTP/1.1\r\n")
        + "Host: 127.0.0.1\r\n"
        + "Content-Type: application/json\r\n"
        + "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
        + "Connection: close\r\n\r\n"
        + body;

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY2(socket.waitForConnected(3000), qPrintable(socket.errorString()));
    socket.write(request);
    QVERIFY(socket.waitForBytesWritten(3000));

    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (response.isEmpty() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        socket.waitForReadyRead(50);
        response += socket.readAll();
    }

    QVERIFY(!response.isEmpty());
    QVERIFY(response.contains("200"));
    QVERIFY(!response.contains("invalid_json"));
}

} // namespace

QTEST_GUILESS_MAIN(PhaseOneServerRuntimeTest)

#include "test_phase_one_server_runtime.moc"
