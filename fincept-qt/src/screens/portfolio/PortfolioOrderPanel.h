// src/screens/portfolio/PortfolioOrderPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

/// 200px right sidebar for BUY/SELL order entry.
class PortfolioOrderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioOrderPanel(QWidget* parent = nullptr);

    void set_holding(const portfolio::HoldingWithQuote* holding);
    void set_currency(const QString& currency);
    void set_side(const QString& side); // "BUY" or "SELL"

  signals:
    void buy_submitted();
    void sell_submitted();
    void close_requested();

  private:
    void build_ui();
    void update_display();

    QPushButton* buy_tab_ = nullptr;
    QPushButton* sell_tab_ = nullptr;
    QPushButton* submit_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;

    QLabel* symbol_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* qty_label_ = nullptr;
    QLabel* mv_label_ = nullptr;

    QString side_ = "BUY";
    QString currency_ = "USD";
    const portfolio::HoldingWithQuote* holding_ = nullptr;
};

} // namespace fincept::screens
