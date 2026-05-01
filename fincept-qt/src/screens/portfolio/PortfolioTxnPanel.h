// src/screens/portfolio/PortfolioTxnPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
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

  signals:
    /// Emitted when the user toggles the collapse chevron in the header.
    /// `collapsed=true` → caller should shrink the panel to header height;
    /// `collapsed=false` → restore to its full size.
    void collapse_toggled(bool collapsed);

  private:
    void build_ui();
    void populate();
    void apply_collapsed_state();

    QLabel* count_label_ = nullptr;
    QPushButton* collapse_btn_ = nullptr;
    QTableWidget* table_ = nullptr;
    bool collapsed_ = false;

    QVector<portfolio::Transaction> txns_;
};

} // namespace fincept::screens
