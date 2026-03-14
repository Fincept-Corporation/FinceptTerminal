#pragma once
// Government Data (GOVT) — main screen with provider sidebar and content dispatch

#include "gov_data_types.h"
#include "gov_data.h"
#include "providers/us_treasury.h"
#include "providers/us_congress.h"
#include "providers/canada_gov.h"
#include "providers/openafrica.h"
#include "providers/spain_data.h"
#include "providers/pxweb.h"
#include "providers/swiss_gov.h"
#include "providers/french_gov.h"
#include "providers/universal_ckan.h"
#include "providers/data_gov_hk.h"

namespace fincept::gov_data {

class GovDataScreen {
public:
    void render();

private:
    ProviderId active_provider_ = ProviderId::USTreasury;

    // Provider panel instances
    USTreasuryProvider us_treasury_;
    USCongressProvider us_congress_;
    CanadaGovProvider canada_gov_;
    OpenAfricaProvider openafrica_;
    SpainDataProvider spain_data_;
    PxWebProvider pxweb_;
    SwissGovProvider swiss_gov_;
    FrenchGovProvider french_gov_;
    UniversalCkanProvider universal_ckan_;
    DataGovHkProvider data_gov_hk_;

    void render_sidebar();
    void render_content();
    void render_status_bar();
};

} // namespace fincept::gov_data
