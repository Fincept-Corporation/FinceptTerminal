// src/screens/equity_research/EquityValuationTab.h
#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QDoubleSpinBox>
#include <QHash>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

#include <cmath>

class QFrame;
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;

namespace fincept::screens {

class EquityValuationTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityValuationTab(QWidget* parent = nullptr);

    // Called by EquityResearchScreen when symbol changes
    void set_symbol(const QString& symbol);

    // Called by EquityResearchScreen::on_info_loaded()
    void set_stock_info(const services::equity::StockInfo& info);

    // Called by EquityResearchScreen::on_financials_loaded()
    void set_financials(const services::equity::FinancialsData& data);

  protected:
    void changeEvent(QEvent* e) override;

  private slots:
    void on_calculate_clicked();

  private:
    // ── UI builders ──────────────────────────────────────────────
    void build_ui();
    void build_dcf_section(QVBoxLayout* parent);
    void build_scoring_section(QVBoxLayout* parent);
    void build_multiples_section(QVBoxLayout* parent);

    // ── Data helpers ─────────────────────────────────────────────
    void pre_fill_dcf_inputs();
    void clear_results();
    void run_scoring_models();
    void run_multiples();

    // ── Result display ───────────────────────────────────────────
    void display_dcf_result(const QJsonValue& data);
    void display_altman_result(const QJsonValue& data);
    void display_piotroski_result(const QJsonValue& data);
    void display_beneish_result(const QJsonValue& data);
    void display_multiples_result(const QJsonValue& data);

    // ── Formatting helpers ────────────────────────────────────────
    void set_badge(QLabel* badge, const QString& text, const QString& hex_color);
    QString fmt_currency(double value) const;
    QString fmt_num(double value, int decimals = 2) const;
    double get_stmt_val(const QVector<QPair<QString, QJsonObject>>& stmt, const QString& key, int period_idx = 0) const;

    // ── DCF inputs ───────────────────────────────────────────────
    QDoubleSpinBox* fcf_input_ = nullptr;      // Free cash flow (millions)
    QDoubleSpinBox* growth_input_ = nullptr;   // Near-term growth %
    QDoubleSpinBox* terminal_input_ = nullptr; // Terminal growth %
    QDoubleSpinBox* discount_input_ = nullptr; // Discount rate / WACC %
    QDoubleSpinBox* years_input_ = nullptr;    // Projection years
    QDoubleSpinBox* shares_input_ = nullptr;   // Shares outstanding (millions)
    QPushButton* calc_btn_ = nullptr;

    // ── DCF result labels ─────────────────────────────────────────
    QLabel* intrinsic_val_ = nullptr;
    QLabel* current_price_val_ = nullptr;
    QLabel* margin_val_ = nullptr;
    QLabel* verdict_badge_ = nullptr;

    // ── Scoring labels ────────────────────────────────────────────
    QLabel* altman_val_ = nullptr;
    QLabel* altman_badge_ = nullptr;
    QLabel* piotroski_val_ = nullptr;
    QLabel* piotroski_badge_ = nullptr;
    QLabel* beneish_val_ = nullptr;
    QLabel* beneish_badge_ = nullptr;
    QLabel* scoring_note_ = nullptr; // "Load financials first" hint

    // ── Multiples labels ──────────────────────────────────────────
    QLabel* ev_ebitda_val_ = nullptr;
    QLabel* price_sales_val_ = nullptr;
    QLabel* ev_revenue_val_ = nullptr;
    QLabel* graham_val_ = nullptr;

    // ── State ─────────────────────────────────────────────────────
    QString current_symbol_;
    double current_price_ = 0.0;
    QString current_currency_ = QStringLiteral("USD");
    services::equity::StockInfo last_info_;
    services::equity::FinancialsData last_financials_;
    bool financials_loaded_ = false;

    // i18n: label → translatable source string
    QHash<QLabel*, const char*> i18n_labels_;
};

} // namespace fincept::screens
