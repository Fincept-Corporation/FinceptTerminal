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

    // Signal helper for threshold-based logic matching reference
    static QString compute_signal(const services::equity::TechIndicator& ti);
    static QString signal_label(services::equity::TechSignal s);
    static QString signal_color(services::equity::TechSignal s);
    static QString signal_bg(services::equity::TechSignal s);

    QString current_symbol_;

    // TECHNICAL RATING panel
    QLabel* rating_label_ = nullptr;    // "STRONG BUY" etc
    QProgressBar* gauge_bar_ = nullptr; // sell|neutral|buy gauge
    QLabel* buy_count_ = nullptr;
    QLabel* neutral_count_ = nullptr;
    QLabel* sell_count_ = nullptr;
    QLabel* total_label_ = nullptr;

    // KEY INDICATORS panel — 8 fixed mini cards
    // Stored as pairs (name_lbl, value_lbl, signal_lbl) — built dynamically
    QWidget* key_grid_ = nullptr;

    // Scrollable section container
    QWidget* sections_container_ = nullptr;

    ui::LoadingOverlay* loading_overlay_ = nullptr;
};

} // namespace fincept::screens
