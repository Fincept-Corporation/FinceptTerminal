#include "surface_screen.h"
#include <imgui.h>
#include <cstdio>

namespace fincept::surface {

using namespace theme::colors;

static const char* MAT_LABELS[] = {"1M","3M","6M","1Y","2Y","3Y","5Y","7Y","10Y","20Y","30Y"};

void SurfaceScreen::render_vol_table() {
    if (vol_data_.z.empty()) return;
    const int n_exp = (int)vol_data_.expirations.size();
    const int n_str = (int)vol_data_.strikes.size();

    ImGui::TextColored(ACCENT, "IV SURFACE — %s ($%.2f)", vol_data_.underlying.c_str(), vol_data_.spot_price);
    ImGui::Spacing();

    if (ImGui::BeginTable("##vol_tbl", n_str + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("DTE", ImGuiTableColumnFlags_WidthFixed, 50);
        for (int j = 0; j < n_str; j++) {
            char h[16]; std::snprintf(h, 16, "%.0f", vol_data_.strikes[j]);
            ImGui::TableSetupColumn(h, ImGuiTableColumnFlags_WidthFixed, 50);
        }
        ImGui::TableHeadersRow();

        for (int i = 0; i < n_exp; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%dD", vol_data_.expirations[i]);
            for (int j = 0; j < n_str; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                ImGui::TextColored(TEXT_PRIMARY, "%.1f", vol_data_.z[i][j]);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_corr_table() {
    if (corr_data_.z.empty()) return;
    const int n = (int)corr_data_.assets.size();
    const auto& last = corr_data_.z.back();

    ImGui::TextColored(ACCENT, "CORRELATION MATRIX — %dD ROLLING", corr_data_.window);
    ImGui::Spacing();

    if (ImGui::BeginTable("##corr_tbl", n + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50);
        for (int j = 0; j < n; j++)
            ImGui::TableSetupColumn(corr_data_.assets[j].c_str(), ImGuiTableColumnFlags_WidthFixed, 55);
        ImGui::TableHeadersRow();

        for (int i = 0; i < n; i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%s", corr_data_.assets[i].c_str());
            for (int j = 0; j < n; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float c = last[i * n + j];
                ImVec4 col = c > 0.5f ? MARKET_GREEN : (c < -0.2f ? MARKET_RED : TEXT_PRIMARY);
                if (i == j) col = TEXT_DIM;
                ImGui::TextColored(col, "%.2f", c);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_yield_table() {
    if (yield_data_.z.empty()) return;
    const int n_mat = (int)yield_data_.maturities.size();
    const int n_show = std::min(30, (int)yield_data_.z.size());
    const int start = (int)yield_data_.z.size() - n_show;

    ImGui::TextColored(ACCENT, "US TREASURY YIELD CURVE");
    ImGui::Spacing();

    if (ImGui::BeginTable("##yield_tbl", n_mat + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Day", ImGuiTableColumnFlags_WidthFixed, 45);
        for (int j = 0; j < n_mat && j < 11; j++)
            ImGui::TableSetupColumn(MAT_LABELS[j], ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();

        for (int i = start; i < (int)yield_data_.z.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "D-%d", (int)yield_data_.z.size() - i);
            for (int j = 0; j < n_mat; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                ImGui::TextColored(TEXT_PRIMARY, "%.2f", yield_data_.z[i][j]);
            }
        }
        ImGui::EndTable();
    }
}

void SurfaceScreen::render_pca_table() {
    if (pca_data_.z.empty()) return;
    const int n_f = (int)pca_data_.factors.size();

    ImGui::TextColored(ACCENT, "PCA FACTOR LOADINGS");
    ImGui::Spacing();

    for (int i = 0; i < n_f; i++) {
        char b[32]; std::snprintf(b, 32, "PC%d: %.1f%%", i+1, pca_data_.variance_explained[i]*100);
        ImGui::SameLine(0, i == 0 ? 0 : 8);
        ImGui::TextColored(i == 0 ? ACCENT : TEXT_SECONDARY, "%s", b);
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("##pca_tbl", n_f + 1,
        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthFixed, 55);
        for (int j = 0; j < n_f; j++)
            ImGui::TableSetupColumn(pca_data_.factors[j].c_str(), ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)pca_data_.assets.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(TEXT_SECONDARY, "%s", pca_data_.assets[i].c_str());
            for (int j = 0; j < n_f; j++) {
                ImGui::TableSetColumnIndex(j + 1);
                float v = pca_data_.z[i][j];
                ImVec4 col = v > 0.5f ? MARKET_GREEN : (v < -0.3f ? MARKET_RED : TEXT_PRIMARY);
                ImGui::TextColored(col, "%.3f", v);
            }
        }
        ImGui::EndTable();
    }
}

} // namespace fincept::surface
