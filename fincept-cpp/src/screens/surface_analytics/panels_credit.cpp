#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace fincept::surface {

using namespace theme::colors;

void SurfaceScreen::render_metrics_credit() {
    float w = ImGui::GetContentRegionAvail().x;
    float metric_label_w = w * 0.52f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto row = [&](const char* lbl, const char* val, ImVec4 col) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(p.x + 4.0f, p.y + ImGui::GetTextLineHeight() * 0.5f + 1.0f),
                            3.0f, ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.7f)));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 12.0f);
        ImGui::TextColored(TEXT_DIM, "%s", lbl);
        ImGui::SameLine(metric_label_w);
        ImGui::TextColored(col, "%s", val);
    };
    char b[64];

    switch (active_chart_) {
    case ChartType::CDSSpread: {
        if (cds_data_.z.empty()) break;
        row("TYPE", "CDS Spread Curve", TEXT_PRIMARY);
        // Tightest and widest 5Y
        int y5_idx = 3; // ~36M
        float tightest = 9999, widest = 0;
        for (int i = 0; i < (int)cds_data_.entities.size(); i++) {
            float s = cds_data_.z[i][y5_idx];
            if (s < tightest) tightest = s;
            if (s > widest) widest = s;
        }
        std::snprintf(b, 64, "%.0f bps", tightest); row("TIGHTEST 5Y", b, MARKET_GREEN);
        std::snprintf(b, 64, "%.0f bps", widest); row("WIDEST 5Y", b, MARKET_RED);
        std::snprintf(b, 64, "%d", (int)cds_data_.entities.size()); row("ENTITIES", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::CreditTransition: {
        if (credit_trans_data_.z.empty()) break;
        row("TYPE", "1Y Transition Matrix", TEXT_PRIMARY);
        // Average stay probability
        float avg_stay = 0;
        int n = (int)credit_trans_data_.ratings.size();
        for (int i = 0; i < n; i++) avg_stay += credit_trans_data_.z[i][i];
        avg_stay /= (float)n;
        std::snprintf(b, 64, "%.1f%%", avg_stay); row("AVG STAY", b, MARKET_GREEN);
        std::snprintf(b, 64, "%dx%d", n, n); row("MATRIX", b, TEXT_SECONDARY);
        break;
    }
    case ChartType::RecoveryRate: {
        if (recovery_data_.z.empty()) break;
        row("TYPE", "Recovery by Seniority", TEXT_PRIMARY);
        // Average senior secured recovery
        float avg_sr = 0;
        for (int i = 0; i < (int)recovery_data_.sectors.size(); i++) avg_sr += recovery_data_.z[i][0];
        avg_sr /= (float)recovery_data_.sectors.size();
        std::snprintf(b, 64, "%.1f%%", avg_sr); row("SR SEC AVG", b, MARKET_GREEN);
        std::snprintf(b, 64, "%d", (int)recovery_data_.sectors.size()); row("SECTORS", b, TEXT_SECONDARY);
        break;
    }
    default: break;
    }
}

void SurfaceScreen::render_chart_credit() {
    char lb[32];
    switch (active_chart_) {
    case ChartType::CDSSpread: {
        if (cds_data_.z.empty()) break;
        float mn = 999, mx = 0;
        for (const auto& r : cds_data_.z) for (float v : r) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<std::string> tenor_labels;
        for (int t : cds_data_.tenors) { std::snprintf(lb, 32, "%dM", t); tenor_labels.push_back(lb); }
        render_3d_surface(cds_data_.z, "TENOR", "SPREAD (bps)", "ENTITY", mn, mx, false, &tenor_labels, &cds_data_.entities);
        break;
    }
    case ChartType::CreditTransition: {
        if (credit_trans_data_.z.empty()) break;
        render_3d_surface(credit_trans_data_.z, "TO RATING", "PROB %", "FROM RATING",
                          0, 100, false, &credit_trans_data_.to_ratings, &credit_trans_data_.ratings);
        break;
    }
    case ChartType::RecoveryRate: {
        if (recovery_data_.z.empty()) break;
        render_3d_surface(recovery_data_.z, "SENIORITY", "RECOVERY %", "SECTOR",
                          0, 80, false, &recovery_data_.seniorities, &recovery_data_.sectors);
        break;
    }
    default: break;
    }
}

} // namespace fincept::surface
