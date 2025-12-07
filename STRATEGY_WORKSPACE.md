# Kalahari - Strategiczny Dokument Roboczy

**Data utworzenia:** 2025-11-27
**Ostatnia aktualizacja:** 2025-11-28
**Cel:** Strategiczne planowanie automatyzacji workflow Claude Code
**Status:** WDROŻONE - Implementacja ukończona, pozostały testy agentów

---

## 1. Analiza Wersji Claude Code

### 1.1 Nasza wersja vs Artykuł

| Parametr | Wartość |
|----------|---------|
| **Nasza wersja** | 2.0.55 |
| **Wersja z artykułu** | 2.0.44 (18 Nov 2025) |
| **Różnica** | +11 wersji |

### 1.2 Kluczowe funkcje w wersjach 2.0.41-2.0.55

| Wersja | Funkcja | Opis |
|--------|---------|------|
| 2.0.55 | Rozszerzenie `~` w `/add-dir` | Drobna poprawka |
| 2.0.54 | **PermissionRequest hooks** | Custom logika "always allow" |
| 2.0.51 | **Opus 4.5** | Najnowszy model (używamy) |
| 2.0.49 | **Poprawka uprawnień subagentów** | Krytyczne dla naszego workflow |
| 2.0.45 | **PermissionRequest hook** | Custom approval/denial logic |
| 2.0.43 | **permissionMode** | manual/acceptEdits/bypassPermissions |
| 2.0.43 | **skills frontmatter** | Auto-ładowanie skills w subagentach |
| 2.0.43 | **SubagentStart hook** | Inject context do każdego agenta |
| 2.0.42 | **SubagentStop + transcript** | Audit trail + chaining |
| 2.0.41 | **Prompt-based hooks z `model`** | Haiku/Sonnet dla walidacji |

---

## 2. Funkcje do Wdrożenia

### 2.1 Legenda

- 🔴 **WDROŻYĆ** = Krytyczne dla workflow
- 🟡 **ROZWAŻYĆ** = Przydatne, do decyzji
- ✅ **OK** = Już używamy

### 2.2 Lista Funkcji

| Funkcja | Status | Priorytet | Uzasadnienie |
|---------|--------|-----------|--------------|
| `SubagentStart` hook | ✅ | ✅ **WDROŻONE** | Inject kontekstu projektu do KAŻDEGO agenta |
| `SubagentStop` hook | ✅ | ✅ **WDROŻONE** | Audit trail (logowanie do .claude/logs/agents.log) |
| `permissionMode` | ✅ | ✅ **WDROŻONE** | architect=manual, code-writer/editor/ui=bypassPermissions |
| `skills` frontmatter | ✅ | ✅ **WDROŻONE** | Auto-ładowanie wiedzy do agentów (8 skills) |
| Prompt-based hooks | ✅ | ✅ **WDROŻONE** | PreToolUse: walidacja przed git commit (haiku) |
| `SessionStart` hook | ✅ | ✅ **WDROŻONE** | Auto-load session-state.json przy starcie sesji |
| `SessionEnd` hook | ✅ | ✅ **WDROŻONE** | Przypomnienie o /save-session przy wyjściu |
| `PermissionRequest` hook | ❌ | 🟡 **ROZWAŻYĆ** | Custom approval - może wystarczy permissionMode? |
| Opus 4.5 | ✅ | ✅ **OK** | Już używamy |

---

## 3. Agenci - Szczegółowa Specyfikacja (7 agentów)

### 3.1 TASK-MANAGER

**Rola:** Manager Projektu - pilnuje procesu, NIE analizuje kodu

**permissionMode:** `manual`

**Skills:** `openspec-workflow`, `roadmap-analysis`, `session-protocol`

**Tools:** Read, Write, Glob, Grep (NIE Serena - nie analizuje kodu!)

**Triggery:** "nowe zadanie", "new task", "co dalej", "status taska", "zamknij task"

**Tryby pracy (3):**

```
TRYB 1: TWORZENIE TASKA
────────────────────────
Trigger: "nowe zadanie", "chcę zrobić X"

1. Zapytaj czy user ma pomysł:
   - Jeśli TAK → krok 3
   - Jeśli NIE → krok 2
2. Przeczytaj ROADMAP.md:
   - Znajdź 3 niezrobione pozycje [ ]
   - Zaproponuj je userowi
   - Czekaj na wybór
3. Zbierz wymagania:
   - CEL: Co chcemy osiągnąć?
   - ZAKRES: Co wchodzi/nie wchodzi?
   - KRYTERIA: Jak poznamy że zrobione?
   - Pytaj aż user powie "OK" lub "wystarczy"
4. Znajdź ostatni numer OpenSpec:
   - `ls openspec/changes/ | sort -r | head -1`
   - Nowy numer = ostatni + 1
5. Utwórz folder:
   - `openspec/changes/NNNNN-nazwa/`
6. Wygeneruj proposal.md:
   - Użyj szablonu z skill openspec-workflow
7. Wygeneruj tasks.md:
   - Lista checkboxów z podzadaniami
8. Pokaż podsumowanie:
   - "Utworzono OpenSpec #NNNNN"
   - "Następny krok: architect przeanalizuje"

TRYB 2: ŚLEDZENIE POSTĘPU
─────────────────────────
Trigger: "status", "jak idzie", "gdzie jesteśmy"

1. Znajdź aktywny OpenSpec:
   - Status = IN_PROGRESS
2. Sprawdź tasks.md:
   - Ile checkboxów [x] vs [ ]
3. Raportuj status:
   - "OpenSpec #NNNNN: 4/7 zadań done"
   - "Następny krok: [opis]"

TRYB 3: ZAMYKANIE TASKA
───────────────────────
Trigger: "zamknij task", "task gotowy", przed commitem

1. Zweryfikuj completeness:
   - [ ] Wszystkie checkboxy w tasks.md = [x]?
   - [ ] Code review passed?
   - [ ] Testy passed?
2. Zweryfikuj dokumentację:
   - [ ] CHANGELOG.md ma wpis w [Unreleased]?
   - [ ] ROADMAP.md ma checkbox [x] (jeśli feature)?
3. Jeśli braki → raportuj co brakuje
4. Jeśli OK:
   - Zmień status OpenSpec → DEPLOYED
   - Zaproponuj commit message
   - "Task #NNNNN gotowy do zamknięcia"
```

**NIE ROBI:**
- ❌ Analizy kodu (to architect)
- ❌ Projektowania rozwiązań (to architect)
- ❌ Pisania/edycji kodu (to code-writer/editor)
- ❌ Uruchamiania testów (to tester)
- ❌ Code review (to code-reviewer)

---

### 3.2 ARCHITECT

**Rola:** Analityk + Projektant - analizuje kod, projektuje rozwiązania

**permissionMode:** `manual`

**Skills:** `kalahari-coding`, `architecture-patterns`

**Tools:** Read, Glob, Grep, Serena (find_symbol, get_symbols_overview, find_referencing_symbols)

**Triggery:** "zaprojektuj", "przeanalizuj", "jak to zrobić", "gdzie to dodać"

**Flow pracy:**

```
ANALIZA I PROJEKTOWANIE
───────────────────────
Trigger: Po utworzeniu OpenSpec przez task-manager

1. Przeczytaj OpenSpec:
   - proposal.md → zrozum CEL
   - tasks.md → zrozum ZAKRES
2. Przeanalizuj istniejący kod:
   - Serena: get_symbols_overview dla relevantnych plików
   - Serena: find_symbol dla kluczowych klas
   - Serena: find_referencing_symbols dla powiązań
3. Zidentyfikuj wzorce do użycia:
   - ArtProvider → czy potrzebne ikony?
   - SettingsManager → czy potrzebna konfiguracja?
   - Theme/QPalette → czy potrzebne kolory?
   - tr() → czy są stringi UI?
4. Zaprojektuj rozwiązanie:
   - Które ISTNIEJĄCE pliki modyfikować?
   - Jakie NOWE pliki utworzyć?
   - Jaka struktura klas?
   - Jakie zależności między klasami?
5. Uzupełnij OpenSpec:
   - Dodaj sekcję "## Design" do proposal.md
   - Lista plików do modyfikacji
   - Lista nowych plików
   - Diagram klas (jeśli potrzebny)
6. Zaraportuj:
   - "Design gotowy dla #NNNNN"
   - "Pliki do zmiany: X, nowe: Y"
   - "Następny krok: implementacja"
```

**NIE ROBI:**
- ❌ Zbierania wymagań (to task-manager)
- ❌ Pisania kodu produkcyjnego (to code-writer/editor/ui-designer)
- ❌ Code review (to code-reviewer)
- ❌ Uruchamiania testów (to tester)

---

### 3.3 CODE-WRITER

**Rola:** Pisanie NOWEGO kodu - nowe klasy, nowe funkcje, nowe pliki

**permissionMode:** `bypassPermissions`

**Skills:** `kalahari-coding`

**Tools:** Read, Write, Edit, Bash (build), Glob, Grep

**Triggery:** "napisz", "utwórz klasę", "dodaj nową funkcję", "nowy plik"

**Flow pracy:**

```
PISANIE NOWEGO KODU
───────────────────
Trigger: Design gotowy od architect

1. Przeczytaj design z OpenSpec:
   - Które nowe pliki utworzyć?
   - Jaka struktura klas?
2. Dla każdego nowego pliku:
   a. Utwórz plik .h:
      - #pragma once
      - namespace kalahari::xxx
      - Doxygen komentarze
   b. Utwórz plik .cpp:
      - #include odpowiednie
      - Implementacja metod
3. Zastosuj wzorce (z skill kalahari-coding):
   - ArtProvider::instance().getIcon("name") dla ikon
   - SettingsManager::instance().getValue() dla config
   - tr("text") dla stringów UI
   - QVBoxLayout/QHBoxLayout dla layoutów
4. Uruchom build:
   - `scripts/build_windows.bat Debug`
   - Jeśli błędy → napraw
5. Zaktualizuj tasks.md:
   - Oznacz [x] ukończone podzadania
6. Zaraportuj:
   - "Utworzono X nowych plików"
   - "Build: PASS/FAIL"
```

**NIE ROBI:**
- ❌ Modyfikacji istniejącego kodu (to code-editor)
- ❌ Projektowania (to architect)
- ❌ Zbierania wymagań (to task-manager)

---

### 3.4 CODE-EDITOR

**Rola:** Modyfikacja ISTNIEJĄCEGO kodu - zmiany, refaktoring, bugfixy

**permissionMode:** `bypassPermissions`

**Skills:** `kalahari-coding`

**Tools:** Read, Write, Edit, Bash (build), Glob, Grep, Serena (do znalezienia co zmienić)

**Triggery:** "zmień", "popraw", "napraw", "refaktoruj", "dodaj do istniejącej klasy"

**Flow pracy:**

```
MODYFIKACJA ISTNIEJĄCEGO KODU
─────────────────────────────
Trigger: Design gotowy od architect (modyfikacje)

1. Przeczytaj design z OpenSpec:
   - Które pliki modyfikować?
   - Jakie zmiany?
2. Dla każdego pliku do modyfikacji:
   a. Przeczytaj obecny kod (Serena lub Read)
   b. Zidentyfikuj miejsce zmiany
   c. Wykonaj Edit (nie Write całego pliku!)
3. Zastosuj wzorce (z skill kalahari-coding):
   - Zachowaj istniejący styl
   - Dodaj ArtProvider jeśli nowe ikony
   - Dodaj tr() jeśli nowe stringi
4. Uruchom build:
   - `scripts/build_windows.bat Debug`
   - Jeśli błędy → napraw
5. Zaktualizuj tasks.md
6. Zaraportuj:
   - "Zmodyfikowano X plików"
   - "Build: PASS/FAIL"
```

**NIE ROBI:**
- ❌ Tworzenia nowych klas od zera (to code-writer)
- ❌ Projektowania (to architect)

---

### 3.5 UI-DESIGNER

**Rola:** UI/UX - dialogi, panele, toolbary, layouty Qt

**permissionMode:** `bypassPermissions`

**Skills:** `kalahari-coding`, `qt6-desktop-ux`

**Tools:** Read, Write, Edit, Bash (build), Glob, Grep

**Triggery:** "dialog", "panel", "toolbar", "UI", "widget", "layout"

**Flow pracy:**

```
TWORZENIE/MODYFIKACJA UI
────────────────────────
Trigger: Design gotowy od architect (komponenty UI)

1. Przeczytaj design z OpenSpec:
   - Jaki typ UI? (dialog/panel/toolbar)
   - Jakie kontrolki?
   - Jaki layout?
2. Zastosuj wzorce Qt6 (z skill qt6-desktop-ux):
   - QDialog dla okien modalnych
   - QDockWidget dla paneli dokowalnych
   - QGroupBox dla grupowania
   - QVBoxLayout/QHBoxLayout dla layoutów
   - QSizePolicy dla responsywności
3. Zastosuj wzorce projektu (z skill kalahari-coding):
   - ArtProvider dla ikon
   - Theme/QPalette dla kolorów
   - tr() dla tekstów
   - SettingsManager dla persystencji
4. Dbaj o UX:
   - Spójne spacing (6px między, 11px margines)
   - Dostępność (tooltips, tab order)
   - Responsywność (stretch factors)
5. Uruchom build i test wizualny
6. Zaktualizuj tasks.md
```

**NIE ROBI:**
- ❌ Logiki biznesowej (to code-writer/editor)
- ❌ Projektowania architektury (to architect)

---

### 3.6 CODE-REVIEWER

**Rola:** Code review - jakość kodu, standardy, dokumentacja

**permissionMode:** `manual`

**Skills:** `kalahari-coding`, `quality-checklist`

**Tools:** Read, Grep, Glob (TYLKO do czytania - nie edytuje!)

**Triggery:** "review", "sprawdź kod", "przed commitem", "czy mogę commitować"

**Flow pracy:**

```
CODE REVIEW
───────────
Trigger: Po implementacji, przed testerem

1. Pobierz listę zmienionych plików:
   - `git diff --name-only HEAD~1` lub z OpenSpec
2. Dla każdego pliku sprawdź (z skill quality-checklist):

   WZORCE PROJEKTU:
   - [ ] Ikony przez ArtProvider (nie hardcoded paths)?
   - [ ] Stringi UI przez tr() (nie hardcoded)?
   - [ ] Config przez SettingsManager (nie hardcoded)?
   - [ ] Kolory przez Theme/QPalette (nie hardcoded)?

   JAKOŚĆ KODU:
   - [ ] Brak TODO/FIXME w nowym kodzie?
   - [ ] Brak zakomentowanego kodu?
   - [ ] Nazwy zgodne z konwencją (m_member, camelCase)?
   - [ ] Doxygen dla publicznych metod?

   DOKUMENTACJA:
   - [ ] CHANGELOG.md ma wpis w [Unreleased]?
   - [ ] ROADMAP.md zaktualizowany (jeśli nowa funkcja)?
   - [ ] OpenSpec tasks.md aktualny?

3. Wydaj decyzję:
   - Jeśli wszystko OK:
     `{"decision": "approve", "summary": "Code review passed"}`
   - Jeśli problemy:
     `{"decision": "block", "issues": ["lista problemów"]}`
4. Zaraportuj:
   - "Review PASSED" lub "Review BLOCKED: [powody]"
```

**NIE ROBI:**
- ❌ Naprawiania kodu (to code-editor)
- ❌ Uruchamiania testów (to tester)
- ❌ Commitowania (to task-manager)

---

### 3.7 TESTER

**Rola:** QA - uruchamianie testów, walidacja działania

**permissionMode:** `manual`

**Skills:** `kalahari-coding`, `testing-procedures`

**Tools:** Bash (build, testy), Read, Grep

**Triggery:** "przetestuj", "uruchom testy", "QA", "czy działa"

**Flow pracy:**

```
TESTOWANIE
──────────
Trigger: Po code review (PASSED)

1. Uruchom build:
   - `scripts/build_windows.bat Debug`
   - Jeśli FAIL → raportuj błędy
2. Uruchom testy jednostkowe:
   - `./build-windows/bin/kalahari-tests.exe`
   - Lub odpowiednik dla platformy
3. Analizuj wyniki:
   - Ile testów PASSED/FAILED?
   - Które testy FAILED?
   - Czy nowe testy dodane (jeśli nowa funkcja)?
4. (Opcjonalnie) Test manualny:
   - Uruchom aplikację
   - Sprawdź wizualnie nową funkcję
5. Wydaj decyzję:
   - Jeśli wszystko OK:
     `{"decision": "pass", "tests": "42/42 passed"}`
   - Jeśli problemy:
     `{"decision": "fail", "failures": ["lista"]}`
6. Zaraportuj:
   - "Testy PASSED: 42/42"
   - lub "Testy FAILED: 2 failures"
```

**NIE ROBI:**
- ❌ Naprawiania kodu (to code-editor)
- ❌ Code review (to code-reviewer)
- ❌ Pisania testów (to code-writer przy TDD)

---

## 4. Skills - Szczegółowa Specyfikacja (8 skills)

### 4.1 Macierz: Agent → Skills

| Agent | kalahari-coding | qt6-desktop-ux | openspec-workflow | roadmap-analysis | architecture-patterns | quality-checklist | testing-procedures | session-protocol |
|-------|-----------------|----------------|-------------------|------------------|----------------------|-------------------|-------------------|------------------|
| task-manager | | | ✓ | ✓ | | | | ✓ |
| architect | ✓ | | | | ✓ | | | |
| code-writer | ✓ | | | | | | | |
| code-editor | ✓ | | | | | | | |
| ui-designer | ✓ | ✓ | | | | | | |
| code-reviewer | ✓ | | | | | ✓ | | |
| tester | ✓ | | | | | | ✓ | |
| commands | | | | | | | | ✓ |

### 4.2 Zawartość Każdego Skill

#### SKILL: kalahari-coding
```
Używany przez: architect, code-writer, code-editor, ui-designer, code-reviewer, tester

ZAWARTOŚĆ:
1. Ikony
   - ZAWSZE: core::ArtProvider::getInstance().getIcon("cmd_id")
   - ZAWSZE: core::ArtProvider::getInstance().createAction("cmd_id", parent)  // dla QAction
   - NIGDY: QIcon("path/to/icon.svg")
   - Lista dostępnych ikon: resources/icons/

2. Konfiguracja
   - ZAWSZE: core::SettingsManager::getInstance().getValue("key", "default")
   - ZAWSZE: core::SettingsManager::getInstance().setValue("key", "value")
   - NIGDY: hardcoded wartości

3. Teksty UI
   - ZAWSZE: tr("User visible text")
   - NIGDY: "Hardcoded string"

4. Kolory ikon
   - ZAWSZE: core::ArtProvider::getInstance().getPrimaryColor()
   - ZAWSZE: core::ArtProvider::getInstance().getSecondaryColor()
   - ZAWSZE: core::ArtProvider::getInstance().setPrimaryColor(QColor("#hex"))
   - NIGDY: QColor(255, 0, 0) hardcoded

5. Motywy (dla dostępu do palety/kolorów tematu)
   - ZAWSZE: core::ThemeManager::getInstance().getCurrentTheme()
   - Dostęp: theme.colors.primary, theme.colors.secondary, theme.palette

6. Layouty Qt6
   - Podstawowe: QVBoxLayout, QHBoxLayout
   - Grupowanie: QGroupBox
   - Stretch factors: 0=fixed, 1+=flex

7. Build
   - Windows: scripts/build_windows.bat Debug
   - Linux: scripts/build_linux.sh
   - NIGDY: cmake bezpośrednio

8. Nazewnictwo
   - Pliki: snake_case.cpp
   - Klasy: PascalCase
   - Metody: camelCase
   - Membery: m_camelCase
   - Stałe: UPPER_SNAKE_CASE

9. Logowanie
   - ZAWSZE: core::Logger::getInstance().info("msg: {}", var)
   - Poziomy: trace, debug, info, warn, error, critical
```

#### SKILL: qt6-desktop-ux
```
Używany przez: ui-designer

ZAWARTOŚĆ:
1. QDockWidget
   - Kiedy: panele dokowane (Navigator, Properties, Log)
   - Jak: setAllowedAreas(), setFeatures()

2. QGroupBox
   - Kiedy: grupowanie powiązanych kontrolek
   - Jak: z tytułem, wewnętrzny layout

3. Spacing i Margins
   - Między kontrolkami: 6px
   - Margines grupowy: 11px
   - setContentsMargins(11, 11, 11, 11)

4. QSizePolicy
   - Fixed: stały rozmiar
   - Preferred: preferowany ale elastyczny
   - Expanding: wypełnia dostępną przestrzeń
   - Minimum: minimalny rozmiar

5. Accessibility
   - setToolTip() dla każdej kontrolki
   - setWhatsThis() dla złożonych
   - Tab order logiczny
```

#### SKILL: openspec-workflow
```
Używany przez: task-manager

ZAWARTOŚĆ:
1. Struktura folderów
   openspec/
   └── changes/
       └── NNNNN-nazwa/
           ├── proposal.md
           └── tasks.md

2. Format proposal.md
   # NNNNN: Nazwa Zmiany

   ## Status
   PENDING | IN_PROGRESS | DEPLOYED

   ## Cel
   Co chcemy osiągnąć?

   ## Zakres
   Co wchodzi / nie wchodzi?

   ## Kryteria Akceptacji
   - [ ] Kryterium 1
   - [ ] Kryterium 2

   ## Design (uzupełnia architect)
   ...

3. Format tasks.md
   # Tasks dla #NNNNN

   - [ ] Podzadanie 1
   - [ ] Podzadanie 2
   - [ ] ...

4. Lifecycle
   PENDING → IN_PROGRESS → DEPLOYED

5. Numeracja
   - Znajdź ostatni: ls openspec/changes/ | sort -r | head -1
   - Nowy = ostatni + 1
   - Format: 5 cyfr z zerami (00001, 00027)
```

#### SKILL: roadmap-analysis
```
Używany przez: task-manager

ZAWARTOŚĆ:
1. Format ROADMAP.md
   - Checkboxy: [ ] niezrobione, [x] zrobione
   - BEZ numerów tasków!
   - Tylko nazwy funkcji/pomysłów

2. Jak czytać
   - Szukaj [ ] (niezrobione)
   - Sprawdź sekcję (Phase 0, Phase 1, etc.)
   - Priorytet: od góry do dołu w sekcji

3. Jak proponować
   - Wybierz 3 pozycje [ ] z aktualnej fazy
   - Przedstaw userowi z krótkim opisem
   - Czekaj na wybór

4. Jak aktualizować
   - Po zakończeniu funkcji: [ ] → [x]
   - NIE dodawaj numerów tasków!
```

#### SKILL: architecture-patterns
```
Używany przez: architect

ZAWARTOŚĆ:
1. Kluczowe klasy projektu
   - MainWindow: główne okno, zarządza panelami
   - SettingsManager: singleton, persystencja konfiguracji
   - ArtProvider: singleton, dostęp do ikon
   - Theme: singleton, kolory i style
   - CommandRegistry: rejestr akcji/komend

2. Wzorce używane
   - Singleton: SettingsManager, ArtProvider, Theme
   - Command: akcje w CommandRegistry
   - Observer: sygnały Qt (signals/slots)
   - Composite: Book → Part → Document

3. Struktura katalogów źródłowych
   include/kalahari/
   ├── core/       # logika biznesowa
   ├── gui/        # komponenty UI
   └── utils/      # pomocnicze

   src/
   ├── core/
   ├── gui/
   └── utils/

4. Jak dodawać nowe komponenty
   - Panel: dziedzicz z QDockWidget
   - Dialog: dziedzicz z QDialog
   - Widget: dziedzicz z QWidget
   - Zarejestruj w MainWindow
```

#### SKILL: quality-checklist
```
Używany przez: code-reviewer

ZAWARTOŚĆ:
CHECKLISTA CODE REVIEW:

## Wzorce Projektu
- [ ] Ikony przez ArtProvider?
- [ ] Stringi przez tr()?
- [ ] Config przez SettingsManager?
- [ ] Kolory przez Theme/QPalette?

## Jakość Kodu
- [ ] Brak TODO/FIXME?
- [ ] Brak zakomentowanego kodu?
- [ ] Nazwy zgodne z konwencją?
- [ ] Doxygen dla public metod?

## Dokumentacja
- [ ] CHANGELOG [Unreleased]?
- [ ] ROADMAP [x] jeśli feature?
- [ ] OpenSpec tasks.md aktualny?

## Build
- [ ] Build PASS?
- [ ] Brak nowych warnings?
```

#### SKILL: testing-procedures
```
Używany przez: tester

ZAWARTOŚĆ:
1. Uruchamianie testów
   Windows:
   - Build: scripts/build_windows.bat Debug
   - Testy: ./build-windows/bin/kalahari-tests.exe

   Linux:
   - Build: scripts/build_linux.sh
   - Testy: ./build-linux/bin/kalahari-tests

2. Interpretacja wyników
   - PASSED: test przeszedł
   - FAILED: test nie przeszedł
   - Format: [PASS/FAIL] TestName

3. Co sprawdzać
   - Wszystkie testy PASSED?
   - Czy dodano nowe testy (dla nowej funkcji)?
   - Czy nie ma regresji (wcześniej działające)?

4. Raportowanie
   - "42/42 tests PASSED"
   - "FAILED: TestXxx - expected Y, got Z"
```

#### SKILL: session-protocol
```
Używany przez: task-manager, /save-session, /load-session

ZAWARTOŚĆ:
1. Session State Location
   - .claude/session-state.json
   - NIE Serena memories! (Serena = tylko nawigacja po kodzie)
   - NIE commitować tego pliku

2. Session State Format
   {
     "timestamp": "2025-11-27T15:30:00",
     "mode": "quick|sync|full",
     "openspec": "00027",
     "openspec_status": "IN_PROGRESS",
     "working_on": "Brief description",
     "git_branch": "main",
     "git_commit": "abc1234",
     "uncommitted_changes": true,
     "next_steps": ["Step 1", "Step 2"]
   }

3. Save Modes
   quick: lokalny commit, ~15s (checkpoints, WIP)
   sync: push + CI/CD, ~30s (end of day, subtask)
   full: weryfikacja + docs, ~4min (task/phase complete)

4. Integration with OpenSpec
   - "status taska" → read session-state.json FIRST
   - Cross-reference with active OpenSpec
   - "zamknij task" → suggest /save-session --full

5. Key Rules
   - Session state ≠ Serena memories
   - OpenSpec = truth for tasks
   - Session state = "where we are"
   - Always suggest save before ending work
```

---

## 4.3 MCP Servers - Rola i Ograniczenia

### Serena (Code Navigation ONLY)

**Dozwolone narzędzia:**
- `get_symbols_overview` - lista symboli w pliku
- `find_symbol` - wyszukiwanie symboli po nazwie
- `find_referencing_symbols` - znajdowanie referencji

**NIE używać do:**
- ❌ Przechowywania stanu sesji (używaj session-state.json)
- ❌ Zapamiętywania decyzji (używaj OpenSpec)
- ❌ Memory files (wszystkie usunięte)

**Kto używa Sereny:**
- `architect` - analiza kodu przed projektowaniem
- `code-editor` - znajdowanie miejsc do modyfikacji

### Context7 (External Docs)

**Workflow:**
1. `resolve-library-id` - znajdź ID biblioteki
2. `get-library-docs` - pobierz dokumentację

**Kiedy używać:**
- Qt6 API documentation
- External library references

---

## 5. Hooks - Szczegółowa Specyfikacja

### 5.1 Macierz: Hook → Zastosowanie

| Hook | Trigger | Cel | Status |
|------|---------|-----|--------|
| SessionStart | Start sesji (startup, resume, clear, compact) | Auto-load session-state.json | ✅ WDROŻONE |
| SessionEnd | Koniec sesji (exit, Ctrl+C, logout, clear) | Przypomnienie o /save-session | ✅ WDROŻONE |
| SubagentStart | Start KAŻDEGO agenta | Inject project context | ✅ WDROŻONE |
| SubagentStop | Koniec KAŻDEGO agenta | Audit log | ✅ WDROŻONE |
| PreToolUse(git commit) | Przed commitem | Walidacja docs (haiku) | ✅ WDROŻONE |

### 5.2 Szczegóły Implementacji (AKTUALNE w settings.json)

#### HOOK: SessionStart (✅ WDROŻONE)
```json
{
  "SessionStart": [{
    "hooks": [{
      "type": "command",
      "command": "echo === SESSION START === && if exist .claude\\session-state.json (type .claude\\session-state.json) else (echo {\"status\": \"new_session\", \"note\": \"No previous session found\"})"
    }]
  }]
}
```
**Cel:** Automatycznie wyświetla stan poprzedniej sesji przy starcie
**Triggery:** startup, resume, clear, compact
**Output:** Zawartość session-state.json lub informacja o nowej sesji

#### HOOK: SessionEnd (✅ WDROŻONE)
```json
{
  "SessionEnd": [{
    "hooks": [{
      "type": "command",
      "command": "echo === SESSION END === && echo Remember to use /save-session before leaving! && echo Session ended at: %date% %time%"
    }]
  }]
}
```
**Cel:** Przypomnienie o zapisaniu sesji przed wyjściem
**Triggery:** exit, Ctrl+C, logout, clear, zamknięcie terminala
**Output:** Ostrzeżenie i timestamp zakończenia

#### HOOK: SubagentStart (✅ WDROŻONE)
```json
{
  "SubagentStart": [{
    "hooks": [{
      "type": "command",
      "command": "type .claude\\context\\project-brief.txt 2>nul || echo Kalahari | C++20 + Qt6 | Build: scripts/build_windows.bat Debug"
    }]
  }]
}
```
**Cel:** Każdy agent dostaje podstawowy kontekst projektu
**Output:** Treść project-brief.txt lub fallback string
**Składnia:** Windows (type, 2>nul)

#### HOOK: SubagentStop (✅ WDROŻONE)
```json
{
  "SubagentStop": [{
    "hooks": [{
      "type": "command",
      "command": "echo %date% %time%|%CLAUDE_AGENT_ID%|complete >> .claude\\logs\\agents.log"
    }]
  }]
}
```
**Cel:** Audit trail - kto co robił
**Output:** Wpis w .claude/logs/agents.log
**Składnia:** Windows (%date%, %time%, backslash paths)

#### HOOK: PreToolUse git commit (✅ WDROŻONE)
```json
{
  "PreToolUse": [{
    "matcher": "Bash(git commit*)",
    "hooks": [{
      "type": "prompt",
      "prompt": "Before commit, verify:\n1. CHANGELOG.md has entry in [Unreleased]?\n2. ROADMAP.md updated if new feature?\n3. OpenSpec status current?\n4. No TODO in staged files?\n\nReturn JSON:\n{\"decision\": \"approve\"} or\n{\"decision\": \"block\", \"reason\": \"...\"}",
      "model": "haiku",
      "timeout": 30000
    }]
  }]
}
```
**Cel:** Automatyczna walidacja przed każdym commitem
**Output:** approve lub block z powodem
**Model:** haiku (szybki, tani)

---

## 6. Workflow - Instrukcja Użycia

### 6.1 Tryb Pracy: Ręczne Wyzwalanie (DOMYŚLNY)

**Ty wyzwalasz agentów słowami kluczowymi. Kontekst przepływa automatycznie przez hooks.**

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    WORKFLOW: PEŁNY CYKL TASKA                                    │
│                                                                                  │
│  TY                              AGENT                    HOOK                   │
│  ──                              ─────                    ────                   │
│                                                                                  │
│  "nowe zadanie"                                                                  │
│       │                                                                          │
│       └──────────────────► TASK-MANAGER ◄──────── SubagentStart                  │
│                               │                   (inject context)              │
│                               │                                                  │
│                               ▼                                                  │
│                            Tworzy OpenSpec #NNNNN                                │
│                               │                                                  │
│                               └──────────────────► SubagentStop                  │
│                                                    (zapisuje transcript          │
│                                                    → .claude/context/)           │
│  "zaprojektuj"                                                                   │
│       │                                                                          │
│       └──────────────────► ARCHITECT ◄─────────── SubagentStart                  │
│                               │                   (inject context +              │
│                               │                    poprzedni transcript)         │
│                               ▼                                                  │
│                            Analizuje kod (Serena)                                │
│                            Projektuje rozwiązanie                                │
│                            Uzupełnia OpenSpec                                    │
│                               │                                                  │
│                               └──────────────────► SubagentStop                  │
│                                                                                  │
│  "napisz kod" / "zmień" / "panel"                                                │
│       │                                                                          │
│       └──────────────────► CODE-WRITER / CODE-EDITOR / UI-DESIGNER               │
│                               │                                                  │
│                               ▼                                                  │
│                            Implementuje wg designu                               │
│                            Uruchamia build                                       │
│                               │                                                  │
│                               └──────────────────► SubagentStop                  │
│                                                                                  │
│  "review" / "sprawdź kod"                                                        │
│       │                                                                          │
│       └──────────────────► CODE-REVIEWER                                         │
│                               │                                                  │
│                               ├── approve ──────► (kontynuuj)                    │
│                               │                                                  │
│                               └── block ────────► (zobacz 6.2 Pętle)             │
│                                                                                  │
│  "testy" / "przetestuj"                                                          │
│       │                                                                          │
│       └──────────────────► TESTER                                                │
│                               │                                                  │
│                               ├── pass ─────────► (kontynuuj)                    │
│                               │                                                  │
│                               └── fail ─────────► (zobacz 6.2 Pętle)             │
│                                                                                  │
│  "zamknij task"                                                                  │
│       │                                                                          │
│       └──────────────────► TASK-MANAGER                                          │
│                               │                                                  │
│                               ▼                                                  │
│                            Weryfikuje completeness                               │
│                            git commit ◄────────── PreToolUse hook                │
│                            OpenSpec → DEPLOYED    (waliduje docs)                │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Pętle Naprawcze

**Gdy code-reviewer zwraca BLOCK:**
```
Ty: "review"
    └──► CODE-REVIEWER
            │
            └── {"decision": "block", "issues": ["brak tr()", "hardcoded icon"]}
                    │
                    ▼
         [Agent pokazuje Ci issues]

Ty: "napraw te problemy"
    └──► CODE-EDITOR
            │
            └── Naprawia issues
                    │
                    ▼
Ty: "review ponownie"
    └──► CODE-REVIEWER
            │
            └── {"decision": "approve"}
```

**Gdy tester zwraca FAIL:**
```
Ty: "testy"
    └──► TESTER
            │
            └── {"decision": "fail", "failures": ["TestSettings::save failed"]}
                    │
                    ▼
         [Agent pokazuje Ci failures]

Ty: "napraw test TestSettings"
    └──► CODE-EDITOR
            │
            └── Naprawia kod
                    │
                    ▼
Ty: "testy ponownie"
    └──► TESTER
            │
            └── {"decision": "pass", "tests": "42/42"}
```

### 6.3 Przepływ Kontekstu (Hooks)

**SubagentStop zapisuje transcript każdego agenta:**
```json
{
  "SubagentStop": [{
    "hooks": [{
      "type": "command",
      "command": "cat $AGENT_TRANSCRIPT_PATH >> .claude/context/$AGENT_ID-latest.jsonl"
    }]
  }]
}
```

**Struktura plików kontekstu:**
```
.claude/context/
├── project-brief.txt              # Stały kontekst projektu
├── task-manager-latest.jsonl      # Ostatni transcript task-managera
├── architect-latest.jsonl         # Ostatni transcript architekta
├── code-writer-latest.jsonl       # ...
├── code-editor-latest.jsonl
├── ui-designer-latest.jsonl
├── code-reviewer-latest.jsonl
└── tester-latest.jsonl
```

**Każdy agent może czytać transcripty poprzedników** jeśli potrzebuje kontekstu.

### 6.4 Słowa Kluczowe - Szybka Ściągawka

| Słowo kluczowe | Agent | Co robi |
|----------------|-------|---------|
| "nowe zadanie", "co dalej" | task-manager | Tworzy OpenSpec |
| "status", "gdzie jesteśmy" | task-manager | Pokazuje postęp |
| "zaprojektuj", "przeanalizuj" | architect | Analizuje kod, projektuje |
| "napisz", "nowa klasa" | code-writer | Pisze NOWY kod |
| "zmień", "popraw", "napraw" | code-editor | Modyfikuje istniejący |
| "dialog", "panel", "UI" | ui-designer | Tworzy UI |
| "review", "sprawdź kod" | code-reviewer | Code review |
| "testy", "przetestuj" | tester | Uruchamia testy |
| "zamknij task" | task-manager | Zamyka OpenSpec |

### 6.5 Przykład Pełnego Cyklu

```
Ty: "nowe zadanie - chcę dodać panel statystyk"
    → TASK-MANAGER zbiera wymagania, tworzy OpenSpec #00028

Ty: "zaprojektuj rozwiązanie"
    → ARCHITECT analizuje MainWindow, ArtProvider, projektuje StatsPanel

Ty: "zrób panel"
    → UI-DESIGNER tworzy stats_panel.h/.cpp z QDockWidget

Ty: "review"
    → CODE-REVIEWER sprawdza, znajduje: "brak tr() w labelach"

Ty: "napraw"
    → CODE-EDITOR dodaje tr()

Ty: "review ponownie"
    → CODE-REVIEWER: approve

Ty: "testy"
    → TESTER: 42/42 pass

Ty: "zamknij task"
    → TASK-MANAGER: commit, DEPLOYED
```

### 6.6 Przyszłość: Orkiestrator Python (PLANOWANE)

**Po ustabilizowaniu workflow (za 2-3 tygodnie):**

Zbadamy Claude Agent SDK Python do automatycznej orkiestracji:

```python
from claude_agent_sdk import ClaudeSDKClient

client = ClaudeSDKClient(setting_sources=["project"])

def run_full_task(description: str):
    # Automatycznie: task-manager → architect → impl → review → test → close
    client.query(prompt=description, agents=["task-manager"])
    client.query(prompt="zaprojektuj", agents=["architect"])
    # ... z pętlami naprawczymi
```

**Korzyści:**
- Jedna komenda zamiast wielu
- Automatyczna obsługa pętli
- Możliwość równoległego uruchamiania agentów

**Sources:**
- [Claude Agent SDK Python - GitHub](https://github.com/anthropics/claude-agent-sdk-python)
- [Subagents in SDK - Claude Docs](https://docs.claude.com/en/docs/agent-sdk/subagents)

---

## 7. Diagram Powiązań

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              WORKFLOW KALAHARI                                   │
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │                        HOOKS (5 automatycznych)                         │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────────┐ │    │
│  │  │SessionStart  │  │SessionEnd    │  │PreToolUse(git commit)          │ │    │
│  │  │→ load state  │  │→ save remind │  │→ walidacja CHANGELOG/ROADMAP   │ │    │
│  │  └──────────────┘  └──────────────┘  └────────────────────────────────┘ │    │
│  │  ┌──────────────┐  ┌──────────────┐                                     │    │
│  │  │SubagentStart │  │SubagentStop  │                                     │    │
│  │  │→ inject ctx  │  │→ audit log   │                                     │    │
│  │  └──────────────┘  └──────────────┘                                     │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │                           AGENCI (7)                                     │    │
│  │                                                                          │    │
│  │   USER REQUEST                                                           │    │
│  │        │                                                                 │    │
│  │        ▼                                                                 │    │
│  │  ┌─────────────────────┐                                                 │    │
│  │  │   TASK-MANAGER      │ Skills: openspec-workflow, roadmap-analysis,    │    │
│  │  │   permissionMode:   │         session-protocol                        │    │
│  │  │   manual            │ Tworzy OpenSpec, pilnuje procesu                │    │
│  │  └─────────┬───────────┘                                                 │    │
│  │            │                                                             │    │
│  │            ▼                                                             │    │
│  │  ┌─────────────────────┐                                                 │    │
│  │  │    ARCHITECT        │ Skills: kalahari-coding, architecture-patterns  │    │
│  │  │   permissionMode:   │                                                 │    │
│  │  │   manual            │ Analizuje kod (Serena), projektuje rozwiązanie  │    │
│  │  └─────────┬───────────┘                                                 │    │
│  │            │                                                             │    │
│  │            ├────────────────────┬────────────────────┐                   │    │
│  │            ▼                    ▼                    ▼                   │    │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │    │
│  │  │  CODE-WRITER    │  │  CODE-EDITOR    │  │  UI-DESIGNER    │          │    │
│  │  │  bypassPerms    │  │  bypassPerms    │  │  bypassPerms    │          │    │
│  │  │                 │  │                 │  │                 │          │    │
│  │  │  Skill:         │  │  Skill:         │  │  Skills:        │          │    │
│  │  │  kalahari-coding│  │  kalahari-coding│  │  kalahari-coding│          │    │
│  │  │                 │  │                 │  │  qt6-desktop-ux │          │    │
│  │  │  NOWY kod       │  │  MODYFIKACJE    │  │  UI/UX          │          │    │
│  │  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘          │    │
│  │           │                    │                    │                   │    │
│  │           └────────────────────┼────────────────────┘                   │    │
│  │                                │                                        │    │
│  │                                ▼                                        │    │
│  │  ┌─────────────────────────────────────────┐                            │    │
│  │  │           CODE-REVIEWER                 │                            │    │
│  │  │   permissionMode: manual                │                            │    │
│  │  │   Skills: kalahari-coding,              │                            │    │
│  │  │           quality-checklist             │                            │    │
│  │  │                                         │                            │    │
│  │  │   Sprawdza: ArtProvider, tr(),          │                            │    │
│  │  │   CHANGELOG, ROADMAP, TODO              │                            │    │
│  │  │   Output: approve/block                 │                            │    │
│  │  └─────────────────┬───────────────────────┘                            │    │
│  │                    │                                                    │    │
│  │                    ▼                                                    │    │
│  │  ┌─────────────────────────────────────────┐                            │    │
│  │  │              TESTER                     │                            │    │
│  │  │   permissionMode: manual                │                            │    │
│  │  │   Skills: kalahari-coding,              │                            │    │
│  │  │           testing-procedures            │                            │    │
│  │  │                                         │                            │    │
│  │  │   Uruchamia: build, testy               │                            │    │
│  │  │   Output: pass/fail                     │                            │    │
│  │  └─────────────────┬───────────────────────┘                            │    │
│  │                    │                                                    │    │
│  │                    ▼                                                    │    │
│  │  ┌─────────────────────────────────────────┐                            │    │
│  │  │   TASK-MANAGER (zamknięcie)             │                            │    │
│  │  │                                         │                            │    │
│  │  │   Weryfikuje: wszystko done?            │                            │    │
│  │  │   → git commit (trigger: PreToolUse)    │                            │    │
│  │  │   → OpenSpec status → DEPLOYED          │                            │    │
│  │  └─────────────────────────────────────────┘                            │    │
│  │                                                                          │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐    │
│  │                           SKILLS (8)                                     │    │
│  │                                                                          │    │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐       │    │
│  │  │ kalahari-coding  │  │ qt6-desktop-ux   │  │ openspec-workflow│       │    │
│  │  │ (6 agentów)      │  │ (ui-designer)    │  │ (task-manager)   │       │    │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────┘       │    │
│  │                                                                          │    │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐       │    │
│  │  │ roadmap-analysis │  │architecture-     │  │ quality-checklist│       │    │
│  │  │ (task-manager)   │  │patterns(architect│  │ (code-reviewer)  │       │    │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────┘       │    │
│  │                                                                          │    │
│  │  ┌──────────────────┐  ┌──────────────────┐                              │    │
│  │  │testing-procedures│  │ session-protocol │                              │    │
│  │  │ (tester)         │  │ (task-manager,   │                              │    │
│  │  │                  │  │  commands)       │                              │    │
│  │  └──────────────────┘  └──────────────────┘                              │    │
│  │                                                                          │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Struktura `.claude/`

```
.claude/
├── settings.json                    # Hooks configuration
├── context/
│   └── project-brief.txt            # ~50 linii, inject przez SubagentStart
├── agents/
│   ├── task-manager.md              # Pełna specyfikacja z sekcji 3.1
│   ├── architect.md                 # Pełna specyfikacja z sekcji 3.2
│   ├── code-writer.md               # Pełna specyfikacja z sekcji 3.3
│   ├── code-editor.md               # Pełna specyfikacja z sekcji 3.4
│   ├── ui-designer.md               # Pełna specyfikacja z sekcji 3.5
│   ├── code-reviewer.md             # Pełna specyfikacja z sekcji 3.6
│   └── tester.md                    # Pełna specyfikacja z sekcji 3.7
├── skills/
│   ├── kalahari-coding/SKILL.md
│   ├── qt6-desktop-ux/SKILL.md
│   ├── openspec-workflow/SKILL.md
│   ├── roadmap-analysis/SKILL.md
│   ├── architecture-patterns/SKILL.md
│   ├── quality-checklist/SKILL.md
│   ├── testing-procedures/SKILL.md
│   └── session-protocol/SKILL.md
├── commands/
│   ├── save-session.md
│   ├── load-session.md
│   └── openspec/
│       ├── proposal.md
│       ├── apply.md
│       └── archive.md
├── logs/
│   ├── agents.log                   # Audit trail z SubagentStop
│   └── session.log
└── session-state.json               # Stan sesji (NIE commitować)
```

---

## 9. Minimalne CLAUDE.md (~80 linii)

```markdown
# KALAHARI - Writer's IDE

C++20 + Qt6 | Desktop Application
Szczegóły: .claude/context/project-brief.txt

## Agenci (7)

| Trigger | Agent | Co robi |
|---------|-------|---------|
| "nowe zadanie", "co dalej", "status" | task-manager | Tworzy/śledzi/zamyka OpenSpec |
| "zaprojektuj", "przeanalizuj" | architect | Analizuje kod, projektuje rozwiązania |
| "napisz", "nowa klasa" | code-writer | Pisze NOWY kod |
| "zmień", "popraw", "napraw" | code-editor | Modyfikuje ISTNIEJĄCY kod |
| "dialog", "panel", "UI" | ui-designer | Tworzy komponenty UI |
| "review", "sprawdź kod" | code-reviewer | Code review przed commitem |
| "testy", "przetestuj" | tester | Uruchamia testy, raportuje wyniki |

## Flow

1. task-manager → tworzy OpenSpec
2. architect → analizuje i projektuje
3. code-writer/editor/ui-designer → implementuje
4. code-reviewer → sprawdza jakość
5. tester → uruchamia testy
6. task-manager → zamyka task

## MCP

- **Serena:** TYLKO nawigacja po kodzie
- **Context7:** Dokumentacja zewnętrznych bibliotek

## Build

- Windows: `scripts/build_windows.bat Debug`
- Linux: `scripts/build_linux.sh`
```

---

## 10. Plan Implementacji - Szczegółowy

### 10.1 Fazy

| Faza | Nazwa | Czas | Zawartość |
|------|-------|------|-----------|
| A | Przygotowanie | 15 min | Backup, weryfikacja backupu |
| B | Struktura | 10 min | Katalogi, pliki .gitkeep |
| C | Context | 20 min | project-brief.txt, CLAUDE.md |
| D | Skills | 120 min | 8 skills z pełną zawartością |
| E | Agenci | 120 min | 7 agentów z pełną specyfikacją |
| F | Hooks | 30 min | settings.json z 5 hooks |
| G | Commands | 30 min | save-session, load-session, openspec/* |
| H | Serena cleanup | 15 min | Usunięcie sesyjnych memories |
| I | Testy | 90 min | Test każdego agenta i hooka |

**RAZEM: ~7.5 godziny**

### 10.2 FAZA A: Przygotowanie (15 min)

```
1. Utworzenie backup:
   - [ ] mv .claude .claude-backup-$(date +%Y%m%d-%H%M%S)
   - [ ] cp CLAUDE.md CLAUDE.md.backup

2. Weryfikacja backup:
   - [ ] ls -la .claude-backup-* (istnieje?)
   - [ ] ls -la CLAUDE.md.backup (istnieje?)

3. Sprawdzenie obecnych plików:
   - [ ] Przeczytaj .claude-backup-*/skills/*/SKILL.md
   - [ ] Zanotuj co wartościowe do migracji
```

### 10.3 FAZA B: Struktura (10 min)

```
1. Katalog główny:
   - [ ] mkdir .claude

2. Podkatalogi:
   - [ ] mkdir .claude/context
   - [ ] mkdir .claude/agents
   - [ ] mkdir .claude/skills
   - [ ] mkdir .claude/commands
   - [ ] mkdir .claude/commands/openspec
   - [ ] mkdir .claude/logs

3. Katalogi skills (8):
   - [ ] mkdir .claude/skills/kalahari-coding
   - [ ] mkdir .claude/skills/qt6-desktop-ux
   - [ ] mkdir .claude/skills/openspec-workflow
   - [ ] mkdir .claude/skills/roadmap-analysis
   - [ ] mkdir .claude/skills/architecture-patterns
   - [ ] mkdir .claude/skills/quality-checklist
   - [ ] mkdir .claude/skills/testing-procedures
   - [ ] mkdir .claude/skills/session-protocol

4. Pliki pomocnicze:
   - [ ] touch .claude/logs/.gitkeep
   - [ ] echo '{}' > .claude/session-state.json
```

### 10.4 FAZA C: Context (20 min)

```
1. Project Brief (~50 linii):
   - [ ] Utwórz .claude/context/project-brief.txt
   - [ ] Zawartość: nazwa, stack, build, faza, OpenSpec lokalizacja
   - [ ] Weryfikuj: wc -l < 60

2. CLAUDE.md (~80 linii):
   - [ ] Utwórz nowy CLAUDE.md
   - [ ] Tabela 7 agentów z triggerami
   - [ ] Flow 6 kroków
   - [ ] MCP rules
   - [ ] Build commands
   - [ ] Weryfikuj: wc -l < 100
```

### 10.5 FAZA D: Skills (120 min)

```
Dla KAŻDEGO skill:
1. Utwórz plik SKILL.md w odpowiednim katalogu
2. Frontmatter YAML: name, description
3. Treść w formacie proceduralnym (listy, nie opisy!)
4. Weryfikuj: < 150 linii, format prawidłowy

Kolejność:
- [ ] D.1: kalahari-coding (20 min) - największy, używany przez 6 agentów
- [ ] D.2: qt6-desktop-ux (15 min)
- [ ] D.3: openspec-workflow (15 min)
- [ ] D.4: roadmap-analysis (10 min)
- [ ] D.5: architecture-patterns (20 min) - NOWY
- [ ] D.6: quality-checklist (15 min)
- [ ] D.7: testing-procedures (15 min) - NOWY
- [ ] D.8: session-protocol (10 min)
```

### 9.6 FAZA E: Agenci (120 min)

```
Dla KAŻDEGO agenta:
1. Utwórz plik .md w .claude/agents/
2. Frontmatter YAML:
   - name
   - description (z triggerami!)
   - permissionMode
   - skills (lista)
   - tools (lista)
3. Treść: flow pracy z sekcji 3.x
4. Sekcja "NIE ROBI"
5. Weryfikuj: spójność z sekcją 3

Kolejność:
- [ ] E.1: task-manager (25 min) - najbardziej złożony, 3 tryby
- [ ] E.2: architect (20 min) - analiza + design
- [ ] E.3: code-writer (15 min)
- [ ] E.4: code-editor (15 min)
- [ ] E.5: ui-designer (15 min)
- [ ] E.6: code-reviewer (15 min)
- [ ] E.7: tester (15 min)
```

### 9.7 FAZA F: Hooks (30 min)

```
1. Utwórz .claude/settings.json:
   - [ ] SubagentStart hook
   - [ ] SubagentStop hook
   - [ ] PreToolUse(git commit) hook
   - [ ] (opcjonalnie) SessionStart hook
   - [ ] (opcjonalnie) SessionEnd hook

2. Walidacja:
   - [ ] JSON syntax valid
   - [ ] Paths correct
   - [ ] Commands executable
```

### 9.8 FAZA G: Commands (30 min)

```
1. Migracja z backup:
   - [ ] save-session.md (zmodyfikować dla session-state.json)
   - [ ] load-session.md (zmodyfikować dla session-state.json)
   - [ ] openspec/proposal.md
   - [ ] openspec/apply.md
   - [ ] openspec/archive.md

2. NIE kopiować:
   - next-task.md (zastąpiony przez task-manager)
   - push.md (nieużywany)
```

### 9.9 FAZA H: Serena Cleanup (15 min)

```
1. Lista memories:
   - [ ] mcp__serena__list_memories

2. Usunąć sesyjne:
   - [ ] *session* memories
   - [ ] *status* memories (jeśli sesyjne)

3. Zachować projektowe:
   - kalahari_*
   - qt_migration_*
   - phase*
```

### 9.10 FAZA I: Testy (90 min)

```
I.1 Test SubagentStart (5 min):
- [ ] Uruchom dowolnego agenta
- [ ] Sprawdź czy dostał context z project-brief.txt

I.2 Test SubagentStop (5 min):
- [ ] Po agencie sprawdź .claude/logs/agents.log
- [ ] Czy jest wpis z timestamp i AGENT_ID?

I.3 Test task-manager - TRYB 1 (15 min):
- [ ] "nowe zadanie"
- [ ] Czy pyta o wymagania?
- [ ] Czy tworzy OpenSpec?
- [ ] Czy numeracja poprawna?

I.4 Test task-manager - TRYB 2 (5 min):
- [ ] "status taska"
- [ ] Czy pokazuje postęp?

I.5 Test architect (15 min):
- [ ] "zaprojektuj rozwiązanie dla OpenSpec #X"
- [ ] Czy używa Serena?
- [ ] Czy uzupełnia design?

I.6 Test code-writer (10 min):
- [ ] "napisz nową klasę X"
- [ ] Czy stosuje wzorce (ArtProvider, tr())?
- [ ] Czy uruchamia build?

I.7 Test code-editor (10 min):
- [ ] "zmień klasę Y"
- [ ] Czy używa Edit (nie Write)?

I.8 Test ui-designer (10 min):
- [ ] "utwórz dialog Z"
- [ ] Czy stosuje qt6-desktop-ux patterns?

I.9 Test code-reviewer (10 min):
- [ ] "review kodu"
- [ ] Czy sprawdza wszystkie punkty z checklist?
- [ ] Czy zwraca approve/block?

I.10 Test tester (5 min):
- [ ] "uruchom testy"
- [ ] Czy uruchamia build + testy?
- [ ] Czy raportuje wyniki?

I.11 Test PreToolUse hook (10 min):
- [ ] Spróbuj git commit bez CHANGELOG
- [ ] Czy hook blokuje?
- [ ] Dodaj CHANGELOG, spróbuj ponownie
- [ ] Czy przepuszcza?
```

### 9.11 Status Implementacji

| Faza | Status | Data | Notatki |
|------|--------|------|---------|
| A: Przygotowanie | [x] | 2025-11-27 | Backup wykonany |
| B: Struktura | [x] | 2025-11-27 | Wszystkie katalogi utworzone |
| C: Context | [x] | 2025-11-27 | project-brief.txt, CLAUDE.md |
| D: Skills | [x] | 2025-11-27 | 8 skills z pełną zawartością |
| E: Agenci | [x] | 2025-11-27 | 7 agentów z pełną specyfikacją |
| F: Hooks | [x] | 2025-11-27 | SubagentStart, SubagentStop, PreToolUse |
| G: Commands | [x] | 2025-11-27 | save-session, load-session, openspec/* |
| H: Serena | [x] | 2025-11-28 | Wszystkie memories usunięte (14 plików) |
| I: Testy | [ ] | | Do przeprowadzenia |

**Data rozpoczęcia:** 2025-11-27
**Data zakończenia:** W trakcie (pozostały testy)

### 9.12 Rollback Plan

```
Jeśli coś pójdzie nie tak:

1. rm -rf .claude
2. mv .claude-backup-YYYYMMDD-HHMMSS .claude
3. mv CLAUDE.md.backup CLAUDE.md
4. Zweryfikuj: claude doctor
```

---

## 11. Zasada Formatowania

**REGUŁA KRYTYCZNA:** Wszystkie dokumenty (skills, agenci) w formacie PROCEDURALNYM.

**NIE:**
```
Task manager jest agentem odpowiedzialnym za zarządzanie zadaniami.
Gdy użytkownik chce utworzyć nowe zadanie, agent najpierw sprawdza
czy użytkownik ma już pomysł na zadanie...
```

**TAK:**
```
## Task Manager - Tworzenie Taska

1. User mówi "nowe zadanie"
2. Sprawdź czy ma pomysł:
   - Jeśli TAK → krok 4
   - Jeśli NIE → krok 3
3. Przeczytaj ROADMAP, zaproponuj 3 opcje
4. Zbierz wymagania (cel, zakres, kryteria)
5. Utwórz folder OpenSpec
6. Wygeneruj proposal.md
```

---

## 12. Notatki z Dyskusji

### 2025-11-27

- User chce KOMPLETNE usunięcie obecnych rozwiązań
- CLAUDE.md z 620 linii → ~80 linii
- Format proceduralny (listy), nie opisowy
- **7 agentów:**
  - task-manager, architect, code-writer, code-editor, ui-designer, code-reviewer, tester
- **Task Manager = Manager Projektu:**
  - 3 tryby: tworzenie, śledzenie, zamykanie
  - Pilnuje procesu, NIE analizuje kodu
- **Architect = Analityk + Projektant:**
  - Używa Serena intensywnie
  - Projektuje rozwiązania
- **Jasny podział:**
  - code-writer = NOWY kod
  - code-editor = MODYFIKACJE
  - ui-designer = UI/UX
  - code-reviewer = jakość kodu
  - tester = uruchamianie testów
- **Skills dopasowane do agentów:**
  - 8 skills, każdy przypisany do konkretnych agentów
- **Hooks:**
  - 3 core (WDROŻYĆ): SubagentStart, SubagentStop, PreToolUse
  - 2 optional (ROZWAŻYĆ): SessionStart, SessionEnd

---

## 12A. WERYFIKACJA WZORCÓW - Stan Faktyczny w Kodzie (2025-11-27)

### 12A.1 Singletony - Faktyczne API

| Klasa | Metoda dostępu | Namespace | Użycie |
|-------|----------------|-----------|--------|
| **Logger** | `Logger::getInstance()` | `kalahari::core` | Logowanie |
| **SettingsManager** | `SettingsManager::getInstance()` | `kalahari::core` | Konfiguracja |
| **ArtProvider** | `ArtProvider::getInstance()` | `kalahari::core` | Ikony, kolory, rozmiary |
| **IconRegistry** | `IconRegistry::getInstance()` | `kalahari::core` | Rejestr ikon (wewnętrzny) |
| **ThemeManager** | `ThemeManager::getInstance()` | `kalahari::core` | Motywy, palety |
| **CommandRegistry** | `CommandRegistry::getInstance()` | `kalahari::gui` | Rejestr komend |
| **DiagnosticManager** | `DiagnosticManager::getInstance()` | `kalahari::core` | Diagnostyka |

### 12A.2 Wzorce - FAKTYCZNY Stan (vs STRATEGY sekcja 4.2)

| Wzorzec | STRATEGY (BŁĘDNE) | FAKTYCZNE API | Przykład z kodu |
|---------|-------------------|---------------|-----------------|
| **Ikony** | `ArtProvider::instance().getIcon()` | `ArtProvider::getInstance().getIcon()` | main_window.cpp:1976 |
| **Kolory** | `Theme::instance().getColor()` | `ArtProvider::getInstance().getPrimaryColor()` / `getSecondaryColor()` | art_provider.cpp:144-149 |
| **Config** | `SettingsManager::instance().getValue()` | `SettingsManager::getInstance().getValue()` | main.cpp:64 |
| **Akcje** | (brak) | `ArtProvider::getInstance().createAction()` | menu_builder.cpp:182 |
| **Motywy** | (brak) | `ThemeManager::getInstance().getCurrentTheme()` | main_window.cpp:109 |

### 12A.3 Prawidłowe Wzorce do Użycia

```cpp
// 1. IKONY - przez ArtProvider
QIcon icon = core::ArtProvider::getInstance().getIcon("file.new");
QPixmap pixmap = core::ArtProvider::getInstance().getPixmap("file.new", 24);

// 2. AKCJE - przez ArtProvider (auto-odświeżanie przy zmianie motywu)
QAction* action = core::ArtProvider::getInstance().createAction("file.new", parent);

// 3. KOLORY IKON - przez ArtProvider
QColor primary = core::ArtProvider::getInstance().getPrimaryColor();
QColor secondary = core::ArtProvider::getInstance().getSecondaryColor();
core::ArtProvider::getInstance().setPrimaryColor(QColor("#333333"));
core::ArtProvider::getInstance().setSecondaryColor(QColor("#999999"));

// 4. KONFIGURACJA - przez SettingsManager
auto& settings = core::SettingsManager::getInstance();
std::string value = settings.getValue("key", "default");
settings.setValue("key", "value");

// 5. MOTYWY - przez ThemeManager
const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
// theme.colors.primary, theme.colors.secondary, theme.palette, theme.log

// 6. TEKSTY UI - przez tr()
tr("User visible text")

// 7. LOGOWANIE - przez Logger
core::Logger::getInstance().info("Message: {}", value);
core::Logger::getInstance().debug("Debug: {}", value);
core::Logger::getInstance().error("Error: {}", value);

// 8. KOMENDY - przez CommandRegistry
gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
registry.registerCommand(cmdDef);
```

### 12A.4 NIGDY nie używać

```cpp
// BŁĘDNE - nie istnieje!
Theme::instance().getColor()           // NIE MA takiego API!
ArtProvider::instance()                // Błędna nazwa metody!
SettingsManager::instance()            // Błędna nazwa metody!

// BŁĘDNE - hardcoded wartości
QIcon("path/to/icon.svg")              // Użyj ArtProvider
QColor(255, 0, 0)                      // Użyj ArtProvider lub Theme
"Hardcoded string"                     // Użyj tr()
```

### 12A.5 Korekty wymagane w STRATEGY sekcja 4.2

**Linia 473:** `ArtProvider::instance().getIcon()` → `ArtProvider::getInstance().getIcon()`
**Linia 478:** `SettingsManager::instance().getValue()` → `SettingsManager::getInstance().getValue()`
**Linia 486:** `Theme::instance().getColor()` → `ArtProvider::getInstance().getPrimaryColor()` / `getSecondaryColor()`

---

## 13. Claude Agent SDK Python - Dokumentacja (2025-11-27)

### 13.1 Źródła

| Źródło | URL | Status |
|--------|-----|--------|
| GitHub SDK | https://github.com/anthropics/claude-agent-sdk-python | ✅ Przeanalizowane |
| Subagents Docs | https://code.claude.com/docs/en/sub-agents | ✅ Przeanalizowane |
| Blog Anthropic | https://www.anthropic.com/engineering/building-agents-with-the-claude-agent-sdk | 🔗 Dostępne |

### 13.2 Instalacja i Wymagania

```bash
pip install claude-agent-sdk
```

**Wymagania:**
- Python 3.10+
- Claude Code CLI (bundlowany automatycznie)
- Node.js 18+ (dla CLI)

**Opcjonalnie:** Custom path do CLI:
```python
ClaudeAgentOptions(cli_path="/path/to/claude")
```

### 13.3 Dwa Sposoby Użycia SDK

#### Sposób A: `query()` - Prosty (one-shot)

```python
import anyio
from claude_agent_sdk import query

async def main():
    async for message in query(prompt="What is 2 + 2?"):
        print(message)

anyio.run(main)
```

**Użycie:** Proste zapytania, brak konwersacji wieloturowej.

#### Sposób B: `ClaudeSDKClient` - Zaawansowany (bidirectional)

```python
from claude_agent_sdk import ClaudeSDKClient, ClaudeAgentOptions

options = ClaudeAgentOptions(
    system_prompt="You are helpful",
    allowed_tools=["Read", "Write", "Bash"],
    permission_mode='acceptEdits'
)

async with ClaudeSDKClient(options=options) as client:
    await client.query("Your prompt here")
    async for msg in client.receive_response():
        print(msg)
```

**Użycie:** Konwersacje wieloturowe, custom tools, hooks.

### 13.4 ClaudeAgentOptions - Pełna Lista

| Parametr | Typ | Opis |
|----------|-----|------|
| `system_prompt` | str | System prompt dla agenta |
| `allowed_tools` | list[str] | Lista dozwolonych narzędzi |
| `permission_mode` | str | 'default', 'acceptEdits', 'bypassPermissions' |
| `cwd` | str/Path | Working directory |
| `max_turns` | int | Limit iteracji konwersacji |
| `mcp_servers` | dict | Custom MCP servers |
| `hooks` | dict | Event handlers |
| `cli_path` | str | Custom path do CLI |

### 13.5 Custom Tools (SDK MCP Servers)

**Kluczowa funkcja:** Definiowanie narzędzi jako funkcji Python (bez subprocess!)

```python
from claude_agent_sdk import tool, create_sdk_mcp_server, ClaudeAgentOptions, ClaudeSDKClient

@tool("greet", "Greet a user", {"name": str})
async def greet_user(args):
    return {
        "content": [
            {"type": "text", "text": f"Hello, {args['name']}!"}
        ]
    }

server = create_sdk_mcp_server(
    name="my-tools",
    version="1.0.0",
    tools=[greet_user]
)

options = ClaudeAgentOptions(
    mcp_servers={"tools": server},
    allowed_tools=["mcp__tools__greet"]
)
```

**Zalety vs zewnętrzne serwery:**
- ✅ Brak subprocess overhead
- ✅ Lepsza wydajność (brak IPC)
- ✅ Łatwiejszy debugging
- ✅ Type-safe Python

### 13.6 Hooks w SDK

**Hooks = deterministyczne callbacki podczas wykonywania agenta**

```python
from claude_agent_sdk import ClaudeAgentOptions, HookMatcher

async def check_bash_command(input_data, tool_use_id, context):
    tool_name = input_data["tool_name"]
    if tool_name != "Bash":
        return {}

    command = input_data["tool_input"].get("command", "")
    if "dangerous" in command:
        return {
            "hookSpecificOutput": {
                "hookEventName": "PreToolUse",
                "permissionDecision": "deny",
                "permissionDecisionReason": "Blocked pattern detected"
            }
        }
    return {}

options = ClaudeAgentOptions(
    allowed_tools=["Bash"],
    hooks={
        "PreToolUse": [
            HookMatcher(matcher="Bash", hooks=[check_bash_command]),
        ],
    }
)
```

### 13.7 Subagenci - Jak Działają

**Subagent = wyspecjalizowany asystent AI z:**
- Oddzielnym context window
- Custom system prompt
- Skonfigurowanym dostępem do narzędzi
- Task-specific expertise

**Lokalizacje plików subagentów:**
- `.claude/agents/` - projektowe (wyższy priorytet)
- `~/.claude/agents/` - globalne (niższy priorytet)

### 13.8 Format Pliku Subagenta (.md)

```markdown
---
name: agent-identifier
description: When and how this agent should be used
tools: Tool1, Tool2, Tool3  # Optional
model: sonnet  # Optional: sonnet, opus, haiku, 'inherit'
permissionMode: default  # Optional
skills: skill1, skill2  # Optional
---

Your system prompt describing role, capabilities, and constraints.
```

**Pola konfiguracji:**

| Pole | Wymagane | Wartości |
|------|----------|----------|
| `name` | ✅ | lowercase-with-hyphens |
| `description` | ✅ | Kiedy i jak używać (triggery!) |
| `tools` | ❌ | Lista lub dziedziczenie |
| `model` | ❌ | sonnet/opus/haiku/inherit |
| `permissionMode` | ❌ | default/acceptEdits/bypassPermissions/plan/ignore |
| `skills` | ❌ | Auto-ładowane skills |

### 13.9 Wyzwalanie Subagentów

#### Automatyczne (przez Claude)
Claude deleguje gdy opis agenta pasuje do kontekstu zadania.

**Tip:** W opisie użyj "use PROACTIVELY" lub "MUST BE USED" aby zachęcić do automatycznego użycia.

#### Jawne (przez użytkownika)
```
> Use the code-reviewer subagent to check my recent changes
> Have the debugger subagent investigate this error
```

#### Przez CLI
```bash
claude --agents '{
  "code-reviewer": {
    "description": "Expert code reviewer",
    "prompt": "You are a senior code reviewer...",
    "tools": ["Read", "Grep", "Bash"],
    "model": "sonnet"
  }
}'
```

### 13.10 Kontekst Między Agentami

**WAŻNE: Subagenci są IZOLOWANI od głównej konwersacji!**

> "Each subagent operates in its own context, preventing pollution of the main conversation."

**Konsekwencje:**
- Subagent NIE widzi historii głównej konwersacji
- Musisz przekazać kontekst w prompcie
- Wynik subagenta wraca do głównej konwersacji

**Dziedziczenie narzędzi:** Gdy `tools` pominięte → subagent dziedziczy wszystkie narzędzia (w tym MCP).

### 13.11 Chainowanie i Pętle

#### Chainowanie wielu subagentów
```
> First use the code-analyzer subagent to find performance issues,
  then use the optimizer subagent to fix them
```

#### Wznawialne subagenty (agentId)
```
> Use the code-analyzer agent to start reviewing the authentication module
[Returns agentId: "abc123"]

> Resume agent abc123 and now analyze the authorization logic
```

**Przechowywanie:** `agent-{agentId}.jsonl` w katalogu projektu.

### 13.12 Przykład: Code Reviewer Subagent

```markdown
---
name: code-reviewer
description: Expert code review specialist. Proactively reviews
code for quality, security, and maintainability.
tools: Read, Grep, Glob, Bash
model: inherit
---

You are a senior code reviewer ensuring high standards.

When invoked:
1. Run git diff to see recent changes
2. Focus on modified files
3. Begin review immediately

Review checklist:
- Code is simple and readable
- Functions and variables are well-named
- No duplicated code
- Proper error handling
```

### 13.13 Wbudowane Subagenty Claude Code

| Nazwa | Model | Narzędzia | Cel |
|-------|-------|-----------|-----|
| general-purpose | Sonnet | Wszystkie | Złożone zadania wielokrokowe |
| plan | Sonnet | Read/Glob/Grep/Bash | Research w trybie planowania |
| explore | Haiku | Read-only | Szybkie przeszukiwanie codebase |

### 13.14 Error Handling w SDK

```python
from claude_agent_sdk import (
    ClaudeSDKError,      # Bazowa klasa
    CLINotFoundError,    # CLI nie zainstalowane
    CLIConnectionError,  # Błąd połączenia
    ProcessError,        # Proces zakończony błędem
    CLIJSONDecodeError   # Błąd parsowania odpowiedzi
)

try:
    async for msg in query(prompt="..."):
        pass
except CLINotFoundError:
    print("Zainstaluj Claude Code CLI!")
except ProcessError as e:
    print(f"Błąd procesu: {e}")
```

### 13.15 Message Types

| Typ | Opis |
|-----|------|
| `AssistantMessage` | Odpowiedzi Claude |
| `UserMessage` | Input użytkownika |
| `SystemMessage` | Instrukcje systemowe |
| `ResultMessage` | Wyniki narzędzi |

**Content blocks:** `TextBlock`, `ToolUseBlock`, `ToolResultBlock`

---

## 14. Orkiestrator Python - Koncepcja

### 14.1 Dwa Podejścia

| Podejście | Opis | Zalety | Wady |
|-----------|------|--------|------|
| **A: Subagenci .claude/agents/** | Pliki .md, Claude deleguje | Proste, natywne | Brak pętli automatycznych |
| **B: SDK Python orchestrator** | Skrypt Python kontroluje flow | Pełna kontrola, pętle | Wymaga kodu, utrzymania |

### 14.2 Podejście Hybrydowe (REKOMENDOWANE)

**Połączenie obu:**
1. Subagenci w `.claude/agents/` (specjalizacja, wiedza domenowa)
2. SDK Python do orkiestracji (pętle, decyzje, flow control)

```
┌─────────────────────────────────────────────────────────────────┐
│                    ORCHESTRATOR (Python SDK)                     │
│                                                                  │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐  │
│  │ architect│───►│code-writer│───►│ reviewer │───►│  tester  │  │
│  │ subagent │    │ subagent │    │ subagent │    │ subagent │  │
│  └──────────┘    └──────────┘    └────┬─────┘    └──────────┘  │
│                                       │                         │
│                        ┌──────────────┴──────────────┐          │
│                        │ DECISION LOOP (Python)      │          │
│                        │ if REJECT → back to writer  │          │
│                        │ if APPROVE → to tester      │          │
│                        └─────────────────────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

### 14.3 Minimalny Orkiestrator - POC

```python
"""
kalahari_orchestrator.py - Proof of Concept

Prosty orkiestrator: architect → code-writer → code-reviewer (loop) → tester
"""

import anyio
import json
from claude_agent_sdk import query, ClaudeAgentOptions

# ══════════════════════════════════════════════════════════════════
# KONFIGURACJA
# ══════════════════════════════════════════════════════════════════

MAX_REVIEW_ITERATIONS = 3
PROJECT_DIR = "E:/Python/Projekty/Kalahari"

# ══════════════════════════════════════════════════════════════════
# AGENCI (używają subagentów z .claude/agents/)
# ══════════════════════════════════════════════════════════════════

async def call_agent(agent_name: str, prompt: str, context: str = "") -> str:
    """Wywołuje subagenta i zwraca jego odpowiedź."""

    full_prompt = f"""Use the {agent_name} subagent for this task.

Context from previous steps:
{context}

Task:
{prompt}

Return your complete analysis/output."""

    options = ClaudeAgentOptions(
        cwd=PROJECT_DIR,
        permission_mode='acceptEdits'
    )

    result = []
    async for message in query(prompt=full_prompt, options=options):
        if hasattr(message, 'content'):
            for block in message.content:
                if hasattr(block, 'text'):
                    result.append(block.text)

    return "\n".join(result)


async def architect_phase(task_description: str) -> dict:
    """Faza 1: Architect analizuje i projektuje."""
    print("🏗️  ARCHITECT: Analyzing and designing...")

    response = await call_agent(
        "architect",
        f"Analyze the codebase and create a design for: {task_description}"
    )

    return {
        "phase": "architect",
        "design": response,
        "status": "complete"
    }


async def code_writer_phase(design: str, feedback: str = "") -> dict:
    """Faza 2: Code-writer implementuje."""
    print("✍️  CODE-WRITER: Implementing...")

    context = f"Design:\n{design}"
    if feedback:
        context += f"\n\nPrevious review feedback to address:\n{feedback}"

    response = await call_agent(
        "code-writer",
        "Implement the code according to the design. Address any feedback.",
        context=context
    )

    return {
        "phase": "code-writer",
        "code": response,
        "status": "complete"
    }


async def code_reviewer_phase(code: str, design: str) -> dict:
    """Faza 3: Code-reviewer sprawdza jakość."""
    print("🔍  CODE-REVIEWER: Reviewing...")

    context = f"Design:\n{design}\n\nCode to review:\n{code}"

    response = await call_agent(
        "code-reviewer",
        """Review the code against the design. Check:
1. ArtProvider for icons (not hardcoded paths)
2. tr() for UI strings (not hardcoded)
3. SettingsManager for config
4. Code quality and naming

Return JSON:
{"decision": "APPROVE"} or {"decision": "REJECT", "issues": ["issue1", "issue2"]}""",
        context=context
    )

    # Parsuj decyzję
    try:
        # Szukaj JSON w odpowiedzi
        import re
        json_match = re.search(r'\{[^}]+\}', response)
        if json_match:
            decision = json.loads(json_match.group())
        else:
            decision = {"decision": "APPROVE"}  # Default jeśli brak JSON
    except:
        decision = {"decision": "APPROVE"}

    return {
        "phase": "code-reviewer",
        "feedback": response,
        "decision": decision.get("decision", "APPROVE"),
        "issues": decision.get("issues", []),
        "status": "complete"
    }


async def tester_phase(code: str) -> dict:
    """Faza 4: Tester uruchamia testy."""
    print("🧪  TESTER: Running tests...")

    response = await call_agent(
        "tester",
        """Run the build and tests:
1. scripts/build_windows.bat Debug
2. Run kalahari-tests.exe

Return JSON:
{"decision": "PASS", "tests": "X/Y passed"} or {"decision": "FAIL", "failures": ["test1"]}""",
        context=f"Code to test:\n{code}"
    )

    return {
        "phase": "tester",
        "report": response,
        "status": "complete"
    }


# ══════════════════════════════════════════════════════════════════
# GŁÓWNY PIPELINE
# ══════════════════════════════════════════════════════════════════

async def run_pipeline(task_description: str) -> dict:
    """
    Główny pipeline orkiestracji:
    architect → code-writer → code-reviewer (loop) → tester
    """

    print("=" * 60)
    print(f"🚀 STARTING PIPELINE: {task_description}")
    print("=" * 60)

    results = {
        "task": task_description,
        "phases": []
    }

    # ─────────────────────────────────────────────────────────────
    # FAZA 1: ARCHITECT
    # ─────────────────────────────────────────────────────────────
    arch_result = await architect_phase(task_description)
    results["phases"].append(arch_result)
    design = arch_result["design"]

    # ─────────────────────────────────────────────────────────────
    # FAZA 2-3: CODE-WRITER + CODE-REVIEWER (PĘTLA)
    # ─────────────────────────────────────────────────────────────
    feedback = ""
    code = ""

    for iteration in range(MAX_REVIEW_ITERATIONS):
        print(f"\n--- Iteration {iteration + 1}/{MAX_REVIEW_ITERATIONS} ---")

        # Code-writer
        write_result = await code_writer_phase(design, feedback)
        results["phases"].append(write_result)
        code = write_result["code"]

        # Code-reviewer
        review_result = await code_reviewer_phase(code, design)
        results["phases"].append(review_result)

        if review_result["decision"] == "APPROVE":
            print("✅ Code APPROVED!")
            break
        else:
            print(f"❌ Code REJECTED: {review_result['issues']}")
            feedback = review_result["feedback"]

            if iteration == MAX_REVIEW_ITERATIONS - 1:
                print("⚠️  Max iterations reached, proceeding anyway...")

    # ─────────────────────────────────────────────────────────────
    # FAZA 4: TESTER
    # ─────────────────────────────────────────────────────────────
    test_result = await tester_phase(code)
    results["phases"].append(test_result)

    # ─────────────────────────────────────────────────────────────
    # PODSUMOWANIE
    # ─────────────────────────────────────────────────────────────
    print("\n" + "=" * 60)
    print("📊 PIPELINE COMPLETE")
    print("=" * 60)
    print(f"Phases executed: {len(results['phases'])}")
    print(f"Final review: {review_result['decision']}")
    print(f"Tests: {test_result['report'][:100]}...")

    return results


# ══════════════════════════════════════════════════════════════════
# ENTRY POINT
# ══════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    # Przykład użycia
    task = "Add a statistics panel that shows word count and character count"

    anyio.run(lambda: run_pipeline(task))
```

### 14.4 Uproszczony Flow dla Testów

**Aby przetestować koncept TERAZ, bez pełnego SDK:**

```
┌─────────────────────────────────────────────────────────────┐
│  TEST FLOW (w Claude Code CLI)                               │
│                                                              │
│  User: "Use architect subagent to design stats panel"        │
│        ↓                                                     │
│  Claude: [deleguje do architect]                             │
│        ↓                                                     │
│  User: "Now use code-writer to implement it"                 │
│        ↓                                                     │
│  Claude: [deleguje do code-writer]                           │
│        ↓                                                     │
│  User: "Use code-reviewer to check the code"                 │
│        ↓                                                     │
│  Claude: [deleguje do code-reviewer]                         │
│        ↓                                                     │
│  [JEŚLI REJECT] User: "Fix the issues and review again"      │
│        ↓                                                     │
│  [JEŚLI APPROVE] User: "Use tester to run tests"             │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 14.5 Następne Kroki

| Krok | Opis | Priorytet |
|------|------|-----------|
| 1 | Utworzyć 4 podstawowych subagentów (.claude/agents/) | 🔴 WYSOKI |
| 2 | Przetestować ręczne chainowanie w CLI | 🔴 WYSOKI |
| 3 | Zainstalować `claude-agent-sdk` i przetestować | 🟡 ŚREDNI |
| 4 | Napisać minimalny orchestrator.py | 🟡 ŚREDNI |
| 5 | Dodać pętle i automatyzację | 🟢 NISKI |

---

## 15. Decyzje do Podjęcia

### 15.1 Czy SDK jest potrzebne TERAZ?

**Argumenty ZA SDK:**
- Pełna kontrola nad flow
- Automatyczne pętle (review → fix → review)
- Programowalna logika decyzyjna
- Możliwość integracji z CI/CD

**Argumenty PRZECIW SDK (na razie):**
- Można testować ręcznie w CLI
- Subagenci w .claude/agents/ już działają
- SDK wymaga dodatkowego kodu i utrzymania
- Lepiej najpierw ustabilizować workflow

**REKOMENDACJA:**
1. **TERAZ:** Wdrożyć subagentów (.claude/agents/) + testować ręcznie
2. **ZA 1-2 TYGODNIE:** Gdy workflow stabilny → dodać SDK orchestrator

### 15.2 Który Trigger dla Automatycznego Architekt?

**Opcja A:** Po zatwierdzeniu planu OpenSpec
```
User: "plan zatwierdzony"
→ Automatycznie: architect subagent
```

**Opcja B:** Hook na zmianę statusu OpenSpec
```
PreToolUse(Write) → jeśli proposal.md status=IN_PROGRESS → spawn architect
```

**Opcja C:** Jawne wyzwolenie
```
User: "zaprojektuj rozwiązanie"
→ architect subagent
```

**REKOMENDACJA:** Opcja C (jawne) na start, Opcja A (automatyczne) po ustabilizowaniu.

---

## 16. Workflow Engine - Profesjonalna Architektura (2025-11-29)

### 16.1 Cel i Założenia

**Cel:** Stworzenie deklaratywnego systemu orkiestracji agentów opartego na JSON.

**Kluczowe założenia:**
1. **Deklaratywność** - definiujesz CO ma się stać, nie JAK
2. **Konfigurowalność** - zmiany w JSON bez modyfikacji Python
3. **Deterministyczność** - jasny protokół statusów
4. **Elastyczność** - fallback patterns gdy agent nie współpracuje
5. **Bezpieczeństwo** - limity, loop detection, graceful fallbacks

### 16.2 Protokół Statusów

#### Standaryzowany blok statusu

Każdy agent kończy odpowiedź jawnym blokiem:

```
Agent output...
...
═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: OpenSpec #00028 created
next_hint: architect should analyze
═══════════════════════════════════════════════════════════════
```

**Dlaczego tak?**
- Łatwe do parsowania (regex na `\[WORKFLOW_STATUS\]`)
- Oddzielone od treści - zero false positives
- Zawiera kontekst do przekazania dalej
- Agent jawnie deklaruje swój stan

#### Cztery uniwersalne stany

| Status | Znaczenie | Akcja orchestratora |
|--------|-----------|---------------------|
| `READY` | Sukces, można kontynuować | Wykonaj następną regułę |
| `BLOCKED` | Problem wymagający naprawy | Uruchom agenta naprawczego |
| `FAILED` | Błąd krytyczny | Stop + raport |
| `DECISION_NEEDED` | Wybór użytkownika | Pokaż opcje |

### 16.3 Fallback Patterns (Opcja B)

Gdy agent **nie wyemituje** bloku statusu, orchestrator szuka wzorców w ostatnich liniach:

```json
{
  "fallback_patterns": {
    "READY": "READY|DONE|COMPLETE|PASS|APPROVE|SUCCESS|CREATED|DEPLOYED",
    "BLOCKED": "BLOCKED|CHANGES|REJECT|NEED|REQUEST_CHANGES|ISSUES|MISSING",
    "FAILED": "FAIL|ERROR|ABORT|CRITICAL|CANNOT|FATAL"
  },
  "pattern_priority": ["FAILED", "BLOCKED", "READY"],
  "fallback_search_lines": 10,
  "on_no_match": "ask_user"
}
```

**Priorytet parsowania:** FAILED > BLOCKED > READY (pesymistyczne założenie)

### 16.4 Strategia Parsowania

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        OUTPUT PARSING PIPELINE                               │
│                                                                              │
│  Agent Output                                                                │
│       │                                                                      │
│       ▼                                                                      │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ STEP 1: Szukaj [WORKFLOW_STATUS] block                              │    │
│  │         Regex: \[WORKFLOW_STATUS\][\s\S]*?status:\s*(\w+)           │    │
│  └──────────────────────────┬──────────────────────────────────────────┘    │
│                             │                                                │
│              ┌──────────────┴──────────────┐                                │
│              │                             │                                │
│              ▼                             ▼                                │
│         [FOUND]                      [NOT FOUND]                            │
│              │                             │                                │
│              ▼                             ▼                                │
│  ┌─────────────────────┐    ┌─────────────────────────────────────────┐    │
│  │ Parse:              │    │ STEP 2: Fallback - szukaj patterns      │    │
│  │ - status            │    │         w ostatnich N liniach           │    │
│  │ - context           │    │                                         │    │
│  │ - next_hint         │    │  for pattern in [FAILED, BLOCKED, READY]:│   │
│  └──────────┬──────────┘    │    if regex.search(pattern, last_lines): │   │
│             │               │      return status                       │    │
│             │               └──────────────────────┬──────────────────┘    │
│             │                                      │                        │
│             │                       ┌──────────────┴──────────────┐         │
│             │                       │                             │         │
│             │                       ▼                             ▼         │
│             │                  [MATCHED]                    [NO MATCH]      │
│             │                       │                             │         │
│             │                       │                             ▼         │
│             │                       │               ┌─────────────────────┐ │
│             │                       │               │ STEP 3: ask_user    │ │
│             │                       │               │ "What should we do?"│ │
│             │                       │               └─────────────────────┘ │
│             │                       │                                       │
│             └───────────────────────┴───────────────────────────────────────│
│                                     │                                       │
│                                     ▼                                       │
│                          ┌─────────────────────┐                           │
│                          │   WorkflowStatus    │                           │
│                          │   - status          │                           │
│                          │   - context         │                           │
│                          │   - next_hint       │                           │
│                          └─────────────────────┘                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 16.5 Kompletna Konfiguracja workflow.json

```json
{
  "workflow": {
    "name": "kalahari-standard",
    "version": "1.0",

    "protocol": {
      "status_block_marker": "[WORKFLOW_STATUS]",
      "valid_statuses": ["READY", "BLOCKED", "FAILED", "DECISION_NEEDED"],

      "fallback_patterns": {
        "READY": "READY|DONE|COMPLETE|PASS|APPROVE|SUCCESS|CREATED|DEPLOYED",
        "BLOCKED": "BLOCKED|CHANGES|REJECT|NEED|REQUEST_CHANGES|ISSUES|MISSING",
        "FAILED": "FAIL|ERROR|ABORT|CRITICAL|CANNOT|FATAL"
      },
      "pattern_priority": ["FAILED", "BLOCKED", "READY"],
      "fallback_search_lines": 10,
      "on_unknown_status": "ask_user"
    },

    "limits": {
      "max_workflow_iterations": 20,
      "max_retries_per_rule": 3,
      "agent_timeout_seconds": 300
    },

    "prompt_injection": {
      "enabled": true,
      "template": "STATUS_PROTOCOL_V1"
    },

    "rules": [
      {
        "id": "initial",
        "trigger": { "type": "start" },
        "action": {
          "agent": "task-manager",
          "prompt_mode": "passthrough"
        }
      },

      {
        "id": "task_to_architect",
        "description": "After task created, run architect",
        "trigger": {
          "agent": "task-manager",
          "status": "READY",
          "context_excludes": "DEPLOYED|closed|complete"
        },
        "action": {
          "agent": "architect",
          "prompt": "Analyze and design solution based on the created OpenSpec. Context: {context}"
        }
      },

      {
        "id": "architect_decision",
        "trigger": {
          "agent": "architect",
          "status": "READY"
        },
        "action": {
          "type": "decision",
          "message": "Design complete. Choose implementation approach:",
          "options": [
            {"key": "1", "label": "New files (code-writer)", "agent": "code-writer"},
            {"key": "2", "label": "Modify existing (code-editor)", "agent": "code-editor"},
            {"key": "3", "label": "UI component (ui-designer)", "agent": "ui-designer"}
          ],
          "prompt_template": "Implement according to design. Context: {context}"
        }
      },

      {
        "id": "implementation_to_review",
        "trigger": {
          "agent": ["code-writer", "code-editor", "ui-designer"],
          "status": "READY"
        },
        "action": {
          "agent": "code-reviewer",
          "prompt": "Review the implemented changes"
        }
      },

      {
        "id": "review_blocked_loop",
        "trigger": {
          "agent": "code-reviewer",
          "status": "BLOCKED"
        },
        "action": {
          "agent": "code-editor",
          "prompt": "Fix the issues found in review: {context}",
          "on_complete": "implementation_to_review"
        },
        "retry": {
          "max": 3,
          "on_exhausted": {
            "type": "ask_user",
            "message": "Review loop exceeded 3 attempts. Manual intervention needed."
          }
        }
      },

      {
        "id": "review_to_tests",
        "trigger": {
          "agent": "code-reviewer",
          "status": "READY"
        },
        "action": {
          "agent": "tester",
          "prompt": "Run build and tests"
        }
      },

      {
        "id": "tests_blocked_loop",
        "trigger": {
          "agent": "tester",
          "status": "BLOCKED"
        },
        "action": {
          "agent": "code-editor",
          "prompt": "Fix failing tests: {context}",
          "on_complete": "review_to_tests"
        },
        "retry": {
          "max": 3,
          "on_exhausted": {
            "type": "ask_user",
            "message": "Test fix loop exceeded 3 attempts."
          }
        }
      },

      {
        "id": "tests_to_close",
        "trigger": {
          "agent": "tester",
          "status": "READY"
        },
        "action": {
          "agent": "task-manager",
          "prompt": "Close the task - all checks passed"
        }
      },

      {
        "id": "workflow_complete",
        "description": "Task closed, workflow complete",
        "trigger": {
          "agent": "task-manager",
          "status": "READY",
          "context_contains": "DEPLOYED|closed|complete"
        },
        "action": {
          "type": "complete",
          "message": "Workflow completed successfully!"
        }
      }
    ]
  }
}
```

### 16.6 Protokół Injekcji do Promptów

Orchestrator automatycznie dodaje do każdego prompta:

```
───────────────────────────────────────────────────────────────
[WORKFLOW_PROTOCOL]

When you complete your task, END your response with this block:

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: <STATUS>
context: <brief outcome description>
next_hint: <suggested next step>
═══════════════════════════════════════════════════════════════

Valid STATUS values:
• READY           - Task completed successfully
• BLOCKED         - Issue found, needs fixing (describe in context)
• FAILED          - Critical error, cannot proceed
• DECISION_NEEDED - Multiple paths, user must choose

Example:
═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: READY
context: OpenSpec #00028 created successfully
next_hint: architect should design solution
═══════════════════════════════════════════════════════════════
───────────────────────────────────────────────────────────────
```

### 16.7 Architektura Kodu

```
.claude/
├── workflow.json                    # Konfiguracja reguł
└── orchestrator/
    ├── __init__.py
    ├── main.py                      # Entry point
    ├── config.py                    # Load & validate workflow.json
    ├── protocol.py                  # Status parsing & prompt injection
    ├── engine.py                    # Rule matching
    ├── runner.py                    # Agent execution (SDK)
    ├── state.py                     # Workflow state & history
    └── ui.py                        # User interaction (decisions, fallbacks)
```

### 16.8 Kluczowe Klasy

#### protocol.py

```python
from dataclasses import dataclass
from typing import Literal

@dataclass
class WorkflowStatus:
    status: Literal["READY", "BLOCKED", "FAILED", "DECISION_NEEDED", "UNKNOWN"]
    context: str = ""
    next_hint: str = ""
    source: Literal["explicit", "fallback", "user"] = "explicit"

class StatusParser:
    """Parse agent output to extract workflow status."""

    def __init__(self, protocol_config: dict):
        self.marker = protocol_config["status_block_marker"]
        self.fallback_patterns = protocol_config["fallback_patterns"]
        self.pattern_priority = protocol_config["pattern_priority"]
        self.search_lines = protocol_config["fallback_search_lines"]

    def parse(self, output: str) -> WorkflowStatus:
        # 1. Try explicit [WORKFLOW_STATUS] block
        explicit = self._parse_explicit_block(output)
        if explicit:
            return explicit

        # 2. Fallback to pattern matching
        fallback = self._parse_fallback_patterns(output)
        if fallback:
            return fallback

        # 3. Return UNKNOWN
        return WorkflowStatus(status="UNKNOWN", source="fallback")

    def _parse_explicit_block(self, output: str) -> Optional[WorkflowStatus]:
        pattern = rf"\[WORKFLOW_STATUS\][\s\S]*?status:\s*(\w+)(?:[\s\S]*?context:\s*(.+?))?(?:[\s\S]*?next_hint:\s*(.+?))?"
        match = re.search(pattern, output, re.IGNORECASE)
        if match:
            return WorkflowStatus(
                status=match.group(1).upper(),
                context=match.group(2) or "",
                next_hint=match.group(3) or "",
                source="explicit"
            )
        return None

    def _parse_fallback_patterns(self, output: str) -> Optional[WorkflowStatus]:
        last_lines = "\n".join(output.strip().split("\n")[-self.search_lines:])

        for status in self.pattern_priority:
            pattern = self.fallback_patterns.get(status, "")
            if pattern and re.search(pattern, last_lines, re.IGNORECASE):
                return WorkflowStatus(status=status, source="fallback")

        return None

class PromptInjector:
    """Inject workflow protocol into agent prompts."""

    PROTOCOL_TEMPLATE = '''
───────────────────────────────────────────────────────────────
[WORKFLOW_PROTOCOL]

When you complete your task, END your response with this block:

═══════════════════════════════════════════════════════════════
[WORKFLOW_STATUS]
status: <STATUS>
context: <brief outcome description>
next_hint: <suggested next step>
═══════════════════════════════════════════════════════════════

Valid STATUS values:
• READY           - Task completed successfully
• BLOCKED         - Issue found, needs fixing (describe in context)
• FAILED          - Critical error, cannot proceed
• DECISION_NEEDED - Multiple paths, user must choose
───────────────────────────────────────────────────────────────
'''

    def inject(self, prompt: str) -> str:
        return prompt + self.PROTOCOL_TEMPLATE
```

#### engine.py

```python
from dataclasses import dataclass
from typing import Optional, List

@dataclass
class RuleMatch:
    rule: dict
    captured: dict = None  # From context_contains regex

class RuleEngine:
    """Match rules based on agent and status."""

    def __init__(self, rules: List[dict]):
        self.rules = rules

    def find_initial(self) -> Optional[dict]:
        """Find rule with trigger.type == 'start'"""
        for rule in self.rules:
            if rule.get("trigger", {}).get("type") == "start":
                return rule
        return None

    def match(self, agent: str, status: WorkflowStatus) -> Optional[RuleMatch]:
        """Find matching rule for agent + status."""
        for rule in self.rules:
            trigger = rule.get("trigger", {})

            # Check agent match
            trigger_agent = trigger.get("agent")
            if trigger_agent:
                if isinstance(trigger_agent, list):
                    if agent not in trigger_agent:
                        continue
                elif trigger_agent != agent:
                    continue

            # Check status match
            trigger_status = trigger.get("status")
            if trigger_status and trigger_status != status.status:
                continue

            # Check context_contains (optional)
            context_pattern = trigger.get("context_contains")
            captured = {}
            if context_pattern:
                match = re.search(context_pattern, status.context, re.IGNORECASE)
                if not match:
                    continue
                captured = {"match": match}

            return RuleMatch(rule=rule, captured=captured)

        return None
```

#### state.py

```python
from dataclasses import dataclass, field
from typing import List, Dict, Optional
from datetime import datetime

@dataclass
class ExecutionRecord:
    agent: str
    prompt: str
    status: WorkflowStatus
    timestamp: datetime
    duration_seconds: float

class WorkflowState:
    """Track workflow execution state."""

    def __init__(self, limits: dict):
        self.limits = limits
        self.iteration: int = 0
        self.history: List[ExecutionRecord] = []
        self.retry_counts: Dict[str, int] = {}  # rule_id -> count
        self.complete: bool = False

    def record(self, agent: str, prompt: str, status: WorkflowStatus, duration: float):
        self.history.append(ExecutionRecord(
            agent=agent,
            prompt=prompt,
            status=status,
            timestamp=datetime.now(),
            duration_seconds=duration
        ))
        self.iteration += 1

    def increment_retry(self, rule_id: str):
        self.retry_counts[rule_id] = self.retry_counts.get(rule_id, 0) + 1

    def can_retry(self, rule_id: str, max_retries: int) -> bool:
        return self.retry_counts.get(rule_id, 0) < max_retries

    def is_at_limit(self) -> bool:
        return self.iteration >= self.limits["max_workflow_iterations"]

    def is_in_loop(self, lookback: int = 5) -> bool:
        """Detect if we're stuck in a loop."""
        if len(self.history) < lookback:
            return False

        recent = [r.agent for r in self.history[-lookback:]]
        # If same 2-agent pattern repeats, we're in a loop
        if len(recent) >= 4:
            if recent[-1] == recent[-3] and recent[-2] == recent[-4]:
                return True

        return False

    def summary(self) -> str:
        agents_used = list(set(r.agent for r in self.history))
        total_time = sum(r.duration_seconds for r in self.history)
        final_status = self.history[-1].status.status if self.history else "N/A"

        return f"""
═══════════════════════════════════════════════════════════════
                    WORKFLOW SUMMARY
═══════════════════════════════════════════════════════════════
Iterations:    {self.iteration}
Agents used:   {', '.join(agents_used)}
Total time:    {total_time:.1f}s
Final status:  {final_status}
Complete:      {self.complete}
═══════════════════════════════════════════════════════════════
"""
```

### 16.9 Główny Flow Orchestratora

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           ORCHESTRATOR MAIN LOOP                             │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         INITIALIZATION                                │   │
│  │  1. Load workflow.json                                               │   │
│  │  2. Initialize StatusParser, RuleEngine, Runner, State               │   │
│  │  3. Find initial rule (trigger.type == "start")                      │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│                                    ▼                                         │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                          MAIN LOOP                                    │   │
│  │                                                                       │   │
│  │   while not complete and iteration < max:                            │   │
│  │       │                                                              │   │
│  │       ▼                                                              │   │
│  │   ┌─────────────────────────────────────────────────────────────┐   │   │
│  │   │ 1. PREPARE PROMPT                                           │   │   │
│  │   │    prompt = injector.inject(rule.action.prompt)             │   │   │
│  │   │    prompt = prompt.replace("{context}", last_context)       │   │   │
│  │   └─────────────────────────────────────────────────────────────┘   │   │
│  │       │                                                              │   │
│  │       ▼                                                              │   │
│  │   ┌─────────────────────────────────────────────────────────────┐   │   │
│  │   │ 2. EXECUTE AGENT                                            │   │   │
│  │   │    output = await runner.run(agent, prompt)                 │   │   │
│  │   └─────────────────────────────────────────────────────────────┘   │   │
│  │       │                                                              │   │
│  │       ▼                                                              │   │
│  │   ┌─────────────────────────────────────────────────────────────┐   │   │
│  │   │ 3. PARSE STATUS                                             │   │   │
│  │   │    status = parser.parse(output)                            │   │   │
│  │   │    state.record(agent, status)                              │   │   │
│  │   └─────────────────────────────────────────────────────────────┘   │   │
│  │       │                                                              │   │
│  │       ▼                                                              │   │
│  │   ┌─────────────────────────────────────────────────────────────┐   │   │
│  │   │ 4. HANDLE STATUS                                            │   │   │
│  │   │                                                             │   │   │
│  │   │    FAILED ──────────► STOP + report error                   │   │   │
│  │   │                                                             │   │   │
│  │   │    UNKNOWN ─────────► ask_user() for next action            │   │   │
│  │   │                                                             │   │   │
│  │   │    DECISION_NEEDED ─► show_options() from rule              │   │   │
│  │   │                                                             │   │   │
│  │   │    READY/BLOCKED ───► continue to step 5                    │   │   │
│  │   └─────────────────────────────────────────────────────────────┘   │   │
│  │       │                                                              │   │
│  │       ▼                                                              │   │
│  │   ┌─────────────────────────────────────────────────────────────┐   │   │
│  │   │ 5. MATCH NEXT RULE                                          │   │   │
│  │   │    match = engine.match(agent, status)                      │   │   │
│  │   │                                                             │   │   │
│  │   │    NO MATCH ────────► ask_user() for next action            │   │   │
│  │   │                                                             │   │   │
│  │   │    MATCH + retry exhausted ──► ask_user()                   │   │   │
│  │   │                                                             │   │   │
│  │   │    MATCH + action.type == "complete" ──► STOP (success)     │   │   │
│  │   │                                                             │   │   │
│  │   │    MATCH + action.type == "decision" ──► show_options()     │   │   │
│  │   │                                                             │   │   │
│  │   │    MATCH ───────────► set next agent & prompt, continue     │   │   │
│  │   └─────────────────────────────────────────────────────────────┘   │   │
│  │                                                                       │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│                                    ▼                                         │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                          COMPLETION                                   │   │
│  │  - Print summary (iterations, agents used, final status)            │   │
│  │  - Save execution log to .claude/logs/workflow-{timestamp}.json     │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 16.10 Diagram Przepływu Reguł

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         WORKFLOW RULES FLOW                                  │
│                                                                              │
│  START                                                                       │
│    │                                                                         │
│    ▼                                                                         │
│  ┌──────────────────┐                                                       │
│  │   task-manager   │ ◄─────────────────────────────────────────────────┐   │
│  │                  │                                                    │   │
│  │  "nowe zadanie"  │                                                    │   │
│  └────────┬─────────┘                                                    │   │
│           │                                                              │   │
│           │ READY + "OpenSpec #"                                         │   │
│           ▼                                                              │   │
│  ┌──────────────────┐                                                    │   │
│  │    architect     │                                                    │   │
│  │                  │                                                    │   │
│  │  "zaprojektuj"   │                                                    │   │
│  └────────┬─────────┘                                                    │   │
│           │                                                              │   │
│           │ READY                                                        │   │
│           ▼                                                              │   │
│  ┌──────────────────────────────────────────────────────────────────┐   │   │
│  │                      USER DECISION                                │   │   │
│  │                                                                   │   │   │
│  │   [1] New files      [2] Modify existing     [3] UI component    │   │   │
│  │       │                     │                       │            │   │   │
│  │       ▼                     ▼                       ▼            │   │   │
│  │   code-writer         code-editor              ui-designer       │   │   │
│  └───────┬─────────────────────┬───────────────────────┬────────────┘   │   │
│          │                     │                       │                │   │
│          └─────────────────────┼───────────────────────┘                │   │
│                                │                                        │   │
│                                │ READY                                  │   │
│                                ▼                                        │   │
│  ┌──────────────────────────────────────────────────────────────────┐   │   │
│  │                      code-reviewer                                │   │   │
│  │                                                                   │   │   │
│  │   READY ─────────────────────────┐                               │   │   │
│  │                                  │                               │   │   │
│  │   BLOCKED ───┐                   │                               │   │   │
│  └──────────────┼───────────────────┼───────────────────────────────┘   │   │
│                 │                   │                                   │   │
│                 │ (max 3x)          │                                   │   │
│                 ▼                   │                                   │   │
│  ┌──────────────────┐               │                                   │   │
│  │   code-editor    │               │                                   │   │
│  │   (fix issues)   │───────────────┤                                   │   │
│  └──────────────────┘               │                                   │   │
│         ▲                           │                                   │   │
│         │                           │                                   │   │
│         │ (loop back)               │                                   │   │
│         │                           │                                   │   │
│         │                           ▼                                   │   │
│         │              ┌──────────────────┐                             │   │
│         │              │     tester       │                             │   │
│         │              │                  │                             │   │
│         │              │  READY ──────────┼─────────────────────┐       │   │
│         │              │                  │                     │       │   │
│         │              │  BLOCKED ────────┼──┐                  │       │   │
│         │              └──────────────────┘  │                  │       │   │
│         │                                    │ (max 3x)         │       │   │
│         │                                    ▼                  │       │   │
│         │              ┌──────────────────┐                     │       │   │
│         └──────────────│   code-editor    │                     │       │   │
│                        │   (fix tests)    │                     │       │   │
│                        └──────────────────┘                     │       │   │
│                                                                 │       │   │
│                                                                 │       │   │
│                                                                 ▼       │   │
│  ┌──────────────────────────────────────────────────────────────────┐   │   │
│  │                      task-manager                                 │   │   │
│  │                      "zamknij task"                               │   │   │
│  │                                                                   │   │   │
│  │   READY + "DEPLOYED" ─────────────────────────────────────────────┼───┘   │
│  │                                                                   │       │
│  └───────────────────────────────────────────────────────────────────┘       │
│                                    │                                         │
│                                    ▼                                         │
│                           ┌──────────────┐                                  │
│                           │   COMPLETE   │                                  │
│                           │      ✅      │                                  │
│                           └──────────────┘                                  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 16.11 Obsługa Edge Cases

| Sytuacja | Rozwiązanie |
|----------|-------------|
| Agent nie wyemituje statusu | Fallback patterns → ask_user |
| Agent wyemituje błędny status | Validate against `valid_statuses`, treat as UNKNOWN |
| Pętla nieskończona | `max_workflow_iterations` + loop detection |
| Retry się zapętla | `max_retries_per_rule` → ask_user |
| Agent timeout | Catch exception → status = FAILED |
| User chce przerwać | Ctrl+C → graceful shutdown + save state |
| Brak reguły dla statusu | ask_user z listą dostępnych agentów |

### 16.12 Plan Implementacji (STATUS: COMPLETED 2025-11-29)

| Krok | Opis | Status |
|------|------|--------|
| 1 | Utworzyć `.claude/workflow.json` z pełną konfiguracją | ✅ DONE |
| 2 | Utworzyć `orchestrator/protocol.py` (parsing) | ✅ DONE |
| 3 | Utworzyć `orchestrator/engine.py` (rule matching) | ✅ DONE |
| 4 | Utworzyć `orchestrator/state.py` (state tracking) | ✅ DONE |
| 5 | Utworzyć `orchestrator/runner.py` (agent execution) | ✅ DONE |
| 6 | Utworzyć `orchestrator/ui.py` (user interaction) | ✅ DONE |
| 7 | Utworzyć `orchestrator/main.py` (entry point) | ✅ DONE |
| 8 | Test w mock mode (bez SDK) | ✅ DONE |
| 9 | Integracja z `claude-agent-sdk` | ✅ DONE |
| 10 | Slash commands `/workflow` i `/workflow-mock` | ✅ DONE |

### 16.13 Trzy Sposoby Pracy

Workflow Engine oferuje **trzy sposoby pracy** z agentami:

#### Sposób 1: Bez orkiestratora (ręczne triggery)

Piszesz w chacie Claude Code i używasz triggerów z CLAUDE.md:

```
# Nowe zadanie
"nowe zadanie - panel statystyk"     → uruchomi task-manager (MODE 1: create)

# Kontynuacja istniejącego zadania
"kontynuuj task 00027"               → uruchomi task-manager (MODE 2: continue)
"wznów"                              → uruchomi task-manager (MODE 2: continue)

# Pozostałe kroki
"zaprojektuj to"                     → uruchomi architect
"napisz kod"                         → uruchomi code-writer
"review"                             → uruchomi code-reviewer
"testy"                              → uruchomi tester
```

**Zalety:**
- Pełna kontrola nad kolejnością
- Możesz pominąć/dodać kroki
- Możesz iterować na pojedynczym agencie

**Wady:**
- Musisz pamiętać o wszystkich krokach
- Ręczne przekazywanie kontekstu

#### Sposób 2: Z orkiestratorem (slash command)

```
/workflow nowe zadanie - panel statystyk
```

Orkiestrator automatycznie przeprowadzi cały flow:
```
task-manager → architect → (wybór) → code-reviewer → tester → task-manager → COMPLETE
```

**Zalety:**
- Automatyczne przejścia między agentami
- Spójny proces dla każdego zadania
- Retry loops dla błędów
- Log wykonania (JSON)

**Wady:**
- Mniejsza kontrola
- Koszt API za cały flow

#### Sposób 3: Orkiestrator w trybie testowym

```
/workflow-mock test zadania
```

Symuluje flow bez wywoływania Claude (do testowania logiki).

#### Podsumowanie

| Sposób | Komenda | Kontrola | Koszt API |
|--------|---------|----------|-----------|
| Ręczny | triggery w chacie | Pełna | Tylko użyte agenty |
| Orkiestrator | `/workflow zadanie` | Automatyczna + decyzje | Cały flow |
| Mock | `/workflow-mock zadanie` | Test flow | Brak |

---

### 16.14 Struktura Plików Orkiestratora

```
.claude/
├── workflow.json              # Konfiguracja reguł workflow
├── orchestrator/
│   ├── __init__.py           # Eksporty pakietu
│   ├── protocol.py           # StatusParser, PromptInjector, WorkflowStatus
│   ├── engine.py             # RuleEngine, RuleMatch
│   ├── state.py              # WorkflowState, ExecutionRecord
│   ├── runner.py             # AgentRunner (mock + SDK)
│   ├── ui.py                 # WorkflowUI (terminal interface)
│   └── main.py               # WorkflowOrchestrator, CLI entry point
├── commands/
│   ├── workflow.md           # Slash command /workflow
│   └── workflow-mock.md      # Slash command /workflow-mock
└── logs/
    └── workflow-*.json       # Logi wykonania (auto-generowane)
```

---

### 16.15 Pełna Dokumentacja Konfiguracji JSON

Plik `.claude/workflow.json` definiuje całe zachowanie orkiestratora.

#### 16.15.1 Struktura Główna

```json
{
  "workflow": {
    "name": "string",           // Nazwa workflow
    "version": "string",        // Wersja konfiguracji
    "protocol": { ... },        // Konfiguracja protokołu statusów
    "limits": { ... },          // Limity bezpieczeństwa
    "prompt_injection": { ... }, // Konfiguracja injekcji protokołu
    "rules": [ ... ]            // Lista reguł przejść
  }
}
```

#### 16.15.2 Sekcja `protocol`

Definiuje jak parsować statusy z outputu agentów.

```json
{
  "protocol": {
    "status_block_marker": "[WORKFLOW_STATUS]",
    "valid_statuses": ["READY", "BLOCKED", "FAILED", "DECISION_NEEDED"],
    "fallback_patterns": {
      "READY": "READY|DONE|COMPLETE|PASS|APPROVE|SUCCESS|CREATED|DEPLOYED",
      "BLOCKED": "BLOCKED|CHANGES|REJECT|NEED|REQUEST_CHANGES|ISSUES|MISSING",
      "FAILED": "FAIL|ERROR|ABORT|CRITICAL|CANNOT|FATAL"
    },
    "pattern_priority": ["FAILED", "BLOCKED", "READY"],
    "fallback_search_lines": 10,
    "on_unknown_status": "ask_user"
  }
}
```

| Pole | Typ | Opis |
|------|-----|------|
| `status_block_marker` | string | Marker bloku statusu (regex-safe) |
| `valid_statuses` | string[] | Lista dozwolonych statusów |
| `fallback_patterns` | object | Regex patterns gdy brak explicit block |
| `pattern_priority` | string[] | Kolejność sprawdzania (pesymistyczna) |
| `fallback_search_lines` | int | Ile linii od końca przeszukać |
| `on_unknown_status` | string | Co robić gdy status nieznany: `"ask_user"` lub `"fail"` |

#### 16.15.3 Sekcja `limits`

Limity bezpieczeństwa zapobiegające nieskończonym pętlom.

```json
{
  "limits": {
    "max_workflow_iterations": 20,
    "max_retries_per_rule": 3,
    "agent_timeout_seconds": 300
  }
}
```

| Pole | Typ | Domyślnie | Opis |
|------|-----|-----------|------|
| `max_workflow_iterations` | int | 20 | Max iteracji całego workflow |
| `max_retries_per_rule` | int | 3 | Max retry dla pojedynczej reguły |
| `agent_timeout_seconds` | int | 300 | Timeout pojedynczego agenta (5 min) |

#### 16.15.4 Sekcja `prompt_injection`

Kontroluje dodawanie protokołu statusów do promptów.

```json
{
  "prompt_injection": {
    "enabled": true,
    "template": "STATUS_PROTOCOL_V1"
  }
}
```

| Pole | Typ | Opis |
|------|-----|------|
| `enabled` | bool | Czy dodawać protokół do promptów |
| `template` | string | Nazwa szablonu (aktualnie tylko `STATUS_PROTOCOL_V1`) |

#### 16.15.5 Sekcja `rules` - Format Reguł

Każda reguła definiuje: **KIEDY** (trigger) → **CO** (action).

##### Trigger - Warunki Dopasowania

```json
{
  "trigger": {
    "type": "start",              // Tylko dla initial rule
    "agent": "task-manager",      // string lub string[] dla wielu agentów
    "status": "READY",            // Wymagany status
    "context_contains": "regex",  // Regex pattern - musi matchować context
    "context_excludes": "regex"   // Regex pattern - NIE może matchować context
  }
}
```

| Pole | Typ | Wymagane | Opis |
|------|-----|----------|------|
| `type` | string | Dla initial | `"start"` - reguła startowa |
| `agent` | string/string[] | Nie | Agent(y) który musi zakończyć |
| `status` | string | Nie | Wymagany status (`READY`, `BLOCKED`, etc.) |
| `context_contains` | string | Nie | Regex - context MUSI matchować |
| `context_excludes` | string | Nie | Regex - context NIE MOŻE matchować |

**UWAGA:** `context_excludes` ma priorytet nad `context_contains`.

##### Action - Typy Akcji

**Typ 1: Uruchom agenta**
```json
{
  "action": {
    "agent": "architect",
    "prompt": "Design solution. Context: {context}",
    "prompt_mode": "passthrough"  // Opcjonalne - użyj original prompt
  }
}
```

**Typ 2: Decyzja użytkownika**
```json
{
  "action": {
    "type": "decision",
    "message": "Choose implementation approach:",
    "options": [
      {"key": "1", "label": "New files", "agent": "code-writer"},
      {"key": "2", "label": "Modify existing", "agent": "code-editor"},
      {"key": "3", "label": "UI component", "agent": "ui-designer"}
    ],
    "prompt_template": "Implement: {context}"
  }
}
```

**Typ 3: Zakończ workflow**
```json
{
  "action": {
    "type": "complete",
    "message": "Workflow completed successfully!"
  }
}
```

##### Retry - Konfiguracja Powtórzeń

```json
{
  "retry": {
    "max": 3,
    "on_exhausted": {
      "type": "ask_user",
      "message": "Retry limit exceeded. Manual intervention needed."
    }
  }
}
```

| Pole | Typ | Opis |
|------|-----|------|
| `max` | int | Max liczba powtórzeń |
| `on_exhausted.type` | string | `"ask_user"` lub `"fail"` |
| `on_exhausted.message` | string | Komunikat dla użytkownika |

#### 16.15.6 Zmienne w Promptach

W polach `prompt` i `prompt_template` można używać zmiennych:

| Zmienna | Opis | Przykład |
|---------|------|----------|
| `{context}` | Context z poprzedniego statusu | `"OpenSpec #00029 created"` |
| `{status}` | Status poprzedniego agenta | `"READY"` |
| `{agent}` | Nazwa poprzedniego agenta | `"task-manager"` |

#### 16.15.7 Przykład Kompletnej Reguły

```json
{
  "id": "review_blocked_loop",
  "description": "Review found issues, send back to fix",
  "trigger": {
    "agent": "code-reviewer",
    "status": "BLOCKED"
  },
  "action": {
    "agent": "code-editor",
    "prompt": "Fix issues from review: {context}",
    "on_complete": "implementation_to_review"
  },
  "retry": {
    "max": 3,
    "on_exhausted": {
      "type": "ask_user",
      "message": "Review loop exceeded 3 attempts."
    }
  }
}
```

---

### 16.16 Integracja z Claude Agent SDK

#### 16.16.1 Instalacja

```bash
pip install claude-agent-sdk
```

#### 16.16.2 Architektura SDK

```
┌─────────────────────────────────────────────────────────────┐
│                    WorkflowOrchestrator                     │
├─────────────────────────────────────────────────────────────┤
│  AgentRunner                                                │
│  ├── mock=True  → _mock_run() → symulowane odpowiedzi      │
│  └── mock=False → _sdk_run()  → claude_agent_sdk.query()   │
├─────────────────────────────────────────────────────────────┤
│  claude_agent_sdk                                           │
│  ├── query(prompt, options) → AsyncIterator[Message]       │
│  ├── ClaudeAgentOptions(cwd, permission_mode, allowed_tools)│
│  ├── AssistantMessage → .content → TextBlock[]             │
│  └── TextBlock → .text                                      │
└─────────────────────────────────────────────────────────────┘
```

#### 16.16.3 Kluczowy Kod (runner.py)

```python
from claude_agent_sdk import query, ClaudeAgentOptions, AssistantMessage, TextBlock

async def _sdk_run(self, agent: str, prompt: str) -> str:
    full_prompt = f"""Use the Task tool to spawn a '{agent}' subagent with this prompt:

{prompt}

IMPORTANT:
- Use subagent_type="{agent}"
- Wait for the agent to complete and return its full output
- The agent MUST end its response with a [WORKFLOW_STATUS] block"""

    options = ClaudeAgentOptions(
        cwd=str(self.project_dir),
        permission_mode="acceptEdits",
        allowed_tools=["Task", "Read", "Write", "Edit", "Bash", "Glob", "Grep"],
    )

    result_parts = []
    async for message in query(prompt=full_prompt, options=options):
        if isinstance(message, AssistantMessage):
            for block in message.content:
                if isinstance(block, TextBlock):
                    result_parts.append(block.text)

    return "\n".join(result_parts)
```

---

### 16.17 Naprawione Problemy (2025-11-29)

#### Problem 1: UnicodeEncodeError na Windows

**Błąd:**
```
UnicodeEncodeError: 'charmap' codec can't encode characters
```

**Przyczyna:** Znaki Unicode (═, ─, ✅, ❌, ⚠️, →) w terminalu Windows (cp1250).

**Rozwiązanie:** Zamiana na ASCII w `ui.py` i `state.py`:
- `═` → `=`
- `─` → `-`
- `✅` → `[OK]`
- `❌` → `[XX]`
- `⚠️` → `[!!]`
- `→` → `->`

#### Problem 2: workflow_complete nie matchowało

**Błąd:** Po zamknięciu taska z "DEPLOYED", matchowała reguła `task_to_architect` zamiast `workflow_complete`.

**Przyczyna:** `task_to_architect` matchowało pierwsze (bo sprawdzane wcześniej).

**Rozwiązanie:** Dodanie `context_excludes` do `engine.py`:
```json
{
  "id": "task_to_architect",
  "trigger": {
    "agent": "task-manager",
    "status": "READY",
    "context_excludes": "DEPLOYED|closed|complete"
  }
}
```

---

### 16.18 Użycie

#### Wiersz Poleceń

```bash
# SDK mode (produkcja)
cd .claude && python -m orchestrator.main "nowe zadanie - panel statystyk"

# Mock mode (testowanie)
cd .claude && python -m orchestrator.main --mock "nowe zadanie"

# Verbose mode
cd .claude && python -m orchestrator.main --verbose --mock "test"
```

#### Slash Commands w Claude Code

```
/workflow nowe zadanie - panel statystyk
/workflow-mock test przepływu
```

#### Argumenty CLI

| Argument | Opis |
|----------|------|
| `prompt` | Opis zadania (wymagany lub interaktywny) |
| `--mock` | Tryb testowy bez wywołań SDK |
| `--verbose`, `-v` | Szczegółowe logi |
| `--config PATH` | Alternatywna ścieżka do workflow.json |
| `--project-dir PATH` | Katalog projektu |

---

### 16.19 Wynik Testu Mock (2025-11-29)

```
=================================================================
  KALAHARI WORKFLOW ORCHESTRATOR (MOCK MODE)
=================================================================

-----------------------------------------------------------------
[>] Iteration 1: task-manager [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: OpenSpec #00029 created - panel statystyk

  [+] Matched rule: task_to_architect
    After task created, run architect

-----------------------------------------------------------------
[>] Iteration 2: architect [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: Design complete for StatsPanel

  [+] Matched rule: architect_decision
    After design, choose implementation approach

=================================================================
  [?] Design complete. Choose implementation approach:
-----------------------------------------------------------------
    [1] New files (code-writer)
        -> code-writer
    [2] Modify existing (code-editor)
        -> code-editor
    [3] UI component (ui-designer)
        -> ui-designer

    [q] Quit workflow
=================================================================

  Your choice: 3

-----------------------------------------------------------------
[>] Iteration 3: ui-designer [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: UI component created, build successful

  [+] Matched rule: implementation_to_review

-----------------------------------------------------------------
[>] Iteration 4: code-reviewer [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: Code review APPROVED

  [+] Matched rule: review_to_tests

-----------------------------------------------------------------
[>] Iteration 5: tester [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: All tests PASS

  [+] Matched rule: tests_to_close

-----------------------------------------------------------------
[>] Iteration 6: task-manager [MOCK]
-----------------------------------------------------------------
  [OK] Status: READY (via explicit_block)
     Context: Task closed - DEPLOYED

  [+] Matched rule: workflow_complete

=================================================================
  [OK] Workflow completed successfully!
=================================================================

=================================================================
                    WORKFLOW SUMMARY
=================================================================
  Status:      [OK] COMPLETE
  Iterations:  6
  Agents used: task-manager, architect, ui-designer, code-reviewer, tester
  Total time:  3.0s
  Last status: READY

  Execution trace:
    1. [+] task-manager -> READY (0.5s)
    2. [+] architect -> READY (0.5s)
    3. [+] ui-designer -> READY (0.5s)
    4. [+] code-reviewer -> READY (0.5s)
    5. [+] tester -> READY (0.5s)
    6. [+] task-manager -> READY (0.5s)
=================================================================

  Log saved to: .claude\logs\workflow-20251129_133513.json
```

---

### 16.20 Kontynuacja Istniejących Tasków (2025-11-29)

#### 16.20.1 Problem

Oryginalny workflow obsługiwał tylko **nowe taski**. Brakowało możliwości:
- Wczytania istniejącego OpenSpec
- Przeglądu i edycji specyfikacji przed implementacją
- Potwierdzenia, że specyfikacja jest kompletna

#### 16.20.2 Rozwiązanie

Dodano **MODE 2: CONTINUE EXISTING TASK** do task-manager:

```
"kontynuuj task 00027"  →  task-manager (MODE 2)
"continue task"         →  task-manager (MODE 2)
"wznów"                 →  task-manager (MODE 2)
```

#### 16.20.3 Flow Kontynuacji

```
┌─────────────────────────────────────────────────────────────┐
│                 CONTINUE TASK FLOW                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  User: "kontynuuj task 00027"                               │
│           │                                                 │
│           ▼                                                 │
│  ┌─────────────────────┐                                    │
│  │   task-manager      │  1. Load OpenSpec #00027           │
│  │   (MODE 2)          │  2. Display summary & tasks        │
│  └─────────┬───────────┘  3. Ask for confirmation           │
│            │                                                │
│            ▼                                                │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  "Is specification complete?"                        │    │
│  │                                                      │    │
│  │  [1] Yes, proceed to architect    → status: READY    │    │
│  │  [2] No, I need to modify         → status: DECISION │    │
│  │  [3] Cancel                       → stop             │    │
│  └─────────────────────────────────────────────────────┘    │
│            │                                                │
│      ┌─────┴─────┐                                          │
│      │           │                                          │
│  [1] ▼       [2] ▼                                          │
│  READY      DECISION_NEEDED                                 │
│    │            │                                           │
│    ▼            ▼                                           │
│  architect   task-manager (edit mode)                       │
│    │            │                                           │
│    ▼            └──► (loop back to confirmation)            │
│  (normal flow continues...)                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 16.20.4 Nowe Triggery w CLAUDE.md

```
| Triggers | Agent | Role |
|----------|-------|------|
| "kontynuuj task", "continue task", "wznów", ... | task-manager | Continue existing task |
```

#### 16.20.5 Nowe Reguły w workflow.json

```json
{
  "id": "task_needs_edit",
  "description": "User wants to edit specification",
  "trigger": {
    "agent": "task-manager",
    "status": "DECISION_NEEDED",
    "context_contains": "edit|modify|add|change|update spec"
  },
  "action": {
    "agent": "task-manager",
    "prompt": "User wants to modify the specification..."
  }
}
```

#### 16.20.6 Tryby Task Manager

| Mode | Trigger | Opis |
|------|---------|------|
| MODE 1 | "nowe zadanie" | Tworzy nowy OpenSpec |
| MODE 2 | "kontynuuj task" | Wczytuje istniejący, pyta o potwierdzenie |
| MODE 3 | "status" | Pokazuje postęp |
| MODE 4 | "zamknij task" | Weryfikuje i zamyka |

#### 16.20.7 Przykład Użycia

```
User: kontynuuj task 00027

task-manager:
═══════════════════════════════════════════════════════════════
📂 OpenSpec #00027: Theme Color Configuration
═══════════════════════════════════════════════════════════════

📋 SUMMARY:
Create a centralized UI for configuring all theme colors...

🎯 GOALS:
- Centralized color configuration in Appearance/Theme
- Per-theme storage in settings.json
- Reset to Theme Defaults functionality

📊 PROGRESS: 0/18 tasks completed

⏳ PENDING:
- [ ] Phase 1: Color Storage Architecture (3 tasks)
- [ ] Phase 2: ColorConfigWidget (3 tasks)
- [ ] Phase 3: Theme Color Editor UI (4 tasks)
- [ ] Phase 4: Reset to Defaults (3 tasks)
- [ ] Phase 5: Integration & Testing (5 tasks)

═══════════════════════════════════════════════════════════════

🔍 REVIEW SPECIFICATION:

Is the specification complete and ready for implementation?

[1] ✅ Yes, proceed to architect
[2] 📝 No, I need to add/modify requirements
[3] ❌ Cancel

User: 1

task-manager:
✅ OpenSpec #00027 ready to continue
📁 Location: openspec/changes/00027-theme-color-configuration/
📋 Next step: architect will review/update design

═══════════════════════════════════════════════════════════════
📋 NEXT STEPS - Choose one:
───────────────────────────────────────────────────────────────
▶ "zaprojektuj"                 → Architect reviews/creates design
▶ "status"                      → Check detailed progress
═══════════════════════════════════════════════════════════════
```

---

*Ten dokument jest TYLKO do planowania i NIE powinien być commitowany do git.*
