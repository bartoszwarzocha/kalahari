# KALAHARI - Koncepcja Programu

## 1. Wizja i Cel

**Kalahari** to zaawansowane środowisko pisarskie (Writer's IDE) dedykowane autorom książek, oferujące kompleksowe wsparcie całego procesu twórczego - od koncepcji, przez pisanie, po finalną publikację.

### Misja
Stworzyć narzędzie, które pozwala autorom skoncentrować się na twórczości, eliminując bariery techniczne i organizacyjne związane z pisaniem książek.

### Grupa docelowa
- Pisarze powieści (science fiction, fantasy, kryminały, romanse)
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
  - `wx.richtext.RichTextCtrl` dla edytora tekstu
  - `wx.aui` dla zarządzania układem okien
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

### 2.4 Format dokumentów
- **Własny format wewnętrzny** (JSON + embedded resources)
- **RTF** dla edytora rich text
- **Markdown** dla trybu pisania bez rozpraszaczy
- **HTML** jako format pośredni

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
- **OpenAI API** lub **Anthropic Claude API** - asystent AI
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

### 2.6 Struktura projektu (propozycja)

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

### 3.1 Core Features (MVP)

#### A. Zarządzanie projektem książki
- **Kreator nowego projektu**
  - Wizard z wyborem gatunku/typu książki
  - Gotowe szablony struktury (powieść, literatura faktu, historyczna, naukowa)
  - Ustawienia podstawowe (tytuł, autor, język, lokalizacja)

- **Format projektu**
  - Struktura JSON z embedded resources
  - SQLite dla metadanych i wyszukiwania
  - Kompresja do pojedynczego pliku .klh (Kalahari Book Project)

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

##### Tryb Markdown (Distraction-Free)
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

- **Wyszukiwanie i zamiana**
  - Find & Replace z regex
  - Wyszukiwanie w całym projekcie
  - Historia wyszukiwań

#### C. Organizacja warsztatu pisarskiego

##### Bank postaci
- **Karty postaci** z polami:
  - Imię, nazwisko, pseudonimy
  - Wygląd fizyczny (opis, zdjęcia)
  - Osobowość, cechy charakteru
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

- **Mapa interaktywna** (opcjonalnie)
  - Import map (obrazy)
  - Zaznaczanie lokacji
  - Ścieżki podróży postaci

##### Bank motywów/przedmiotów
- **Karty motywów**
  - Nazwa motywu (np. "pierścień", "tajemnica")
  - Opis, znaczenie symboliczne
  - Pojawienia się w tekście
  - Ewolucja motywu

- **Tracking przedmiotów**
  - Gdzie się znajduje dany przedmiot
  - Kto go posiada w danym momencie
  - Graf przepływu przedmiotów

##### Timeline (Oś czasu)
- **Chronologia wydarzeń**
  - Globalna oś czasu książki
  - Wydarzenia dla każdej postaci
  - Synchronizacja wątków
  - Wykrywanie niespójności czasowych

- **Widoki**
  - Liniowa oś czasu
  - Kalendarz
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
  - Słowa napisane dziś
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
- Animacje przy osiąganiu kamieni milowych
- Konfetti przy ukończeniu rozdziału!

#### System osiągnięć (Achievements)
- Odznaki za milestones (pierwsza 1000 słów, 10k, 50k, 100k)
- "Pisarz nocny" - pisanie po 22:00
- "Poranny ptaszek" - pisanie przed 7:00
- "Maraton" - 5k słów w jeden dzień
- "Konsekwentny" - 30 dni z rzędu

### 3.3 Graficzny asystent

#### Koncepcja główna
**JEDEN asystent do wyboru** - użytkownik wybiera JEDNO ulubione zwierzę, które staje się jego osobistym asystentem pisarskim.

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
  - Zmart wiony - gdy długo brak aktywności
  - Ekscytujący - gdy osiągasz cele
  - Śpiący/zmęczony - gdy pracujesz bardzo długo
  - Dumny - przy osiągnięciach
  - Neutralny - stan podstawowy

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

#### Dostępni asystenci - afrykańskie zwierzęta

**Każde zwierzę wykonuje wszystkie podstawowe funkcje asystenta, ale w swoim charakterystycznym stylu!**

##### **Surykatka** (Meerkat) - Przyjazny towarzysz [DOMYŚLNY]
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

##### **Lew** (Lion) - Majestatyczny mentor
- **Osobowość**: majestatyczny, pewny siebie, przywódczy, autorytatywny, wymagający
- **Styl komunikacji**: Silny, autorytatywny, motywujący, stawiający wyzwania
- **Jak przypomina o odpoczynku**: "Nawet król potrzebuje odpoczynku. Wstań i się rozciągnij."
- **Jak gratuluje**: "Godne uznania. Ale możesz jeszcze więcej."
- **Jak wskazuje błąd**: "Ta scena potrzebuje więcej mocy. Nie oszczędzaj emocji!"
- **Przy osiągnięciu celu**: "Ukończyłeś pierwszy akt. Teraz zaczyna się prawdziwe wyzwanie!"

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
- **Okno asystenta**: Małe subokienko (floating lub dokowane na boku)
- **Animacje**: Subtelne animacje głowy:
  - Mruganie (co kilka sekund)
  - Poruszanie głową
  - Zmiana wyrazu twarzy (emocje)
  - Animacja mówienia przy wyświetlaniu tekstu
- **Dymek dialogowy**: Kwestie asystenta w dymku nad głową (comic-style)
- **Personalizacja**:
  - Wybór ulubionego zwierzęcia (JEDEN na start)
  - Możliwość zmiany asystenta w każdej chwili
  - Dostosowanie częstotliwości interwencji (często/normalnie/rzadko)
  - Custom triggers dla przypomnień
  - Wybór trybu: "aktywny" vs "tylko na żądanie"
- **Inteligencja**:
  - Asystent uczy się nawyków użytkownika
  - Dostosowuje komunikaty do pory dnia
  - Zapamiętuje preferencje
  - Nie przerywa w kluczowych momentach (flow state)

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

## 5. Roadmap

### Faza 1: MVP (3-6 miesięcy)
- [ ] Podstawowy edytor tekstu (Rich Text + Markdown)
- [ ] Zarządzanie projektem (tworzenie, zapis, wczytywanie)
- [ ] Bank postaci (podstawowy)
- [ ] Bank miejsc (podstawowy)
- [ ] Biblioteka źródeł (bez OCR, bez AI)
- [ ] Eksport do DOCX, PDF, TXT
- [ ] Statystyki podstawowe
- [ ] Auto-save i backupy

### Faza 2: Rozbudowa (6-9 miesięcy)
- [ ] Graficzny asystent
- [ ] Timeline/oś czasu
- [ ] Planer wątków (wizualizacja)
- [ ] Graf powiązań postaci
- [ ] OCR dla źródeł
- [ ] Sprawdzanie ortografii i gramatyki
- [ ] Eksport do EPUB
- [ ] Moduł notatek ("żółte karteczki")
- [ ] Dark mode
- [ ] Kalendarz pisarza

### Faza 3: Inteligencja (9-12 miesięcy)
- [ ] Integracja AI (OpenAI/Claude)
- [ ] Analiza tekstu z AI
- [ ] Generator nazw
- [ ] Thesaurus i synoniny
- [ ] NotebookLM integration
- [ ] Walidator cytatów
- [ ] Wykrywanie niespójności w fabule

### Faza 4: Współpraca (12-18 miesięcy)
- [ ] Moduł komunikacji z wydawcą
- [ ] System dla beta-readers
- [ ] Online writing sprints
- [ ] Cloud sync (opcjonalnie)
- [ ] Plugin system dla społeczności

---

## 6. Analiza konkurencji

### Scrivener
**Mocne strony:**
- Uznany standard w branży
- Bogata funkcjonalność
- Cork board view

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

## 8. Kwestie do przemyślenia

### 8.1 Model biznesowy
- **Open source** (GPL/MIT) czy **komercyjny**?
- **Freemium** - podstawa free, premium płatne?
- **Jednorazowy zakup** vs **subskrypcja**?
- **Donacje** (Patreon, Buy me a coffee)?

### 8.2 Platforma
- **Windows** (priorytet?)
- **macOS** (duża społeczność pisarzy)
- **Linux** (społeczność open source)
- Budowanie natywnych binarni czy dystryb ucja Python?

### 8.3 Integracja AI
- **Koszt API** - czy użytkownik podaje swój klucz?
- **Lokalne modele** (offline) - Llama, Mistral?
- **Privacy** - czy dane księga idą do OpenAI/Anthropic?

### 8.4 Synchronizacja w chmurze
- Własny backend czy integracja (Dropbox, Google Drive)?
- End-to-end encryption?
- Konfliktowanie plików?

### 8.5 Mobilna aplikacja
- Czy warto robić wersję mobilną (Android/iOS)?
- Jako companion app (czytanie, notatki)?

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
- **Kalahari**: Surykatka (czujny strażnik)
- **Serengeti**: Gnu lub zebra (migracja, współpraca)
- **Okavango**: Hipopotam (zbiera wodę/informacje)
- **Kilimanjaro**: Orzeł (widok z góry, cele)
- **Victoria**: Flaming (różowy kolor, piękno i ochrona)
- **Zambezi**: Krokodyl (silny, skuteczny przepływ)
- **Sahara**: Feniek (mały lis, minimalizm)
- **Ngorongoro**: Nosorożec (siła analityczna)

### Logo i visual identity

#### Logo Kalahari (główne)
- **Koncepcja 1**: Stylizowana wydma z wschodzącym słońcem
- **Koncepcja 2**: Sylwetka afrykańskiego drzewa (baobab/akacja)
- **Koncepcja 3**: Surykatka na czatach (maskotka)
- **Koncepcja 4**: Geometryczna kompozycja z elementami afrykańskimi

#### Paleta kolorów
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

#### Style graficzne
- **Minimalist geometric**: Geometric/polygonal animals (nowoczesne, czyste)
- **Hand-drawn**: Rysunkowe zwierzęta (ciepłe, przyjazne)
- **Realistic**: Fotorealistyczne (profesjonalne, serious)
- **Tribal patterns**: Elementy wzorów plemiennych jako akcenty

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

#### Partnership z ochroną przyrody
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
