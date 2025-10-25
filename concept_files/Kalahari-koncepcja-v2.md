# KALAHARI - Koncepcja Programu

## 1. Wizja i Cel

**Kalahari** to zaawansowane środowisko pisarskie (Writer's IDE) dedykowane autorom książek, oferujące kompleksowe wsparcie całego procesu twórczego - od koncepcji, przez pisanie, po finalną publikację.

### Misja
Stworzyć narzędzie, które pozwala autorom skoncentrować się na twórczości, eliminując bariery techniczne i organizacyjne związane z pisaniem książek.

### Grupa docelowa
- Pisarze powieści (science fiction, fantasy, kryminały, romanse, inne)
- Autorzy literatury faktu i popularnonaukowej
- Pisarze książek historycznych
- Dziennikarze piszący reportaże książkowe
- Początkujący autorzy potrzebujący struktury i wsparcia
- Doświadczeni pisarze szukający profesjonalnego narzędzia

---

## 2. Stack Technologiczny

### 2.1 Język programowania
- **Python 3.11+**
  - Doskonała integracja z bibliotekami AI/ML
  - Bogaty ekosystem bibliotek
  - Łatwość prototypowania i rozwoju
  - Cross-platform

### 2.2 GUI Framework
- **wxPython 4.2+ (wxWidgets)**
  - Natywny wygląd na każdej platformie
  - Wydajność zbliżona do natywnych aplikacji
  - Bogaty zestaw kontrolek
  - `wx.richtext.RichTextCtrl` dla edytora tekstu (rozważenie alternatyw, w tym możliwość wykorzystania silnika OpenOffice Writer - wybór w programie; konieczność zapewnienia integracji danych między edytorami)
  - `wx.aui` dla zarządzania układem okien (pełne zarządzanie GUI, zapisywanie układu)
  - `wx.stc.StyledTextCtrl` (Scintilla) dla trybu Markdown

### 2.3 Bazy danych
- **SQLite 3**
  - Główna baza danych projektu (metadane, konfiguracja)
  - Biblioteka źródeł
  - Historia zmian i wersjonowanie
  - Full-text search (FTS5)

- **JSON**
  - Struktura projektu książki
  - Eksport/import ustawień
  - Snapshoty projektów

### 2.4 Format dokumentów (do zastanowienia)
- **Własny format wewnętrzny** (JSON + embedded resources) - do sprawdzenia, jakie formaty są wspierane przez kontrolkę wxPython (xml, html, ale czy inne?)
- **RTF** dla edytora rich text - nei mam przekonania
- **Markdown** dla trybu pisania bez rozpraszaczy - pisarz musi mieć łatwość przełaczania sie między trybami/edytorami, więc chyba lepiej zaproponować bogaty i prosty wygląd głównego edytora
- **HTML** jako format pośredni (do rozważenia w związku z możliwościami wxPython)

### 2.5 Biblioteki kluczowe

#### Edycja tekstu i formatowanie
- **python-docx** - eksport do MS Word (.docx)
- **reportlab** lub **weasyprint** - generowanie PDF
- **ebooklib** - tworzenie EPUB
- **Markdown** - parser i renderer
- **python-rtf** - obsługa RTF

#### OCR i analiza obrazów
- **pytesseract** (Tesseract OCR) - OCR dla skanów
- **pdf2image** - konwersja PDF na obrazy
- **Pillow (PIL)** - przetwarzanie obrazów

#### AI i analiza tekstu
- **OpenAI API** lub **Anthropic Claude API** - asystent AI (do rozważenia możliwości wykorzystania mechanizmów lokalnych; włąsny lokalny RAG)
- **langchain** - framework do pracy z LLM
- **spaCy** - NLP (analiza tekstu, rozpoznawanie nazwisk)
- **transformers** (Hugging Face) - lokalne modele AI

#### Sprawdzanie języka
- **language-tool-python** - sprawdzanie gramatyki
- **pyspellchecker** lub **hunspell** - sprawdzanie ortografii
- **pl_core_news_sm** (spaCy) - model dla języka polskiego

#### Zarządzanie projektami
- **GitPython** - integracja z Git dla wersjonowania
- **watchdog** - monitorowanie zmian w plikach
- **schedule** - automatyczne backupy

#### Wizualizacje
- **matplotlib** - wykresy statystyk
- **networkx** - grafy powiązań postaci
- **graphviz** - wizualizacje wątków

#### Eksport i raportowanie
- **Jinja2** - templating dla raportów
- **pandoc** (wrapper) - konwersje między formatami

#### Multimedialne
- **pygame** lub **python-vlc** - odtwarzanie audio/wideo
- **mutagen** - metadane plików audio

#### Komunikacja
- **smtplib** + **email** - wysyłanie maili do wydawcy
- **requests** - API calls
- **icalendar** - zarządzanie kalendarzem

#### Utilities
- **python-dateutil** - operacje na datach
- **pytz** - strefy czasowe
- **tqdm** - progress bars
- **loguru** - zaawansowane logowanie
- **pydantic** - walidacja danych
- **cryptography** - szyfrowanie backupów

#### GUI Enhancements
- **wxasync** - Asynchronous support dla wxPython (background tasks bez freezing UI)
- **ObjectListView** - Advanced list controls dla characters/locations (sorting, filtering, grouping)

#### Performance & Optimization
- **msgpack** - Szybsza serializacja niż JSON (internal storage, caching)
- **lz4** - Ultra-szybka kompresja dla backupów (szybsze niż gzip/zip)

#### AI/NLP Specific
- **tiktoken** - Token counting dla AI APIs (cost estimation, context limits)
- **anthropic** - Official Claude SDK (lepszy niż generic HTTP client)

#### Development Tools (dev dependencies, nie w prodzie)
- **black** - Auto-formatter PEP 8 (consistent code style)
- **mypy** - Static type checker (catch bugs early)
- **ruff** - Super-fast linter (zamienia flake8, isort, etc.)
- **pytest** - Testing framework
- **pytest-cov** - Coverage reports
- **pytest-wxpython** - wxPython testing utilities

### 2.6 Package Management ✅ USTALONE

**Manager:** Poetry (preferowany) lub uv (alternatywa)

**Dlaczego Poetry?**
- ✅ Nowoczesny dependency resolver (unika konfliktów)
- ✅ Reproducible builds (poetry.lock file)
- ✅ Virtual environment management (automatyczny)
- ✅ Build tool (packaging, distribution)
- ✅ Cross-platform (Windows, Linux, macOS identycznie)
- ✅ Industry standard w 2024/2025

**pyproject.toml - przykład:**
```toml
[tool.poetry]
name = "kalahari"
version = "0.1.0"
description = "Writer's IDE - Advanced writing environment for book authors"
authors = ["Your Name <email@example.com>"]
license = "MIT"
readme = "README.md"

[tool.poetry.dependencies]
python = "^3.11"
wxpython = "^4.2.0"
anthropic = "^0.21.0"
python-docx = "^1.1.0"
reportlab = "^4.0.0"
ebooklib = "^0.18"
gitpython = "^3.1.0"
loguru = "^0.7.0"
pydantic = "^2.5.0"
# ... (all dependencies)

[tool.poetry.group.dev.dependencies]
black = "^23.12.0"
mypy = "^1.7.0"
ruff = "^0.1.7"
pytest = "^7.4.0"
pytest-cov = "^4.1.0"

[tool.poetry.scripts]
kalahari = "kalahari.main:main"

[build-system]
requires = ["poetry-core"]
build-backend = "poetry.core.masonry.api"
```

**Workflow:**
```bash
# Install poetry
curl -sSL https://install.python-poetry.org | python3 -

# Setup project
poetry install              # Install all dependencies
poetry add wxpython         # Add new package
poetry remove old-package   # Remove package
poetry update              # Update dependencies
poetry lock                # Lock versions (reproducible)
poetry build               # Build wheel/sdist
poetry run kalahari        # Run application
```

**Alternatywa: uv (jeśli performance krytyczna)**
- 10-100x szybszy niż pip
- Compatible z poetry
- Napisany w Rust
- Młodszy projekt (mniej mature)

### 2.7 Struktura projektu (propozycja)

```
kalahari/
├── src/
│   ├── core/              # Rdzeń aplikacji
│   │   ├── project.py     # Zarządzanie projektem książki
│   │   ├── document.py    # Model dokumentu
│   │   ├── database.py    # Warstwa bazodanowa
│   │   └── config.py      # Konfiguracja
│   ├── gui/               # Interfejs użytkownika
│   │   ├── main_window.py
│   │   ├── editor/        # Edytor tekstu
│   │   ├── organizer/     # Organizator warsztatu
│   │   ├── sources/       # Biblioteka źródeł
│   │   ├── characters/    # Bank postaci
│   │   ├── timeline/      # Oś czasu
│   │   └── assistant/     # Graficzny asystent
│   ├── modules/           # Moduły funkcjonalne
│   │   ├── export/        # Eksport do różnych formatów
│   │   ├── import/        # Import projektów
│   │   ├── backup/        # System backupów
│   │   ├── statistics/    # Statystyki i motywacja
│   │   ├── ai/            # Integracja AI
│   │   ├── ocr/           # OCR źródeł
│   │   ├── validation/    # Walidatory
│   │   └── communication/ # Komunikacja z wydawcą
│   ├── templates/         # Szablony projektów
│   └── resources/         # Zasoby (ikony, style)
├── tests/                 # Testy jednostkowe
├── docs/                  # Dokumentacja
└── data/                  # Dane aplikacji
    ├── dictionaries/      # Słowniki
    ├── templates/         # Szablony dokumentów
    └── themes/            # Motywy wizualne
```

---

## 3. Funkcjonalności (Features)

### 3.0 Interfejs użytkownika (GUI Layout) ✅ USTALONE

#### Architektura GUI
Kalahari wykorzystuje **wxPython** z **wxAUI** (Advanced User Interface) dla elastycznego systemu dockable panels.

**Filozofia designu:**
- Wszystkie panele dokowane (nie floating windows)
- Pełna konfigurowalność układu (drag & drop)
- Zapisywanie perspektyw (layouts)
- Wsparcie dla wielu monitorów
- Responsive design (skalowanie dla różnych rozdzielczości)

#### Struktura głównego okna

```
┌─────────────────────────────────────────────────────────────┐
│ Menu Bar  [File] [Edit] [View] [Project] [Tools] [Help]     │
├─────────────────────────────────────────────────────────────┤
│ Toolbar  [New] [Save] [Undo] [Redo] [Bold] [Italic] [...]   │
├──────────┬────────────────────────────┬─────────────────────┤
│          │                            │  Preview/Inspector  │
│ Project  │                            │  ┌───────────────┐  │
│ Navigator│        EDITOR              │  │ Live preview  │  │
│          │   (Rich Text / Markdown)   │  │ of export     │  │
│ ├ Chapters│                            │  └───────────────┘  │
│ ├ Scenes  │   [Document tabs here]     │                     │
│ ├ Characters│                          │  Properties Panel   │
│ ├ Locations│                          │  ┌───────────────┐  │
│ └ Sources│                            │  │ Font: Arial   │  │
│          │                            │  │ Size: 12pt    │  │
│          │                            │  └───────────────┘  │
│          │                            ├─────────────────────┤
│          │                            │   Assistant Panel   │
│          │                            │  ┌───────────────┐  │
│          │                            │  │   [Lion 🦁]   │  │
│          │                            │  │               │  │
│          │                            │  │ ╭───────────╮ │  │
│          │                            │  │ │Speech bbl │ │  │
│          │                            │  │ ╰───────────╯ │  │
│          │                            │  └───────────────┘  │
├──────────┴────────────────────────────┴─────────────────────┤
│ Output/Statistics/Timeline Panel                             │
│ [Stats: 1,547 words | 8,921 chars] [Timeline] [Notes]       │
└─────────────────────────────────────────────────────────────┘
│ Status Bar: Document: Chapter_03.rtf | Words: 1547 | 67%    │
└─────────────────────────────────────────────────────────────┘
```

#### Panele dokowane (wxAUI)

##### 1. Project Navigator (Lewy panel) ✅ MVP
**Rozmiar domyślny:** 250px szerokości
**Pozycja:** Lewo, pełna wysokość
**Zawartość:**
- **Tree view** hierarchii projektu:
  ```
  📚 Book Project
  ├─ 📁 Part I: Beginning
  │  ├─ 📄 Chapter 1: Introduction
  │  ├─ 📄 Chapter 2: The Journey
  │  └─ 📄 Chapter 3: Discovery
  ├─ 📁 Part II: Middle
  └─ 📁 Part III: End

  👥 Characters (15)
  📍 Locations (8)
  📚 Sources (23)
  📅 Calendar
  ```
- **Kontekstowe menu** (prawy klik): New, Delete, Rename, Properties
- **Drag & drop** dla reorganizacji struktury
- **Ikony** wskazujące typ elementu
- **Search bar** u góry (filtrowanie drzewa)

**Zakładki (tabs) w tym panelu:**
- Project (domyślnie)
- Characters Bank
- Locations Bank
- Sources Library
- Calendar (opcjonalnie)

##### 2. Central Editor (Panel centralny) ✅ MVP
**Rozmiar:** Elastyczny (zajmuje resztę miejsca)
**Zawartość:**
- **Tab control** dla wielu otwartych dokumentów
- **Rich Text Editor** (wx.richtext.RichTextCtrl)
- **Toolbar kontekstowy** (formatowanie tekstu)
- **Minimap** (opcjonalny scrollbar po prawej z podglądem dokumentu)

**Tryby edytora:**
1. **Normal Mode** - pełny GUI z wszystkimi panelami
2. **Focused Mode** - tylko editor + assistant (ukrywa pozostałe panele)
3. **Distraction-Free Mode** - tylko editor na pełnym ekranie (F11)

**Features edytora:**
- Line numbering (opcjonalnie)
- Word wrap (domyślnie włączony)
- Syntax highlighting dla Markdown (jeśli tryb MD)
- Spell checking (podkreślenia na czerwono)
- Auto-save indicator (ikona w tab)

##### 3. Preview/Inspector (Prawy górny panel) ⏳ FAZA 2
**Rozmiar domyślny:** 300px szerokości
**Pozycja:** Prawo górne
**Zawartość:**

**Zakładki:**
1. **Preview** - Live preview eksportu (HTML rendering)
2. **Properties** - Właściwości zaznaczonego elementu
3. **Minimap** - Podgląd struktury dokumentu (opcjonalnie)

**Preview tab:**
- Renderowanie tekstu jak po eksporcie
- Wybór formatu podglądu (DOCX style, EPUB style, PDF)
- Auto-scroll synchronizowany z edytorem

**Properties tab:**
- Font, size, style zaznaczonego tekstu
- Paragraph properties
- Metadata elementu (timestamp, author)

##### 4. Assistant Panel (Prawy dolny panel) ✅ MVP
**Rozmiar domyślny:** 300x350px
**Pozycja:** Prawo dolne
**Zawartość:**
- Avatar zwierzęcia (200x200px)
- Speech bubble z komunikatami
- Możliwość zwinięcia do małej ikony

**Szczegóły:** Patrz sekcja 3.3 "Graficzny asystent"

##### 5. Output/Statistics Panel (Dolny panel) ✅ MVP
**Rozmiar domyślny:** 150-200px wysokości
**Pozycja:** Dół, pełna szerokość
**Zawartość:**

**Zakładki:**
1. **Statistics** - Statystyki bieżącego dokumentu
2. **Timeline** - Oś czasu wydarzeń w książce
3. **Notes** - Szybkie notatki
4. **Progress** - Wykres postępu prac

**Statistics tab:**
```
Words: 1,547 / 3,000 (51.6%)  ━━━━━━━━━━░░░░░░░░░░
Characters: 8,921 (with spaces) | 7,234 (without)
Pages: 6.2 (A4, 12pt, single-spaced)
Reading time: ~7 minutes
```

**Timeline tab:**
- Interaktywna oś czasu
- Wydarzenia fabuły z datami
- Drag & drop do edycji
- Eksport do PNG

**Notes tab:**
- Szybkie notatki podczas pisania
- Auto-save
- Tagging
- Search

##### 6. Calendar Panel (Opcjonalnie) ⏳ FAZA 2
**Pozycja:** Zakładka w lewym panelu lub osobny floating
**Zawartość:**
- Kalendarz pisarza z deadlinami
- Writing goals i tracking
- Sesje pisarskie (writing sprints)
- Historia produktywności

#### Menu Bar ✅ MVP

**[File]**
- New Project... (Ctrl+N)
- Open Project... (Ctrl+O)
- Save (Ctrl+S)
- Save As...
- Close Project
- ---
- Import... (Scrivener, yWriter, DOCX, etc.)
- Export... (DOCX, PDF, EPUB, etc.)
- ---
- Recent Projects >
- ---
- Exit (Ctrl+Q)

**[Edit]**
- Undo (Ctrl+Z)
- Redo (Ctrl+Y)
- ---
- Cut (Ctrl+X)
- Copy (Ctrl+C)
- Paste (Ctrl+V)
- Paste Special...
- ---
- Find... (Ctrl+F)
- Replace... (Ctrl+H)
- Find in Project... (Ctrl+Shift+F)
- ---
- Preferences... (Ctrl+,)

**[View]**
- Panels >
  - Project Navigator (Toggle)
  - Preview/Inspector (Toggle)
  - Assistant (Toggle)
  - Statistics (Toggle)
  - Output (Toggle)
- ---
- Focus Mode (F9)
- Distraction-Free Mode (F11)
- Fullscreen (F11)
- ---
- Layout >
  - Default Layout
  - Writer Layout
  - Editor Layout
  - Save Current Layout...
  - Manage Layouts...
- ---
- Zoom In (Ctrl++)
- Zoom Out (Ctrl+-)
- Zoom Reset (Ctrl+0)

**[Project]**
- Add Chapter...
- Add Scene...
- ---
- New Character...
- New Location...
- New Source...
- ---
- Project Properties...
- Project Statistics...
- ---
- Compile/Export Project...

**[Tools]**
- Spell Check (F7)
- Grammar Check
- ---
- Assistant Settings...
- Writing Sprint... (Pomodoro timer)
- ---
- Generate Name... (Generator nazw)
- Thesaurus... (Synonyms)
- ---
- Options/Preferences...

**[Help]**
- Help Contents (F1)
- Tutorials...
- ---
- Check for Updates...
- Report Bug...
- ---
- About Kalahari...

#### Toolbar ✅ MVP

**Quick access icons (32x32px):**
```
[New] [Open] [Save] │ [Undo] [Redo] │ [Cut] [Copy] [Paste] │
[Bold] [Italic] [Underline] │ [Align L/C/R/J] │ [Find] [Export] │
[Focus Mode] [Assistant Toggle] [Settings]
```

**Customization:**
- Drag & drop toolbar reorganization
- Add/remove icons
- Icon size (16/24/32/48px)
- Show labels (text under icons)

#### Status Bar ✅ MVP

**Layout:**
```
[Document: Chapter_03.rtf] [Autosave: OK] [Words: 1,547] [Chars: 8,921]
[Progress: 67%] [Line: 45, Col: 12] [Language: PL] [Mode: Rich Text]
```

**Sections:**
1. Document name (clickable → properties)
2. Autosave indicator (green checkmark)
3. Word count (live update)
4. Character count (with/without spaces)
5. Progress indicator (% completion vs goal)
6. Cursor position (line, column)
7. Language (spell checker)
8. Edit mode (Rich Text / Markdown)

#### Perspectives (Layouts) ✅ MVP

**Predefiniowane layouty:**

1. **Default Perspective** - wszystkie panele widoczne
2. **Writer Perspective** - Project + Editor + Assistant + Stats
3. **Editor Perspective** - tylko Editor + minimal toolbars
4. **Research Perspective** - Sources Library + Editor + Preview

**User-defined perspectives:**
- Save current layout with name
- Quick switch (Ctrl+1, Ctrl+2, etc.)
- Export/Import layouts (XML)

#### Responsywność i accessibility

**Skalowanie:**
- DPI aware dla HiDPI displays
- Minimum resolution: 1366x768
- Recommended: 1920x1080 lub wyżej
- Multi-monitor support

**Keyboard shortcuts:**
- Wszystkie funkcje dostępne z klawiatury
- Customizable shortcuts
- Vim mode (opcjonalnie)

**Accessibility:**
- Screen reader compatible (NVDA, JAWS)
- High contrast themes
- Keyboard navigation dla wszystkiego
- Accessible tooltips

#### Themes ⏳ FAZA 2

**Built-in themes:**
1. Light (default) - jasny, profesjonalny
2. Dark - ciemny, dla nocnego pisania
3. Sepia - vintage, ciepły
4. Africa - kolory sawanny (beże, pomarańcze, zieleń)

**Custom themes:**
- JSON-based theme files
- User-created themes
- Share themes with community

---

### 3.1 Core Features (MVP)

#### A. Zarządzanie projektem książki
- **Kreator nowego projektu**
  - Wizard z wyborem gatunku/typu książki
  - Gotowe szablony struktury (powieść, literatura faktu, historyczna, naukowa)
  - Ustawienia podstawowe (tytuł, autor, język, lokalizacja)

- **Format projektu**
  - Struktura JSON z embedded resources
  - SQLite dla metadanych i wyszukiwania
  - Kompresja do pojedynczego pliku .klh (Kalahari Book Project) - SUPER!

- **Automatyczne wersjonowanie**
  - Auto-save co N minut (konfigurowalne)
  - System snapshotów z timestampami
  - Historia zmian z możliwością powrotu
  - Integracja z Git (opcjonalnie)

- **Backup i eksport**
  - Automatyczne backupy do .zip z checksumami MD5/SHA256
  - Eksport projektu do różnych formatów
  - Import projektów z innych narzędzi (Scrivener, yWriter)

#### B. Edytor tekstu

##### Tryb Rich Text
- **wx.richtext.RichTextCtrl** z rozszerzeniami
- Formatowanie: bold, italic, underline, strike-through
- Nagłówki (H1-H6), listy (punktowane, numerowane)
- Wyrównanie tekstu
- Kolorowanie tekstu i tła
- Style i arkusze stylów
- Obsługa przypisów dolnych i końcowych

##### Tryb Markdown (Distraction-Free) - nie jestem do tego przekonany (powody wspomniane wyżej; autor nie musi znać składni markdown i powinien mieć możliwość prostego przełączania się w różne tryby pracy: pełen GUI, pełny edytor, skupienie itd.)
- Prosty edytor z podglądem na żywo
- Minimalistyczny interfejs (tryb "tablicy korkowej")
- Fokus na pisaniu bez rozpraszaczy
- Obsługa podstawowej składni Markdown

##### Funkcje edytora
- **Notatki w tekście**
  - Komentarze przypisane do fragmentów
  - Kolory notatek (TODO, idea, sprawdzić, usunąć)
  - Panel notatek na boku

- **Dowiązania źródeł**
  - Linkowanie fragmentów tekstu do źródeł
  - Wizualna indykacja (np. kolorowa linijka)
  - Szybki dostęp do źródła (tooltip, klik)

- **Statystyki na żywo**
  - Licznik znaków/słów/stron
  - Czas czytania (reading time)
  - Poziom trudności tekstu (Flesch-Kincaid)
  - Tempo pisania (plan/harmonogram vs realizacja; system motywacji/wyzwań - do przemyślenia)
  
- **Wyszukiwanie i zamiana**
  - Find & Replace z regex
  - Wyszukiwanie w całym projekcie (lub na róznych poziomach: w tekście, w źródłach, w postaciach itd.)
  - Historia wyszukiwań

#### C. Organizacja warsztatu pisarskiego

##### Bank postaci
- **Karty postaci** z polami:
  - Imię, nazwisko, pseudonimy
  - Wygląd fizyczny (opis, zdjęcia) - lub możliwość samodzielnego zestawienia wyglądu postaci w prostym edytorze graficznym (płeć (symbol), kolor skóry, kolor włosów, itd.) 
  - Osobowość, cechy charakteru (może na zasadzie tagów???)
  - Biografia, motywacje
  - Relacje z innymi postaciami
  - Pojawienia się w rozdziałach (linki)

- **Graf powiązań**
  - Wizualizacja relacji między postaciami (networkx)
  - Rodzaje relacji (rodzina, przyjaciel, wróg, mentor)
  - Intensywność relacji
  - Eksport do PNG/SVG

##### Bank miejsc
- **Karty lokacji**
  - Nazwa, opis geograficzny
  - Zdjęcia, mapy
  - Historia miejsca
  - Powiązane postacie
  - Sceny rozgrywające się w lokacji
  - Powiązane źródłą - np. strony wikipedii itd.
  
- **Mapa interaktywna** (opcjonalnie)
  - Import map (obrazy)
  - Tworzenie mapy świata ???
  - Zaznaczanie lokacji
  - Ścieżki podróży postaci

##### Bank przedmiotów
- **Karty przedmiotów**
  
  - Nazwa przedmiotu (np. "pierścień", "tajemnica")
  - Opis, znaczenie symboliczne
  - Pojawienia się w tekście
  - Ewolucja przedmiotu
  
- **Tracking przedmiotów**
  - Gdzie się znajduje dany przedmiot
  - Kto go posiada w danym momencie
  - Graf przepływu przedmiotów
  
  MOTYWY POWINNY BYĆ POWIĄZANE Z POSTACIAMI i powinny definiować modus operandi, motywy działania itd.

##### Timeline (Oś czasu)
- **Chronologia wydarzeń**
  - Globalna oś czasu książki (lub różne osie dla róznych rodziałów, np. dwie linie czasowe)
  - Wydarzenia dla każdej postaci
  - Synchronizacja wątków
  - Wykrywanie niespójności czasowych

- **Widoki**
  - Liniowa oś czasu (lub wiele osi)
  - Kalendarz/kalendarze
  - Lista wydarzeń

- **Eksport**
  - Timeline jako obraz
  - Raport chronologii

##### Planer wątków
- **Wizualizacja wątków**
  - Mapa myśli dla głównych wątków
  - Struktura rozdziałów i scen
  - Drag & drop dla reorganizacji

- **Analiza struktury**
  - Struktura trzech aktów
  - Punkty zwrotne (plot points)
  - Climax, rozwiązanie

#### D. Biblioteka źródeł

##### Typy źródeł
- Dokumenty (PDF, DOCX, TXT, RTF)
- Obrazy (JPG, PNG, WEBP)
- Audio (MP3, WAV, OGG)
- Wideo (MP4, AVI)
- Linki internetowe (z web clipping)
- Notatki tekstowe

##### Funkcje biblioteki
- **Kategoryzacja**
  - Foldery i tagi
  - Wyszukiwanie full-text (SQLite FTS5)
  - Filtry zaawansowane

- **OCR**
  - Automatyczne OCR dla skanów (Tesseract)
  - Korekta rozpoznanego tekstu
  - Wyszukiwanie w zeskanowanych dokumentach

- **Przeglądarka źródeł**
  - Wbudowana przeglądarka PDF
  - Viewer obrazów z zoom
  - Odtwarzacz audio/wideo
  - Podświetlanie cytatów użytych w tekście

- **Integracja AI**
  - Możliwość odpytywania źródeł (AI Q&A)
  - Automatyczne streszczenia
  - Opcjonalna integracja z Google NotebookLM

##### Walidator cytatów
- Sprawdzanie czy cytaty w tekście mają przypisane źródło
- Weryfikacja poprawności cytatów
- Ostrzeżenia o brakujących źródłach
- Generator bibliografii (Chicago, MLA, APA, Harvard)

#### E. Moduł notatek

##### Notatki robocze
- Osobny panel dla notatek ogólnych
- Kategoryzacja notatek
- Linkowanie do rozdziałów, postaci, miejsc

##### "Żółte karteczki" (Sticky Notes)
- Małe okienka floating notes
- Przypinanie do fragmentów tekstu
- Kolorowanie według priorytetu
- Auto-hide i show on hover

#### F. Kalendarz i harmonogram

##### Kartka z kalendarza
- **Widok tradycyjnej kartki**
  - Aktualna data
  - Wschody/zachody słońca i księżyca (na podstawie lokalizacji)
  - Fazy księżyca
  - Imieniny (polskie, międzynarodowe)
  - Święta państwowe i religijne

- **Lokalizacja**
  - Wybór miasta/współrzędnych
  - Automatyczne dane astronomiczne

##### Planer pracy
- **Harmonogram pisania**
  - Widoki: dzień, tydzień, miesiąc
  - Cele dzienne/tygodniowe (liczba słów)
  - Deadline'y (oddanie rozdziału, końcowa data)

- **Mapa myśli projektu**
  - Graficzny roadmap książki
  - Kamienie milowe
  - Postęp prac (%)

##### Komunikacja z wydawcą
- **Kalendarz spotkań**
  - Terminy spotkań, deadlines wydawcy
  - Eksport do iCal
- **Automatyczne maile**
  - Szablony maili
  - Wysyłanie raportów postępu
  - Przypomnienia o terminach

### 3.2 Statystyki i motywacja

#### Panel statystyk
- **Ogólne**
  - Całkowita liczba słów/znaków
  - Liczba rozdziałów/scen
  - Szacowany czas czytania
  - Postęp względem celu

- **Dzienny tracking**
  - Słowa napisane dziś/ średnio słowa na godzinę
  - Czas spędzony na pisaniu
  - Wykres produktywności (ostatnie 7/30 dni)
  - Streaks (dni z rzędu z pisaniem)

- **Temperatura pracy**
  - Dużo i szybko (wysoka produktywność)
  - Dużo i powoli (planowanie)
  - Mało i szybko (poprawki)
  - Mało i powoli (kryzys twórczy?)

- **Analiza tekstu**
  - Najczęściej używane słowa
  - Długość zdań (średnia, rozkład)
  - Różnorodność słownictwa
  - Analiza dialogów vs narracja

#### Pasek postępu
- Wizualizacja celu dziennego/tygodniowego
- Animacje przy osiąganiu kamieni milowych (preferowana właściwa reakcja asystenta)
- Konfetti przy ukończeniu rozdziału! (preferowana informacja i pochwała od asystenta)

#### System osiągnięć (Achievements)/ osiągnięcia dzienne,tygodniowe, miesięczne
- Odznaki za milestones (pierwsza 1000 słów, 10k, 50k, 100k)
- "Pisarz nocny" - pisanie po 22:00
- "Poranny ptaszek" - pisanie przed 7:00
- "Maraton" - 5k słów w jeden dzień
- "Konsekwentny" - 30 dni z rzędu

### 3.3 Graficzny asystent

#### Koncepcja główna
**JEDEN asystent do wyboru** - użytkownik wybiera JEDNO ulubione zwierzę, które staje się jego osobistym asystentem pisarskim.

Asystent jest wyświetlany w odrębnej ramce GUI (wxAUI), którą w każdej chwili można wyłączyć lub przywrócić. Avatar u góry i komiksowy tekst u dołu. Avatar zmienia się w zalezności od "nastroju" asystenta.

- **Avatar** - graficzna reprezentacja w formie "gadającej głowy" afrykańskiego zwierzęcia
- **Wspólne zadania dla wszystkich asystentów**:
  - Monitorowanie pracy pisarza
  - Pochwały za dobrą pracę i osiągnięcia
  - Przypomnienia o odpoczynku i zdrowiu
  - Motywowanie do działania
  - Monitorowanie postępów
  - Podpowiedzi dotyczące tekstu

- **Różnica**: Każde zwierzę wykonuje TE SAME zadania, ale w swój charakterystyczny sposób - zgodnie ze swoją osobowością!

#### Stany emocjonalne (wspólne dla wszystkich)
  - Zadowolony - gdy piszesz produktywnie
  - Zachęcający - gdy przestajesz pisać
  - Zmartwiony - gdy długo brak aktywności
  - Ekscytujący - gdy osiągasz cele
  - Śpiący/zmęczony - gdy pracujesz bardzo długo
  - Dumny - przy osiągnięciach
  - Neutralny - stan podstawowy
  - Złu - gdy ignorowany (np. powinieneś zrobić przewę 5 minut, ale wciąż pracujesz)

Wszystkie stany konfigurowalne - autor książki jest tu szefem i może nie chce widzieć jakichś stanów.

#### Podstawowe funkcje asystenta (te same dla wszystkich zwierząt)

##### 1. Przypomnienia o zdrowiu
- Przypomnienie o przerwach co X minut
- Reguła 20-20-20 dla oczu
- Zachęta do rozciągania się
- Przypomnienie o piciu wody

##### 2. Motywacja i pochwały
- Komentarze do postępu prac
- Gratulacje przy osiąganiu celów
- Dopingowanie przy spadku motywacji
- Celebrowanie kamieni milowych

##### 3. Monitorowanie postępów
- Tracking celów dziennych/tygodniowych
- Informacje o zbliżaniu się do celu
- Przypomnienia o deadlinach
- Statystyki produktywności

##### 4. Podpowiedzi pisarskie
- Wykrywanie powtórzeń słów
- Przypominanie o zaniedbanych wątkach
- Sugestie dotyczące struktury
- Wskazywanie niespójności

##### 5. Zarządzanie sesjami
- Obsługa writing sprints
- Zarządzanie fokusem
- Przypomnienia o planowanych zadaniach

#### Konfiguracja
- **Wybór awatara** - JEDNO zwierzę spośród dostępnych (patrz niżej)
- **Częstotliwość przypomnień** - jak często asystent się odzywa
- **Custom triggers** - kiedy asystent powinien interweniować
- **Wyłączenie asystenta** - dla purystów
- **Aktywne nastroje asystenta** - np. wyłączenie upomnień

#### Dostępni asystenci - afrykańskie zwierzęta

**Każde zwierzę wykonuje wszystkie podstawowe funkcje asystenta, ale w swoim charakterystycznym stylu!**

**🎯 MVP:** Dostępne 4 zwierzęta (Lew, Surykatka, Słoń, Gepard)
**📅 Faza 2:** Pozostałe 4 zwierzęta (Żyrafa, Bawół, Papuga, Kameleon)

---

##### **Lew** (Lion) - Majestatyczny mentor [DOMYŚLNY]
- **Osobowość**: majestatyczny, pewny siebie, przywódczy, autorytatywny, wymagający
- **Styl komunikacji**: Silny, autorytatywny, motywujący, stawiający wyzwania
- **Jak przypomina o odpoczynku**: "Nawet król potrzebuje odpoczynku. Wstań i się rozciągnij."
- **Jak gratuluje**: "Godne uznania. Ale możesz jeszcze więcej."
- **Jak wskazuje błąd**: "Ta scena potrzebuje więcej mocy. Nie oszczędzaj emocji!"
- **Przy osiągnięciu celu**: "Ukończyłeś pierwszy akt. Teraz zaczyna się prawdziwe wyzwanie!"

**Dlaczego Lew jako domyślny:**
- Symbol marki Kalahari (logo)
- Reprezentuje autorytet i mądrość
- Konsekwentny branding (Lew = storyteller)

##### **Surykatka** (Meerkat) - Przyjazny towarzysz
- **Osobowość**: czujny, pomocny, przyjazny, społeczny, troskliwy
- **Styl komunikacji**: Ciepły, przyjacielski, bezpośredni, używa wykrzykników
- **Jak przypomina o odpoczynku**: "Hej! Już 90 minut przed ekranem. Może dasz oczom odpocząć?"
- **Jak gratuluje**: "Świetnie Ci idzie! Jeszcze tylko 312 słów do dzisiejszego celu!"
- **Jak wskazuje błąd**: "Zauważyłem, że to słowo powtarza się bardzo często..."
- **Przy osiągnięciu celu**: "Tak! Zrobiłeś to! 2000 słów dzisiaj!"

##### **Słoń** (Elephant) - Mądry doradca
- **Osobowość**: mądry, spokojna pewność, cierpliwy, doskonała pamięć
- **Styl komunikacji**: Spokojny, refleksyjny, pełen mądrości, niespiesznie
- **Jak przypomina o odpoczynku**: "Pamiętaj, że odpoczynek jest częścią procesu twórczego. Czas na przerwę."
- **Jak gratuluje**: "Widzę stały postęp. Konsekwencja prowadzi do sukcesu."
- **Jak wskazuje błąd**: "Jeśli dobrze pamiętam, w rozdziale 3 ta postać miała niebieskie oczy..."
- **Przy osiągnięciu celu**: "Jeszcze jeden kamień milowy za Tobą. Mądrze wykorzystałeś dzisiejszy dzień."

##### **Gepard** (Cheetah) - Energiczny motywator
- **Osobowość**: szybki, efektywny, skupiony, energiczny, dynamiczny
- **Styl komunikacji**: Krótki, energetyczny, pełen akcji, skupiony na tempie
- **Jak przypomina o odpoczynku**: "Regeneracja! Nawet gepard musi odpocząć po sprincie!"
- **Jak gratuluje**: "Wow! 500 słów w 15 minut! To jest tempo!"
- **Jak wskazuje błąd**: "Zwolnij! To słowo trzeci raz z rzędu!"
- **Przy osiągnięciu celu**: "FINISH! Rekordowe tempo dzisiaj! 847 słów/h!"

---

**⏳ FAZA 2 - Dodatkowi asystenci (post-MVP):**

##### **Żyrafa** (Giraffe) - Delikatny strateg
- **Osobowość**: widzi szerszą perspektywę, delikatna, elegancka, myśląca długoterminowo
- **Styl komunikacji**: Delikatny, pokazujący szerszy kontekst, refleksyjny
- **Jak przypomina o odpoczynku**: "Z perspektywy całego dnia, przerwa teraz da Ci więcej energii później."
- **Jak gratuluje**: "Spójrz jak daleko zaszedłeś. Z tej perspektywy widać już końcową linię."
- **Jak wskazuje błąd**: "Patrząc na całość - ta scena może być trochę za długa."
- **Przy osiągnięciu celu**: "Widzę piękną strukturę całości. To dobrze się układa."

##### **Bawół** (Buffalo) - Wytrwały kompan
- **Osobowość**: wytrzymały, konsekwentny, silny, nieustępliwy, twardy
- **Styl komunikacji**: Prosty, bezpośredni, skupiony na konsekwencji i sile
- **Jak przypomina o odpoczynku**: "Siła wymaga regeneracji. Zrób przerwę."
- **Jak gratuluje**: "To już 14 dzień z rzędu! Tak trzymaj!"
- **Jak wskazuje błąd**: "To wymaga poprawy. Ale jesteś wystarczająco silny, by to zrobić."
- **Przy osiągnięciu celu**: "Konsekwencja prowadzi do zwycięstwa. Cel osiągnięty."

##### **Papuga** (Parrot) - Gadatliwy językoznawca
- **Osobowość**: kolorowy, gadatliwy, zwraca uwagę na język, komunikatywny
- **Styl komunikacji**: Bogaty słownie, pełen synonimów, skupiony na języku
- **Jak przypomina o odpoczynku**: "Relaks, odpoczynek, przerwa, czas na regenerację oczu!"
- **Jak gratuluje**: "Brawo! Rewelacja! Fantastycznie! Jeszcze 312 słów!"
- **Jak wskazuje błąd**: "To słowo użyłeś już 5 razy. Może: ekscytujący, pasjonujący, fascynujący?"
- **Przy osiągnięciu celu**: "Sukces! Cel osiągnięty! Wspaniałe! Znakomite!"

##### **Kameleon** (Chameleon) - Elastyczny obserwator
- **Osobowość**: adaptujący się, elastyczny, obserwujący, zmienny, dostosowujący ton
- **Styl komunikacji**: Zmienia styl w zależności od kontekstu, elastyczny
- **Jak przypomina o odpoczynku**: *[dostosowuje ton do nastroju pisarza]* "Widzę że jesteś zmęczony... może przerwa?"
- **Jak gratuluje**: *[dopasowuje się do gatunku]* "Świetny thriller! Trzyma w napięciu!"
- **Jak wskazuje błąd**: "Ten ton nie pasuje do gatunku. Może bardziej... [sugestia]"
- **Przy osiągnięciu celu**: *[celebruje w stylu gatunku książki]* "Happy end! Cel osiągnięty!"

#### Porównanie - jak różne zwierzęta reagują na tę samą sytuację

Przykład: **Użytkownik pracuje już 2 godziny bez przerwy**

| Zwierzę | Reakcja |
|---------|---------|
| **Surykatka** | "Hej! Już 2 godziny przed ekranem! Czas dać oczom odpocząć! 🎯" |
| **Słoń** | "Pamiętaj, że nawet najdłuższa podróż wymaga postojów. Czas na przerwę." |
| **Gepard** | "Regeneracja! Po 2h sprintu potrzebujesz breakpoint!" |
| **Lew** | "Nawet król musi odpocząć, by zachować siłę. Wstań." |
| **Żyrafa** | "Z perspektywy całego dnia - przerwa teraz da Ci więcej energii na później." |
| **Bawół** | "Siła wymaga odpoczynku. 2 godziny za Tobą. Przerwa." |
| **Papuga** | "Relaks! Odpoczynek! Regeneracja! Już 120 minut!" |
| **Kameleon** | *[dostosowuje ton]* "Widzę, że jesteś zmęczony... może czas na przerwę?" |

Przykład: **Użytkownik osiągnął cel 2000 słów dziennie**

| Zwierzę | Reakcja |
|---------|---------|
| **Surykatka** | "Tak! Zrobiłeś to! 2000 słów! Jesteś niesamowity! 🎉" |
| **Słoń** | "Cel osiągnięty. Konsekwencja prowadzi do mądrości. Dobrze się spisałeś." |
| **Gepard** | "FINISH! 2000 słów! Rekordowe tempo! 💪" |
| **Lew** | "Godne króla. Ale pamiętaj - to dopiero początek." |
| **Żyrafa** | "Patrząc na całość - już 40% książki za Tobą. Piękna struktura!" |
| **Bawół** | "Cel osiągnięty. Konsekwencja prowadzi do zwycięstwa. Tak trzymaj." |
| **Papuga** | "Brawo! Wspaniale! Fantastycznie! Cel! 2000! Sukces!" |
| **Kameleon** | *[w stylu gatunku]* "Thrilling! Ta książka będzie petardą! Cel osiągnięty!" |

#### Implementacja techniczna

##### Grafika i styl ✅ USTALONE
- **Styl graficzny**: Realistyczny (jak logo - majestatyczny lew)
- **Format**: Wysokiej jakości obrazy statyczne (PNG z przezroczystością)
- **Bank nastrojów**: 6-8 różnych obrazów głowy na każde zwierzę
  - Neutralny (domyślny)
  - Zadowolony
  - Zachęcający
  - Zmartwiony
  - Ekscytujący
  - Zmęczony
  - Dumny
  - Złość/upomnienie (opcjonalnie)
- **MVP**: Statyczne obrazy + dymki tekstowe
- **Faza 2**: Rozważenie subtelnych animacji (mruganie, lekki ruch głowy)

##### UI/UX ✅ USTALONE
- **Panel asystenta**: Dokowany panel wxAUI (nie floating window!)
  - Lokalizacja: konfigurowalna (domyślnie prawy dolny róg)
  - Rozmiar: ~250x350px (avatar + dymek)
  - Możliwość zwinięcia/rozwinięcia
  - Możliwość całkowitego ukrycia
- **Layout panelu**:
  ```
  ┌─────────────────┐
  │   [Avatar]      │  ← Obrazek głowy zwierzęcia
  │    200x200      │
  ├─────────────────┤
  │  ╭─────────╮    │
  │  │ Tekst   │    │  ← Speech bubble (dymek)
  │  │ asyst.  │    │
  │  ╰─────────╯    │
  └─────────────────┘
  ```
- **Speech bubble**: Comic-style dymek (konfigurowalne kolory)
  - Font: czytelny, sans-serif, rozmiar 10-12pt
  - Auto-wrap dla długich tekstów
  - Fade-in animacja pojawienia się (200ms)
  - Auto-hide po X sekundach (konfigurowalne, domyślnie 10s)

##### Personalizacja
- **Wybór zwierzęcia**: Dropdown lub wizard przy pierwszym uruchomieniu
- **Zmiana asystenta**: W każdej chwili przez Settings
- **Częstotliwość interwencji**: Suwak "rzadko → często"
- **Custom triggers**: Zaawansowane ustawienia (przerww, cele, deadliny)
- **Tryb pracy**:
  - Aktywny (proaktywne przypomnienia)
  - Pasywny (tylko na żądanie użytkownika)
  - Wyłączony
- **Konfiguracja nastrojów**: Możliwość wyłączenia wybranych stanów (np. "złość")

##### Inteligencja i zachowanie
- **Uczenie się**: Asystent analizuje wzorce pracy użytkownika
- **Kontekst czasowy**: Dostosowanie komunikatów do pory dnia
- **Flow state detection**: Nie przerywa podczas intensywnej pracy
- **Pamięć preferencji**: Zapamiętywa reakcje użytkownika na różne typy komunikatów
- **Progresywne zmniejszanie**: Jeśli użytkownik regularnie ignoruje przypomnienia → rzadziej się odzywa

##### Implementacja techniczna (kod)
- **wxPanel** z własnym malowaniem lub **wx.StaticBitmap** dla avatara
- **wx.html.HtmlWindow** dla speech bubble (wsparcie HTML/CSS dla stylizacji)
- **Timer** dla automatycznego ukrywania dymków
- **Event system** dla komunikacji z głównym oknem (wxPython events)
- **JSON config** dla ustawień asystenta (częstotliwość, wybrany avatar, etc.)

### 3.4 Eksport i publikacja

#### Formaty eksportu
- **DOCX** (Microsoft Word)
  - Ze stylami i formatowaniem
  - Przypisy, spis treści
  - Gotowy do wysłania do wydawcy

- **PDF**
  - Z konfigurowalnymi stylami
  - Różne profile (draft, final, ebook)
  - Watermarks, numeracja stron

- **EPUB** (ebook)
  - EPUB 2/3
  - Z metadanymi (autor, ISBN, okładka)
  - Table of contents

- **HTML**
  - Pojedynczy plik lub strona
  - Z CSS

- **Markdown**
  - Czysty Markdown
  - Dla Github, blog, itp.

- **LaTeX**
  - Dla publikacji naukowych

- **Scrivener/yWriter format**
  - Dla migracji do innych narzędzi

#### Ustawienia eksportu
- **Profile eksportu**
  - Zapisywanie ustawień dla powtarzalnego eksportu
  - Wydawca A, Wydawca B (różne wymagania)

- **Selekcja treści**
  - Eksport całości lub wybranych rozdziałów
  - Włączanie/wyłączanie notatek, komentarzy

- **Preprocessing**
  - Automatyczne zastępowania (np. "..." → "…")
  - Usuwanie podwójnych spacji
  - Normalizacja znaków końca linii

### 3.5 Dodatkowe funkcje

#### Sprawdzanie języka
- **Ortografia**
  - Słowniki dla wielu języków (polski, angielski, etc.)
  - Słownik użytkownika (dodawanie słów)
  - Sprawdzanie w tle z podkreślaniem

- **Gramatyka**
  - Integracja z LanguageTool
  - Sugestie poprawek
  - Ignorowanie false positives

- **Styl**
  - Wykrywanie powtórzeń
  - Sugestie synonimy (thesaurus)
  - Analiza czytelności

#### Generator nazw
- **Postacie**
  - Generator imion i nazwisk
  - Różne kultury i epoki
  - Gatunki fantasy (elfy, krasnoludy)

- **Miejsca**
  - Generator nazw miast, gór, rzek
  - Stylizacja według gatunku

#### Tryb Focus (Distraction-Free)
- **Minimalistyczny interfejs**
  - Pełny ekran
  - Tylko edytor tekstu
  - Opcjonalnie: muzyka w tle (ambient)
  - Animowany background (opcjonalnie)

- **Typewriter mode**
  - Przewijanie tekstu, kursor zawsze na środku
  - Wygaszanie pozostałych akapitów

#### Sesje pisarskie (Writing Sprints)
- Timer na określony czas (15/25/45 min)
- Blokowanie innych funkcji podczas sprintu
- Podsumowanie produktywności po sprincie
- Możliwość grupowych sprintów online (przyszłość)

#### Współpraca
- **Beta-readers**
  - Eksport projektu dla recenzentów
  - Zbieranie komentarzy
  - Import feedbacku

- **Kontrola wersji**
  - Porównywanie wersji (diff)
  - Merge'owanie zmian
  - Annotated history

#### Dark/Light mode
- **Motywy wizualne**
  - Jasny motyw (dzień)
  - Ciemny motyw (noc)
  - Sepia (vintage)
  - Africa (kolory pustyni i sawanny)
  - Custom themes (JSON)
- **Ikony**
  - Zestawy ikon (modern, classic, minimal)
  - Kustomizacja kolorów

#### Dostępność
- **Skróty klawiszowe**
  - Konfigurowalne shortcuts
  - Vim mode (opcjonalnie)

- **Czytnik ekranu**
  - Kompatybilność z NVDA/JAWS

- **Skalowanie interfejsu**
  - DPI aware
  - Zoom in/out

---

## 4. Architektura systemu

### 4.1 Wzorce projektowe
- **MVC/MVP** dla separacji logiki i UI
- **Plugin architecture** dla rozszerzeń
- **Observer pattern** dla synchronizacji widoków
- **Strategy pattern** dla różnych eksporterów

### 4.2 Bezpieczeństwo
- **Szyfrowanie backupów** (opcjonalne, AES-256)
- **Haszowanie haseł** dla funkcji współpracy
- **Bezpieczne przechowywanie API keys** (AI, NotebookLM)

### 4.3 Wydajność
- **Lazy loading** dla dużych projektów
- **Indeksowanie full-text** (SQLite FTS5)
- **Asynchroniczne operacje** (backup, eksport)
- **Cache** dla często używanych danych

### 4.4 Testowanie
- **Unit tests** (pytest)
- **Integration tests**
- **UI tests** (opcjonalnie)
- **CI/CD** z GitHub Actions

---

## 5. Roadmap ✅ ZAKTUALIZOWANO

**Strategia:** MVP 1.0 (5-6 miesięcy) → Faza 2 → Faza 3 → Faza 4

### Faza 1: MVP 1.0 (5-6 miesięcy) ✅ USTALONE

MVP rozbite na 3 subfazy: Alpha → Beta → 1.0

#### **Alpha (1-2 miesiące)** - Fundament techniczny
**Cel:** Działający prototyp z core functionality

**Features:**
- [ ] **Architektura projektu**
  - [ ] Struktura katalogów (MVC/MVP pattern)
  - [ ] System konfiguracji (JSON config files)
  - [ ] Logging system
  - [ ] Error handling framework
- [ ] **GUI Framework**
  - [ ] wxPython + wxAUI setup
  - [ ] Główne okno z menu bar
  - [ ] Dockable panels system
  - [ ] Podstawowe panele (Project Navigator, Editor, Stats)
- [ ] **Project Management**
  - [ ] Tworzenie nowego projektu (.klh file format)
  - [ ] Otwieranie/zamykanie projektów
  - [ ] Zapisywanie (manual + auto-save)
  - [ ] Format: JSON + SQLite + ZIP compression
- [ ] **Edytor tekstu - podstawy**
  - [ ] wx.richtext.RichTextCtrl integration
  - [ ] Podstawowe formatowanie (bold, italic, underline)
  - [ ] Undo/Redo
  - [ ] Copy/Paste
  - [ ] Word count (live)

**Deliverable:** Działający proof-of-concept dla internal testing

---

#### **Beta (3-4 miesiące od startu)** - Kompletne features
**Cel:** Feature-complete MVP gotowy do user testing

**Features:**
- [ ] **Edytor tekstu - kompletny**
  - [ ] Pełne formatowanie Rich Text
  - [ ] Nagłówki, listy, wyrównanie
  - [ ] Style i arkusze stylów
  - [ ] Przypisy
  - [ ] Find & Replace (basic)
- [ ] **Bank postaci**
  - [ ] Karty postaci (podstawowe pola)
  - [ ] CRUD operations
  - [ ] Panel w lewym dock
  - [ ] Lista postaci + detail view
- [ ] **Bank miejsc**
  - [ ] Karty lokacji (podstawowe pola)
  - [ ] CRUD operations
  - [ ] Panel w lewym dock
- [ ] **Biblioteka źródeł**
  - [ ] Import plików (PDF, TXT, DOCX, obrazy)
  - [ ] Organizacja w katalogi
  - [ ] Podstawowe tagowanie
  - [ ] Linkowanie do fragmentów tekstu
  - [ ] **BEZ OCR, BEZ AI** (Faza 2)
- [ ] **Eksport podstawowy**
  - [ ] DOCX (ze stylami)
  - [ ] PDF (basic)
  - [ ] TXT (plain text)
- [ ] **Graficzny asystent** ✅ W MVP!
  - [ ] 4 zwierzęta (Lew, Surykatka, Słoń, Gepard)
  - [ ] Statyczne obrazy (6-8 nastrojów per zwierzę)
  - [ ] Speech bubble system
  - [ ] Dokowany panel (prawy dolny)
  - [ ] Podstawowa inteligencja (health reminders, motivations)
  - [ ] Konfiguracja (wybór zwierzęcia, częstotliwość)
- [ ] **Statystyki**
  - [ ] Licznik słów, znaków, stron
  - [ ] Reading time estimate
  - [ ] Panel dolny z wykresami (matplotlib)
  - [ ] Cele dzienne/tygodniowe + tracking
- [ ] **Auto-save & Backupy**
  - [ ] Auto-save co N minut (konfigurowalne)
  - [ ] System snapshotów z timestamps
  - [ ] Backup do ZIP z checksumami
  - [ ] Recovery po crash

**Deliverable:** Beta release dla zamkniętej grupy beta testerów (20-30 osób)

---

#### **1.0 (5-6 miesięcy od startu)** - Polish & Release
**Cel:** Stabilny, publiczny release MVP

**Tasks:**
- [ ] **Bug fixing** z beta testów
- [ ] **Performance optimization**
  - [ ] Lazy loading dla dużych projektów
  - [ ] Optymalizacja renderowania edytora
  - [ ] Memory management
- [ ] **UI/UX polish**
  - [ ] Ikony (komplet set 32x32px)
  - [ ] Splash screen z losowym zwierzęciem
  - [ ] About dialog
  - [ ] Tooltips dla wszystkich funkcji
- [ ] **Dokumentacja**
  - [ ] User manual (PDF + HTML)
  - [ ] Tutorials (video + text)
  - [ ] FAQ
  - [ ] API documentation dla przyszłych pluginów
- [ ] **Instalatory**
  - [ ] Windows (NSIS installer, .exe)
  - [ ] Linux (AppImage + instructions)
  - [ ] macOS (opcjonalnie, jeśli czas pozwoli)
- [ ] **Testy**
  - [ ] Unit tests (pytest, 70%+ coverage)
  - [ ] Integration tests
  - [ ] Manual QA checklist
  - [ ] Cross-platform testing
- [ ] **Branding & Marketing prep**
  - [ ] Logo finalizacja (app icon + multi-animal)
  - [ ] Landing page (statyczna)
  - [ ] GitHub README polish
  - [ ] License file (MIT)
  - [ ] CONTRIBUTING.md
- [ ] **Release na GitHub**
  - [ ] Repo publiczny (MIT License)
  - [ ] Release notes
  - [ ] Instalatory w GitHub Releases
  - [ ] Announcement (social media, fora pisarskie)

**Deliverable:** Kalahari MVP 1.0 - publiczny open source release!

---

### Faza 2: Rozbudowa Core + Premium (6-9 miesięcy po MVP)

**Core (Open Source):**
- [ ] Eksport do EPUB (ebook)
- [ ] Timeline/oś czasu (zaawansowana wizualizacja)
- [ ] Planer wątków (wizualizacja grafu)
- [ ] Graf powiązań postaci (networkx)
- [ ] Moduł notatek ("żółte karteczki")
- [ ] Dark mode + tematyzacja
- [ ] Kalendarz pisarza (goals, deadlines)
- [ ] Sprawdzanie ortografii i gramatyki (LanguageTool)
- [ ] Thesaurus i synonimy
- [ ] 4 dodatkowe zwierzęta-asystenci (Żyrafa, Bawół, Papuga, Kameleon)

**Premium Features (Closed Source Plugins):**
- [ ] Graficzny asystent z zaawansowaną AI (wszystkie 8 zwierząt)
- [ ] OCR dla materiałów źródłowych (Tesseract)
- [ ] Zaawansowana analiza tekstu z Claude/GPT
- [ ] Generator nazw (AI-powered)
- [ ] Wykrywanie niespójności fabularnych
- [ ] Zaawansowane visualizations (timeline, plots)
- [ ] Worldbuilding module (fantasy/sci-fi)

**System:**
- [ ] System pluginów (API, loading mechanism)
- [ ] System licencjonowania premium features
- [ ] Marketplace infrastructure (podstawy)
- [ ] Strategia instalacyjna dla platform (packaging)

---

### Faza 3: Cloud Services (9-12 miesięcy po MVP)

**Victoria - Cloud Sync Service:**
- [ ] Backend infrastructure (FastAPI/Flask)
- [ ] Synchronizacja projektów między urządzeniami
- [ ] End-to-end encryption
- [ ] Conflict resolution
- [ ] Wersjonowanie w chmurze
- [ ] Automatyczne backupy
- [ ] Web access do projektów (read-only)
- [ ] Model subskrypcyjny

**Integracje zewnętrzne:**
- [ ] NotebookLM integration (research assistant)
- [ ] Dropbox/Google Drive sync (alternatywa)
- [ ] Git-based sync (opcjonalnie dla power users)

---

### Faza 4: Ekosystem & Współpraca (12-18 miesięcy po MVP)

**Serengeti - Collaborative Writing:**
- [ ] Multi-author support
- [ ] Real-time editing (WebSocket)
- [ ] System dla beta-readers
- [ ] Komentarze i feedback
- [ ] Wersjonowanie współpracy
- [ ] Moduł komunikacji z wydawcą

**Community & Marketplace:**
- [ ] Marketplace pluginów
- [ ] Marketplace szablonów (book templates)
- [ ] Forum użytkowników Kalahari
- [ ] Writing challenges & sprints online
- [ ] Sharing custom themes & layouts

**Mobile:**
- [ ] Sahara Mobile Companion (iOS/Android)
- [ ] Read-only access do projektów
- [ ] Podstawowe notatki
- [ ] Sync z desktop

**Analytics:**
- [ ] Ngorongoro Analytics & Insights
- [ ] Statystyki sprzedaży (integracja z platformami)
- [ ] Engagement metrics
- [ ] AI-powered writing insights

---

## 6. Analiza konkurencji

### Scrivener
**Mocne strony:**
- Uznany standard w branży
- Bogata funkcjonalność
- Cork board view (to uważam za głupie, niech user sam zdecyduje, czy chce mieć tło)

**Słabe strony:**
- Nie ma integracji AI
- Brak polskich słowników
- Przestarzały interfejs
- Drogi (€60)

**Nasza przewaga:**
- AI assistant
- Polski język (UI + słowniki)
- Graficzny asystent motywacyjny
- Open source (potencjalnie)

### yWriter
**Mocne strony:**
- Darmowy
- Fokus na strukturę powieści

**Słabe strony:**
- Tylko Windows
- Bardzo przestarzały
- Brak funkcji zaawansowanych

**Nasza przewaga:**
- Cross-platform
- Nowoczesny UI
- Dużo więcej funkcji

### Manuskript
**Mocne strony:**
- Open source
- Markdown support
- Darmowy

**Słabe strony:**
- Buggy
- Brak wsparcia
- Uboga funkcjonalność

**Nasza przewaga:**
- Stabilność
- Wsparcie
- Rozbudowana funkcjonalność

---

## 7. Nowe propozycje (od asystenta AI)

### 7.1 Moduł Research Assistant
- **Web scraping** dla researchu
- Zapisywanie stron w formacie offline
- Automatyczne tagowanie zebranego materiału
- "Research mode" - przeglądarka wbudowana w aplikację

### 7.2 Moduł Voice-to-Text
- Dyktowanie fragmentów książki
- Integracja z Whisper (OpenAI)
- Transkrypcja wywiadów (dla reportaży)

### 7.3 Moduł Translator Assistant
- Pomoc w tłumaczeniu książki na inne języki
- Glossariusz terminów dla tłumaczy
- Przekazywanie projektu tłumaczom

### 7.4 Worldbuilding Module (dla fantasy/sci-fi)
- **System magii** - definicje zasad
- **System walutowy**
- **System polityczny**
- **Języki** - słowniczek wymyślonych słów
- **Fauna i flora**
- **Technologia** (dla sci-fi)

### 7.5 Character Interview
- AI prowadzi wywiad z postacią
- Pomaga w zgłębieniu psychologii postaci
- Generuje nowe pomysły na rozwój postaci

### 7.6 Plot Hole Detector
- AI analizuje fabułę
- Wykrywa dziury fabularne
- Wykrywa niespójności
- Sugeruje rozwiązania

### 7.7 Writing Prompts Generator
- Generator promptów na kryzys twórczy
- Losowe sytuacje, konflikty, dialogi
- Dostosowany do gatunku książki

### 7.8 Scene Mood Board
- Pinterest-like board dla każdej sceny
- Zdjęcia inspirujące klimat
- Palety kolorów
- Muzyka do sceny

### 7.9 Publishing Wizard
- Przewodnik po publikacji
- Checklist przed wysłaniem do wydawcy
- Wzory umów wydawniczych
- Kalkulatory tantiemów

### 7.10 Community Features
- Forum użytkowników Kalahari (web)
- Wymiana szablonów
- Marketplace dla pluginów
- Writing challenges

---

## 8. Potwierdzone decyzje strategiczne

### 8.1 Model biznesowy ✅ USTALONE

**Strategia:** Open Core + SaaS Hybrid

Kalahari przyjmuje sprawdzony model biznesowy łączący open source z funkcjami premium i usługami chmurowymi.

#### Core (Open Source)
**Licencja:** MIT License
**Repozytorium:** GitHub (publiczne po MVP 1.0)
**Funkcje:** Wszystkie podstawowe narzędzia pisarskie

**Dlaczego Open Source?**
- ✅ Zaufanie społeczności i możliwość wkładu
- ✅ Transparentność i audyty bezpieczeństwa
- ✅ Marketing poprzez społeczność open source
- ✅ Budowanie bazy użytkowników
- ✅ Wartość edukacyjna

**Funkcje Core (Darmowe na zawsze):**
- Edytor tekstu (Rich Text)
- Zarządzanie projektem książki
- Bank postaci (podstawowy)
- Bank miejsc (podstawowy)
- Biblioteka źródeł (podstawowa)
- Eksport do DOCX, PDF, TXT, EPUB
- Statystyki podstawowe
- Auto-save i backupy
- Integracja z Git
- Import z podstawowych formatów (Markdown, DOCX, TXT)

#### Premium Features (Closed Source)
**Dystrybucja:** Płatne pluginy/rozszerzenia
**Model płatności:** Do ustalenia (jednorazowy zakup lub subskrypcja)

**Funkcje Premium:**
- 🦁 Graficzny asystent AI (8 zwierząt z osobowościami)
- 🤖 Zaawansowana analiza tekstu z AI
- 📊 Zaawansowane analytics i insights
- 📷 OCR dla materiałów źródłowych
- 🗺️ Zaawansowana wizualizacja timeline
- 📈 Narzędzia analizy fabuły
- 🔍 Głębokie wykrywanie niespójności
- 🌍 Worldbuilding module (fantasy/sci-fi)

#### Cloud Services (SaaS)
**Dystrybucja:** Usługi webowe oparte na subskrypcji
**Model płatności:** Miesięczna/roczna subskrypcja

**Usługi Cloud:**
- ☁️ **Victoria** - Synchronizacja w chmurze między urządzeniami
- 🤝 **Serengeti** - Narzędzia współpracy (beta-readers, współautorzy)
- 💾 Automatyczne backupy w chmurze
- 🌐 Dostęp web do projektów
- 📱 Aplikacje towarzyszące mobilne

#### Fazy rozwoju

**Faza 1: MVP (Open Source Only) - 5-6 miesięcy**
- Budowa i wydanie funkcji core
- Licencja MIT na GitHub
- Focus na jakość i feedback społeczności
- Repo prywatne podczas developmentu, publiczne po release

**Faza 2: Rozbudowa Core + Premium Features - 6-9 miesięcy**
- Rozbudowa core (timeline, plot planner, advanced stats)
- Dodanie closed-source premium plugins
- System pluginów w core
- System licencjonowania

**Faza 3: Cloud Services - 9-12 miesięcy**
- Infrastruktura backendowa
- Serwis synchronizacji Victoria
- Model subskrypcyjny

**Faza 4: Ekosystem - 12-18 miesięcy**
- Marketplace pluginów
- Marketplace szablonów
- Treści społeczności + premium
- Partnerstwa z wydawnictwami

#### Model przychodów (Szacunkowy)

**Cel:**
- Darmowy tier: 80% użytkowników (open source core)
- Premium: 15% użytkowników (jednorazowy zakup ~$49-99 lub subskrypcja)
- Cloud: 5% użytkowników (subskrypcja ~$9.99/miesiąc)

**Filozofia:**
> "Darmowi użytkownicy otrzymują kompletne, funkcjonalne środowisko pisarskie.
> Użytkownicy Premium zyskują supermoce AI.
> Użytkownicy Cloud otrzymują bezproblemowe doświadczenie wielourządzeniowe."

#### Implementacja techniczna

**Struktura kodu:**
```
kalahari/
├── core/              # Open source (MIT, GitHub)
├── premium/           # Closed source (prywatne repo)
└── cloud_services/    # Usługi backendowe (prywatne)
```

**System pluginów:**
- Core udostępnia API pluginów
- Funkcje premium jako pluginy
- Weryfikacja licencji w core (open source)
- Mechanizm ładowania pluginów

#### Przewagi konkurencyjne

**vs Zamknięte źródło (Scrivener, etc.):**
- ✅ Zaufanie (kod widoczny)
- ✅ Prywatność (możliwy self-hosting)
- ✅ Elastyczność (możliwość modyfikacji core)
- ✅ Brak vendor lock-in

**vs Pełne Open Source:**
- ✅ Zrównoważony model przychodów
- ✅ Profesjonalny development
- ✅ Zaawansowane funkcje AI
- ✅ Infrastruktura cloud

### 8.2 Platforma ✅ USTALONE

**Strategia:** Cross-platform z priorytetem Windows

**Kolejność wsparcia:**
1. 🪟 **Windows** - PLATFORMA GŁÓWNA
   - Target: Windows 10/11 (64-bit)
   - Najwięcej testów tutaj
   - Pierwsze do otrzymania nowych funkcji
   - Największa publiczność pisarzy

2. 🐧 **Linux** - PLATFORMA WTÓRNA
   - Target: Ubuntu 22.04+, Linux Mint, Fedora 38+
   - Wsparcie społeczności open source
   - Wydanie po stabilizacji Windows

3. 🍎 **macOS** - PLATFORMA TRZECIORZĘDNA
   - Target: macOS 11+
   - Rynek profesjonalnych pisarzy
   - Wydanie po stabilizacji Linux

**Dystrybucja:**
- PyInstaller dla standalone executables
- Instalatory: NSIS (Windows), AppImage/Snap (Linux), DMG (macOS)
- wxPython zapewnia natywny wygląd na wszystkich platformach

**Strategia cross-platform:**
- wxPython oferuje natywny wygląd na wszystkich platformach
- Testowanie na Windows przez cały development
- Testowanie macOS/Linux przed odpowiednimi release'ami
- Funkcje specyficzne dla platformy tylko gdy absolutnie konieczne
- Cel: 95%+ współdzielenia kodu między platformami

### 8.3 Integracja AI ✅ USTALONE

**MVP (Faza 1):** Claude API (Anthropic)
**Faza 2+:** Lokalne modele (Ollama, Llama)

**Model API:**
- Użytkownik podaje własny klucz API (no cost for us)
- Bezpieczne przechowywanie kluczy (encrypted local storage)
- Wybór providera: Claude (Anthropic) lub OpenAI GPT
- 100k token context (Claude) idealny dla długich tekstów

**Lokalne modele (Faza 2):**
- Ollama integration dla privacy-conscious users
- Modele: Llama 3, Mistral, Qwen
- Wymaga GPU dla sensownej wydajności
- Wolniejsze ale darmowe i offline

**Privacy:**
- Core = NIE wysyła danych bez zgody
- AI features = opcjonalne, user kontroluje
- Jasne info co idzie do API
- Możliwość pracy offline (bez AI)

### 8.4 Synchronizacja w chmurze ⏳ PRZYSZŁOŚĆ

**Faza 3-4:** Victoria Cloud Service

**Strategia:**
- Własny backend (kontrola, bezpieczeństwo)
- End-to-end encryption
- Konflikt resolution
- Wersjonowanie

**Alternatywy rozważane:**
- Integracja Dropbox/Google Drive
- Git-based sync
- WebDAV support

### 8.5 Mobilna aplikacja ⏳ PRZYSZŁOŚĆ

**Faza 4+:** Sahara Mobile Companion

**Koncepcja:**
- Read-only access do projektów
- Podstawowe notatki
- Synchronizacja z desktop
- Distraction-free writing mode

---

## 9. Nazwa i branding

### Dlaczego "Kalahari"?
- Afrykańska pustynia - przestrzeń twórcza, bezgraniczne możliwości
- Egzotyczna, zapadająca w pamięć
- Krótka, łatwa do wymówienia
- Tworzy bazę dla całego ekosystemu narzędzi (patrz niżej)
- Dostępna domena?

### Ekosystem narzędzi - konwencja nazewnicza

Nazwa "Kalahari" otwiera możliwość stworzenia całego **African Wildlife Writing Suite** - rodziny narzędzi pisarskich inspirowanych afrykańską przyrodą:

#### **Kalahari** - Główne środowisko pisarskie
- Rdzeń ekosystemu
- Pełne IDE dla pisarzy
- Pustynia Kalahari = przestrzeń twórcza, gdzie wszystko się zaczyna

#### **Serengeti** - Narzędzie do collaborative writing
- Współpraca nad projektami
- Multi-author support
- Real-time editing
- **Dlaczego Serengeti**: Wielka migracja zwierząt = współpraca wielu autorów w ruchu

#### **Okavango** - Research & knowledge management
- Zaawansowane narzędzie do zbierania i organizacji źródeł
- Web scraping, PDF management, note-taking
- Knowledge graph
- **Dlaczego Okavango**: Delta rzeki = zbieranie informacji z wielu źródeł w jedno miejsce

#### **Kilimanjaro** - Project management i goal tracking
- Standalone aplikacja do zarządzania wieloma projektami książkowymi
- Zaawansowane cele, deadliny, tracking
- Portfolio pisarza
- **Dlaczego Kilimanjaro**: Najwyższa góra Afryki = osiąganie szczytu, wielkie cele

#### **Victoria** - Cloud sync i backup
- Chmurowa synchronizacja projektów
- Automatyczne backupy
- Versioning i historia zmian
- **Dlaczego Victoria**: Wielkie jezioro = storage, rezerwuar danych

#### **Zambezi** - Publishing & export toolkit
- Zaawansowane narzędzie do eksportu i przygotowania do publikacji
- Konwersje formatów (EPUB, MOBI, PDF, itp.)
- Integracja z self-publishing platforms (Amazon KDP, Draft2Digital)
- **Dlaczego Zambezi**: Wielka rzeka = przepływ książki do czytelników, dystrybucja

#### **Sahara** - Minimalist writing app (mobile)
- Aplikacja mobilna
- Distraction-free writing
- Sync z Kalahari
- **Dlaczego Sahara**: Największa pustynia = minimalizm, pustka, fokus

#### **Ngorongoro** - Analytics & insights
- Analityka dla pisarzy
- Statystyki sprzedaży, engagement, metryki
- AI-powered insights
- **Dlaczego Ngorongoro**: Krater pełen życia = głęboka analiza ekosystemu książki

### Afrykańskie zwierzęta jako ikony i maskotki

Każde narzędzie może mieć przypisane zwierzę:
- **Kalahari**: Surykatka (czujny strażnik) lub Lew (ostoja mądrości i wiedzy, sugeruje snucie opowieści - patrz propozycje logo aplikacji)
- **Serengeti**: Gnu lub zebra (migracja, współpraca)
- **Okavango**: Hipopotam (zbiera wodę/informacje)
- **Kilimanjaro**: Orzeł (widok z góry, cele)
- **Victoria**: Flaming (różowy kolor, piękno i ochrona)
- **Zambezi**: Krokodyl (silny, skuteczny przepływ)
- **Sahara**: Feniek (mały lis, minimalizm)
- **Ngorongoro**: Nosorożec (siła analityczna)

### Logo i visual identity ✅ USTALONE

#### System logo (wielowarstwowy)

Kalahari używa **dwóch wersji logo** w zależności od kontekstu:

##### 1. **App Icon / Logo aplikacji** (Lew solo) ✅ MVP
**Zastosowanie:**
- Ikona aplikacji (Windows/macOS/Linux)
- Ikona pliku .klh (Kalahari project)
- About dialog
- Ikona okna aplikacji

**Design:**
- Głowa majestatycznego lwa (realistyczny styl)
- Z otwartą książką i piórem
- Kolory: ciepłe złoto, brąz, pomarańcz
- Rozmiary: 16x16, 32x32, 64x64, 128x128, 256x256, 512x512px
- Format: PNG z przezroczystością + ICO/ICNS

**Symbolika:**
- Lew = mądrość, autorytet, storytelling
- Książka = pisanie, literatura
- Pióro = twórczość, craft

**Projekt referencyjny:** `/init_concept/propozycja logo programu 2.jpg`

##### 2. **Multi-Animal Logo** (8 zwierząt) ✅ MARKETING
**Zastosowanie:**
- Marketing materials (plakaty, banery, social media)
- Website header
- Splash screen (wybór losowy)
- Promotional videos
- Conference booths

**Design:**
- Wszystkie 8 zwierząt-asystentów w kompozycji
- Okrągły układ (circular composition) wokół centralnego motywu
- Każde zwierzę reprezentuje swoją osobowość gestem/pozycją
- Styl: realistyczny (jak Lew w app icon)

**Zwierzęta w kompozycji (przykładowy layout):**
```
         [Giraffe]
    [Lion]     [Elephant]
[Cheetah]  📖  [Meerkat]
  [Buffalo]   [Parrot]
      [Chameleon]
```

**Warianty multi-animal logo:**
1. **Full version** - wszystkie 8 zwierząt + książka w centrum
2. **MVP version** - tylko 4 zwierzęta MVP (Lew, Surykatka, Słoń, Gepard)
3. **Horizontal banner** - zwierzęta w linii z "KALAHARI" tekstem

##### 3. **Splashscreen** (Dynamiczny) ✅ MVP
**Koncepcja:**
- **Tło**: Statyczny (sawanna/pustynny krajobraz)
- **Zwierzę**: Losowo wybierane przy każdym uruchomieniu
- **Kompozycja**: Zwierzę + logo text + loading bar + user info

**Elementy splashscreen:**
```
┌───────────────────────────────────────┐
│                                       │
│         [Random Animal Head]          │
│              200x200px                │
│                                       │
│            K A L A H A R I            │
│       The best book writers IDE       │
│                                       │
│         Version 1.0.0 (build)         │
│      Registered for: User Name        │
│                                       │
│    LOADING, please wait...            │
│    ████████████░░░░░░░░░░░  45%      │
│                                       │
└───────────────────────────────────────┘
```

**Implementacja:**
- 2 warstwy graficzne: tło (stałe) + głowa zwierzęcia (zmienna)
- Random selection z dostępnych zwierząt (4 w MVP, 8 w Fazie 2)
- Animacja fade-in (500ms)
- Wyświetlanie przez minimum 2 sekundy (user experience)

**Projekt referencyjny:** `/init_concept/Splashscreen_koncepcja1.jpg`

#### Wcześniejsze koncepcje (odrzucone)
- ~~**Koncepcja 1**: Stylizowana wydma z wschodzącym słońcem~~
- ~~**Koncepcja 2**: Sylwetka afrykańskiego drzewa (baobab/akacja)~~
- ~~**Koncepcja 3**: Surykatka na czatach (maskotka)~~
- ~~**Koncepcja 4**: Geometryczna kompozycja z elementami afrykańskimi~~

*Niektóre motywy mogą być wykorzystane jako akcenty graficzne w UI (ikony, ozdobniki ramek)*

#### Paleta kolorów (wyłącznie do rozważań; opcjonalnie)
- **Podstawowe**:
  - Piaskowy beż (#E6D5B8)
  - Ciepły pomarańczowy (#D97642)
  - Głęboka czerwień zachodu słońca (#8B3A3A)
- **Akcenty**:
  - Zieleń sawanny (#6B8E23)
  - Błękit nieba (#87CEEB)
  - Złoty (#DAA520)
- **Neutralne**:
  - Ciemny brąz (#3E2723)
  - Kremowy (#FFF8DC)

#### Typografia
- **Nagłówki**: Serif z afrykańskim charakterem (elegancja, tradycja)
- **Tekst**: Sans-serif dla czytelności
- **Akcenty**: Opcjonalnie font inspirowany pismem afrykańskim

#### Styl graficzny ✅ USTALONE

**Wybrany styl: Realistic** (Fotorealistyczny)

**Charakterystyka:**
- Wysokiej jakości, fotorealistyczne renderingi zwierząt
- Profesjonalny, poważny, dostojny wygląd
- Naturalne światło i cienie
- Detale tekstur (futro, skóra, piórka)
- Ciepła paleta kolorów (złoto, brąz, pomarańcz)

**Zastosowanie:**
- App icon (Lew)
- Multi-animal marketing logo
- Asystent w aplikacji (6-8 nastrojów per zwierzę)
- Splashscreen (losowe zwierzę)

**Odrzucone style:**
- ~~**Minimalist geometric**: Geometric/polygonal animals (zbyt nowoczesne, zimne)~~
- ~~**Hand-drawn**: Rysunkowe zwierzęta (zbyt casualowe, mniej profesjonalne)~~

**Akcenty graficzne** (opcjonalnie):
- Elementy wzorów plemiennych jako ozdobniki UI
- Stilizowane ikony (flat design) dla menu/toolbar
- Mix: realistyczne zwierzęta + minimalistyczne UI elements

### Branding guidelines

#### Ton komunikacji
- **Ciepły i wspierający** (jak asystent)
- **Inspirujący** (motywacja do pisania)
- **Profesjonalny, ale przystępny**
- **Z nutką humoru** (gadające zwierzęta!)

#### Hasła reklamowe (przykłady)
- *"Where stories roam free"* (Gdzie historie wędrują swobodnie)
- *"Your creative safari begins here"* (Twoje twórcze safari zaczyna się tu)
- *"Write wild, write free"* (Pisz dziko, pisz swobodnie)
- *"The savannah of storytelling"* (Sawanna opowieści)

#### Partnership z ochroną przyrody (DOSKONAŁY POMYSŁ!!!)
- **1% zysków** dla organizacji chroniących afrykańską przyrodę
- Logo partnera (WWF, African Wildlife Foundation)
- Storytelling o ochronie dzikich zwierząt w marketingu
- "Adopt an animal" program dla użytkowników

### Społeczność

#### Nazwa społeczności
- **"The Pride"** (stado lwów - społeczność pisarzy Kalahari)
- **"The Herd"** (stado - wspólnota)
- **"Kalahari Writers"**

#### Eventy
- **"Writing Safari"** - wyzwania pisarskie (NaNoWriMo style)
- **"Watering Hole"** - meetupy online (gdzie pisarze się spotykają)
- **"Migration"** - coroczna konferencja użytkowników

### Konkurencja i pozycjonowanie

Kalahari wyróżnia się:
- **Unikalny branding** - żadne inne narzędzie nie ma tak spójnej tematyki
- **Emotionalny connection** - zwierzęta = sympat ia użytkowników
- **Ekosystem** - możliwość rozbudowy o kolejne narzędzia
- **Storytelling potential** - łatwy marketing (opowieści o Afryce)
- **Wartości**: natura, wolność, kreatywność, wspólnota

---

## 10. Następne kroki

1. **Walidacja pomysłu**
   - Ankiety wśród pisarzy
   - Beta testerzy zainteresowani?

2. **Prototyp**
   - Podstawowy edytor + projekt
   - Test interfejsu

3. **MVP Development**
   - 3-6 miesięcy pracy
   - Pierwsze testy z użytkownikami

4. **Iteracja**
   - Feedback, poprawki
   - Dodawanie funkcji

5. **Launch**
   - Marketing (social media, fora pisarskie)
   - Landing page
   - Dokumentacja

---

## 11. Kontakt i współpraca

Jeśli jesteś zainteresowany współpracą przy projekcie Kalahari:
- Programiści Python/wxPython
- UX/UI designers
- Pisarze (beta testerzy)
- Tłumacze (internacjonalizacja)

---

**Wersja dokumentu:** 2.2
**Data:** 2025-10-22
**Status:** Koncepcja w fazie planowania
**Ostatnia aktualizacja:** Sprecyzowanie koncepcji asystenta - jeden asystent do wyboru, różne style komunikacji; rozszerzenie o charakterystykę asystentów-zwierząt i ekosystem narzędzi afrykańskich
