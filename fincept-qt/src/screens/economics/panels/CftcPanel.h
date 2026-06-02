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
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void show_sentiment(const QJsonObject& sentiment);
    void build_sentiment_widget();
    void retranslateUi() override;

    QComboBox* market_combo_ = nullptr;
    QComboBox* report_type_combo_ = nullptr;
    QComboBox* view_combo_ = nullptr;

    // Sentiment view widgets
    QWidget* sentiment_widget_ = nullptr;
    QLabel* sent_market_lbl_ = nullptr;
    QLabel* sent_date_lbl_ = nullptr;
    QLabel* sent_oi_lbl_ = nullptr;
    QLabel* sent_comm_bias_ = nullptr;
    QLabel* sent_noncomm_bias_ = nullptr;
    QLabel* sent_oi_trend_ = nullptr;
    QLabel* sent_comm_net_ = nullptr;
    QLabel* sent_noncomm_net_ = nullptr;

    QStackedWidget* content_stack_ = nullptr; // 0=base table, 1=sentiment

    // Cached for retranslateUi — toolbar labels
    QLabel* market_lbl_ = nullptr;
    QLabel* report_lbl_ = nullptr;
    QLabel* view_lbl_ = nullptr;

    // Cached for retranslateUi — sentiment widget static labels
    QLabel* sent_title_lbl_ = nullptr;
    QLabel* sent_comm_card_lbl_ = nullptr;
    QLabel* sent_comm_card_desc_ = nullptr;
    QLabel* sent_noncomm_card_lbl_ = nullptr;
    QLabel* sent_noncomm_card_desc_ = nullptr;
    QLabel* sent_oi_caption_ = nullptr;
    QLabel* sent_oi_trend_caption_ = nullptr;
};

} // namespace fincept::screens
