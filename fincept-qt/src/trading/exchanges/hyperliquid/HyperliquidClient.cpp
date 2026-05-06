#include "trading/exchanges/hyperliquid/HyperliquidClient.h"

#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QWebSocket>

namespace fincept::trading::hyperliquid {

namespace {
const QString kMainnetRest = QStringLiteral("https://api.hyperliquid.xyz");
const QString kMainnetWs   = QStringLiteral("wss://api.hyperliquid.xyz/ws");
const QString kTestnetRest = QStringLiteral("https://api.hyperliquid-testnet.xyz");
const QString kTestnetWs   = QStringLiteral("wss://api.hyperliquid-testnet.xyz/ws");
} // namespace

class HyperliquidClient::WsHolder : public QObject {
  public:
    explicit WsHolder(HyperliquidClient* parent) : QObject(parent), owner_(parent) {
        connect(&socket_, &QWebSocket::connected, this, [this]() {
            owner_->emit ws_state_changed(static_cast<int>(QAbstractSocket::ConnectedState));
            for (const auto& s : pending_subs_) socket_.sendTextMessage(s);
            pending_subs_.clear();
        });
        connect(&socket_, &QWebSocket::disconnected, this, [this]() {
            owner_->emit ws_state_changed(static_cast<int>(QAbstractSocket::UnconnectedState));
        });
        connect(&socket_, &QWebSocket::textMessageReceived, this,
                [this](const QString& s) {
                    auto doc = QJsonDocument::fromJson(s.toUtf8());
                    if (doc.isObject()) emit owner_->ws_message(doc.object());
                });
    }

    void open(const QString& url) { socket_.open(QUrl(url)); }
    void close() { socket_.close(); }
    bool is_open() const { return socket_.state() == QAbstractSocket::ConnectedState; }
    void send_or_queue(const QString& text) {
        if (is_open()) socket_.sendTextMessage(text);
        else pending_subs_.append(text);
    }

  private:
    HyperliquidClient* owner_;
    QWebSocket socket_;
    QStringList pending_subs_;
};

HyperliquidClient::HyperliquidClient(QObject* parent) : QObject(parent) {}
HyperliquidClient::~HyperliquidClient() { disconnect_ws(); }

void HyperliquidClient::set_testnet(bool testnet) { testnet_ = testnet; }

QString HyperliquidClient::rest_base() const { return testnet_ ? kTestnetRest : kMainnetRest; }
QString HyperliquidClient::ws_base() const { return testnet_ ? kTestnetWs : kMainnetWs; }

void HyperliquidClient::post_json(const QString& url, const QJsonObject& body, JsonCallback cb) {
    static QNetworkAccessManager s_nam;
    QNetworkRequest req(QUrl{url});
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QPointer<HyperliquidClient> self(this);
    auto* reply = s_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        const auto data = reply->readAll();
        const auto err = reply->error();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            cb(Result<QJsonDocument>::err(reply->errorString().toStdString()));
            return;
        }
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) {
            cb(Result<QJsonDocument>::err(pe.errorString().toStdString()));
            return;
        }
        cb(Result<QJsonDocument>::ok(doc));
    });
}

void HyperliquidClient::info(const QJsonObject& body, JsonCallback cb) {
    post_json(rest_base() + QStringLiteral("/info"), body, std::move(cb));
}

void HyperliquidClient::exchange(const QJsonObject& signed_envelope, JsonCallback cb) {
    post_json(rest_base() + QStringLiteral("/exchange"), signed_envelope, std::move(cb));
}

void HyperliquidClient::connect_ws() {
    if (!ws_) ws_ = new WsHolder(this);
    ws_->open(ws_base());
}

void HyperliquidClient::disconnect_ws() {
    if (ws_) ws_->close();
}

bool HyperliquidClient::ws_connected() const {
    return ws_ && ws_->is_open();
}

void HyperliquidClient::subscribe(const QJsonObject& subscription) {
    if (!ws_) connect_ws();
    QJsonObject env;
    env[QStringLiteral("method")] = QStringLiteral("subscribe");
    env[QStringLiteral("subscription")] = subscription;
    ws_->send_or_queue(QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Compact)));
}

void HyperliquidClient::unsubscribe(const QJsonObject& subscription) {
    if (!ws_) return;
    QJsonObject env;
    env[QStringLiteral("method")] = QStringLiteral("unsubscribe");
    env[QStringLiteral("subscription")] = subscription;
    ws_->send_or_queue(QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Compact)));
}

} // namespace fincept::trading::hyperliquid
