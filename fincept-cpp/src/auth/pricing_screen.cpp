#include "pricing_screen.h"
#include "auth_manager.h"
#include "auth_api.h"
#include "theme/bloomberg_theme.h"
#include <imgui.h>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <thread>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fincept::auth {

using json = nlohmann::json;

static void open_url(const char* url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    std::string cmd = std::string("open ") + url;
    system(cmd.c_str());
#else
    std::string cmd = std::string("xdg-open ") + url;
    system(cmd.c_str());
#endif
}

void PricingScreen::reset() {
    plans_.clear();
    loading_ = false;
    fetched_ = false;
    error_.clear();
    payment_error_.clear();
    payment_loading_ = false;
}

void PricingScreen::fetch_plans_async() {
    loading_ = true;
    fetched_ = true;

    fetch_future_ = std::async(std::launch::async, [this]() {
        auto result = AuthApi::instance().get_subscription_plans();

        if (result.success && !result.data.is_null()) {
            json plans_data = result.data;
            if (plans_data.contains("data") && plans_data["data"].is_array()) {
                plans_data = plans_data["data"];
            }
            if (plans_data.is_array()) {
                std::vector<api::SubscriptionPlan> new_plans;
                for (auto& p : plans_data) {
                    api::SubscriptionPlan plan;
                    api::from_json_obj(p, plan);
                    new_plans.push_back(plan);
                }
                std::sort(new_plans.begin(), new_plans.end(),
                    [](const api::SubscriptionPlan& a, const api::SubscriptionPlan& b) {
                        return a.display_order < b.display_order;
                    });
                plans_ = std::move(new_plans);
            }
        } else {
            error_ = result.error.empty() ? "Failed to load plans" : result.error;
        }

        loading_ = false;
    });
}

// ─── Helper: render a single plan card ─────────────────────────────────────────
void PricingScreen::render_plan_card(size_t index, float card_w, float card_h,
                                      AppScreen& next_screen) {
    using namespace theme::colors;
    auto& auth = AuthManager::instance();
    auto& plan = plans_[index];

    bool is_popular = (plan.name == "Standard" || plan.name == "Pro");
    bool is_current = false;
    if (auth.is_authenticated()) {
        std::string user_plan = auth.session().user_info.account_type;
        for (auto& c : user_plan) c = (char)std::tolower(c);
        // Match against plan_id (e.g. "enterprise") not plan name (e.g. "Enterprise Plan")
        std::string pid = plan.plan_id;
        for (auto& c : pid) c = (char)std::tolower(c);
        is_current = (user_plan == pid);
    }

    // Card background
    ImVec4 card_bg = is_popular ? ImVec4(0.08f, 0.06f, 0.02f, 1.0f) : BG_DARK;
    ImVec4 card_border = is_popular ? ACCENT : (is_current ? SUCCESS : BORDER_DIM);
    float border_sz = is_popular ? 2.0f : 1.0f;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, card_bg);
    ImGui::PushStyleColor(ImGuiCol_Border, card_border);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border_sz);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 16));

    std::string child_id = "##plan_card_" + std::to_string(index);
    ImGui::BeginChild(child_id.c_str(), ImVec2(card_w, card_h),
        ImGuiChildFlags_Borders);

    float content_w = ImGui::GetContentRegionAvail().x;

    // ── Popular badge ──
    if (is_popular) {
        float badge_w = ImGui::CalcTextSize("MOST POPULAR").x + 16;
        ImGui::SetCursorPosX((content_w - badge_w) / 2);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ACCENT.x, ACCENT.y, ACCENT.z, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_Text, ACCENT);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
        ImGui::SmallButton("MOST POPULAR");
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::Spacing();
    } else {
        ImGui::Spacing();
    }

    // ── Plan Name (centered, large) ──
    ImGui::SetWindowFontScale(1.3f);
    float name_w = ImGui::CalcTextSize(plan.name.c_str()).x;
    ImGui::SetCursorPosX((content_w - name_w) / 2);
    ImGui::TextColored(TEXT_PRIMARY, "%s", plan.name.c_str());
    ImGui::SetWindowFontScale(1.0f);

    // ── Description (wrapped) ──
    if (!plan.description.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + content_w);
        ImGui::TextColored(TEXT_DIM, "%s", plan.description.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing(); ImGui::Spacing();

    // ── Price (centered, prominent) ──
    if (plan.is_free) {
        ImGui::SetWindowFontScale(1.8f);
        float fw = ImGui::CalcTextSize("FREE").x;
        ImGui::SetCursorPosX((content_w - fw) / 2);
        ImGui::TextColored(TEXT_SECONDARY, "FREE");
        ImGui::SetWindowFontScale(1.0f);
    } else {
        char price_buf[32];
        std::snprintf(price_buf, sizeof(price_buf), "$%.0f", plan.price_usd);
        ImGui::SetWindowFontScale(1.8f);
        float pw = ImGui::CalcTextSize(price_buf).x;
        ImGui::SetCursorPosX((content_w - pw) / 2);
        ImGui::TextColored(ACCENT, "%s", price_buf);
        ImGui::SetWindowFontScale(1.0f);

        char period_buf[32];
        std::snprintf(period_buf, sizeof(period_buf), "/ %d days", plan.validity_days);
        float per_w = ImGui::CalcTextSize(period_buf).x;
        ImGui::SetCursorPosX((content_w - per_w) / 2);
        ImGui::TextColored(TEXT_DIM, "%s", period_buf);
    }

    ImGui::Spacing();

    // ── Credits & Support (centered) ──
    char cred_buf[64];
    std::snprintf(cred_buf, sizeof(cred_buf), "%d credits", plan.credits);
    float cw = ImGui::CalcTextSize(cred_buf).x;
    ImGui::SetCursorPosX((content_w - cw) / 2);
    ImGui::TextColored(TEXT_SECONDARY, "%s", cred_buf);

    char sup_buf[64];
    std::snprintf(sup_buf, sizeof(sup_buf), "%s support", plan.support_type.c_str());
    float sw = ImGui::CalcTextSize(sup_buf).x;
    ImGui::SetCursorPosX((content_w - sw) / 2);
    ImGui::TextColored(TEXT_DIM, "%s", sup_buf);

    ImGui::Spacing();

    // ── Separator ──
    ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── Features (wrapped) ──
    for (auto& feature : plan.features) {
        ImGui::TextColored(is_popular ? ACCENT : SUCCESS, "  +");
        ImGui::SameLine();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (content_w - ImGui::GetCursorPosX() + ImGui::GetWindowContentRegionMin().x));
        ImGui::TextColored(TEXT_SECONDARY, "%s", feature.c_str());
        ImGui::PopTextWrapPos();
    }

    // Push button to bottom
    float btn_h = 36.0f;
    float cur_y = ImGui::GetCursorPosY();
    float avail_y = ImGui::GetContentRegionAvail().y;
    if (avail_y > btn_h + 8) {
        ImGui::SetCursorPosY(cur_y + avail_y - btn_h - 4);
    } else {
        ImGui::Spacing();
    }

    // ── Action Button ──
    if (is_current) {
        // Highlight border already indicates current; show a subtle active-plan button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::Button(("Active##" + std::to_string(index)).c_str(), ImVec2(-1, btn_h));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else if (plan.is_free) {
        bool has_paid = auth.is_authenticated() && auth.session().has_paid_plan();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (has_paid) {
            // User has a paid plan — grey out the free option
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, TEXT_DIM);
            ImGui::Button(("Free Tier##" + std::to_string(index)).c_str(), ImVec2(-1, btn_h));
            ImGui::PopStyleColor(2);
        } else if (theme::SecondaryButton(("Continue Free##" + std::to_string(index)).c_str(), ImVec2(-1, btn_h))) {
            next_screen = AppScreen::Dashboard;
        }
        ImGui::PopStyleVar();
    } else {
        std::string btn_label = payment_loading_
            ? ("Processing...##" + std::to_string(index))
            : ("Select Plan##" + std::to_string(index));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        bool clicked = theme::AccentButton(btn_label.c_str(), ImVec2(-1, btn_h));
        ImGui::PopStyleVar();

        if (clicked && !payment_loading_) {
            payment_loading_ = true;
            payment_error_.clear();

            std::string pid = plan.plan_id;
            std::string api_key = auth.session().api_key;
            std::thread([this, pid, api_key]() {
                auto& api = AuthApi::instance();
                json body;
                body["plan_id"] = pid;
                body["currency"] = "USD";

                auto headers = api.get_auth_headers(api_key);
                auto& client = http::HttpClient::instance();
                auto resp = client.post_json("https://api.fincept.in/cashfree/create-order", body, headers);

                if (resp.success) {
                    try {
                        auto j = json::parse(resp.body);
                        json api_data = j.contains("data") ? j["data"] : j;
                        std::string checkout_url;
                        if (api_data.contains("payment_session_id")) {
                            std::string sid = api_data["payment_session_id"].get<std::string>();
                            checkout_url = "https://fincept.in/checkout.html?session=" + sid;
                        }
                        if (checkout_url.empty() && api_data.contains("checkout_url"))
                            checkout_url = api_data["checkout_url"].get<std::string>();
                        if (checkout_url.empty() && api_data.contains("url"))
                            checkout_url = api_data["url"].get<std::string>();

                        if (!checkout_url.empty()) {
                            open_url(checkout_url.c_str());
                        } else {
                            payment_error_ = "No checkout URL received from server";
                        }
                    } catch (...) {
                        payment_error_ = "Failed to parse payment response";
                    }
                } else {
                    std::string err_msg;
                    try {
                        auto j = json::parse(resp.body);
                        if (j.contains("message")) err_msg = j["message"].get<std::string>();
                        if (err_msg.empty() && j.contains("error")) err_msg = j["error"].get<std::string>();
                    } catch (...) {}
                    payment_error_ = err_msg.empty()
                        ? (resp.error.empty() ? "Failed to create payment session" : resp.error)
                        : err_msg;
                }
                payment_loading_ = false;
            }).detach();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

// ─── Main Render — Fullscreen Layout ────────────────────────────────────────────
void PricingScreen::render(AppScreen& next_screen) {
    using namespace theme::colors;
    auto& auth = AuthManager::instance();

    // Fetch plans on first render (async)
    if (!fetched_ && !loading_) {
        fetch_plans_async();
    }

    // Fullscreen window
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##pricing_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 avail = ImGui::GetContentRegionAvail();

    // ── Scrollable content ──
    ImGui::BeginChild("##pricing_scroll", ImVec2(0, 0), false);

    float content_avail = ImGui::GetContentRegionAvail().x;
    float side_pad = std::max(content_avail * 0.05f, 20.0f);

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    // ── Header Section ──
    {
        ImGui::SetWindowFontScale(1.4f);
        const char* title = "Choose Your Plan";
        float tw = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((content_avail - tw) / 2);
        ImGui::TextColored(TEXT_PRIMARY, "%s", title);
        ImGui::SetWindowFontScale(1.0f);

        const char* subtitle = "Unlock the full power of Fincept Terminal with a plan that fits your needs";
        float stw = ImGui::CalcTextSize(subtitle).x;
        ImGui::SetCursorPosX((content_avail - stw) / 2);
        ImGui::TextColored(TEXT_DIM, "%s", subtitle);

        // Show current user info and active plan
        if (auth.is_authenticated() && auth.session().is_registered()) {
            ImGui::Spacing();
            auto& session = auth.session();
            std::string info = "Logged in as " + (session.user_info.email.empty() ? session.user_info.username : session.user_info.email);
            char cred[64];
            std::snprintf(cred, sizeof(cred), "  |  %.0f credits remaining", session.user_info.credit_balance);
            info += cred;
            float iw = ImGui::CalcTextSize(info.c_str()).x;
            ImGui::SetCursorPosX((content_avail - iw) / 2);
            ImGui::TextColored(TEXT_SECONDARY, "%s", info.c_str());

            // Current plan badge
            if (session.has_paid_plan()) {
                ImGui::Spacing();
                std::string plan_name = session.user_info.account_type;
                if (!plan_name.empty()) plan_name[0] = (char)std::toupper(plan_name[0]);

                std::string plan_label = "Current Plan: " + plan_name;
                float pw = ImGui::CalcTextSize(plan_label.c_str()).x + 24;
                ImGui::SetCursorPosX((content_avail - pw) / 2);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(SUCCESS.x, SUCCESS.y, SUCCESS.z, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_Text, SUCCESS);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 4));
                ImGui::Button(plan_label.c_str());
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4);
            }
        }
    }

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    // ── Loading / Error / Plans ──
    if (loading_) {
        ImGui::Spacing(); ImGui::Spacing();
        float spinner_x = (content_avail - 100) / 2;
        ImGui::SetCursorPosX(spinner_x);
        theme::LoadingSpinner("Loading plans...");
    } else if (!error_.empty()) {
        ImGui::SetCursorPosX(side_pad);
        theme::ErrorMessage(error_.c_str());
        ImGui::Spacing();
        ImGui::SetCursorPosX(side_pad);
        if (theme::SecondaryButton("Retry")) {
            fetched_ = false;
            error_.clear();
        }
    } else if (!plans_.empty()) {
        // Responsive grid: compute card size based on available width
        int num_plans = (int)plans_.size();
        float usable_w = content_avail - side_pad * 2;
        float gap = 16.0f;

        // Determine columns: try to fit all in one row, else wrap
        int cols = num_plans;
        float card_w = (usable_w - (cols - 1) * gap) / cols;

        // If cards are too narrow, reduce columns
        float min_card_w = 260.0f;
        if (card_w < min_card_w && cols > 3) { cols = 3; card_w = (usable_w - (cols - 1) * gap) / cols; }
        if (card_w < min_card_w && cols > 2) { cols = 2; card_w = (usable_w - (cols - 1) * gap) / cols; }
        if (card_w < min_card_w) { cols = 1; card_w = usable_w; }

        // Cap card width for aesthetics
        if (card_w > 380 && cols > 1) card_w = 380;

        // Card height — uniform for all cards, bigger to fit wrapped text
        float card_h = std::max(avail.y * 0.78f, 480.0f);
        if (card_h > 640) card_h = 640;

        // Center the row of cards
        float total_row_w = cols * card_w + (cols - 1) * gap;
        float start_x = (content_avail - total_row_w) / 2;

        int col = 0;
        for (size_t i = 0; i < plans_.size(); i++) {
            if (col == 0) {
                ImGui::SetCursorPosX(start_x);
            } else {
                ImGui::SameLine(0, gap);
            }

            render_plan_card(i, card_w, card_h, next_screen);

            col++;
            if (col >= cols) col = 0;
        }

        // Payment error
        if (!payment_error_.empty()) {
            ImGui::Spacing(); ImGui::Spacing();
            ImGui::SetCursorPosX(side_pad);
            theme::ErrorMessage(payment_error_.c_str());
        }
    } else {
        ImGui::Spacing(); ImGui::Spacing();
        const char* msg = "No plans available.";
        float mw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((content_avail - mw) / 2);
        ImGui::TextColored(TEXT_DIM, "%s", msg);
    }

    ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

    // ── Footer ──
    {
        ImGui::PushStyleColor(ImGuiCol_Separator, BORDER_DIM);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Show "Continue with Free Plan" only if user has no paid plan
        bool user_has_paid = auth.is_authenticated() && auth.session().has_paid_plan();

        if (user_has_paid) {
            // User already has a plan — show "Back to Dashboard"
            const char* back_btn = "Back to Dashboard";
            float bw = ImGui::CalcTextSize(back_btn).x;
            ImGui::SetCursorPosX((content_avail - bw) / 2);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, INFO);
            if (ImGui::Button(back_btn)) {
                next_screen = AppScreen::Dashboard;
            }
            ImGui::PopStyleColor(4);
        } else {
            const char* explore = "Want to explore first?";
            const char* free_btn = "Continue with Free Plan";
            float ew = ImGui::CalcTextSize(explore).x;
            float bw = ImGui::CalcTextSize(free_btn).x;
            float total = ew + 8 + bw;
            ImGui::SetCursorPosX((content_avail - total) / 2);

            ImGui::TextColored(TEXT_DIM, "%s", explore);
            ImGui::SameLine(0, 8);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, INFO);
            if (ImGui::Button(free_btn)) {
                next_screen = AppScreen::Dashboard;
            }
            ImGui::PopStyleColor(4);
        }

        ImGui::Spacing();
    }

    ImGui::EndChild(); // pricing_scroll

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace fincept::auth
