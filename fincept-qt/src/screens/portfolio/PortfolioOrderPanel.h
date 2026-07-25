// src/screens/portfolio/PortfolioOrderPanel.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <optional>

namespace fincept::screens {

/// 200px right sidebar for BUY/SELL order entry.
class PortfolioOrderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioOrderPanel(QWidget* parent = nullptr);

    void set_holding(const portfolio::HoldingWithQuote* holding);
    void set_currency(const QString& currency);
    void set_side(const QString& side); // "BUY" or "SELL"

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void buy_submitted();
    void sell_submitted();
    void close_requested();

  private:
    void build_ui();
    void update_display();
    void apply_side_styles();
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QLabel* price_prefix_ = nullptr;
    QLabel* qty_prefix_ = nullptr;
    QLabel* mv_prefix_ = nullptr;
    QLabel* note_label_ = nullptr;

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
    /// Value copy, deliberately NOT the caller's pointer. set_holding() used to
    /// store the raw `HoldingWithQuote*` returned by PortfolioScreen::
    /// find_holding(), which points into PortfolioSummary::holdings — that
    /// QVector is reassigned wholesale on every summary refresh, so the stored
    /// pointer dangled whenever a refresh did not immediately re-select
    /// (e.g. the portfolio went to zero positions).
    std::optional<portfolio::HoldingWithQuote> holding_;
};

} // namespace fincept::screens
