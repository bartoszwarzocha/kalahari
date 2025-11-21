/// @file event_bus.cpp
/// @brief Implementation of thread-safe event pub/sub system

#include "kalahari/core/event_bus.h"
#include "kalahari/core/logger.h"

// Disable Qt keywords (emit, signals, slots) to avoid conflicts with EventBus::emit()
#ifndef QT_NO_KEYWORDS
#define QT_NO_KEYWORDS
#endif

#include <QCoreApplication>
#include <QMetaObject>

namespace kalahari {
namespace core {

EventBus& EventBus::getInstance() {
    static EventBus instance;
    return instance;
}

void EventBus::subscribe(const std::string& eventType, EventListener listener) {
    if (eventType.empty()) {
        throw std::invalid_argument("Event type cannot be empty");
    }

    if (!listener) {
        throw std::invalid_argument("Listener cannot be null");
    }

    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    m_listeners[eventType].push_back(listener);

    Logger::getInstance().debug("EventBus: Subscribed to event type '{}' (subscribers: {})",
                               eventType, m_listeners[eventType].size());
}

void EventBus::unsubscribe(const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    auto it = m_listeners.find(eventType);
    if (it != m_listeners.end()) {
        size_t count = it->second.size();
        m_listeners.erase(it);
        Logger::getInstance().debug("EventBus: Unsubscribed {} listener(s) from event type '{}'",
                                   count, eventType);
    }
}

void EventBus::emit(const Event& event) {
    std::lock_guard<std::mutex> lock(m_listeners_mutex);

    auto it = m_listeners.find(event.type);
    if (it == m_listeners.end()) {
        // No subscribers, just log and return
        return;
    }

    Logger::getInstance().debug("EventBus: Emitting event '{}' to {} subscribers",
                               event.type, it->second.size());

    // Invoke all listeners
    for (auto& listener : it->second) {
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
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_eventQueue.push(event);
    }

    Logger::getInstance().debug("EventBus: Queued async event '{}' (queue size: {})",
                               event.type, m_eventQueue.size());

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
