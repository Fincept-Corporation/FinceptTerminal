#include "demo_data.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace fincept::surface {



CDSSpreadData generate_cds_spread() {
    CDSSpreadData data;
    data.entities = {"Apple", "JPMorgan", "Ford", "Tesla", "AT&T",
                     "Boeing", "US Steel", "AMC Ent"};
    data.tenors   = {6, 12, 24, 36, 60, 84, 120};

    // Base CDS spread by entity (bps)
    const float base[] = {25, 40, 80, 120, 90, 150, 350, 600};

    for (int ei = 0; ei < (int)data.entities.size(); ei++) {
        std::vector<float> row;
        for (int tenor : data.tenors) {
            float term = base[ei] * 0.12f * std::log((float)tenor / 6.0f + 1.0f);
            float spread = base[ei] + term + (demo_randf() - 0.5f) * base[ei] * 0.08f;
            row.push_back(std::max(5.0f, spread));
        }
        data.z.push_back(row);
    }
    return data;
}

CreditTransitionData generate_credit_transition() {
    CreditTransitionData data;
    data.ratings    = {"AAA", "AA", "A", "BBB", "BB", "B", "CCC", "D"};
    data.to_ratings = {"AAA", "AA", "A", "BBB", "BB", "B", "CCC", "D"};

    // S&P-style 1-year transition matrix (approximate)
    const float matrix[8][8] = {
        {90.0f, 7.5f,  1.5f,  0.5f,  0.3f,  0.1f,  0.05f, 0.05f}, // AAA
        {0.5f,  88.0f, 8.5f,  2.0f,  0.5f,  0.3f,  0.1f,  0.1f},  // AA
        {0.05f, 2.0f,  87.5f, 8.0f,  1.5f,  0.5f,  0.3f,  0.15f}, // A
        {0.02f, 0.2f,  3.5f,  85.0f, 7.5f,  2.5f,  0.8f,  0.48f}, // BBB
        {0.01f, 0.05f, 0.3f,  4.0f,  78.0f, 10.0f, 5.0f,  2.64f}, // BB
        {0.0f,  0.05f, 0.1f,  0.5f,  5.0f,  75.0f, 12.0f, 7.35f}, // B
        {0.0f,  0.0f,  0.05f, 0.2f,  1.0f,  5.0f,  60.0f, 33.75f},// CCC
        {0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  100.0f},// D
    };

    for (int i = 0; i < 8; i++) {
        std::vector<float> row;
        for (int j = 0; j < 8; j++) {
            float val = matrix[i][j];
            if (i != j && i != 7) val += (demo_randf() - 0.5f) * val * 0.05f;
            row.push_back(std::max(0.0f, val));
        }
        data.z.push_back(row);
    }
    return data;
}

RecoveryRateData generate_recovery_rate() {
    RecoveryRateData data;
    data.sectors      = {"Technology", "Healthcare", "Energy", "Financials",
                         "Industrials", "Utilities", "Telecom", "Real Estate"};
    data.seniorities  = {"Sr Secured", "Sr Unsecured", "Subordinated", "Junior"};

    // Base recovery rates by seniority
    const float base_recovery[] = {55.0f, 42.0f, 30.0f, 18.0f};
    // Sector adjustment
    const float sector_adj[] = {5.0f, 8.0f, -5.0f, 3.0f, 0.0f, 10.0f, 2.0f, -3.0f};

    for (int si = 0; si < (int)data.sectors.size(); si++) {
        std::vector<float> row;
        for (int ri = 0; ri < (int)data.seniorities.size(); ri++) {
            float rec = base_recovery[ri] + sector_adj[si];
            rec += (demo_randf() - 0.5f) * 5.0f;
            row.push_back(std::max(5.0f, std::min(85.0f, rec)));
        }
        data.z.push_back(row);
    }
    return data;
}

} // namespace fincept::surface
