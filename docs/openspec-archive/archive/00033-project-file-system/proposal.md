# 00033: Project File System - Solution-like Architecture

## Status
DEPLOYED

## Goal
Implement a "Solution-like" project architecture where .klh is a project manifest file and content is stored as separate human-readable files in a project folder structure.

## Context

### Decision Made
After architectural analysis, the **Solution-like** approach was chosen (similar to Visual Studio .sln/.csproj):
- `.klh` = project manifest (JSON) - structure, metadata, settings
- `content/` = RTF chapter files - human-readable, editable externally
- `metadata/` = JSON databases - characters, locations, notes
- Standalone files can be opened without project context

### Why Solution-like?
1. **Disaster recovery** - RTF files readable in Word/LibreOffice
2. **Git-friendly** - diff per chapter, sensible history
3. **Cloud sync** - only changed files sync
4. **Performance** - save = 1 file, not entire archive
5. **Extensibility** - plugins can add folders (e.g., `translations/`)
6. **Incremental save** - no ZIP rewrite on every save

### Previous Approach (Deprecated)
Phase 0 used ZIP archive with manifest.json - this is being replaced.

## Architecture

### Project Folder Structure
```
MyNovel/                              # Project folder
├── MyNovel.klh                       # Project manifest (JSON)
│
├── content/                          # Human-readable content
│   ├── frontmatter/
│   │   ├── 001_title_page.rtf
│   │   └── 002_dedication.rtf
│   ├── body/
│   │   ├── part_001_introduction/
│   │   │   ├── chapter_001_beginning.rtf
│   │   │   └── chapter_002_conflict.rtf
│   │   └── part_002_development/
│   │       └── chapter_001_twist.rtf
│   └── backmatter/
│       └── 001_epilogue.rtf
│
├── metadata/                         # JSON databases
│   ├── characters.json               # Character bank
│   ├── locations.json                # Location bank
│   └── notes.json                    # Notes
│
├── mindmaps/                         # Mind maps (.kmap)
│   └── plot_outline.kmap
│
├── timelines/                        # Timelines (.ktl)
│   └── main_story.ktl
│
├── resources/                        # Images, research (Phase 2+)
│   ├── images/
│   └── research/
│
└── .kalahari/                        # IDE-specific (gitignored)
    ├── cache/
    ├── backup/
    └── session.json
```

### .klh Manifest Format (JSON)
```json
{
  "kalahari": {
    "version": "1.0",
    "minVersion": "0.4.0"
  },
  "document": {
    "id": "uuid-here",
    "title": "My Novel",
    "author": "John Doe",
    "language": "pl",
    "genre": "fantasy",
    "created": "2025-12-11T10:00:00Z",
    "modified": "2025-12-11T15:30:00Z"
  },
  "structure": {
    "frontmatter": [
      {"id": "fm_001", "type": "title_page", "file": "frontmatter/001_title_page.rtf"},
      {"id": "fm_002", "type": "dedication", "file": "frontmatter/002_dedication.rtf"}
    ],
    "body": [
      {
        "id": "part_001",
        "type": "part",
        "title": "Introduction",
        "folder": "body/part_001_introduction",
        "chapters": [
          {"id": "ch_001", "type": "chapter", "title": "Beginning", "file": "chapter_001_beginning.rtf", "wordCount": 2500},
          {"id": "ch_002", "type": "chapter", "title": "Conflict", "file": "chapter_002_conflict.rtf", "wordCount": 3200}
        ]
      }
    ],
    "backmatter": [
      {"id": "bm_001", "type": "epilogue", "file": "backmatter/001_epilogue.rtf"}
    ]
  },
  "statistics": {
    "totalWords": 45000,
    "totalChapters": 25,
    "lastEdited": "ch_015"
  },
  "settings": {
    "defaultPerspective": "writer",
    "autoSaveInterval": 300
  }
}
```

### File Associations
| Extension | Type | Description |
|-----------|------|-------------|
| `.klh` | Project | Kalahari project manifest |
| `.kmap` | Mind Map | Mind map file |
| `.ktl` | Timeline | Timeline file |
| `.rtf` | Chapter | Rich text content (also system default) |

### Work Modes
```cpp
enum class WorkMode {
    NoDocument,      // Nothing open
    ProjectMode,     // .klh project open - full features
    StandaloneMode   // Single file without project - limited features
};
```

### Navigator Tree Structure
```
MyNovel                              # Root (project name)
├── Front Matter
│   ├── Title Page
│   └── Dedication
├── Part 1: Introduction
│   ├── Chapter 1: Beginning
│   └── Chapter 2: Conflict
├── Part 2: Development
│   └── Chapter 1: Twist
├── Back Matter
│   └── Epilogue
├── Mind Maps
│   └── Plot Outline
├── Timelines
│   └── Main Story
└── Other Files                      # Standalone files not in project
    ├── notes.rtf
    └── research.kmap
```

### Extended FILE Menu
```
FILE
├── New
│   ├── Project...              Ctrl+Shift+N
│   ├── ─────────────
│   ├── Chapter                 (in project context)
│   ├── Mind Map
│   ├── Timeline
│   └── Note
│
├── Open
│   ├── Project...              Ctrl+O
│   ├── ─────────────
│   ├── File...                 Ctrl+Shift+O   ← NEW
│   └── Recent Files            →              ← NEW
│
├── ─────────────
├── Save                        Ctrl+S
├── Save As...                  Ctrl+Shift+S
├── Save All                    Ctrl+Alt+S     ← NEW
│
├── ─────────────
├── Close File                  Ctrl+W         ← NEW
├── Close Project               Ctrl+Shift+W
│
├── ─────────────
├── Add to Project...                          ← NEW (for standalone files)
├── Project Properties...
│
├── ─────────────
├── Import
│   ├── Project Archive...      (.klh.zip)
│   ├── Word Document...        (.docx)
│   └── Plain Text...           (.txt)
│
├── Export
│   ├── Project Archive...      → .klh.zip     ← Bundle for sharing
│   ├── Current Chapter...      → .rtf/.docx/.pdf
│   └── Entire Book...          → .docx/.pdf/.epub
│
├── ─────────────
├── Recent Projects             →
└── Exit
```

## Scope

### Phase 1 (This OpenSpec)
1. **New project creation** - creates folder structure
2. **Project loading** - reads .klh, displays in Navigator
3. **Chapter editing** - open/edit/save RTF files
4. **Incremental save** - save only changed files
5. **Standalone mode** - open files without project
6. **"Other Files" in Navigator** - show standalone files
7. **Add to Project** - move standalone file into project
8. **Project Properties dialog** - edit metadata
9. **Export Archive** - bundle project to .klh.zip

### Phase 2 (Future)
- Character/Location/Notes banks
- Mind Maps/Timelines editors
- Resources library (images, research)
- Import from DOCX/Scrivener

## Acceptance Criteria
- [ ] File > New Project creates folder structure
- [ ] File > Open Project loads .klh and shows Navigator
- [ ] Double-click chapter opens in Editor
- [ ] Save writes only changed RTF file (not entire project)
- [ ] File > Open File opens standalone file
- [ ] Standalone files appear in Navigator under "Other Files"
- [ ] "Add to Project" moves file into project structure
- [ ] Project Properties dialog edits title/author/language
- [ ] Export Archive creates .klh.zip bundle
- [ ] File associations work (double-click .klh opens Kalahari)
- [ ] Build passes, all tests pass

## Migration Path
1. Old .klh ZIP archives → "Import Project Archive" extracts to folder
2. Existing test.klh → convert to new format or recreate

## Technical Notes

### Incremental Save Implementation
```cpp
void ProjectManager::saveChapter(const QString& chapterId) {
    BookElement* chapter = findChapter(chapterId);
    if (!chapter->isDirty()) return;

    QString path = m_projectPath / "content" / chapter->file();
    QFile file(path);
    file.write(chapter->content().toUtf8());
    chapter->setDirty(false);

    // Update manifest only if structure changed
    if (m_structureChanged) {
        saveManifest();
    }
}
```

### Standalone File Handling
```cpp
void MainWindow::openStandaloneFile(const QString& path) {
    m_workMode = WorkMode::StandaloneMode;

    // Add to "Other Files" in Navigator
    m_navigatorPanel->addStandaloneFile(path);

    // Open in editor
    openFileInEditor(path);

    // Show info bar
    showStandaloneInfoBar("Add to project for full features");
}
```

## Files to Modify/Create

### New Files
| File | Purpose |
|------|---------|
| `include/kalahari/core/project_manager.h` | Project folder management |
| `src/core/project_manager.cpp` | Implementation |
| `include/kalahari/gui/dialogs/project_properties_dialog.h` | Properties dialog |
| `src/gui/dialogs/project_properties_dialog.cpp` | Implementation |
| `include/kalahari/gui/dialogs/new_project_dialog.h` | New project wizard |
| `src/gui/dialogs/new_project_dialog.cpp` | Implementation |

### Modified Files
| File | Changes |
|------|---------|
| `src/core/document_archive.cpp` | Add import/export archive methods |
| `src/gui/main_window.cpp` | WorkMode handling, new menu items |
| `src/gui/panels/navigator_panel.cpp` | "Other Files" group, standalone support |
| `src/gui/command_definitions.cpp` | New commands for file operations |

### Deprecated (Phase 0)
| File | Status |
|------|--------|
| `test.klh` (ZIP format) | Will be replaced with folder structure |

---

## Book Templates System

### Available Templates (Initial)

| Template | Structure | Use Case |
|----------|-----------|----------|
| **Novel** (default) | Front Matter + 3 Parts + Back Matter | Standard fiction |
| **Short Story Collection** | Front Matter + Stories (flat) + Back Matter | Anthology |
| **Non-fiction** | Front Matter + Chapters (flat) + Back Matter | Essays, guides |
| **Screenplay** | Title + Acts + Scenes | Film/TV scripts |
| **Poetry Collection** | Front Matter + Sections + Poems | Poetry |
| **Empty Project** | Just manifest, no content | Advanced users |

**Note:** Templates are stored as JSON in `resources/templates/`. Easy to modify and extend via Python plugins in future.

### New Book Dialog

```
┌─────────────────────────────────────────────────────────────┐
│  New Book Project                                           │
├─────────────────────────────────────────────────────────────┤
│  Book Type:  ○ Novel (recommended)                          │
│              ○ Short Story Collection                       │
│              ○ Non-fiction                                  │
│              ○ Screenplay                                   │
│              ○ Poetry Collection                            │
│              ○ Empty Project                                │
│                                                             │
│  ─────────────────────────────────────────────────────────  │
│  Title:      [________________________________]             │
│  Author:     [John Doe_______________________] (from prefs) │
│  Language:   [Polish________________________▼]              │
│  Genre:      [Fantasy_______________________▼]              │
│                                                             │
│  ─────────────────────────────────────────────────────────  │
│  Location:   [E:\Documents\Books\____________] [Browse...]  │
│  ☑ Create "Title" subfolder automatically                   │
│                                                             │
│  ─────────────────────────────────────────────────────────  │
│  Template Preview:                                          │
│  ┌─────────────────────────────────────────┐               │
│  │  📁 My Novel                             │               │
│  │  ├── 📄 Front Matter                     │               │
│  │  │   └── Title Page                      │               │
│  │  ├── 📁 Part 1                           │               │
│  │  │   └── Chapter 1                       │               │
│  │  └── 📄 Back Matter                      │               │
│  └─────────────────────────────────────────┘               │
├─────────────────────────────────────────────────────────────┤
│                              [Create Project]    [Cancel]   │
└─────────────────────────────────────────────────────────────┘
```

---

## Recovery System (OpenOffice-style)

### Recovery Folder Structure

```
MyNovel/
├── .kalahari/
│   └── recovery/
│       ├── session.json           # What was open at crash time
│       ├── chapter_001.rtf.rec    # Unsaved changes
│       └── chapter_003.rtf.rec
```

### Recovery Dialog (shown after crash)

```
┌─────────────────────────────────────────────────────────────┐
│  ⚠ Kalahari - Document Recovery                             │
├─────────────────────────────────────────────────────────────┤
│  Kalahari did not shut down properly. The following         │
│  documents can be recovered:                                │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ ☑ Chapter 1: Beginning                              │   │
│  │   Last auto-save: 2025-12-11 15:47:32               │   │
│  │   Original: 2025-12-11 15:30:00                     │   │
│  │                                                      │   │
│  │ ☑ Chapter 3: Conflict                               │   │
│  │   Last auto-save: 2025-12-11 15:47:30               │   │
│  │   Original: 2025-12-11 14:00:00                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  [Recover Selected]  [Discard All]  [Show Folder]          │
│  □ Don't show this dialog again (always recover)           │
└─────────────────────────────────────────────────────────────┘
```

### Recovery Flow

```
App Start
    │
    ├─► Check .kalahari/recovery/session.json exists?
    │       │
    │       ├─► YES: Show Recovery Dialog
    │       │       ├─► "Recover" → Load .rec files, merge with originals
    │       │       └─► "Discard" → Delete recovery folder
    │       │
    │       └─► NO: Normal startup
```

---

## Auto-save System

### Hybrid Approach

| Mode | Target | When | Purpose |
|------|--------|------|---------|
| **Recovery save** | `.rec` files | Every N seconds | Crash protection |
| **User save** (Ctrl+S) | Original files | On demand | User commits changes |
| **Auto-save** (optional) | Original files | Every M minutes | Convenience |

**Default:** Recovery save every 60 seconds. Auto-save to original disabled by default.

### Settings UI

```
Settings > Editor > Auto-save
┌─────────────────────────────────────────────────────────────┐
│  Auto-save & Recovery                                       │
├─────────────────────────────────────────────────────────────┤
│  ☑ Enable auto-recovery                                     │
│    Save recovery data every: [60 seconds ▼]                 │
│                                                             │
│  □ Auto-save to original files                              │
│    Save original files every: [5 minutes ▼]                 │
│    ⚠ This will overwrite your files automatically           │
│                                                             │
│  [Clear Recovery Data]                                      │
└─────────────────────────────────────────────────────────────┘
```

### First Run Wizard (Auto-save step)

```
┌─────────────────────────────────────────────────────────────┐
│  Welcome to Kalahari                               Step 3/5 │
├─────────────────────────────────────────────────────────────┤
│  💾 Auto-save Settings                                      │
│                                                             │
│  ● Recommended (auto-recovery every 60 seconds)             │
│    Your work is saved to recovery files. Original files     │
│    are only updated when you press Ctrl+S.                  │
│                                                             │
│  ○ Aggressive (auto-save every 5 minutes)                   │
│    Original files are automatically saved.                  │
│                                                             │
│  ○ Manual only                                              │
│    No automatic saving. Press Ctrl+S to save.               │
│    ⚠ Risk of data loss if application crashes.              │
├─────────────────────────────────────────────────────────────┤
│                                    [← Back]  [Next →]       │
└─────────────────────────────────────────────────────────────┘
```

---

## Dirty State Indication

### Window Title
```
MyNovel* - Kalahari                    ← Asterisk = unsaved changes
```

### Navigator Icons
```
📁 MyNovel*
├── 📄 Chapter 1: Beginning*           ← Dirty
├── 📄 Chapter 2: Conflict             ← Clean
└── 📄 Chapter 3: Resolution*          ← Dirty
```

### Close with Unsaved Changes Dialog
```
┌─────────────────────────────────────────────────────────────┐
│  ⚠ Save Changes?                                            │
├─────────────────────────────────────────────────────────────┤
│  The following documents have unsaved changes:              │
│    • Chapter 1: Beginning                                   │
│    • Chapter 3: Resolution                                  │
│                                                             │
│  Do you want to save before closing?                        │
├─────────────────────────────────────────────────────────────┤
│  [Save All]      [Don't Save]      [Cancel]                │
└─────────────────────────────────────────────────────────────┘
```

---

## Single Instance Mode

### Behavior

When user double-clicks .klh file:
1. Check if Kalahari is already running
2. If YES: Send IPC message to existing instance
   - Bring window to front
   - Open/switch to requested project
3. If NO: Start new instance, open project

### Implementation (Qt IPC)

```cpp
// main.cpp
QSharedMemory sharedMem("KalahariSingleInstance");
if (!sharedMem.create(1)) {
    // Instance already running - send file path via IPC
    QLocalSocket socket;
    socket.connectToServer("KalahariIPC");
    socket.write(filePath.toUtf8());
    return 0;  // Exit this instance
}

// MainWindow - listen for IPC
QLocalServer* server = new QLocalServer(this);
server->listen("KalahariIPC");
connect(server, &QLocalServer::newConnection, [this]() {
    QLocalSocket* socket = server->nextPendingConnection();
    QString path = socket->readAll();
    openProject(path);
    activateWindow();
});
```

### Project Switching

Via FILE > Recent Projects menu. Clicking different project:
1. Ask to save current if dirty
2. Close current project
3. Open selected project

---

## Project Lifecycle Diagram

```
    CREATE                    OPEN                     CLOSE
       │                        │                        │
       ▼                        ▼                        ▼
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│ New Project │         │ Open .klh   │         │ Check Dirty │
│ Dialog      │         │ + Recovery  │         │ State       │
└──────┬──────┘         └──────┬──────┘         └──────┬──────┘
       │                        │                        │
       ▼                        ▼                        ▼
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│ Create      │         │ Load        │         │ Save/Discard│
│ Folder      │         │ Manifest    │         │ Dialog      │
│ Structure   │         │             │         │             │
└──────┬──────┘         └──────┬──────┘         └──────┬──────┘
       │                        │                        │
       ▼                        ▼                        ▼
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│ Write       │         │ Display in  │         │ Clear       │
│ Manifest    │         │ Navigator   │         │ Recovery    │
└──────┬──────┘         └──────┬──────┘         └─────────────┘
       │                        │
       ▼                        ▼
┌─────────────┐         ┌─────────────┐
│ Open First  │         │ Load Last   │
│ Chapter     │         │ Edited      │
└─────────────┘         └─────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │     EDITING     │
                    ├─────────────────┤
                    │ • Edit content  │
                    │ • Track dirty   │
                    │ • Auto-recovery │
                    │ • User save     │
                    └─────────────────┘
```

---

## References
- ROADMAP.md Section 1.2 Project File System
- Visual Studio Solution/Project architecture
- Scrivener project format (inspiration)
- OpenOffice recovery system
