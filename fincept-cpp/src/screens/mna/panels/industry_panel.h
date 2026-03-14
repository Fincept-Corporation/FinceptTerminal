#pragma once
#include "../mna_types.h"
#include "../mna_constants.h"
#include "../mna_data.h"

namespace fincept::mna {

class IndustryPanel {
public:
    void render(MNAData& data);

private:
    int industry_tab_ = 0;  // 0=Tech, 1=Healthcare, 2=Financial
    int tech_sector_ = 0;   // 0=saas, 1=marketplace, 2=semiconductor
    int hc_sector_ = 0;     // 0=pharma, 1=biotech, 2=devices, 3=services
    int fin_sector_ = 0;    // 0=banking, 1=insurance, 2=asset_management
    bool inputs_collapsed_ = false;

    // Dynamic inputs stored as key-value
    double tech_inputs_[8] = {};
    double hc_inputs_[8] = {};
    double fin_inputs_[8] = {};

    nlohmann::json result_;
    std::string status_;
    double status_time_ = 0;

    void render_tech(MNAData& data);
    void render_healthcare(MNAData& data);
    void render_financial(MNAData& data);
};

} // namespace fincept::mna
