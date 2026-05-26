// src/screens/equity_research/EquityTechnicalsTab.h
#pragma once
#include "services/equity/EquityResearchModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QWidget>

namespace fincept::screens {

class EquityTechnicalsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityTechnicalsTab(QWidget* parent = nullptr);
    void set_symbol(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_technicals_loaded(services::equity::TechnicalsData data);

  private:
    void build_ui();
    void retranslateUi();
    void populate(const services::equity::TechnicalsData& data);
    void clear_sections();

    QString signal_text(services::equity::TechSignal s) const;
    static const char* signal_color(services::equity::TechSignal s);
    QString interpretation(const QString& col_key, double value) const;

    QString current_symbol_;
    services::equity::TechnicalsData cached_data_;
    bool data_loaded_ = false;

    // Static chrome captions (cached for retranslate)
    QLabel* rating_title_ = nullptr;
    QLabel* key_title_ = nullptr;
    QLabel* str_buy_caption_ = nullptr;
    QLabel* buy_caption_ = nullptr;
    QLabel* neutral_caption_ = nullptr;
    QLabel* sell_caption_ = nullptr;
    QLabel* str_sell_caption_ = nullptr;

    // Rating panel
    QLabel* rating_label_ = nullptr;
    QProgressBar* gauge_bar_ = nullptr;
    QLabel* strong_buy_count_ = nullptr;
    QLabel* buy_count_ = nullptr;
    QLabel* neutral_count_ = nullptr;
    QLabel* sell_count_ = nullptr;
    QLabel* strong_sell_count_ = nullptr;
    QLabel* total_label_ = nullptr;

    // Key indicators container
    QWidget* key_container_ = nullptr;

    // Category sections container
    QWidget* sections_container_ = nullptr;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
