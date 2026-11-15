# Materiały źródłowe – plan wdrożenia

Data: 2026-11-15 · Makieta: [Materiały źródłowe](https://claude.ai/artifact/YSi5QCzMQLdaVocWVbrVnK), przyjęta
15.11 w całości: możliwość C, ustawienia domyślne według propozycji, warianty jako ustawienia, uzupełnianie
opisu z sieci i zakreślenia w PDF jako opcje (domyślnie wyłączone). Plan rozwija etapy 6–8 planu
[zarządzania materiałami](2026-10-25-zarzadzanie-materialami.md).

## Cel

Każdy materiał może być źródłem: publikacja, dokument, strona internetowa i nagranie mają opis
bibliograficzny, a „Źródła” pod „Materiałami” to zestawienie materiałów, które go mają. Opis daje zapis
w bibliografii w normie książki i odnośnik w tekście. Cytat wskazuje źródło i stronę. PDF otwiera się na
karcie podglądu w programie: strony, zakładki, spis treści, miniatury, szukanie, zaznaczanie tekstu, cytat
z zaznaczenia ze stroną.

## Punkt wyjścia

- Materiał ma tytuł, plik albo adres, opis, własne pola, powiązania i dane okładki
  (`include/kalahari/core/book_project.h:160–197`, zapis `src/core/book_project.cpp:335–372`, `:767–795`);
  zmiany przez `ProjectManager::changeDetails` (`src/core/project_manager.cpp:2390–2410`).
- Słowa cytatu to opis materiału (`src/gui/material_list.cpp:306–308`); cytat nie ma autora, dzieła ani strony.
- „Podgląd” pokazuje w programie tylko obrazy (`src/gui/panels/material_page.cpp:721–728`).
- Rodzaj „Źródło” literatury faktu ma edytor kart (`resources/booktypes/kalahari.nonfiction/booktype.json:106–118`),
  którego program nie umie dodawać (`src/core/project_manager.cpp:1477–1480`), więc żadna książka nie ma źródeł.
- Listy materiałów nazywa rodzaj z paczką; `MaterialList::ALL` to „Wszystkie materiały”
  (`include/kalahari/gui/material_list.h:55–66`).

## Zapis w pliku książki

```json
"source": {
  "type": "book",
  "people": [{ "name": "Anna Wiśniewska", "role": "author" }],
  "title": "Atlas pustyń świata", "subtitle": "Krajobrazy, ludzie, szlaki",
  "edition": "2 popr.", "place": "Warszawa", "publisher": "Wydawnictwo Piasek",
  "year": "2011", "pages": "212", "isbn": "978-83-0000-000-0", "language": "polski",
  "bibliography": "linked"
},
"bookmarks": [{ "page": 45, "note": "Przysłowie o karawanie" }],
"lastPage": 45,
"quote": { "source": "m12", "page": "45" }
```

- `source` – opis bibliograficzny; tytuł opisu pusty: tytuł materiału. `bibliography`: `linked` (domyślnie,
  bez zapisu), `always`, `never`.
- `bookmarks`, `lastPage` – zakładki (strona od 1 i notatka) i ostatnio oglądana strona pliku.
- `quote` – źródło (identyfikator materiału) i strona cytatu; bez źródła `author` i `work`. Słowa cytatu
  zostają opisem materiału.
- Książka: `"bibliographyStyle": "pn-iso-690"`; bez niego – ustawienie programu.
- Program zapisuje z powrotem pola, których nie zna.

## Etapy

### 1. Opis bibliograficzny i cytaty ze źródłem

- Model: `core::SourceDescription` (rodzaje: książka, rozdział w książce, artykuł w czasopiśmie, strona
  internetowa, dokument archiwalny, rozmowa lub wywiad, film lub nagranie, praca dyplomowa, inne; osoby
  z rolami), `core::QuoteSource`, zakładki, ostatnia strona, norma książki; zapis i odczyt z polami nieznanymi.
- `core::Bibliography`: zapis w normach PN-ISO 690 (domyślna), APA, Chicago, MLA, Harvard, jako tekst
  i z kursywą; odnośnik w trzech postaciach: „(Wiśniewska 2011, s. 45)” (domyślny), „Wiśniewska, Atlas
  pustyń świata, s. 45”, „[1, s. 45]”. Odczyt opisu BibTeX i RIS („Wklej opis”).
- Karta materiału (publikacja, dokument, strona internetowa, nagranie): grupy „Opis bibliograficzny”
  (rodzaj zmienia pola; osoby w tabeli z Insert i Delete; „Wklej opis (BibTeX, RIS)”), „Tak wygląda
  w bibliografii” (norma, zapis, „W bibliografii”, „Kopiuj zapis”), „Zakładki” (strona, notatka) i „Cytaty
  z tego źródła” (Insert tworzy cytat z tym źródłem, Enter otwiera). Krótka karta we Właściwościach
  pokazuje zapis w bibliografii.
- Karta cytatu: grupa „Skąd” – źródło (materiały z opisem i „bez źródła”), strona, autor i dzieło bez
  źródła, odnośnik.
- Nawigator: „Źródła” pod „Materiałami” (kursywą, z liczbą) otwiera zestawienie: typ materiału, rodzaj
  źródła, autor, tytuł, rok, w bibliografii, w tekście; „Kopiuj zapisy”.
- Ustawienia › Materiały (nowa strona): Źródła (norma, odnośnik). Właściwości książki: norma książki.
  Grupa Otwieranie (obrazy i strony ze strony Nawigator) przechodzi tu w etapie 2, razem z otwieraniem PDF.
- Literatura faktu: rodzaj „Źródło” wycofany (`"retired": true`).

Stan: zrobiony (gałąź `claude/project-thread-zsum8e`). Zakładki są na karcie PDF-u i materiału, który je
ma; otwierają podgląd dopiero w etapie 2.

### 2. Podgląd PDF w programie

- Moduł opcjonalny Qt PDF (`find_package(Qt6 COMPONENTS Pdf QUIET)`, jak synteza mowy); bez niego PDF
  otwiera się w programie systemowym, a pozycje podglądu są wyszarzone.
- Karta podglądu: własne rysowanie stron (`QPdfDocument::render`), bo `QPdfView` w Qt 6.4 nie zaznacza
  tekstu; układ: jedna pod drugą (domyślnie), po jednej, dwie obok siebie; powiększenie poleceniami
  Widok › Powiększenie; numer strony z etykietą pliku (`pageLabel`), Ctrl+G; PgUp, PgDn, Home, End.
- Panel boczny: zakładki, spis treści (`QPdfBookmarkModel`), miniatury, wyniki szukania z fragmentem
  (`QPdfSearchModel`); Ctrl+F, Enter, Shift+Enter, Esc; F6 i Shift+F6 między paskiem, panelem i stronami.
- Zaznaczanie tekstu (`getSelection`), Ctrl+C, menu zaznaczenia (klawisz menu, Shift+F10) i pasek
  zaznaczenia: Kopiuj, Kopiuj z odnośnikiem, Utwórz cytat… (okno „Nowy cytat” ze źródłem i stroną),
  Zakładka tutaj.
- Strony w ciemnym motywie: jak w pliku (domyślnie), przyciemnione, odwrócone kolory. Kolory tła stron,
  zaznaczenia i wyników: Edytor › Kolory, osobno w każdym motywie.
- Otwieranie na ostatniej stronie (domyślnie włączone); „Uzupełnij z pliku” (`metaData`); okładka
  publikacji z pierwszej strony PDF; „Pokaż stronę” na karcie cytatu i dwuklik zakładki.
- Ustawienia › Materiały: Otwieranie PDF (w programie, domyślnie, albo w programie systemowym), grupa
  „Podgląd PDF”.

Stan: zrobiony (gałąź `claude/project-thread-zsum8e`, po etapie 1). Zakładkę na karcie pokazuje przycisk
„Pokaż stronę w podglądzie”, nie dwuklik, bo dwuklik w tabeli zmienia stronę lub notatkę. Kolory wokół
stron, zaznaczenia i wyników są wspólne z edytorem (Edytor › Kolory). Grupa Otwieranie (obrazy, strony
w sieci, PDF) przeszła ze strony Nawigator na stronę Materiały. „Utwórz cytat…” w oknie „Nowy cytat”
wiąże cytat z rozdziałem otwartym ostatnio, z innym elementem albo z żadnym; „Utwórz i wstaw do
rozdziału” wstawia słowa z odnośnikiem. Linux ma Qt PDF z pakietu `qt6-pdf-dev`. Na Windows i macOS
Qt PDF daje tylko opcjonalna cecha vcpkg `pdf-preview` (`qtwebengine` z samą cechą `pdf`), domyślnie
wyłączona lokalnie i w CI: jej kompilacja trwa godziny i zajmuje dziesiątki GB, a na Windows pada na
ścieżkach dłuższych niż 260 znaków. Bez niej PDF otwiera się w programie systemowym.

### 3. Opcje

- „Uzupełniaj opis z sieci po ISBN i DOI” (domyślnie wyłączone): wysyła sam numer do katalogu w sieci;
  bez opcji przycisk na karcie jest ukryty.
- Zakreślenia w PDF zapisywane w książce (domyślnie wyłączone): kolor i notatka, w panelu bocznym obok
  zakładek; plik PDF zostaje bez zmian.

Stan: zrobiony (gałąź `claude/project-thread-zsum8e`, po etapach 1 i 2). Opcja „Opisy z sieci na podstawie
ISBN i DOI” jest w Ustawieniach › Materiały › Źródła razem z wyborem katalogu dla ISBN
(Google Books – zalecany – albo Open Library); DOI zawsze trafia do doi.org. Przycisk „Opis z sieci
(ISBN, DOI)...” na karcie otwiera okno z numerem z karty; znaleziony opis zastępuje te pola, które ma,
a pozostałe zostają. Odpowiedź jest ograniczona rozmiarem i czasem, Esc przerywa wyszukiwanie. Opcja
„Zakreślenia na stronach, zapisane w książce” jest w Ustawieniach › Materiały › Podgląd PDF.
„Zakreśl” i „Zakreśl kolorem” są na pasku i w menu zaznaczenia; karta „Zakreślenia” w panelu bocznym
pokazuje stronę, tekst i notatkę (Enter, F2, Delete, Insert i przycisk „Kolor” działają w tej liście),
a menu zakreślenia na stronie zmienia jego kolor i notatkę albo je usuwa. Zakreślenia są zapisywane
w pliku książki jako `highlights` źródła (strona, obszary w punktach, tekst, kolor, notatka), a nie
w pliku PDF. Przy okazji poprawiono błąd etapu 2: Qt PDF rysuje strony na przezroczystym tle, więc
strony, miniatury i okładka z pierwszej strony są teraz na białym papierze.

## Klawiatura

Bez nowych skrótów: podgląd używa Ctrl+G, Ctrl+F, Ctrl++, Ctrl+-, Ctrl+0, F6 i klawiszy stron; polecenia
zaznaczenia są w menu i można im przypisać klawisze w Ustawieniach › Skróty klawiszowe.

## Każdy etap

Osobny PR; testy modelu, zapisu i odczytu, zapisu w normach, kart i list; cały zestaw testów pod Linuksem;
przegląd kodu; CHANGELOG; analiza SonarQube przed oddaniem.
