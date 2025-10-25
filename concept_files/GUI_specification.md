# Kalahari - GUI Specification

**Version:** 1.0
**Date:** 2025-10-24
**Status:** Confirmed

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Main Window Structure](#main-window-structure)
4. [Panels Specification](#panels-specification)
5. [Menu System](#menu-system)
6. [Toolbar](#toolbar)
7. [Status Bar](#status-bar)
8. [Perspectives (Layouts)](#perspectives-layouts)
9. [Themes](#themes)
10. [Keyboard Shortcuts](#keyboard-shortcuts)
11. [Accessibility](#accessibility)
12. [Implementation Notes](#implementation-notes)

---

## Overview

### Design Philosophy

Kalahari follows a **professional IDE-style interface** with these core principles:

- **Dockable everything** - All panels can be moved, resized, closed, reopened
- **Configurable layouts** - Save and restore custom perspectives
- **Native look & feel** - wxPython provides platform-native rendering
- **Keyboard-first** - Every action accessible via keyboard
- **Non-intrusive** - Tools available when needed, hidden when not

### Target Resolution

- **Minimum:** 1366x768
- **Recommended:** 1920x1080 or higher
- **Multi-monitor:** Full support

### Framework

- **GUI:** wxPython 4.2+ with wxAUI (Advanced User Interface)
- **Rendering:** Native OS controls (Windows: Win32, macOS: Cocoa, Linux: GTK3)

---

## Architecture

### wxAUI System

Kalahari uses **wxAUI** (wx.aui.AuiManager) for panel management:

```python
# Pseudo-code structure
class MainFrame(wx.Frame):
    def __init__(self):
        self.aui_manager = wx.aui.AuiManager(self)

        # Add panels
        self.aui_manager.AddPane(
            self.project_panel,
            wx.aui.AuiPaneInfo()
                .Name("project")
                .Caption("Project Navigator")
                .Left()
                .MinSize(200, -1)
                .BestSize(250, -1)
                .CloseButton(True)
                .MaximizeButton(True)
        )

        # ... more panels

        self.aui_manager.Update()
```

### Panel Communication

Panels communicate via **wxPython events**:

```python
# Custom event for panel communication
EVT_DOCUMENT_CHANGED = wx.NewEventType()
EVT_CHARACTER_SELECTED = wx.NewEventType()
EVT_ASSISTANT_MESSAGE = wx.NewEventType()
```

---

## Main Window Structure

### ASCII Layout

```
┌─────────────────────────────────────────────────────────────────────┐
│ Menu Bar:  [File] [Edit] [View] [Project] [Tools] [Help]            │
├─────────────────────────────────────────────────────────────────────┤
│ Toolbar:   [New] [Save] [Undo] [Redo] │ [B] [I] [U] │ [Export] ...  │
├─────────────┬───────────────────────────────────┬───────────────────┤
│             │                                   │                   │
│  Project    │                                   │   Preview /       │
│  Navigator  │         EDITOR                    │   Inspector       │
│             │   (wx.richtext.RichTextCtrl)      │                   │
│  ┌────────┐ │                                   │  ┌──────────────┐ │
│  │Chapters│ │   [Document Tabs]                 │  │ Live Preview │ │
│  │Scenes  │ │                                   │  └──────────────┘ │
│  │Chars   │ │   Lorem ipsum dolor sit amet...   │                   │
│  │Locs    │ │   consectetur adipiscing elit.    │   Properties      │
│  │Sources │ │                                   │  ┌──────────────┐ │
│  └────────┘ │                                   │  │ Font: Arial  │ │
│             │                                   │  │ Size: 12pt   │ │
│             │                                   │  └──────────────┘ │
│             │                                   ├───────────────────┤
│             │                                   │   Assistant       │
│             │                                   │  ┌──────────────┐ │
│             │                                   │  │  [Lion 🦁]   │ │
│             │                                   │  │   200x200    │ │
│             │                                   │  │ ╭──────────╮ │ │
│             │                                   │  │ │Speech    │ │ │
│             │                                   │  │ │bubble    │ │ │
│             │                                   │  │ ╰──────────╯ │ │
│             │                                   │  └──────────────┘ │
├─────────────┴───────────────────────────────────┴───────────────────┤
│ Output/Statistics Panel                                              │
│  [Stats] [Timeline] [Notes] [Progress]                               │
│  Words: 1,547 / 3,000 (51.6%)  ━━━━━━━━━━░░░░░░░░░░                 │
└─────────────────────────────────────────────────────────────────────┘
│ Status Bar: Chapter_03.rtf │ Autosave: ✓ │ Words: 1,547 │ 67% │ Ln:45│
└─────────────────────────────────────────────────────────────────────┘
```

### Window Properties

```python
class MainFrame(wx.Frame):
    TITLE = "Kalahari - Writer's IDE"
    MIN_SIZE = (1024, 600)
    DEFAULT_SIZE = (1280, 800)

    # Icon
    ICON_PATH = "assets/icons/kalahari.ico"  # Windows
    ICON_PATH_MAC = "assets/icons/kalahari.icns"  # macOS
    ICON_PATH_LINUX = "assets/icons/kalahari.png"  # Linux
```

---

## Panels Specification

### 1. Project Navigator (Left Panel)

**Default:**
- **Position:** Left
- **Size:** 250px width
- **Min Size:** 150px
- **Max Size:** 500px

**Components:**

```
┌──────────────────────────┐
│ [Search: 🔍______]       │ ← Quick filter
├──────────────────────────┤
│ 📚 Book Project          │ ← Tree view
│ ├─ 📁 Part I             │
│ │  ├─ 📄 Chapter 1       │
│ │  ├─ 📄 Chapter 2       │
│ │  └─ 📄 Chapter 3       │
│ ├─ 📁 Part II            │
│ └─ 📁 Part III           │
│                          │
│ 👥 Characters (15)       │ ← Collapsible sections
│ ├─ 👤 John Smith         │
│ ├─ 👤 Jane Doe           │
│ └─ ...                   │
│                          │
│ 📍 Locations (8)         │
│ 📚 Sources (23)          │
│ 📅 Calendar              │
└──────────────────────────┘
```

**Tabs:**
- **Project** (default) - Tree view of book structure
- **Characters** - Character bank list
- **Locations** - Location bank list
- **Sources** - Source library
- **Calendar** - Writer's calendar (Phase 2)

**Context Menus:**
- Right-click on items → New, Delete, Rename, Duplicate, Properties
- Drag & drop support for reorganization

**Implementation:**
```python
class ProjectNavigatorPanel(wx.Panel):
    def __init__(self, parent):
        super().__init__(parent)

        # Search bar
        self.search_ctrl = wx.SearchCtrl(self)

        # Tree control
        self.tree = wx.TreeCtrl(self, style=wx.TR_DEFAULT_STYLE | wx.TR_EDIT_LABELS)

        # Icons
        self.image_list = wx.ImageList(16, 16)
        self.tree.SetImageList(self.image_list)

        # Tabs
        self.notebook = wx.Notebook(self)
```

---

### 2. Central Editor (Main Panel)

**Default:**
- **Position:** Center
- **Size:** Flexible (remaining space)

**Components:**

```
┌────────────────────────────────────────────┐
│ [Chapter_01.rtf] [Chapter_02.rtf] [×] [+] │ ← Tab bar
├────────────────────────────────────────────┤
│ [B] [I] [U] [Align] [H1▼] [Font▼] [12pt▼]│ ← Format toolbar
├────────────────────────────────────────────┤
│                                            │
│   Lorem ipsum dolor sit amet,              │ ← Editor
│   consectetur adipiscing elit.             │
│                                            │
│   Sed do eiusmod tempor incididunt         │
│   ut labore et dolore magna aliqua.        │
│                                            │
│                                            │
│                                         [≡] │ ← Minimap (opt)
└────────────────────────────────────────────┘
```

**Editor Features:**
- **Type:** `wx.richtext.RichTextCtrl`
- **Formatting:** Bold, Italic, Underline, Strikethrough
- **Headings:** H1-H6
- **Lists:** Bulleted, Numbered
- **Alignment:** Left, Center, Right, Justify
- **Styles:** Custom paragraph styles
- **Footnotes:** Inline footnote support
- **Spell check:** Underline misspelled words
- **Word wrap:** Enabled by default
- **Line numbers:** Optional (togglable)

**Tab Management:**
```python
class EditorNotebook(wx.aui.AuiNotebook):
    def __init__(self, parent):
        super().__init__(parent, style=
            wx.aui.AUI_NB_TOP |
            wx.aui.AUI_NB_TAB_SPLIT |
            wx.aui.AUI_NB_TAB_MOVE |
            wx.aui.AUI_NB_CLOSE_ON_ACTIVE_TAB
        )
```

**Focus Modes:**

1. **Normal Mode** - All panels visible
2. **Focused Mode** (F9) - Editor + Assistant only
3. **Distraction-Free Mode** (F11) - Editor fullscreen

```python
def enter_focus_mode(self):
    # Hide all panels except editor and assistant
    for pane in self.aui_manager.GetAllPanes():
        if pane.name not in ['editor', 'assistant']:
            pane.Hide()
    self.aui_manager.Update()

def enter_distraction_free_mode(self):
    # Fullscreen, hide everything except editor
    self.ShowFullScreen(True)
    for pane in self.aui_manager.GetAllPanes():
        if pane.name != 'editor':
            pane.Hide()
    self.aui_manager.Update()
```

---

### 3. Preview/Inspector (Right Top Panel)

**Default:**
- **Position:** Right top
- **Size:** 300px width, 50% height
- **Phase:** 2 (not in MVP)

**Tabs:**

#### 3.1 Preview Tab
```
┌────────────────────────┐
│ [Format: DOCX style ▼] │ ← Format selector
├────────────────────────┤
│                        │
│  Chapter 1             │ ← Rendered preview
│                        │
│  Lorem ipsum dolor     │
│  sit amet, con-        │
│  sectetur adipi-       │
│  scing elit.           │
│                        │
└────────────────────────┘
```

**Features:**
- Live HTML rendering of current document
- Format selection (DOCX style, EPUB style, PDF preview)
- Auto-scroll sync with editor

#### 3.2 Properties Tab
```
┌────────────────────────┐
│ Selection Properties   │
├────────────────────────┤
│ Font: Arial            │
│ Size: 12pt             │
│ Weight: Bold           │
│ Style: Normal          │
│ Color: #000000         │
│                        │
│ Paragraph:             │
│ Align: Left            │
│ Spacing: 1.5           │
└────────────────────────┘
```

---

### 4. Assistant Panel (Right Bottom Panel)

**Default:**
- **Position:** Right bottom
- **Size:** 300px width, 350px height
- **Min Size:** 250x300
- **Collapsible:** Yes (can minimize to icon)

**Layout:**

```
┌────────────────────────┐
│  [Settings ⚙]  [×]     │ ← Header
├────────────────────────┤
│                        │
│      [Lion Avatar]     │ ← 200x200px
│       Neutral          │
│                        │
├────────────────────────┤
│  ╭──────────────────╮  │
│  │ Great progress   │  │ ← Speech bubble
│  │ today! Keep it   │  │
│  │ up! 🦁           │  │
│  ╰──────────────────╯  │
└────────────────────────┘
```

**Components:**
- **Avatar Image:** wx.StaticBitmap (200x200px)
- **Speech Bubble:** wx.html.HtmlWindow (for rich text + emoji)
- **Settings Button:** Opens assistant configuration dialog

**Mood States:**
1. Neutral (default)
2. Happy/Pleased
3. Encouraging
4. Worried
5. Excited
6. Tired
7. Proud
8. Annoyed (optional, configurable)

**Speech Bubble Behavior:**
- **Fade-in:** 200ms animation
- **Auto-hide:** After 10 seconds (configurable)
- **Manual dismiss:** Click to close
- **History:** Keep last 5 messages (accessible via settings)

**Implementation:**
```python
class AssistantPanel(wx.Panel):
    def __init__(self, parent, animal="lion"):
        super().__init__(parent)

        # Avatar
        self.avatar = wx.StaticBitmap(self)
        self.load_avatar(animal, mood="neutral")

        # Speech bubble
        self.speech_bubble = wx.html.HtmlWindow(self)
        self.speech_bubble.SetPage(self.format_message(""))

        # Timer for auto-hide
        self.hide_timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_hide_timer)

    def show_message(self, text, mood="neutral", duration=10000):
        self.load_avatar(self.current_animal, mood)
        self.speech_bubble.SetPage(self.format_message(text))
        self.speech_bubble.Show()
        self.hide_timer.Start(duration, wx.TIMER_ONE_SHOT)
```

---

### 5. Output/Statistics Panel (Bottom Panel)

**Default:**
- **Position:** Bottom
- **Size:** Full width, 180px height
- **Min Size:** 150px height

**Tabs:**

#### 5.1 Statistics Tab
```
┌──────────────────────────────────────────────────────────────┐
│ Words: 1,547 / 3,000 (51.6%)  ━━━━━━━━━━░░░░░░░░░░          │
│ Characters: 8,921 (with spaces) │ 7,234 (without)           │
│ Pages: 6.2 (A4, 12pt, single-spaced)                        │
│ Reading time: ~7 minutes                                     │
│                                                              │
│ [Chart: Words per day]                                       │
│    ▄▄                                                         │
│   ███▄                                                        │
│  ██████▄▄                                                     │
│ ██████████▄                                                   │
└──────────────────────────────────────────────────────────────┘
```

**Widgets:**
- Progress bars (wx.Gauge)
- Text stats labels
- Charts (matplotlib embedded via wx backend)

#### 5.2 Timeline Tab (Phase 2)
```
┌──────────────────────────────────────────────────────────────┐
│ [Year 1800]────●─────────────●────────────●────[Year 1850]   │
│              Event 1       Event 2      Event 3              │
│                                                              │
│ Event 1: John arrives in London (Ch. 3)                      │
│ Event 2: Battle of Waterloo (Ch. 7)                          │
│ Event 3: Jane meets Thomas (Ch. 12)                          │
└──────────────────────────────────────────────────────────────┘
```

#### 5.3 Notes Tab
```
┌──────────────────────────────────────────────────────────────┐
│ [+ New Note]  [🏷 All Tags ▼]  [🔍 Search...]                │
├──────────────────────────────────────────────────────────────┤
│ 📝 Research Lord Byron's works                               │
│    Added: 2025-10-24 14:30  Tags: #research #history         │
│                                                              │
│ 📝 Check date of Battle of Leipzig                           │
│    Added: 2025-10-23 09:15  Tags: #factcheck                 │
└──────────────────────────────────────────────────────────────┘
```

#### 5.4 Progress Tab
```
┌──────────────────────────────────────────────────────────────┐
│ Daily Goal: 2,000 words                                      │
│ Weekly Goal: 10,000 words                                    │
│                                                              │
│ Mon: ███████████░░░░░░░░░░  1,847 / 2,000                   │
│ Tue: █████████████████████  2,156 / 2,000 ✓                 │
│ Wed: ███░░░░░░░░░░░░░░░░░░    543 / 2,000                   │
│ Thu: [Today]                                                 │
└──────────────────────────────────────────────────────────────┘
```

---

### 6. Calendar Panel (Phase 2)

**Default:**
- **Position:** Tab in Project Navigator or floating
- **Size:** 280x320

**Layout:**
```
┌────────────────────────────┐
│ ◄ October 2025 ►           │
├────────────────────────────┤
│ Mo Tu We Th Fr Sa Su       │
│     1  2  3  4  5  6       │
│  7  8  9 10 11 12 13       │
│ 14 15 16 17 18 19 20       │
│ 21 22 23 24 [25] 26 27     │
│ 28 29 30 31                │
├────────────────────────────┤
│ Today's Goals:             │
│ ☐ Write 2,000 words        │
│ ☐ Finish Chapter 5         │
│ ☑ Research Victorian era   │
└────────────────────────────┘
```

---

## Menu System

### [File] Menu

```
File
├─ New Project...          Ctrl+N
├─ Open Project...         Ctrl+O
├─ Save                    Ctrl+S
├─ Save As...              Ctrl+Shift+S
├─ Close Project
├─ ──────────────────────
├─ Import...
│  ├─ From Scrivener...
│  ├─ From yWriter...
│  ├─ From DOCX...
│  └─ From Markdown...
├─ Export...               Ctrl+E
│  ├─ To DOCX...
│  ├─ To PDF...
│  ├─ To TXT...
│  └─ To EPUB...          (Phase 2)
├─ ──────────────────────
├─ Recent Projects ►
│  ├─ My Novel.klh
│  ├─ Thriller.klh
│  └─ ...
├─ ──────────────────────
└─ Exit                    Ctrl+Q
```

### [Edit] Menu

```
Edit
├─ Undo                    Ctrl+Z
├─ Redo                    Ctrl+Y
├─ ──────────────────────
├─ Cut                     Ctrl+X
├─ Copy                    Ctrl+C
├─ Paste                   Ctrl+V
├─ Paste Special...        Ctrl+Shift+V
├─ ──────────────────────
├─ Select All              Ctrl+A
├─ ──────────────────────
├─ Find...                 Ctrl+F
├─ Replace...              Ctrl+H
├─ Find in Project...      Ctrl+Shift+F
├─ Go to Line...           Ctrl+G
├─ ──────────────────────
└─ Preferences...          Ctrl+,
```

### [View] Menu

```
View
├─ Panels ►
│  ├─ Project Navigator    Ctrl+1  [✓]
│  ├─ Preview/Inspector    Ctrl+2  [✓]
│  ├─ Assistant            Ctrl+3  [✓]
│  ├─ Statistics           Ctrl+4  [✓]
│  └─ Output               Ctrl+5  [✓]
├─ ──────────────────────
├─ Focus Mode              F9
├─ Distraction-Free Mode   F11
├─ Fullscreen              Alt+Enter
├─ ──────────────────────
├─ Layout ►
│  ├─ Default Layout
│  ├─ Writer Layout
│  ├─ Editor Layout
│  ├─ Research Layout
│  ├─ ──────────────────
│  ├─ Save Current Layout...
│  └─ Manage Layouts...
├─ ──────────────────────
├─ Zoom In                 Ctrl++
├─ Zoom Out                Ctrl+-
└─ Zoom Reset              Ctrl+0
```

### [Project] Menu

```
Project
├─ Add Chapter...          Ctrl+Alt+C
├─ Add Scene...            Ctrl+Alt+S
├─ ──────────────────────
├─ New Character...        Ctrl+Alt+P
├─ New Location...         Ctrl+Alt+L
├─ New Source...           Ctrl+Alt+R
├─ ──────────────────────
├─ Project Properties...
├─ Project Statistics...
├─ ──────────────────────
└─ Compile/Export...       Ctrl+Shift+E
```

### [Tools] Menu

```
Tools
├─ Spell Check             F7
├─ Grammar Check           Shift+F7    (Phase 2)
├─ ──────────────────────
├─ Assistant Settings...
├─ Writing Sprint...
├─ ──────────────────────
├─ Generate Name...        (Phase 2+)
├─ Thesaurus...            (Phase 2)
├─ ──────────────────────
└─ Options...
```

### [Help] Menu

```
Help
├─ Help Contents           F1
├─ Tutorials...
├─ ──────────────────────
├─ Check for Updates...
├─ Report Bug...
├─ ──────────────────────
└─ About Kalahari...
```

---

## Toolbar

### Main Toolbar (Default)

```
┌────────────────────────────────────────────────────────────────┐
│ [New] [Open] [Save] │ [Undo] [Redo] │ [Cut] [Copy] [Paste] │   │
│ [Bold] [Italic] [Underline] │ [◄] [═] [►] [≡] │ [Find] [Export]│
│ [Focus Mode] [Assistant] [Settings]                            │
└────────────────────────────────────────────────────────────────┘
```

**Icon Sizes:** 32x32px (configurable: 16/24/32/48)

**Groups:**

1. **File Operations**
   - New Project (Ctrl+N)
   - Open Project (Ctrl+O)
   - Save (Ctrl+S)

2. **Edit Operations**
   - Undo (Ctrl+Z)
   - Redo (Ctrl+Y)

3. **Clipboard**
   - Cut (Ctrl+X)
   - Copy (Ctrl+C)
   - Paste (Ctrl+V)

4. **Formatting**
   - Bold (Ctrl+B)
   - Italic (Ctrl+I)
   - Underline (Ctrl+U)

5. **Alignment**
   - Align Left (Ctrl+L)
   - Align Center (Ctrl+E)
   - Align Right (Ctrl+R)
   - Justify (Ctrl+J)

6. **Tools**
   - Find (Ctrl+F)
   - Export (Ctrl+E)

7. **Mode**
   - Focus Mode (F9)
   - Assistant Toggle (Ctrl+3)
   - Settings (Ctrl+,)

**Customization:**
- Right-click on toolbar → Customize
- Drag & drop to reorganize
- Add/remove icons
- Show/hide labels
- Icon size selection

---

## Status Bar

### Layout

```
┌────────────────────────────────────────────────────────────────────┐
│ [Document: Chapter_03.rtf] │ [Autosave: ✓] │ [Words: 1,547] │      │
│ [Chars: 8,921] │ [Progress: 67%] │ [Line: 45, Col: 12] │ [PL] │ [RT]│
└────────────────────────────────────────────────────────────────────┘
```

### Fields (Left to Right)

1. **Document Name**
   - Current document filename
   - Clickable → opens document properties

2. **Autosave Indicator**
   - ✓ Green checkmark: Saved
   - ⏳ Yellow hourglass: Saving...
   - ✗ Red X: Error

3. **Word Count**
   - Live update as user types
   - Click → opens statistics panel

4. **Character Count**
   - With spaces / without spaces
   - Tooltip shows both

5. **Progress**
   - Percentage vs daily goal
   - Click → opens progress panel

6. **Cursor Position**
   - Line number, Column number
   - Click → "Go to Line" dialog

7. **Language**
   - Current spell check language (EN/PL/DE/etc.)
   - Click → change language

8. **Edit Mode**
   - RT (Rich Text) / MD (Markdown)
   - Click → toggle mode (Phase 2)

**Implementation:**
```python
class StatusBar(wx.StatusBar):
    def __init__(self, parent):
        super().__init__(parent)
        self.SetFieldsCount(8)
        self.SetStatusWidths([200, 100, 80, 80, 80, 100, 40, 40])

    def update_word_count(self, count):
        self.SetStatusText(f"Words: {count:,}", 2)
```

---

## Perspectives (Layouts)

### Concept

**Perspectives** are saved window layouts (panel positions, sizes, visibility).

### Built-in Perspectives

#### 1. Default Perspective
- All panels visible
- Standard 3-column layout (Project | Editor | Preview+Assistant)
- Bottom panel: Statistics

#### 2. Writer Perspective
- Project Navigator (left)
- Editor (center, maximized)
- Assistant (right, narrow)
- Statistics (bottom, minimized)
- Preview hidden

#### 3. Editor Perspective
- Only Editor visible
- Minimal toolbars
- Status bar only

#### 4. Research Perspective
- Sources Library (left)
- Editor (center)
- Preview (right, larger)
- Notes (bottom)

### Custom Perspectives

**Save Current Layout:**
```
View → Layout → Save Current Layout...
→ Dialog: "Enter layout name: _________"
→ Saved to: ~/.kalahari/layouts/custom_name.layout
```

**Switch Layouts:**
```
View → Layout → [Layout Name]
or
Ctrl+1, Ctrl+2, Ctrl+3, Ctrl+4 (first 4 layouts)
```

**Manage Layouts:**
```
View → Layout → Manage Layouts...
→ Dialog with list:
   - Default (built-in, cannot delete)
   - Writer (built-in)
   - Editor (built-in)
   - Research (built-in)
   - My Layout 1 [Edit] [Delete]
   - My Layout 2 [Edit] [Delete]
```

**Implementation:**
```python
def save_perspective(self, name):
    perspective = self.aui_manager.SavePerspective()
    config = {
        'name': name,
        'perspective': perspective,
        'timestamp': datetime.now().isoformat()
    }

    with open(f'layouts/{name}.json', 'w') as f:
        json.dump(config, f, indent=2)

def load_perspective(self, name):
    with open(f'layouts/{name}.json', 'r') as f:
        config = json.load(f)

    self.aui_manager.LoadPerspective(config['perspective'])
    self.aui_manager.Update()
```

---

## Themes

### Built-in Themes (Phase 2)

#### 1. Light Theme (Default)
- Background: #FFFFFF
- Text: #000000
- Accent: #D97642 (warm orange)
- Panel background: #F5F5F5

#### 2. Dark Theme
- Background: #1E1E1E
- Text: #D4D4D4
- Accent: #E6D5B8 (sandy beige)
- Panel background: #252526

#### 3. Sepia Theme
- Background: #F4ECD8
- Text: #3E2723
- Accent: #8B3A3A (sunset red)
- Panel background: #FFF8DC

#### 4. Africa Theme
- Background: #FFF8DC (cream)
- Text: #3E2723 (dark brown)
- Accent: #D97642 (warm orange)
- Panel background: #E6D5B8 (sandy beige)
- Highlights: #6B8E23 (savanna green)

### Theme Structure

```json
{
  "name": "Dark",
  "version": "1.0",
  "colors": {
    "background": "#1E1E1E",
    "foreground": "#D4D4D4",
    "accent": "#E6D5B8",
    "panel_background": "#252526",
    "border": "#3E3E42",
    "selection": "#264F78",
    "link": "#3794FF"
  },
  "editor": {
    "background": "#1E1E1E",
    "foreground": "#D4D4D4",
    "caret": "#FFFFFF",
    "selection": "#264F78"
  },
  "syntax": {
    "bold": "#FFFFFF",
    "italic": "#C586C0",
    "heading": "#4EC9B0"
  }
}
```

**Location:** `~/.kalahari/themes/`

---

## Keyboard Shortcuts

### Global

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| New Project | Ctrl+N | |
| Open Project | Ctrl+O | |
| Save | Ctrl+S | |
| Save As | Ctrl+Shift+S | |
| Close Project | Ctrl+W | |
| Export | Ctrl+E | |
| Exit | Ctrl+Q | Alt+F4 |

### Edit

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| Undo | Ctrl+Z | |
| Redo | Ctrl+Y | Ctrl+Shift+Z |
| Cut | Ctrl+X | Shift+Del |
| Copy | Ctrl+C | Ctrl+Insert |
| Paste | Ctrl+V | Shift+Insert |
| Select All | Ctrl+A | |
| Find | Ctrl+F | |
| Replace | Ctrl+H | |
| Find in Project | Ctrl+Shift+F | |
| Go to Line | Ctrl+G | |

### Formatting

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| Bold | Ctrl+B | |
| Italic | Ctrl+I | |
| Underline | Ctrl+U | |
| Align Left | Ctrl+L | |
| Align Center | Ctrl+E | |
| Align Right | Ctrl+R | |
| Justify | Ctrl+J | |
| Increase Font | Ctrl++ | Ctrl+Scroll Up |
| Decrease Font | Ctrl+- | Ctrl+Scroll Down |

### View

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| Toggle Project | Ctrl+1 | |
| Toggle Preview | Ctrl+2 | |
| Toggle Assistant | Ctrl+3 | |
| Toggle Stats | Ctrl+4 | |
| Toggle Output | Ctrl+5 | |
| Focus Mode | F9 | |
| Distraction-Free | F11 | |
| Fullscreen | Alt+Enter | |
| Zoom In | Ctrl++ | |
| Zoom Out | Ctrl+- | |
| Zoom Reset | Ctrl+0 | |

### Project

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| New Chapter | Ctrl+Alt+C | |
| New Scene | Ctrl+Alt+S | |
| New Character | Ctrl+Alt+P | |
| New Location | Ctrl+Alt+L | |
| New Source | Ctrl+Alt+R | |

### Tools

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| Spell Check | F7 | |
| Grammar Check | Shift+F7 | |
| Preferences | Ctrl+, | |

### Help

| Action | Shortcut | Alternative |
|--------|----------|-------------|
| Help Contents | F1 | |

### Custom Shortcuts

Users can customize shortcuts in:
```
Tools → Options → Keyboard Shortcuts
```

**Format:** JSON file at `~/.kalahari/shortcuts.json`

---

## Accessibility

### Screen Reader Support

- **Windows:** NVDA, JAWS compatible
- **macOS:** VoiceOver compatible
- **Linux:** Orca compatible

**Implementation:**
```python
# Set accessible names for all controls
self.project_tree.SetAccessibleName("Project Navigator Tree")
self.editor.SetAccessibleName("Text Editor")
self.assistant_panel.SetAccessibleName("Writing Assistant")
```

### High Contrast Mode

Automatically detects system high contrast settings:
```python
def check_high_contrast(self):
    if wx.SystemSettings.GetAppearance().IsUsingDarkBackground():
        self.apply_dark_theme()

    if wx.SystemSettings.GetColour(wx.SYS_COLOUR_WINDOWTEXT) == wx.BLACK:
        # High contrast mode detected
        self.apply_high_contrast_theme()
```

### Keyboard Navigation

- **Tab:** Navigate between controls
- **Shift+Tab:** Navigate backwards
- **Arrow keys:** Navigate within trees/lists
- **Space/Enter:** Activate buttons/items
- **Escape:** Close dialogs/panels

### Font Scaling

- Respects system DPI settings
- Minimum font size: 8pt
- Maximum font size: 72pt
- DPI aware on Windows/Linux

---

## Implementation Notes

### wxPython Version

```python
MIN_WXPYTHON_VERSION = "4.2.0"

import wx
if wx.version() < MIN_WXPYTHON_VERSION:
    raise RuntimeError(f"wxPython {MIN_WXPYTHON_VERSION}+ required")
```

### AUI Manager Configuration

```python
class MainFrame(wx.Frame):
    def setup_aui(self):
        self.aui_manager = wx.aui.AuiManager(self)

        # Configure AUI flags
        self.aui_manager.SetFlags(
            wx.aui.AUI_MGR_DEFAULT |
            wx.aui.AUI_MGR_ALLOW_FLOATING |
            wx.aui.AUI_MGR_TRANSPARENT_DRAG |
            wx.aui.AUI_MGR_TRANSPARENT_HINT |
            wx.aui.AUI_MGR_HINT_FADE |
            wx.aui.AUI_MGR_NO_VENETIAN_BLINDS_FADE
        )
```

### Performance Considerations

1. **Lazy Loading**
   - Only load visible panels
   - Defer heavy UI until needed

2. **Virtual Lists**
   - Use wx.VirtualListCtrl for large character/location lists

3. **Text Editor**
   - Buffer rendering for large documents
   - Syntax highlighting on-demand

4. **Statistics**
   - Update every 500ms, not on every keystroke
   - Cache calculated values

### Testing

```python
# Unit tests for UI components
class TestMainFrame(unittest.TestCase):
    def setUp(self):
        self.app = wx.App()
        self.frame = MainFrame(None)

    def test_panel_visibility(self):
        # Test panel show/hide
        self.frame.toggle_panel('project')
        pane = self.frame.aui_manager.GetPane('project')
        self.assertFalse(pane.IsShown())

    def tearDown(self):
        self.frame.Destroy()
        self.app.Destroy()
```

---

## Future Enhancements

### Phase 2+

- [ ] Minimap in editor scrollbar
- [ ] Code folding for chapters/sections
- [ ] Multiple cursor editing
- [ ] Column selection mode
- [ ] Vim mode (emulation)
- [ ] Customizable toolbar positions (top/left/right/bottom)
- [ ] Floating tool palettes
- [ ] Breadcrumb navigation
- [ ] Quick switcher (Ctrl+P style)
- [ ] Command palette (Ctrl+Shift+P)

---

**End of GUI Specification v1.0**
