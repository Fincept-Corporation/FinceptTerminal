// src/screens/portfolio/PortfolioFFNView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QChartView>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// FFN (Full Financial Networks) analytics view with 6 sub-tabs.
/// Uses Python ffn library via PythonRunner for calculations.
class PortfolioFFNView : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioFFNView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  signals:
    void back_requested();

  private:
    void build_ui();
    void update_overview();
    void run_ffn();

    QTabWidget* tabs_ = nullptr;

    // Overview tab
    QTableWidget* overview_table_ = nullptr;

    // Benchmark tab
    QWidget* benchmark_panel_ = nullptr;

    // Other tabs
    QWidget* optimization_panel_ = nullptr;
    QWidget* rebased_panel_ = nullptr;
    QWidget* drawdowns_panel_ = nullptr;
    QWidget* rolling_panel_ = nullptr;

    // Header widgets
    QPushButton* back_btn_ = nullptr;
    QPushButton* run_btn_  = nullptr;
    QLabel*      status_label_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
    QJsonObject ffn_data_;
};

} // namespace fincept::screens
