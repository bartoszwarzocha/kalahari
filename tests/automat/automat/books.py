"""The test books, made fresh for every scenario: the same books as in the user's manual
test kit of 16.11.2026 (Karawana, Wyspa, Opowieści znad oazy, Kroniki podróży).

A book is written the way the program writes it (a .klh manifest in format 2 and one
.kchapter file per text, docs/kml_format.md), so the program opens it like any other book.
The texts are Polish because the program runs in Polish during the tests.
"""

import json
import unicodedata
import uuid
from pathlib import Path
from xml.sax.saxutils import escape

AUTHOR = "Jan Testowy"

# Paragraphs: ("p", text), ("h2", text), ("pusty",) (empty), ("scena",) (scene break) or
# ("kml", the paragraph's <p>…</p> markup, its plain text) for a paragraph with annotations
def P(text):  # noqa: N802 - short on purpose, the texts below use it a lot
    return ("p", text)
EMPTY = ("pusty",)
SCENE_BREAK = ("scena",)

STORM = [
    ("h2", "Przed przełęczą"),
    P("Od rana wiatr zmieniał kierunek co kilka minut, jakby nie mógł się zdecydować, w którą "
      "stronę pchnąć piasek. Tau prowadził karawanę wąskim korytem wyschniętej rzeki, bo tylko "
      "tam wydmy nie zasypywały śladów poprzedniego dnia. Za nim szło jedenaście wielbłądów, "
      "związanych liną jeden za drugim, a na końcu Nadia, która co chwilę odwracała się "
      "i patrzyła na zachód, gdzie niebo robiło się coraz bardziej żółte."),
    P("– Widzisz to? – zawołała, kiedy dogoniła go przy zakręcie. – Tam, nad grzbietem. "
      "Wygląda jak dym, ale się nie rozwiewa."),
    P("Tau nie musiał patrzeć. Czuł to w powietrzu od godziny: ciężkie, suche ciepło, które "
      "nie przychodzi ze słońca, tylko z ziemi, i drobny pył osiadający na rzęsach. Jego ojciec "
      "mówił, że pustynia oddycha przed burzą tak samo jak człowiek przed krzykiem – najpierw "
      "cicho i głęboko, a potem wszystko naraz."),
    P("– Do przełęczy mamy jeszcze cztery godziny – powiedział w końcu. – Jeśli burza przyjdzie "
      "wcześniej, nie zdążymy. Jeśli później, przejdziemy na drugą stronę i schowamy się pod "
      "skałami przy starej studni."),
    P("Nadia przeliczyła bukłaki, choć znała ich liczbę na pamięć. Dwanaście pełnych i trzy "
      "do połowy. Wystarczy na trzy dni, jeśli nikt nie będzie pił więcej, niż trzeba, i jeśli "
      "wielbłądy wytrzymają bez wody do studni. Nie powiedziała tego głośno. Tau i tak wiedział, "
      "a reszta karawany nie musiała jeszcze się bać."),
    P("Szli dalej w milczeniu. Piasek pod nogami robił się coraz drobniejszy i coraz bardziej "
      "miękki, aż każdy krok wymagał tyle wysiłku co dwa. Najmłodszy z poganiaczy, chłopiec "
      "o imieniu Samir, zaczął nucić pod nosem piosenkę o rybaku, który nigdy nie widział "
      "morza, ale urwał w połowie, gdy pierwszy podmuch zasypał mu usta piaskiem."),
    P("Wiatr ucichł nagle, jakby ktoś zamknął drzwi. Wielbłądy stanęły same, bez komendy, "
      "i zaczęły kłaść się na ziemi, podwijając nogi pod siebie. Tau zeskoczył z siodła "
      "i podszedł do pierwszego zwierzęcia, przyłożył dłoń do jego szyi i przez chwilę stał "
      "nieruchomo, słuchając czegoś, czego nikt inny nie słyszał."),
    P("– Nie pójdą dalej – powiedział. – One wiedzą lepiej niż my. Rozładować juki, ustawić je "
      "w krąg od zachodu. Wodę do środka, pod koce. Każdy kładzie się za swoim zwierzęciem i nie "
      "wstaje, dopóki nie powiem."),
    EMPTY,
    P("Zrobili, co kazał, szybko i bez pytań. Nadia owinęła głowę chustą tak, że zostały tylko "
      "szparki na oczy, i przykucnęła za starą wielbłądzicą, która nazywała się Mgła, bo "
      "w młodości miała prawie białą sierść. Teraz sierść była szara od kurzu, tak jak wszystko "
      "wokół: siodła, liny, ręce, nawet niebo."),
    P("Pierwsza fala przyszła z hukiem, który bardziej czuło się w piersi, niż słyszało. Piasek "
      "uderzył w juki jak grad w dach, a potem nie było już ani góry, ani dołu, tylko szum "
      "i ciemność pachnąca kamieniem. Nadia zacisnęła powieki i liczyła oddechy, żeby wiedzieć, "
      "że czas płynie, bo inaczej nie dało się tego poznać."),
    P("Przy trzysta czterdziestym oddechu poczuła, że ktoś szarpie ją za ramię. To był Samir. "
      "Krzyczał coś, ale wiatr zabierał słowa, zanim doleciały. Pokazywał ręką w stronę środka "
      "kręgu, tam gdzie leżały bukłaki. Jeden z koców zerwał się i odfrunął, a pod nim widać "
      "było mokrą plamę, która szybko zmieniała kolor z ciemnego na jasny."),
    P("Nadia rzuciła się na czworakach w tamtą stronę i przycisnęła do piersi pękający bukłak. "
      "Woda przeciekała jej przez palce, ciepła i lepka od piasku. Samir przyniósł drugi koc "
      "i razem przywalili resztę bukłaków własnymi ciałami, czekając, aż burza przejdzie nad "
      "nimi albo zabierze ich ze sobą."),
    P("Nie wiedzieli, ile to trwało. Kiedy wiatr osłabł na tyle, że można było otworzyć oczy, "
      "słońce stało już nisko, czerwone jak rozżarzony węgiel, a krąg juków był w połowie "
      "zasypany. Wielbłądy podnosiły się powoli, otrząsając piasek z grzbietów, i ryczały "
      "cicho, jakby skarżyły się na to, co przeżyły."),
    SCENE_BREAK,
    P("Tau obszedł obóz dwa razy, zanim odezwał się do kogokolwiek. Policzył ludzi, potem "
      "zwierzęta, potem bukłaki. Ludzi było tylu, ilu wyruszyło. Zwierząt też. Bukłaków – "
      "dziewięć pełnych i jeden pęknięty, który Nadia wciąż trzymała w ramionach, jakby to było "
      "dziecko."),
    P("– Trzy dni – powiedział, patrząc na nią. – Mówiłaś, że trzy dni."),
    P("– Mówiłam, że trzy dni, jeśli nikt nie będzie pił więcej, niż trzeba – odpowiedziała. "
      "– Teraz to są dwa. Może dwa i pół, jeśli wielbłądy znajdą coś przy studni."),
    P("Studnia była po drugiej stronie przełęczy, pół dnia drogi od miejsca, w którym stali. "
      "Tau znał ją od dzieciństwa. Pamiętał, jak ojciec opuszczał wiadro na długiej linie i jak "
      "długo trzeba było czekać, zanim z dołu dobiegł plusk. Pamiętał też rok, w którym plusku "
      "nie było, i to, jak cała karawana wracała wtedy do oazy, licząc każdy łyk."),
    P("Wyruszyli przed świtem, kiedy powietrze było jeszcze chłodne, a piasek twardy od nocnej "
      "rosy. Szli szybciej niż poprzedniego dnia, bo wszyscy wiedzieli, że każda godzina "
      "w słońcu kosztuje wodę, której nie mają. Nikt nie nucił. Samir szedł obok Mgły i trzymał "
      "rękę na jej boku, jakby to on prowadził ją, a nie ona jego."),
    P("Przełęcz zobaczyli koło południa: dwa nagie, czerwone zbocza i wąska szczelina między "
      "nimi, w której cień leżał jak woda w misce. Tau zatrzymał karawanę u podnóża i długo "
      "patrzył w górę. Kamienie na ścieżce leżały inaczej niż zwykle, część z nich była świeżo "
      "odłupana, a w jednym miejscu ścieżka znikała pod rumowiskiem."),
    P("– Burza zrzuciła skały – powiedział. – Wielbłądy tędy nie przejdą. Trzeba będzie "
      "rozładować wszystko i przenieść na plecach, a zwierzęta poprowadzić dookoła, starą drogą "
      "przez solnisko. To dzień drogi więcej."),
    P("Nadia spojrzała na bukłaki, potem na niego. Nie musiała nic mówić. Dzień drogi więcej "
      "przez solnisko, w pełnym słońcu, to była cała woda, jaka im została, i jeszcze trochę. "
      "Tau skinął głową, jakby odpowiadał na pytanie, którego nie zadała, i zaczął rozwiązywać "
      "juki pierwszego wielbłąda."),
    P("– Ty poprowadzisz zwierzęta – powiedział, nie odwracając się. – Weź Samira i dwóch "
      "ludzi. My przeniesiemy ładunek przez przełęcz i spotkamy się przy studni jutro "
      "wieczorem. Jeśli nas nie będzie, nie czekajcie. Napełnijcie bukłaki i wracajcie do oazy."),
    P("– A jeśli w studni nie będzie wody? – zapytała."),
    P("Tau zatrzymał się z liną w rękach. Przez chwilę wyglądał, jakby chciał coś powiedzieć, "
      "ale tylko wzruszył ramionami i pociągnął mocniej za węzeł. Wiatr zawiał od zachodu, "
      "słabo, ledwie wyczuwalnie, i przyniósł zapach, którego żadne z nich nie potrafiło nazwać, "
      "choć oboje znali go od dziecka."),
]

WELL = [
    P("Studnia była sucha. Tau opuścił wiadro na linie i długo nasłuchiwał."),
    P("– Nic – powiedział w końcu. – Ani kropli."),
    P("Nadia usiadła na kamiennej cembrowinie i zaczęła liczyć bukłaki, choć wiedziała, że od "
      "rana nic się nie zmieniło. Samir przyprowadził wielbłądy w cień i rozluźnił im popręgi, "
      "a potem przyszedł do niej i usiadł obok, bez słowa."),
    P("Dopiero po zmroku Tau zauważył, że lina ma na końcu ciemną, mokrą plamę."),
]



def short_chapter(title):
    """A short chapter: two paragraphs"""
    return [P(f"{title}. Karawana szła dalej na południe, a wiatr zacierał jej ślady."),
            P("Nikt nie mówił, bo każde słowo kosztowało wodę.")]


# A chapter with words repeated close together, also inflected forms of one word
GORGE = [
    ("h2", "Ognisko"),
    P("Noc w wąwozie była zimna. Samir mówił, że woda wróci do studni przed pełnią. Nadia "
      "mówiła, że woda nie wróci, bo studnia jest stara, a stara studnia zawsze wysycha przed "
      "porą deszczów. Tau mówił mało, ale kiedy mówił, wszyscy słuchali."),
    P("Ogień trzaskał cicho. Wielbłądy leżały w kręgu wokół ognia, a ogień rzucał na ściany "
      "wąwozu długie cienie wielbłądów. Samir dorzucał suche gałęzie i patrzył, jak gałęzie "
      "zmieniają się w popiół."),
    P("– Jutro wyruszymy przed świtem – powiedział Tau. – Przed świtem piasek jest twardy, "
      "a twardy piasek nie męczy zwierząt."),
]

# A chapter with annotations: a comment, a to-do and a note with a long text
NOTES_TIME = "2026-11-12T09:30:00Z"
ANNOTATIONS = (
    f'<annotations>'
    f'<annotation id="uwaga-ghadames" kind="comment" author="{AUTHOR}" created="{NOTES_TIME}">'
    f'Sprawdzić, czy Ghadames leży na tej trasie, czy dalej na zachód.</annotation>'
    f'<annotation id="uwaga-przewodnik" kind="todo" author="{AUTHOR}" created="{NOTES_TIME}">'
    f'Dopisać scenę z przewodnikiem, który zna drogę przez solnisko.</annotation>'
    f'<annotation id="uwaga-sol" kind="note" author="{AUTHOR}" created="{NOTES_TIME}">'
    f'Sól wozi się w płytach, każda płyta waży około trzydziestu kilogramów. Wielbłąd niesie '
    f'cztery płyty, po dwie z każdej strony, owinięte w maty z trawy. Na targu płytę dzieli się '
    f'na mniejsze kawałki, a kupcy sprawdzają, czy sól jest biała i czy nie ma w niej piasku. '
    f'Najlepsza sól pochodzi z kopalni na północy, gdzie wydobywa się ją ręcznie, warstwa po '
    f'warstwie. Karawana z solą idzie wolniej, bo zwierzęta muszą odpoczywać co kilka godzin, '
    f'a w upale nie idzie się wcale. Do sprawdzenia w materiałach: ile dni trwała taka '
    f'droga w dziewiętnastym wieku i ile płyt sprzedawano na jednym targu.</annotation>'
    f'</annotations>'
)
ROAD_NOTES = [
    ("kml",
     '<p>Za trzecim wzgórzem droga skręcała na zachód, w stronę miasta '
     '<anchor ref="uwaga-ghadames">Ghadames</anchor>, gdzie kupcy czekali na sól.</p>',
     "Za trzecim wzgórzem droga skręcała na zachód, w stronę miasta Ghadames, gdzie kupcy "
     "czekali na sól."),
    ("kml",
     '<p>Nikt z karawany nie znał drogi przez solnisko.<anchor ref="uwaga-przewodnik"/> '
     'Tau pamiętał ją tylko z opowieści ojca.</p>',
     "Nikt z karawany nie znał drogi przez solnisko. Tau pamiętał ją tylko z opowieści ojca."),
    ("kml",
     '<p>Każdy wielbłąd niósł <anchor ref="uwaga-sol">cztery płyty soli</anchor>, owinięte '
     'w maty z trawy.</p>',
     "Każdy wielbłąd niósł cztery płyty soli, owinięte w maty z trawy."),
    P("Wieczorem Nadia zapisała w zeszycie, ile wody zostało, i schowała zeszyt pod siodło."),
]

EPILOGUE = [
    P("Po latach Samir prowadził własną karawanę. Kiedy ktoś pytał go o burzę pod przełęczą, "
      "mówił tylko, że woda zawsze jest tam, gdzie się jej szuka najdłużej."),
]

# "Karawana": a title page; in the main section a part with three chapters, seven chapters
# outside it and an epilogue
PART = ("Droga przez wydmy", [("Burza", STORM), ("Studnia", WELL),
                               ("Noc w wąwozie", GORGE)])
OUTSIDE_PART = [("Notatki z drogi", ROAD_NOTES)] + [
    (title, short_chapter(title)) for title in (
        "Karawana wyrusza o świcie", "Targ w Ghadames", "Burza piaskowa nad wydmami",
        "Oaza bez wody", "Powrót do oazy", "Ostatni postój")
]

# "Wyspa": a second novel, with its own paragraph style "Głos wewnętrzny" (id inner_voice)
ISLAND_STYLE = {"id": "inner_voice", "name": "Głos wewnętrzny", "base": "normal",
              "properties": {"italic": True, "leftMargin": 36, "firstLineIndent": 0}}
ISLAND = [
    ("Przypływ", [
        P("Morze przyszło w nocy i zabrało połowę plaży. Rano Ewa zeszła na brzeg i liczyła "
          "deski wyrzucone przez fale."),
        ("styl", "inner_voice", "Nie wrócisz tu już nigdy – powtarzała sobie – a jednak "
                                "wracasz co roku."),
        P("Latarnik pomachał do niej z daleka i zniknął za wydmą."),
    ]),
    ("Latarnia", short_chapter("Latarnia")),
    ("Odpływ", short_chapter("Odpływ")),
]

# "Opowieści znad oazy": a collection of short stories
STORIES = [
    ("Sól", [
        ("h2", "Targ"),
        P("Na targu sprzedawano sól w płytach, a każda płyta miała swoją cenę."),
        P("Kupiec z południa targował się do wieczora i kupił tylko jedną płytę."),
        ("h2", "Droga powrotna"),
        P("Wracał sam, nocą, z płytą soli przywiązaną do siodła."),
    ]),
    ("Cień", short_chapter("Cień")),
]

# A book of a type the program does not have: it opens read-only
FOREIGN_TYPE = "pl.przyklad.kroniki-podrozy"
FOREIGN = [("Pierwszy dzień", short_chapter("Pierwszy dzień")), ("Drugi dzień", short_chapter("Drugi dzień"))]



def _kml(paragraphs, annotations=""):
    """The KML of a text (BookEditor::toKml): annotations first, then the paragraphs."""
    parts = [annotations]
    for paragraph in paragraphs:
        if paragraph[0] == "pusty":
            parts.append("<p></p>")
        elif paragraph[0] == "scena":
            parts.append("<scene-break/>")
        elif paragraph[0] == "kml":
            parts.append(paragraph[1])
        elif paragraph[0] == "styl":
            parts.append(f'<p style="{paragraph[1]}">{escape(paragraph[2])}</p>')
        else:
            parts.append(f"<{paragraph[0]}>{escape(paragraph[1])}</{paragraph[0]}>")
    return "<kml>" + "".join(parts) + "</kml>"


def _plain(paragraphs):
    """The plain text lines of a text (ChapterDocument::kmlToPlainText)."""
    lines = []
    for paragraph in paragraphs:
        if paragraph[0] == "scena":
            continue
        if paragraph[0] == "pusty":
            lines.append("")
        elif paragraph[0] in ("kml", "styl"):
            lines.append(paragraph[2])
        else:
            lines.append(paragraph[1])
    return lines


def _words(lines):
    """Words as the program counts them (core::countText): a word has a letter or a digit."""
    words = 0
    for piece in " ".join(lines).split():
        if any(char.isalnum() for char in piece):
            words += 1
    return words


def _chapter_file(title, paragraphs, moment, annotations=""):
    lines = _plain(paragraphs)
    return {
        "annotations": {"comments": [], "highlights": []},
        "content": {"kml": _kml(paragraphs, annotations), "plainText": "\n".join(lines)},
        "kalahari": {"type": "chapter", "version": "1.0"},
        "metadata": {"notes": "", "status": "draft", "title": title},
        "statistics": {
            "characterCount": sum(len("".join(line.split())) for line in lines),
            "lastModified": moment,
            "paragraphCount": len([line for line in lines if line]),
            "wordCount": _words(lines),
        },
    }


def _write_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=4)
        handle.write("\n")


class _Book:
    """A book as the program writes it (BookProject, .klh format 2)."""

    def __init__(self, folder: Path, name, title, book_type, author, moment):
        self.folder = folder / name
        self.name = name
        self.title = title
        self.type = book_type
        self.author = author
        self.moment = moment
        self.numbers = {}

    def text(self, kind, title, paragraphs, annotations=""):
        short = kind.split(":", 1)[1]
        self.numbers[short] = self.numbers.get(short, 0) + 1
        file = f"book/{short}_{self.numbers[short]:03d}.kchapter"
        _write_json(self.folder / file, _chapter_file(title, paragraphs, self.moment, annotations))
        return {"file": file, "id": str(uuid.uuid4()), "kind": kind, "status": "draft", "title": title}

    def save(self, main, styles=None) -> Path:
        title_page = self.text("kalahari.base:title_page", "Strona tytułowa", [P(self.title), EMPTY, P(self.author)])
        manifest = {
            "books": [{
                "author": self.author, "back": [], "folder": "book", "front": [title_page],
                "genre": "", "id": str(uuid.uuid4()), "language": "pl", "main": main,
                "partsLayer": True, "sections": "sections", "title": self.title,
            }],
            "created": self.moment, "format": 2, "id": str(uuid.uuid4()), "kinds": [],
            "modified": self.moment, "type": {"id": self.type, "version": "1.0"},
            "workshop": {"elements": [], "grouped": False},
        }
        if styles:
            manifest["paragraphStyles"] = styles
        path = self.folder / f"{self.name}.klh"
        _write_json(path, manifest)
        return path


def _karawana(folder, moment):
    book = _Book(folder, "karawana", "Karawana", "kalahari.novel", AUTHOR, moment)
    part_title, part_chapters = PART
    part = {"elements": [book.text("kalahari.base:chapter", t, p) for t, p in part_chapters],
            "id": str(uuid.uuid4()), "kind": "kalahari.base:part", "title": part_title}
    main = [part]
    for title, paragraphs in OUTSIDE_PART:
        main.append(book.text("kalahari.base:chapter", title, paragraphs,
                              ANNOTATIONS if paragraphs is ROAD_NOTES else ""))
    main.append(book.text("kalahari.novel:epilogue", "Epilog", EPILOGUE))
    return book.save(main)


def _simple(folder, name, title, book_type, kind, texts, moment, styles=None, author=AUTHOR):
    book = _Book(folder, name, title, book_type, author, moment)
    return book.save([book.text(kind, t, p) for t, p in texts], styles)


BOOKS = {
    "karawana": lambda folder, moment: _karawana(folder, moment),
    "wyspa": lambda folder, moment: _simple(folder, "wyspa", "Wyspa", "kalahari.novel",
                                            "kalahari.base:chapter", ISLAND, moment, [ISLAND_STYLE],
                                            author="Anna Testowa"),
    "opowiesci": lambda folder, moment: _simple(folder, "opowiesci", "Opowieści znad oazy",
                                                "kalahari.short_stories", "kalahari.short_stories:story",
                                                STORIES, moment),
    "obca-ksiazka": lambda folder, moment: _simple(folder, "obca-ksiazka", "Kroniki podróży", FOREIGN_TYPE,
                                                   "kalahari.base:chapter", FOREIGN, moment,
                                                   author="Piotr Obcy"),
}


def make_books(names, folder: Path, moment: str = "2026-11-12T08:00:00Z") -> dict:
    """Write fresh copies of the books `names` into `folder`; returns {name: .klh path}."""
    made = {}
    for name in names:
        if name not in BOOKS:
            raise ValueError(f"nie ma książki „{name}” w zestawie; są: {', '.join(BOOKS)}")
        made[name] = BOOKS[name](Path(folder), moment)
    return made
