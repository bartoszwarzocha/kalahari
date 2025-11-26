# Kalahari - GUI Design Specification

> **Writer's IDE** - Comprehensive GUI architecture and layout design

**Document Version:** 2.3 (QPalette Theme System)
**Status:** 🔄 IN PROGRESS (Qt conversion + theme system complete)
**Last Updated:** 2025-11-25
**Phase:** Phase 1 (Core Editor) 🔄 IN PROGRESS, Task #00025 (Log Panel Theming)

---

## Table of Contents

1. [Overview](#overview)
2. [Design Philosophy](#design-philosophy)
3. [Three-Column Layout Architecture](#three-column-layout-architecture)
4. [Panel Catalog](#panel-catalog)
5. [Command Registry System](#command-registry-system) ✅ Qt6
6. [Customizable Toolbars](#customizable-toolbars)
7. [Plugin Command Integration](#plugin-command-integration)
8. [Perspectives (Workspace Presets)](#perspectives-workspace-presets)
9. [Gamification System](#gamification-system)
10. [Info Bar & Status Bar](#info-bar--status-bar)
11. [Keyboard Shortcuts](#keyboard-shortcuts)
12. [Focus Modes](#focus-modes)
13. [Settings Dialog](#settings-dialog)
14. [Qt Implementation Notes](#qt-implementation-notes) ✅ Qt6
15. [QPalette Theme System](#qpalette-theme-system-task-00023) ✅ Qt6 (NEW)

---

## Overview

This document defines the complete GUI architecture for Kalahari, a professional Writer's IDE built with Qt6.

**Migration Status (2025-11-21):**
- ✅ **Command Registry System** (Section 5) - Fully migrated to Qt6 (Task #00013, 2025-11-21)
- ✅ **Qt Implementation Notes** (Section 14) - Qt6 QDockWidget, QSettings, Focus Modes
- ⏳ **Panel Catalog** (Section 4) - wxAUI code examples retained as design reference, will be rewritten with Qt6 during Phase 1 panel implementation
- ⏳ **Customizable Toolbars** (Section 6) - Will be updated during Phase 1 toolbar customization feature
- 📝 **Note:** Sections 3-4 contain high-level design concepts with wxWidgets code examples from original Phase 1 plan (pre-migration). These serve as design documentation and will be replaced with Qt6 implementations as features are built during Phase 1.

### Key Features

- **3-column dockable layout** (left tools | center workspace | right tools)
- **Qt QDockWidget framework** - fully customizable, drag & drop panels
- **Customizable toolbars** - user-defined command shortcuts (QToolBar)
- **Command Registry** - unified command system (core + plugins)
- **4 built-in perspectives** (Writer, Editor, Researcher, Planner) - QSettings-based
- **Plugin panels** - extensions can add their own dockable panels
- **Gamification** - badges, challenges, statistics
- **3 focus modes** (Normal, Focused, Distraction-free)
- **Context-sensitive UI** - panels adapt to current document/selection

### Design Goals

1. **Professional** - IDE-grade UI (Visual Studio, IntelliJ quality)
2. **Writer-centric** - optimized for book authoring workflow
3. **Flexible** - supports diverse writing styles (linear, research-heavy, perfectionist)
4. **Extensible** - plugins can extend UI seamlessly
5. **Motivating** - gamification encourages consistency
6. **Non-intrusive** - focus modes eliminate distractions
7. **Accessible** - keyboard shortcuts, screen reader support, high contrast themes

---

## Design Philosophy

### 1. "Everything in one place, but not in your face"

```
Default view: Essential panels visible (Files, Editor, Assistant)
Hidden panels: Advanced tools available but not cluttering UI
User chooses: What to show based on current task
```

### 2. "IDE for writers, not developers"

```
Visual: Large icons, readable fonts, generous spacing
Language: "Chapter" not "File", "Characters" not "Objects"
Workflow: Writing-first (code editors are coding-first)
```

### 3. "Perspectives over modes"

```
BAD: "Writing Mode" vs "Editing Mode" (binary, limiting)
GOOD: "Writer Perspective" (focused) + "Editor Perspective" (analytical)
      → User can switch instantly, customize each
```

### 4. "Plugins are first-class citizens"

```
Plugin command = Core command (same treatment)
Plugin panel = Core panel (same docking, perspectives)
Plugin toolbar = Core toolbar (same customization)
```

### 5. "Gamification without fatigue"

```
Badges: Celebrate real achievements (not spam)
Challenges: Opt-in (user enables if motivated)
Statistics: Always visible but not obtrusive
Progress: Visual (progress bars, charts) not just numbers
```

---

## Three-Column Layout Architecture

### Overall Structure

```
┌─────────────────────────────────────────────────────────────────┐
│ Title Bar: Kalahari 1.0 - [Project Name]                 [_][□][X]│
├─────────────────────────────────────────────────────────────────┤
│ Menu Bar: [File] [Edit] [View] [Insert] [Format] [Tools] [...] │
├─────────────────────────────────────────────────────────────────┤
│ Quick Access Toolbar: [💾] [⎌] [↩] [↪] [📊] [▼]               │
├─────────────────────────────────────────────────────────────────┤
│ Toolbar: Standard                                                │
│ [New] [Open] [Save] | [Undo] [Redo] | [Cut] [Copy] [Paste] ... │
├─────────────────────────────────────────────────────────────────┤
│ Toolbar: Format (optional, can be hidden)                        │
│ [B] [I] [U] [S] | [Font ▼] [Size ▼] | [Style ▼] | [Color] ... │
├───────────────┬─────────────────────────────────┬───────────────┤
│               │                                 │               │
│  LEFT COLUMN  │      CENTER WORKSPACE           │ RIGHT COLUMN  │
│   (Tools)     │        (Main Area)              │    (Info)     │
│               │                                 │               │
│  [Files/Libs] │  ┌─────────────────────────┐   │ [Stats/       │
│  ┌─────────┐  │  │ Daily Statistics        │   │  Challenges]  │
│  │Files    │  │  │ ▓▓▓▓▓░░░░ 850/1000 wds │   │ ┌───────────┐ │
│  │         │  │  └─────────────────────────┘   │ │📊 Today   │ │
│  │ • Ch 1  │  │                                 │ │ 850 words │ │
│  │ • Ch 2  │  │  ┌─────────────────────────┐   │ │🏆 Streak  │ │
│  │ • Ch 3  │  │  │ Editor Tabs             │   │ │ 7 days    │ │
│  │         │  │  │ [Chapter 1] [Biblio] ...│   │ └───────────┘ │
│  └─────────┘  │  ├─────────────────────────┤   │               │
│               │  │                         │   │ [Character    │
│  [Assistant]  │  │  Main editor area       │   │  Preview]     │
│  ┌─────────┐  │  │  (wxRichTextCtrl)       │   │ ┌───────────┐ │
│  │  🦁      │  │  │                         │   │ │ John Doe  │ │
│  │  "Keep   │  │  │  Lorem ipsum dolor...   │   │ │ Age: 42   │ │
│  │  going!" │  │  │                         │   │ │ Detective │ │
│  └─────────┘  │  │                         │   │ │ [Open]    │ │
│               │  └─────────────────────────┘   │ └───────────┘ │
│               │                                 │               │
│               │                                 │ [Calendar]    │
│               │                                 │ ┌───────────┐ │
│               │                                 │ │ Oct 25    │ │
│               │                                 │ │ Goal: 1000│ │
│               │                                 │ └───────────┘ │
├───────────────┴─────────────────────────────────┴───────────────┤
│ Info Bar: ℹ️ Auto-save completed at 14:23                     │
├─────────────────────────────────────────────────────────────────┤
│ Status: [1,234 words] [5,678 chars] [6min read] [45min] [🔥7] │
└─────────────────────────────────────────────────────────────────┘
```

### Column Proportions (Default)

```
Left column:   20-25% (250-350px, min 200px)
Center column: 50-60% (flexible, main area)
Right column:  20-25% (250-350px, min 200px)

User can:
- Resize by dragging splitters
- Collapse/expand columns (double-click splitter)
- Float panels as separate windows
- Stack panels as tabs (drag & drop)
```

### wxAUI Docking Zones

```cpp
// wxAUI supports 5 docking zones:

wxAUI_DOCK_LEFT     // Left column
wxAUI_DOCK_RIGHT    // Right column
wxAUI_DOCK_TOP      // Above center (e.g., toolbars, daily stats)
wxAUI_DOCK_BOTTOM   // Below center (e.g., output panel)
wxAUI_DOCK_CENTER   // Center workspace (tabbed documents)

// Plus:
wxAUI_DOCK_FLOAT    // Floating window (detached panel)
```

---

## Panel Catalog

This section lists all available panels, grouped by default location.

### LEFT Column Panels

#### 1. Navigator Panel (default: visible)

**Type:** QTreeWidget (Qt6)
**Position:** Top of left column
**Default size:** 250px wide, 50% of left column height

**Content:**
- Tree structure: Project → Book structure → Libraries
- **Book structure:**
  - Front Matter
  - Body (Parts → Chapters)
  - Back Matter
- **Libraries section:**
  - Characters, Locations, Items
  - Mind Maps (multiple .kmap files) - NEW
  - Timelines (multiple .ktl files) - NEW
  - Resources
- Icons: Dynamic (📄 chapter, 👤 character, 📍 location, 🗺️ mind map, 📅 timeline)
- Double-click: Opens in editor (center workspace)
- Right-click context menu:
  - New Chapter/Character/Location/Mind Map/Timeline
  - Rename (F2)
  - Delete (Del)
  - Move Up/Down
  - Properties...
- Drag & drop: Reorder chapters/sections

**Implementation:**
```cpp
class NavigatorPanel : public QWidget {
    QTreeWidget* m_treeWidget;

public:
    void loadDocument(const core::Document& document);
    void clearDocument();

signals:
    void chapterDoubleClicked(const QString& chapterTitle);
    void libraryItemDoubleClicked(const QString& itemId);
};
```

---

#### 2. Assistant Panel (default: visible)

**Type:** Custom panel with graphical assistant
**Position:** Bottom of left column
**Default size:** 250px wide, 50% of left column height (flexible)

**Components:**
- Animal image (realistic, 6-8 moods per animal)
  - Happy, Thinking, Encouraging, Warning, Celebrating, Sleeping
- Speech bubble (wxHtmlWindow or custom drawing)
- Compact mode toggle button (text-only, hides image)
- Settings button (choose animal, configure triggers)

**Moods & Triggers:**

| Mood | When | Example Message |
|------|------|----------------|
| Happy | Session start | "Ready to write! Let's create something amazing today." |
| Thinking | User paused 30s | "Taking a moment to think? That's part of the creative process." |
| Encouraging | Reached 50% of daily goal | "You're halfway there! Keep the momentum going!" |
| Warning | 1 hour without break | "Time for a break! Remember the 20-20-20 rule." |
| Celebrating | Daily goal reached | "🎉 You did it! 1000 words today. Amazing work!" |
| Sleeping | No activity 5+ min | "Zzz... I'll be here when you're ready to continue." |

**Flow State Detection:**
- If user writes continuously for 10+ minutes, assistant goes silent
- Only shows urgent messages (auto-save errors, critical warnings)

**Implementation:**
```cpp
class AssistantPanel : public wxPanel {
public:
    enum Mood {
        HAPPY, THINKING, ENCOURAGING,
        WARNING, CELEBRATING, SLEEPING
    };

    void SetAnimal(AnimalType animal);  // Lion, Meerkat, Elephant, Cheetah
    void SetMood(Mood mood);
    void ShowMessage(const wxString& msg, Mood mood = HAPPY);
    void QueueMessage(const wxString& msg, Mood mood, int delayMs);

    void SetCompactMode(bool compact);  // Hide image, text only

private:
    wxStaticBitmap* m_animalImage;
    wxHtmlWindow* m_speechBubble;
    wxTimer m_messageTimer;

    std::queue<std::pair<wxString, Mood>> m_messageQueue;
    bool m_flowStateActive = false;
};
```

---

### CENTER Workspace Panels

#### 3. Statistics Bar (default: visible, always on top)

**Type:** Custom QWidget with 4 layers
**Position:** Top of central window, above editor tabs
**Purpose:** Live writing monitoring (Task Manager style)

**4-Layer Architecture:**
```
┌─────────────────────────────────────────────────────────────┐
│ Layer 1: Time Grid (Hours: 8:00, 9:00, 10:00, ..., 18:00)  │
│ Layer 2: Daily Graph (Real-time word count bars/line)      │
│ Layer 3: Weekly/Monthly Trend (Mini sparkline)             │
│ Layer 4: Text Info (850 words | 45 min | 🔥 7 days)       │
└─────────────────────────────────────────────────────────────┘
```

**Features:**
- **Real-time updates:** Word count graph updates as you type
- **Time-based visualization:** Shows writing activity throughout the day
- **Live monitoring:** Similar to Windows Task Manager CPU graph
- **Persistent:** Always visible, cannot be closed
- **Minimizable:** Can be collapsed to single-line text mode

**Implementation:**
```cpp
class StatisticsBar : public QWidget {
public:
    void updateWordCount(int words);
    void updateSessionTime(int minutes);
    void updateStreak(int days);
    void setMinimized(bool minimized);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawTimeGrid(QPainter& painter);
    void drawDailyGraph(QPainter& painter);
    void drawWeeklyTrend(QPainter& painter);
    void drawTextInfo(QPainter& painter);

    std::vector<int> m_hourlyWordCounts;  // 24 hours
    std::vector<int> m_weeklyData;        // 7 days
    int m_currentWords = 0;
    int m_sessionMinutes = 0;
    int m_streak = 0;
    bool m_minimized = false;
};
```

**See also:** ROADMAP.md Section 1.5 (Statistics Architecture)

---

#### 4. Timeline Panel (default: hidden, optional)

**Type:** Horizontal panel
**Position:** `wxAUI_DOCK_TOP` (center region, below Daily Stats)

**Content:**
- Horizontal timeline with events/milestones
- Zoom controls: Day | Week | Month | Year | All
- Current position marker (red line)
- Event markers (draggable)
- Context-sensitive: Shows timeline of current character/location/document

**Example Timeline View:**
```
┌────────────────────────────────────────────────────┐
│ [Day] [Week] [Month] [Year] [All]  Zoom: [- 100% +]│
├────────────────────────────────────────────────────┤
│         2020        2021        2022        2023   │
│          |           |           |           |     │
│          ●───────────●───────────────●───────●     │
│       Meeting    Marriage      Accident   Today   │
│       Sarah      John+Sarah    Car crash           │
└────────────────────────────────────────────────────┘
```

**When visible:**
- View → Timeline (F12)
- Editing character/location with timeline data
- Clicking timeline button in toolbar

**Implementation:**
```cpp
class TimelinePanel : public wxPanel {
public:
    enum ZoomLevel { DAY, WEEK, MONTH, YEAR, ALL };

    void SetTimeline(const Timeline& timeline);
    void AddEvent(const TimelineEvent& event);
    void RemoveEvent(int eventId);
    void SetZoom(ZoomLevel zoom);

private:
    wxScrolledWindow* m_canvas;
    std::vector<TimelineEvent> m_events;
    ZoomLevel m_zoomLevel = YEAR;
    wxDateTime m_currentPosition;

    void OnPaint(wxPaintEvent& event);
    void OnEventDrag(wxMouseEvent& event);
};
```

---

#### 5. Editor Notebook (default: visible, main area)

**Type:** `wxAuiNotebook` with custom tab rendering
**Position:** `wxAUI_DOCK_CENTER`

**Tab Types:**
- **Chapter Editor** - wxRichTextCtrl (RTF editing)
- **Markdown Editor** - wxStyledTextCtrl (Markdown with preview)
- **Library Editor** - Custom panel (Character/Location/Item cards)
- **Mind Map** - Plugin panel (visual outline)
- **Gantt Chart** - Plugin panel (project timeline)
- **Media Preview** - Image/PDF viewer

**Tab Features:**
- Close buttons (×) on each tab
- Unsaved indicator (• prefix on tab label)
- Dynamic icons (📄 chapter, 👤 character, 📍 location, 🗺️ mind map)
- Reorderable (drag & drop)
- Detachable (drag out to float)

**Example Tab Bar:**
```
[• Chapter 1 ×] [Bibliography ×] [John Doe ×] [Mind Map ×] [+]
```

**Context Menu (right-click tab):**
- Close
- Close Others
- Close All
- Close to the Right
- ---
- Detach (float as separate window)
- ---
- Copy Full Path

**Implementation:**
```cpp
class EditorNotebook : public wxAuiNotebook {
public:
    enum DocumentType {
        CHAPTER_RTF, CHAPTER_MD, CHARACTER,
        LOCATION, ITEM, MINDMAP, GANTT, MEDIA
    };

    wxWindow* CreateEditor(DocumentType type, Document* doc);
    void OpenDocument(Document* doc);
    void CloseDocument(Document* doc);
    void SaveDocument(Document* doc);

    void MarkDirty(int tabIndex);
    void MarkClean(int tabIndex);
    bool HasUnsavedChanges();

private:
    std::map<int, Document*> m_tabToDocument;
    std::map<Document*, int> m_documentToTab;
};
```

---

### RIGHT Column Panels

#### 6. Weekly Statistics Panel (default: visible, dockable)

**Type:** Custom QWidget
**Position:** Top of right column
**Default size:** 250px wide, 40% of right column height
**Purpose:** Quick analytics (weekly/monthly aggregated stats)

**Content:**
```
┌─────────────────────────┐
│ 📊 WEEKLY STATISTICS    │
├─────────────────────────┤
│ This Week:              │
│ ▓▓▓▓▓▓░░░ 5,200/7,000  │
│                         │
│ Daily Breakdown:        │
│  Mo  850 ▇▇▇▇▇         │
│  Tu 1200 ████████      │
│  We  750 ▇▇▇▇          │
│  Th  920 ▇▇▇▇▇         │
│  Fr 1480 ████████████  │
│  Sa    0               │
│  Su    0               │
│                         │
│ 🔥 STREAK: 5 days      │
│ 📈 Avg: 1,040 wds/day  │
│                         │
│ 🎯 ACTIVE CHALLENGES    │
│ ┌─────────────────────┐ │
│ │ 5K This Week        │ │
│ │ ▓▓▓▓▓░░ 5,200/5,000 │ │
│ │ ✅ COMPLETED!       │ │
│ └─────────────────────┘ │
│                         │
│ 🏆 RECENT BADGES        │
│ [🌅] [🏃] [📝] [💪]    │
│                         │
│ [View All Achievements] │
│ [Advanced Analytics...] │
└─────────────────────────┘
```

**Click Interactions:**
- Click challenge → Shows detailed progress
- Click badge → Shows badge description & date earned
- Click "Advanced Analytics" → Opens Advanced Analytics in center window (Premium $14)
- Click "View All" → Opens achievements panel in center

**Implementation:**
```cpp
class WeeklyStatisticsPanel : public QWidget {
public:
    void updateWeeklyStats(const std::vector<int>& dailyWords);
    void updateStreak(int days);
    void updateAverage(int wordsPerDay);
    void addChallenge(const Challenge& challenge);
    void updateChallengeProgress(int challengeId, int current, int total);
    void showBadge(const Badge& badge);

private:
    QLabel* m_weeklyProgressLabel;
    QProgressBar* m_weeklyProgressBar;
    QWidget* m_dailyBreakdownWidget;
    QWidget* m_challengesWidget;
    QGridLayout* m_badgeGrid;  // Recent badges (max 8)
};
```

**See also:**
- ROADMAP.md Section 1.5 (Statistics Architecture - 3-Tier System)
- Statistics Bar (live monitoring, always visible)
- Advanced Analytics (central window, Premium plugin $14)

---

#### 7. Properties Panel (default: visible, context-sensitive)

**Type:** Custom QWidget
**Position:** Middle of right column
**Purpose:** Show properties of selected item (chapter, character, location, etc.)

**Content:**
```
┌─────────────────────────┐
│ 📋 PROPERTIES           │
├─────────────────────────┤
│ [Content depends on     │
│  selected item]         │
│                         │
│ Chapter Properties:     │
│  - Title                │
│  - Word count           │
│  - Status (Draft/Final) │
│  - Tags                 │
│                         │
│ Character Properties:   │
│  - Name                 │
│  - Age                  │
│  - Role                 │
│  - Traits               │
│  - Brief description    │
│                         │
│ [Edit Properties...]    │
└─────────────────────────┘
```

**When visible:**
- User selects chapter in Navigator
- User clicks character/location/item in library
- Auto-show when configured in settings

**Implementation:**
```cpp
class PropertiesPanel : public QWidget {
public:
    enum ItemType { CHAPTER, CHARACTER, LOCATION, ITEM, MINDMAP, TIMELINE, NONE };

    void showProperties(ItemType type, const QVariant& itemData);
    void clear();

private:
    QStackedWidget* m_stackedWidget;  // Different layouts for different types
    ChapterPropertiesWidget* m_chapterWidget;
    CharacterPropertiesWidget* m_characterWidget;
    LocationPropertiesWidget* m_locationWidget;
    // ... other type-specific widgets
};
```

---

#### 8. Calendar Panel (default: visible)

**Type:** Custom panel with `wxCalendarCtrl`
**Position:** Bottom of right column
**Default size:** 250px wide, 30% of right column height

**Content:**
```
┌─────────────────────────┐
│ 📅 OCTOBER 2025         │
├─────────────────────────┤
│ Mo Tu We Th Fr Sa Su    │
│        1  2  3  4  5    │
│  6  7  8  9 10 11 12    │
│ 13 14 15 16 17 18 19    │
│ 20 21 22 23 24 25 26    │
│ 27 28 29 30 31          │
│                         │
│ Today: Oct 25           │
│ Goal: 1,000 words       │
│                         │
│ Deadlines:              │
│ • Oct 31 - Ch 5 draft   │
│ • Nov 15 - Full draft   │
└─────────────────────────┘
```

**Features:**
- Today highlighted (bold border)
- Deadlines marked (colored dots on dates)
- Hover tooltip shows deadline details
- Click date → Filter events for that day
- Double-click → Open full Schedule in center

**Implementation:**
```cpp
class CalendarPanel : public wxPanel {
public:
    void SetDeadlines(const std::vector<Deadline>& deadlines);
    void AddDeadline(const wxDateTime& date, const wxString& description);
    void SetDailyGoal(int wordCount);

private:
    wxCalendarCtrl* m_calendar;
    wxStaticText* m_todayInfo;
    wxListBox* m_deadlinesList;

    std::map<wxDateTime, std::vector<Deadline>> m_deadlines;

    void OnDateSelected(wxCalendarEvent& event);
    void OnDateDoubleClick(wxCalendarEvent& event);
};
```

---

### BOTTOM Panels (hidden by default)

#### 9. Output/Log Panel (default: hidden)

**Type:** `wxTextCtrl` (read-only, multiline, monospace)
**Position:** `wxAUI_DOCK_BOTTOM`

**Purpose:**
- Plugin debug messages
- Export operation logs
- System messages
- Error messages

**Content Example:**
```
[14:23:15 INFO] Auto-save completed: Chapter 1
[14:25:32 INFO] Plugin loaded: AI Assistant Pro v1.2
[14:27:41 WARN] Spell check: 12 potential issues found
[14:30:00 INFO] Exporting to DOCX...
[14:30:05 INFO] Export completed: output.docx (342 KB)
[14:32:19 ERROR] Plugin error: AI Assistant - API key not configured
```

**Features:**
- Filterable by level (Debug, Info, Warn, Error)
- Copy selected text
- Clear log
- Save log to file
- Auto-scroll (optional)

**When visible:**
- View → Output Panel (Ctrl+Shift+O)
- Error occurs (auto-show)
- Plugin development mode

**Implementation:**
```cpp
class OutputPanel : public wxPanel {
public:
    enum LogLevel { DEBUG, INFO, WARN, ERROR };

    void Log(const wxString& message, LogLevel level = INFO);
    void Clear();
    void SetFilter(LogLevel minLevel);
    void SaveToFile(const wxString& path);

private:
    wxTextCtrl* m_logText;
    wxChoice* m_filterChoice;
    LogLevel m_minLevel = INFO;

    wxString FormatLogEntry(const wxString& msg, LogLevel level);
};
```

---

### FLOATING Panels (plugin-defined)

Plugins can create custom panels that integrate seamlessly:

**Properties:**
- Dockable in any zone (left, right, top, bottom, center)
- Floatable as separate windows
- Resizable, closable
- Saved in perspectives
- Hideable via View menu

**Example Plugin Panels:**
- **Mind Map Editor** (visualization plugin)
- **AI Chat Interface** (AI Assistant Pro plugin)
- **Research Sources** (Research Pro plugin)
- **Version History** (Git integration plugin)
- **Export Preview** (Export Suite plugin)

**Plugin API:**
```python
# Python plugin creating custom panel

class MyPlugin:
    def on_load(self, api):
        # Create custom panel
        panel = api.panels.create(
            name="My Tool",
            position="right",  # left, right, top, bottom, center, float
            default_visible=False,
            min_size=(200, 300),
            content=self.create_panel_content()
        )

        # Register panel with View menu
        api.menus.add_panel_toggle(
            menu="View",
            label="My Tool",
            panel_id=panel.id
        )
```

---

## Command Registry System

The Command Registry is a centralized system that manages all commands (actions) in Kalahari. It enables customizable toolbars, keyboard shortcuts, and plugin integration.

### Design Goals

1. **Unified command system** - All actions (core + plugins) registered in one place
2. **Discoverable** - Users can browse all available commands
3. **Customizable** - Users can assign shortcuts, add to toolbars
4. **Context-sensitive** - Commands can enable/disable based on application state
5. **Plugin-friendly** - Plugins register commands like core features

---

### Command Structure

```cpp
namespace kalahari {
namespace commands {

struct Command {
    // Identification
    std::string id;              // Unique: "file.save", "plugin.ai.suggest"
    std::string label;           // Display text: "Save"
    std::string tooltip;         // "Save current document to disk"
    std::string category;        // "File", "Edit", "Plugin: AI Assistant"

    // Visual
    IconSet icons;               // 16x16, 24x24, 32x32 (all sizes)
    bool showInMenu = true;      // Appears in menus
    bool showInToolbar = false;  // Appears in default toolbar

    // Keyboard
    KeyboardShortcut shortcut;   // e.g., Ctrl+S (optional)
    bool isShortcutCustomizable = true;

    // Execution
    std::function<void()> execute;       // The actual action
    std::function<bool()> isEnabled;     // Context check (can execute now?)
    std::function<bool()> isChecked;     // For toggle commands (optional)

    // Metadata
    bool isPluginCommand = false;
    std::string pluginId;        // If plugin command, which plugin?
    int apiVersion = 1;          // For future compatibility
};

// Icon set supporting multiple sizes
struct IconSet {
    wxBitmap icon16;  // 16x16 - menus
    wxBitmap icon24;  // 24x24 - default toolbars
    wxBitmap icon32;  // 32x32 - large toolbars

    IconSet() = default;
    IconSet(const wxString& path);  // Load from single SVG (auto-scale)
};

// Keyboard shortcut
struct KeyboardShortcut {
    int keyCode;           // wxKeyCode (e.g., 'S', WXK_F1)
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    wxString ToString() const;  // "Ctrl+S", "Ctrl+Shift+A"
    static KeyboardShortcut FromString(const wxString& str);
};

} // namespace commands
} // namespace kalahari
```

---

### Command Registry Class

```cpp
class CommandRegistry {
public:
    static CommandRegistry& Instance();  // Singleton

    // Registration (Phase 0 - core commands)
    void RegisterCoreCommands();

    // Registration (Phase 2+ - plugins)
    void RegisterCommand(const Command& cmd);
    void UnregisterCommand(const std::string& id);
    bool IsCommandRegistered(const std::string& id);

    // Query
    Command* GetCommand(const std::string& id);
    std::vector<Command*> GetCommandsByCategory(const std::string& category);
    std::vector<Command*> GetAllCommands();
    std::vector<std::string> GetCategories();

    // Execution
    bool ExecuteCommand(const std::string& id);
    bool CanExecuteCommand(const std::string& id);  // Check isEnabled

    // Keyboard shortcuts
    bool ExecuteShortcut(const KeyboardShortcut& shortcut);
    void SetShortcut(const std::string& cmdId, const KeyboardShortcut& sc);
    void RemoveShortcut(const std::string& cmdId);
    KeyboardShortcut GetShortcut(const std::string& cmdId);

    // Check for conflicts
    bool IsShortcutInUse(const KeyboardShortcut& sc);
    std::string GetCommandByShortcut(const KeyboardShortcut& sc);

    // Persistence
    void SaveShortcuts(const wxString& path);
    void LoadShortcuts(const wxString& path);
    void ResetToDefaults();

private:
    CommandRegistry() = default;

    std::unordered_map<std::string, Command> m_commands;
    std::unordered_map<KeyboardShortcut, std::string> m_shortcuts;
    std::map<std::string, std::vector<std::string>> m_categorizedCommands;
};
```

---

### Core Commands List

**Total core commands:** ~80-100

| Category | Count | Examples |
|----------|-------|----------|
| **File** | 15 | New, Open, Save, Close, Import, Export, Print |
| **Edit** | 20 | Undo, Redo, Cut, Copy, Paste, Find, Replace |
| **View** | 15 | Focus modes, panels toggle, perspectives |
| **Insert** | 10 | Chapter, Section, Image, Table, Link |
| **Format** | 15 | Bold, Italic, Styles, Font, Size, Alignment |
| **Tools** | 10 | Word count, Spell check, Thesaurus |
| **Window** | 5 | Next tab, Previous tab, Close tab |
| **Help** | 5 | User manual, Tutorials, About |

**Plugin commands:** ~50-100 (added dynamically)

---

### Context Sensitivity Examples

```cpp
// File.Save - enabled only if document is open and dirty
.isEnabled = []() {
    auto* doc = MainFrame::Instance()->GetActiveDocument();
    return doc != nullptr && doc->IsModified();
}

// Edit.Undo - enabled only if undo stack not empty
.isEnabled = []() {
    auto* editor = MainFrame::Instance()->GetActiveEditor();
    return editor != nullptr && editor->CanUndo();
}

// Format.Bold - checked if selection is bold
.isChecked = []() {
    auto* editor = MainFrame::Instance()->GetActiveEditor();
    return editor != nullptr && editor->IsSelectionBold();
}
```

---

### Persistence (JSON Format)

```json
// ~/.kalahari/config/commands.json
{
  "version": "1.0",
  "keyboard_shortcuts": {
    "file.save": "Ctrl+S",
    "edit.undo": "Ctrl+Z",
    "plugin.ai_assistant.suggest": "Ctrl+Shift+A"
  }
}
```

---

## Customizable Toolbars

One of Kalahari's most powerful features is fully customizable toolbars. Users can create, modify, and organize toolbars to match their personal workflow.

### Design Philosophy

**"Every writer works differently"**
- Linear writer needs: [New Chapter] [Save] [Word Count] [Export]
- Research-heavy writer needs: [Sources] [Web Clip] [Characters] [Timeline]
- Perfectionist writer needs: [Thesaurus] [Spell Check] [AI Rewrite] [Compare]

**Solution:** Let users build their own perfect toolbar!

---

### Default Toolbars (6 built-in)

#### 1. Standard Toolbar
**Commands:** `[New] [Open] [Save] | [Undo] [Redo] | [Cut] [Copy] [Paste]`

#### 2. Format Toolbar
**Commands:** `[B] [I] [U] [S] | [Font ▼] [Size ▼] | [Style ▼] | [Color]`

#### 3. Edit Toolbar (hidden by default)
**Commands:** `[Find] [Replace] | [Spell Check] | [Word Count]`

#### 4. View Toolbar (hidden by default)
**Commands:** `[Focus Mode] [Fullscreen] | [Perspectives ▼]`

#### 5. Tools Toolbar (hidden by default)
**Commands:** `[Characters] [Locations] [Timeline] | [Statistics]`

#### 6. Plugin Toolbar (dynamic)
**Commands:** Added by plugins (e.g., `[AI Suggest] [Mind Map] [Export Suite]`)

---

### Toolbar Manager Architecture

```cpp
class ToolbarManager {
public:
    static ToolbarManager& Instance();

    // Toolbar lifecycle
    wxAuiToolBar* CreateToolbar(const wxString& name);
    wxAuiToolBar* GetToolbar(const wxString& name);

    // Configuration
    void AddCommandToToolbar(const wxString& toolbarName, const wxString& commandId);
    void RemoveCommandFromToolbar(const wxString& toolbarName, int position);
    void InsertSeparator(const wxString& toolbarName, int position);

    // Visibility & properties
    void ShowToolbar(const wxString& name, bool show = true);
    void SetToolbarIconSize(const wxString& name, int size);  // 16, 24, 32
    void SetToolbarStyle(const wxString& name, ToolbarStyle style);

    // Persistence
    void SaveToolbars(const wxString& path);
    void LoadToolbars(const wxString& path);

    // Profiles
    void SaveProfile(const wxString& profileName);
    void LoadProfile(const wxString& profileName);

private:
    std::map<wxString, wxAuiToolBar*> m_toolbars;
    std::map<wxString, ToolbarConfig> m_configs;
};

enum class ToolbarStyle {
    ICON_ONLY,
    TEXT_ONLY,
    ICON_AND_TEXT
};

struct ToolbarConfig {
    wxString name;
    bool visible;
    int iconSize;
    ToolbarStyle style;
    std::vector<wxString> commandIds;
};
```

---

### Customization UI

**Access:** Tools → Customize Toolbars (Alt+Shift+T)

```
┌─────────────────────────────────────────────────────────────┐
│ Customize Toolbars                                    [X]   │
├─────────────────────────────────────────────────────────────┤
│ [Toolbars] [Commands] [Keyboard] [Options]                 │
├─────────────────────────────────────────────────────────────┤
│ TOOLBARS:                       [New...] [Rename...] [Del] │
│ ☑ Standard                      [Reset to Default]          │
│ ☑ Format                        [Import...] [Export...]     │
│ ☐ Edit                                                       │
│                                                              │
│ SELECTED: Standard                                           │
│ ┌────────────────────────────────────────────────────┐      │
│ │ [New] [Open] [Save] [--] [Undo] [Redo]           │      │
│ └────────────────────────────────────────────────────┘      │
│                                                              │
│ [<< Remove]  [↑ Up]  [↓ Down]  [Add >>]  [Separator]       │
│                                                              │
│ Icon size: (•) 24px  ( ) 32px     Style: (•) Icon+Text     │
│                                                              │
├─────────────────────────────────────────────────────────────┤
│                    [OK]  [Cancel]  [Apply]                  │
└─────────────────────────────────────────────────────────────┘
```

---

### Quick Access Toolbar (QAT)

**Concept:** Small toolbar above menu bar (always visible)

```
┌──────────────────────────────────────────────────┐
│ [💾] [⎌] [↩] [↪] [📊] │ [▼]                     │ ← QAT
├──────────────────────────────────────────────────┤
│ File  Edit  View  Insert  Format  Tools  ...    │ ← Menu
└──────────────────────────────────────────────────┘
```

**Default:** [Save] [Undo] [Redo] [Word Count] [▼ More]

**Customization:** Right-click QAT → Customize, or drag commands to QAT

---

### Live Customization Mode

**Activation:** View → Enter Customize Mode (Alt+Shift+C)

**Features:**
- Drag & drop buttons between toolbars
- Delete buttons (drag to trash or press Del)
- Insert separators (right-click → Insert Separator)
- Resize toolbars
- Exit with ESC

---

### Toolbar Profiles

**5 Built-in Profiles:**

1. **Kalahari Default** - Balanced (Standard + Format visible)
2. **Minimal** - Distraction-free (only QAT)
3. **Power Writer** - All toolbars visible
4. **Classic** - Word/LibreOffice-like
5. **Writer's Workshop** - Research-heavy workflow

**Switching:** View → Toolbar Profile → [Select]

**Custom profiles:** Save current layout as new profile, export/import `.klh-toolbar` files

---

### Command Palette (Phase 3)

**Activation:** Ctrl+Shift+P

```
┌─────────────────────────────────────────────────┐
│ > type to search commands...___                 │
├─────────────────────────────────────────────────┤
│ 📄 Save                              Ctrl+S     │
│ 🤖 AI Suggest                        Ctrl+Sh+A  │
│ ✏️ Bold                              Ctrl+B     │
└─────────────────────────────────────────────────┘
```

**Features:** Fuzzy search, keyboard navigation, shows shortcuts, add to toolbar (Ctrl+T)

---

## Plugin Command Integration

Plugins can register commands that integrate seamlessly with the command system.

### Python Plugin API

```python
# Plugin registering commands

class MyPlugin:
    def on_load(self, api):
        # Register command
        api.commands.register(
            id="my_plugin.awesome_feature",
            label=_("Awesome Feature"),
            tooltip=_("Does something awesome"),
            category="My Plugin",
            icon=self.load_icon("awesome.svg"),
            shortcut="Ctrl+Shift+A",  # Suggested shortcut
            action=self.awesome_feature,
            enabled=lambda: api.editor.has_text()
        )

        # Add to default toolbar (optional)
        api.toolbars.add_command(
            toolbar="Plugin",
            command_id="my_plugin.awesome_feature"
        )

        # Add to menu (optional)
        api.menus.add_command(
            menu="Tools",
            submenu="My Plugin",
            command_id="my_plugin.awesome_feature"
        )

    def on_unload(self, api):
        # Commands automatically unregistered
        pass

    def awesome_feature(self):
        # Implementation
        text = api.editor.get_selected_text()
        result = self.process(text)
        api.editor.replace_selection(result)
```

### Plugin Command Lifecycle

```
Plugin Load:
1. Plugin calls api.commands.register()
2. Command added to CommandRegistry
3. Command appears in:
   - Tools → Customize Toolbars → Commands list
   - Command Palette (Ctrl+Shift+P)
   - Keyboard Shortcuts customization
   - Plugin toolbar (if specified)

Plugin Unload:
1. All plugin commands automatically unregistered
2. Removed from toolbars
3. Removed from menus
4. Keyboard shortcuts cleared
```

---

## Perspectives (Workspace Presets)

Perspectives save the entire window layout: panel positions, visibility, toolbar configuration.

### 4 Default Perspectives

#### 1. WRITER (Focused Writing)

```
┌──────────────┬───────────────────────────┬──────┐
│ Files        │ Editor (maximized)        │ Asst │
│ (minimized)  │                           │      │
│              │                           │      │
│              │                           │      │
│              │ Daily stats (minimized)   │      │
└──────────────┴───────────────────────────┴──────┘
```

**Visible panels:**
- Files/Libraries (left, narrow, 15%)
- Editor (center, 70%)
- Assistant (right, narrow, 15%)
- Daily Statistics (top, minimized)

**Hidden panels:**
- Statistics/Challenges, Calendar, Context Preview, Timeline

**Toolbars:** Standard only, QAT visible

**Use case:** Flow state writing, minimal distractions

---

#### 2. EDITOR (Revision Mode)

```
┌──────────┬─────────────────────┬──────────────┐
│ Files/   │ Editor + Timeline   │ Stats        │
│ Libs     │                     │ Context      │
│          │                     │ Calendar     │
│ Assistant│                     │              │
└──────────┴─────────────────────┴──────────────┘
```

**Visible panels:**
- All panels visible
- Timeline panel (horizontal, below Daily Stats)
- Context Preview (auto-show on selection)

**Toolbars:** Standard + Format + Tools

**Use case:** Editing, fact-checking, consistency review

---

#### 3. RESEARCHER (Information Gathering)

```
┌──────────────┬──────────────────┬──────────────┐
│ Libraries    │ Multiple tabs:   │ Calendar     │
│ (prominent)  │ - Research notes │ Context      │
│              │ - Web sources    │ Preview      │
│ Files        │ - Mind map       │              │
│ (minimized)  │ - Bibliography   │              │
└──────────────┴──────────────────┴──────────────┘
```

**Visible panels:**
- Libraries (left, large, 30%)
- Multiple document tabs (research materials)
- Calendar + Context Preview (right)

**Toolbars:** Standard + Tools + Plugin

**Use case:** Research phase, gathering materials, outlining

---

#### 4. PLANNER (Project Management)

```
┌──────────┬─────────────────────────┬──────────┐
│ Files    │ Gantt Chart / Schedule  │ Stats    │
│ (tree)   │ Calendar (large)        │ Deadlines│
│          │ Timeline                │ Badges   │
│          │                         │          │
└──────────┴─────────────────────────┴──────────┘
```

**Visible panels:**
- Files (tree structure, expanded)
- Gantt Chart (center, main view)
- Statistics/Challenges (right, prominent)
- Calendar (center or right, large)

**Toolbars:** Standard + Tools

**Use case:** Planning, deadlines, progress tracking

---

### Perspective Management

**Switching:**
- View → Perspectives → [Select perspective]
- Keyboard: F5 (Writer), F6 (Editor), F7 (Researcher), F8 (Planner)
- Toolbar: [Perspectives ▼] dropdown

**Saving custom:**
- View → Perspectives → Save Current As...
- Enter name (e.g., "My Novel Workflow")
- Saved to `~/.kalahari/perspectives/`

**Editing:**
- Arrange panels as desired
- Modify toolbars
- View → Perspectives → Update "[Current]"

**Reset:**
- View → Perspectives → Reset to Default

---

## Gamification System

Motivate writers through achievements, challenges, and statistics.

### Badge System

**25+ Badges across 4 categories:**

#### Daily Badges
- 🌅 **Early Bird** - Writing before 8:00 AM
- 🦉 **Night Owl** - Writing after 10:00 PM
- 🏃 **Sprint Champion** - 500+ words in one session
- 📝 **Daily Writer** - Wrote today (any amount)
- 🎯 **Goal Crusher** - Exceeded daily goal by 50%
- ⏱️ **Flow State** - 20+ minutes uninterrupted writing

#### Weekly Badges
- 💪 **Dedicated** - Wrote 5/7 days
- 🔥 **Week Streak** - 7 days in a row
- 📚 **5K Club** - 5,000+ words this week
- ⏰ **Marathon** - Single 2h+ session
- 🎨 **Versatile** - Worked on 3+ different chapters

#### Monthly Badges
- 🏆 **NaNoWriMo** - 50,000+ words in month
- ✨ **Perfect Month** - Wrote every single day
- 📖 **Chapter Master** - Completed 5+ chapters
- 🌟 **Consistency King/Queen** - No day below 50% of goal

#### Project Milestones
- 🎉 **First Draft** - Completed first draft
- 👑 **The End** - Finished the book
- ✅ **Revised** - Completed full revision
- 📤 **Published** - Exported final version
- 🌟 **Opus Magnum** - 100,000+ word project
- 🏅 **Series Writer** - Completed 3+ books

**Badge display:**
- Recent badges in Statistics/Challenges panel (max 8)
- Click badge → Shows details (date earned, description)
- View All Achievements → Full grid in center workspace

---

### Challenge System

**Active challenges** (opt-in, user enables):

#### Daily Challenges
- "Write 1,000 words today"
- "Edit for 30 minutes"
- "Add 2 new characters"

#### Weekly Challenges
- "5K This Week" (5,000 words)
- "Write 5 days in a row"
- "Complete 3 chapters"

#### Monthly Challenges
- "NaNoWriMo" (50,000 words)
- "Revise entire manuscript"
- "Perfect attendance"

**Challenge UI:**
```
🎯 ACTIVE CHALLENGES
┌─────────────────────┐
│ 5K This Week        │
│ ▓▓▓▓░░░ 3,200/5,000 │
│ 2 days left         │
└─────────────────────┘
```

**Rewards:**
- Badge on completion
- Progress notification
- Confetti animation (optional, can disable)

**Opt-in/out:**
- Settings → Gamification → Enable Challenges
- Choose which challenge types to show

---

### Streak System

**Tracks consecutive writing days:**
- 🔥 **Streak counter** in status bar: "🔥 7 days"
- Click → Shows streak calendar
- Freeze days: User can mark 2 "freeze" days per month (vacation, sick)

**Streak milestones:**
- 7 days → Badge
- 30 days → Badge + special animation
- 100 days → Badge + trophy
- 365 days → "Year Warrior" badge

---

## Info Bar & Status Bar

### Info Bar (Above Status Bar)

**Purpose:** Non-intrusive notifications

**5 message types:**

```
[INFO] 🔵
ℹ️ Auto-save completed at 14:23

[SUCCESS] 🟢
✅ Daily goal reached: 1,000 words!

[WARNING] 🟡
⚠️ Plugin 'AI Assistant' requires update

[UPDATE] 🟣
🔔 Kalahari 1.1 available [Update Now] [Later]

[ACHIEVEMENT] 🟠
🏆 Achievement unlocked: Sprint Champion!
```

**Behavior:**
- Auto-dismiss after 5 seconds (INFO, SUCCESS)
- Persistent until user closes (WARNING, UPDATE)
- Click for details (expandable)
- History: View → Recent Messages

---

### Status Bar (Bottom)

**8 segments (left to right):**

```
[Status] [Words] [Chars] [Reading] [Progress] [Session] [Cursor] [Streak]
```

**Segment details:**

| # | Content | Example | Click Action |
|---|---------|---------|--------------|
| 1 | Status | Ready / Saving / Error | - |
| 2 | Word count | 1,234 words | Opens Statistics |
| 3 | Char count | 5,678 chars | Opens Statistics |
| 4 | Reading time | 6 min read | Opens Statistics |
| 5 | Daily progress | 850/1000 (85%) | Opens Daily Stats |
| 6 | Session time | 45 min | Opens Session Tracker |
| 7 | Cursor pos | Ln 42, Col 15 | Go to Line dialog |
| 8 | Streak | 🔥 7 days | Opens Streak Calendar |

**Configuration:**
- Right-click → Customize Status Bar
- Show/hide individual segments
- Reorder segments

---

## Keyboard Shortcuts

### Default Shortcuts (80+ total)

#### File (10)
- `Ctrl+N` - New Project
- `Ctrl+O` - Open Project
- `Ctrl+S` - Save
- `Ctrl+Shift+S` - Save As
- `Ctrl+W` - Close Document
- `Ctrl+P` - Print
- `Ctrl+Q` - Exit

#### Edit (15)
- `Ctrl+Z` - Undo
- `Ctrl+Y` / `Ctrl+Shift+Z` - Redo
- `Ctrl+X` - Cut
- `Ctrl+C` - Copy
- `Ctrl+V` - Paste
- `Ctrl+A` - Select All
- `Ctrl+F` - Find
- `Ctrl+H` - Replace
- `Ctrl+G` - Go to Line
- `F3` - Find Next

#### Format (10)
- `Ctrl+B` - Bold
- `Ctrl+I` - Italic
- `Ctrl+U` - Underline
- `Ctrl+Shift+K` - Strikethrough
- `Ctrl+0` - Normal style
- `Ctrl+1` - Heading 1
- `Ctrl+2` - Heading 2

#### View (10)
- `F11` - Fullscreen
- `F9` - Normal mode
- `F10` - Focused mode
- `Ctrl+\` - Toggle Focus Mode
- `F5-F8` - Quick perspective switch
- `Ctrl+1-5` - Toggle panels

#### Tools (8)
- `F7` - Spell Check
- `Ctrl+Shift+C` - Word Count
- `Ctrl+Shift+T` - Timeline
- `Ctrl+Shift+M` - Mind Map

#### Window (5)
- `Ctrl+Tab` - Next Tab
- `Ctrl+Shift+Tab` - Previous Tab
- `Ctrl+F4` - Close Tab

### Shortcut Customization

**Access:** Tools → Options → Keyboard Shortcuts

```
┌─────────────────────────────────────────────────┐
│ Keyboard Shortcuts                        [X]   │
├─────────────────────────────────────────────────┤
│ Search: [________]  🔍                          │
│                                                  │
│ Command          │ Shortcut   │ [Assign] [Clear]│
├──────────────────┼────────────┤                 │
│ File: Save       │ Ctrl+S     │                 │
│ File: Save As    │ Ctrl+Sh+S  │                 │
│ Edit: Undo       │ Ctrl+Z     │                 │
│ Edit: Redo       │ Ctrl+Y     │                 │
│ Format: Bold     │ Ctrl+B     │                 │
│ AI: Suggest      │ Ctrl+Sh+A  │ ← Plugin        │
│ ...                                             │
├─────────────────────────────────────────────────┤
│ Conflict detection: ✅ Enabled                  │
│                                                  │
│ [Reset to Defaults]  [Import...]  [Export...]  │
│                                                  │
│                    [OK]  [Cancel]  [Apply]      │
└─────────────────────────────────────────────────┘
```

**Features:**
- Search/filter commands
- Assign new shortcut (click command, press keys)
- Clear shortcut
- Conflict detection (warns if shortcut already used)
- Import/export profiles
- Reset to defaults

---

## Focus Modes

**3 modes for different concentration levels:**

### Normal Mode (F9)

**Layout:**
- All panels visible (per current perspective)
- Menu bar, toolbars, status bar visible
- Full feature access

**Use:** Regular work, full feature set

---

### Focused Mode (F10)

**Layout:**
- Editor maximized
- Files/Libraries + Assistant panels dimmed (50% opacity)
- Toolbars hidden
- Menu bar minimized (auto-hide)
- Status bar visible

**Interaction:**
- Hover over dimmed panel → Full opacity
- Press Alt → Show menu bar temporarily
- Still access all features, but less visual clutter

**Use:** Concentrated writing with occasional reference needs

---

### Distraction-Free Mode (F11)

**Layout:**
- Fullscreen
- Only editor visible
- Everything else hidden (panels, menu, toolbars, status bar)
- Minimal chrome (no window borders)

**Interaction:**
- ESC → Exit distraction-free mode
- Move mouse to top → Show minimal toolbar (Save, Word Count, Exit)

**Use:** Deep focus, flow state, zero distractions

---

## Qt Implementation Notes

### Qt QDockWidget Panel Management

```cpp
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>

// Adding dockable panel with Qt
QDockWidget* filesPanel = new QDockWidget(tr("Files"), this);
filesPanel->setObjectName("files");  // Important for QSettings persistence
filesPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
filesPanel->setFeatures(QDockWidget::DockWidgetClosable |
                        QDockWidget::DockWidgetMovable |
                        QDockWidget::DockWidgetFloatable);
filesPanel->setMinimumWidth(200);

// Set widget content
FilesWidget* filesWidget = new FilesWidget(filesPanel);
filesPanel->setWidget(filesWidget);

// Add to main window
addDockWidget(Qt::LeftDockWidgetArea, filesPanel);

// Tab multiple docks together
QDockWidget* librariesPanel = new QDockWidget(tr("Libraries"), this);
// ... configure librariesPanel ...
tabifyDockWidget(filesPanel, librariesPanel);  // Tab them together
```

**Qt QDockWidget Benefits:**
- **Automatic persistence:** QMainWindow::saveState() saves dock positions/visibility
- **Built-in UI:** Close, float, resize handles out-of-the-box
- **Tabbing:** `tabifyDockWidget()` for tabbed panels
- **Signals:** `dockLocationChanged`, `visibilityChanged`, `topLevelChanged`

### Perspective Saving with QSettings

```cpp
#include <QSettings>
#include <QByteArray>

void MainWindow::savePerspective(const QString& name) {
    // Save window state (all dock positions, toolbar state, sizes)
    QByteArray state = saveState();
    QByteArray geometry = saveGeometry();

    // Save to platform-native storage (registry/plist/ini)
    QSettings settings("Kalahari", "App");
    settings.beginGroup("Perspectives");
    settings.setValue(name + "/state", state);
    settings.setValue(name + "/geometry", geometry);
    settings.setValue(name + "/timestamp", QDateTime::currentDateTime());
    settings.endGroup();

    spdlog::info("Saved perspective: {}", name.toStdString());
}

void MainWindow::loadPerspective(const QString& name) {
    QSettings settings("Kalahari", "App");
    settings.beginGroup("Perspectives");

    if (!settings.contains(name + "/state")) {
        spdlog::warn("Perspective not found: {}", name.toStdString());
        return;
    }

    QByteArray state = settings.value(name + "/state").toByteArray();
    QByteArray geometry = settings.value(name + "/geometry").toByteArray();
    settings.endGroup();

    restoreGeometry(geometry);
    restoreState(state);

    spdlog::info("Loaded perspective: {}", name.toStdString());
}

// Default perspectives in Phase 0 Week 1
void MainWindow::createDefaultPerspectives() {
    // Writer perspective (focused)
    // ... configure docks for writing ...
    savePerspective("Writer");

    // Editor perspective (all tools visible)
    // ... configure docks for editing ...
    savePerspective("Editor");

    // Researcher perspective
    savePerspective("Researcher");

    // Planner perspective
    savePerspective("Planner");
}
```

**QSettings Benefits:**
- **Platform-native:** Windows Registry, macOS plist, Linux ~/.config/
- **Automatic:** No JSON parsing needed
- **Hierarchical:** Natural grouping with beginGroup()/endGroup()
- **Type-safe:** Stores QByteArray, QString, int, bool, etc.

### Focus Mode Implementation with Qt

```cpp
void MainWindow::setFocusMode(FocusMode mode) {
    switch (mode) {
        case FocusMode::NORMAL:
            // Restore normal state
            if (isFullScreen()) {
                showNormal();
            }
            menuBar()->show();
            statusBar()->show();

            // Show all docks
            findChild<QDockWidget*>("files")->show();
            findChild<QDockWidget*>("assistant")->show();
            findChild<QDockWidget*>("statistics")->show();

            // Show toolbars
            foreach (QToolBar* toolbar, findChildren<QToolBar*>()) {
                toolbar->show();
            }
            break;

        case FocusMode::FOCUSED:
            // Dim side panels using QGraphicsOpacityEffect
            QGraphicsOpacityEffect* filesOpacity = new QGraphicsOpacityEffect(this);
            filesOpacity->setOpacity(0.5);  // 50% opacity
            findChild<QDockWidget*>("files")->setGraphicsEffect(filesOpacity);

            QGraphicsOpacityEffect* assistantOpacity = new QGraphicsOpacityEffect(this);
            assistantOpacity->setOpacity(0.5);
            findChild<QDockWidget*>("assistant")->setGraphicsEffect(assistantOpacity);

            // Hide toolbars
            foreach (QToolBar* toolbar, findChildren<QToolBar*>()) {
                toolbar->hide();
            }

            // Minimize menu bar (auto-hide not directly supported, use custom)
            menuBar()->setMaximumHeight(0);  // Workaround: collapse menu
            break;

        case FocusMode::DISTRACTION_FREE:
            // Fullscreen
            showFullScreen();
            menuBar()->hide();
            statusBar()->hide();

            // Hide all docks
            foreach (QDockWidget* dock, findChildren<QDockWidget*>()) {
                dock->hide();
            }

            // Hide all toolbars
            foreach (QToolBar* toolbar, findChildren<QToolBar*>()) {
                toolbar->hide();
            }

            // Only editor visible
            // (central widget remains visible by default)
            break;
    }

    m_currentFocusMode = mode;
    emit focusModeChanged(mode);
}
```

**Qt Focus Mode Patterns:**
- **QGraphicsOpacityEffect:** Dim widgets without hiding them
- **showFullScreen():** Native fullscreen support
- **findChildren<T*>():** Query all widgets of type T
- **Signals:** Emit `focusModeChanged` for observers to react

### Qt vs wxWidgets Comparison

| Feature | wxWidgets (wxAUI) | Qt6 (QDockWidget) |
|---------|-------------------|-------------------|
| **Docking** | wxAuiManager::AddPane() | addDockWidget() (built-in QMainWindow) |
| **Persistence** | SavePerspective() string → manual JSON | saveState()/restoreState() + QSettings (automatic) |
| **Tabbing** | Manual configuration | tabifyDockWidget() (one line) |
| **Styling** | Limited | QSS (CSS-like, full theming) |
| **Signals** | Custom events | Qt signals/slots (type-safe) |
| **Opacity** | SetTransparent() | QGraphicsOpacityEffect (hardware-accelerated) |
| **Layout** | Manual sizer management | Automatic layout system |

---

## Settings Dialog

### Overview

The Settings Dialog provides comprehensive access to all application configuration options. It uses a **tree-based navigation system** (wxTreeCtrl + wxScrolledWindow) to organize settings into hierarchical categories, ensuring scalability as the application grows in complexity.

**Access:** File → Settings... (Ctrl+,)

### Design Philosophy

**Why Tree Navigation?**

1. **Scalability** - Can accommodate 50+ settings categories without UI clutter
2. **Hierarchy** - Natural grouping (General → Appearance → Theme)
3. **Familiarity** - Industry standard (Visual Studio, FreeCAD, TortoiseGit, Eclipse)
4. **Expandability** - Plugins can add their own branches seamlessly
5. **Clarity** - Icons + text labels make navigation intuitive

**Rejected Alternatives:**
- ❌ **wxNotebook (tabs)** - Doesn't scale beyond ~10 categories, no hierarchy
- ❌ **Flat list with icons** - No hierarchy, becomes unwieldy with plugins

### Layout Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Settings                                          [X]       │
├───────────────┬─────────────────────────────────────────────┤
│               │                                             │
│  Tree         │  Panel (wxScrolledWindow)                  │
│  280px        │  Remaining width                           │
│  (resizable)  │                                             │
│               │  ┌───────────────────────────────────────┐ │
│ ⚙️ General    │  │ [Content for selected category]      │ │
│ ├─ 🌍 Lang    │  │                                       │ │
│ ├─ 🎨 Theme   │  │  Settings controls here...            │ │
│ └─ 💾 Save    │  │  - Checkboxes                         │ │
│               │  │  - Text fields                        │ │
│ 📝 Editor     │  │  - Dropdowns                          │ │
│ ├─ 🔤 Font    │  │  - Sliders                            │ │
│ └─ ⌨️ Keys     │  │  - Color pickers                      │ │
│               │  │                                       │ │
│ 🔧 Advanced   │  │  [Vertical scrolling if content       │ │
│ └─ 🐛 Diag ✓  │  │   exceeds panel height]               │ │
│               │  │                                       │ │
│               │  └───────────────────────────────────────┘ │
├───────────────┴─────────────────────────────────────────────┤
│                                      [OK] [Cancel] [Apply]  │
└─────────────────────────────────────────────────────────────┘
```

**Key Implementation Details:**
- **wxSplitterWindow** - User-resizable divider between tree and panel
- **Tree width:** 280px default, min 200px, max 400px (resizable via sash drag)
- **Panel:** wxScrolledWindow with vertical scrolling for long content
- **Buttons:** OK (save + close), Cancel (discard), Apply (save + keep open)

### Full Settings Tree Structure (Planned)

This section documents the **complete vision** for Kalahari's settings hierarchy across all development phases. Implementation is incremental - only actively needed categories are implemented in each phase.

```
📋 General
├─ 🌍 Language                        [Phase 1]
│  ├─ Interface Language (EN/PL/DE/RU/FR/ES)
│  ├─ Spellcheck Languages
│  └─ Date/Time Format
├─ 🎨 Appearance                      [Phase 1]
│  ├─ Theme (Light/Dark/Savanna/Midnight)
│  ├─ Font Size (UI scale)
│  ├─ Icon Set (default/large/colorblind-friendly)
│  └─ High Contrast Mode
├─ 💾 Auto-save                       [Phase 1]
│  ├─ Auto-save interval (1-60 min)
│  ├─ Backup location
│  └─ Version history (keep last N)
└─ 🗂️ File Locations                 [Phase 1]
   ├─ Default project folder
   ├─ Templates folder
   └─ Export output folder

📝 Editor
├─ 🔤 Font & Typography               [Phase 1]
│  ├─ Editor font (monospace/serif/sans-serif)
│  ├─ Font size (10-24pt)
│  ├─ Line height (1.0-2.0)
│  └─ Letter spacing
├─ ⌨️ Keybindings                     [Phase 2]
│  ├─ Preset (Default/Vim/Emacs)
│  └─ Custom shortcuts
├─ 📐 Layout & Panels                 [Phase 2]
│  ├─ Default perspective
│  ├─ Panel positions (saved per perspective)
│  └─ Focus mode settings
└─ ✍️ Writing Aids                    [Phase 2]
   ├─ Auto-capitalize sentences
   ├─ Smart quotes (" " vs '')
   ├─ Em/En dash replacement
   └─ Word suggestions

🦁 Assistant                          [Phase 1+]
├─ 🎭 Personality Selection
│  ├─ Default animal (Lion/Meerkat/Elephant/Cheetah)
│  ├─ Unlock all 8 animals (premium)
│  └─ Custom personality (Phase 3+)
├─ 💬 Interaction Settings
│  ├─ Notification frequency (Never/Hourly/Daily/Smart)
│  ├─ Encouragement style (Gentle/Motivating/Strict)
│  └─ Break reminders (20-20-20 rule)
└─ 🔊 Voice & Appearance             [Phase 3+]
   ├─ Voice enabled (TTS)
   ├─ Voice speed
   └─ Animation level (None/Subtle/Full)

🔌 Plugins                            [Phase 2+]
├─ 📦 Installed Plugins
│  ├─ Enable/Disable plugins
│  ├─ Plugin update settings
│  └─ Plugin load order
├─ 🛒 Plugin Marketplace              [Phase 3+]
│  ├─ Marketplace URL
│  ├─ Auto-check updates
│  └─ Beta plugins enabled
└─ ⚙️ Plugin Settings                [Dynamic - added by plugins]
   ├─ AI Assistant Pro
   │  ├─ API Key (OpenAI/Claude)
   │  ├─ Model selection
   │  └─ Max tokens
   ├─ Export Suite
   │  ├─ PDF settings (margins, fonts)
   │  ├─ EPUB metadata
   │  └─ Template paths
   ├─ Research Pro
   │  ├─ Citation style (MLA/APA/Chicago)
   │  ├─ Bibliography format
   │  └─ Web clipper hotkey
   └─ [Other installed plugins...]

📚 Project Management                 [Phase 2+]
├─ 📖 Default Structure
│  ├─ Default chapter template
│  ├─ Part/Chapter naming scheme
│  └─ Metadata fields (author, genre, tags)
├─ 🏷️ Metadata Templates
│  ├─ Fiction (genre, POV, tense)
│  ├─ Non-fiction (topic, audience)
│  └─ Academic (citations, references)
└─ 🔖 Bookmarks & Tags               [Phase 3+]
   ├─ Tag colors
   ├─ Bookmark shortcuts
   └─ Search scope (current/all)

✉️ Publishing & Integration           [Phase 3+]
├─ 📧 Email Configuration
│  ├─ IMAP/SMTP Settings
│  │  ├─ Server addresses
│  │  ├─ Port & encryption (SSL/TLS)
│  │  └─ Authentication
│  ├─ Publisher Contacts
│  │  ├─ Add/edit/remove contacts
│  │  ├─ Email templates (query/submission)
│  │  └─ Attachment settings
│  └─ Email Templates
│     ├─ Query letter
│     ├─ Manuscript submission
│     └─ Follow-up
├─ ☁️ Cloud Services                 [Phase 4+]
│  ├─ Dropbox
│  │  ├─ API token
│  │  ├─ Sync folder
│  │  └─ Conflict resolution
│  ├─ Google Drive
│  │  ├─ OAuth credentials
│  │  └─ Sync settings
│  └─ OneDrive
│     └─ Similar to above
└─ 🌐 Web Publishing                 [Phase 4+]
   ├─ WordPress integration
   ├─ Medium API
   └─ Custom blog (FTP/SFTP)

🔧 Advanced
├─ 🐛 Diagnostics                    [Phase 0] ✅ CURRENT
│  └─ Enable Diagnostic Options
│     ├─ Shows "Diagnostics" menu
│     ├─ Runtime only (not saved)
│     ├─ Confirmation required
│     └─ Grayed out if launched with --diag flag
├─ 🗄️ Database                       [Phase 2+]
│  ├─ Vacuum on exit
│  ├─ Index optimization
│  └─ Cache size (MB)
├─ 🧵 Threading                      [Phase 2+]
│  ├─ Worker thread count (2-8)
│  ├─ Background indexing
│  └─ Async save enabled
└─ 🔐 Security                       [Phase 4+]
   ├─ Master password
   ├─ Encrypt project files
   └─ Plugin signature verification
```

### Phase 0 Implementation (Current)

**Minimal viable implementation:**

```
🔧 Advanced
└─ 🐛 Diagnostics
   └─ [✓] Enable Diagnostic Options
```

**Features:**
- Single tree node (Advanced → Diagnostics)
- One checkbox: "Enable Diagnostic Options"
- Warning text + confirmation dialog
- Runtime state only (not persisted to settings.json)
- Grayed out if `--diag` CLI flag used

**Rationale:**
- Fixes terminal hang bug (removes wxExecute restart mechanism)
- Establishes Settings Dialog infrastructure for future expansion
- Follows YAGNI principle (implement only what's needed now)

### Implementation Details

#### wxWidgets Components

```cpp
class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, const SettingsState& currentState);

    bool ShowModal() override;
    SettingsState GetNewState() const;

private:
    // UI Components
    wxSplitterWindow* m_splitter;        // Divider between tree and panel
    wxTreeCtrl* m_tree;                  // Left: navigation tree
    wxScrolledWindow* m_contentPanel;    // Right: settings content
    wxImageList* m_iconList;             // Icons for tree nodes

    // Panel management
    std::map<wxTreeItemId, wxPanel*> m_panels;  // Category → Panel mapping
    wxPanel* m_currentPanel;             // Currently visible panel

    // State
    SettingsState m_originalState;       // State when dialog opened
    SettingsState m_workingState;        // Modified state (before OK)

    // Event handlers
    void onTreeSelectionChanged(wxTreeEvent& event);
    void onOK(wxCommandEvent& event);
    void onCancel(wxCommandEvent& event);
    void onApply(wxCommandEvent& event);

    // Helpers
    void buildTree();                    // Construct tree structure
    void showPanel(wxTreeItemId item);   // Switch visible panel
    bool validateSettings();             // Check for invalid values
    void applyChanges();                 // Commit to SettingsManager
};
```

#### Tree Construction (Phase 0)

```cpp
void SettingsDialog::buildTree() {
    // Icon indices (loaded from resources)
    enum IconIndex {
        ICON_SETTINGS = 0,
        ICON_ADVANCED = 1,
        ICON_DIAGNOSTICS = 2,
        // ... future icons
    };

    // Root (hidden in most OS styles)
    wxTreeItemId root = m_tree->AddRoot("Settings", ICON_SETTINGS);

    // Advanced branch
    wxTreeItemId advanced = m_tree->AppendItem(
        root,
        "Advanced",
        ICON_ADVANCED
    );

    // Diagnostics leaf
    wxTreeItemId diagnostics = m_tree->AppendItem(
        advanced,
        "Diagnostics",
        ICON_DIAGNOSTICS
    );

    // Create panel for Diagnostics
    DiagnosticsPanel* diagPanel = new DiagnosticsPanel(
        m_contentPanel,
        m_workingState
    );
    m_panels[diagnostics] = diagPanel;

    // Expand Advanced by default
    m_tree->Expand(advanced);

    // Select Diagnostics by default
    m_tree->SelectItem(diagnostics);
}
```

#### Panel Switching

```cpp
void SettingsDialog::onTreeSelectionChanged(wxTreeEvent& event) {
    wxTreeItemId item = event.GetItem();

    // Hide current panel
    if (m_currentPanel) {
        m_currentPanel->Hide();
    }

    // Show selected panel
    auto it = m_panels.find(item);
    if (it != m_panels.end()) {
        m_currentPanel = it->second;
        m_currentPanel->Show();

        // Trigger layout recalculation
        m_contentPanel->Layout();
        m_contentPanel->FitInside();  // Update scrollbars
        m_contentPanel->Scroll(0, 0); // Reset to top
    }
}
```

#### Diagnostics Panel (Phase 0)

```cpp
class DiagnosticsPanel : public wxPanel {
public:
    DiagnosticsPanel(wxWindow* parent, SettingsState& state)
        : wxPanel(parent), m_state(state)
    {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        // Warning header
        wxStaticText* warning = new wxStaticText(
            this,
            wxID_ANY,
            "⚠️ Advanced Diagnostic Options"
        );
        warning->SetFont(warning->GetFont().Bold().Larger());
        sizer->Add(warning, 0, wxALL, 10);

        // Checkbox
        m_diagnosticCheckbox = new wxCheckBox(
            this,
            wxID_ANY,
            "Enable Diagnostic Options"
        );
        m_diagnosticCheckbox->SetValue(m_state.diagnosticModeEnabled);
        m_diagnosticCheckbox->Bind(wxEVT_CHECKBOX,
            &DiagnosticsPanel::onCheckboxChanged, this);
        sizer->Add(m_diagnosticCheckbox, 0, wxALL, 10);

        // Description
        wxStaticText* desc = new wxStaticText(
            this,
            wxID_ANY,
            "Shows the Diagnostics menu with developer tools for debugging.\n"
            "This setting is not saved and resets on application restart.\n\n"
            "Use this only when troubleshooting issues or requested by support."
        );
        desc->Wrap(500);
        sizer->Add(desc, 0, wxALL, 10);

        // Gray out if launched with --diag
        if (m_state.launchedWithDiagFlag) {
            m_diagnosticCheckbox->Enable(false);
            m_diagnosticCheckbox->SetToolTip(
                "Diagnostic mode was enabled via --diag command-line flag.\n"
                "It cannot be changed during this session."
            );
        }

        SetSizer(sizer);
    }

    void SaveToState() {
        if (!m_state.launchedWithDiagFlag) {
            m_state.diagnosticModeEnabled = m_diagnosticCheckbox->GetValue();
        }
    }

private:
    SettingsState& m_state;
    wxCheckBox* m_diagnosticCheckbox;

    void onCheckboxChanged(wxCommandEvent&) {
        // State updated in SaveToState() when OK/Apply clicked
    }
};
```

#### Confirmation Dialog (Diagnostics Enable)

```cpp
void SettingsDialog::onOK(wxCommandEvent& event) {
    // Save all panel states to m_workingState
    for (auto& [id, panel] : m_panels) {
        if (auto* diagPanel = dynamic_cast<DiagnosticsPanel*>(panel)) {
            diagPanel->SaveToState();
        }
    }

    // Confirmation if enabling diagnostics
    if (m_workingState.diagnosticModeEnabled &&
        !m_originalState.diagnosticModeEnabled)
    {
        int result = wxMessageBox(
            "Are you sure you want to enable advanced diagnostic options?\n\n"
            "These options are intended for debugging and may expose\n"
            "internal application state. Enable only when troubleshooting.",
            "Enable Diagnostic Options?",
            wxYES_NO | wxICON_WARNING,
            this
        );

        if (result != wxYES) {
            m_workingState.diagnosticModeEnabled = false;
            return; // Don't close dialog
        }
    }

    // Validate all settings
    if (!validateSettings()) {
        wxMessageBox(
            "Some settings have invalid values. Please correct them.",
            "Invalid Settings",
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    // Apply changes
    applyChanges();

    EndModal(wxID_OK);
}
```

### Future Expansion (Phase 1+)

**Adding new categories is straightforward:**

```cpp
// In buildTree()
wxTreeItemId general = m_tree->AppendItem(root, "General", ICON_GENERAL);
wxTreeItemId language = m_tree->AppendItem(general, "Language", ICON_LANGUAGE);

LanguagePanel* langPanel = new LanguagePanel(m_contentPanel, m_workingState);
m_panels[language] = langPanel;
```

**Plugin categories:**

```cpp
// Plugin registers its settings panel via Extension Point
PluginManager::registerSettingsPanel("AI Assistant Pro",
    [](wxWindow* parent, SettingsState& state) {
        return new AIAssistantSettingsPanel(parent, state);
    }
);

// SettingsDialog automatically adds plugin panels under "Plugins" branch
```

### Design Rationale

**Why runtime-only for Diagnostics?**
- Prevents accidental leave-on (diagnostic mode is for debugging, not normal use)
- Cleaner settings.json (no debug flags in production config)
- CLI `--diag` remains available for developers who need it persistently

**Why confirmation dialog?**
- Prevents accidental enabling (checkbox misclick)
- Sets user expectation: "This is advanced/dangerous territory"
- Consistent with industry patterns (Firefox about:config warning, etc.)

**Why 280px tree width (not 200px)?**
- Accommodates longer labels: "Publishing & Integration", "Diagnostics"
- Future-proof for plugin names: "Export Suite Settings"
- Resizable anyway (user can adjust if needed)

**Why wxSplitterWindow (not fixed layout)?**
- User preferences vary (some want wider tree, some want more panel space)
- Persistence: Save splitter position in settings.json (Phase 1+)
- Professional UX: All modern IDEs have resizable settings panels

### Integration with MainWindow

**Menu item creation:**

```cpp
void MainWindow::createMenuBar() {
    wxMenu* fileMenu = new wxMenu;
    // ... other items
    fileMenu->Append(wxID_PREFERENCES, "Settings...\tCtrl+,");
    Bind(wxEVT_MENU, &MainWindow::onFileSettings, this, wxID_PREFERENCES);
}
```

**Opening dialog + applying changes:**

```cpp
void MainWindow::onFileSettings(wxCommandEvent&) {
    SettingsState currentState;
    currentState.diagnosticModeEnabled = m_diagnosticMode;
    currentState.launchedWithDiagFlag = m_launchedWithDiagFlag;
    // ... other state

    SettingsDialog dlg(this, currentState);
    if (dlg.ShowModal() == wxID_OK) {
        SettingsState newState = dlg.GetNewState();

        // Apply diagnostic mode change (immediate)
        if (newState.diagnosticModeEnabled != m_diagnosticMode) {
            setDiagnosticMode(newState.diagnosticModeEnabled);
        }

        // Apply other settings (Phase 1+)
        // ...
    }
}
```

**Dynamic menu toggle:**

```cpp
void MainWindow::setDiagnosticMode(bool enabled) {
    if (m_diagnosticMode == enabled) return;

    m_diagnosticMode = enabled;

    // Rebuild menu bar to show/hide Diagnostics menu
    wxMenuBar* oldMenuBar = GetMenuBar();
    SetMenuBar(nullptr);
    createMenuBar(); // Respects m_diagnosticMode
    delete oldMenuBar;

    Logger::info("Diagnostic mode {}", enabled ? "enabled" : "disabled");
}
```

---

## Related Documents

- **[03_architecture.md](03_architecture.md)** - Core architecture patterns (Qt MVP + signals/slots)
- **[04_plugin_system.md](04_plugin_system.md)** - Plugin API details (Qt-agnostic)
- **[10_branding.md](10_branding.md)** - Colors, fonts, icons
- **[02_tech_stack.md](02_tech_stack.md)** - Qt6 6.5.0+ framework

---

## Implementation Phases (Qt)

| Feature | Phase | Priority | Qt Components |
|---------|-------|----------|---------------|
| Basic layout (3 columns) | Phase 0 Week 1 | Critical | QMainWindow + QDockWidget |
| Core panels (Files, Editor, Assistant) | Phase 0 Weeks 1-4 | Critical | QTreeWidget, QPlainTextEdit, custom QWidget |
| Command Registry | Phase 0 (preserved from core) | Critical | Qt-agnostic (pure C++) |
| Default toolbars (6) | Phase 1 | High | QToolBar + QAction |
| Perspectives (4 default) | Phase 0 Week 1 | High | QSettings + saveState()/restoreState() |
| Customizable toolbars UI | Phase 2 | Medium | QDialog + toolbar editor |
| Quick Access Toolbar | Phase 2 | Medium | Custom QToolBar |
| Gamification (badges, challenges) | Phase 3 | Low | Custom QWidget panels |
| Live Customization Mode | Phase 3 | Low | Qt Designer-like UI |
| Command Palette | Phase 3 | Low | QDialog + fuzzy search |
| Focus Modes | Phase 1 | High | QGraphicsOpacityEffect + showFullScreen() |
| Info Bar / Status Bar | Phase 0 Week 1 | High | QStatusBar + custom QWidget |

---

## Migration Notes (2025-11-19)

**Previous Implementation:** wxWidgets 3.3.0+ with wxAUI (Advanced User Interface)
- Manual perspective serialization (wxAuiManager::SavePerspective() → JSON)
- Complex sizer-based layouts
- Custom panel management system

**New Implementation:** Qt6 6.5.0+ with QDockWidget
- **Automatic persistence:** QMainWindow::saveState() + QSettings (platform-native)
- **Simpler API:** addDockWidget() vs wxAuiManager::AddPane()
- **Better theming:** QSS (CSS-like) vs limited wxWidgets styling
- **Hardware-accelerated effects:** QGraphicsOpacityEffect for dimming

**Code Migration Status:**
- ✅ Section 14 updated with Qt implementation patterns
- ⏳ Remaining code examples still use wxWidgets patterns (will update during Phase 0-1)
- ✅ Architecture concepts remain valid (Command Registry, perspectives, panels)

**Benefits:**
- Simpler codebase (Qt handles docking/persistence automatically)
- Better cross-platform consistency
- Superior documentation and community support
- Modern QSS styling system

---

## QPalette Theme System (Task #00023)

### Overview

Kalahari uses Qt's `QPalette` system for native widget theming. This architecture ensures that **all Qt Fusion style widgets** (spinboxes, comboboxes, scrollbars, etc.) properly display their control elements (arrows, buttons) in both light and dark themes.

### Problem Statement

**Issue:** Qt Fusion style uses QPalette colors to render widget elements. Without proper palette configuration, widget controls (spinbox arrows, combobox dropdowns) become invisible in dark themes because they render with default light colors.

**Previous attempts that failed:**
- CSS border-based triangles (works in browsers, not in Qt)
- QSS with hardcoded colors (doesn't integrate with theme system)

**Solution:** Extend Theme JSON with QPalette color roles and apply via `QApplication::setPalette()`.

### Architecture

```
Theme JSON (Dark.json/Light.json)
    ↓
Theme::fromJson() parses "palette" section
    ↓
Theme::Palette::toQPalette() creates QPalette
    ↓
ThemeManager::applyTheme() calls QApplication::setPalette()
    ↓
Qt Fusion style uses palette colors for ALL widgets
```

### Theme JSON Structure

```json
{
  "name": "Dark",
  "version": "1.1",
  "colors": {
    "primary": "#E0E0E0",
    "secondary": "#808080",
    "accent": "#0078D4",
    "background": "#1E1E1E",
    "text": "#FFFFFF"
  },
  "palette": {
    "window": "#2b2b2b",
    "windowText": "#e0e0e0",
    "base": "#3c3c3c",
    "alternateBase": "#323232",
    "text": "#e0e0e0",
    "button": "#4a4a4a",
    "buttonText": "#e0e0e0",
    "highlight": "#0078d4",
    "highlightedText": "#ffffff",
    "light": "#606060",
    "midlight": "#505050",
    "mid": "#404040",
    "dark": "#202020",
    "shadow": "#000000",
    "link": "#0078d4",
    "linkVisited": "#5856d6"
  },
  "log": {
    "info": "#CCCCCC",
    "debug": "#888888",
    "background": "#252525"
  }
}
```

### Theme Struct (C++)

```cpp
struct Theme {
    // ... existing fields ...

    /// @brief Qt Palette colors (for native widget styling)
    struct Palette {
        QColor window;          ///< General background color
        QColor windowText;      ///< General foreground color
        QColor base;            ///< Background for text entry widgets
        QColor alternateBase;   ///< Alternate background for views
        QColor text;            ///< Foreground for text entry widgets
        QColor button;          ///< Button background color
        QColor buttonText;      ///< Button foreground color
        QColor highlight;       ///< Selection/focus highlight color
        QColor highlightedText; ///< Text color when highlighted
        QColor light;           ///< Lighter than button color
        QColor midlight;        ///< Between button and light
        QColor mid;             ///< Between button and dark
        QColor dark;            ///< Darker than button color
        QColor shadow;          ///< Very dark, for shadows
        QColor link;            ///< Hyperlink color
        QColor linkVisited;     ///< Visited hyperlink color

        /// @brief Convert to QPalette object
        QPalette toQPalette() const;
    } palette;
};
```

### Key Implementation Details

**toQPalette() method:**
```cpp
QPalette Theme::Palette::toQPalette() const {
    QPalette pal;

    // Set colors for all three color groups (Active, Inactive, Disabled)
    auto setColorForAllGroups = [&pal](QPalette::ColorRole role, const QColor& color) {
        pal.setColor(QPalette::Active, role, color);
        pal.setColor(QPalette::Inactive, role, color);
        // Disabled state: reduce alpha for grayed-out appearance
        QColor disabledColor = color;
        disabledColor.setAlpha(128);
        pal.setColor(QPalette::Disabled, role, disabledColor);
    };

    setColorForAllGroups(QPalette::Window, window);
    setColorForAllGroups(QPalette::WindowText, windowText);
    // ... all 16 color roles ...

    return pal;
}
```

**ThemeManager::applyTheme():**
```cpp
void ThemeManager::applyTheme(const Theme& theme) {
    m_currentTheme = theme;
    m_baseTheme = theme;
    m_overrides.clear();

    // KEY: Apply QPalette to entire application
    QPalette palette = theme.palette.toQPalette();
    QApplication::setPalette(palette);

    // Save to settings
    auto& settings = SettingsManager::getInstance();
    settings.setTheme(theme.name);
    settings.save();

    emit themeChanged(m_currentTheme);
}
```

### QPalette Color Role Reference

| Role | Purpose | Example (Dark) |
|------|---------|----------------|
| `Window` | General background | #2b2b2b |
| `WindowText` | General foreground | #e0e0e0 |
| `Base` | Text entry background | #3c3c3c |
| `AlternateBase` | Alternate rows in views | #323232 |
| `Text` | Text entry foreground | #e0e0e0 |
| `Button` | Button background | #4a4a4a |
| `ButtonText` | Button foreground | #e0e0e0 |
| `Highlight` | Selection highlight | #0078d4 |
| `HighlightedText` | Text on selection | #ffffff |
| `Light` | Lighter than Button (3D effects) | #606060 |
| `Midlight` | Between Button and Light | #505050 |
| `Mid` | Between Button and Dark | #404040 |
| `Dark` | Darker than Button (3D effects) | #202020 |
| `Shadow` | Very dark (shadows) | #000000 |
| `Link` | Hyperlink color | #0078d4 |
| `LinkVisited` | Visited link color | #5856d6 |

### Backward Compatibility

If a theme JSON lacks the "palette" section, colors are auto-generated from the basic "colors" section:

```cpp
if (!json.contains("palette")) {
    bool isDark = isDarkBackground(theme.colors.background);

    theme.palette.window = theme.colors.background;
    theme.palette.windowText = theme.colors.text;
    theme.palette.base = isDark
        ? theme.colors.background.lighter(120)
        : theme.colors.background;
    // ... auto-generate remaining palette colors ...
}
```

### Benefits

1. **Native Widget Support:** All Qt Fusion widgets render correctly (spinbox arrows visible in dark theme)
2. **Single Source of Truth:** Colors defined in JSON, applied system-wide via QPalette
3. **No QSS Needed:** No hardcoded color values in stylesheets
4. **Runtime Switching:** Theme changes apply immediately to all widgets
5. **Disabled State:** Automatic alpha reduction for disabled widgets

### Files

- `include/kalahari/core/theme.h` - Palette struct definition
- `src/core/theme.cpp` - toQPalette() implementation
- `src/core/theme_manager.cpp` - applyTheme() with setPalette()
- `resources/themes/Light.json` - Light theme palette
- `resources/themes/Dark.json` - Dark theme palette

---

**Document Status:** 🔄 IN PROGRESS (Qt conversion + theme system)
**Last Updated:** 2025-11-25
**Version:** 2.3 (QPalette Theme System)
**Updates:**
- **2025-11-25:** Added QPalette Theme System section (Task #00023)
- Panel Catalog updated (Navigator, Statistics Bar, Weekly Statistics, Properties)
- 3-tier Statistics Architecture documented
- Mind Maps & Timelines library integration
- Qt6 implementation patterns
