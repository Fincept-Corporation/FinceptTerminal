// src/screens/economics/panels/EiaPanel.h
// EIA (U.S. Energy Information Administration)
// WPSR (Weekly Petroleum Status Report): no API key — downloads public XLS files. Opens on the
// latest 52 report weeks (eia_data.py latest_weeks); FULL HISTORY loads the five-year window.
// STEO (Short-Term Energy Outlook): requires EIA_API_KEY env var.
// Script: eia_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QStackedWidget>

#include <memory>

namespace fincept::screens {

class EiaPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit EiaPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    QList<ui::Requirement> load_requirements() const override;
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    /// The rows of a result split per series (symbol), built on a worker: a category holds dozens of
    /// series, ~23,500 rows over five years.
    struct SeriesSplit {
        QStringList symbols;              ///< in order of first appearance
        QHash<QString, QString> titles;   ///< symbol -> title
        QHash<QString, QString> units;    ///< symbol -> units
        QHash<QString, QJsonArray> rows;  ///< symbol -> its rows
        QJsonArray unkeyed;               ///< all rows when none carries a symbol
    };
    static std::shared_ptr<const SeriesSplit> split_rows(const QJsonArray& rows, bool steo, const QString& this_month,
                                                         const QString& forecast_text, const QString& history_text);

    void on_source_changed(int index);
    void retranslateUi() override;
    /// Show the rows of the series picked in series_combo_ (one series at a time: a category holds
    /// dozens of unrelated series, which in one table had no meaningful stat cards).
    void show_selected_series();

    QComboBox* source_combo_ = nullptr;   // WPSR | STEO
    QComboBox* category_combo_ = nullptr; // WPSR categories or STEO tables
    QComboBox* series_combo_ = nullptr;   // series of the last result (symbol as data, title as text)
    QCheckBox* full_history_ = nullptr;   // WPSR: five years instead of the latest 52 weeks
    QLabel* apikey_notice_ = nullptr;

    // Last result, split per series (show_selected_series picks one).
    std::shared_ptr<const SeriesSplit> split_;
    QString result_label_;
    bool result_is_steo_ = false;
    bool result_is_full_ = false;

    // Cached for retranslateUi
    QLabel* source_lbl_ = nullptr;
    QLabel* category_lbl_ = nullptr;
    QLabel* series_lbl_ = nullptr;
};

} // namespace fincept::screens
