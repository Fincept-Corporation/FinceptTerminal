#pragma once
// Time & Sales — custom-painted live trade tape

#include "screens/crypto_trading/CryptoTypes.h"

#include <QEvent>
#include <QMutex>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace fincept::screens::crypto {

class CryptoTimeSales : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoTimeSales(QWidget* parent = nullptr);

    /// A live print, newest first. Dropped when the tape already holds it (a
    /// print seeded by merge_history() can arrive again on the stream).
    void add_trade(const TradeEntry& trade);
    void set_trades(const QVector<TradeEntry>& trades);
    /// Fill the tape with recent prints fetched over REST (any order) without
    /// losing or duplicating the live prints that arrived meanwhile; the result
    /// is sorted newest first.
    void merge_history(const QVector<TradeEntry>& history);
    /// The market the prints come from: its quote asset is the PRICE column's unit.
    void set_price_market(const QString& market);
    /// What the empty tape says instead of "Waiting for trades…" (e.g. why the
    /// live stream is down); empty restores the waiting text.
    void set_status_text(const QString& text);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();

    QVector<TradeEntry> trades_;
    QString price_market_; // empty = unit not known yet
    QString status_text_;  // empty-tape message override ("" = waiting)
    QPixmap cache_;
    QMutex mutex_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;

    static constexpr int ROW_H = 18; // minimum; rows grow with the user's font size
};

} // namespace fincept::screens::crypto
