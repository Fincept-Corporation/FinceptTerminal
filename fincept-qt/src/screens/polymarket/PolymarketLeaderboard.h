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
    /// Render a single full-width row carrying `message`. Used when no
    /// leaderboard data is available so the panel never shows a bare grid.
    void show_empty_state(const QString& message);

    QLabel* header_ = nullptr;
    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens::polymarket
