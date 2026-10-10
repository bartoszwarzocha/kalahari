/// @file section_words.cpp
/// @brief How the program's sentences name the front, main and back part of a book

#include "kalahari/gui/section_words.h"
#include "kalahari/core/book_project.h"

namespace kalahari {
namespace gui {

SectionWords SectionWords::forPart(const core::ProjectBook* book, core::BookPlace place)
{
    using core::BookPlace;

    // A book without sections: the parts are named after their place in the book
    if (book && !book->partsLayer) {
        switch (place) {
        case BookPlace::Front:
            //: A book without sections, its first elements (a title page, a dedication):
            //: "The item is added as the last one at the beginning of the book". In Polish:
            //: na początku książki
            return {tr("at the beginning of the book"),
                    //: A book without sections, the first of its first elements. In Polish: na
                    //: samym początku książki
                    tr("at the very beginning of the book"),
                    //: A book without sections, the last of its first elements: just before its
                    //: chapters. In Polish: przed treścią książki
                    tr("before the content of the book")};
        case BookPlace::Main:
            //: A book without sections, its chapters: "The chapter is added as the last one in
            //: the content of the book". In Polish: w treści książki
            return {tr("in the content of the book"),
                    //: In Polish: na początku treści książki
                    tr("at the start of the content of the book"),
                    //: In Polish: na końcu treści książki
                    tr("at the end of the content of the book")};
        case BookPlace::Back:
            //: A book without sections, its last elements (an afterword, notes). In Polish: na
            //: końcu książki
            return {tr("at the end of the book"),
                    //: A book without sections, the first of its last elements: just after its
                    //: chapters. In Polish: za treścią książki
                    tr("after the content of the book"),
                    //: In Polish: na samym końcu książki
                    tr("at the very end of the book")};
        case BookPlace::Workshop:
            return {};
        }
    }

    // The writer's own names, quoted after the word for a section
    if (book && book->sectionSet == QLatin1String(core::ProjectBook::CUSTOM_SECTIONS)) {
        const QString own = book->sectionName(place);
        //: The writer's own name of a part of the book: "The chapter is added as the last one
        //: in the section "Story"". In Polish: w sekcji „%1”
        return {tr("in the section \"%1\"").arg(own),
                //: In Polish: na początku sekcji „%1”
                tr("at the start of the section \"%1\"").arg(own),
                //: In Polish: na końcu sekcji „%1”
                tr("at the end of the section \"%1\"").arg(own)};
    }

    // A set of names: the program's words for its names
    const QString set = book ? book->sectionNameSet().id
                             : core::ProjectBook::sectionNameSets().first().id;
    if (set == QLatin1String("matter")) {
        switch (place) {
        case BookPlace::Front:
            //: Set "Front Matter, Body, Back Matter". In Polish (Strony początkowe): na
            //: stronach początkowych
            return {tr("in the front matter"),
                    //: In Polish: na początku stron początkowych
                    tr("at the start of the front matter"),
                    //: In Polish: na końcu stron początkowych
                    tr("at the end of the front matter")};
        case BookPlace::Main:
            //: Set "Front Matter, Body, Back Matter". In Polish (Tekst główny): w tekście
            //: głównym
            return {tr("in the body"),
                    //: In Polish: na początku tekstu głównego
                    tr("at the start of the body"),
                    //: In Polish: na końcu tekstu głównego
                    tr("at the end of the body")};
        case BookPlace::Back:
            //: Set "Front Matter, Body, Back Matter". In Polish (Strony końcowe): na stronach
            //: końcowych
            return {tr("in the back matter"),
                    //: In Polish: na początku stron końcowych
                    tr("at the start of the back matter"),
                    //: In Polish: na końcu stron końcowych
                    tr("at the end of the back matter")};
        case BookPlace::Workshop:
            return {};
        }
    }
    if (set == QLatin1String("fragments")) {
        switch (place) {
        case BookPlace::Front:
            //: Set "Opening Fragment, Main Fragment, Closing Fragment". In Polish: we
            //: fragmencie początkowym
            return {tr("in the opening fragment"),
                    //: In Polish: na początku fragmentu początkowego
                    tr("at the start of the opening fragment"),
                    //: In Polish: na końcu fragmentu początkowego
                    tr("at the end of the opening fragment")};
        case BookPlace::Main:
            //: Set "Opening Fragment, Main Fragment, Closing Fragment". In Polish: we
            //: fragmencie głównym
            return {tr("in the main fragment"),
                    //: In Polish: na początku fragmentu głównego
                    tr("at the start of the main fragment"),
                    //: In Polish: na końcu fragmentu głównego
                    tr("at the end of the main fragment")};
        case BookPlace::Back:
            //: Set "Opening Fragment, Main Fragment, Closing Fragment". In Polish: we
            //: fragmencie końcowym
            return {tr("in the closing fragment"),
                    //: In Polish: na początku fragmentu końcowego
                    tr("at the start of the closing fragment"),
                    //: In Polish: na końcu fragmentu końcowego
                    tr("at the end of the closing fragment")};
        case BookPlace::Workshop:
            return {};
        }
    }
    if (set == QLatin1String("arc")) {
        switch (place) {
        case BookPlace::Front:
            //: Set "Opening, Development, Closing". In Polish: w otwarciu
            return {tr("in the opening"),
                    //: In Polish: na początku otwarcia
                    tr("at the start of the opening"),
                    //: In Polish: na końcu otwarcia
                    tr("at the end of the opening")};
        case BookPlace::Main:
            //: Set "Opening, Development, Closing". In Polish: w rozwinięciu
            return {tr("in the development"),
                    //: In Polish: na początku rozwinięcia
                    tr("at the start of the development"),
                    //: In Polish: na końcu rozwinięcia
                    tr("at the end of the development")};
        case BookPlace::Back:
            //: Set "Opening, Development, Closing". In Polish: w zamknięciu
            return {tr("in the closing"),
                    //: In Polish: na początku zamknięcia
                    tr("at the start of the closing"),
                    //: In Polish: na końcu zamknięcia
                    tr("at the end of the closing")};
        case BookPlace::Workshop:
            return {};
        }
    }

    // The first set: sections
    switch (place) {
    case BookPlace::Front:
        //: Set "Front Section, Main Section, Back Section". In Polish: w sekcji początkowej
        return {tr("in the front section"),
                //: In Polish: na początku sekcji początkowej
                tr("at the start of the front section"),
                //: In Polish: na końcu sekcji początkowej
                tr("at the end of the front section")};
    case BookPlace::Main:
        //: Set "Front Section, Main Section, Back Section". In Polish: w sekcji głównej
        return {tr("in the main section"),
                //: In Polish: na początku sekcji głównej
                tr("at the start of the main section"),
                //: In Polish: na końcu sekcji głównej
                tr("at the end of the main section")};
    case BookPlace::Back:
        //: Set "Front Section, Main Section, Back Section". In Polish: w sekcji końcowej
        return {tr("in the back section"),
                //: In Polish: na początku sekcji końcowej
                tr("at the start of the back section"),
                //: In Polish: na końcu sekcji końcowej
                tr("at the end of the back section")};
    case BookPlace::Workshop:
        break;
    }
    return {};
}

QString SectionWords::capitalized(const QString& phrase)
{
    if (phrase.isEmpty()) {
        return phrase;
    }
    return phrase.left(1).toUpper() + phrase.mid(1);
}

} // namespace gui
} // namespace kalahari
