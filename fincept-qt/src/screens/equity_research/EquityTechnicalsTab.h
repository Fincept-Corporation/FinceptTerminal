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

  private slots:
    void on_technicals_loaded(services::equity::TechnicalsData data);

  private:
    void build_ui();
    void populate(const services::equity::TechnicalsData& data);
    void clear_sections();

    static QString signal_text(services::equity::TechSignal s);
    static const char* signal_color(services::equity::TechSignal s);
    static QString interpretation(const QString& col_key, double value);

    QString current_symbol_;

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
