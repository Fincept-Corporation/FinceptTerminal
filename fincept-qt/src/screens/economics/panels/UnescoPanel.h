// src/screens/economics/panels/UnescoPanel.h
// UNESCO UIS (Institute for Statistics) — Education, Science & Tech, Culture data.
// No API key required. 200+ countries, 100,000+ indicators.
// Best command: fetch <indicator> <country> → [{date, value}]
// Script: unesco_data.py
#pragma once

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>

namespace fincept::screens {

class UnescoPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit UnescoPanel(QWidget* parent = nullptr);
    void activate() override;

  protected:
    void build_controls(QHBoxLayout* thl) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;
    void changeEvent(QEvent* event) override;

  private:
    void load_indicators();
    /// Rebuild the indicator list from the live UIS catalogue (all_records_)
    /// for `theme_index`, restoring `keep_code` if still present.
    /// Returns false when the catalogue has no entry for that theme.
    bool populate_indicators_from_records(int theme_index, const QString& keep_code);
    void on_theme_changed(int index);
    void on_indicator_filter(const QString& text);
    void retranslateUi() override;

    QComboBox* theme_combo_ = nullptr;
    QLineEdit* indicator_search_ = nullptr;
    QListWidget* indicator_list_ = nullptr;
    QLineEdit* country_input_ = nullptr;
    QLineEdit* start_input_ = nullptr;
    QLineEdit* end_input_ = nullptr;

    // Cached for retranslateUi
    QLabel* theme_lbl_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* from_lbl_ = nullptr;
    QLabel* to_lbl_ = nullptr;

    // preset indicators per theme
    struct IndicatorDef {
        QString name;
        QString code;
    };
    QList<IndicatorDef> current_indicators_;
    /// Raw `list_indicators` catalogue records, cached for theme re-filtering.
    QJsonArray all_records_;
    bool indicators_loaded_ = false;
};

} // namespace fincept::screens
