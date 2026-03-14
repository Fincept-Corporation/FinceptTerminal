#include "news_panel.h"
#include "ui/theme.h"
#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::research {

using namespace theme::colors;

static void open_url(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

void ResearchNewsPanel::render(ResearchData& data, const std::string& symbol) {
    std::lock_guard<std::mutex> lock(data.mutex());
    auto& articles = data.news();

    // Header bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_WIDGET);
    ImGui::BeginChild("##news_hdr", ImVec2(0, 30), true);
    ImGui::TextColored(ACCENT, "LATEST NEWS");
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "%d articles found", (int)articles.size());
    ImGui::SameLine(0, 16);
    ImGui::TextColored(INFO, "%s", symbol.c_str());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
    if (theme::SecondaryButton("REFRESH")) {
        data.fetch_news(symbol);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (data.is_news_loading()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(avail.x * 0.35f, ImGui::GetCursorPosY() + avail.y * 0.3f));
        theme::LoadingSpinner("Fetching latest news...");
        return;
    }

    if (articles.empty()) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(avail.x * 0.3f, ImGui::GetCursorPosY() + avail.y * 0.3f));
        ImGui::TextColored(WARNING, "NO NEWS FOUND");
        ImGui::SetCursorPosX(avail.x * 0.3f);
        ImGui::TextColored(TEXT_DIM, "No recent news articles found for %s", symbol.c_str());
        return;
    }

    // Article list
    ImGui::BeginChild("##news_list", ImVec2(0, 0), false);

    for (int i = 0; i < (int)articles.size(); i++) {
        auto& article = articles[i];
        bool is_selected = (i == selected_);

        ImGui::PushID(i);

        // Article card
        ImGui::PushStyleColor(ImGuiCol_ChildBg, is_selected ? BG_HOVER : BG_PANEL);
        ImGui::BeginChild("##art", ImVec2(0, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        // Title (clickable)
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_PRIMARY);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("%s", article.title.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            selected_ = (selected_ == i) ? -1 : i;
        }

        // Metadata line
        ImGui::TextColored(INFO, "%s", article.publisher.c_str());
        ImGui::SameLine(0, 16);
        ImGui::TextColored(TEXT_DIM, "%s", article.published_date.c_str());

        // Description (if selected)
        if (is_selected && !article.description.empty()) {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
            ImGui::TextColored(TEXT_SECONDARY, "%s", article.description.c_str());
            ImGui::PopTextWrapPos();
        }

        // Open link button
        if (!article.url.empty()) {
            if (is_selected) {
                ImGui::Spacing();
                if (theme::AccentButton("OPEN IN BROWSER")) {
                    open_url(article.url);
                }
            } else {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 110);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
                if (ImGui::SmallButton("READ ARTICLE >>")) {
                    open_url(article.url);
                }
                ImGui::PopStyleColor(2);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::PopID();
        ImGui::Spacing();
    }

    ImGui::EndChild();
}

} // namespace fincept::research
