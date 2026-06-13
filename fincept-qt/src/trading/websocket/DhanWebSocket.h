#pragma once
// DhanWebSocket — Dhan market-data streaming adapter (BINARY protocol).
//
// Port of OpenAlgo's dhan_websocket.py + dhan_adapter.py to the Fincept
// BrokerWebSocketBase contract. Dhan pushes little-endian binary frames over a
// standard WebSocket (unlike the JSON-based 5Paisa / Noren feeds).
//
// Reference (authoritative for byte offsets):
//   broker/dhan/streaming/dhan_websocket.py   (struct.unpack format strings)
//   broker/dhan/streaming/dhan_adapter.py     (subscribe / normalize logic)
//   broker/dhan/streaming/dhan_mapping.py     (segment ↔ exchange mapping)
//
// ── Protocol summary ─────────────────────────────────────────────────────────
//
// Two endpoints exist:
//   5-depth  : wss://api-feed.dhan.co?version=2&token=TOKEN&clientId=CID&authType=2
//   20-depth : wss://depth-api-feed.dhan.co/twentydepth?token=TOKEN&clientId=CID&authType=2
// Only the 5-depth connection is implemented here; the 20-depth surface is
// stubbed (lazy-init left for a future change) since most consumers want the
// 5-level book that already arrives in the FULL packet.
//
// Subscribe is JSON over the binary socket:
//   {"RequestCode":15,"InstrumentCount":n,
//    "InstrumentList":[{"ExchangeSegment":"NSE_EQ","SecurityId":"11536"}]}
//   RequestCode: 15=TICKER, 17=QUOTE, 21=FULL, 23=20_DEPTH, 12=DISCONNECT.
//   We subscribe in FULL (21) mode so a single feed carries LTP + OHLC + depth.
//
// Responses are little-endian binary. 8-byte header per packet:
//   byte 0    : response_code (uint8)
//   bytes 1-2 : message_length (uint16 LE)  — total incl. the 8-byte header
//   byte 3    : exchange_segment (uint8)
//   bytes 4-7 : security_id (uint32 LE)
// Response codes: 2=TICKER, 4=QUOTE, 5=OI, 6=PREV_CLOSE, 8=FULL, 50=DISCONNECT,
//                 0=heartbeat (ignored). Multiple packets may share one frame.

#include "trading/websocket/BrokerWebSocketBase.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept {

class WebSocketClient;

namespace trading {

class DhanWebSocket : public BrokerWebSocketBase {
    Q_OBJECT
  public:
    /// A single instrument to subscribe to. ExchangeSegment is the Dhan string
    /// form ("NSE_EQ", "NSE_FNO", "BSE_EQ", "MCX_COMM", ...); security_id is the
    /// numeric Dhan security id (sent as a string in the subscribe JSON).
    struct Instrument {
        QString exchange_segment; // "NSE_EQ", "NSE_FNO", ...
        QString security_id;      // numeric security id as string
    };

    /// @param access_token  Dhan OAuth access token (query "token=")
    /// @param client_id     Dhan client id (query "clientId=")
    explicit DhanWebSocket(const QString& access_token, const QString& client_id, QObject* parent = nullptr);

    void open() override;
    void close() override;
    bool is_connected() const override;

    /// Symbol/instrument-based subscribe (Dhan native form: segment + securityId).
    /// Subscribes in FULL mode so consumers get quote + 5-level depth. Dedupes.
    void subscribe(const QVector<Instrument>& instruments);

    /// Token-based subscribe (BrokerWebSocketBase contract). Security ids are
    /// resolved to their ExchangeSegment via InstrumentService::find_by_token;
    /// tokens with no mapping fall back to set_default_segment() (default NSE_EQ).
    void subscribe(const QVector<qint64>& tokens) override;

    /// Unsubscribe everything currently subscribed.
    void unsubscribe() override;

    /// Remove a specific set of instruments.
    void unsubscribe(const QVector<Instrument>& instruments);

    /// Default Dhan ExchangeSegment used when a token-based subscribe has no
    /// per-token mapping. Defaults to "NSE_EQ".
    void set_default_segment(const QString& segment) { default_segment_ = segment; }

    // ── Segment ↔ exchange helpers (public for tests / consumers) ────────────
    // Numeric exchange_segment code → Fincept exchange string.
    // 0=IDX, 1=NSE, 2=NFO, 3=CDS, 4=BSE, 5=MCX, 7=BCD, 8=BFO. (dhan_mapping.py)
    static QString segment_code_to_exchange(int code);
    // Fincept exchange string → Dhan ExchangeSegment string ("NSE"→"NSE_EQ").
    static QString exchange_to_segment_string(const QString& exchange);

  protected:
    void flush_subscribe_queue() override;
    void on_data_stall() override;

  private slots:
    void on_connected();
    void on_binary_message(const QByteArray& data);
    void on_disconnected();

  private:
    // Wire senders
    void send_subscribe(const QVector<Instrument>& instruments, bool subscribe);
    void resubscribe_all();

    // Binary packet parsing. `payload` excludes the 8-byte header. Each returns
    // a freshly-built BrokerQuote (callers run it through merge_tick()).
    BrokerQuote parse_ticker(const uchar* payload, int len, int segment, quint32 security_id) const;
    BrokerQuote parse_quote(const uchar* payload, int len, int segment, quint32 security_id) const;
    BrokerQuote parse_full(const uchar* payload, int len, int segment, quint32 security_id,
                           MarketDepth& depth_out, bool& has_depth) const;
    void handle_disconnect(const uchar* payload, int len);

    // Enrich symbol/exchange for a security id via InstrumentService.
    QString symbol_for_security(quint32 security_id, const QString& fallback) const;

    // Stamp timestamp + derive day change on a merged quote, then emit tick_received.
    void emit_tick(BrokerQuote merged);

    // little-endian readers
    static quint16 read_u16(const uchar* p);
    static quint32 read_u32(const uchar* p);
    static float read_f32(const uchar* p);

    // unique dedupe key for an instrument
    static QString inst_key(const Instrument& i);

    QString access_token_;
    QString client_id_;
    QString default_segment_ = QStringLiteral("NSE_EQ");

    WebSocketClient* ws_ = nullptr;

    // All instruments we want active, keyed by "SEGMENT|SECID".
    QHash<QString, Instrument> subscribed_;

    // Dhan request codes (dhan_websocket.py REQUEST_CODES).
    static constexpr int kReqTicker = 15;
    static constexpr int kReqQuote = 17;
    static constexpr int kReqFull = 21;
    static constexpr int kReqDisconnect = 12;

    // We always subscribe in FULL mode (LTP + OHLC + 5-level depth in one feed).
    static constexpr int kReqMode = kReqFull;

    // Dhan limits: max 100 instruments per subscribe message.
    static constexpr int kBatchSize = 100;
};

} // namespace trading
} // namespace fincept
