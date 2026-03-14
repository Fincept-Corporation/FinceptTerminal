#pragma once
// Support screen — ticket management with list, create, detail views

#include "support_types.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <future>

namespace fincept::support {

class SupportScreen {
public:
    void render();

private:
    // State
    SupportView current_view_ = SupportView::List;
    std::vector<SupportTicket> tickets_;
    int selected_ticket_idx_ = -1;
    SupportStats stats_{1, 1, 0};
    bool loading_ = false;
    bool data_loaded_ = false;
    std::future<void> load_future_;
    std::string status_message_;

    // Create form
    char new_subject_[256] = {};
    char new_description_[4096] = {};
    int category_idx_ = 0;
    int priority_idx_ = 1; // default medium

    // Message form
    char new_message_[2048] = {};

    // Render
    void render_header();
    void render_controls();
    void render_ticket_list();
    void render_create_form();
    void render_ticket_details();
    void render_footer();

    // Data
    void load_tickets();
    void create_ticket();
    void add_message();
    void update_status(const std::string& new_status);
};

} // namespace fincept::support
