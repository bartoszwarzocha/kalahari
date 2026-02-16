# Refaktoryzacja Render Pipeline

**Data ustalenia:** 2026-01-12
**OpenSpec:** #00043 - Editor Performance Rewrite
**Status:** W TRAKCIE

---

## Problem

Mamy TRZY osobne ścieżki renderowania zamiast jednej:

1. **Page Mode** (`paintPageMode()`) - ~400 linii osobnego kodu z DPI scaling
2. **Pipeline** (`m_renderPipeline->render()`) - bez DPI scaling (błąd!)
3. **Legacy fallback** - martwy kod, crashowałby na linii 2384

### Skutki:
- Scroll Mode ma za małą czcionkę (brak DPI scaling)
- Każda naprawa wymaga zmian w dwóch miejscach
- Zaznaczenie działa tylko w Page Mode
- ~800 linii zduplikowanego kodu

---

## Decyzje architektoniczne

### 1. Jeden przepływ dla wszystkich widoków

```
paintEvent() {
    pipeline->render(viewMode)
    │
    ├── 1. OBLICZ SKALOWANIE (raz, dla wszystkich)
    │      • dpiScale = screenDPI / 96
    │      • totalScale = dpiScale * zoom
    │
    ├── 2. PRZYGOTUJ LAYOUT
    │      • oblicz marginesy
    │      • jeśli Page Mode: podziel na strony (paginacja)
    │      • jeśli Scroll Mode: jedna "strona" ciągła
    │
    ├── 3. DLA KAŻDEJ STRONY:
    │      │
    │      ├── jeśli Page Mode: rysuj tło strony, cień
    │      │
    │      ├── renderSelection()   ← JEDEN kod dla wszystkich
    │      ├── renderText()        ← JEDEN kod dla wszystkich
    │      └── renderCursor()      ← JEDEN kod dla wszystkich
    │
    └── 4. OVERLAY (focus mode, etc.)
}
```

### 2. Gdzie żyje pipeline?

**Decyzja:** Refaktor istniejącej klasy `EditorRenderPipeline`
- NIE tworzymy nowej klasy
- Rozszerzamy istniejącą o obsługę Page Mode

### 3. Co z legacy fallback?

**Decyzja:** USUNĄĆ NATYCHMIAST
- To martwy kod
- Crashowałby gdyby się wykonał (linia 2384 używa null pipeline)

### 4. Zoom w Scroll Mode?

**Decyzja:** PageScaling (skaluje całość jak Page Mode)
- Spójność między widokami
- Jeden mechanizm skalowania

---

## Struktura RenderParams

```cpp
struct RenderParams {
    // Skalowanie (obliczane RAZ)
    qreal screenDpi;      // z screen()->physicalDotsPerInch()
    qreal dpiScale;       // screenDpi / 96.0
    qreal zoomScale;      // poziom zoom użytkownika
    qreal totalScale;     // dpiScale * zoomScale

    // Layout
    qreal textWidth;      // dostępna szerokość tekstu (w jednostkach 96 DPI)
    QMarginsF margins;    // w pikselach ekranu

    // Flagi zależne od widoku
    bool showPageBackground;   // Page Mode: true, Scroll: false
    bool enablePagination;     // Page Mode: true, Scroll: false
    QSizeF pageSize;           // tylko dla Page Mode
};
```

---

## Plan implementacji

### Krok 1: Wyczyścić legacy fallback
- [ ] Usunąć martwy kod z `paintEvent()` (linie 2353-2450+)
- [ ] Upewnić się, że pipeline ZAWSZE istnieje

### Krok 2: Dodać DPI scaling do pipeline
- [ ] Dodać `screenDpi`, `dpiScale` do `RenderContext`
- [ ] Zastosować skalowanie w `EditorRenderPipeline::render()`
- [ ] Scroll Mode zyskuje poprawną czcionkę

### Krok 3: Przenieść paginację do pipeline
- [ ] Przenieść struktury `ParagraphSlice`, `PageContent` do pipeline
- [ ] Przenieść logikę paginacji z `paintPageMode()` do pipeline
- [ ] Cache paginacji w pipeline (nie w BookEditor)

### Krok 4: Zunifikować renderowanie
- [ ] Jeden `renderText()` dla wszystkich widoków
- [ ] Jeden `renderSelection()` dla wszystkich widoków
- [ ] Jeden `renderCursor()` dla wszystkich widoków
- [ ] Page Mode = flagi w pipeline, nie osobna ścieżka

### Krok 5: Usunąć stary kod
- [ ] Usunąć `paintPageMode()` z BookEditor
- [ ] Usunąć `drawSelection()` z BookEditor
- [ ] Usunąć `drawCursor()` z BookEditor
- [ ] Usunąć duplikaty struktur paginacji z BookEditor

---

## Oczekiwane rezultaty

| Metryka | Przed | Po |
|---------|-------|-----|
| Ścieżki renderowania | 3 | 1 |
| Linie kodu renderowania | ~800 | ~400 |
| DPI scaling | tylko Page Mode | wszędzie |
| Czcionka w Scroll Mode | za mała | poprawna |
| Miejsca do naprawy błędów | 2-3 | 1 |

---

## Pliki do modyfikacji

| Plik | Zmiany |
|------|--------|
| `src/editor/book_editor.cpp` | Usunąć legacy, paintPageMode, drawSelection, drawCursor |
| `include/kalahari/editor/book_editor.h` | Usunąć deklaracje usuniętych funkcji |
| `src/editor/editor_render_pipeline.cpp` | Dodać DPI, paginację, zunifikowane renderowanie |
| `include/kalahari/editor/editor_render_pipeline.h` | Dodać RenderParams, PageContent, ParagraphSlice |
| `include/kalahari/editor/render_context.h` | Dodać screenDpi, dpiScale |

---

## WAŻNE - nie zapomnieć!

1. **ZAWSZE** używać `totalScale` do skalowania tekstu
2. **NIGDY** nie tworzyć osobnych ścieżek dla widoków
3. **JEDEN** kod rysowania = JEDEN zestaw błędów
4. **CACHE** paginacji współdzielony między renderowaniem a hit-testingiem
