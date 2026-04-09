// src/screens/portfolio/PortfolioTxnPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Compact transaction history panel — shows the last N transactions for the
/// currently selected portfolio, with live reload on asset changes.
class PortfolioTxnPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioTxnPanel(QWidget* parent = nullptr);

    /// Populate with a fresh batch of transactions.
    void set_transactions(const QVector<portfolio::Transaction>& txns);

    /// Clear the table (e.g. when portfolio changes).
    void clear();
    void refresh_theme();

  private:
    void build_ui();
    void populate();

    QLabel*       count_label_ = nullptr;
    QTableWidget* table_       = nullptr;

    QVector<portfolio::Transaction> txns_;
};

} // namespace fincept::screens
