# Import projektu Scrivenera – plan wdrożenia

Data: 2026-11-16 · Makieta: [Import tekstu](https://claude.ai/artifact/V3oiYG3NPHEESrMhh6Co3M), część
„Scrivener”. Plan uzupełnia import dokumentów Worda, plików Markdown i zwykłego tekstu (PR „Import tekstu”
(#151)) i trafia do osobnego PR.

## Cel

Plik › Importuj › Projekt Scrivenera... wczytuje projekt Scrivenera do książki tym samym oknem importu co
pozostałe formaty. Szuflada „Draft”/„Manuscript” daje części, rozdziały i sceny, a dokumenty zachowują to,
co autor w nich zapisał: tekst z formatowaniem i stylami, streszczenie, notatki, stan, etykietę, słowa
kluczowe i migawki. Folder „Research” może trafić do materiałów książki.

## Punkt wyjścia

- Okno importu i jego plan znają formaty Worda, Markdown, zwykłego tekstu i wtyczek
  (`include/kalahari/gui/book_import_plan.h:37–42`); teksty powstają z bloków jednego dokumentu
  (`editor::ImportDocument`, `include/kalahari/editor/book_import.h:228`) podzielonego na części i teksty
  (`splitDocument`, `:381`).
- Polecenie „Projekt Scrivenera...” jest w menu jako zapowiedź fazy 2 (`src/gui/command_registrar.cpp:272`).
- Program nie czyta RTF; Scrivener zapisuje w nim tekst każdego dokumentu.
- Książka ma to, czego potrzebują dane Scrivenera: stan tekstu (`ProjectManager::setStatus`), opis
  materiału (`setDescription`), notatki pliku rozdziału (`src/core/chapter_document.cpp:225`), wersje tekstu
  (`core::ChapterVersions::add`, `include/kalahari/core/chapter_versions.h:160`) i materiały z plikami
  (`ProjectManager::addMaterial`, `include/kalahari/core/project_manager.h:510`).

## Format projektu

Scrivener nie publikuje opisu formatu. Zgodne w niezależnych narzędziach, które czytają jego projekty:
- projekt to folder `Nazwa.scriv` z plikiem `Nazwa.scrivx` (XML: `ScrivenerProject` › `Binder` ›
  zagnieżdżone `BinderItem` z atrybutami `UUID` i `Type`: `DraftFolder`, `ResearchFolder`, `TrashFolder`,
  `Folder`, `Text` i rodzaje materiałów; w środku `Title`, `MetaData`, `Children`);
- tekst dokumentu w `Files/Data/<UUID>/content.rtf`, streszczenie w `synopsis.txt`, notatki w `notes.rtf`;
  materiały w tym samym folderze jako `content.<rozszerzenie>`;
- migawki w `Snapshots/<UUID>.snapshots/`: `index.xml` (`Snapshots` › `Snapshot` z `Title` i `Date`)
  i po jednym pliku RTF na migawkę;
- etykiety i stany w `LabelSettings` › `Labels` › `Label ID="…"` oraz `StatusSettings` › `StatusItems` ›
  `Status ID="…"`; dokument wskazuje je w `MetaData` (`LabelID`, `StatusID`), a do kompilacji bierze go
  `IncludeInCompile` (brak oznacza „tak”).

Niepewne (źródła się różnią): zapis słów kluczowych dokumentu, komentarzy i przypisów połączonych
(`content.comments`) oraz adnotacji i przypisów w tekście RTF. Czytnik traktuje je ostrożnie: zna kilka
zapisów, a nieznane grupy RTF pomija bez utraty zwykłego tekstu. **Przed scaleniem potrzebny jest mały
prawdziwy projekt Scrivenera 3** (z komentarzem, przypisem, adnotacją, migawką, streszczeniem, notatką,
stanem, etykietą, słowem kluczowym i obrazem w „Research”), żeby sprawdzić te zapisy na pliku, a nie na
domysłach. Scrivener 2 (`Files/Docs/<ID>.rtf`) – tylko gdy taki projekt się trafi.

## Etapy

1. **Czytnik RTF** (`editor/rtf_reader`): akapity, wyrównanie, wcięcia, pogrubienie, kursywa,
   podkreślenie, przekreślenie, indeks górny i dolny, nazwane style z arkusza stylów, łamanie wiersza,
   tabulator, podział strony, Unicode i strony kodowe, obrazy PNG i JPEG, przypisy `\footnote`, łącza;
   nieznane grupy `{\*…}` pominięte. Daje `ImportDocument`, więc porządki, style i obrazy działają jak
   w Wordzie.
2. **Czytnik projektu** (`editor/scrivener_reader`): szuflada z metadanymi, etykiety, stany, słowa kluczowe,
   streszczenia, notatki, migawki i „Research”; z szuflady jeden `ImportDocument` z nagłówkami: folder
   folderów to część, folder dokumentów to rozdział z jego scenami (między scenami podział sceny),
   dokument na poziomie części albo szuflady to rozdział. Kosz i ustawienia kompilacji pominięte; tylko
   to, co jest „Include in Compile” (opcja).
3. **Okno i polecenie**: format „Projekt Scrivenera (.scrivx)”, wybór pliku `.scrivx` albo folderu
   `.scriv`, zakładka „Notatki i materiały” (streszczenia na początku notatek – opisu tekstu program nie
   pokazuje, notatki jako notatki tekstów, migawki
   jako wersje, stany: „To Do” i „First Draft” – Szkic, „Revised Draft” – Poprawki, „Final Draft” i „Done” –
   Gotowy, etykieta i słowa kluczowe na koniec notatek, folder „Research” jako materiały), grupa
   w Ustawieniach › Pliki › Import, wynik z tym, co import pominął (Kosz, dokumenty bez „Include in
   Compile”).
4. **Tłumaczenia, testy, lista testów**: projekty testowe zbudowane w testach (XML i RTF), sprawdzenie na
   prawdziwym projekcie, gdy będzie, analiza SonarQube i test na Windows.
