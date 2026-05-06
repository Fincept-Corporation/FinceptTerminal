#pragma once
// TemplatePickerPanel — Sensibull-style ready-made strategy picker.
//
// Layout
// ──────
//   ┌──────────────────────────────────────────────────────┐
//   │  [Bullish] [Bearish] [Neutral] [Volatile] [Others]   │  category strip
//   ├──────────────────────────────────────────────────────┤
//   │  Long Call            — Strongly bullish, expect …   │  list
//   │  Short Put            — Mildly bullish, earns prem.  │
//   │  Bull Call Spread     — Capped upside, cheaper than… │
//   │  …                                                   │
//   ├──────────────────────────────────────────────────────┤
//   │  Width: [1] ▴▾   Shift: [0] ▴▾   Lots: [1] ▴▾  USE   │
//   └──────────────────────────────────────────────────────┘
//
// Width is disabled for templates whose `supports_width=false` (single-leg
// constructions like Long Call). Selecting a row updates the description
// and seeds Width to the template's `default_width`.

#include "services/options/StrategyTemplates.h"

#include <QListWidget>
#include <QPushButton>
#include <QString>
#include <QSpinBox>
#include <QVector>
#include <QWidget>

namespace fincept::screens::fno {

class TemplatePickerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit TemplatePickerPanel(QWidget* parent = nullptr);

  signals:
    /// Fired when the user clicks USE with a valid selection.
    void template_chosen(const QString& template_id,
                         const fincept::services::options::StrategyInstantiationOptions& options);

  private slots:
    void on_category_changed(int index);
    void on_template_selected();
    void on_use_clicked();

  private:
    void setup_ui();
    void rebuild_list_for_category(fincept::services::options::StrategyCategory cat);
    void apply_template_to_controls(const fincept::services::options::StrategyTemplate& tpl);

    fincept::services::options::StrategyCategory active_category_ =
        fincept::services::options::StrategyCategory::Bullish;

    QVector<QPushButton*> category_btns_;
    QListWidget* list_ = nullptr;
    QSpinBox* width_spin_ = nullptr;
    QSpinBox* shift_spin_ = nullptr;
    QSpinBox* lots_spin_ = nullptr;
    QPushButton* use_btn_ = nullptr;

    /// Current selection — null until the user clicks a row.
    const fincept::services::options::StrategyTemplate* selected_ = nullptr;
};

} // namespace fincept::screens::fno
