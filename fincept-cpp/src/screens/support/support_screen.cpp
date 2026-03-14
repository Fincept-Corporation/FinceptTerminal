#include "support_screen.h"
#include "ui/theme.h"
#include "core/config.h"
#include "auth/auth_manager.h"
#include "auth/auth_api.h"
#include "http/http_client.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>

namespace fincept::support {

using json = nlohmann::json;

static const char* BASE_URL = "https://api.fincept.in";

// =============================================================================
// Main render
// =============================================================================
void SupportScreen::render() {
    using namespace theme::colors;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tab_h = ImGui::GetFrameHeight() + 4;
    float footer_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + tab_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - tab_h - footer_h));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BG_DARK);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##support_screen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Initialize with demo ticket
    if (!data_loaded_ && tickets_.empty()) {
        tickets_.push_back(get_demo_ticket());
        data_loaded_ = true;
        // Also try loading real tickets
        load_tickets();
    }

    // Check async load
    if (load_future_.valid() &&
        load_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        load_future_.get();
        loading_ = false;
    }

    render_header();
    render_controls();

    switch (current_view_) {
        case SupportView::List:    render_ticket_list(); break;
        case SupportView::Create:  render_create_form(); break;
        case SupportView::Details: render_ticket_details(); break;
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// =============================================================================
// Header
// =============================================================================
void SupportScreen::render_header() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_DARKEST);
    ImGui::BeginChild("##support_header", ImVec2(0, 40), ImGuiChildFlags_None);

    ImGui::SetCursorPos(ImVec2(16, 10));
    ImGui::TextColored(ACCENT, "FINCEPT SUPPORT CENTER");

    // Clock
    ImGui::SameLine(0, 16);
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tb[32];
    std::strftime(tb, sizeof(tb), "%H:%M:%S", t);
    ImGui::TextColored(TEXT_DIM, "%s", tb);

    // Stats on right
    float right_x = ImGui::GetWindowWidth() - 360;
    ImGui::SameLine(right_x);
    ImGui::TextColored(TEXT_DIM, "Total:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(INFO, "%d", stats_.total_tickets);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Open:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(WARNING, "%d", stats_.open_tickets);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(TEXT_DIM, "Resolved:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(SUCCESS, "%d", stats_.resolved_tickets);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Controls
// =============================================================================
void SupportScreen::render_controls() {
    using namespace theme::colors;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##support_controls", ImVec2(0, 40), ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(12, 6));

    if (current_view_ == SupportView::List) {
        if (theme::AccentButton("+ Create Ticket")) {
            current_view_ = SupportView::Create;
            std::memset(new_subject_, 0, sizeof(new_subject_));
            std::memset(new_description_, 0, sizeof(new_description_));
            category_idx_ = 0;
            priority_idx_ = 1;
        }
        ImGui::SameLine(0, 12);
        if (theme::SecondaryButton("Refresh")) {
            load_tickets();
        }
    } else {
        if (theme::SecondaryButton("<< Back to Tickets")) {
            current_view_ = SupportView::List;
            selected_ticket_idx_ = -1;
        }
    }

    if (loading_) {
        ImGui::SameLine(0, 16);
        theme::LoadingSpinner("Loading...");
    }

    if (!status_message_.empty()) {
        ImGui::SameLine(0, 16);
        ImGui::TextColored(INFO, "%s", status_message_.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// =============================================================================
// Ticket List
// =============================================================================
void SupportScreen::render_ticket_list() {
    using namespace theme::colors;

    ImGui::BeginChild("##ticket_list_area", ImVec2(0, 0), ImGuiChildFlags_None);

    ImGui::Spacing();
    ImGui::SetCursorPosX(16);
    theme::SectionHeader("SUPPORT TICKETS");
    ImGui::Spacing();

    if (tickets_.empty()) {
        ImGui::SetCursorPosX(16);
        ImGui::TextColored(TEXT_DIM, "No tickets found. Create one to get started.");
        ImGui::EndChild();
        return;
    }

    float avail_w = ImGui::GetContentRegionAvail().x - 32;

    for (int i = 0; i < static_cast<int>(tickets_.size()); i++) {
        auto& ticket = tickets_[i];

        ImVec4 border_col = status_color(ticket.status);

        ImGui::SetCursorPosX(16);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);

        char cid[32];
        std::snprintf(cid, sizeof(cid), "##ticket_%d", i);
        ImGui::BeginChild(cid, ImVec2(avail_w, 90), ImGuiChildFlags_Borders);

        // Left colored border
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 s = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + 4, p.y + s.y),
            ImGui::ColorConvertFloat4ToU32(border_col));

        // Content
        ImGui::SetCursorPos(ImVec2(16, 8));

        // ID + Subject
        ImGui::TextColored(TEXT_DIM, "%s", ticket.id.c_str());
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_PRIMARY, "%s", ticket.subject.c_str());

        // Demo badge
        if (ticket.is_demo) {
            ImGui::SameLine(0, 8);
            ImGui::TextColored(INFO, "[DEMO]");
        }

        // Status + Priority badges on right
        float badge_x = avail_w - 200;
        ImGui::SameLine(badge_x);
        ImGui::TextColored(status_color(ticket.status), "[%s]", ticket.status.c_str());
        ImGui::SameLine(0, 8);
        ImGui::TextColored(priority_color(ticket.priority), "%s", ticket.priority.c_str());

        // Second row: category + date
        ImGui::SetCursorPosX(16);
        ImGui::TextColored(TEXT_DIM, "Category: %s", ticket.category.c_str());
        ImGui::SameLine(0, 24);
        ImGui::TextColored(TEXT_DIM, "Created: %s", ticket.created_at.c_str());

        // Third row: description preview
        ImGui::SetCursorPosX(16);
        std::string preview = ticket.description;
        if (preview.size() > 100) preview = preview.substr(0, 100) + "...";
        ImGui::TextColored(TEXT_DISABLED, "%s", preview.c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            selected_ticket_idx_ = i;
            current_view_ = SupportView::Details;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        ImGui::Spacing();
    }

    ImGui::EndChild();
}

// =============================================================================
// Create Form
// =============================================================================
void SupportScreen::render_create_form() {
    using namespace theme::colors;

    ImGui::BeginChild("##create_form_area", ImVec2(0, 0), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float form_w = (avail_w > 700.0f) ? 700.0f : avail_w - 32;
    float pad_x = (avail_w - form_w) * 0.5f;
    if (pad_x < 16) pad_x = 16;

    ImGui::SetCursorPos(ImVec2(pad_x, 16));
    ImGui::BeginChild("##create_form", ImVec2(form_w, 0), ImGuiChildFlags_None);

    theme::SectionHeader("CREATE NEW TICKET");
    ImGui::Spacing();

    // Subject
    ImGui::TextColored(TEXT_SECONDARY, "Subject");
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##ticket_subject", new_subject_, sizeof(new_subject_));
    ImGui::PopItemWidth();
    ImGui::Spacing();

    // Category + Priority side by side
    float half_w = (form_w - 12) * 0.5f;

    ImGui::TextColored(TEXT_SECONDARY, "Category");
    ImGui::PushItemWidth(half_w);
    ImGui::Combo("##ticket_category", &category_idx_, SUPPORT_CATEGORIES, NUM_SUPPORT_CATEGORIES);
    ImGui::PopItemWidth();

    ImGui::SameLine(0, 12);

    ImGui::TextColored(TEXT_SECONDARY, "Priority");
    ImGui::PushItemWidth(half_w);
    ImGui::Combo("##ticket_priority", &priority_idx_, SUPPORT_PRIORITIES, NUM_SUPPORT_PRIORITIES);
    ImGui::PopItemWidth();
    ImGui::Spacing();

    // Description
    ImGui::TextColored(TEXT_SECONDARY, "Description");
    ImGui::PushItemWidth(-1);
    ImGui::InputTextMultiline("##ticket_desc", new_description_, sizeof(new_description_),
        ImVec2(-1, 200));
    ImGui::PopItemWidth();
    ImGui::Spacing(); ImGui::Spacing();

    // Buttons
    if (theme::AccentButton("Submit Ticket")) {
        create_ticket();
    }
    ImGui::SameLine(0, 12);
    if (theme::SecondaryButton("Cancel")) {
        current_view_ = SupportView::List;
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Ticket Details
// =============================================================================
void SupportScreen::render_ticket_details() {
    using namespace theme::colors;

    if (selected_ticket_idx_ < 0 || selected_ticket_idx_ >= static_cast<int>(tickets_.size())) {
        current_view_ = SupportView::List;
        return;
    }

    auto& ticket = tickets_[selected_ticket_idx_];

    ImGui::BeginChild("##ticket_detail_area", ImVec2(0, 0), ImGuiChildFlags_None);

    float avail_w = ImGui::GetContentRegionAvail().x;
    float detail_w = (avail_w > 800.0f) ? 800.0f : avail_w - 32;
    float pad_x = (avail_w - detail_w) * 0.5f;
    if (pad_x < 16) pad_x = 16;

    ImGui::SetCursorPos(ImVec2(pad_x, 16));
    ImGui::BeginChild("##ticket_detail", ImVec2(detail_w, 0), ImGuiChildFlags_None);

    // Header
    ImGui::TextColored(ACCENT, "%s", ticket.subject.c_str());
    if (ticket.is_demo) {
        ImGui::SameLine(0, 8);
        ImGui::TextColored(INFO, "[DEMO]");
    }
    ImGui::Spacing();

    // Metadata grid
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ticket_meta", ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::SetCursorPos(ImVec2(12, 8));

    ImGui::TextColored(TEXT_DIM, "ID:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_PRIMARY, "%s", ticket.id.c_str());
    ImGui::SameLine(0, 24);

    ImGui::TextColored(TEXT_DIM, "Status:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(status_color(ticket.status), "%s", ticket.status.c_str());
    ImGui::SameLine(0, 24);

    ImGui::TextColored(TEXT_DIM, "Priority:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(priority_color(ticket.priority), "%s", ticket.priority.c_str());
    ImGui::SameLine(0, 24);

    ImGui::TextColored(TEXT_DIM, "Category:");
    ImGui::SameLine(0, 4);
    ImGui::TextColored(TEXT_SECONDARY, "%s", ticket.category.c_str());

    ImGui::SetCursorPosX(12);
    ImGui::TextColored(TEXT_DIM, "Created: %s", ticket.created_at.c_str());

    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Description
    theme::SectionHeader("DESCRIPTION");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_PANEL);
    ImGui::BeginChild("##ticket_desc_view", ImVec2(0, 0),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + detail_w - 24);
    ImGui::TextColored(TEXT_SECONDARY, "%s", ticket.description.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Conversation
    theme::SectionHeader("CONVERSATION");

    for (int i = 0; i < static_cast<int>(ticket.messages.size()); i++) {
        auto& msg = ticket.messages[i];
        bool is_user = (msg.sender_type == "user");

        ImVec4 bg = is_user ? ImVec4(0.10f, 0.15f, 0.25f, 1.0f) :
                              ImVec4(0.15f, 0.10f, 0.20f, 1.0f);
        ImVec4 label_col = is_user ? INFO : ImVec4(0.70f, 0.40f, 0.90f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        char mid[32];
        std::snprintf(mid, sizeof(mid), "##msg_%d", i);
        ImGui::BeginChild(mid, ImVec2(0, 0),
            ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

        ImGui::SetCursorPos(ImVec2(12, 8));
        ImGui::TextColored(label_col, "%s", is_user ? "You" : "Support Team");
        ImGui::SameLine(0, 12);
        ImGui::TextColored(TEXT_DIM, "%s", msg.created_at.c_str());

        ImGui::SetCursorPosX(12);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + detail_w - 24);
        ImGui::TextColored(TEXT_SECONDARY, "%s", msg.message.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Add message (only for non-demo, non-closed)
    if (!ticket.is_demo && ticket.status != "closed") {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_SECONDARY, "Add a message:");
        ImGui::PushItemWidth(-1);
        ImGui::InputTextMultiline("##new_msg", new_message_, sizeof(new_message_),
            ImVec2(-1, 80));
        ImGui::PopItemWidth();
        ImGui::Spacing();

        if (theme::AccentButton("Send Message")) {
            add_message();
        }
        ImGui::SameLine(0, 12);

        if (ticket.status == "open" || ticket.status == "in_progress") {
            if (theme::SecondaryButton("Close Ticket")) {
                update_status("closed");
            }
        } else if (ticket.status == "resolved") {
            if (theme::SecondaryButton("Reopen Ticket")) {
                update_status("open");
            }
        }
    }

    if (ticket.is_demo) {
        ImGui::Spacing();
        ImGui::TextColored(TEXT_DIM,
            "This is a demo ticket. Create a real ticket for actual support.");
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

// =============================================================================
// Footer (rendered by app)
// =============================================================================
void SupportScreen::render_footer() {}

// =============================================================================
// Data loading
// =============================================================================
void SupportScreen::load_tickets() {
    auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated() || !auth.session().is_registered()) return;

    loading_ = true;
    status_message_ = "Loading tickets...";

    load_future_ = std::async(std::launch::async, [this]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            std::string url = std::string(BASE_URL) + "/support/tickets";
            auto resp = http::HttpClient::instance().get(url, headers);
            auto j = resp.json_body();

            if (j.contains("data") && j["data"].is_array()) {
                // Keep demo ticket, add real ones
                std::vector<SupportTicket> real_tickets;
                for (auto& item : j["data"]) {
                    SupportTicket t;
                    t.id = item.value("id", "");
                    t.subject = item.value("subject", "");
                    t.description = item.value("description", "");
                    t.category = item.value("category", "other");
                    t.priority = item.value("priority", "medium");
                    t.status = item.value("status", "open");
                    t.created_at = item.value("created_at", "");
                    t.updated_at = item.value("updated_at", "");
                    t.is_demo = false;

                    if (item.contains("messages") && item["messages"].is_array()) {
                        for (auto& m : item["messages"]) {
                            SupportMessage msg;
                            msg.sender_type = m.value("sender_type", "user");
                            msg.message = m.value("message", "");
                            msg.created_at = m.value("created_at", "");
                            t.messages.push_back(msg);
                        }
                    }
                    real_tickets.push_back(std::move(t));
                }

                // Merge: keep demo + add real
                tickets_.erase(
                    std::remove_if(tickets_.begin(), tickets_.end(),
                        [](const SupportTicket& t) { return !t.is_demo; }),
                    tickets_.end());
                for (auto& rt : real_tickets)
                    tickets_.push_back(std::move(rt));

                // Update stats
                stats_.total_tickets = static_cast<int>(tickets_.size());
                stats_.open_tickets = 0;
                stats_.resolved_tickets = 0;
                for (auto& t : tickets_) {
                    if (t.status == "open" || t.status == "in_progress") stats_.open_tickets++;
                    if (t.status == "resolved" || t.status == "closed") stats_.resolved_tickets++;
                }

                status_message_ = "Tickets loaded";
            }
        } catch (...) {
            status_message_ = "Using demo data (API unavailable)";
        }
    });
}

void SupportScreen::create_ticket() {
    auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated() || !auth.session().is_registered()) {
        status_message_ = "Please log in to create tickets";
        return;
    }

    if (std::strlen(new_subject_) == 0) {
        status_message_ = "Subject is required";
        return;
    }

    loading_ = true;
    status_message_ = "Creating ticket...";

    std::string subject = new_subject_;
    std::string description = new_description_;
    std::string category = SUPPORT_CATEGORIES[category_idx_];
    std::string priority_str = SUPPORT_PRIORITIES[priority_idx_];

    load_future_ = std::async(std::launch::async, [this, subject, description, category, priority_str]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            json body;
            body["subject"] = subject;
            body["description"] = description;
            body["category"] = category;
            body["priority"] = priority_str;

            std::string url = std::string(BASE_URL) + "/support/tickets";
            auto resp = http::HttpClient::instance().post_json(url, body, headers);

            if (resp.status_code >= 200 && resp.status_code < 300) {
                status_message_ = "Ticket created successfully!";
                current_view_ = SupportView::List;
                // Reload tickets
                load_tickets();
            } else {
                status_message_ = "Failed to create ticket";
            }
        } catch (...) {
            status_message_ = "Error creating ticket";
        }
    });
}

void SupportScreen::add_message() {
    if (selected_ticket_idx_ < 0 || selected_ticket_idx_ >= static_cast<int>(tickets_.size()))
        return;
    auto& ticket = tickets_[selected_ticket_idx_];
    if (ticket.is_demo || std::strlen(new_message_) == 0) return;

    std::string msg_text = new_message_;
    std::string ticket_id = ticket.id;

    load_future_ = std::async(std::launch::async, [this, msg_text, ticket_id]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            json body;
            body["message"] = msg_text;

            std::string url = std::string(BASE_URL) + "/support/tickets/" + ticket_id + "/messages";
            auto resp = http::HttpClient::instance().post_json(url, body, headers);

            if (resp.status_code >= 200 && resp.status_code < 300) {
                // Add locally
                SupportMessage m;
                m.sender_type = "user";
                m.message = msg_text;
                time_t now = time(nullptr);
                char tb[32];
                std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", localtime(&now));
                m.created_at = tb;

                if (selected_ticket_idx_ >= 0 &&
                    selected_ticket_idx_ < static_cast<int>(tickets_.size())) {
                    tickets_[selected_ticket_idx_].messages.push_back(m);
                }
                std::memset(new_message_, 0, sizeof(new_message_));
                status_message_ = "Message sent";
            }
        } catch (...) {
            status_message_ = "Error sending message";
        }
    });
}

void SupportScreen::update_status(const std::string& new_status) {
    if (selected_ticket_idx_ < 0 || selected_ticket_idx_ >= static_cast<int>(tickets_.size()))
        return;
    auto& ticket = tickets_[selected_ticket_idx_];
    if (ticket.is_demo) return;

    std::string ticket_id = ticket.id;

    load_future_ = std::async(std::launch::async, [this, new_status, ticket_id]() {
        try {
            auto headers = auth::AuthApi::instance().get_auth_headers(
                auth::AuthManager::instance().session().api_key);

            json body;
            body["status"] = new_status;

            std::string url = std::string(BASE_URL) + "/support/tickets/" + ticket_id;
            auto resp = http::HttpClient::instance().put_json(url, body, headers);

            if (resp.status_code >= 200 && resp.status_code < 300) {
                if (selected_ticket_idx_ >= 0 &&
                    selected_ticket_idx_ < static_cast<int>(tickets_.size())) {
                    tickets_[selected_ticket_idx_].status = new_status;
                }
                status_message_ = "Status updated to " + new_status;
            }
        } catch (...) {
            status_message_ = "Error updating status";
        }
    });
}

} // namespace fincept::support
