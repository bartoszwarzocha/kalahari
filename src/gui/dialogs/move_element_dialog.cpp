/// @file move_element_dialog.cpp
/// @brief Move: a new place in the book for an element, with the elements inside it

#include "kalahari/gui/dialogs/move_element_dialog.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/section_words.h"

#include <QHash>
#include <QPushButton>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// Element @p id in @p elements or inside their groups, at any depth; nullptr when none is
const core::ProjectElement* findIn(const QList<core::ProjectElement>& elements,
                                   const QString& id) {
    for (const core::ProjectElement& element : elements) {
        if (element.id == id) {
            return &element;
        }
        if (const core::ProjectElement* inside = findIn(element.elements, id)) {
            return inside;
        }
    }
    return nullptr;
}

} // anonymous namespace

MoveElementDialog::Book MoveElementDialog::openBook(const QString& elementId) {
    Book book;
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    if (!project || !projectBook) {
        return book;
    }
    const std::optional<core::ElementPlace> from = project->placeOf(elementId);
    if (!from || from->place == core::BookPlace::Workshop) {
        return book;
    }

    // The book without the element: the places are in it
    core::BookProject without = *project;
    std::optional<core::ProjectElement> element = without.takeElement(elementId);
    if (!element) {
        return book;
    }
    book.element = std::move(*element);
    book.from = *from;
    for (const core::BookPlace place :
         {core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back}) {
        // A book without sections has no rows of its parts
        book.sections.append({place,
                              projectBook->partsLayer ? projectBook->sectionName(place)
                                                      : QString(),
                              SectionWords::forPart(projectBook, place),
                              without.elementsIn(place)});
    }
    return book;
}

MoveElementDialog::MoveElementDialog(const Book& book, const core::BookTypeRegistry& registry,
                                     QWidget* parent)
    : KalahariDialog(parent)
    , m_book(book)
    , m_registry(&registry)
{
    //: The heading of the dialog; %1: the title of the element. In Polish: Przenieś „%1”
    setHeading(tr("Move \"%1\"").arg(m_book.element.title),
               //: In Polish: Wybierz nowe miejsce. Lista pokazuje {kind:accusative} tam,
               //: gdzie {kind:s=trafi|p=trafią}.
               named(tr("Choose the new place. The list shows the {kind} where "
                        "{kind:s=it goes|p=they go}.")));
    //: The title of the window. In Polish: Przenieś
    setWindowTitle(tr("Move"));
    setHeadingIcon(QStringLiteral("common.driveFileMove"));

    // The book with the element at the chosen place, and the elements inside it
    m_picker = new ElementPlacePicker(this);
    m_picker->setBook(m_book.sections, registry);
    m_picker->setElement(core::BookProject::kindOf(registry, m_book.element),
                         m_book.element.title, m_book.element.elements);
    m_picker->setOptions(edgeOptions());
    m_picker->setMovedFrom(m_book.from);
    m_picker->setPlace(m_book.from);
    //: Under the list of the book; %1: where the element is, "at the end of the main
    //: section". In Polish: Obecne miejsce: %1.
    m_picker->setNote(tr("Current place: %1.").arg(placeText(m_book.from)));
    contentLayout()->addWidget(m_picker, 1);
    connect(m_picker, &ElementPlacePicker::placeChanged, this,
            &MoveElementDialog::updateAcceptButton);

    //: The button that moves the element. In Polish: Przenieś
    setAcceptText(tr("Move"));
    updateAcceptButton();

    // Up and Down move the element at once
    m_picker->focusList();
}

core::ElementPlace MoveElementDialog::place() const
{
    return m_picker->place();
}

QString MoveElementDialog::placeText(const core::ElementPlace& place) const
{
    if (!place.groupId.isEmpty()) {
        const core::ProjectElement* group = groupOf(place.groupId);
        if (!group) {
            return QString();
        }
        if (group->elements.isEmpty()) {
            //: Where an element is: in a group without other elements; %1: the title of the
            //: group. In Polish: w {group:locative} „%1”
            return named(tr("in the {group} \"%1\""), group).arg(group->title);
        }
        if (place.index >= group->elements.size()) {
            //: In Polish: na końcu {group:genitive} „%1”
            return named(tr("at the end of the {group} \"%1\""), group).arg(group->title);
        }
        if (place.index == 0) {
            //: In Polish: na początku {group:genitive} „%1”
            return named(tr("at the start of the {group} \"%1\""), group).arg(group->title);
        }
        //: %1: the title of the group; %2: the title of the element after the place. In
        //: Polish: w {group:locative} „%1”, przed elementem „%2”
        return named(tr("in the {group} \"%1\", before \"%2\""), group)
            .arg(group->title, group->elements.at(place.index).title);
    }

    const PickerSection* section = sectionOf(place.place);
    if (!section) {
        return QString();
    }
    if (section->elements.isEmpty()) {
        return section->words.inPart;
    }
    if (place.index >= section->elements.size()) {
        return section->words.atEnd;
    }
    if (place.index == 0) {
        return section->words.atStart;
    }
    //: %1: a part of the book with its preposition, "in the main section"; %2: the title of
    //: the element after the place. In Polish: %1, przed elementem „%2”
    return tr("%1, before \"%2\"")
        .arg(section->words.inPart, section->elements.at(place.index).title);
}

QList<PlaceOption> MoveElementDialog::edgeOptions() const
{
    QList<PlaceOption> options;
    const core::KindRef kind = core::BookProject::kindOf(*m_registry, m_book.element);
    if (!kind || kind.kind->position == core::KindPosition::Any) {
        return options;
    }

    // A prologue at the start of its part of the book or first in its first group; an
    // epilogue at the end or last in its last group
    const bool start = kind.kind->position == core::KindPosition::Start;
    for (const PickerSection& section : std::as_const(m_book.sections)) {
        const core::ElementPlace edge{section.place, QString(),
                                      start ? 0 : section.elements.size()};
        if (m_picker->canBeAt(edge)) {
            options.append({SectionWords::capitalized(start ? section.words.atStart
                                                            : section.words.atEnd),
                            edge});
        }
        const core::ProjectElement* group = nullptr;
        for (const core::ProjectElement& element : section.elements) {
            if (m_picker->canBeAt({section.place, element.id, 0}) && (!start || !group)) {
                group = &element;
            }
        }
        if (group) {
            //: In Polish: Jako pierwszy element {group:genitive} „%1”
            const QString first = tr("First in the {group} \"%1\"");
            //: In Polish: Jako ostatni element {group:genitive} „%1”
            const QString last = tr("Last in the {group} \"%1\"");
            options.append({named(start ? first : last, group).arg(group->title),
                            {section.place, group->id, start ? 0 : group->elements.size()}});
        }
    }
    return options;
}

QString MoveElementDialog::named(const QString& text, const core::ProjectElement* group) const
{
    QHash<QString, core::KindWords> nouns{
        {QStringLiteral("kind"), wordsOf(*m_registry, m_book.element)}};
    if (group) {
        nouns.insert(QStringLiteral("group"), wordsOf(*m_registry, *group));
    }
    return core::fillWords(text, nouns);
}

const core::ProjectElement* MoveElementDialog::groupOf(const QString& id) const
{
    for (const PickerSection& section : m_book.sections) {
        if (const core::ProjectElement* group = findIn(section.elements, id)) {
            return group;
        }
    }
    return nullptr;
}

const PickerSection* MoveElementDialog::sectionOf(core::BookPlace place) const
{
    for (const PickerSection& section : m_book.sections) {
        if (section.place == place) {
            return &section;
        }
    }
    return nullptr;
}

void MoveElementDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!m_picker->places().isEmpty() &&
                               !(m_picker->place() == m_book.from));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
