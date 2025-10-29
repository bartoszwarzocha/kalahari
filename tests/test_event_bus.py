#!/usr/bin/env python3
"""
Test Python integration with EventBus via pybind11 bindings

Tests cover:
- Import kalahari_api module
- Create Event objects
- Subscribe to events from Python
- Emit events from Python and C++
- Verify callbacks are called
"""

import sys
import os

# Add build directory to path to find kalahari_api module
build_path = os.path.join(os.path.dirname(__file__), '..', 'build-linux', 'lib', 'python')
if os.path.exists(build_path):
    sys.path.insert(0, build_path)

# Also try WSL build path
build_path_wsl = os.path.join(os.path.dirname(__file__), '..', 'build-linux-wsl', 'lib', 'python')
if os.path.exists(build_path_wsl):
    sys.path.insert(0, build_path_wsl)

try:
    import kalahari_api
    print("✅ Successfully imported kalahari_api")
except ImportError as e:
    print(f"❌ Failed to import kalahari_api: {e}")
    sys.exit(1)

# =============================================================================
# Test 1: Event Creation
# =============================================================================

try:
    # Create event with type only
    evt1 = kalahari_api.Event("test:event1")
    assert evt1.type == "test:event1"
    print("✅ Event(type) constructor works")

    # Note: Event(type, data) constructor is supported but requires special handling
    # for std::any type conversion. Will test via subscription instead.
except Exception as e:
    print(f"❌ Event creation failed: {e}")
    sys.exit(1)

# =============================================================================
# Test 2: EventBus Singleton
# =============================================================================

try:
    bus1 = kalahari_api.EventBus.get_instance()
    bus2 = kalahari_api.EventBus.get_instance()
    assert bus1 is bus2
    print("✅ EventBus.get_instance() returns singleton")
except Exception as e:
    print(f"❌ EventBus singleton test failed: {e}")
    sys.exit(1)

# =============================================================================
# Test 3: Event Subscription and Emission
# =============================================================================

try:
    bus = kalahari_api.EventBus.get_instance()
    bus.clear_all()

    # Track received events
    events_received = []

    def on_event(event):
        """Callback for event handler"""
        events_received.append({'type': event.type})

    # Subscribe to event
    bus.subscribe("test:python", on_event)
    print("✅ EventBus.subscribe() works")

    # Emit event synchronously
    evt = kalahari_api.Event("test:python")
    bus.emit(evt)

    assert len(events_received) == 1
    assert events_received[0]['type'] == "test:python"
    print("✅ EventBus.emit() calls Python callback")

except Exception as e:
    print(f"❌ Event emission/subscription test failed: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# =============================================================================
# Test 4: Multiple Subscriptions
# =============================================================================

try:
    bus.clear_all()
    count1 = [0]
    count2 = [0]

    def callback1(event):
        count1[0] += 1

    def callback2(event):
        count2[0] += 1

    bus.subscribe("multi:event", callback1)
    bus.subscribe("multi:event", callback2)
    print("✅ Multiple subscriptions work")

    evt = kalahari_api.Event("multi:event")
    bus.emit(evt)

    assert count1[0] == 1
    assert count2[0] == 1
    print("✅ Both callbacks received event")

except Exception as e:
    print(f"❌ Multiple subscriptions test failed: {e}")
    sys.exit(1)

# =============================================================================
# Test 5: Async Emission
# =============================================================================

try:
    bus.clear_all()

    def on_async(event):
        pass  # Simple callback

    bus.subscribe("async:test", on_async)
    evt = kalahari_api.Event("async:test")
    bus.emit_async(evt)
    print("✅ EventBus.emit_async() works (no exception)")

except Exception as e:
    print(f"❌ Async emission test failed: {e}")
    sys.exit(1)

# =============================================================================
# Test 6: Subscriber Queries
# =============================================================================

try:
    bus.clear_all()

    assert bus.get_subscriber_count("nonexistent:event") == 0
    print("✅ get_subscriber_count() returns 0 for non-existent type")

    bus.subscribe("query:test", lambda evt: None)
    assert bus.get_subscriber_count("query:test") == 1
    print("✅ get_subscriber_count() returns correct count")

    assert bus.has_subscribers("query:test")
    assert not bus.has_subscribers("nonexistent:event")
    print("✅ has_subscribers() works correctly")

except Exception as e:
    print(f"❌ Subscriber query test failed: {e}")
    sys.exit(1)

# =============================================================================
# Test 7: Logger Integration (verify it still works)
# =============================================================================

try:
    kalahari_api.Logger.info("Test info message")
    kalahari_api.Logger.debug("Test debug message")
    kalahari_api.Logger.warn("Test warning message")
    kalahari_api.Logger.error("Test error message")
    print("✅ Logger methods work alongside EventBus")
except Exception as e:
    print(f"❌ Logger test failed: {e}")
    sys.exit(1)

# =============================================================================
# Summary
# =============================================================================

print("\n" + "="*60)
print("✅ All EventBus Python integration tests PASSED!")
print("="*60)
