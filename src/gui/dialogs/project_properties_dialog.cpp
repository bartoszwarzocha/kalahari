/// @file project_properties_dialog.cpp
/// @brief Project Properties: the type of the open book and the kinds of elements it can have

#include "kalahari/gui/dialogs/project_properties_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/custom_book_type.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/save_type_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/widgets/kind_palette.h"

#include <QFont>
#include <QFontInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr qreal WIDTH_EMS = 80.0;         ///< Eight tiles in a row, as in the window of a new
                                          ///< user project
constexpr qreal HEIGHT_EMS = 61.0;        ///< As high as that window
constexpr int MINIMUM_PALETTE_LINES = 8;  ///< Of the palette on a small screen
constexpr int BAR_SPACING = 12;           ///< Between the texts of the bar over the palette
constexpr int TYPE_SPACING = 12;          ///< Between the icon of the type and its texts
constexpr int TYPE_LINE_SPACING = 2;      ///< Between the name of the type and its sentence

/// The places of the book and its Workshop, as the palette groups its kinds
constexpr std::array<core::BookPlace, 4> PLACES{core::BookPlace::Front, core::BookPlace::Main,
                                                core::BookPlace::Back,
                                                core::BookPlace::Workshop};

/// Icon of a book without a type, as the New Book window shows it
constexpr const char* USER_PROJECT_ICON = "template.empty";

/// @brief Count the elements of @p elements, and of the elements inside them, by their kinds
void countKinds(const core::BookTypeRegistry& registry,
                const QList<core::ProjectElement>& elements,
                QHash<const core::ElementKind*, int>& counts)
{
    for (const core::ProjectElement& element : elements) {
        if (const core::KindRef kind = core::BookProject::kindOf(registry, element)) {
            ++counts[kind.kind];
        }
        countKinds(registry, element.elements, counts);
    }
}

} // anonymous namespace

ProjectPropertiesDialog::ProjectPropertiesDialog(QWidget* parent)
    : KalahariDialog(parent)
    , m_registry(core::ProjectManager::getInstance().installedBookTypes())
    , m_typesFolder(core::CustomBookTypes::folder())
{
    auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* book = pm.book();
    m_readOnly = pm.isReadOnly();
    const QString language = programLanguage();

    //: The heading of the window of the project's properties; %1: the title of the book. In
    //: Polish: Właściwości projektu „%1”
    setHeading(tr("Project Properties \"%1\"").arg(book ? book->title : QString()),
               //: In Polish: Typ książki i rodzaje elementów, które można do niej dodawać.
               tr("The book type and the kinds of elements that can be added to the book."));
    setHeadingIcon(QStringLiteral("book.properties"));

    // The type of the book, which cannot change
    const core::BookTypePackage* type =
        project && project->type ? m_registry.package(project->type->id) : nullptr;
    m_typeId = project && project->type ? project->type->id : QString();
    auto* typeRow = new QHBoxLayout();
    typeRow->setSpacing(TYPE_SPACING);
    auto* typeIcon = new QLabel(this);
    typeIcon->setAlignment(Qt::AlignTop);
    const QString typeIconId = type ? type->icon : QString::fromLatin1(USER_PROJECT_ICON);
    const auto drawTypeIcon = [typeIcon, typeIconId]() {
        auto& artProvider = core::ArtProvider::getInstance();
        const int size = artProvider.getIconSize(core::IconContext::Dialog);
        typeIcon->setPixmap(artProvider.getIcon(typeIconId, core::IconContext::Dialog)
                                .pixmap(QSize(size, size), typeIcon->devicePixelRatioF()));
    };
    drawTypeIcon();
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, typeIcon,
            drawTypeIcon);
    typeRow->addWidget(typeIcon, 0, Qt::AlignTop);
    auto* typeTexts = new QVBoxLayout();
    typeTexts->setSpacing(TYPE_LINE_SPACING);
    m_typeLabel = new QLabel(this);
    m_typeLabel->setObjectName(QStringLiteral("projectPropertiesType"));
    m_typeLabel->setTextFormat(Qt::PlainText);
    m_typeLabel->setWordWrap(true);
    QFont bold;  // only the weight: the rest follows the font of the window
    bold.setWeight(QFont::Bold);
    m_typeLabel->setFont(bold);
    QString typeSentence;
    if (!m_typeId.isEmpty()) {
        //: %1: the type of the book, e.g. Novel. In Polish: Typ książki: %1
        m_typeLabel->setText(tr("Book type: %1")
                                 .arg(project ? typeNameOf(m_registry, *project,
                                                           pm.packageCard(m_typeId))
                                              : QString()));
        //: In Polish: Typu nie można zmienić. Rodzaje typu są zaznaczone na stałe; zaznacz
        //: rodzaje innych typów, które chcesz dodawać do tej książki.
        typeSentence = tr("The type cannot be changed. The kinds of the type are checked for "
                          "good; check the kinds of other types you want to add to this book.");
    } else {
        //: In Polish: Projekt użytkownika – bez typu
        m_typeLabel->setText(tr("User project – without a type"));
        //: In Polish: Zaznacz rodzaje, które chcesz dodawać do książki. Odznaczenie rodzaju nie
        //: usuwa jego elementów.
        typeSentence = tr("Check the kinds you want to add to the book. Unchecking a kind does "
                          "not delete its elements.");
    }
    if (m_readOnly) {
        typeSentence = readOnlyBookMessage();
    }
    typeTexts->addWidget(m_typeLabel);
    auto* typeText = new QLabel(typeSentence, this);
    typeText->setTextFormat(Qt::PlainText);
    typeText->setWordWrap(true);
    typeText->setForegroundRole(QPalette::PlaceholderText);
    typeTexts->addWidget(typeText);
    typeRow->addLayout(typeTexts, 1);
    contentLayout()->addLayout(typeRow);

    // The number of the checked kinds, and the links that check many kinds at once
    auto* bar = new QHBoxLayout();
    bar->setSpacing(BAR_SPACING);
    m_countLabel = new QLabel(this);
    bar->addWidget(m_countLabel);
    bar->addStretch(1);
    QList<QLabel*> links;
    if (!m_typeId.isEmpty()) {
        //: A link over the palette of a book of a type. In Polish: Odznacz rodzaje innych
        //: typów
        links << newLink(tr("Uncheck Kinds of Other Types"), this);
    } else {
        //: In Polish: Zaznacz wszystkie
        links << newLink(tr("Check All"), this);
        //: The kinds of the package "Base" and only them. In Polish: Tylko „Podstawa”
        links << newLink(tr("Only \"Base\""), this);
        //: In Polish: Odznacz wszystkie
        links << newLink(tr("Uncheck All"), this);
    }
    for (QLabel* link : std::as_const(links)) {
        link->setEnabled(!m_readOnly);
        bar->addWidget(link);
    }
    contentLayout()->addLayout(bar);

    // The palette, which scrolls and gets the room of a resized window
    m_palette = new KindPalette(m_registry);
    m_palette->hideCreateNow();
    auto* area = new QScrollArea(this);
    area->setWidget(m_palette);
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setMinimumHeight(MINIMUM_PALETTE_LINES * fontMetrics().lineSpacing());
    //: The list of the kinds of elements of the book. In Polish: Paleta rodzajów
    area->setAccessibleName(tr("Palette of Kinds"));
    contentLayout()->addWidget(area, 1);

    // The groups have the names of the sections of the book, as the Navigator shows them
    for (const core::BookPlace place : PLACES) {
        if (place == core::BookPlace::Workshop) {
            //: The group of the kinds of the Workshop in the palette. In Polish: Warsztat
            m_palette->setGroupName(place, tr("Workshop"));
        } else {
            m_palette->setGroupName(place, book && book->partsLayer
                                               ? book->sectionName(place)
                                               : SectionWords::groupName(place));
        }
    }

    // The kinds of the type for good, the kinds of the book checked, and how many elements
    // each kind has in the book
    QList<core::KindRef> typeKinds;
    if (type) {
        for (const core::BookPlace place : PLACES) {
            for (const core::KindRef& kind : m_registry.kindsIn(type->id, place)) {
                if (std::none_of(typeKinds.cbegin(), typeKinds.cend(),
                                 [&kind](const core::KindRef& other) {
                                     return other.kind == kind.kind;
                                 })) {
                    typeKinds.append(kind);
                }
            }
        }
    }
    m_typeKinds = static_cast<int>(typeKinds.size());
    //: The tooltip of a kind of the book's type in the palette; %1: the type, e.g. Novel. In
    //: Polish: Rodzaj typu „%1” – zawsze w książce
    m_palette->setLocked(typeKinds, type ? tr("A kind of the type \"%1\": always in the book")
                                               .arg(type->name.text(language))
                                         : QString());
    core::PaletteChoice checked;
    if (project) {
        for (const core::KindReference& reference : project->kinds) {
            if (const core::KindRef kind =
                    m_registry.findKind(reference.packageId, reference.kindId)) {
                checked.kinds.append(kind);
            }
        }
        for (const core::ProjectBook& each : project->books) {
            countKinds(m_registry, each.frontElements, m_counts);
            countKinds(m_registry, each.mainElements, m_counts);
            countKinds(m_registry, each.backElements, m_counts);
        }
        countKinds(m_registry, project->workshop.elements, m_counts);
    }
    m_palette->setChoice(checked);
    m_palette->setElementCounts(m_counts);
    m_palette->setEnabled(!m_readOnly);

    // What happens to the elements of the kinds the writer unchecks
    QLabel* noteIcon = nullptr;
    m_note = newNote(this, &noteIcon, &m_noteText);
    m_noteText->setObjectName(QStringLiteral("projectPropertiesNote"));
    setNoteIcon(noteIcon, QStringLiteral("common.warning"));
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, noteIcon,
            [noteIcon]() { setNoteIcon(noteIcon, QStringLiteral("common.warning")); });
    m_note->hide();
    contentLayout()->addWidget(m_note);

    // The tools of the types, which no type has yet
    contentLayout()->addWidget(newToolsBox(this));

    //: The button that saves the kinds of the book as a type of the writer's own. In Polish:
    //: Zapisz jako typ...
    m_saveTypeButton = addSideButton(tr("Save as &Type..."));
    connect(m_saveTypeButton, &QPushButton::clicked, this, &ProjectPropertiesDialog::saveAsType);
    if (!links.isEmpty()) {
        if (!m_typeId.isEmpty()) {
            connect(links.at(0), &QLabel::linkActivated, m_palette, &KindPalette::uncheckAll);
        } else {
            connect(links.at(0), &QLabel::linkActivated, m_palette, &KindPalette::checkAll);
            connect(links.at(1), &QLabel::linkActivated, this, [this]() {
                m_palette->checkOnly(QString::fromLatin1(core::ProjectManager::BASE_PACKAGE));
            });
            connect(links.at(2), &QLabel::linkActivated, m_palette, &KindPalette::uncheckAll);
        }
    }
    connect(m_palette, &KindPalette::choiceChanged, this, &ProjectPropertiesDialog::updateState);
    updateState();

    // The size of the window of a new user project at the font of the window; on a smaller
    // screen the window is smaller and the palette scrolls
    const int em = QFontInfo(font()).pixelSize();
    resize(qRound(WIDTH_EMS * em), qRound(HEIGHT_EMS * em));
}

QList<core::KindRef> ProjectPropertiesDialog::chosenKinds() const
{
    return m_palette->choice().kinds;
}

void ProjectPropertiesDialog::setTypesFolder(const QString& folder)
{
    m_typesFolder = folder;
}

void ProjectPropertiesDialog::updateState()
{
    const QList<core::KindRef> kinds = chosenKinds();
    const int count = static_cast<int>(kinds.size());
    if (!m_typeId.isEmpty()) {
        //: Over the palette of a book of a type. In Polish: Rodzaje typu: %1 · z innych typów:
        //: %2
        m_countLabel->setText(tr("Kinds of the type: %1 · from other types: %2")
                                  .arg(m_typeKinds)
                                  .arg(count - m_typeKinds));
    } else {
        //: Over the palette of a user project. In Polish: Zaznaczone rodzaje: %1
        m_countLabel->setText(tr("Checked kinds: %1").arg(count));
    }

    // The unchecked kinds that have elements in the book
    QStringList unchecked;
    const QString language = programLanguage();
    for (const core::KindRef& kind : m_registry.allKinds()) {
        const int elements = m_counts.value(kind.kind);
        if (elements > 0 && !m_palette->isChecked(kind.kind)) {
            unchecked << QStringLiteral("%1 (%2)")
                             .arg(kind.kind->name.text(language))
                             .arg(elements);
        }
    }
    unchecked.removeDuplicates();
    m_note->setVisible(!unchecked.isEmpty());
    //: %1: the unchecked kinds with the numbers of their elements, e.g. Poem (5). In Polish:
    //: Odznaczone rodzaje mają elementy w książce: %1. Te elementy zostają i działają, ale
    //: nowych nie dodasz.
    m_noteText->setText(tr("Unchecked kinds have elements in the book: %1. Those elements stay "
                           "and work, but no new ones can be added.")
                            .arg(unchecked.join(QStringLiteral(", "))));

    // A user project needs a kind
    const bool can = m_readOnly || !m_typeId.isEmpty() || count > 0;
    acceptButton()->setEnabled(can);
    //: The tooltip of OK when no kind is checked. In Polish: Zaznacz co najmniej jeden rodzaj.
    acceptButton()->setToolTip(can ? QString() : tr("Check at least one kind."));
}

void ProjectPropertiesDialog::saveAsType()
{
    // The type of the book with the kinds checked now; the kinds of the type are not taken
    core::BookProject kinds;
    if (const core::BookProject* project = core::ProjectManager::getInstance().project()) {
        kinds.type = project->type;
    }
    for (const core::KindRef& kind : chosenKinds()) {
        if (!m_palette->isLocked(kind.kind)) {
            kinds.kinds.append(core::KindReference{kind.package->id, kind.kind->id});
        }
    }
    SaveTypeDialog dialog(core::CustomBookType::of(kinds, m_registry), m_registry, this);
    dialog.setFolder(m_typesFolder);
    dialog.exec();
}

void ProjectPropertiesDialog::accept()
{
    auto& pm = core::ProjectManager::getInstance();
    if (m_readOnly || !pm.project()) {
        KalahariDialog::accept();
        return;
    }
    const QList<core::KindReference> before = pm.project()->kinds;
    if (!pm.setProjectKinds(chosenKinds())) {
        //: In Polish: Nie udało się zapisać rodzajów elementów książki. Szczegóły są w
        //: dzienniku programu.
        MessageDialog::error(this, windowTitle(),
                             tr("The kinds of elements of the book could not be saved. The "
                                "program's log has the details."));
        return;
    }
    m_changedKinds = pm.project()->kinds != before;
    KalahariDialog::accept();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
