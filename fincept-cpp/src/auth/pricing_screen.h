#pragma once
#include "core/app_screen.h"
#include "http/api_types.h"
#include <vector>
#include <string>
#include <future>
#include <atomic>

namespace fincept::auth {

class PricingScreen {
public:
    void render(fincept::AppScreen& next_screen);
    void reset();

private:
    std::vector<api::SubscriptionPlan> plans_;
    std::atomic<bool> loading_{false};
    bool fetched_ = false;
    std::string error_;

    // Async plan fetching
    std::future<void> fetch_future_;
    void fetch_plans_async();

    // Payment
    std::string payment_error_;
    bool payment_loading_ = false;

    // Card rendering helper
    void render_plan_card(size_t index, float card_w, float card_h,
                          fincept::AppScreen& next_screen);
};

} // namespace fincept::auth
