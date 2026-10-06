/// @file event_bus.cpp
/// @brief Implementation of thread-safe event pub/sub system

#include "kalahari/core/event_bus.h"
#include "kalahari/core/logger.h"

// Disable Qt keywords (emit, signals, slots) to avoid conflicts with EventBus::emit()
#ifndef QT_NO_KEYWORDS
#define QT_NO_KEYWORDS
#endif

#include <QCoreApplication>
#include <algorithm>
#include <QMetaObject>

namespace kalahari {
namespace core {

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

SubscriptionId EventBus::subscribe(const std::string& eventType, EventListener listener) {
    if (eventType.empty()) {
        throw std::invalid_argument("Event type cannot be empty");
    }

    if (!listener) {
        throw std::invalid_argument("Listener cannot be null");
    }

    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    const SubscriptionId id = m_nextId++;
    auto& listeners = m_listeners[eventType];
    listeners.emplace_back(id, std::move(listener));

    Logger::getInstance().debug("EventBus: Subscribed to event type '{}' (subscribers: {})",
                               eventType, listeners.size());
    return id;
}

bool EventBus::unsubscribe(SubscriptionId id) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it) {
        auto& listeners = it->second;
        auto found = std::find_if(listeners.begin(), listeners.end(),
                                  [id](const auto& entry) { return entry.first == id; });
        if (found == listeners.end()) {
            continue;
        }
        listeners.erase(found);
        Logger::getInstance().debug("EventBus: Unsubscribed listener {} from event type '{}'",
                                   id, it->first);
        // An empty list would still count as "has subscribers"
        if (listeners.empty()) {
            m_listeners.erase(it);
        }
        return true;
    }
    return false;
}

void EventBus::emit(const Event& event) {
    // Listeners run on a copy without the lock held, so a listener may subscribe,
    // unsubscribe or emit again without deadlocking on the non-recursive mutex
    std::vector<EventListener> listeners;
    {
        std::lock_guard<std::mutex> lock(m_listeners_mutex);
        auto it = m_listeners.find(event.type);
        if (it == m_listeners.end()) {
            return;
        }
        listeners.reserve(it->second.size());
        for (const auto& entry : it->second) {
            listeners.push_back(entry.second);
        }
    }

    Logger::getInstance().debug("EventBus: Emitting event '{}' to {} subscribers",
                               event.type, listeners.size());

    for (auto& listener : listeners) {
        try {
            listener(event);
        } catch (const std::exception& e) {
            Logger::getInstance().error("EventBus: Listener for '{}' threw exception: {}",
                                       event.type, e.what());
        } catch (...) {
            Logger::getInstance().error("EventBus: Listener for '{}' threw unknown exception",
                                       event.type);
        }
    }
}

void EventBus::emitAsync(const Event& event) {
    size_t queueSize = 0;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_eventQueue.push(event);
        queueSize = m_eventQueue.size();
    }

    Logger::getInstance().debug("EventBus: Queued async event '{}' (queue size: {})",
                               event.type, queueSize);

    // Qt6 GUI thread marshalling via QMetaObject::invokeMethod
    QCoreApplication* app = QCoreApplication::instance();
    if (app) {
        // Schedule event processing on GUI thread (Qt::QueuedConnection)
        QMetaObject::invokeMethod(
            app,
            [this]() {
                // Process all queued events on GUI thread
                while (true) {
                    Event evt;
                    {
                        std::lock_guard<std::mutex> lock(m_queue_mutex);
                        if (m_eventQueue.empty()) {
                            break;
                        }
                        evt = m_eventQueue.front();
                        m_eventQueue.pop();
                    }

                    // Emit synchronously on GUI thread
                    emit(evt);
                }
            },
            Qt::QueuedConnection
        );
        return;
    }

    // Fallback: emit directly if QCoreApplication not available
    Logger::getInstance().warn("EventBus: QCoreApplication not available for async marshalling, "
                              "emitting directly");
    Event evt = event;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (!m_eventQueue.empty()) {
            evt = m_eventQueue.front();
            m_eventQueue.pop();
        }
    }
    emit(evt);
}

size_t EventBus::getSubscriberCount(const std::string& eventType) const {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    auto it = m_listeners.find(eventType);
    return it != m_listeners.end() ? it->second.size() : 0;
}

bool EventBus::hasSubscribers(const std::string& eventType) const {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);
    return m_listeners.find(eventType) != m_listeners.end();
}

void EventBus::clearAll() {
    std::lock_guard<std::mutex> listeners_lock(m_listeners_mutex);
    std::lock_guard<std::mutex> queue_lock(m_queue_mutex);

    size_t listener_count = 0;
    for (const auto& [type, listeners] : m_listeners) {
        listener_count += listeners.size();
    }

    m_listeners.clear();

    size_t queue_size = m_eventQueue.size();
    while (!m_eventQueue.empty()) {
        m_eventQueue.pop();
    }

    Logger::getInstance().info("EventBus: Cleared {} listeners and {} queued events",
                              listener_count, queue_size);
}

} // namespace core
} // namespace kalahari
