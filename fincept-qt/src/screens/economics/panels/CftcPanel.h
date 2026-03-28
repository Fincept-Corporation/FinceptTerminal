// src/screens/economics/panels/CftcPanel.h
// CFTC (Commodity Futures Trading Commission) — Commitments of Traders reports.
// No API key required. Covers 40+ futures markets: commodities, FX, indices, crypto.
// Script: cftc_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>

namespace fincept::screens {

class CftcPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit CftcPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id,
                   const services::EconomicsResult& result) override;

  private:
    void show_sentiment(const QJsonObject& sentiment);
    void build_sentiment_widget();

    QComboBox* market_combo_      = nullptr;
    QComboBox* report_type_combo_ = nullptr;
    QComboBox* view_combo_        = nullptr;

    // Sentiment view widgets
    QWidget*   sentiment_widget_  = nullptr;
    QLabel*    sent_market_lbl_   = nullptr;
    QLabel*    sent_date_lbl_     = nullptr;
    QLabel*    sent_oi_lbl_       = nullptr;
    QLabel*    sent_comm_bias_    = nullptr;
    QLabel*    sent_noncomm_bias_ = nullptr;
    QLabel*    sent_oi_trend_     = nullptr;
    QLabel*    sent_comm_net_     = nullptr;
    QLabel*    sent_noncomm_net_  = nullptr;

    QStackedWidget* content_stack_ = nullptr;  // 0=base table, 1=sentiment
};

} // namespace fincept::screens
