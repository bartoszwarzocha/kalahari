# Zarządzanie materiałami – plan wdrożenia

Data: 2026-10-25 · Makieta: [Zarządzanie materiałami](https://claude.ai/artifact/BpyCw6tLnX12k1dV2rc6hG)
(wersja 3 przyjęta w całości 25.10; wersja 4 dokłada otwieranie, karty podglądu i Warsztat według typu
książki; wersja 5 – karty typów, Nawigator w dwóch panelach, kolory ikon i wszystkie warianty jako
ustawienia) · Stan kodu: gałąź materiałów `claude/project-thread-4fuix2` (PR #96, otwarty), commit `86bfedf`.

## Cel

Materiały mają stałe typy (notatki robocze, cytaty, strony internetowe, obrazy, dokumenty, publikacje,
nagrania, inne pliki i typy autora), a praca z nimi idzie w trzech krokach: lista na karcie typu, podgląd
tylko do odczytu we Właściwościach i karta materiału na środku okna. Powiązanie mówi, z czym materiał jest
związany i w jaki sposób. Wszystko działa z klawiatury, w obu motywach i we własnych oknach programu.

## Punkt wyjścia

- Wszystkie materiały to jeden rodzaj „Materiał” (`resources/booktypes/kalahari.base/booktype.json:257–269`),
  rozróżniany tylko po źródle: plik, strona, obraz z sieci, nic (`include/kalahari/gui/material_source.h:23–32`).
- Materiał rozpoznaje się po edytorze „material”, nie po nazwie rodzaju (`include/kalahari/core/book_type_package.h:199–201`),
  więc kilka rodzajów materiałów mieści się w obecnym modelu.
- Nawigator robi grupę dla każdego rodzaju Warsztatu i wstawia do niej każdy element
  (`src/gui/panels/navigator_panel.cpp:1188–1291`). Szybkie dodawanie bierze pierwszy rodzaj materiału z listy
  (`src/gui/navigator_coordinator.cpp:1487–1498`).
- Powiązanie to sam identyfikator elementu (`include/kalahari/core/book_project.h:122`).
- Dwuklik pokazuje w programie tylko obraz; inne pliki idą do programu systemowego
  (`src/gui/navigator_coordinator.cpp:1256–1291`).
- Książka może mieć rodzaje spoza swojego typu (`BookProject::kinds`, `book_project.h:256`, zapis
  `book_project.cpp:1215–1218`), ale wypełnia je tylko książka własnego typu (`project_manager.cpp:524–535`).
- Nawigator to jeden panel w jednym miejscu okna (`src/gui/dock_coordinator.cpp:210–222`); jego sygnały łączy
  około trzydziestu połączeń (`dock_coordinator.cpp:221–253`, `src/gui/main_window.cpp:194–262`).
- Ikona ma dwa kolory motywu: główny i drugi, słabszy odcień tej samej ikony (`src/core/utils/svg_converter.cpp:138–153`,
  `resources/themes/Light.json:7–8`, `Dark.json:7–8`); ikonę w innych kolorach daje
  `ArtProvider::getThemedIcon` (`include/kalahari/core/art_provider.h:133–140`).

## Zasady wspólne

- **Etapy jeden po drugim**, każdy w osobnym PR, z bazą na poprzednim; pierwszy na gałęzi materiałów (#96).
- **Warianty to ustawienia** (decyzja z 25.10): każdą możliwość z makiety autor wybiera w Ustawieniach, w miarę
  możliwości technicznych. Ustawienia domyślne wskazuje autor; makieta pokazuje proponowane.
- **Klawiatura**: każde polecenie w menu, menu podręczne pod klawiszem menu i Shift+F10, okna w całości z
  klawiatury.
- **Okna** tylko własne programu, mieszczące się na ekranie 1366×768 przy skali 150%.
- **Zapis wstecz**: każdy etap czyta książki zapisane wcześniej; to, czego wersja nie zna, zapisuje z powrotem.
- **Każdy PR**: testy (model, zapis i odczyt, Nawigator, okna), cały zestaw testów pod Linuksem, przegląd kodu,
  opis w CHANGELOG, sprawdzenie w programie.

## Etapy

### 1. Typy materiałów w modelu i w Nawigatorze

- Paczka bazowa dostaje rodzaje materiałów: cytat, strona internetowa, obraz, dokument, publikacja, nagranie,
  inny plik – każdy z edytorem „material”, grupą „resources”, nazwami i odmianą. Zestawy Warsztatu wszystkich
  paczek wymieniają je zamiast „Materiału”; stary rodzaj zostaje w paczce z kluczem `"retired": true` – program
  czyta jego elementy, ale nigdzie go nie proponuje.
- Przy otwarciu książki materiały starego rodzaju przechodzą do typów według źródła: obraz z dysku albo z sieci
  – obraz, strona – strona internetowa, plik dźwięku albo filmu – nagranie, plik z tekstem (PDF, Word, ODT,
  RTF, TXT, MD, arkusze, prezentacje) – dokument, inny plik – inny plik, bez pliku i adresu – notatka robocza
  (opis staje się jej tekstem). To samo dotyczy listy rodzajów książki własnego typu.
- Szybkie dodawanie (upuszczenie, wklejenie) wybiera typ według źródła tą samą regułą. W oknie „Dodaj element”
  typ materiału wybiera autor; źródło jest ustawione według typu (strona – adres, cytat – bez pliku, pozostałe
  – plik).
- Nawigator: w Warsztacie stały węzeł „Materiały”, pod nim typy z liczbą, widoczne zawsze, także puste,
  i notatki robocze jako pierwszy typ. Do czasu kart typów (etap 3) typ rozwija się do swoich materiałów.
  Grupa „resources” znaczy teraz węzeł „Materiały”: przy grupowaniu zostaje on na pierwszym poziomie,
  a „Zasoby” znikają. Menu Warsztatu i „Materiałów” ma polecenie „Dodaj notatkę roboczą...”, a typy
  materiałów, wiele równorzędnych, wybiera się w oknie „Dodaj element”; menu grupy typu ma polecenie
  tego typu („Dodaj cytat...”).

### 2. Powiązania z rodzajem, miejscem i notatką

- Powiązanie zapisuje się jako obiekt: element, rodzaj, miejsce w tekście (akapit), notatka. Stary zapis, sam
  identyfikator, program czyta jako powiązanie bez rodzaju.
- Rodzaje na start: cytowany w tekście, źródło informacji, inspiracja, ilustracja, do sprawdzenia; typ
  materiału podpowiada rodzaj.
- Okno powiązań: drzewo z polami wyboru, a po prawej rodzaj, miejsce i notatka wybranego elementu. Lista
  powiązań we Właściwościach i materiały rozdziału pokazują rodzaj i akapit; spinacz na marginesie edytora
  oznacza akapit powiązany z materiałem. Ustawienie „Miejsce w tekście” wyłącza akapit i spinacze.

### 3. Karty typów na środku okna (krok 1)

- Kliknięty w Nawigatorze typ otwiera własną kartę, a otwartą – pokazuje; „Materiały” otwiera kartę „Wszystkie
  materiały”. Ustawienie „Typ kliknięty w Nawigatorze”: własna karta albo jedna karta, której typ się zmienia.
  Każda karta pamięta zaznaczenie, szukanie i filtry.
- Na karcie: tabela z kolumnami typu, sortowanie, szukanie (Ctrl+F), filtry („Powiązane z”, „Bez powiązań”,
  filtry kolumn), pasek narzędzi i menu wiersza; przy obrazach, dokumentach i publikacjach kafelki. Kolumny
  dobiera autor, osobno dla każdego typu.
- Nawigator: typy z liczbą; typy z listą do rozwinięcia zostają ustawieniem.

### 4. Właściwości tylko do odczytu (krok 2) i karta materiału (krok 3)

- Właściwości pokazują podgląd bez pól do wpisywania: cały cytat, tekst notatki, obraz, okładkę z zakładkami,
  dane i powiązania; w prawym górnym rogu „Edytuj”.
- Enter albo „Edytuj” otwiera kartę materiału na środku okna: duże pole tekstu, pola typu (cytat: tekst, autor,
  dzieło, strona), opis, własne pola, powiązania. Zmiany zapisują się same. Notatka robocza otwiera się w tej
  samej karcie.

### 5. Pasek kart

- Jeden wiersz: skracanie długich nazw, lista wszystkich kart, menu paska kart (zamykanie, układ).
- Kilka wierszy: własny pasek kart programu (`QTabBar` układa karty tylko w jednym wierszu).
- Ustawienia › Wygląd › Ogólne, grupa „Karty”. Szybka zmiana układu – każde miejsce osobnym ustawieniem:
  przycisk na końcu paska kart, menu paska kart, podmenu Widok › Karty.

### 6. Ustawienia › Materiały i typy autora

- Nowa strona „Materiały”: Nawigator, karty typów, typy (widoczność, kolejność), otwieranie, rodzaje
  powiązań i miejsce w tekście, eksport i import. Grupa „Materiały” ze strony „Nawigator” przechodzi tutaj.
- Typy autora (np. „Korespondencja”) z nazwą, ikoną i polami; zapisują się w pliku książki.

### 7. Publikacje i cytaty

- Publikacja: okładka (plik, schowek, pierwsza strona PDF) i zakładki – miejsce i notatka.
- Cytat: „Wstaw do rozdziału” stylem „Cytat” (`kalahari.base/styles.json:29`) z powiązaniem „cytowany
  w tekście”; zwykły tekst wklejony na „Cytaty” tworzy cytat.

### 8. Otwieranie i podgląd w programie

- Ustawienie dla każdego rodzaju pliku: obraz, PDF, tekst, dźwięk, film, strona – w programie albo w programie
  systemowym; dokumenty biurowe zawsze w programie systemowym.
- Karty podglądu: PDF (Qt PDF), film i odtwarzacz nagrań ze znacznikami (Qt Multimedia), strona – ustawienie
  „Strona w programie”: kopia uproszczona zapisana z książką albo cała strona (Qt WebEngine).
- Moduły opcjonalne, jak synteza mowy (`CMakeLists.txt:126–129`): program zbudowany bez modułu ma jego pozycje
  wyszarzone i otwiera plik w programie systemowym; bez Qt WebEngine strona w programie to kopia uproszczona.

### 9. Warsztat według typu książki

- „Dodaj do Warsztatu…” dopisuje rodzaj spoza zestawu do `BookProject::kinds`; „Usuń z Warsztatu” – pusty
  rodzaj dodany przez autora.
- Menu grupy: „Nowa postać” itd. (Insert) i „Dodaj…” (Ctrl+Shift+Enter); menu zostaje zawsze, także z paskiem
  Warsztatu (etap 10).
- Ustawienie „Przy grupowaniu Warsztatu”: Teksty, Plany, Biblioteki albo same „Biblioteki”; grupowanie włącza
  się jak dziś, w menu węzła „Warsztat”.

### 10. Nawigator w dwóch panelach

- Ustawienie „Układ Nawigatora”: dwa panele, „Książka” i „Warsztat” (proponowane domyślne), albo jedno drzewo,
  jak dziś. Dane się nie zmieniają; inny jest tylko widok.
- `NavigatorPanel` dostaje zakres: Książka, Warsztat albo całość. Drugi egzemplarz leży w osobnym panelu
  „Warsztat” z własną nazwą obiektu (zapis układu okna) i miejscem w „Przywróć układ paneli”. Położenie ustala
  tylko `DockCoordinator`; kod paneli go nie zakłada, bo układ jest tymczasowy.
- Połączenia sygnałów przechodzą do jednej funkcji wywoływanej dla każdego panelu. Każdy panel ma swój zapis
  rozwiniętych węzłów i swój filtr; podświetla element ten panel, który go ma. Pliki spoza książki zostają
  w panelu Książki.
- Paski narzędzi, każdy osobnym ustawieniem i tylko przy dwóch panelach: Książka – Dodaj…, Przenieś w górę
  i w dół, Przenieś…, Zmień nazwę…, Usuń…, Rozwiń i Zwiń wszystko (polecenia menu węzła); Warsztat – nowy element
  każdej grupy i „Dodaj do Warsztatu…”.
- Polecenie Widok › Panele › Warsztat, proponowany klawisz Shift+F2 (dziś wolny); F2 zostaje przy Książce.
- Możliwe także w jednym drzewie: materiał upuszczony na rozdział dostaje z nim powiązanie (przeciągany węzeł
  niesie już identyfikatory powiązań, `navigator_panel.cpp:359–375`).

### 11. Kolory ikon w Nawigatorze

- Ustawienie „Kolory ikon”: kolor programu (proponowane domyślne), kolor obszaru (Książka, Warsztat) albo kolor
  grupy (Materiały, Biblioteki, Plany, Teksty). Kolory wybiera autor; zapisują się osobno dla każdego motywu.
- Kolor zastępuje kolor główny ikony; drugi odcień program liczy z tego samego koloru, mieszając go z tłem panelu,
  więc drugi kolor motywu nadal jest akcentem ikony. Tekst węzłów ma kolor tekstu motywu.
- Przy kolorze obszaru nagłówek panelu dostaje pasek w tym kolorze. Ikony odświeżają się przy zmianie motywu
  i ustawienia.

### 12. Eksport i import materiałów (propozycja)

- Plik `.kmat` z materiałami, ich plikami, okładkami, zakładkami i typami autora; import z `.kmat` albo z pliku
  `.klh` innej książki, z rozpoznawaniem materiałów, które już są.

## Kolejność i zależności

1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11, eksport i import (12) na końcu. Etapy 5, 10 i 11 nie zależą od
materiałów i mogą pójść wcześniej, jeśli będą pilniejsze; pasek Warsztatu z etapu 10 korzysta z poleceń etapu 9.
PR #96 trzeba scalić przed pierwszym z nich; ma dziś konflikt z gałęzią główną.
