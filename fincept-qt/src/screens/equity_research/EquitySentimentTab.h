#pragma once

#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class EquitySentimentTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquitySentimentTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  private slots:
    void on_snapshot_loaded(QString symbol, fincept::services::equity::MarketSentimentSnapshot snapshot);

  private:
    void build_ui();
    void populate(const fincept::services::equity::MarketSentimentSnapshot& snapshot);
    void clear_sources();
    QWidget* build_summary_panel();

    QString current_symbol_;
    QWidget* content_widget_ = nullptr;
    QWidget* sources_widget_ = nullptr;
    QGridLayout* sources_layout_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* company_label_ = nullptr;
    QLabel* coverage_label_ = nullptr;
    QLabel* avg_buzz_value_ = nullptr;
    QLabel* avg_bullish_value_ = nullptr;
    QLabel* coverage_value_ = nullptr;
    QLabel* alignment_value_ = nullptr;
    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
