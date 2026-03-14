#pragma once
// EventBus — decoupled communication between modules
// Publish/subscribe pattern. Thread-safe.
//
// Usage:
//   // Subscribe
//   EventBus::instance().subscribe("market.quote", [](const Event& e) {
//       auto symbol = e.get<std::string>("symbol");
//   });
//
//   // Publish
//   EventBus::instance().publish("market.quote", {{"symbol", "AAPL"}, {"price", 150.0}});

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace fincept::core {

using json = nlohmann::json;

struct Event {
    std::string type;
    json data;

    template <typename T>
    T get(const std::string& key) const {
        return data.at(key).get<T>();
    }

    template <typename T>
    T get(const std::string& key, const T& fallback) const {
        if (data.contains(key)) return data[key].get<T>();
        return fallback;
    }
};

using EventHandler = std::function<void(const Event&)>;

class EventBus {
public:
    static EventBus& instance() {
        static EventBus s;
        return s;
    }

    // Subscribe to an event type. Returns subscription ID for unsubscribe.
    int subscribe(const std::string& event_type, EventHandler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        int id = next_id_++;
        subs_[event_type].push_back({id, std::move(handler)});
        return id;
    }

    // Unsubscribe by ID
    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [type, handlers] : subs_) {
            handlers.erase(
                std::remove_if(handlers.begin(), handlers.end(),
                    [id](const Sub& s) { return s.id == id; }),
                handlers.end());
        }
    }

    // Publish event to all subscribers
    void publish(const std::string& event_type, const json& data = {}) {
        std::vector<EventHandler> handlers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subs_.find(event_type);
            if (it == subs_.end()) return;
            for (auto& s : it->second) handlers.push_back(s.handler);
        }
        Event e{event_type, data};
        for (auto& h : handlers) h(e);
    }

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    EventBus() = default;

    struct Sub {
        int id;
        EventHandler handler;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Sub>> subs_;
    int next_id_ = 1;
};

} // namespace fincept::core
