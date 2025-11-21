/// @file event_bus.h
/// @brief Thread-safe event publish/subscribe system for plugin communication
///
/// The EventBus provides a central hub for asynchronous communication between
/// core components and plugins. It implements the Observer pattern with support
/// for both synchronous and asynchronous event delivery.
///
/// **Features:**
/// - Type-based event filtering (events grouped by type string)
/// - Thread-safe subscription/emission
/// - Synchronous emit (direct callback invocation)
/// - Asynchronous emit (Qt6 GUI thread marshalling via QMetaObject::invokeMethod)
/// - Python integration via pybind11
///
/// **Event Types (Standard):**
/// - "document:opened" - Document opened
/// - "document:saved" - Document saved
/// - "document:closed" - Document closed
/// - "editor:selection_changed" - Text selection changed
/// - "editor:content_changed" - Document content modified
/// - "plugin:loaded" - Plugin successfully loaded
/// - "plugin:unloaded" - Plugin unloaded
/// - "goal:reached" - User reached writing goal
///
/// **Example Usage (C++):**
/// @code
/// #include "kalahari/core/event_bus.h"
/// using namespace kalahari::core;
///
/// // Subscribe to an event
/// auto listener = [](const Event& event) {
///     if (event.type == "document:opened") {
///         std::cout << "Document opened!" << std::endl;
///     }
/// };
/// EventBus::getInstance().subscribe("document:opened", listener);
///
/// // Emit synchronously
/// Event evt{"document:opened", "my_document.klh"};
/// EventBus::getInstance().emit(evt);
///
/// // Emit asynchronously (safe from any thread)
/// EventBus::getInstance().emitAsync(evt);
/// @endcode
///
/// **Example Usage (Python):**
/// @code
/// import kalahari_api
///
/// def on_document_opened(event):
///     print(f"Document opened: {event.type}")
///
/// # Subscribe
/// kalahari_api.EventBus.subscribe("document:opened", on_document_opened)
///
/// # Emit (from Python)
/// event = kalahari_api.Event("document:opened")
/// kalahari_api.EventBus.emit(event)
/// @endcode

#pragma once

#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <any>
#include <memory>

namespace kalahari {
namespace core {

/// @brief Event data structure for pub/sub communication
///
/// Events are the primary communication mechanism between core and plugins.
/// Each event has a type identifier and optional data payload.
struct Event {
    /// Event type identifier (e.g., "document:opened", "goal:reached")
    std::string type;

    /// Event data payload (any type via std::any)
    /// Accessing: data = event.data to retrieve as std::any,
    ///           then use std::any_cast<T> to extract actual value
    std::any data;

    /// @brief Constructor with type and optional data
    /// @param eventType Type identifier for this event
    /// @param eventData Optional data payload
    Event(const std::string& eventType = "", const std::any& eventData = std::any())
        : type(eventType), data(eventData) {}
};

/// @brief Event listener callback type
///
/// Callbacks are invoked whenever a matching event is emitted.
/// Should be exception-safe; exceptions will be logged.
using EventListener = std::function<void(const Event&)>;

/// @brief Thread-safe pub/sub event bus (singleton)
///
/// Central event hub for core ↔ plugin communication.
/// Supports both sync and async event delivery with thread-safe operations.
class EventBus {
public:
    /// @brief Get singleton instance
    /// @return Reference to EventBus singleton
    static EventBus& getInstance();

    // Prevent copying/moving
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /// @brief Subscribe to event type
    ///
    /// Registers a callback to be invoked whenever an event of the specified
    /// type is emitted. Multiple listeners can subscribe to the same event type.
    ///
    /// @param eventType Event type to listen for (e.g., "document:opened")
    /// @param listener Callback function to invoke
    /// @throws std::invalid_argument if eventType is empty
    ///
    /// **Thread Safety:** Safe to call from any thread
    ///
    /// **Example:**
    /// ```cpp
    /// EventBus::getInstance().subscribe("document:opened",
    ///     [](const Event& evt) { std::cout << "Doc opened\n"; }
    /// );
    /// ```
    void subscribe(const std::string& eventType, EventListener listener);

    /// @brief Unsubscribe from event type
    ///
    /// Removes all listeners for the given event type.
    /// If no listeners are registered for the type, does nothing.
    ///
    /// @param eventType Event type to stop listening for
    /// **Thread Safety:** Safe to call from any thread
    void unsubscribe(const std::string& eventType);

    /// @brief Emit event synchronously
    ///
    /// Invokes all registered listeners for the event type immediately,
    /// in the calling thread. If any listener throws, the exception is logged
    /// and processing continues with remaining listeners.
    ///
    /// @param event Event to emit
    /// **Thread Safety:** Safe from any thread
    /// **Performance:** Direct callback invocation, lowest latency
    /// **Use Case:** When you need immediate response (e.g., state updates)
    ///
    /// **Example:**
    /// ```cpp
    /// Event evt("document:opened", std::string("document.klh"));
    /// EventBus::getInstance().emit(evt);
    /// ```
    void emit(const Event& event);

    /// @brief Emit event asynchronously
    ///
    /// Queues the event for delivery on the main GUI thread via
    /// wxTheApp->CallAfter. This is safe to call from worker threads
    /// and ensures GUI updates happen on the correct thread.
    ///
    /// If wxTheApp is not available, logs a warning and invokes listeners
    /// directly as fallback.
    ///
    /// @param event Event to emit asynchronously
    /// **Thread Safety:** Safe from any thread
    /// **Performance:** Queued delivery, slight latency for thread marshalling
    /// **Use Case:** When emitting from worker threads or needing GUI updates
    ///
    /// **Example:**
    /// ```cpp
    /// // Safe to call from worker thread
    /// Event evt("document:saved");
    /// EventBus::getInstance().emitAsync(evt);
    /// ```
    void emitAsync(const Event& event);

    /// @brief Get number of subscribers for an event type
    ///
    /// Useful for debugging and monitoring event subscription.
    ///
    /// @param eventType Event type to query
    /// @return Number of registered listeners, or 0 if type not found
    /// **Thread Safety:** Safe to call from any thread
    size_t getSubscriberCount(const std::string& eventType) const;

    /// @brief Clear all subscriptions
    ///
    /// Removes all listeners for all event types. Useful for shutdown
    /// or testing scenarios.
    ///
    /// **Thread Safety:** Safe to call from any thread
    void clearAll();

    /// @brief Check if event type has subscribers
    ///
    /// @param eventType Event type to check
    /// @return true if at least one listener is subscribed
    bool hasSubscribers(const std::string& eventType) const;

private:
    /// @brief Private constructor (singleton pattern)
    EventBus() = default;

public:
    /// @brief Destructor (public for pybind11 compatibility)
    /// @note Should never be called directly - only for RAII cleanup
    ~EventBus() = default;

private:

    /// Map of event type → list of listeners
    std::map<std::string, std::vector<EventListener>> m_listeners;

    /// Queue of pending async events
    std::queue<Event> m_eventQueue;

    /// Mutex for thread-safe access to listeners
    mutable std::mutex m_listeners_mutex;

    /// Mutex for thread-safe access to event queue
    mutable std::mutex m_queue_mutex;
};

} // namespace core
} // namespace kalahari
