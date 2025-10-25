# Kalahari MVP - Detailed Task Breakdown

**Version:** 1.0
**Date:** 2025-10-24
**Timeline:** 5-6 months (Alpha → Beta → 1.0)
**Status:** Planning Phase

---

## Table of Contents

1. [Overview](#overview)
2. [Alpha Phase (1-2 months)](#alpha-phase-1-2-months)
3. [Beta Phase (3-4 months)](#beta-phase-3-4-months)
4. [1.0 Release Phase (5-6 months)](#10-release-phase-5-6-months)
5. [Dependencies & Prerequisites](#dependencies--prerequisites)
6. [Risk Assessment](#risk-assessment)
7. [Success Metrics](#success-metrics)

---

## Overview

### Strategy

```
Alpha (1-2mo)        Beta (3-4mo)           1.0 (5-6mo)
Technical Foundation → Feature Complete → Polish & Release
     ↓                      ↓                    ↓
Internal testing    Closed beta (20-30)   Public GitHub
Proof of concept    User feedback         MIT License
```

### Team

- **Lead Developer:** TBD
- **Beta Testers:** 20-30 writers (recruited Phase 2)
- **Documentation:** TBD

### Tools & Environment

- **IDE:** VS Code with Python extensions
- **Version Control:** Git + GitHub (private during development)
- **Package Manager:** Poetry
- **Testing:** pytest, pytest-cov, pytest-qt (for wxPython)
- **CI/CD:** GitHub Actions
- **Project Management:** GitHub Projects

---

## Alpha Phase (1-2 months)

**Goal:** Functional proof-of-concept with core architecture

**Deliverable:** Internal demo showing basic writing workflow

### Week 1-2: Project Setup & Architecture

#### Task 1.1: Repository & Environment Setup
**Priority:** Critical
**Estimated Time:** 2 days

- [ ] Create GitHub repository (private)
- [ ] Initialize Poetry project
  ```bash
  poetry init
  poetry add wxpython
  poetry add --group dev pytest pytest-cov black mypy ruff
  ```
- [ ] Set up .gitignore
- [ ] Create pyproject.toml with dependencies
- [ ] Set up pre-commit hooks (black, mypy, ruff)
- [ ] Create README.md (initial)

**Acceptance Criteria:**
- Poetry environment activates successfully
- All dependencies install without errors
- Pre-commit hooks run on commit

---

#### Task 1.2: Project Structure
**Priority:** Critical
**Estimated Time:** 1 day

Create folder structure:
```
kalahari/
├── kalahari/                 # Main package
│   ├── __init__.py
│   ├── __main__.py          # Entry point
│   ├── app.py               # Application class
│   ├── gui/                 # GUI components
│   │   ├── __init__.py
│   │   ├── main_frame.py   # Main window
│   │   ├── panels/         # Panel components
│   │   ├── dialogs/        # Dialog windows
│   │   └── widgets/        # Custom widgets
│   ├── core/               # Business logic
│   │   ├── __init__.py
│   │   ├── project.py      # Project management
│   │   ├── document.py     # Document handling
│   │   └── config.py       # Configuration
│   ├── models/             # Data models
│   │   ├── __init__.py
│   │   ├── book.py
│   │   ├── chapter.py
│   │   └── character.py
│   ├── utils/              # Utilities
│   │   ├── __init__.py
│   │   ├── logger.py
│   │   └── file_utils.py
│   └── resources/          # Assets
│       ├── icons/
│       ├── images/
│       └── locales/
├── tests/                   # Test suite
│   ├── __init__.py
│   ├── test_project.py
│   └── test_document.py
├── docs/                    # Documentation
├── scripts/                 # Utility scripts
├── pyproject.toml
├── README.md
└── LICENSE
```

**Acceptance Criteria:**
- All directories created
- __init__.py in all packages
- Basic imports work

---

#### Task 1.3: Configuration System
**Priority:** High
**Estimated Time:** 2 days

- [ ] Create Config class (Singleton pattern)
- [ ] Load/save configuration from JSON
- [ ] Default configuration values
- [ ] User preferences storage (~/.kalahari/)
- [ ] Recent projects list
- [ ] Window size/position persistence

**File:** `kalahari/core/config.py`

```python
class Config:
    """Application configuration manager"""

    DEFAULT_CONFIG = {
        'version': '0.1.0',
        'window': {
            'width': 1280,
            'height': 800,
            'maximized': False
        },
        'editor': {
            'font_family': 'Arial',
            'font_size': 12,
            'line_spacing': 1.5,
            'word_wrap': True
        },
        'assistant': {
            'enabled': True,
            'animal': 'lion',
            'frequency': 'normal'
        },
        'recent_projects': []
    }

    def load(self):
        """Load from ~/.kalahari/config.json"""
        pass

    def save(self):
        """Save to ~/.kalahari/config.json"""
        pass
```

**Acceptance Criteria:**
- Config loads from file
- Config saves to file
- Defaults work if file missing

---

#### Task 1.4: Logging System
**Priority:** High
**Estimated Time:** 1 day

- [ ] Set up Python logging
- [ ] Log to file (~/.kalahari/logs/)
- [ ] Log rotation (keep last 7 days)
- [ ] Different levels (DEBUG, INFO, WARNING, ERROR)
- [ ] Console output for development

**File:** `kalahari/utils/logger.py`

```python
import logging
from logging.handlers import RotatingFileHandler

def setup_logger(name: str, level=logging.INFO):
    logger = logging.getLogger(name)
    logger.setLevel(level)

    # File handler
    fh = RotatingFileHandler(
        '~/.kalahari/logs/kalahari.log',
        maxBytes=10*1024*1024,  # 10MB
        backupCount=7
    )
    fh.setLevel(logging.DEBUG)

    # Console handler
    ch = logging.StreamHandler()
    ch.setLevel(logging.INFO)

    # Formatter
    formatter = logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)

    logger.addHandler(fh)
    logger.addHandler(ch)

    return logger
```

**Acceptance Criteria:**
- Logs written to file
- Console output visible
- Log rotation works

---

### Week 3-4: GUI Framework

#### Task 2.1: Main Window Setup
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Create MainFrame class (wx.Frame)
- [ ] Set up wxAUI Manager
- [ ] Create menu bar (structure only, no actions)
- [ ] Create toolbar (structure only)
- [ ] Create status bar
- [ ] Window icon loading
- [ ] Handle window close event

**File:** `kalahari/gui/main_frame.py`

```python
class MainFrame(wx.Frame):
    """Main application window"""

    def __init__(self, parent, title):
        super().__init__(parent, title=title, size=(1280, 800))

        # AUI Manager
        self.aui_mgr = wx.aui.AuiManager(self)

        # Components
        self.create_menu_bar()
        self.create_toolbar()
        self.create_status_bar()
        self.create_panels()

        # Bind events
        self.Bind(wx.EVT_CLOSE, self.on_close)

        # Update AUI
        self.aui_mgr.Update()

    def create_menu_bar(self):
        """Create menu bar"""
        menubar = wx.MenuBar()

        # File menu
        file_menu = wx.Menu()
        file_menu.Append(wx.ID_NEW, "&New Project\tCtrl+N")
        file_menu.Append(wx.ID_OPEN, "&Open Project\tCtrl+O")
        # ... more items
        menubar.Append(file_menu, "&File")

        self.SetMenuBar(menubar)
```

**Acceptance Criteria:**
- Window opens with correct size
- Menu bar visible
- Toolbar visible
- Status bar visible
- Window closes properly

---

#### Task 2.2: Panel System
**Priority:** Critical
**Estimated Time:** 3 days

Create base panel classes and dock them:

- [ ] Create BasePanel abstract class
- [ ] Create ProjectNavigatorPanel (empty tree)
- [ ] Create EditorPanel (empty wx.richtext.RichTextCtrl)
- [ ] Create StatsPanel (placeholder labels)
- [ ] Dock panels using wxAUI

**File:** `kalahari/gui/panels/base_panel.py`

```python
class BasePanel(wx.Panel):
    """Base class for all panels"""

    def __init__(self, parent, name: str):
        super().__init__(parent)
        self.name = name

    def get_aui_pane_info(self) -> wx.aui.AuiPaneInfo:
        """Return AUI pane configuration"""
        raise NotImplementedError
```

**Acceptance Criteria:**
- All panels visible
- Panels can be dragged/docked
- Panels can be closed/reopened
- Layout persists

---

#### Task 2.3: Event System
**Priority:** High
**Estimated Time:** 2 days

Set up custom events for panel communication:

- [ ] Define custom event types
- [ ] Create event dispatcher
- [ ] Document event flow

**File:** `kalahari/core/events.py`

```python
import wx

# Custom event types
EVT_PROJECT_OPENED = wx.NewEventType()
EVT_DOCUMENT_CHANGED = wx.NewEventType()
EVT_CHARACTER_SELECTED = wx.NewEventType()
EVT_STATS_UPDATE = wx.NewEventType()

class ProjectEvent(wx.PyEvent):
    """Event for project changes"""

    def __init__(self, event_type, project):
        super().__init__()
        self.SetEventType(event_type)
        self.project = project

# Bind examples:
# self.Bind(EVT_PROJECT_OPENED, self.on_project_opened)
```

**Acceptance Criteria:**
- Events fire correctly
- Handlers receive events
- No circular dependencies

---

### Week 5-6: Project Management

#### Task 3.1: Project Data Model
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Create Project class
- [ ] Create Chapter class
- [ ] Create Document class
- [ ] Implement hierarchical structure
- [ ] Add/remove chapters

**File:** `kalahari/models/book.py`

```python
from dataclasses import dataclass, field
from typing import List
from datetime import datetime

@dataclass
class Chapter:
    """Represents a book chapter"""
    id: str
    title: str
    content: str = ""
    created: datetime = field(default_factory=datetime.now)
    modified: datetime = field(default_factory=datetime.now)
    word_count: int = 0

@dataclass
class Book:
    """Represents a book project"""
    id: str
    title: str
    author: str
    language: str = "en"
    chapters: List[Chapter] = field(default_factory=list)
    created: datetime = field(default_factory=datetime.now)
    modified: datetime = field(default_factory=datetime.now)

    def add_chapter(self, title: str) -> Chapter:
        """Add new chapter"""
        chapter = Chapter(
            id=str(uuid.uuid4()),
            title=title
        )
        self.chapters.append(chapter)
        return chapter

    def remove_chapter(self, chapter_id: str):
        """Remove chapter by ID"""
        self.chapters = [c for c in self.chapters if c.id != chapter_id]

    def to_dict(self) -> dict:
        """Serialize to dictionary"""
        return dataclasses.asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> 'Book':
        """Deserialize from dictionary"""
        return cls(**data)
```

**Acceptance Criteria:**
- Can create project
- Can add/remove chapters
- Serialization works
- Deserialization works

---

#### Task 3.2: File Format (.klh)
**Priority:** Critical
**Estimated Time:** 2 days

- [ ] Design .klh file format (ZIP container)
- [ ] Create ProjectSerializer class
- [ ] Implement save_project()
- [ ] Implement load_project()
- [ ] Add checksums (SHA256)

**Format Structure:**
```
my_book.klh (ZIP file)
├── project.json        # Metadata
├── book.json           # Book structure
├── chapters/
│   ├── chapter_01.rtf
│   ├── chapter_02.rtf
│   └── ...
├── characters/
│   └── characters.json
├── locations/
│   └── locations.json
├── sources/
│   └── sources.json
└── manifest.json       # File listing + checksums
```

**File:** `kalahari/core/project.py`

```python
class ProjectManager:
    """Manages project save/load"""

    @staticmethod
    def save_project(book: Book, filepath: str):
        """Save project to .klh file"""
        with zipfile.ZipFile(filepath, 'w', zipfile.ZIP_DEFLATED) as zf:
            # Write book.json
            zf.writestr('book.json', json.dumps(book.to_dict(), indent=2))

            # Write chapters
            for chapter in book.chapters:
                path = f'chapters/{chapter.id}.rtf'
                zf.writestr(path, chapter.content)

            # Write manifest
            manifest = create_manifest(zf)
            zf.writestr('manifest.json', json.dumps(manifest, indent=2))

    @staticmethod
    def load_project(filepath: str) -> Book:
        """Load project from .klh file"""
        with zipfile.ZipFile(filepath, 'r') as zf:
            # Verify manifest
            verify_manifest(zf)

            # Load book.json
            book_data = json.loads(zf.read('book.json'))
            book = Book.from_dict(book_data)

            # Load chapters
            for chapter in book.chapters:
                path = f'chapters/{chapter.id}.rtf'
                chapter.content = zf.read(path).decode('utf-8')

            return book
```

**Acceptance Criteria:**
- Can save project
- Can load project
- ZIP compression works
- Checksums verified

---

#### Task 3.3: New Project Wizard
**Priority:** High
**Estimated Time:** 2 days

- [ ] Create NewProjectDialog
- [ ] Title input
- [ ] Author input
- [ ] Language selection
- [ ] Template selection (basic only)
- [ ] Save location picker

**File:** `kalahari/gui/dialogs/new_project_dialog.py`

```python
class NewProjectDialog(wx.Dialog):
    """Dialog for creating new project"""

    def __init__(self, parent):
        super().__init__(parent, title="New Project", size=(500, 400))

        # Title
        self.title_ctrl = wx.TextCtrl(self, size=(400, -1))

        # Author
        self.author_ctrl = wx.TextCtrl(self, size=(400, -1))

        # Language
        self.language_choice = wx.Choice(self, choices=['English', 'Polish'])

        # Template
        self.template_choice = wx.Choice(self, choices=['Blank', 'Novel', 'Non-fiction'])

        # Location
        self.location_picker = wx.FilePickerCtrl(self, style=wx.FLP_SAVE)

        # Buttons
        btn_sizer = self.CreateButtonSizer(wx.OK | wx.CANCEL)

        # Layout
        # ...

    def get_project_data(self) -> dict:
        """Return entered data"""
        return {
            'title': self.title_ctrl.GetValue(),
            'author': self.author_ctrl.GetValue(),
            'language': self.language_choice.GetStringSelection(),
            'template': self.template_choice.GetStringSelection(),
            'location': self.location_picker.GetPath()
        }
```

**Acceptance Criteria:**
- Dialog opens
- All fields work
- Validation (no empty title)
- Returns data correctly

---

### Week 7-8: Basic Editor

#### Task 4.1: Rich Text Editor Integration
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Integrate wx.richtext.RichTextCtrl
- [ ] Basic formatting (bold, italic, underline)
- [ ] Undo/Redo support
- [ ] Copy/Paste
- [ ] Word count (live update)

**File:** `kalahari/gui/panels/editor_panel.py`

```python
class EditorPanel(BasePanel):
    """Rich text editor panel"""

    def __init__(self, parent):
        super().__init__(parent, "editor")

        # Rich text control
        self.editor = wx.richtext.RichTextCtrl(
            self,
            style=wx.VSCROLL | wx.HSCROLL | wx.NO_BORDER | wx.WANTS_CHARS
        )

        # Set font
        font = wx.Font(12, wx.FONTFAMILY_DEFAULT, wx.FONTSTYLE_NORMAL, wx.FONTWEIGHT_NORMAL, False, 'Arial')
        self.editor.SetFont(font)

        # Bind events
        self.editor.Bind(wx.EVT_TEXT, self.on_text_changed)

        # Layout
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self.editor, 1, wx.EXPAND)
        self.SetSizer(sizer)

    def on_text_changed(self, event):
        """Handle text changes"""
        # Update word count
        text = self.editor.GetValue()
        word_count = len(text.split())

        # Fire event
        evt = StatsEvent(EVT_STATS_UPDATE, word_count=word_count)
        wx.PostEvent(self, evt)

    def apply_bold(self):
        """Apply bold to selection"""
        self.editor.ApplyBoldToSelection()

    def apply_italic(self):
        """Apply italic to selection"""
        self.editor.ApplyItalicToSelection()
```

**Acceptance Criteria:**
- Can type text
- Formatting buttons work
- Undo/Redo works
- Word count updates

---

#### Task 4.2: Document Tab System
**Priority:** High
**Estimated Time:** 2 days

- [ ] Use wx.aui.AuiNotebook for tabs
- [ ] Open multiple documents
- [ ] Switch between tabs
- [ ] Close tabs
- [ ] Tab context menu (Close, Close Others, Close All)

**File:** `kalahari/gui/panels/editor_panel.py` (extended)

```python
class EditorNotebook(wx.aui.AuiNotebook):
    """Notebook for editor tabs"""

    def __init__(self, parent):
        super().__init__(parent, style=
            wx.aui.AUI_NB_TOP |
            wx.aui.AUI_NB_TAB_SPLIT |
            wx.aui.AUI_NB_TAB_MOVE |
            wx.aui.AUI_NB_CLOSE_ON_ACTIVE_TAB |
            wx.aui.AUI_NB_MIDDLE_CLICK_CLOSE
        )

    def open_document(self, chapter: Chapter):
        """Open chapter in new tab"""
        editor = wx.richtext.RichTextCtrl(self)
        editor.SetValue(chapter.content)

        self.AddPage(editor, chapter.title, select=True)

    def get_current_editor(self) -> wx.richtext.RichTextCtrl:
        """Get currently active editor"""
        page = self.GetCurrentPage()
        return page if isinstance(page, wx.richtext.RichTextCtrl) else None
```

**Acceptance Criteria:**
- Multiple tabs work
- Can switch tabs
- Can close tabs
- Tab titles update

---

### Week 8: Alpha Integration & Testing

#### Task 5.1: Integration
**Priority:** Critical
**Estimated Time:** 2 days

- [ ] Connect all components
- [ ] New Project → Creates project → Opens in editor
- [ ] Save Project → Writes .klh file
- [ ] Open Project → Loads .klh file → Opens in editor
- [ ] Edit text → Updates word count

**Acceptance Criteria:**
- Full workflow works end-to-end
- No crashes
- Data persists correctly

---

#### Task 5.2: Basic Testing
**Priority:** High
**Estimated Time:** 2 days

- [ ] Write unit tests for Project model
- [ ] Write unit tests for file serialization
- [ ] Write integration test for save/load
- [ ] Manual testing checklist

**File:** `tests/test_project.py`

```python
def test_create_project():
    """Test project creation"""
    book = Book(
        id="test-id",
        title="Test Book",
        author="Test Author"
    )
    assert book.title == "Test Book"
    assert len(book.chapters) == 0

def test_add_chapter():
    """Test adding chapter"""
    book = Book(id="test", title="Test", author="Test")
    chapter = book.add_chapter("Chapter 1")
    assert len(book.chapters) == 1
    assert chapter.title == "Chapter 1"

def test_save_load_project(tmp_path):
    """Test save and load"""
    book = Book(id="test", title="Test", author="Test")
    book.add_chapter("Chapter 1")

    filepath = tmp_path / "test.klh"
    ProjectManager.save_project(book, str(filepath))

    loaded = ProjectManager.load_project(str(filepath))
    assert loaded.title == book.title
    assert len(loaded.chapters) == 1
```

**Acceptance Criteria:**
- All tests pass
- Coverage > 50%
- CI pipeline runs

---

#### Task 5.3: Alpha Demo
**Priority:** Medium
**Estimated Time:** 1 day

- [ ] Prepare demo script
- [ ] Record demo video (optional)
- [ ] Internal presentation
- [ ] Gather feedback

**Demo Script:**
1. Launch Kalahari
2. New Project → "My Novel"
3. Add Chapter → "Chapter 1"
4. Write some text
5. Save Project
6. Close application
7. Open Project
8. Verify text is there
9. Show word count updates

**Acceptance Criteria:**
- Demo runs smoothly
- Feedback documented
- Issues logged on GitHub

---

## Beta Phase (3-4 months)

**Goal:** Feature-complete MVP ready for user testing

**Deliverable:** Beta release for 20-30 writers

### Week 9-10: Complete Editor

#### Task 6.1: Full Formatting Support
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Headings (H1-H6)
- [ ] Lists (bulleted, numbered)
- [ ] Alignment (left, center, right, justify)
- [ ] Text color
- [ ] Background color
- [ ] Paragraph styles
- [ ] Format painter

**Acceptance Criteria:**
- All formatting options work
- Formatting persists in saved files

---

#### Task 6.2: Footnotes
**Priority:** Medium
**Estimated Time:** 2 days

- [ ] Insert footnote
- [ ] Edit footnote
- [ ] Delete footnote
- [ ] Footnote numbering
- [ ] Export with footnotes

**Acceptance Criteria:**
- Footnotes work in editor
- Footnotes export to DOCX

---

#### Task 6.3: Find & Replace
**Priority:** High
**Estimated Time:** 2 days

- [ ] Find dialog
- [ ] Replace dialog
- [ ] Find in project (all chapters)
- [ ] Regular expressions support
- [ ] Highlight matches

**File:** `kalahari/gui/dialogs/find_dialog.py`

```python
class FindDialog(wx.Dialog):
    """Find and replace dialog"""

    def __init__(self, parent, editor):
        super().__init__(parent, title="Find", size=(400, 200))
        self.editor = editor

        # Find text
        self.find_ctrl = wx.TextCtrl(self)

        # Options
        self.case_sensitive = wx.CheckBox(self, label="Case sensitive")
        self.whole_word = wx.CheckBox(self, label="Whole word")
        self.regex = wx.CheckBox(self, label="Regular expression")

        # Buttons
        self.find_btn = wx.Button(self, label="Find Next")
        self.find_btn.Bind(wx.EVT_BUTTON, self.on_find_next)

    def on_find_next(self, event):
        """Find next occurrence"""
        text = self.find_ctrl.GetValue()
        # Search in editor...
```

**Acceptance Criteria:**
- Can find text
- Can replace text
- Regex works
- Case sensitive option works

---

### Week 11-12: Character Bank

#### Task 7.1: Character Data Model
**Priority:** High
**Estimated Time:** 2 days

- [ ] Create Character class
- [ ] Fields: name, age, appearance, personality, bio
- [ ] Add photo support
- [ ] Relationships to other characters
- [ ] Serialize to JSON

**File:** `kalahari/models/character.py`

```python
@dataclass
class Character:
    """Represents a character"""
    id: str
    name: str
    age: Optional[int] = None
    gender: Optional[str] = None
    appearance: str = ""
    personality: str = ""
    biography: str = ""
    photo_path: Optional[str] = None
    relationships: List[Tuple[str, str]] = field(default_factory=list)  # [(char_id, relationship_type)]
    created: datetime = field(default_factory=datetime.now)
    modified: datetime = field(default_factory=datetime.now)

    def to_dict(self) -> dict:
        return dataclasses.asdict(self)

    @classmethod
    def from_dict(cls, data: dict) -> 'Character':
        return cls(**data)
```

**Acceptance Criteria:**
- Can create character
- All fields save/load
- Relationships track correctly

---

#### Task 7.2: Character List Panel
**Priority:** High
**Estimated Time:** 3 days

- [ ] List view of characters
- [ ] Add new character button
- [ ] Edit character button
- [ ] Delete character button
- [ ] Search/filter characters

**File:** `kalahari/gui/panels/character_panel.py`

```python
class CharacterPanel(BasePanel):
    """Character bank panel"""

    def __init__(self, parent):
        super().__init__(parent, "characters")

        # Toolbar
        toolbar = wx.ToolBar(self)
        toolbar.AddTool(wx.ID_ADD, "Add", wx.ArtProvider.GetBitmap(wx.ART_PLUS))
        toolbar.AddTool(wx.ID_DELETE, "Delete", wx.ArtProvider.GetBitmap(wx.ART_DELETE))
        toolbar.Realize()

        # List control
        self.list_ctrl = wx.ListCtrl(self, style=wx.LC_REPORT | wx.LC_SINGLE_SEL)
        self.list_ctrl.InsertColumn(0, "Name", width=150)
        self.list_ctrl.InsertColumn(1, "Age", width=50)
        self.list_ctrl.InsertColumn(2, "Gender", width=80)

        # Layout
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(toolbar, 0, wx.EXPAND)
        sizer.Add(self.list_ctrl, 1, wx.EXPAND)
        self.SetSizer(sizer)

    def add_character(self, character: Character):
        """Add character to list"""
        index = self.list_ctrl.InsertItem(self.list_ctrl.GetItemCount(), character.name)
        self.list_ctrl.SetItem(index, 1, str(character.age) if character.age else "")
        self.list_ctrl.SetItem(index, 2, character.gender or "")
```

**Acceptance Criteria:**
- List displays characters
- Can add character
- Can edit character
- Can delete character

---

#### Task 7.3: Character Edit Dialog
**Priority:** High
**Estimated Time:** 3 days

- [ ] Create CharacterDialog
- [ ] All fields editable
- [ ] Photo upload
- [ ] Tab layout (Basic, Appearance, Biography)
- [ ] Validation

**File:** `kalahari/gui/dialogs/character_dialog.py`

```python
class CharacterDialog(wx.Dialog):
    """Dialog for editing character"""

    def __init__(self, parent, character: Optional[Character] = None):
        super().__init__(parent, title="Edit Character", size=(600, 500))

        self.character = character or Character(id=str(uuid.uuid4()), name="")

        # Notebook for tabs
        notebook = wx.Notebook(self)

        # Basic tab
        basic_panel = self.create_basic_tab(notebook)
        notebook.AddPage(basic_panel, "Basic Info")

        # Appearance tab
        appearance_panel = self.create_appearance_tab(notebook)
        notebook.AddPage(appearance_panel, "Appearance")

        # Biography tab
        bio_panel = self.create_biography_tab(notebook)
        notebook.AddPage(bio_panel, "Biography")

        # ... more tabs

    def create_basic_tab(self, parent) -> wx.Panel:
        """Create basic info tab"""
        panel = wx.Panel(parent)

        # Name
        wx.StaticText(panel, label="Name:")
        self.name_ctrl = wx.TextCtrl(panel, value=self.character.name)

        # Age
        wx.StaticText(panel, label="Age:")
        self.age_ctrl = wx.SpinCtrl(panel, value=str(self.character.age or 0), min=0, max=200)

        # Gender
        wx.StaticText(panel, label="Gender:")
        self.gender_choice = wx.Choice(panel, choices=["Male", "Female", "Other", "N/A"])

        # Layout...

        return panel

    def get_character(self) -> Character:
        """Return updated character"""
        self.character.name = self.name_ctrl.GetValue()
        self.character.age = self.age_ctrl.GetValue()
        # ... more fields
        return self.character
```

**Acceptance Criteria:**
- All tabs work
- Data saves correctly
- Validation prevents empty name
- Photo upload works

---

### Week 13-14: Location Bank & Source Library

#### Task 8.1-8.2: Location Bank
**Priority:** Medium
**Estimated Time:** 4 days

Similar to Character Bank:
- [ ] Location model
- [ ] Location list panel
- [ ] Location edit dialog
- [ ] Fields: name, description, photo, coordinates (optional)

**Acceptance Criteria:**
- Same as Character Bank

---

#### Task 8.3-8.4: Source Library
**Priority:** Medium
**Estimated Time:** 4 days

- [ ] Source model (file reference)
- [ ] Import files (PDF, TXT, DOCX, images)
- [ ] Organize in folders
- [ ] Basic tagging
- [ ] Link to text fragments
- [ ] Preview panel (Phase 2: OCR, AI)

**File:** `kalahari/models/source.py`

```python
@dataclass
class Source:
    """Represents a research source"""
    id: str
    title: str
    type: str  # pdf, docx, txt, image, web
    file_path: str
    tags: List[str] = field(default_factory=list)
    notes: str = ""
    created: datetime = field(default_factory=datetime.now)
```

**Acceptance Criteria:**
- Can import files
- Files stored in project
- Can organize in folders
- Can tag sources

---

### Week 15-16: Export System

#### Task 9.1: DOCX Export
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Use python-docx library
- [ ] Convert Rich Text to DOCX
- [ ] Preserve formatting
- [ ] Include footnotes
- [ ] Table of contents (optional)
- [ ] Custom styles

**File:** `kalahari/core/exporters/docx_exporter.py`

```python
from docx import Document
from docx.shared import Pt, Inches

class DOCXExporter:
    """Export to Microsoft Word format"""

    @staticmethod
    def export(book: Book, filepath: str, options: dict):
        """Export book to DOCX"""
        doc = Document()

        # Set styles
        style = doc.styles['Normal']
        font = style.font
        font.name = options.get('font_family', 'Arial')
        font.size = Pt(options.get('font_size', 12))

        # Title page
        doc.add_heading(book.title, 0)
        doc.add_paragraph(f"by {book.author}")
        doc.add_page_break()

        # Chapters
        for chapter in book.chapters:
            doc.add_heading(chapter.title, 1)

            # Convert rich text to paragraphs
            paragraphs = parse_rich_text(chapter.content)
            for para in paragraphs:
                p = doc.add_paragraph(para.text)
                # Apply formatting...

        # Save
        doc.save(filepath)
```

**Acceptance Criteria:**
- Export creates valid DOCX
- Formatting preserved
- Can open in Word

---

#### Task 9.2: PDF Export
**Priority:** High
**Estimated Time:** 3 days

- [ ] Use reportlab library
- [ ] Convert Rich Text to PDF
- [ ] Basic formatting
- [ ] Page numbers
- [ ] Headers/footers

**File:** `kalahari/core/exporters/pdf_exporter.py`

```python
from reportlab.lib.pagesizes import A4
from reportlab.platypus import SimpleDocTemplate, Paragraph, PageBreak
from reportlab.lib.styles import getSampleStyleSheet

class PDFExporter:
    """Export to PDF format"""

    @staticmethod
    def export(book: Book, filepath: str, options: dict):
        """Export book to PDF"""
        doc = SimpleDocTemplate(filepath, pagesize=A4)
        styles = getSampleStyleSheet()
        story = []

        # Title page
        title = Paragraph(book.title, styles['Title'])
        story.append(title)
        story.append(PageBreak())

        # Chapters
        for chapter in book.chapters:
            heading = Paragraph(chapter.title, styles['Heading1'])
            story.append(heading)

            # Content
            paragraphs = parse_rich_text(chapter.content)
            for para in paragraphs:
                p = Paragraph(para.text, styles['Normal'])
                story.append(p)

        # Build PDF
        doc.build(story)
```

**Acceptance Criteria:**
- Export creates valid PDF
- Basic formatting works
- Can open in PDF reader

---

#### Task 9.3: TXT Export
**Priority:** Medium
**Estimated Time:** 1 day

- [ ] Plain text export
- [ ] Strip all formatting
- [ ] UTF-8 encoding
- [ ] Line breaks preserved

**Acceptance Criteria:**
- Creates valid TXT file
- UTF-8 encoding correct

---

### Week 17-20: Graphical Assistant

#### Task 10.1: Assistant Assets
**Priority:** High
**Estimated Time:** External (1-2 weeks)

- [ ] Commission/create realistic animal head renders
  - Lion: 8 moods
  - Meerkat: 8 moods
  - Elephant: 8 moods
  - Cheetah: 8 moods
- [ ] Format: PNG, 200x200px, transparent background
- [ ] Name: `{animal}_{mood}.png`

**Asset List:**
- lion_neutral.png, lion_happy.png, lion_encouraging.png, etc.
- meerkat_neutral.png, meerkat_happy.png, ...
- elephant_neutral.png, ...
- cheetah_neutral.png, ...

**Acceptance Criteria:**
- All 32 images created (4 animals × 8 moods)
- High quality realistic style
- Consistent size and format

---

#### Task 10.2: Assistant Panel Implementation
**Priority:** High
**Estimated Time:** 3 days

- [ ] Create AssistantPanel
- [ ] Load animal images
- [ ] Display current mood
- [ ] Speech bubble rendering
- [ ] Fade-in animation
- [ ] Auto-hide timer
- [ ] Settings button

**File:** `kalahari/gui/panels/assistant_panel.py`

```python
class AssistantPanel(BasePanel):
    """Writing assistant panel"""

    def __init__(self, parent):
        super().__init__(parent, "assistant")

        self.current_animal = "lion"
        self.current_mood = "neutral"

        # Settings button
        settings_btn = wx.Button(self, label="⚙")
        settings_btn.Bind(wx.EVT_BUTTON, self.on_settings)

        # Avatar
        self.avatar = wx.StaticBitmap(self)
        self.load_avatar(self.current_animal, self.current_mood)

        # Speech bubble
        self.speech_bubble = wx.html.HtmlWindow(self, size=(-1, 100))
        self.speech_bubble.Show(False)

        # Timer for auto-hide
        self.hide_timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_hide_timer)

        # Layout...

    def load_avatar(self, animal: str, mood: str):
        """Load animal image"""
        image_path = f"resources/images/assistant/{animal}_{mood}.png"
        bitmap = wx.Bitmap(image_path, wx.BITMAP_TYPE_PNG)
        self.avatar.SetBitmap(bitmap)

    def show_message(self, text: str, mood: str = "neutral", duration: int = 10000):
        """Show message in speech bubble"""
        self.load_avatar(self.current_animal, mood)

        html = f"""
        <div style="padding: 10px; background-color: #ffffff; border: 2px solid #000000; border-radius: 10px;">
            {text}
        </div>
        """
        self.speech_bubble.SetPage(html)
        self.speech_bubble.Show(True)

        # Fade-in animation (simplified)
        self.speech_bubble.SetTransparent(0)
        for i in range(0, 255, 15):
            self.speech_bubble.SetTransparent(i)
            wx.MilliSleep(20)

        # Start auto-hide timer
        self.hide_timer.Start(duration, wx.TIMER_ONE_SHOT)

    def on_hide_timer(self, event):
        """Hide speech bubble after timeout"""
        self.speech_bubble.Show(False)
```

**Acceptance Criteria:**
- Avatar displays correctly
- Mood changes work
- Speech bubble appears/disappears
- Fade-in animation smooth

---

#### Task 10.3: Assistant Logic Engine
**Priority:** High
**Estimated Time:** 4 days

- [ ] Create AssistantEngine class
- [ ] Implement triggers (time, word count, idle, goal reached)
- [ ] Message generation for each animal personality
- [ ] Flow state detection
- [ ] User preference tracking

**File:** `kalahari/core/assistant/engine.py`

```python
class AssistantEngine:
    """AI Assistant logic engine"""

    PERSONALITIES = {
        'lion': {
            'break_reminder': "Even a king needs rest. Stand up and stretch.",
            'goal_reached': "Completed act one. The real challenge begins now!",
            'encouragement': "Worthy work. But you can do more.",
        },
        'meerkat': {
            'break_reminder': "Hey! 90 minutes at the screen. Time to rest those eyes!",
            'goal_reached': "Yes! You did it! 2000 words today!",
            'encouragement': "You're doing great! Just 312 more words!",
        },
        # ... more personalities
    }

    def __init__(self, config: Config):
        self.config = config
        self.animal = config.assistant['animal']
        self.enabled = config.assistant['enabled']
        self.last_message_time = None
        self.work_start_time = datetime.now()
        self.last_break_reminder = datetime.now()

    def check_triggers(self, context: dict) -> Optional[Tuple[str, str]]:
        """
        Check if any triggers fire, return (message, mood) or None

        context = {
            'words_written': 547,
            'total_words': 1547,
            'daily_goal': 2000,
            'time_since_break': 90,  # minutes
            'idle_time': 0,  # seconds
            'in_flow_state': True
        }
        """
        if not self.enabled:
            return None

        if context['in_flow_state']:
            return None  # Don't interrupt flow state

        # Break reminder (every 90 minutes)
        if context['time_since_break'] >= 90:
            msg = self.PERSONALITIES[self.animal]['break_reminder']
            return (msg, 'worried')

        # Goal reached
        if context['total_words'] >= context['daily_goal']:
            msg = self.PERSONALITIES[self.animal]['goal_reached']
            return (msg, 'proud')

        # Encouragement (when close to goal)
        remaining = context['daily_goal'] - context['total_words']
        if 0 < remaining < 500:
            msg = self.PERSONALITIES[self.animal]['encouragement']
            return (msg, 'encouraging')

        return None
```

**Acceptance Criteria:**
- Break reminders fire at 90min
- Goal reached messages fire
- Encouragement messages fire
- Flow state prevents interruption
- Personality differences clear

---

#### Task 10.4: Assistant Settings Dialog
**Priority:** Medium
**Estimated Time:** 2 days

- [ ] Animal selection dropdown
- [ ] Frequency slider (rarely/normal/often)
- [ ] Enable/disable toggle
- [ ] Mood checkboxes (which moods to show)
- [ ] Preview button

**File:** `kalahari/gui/dialogs/assistant_settings_dialog.py`

```python
class AssistantSettingsDialog(wx.Dialog):
    """Assistant configuration dialog"""

    def __init__(self, parent, config: Config):
        super().__init__(parent, title="Assistant Settings", size=(400, 400))

        self.config = config

        # Animal selection
        wx.StaticText(self, label="Choose your assistant:")
        self.animal_choice = wx.Choice(self, choices=[
            'Lion - Majestic Mentor',
            'Meerkat - Friendly Companion',
            'Elephant - Wise Advisor',
            'Cheetah - Energetic Motivator'
        ])
        self.animal_choice.SetSelection(self.get_animal_index(config.assistant['animal']))

        # Frequency
        wx.StaticText(self, label="Message frequency:")
        self.freq_slider = wx.Slider(self, value=1, minValue=0, maxValue=2)
        # 0=rarely, 1=normal, 2=often

        # Enable/disable
        self.enable_checkbox = wx.CheckBox(self, label="Enable assistant")
        self.enable_checkbox.SetValue(config.assistant['enabled'])

        # Mood selection
        wx.StaticText(self, label="Active moods:")
        self.mood_checks = {}
        for mood in ['neutral', 'happy', 'encouraging', 'worried', 'excited', 'tired', 'proud', 'annoyed']:
            cb = wx.CheckBox(self, label=mood.capitalize())
            cb.SetValue(True)  # All enabled by default
            self.mood_checks[mood] = cb

        # Preview button
        preview_btn = wx.Button(self, label="Preview")
        preview_btn.Bind(wx.EVT_BUTTON, self.on_preview)

        # OK/Cancel
        btn_sizer = self.CreateButtonSizer(wx.OK | wx.CANCEL)

        # Layout...

    def on_preview(self, event):
        """Show preview of selected animal"""
        animal = self.get_selected_animal()
        msg = f"Hello! I'm your {animal} assistant!"
        wx.MessageBox(msg, "Preview", wx.OK | wx.ICON_INFORMATION)

    def get_config(self) -> dict:
        """Return updated config"""
        return {
            'animal': self.get_selected_animal(),
            'enabled': self.enable_checkbox.GetValue(),
            'frequency': ['rarely', 'normal', 'often'][self.freq_slider.GetValue()],
            'active_moods': [mood for mood, cb in self.mood_checks.items() if cb.GetValue()]
        }
```

**Acceptance Criteria:**
- All settings work
- Preview shows correct animal
- Settings save to config

---

### Week 21-22: Statistics & Auto-save

#### Task 11.1: Statistics Panel Implementation
**Priority:** High
**Estimated Time:** 3 days

- [ ] Real-time word/char count
- [ ] Progress bars for goals
- [ ] Reading time estimate
- [ ] Page count estimate
- [ ] Chart: Words per day (matplotlib)

**File:** `kalahari/gui/panels/stats_panel.py`

```python
class StatsPanel(BasePanel):
    """Statistics panel"""

    def __init__(self, parent):
        super().__init__(parent, "stats")

        # Word count
        self.word_label = wx.StaticText(self, label="Words: 0 / 0 (0%)")

        # Progress bar
        self.progress_bar = wx.Gauge(self, range=100)

        # Charts
        self.figure = Figure(figsize=(5, 3))
        self.canvas = FigureCanvas(self, -1, self.figure)
        self.ax = self.figure.add_subplot(111)

        # Layout...

        # Bind events
        self.Bind(EVT_STATS_UPDATE, self.on_stats_update)

    def on_stats_update(self, event):
        """Update statistics"""
        words = event.word_count
        goal = 2000  # TODO: Get from config

        # Update labels
        percent = int((words / goal) * 100) if goal > 0 else 0
        self.word_label.SetLabel(f"Words: {words:,} / {goal:,} ({percent}%)")
        self.progress_bar.SetValue(percent)

        # Update chart
        self.update_chart()

    def update_chart(self):
        """Update words per day chart"""
        # Get data from project history
        dates = []  # TODO: Get from project
        words = []

        self.ax.clear()
        self.ax.bar(dates, words)
        self.ax.set_title("Words per Day")
        self.canvas.draw()
```

**Acceptance Criteria:**
- Stats update in real-time
- Progress bar works
- Chart displays correctly

---

#### Task 11.2: Auto-save System
**Priority:** Critical
**Estimated Time:** 2 days

- [ ] Timer-based auto-save (every N minutes)
- [ ] Save indicator in status bar
- [ ] Snapshot system with timestamps
- [ ] Recovery after crash

**File:** `kalahari/core/autosave.py`

```python
class AutoSave:
    """Automatic save system"""

    def __init__(self, project_manager: ProjectManager, config: Config):
        self.project_manager = project_manager
        self.interval = config.get('autosave_interval', 300)  # seconds
        self.enabled = config.get('autosave_enabled', True)
        self.timer = None

    def start(self, project: Book):
        """Start auto-save timer"""
        if not self.enabled:
            return

        self.timer = wx.Timer()
        self.timer.Bind(wx.EVT_TIMER, lambda e: self.save(project))
        self.timer.Start(self.interval * 1000)  # Convert to milliseconds

    def stop(self):
        """Stop auto-save timer"""
        if self.timer:
            self.timer.Stop()

    def save(self, project: Book):
        """Perform auto-save"""
        try:
            # Save to temp location first
            temp_path = f"{project.filepath}.autosave"
            self.project_manager.save_project(project, temp_path)

            # Create snapshot
            snapshot_path = f"{project.filepath}.snapshot.{datetime.now().strftime('%Y%m%d_%H%M%S')}"
            shutil.copy(temp_path, snapshot_path)

            # Keep only last 5 snapshots
            self.cleanup_old_snapshots(project.filepath)

            logger.info(f"Auto-saved project: {project.title}")

        except Exception as e:
            logger.error(f"Auto-save failed: {e}")

    def cleanup_old_snapshots(self, filepath: str):
        """Remove old snapshots, keep last 5"""
        pattern = f"{filepath}.snapshot.*"
        snapshots = sorted(glob.glob(pattern), reverse=True)

        for snapshot in snapshots[5:]:
            os.remove(snapshot)
```

**Acceptance Criteria:**
- Auto-save fires every N minutes
- Status bar shows save status
- Snapshots created
- Old snapshots deleted

---

#### Task 11.3: Crash Recovery
**Priority:** High
**Estimated Time:** 2 days

- [ ] Detect crashed session on startup
- [ ] Show recovery dialog
- [ ] Load from latest snapshot
- [ ] Clear recovery files after successful load

**File:** `kalahari/core/recovery.py`

```python
class CrashRecovery:
    """Crash recovery system"""

    @staticmethod
    def check_for_crash():
        """Check if last session crashed"""
        crash_marker = Path.home() / ".kalahari" / ".running"

        if crash_marker.exists():
            # Last session crashed
            return True

        # Mark session as running
        crash_marker.touch()
        return False

    @staticmethod
    def get_recovery_files() -> List[str]:
        """Get list of auto-save files"""
        autosave_dir = Path.home() / ".kalahari" / "autosave"
        return sorted(autosave_dir.glob("*.snapshot.*"), reverse=True)

    @staticmethod
    def show_recovery_dialog(parent) -> Optional[str]:
        """Show recovery dialog, return selected file or None"""
        files = CrashRecovery.get_recovery_files()

        if not files:
            return None

        dlg = wx.SingleChoiceDialog(
            parent,
            "Kalahari detected a previous crash. Recover unsaved work?",
            "Crash Recovery",
            [f"{f.name} - {datetime.fromtimestamp(f.stat().st_mtime)}" for f in files]
        )

        if dlg.ShowModal() == wx.ID_OK:
            return str(files[dlg.GetSelection()])

        return None

    @staticmethod
    def clear_running_marker():
        """Clear crash marker on clean exit"""
        crash_marker = Path.home() / ".kalahari" / ".running"
        crash_marker.unlink(missing_ok=True)
```

**Acceptance Criteria:**
- Crash detected on restart
- Recovery dialog appears
- Can recover from snapshot
- Marker cleared on exit

---

### Week 23-24: Beta Testing Prep

#### Task 12.1: Beta Tester Recruitment
**Priority:** High
**Estimated Time:** Ongoing (2 weeks)

- [ ] Post on writing forums
- [ ] Post on Reddit (r/writing, r/writers)
- [ ] Facebook writer groups
- [ ] Twitter announcement
- [ ] Target: 20-30 beta testers

**Beta Tester Criteria:**
- Active writers (working on a book)
- Mix of genres (fiction, non-fiction, etc.)
- Various experience levels
- Willing to provide feedback

**Acceptance Criteria:**
- 20+ beta testers recruited
- Feedback form created
- Communication channel set up (Discord/Slack)

---

#### Task 12.2: Beta Build & Packaging
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Create beta installer (Windows)
- [ ] Create installation instructions (Linux)
- [ ] Beta release notes
- [ ] Known issues list
- [ ] Feedback form

**Files:**
- `scripts/build_windows.py` - PyInstaller build script
- `BETA_INSTRUCTIONS.md`
- `KNOWN_ISSUES.md`

**Acceptance Criteria:**
- Installer works on clean Windows machine
- Linux instructions tested
- All docs included

---

#### Task 12.3: Beta Testing Period
**Priority:** Critical
**Estimated Time:** 4 weeks

- [ ] Week 1-2: Initial feedback collection
- [ ] Week 3: Bug fix sprint
- [ ] Week 4: Validation & re-test

**Feedback Categories:**
- Bugs / crashes
- Usability issues
- Feature requests
- Performance problems
- Documentation gaps

**Process:**
1. Tester reports issue → GitHub issue
2. Triage: critical / high / medium / low
3. Fix critical/high bugs
4. Release beta patch
5. Testers validate fix

**Acceptance Criteria:**
- All critical bugs fixed
- >80% high bugs fixed
- Feedback incorporated

---

## 1.0 Release Phase (5-6 months)

**Goal:** Stable, public release on GitHub

**Deliverable:** Kalahari MVP 1.0 (MIT License)

### Week 25: Bug Fixing

#### Task 13.1: Critical Bug Fixes
**Priority:** Critical
**Estimated Time:** 1 week

- [ ] Fix all P0 (critical) bugs from beta
- [ ] Fix all P1 (high) bugs from beta
- [ ] Re-test after each fix

**Process:**
1. Prioritize bugs by severity
2. Fix one bug at a time
3. Write regression test
4. Verify fix manually
5. Mark as resolved

**Acceptance Criteria:**
- Zero P0 bugs
- <5 P1 bugs remaining
- All fixes verified

---

### Week 26: Performance Optimization

#### Task 14.1: Performance Profiling
**Priority:** High
**Estimated Time:** 2 days

- [ ] Profile application startup time
- [ ] Profile document loading time
- [ ] Profile editor typing latency
- [ ] Identify bottlenecks

**Tools:**
- Python cProfile
- Memory profiler
- wxPython profiler

**Acceptance Criteria:**
- Startup < 3 seconds
- Open project < 2 seconds
- Typing latency < 50ms

---

#### Task 14.2: Optimization Implementation
**Priority:** High
**Estimated Time:** 3 days

- [ ] Lazy-load panels
- [ ] Optimize editor rendering
- [ ] Cache word count calculations
- [ ] Reduce memory usage

**Techniques:**
- Lazy imports
- Generator expressions
- Object pooling
- Weak references

**Acceptance Criteria:**
- Performance targets met
- No regressions

---

### Week 27: UI/UX Polish

#### Task 15.1: Icon Set
**Priority:** High
**Estimated Time:** 3 days (external + integration)

- [ ] Commission/create full icon set (32x32px)
  - File operations (new, open, save, etc.)
  - Edit operations (undo, redo, cut, copy, paste)
  - Formatting (bold, italic, underline, align)
  - Tools (find, export, settings)
- [ ] Integrate into toolbar
- [ ] Integrate into menus

**Icon List:**
- new.png, open.png, save.png
- undo.png, redo.png
- cut.png, copy.png, paste.png
- bold.png, italic.png, underline.png
- align_left.png, align_center.png, align_right.png, justify.png
- find.png, replace.png
- export.png
- settings.png
- assistant.png

**Acceptance Criteria:**
- All icons created
- Consistent style
- High DPI support

---

#### Task 15.2: Splash Screen
**Priority:** Medium
**Estimated Time:** 2 days

- [ ] Create splash screen graphics
  - Background (savanna landscape)
  - 4 animal heads for random selection
- [ ] Implement splash screen
- [ ] Loading progress bar
- [ ] Random animal selection

**File:** `kalahari/gui/splash_screen.py`

```python
class SplashScreen(wx.SplashScreen):
    """Application splash screen"""

    def __init__(self, parent=None):
        # Random animal
        animals = ['lion', 'meerkat', 'elephant', 'cheetah']
        animal = random.choice(animals)

        # Load splash image
        bitmap = self.create_splash_bitmap(animal)

        super().__init__(
            bitmap,
            wx.adv.SPLASH_CENTRE_ON_SCREEN | wx.adv.SPLASH_TIMEOUT,
            2000,  # 2 seconds minimum
            parent
        )

    def create_splash_bitmap(self, animal: str) -> wx.Bitmap:
        """Create splash bitmap with background + animal"""
        # Load background
        bg = wx.Image("resources/images/splash_bg.png")

        # Load animal
        animal_img = wx.Image(f"resources/images/assistant/{animal}_neutral.png")

        # Composite
        # ... (draw animal on background, add text, version, etc.)

        return bg.ConvertToBitmap()
```

**Acceptance Criteria:**
- Splash displays on startup
- Random animal works
- Looks professional

---

#### Task 15.3: About Dialog
**Priority:** Low
**Estimated Time:** 1 day

- [ ] Create AboutDialog
- [ ] Show version, build date
- [ ] Credits
- [ ] License info
- [ ] Links (GitHub, website)

**Acceptance Criteria:**
- Dialog displays correctly
- All info accurate

---

### Week 28: Documentation

#### Task 16.1: User Manual
**Priority:** Critical
**Estimated Time:** 4 days

- [ ] Getting Started guide
- [ ] Feature overview
- [ ] Step-by-step tutorials
- [ ] FAQ
- [ ] Troubleshooting

**Sections:**
1. Installation
2. Creating your first project
3. Writing and formatting
4. Managing characters and locations
5. Using the assistant
6. Exporting your work
7. Tips and tricks
8. FAQ

**Format:** Markdown + HTML (generated)

**Acceptance Criteria:**
- All features documented
- Screenshots included
- Clear instructions

---

#### Task 16.2: Video Tutorials
**Priority:** Medium
**Estimated Time:** 3 days

- [ ] Tutorial 1: Installation & First Project (5 min)
- [ ] Tutorial 2: Writing & Formatting (10 min)
- [ ] Tutorial 3: Character Bank (8 min)
- [ ] Tutorial 4: Assistant (5 min)
- [ ] Tutorial 5: Export (5 min)

**Platform:** YouTube
**Format:** Screen recording + voiceover

**Acceptance Criteria:**
- 5 videos published
- High quality (1080p)
- Clear audio

---

#### Task 16.3: API Documentation
**Priority:** Low
**Estimated Time:** 2 days

- [ ] Document plugin API (for future)
- [ ] Code examples
- [ ] Sphinx documentation

**Acceptance Criteria:**
- API endpoints documented
- Examples work

---

### Week 29: Testing

#### Task 17.1: Automated Testing
**Priority:** High
**Estimated Time:** 4 days

- [ ] Unit tests (70%+ coverage)
- [ ] Integration tests
- [ ] UI tests (pytest-qt)
- [ ] Set up CI/CD

**Test Categories:**
- Models (Book, Chapter, Character)
- File I/O (save/load)
- Exporters (DOCX, PDF, TXT)
- Config system
- Assistant logic

**File:** `tests/test_integration.py`

```python
def test_full_workflow(tmp_path):
    """Test complete user workflow"""
    # Create project
    book = Book(id="test", title="Test Book", author="Test")

    # Add chapters
    ch1 = book.add_chapter("Chapter 1")
    ch1.content = "Lorem ipsum dolor sit amet..."

    # Save
    filepath = tmp_path / "test.klh"
    ProjectManager.save_project(book, str(filepath))

    # Load
    loaded = ProjectManager.load_project(str(filepath))

    # Verify
    assert loaded.title == "Test Book"
    assert len(loaded.chapters) == 1
    assert loaded.chapters[0].content == ch1.content
```

**Acceptance Criteria:**
- 70%+ code coverage
- All tests pass
- CI runs on push

---

#### Task 17.2: Manual QA
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Test on Windows 10
- [ ] Test on Windows 11
- [ ] Test on Ubuntu 22.04
- [ ] Test on Linux Mint
- [ ] Manual test checklist (100+ items)

**Checklist Categories:**
- Installation
- First run experience
- Project creation/opening
- Editing operations
- Export functions
- Assistant functionality
- Settings & preferences
- Error handling
- Performance

**Acceptance Criteria:**
- All manual tests pass
- No critical issues
- Checklist signed off

---

### Week 30: Release

#### Task 18.1: Installers
**Priority:** Critical
**Estimated Time:** 3 days

- [ ] Windows installer (NSIS)
- [ ] Linux AppImage
- [ ] Installation guides for each platform

**Windows:**
```bash
poetry run pyinstaller kalahari.spec --clean
makensis installer.nsi
```

**Linux:**
```bash
poetry run pyinstaller kalahari.spec --clean
appimagetool kalahari.AppDir
```

**Acceptance Criteria:**
- Installers work on clean machines
- All dependencies bundled
- File associations work (.klh)

---

#### Task 18.2: GitHub Preparation
**Priority:** Critical
**Estimated Time:** 2 days

- [ ] Polish README.md
- [ ] Add badges (license, version, build status)
- [ ] Create CONTRIBUTING.md
- [ ] Create CODE_OF_CONDUCT.md
- [ ] Add LICENSE (MIT)
- [ ] Create CHANGELOG.md
- [ ] Set up GitHub Pages (docs)

**README.md Structure:**
1. Logo + tagline
2. Features
3. Screenshots
4. Installation
5. Quick start
6. Documentation link
7. Contributing
8. License
9. Credits

**Acceptance Criteria:**
- README professional
- All files in place
- Repo ready for public

---

#### Task 18.3: Release Announcement
**Priority:** High
**Estimated Time:** 1 day

- [ ] Create release notes
- [ ] Tag version 1.0.0
- [ ] Create GitHub release
- [ ] Upload installers
- [ ] Announce on social media
- [ ] Post on forums

**Platforms:**
- GitHub (release)
- Reddit (r/writing, r/opensource)
- HackerNews
- Twitter
- Writing forums

**Announcement Template:**
```
🎉 Kalahari 1.0 - Writer's IDE is now available!

After 6 months of development, we're proud to release Kalahari,
a free and open-source writing environment for book authors.

Features:
✅ Rich text editor with formatting
✅ Character & location banks
✅ Source library
✅ AI writing assistant (4 personalities!)
✅ Export to DOCX, PDF, TXT
✅ Statistics & goal tracking
✅ Cross-platform (Windows, Linux)

Download: https://github.com/username/kalahari/releases
License: MIT (free and open source)

We'd love your feedback!
```

**Acceptance Criteria:**
- Release published on GitHub
- Announced on 5+ platforms
- Downloads tracked

---

## Dependencies & Prerequisites

### External Dependencies

1. **Assets (Week 17-18)**
   - 32 animal head renders (4 animals × 8 moods)
   - Icon set (20+ icons)
   - Splash screen graphics (background + animals)
   - **Lead Time:** 2-3 weeks
   - **Provider:** Graphic designer / AI generation

2. **Beta Testers (Week 23)**
   - 20-30 writers
   - **Lead Time:** 2 weeks recruitment
   - **Source:** Writing communities

### Technical Prerequisites

- Python 3.11+ installed
- Poetry installed
- Git installed
- wxPython compatible OS
- Development tools (compiler for wxPython)

---

## Risk Assessment

### High Risk Items

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| wxPython compatibility issues | High | Medium | Early testing on all platforms |
| Assistant assets delayed | Medium | Medium | Start commissioning in Week 10 |
| Beta tester recruitment fails | High | Low | Start early, multiple channels |
| Performance issues with large projects | High | Medium | Profiling and optimization sprints |
| File format corruption bugs | Critical | Low | Extensive testing, checksums |

### Medium Risk Items

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Export quality issues (DOCX/PDF) | Medium | Medium | Early prototyping, user testing |
| Assistant annoying users | Medium | Medium | Configurable, can disable |
| Cross-platform UI inconsistencies | Medium | High | Test on all platforms regularly |

---

## Success Metrics

### Alpha Success
- [ ] Demo completes without crash
- [ ] Can create, edit, save, load project
- [ ] Internal team approval

### Beta Success
- [ ] 20+ beta testers recruited
- [ ] <10 critical bugs reported
- [ ] >70% tester satisfaction
- [ ] All core features work

### 1.0 Success
- [ ] Zero P0 bugs
- [ ] 70%+ test coverage
- [ ] Performance targets met
- [ ] Documentation complete
- [ ] Installers work on all platforms
- [ ] Public release on GitHub
- [ ] 100+ GitHub stars in first month (aspirational)
- [ ] 1,000+ downloads in first month (aspirational)

---

**End of MVP Breakdown v1.0**
