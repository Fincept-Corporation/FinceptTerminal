#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



InflationExpData generate_inflation_expectations() {
    InflationExpData data;
    data.horizons    = {1, 2, 3, 5, 7, 10, 20, 30};
    data.time_points = {};
    for (int i = 0; i < 60; i++) data.time_points.push_back(i);

    // US breakeven rates — humped at 5Y
    const float base_be[] = {2.8f, 2.6f, 2.5f, 2.4f, 2.35f, 2.3f, 2.25f, 2.2f};

    for (int day : data.time_points) {
        std::vector<float> row;
        float trend = ((float)day - 30.0f) / 200.0f;
        float cycle = 0.1f * std::sin((float)day / 10.0f);
        for (int hi = 0; hi < (int)data.horizons.size(); hi++) {
            float be = base_be[hi] + trend + cycle;
            be += (demo_randf() - 0.5f) * 0.05f;
            row.push_back(std::max(0.5f, be));
        }
        data.z.push_back(row);
    }
    return data;
}

MonetaryPolicyData generate_monetary_policy() {
    MonetaryPolicyData data;
    data.central_banks  = {"Fed", "ECB", "BoJ", "BoE", "PBoC", "RBA", "BoC", "SNB"};
    data.meetings_ahead = {1, 2, 3, 4, 5, 6, 7, 8};

    // Current policy rates and expected paths
    const float current_rates[] = {4.75f, 3.75f, 0.25f, 4.50f, 3.45f, 4.35f, 4.25f, 1.50f};
    // Expected per-meeting change (bps)
    const float path_slope[]    = {-10.0f, -8.0f, 2.0f, -12.0f, -3.0f, -5.0f, -8.0f, -5.0f};

    for (int bi = 0; bi < (int)data.central_banks.size(); bi++) {
        std::vector<float> row;
        for (int m : data.meetings_ahead) {
            float rate = current_rates[bi] + path_slope[bi] * (float)m / 100.0f;
            rate += (demo_randf() - 0.5f) * 0.08f;
            row.push_back(std::max(0.0f, rate));
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
