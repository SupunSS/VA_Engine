/**
 * Event System Implementation
 * 
 * Provides pub/sub event system for decoupled communication between engine and game.
 */

#include <engine/public/EventSystem.h>
#include <unordered_map>
#include <vector>

/**
 * Internal implementation of event system
 */
class EventSystemImpl : public IEventSystem {
public:
    EventSystemImpl() = default;
    ~EventSystemImpl() override = default;

    void Subscribe(const std::string& eventName, EventCallback callback) override {
        if (callback) {
            m_subscribers[eventName].push_back(callback);
        }
    }

    void Unsubscribe(const std::string& eventName, EventCallback callback) override {
        auto it = m_subscribers.find(eventName);
        if (it == m_subscribers.end()) return;

        auto& callbacks = it->second;
        callbacks.erase(
            std::remove_if(callbacks.begin(), callbacks.end(),
                [&callback](const EventCallback& cb) {
                    return cb.target_type() == callback.target_type();
                }),
            callbacks.end()
        );
    }

    void Emit(const std::string& eventName, const void* data = nullptr) override {
        auto it = m_subscribers.find(eventName);
        if (it == m_subscribers.end()) return;

        // Make a copy in case callbacks unsubscribe during iteration
        auto callbacksCopy = it->second;
        for (const auto& callback : callbacksCopy) {
            if (callback) {
                callback(data);
            }
        }
    }

    void Clear(const std::string& eventName) override {
        auto it = m_subscribers.find(eventName);
        if (it != m_subscribers.end()) {
            it->second.clear();
        }
    }

    /**
     * Clear all events (called on shutdown)
     */
    void ClearAll() {
        m_subscribers.clear();
    }

private:
    std::unordered_map<std::string, std::vector<EventCallback>> m_subscribers;
};

// Global event system instance
static EventSystemImpl* g_eventSystem = nullptr;

/**
 * Get the global event system
 */
IEventSystem& GetEventSystem() {
    if (!g_eventSystem) {
        g_eventSystem = new EventSystemImpl();
    }
    return *g_eventSystem;
}

/**
 * Shutdown event system
 */
void ShutdownEventSystem() {
    if (g_eventSystem) {
        g_eventSystem->ClearAll();
        delete g_eventSystem;
        g_eventSystem = nullptr;
    }
}
