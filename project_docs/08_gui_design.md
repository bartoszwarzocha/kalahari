# Kalahari - GUI Design Specification

> **Writer's IDE** - Comprehensive GUI architecture and layout design

**Document Version:** 1.0
**Status:** ✅ Complete
**Last Updated:** 2025-10-25
**Phase:** Architecture Phase

---

## Table of Contents

1. [Overview](#overview)
2. [Design Philosophy](#design-philosophy)
3. [Three-Column Layout Architecture](#three-column-layout-architecture)
4. [Panel Catalog](#panel-catalog)
5. [Command Registry System](#command-registry-system)
6. [Customizable Toolbars](#customizable-toolbars)
7. [Plugin Command Integration](#plugin-command-integration)
8. [Perspectives (Workspace Presets)](#perspectives-workspace-presets)
9. [Gamification System](#gamification-system)
10. [Info Bar & Status Bar](#info-bar--status-bar)
11. [Keyboard Shortcuts](#keyboard-shortcuts)
12. [Focus Modes](#focus-modes)
13. [Settings Dialog](#settings-dialog)
14. [wxWidgets Implementation Notes](#wxwidgets-implementation-notes)

---

## Overview

This document defines the complete GUI architecture for Kalahari, a professional Writer's IDE built with wxWidgets and wxAUI (Advanced User Interface).

### Key Features

- **3-column dockable layout** (left tools | center workspace | right tools)
- **wxAUI framework** - fully customizable, drag & drop panels
- **Customizable toolbars** - user-defined command shortcuts
- **Command Registry** - unified command system (core + plugins)
- **4 built-in perspectives** (Writer, Editor, Researcher, Planner)
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

#### 1. Files/Libraries Notebook (default: visible)

**Type:** `wxAuiNotebook` (tabbed)
**Default tabs:** [Files] [Libraries]
**Position:** Top of left column
**Default size:** 250px wide, 50% of left column height

**Tab: Files**
- Tree structure: Book → Parts → Chapters → Scenes
- Icons: Dynamic (📄 chapter, 📋 note, 🖼️ image thumbnail)
- Double-click: Opens in editor (center workspace)
- Right-click context menu:
  - New Chapter
  - New Section
  - Rename (F2)
  - Delete (Del)
  - Move Up/Down
  - Properties...
- Drag & drop: Reorder chapters/sections

**Tab: Libraries**
- Grid/icon view of available libraries
- Libraries vary by book type:
  - **Novel:** Characters, Locations, Items, Timeline, Plot Threads
  - **Non-fiction:** People, Places, Sources, References, Timeline
  - **Screenplay:** Characters, Locations, Scenes, Acts, Props
  - **Reportage:** People, Places, Events, Sources, Interviews
  - **Historical:** People, Places, Events, Timeline, Sources
- Double-click library: Opens library manager in center workspace
- User can add/remove/create custom libraries

**Implementation:**
```cpp
class FilesLibrariesPanel : public wxPanel {
private:
    wxAuiNotebook* m_notebook;
    wxTreeCtrl* m_filesTree;
    wxScrolledWindow* m_librariesGrid;
    wxImageList* m_iconList;  // 16x16, 24x24, 32x32

public:
    void LoadLibraries(BookType type);
    void AddLibrary(const wxString& name, const wxString& iconPath);
    void RemoveLibrary(const wxString& name);

    // Dynamic icon generation
    wxBitmap GenerateThumbnail(const wxString& filePath, int size);
    wxBitmap GenerateCharacterIcon(const Character& character);
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

#### 3. Daily Statistics Panel (default: minimized)

**Type:** Horizontal panel
**Position:** `wxAUI_DOCK_TOP` (center region, above editor tabs)
**Default state:** Minimized (single line)

**Content (Minimized):**
```
▓▓▓▓▓░░░░░ 850/1000 words (85%) | 📈 Week: ▁▃▅▇█▆▄ | 🔥 7 days | ⏱️ 45min
```

**Content (Expanded):**
```
┌─────────────────────────────────────────────────────┐
│ TODAY'S PROGRESS                                     │
│ ▓▓▓▓▓▓▓▓▓░░░░░░░░░░ 850/1000 words (85%)           │
│                                                      │
│ Week Overview:        Speed:        Streak:         │
│  Mo  850 ▇▇▇▇▇       45 wpm        🔥 7 days       │
│  Tu 1200 ████████                                   │
│  We  750 ▇▇▇▇        Session:       Total:          │
│  Th  920 ▇▇▇▇▇       45 min         12,543 words   │
│  Fr  850 ▇▇▇▇▇                                      │
│  Sa    0             Reading time:                  │
│  Su    0             6 minutes                      │
└─────────────────────────────────────────────────────┘
```

**Toggle:** Click header to expand/collapse

**Implementation:**
```cpp
class DailyStatsPanel : public wxPanel {
public:
    void SetExpanded(bool expanded);
    void UpdateStats(int wordsToday, int wordsGoal, int streak);
    void UpdateWeeklyData(const std::vector<int>& dailyWords);

private:
    wxGauge* m_progressBar;
    wxStaticText* m_statsText;
    wxPanel* m_chartPanel;  // Weekly sparkline
    bool m_expanded = false;

    void DrawSparkline(wxDC& dc, const std::vector<int>& data);
};
```

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

#### 6. Statistics/Challenges Panel (default: visible)

**Type:** Custom panel
**Position:** Top of right column
**Default size:** 250px wide, 40% of right column height

**Content:**
```
┌─────────────────────────┐
│ 📊 TODAY'S STATS        │
├─────────────────────────┤
│ Words: 850              │
│ Characters: 4,523       │
│ Reading time: 4 min     │
│                         │
│ 🔥 STREAK               │
│ 7 days in a row!        │
│                         │
│ 🎯 ACTIVE CHALLENGES    │
│ ┌─────────────────────┐ │
│ │ 5K This Week        │ │
│ │ ▓▓▓▓░░░ 3,200/5,000 │ │
│ └─────────────────────┘ │
│                         │
│ 🏆 RECENT BADGES        │
│ [🌅] [🏃] [📝] [💪]    │
│                         │
│ [View All Achievements] │
└─────────────────────────┘
```

**Click Interactions:**
- Click challenge → Shows detailed progress
- Click badge → Shows badge description & date earned
- Click "View All" → Opens achievements panel in center

**Implementation:**
```cpp
class StatsChallengesPanel : public wxPanel {
public:
    void UpdateTodayStats(int words, int chars, int readingTime);
    void UpdateStreak(int days);
    void AddChallenge(const Challenge& challenge);
    void UpdateChallengeProgress(int challengeId, int current, int total);
    void ShowBadge(const Badge& badge);

private:
    wxStaticText* m_statsText;
    wxStaticText* m_streakText;
    wxPanel* m_challengesPanel;
    wxGridSizer* m_badgeGrid;  // Recent badges (max 8)
};
```

---

#### 7. Character/Item Preview Panel (default: hidden, context-sensitive)

**Type:** Custom panel
**Position:** Middle of right column
**Name:** "Context Preview" or "Quick Preview"

**Content:**
```
┌─────────────────────────┐
│ 👤 JOHN DOE             │
├─────────────────────────┤
│ [Photo/Generated Icon]  │
│                         │
│ Age: 42                 │
│ Role: Detective         │
│ Traits: Cynical, Smart  │
│                         │
│ Brief: Veteran NYPD     │
│ detective investigating │
│ the serial murders...   │
│                         │
│ [Open Full View]        │
└─────────────────────────┘
```

**When visible:**
- User clicks character mention in text
- User clicks character in library
- User selects "Show in Context Panel" from context menu
- Auto-show when configured in settings

**Auto-hide:**
- No selection (after 30s)
- User clicks × close button
- User disables in settings

**Implementation:**
```cpp
class ContextPreviewPanel : public wxPanel {
public:
    enum PreviewType { CHARACTER, LOCATION, ITEM, NONE };

    void ShowPreview(PreviewType type, const Entity& entity);
    void Hide();
    void SetAutoHideDelay(int milliseconds);

private:
    wxStaticBitmap* m_icon;
    wxHtmlWindow* m_contentHtml;
    wxButton* m_openFullButton;
    wxTimer m_autoHideTimer;

    PreviewType m_currentType = NONE;
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

## wxWidgets Implementation Notes

### wxAUI Panel Management

```cpp
// Adding panel with wxAUI
m_auiMgr.AddPane(m_filesPanel,
    wxAuiPaneInfo()
        .Name("files")
        .Caption("Files")
        .Left()
        .Layer(1)
        .Position(0)
        .MinSize(200, -1)
        .BestSize(250, -1)
        .CloseButton(true)
        .MaximizeButton(false)
        .PinButton(true));

m_auiMgr.Update();
```

### Perspective Saving

```cpp
void MainFrame::SavePerspective(const wxString& name) {
    wxString perspective = m_auiMgr.SavePerspective();

    // Save to JSON
    nlohmann::json j;
    j["name"] = name.ToStdString();
    j["perspective"] = perspective.ToStdString();
    j["toolbars"] = ToolbarManager::Instance().SaveState();

    std::ofstream file(GetPerspectivePath(name));
    file << j.dump(2);
}

void MainFrame::LoadPerspective(const wxString& name) {
    std::ifstream file(GetPerspectivePath(name));
    nlohmann::json j = nlohmann::json::parse(file);

    wxString perspective(j["perspective"].get<std::string>());
    m_auiMgr.LoadPerspective(perspective);

    ToolbarManager::Instance().LoadState(j["toolbars"]);

    m_auiMgr.Update();
}
```

### Focus Mode Implementation

```cpp
void MainFrame::SetFocusMode(FocusMode mode) {
    switch (mode) {
        case FocusMode::NORMAL:
            ShowFullScreen(false);
            GetMenuBar()->Show();
            m_auiMgr.GetPane("files").Show();
            m_auiMgr.GetPane("assistant").Show();
            // ... show all panels
            break;

        case FocusMode::FOCUSED:
            // Dim panels instead of hiding
            m_filesPanel->SetTransparent(128);  // 50% opacity
            m_assistantPanel->SetTransparent(128);
            ToolbarManager::Instance().HideAll();
            break;

        case FocusMode::DISTRACTION_FREE:
            ShowFullScreen(true, wxFULLSCREEN_ALL);
            GetMenuBar()->Hide();
            GetStatusBar()->Hide();
            // Hide all panels except editor
            m_auiMgr.GetPane("files").Hide();
            m_auiMgr.GetPane("assistant").Hide();
            // ...
            break;
    }

    m_auiMgr.Update();
    m_currentFocusMode = mode;
}
```

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

- **[03_architecture.md](03_architecture.md)** - Core architecture patterns
- **[04_plugin_system.md](04_plugin_system.md)** - Plugin API details
- **[10_branding.md](10_branding.md)** - Colors, fonts, icons
- **[02_tech_stack.md](02_tech_stack.md)** - wxWidgets version & libraries

---

## Implementation Phases

| Feature | Phase | Priority |
|---------|-------|----------|
| Basic layout (3 columns) | Phase 1 | High |
| Core panels (Files, Editor, Assistant) | Phase 1 | High |
| Command Registry | Phase 0 | Critical |
| Default toolbars (6) | Phase 1 | High |
| Perspectives (4 default) | Phase 2 | Medium |
| Customizable toolbars UI | Phase 2 | Medium |
| Quick Access Toolbar | Phase 2 | Medium |
| Gamification (badges, challenges) | Phase 3 | Low |
| Live Customization Mode | Phase 3 | Low |
| Command Palette | Phase 3 | Low |
| Focus Modes | Phase 2 | High |
| Info Bar / Status Bar | Phase 1 | High |

---

**Document Status:** ✅ Complete
**Last Updated:** 2025-10-25
**Version:** 1.0
