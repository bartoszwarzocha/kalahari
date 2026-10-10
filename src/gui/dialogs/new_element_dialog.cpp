/// @file new_element_dialog.cpp
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, or an item of
/// the front or back section

#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/widgets/element_place_picker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int NOTICE_ICON_SIZE = 16;  ///< Size of the icon of the note about the place

/// The part of the book an element of @p kind goes to
core::BookPlace partOf(NewElementKind kind) {
    switch (kind) {
    case NewElementKind::FrontMatterItem:
        return core::BookPlace::Front;
    case NewElementKind::BackMatterItem:
        return core::BookPlace::Back;
    case NewElementKind::Chapter:
    case NewElementKind::Part:
        break;
    }
    return core::BookPlace::Main;
}

/// Element @p id of @p elements, at any depth
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

/// @p elements without the elements @p ids, at any depth
QList<core::ProjectElement> without(QList<core::ProjectElement> elements,
                                    const QStringList& ids) {
    elements.removeIf(
        [&ids](const core::ProjectElement& element) { return ids.contains(element.id); });
    for (core::ProjectElement& element : elements) {
        element.elements = without(element.elements, ids);
    }
    return elements;
}

QStringList titlesOf(const QList<core::ProjectElement>& elements) {
    QStringList titles;
    for (const core::ProjectElement& element : elements) {
        titles << element.title;
    }
    return titles;
}

QStringList idsOf(const QList<core::ProjectElement>& elements) {
    QStringList ids;
    for (const core::ProjectElement& element : elements) {
        ids << element.id;
    }
    return ids;
}

} // anonymous namespace

NewElementDialog::NewElementDialog(NewElementKind kind, const QList<NewElementChoice>& choices,
                                   int current, const QString& groupTitle, QWidget* parent)
    : KalahariDialog(parent)
    , m_dialogKind(kind)
    , m_groupTitle(groupTitle)
    , m_choices(choices)
    , m_current(choices.isEmpty() ? -1 : qBound(0, current, static_cast<int>(choices.size()) - 1))
    , m_part(partOf(kind))
    , m_words(SectionWords::forPart(nullptr, partOf(kind)))
{
    switch (kind) {
    case NewElementKind::Chapter:
        setHeadingIcon(QStringLiteral("template.chapter"));
        break;
    case NewElementKind::Part:
        setHeadingIcon(QStringLiteral("structure.part"));
        break;
    case NewElementKind::FrontMatterItem:
        setHeadingIcon(QStringLiteral("structure.frontmatter"));
        break;
    case NewElementKind::BackMatterItem:
        setHeadingIcon(QStringLiteral("structure.backmatter"));
        break;
    }

    // With more than one kind the writer chooses it; kinds are named in the program's language
    if (m_choices.size() > 1) {
        const QString language =
            QString::fromStdString(core::SettingsManager::getInstance().getLanguage());
        m_kindBox = new QComboBox(this);
        for (const NewElementChoice& choice : std::as_const(m_choices)) {
            m_kindBox->addItem(choice.kind ? choice.kind.kind->name.text(language) : QString());
        }
        m_kindBox->setCurrentIndex(m_current);
        addField(tr("Kind"), m_kindBox);
        connect(m_kindBox, &QComboBox::currentIndexChanged, this, &NewElementDialog::onKindChanged);
    }

    m_titleEdit = new QLineEdit(m_current >= 0 ? m_choices.at(m_current).title : QString(), this);
    m_titleEdit->selectAll();
    addField(tr("Title"), m_titleEdit);
    connect(m_titleEdit, &QLineEdit::textEdited, this, [this]() { m_titleChanged = true; });
    connect(m_titleEdit, &QLineEdit::textChanged, this, &NewElementDialog::updateAcceptButton);
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() { m_picker->setTitle(title()); });

    // A new part takes inside it the elements that close the body, if the writer wants
    m_takeBox = new QCheckBox(this);
    m_takeBox->setChecked(true);
    m_takeBox->hide();
    contentLayout()->addWidget(m_takeBox);
    connect(m_takeBox, &QCheckBox::toggled, this, &NewElementDialog::updatePart);

    // What the dialog notes about the place
    m_noticeBox = new QWidget(this);
    auto* noticeLayout = new QHBoxLayout(m_noticeBox);
    noticeLayout->setContentsMargins(0, 0, 0, 0);
    noticeLayout->setSpacing(8);
    m_noticeIcon = new QLabel(m_noticeBox);
    m_noticeIcon->setAlignment(Qt::AlignTop);
    noticeLayout->addWidget(m_noticeIcon);
    m_noticeText = new QLabel(m_noticeBox);
    m_noticeText->setTextFormat(Qt::PlainText);
    m_noticeText->setWordWrap(true);
    noticeLayout->addWidget(m_noticeText, 1);
    m_noticeBox->hide();
    contentLayout()->addWidget(m_noticeBox);

    // The place: the part of the book with the new element in it. A new part only shows
    // the body as it will be.
    m_picker = new ElementPlacePicker(this);
    if (kind == NewElementKind::Part) {
        m_picker->setLabel(tr("After adding the part"));
        m_picker->setPreview(true);
    }
    contentLayout()->addWidget(m_picker, 1);
    m_previewNote = new QLabel(tr("The list only shows how the book will look."), this);
    m_previewNote->setTextFormat(Qt::PlainText);
    m_previewNote->setWordWrap(true);
    m_previewNote->hide();
    contentLayout()->addWidget(m_previewNote);

    // The list takes the room of a resized dialog; without it the room stays under the fields
    m_pickerStretch = contentLayout()->count();
    contentLayout()->addStretch(1);
    m_picker->hide();

    setAcceptText(tr("Add"));
    updateAcceptButton();
    updatePlaces();

    // The writer starts with what the element is: its kind, or straight away its title
    if (m_kindBox) {
        m_kindBox->setFocus();
    } else {
        m_titleEdit->setFocus();
    }
}

void NewElementDialog::setSection(const QList<core::ProjectElement>& elements,
                                  const QString& name, const SectionWords& words,
                                  const core::BookTypeRegistry& registry,
                                  const QString& groupId)
{
    m_elements = elements;
    m_sectionName = name;
    m_words = words;
    m_registry = &registry;
    m_openedOn = groupId;
    updatePlaces();
}

core::KindRef NewElementDialog::kind() const
{
    return m_current >= 0 ? m_choices.at(m_current).kind : core::KindRef{};
}

QString NewElementDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

core::ElementPlace NewElementDialog::place() const
{
    return m_choosing ? m_picker->place() : defaultPlace();
}

QStringList NewElementDialog::takeInside() const
{
    const bool taken = m_dialogKind == NewElementKind::Part && m_canTake && m_takeBox->isChecked();
    return taken ? idsOf(m_closing) : QStringList();
}

void NewElementDialog::onKindChanged()
{
    m_current = m_kindBox->currentIndex();
    if (!m_titleChanged && m_current >= 0) {
        m_titleEdit->setText(m_choices.at(m_current).title);
    }
    updatePlaces();
}

void NewElementDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!title().isEmpty() && kind());
}

void NewElementDialog::updatePlaces()
{
    if (m_dialogKind == NewElementKind::Part) {
        updatePart();
        return;
    }

    m_choosing = false;
    m_beforeClosing = false;
    const core::KindRef chosen = kind();
    const core::ElementPlace usual = defaultPlace();
    if (m_registry && chosen) {
        m_picker->setBook({{m_part, m_sectionName, m_words.inPart, m_elements}}, *m_registry);
        m_picker->setElement(chosen, title());
    }

    QList<PlaceOption> options;
    const bool placed = m_registry && chosen && !m_picker->places().isEmpty();
    if (placed && chosen.kind->position != core::KindPosition::Any) {
        // A prologue at the start of the part or first in its first group; an epilogue at
        // the end of the part or last in its last group; or anywhere else
        const bool start = chosen.kind->position == core::KindPosition::Start;
        const core::ElementPlace edge{m_part, QString(), start ? 0 : m_elements.size()};
        if (m_picker->canBeAt(edge)) {
            options.append(
                {SectionWords::capitalized(start ? m_words.atStart : m_words.atEnd), edge});
        }
        const core::ProjectElement* group = nullptr;
        for (const core::ProjectElement& element : std::as_const(m_elements)) {
            if (m_picker->canBeAt({m_part, element.id, 0}) && (!start || !group)) {
                group = &element;
            }
        }
        if (group) {
            options.append({start ? tr("First in \"%1\"").arg(group->title)
                                  : tr("Last in \"%1\"").arg(group->title),
                            {m_part, group->id, start ? 0 : group->elements.size()}});
        }
        m_choosing = true;
    } else if (placed) {
        m_picker->setPlace(usual);
        if (m_openedOn.isEmpty() && !usual.groupId.isEmpty() && m_picker->place() == usual) {
            // A chapter of the body goes before the epilogue that ends the last part, or the
            // writer puts it at the end of the body
            const core::ElementPlace end{
                m_part, QString(), core::BookProject::newIndexIn(*m_registry, m_elements, chosen)};
            options.append({placeText(usual), usual});
            if (m_picker->canBeAt(end)) {
                options.append({placeText(end), end});
            }
            m_choosing = true;
            m_beforeClosing = true;
        } else if (!m_picker->warnings().isEmpty()) {
            // The place of its kind makes another element stop opening or closing the part:
            // the writer sees it, and can choose another place
            options.append({placeText(usual), usual});
            m_choosing = true;
        }
    }
    m_picker->setOptions(options);
    m_picker->setPlace(m_picker->canBeAt(usual) || options.isEmpty() ? usual
                                                                     : options.first().place);

    m_picker->setVisible(m_choosing);
    contentLayout()->setStretch(m_pickerStretch, m_choosing ? 0 : 1);
    updateDescription();

    // A dialog on the screen takes the height of what it shows now
    if (isVisible()) {
        adjustSize();
    }
}

void NewElementDialog::updatePart()
{
    const core::KindRef chosen = kind();
    const core::ElementPlace usual = defaultPlace();

    // The elements that close the body at the end of the groups before the new part, and
    // whether the part can have them inside it
    m_closing.clear();
    m_canTake = false;
    if (m_registry && chosen) {
        const qsizetype before = qMin(usual.index, m_elements.size());
        for (const core::ProjectElement* element :
             core::BookProject::closingElementsOf(*m_registry, m_elements)) {
            for (qsizetype i = 0; i < before; ++i) {
                if (findIn(m_elements.at(i).elements, element->id)) {
                    m_closing.append(*element);
                    break;
                }
            }
        }
        m_canTake = !m_closing.isEmpty() &&
                    std::all_of(m_closing.cbegin(), m_closing.cend(),
                                [this, &chosen](const core::ProjectElement& element) {
                                    const core::KindRef inside =
                                        core::BookProject::kindOf(*m_registry, element);
                                    return inside && inside.kind->allowsInside(chosen.kind->id);
                                });
    }
    const QStringList titles = titlesOf(m_closing);
    const QString named = ElementPlacePicker::quoted(titles);
    const bool one = titles.size() == 1;
    m_takeBox->setText(one ? tr("Move \"%1\" to the end of the new part").arg(titles.value(0))
                           : tr("Move %1 to the end of the new part").arg(named));
    m_takeBox->setVisible(m_canTake);
    const bool taken = m_canTake && m_takeBox->isChecked();

    if (m_closing.isEmpty()) {
        showNotice(QString(), QString());
    } else if (taken) {
        //: In Polish: Element „%1” jest teraz ostatni %2. Na końcu nowej części nadal będzie
        //: ostatni.
        showNotice(QStringLiteral("help.about"),
                   one ? tr("\"%1\" is now the last element %2. At the end of the new part it "
                            "stays the last one.")
                             .arg(titles.first(), m_words.inPart)
                       //: In Polish: Elementy %1 są teraz ostatnie %2. Na końcu nowej części
                       //: nadal będą ostatnie.
                       : tr("%1 are now the last elements %2. At the end of the new part they "
                            "stay the last ones.")
                             .arg(named, m_words.inPart));
    } else {
        //: In Polish: Element „%1” zostanie na swoim miejscu, a rozdziały dodane do nowej
        //: części znajdą się za nim.
        showNotice(QStringLiteral("common.warning"),
                   one ? tr("\"%1\" stays where it is, and the chapters added to the new part "
                            "go after it.")
                             .arg(titles.first())
                       //: In Polish: Elementy %1 zostaną na swoich miejscach, a rozdziały
                       //: dodane do nowej części znajdą się za nimi.
                       : tr("%1 stay where they are, and the chapters added to the new part go "
                            "after them.")
                             .arg(named));
    }

    // The body as it will be
    if (m_registry && chosen) {
        const QList<core::ProjectElement> body =
            taken ? without(m_elements, idsOf(m_closing)) : m_elements;
        m_picker->setBook({{m_part, m_sectionName, m_words.inPart, body}}, *m_registry);
        m_picker->setElement(chosen, title(), taken ? m_closing : QList<core::ProjectElement>());
        m_picker->setPlace(usual);
    }
    const bool shown = m_registry && chosen && !m_picker->places().isEmpty();
    m_picker->setVisible(shown);
    m_previewNote->setVisible(shown);
    contentLayout()->setStretch(m_pickerStretch, shown ? 0 : 1);
    updateDescription();

    if (isVisible()) {
        adjustSize();
    }
}

void NewElementDialog::updateDescription()
{
    const core::KindRef chosen = kind();
    const core::KindPosition position =
        chosen ? chosen.kind->position : core::KindPosition::Any;
    const core::ElementPlace usual = defaultPlace();
    const core::ProjectElement* group = usual.groupId.isEmpty() ? nullptr : groupOf(usual.groupId);
    const QList<core::ProjectElement>* list =
        group ? &group->elements : (usual.groupId.isEmpty() ? &m_elements : nullptr);
    const QString before = list && usual.index >= 0 && usual.index < list->size()
                               ? list->at(usual.index).title
                               : QString();
    const QString choose = tr("Choose where the new element goes in the book.");

    QString heading;
    QString description;
    switch (m_dialogKind) {
    case NewElementKind::Chapter:
        heading = tr("Add Chapter");
        if (m_beforeClosing && m_registry) {
            QStringList closing;
            for (const core::ProjectElement* element :
                 core::BookProject::closingElementsOf(*m_registry, m_elements)) {
                closing << element->title;
            }
            const QString groupTitle = group ? group->title : QString();
            description =
                closing.size() == 1
                    //: %2: the body with its preposition, "in the main section"; %3: the
                    //: part. In Polish: Element „%1” jest ostatni %2, więc nowy rozdział
                    //: zostanie dodany przed nim, na końcu części „%3”. Możesz wybrać inne
                    //: miejsce.
                    ? tr("\"%1\" is the last element %2, so the new chapter goes before it, at "
                         "the end of \"%3\". You can choose another place.")
                          .arg(closing.first(), m_words.inPart, groupTitle)
                    //: In Polish: Elementy %1 są ostatnie %2, więc nowy rozdział zostanie
                    //: dodany przed nimi, na końcu części „%3”. Możesz wybrać inne miejsce.
                    : tr("%1 are the last elements %2, so the new chapter goes before them, at "
                         "the end of \"%3\". You can choose another place.")
                          .arg(ElementPlacePicker::quoted(closing), m_words.inPart, groupTitle);
        } else if (m_choosing) {
            description = choose;
        } else if (position == core::KindPosition::Start) {
            description = tr("The element is added as the first one %1.").arg(m_words.inPart);
        } else if (position == core::KindPosition::End) {
            description = tr("The element is added as the last one %1.").arg(m_words.inPart);
        } else if (!m_groupTitle.isEmpty()) {
            description =
                before.isEmpty()
                    ? tr("The chapter is added as the last one in \"%1\".").arg(m_groupTitle)
                    : tr("The chapter is added at the end of \"%1\", before \"%2\".")
                          .arg(m_groupTitle, before);
        } else {
            description =
                before.isEmpty()
                    //: %1: the body with its preposition, "in the main section"
                    ? tr("The chapter is added as the last one %1.").arg(m_words.inPart)
                    //: %1: where in the body, "at the end of the main section". In Polish:
                    //: Rozdział zostanie dodany %1, przed elementem „%2”.
                    : tr("The chapter is added %1, before \"%2\".").arg(m_words.atEnd, before);
        }
        break;
    case NewElementKind::Part:
        heading = tr("Add Part");
        if (m_elements.isEmpty()) {
            //: %1: where in the body, "at the end of the main section". In Polish: Część
            //: zostanie dodana %1.
            description = tr("The part is added %1.").arg(m_words.atEnd);
        } else if (before.isEmpty()) {
            //: In Polish: Część zostanie dodana %1, za elementem „%2”.
            description = tr("The part is added %1, after \"%2\".")
                              .arg(m_words.atEnd, m_elements.last().title);
        } else {
            //: In Polish: Część zostanie dodana %1, przed elementem „%2”.
            description = tr("The part is added %1, before \"%2\".").arg(m_words.atEnd, before);
        }
        break;
    case NewElementKind::FrontMatterItem:
    case NewElementKind::BackMatterItem:
        // The description says in which section: the book names its sections its own way
        heading = tr("Add Item");
        if (m_choosing) {
            description = choose;
        } else if (before.isEmpty()) {
            //: %1: where in a part of the book, "at the end of the front section"; in a book
            //: without sections "before the content of the book" or "at the very end of the
            //: book". In Polish: Element zostanie dodany %1.
            description = tr("The item is added %1.").arg(m_words.atEnd);
        } else {
            //: %1: a part of the book with its preposition, "in the back section"; in a book
            //: without sections "at the end of the book". In Polish: Element zostanie dodany
            //: %1, przed elementem „%2”.
            description = tr("The item is added %1, before \"%2\".").arg(m_words.inPart, before);
        }
        break;
    }
    setHeading(heading, description);
}

QString NewElementDialog::placeText(const core::ElementPlace& place) const
{
    if (!place.groupId.isEmpty()) {
        const core::ProjectElement* group = groupOf(place.groupId);
        if (!group) {
            return QString();
        }
        if (place.index >= group->elements.size()) {
            return tr("Last in \"%1\"").arg(group->title);
        }
        //: %1: the element the new one goes before; %2: the part it is in. In Polish: Przed
        //: elementem „%1”, na końcu części „%2”
        return tr("Before \"%1\", at the end of \"%2\"")
            .arg(group->elements.at(place.index).title, group->title);
    }
    if (m_elements.isEmpty()) {
        return SectionWords::capitalized(m_words.atEnd);
    }
    const QString atEnd = SectionWords::capitalized(m_words.atEnd);
    if (place.index >= m_elements.size()) {
        //: An option: %1 is where in a part of the book, "At the end of the main section"; %2
        //: is the title of its last element. In Polish: %1, za elementem „%2”
        return tr("%1, after \"%2\"").arg(atEnd, m_elements.last().title);
    }
    //: An option: %1 is where in a part of the book, "At the end of the main section"; %2 is the
    //: title of the element the new one goes before. In Polish: %1, przed elementem „%2”
    return tr("%1, before \"%2\"").arg(atEnd, m_elements.at(place.index).title);
}

const core::ProjectElement* NewElementDialog::groupOf(const QString& id) const
{
    return findIn(m_elements, id);
}

core::ElementPlace NewElementDialog::defaultPlace() const
{
    return m_current >= 0 ? m_choices.at(m_current).place
                          : core::ElementPlace{m_part, QString(), 0};
}

void NewElementDialog::showNotice(const QString& iconId, const QString& text)
{
    m_noticeText->setText(text);
    m_noticeIcon->setPixmap(iconId.isEmpty()
                                ? QPixmap()
                                : core::ArtProvider::getInstance().getIcon(iconId).pixmap(
                                      NOTICE_ICON_SIZE, NOTICE_ICON_SIZE));
    m_noticeBox->setVisible(!text.isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
