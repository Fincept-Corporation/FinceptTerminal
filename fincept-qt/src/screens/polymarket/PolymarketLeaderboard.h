#pragma once

#include "services/polymarket/PolymarketTypes.h"

#include <QEvent>
#include <QLabel>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::polymarket {

/// Leaderboard panel showing top traders.
class PolymarketLeaderboard : public QWidget {
    Q_OBJECT
  public:
    explicit PolymarketLeaderboard(QWidget* parent = nullptr);

    void set_entries(const QVector<services::polymarket::LeaderboardEntry>& entries);
    void set_loading(bool loading);

  signals:
    void trader_clicked(const QString& address);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void retranslateUi();

    QLabel* header_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens::polymarket
