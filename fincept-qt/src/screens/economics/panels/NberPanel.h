#pragma once
// NBER Business Cycle Dating — US recession peaks/troughs and a monthly
// recession indicator, from the NBER Business Cycle Dating Committee.
//
// Script: nber_data.py | No API key required.
// The two exposed commands run entirely OFFLINE (the committee's dates are
// embedded in the script), which makes this one of the few economics panels
// that works with no network at all.

#include "screens/economics/panels/EconPanelBase.h"

#include <QComboBox>
#include <QLabel>

namespace fincept::screens {

class NberPanel : public EconPanelBase {
    Q_OBJECT
  public:
    explicit NberPanel(QWidget* parent = nullptr);

    void activate() override;

  protected:
    void build_controls(QHBoxLayout* toolbar_layout) override;
    void on_fetch() override;
    void on_result(const QString& request_id, const services::EconomicsResult& result) override;

    void changeEvent(QEvent* event) override;
    void retranslateUi() override;

  private:
    QLabel* dataset_lbl_ = nullptr;
    QComboBox* dataset_combo_ = nullptr;
};

} // namespace fincept::screens
