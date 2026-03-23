#pragma once
// Time & Sales — custom-painted live trade tape

#include "screens/crypto_trading/CryptoTypes.h"

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

    void add_trade(const TradeEntry& trade);
    void set_trades(const QVector<TradeEntry>& trades);

  protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void rebuild_cache();

    QVector<TradeEntry> trades_;
    QPixmap cache_;
    QMutex mutex_;
    bool cache_dirty_ = true;
    QTimer* repaint_timer_ = nullptr;

    static constexpr int ROW_H = 18;
};

} // namespace fincept::screens::crypto
