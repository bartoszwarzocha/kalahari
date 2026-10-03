# Edytor – etap 0 (siatka bezpieczeństwa): wyniki

Data: 2026-10-03 · gałąź `editor/stage0-safety-net` · baza: `main` @ `b311c1b`

Etap 0 nie zmienia kodu produkcyjnego edytora – dodaje wyłącznie testy i benchmark.

## Co zbudowano

- **`tests/editor/test_editor_stage0_kml_roundtrip.cpp`** – zapis/odczyt KML przez prawdziwą ścieżkę
  `BookEditor::fromKml()` → `toKml()`: wyrównanie, b/i/u/s, zagnieżdżenia, przeciekanie formatu,
  comment/todo/footnote, znaki `& < > " '` w tekście i w atrybutach, marker TODO dodany w edytorze.
- **`tests/editor/test_editor_stage0_layout.cpp`** – geometria akapitów
  (`KalahariTextDocumentLayout`, `QTextDocumentSource`, `ViewportManager`) porównywana z układem
  referencyjnym liczonym od zera: po wczytaniu, wklejeniu >3 akapitów, undo dużego usunięcia,
  zmianie szerokości; zgodność `ViewportManager::paragraphY` z `blockBoundingRect`; liczba pełnych
  przełożeń przy jednym resize.
- **`tests/benchmarks/test_editor_stage0_benchmark.cpp`** – benchmark ukryty (`[.][benchmark][stage0]`),
  nie spowalnia zwykłego zestawu. Uruchomienie:
  `build-windows\bin\kalahari-tests.exe "[benchmark][stage0]"`. Uwaga: krok „Copy” nadpisuje schowek.
- Testy wykazujące błąd mają tagi `[known-bug][!mayfail]` i opis w komentarzu – nie są naprawiane.
  Po naprawie w kolejnym etapie należy usunąć oba tagi.
- Przy okazji: `scripts/build_windows.bat` wykrywa teraz Build Tools przez `vswhere` (osobny commit).

## Wyniki testów (Windows, Debug, MSVC 14.51)

| Zestaw | Przypadki | Wynik |
|---|---:|---|
| Pełny zestaw | 668 | 657 OK, 11 „failed as expected” (wszystkie to `[known-bug]` etapu 0) |
| Istniejące testy (bez etapu 0) | 634 | wszystkie OK |
| `[stage0]` | 34 | 23 OK, 11 known-bug |

## Podejrzenia z raportu – weryfikacja

| Podejrzenie | Wynik | Dowód |
|---|---|---|
| Wyrównanie akapitów ginie przy wczytaniu (`ensureEditMode`) | **Nie potwierdzone** – naprawione wcześniej (`05b3179`) | testy alignment przechodzą (load + save + 2. cykl) |
| Utrata b/i/u/s, przeciekanie formatu między akapitami | **Nie potwierdzone** | testy przechodzą |
| Znaki `& < > "` w tekście | **Nie potwierdzone** | testy przechodzą (też wpisane w edytorze) |
| Znaki specjalne w **wartościach atrybutów** | **Potwierdzone** | `loadKml` składa `name="value"` z nieescapowanej wartości → akapit obcięty do „One ”, id komentarza puste |
| Metadane komentarzy/TODO/przypisów | **Potwierdzone** (częściowo) | zostaje tylko `id`; `author/created/resolved`, `completed/priority`, `number` giną przy każdym wczytaniu (`KmlDocumentModel::parseInlineContent`) |
| Marker TODO dodany w edytorze (`addTodoAtCursor`) | **Potwierdzone** – nowe | po zapisie i odczycie `findAllMarkers()` zwraca 0 (JSON-string w `KmlPropTodo` vs `toMap()` w serializerze) |
| `documentChanged()` układa tylko 3 bloki | **Potwierdzone** | wklejenie 10 akapitów: 8 bloków bez linii; undo usunięcia 14 akapitów: 11 bez linii, 15 złych pozycji – także przez `BookEditor` |
| Szerokość z pipeline’u nie trafia do layoutu | **Potwierdzone** – nowe | `QTextDocumentSource::setTextWidth()` robi pełne przełożenie, ale ze starą szerokością (8/8 akapitów niezgodnych) |
| Potrójne pełne przełożenie przy resize | **Potwierdzone** | 3 pełne przełożenia na jeden resize; zoom: 2 |
| Wczytywanie O(n²) przez brak `beginEditBlock` | **Częściowo** | brak edit blocku kosztuje ~8× (1090 → 133 ms dla 50k słów); skalowanie lekko nadliniowe (×2,0 i ×2,3 na podwojenie), nie kwadratowe |
| Pełna paginacja przy każdym klawiszu w trybie strony | **Nie potwierdzone jako problem** | Page 24,8 ms/znak vs Continuous 30,5 ms/znak (z malowaniem); koszt zdominowany przez samo malowanie |
| Liczenie słów regexem przy każdym paintEvent (DistractionFree) | **Potwierdzone** | paintEvent 309,8 ms vs 25,2 ms w Continuous (×12) |
| Liniowe skanowanie w `ViewportManager` | **Potwierdzone** (mały koszt przy tym rozmiarze) | `paragraphY(ostatni)` 0,54 ms/wywołanie przy 1568 akapitach – O(n) |
| Skalowanie fizycznym DPI | **Potwierdzone** | `physicalDotsPerInch` = 142,4 przy `logicalDotsPerInch` = 96 i DPR 1,25 – skala 1,48 zamiast 1,25 |
| `ViewportManager` vs `blockBoundingRect` | **Zgodne** | ten sam stan layoutu daje te same pozycje (też po wklejeniu) |

## Benchmark – czasy

Konfiguracja: **Debug** (czasy zawyżone względem Release). Ekran: physicalDPI 142,4, logicalDPI 96,
DPR 1,25. Dokument: 150 088 słów, 1568 akapitów, 952 878 znaków (generator `TestDocumentGenerator`,
bez nagłówków `<h>`, których loader nie obsługuje). Okno 1000×800 px.

| Operacja | Czas [ms] | Uwagi |
|---|---:|---|
| fromKml – 25k słów | 906,6 | 257 akapitów; parsowanie modelu 39,9 ms |
| fromKml – 50k słów | 1765,1 | 506 akapitów; parsowanie 74,9 ms |
| fromKml – 100k słów | 4030,4 | 1043 akapitów; parsowanie 151,2 ms |
| fromKml – 150k słów | 7273,0 | 1568 akapitów; parsowanie 230,5 ms |
| Budowa QTextDocument jak `ensureEditMode` – 50k, bez edit block | 1089,9 | replika testowa |
| to samo z `beginEditBlock` | 132,9 | replika testowa |
| paintEvent – Continuous (średnio z 10) | 25,2 | |
| Zmiana szerokości 1000→1200 px | 823,5 | 3 pełne przełożenia |
| paintEvent po zmianie szerokości | 298,0 | |
| Zmiana powiększenia 100→125% (Continuous) | 558,6 | 2 pełne przełożenia |
| paintEvent po zmianie powiększenia | 41,0 | |
| 100 znaków – Continuous (bez malowania) | 204,7 | |
| 100 znaków – Continuous (z paintEvent po każdym) | 3044,6 | 30,45 ms/znak |
| Przełączenie na Page + pierwszy paintEvent | 24,3 | |
| 100 znaków – Page (bez malowania) | 198,7 | |
| 100 znaków – Page (z paintEvent po każdym) | 2482,7 | 24,83 ms/znak |
| Przewinięcie na koniec (scrollTo + paintEvent) | 30,3 | |
| Ctrl+End (kursor na koniec + paintEvent) | 30,6 | |
| `ViewportManager::paragraphY(ostatni)` ×100 | 53,6 | 0,54 ms/wywołanie |
| `ViewportManager::paragraphAtY(koniec)` ×100 | 58,5 | 0,59 ms/wywołanie |
| `ViewportManager::setScrollPosition(koniec)` | 0,6 | |
| paintEvent – DistractionFree (średnio z 10) | 309,8 | ×12 względem Continuous |
| Select All | 5,4 | |
| Copy (cały dokument) | 8,3 | |
| paintEvent z zaznaczonym całym dokumentem | 28,6 | |

## Wnioski dla etapu 1 (priorytety wg pomiaru)

1. **Wczytywanie**: `beginEditBlock`/`endEditBlock` w `ensureEditMode` – największy pojedynczy zysk (~8×).
2. **Układ po edycji**: `documentChanged()` musi układać wszystkie bloki objęte zmianą (wklejenie, undo).
3. **Resize/zoom**: jedno pełne przełożenie zamiast 3/2; ujednolicić szerokość (pipeline ↔ layout).
4. **DistractionFree**: licznik słów z cache, nie w każdym paintEvent.
5. **KML**: przenosić wszystkie atrybuty metadanych, escapować wartości atrybutów, ujednolicić typ
   `KmlPropTodo` (JSON-string vs mapa).
6. **DPI**: przejść na logiczne DPI / devicePixelRatio.
7. `ViewportManager` O(n) – niski priorytet przy obecnych rozmiarach; zbadać po pkt 2–3.
