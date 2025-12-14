# 00030: Menu System Review & Cleanup

## Status
DEPLOYED

## Goal
Kompleksowy przegląd i uporządkowanie całej struktury menu aplikacji Kalahari. Celem jest stworzenie spójnej, intuicyjnej i dobrze udokumentowanej struktury menu bez duplikatów i konfliktów.

## Scope

### Included
- Pełny audyt wszystkich 9 menu (FILE, EDIT, BOOK, INSERT, FORMAT, TOOLS, ASSISTANT, VIEW, HELP)
- Usunięcie duplikatów w menu (szczególnie VIEW/Toolbars)
- Weryfikacja spójności nazw między menu a kodem
- Analiza UX struktury menu dla użytkownika-pisarza
- Weryfikacja skrótów klawiszowych pod kątem konfliktów
- Weryfikacja phase markers dla wszystkich komend
- Stworzenie dokumentacji specyfikacji finalnej struktury menu

### Excluded
- Implementacja nowych komend/akcji
- Zmiany w logice biznesowej komend
- Zmiany w ikonach (te są w osobnym systemie ArtProvider)

## Known Issues
1. **VIEW menu duplikaty:** Zawiera DWIE grupy toolbarów (CommandRegistry vs ToolbarManager)
2. **Niespójność nazw:** Menu pokazuje "Standard Toolbar" vs kod ma "File Toolbar" + "Edit Toolbar"
3. **Brak weryfikacji:** Nie wiadomo czy wszystkie zarejestrowane komendy są używane w menu

## Acceptance Criteria
- [x] Pełny audyt menu zakończony - lista wszystkich pozycji w każdym menu
- [x] Wszystkie duplikaty usunięte (VIEW/Toolbars now dynamic)
- [x] Nazwy w menu spójne z kodem (Toolbars renamed)
- [x] Struktura menu zweryfikowana pod kątem UX
- [x] Brak konfliktów skrótów klawiszowych
- [x] Wszystkie komendy mają prawidłowe phase markers
- [x] Dokumentacja specyfikacji menu utworzona (w tasks.md)
- [x] Wszystkie zarejestrowane komendy są używane lub oznaczone jako unused

## Files to Analyze
- `src/gui/main_window.cpp` - registerCommands, menu building
- `src/gui/toolbar_manager.cpp` - createViewMenuActions
- `src/gui/menu_builder.cpp` - menu building logic
- `include/kalahari/gui/command.h` - Command struct

## Design
(To be added by architect agent)

## Notes
To zadanie wymaga starannej analizy kodu bez wprowadzania zmian do momentu pełnego zrozumienia struktury. Architekt powinien najpierw stworzyć pełną mapę obecnego stanu menu, a dopiero potem zaproponować zmiany.
