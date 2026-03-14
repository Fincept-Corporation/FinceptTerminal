#pragma once
// Support ticket types — data structs, constants, helpers

#include <imgui.h>
#include <string>
#include <vector>

namespace fincept::support {

struct SupportMessage {
    std::string sender_type; // "user" or "support"
    std::string message;
    std::string created_at;
};

struct SupportTicket {
    std::string id;
    std::string subject;
    std::string description;
    std::string category;
    std::string priority;
    std::string status;
    std::string created_at;
    std::string updated_at;
    std::vector<SupportMessage> messages;
    bool is_demo = false;
};

struct SupportStats {
    int total_tickets = 1;
    int open_tickets = 1;
    int resolved_tickets = 0;
};

enum class SupportView { List, Create, Details };

// Categories
constexpr const char* SUPPORT_CATEGORIES[] = {
    "technical", "billing", "feature", "bug", "account", "other"
};
constexpr int NUM_SUPPORT_CATEGORIES = 6;

// Priorities
constexpr const char* SUPPORT_PRIORITIES[] = {
    "low", "medium", "high"
};
constexpr int NUM_SUPPORT_PRIORITIES = 3;

// Status colors
inline ImVec4 status_color(const std::string& status) {
    if (status == "open")       return {0.30f, 0.60f, 0.96f, 1.0f}; // blue
    if (status == "in_progress") return {1.0f, 0.78f, 0.08f, 1.0f}; // yellow
    if (status == "resolved")   return {0.25f, 0.84f, 0.42f, 1.0f}; // green
    if (status == "closed")     return {0.53f, 0.53f, 0.57f, 1.0f}; // gray
    return {0.72f, 0.72f, 0.75f, 1.0f};
}

inline ImVec4 priority_color(const std::string& priority) {
    if (priority == "high")   return {0.96f, 0.30f, 0.30f, 1.0f}; // red
    if (priority == "medium") return {1.0f, 0.78f, 0.08f, 1.0f};  // yellow
    if (priority == "low")    return {0.25f, 0.84f, 0.42f, 1.0f};  // green
    return {0.72f, 0.72f, 0.75f, 1.0f};
}

// Demo ticket
inline SupportTicket get_demo_ticket() {
    SupportTicket t;
    t.id = "DEMO-001";
    t.subject = "Welcome to Fincept Support";
    t.description = "This is a demo ticket to help you understand how the support system works. "
                    "You can create real tickets to get help from our team.";
    t.category = "other";
    t.priority = "low";
    t.status = "open";
    t.created_at = "2024-01-01 00:00:00";
    t.updated_at = "2024-01-01 00:00:00";
    t.is_demo = true;
    t.messages = {
        {"support", "Welcome to Fincept Support! We're here to help you with any questions or issues.", "2024-01-01 00:00:00"},
        {"user", "Thanks! How do I create a new support ticket?", "2024-01-01 00:01:00"},
        {"support", "You can click the 'Create Ticket' button to submit a new support request. "
                     "Fill in the subject, category, priority, and description, then submit.", "2024-01-01 00:02:00"},
    };
    return t;
}

} // namespace fincept::support
