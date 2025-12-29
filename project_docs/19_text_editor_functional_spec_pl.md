# Kalahari — Specyfikacja funkcjonalna kontrolki edytora tekstu (PL)

**Cel dokumentu:** opisać funkcje i koncepcję operacyjną własnej kontrolki edytora tekstu o przeznaczeniu profesjonalnym dla pisarzy (powieść, opowiadanie, non-fiction, poezja), spójnej z filozofią „Writer's IDE".

**Zakres:** UX/funkcje kontrolki (bez implementacji, bez decyzji technologicznych). Kontrolka ma być używana w centrum aplikacji (zakładki dokumentów/rozdziałów), współpracować z panelami (nawigator, statystyki, asystent), działać „keyboard-first".

**Status:** DRAFT v5.0
**Ostatnia aktualizacja:** 2025-12-19

---

## 1. Definicje i pojęcia

- **Dokument**: edytowany zasób tekstowy (np. rozdział, scena, notatka, bibliografia). Kontrolka edytuje *jeden* dokument naraz.
- **Projekt książki**: zbiór dokumentów + metadane (strukturę projektu obsługują inne moduły, kontrolka ma wspierać nawigację i kontekst).
- **Struktura logiczna**: nagłówki, sceny, akapity, listy, cytaty, przypisy itd.
- **Struktura wydruku**: strony, marginesy, sekcje, łamanie stron (istotne w trybie „Strona").
- **Adnotacje**: komentarze, sugestie, śledzenie zmian, zakładki, wyróżnienia „do sprawdzenia".
- **KML (Kalahari Markup Language)**: własny język znaczników HTML-like do przechowywania treści i metadanych.

---

## 2. Persona i cele użytkowników

### 2.1. Pisarz „Draft-first"
- Chce pisać szybko, bez rozpraszania.
- Potrzebuje czytelnego kursora, statystyk (słowa/cele), trybu maszyny do pisania.

### 2.2. Autor „Edit-first"
- Pracuje iteracyjnie: rewizje, komentarze, porządkowanie.
- Potrzebuje nawigacji po strukturze, znajdowania, pracy na stylach.

### 2.3. Redaktor / korektor (wewnętrzny lub zewnętrzny)
- Przegląd, sugestie, komentarze, tryb „proofreading".
- Minimalny wysiłek w przełączaniu kontekstów i wgląd w metadane.

**Uwaga:** Kalahari jest przeznaczona głównie dla samotnych pisarzy lub małych zespołów, gdzie jedna osoba pracuje nad jednym rozdziałem.

---

## 3. Zasady projektowe kontrolki

- **Keyboard-first**: wszystko osiągalne skrótami i poleceniami; mysz jest dodatkiem.
- **Stabilny tekst**: brak „skakania" layoutu, przewidywalne przewijanie, poprawna praca IME.
- **Separacja treści od prezentacji**: style i semantyka są „prawdą"; tryby widoku to różne prezentacje.
- **Pisarz > procesor tekstu**: funkcje typograficzne są ważne, ale priorytetem jest proces pisania (cele, flow, rewizje).
- **Skalowalność dokumentu**: płynna praca na dokumentach do 100k+ słów (cała powieść); szybka nawigacja i przewijanie bez zauważalnych opóźnień. Lazy layout dla bardzo dużych dokumentów.
- **Własny rendering**: kontrolka z własnym renderingiem, pełna kontrola nad interpretacją danych.

---

## 4. Format danych: KML (Kalahari Markup Language)

### 4.1. Filozofia

Zamiast czystego HTML, używamy własnego języka znaczników HTML-like. Kontrolka renderuje KML na swój sposób, co daje pełną kontrolę nad:
- Stylami (odwołania do ID stylu, nie inline CSS)
- Adnotacjami (komentarze, notatki jako elementy inline)
- Metadanymi (linki do bibliotek, tagi)
- Eksportem (łatwe ignorowanie/transformacja elementów)

### 4.2. Podstawowa struktura

```xml
<kml version="1.0" lang="pl">
  <p style="normal">Zwykły akapit tekstu.</p>

  <p style="normal">
    Tekst z <em>kursywą</em> i <strong>pogrubieniem</strong>.
  </p>

  <h1 style="heading1">Rozdział pierwszy</h1>

  <p style="dialog" speaker="Jan">
    — Jak się masz? — zapytał Jan.
  </p>

  <scene-break type="ornament" />

  <p style="normal">
    Spotkał <char-ref id="anna">Annę</char-ref> na ulicy.
    <comment author="Autor" date="2025-12-18" collapsed="true">
      Sprawdzić czy to pasuje do timeline'u
    </comment>
  </p>

  <p style="normal">
    To ważne zdanie.<footnote id="fn1">Przypis dolny z wyjaśnieniem.</footnote>
  </p>

  <p style="normal">
    <todo type="CHECK">Zweryfikować fakty historyczne</todo>
    Bitwa pod Grunwaldem odbyła się w 1410 roku.
  </p>
</kml>
```

### 4.3. Elementy KML

#### Struktura dokumentu
| Element | Opis |
|---------|------|
| `<kml>` | Root element, atrybuty: version, lang |
| `<p>` | Akapit, atrybut: style (ID stylu) |
| `<h1>`, `<h2>`, `<h3>` | Nagłówki poziom 1-3 |
| `<scene-break>` | Separator sceny (wizualizacja zależna od motywu) |

#### Formatowanie inline
| Element | Opis |
|---------|------|
| `<em>` | Wyróżnienie (kursywa) |
| `<strong>` | Mocne wyróżnienie (pogrubienie) |
| `<u>` | Podkreślenie |
| `<s>` | Przekreślenie |
| `<span>` | Generyczny inline, atrybut: style |

#### Listy
| Element | Opis |
|---------|------|
| `<ul>` | Lista nieuporządkowana (punktowana) |
| `<ol>` | Lista uporządkowana (numerowana), atrybut: start (opcjonalny) |
| `<li>` | Element listy, może zawierać formatowanie inline |

```xml
<ul>
  <li>Pierwszy punkt</li>
  <li>Drugi punkt z <em>kursywą</em></li>
</ul>

<ol start="1">
  <li>Krok pierwszy</li>
  <li>Krok drugi</li>
</ol>
```

#### Media
| Element | Opis |
|---------|------|
| `<img>` | Obraz, atrybuty: src (ścieżka), alt, width, height, align |

```xml
<img src="images/mapa.png" alt="Mapa świata" width="400" align="center" />
```

#### Tabele
| Element | Opis |
|---------|------|
| `<table>` | Kontener tabeli |
| `<tr>` | Wiersz tabeli |
| `<td>` | Komórka tabeli, atrybuty: colspan, rowspan, align |
| `<th>` | Komórka nagłówkowa |

```xml
<table>
  <tr>
    <th>Postać</th>
    <th>Rola</th>
    <th>Rozdział wprowadzenia</th>
  </tr>
  <tr>
    <td>Jan Kowalski</td>
    <td>Protagonista</td>
    <td>1</td>
  </tr>
  <tr>
    <td>Anna Nowak</td>
    <td>Antagonista</td>
    <td>3</td>
  </tr>
</table>
```

**Zastosowania dla pisarzy:**
- Zestawienia postaci
- Timeline wydarzeń
- Porównania lokacji
- Notatki badawcze

#### Adnotacje (inline, zwijane)
| Element | Opis |
|---------|------|
| `<comment>` | Komentarz, atrybuty: author, date, collapsed, resolved, anchor (optional) |
| `<note>` | Notatka autora (inna kolorystyka niż comment) |
| `<todo>` | Tag do zrobienia, atrybut: type (TODO, FIX, CHECK, RESEARCH) |
| `<footnote>` | Przypis dolny, atrybut: id |
| `<endnote>` | Przypis końcowy, atrybut: id |

#### Referencje do bibliotek
| Element | Opis |
|---------|------|
| `<char-ref>` | Link do postaci, atrybut: id |
| `<loc-ref>` | Link do lokacji, atrybut: id |
| `<item-ref>` | Link do przedmiotu, atrybut: id |
| `<cite>` | Cytowanie bibliograficzne, atrybut: id |

### 4.4. Renderowanie adnotacji

**Komentarze i notatki** renderowane jako **zwijane subakapity**:
- Domyślnie zwinięte: ikonka trójkąta (▶) po lewej stronie tekstu
- Kliknięcie rozwija: tekst komentarza pojawia się jako wcięty blok w innym kolorze
- Alternatywnie: popup przy kursorze (konfigurowalny tryb wyświetlania)

**Typy komentarzy:**
- **Komentarz ogólny** (bez atrybutu anchor) — przypisany do akapitu, ikonka po lewej
- **Komentarz do zakresu** (z atrybutem anchor) — przypisany do fragmentu tekstu

**Obsługa usuwania tekstu z komentarzem:**
- Komentarz ogólny: bez zmian (przypisany do akapitu)
- Komentarz z anchorem, częściowe usunięcie: automatyczne dostosowanie anchora
- Komentarz z anchorem, pełne usunięcie tekstu: dialog z opcjami:
  - Usuń komentarz
  - Zachowaj jako komentarz ogólny (przenieś na akapit)
  - Przenieś na koniec akapitu
  - Anuluj

**Eksport:** Elementy adnotacji są ignorowane lub transformowane według ustawień eksportu.

### 4.5. Przechowywanie w .kchapter

```json
{
  "kalahari": { "version": "1.0", "type": "chapter" },
  "content": {
    "kml": "<kml version=\"1.0\" lang=\"pl\">...</kml>"
  },
  "statistics": {
    "wordCount": 2500,
    "characterCount": 15000,
    "paragraphCount": 45,
    "lastModified": "2025-12-19T10:30:00Z"
  },
  "metadata": {
    "title": "Rozdział 1",
    "status": "draft"
  },
  "history": [
    { "action": "created", "by": "Anna Kowalska", "at": "2025-12-01T10:00:00Z" },
    { "action": "edited", "by": "Jan Nowak", "at": "2025-12-10T14:30:00Z" },
    { "action": "reviewed", "by": "Maria Wiśniewska", "at": "2025-12-15T09:00:00Z" }
  ]
}
```

**Uwaga:** Pole `plainText` nie jest przechowywane osobno — jest generowane on-demand z KML dla wyszukiwania i disaster recovery.

---

## 5. Tryby widoku (View Modes)

Kontrolka wspiera **3 tryby widoku** z szybkim przełączaniem.

### 5.1. Tryb „Ciągły" (Continuous View)

**Cel:** płynne pisanie i edycja bez mentalnego obciążenia stronami.

**Prezentacja:**
- Brak podziału na strony i brak wizualnych granic stron (jedna ciągła „strona").
- Tekst zawsze zawija się do dostępnej szerokości widoku (word wrap).
- Konfigurowalne dzielenie wyrazów (włączone/wyłączone).
- Delikatne markery struktury: separatory scen, nagłówki, przypisy.

**Ustawienia:**
- Szerokość kolumny (auto / stała wartość dla komfortu czytania)
- Dzielenie wyrazów (on/off)
- Odstępy między akapitami
- Minimalna wysokość linii

### 5.2. Tryb „Strona" (Page View)

**Cel:** praca „jak w edytorze biurowym" i podgląd układu stron.

**Prezentacja:**
- Wizualne granice stron, marginesy, nagłówek/stopka.
- Widoczne łamania stron i sekcji.
- Stabilny układ: przewijanie ciągłe z wyraźnymi stronami.

**Zachowanie edycji:**
- Enter tworzy nowy akapit; Shift+Enter nową linię w akapicie.
- Wstawianie twardego podziału strony jako osobna operacja.

**Ustawienia:**
- Rozmiar papieru, orientacja, marginesy
- Widoczność nagłówka/stopki
- Numeracja stron

**Wskazówka UX:** Najlepszy do finalnych rewizji oraz kontroli paginacji, wdów/sierot.

### 5.3. Tryb „Maszyna do pisania" (Typewriter Mode)

**Cel:** maksymalna koncentracja i stały „punkt pisania".

**Prezentacja i zachowanie:**
- Kursor utrzymywany na stałej wysokości (domyślnie ~50% viewportu, konfigurowalne: 40–60%).
- Przewijanie odbywa się wokół kursora: tekst „płynie" w tle.
- Minimalizacja dystrakcji.

**Focus Mode (Tryb Skupienia):**
- **Dimming (Wygaszanie):** tekst poza aktywnym obszarem staje się półprzezroczysty.
- **Poziomy skupienia:**
  - Aktywny akapit (domyślnie)
  - Aktywne zdanie (maksymalne skupienie)
  - Aktywne 3 akapity (kontekst)
- Płynne przejścia przy przesuwaniu kursora (animacja fade, ~200ms).
- Konfigurowalna intensywność wygaszania (40–80%).

**Ustawienia:**
- Pozycja osi kursora (40–60%)
- Focus Mode: on/off
- Poziom skupienia: zdanie/akapit/kontekst
- Intensywność dimming
- Auto-hide UI (jeżeli host wspiera)

### 5.4. Tryb „Pełne Skupienie" (Distraction-Free Mode)

**Cel:** maksymalna immersja w proces twórczy — tylko tekst, nic więcej.

**Prezentacja:**
- Pełnoekranowy widok bez żadnych elementów UI
- Brak menu, toolbarów, paneli bocznych, pasków statusu
- Tylko tekst na neutralnym tle
- Subtelny kursor, minimalistyczna estetyka

**Wejście/Wyjście:**
- Wejście: F11 lub dedykowany skrót (Ctrl+Shift+F)
- Wyjście: Escape (z potwierdzeniem lub bez, konfigurowalne)
- Przesunięcie kursora do góry ekranu pokazuje minimalne menu (opcjonalnie)

**Kompatybilność:**
- Działa z każdym trybem widoku (Continuous, Page, Typewriter)
- Focus Mode dimming zachowany
- Typewriter scroll zachowany

**Filozofia:**
> Pisarz, szklanka whisky, zanurzenie w procesie twórczym.
> Zero dystraktorów. Tylko słowa.

### 5.5. Split View (Podział widoku)

**Cel:** praca z dwoma fragmentami tego samego dokumentu jednocześnie.

**Prezentacja:**
- Podział poziomy lub pionowy (konfigurowalne)
- Dwa niezależne viewporty tego samego dokumentu
- Każdy viewport ma własny scroll i kursor
- Synchronizacja opcjonalna (scroll-lock)

**Zastosowania:**
- Porównanie początku i końca rozdziału
- Pisanie nawiązując do wcześniejszego fragmentu
- Edycja notatek podczas pisania

**Skróty:**
- Ctrl+\ lub View → Split Horizontal
- Ctrl+Shift+\ lub View → Split Vertical
- Ctrl+W zamyka aktywny panel (jeśli split)

---

## 6. Podstawowe funkcje edycji

### 6.1. Wprowadzanie tekstu
- Pełna obsługa Unicode, IME, wklejania.
- Inteligentne zachowanie białych znaków:
  - Usuwanie nadmiarowych spacji (opcjonalnie)
  - Utrzymanie wcięć w poezji/formatowaniu precyzyjnym

**Smart Typography:**
- Automatyczna zamiana cudzysłowów prostych na drukarskie zgodnie z językiem dokumentu.
- Obsługa ligatur typograficznych.
- Wizualizacja twardych spacji (non-breaking space).
- Konfigurowalne per dokument lub globalnie.

### 6.2. Zaznaczanie i kursor

**Podstawowe operacje:**
- Zaznaczanie myszą, klawiaturą (Shift+strzałki)
- Zaznaczanie słów (double-click), akapitów (triple-click)
- Zaznaczanie prostokątne (Alt+drag) — opcjonalne

**Model pozycjonowania kursora:**

Kursor w KML jest reprezentowany przez trzy współrzędne:

```
CursorPosition {
  paragraphIndex: int    // indeks akapitu w dokumencie
  elementIndex: int      // indeks elementu inline w akapicie (0 = tekst główny)
  offset: int            // pozycja znaku w elemencie
}
```

**Przykład:**
```xml
<p style="normal">
  Tekst główny z <em>kursywą</em> i <strong>pogrubieniem</strong>.
</p>
```

| Pozycja | paragraphIndex | elementIndex | offset |
|---------|----------------|--------------|--------|
| "T" w "Tekst" | 0 | 0 | 0 |
| "k" w "kursywą" | 0 | 1 | 0 |
| "p" w "pogrubieniem" | 0 | 2 | 0 |

**Elementy poza głównym przepływem tekstu:**
- Komentarze (`<comment>`) — edytowane w popup, nie wpływają na pozycję kursora w tekście głównym
- Notatki (`<note>`) — jak komentarze
- Przypisy (`<footnote>`) — osobny kursor w treści przypisu

**Nawigacja przez elementy specjalne:**
- Strzałki lewo/prawo pomijają elementy adnotacji
- Tab/Shift+Tab do przeskakiwania między adnotacjami (opcjonalnie)

### 6.3. Clipboard
- Kopiuj/wytnij/wklej z zachowaniem formatowania
- Wklej bez formatowania (Ctrl+Shift+V)
- Wklej i dopasuj do stylu akapitu (match destination style)

### 6.4. Undo/Redo
- Wielopoziomowe undo/redo z grupowaniem zmian typowania
- Undo obejmuje: tekst, formatowanie, adnotacje
- Konfigurowalna głębokość historii (domyślnie: 500 operacji)

### 6.5. Wyszukiwanie i zamiana
- Znajdź: tekst, całe słowa, wielkość liter, regex (opcjonalnie)
- Znajdź w obrębie zaznaczenia
- Zamień: pojedynczo i „zamień wszystko" z podglądem
- Historia wyszukiwań w sesji
- Wyszukiwanie działa na tekście KML z pominięciem tagów (bez osobnego plainText)

### 6.6. Sprawdzanie pisowni

**Architektura:**
- Silnik: Hunspell (standard przemysłowy)
- Debounce: 300ms od ostatniego keystroke
- Dirty paragraphs: śledzenie zmienionych akapitów
- Background thread: sprawdzanie poza głównym wątkiem UI
- Word cache: cache wyników sprawdzania słów

**Implementacja:**
```
SpellCheckService:
  - m_hunspell: Hunspell*           // silnik sprawdzania
  - m_debounceTimer: QTimer         // 300ms debounce
  - m_dirtyParagraphs: QSet<int>    // akapity do sprawdzenia
  - m_wordCache: QHash<QString, bool>  // cache słów (słowo → poprawne?)

  onParagraphChanged(paragraphIndex):
    m_dirtyParagraphs.insert(paragraphIndex)
    m_debounceTimer.start()

  onDebounceTimeout():
    QtConcurrent::run([this]() {
      for each paragraph in m_dirtyParagraphs:
        words = extractWords(paragraph)
        for each word:
          if not m_wordCache.contains(word):
            m_wordCache[word] = m_hunspell->spell(word)
          if not m_wordCache[word]:
            markAsMisspelled(word, paragraph)
      emit checkComplete()
    })
```

**Funkcje:**
- Podkreślanie błędów w locie (czerwona falista linia)
- Słowniki per język + możliwość ustawienia języka per dokument/fragment
- Dodaj do słownika użytkownika, ignoruj, sugestie (context menu)
- Automatyczne przeładowanie słownika przy zmianie języka dokumentu

### 6.7. Sprawdzanie gramatyki (Phase 2)

**Silnik:** LanguageTool (open source, offline możliwy)

**Architektura:**
- Podobna do spellcheckingu: debounce + background + cache
- Sprawdzanie per zdanie (nie per słowo)
- Wyniki cachowane per akapit

**Wizualizacja:**
- Niebieska/zielona falista linia (odróżnienie od spell check)
- Tooltip z wyjaśnieniem i sugestią
- Context menu: popraw, ignoruj, ignoruj regułę

**Typy błędów:**
- Gramatyka (np. "nie" vs "ni")
- Styl (np. strona bierna, zdania za długie)
- Interpunkcja
- Powtórzenia

**Konfiguracja:**
- Włącz/wyłącz per typ błędu
- Poziom surowości (luźny / standardowy / rygorystyczny)
- Własne reguły (advanced)

### 6.8. Analiza częstości słów (Word Frequency)

**Cel:** wykrywanie nadużywanych słów i powtórzeń.

**Funkcje:**
- Automatyczna analiza dokumentu
- Lista najczęściej używanych słów (z wykluczeniem stop words)
- Próg "nadużycia" (konfigurowalne, np. > 0.5% tekstu)
- Podświetlanie w tekście

**Wizualizacja w panelu:**
```
┌─────────────────────────────────────┐
│ CZĘSTOŚĆ SŁÓW                       │
├─────────────────────────────────────┤
│ jednak ████████████ 47x (0.8%)  ⚠️  │
│ bardzo ████████     32x (0.5%)  ⚠️  │
│ powiedział ██████   28x (0.4%)      │
│ spojrzał █████      25x (0.4%)      │
│ ...                                 │
├─────────────────────────────────────┤
│ [Podświetl w tekście] [Eksportuj]   │
└─────────────────────────────────────┘
```

**Akcje:**
- Kliknięcie na słowo → podświetlenie wszystkich wystąpień
- Nawigacja: następne/poprzednie wystąpienie
- Sugestie synonimów (opcjonalnie, integracja ze słownikiem)

### 6.9. Text-to-Speech (Czytanie na głos)

**Cel:** odsłuchanie własnego tekstu — wychwycenie niezręczności brzmieniowych.

**Technologia:** Qt QTextToSpeech (wbudowane w Qt6)

**Funkcje:**
- Czytaj od kursora
- Czytaj zaznaczenie
- Czytaj cały rozdział
- Pauza/Wznów/Stop

**Synchronizacja:**
- Podświetlenie aktualnie czytanego zdania
- Scroll follow (opcjonalny)

**Ustawienia:**
- Głos (systemowe głosy TTS)
- Prędkość (0.5x – 2.0x)
- Auto-pauza na separatorach scen

**Skróty:**
- Ctrl+Shift+Space: Czytaj/Pauza
- Escape: Stop

---

## 7. Formatowanie i style

### 7.1. Formatowanie znakowe (inline)
- Pogrubienie, kursywa, podkreślenie, przekreślenie
- Indeks górny, indeks dolny
- Kolor tekstu i tła
- Dialog formatowania czcionki (font, rozmiar, styl)

### 7.2. Formatowanie akapitowe
- Wyrównanie: lewo, środek, prawo, justowanie
- Wcięcia: pierwsza linia, lewe, prawe
- Odstępy: przed akapitem, po akapicie
- Interlinia: pojedyncza, 1.5, podwójna, własna
- Listy: punktowane, numerowane (z kontynuacją numeracji)

### 7.3. Style (system stylów)

**Style jako „pierwsza klasa":**
- Style akapitu: Normalny, Nagłówek 1/2/3, Cytat, Dialog, Scena, Kod
- Style znakowe: Emphasis, Strong, Myśl bohatera, Nazwa własna
- Zmiana stylu bez niszczenia semantyki dokumentu

**Paleta stylów:**
- Podgląd wizualny stylu
- Zastosuj do zaznaczenia jednym kliknięciem
- Wskaźnik aktualnego stylu kursora

### 7.4. Biblioteka stylów

**Przechowywanie:**
- Tabele `paragraph_styles` i `character_styles` w bazie SQLite projektu (`project.db`)
- Style dziedziczą po stylach bazowych (pole `base_style`)
- Właściwości jako JSON blob (elastyczność)

**Schemat tabel:**
```sql
paragraph_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    base_style TEXT,              -- dziedziczenie
    properties TEXT               -- JSON: fontFamily, fontSize, lineHeight, etc.
)

character_styles (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    properties TEXT               -- JSON: fontStyle, fontWeight, color, etc.
)
```

**Przykład properties (JSON):**
```json
{
  "fontFamily": "Georgia",
  "fontSize": 12,
  "lineHeight": 1.5,
  "marginTop": 0,
  "marginBottom": 6,
  "textIndent": 24,
  "alignment": "justify"
}
```

**Motywy (Themes):**
- Motyw = zestaw stylów (style set)
- Predefiniowane motywy: "Klasyczny", "Nowoczesny", "Manuskrypt"
- Eksport/import motywów jako plik `.ktheme` (JSON dla przenośności)

### 7.5. Struktura dokumentu
- Nagłówki tworzą outline (nawigator/TOC w osobnym panelu)
- Separatory scen jako semantyczny element (nie tekst "***")
  - Jeden element: `<scene-break/>`
  - Wizualizacja zależna od motywu (ornament, ***, linia, pusty wiersz)
  - Mapowanie na format docelowy przy eksporcie

### 7.6. Dialogi i formatowanie mowy

**Zależne od lokalizacji książki:**
- **Polski:** wcięcie akapitowe + dywiz (—), bez cudzysłowów
- **Angielski:** cudzysłowy ("..."), bez wcięcia lub z wcięciem

**Styl "Dialog":**
- Automatyczne formatowanie przy zastosowaniu stylu
- Opcjonalne różne wcięcie dla dialogu vs narracji
- Atrybut `speaker` dla identyfikacji mówiącego

**Konfiguracja:** W ustawieniach książki (Book Properties), zależne od pola języka.

---

## 8. Funkcje dedykowane pisarzom

### 8.1. Statystyki na bieżąco
- Licznik: słowa, znaki (ze spacjami i bez), akapity
- Przybliżony czas czytania
- Licznik kontekstowy: całość dokumentu vs zaznaczenie
- **Rozszerzone:**
  - Czytelność (indeks Flescha-Kincaida lub polski odpowiednik)
  - Tempo narracji (średnia długość zdania/akapitu)
  - Wykrywanie powtórzeń (nadużywane słowa, konfigurowalny próg)
  - Statystyki per scena/rozdział

### 8.2. Cele pisarskie
- Cel dzienny/sesyjny: liczba słów
- Wskaźnik postępu (kontrolka dostarcza metryki, renderowanie w panelu Statistics Bar)
- Powiadomienia o osiągnięciu celu

### 8.3. Sesje pisania (automatyczne z interwałami godzinowymi)

**Działanie w tle:**
- Start automatyczny: przy pierwszym keystroke po otwarciu dokumentu
- Koniec automatyczny: przy zamknięciu dokumentu lub 5 min idle
- Wykrywanie idle vs aktywnego pisania

**Zbierane dane w interwałach godzinowych:**

```
HourlyStats {
  hour: 0-23
  wordsWritten: int
  wordsDeleted: int
  activeMinutes: int  // ile minut faktycznego pisania w tej godzinie
}

DailySession {
  date: Date
  documentId: string
  hourly: HourlyStats[]  // max 24 wpisów
}
```

**Analiza produktywności:**
- Heat map: dni tygodnia × godziny (jak GitHub contributions)
- Identyfikacja najlepszej pory pisania ("Twoja najlepsza pora: wtorki 9:00-11:00")
- Trendy tygodniowe/miesięczne
- Porównanie: poranek vs wieczór, dni robocze vs weekend

**Panel Weekly Statistics:**
- Wizualizacja heat map
- Statystyki zbiorcze per tydzień/miesiąc
- Tempo pisania (słowa/minutę) w różnych porach

**Opcjonalne:**
- Popup podsumowania po zamknięciu dokumentu (można wyłączyć)

**Przechowywanie statystyk:**
- Tabela `session_stats` w bazie SQLite projektu (`project.db`)
- ACID transactions - atomowe zapisy
- Szybkie agregacje przez SQL

**Schemat tabeli:**
```sql
session_stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,      -- ISO 8601
    document_id TEXT,
    words_written INTEGER DEFAULT 0,
    words_deleted INTEGER DEFAULT 0,
    active_minutes INTEGER DEFAULT 0,
    hour INTEGER                  -- 0-23 dla analizy produktywności
)
```

**Zalety SQLite:**
- Szybkie zapytania agregujące (heat map, best hour)
- Transakcje - brak uszkodzonych danych
- Jeden plik zamiast wielu
- Indeksy dla szybkiego wyszukiwania

**Identyfikacja autora:**
- Pobierana z ustawień programu (Author Card w Settings)
- Używana do:
  - Historii rozdziału (`"by": "Anna Kowalska"`)
  - Komentarzy (`author` atrybut)
  - Metadanych eksportu

### 8.4. Nawigacja „Idź do" (Go To)

Skrót: **Ctrl+G** otwiera dialog z zakładkami:

- **Idź do strony X:** skok do strony (w trybie „Strona")
- **Idź do linii Y:** skok do linii
- **Idź do nagłówka/sekcji:** lista nagłówków z szybkim skokiem
- **Idź do sceny:** lista separatorów scen
- **Idź do zakładki:** lista zakładek użytkownika
- **Idź do komentarza:** następny/poprzedni (nierozwiązany)
- **Idź do tagu:** następny TODO/FIX/CHECK
- **Idź do błędu ortograficznego:** następny/poprzedni
- **Wróć do ostatniej edycji:** historia pozycji (Ctrl+Shift+Backspace)

### 8.5. Snapshots (Migawki)

**Cel:** szybkie punkty kontrolne przed dużymi zmianami.

**Tworzenie:**
- Ręcznie: Ctrl+Shift+S
- Automatycznie: przed „Zamień wszystko", przed wklejeniem dużego fragmentu

**Zarządzanie:**
- Lista snapshotów: miniatury tekstowe + timestamps
- Przywracanie: przywróć snapshot jako nowy stan
- Bez diff view — szczegółowe porównanie używa wersjonowania akapitów (osobna funkcjonalność)

**Limit:** 10–20 snapshotów w pamięci (konfigurowalne), najstarsze usuwane automatycznie.

**Uwaga:** Snapshoty są tymczasowe (czas życia: sesja edycji). Pełne wersjonowanie to zadanie hosta.

### 8.6. Tagi inline (TODO/FIX/CHECK)

- Lekkie tagi inline: TODO, FIX, CHECK, RESEARCH (konfigurowalne typy)
- Wizualizacja: kolorowe wyróżnienie w tekście
- Nawigacja: „Idź do następnego tagu"
- Tagi nie wpływają na eksport (metadane)

### 8.7. Zakładki

- Dodaj zakładkę w miejscu kursora (Ctrl+B lub F2)
- Nazwij zakładkę (opcjonalnie)
- Szybka nawigacja między zakładkami
- Lista zakładek w dialogu „Idź do"

### 8.8. Quick Insert / Snippets

**Szybkie wstawianie:**
- **Referencje do postaci:** wpisz `@` + początek imienia → autocomplete z biblioteki Characters
- **Referencje do lokacji:** wpisz `#` + początek nazwy → autocomplete z biblioteki Locations
- **Szablony scen:** konfigurowalny preset tekstu
- **Znaki typograficzne:** skróty dla em-dash, wielokropka, twardej spacji

**Kliknięcie na referencji:** otwiera podgląd elementu z biblioteki.

---

## 9. Funkcje profesjonalne: adnotacje i historia rozdziału

### 9.1. Komentarze

**Dodawanie:**
- Zaznacz tekst → Ctrl+Alt+C → wpisz komentarz
- Komentarz jako element `<comment>` w KML

**Wyświetlanie (konfigurowalne):**
- **Tryb zwinięty:** ikonka ▶ po lewej, kliknięcie rozwija
- **Tryb rozwinięty:** komentarz jako wcięty blok w innym kolorze
- **Tryb popup:** komentarz jako tooltip przy kursorze

**Edycja komentarza:**

1. Kliknięcie na ikonkę komentarza → context menu:
   - Edytuj
   - Oznacz jako rozwiązany
   - Usuń
   - Odpowiedz

2. "Edytuj" lub "Odpowiedz" → popup window:
   ```
   ┌──────────────────────────────────┐
   │ Komentarz                    [×] │
   ├──────────────────────────────────┤
   │ [Treść komentarza z własnym     ]│
   │ [kursorem i edycją             ]│
   │ [                               ]│
   ├──────────────────────────────────┤
   │ [Zapisz] [Usuń] [Rozwiązany ☐]  │
   └──────────────────────────────────┘
   ```

3. Popup ma własny kursor — niezależny od kursora głównego tekstu

**Funkcje:**
- Odpowiadanie na komentarz (wątki)
- Rozwiązywanie (resolve) — komentarz pozostaje, ale oznaczony jako zamknięty
- Filtrowanie: wszystkie / nierozwiązane / moje

**Eksport:** ignorowane lub eksportowane do formatu (konfigurowalnie).

### 9.2. Notatki autora

Podobne do komentarzy, ale inna kolorystyka i cel:
- Komentarze = do dyskusji/rewizji
- Notatki = prywatne przypomnienia autora

### 9.3. Przypisy dolne (footnotes)

- Wstaw przypis: Ctrl+Alt+F
- Edycja treści przypisu inline (w miejscu wstawienia)
- Automatyczna numeracja i renumeracja
- Wyświetlanie: na dole strony (tryb Strona) lub inline (tryb Ciągły)

### 9.4. Cytowania i bibliografia

**Integracja z modułem Bibliografii:**
- Wstaw cytowanie: wybierz źródło z biblioteki → wstawia `<cite id="..."/>`
- Renderowanie: według wybranego stylu (APA, MLA, Chicago, własny)
- Lista bibliografii generowana przez moduł Bibliografii (nie przez kontrolkę)

### 9.5. Historia rozdziału (zamiast inline Track Changes)

**Cel:** informacja o tym kto i kiedy edytował rozdział (na poziomie całego rozdziału, nie inline).

**Metadane w .kchapter:**
```json
"history": [
  { "action": "created", "by": "Anna Kowalska", "at": "2025-12-01T10:00:00Z" },
  { "action": "edited", "by": "Jan Nowak", "at": "2025-12-10T14:30:00Z" },
  { "action": "reviewed", "by": "Maria Wiśniewska", "at": "2025-12-15T09:00:00Z" }
]
```

**Typy akcji:**
- `created` — utworzenie rozdziału
- `edited` — edycja treści
- `reviewed` — redakcja/korekta

**Wyświetlanie w Properties Panel:**
```
┌─────────────────────────────────────┐
│ HISTORIA ROZDZIAŁU                  │
│ ☑ Pokaż historię (konfigurowalne)   │
├─────────────────────────────────────┤
│ Utworzony: Anna Kowalska            │
│            2025-12-01 10:00         │
│                                     │
│ Ostatnia edycja: Jan Nowak          │
│                  2025-12-10 14:30   │
│                                     │
│ Redakcja: Maria Wiśniewska          │
│           2025-12-15 09:00          │
└─────────────────────────────────────┘
```

**Konfiguracja:** Widoczność historii konfigurowalna w ustawieniach edytora.

**Uwaga:** To uproszczona wersja Track Changes — nie śledzimy zmian inline, tylko metadane na poziomie rozdziału. Dla pełnego śledzenia zmian używaj Snapshots + diff.

---

## 10. Integracja z panelami

### 10.1. Panel „Edytor" (View/Tools)

```
┌─────────────────────────────────────┐
│ TRYB WIDOKU                         │
│ [Ciągły] [Strona] [Typewriter]      │
├─────────────────────────────────────┤
│ FOKUS                               │
│ [Off] [Akapit] [Zdanie] [Kontekst]  │
│ Intensywność: [████████░░] 80%      │
├─────────────────────────────────────┤
│ WIDOK                               │
│ ☑ Numery linii    ☑ Spell check     │
│ ☐ Niewidoczne     ☐ Nagłówek/Stopka │
│ ☑ Komentarze      ☐ Historia        │
├─────────────────────────────────────┤
│ ZOOM: [−] 100% [+]                  │
└─────────────────────────────────────┘
```

**Uwaga:** Cel sesji wyświetlany w osobnym panelu Statistics Bar (u góry).

### 10.2. Panel „Style"

```
┌─────────────────────────────────────┐
│ [Motywy] [Style]                    │
├─────────────────────────────────────┤
│ AKTUALNY STYL: Normalny             │
├─────────────────────────────────────┤
│ STYLE AKAPITU          [+ Nowy]     │
│ ├─ Normalny           ▶ [Edytuj]    │
│ ├─ Nagłówek 1         ▶             │
│ ├─ Nagłówek 2         ▶             │
│ ├─ Nagłówek 3         ▶             │
│ ├─ Cytat              ▶             │
│ ├─ Dialog             ▶             │
│ └─ Scena (separator)  ▶             │
├─────────────────────────────────────┤
│ STYLE ZNAKOWE          [+ Nowy]     │
│ ├─ Emphasis (kursywa)               │
│ ├─ Strong (pogrubienie)             │
│ ├─ Myśl bohatera                    │
│ └─ Nazwa własna                     │
├─────────────────────────────────────┤
│ [Eksportuj motyw...] [Importuj...]  │
└─────────────────────────────────────┘
```

**Zakładka Motywy:**
- Predefiniowane zestawy stylów
- Tworzenie własnych motywów z aktualnego zestawu
- Import/eksport motywów

### 10.3. Integracja z bibliotekami

Kontrolka współpracuje z zewnętrznymi modułami:
- **Characters** (postacie)
- **Locations** (miejsca)
- **Items** (przedmioty)
- **Bibliography** (źródła)
- **Index** (indeksy)

**Sposób integracji:**
- Quick Insert (`@`, `#`) z autocomplete
- Kliknięcie na referencji otwiera podgląd/edycję elementu
- Eksport generuje odpowiednie sekcje (spis postaci, bibliografia, indeks)

---

## 11. Ustawienia książki (Book Properties)

### 11.1. Wizard tworzenia książki

**Strona 1:** Podstawowe informacje (obecny dialog)
- Tytuł, autor, opis
- Szablon struktury

**Strona 2:** Szczegółowe ustawienia
- Język książki (wpływa na typografię, formatowanie dialogów)
- Domyślne style
- Ustawienia eksportu

### 11.2. Okno Book Properties

Dostępne z menu: **Book → Properties...**

**Sekcje:**
- **Ogólne:** tytuł, autor, język, opis
- **Formatowanie:**
  - Styl dialogów (polski: dywiz, angielski: cudzysłowy)
  - Smart quotes według języka
  - Domyślne style dla nowych rozdziałów
- **Eksport:** domyślne ustawienia eksportu
- **Metadane:** ISBN, wydawca, kategorie (dla ebook)

**Przy zmianie formatowania:** pytanie "Przeformatować wszystkie dokumenty?" z opcjami:
- Wszystkie dokumenty
- Tylko aktualny dokument
- Tylko zaznaczenie
- Anuluj

---

## 12. Koncepcja operacyjna

### 12.1. Pisanie (Draft)
1. Użytkownik otwiera rozdział w zakładce.
2. Przełącza na „Ciągły" albo „Maszyna do pisania".
3. Pisze, używa skrótów do stylów i tagów TODO.
4. W panelu Statistics Bar widzi postęp i cele.
5. Wstawia referencje do postaci przez `@imię`.

### 12.2. Edycja strukturalna (Rewrite)
1. Przełącza na „Ciągły" lub „Strona".
2. Korzysta z wyszukiwania, zamiany, zakładek, outline.
3. Pracuje na stylach (zmiany globalne, nie ręczne formatowanie).
4. Tworzy snapshoty przed dużymi zmianami.

### 12.3. Redakcja i korekta
1. Dodaje komentarze (zwijane subakapity).
2. Nawiguje po problemach: błędy pisowni, tagi CHECK, nierozwiązane komentarze.
3. Historia rozdziału automatycznie zapisuje kto edytował.

### 12.4. Przygotowanie do eksportu
1. Przełącza na „Strona" dla kontroli paginacji.
2. Sprawdza podziały sekcji, wstawia twarde podziały stron.
3. Weryfikuje: nierozwiązane TODO, komentarze, statusy rozdziałów.
4. Eksport przez moduł eksportu (nie przez kontrolkę).

---

## 13. Architektura kontrolki (decyzje techniczne)

### 13.1. Podejście hybrydowe

Kontrolka z własnym renderingiem, ale wykorzystująca sprawdzone komponenty Qt dla złożonych operacji:

```
┌─────────────────────────────────────────────────────────┐
│                    BookEditor (QWidget)                  │
│                    ═══════════════════                   │
│  Własny kod - pełna kontrola nad UX                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │              KmlDocument (Model)                 │   │
│  │  - Parsowanie KML (QXmlStreamReader)            │   │
│  │  - Drzewo elementów (własna struktura)          │   │
│  │  - Operacje: insert, delete, format             │   │
│  │  - Undo/Redo (QUndoStack)                       │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │           KmlTextLayout (Layout Engine)          │   │
│  │  - QTextLayout dla każdego akapitu              │   │
│  │  - Line breaking, word wrap (Qt)                │   │
│  │  - Pozycje znaków (hit testing)                 │   │
│  │  - Cache layoutów (lazy, on-demand)             │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │            KmlRenderer (View)                    │   │
│  │  - QPainter rendering (własny, pełna kontrola)  │   │
│  │  - Tryby widoku: Continuous, Page, Typewriter   │   │
│  │  - Focus Mode dimming                           │   │
│  │  - Komentarze jako zwijane bloki                │   │
│  │  - Scrollowanie z wirtualizacją                 │   │
│  └─────────────────────────────────────────────────┘   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### 13.2. Wykorzystanie Qt

| Komponent | Źródło | Uzasadnienie |
|-----------|--------|--------------|
| Layout tekstu | `QTextLayout` | Line breaking, glyph positioning, RTL, ligatures |
| Parsowanie KML | `QXmlStreamReader` | Szybki, strumieniowy parser |
| Undo/Redo | `QUndoStack` | Sprawdzony, z merge commands |
| IME | `QInputMethodEvent` | Obsługa języków azjatyckich |
| Clipboard | `QClipboard`, `QMimeData` | Rich text, formaty |
| Spell check | Hunspell | Standard przemysłowy |

### 13.3. Własna implementacja

| Komponent | Uzasadnienie |
|-----------|--------------|
| KmlDocument | Własny format, własna semantyka |
| Renderer | Tryby widoku, Focus Mode, komentarze inline |
| Cursor/Selection | Specyficzna logika dla KML |
| Page layout | Tryb Strona z nagłówkami/stopkami |
| Typewriter scroll | Kursor w centrum |

### 13.4. Lazy Layout (nie Lazy Load)

- **Cały KML ładowany do pamięci** — tekst jest mały (~600KB dla 100k słów)
- **Layout obliczany on-demand** — tylko widoczne akapity + bufor ±2 strony
- **Cache layoutów** — raz obliczone, trzymane w pamięci

```
Viewport: widoczna część dokumentu
Bufor: ±2 kalkulowane strony powyżej/poniżej viewport
Reszta: layout obliczany dopiero przy przewijaniu
```

---

## 14. Kontrakt funkcjonalny (API kontrolki)

### 14.1. Zdarzenia emitowane

| Zdarzenie | Dane | Kiedy |
|-----------|------|-------|
| `contentChanged` | delta | Każda zmiana treści |
| `cursorMoved` | position, line, column | Ruch kursora |
| `selectionChanged` | start, end, text | Zmiana zaznaczenia |
| `statsUpdated` | words, chars, paragraphs | Po każdej zmianie |
| `styleChanged` | styleId | Zmiana stylu kursora |
| `commentAdded` | comment | Dodanie komentarza |
| `snapshotCreated` | snapshot | Utworzenie snapshota |
| `hourlyTick` | hourlyStats | Co godzinę podczas pisania |

### 14.2. Metody publiczne

| Metoda | Opis |
|--------|------|
| `setViewMode(mode)` | Przełącz tryb widoku |
| `setFocusMode(level, intensity)` | Ustaw Focus Mode |
| `getContent()` / `setContent(kml)` | Odczyt/zapis KML |
| `getPlainText()` | Tekst bez znaczników (generowany on-demand) |
| `getStats()` | Aktualne statystyki |
| `find(query, options)` | Wyszukiwanie |
| `replace(query, replacement, options)` | Zamiana |
| `createSnapshot(name)` | Utwórz snapshot |
| `restoreSnapshot(id)` | Przywróć snapshot |
| `insertReference(type, id)` | Wstaw referencję do biblioteki |
| `applyStyle(styleId)` | Zastosuj styl |
| `addComment(text)` | Dodaj komentarz do zaznaczenia |
| `navigateTo(target)` | Nawigacja (page, line, bookmark, tag) |

### 14.3. Konfiguracja

| Opcja | Typ | Domyślnie |
|-------|-----|-----------|
| `spellCheckEnabled` | bool | true |
| `spellCheckLanguage` | string | "pl" |
| `smartTypography` | bool | true |
| `autoSaveInterval` | int (ms) | 30000 |
| `undoLimit` | int | 500 |
| `snapshotLimit` | int | 20 |
| `showChapterHistory` | bool | true |

---

## 15. Wymagania jakościowe

- **Responsywność:** < 16ms dla 60 FPS przy przewijaniu i edycji
- **Skalowalność:** płynna praca do 100k słów (lazy layout dla dużych dokumentów)
- **Stabilność:** brak utraty danych; integracja z autosave
- **Spójność skrótów:** zgodne z konwencjami systemu i aplikacji
- **Dostępność:** pełna obsługa klawiaturą, czytelne focus states
- **Internacjonalizacja:** poprawne zachowanie dla języków z diakrytykami

---

## 16. Pomysły na przyszłość

### 16.1. Narrator Mode (ROADMAP - Future)

Tryb dla pisarzy pracujących z lektorem/audiobook:
- Podświetlenie aktualnie czytanego zdania
- Integracja z TTS (Text-to-Speech)
- Znaczniki dla lektora: pauza, zmiana tonu, wymowa
- Eksport do formatu skryptu lektora

### 16.2. Heat Map aktywności

Kolorowanie tekstu według daty ostatniej edycji — widać które fragmenty są „świeże", a które nietykane od tygodni.

### 16.3. Collaborative editing

Prawdziwa współpraca wielu osób w czasie rzeczywistym (wymaga backendu, CRDTs).

---

## Changelog

| Wersja | Data | Zmiany |
|--------|------|--------|
| 1.0 | 2025-12-18 | Wersja inicjalna |
| 2.0 | 2025-12-18 | Dodano KML, doprecyzowano panele, sesje, Book Properties, API |
| 3.0 | 2025-12-19 | Sesje z interwałami godzinowymi, historia rozdziału zamiast inline Track Changes, architektura hybrydowa, lazy layout, obsługa komentarzy przy usuwaniu tekstu |
| 4.0 | 2025-12-19 | Model pozycjonowania kursora (paragraphIndex + elementIndex + offset), edycja adnotacji w popup, listy w KML (`<ul>`, `<ol>`, `<li>`), elementy media (`<img>`), identyfikacja autora z Settings, szczegóły implementacji spellcheckingu (debounce + dirty paragraphs + Hunspell), uproszczenie snapshotów (tylko restore, bez diff) |
| 4.1 | 2025-12-19 | **Architektura SQLite**: style i statystyki w project.db zamiast osobnych plików, scene-break uproszczony (wizualizacja w motywie), spellcheck na poziomie dokumentu (nie fragmentu) |
| 5.0 | 2025-12-19 | **Nowe funkcje**: Tabele w KML (`<table>`, `<tr>`, `<td>`, `<th>`), Distraction-Free Mode (F11, pełne skupienie), Split View (podział widoku), Sprawdzanie gramatyki (LanguageTool, Phase 2), Analiza częstości słów (wykrywanie powtórzeń), Text-to-Speech (QTextToSpeech). **Filozofia**: zero wersji mobilnych, system dla pisarza zanurzonego w procesie twórczym |
