/// @file element_place_picker.cpp
/// @brief The place of an element in the book: options, and the list of the book with the
/// element in its place

#include "kalahari/gui/widgets/element_place_picker.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace kalahari {
namespace gui {

namespace {

constexpr int PLACE_ROLE = Qt::UserRole;  ///< Index of the place a click on the row chooses; -1:
                                          ///< none
constexpr int LIST_ROWS = 9;              ///< Rows the list shows when there is room for them
constexpr int MINIMUM_ROWS = 4;           ///< Rows the list keeps on a small screen
constexpr int WARNING_ICON_SIZE = 16;     ///< Size of the icon of the warnings

/// Id of the element in the copy of the book that warnings() makes; no element of a book has it
const QString PLACED_ID = QStringLiteral("<placed>");

/// The list of the book, as high as LIST_ROWS rows when there is room for them
class PlaceList : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    /// Height of a row, before the list has rows to measure
    int rowHeight() const { return qMax(fontMetrics().height() + 6, iconSize().height() + 4); }

    QSize sizeHint() const override {
        return {QTreeWidget::sizeHint().width(), rowHeight() * LIST_ROWS + 2 * frameWidth()};
    }
};

/// ArtProvider id of the icon of the row of @p place
QString sectionIconId(core::BookPlace place) {
    switch (place) {
    case core::BookPlace::Front:
        return QStringLiteral("structure.frontmatter");
    case core::BookPlace::Main:
        return QStringLiteral("structure.body");
    case core::BookPlace::Back:
        return QStringLiteral("structure.backmatter");
    case core::BookPlace::Workshop:
        break;
    }
    return QStringLiteral("structure.otherfiles");
}

/// The list of group @p groupId in @p elements, at any depth; @p elements itself for no group
QList<core::ProjectElement>* listIn(QList<core::ProjectElement>& elements,
                                    const QString& groupId) {
    if (groupId.isEmpty()) {
        return &elements;
    }
    for (core::ProjectElement& element : elements) {
        if (element.id == groupId) {
            return &element.elements;
        }
        if (QList<core::ProjectElement>* inside = listIn(element.elements, groupId)) {
            return inside;
        }
    }
    return nullptr;
}

/// Ids of @p elements
QStringList idsOf(const QList<const core::ProjectElement*>& elements) {
    QStringList ids;
    for (const core::ProjectElement* element : elements) {
        ids << element->id;
    }
    return ids;
}

/// Titles of the elements of @p before that are not in @p after, but the placed one
QStringList titlesLost(const QList<const core::ProjectElement*>& before,
                       const QStringList& after) {
    QStringList titles;
    for (const core::ProjectElement* element : before) {
        if (element->id != PLACED_ID && !after.contains(element->id)) {
            titles << element->title;
        }
    }
    return titles;
}

} // anonymous namespace

ElementPlacePicker::ElementPlacePicker(QWidget* parent)
    : QWidget(parent)
{
    auto& artProvider = core::ArtProvider::getInstance();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_label = new QLabel(tr("Place"), this);
    m_label->setTextFormat(Qt::PlainText);
    layout->addWidget(m_label);

    // The options: places the dialog offers, then any other place
    m_optionLayout = new QVBoxLayout();
    m_optionLayout->setSpacing(4);
    m_elsewhereButton = new QRadioButton(tr("Elsewhere: show the place in the list"), this);
    m_elsewhereButton->hide();
    connect(m_elsewhereButton, &QRadioButton::clicked, this, [this]() { m_list->setFocus(); });
    m_optionLayout->addWidget(m_elsewhereButton);
    layout->addLayout(m_optionLayout);

    // The list of the book with the element in its place, and the buttons that move it
    auto* listRow = new QHBoxLayout();
    listRow->setSpacing(6);
    auto* list = new PlaceList(this);
    m_list = list;
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setItemsExpandable(false);
    m_list->setMinimumHeight(list->rowHeight() * MINIMUM_ROWS + 2 * m_list->frameWidth());
    m_list->setToolTip(tr("Click the element it is to go before"));
    m_list->installEventFilter(this);
    connect(m_list, &QTreeWidget::itemClicked, this, &ElementPlacePicker::onItemClicked);
    listRow->addWidget(m_list, 1);
    m_label->setBuddy(m_list);

    auto* moveButtons = new QVBoxLayout();
    moveButtons->setSpacing(6);
    m_upButton = new QPushButton(this);
    m_upButton->setIcon(artProvider.getIcon(QStringLiteral("navigation.up"),
                                            core::IconContext::Button));
    m_upButton->setToolTip(tr("Move the element up"));
    m_upButton->setAutoRepeat(true);
    m_upButton->setAutoDefault(false);
    m_downButton = new QPushButton(this);
    m_downButton->setIcon(artProvider.getIcon(QStringLiteral("navigation.down"),
                                              core::IconContext::Button));
    m_downButton->setToolTip(tr("Move the element down"));
    m_downButton->setAutoRepeat(true);
    m_downButton->setAutoDefault(false);
    connect(m_upButton, &QPushButton::clicked, this, [this]() { moveTo(m_place - 1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { moveTo(m_place + 1); });
    moveButtons->addWidget(m_upButton);
    moveButtons->addWidget(m_downButton);
    moveButtons->addStretch(1);
    listRow->addLayout(moveButtons);
    layout->addLayout(listRow, 1);

    //: %1: the arrows of the buttons, kept together on a line
    m_hint = new QLabel(tr("Click the element it is to go before, or move it with the buttons %1.")
                            .arg(QStringLiteral("↑\u00A0↓")),
                        this);
    m_hint->setTextFormat(Qt::PlainText);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint);

    // What the place changes
    m_warningBox = new QWidget(this);
    auto* warningLayout = new QHBoxLayout(m_warningBox);
    warningLayout->setContentsMargins(0, 0, 0, 0);
    warningLayout->setSpacing(8);
    m_warningIcon = new QLabel(m_warningBox);
    m_warningIcon->setPixmap(artProvider.getIcon(QStringLiteral("common.warning"))
                                 .pixmap(WARNING_ICON_SIZE, WARNING_ICON_SIZE));
    m_warningIcon->setAlignment(Qt::AlignTop);
    warningLayout->addWidget(m_warningIcon);
    m_warningText = new QLabel(m_warningBox);
    m_warningText->setTextFormat(Qt::PlainText);
    m_warningText->setWordWrap(true);
    warningLayout->addWidget(m_warningText, 1);
    m_warningBox->hide();
    layout->addWidget(m_warningBox);
}

void ElementPlacePicker::setBook(const QList<PickerSection>& sections,
                                 const core::BookTypeRegistry& registry)
{
    const core::ElementPlace previous = place();
    m_sections = sections;
    m_registry = &registry;
    setElement(m_kind, m_title, m_inside);
    setPlace(previous);
}

void ElementPlacePicker::setElement(const core::KindRef& kind, const QString& title,
                                   const QList<core::ProjectElement>& inside)
{
    m_kind = kind;
    m_title = title;
    m_inside = inside;

    m_places.clear();
    if (m_registry && m_kind) {
        for (const PickerSection& section : std::as_const(m_sections)) {
            addPlacesIn(section.place, section.elements, QString(),
                        m_kind.kind->allows(section.place));
        }
    }
    m_place = m_places.isEmpty() ? -1 : 0;
    showPlace();
    emit placeChanged();
}

void ElementPlacePicker::setTitle(const QString& title)
{
    m_title = title;
    if (m_elementItem) {
        m_elementItem->setText(0, title);
    }
    m_warningText->setText(warnings().join(QLatin1Char('\n')));
}

void ElementPlacePicker::setLabel(const QString& text)
{
    m_label->setText(text);
}

void ElementPlacePicker::setOptions(const QList<PlaceOption>& options)
{
    qDeleteAll(m_optionButtons);
    m_optionButtons.clear();
    m_options = options;
    for (qsizetype i = 0; i < m_options.size(); ++i) {
        auto* button = new QRadioButton(m_options.at(i).text, this);
        m_optionLayout->insertWidget(static_cast<int>(i), button);
        connect(button, &QRadioButton::clicked, this,
                [this, i]() { setPlace(m_options.at(i).place); });
        m_optionButtons.append(button);
    }
    m_elsewhereButton->setVisible(!m_options.isEmpty() && !m_preview);
    m_label->setBuddy(m_optionButtons.isEmpty() ? static_cast<QWidget*>(m_list)
                                                : m_optionButtons.first());
    showPlace();
}

void ElementPlacePicker::setPlace(const core::ElementPlace& place)
{
    moveTo(m_places.indexOf(place));
}

bool ElementPlacePicker::canBeAt(const core::ElementPlace& place) const
{
    return m_places.contains(place);
}

core::ElementPlace ElementPlacePicker::place() const
{
    return m_place >= 0 && m_place < m_places.size() ? m_places.at(m_place)
                                                     : core::ElementPlace{};
}

QStringList ElementPlacePicker::warnings() const
{
    QStringList sentences;
    if (!m_registry || !m_kind || m_place < 0) {
        return sentences;
    }
    const core::ElementPlace at = m_places.at(m_place);
    const auto section = std::find_if(m_sections.cbegin(), m_sections.cend(),
                                      [&at](const PickerSection& candidate) {
                                          return candidate.place == at.place;
                                      });
    if (section == m_sections.cend()) {
        return sentences;
    }

    // The part of the book with the element at its place
    core::ProjectElement placed;
    placed.id = PLACED_ID;
    placed.kind = {m_kind.package->id, m_kind.kind->id};
    placed.title = m_title;
    placed.elements = m_inside;
    QList<core::ProjectElement> after = section->elements;
    QList<core::ProjectElement>* list = listIn(after, at.groupId);
    if (!list) {
        return sentences;
    }
    list->insert(qBound(qsizetype(0), at.index, list->size()), placed);

    // The elements that opened or closed it and no longer do, and the element itself
    using core::BookProject;
    const QStringList opening = idsOf(BookProject::openingElementsOf(*m_registry, after));
    const QStringList closing = idsOf(BookProject::closingElementsOf(*m_registry, after));
    const QStringList notFirst =
        titlesLost(BookProject::openingElementsOf(*m_registry, section->elements), opening);
    const QStringList notLast =
        titlesLost(BookProject::closingElementsOf(*m_registry, section->elements), closing);
    if (notFirst.size() == 1) {
        //: %1: the title of an element; %2: a part of the book with its preposition, "in the
        //: main section". In Polish: Element „%1” przestanie być pierwszy %2.
        sentences << tr("\"%1\" will no longer be the first element %2.")
                         .arg(notFirst.first(), section->inPart);
    } else if (notFirst.size() > 1) {
        //: %1: titles in quotes, "A" and "B"; %2: a part of the book with its preposition. In
        //: Polish: Elementy %1 przestaną być pierwsze %2.
        sentences << tr("%1 will no longer be the first elements %2.")
                         .arg(quoted(notFirst), section->inPart);
    }
    if (notLast.size() == 1) {
        //: In Polish: Element „%1” przestanie być ostatni %2.
        sentences << tr("\"%1\" will no longer be the last element %2.")
                         .arg(notLast.first(), section->inPart);
    } else if (notLast.size() > 1) {
        //: In Polish: Elementy %1 przestaną być ostatnie %2.
        sentences << tr("%1 will no longer be the last elements %2.")
                         .arg(quoted(notLast), section->inPart);
    }
    if (m_kind.kind->position == core::KindPosition::Start && !opening.contains(PLACED_ID)) {
        //: A prologue placed after a chapter. In Polish: Element „%1” nie będzie pierwszy %2.
        sentences << tr("\"%1\" will not be the first element %2.").arg(m_title, section->inPart);
    }
    if (m_kind.kind->position == core::KindPosition::End && !closing.contains(PLACED_ID)) {
        //: An epilogue placed before a chapter. In Polish: Element „%1” nie będzie ostatni %2.
        sentences << tr("\"%1\" will not be the last element %2.").arg(m_title, section->inPart);
    }
    return sentences;
}

void ElementPlacePicker::setPreview(bool preview)
{
    m_preview = preview;
    for (QRadioButton* button : std::as_const(m_optionButtons)) {
        button->setVisible(!preview);
    }
    m_elsewhereButton->setVisible(!m_options.isEmpty() && !preview);
    m_upButton->setVisible(!preview);
    m_downButton->setVisible(!preview);
    m_hint->setVisible(!preview);
    m_list->setFocusPolicy(preview ? Qt::NoFocus : Qt::StrongFocus);
    m_list->setToolTip(preview ? QString() : tr("Click the element it is to go before"));
    showPlace();
}

QString ElementPlacePicker::quoted(const QStringList& titles)
{
    QStringList quotedTitles;
    for (const QString& title : titles) {
        //: A title in quotes, in a sentence. In Polish: „%1”
        quotedTitles << tr("\"%1\"").arg(title);
    }
    if (quotedTitles.size() < 2) {
        return quotedTitles.value(0);
    }
    const QString last = quotedTitles.takeLast();
    //: Titles in a sentence, "A", "B" and "C": %1 is the titles but the last one, joined with
    //: commas; %2 is the last one. In Polish: %1 i %2
    return tr("%1 and %2").arg(quotedTitles.join(QStringLiteral(", ")), last);
}

bool ElementPlacePicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list && event->type() == QEvent::KeyPress && !m_preview) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() == Qt::NoModifier &&
            (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
            moveTo(m_place + (key->key() == Qt::Key_Up ? -1 : 1));
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ElementPlacePicker::addPlacesIn(core::BookPlace part,
                                     const QList<core::ProjectElement>& elements,
                                     const QString& groupId, bool here)
{
    for (qsizetype i = 0; i <= elements.size(); ++i) {
        if (here) {
            m_places.append({part, groupId, i});
        }
        if (i < elements.size() && canBeInside(elements.at(i))) {
            addPlacesIn(part, elements.at(i).elements, elements.at(i).id, true);
        }
    }
}

bool ElementPlacePicker::canBeInside(const core::ProjectElement& element) const
{
    return m_registry && m_kind &&
           core::BookProject::formOf(*m_registry, element) == core::ElementForm::Group &&
           m_kind.kind->allowsInside(element.kind.kindId);
}

void ElementPlacePicker::showPlace()
{
    auto& artProvider = core::ArtProvider::getInstance();
    m_list->clear();
    m_elementItem = nullptr;

    if (m_registry) {
        for (const PickerSection& section : std::as_const(m_sections)) {
            QTreeWidgetItem* row = nullptr;
            if (!section.name.isEmpty()) {
                // Its row puts the element first in it
                row = new QTreeWidgetItem(m_list);
                row->setText(0, section.name);
                row->setIcon(0, artProvider.getIcon(sectionIconId(section.place),
                                                    core::IconContext::TreeView));
                row->setForeground(0, m_list->palette().brush(QPalette::PlaceholderText));
                const auto first = std::find_if(m_places.cbegin(), m_places.cend(),
                                                [&section](const core::ElementPlace& place) {
                                                    return place.place == section.place;
                                                });
                row->setData(0, PLACE_ROLE,
                             first != m_places.cend()
                                 ? static_cast<qlonglong>(first - m_places.cbegin())
                                 : qlonglong(-1));
                row->setFlags(Qt::ItemIsEnabled);
            }
            addItems(row, section.place, section.elements, QString());
        }
    }
    m_list->expandAll();
    if (m_elementItem) {
        m_list->setCurrentItem(m_elementItem);
        m_list->scrollToItem(m_elementItem);
    }

    // The option of the place: one of those offered, or any other place
    const core::ElementPlace chosen = place();
    bool offered = false;
    for (qsizetype i = 0; i < m_optionButtons.size(); ++i) {
        const QSignalBlocker blocker(m_optionButtons.at(i));
        if (m_place >= 0 && m_options.at(i).place == chosen && !offered) {
            m_optionButtons.at(i)->setChecked(true);
            offered = true;
        }
    }
    if (!offered && !m_optionButtons.isEmpty()) {
        const QSignalBlocker blocker(m_elsewhereButton);
        m_elsewhereButton->setChecked(true);
    }
    m_upButton->setEnabled(m_place > 0);
    m_downButton->setEnabled(m_place >= 0 && m_place < m_places.size() - 1);

    const QStringList sentences = warnings();
    m_warningText->setText(sentences.join(QLatin1Char('\n')));
    m_warningBox->setVisible(!sentences.isEmpty() && !m_preview);
}

void ElementPlacePicker::addItems(QTreeWidgetItem* parent, core::BookPlace part,
                                  const QList<core::ProjectElement>& elements,
                                  const QString& groupId)
{
    auto& artProvider = core::ArtProvider::getInstance();
    for (qsizetype i = 0; i <= elements.size(); ++i) {
        const core::ElementPlace before{part, groupId, i};
        if (m_place >= 0 && m_places.at(m_place) == before) {
            addElementItem(parent);
        }
        if (i == elements.size()) {
            break;
        }

        // An element of the book: a click puts the element before it, if it can go there
        const core::ProjectElement& element = elements.at(i);
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_list);
        item->setText(0, element.title);
        item->setIcon(0, artProvider.getIcon(NavigatorPanel::iconIdOf(*m_registry, element),
                                             core::IconContext::TreeView));
        item->setData(0, PLACE_ROLE, static_cast<qlonglong>(m_places.indexOf(before)));
        item->setFlags(Qt::ItemIsEnabled);
        addItems(item, part, element.elements, element.id);
    }
}

void ElementPlacePicker::addElementItem(QTreeWidgetItem* parent)
{
    auto& artProvider = core::ArtProvider::getInstance();
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_list);
    item->setText(0, m_title);
    const QString iconId = m_kind && !m_kind.kind->icon.isEmpty()
                               ? m_kind.kind->icon
                               : QStringLiteral("template.chapter");
    item->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));
    QFont font = m_list->font();
    font.setBold(true);
    item->setFont(0, font);
    item->setData(0, PLACE_ROLE, qlonglong(-1));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_elementItem = item;

    // The elements it takes inside it, marked
    QColor marked = m_list->palette().color(QPalette::Highlight);
    marked.setAlpha(48);
    const std::function<void(QTreeWidgetItem*, const QList<core::ProjectElement>&)> addInside =
        [&](QTreeWidgetItem* in, const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                auto* inside = new QTreeWidgetItem(in);
                inside->setText(0, element.title);
                inside->setIcon(0, artProvider.getIcon(
                                       NavigatorPanel::iconIdOf(*m_registry, element),
                                       core::IconContext::TreeView));
                inside->setBackground(0, marked);
                inside->setData(0, PLACE_ROLE, qlonglong(-1));
                inside->setFlags(Qt::ItemIsEnabled);
                addInside(inside, element.elements);
            }
        };
    addInside(item, m_inside);
}

void ElementPlacePicker::moveTo(qsizetype index)
{
    if (index < 0 || index >= m_places.size() || index == m_place) {
        return;
    }
    m_place = index;
    showPlace();
    emit placeChanged();
}

void ElementPlacePicker::onItemClicked(QTreeWidgetItem* item)
{
    if (!m_preview && item && item != m_elementItem) {
        moveTo(item->data(0, PLACE_ROLE).toLongLong());
    }
}

} // namespace gui
} // namespace kalahari
