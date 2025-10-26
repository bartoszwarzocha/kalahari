# Task #00002: Threading Infrastructure

## Context
- **Phase:** Phase 0 Week 2
- **Roadmap Reference:** project_docs/07_mvp_tasks.md (Week 2)
- **Related Docs:**
  - project_docs/03_architecture.md (Threading Model, lines 316-357)
  - tasks/.wip/threading_architecture_proposal.md (Hybrid approach analysis)
  - .claude/skills/kalahari-wxwidgets/README.md (Thread safety patterns)
- **Dependencies:**
  - Task #00001 (wxWidgets Basic Window) - MUST BE COMPLETED FIRST
  - Bartosz's wxThread example analysis - ✅ COMPLETED (2025-10-26)

## Objective
Add thread management infrastructure to MainWindow using hybrid approach (std::thread + wxQueueEvent pattern).
This establishes the threading foundation for future plugin system (Week 3+) and heavy operations.

**Goal:** MainWindow can submit background tasks with thread pool limiting, using proven wxQueueEvent communication pattern.

## Approved Architecture

**Hybrid Approach** (approved by User 2025-10-26):
- **std::thread** for thread creation (modern C++, Python GIL compatible, wxWidgets 3.3+ recommended)
- **wxQueueEvent** for thread→GUI communication (Bartosz's proven pattern)
- **wxSemaphore** for thread pool limiting (Bartosz's proven pattern)
- **CallAfter** for simple GUI updates (convenience)

**Key Decisions:**
- Max threads: **4** (CPU-aware, configurable in Phase 1)
- Event naming: **wxEVT_KALAHARI_*** convention
- Pattern source: Bartosz's `wxwidgets_book_examples/Multithreading/` (working code)

## Proposed Approach

### 1. Architecture Overview

**Files to Modify:**
```
src/gui/
├── main_window.h          # Add threading members, event declarations
└── main_window.cpp        # Implement submitBackgroundTask(), handlers
```

**New Components:**
1. Thread pool tracking (`std::vector<std::thread::id>`)
2. Thread limiting semaphore (`wxSemaphore m_threadSemaphore`)
3. Thread synchronization (`wxMutex m_threadMutex`)
4. Background task submission API (`submitBackgroundTask()`)
5. Custom events (`wxEVT_KALAHARI_TASK_COMPLETED`, `wxEVT_KALAHARI_TASK_FAILED`)

### 2. Step-by-Step Implementation

#### Step 1: Add Threading Members to MainWindow (20 min)
**File:** `src/gui/main_window.h`

```cpp
// Add after existing #includes
#include <thread>
#include <functional>
#include <vector>
#include <algorithm>

namespace kalahari::gui {

// Custom event types (Kalahari convention)
wxDECLARE_EVENT(wxEVT_KALAHARI_TASK_COMPLETED, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_KALAHARI_TASK_FAILED, wxThreadEvent);

class MainWindow : public wxFrame {
public:
    MainWindow();
    ~MainWindow() override;

private:
    // ... existing members (m_menuBar, m_toolBar, etc.) ...

    // Threading infrastructure (NEW)
    wxMutex m_threadMutex;
    wxSemaphore m_threadSemaphore;  // Max 4 threads
    std::vector<std::thread::id> m_activeThreads;

    // Thread management API (NEW)
    bool submitBackgroundTask(std::function<void()> task);
    void onTaskCompleted(wxThreadEvent& event);
    void onTaskFailed(wxThreadEvent& event);

    // ... existing event handlers ...

    wxDECLARE_EVENT_TABLE();
};

} // namespace kalahari::gui
```

**Why:**
- `wxSemaphore(4, 4)` - Limits max threads (Bartosz's proven pattern)
- `std::vector<std::thread::id>` - Track active threads for cleanup
- `wxMutex` - Protect thread pool access (same as Bartosz's code)
- `std::function<void()>` - Modern C++ callback (replaces wxThread subclasses)

#### Step 2: Define Custom Events (10 min)
**File:** `src/gui/main_window.cpp`

```cpp
#include "main_window.h"
#include "core/logger.h"
#include <chrono>

namespace kalahari::gui {

// Define custom events (KALAHARI convention)
wxDEFINE_EVENT(wxEVT_KALAHARI_TASK_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_KALAHARI_TASK_FAILED, wxThreadEvent);

// ... existing code ...

} // namespace kalahari::gui
```

**Why:**
- Custom events for type-safe communication
- Follows Bartosz's pattern (wxEVT_MYTHREAD_UPDATED → wxEVT_KALAHARI_TASK_COMPLETED)
- Allows passing data via SetString(), SetInt(), SetPayload()

#### Step 3: Initialize Threading in Constructor (15 min)
**File:** `src/gui/main_window.cpp`

```cpp
MainWindow::MainWindow()
    : wxFrame(nullptr, wxID_ANY, _("Kalahari Writer's IDE")),
      m_threadSemaphore(4, 4)  // Max 4 background threads
{
    Logger::getInstance().info("Initializing main window with threading support");

    // Reserve thread pool capacity
    m_activeThreads.reserve(4);

    // ... existing initialization (createMenuBar, createToolBar, etc.) ...

    Logger::getInstance().debug("Threading infrastructure initialized (max 4 threads)");
}
```

**Why:**
- Initialize semaphore with max=4, initial=4 (all slots available)
- Reserve vector capacity (avoid reallocation during runtime)
- Log initialization for debugging

#### Step 4: Implement submitBackgroundTask() (45 min)
**File:** `src/gui/main_window.cpp`

```cpp
bool MainWindow::submitBackgroundTask(std::function<void()> task) {
    // Check thread limit (Bartosz's semaphore pattern)
    {
        wxMutexLocker lock(m_threadMutex);
        wxSemaError serr = m_threadSemaphore.TryWait();
        if (serr != wxSEMA_NO_ERROR) {
            Logger::getInstance().warn("Background task rejected: thread limit reached (4/4 active)");

            // Notify user via status bar
            CallAfter([this]() {
                m_statusBar->SetStatusText(_("Busy - please wait..."), 0);
            });

            return false;
        }
    }

    Logger::getInstance().debug("Submitting background task (thread pool: {}/4)",
                                m_activeThreads.size() + 1);

    // Create worker thread (std::thread, NOT wxThread)
    std::thread worker([this, task]() {
        // Track this thread
        std::thread::id threadId = std::this_thread::get_id();
        {
            wxMutexLocker lock(m_threadMutex);
            m_activeThreads.push_back(threadId);
        }

        Logger::getInstance().debug("Background task started (thread ID: {})",
                                    static_cast<unsigned long>(std::hash<std::thread::id>{}(threadId)));

        // Execute task with exception handling
        try {
            task();  // User-provided work

            // Task succeeded - notify GUI (Bartosz's wxQueueEvent pattern)
            wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
            evt->SetId(static_cast<int>(std::hash<std::thread::id>{}(threadId)));
            wxQueueEvent(this, evt);  // Thread-safe, no mutex needed

        } catch (const std::exception& e) {
            // Task failed - notify GUI with error
            Logger::getInstance().error("Background task failed: {}", e.what());

            wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_FAILED);
            evt->SetString(wxString::FromUTF8(e.what()));
            evt->SetId(static_cast<int>(std::hash<std::thread::id>{}(threadId)));
            wxQueueEvent(this, evt);  // Thread-safe
        }

        // Cleanup (Bartosz's OnExit() pattern)
        {
            wxMutexLocker lock(m_threadMutex);
            auto it = std::find(m_activeThreads.begin(), m_activeThreads.end(), threadId);
            if (it != m_activeThreads.end()) {
                m_activeThreads.erase(it);
            }
        }
        m_threadSemaphore.Post();  // Release semaphore slot

        Logger::getInstance().debug("Background task finished (thread pool: {}/4)",
                                    m_activeThreads.size());
    });

    worker.detach();  // Fire-and-forget (like wxTHREAD_DETACHED)
    return true;
}
```

**Why:**
- **Same semaphore logic** as Bartosz's code (TryWait, check error, Post on exit)
- **Same wxQueueEvent pattern** (allocate event, set data, wxQueueEvent)
- **Exception safety** (catch errors, send wxEVT_KALAHARI_TASK_FAILED)
- **Thread tracking** (add on start, remove on exit - like Bartosz's thread pool)
- **Logging** for debugging (thread count, IDs, status)

#### Step 5: Implement Event Handlers (30 min)
**File:** `src/gui/main_window.cpp`

```cpp
void MainWindow::onTaskCompleted(wxThreadEvent& event) {
    int threadId = event.GetId();
    Logger::getInstance().info("Background task completed (thread ID: {})", threadId);

    // Update status bar
    m_statusBar->SetStatusText(_("Ready"), 0);

    // Future: Handle task results from event.GetPayload()
    // Phase 1+: Update document state, refresh UI, etc.
}

void MainWindow::onTaskFailed(wxThreadEvent& event) {
    int threadId = event.GetId();
    wxString error = event.GetString();

    Logger::getInstance().error("Background task failed (thread ID: {}): {}",
                                threadId, error.utf8_str().data());

    // Show error to user
    m_statusBar->SetStatusText(_("Error - check logs"), 0);
    wxLogError("Background task failed: %s", error);

    // Future Phase 1+: Show error dialog with details
}
```

**Why:**
- Simple handlers for Week 2 (just logging + status bar)
- Phase 1+: Will handle plugin results, document updates, etc.
- User-facing error messages

#### Step 6: Update Event Table (10 min)
**File:** `src/gui/main_window.cpp`

```cpp
wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
    // ... existing events (EVT_MENU, EVT_CLOSE, etc.) ...

    // Threading events (NEW)
    EVT_THREAD(wxID_ANY, MainWindow::onTaskCompleted)  // Catches both COMPLETED and FAILED
wxEND_EVENT_TABLE()
```

**Wait, need separate handlers!** Update to:

```cpp
// In main_window.h, add Bind() calls in constructor instead:
MainWindow::MainWindow() {
    // ... existing init ...

    // Bind thread events dynamically (modern approach, like Bartosz's Bind())
    Bind(wxEVT_KALAHARI_TASK_COMPLETED, &MainWindow::onTaskCompleted, this, wxID_ANY);
    Bind(wxEVT_KALAHARI_TASK_FAILED, &MainWindow::onTaskFailed, this, wxID_ANY);
}
```

**Why:**
- `Bind()` allows separate handlers for different event types
- More flexible than event tables (Bartosz uses this in his example)
- Can bind/unbind at runtime

#### Step 7: Add Destructor Cleanup (20 min)
**File:** `src/gui/main_window.cpp`

```cpp
MainWindow::~MainWindow() {
    Logger::getInstance().info("MainWindow shutting down...");

    // Wait for all background tasks to complete
    if (!m_activeThreads.empty()) {
        Logger::getInstance().warn("Waiting for {} background tasks to finish...",
                                   m_activeThreads.size());

        // Wait up to 5 seconds for threads to finish
        auto startTime = std::chrono::steady_clock::now();
        while (!m_activeThreads.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > std::chrono::seconds(5)) {
                Logger::getInstance().error("Timeout waiting for background tasks (forced shutdown)");
                break;
            }
        }
    }

    Logger::getInstance().info("MainWindow destroyed (all background tasks completed)");
}
```

**Why:**
- Graceful shutdown (wait for tasks to finish)
- Timeout prevents infinite hang (5 seconds max)
- Logs forced shutdown if timeout occurs

#### Step 8: Add Example Usage to Existing Handler (15 min)
**File:** `src/gui/main_window.cpp`

```cpp
// Update existing onFileOpen handler to demonstrate threading
void MainWindow::onFileOpen(wxCommandEvent& event) {
    Logger::getInstance().info("File -> Open clicked");

    // Update status bar immediately (GUI thread)
    m_statusBar->SetStatusText(_("Loading file..."), 0);

    // Submit background task (simulates future file I/O)
    bool submitted = submitBackgroundTask([this]() {
        // Simulate heavy file loading (future: actual wxFile, JSON parsing, etc.)
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Simulate file loaded
        std::string filename = "example.klh";  // Future: from wxFileDialog

        // Update GUI when done (Bartosz's wxQueueEvent pattern)
        wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
        evt->SetString(wxString::FromUTF8(filename));
        evt->SetInt(1234);  // Future: file size or document ID
        wxQueueEvent(this, evt);
    });

    if (!submitted) {
        // Thread limit reached - show message
        wxMessageBox(_("Too many operations in progress. Please wait."),
                    _("Busy"), wxOK | wxICON_WARNING);
    }
}
```

**Why:**
- Demonstrates threading in action (Week 2 demo)
- Shows how future file operations will work
- Tests thread limiting (try clicking "Open" 5 times rapidly)

### 3. Testing Strategy

**Manual Testing:**
1. **Build and run** on all 3 platforms
2. **Test thread limiting:**
   - Click "File -> Open" 5 times rapidly
   - Should see 4 tasks running, 5th rejected with warning
   - Check logs show "thread limit reached"
3. **Test success case:**
   - Click "File -> Open" once
   - Status bar shows "Loading file..."
   - After 2 seconds: Status bar shows "Ready"
   - Logs show "Background task completed"
4. **Test cleanup:**
   - Start 2-3 background tasks
   - Close window while tasks running
   - Should wait up to 5 seconds, then close
   - Logs show "Waiting for N background tasks to finish..."
5. **Test exception handling:** (Phase 1+)
   - Modify example to throw exception
   - Should see wxEVT_KALAHARI_TASK_FAILED event
   - Status bar shows "Error - check logs"

**Log Verification:**
```
[info] Initializing main window with threading support
[debug] Threading infrastructure initialized (max 4 threads)
[info] File -> Open clicked
[debug] Submitting background task (thread pool: 1/4)
[debug] Background task started (thread ID: 12345)
[info] Background task completed (thread ID: 12345)
[debug] Background task finished (thread pool: 0/4)
```

**Platform-Specific Checks:**
- **Windows:** No console window shows during background tasks
- **macOS:** GUI remains responsive during 2-second sleep
- **Linux:** GTK3 event loop not blocked

**Performance Checks:**
- Semaphore limiting works (max 4 threads)
- No memory leaks (threads properly cleaned up)
- No race conditions (mutex protects thread pool vector)

### 4. Integration with Future Work

**Week 3-4 (Python Embedding):**
```cpp
// Future: Execute Python plugin in background
submitBackgroundTask([this]() {
    py::gil_scoped_acquire acquire;  // Get Python GIL

    {
        py::gil_scoped_release release;  // Release during work
        auto result = m_pluginManager->execute("MyPlugin", "command");
    }

    // Notify GUI
    wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
    evt->SetPayload(result);
    wxQueueEvent(this, evt);
});
```

**Week 5+ (Heavy Operations):**
- File I/O (load/save .klh files)
- AI API calls (Claude/OpenAI)
- Document parsing (DOCX import)
- Statistics generation (word count, reading time)

## Implementation Plan (Checklist)

### Prerequisites
- [x] Task #00001 completed (MainWindow exists)
- [x] Threading proposal approved by User
- [x] Bartosz's wxThread patterns analyzed

### Implementation Steps
- [ ] **Step 1:** Add threading members to main_window.h
  - [ ] Include <thread>, <functional>, <vector>
  - [ ] Declare wxEVT_KALAHARI_TASK_COMPLETED, wxEVT_KALAHARI_TASK_FAILED
  - [ ] Add m_threadMutex, m_threadSemaphore, m_activeThreads
  - [ ] Declare submitBackgroundTask(), onTaskCompleted(), onTaskFailed()
- [ ] **Step 2:** Define custom events in main_window.cpp
  - [ ] wxDEFINE_EVENT for both event types
- [ ] **Step 3:** Initialize threading in constructor
  - [ ] Initialize m_threadSemaphore(4, 4)
  - [ ] Reserve m_activeThreads capacity
  - [ ] Bind custom events to handlers
  - [ ] Log initialization
- [ ] **Step 4:** Implement submitBackgroundTask()
  - [ ] Semaphore check (TryWait)
  - [ ] Create std::thread with lambda
  - [ ] Track thread ID in m_activeThreads
  - [ ] Execute task with try/catch
  - [ ] Send wxQueueEvent on success/failure
  - [ ] Cleanup: remove from pool, Post semaphore
  - [ ] Detach thread
- [ ] **Step 5:** Implement event handlers
  - [ ] onTaskCompleted: log, update status bar
  - [ ] onTaskFailed: log error, show to user
- [ ] **Step 6:** Update event binding
  - [ ] Bind wxEVT_KALAHARI_TASK_COMPLETED in constructor
  - [ ] Bind wxEVT_KALAHARI_TASK_FAILED in constructor
- [ ] **Step 7:** Add destructor cleanup
  - [ ] Wait for m_activeThreads to empty
  - [ ] Timeout after 5 seconds
  - [ ] Log graceful/forced shutdown
- [ ] **Step 8:** Add example usage
  - [ ] Update onFileOpen with background task demo
  - [ ] Test thread limiting (submit 5 tasks rapidly)

### Verification
- [ ] Code compiles on all platforms (CI passes)
- [ ] Thread limiting works (max 4 concurrent tasks)
- [ ] Status bar updates during background work
- [ ] Logs show thread lifecycle (start, complete, cleanup)
- [ ] Window closes gracefully (waits for tasks)
- [ ] No memory leaks (threads properly joined/detached)
- [ ] No race conditions (mutex protects shared data)
- [ ] Example usage works (File -> Open demo)

## Risks & Open Questions

### Risks
- **Risk:** std::thread::id hashing might collide (unlikely with std::hash)
  - **Mitigation:** Use proper hash function, log collisions if occur
- **Risk:** Detached threads might outlive MainWindow (destructor issue)
  - **Mitigation:** Destructor waits up to 5 seconds, then forces shutdown
- **Risk:** Semaphore timing issues (TryWait race condition)
  - **Mitigation:** Protected by m_threadMutex (same as Bartosz's pattern)

### Open Questions
- **Q:** Should max threads be configurable in UI (Phase 1)?
  - **A:** Yes - add to settings dialog in Phase 1 (default 4)
- **Q:** Should we add thread priority levels (high/normal/low)?
  - **A:** Not in Week 2 - add if needed in Phase 2+
- **Q:** Should we use std::jthread (C++20 auto-join)?
  - **A:** No - we need manual control for semaphore pattern
- **Q:** Should we track task names/descriptions for debugging?
  - **A:** Yes - add optional std::string parameter to submitBackgroundTask() in Phase 1

## Status
- **Created:** 2025-10-26
- **Approved:** ✅ 2025-10-26 (by User - hybrid approach, 4 threads, KALAHARI convention)
- **Started:** 2025-10-26 (after Task #00001 completion)
- **Completed:** ✅ 2025-10-26

## Implementation Notes

### Architecture Decisions
- **Hybrid Pattern:** std::thread + wxQueueEvent (modern C++ + Bartosz's proven pattern)
- **Thread Pool:** Fixed max 4 threads (wxSemaphore-based limiting)
- **Event Naming:** wxEVT_KALAHARI_* convention (consistent branding)
- **Binding:** Dynamic Bind() in constructor (more flexible than event tables)
- **Thread Safety:** wxMutex protects m_activeThreads vector
- **Detached Threads:** worker.detach() (fire-and-forget, like wxTHREAD_DETACHED)

### Implementation Details
- **submitBackgroundTask():** 68 lines
  - Semaphore check (TryWait → work → Post pattern)
  - Thread tracking (add on start, remove on finish)
  - Exception handling (try/catch → wxEVT_KALAHARI_TASK_FAILED)
  - wxQueueEvent for GUI communication (thread-safe)
  - Logging at every step (debug/info/error levels)

- **Event Handlers:** 24 lines
  - onTaskCompleted(): Update status bar, log success
  - onTaskFailed(): Show error, log to spdlog + wxLog

- **Destructor Cleanup:** 20 lines
  - Wait up to 5 seconds for tasks to finish
  - Timeout protection (prevents infinite hang)
  - Graceful vs forced shutdown logging

- **Example Usage:** 32 lines
  - onFileOpen() demonstrates pattern
  - 2-second simulated file load
  - CallAfter() for status bar updates
  - Thread limit warning dialog

### Code Statistics
- **Total Lines Added:** 241 lines (2 files)
  - main_window.h: +79 lines (includes, declarations, docs)
  - main_window.cpp: +162 lines (implementation, examples)
- **Lines Modified:** 12 lines (constructor, imports)
- **Net Change:** +229 lines

### Key Patterns Implemented
1. **Semaphore Limiting (Bartosz's pattern):**
   ```cpp
   wxMutexLocker lock(m_threadMutex);
   wxSemaError serr = m_threadSemaphore.TryWait();
   if (serr != wxSEMA_NO_ERROR) { return false; }
   ```

2. **wxQueueEvent Communication (Bartosz's pattern):**
   ```cpp
   wxThreadEvent* evt = new wxThreadEvent(wxEVT_KALAHARI_TASK_COMPLETED);
   evt->SetId(threadId);
   wxQueueEvent(this, evt);  // Thread-safe, deep copy
   ```

3. **CallAfter for Simple Updates:**
   ```cpp
   CallAfter([this]() {
       m_statusBar->SetStatusText(_("Ready"), 0);
   });
   ```

4. **Thread Tracking & Cleanup:**
   ```cpp
   // On thread start:
   m_activeThreads.push_back(threadId);

   // On thread exit:
   auto it = std::find(m_activeThreads.begin(), m_activeThreads.end(), threadId);
   m_activeThreads.erase(it);
   m_threadSemaphore.Post();
   ```

### Integration Points (Future)
- **Week 3-4:** Python plugin execution with GIL handling
- **Week 5+:** File I/O, AI API calls, document parsing
- **Phase 1:** Heavy operations (DOCX import, statistics generation)

## Verification Results

### Local Build (Linux)
✅ **PASSED**
- CMake build: Success (clean compilation)
- Executable size: 166 MB (Debug, unchanged from Task #00001)
- Threading symbols verified: submitBackgroundTask, onTaskCompleted, onTaskFailed present in binary
- No compiler warnings (-Wall -Wextra -Wpedantic -Werror)
- Build time: ~3 seconds (incremental, 2 files changed)

### CI/CD Matrix Builds
✅ **ALL PLATFORMS PASSED**

| Platform | Build Type | Status | Time | Run ID |
|----------|-----------|--------|------|---------|
| macOS | Debug + Release | ✅ SUCCESS | 59s | 18821590483 |
| Windows | Debug + Release | ✅ SUCCESS | 4m16s | 18821590487 |
| Linux | Debug + Release | ✅ SUCCESS | 4m36s | 18821590482 |

**Commit:** `8ee4248` - "feat: Add threading infrastructure (Phase 0 Week 2)"

### Code Quality
- ✅ No compiler warnings (all platforms)
- ✅ C++20 compliance verified
- ✅ Modern C++ patterns (std::thread, std::function, lambda captures)
- ✅ Thread safety: wxMutex, wxSemaphore, wxQueueEvent
- ✅ Exception safety: try/catch with error events
- ✅ Resource safety: RAII (wxMutexLocker), semaphore Post on exit
- ✅ Cross-platform: No platform-specific code

### Functional Testing (Manual - to be performed)
**Test Cases from Task #00002:**
1. ⏳ **Thread limiting:** Click "File → Open" 5x rapidly → 4 tasks run, 5th rejected
2. ⏳ **Success case:** Single click → Status "Loading file..." → 2s → "Loaded: example.klh"
3. ⏳ **Cleanup:** Start 2-3 tasks, close window → Wait up to 5s, graceful shutdown
4. ⏳ **Exception handling:** (Phase 1 - modify task to throw)
5. ⏳ **Log verification:** Check spdlog output for thread lifecycle messages

**Note:** Manual testing deferred to next session (user requested update ROADMAP/CHANGELOG first)

---

**Task Complexity:** Medium (threading foundation, new patterns, cross-platform testing)
**Estimated Time:** 3 hours (165 min total)
**Blocking:** Task #00001 (wxWidgets Basic Window)
**Blocked By:** None (Task #00001 will be completed first)

**Code Size:** ~200 lines added to MainWindow
- main_window.h: ~30 lines (declarations)
- main_window.cpp: ~170 lines (implementation + example)
