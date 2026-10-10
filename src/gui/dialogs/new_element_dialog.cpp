/// @file new_element_dialog.cpp
/// @brief Dialog for a new element of the book in the Navigator: chapter, part, front or
/// back matter item

#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int PLACE_ROLE = Qt::UserRole;  ///< Index of the place before an element; -1: none
constexpr int LIST_ROWS = 9;              ///< Rows the list of the body shows at the start

} // anonymous namespace

NewElementDialog::NewElementDialog(NewElementKind kind, const QList<NewElementChoice>& choices,
                                   int current, const QString& groupTitle, QWidget* parent)
    : KalahariDialog(parent)
    , m_dialogKind(kind)
    , m_groupTitle(groupTitle)
    , m_choices(choices)
    , m_current(choices.isEmpty() ? -1 : qBound(0, current, static_cast<int>(choices.size()) - 1))
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
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_newItem) {
            m_newItem->setText(0, title());
        }
    });

    // The place of a prologue or an epilogue: the options, and the list of the body with the
    // new element in its place
    m_placeBox = new QWidget(this);
    auto* placeLayout = new QVBoxLayout(m_placeBox);
    placeLayout->setContentsMargins(0, 0, 0, 0);
    placeLayout->setSpacing(4);
    auto* placeLabel = new QLabel(tr("Place"), m_placeBox);
    placeLabel->setTextFormat(Qt::PlainText);
    placeLayout->addWidget(placeLabel);

    m_firstButton = new QRadioButton(m_placeBox);
    m_groupButton = new QRadioButton(m_placeBox);
    m_elsewhereButton = new QRadioButton(tr("Elsewhere: show the place in the list"), m_placeBox);
    placeLabel->setBuddy(m_firstButton);
    placeLayout->addWidget(m_firstButton);
    placeLayout->addWidget(m_groupButton);
    placeLayout->addWidget(m_elsewhereButton);
    connect(m_firstButton, &QRadioButton::clicked, this, [this]() { moveTo(m_firstOption); });
    connect(m_groupButton, &QRadioButton::clicked, this, [this]() { moveTo(m_groupOption); });
    connect(m_elsewhereButton, &QRadioButton::clicked, this, [this]() {
        m_placeList->setFocus();
    });

    auto* listRow = new QHBoxLayout();
    listRow->setSpacing(6);
    m_placeList = new QTreeWidget(m_placeBox);
    m_placeList->setHeaderHidden(true);
    m_placeList->setRootIsDecorated(false);
    m_placeList->setItemsExpandable(false);
    m_placeList->setMinimumHeight(m_placeList->fontMetrics().height() * LIST_ROWS);
    m_placeList->setToolTip(tr("Click the element the new one is to go before"));
    m_placeList->installEventFilter(this);
    connect(m_placeList, &QTreeWidget::itemClicked, this, &NewElementDialog::onElementClicked);
    listRow->addWidget(m_placeList, 1);

    auto& artProvider = core::ArtProvider::getInstance();
    auto* moveButtons = new QVBoxLayout();
    moveButtons->setSpacing(6);
    m_upButton = new QPushButton(m_placeBox);
    m_upButton->setIcon(artProvider.getIcon(QStringLiteral("navigation.up"),
                                            core::IconContext::Button));
    m_upButton->setToolTip(tr("Move the new element up"));
    m_upButton->setAutoRepeat(true);
    m_upButton->setAutoDefault(false);
    m_downButton = new QPushButton(m_placeBox);
    m_downButton->setIcon(artProvider.getIcon(QStringLiteral("navigation.down"),
                                              core::IconContext::Button));
    m_downButton->setToolTip(tr("Move the new element down"));
    m_downButton->setAutoRepeat(true);
    m_downButton->setAutoDefault(false);
    connect(m_upButton, &QPushButton::clicked, this, [this]() { moveTo(m_place - 1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { moveTo(m_place + 1); });
    moveButtons->addWidget(m_upButton);
    moveButtons->addWidget(m_downButton);
    moveButtons->addStretch(1);
    listRow->addLayout(moveButtons);
    placeLayout->addLayout(listRow, 1);

    // The list takes the room of a resized dialog; without it the room stays under the fields
    contentLayout()->addWidget(m_placeBox, 1);
    m_placeStretch = contentLayout()->count();
    contentLayout()->addStretch(1);
    m_placeBox->hide();

    setAcceptText(tr("Add"));
    updateAcceptButton();
    updateDescription();

    // The writer starts with what the element is: its kind, or straight away its title
    if (m_kindBox) {
        m_kindBox->setFocus();
    } else {
        m_titleEdit->setFocus();
    }
}

void NewElementDialog::setBody(const QList<core::ProjectElement>& elements,
                               const core::BookTypeRegistry& registry, const QString& groupId)
{
    m_body = elements;
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

std::optional<NewElementPlace> NewElementDialog::place() const
{
    if (m_place < 0 || m_place >= m_places.size()) {
        return std::nullopt;
    }
    return m_places.at(m_place);
}

bool NewElementDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_placeList && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() == Qt::NoModifier &&
            (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
            moveTo(m_place + (key->key() == Qt::Key_Up ? -1 : 1));
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
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

void NewElementDialog::updateDescription()
{
    const core::KindRef chosen = kind();
    const core::KindPosition position =
        chosen ? chosen.kind->position : core::KindPosition::Any;
    const QString before = m_current >= 0 ? m_choices.at(m_current).before : QString();

    QString heading;
    QString description;
    switch (m_dialogKind) {
    case NewElementKind::Chapter:
        heading = tr("Add Chapter");
        if (!m_groupTitle.isEmpty()) {
            description =
                before.isEmpty()
                    ? tr("The chapter is added as the last one in the part \"%1\".")
                          .arg(m_groupTitle)
                    : tr("The chapter is added at the end of the part \"%1\", before \"%2\".")
                          .arg(m_groupTitle, before);
        } else {
            description =
                before.isEmpty()
                    ? tr("The chapter is added as the last one in the body of the book.")
                    : tr("The chapter is added at the end of the body of the book, before "
                         "\"%1\".")
                          .arg(before);
        }
        break;
    case NewElementKind::Part:
        heading = tr("Add Part");
        description = before.isEmpty()
                          ? tr("The part is added as the last one in the book.")
                          : tr("The part is added at the end of the book, before \"%1\".")
                                .arg(before);
        break;
    case NewElementKind::FrontMatterItem:
        heading = tr("Add Front Matter Item");
        description = tr("The item is added as the last one in the front matter.");
        break;
    case NewElementKind::BackMatterItem:
        heading = tr("Add Back Matter Item");
        description = tr("The item is added as the last one in the back matter.");
        break;
    }

    // A prologue opens the body of the book and an epilogue closes it, unless the writer
    // chooses its place
    const bool body =
        m_dialogKind == NewElementKind::Chapter || m_dialogKind == NewElementKind::Part;
    if (!m_places.isEmpty()) {
        description = tr("Choose where the new element goes in the book.");
    } else if (body && position == core::KindPosition::Start) {
        description = tr("The element is added as the first one in the body of the book.");
    } else if (body && position == core::KindPosition::End) {
        description = tr("The element is added as the last one in the body of the book.");
    }
    setHeading(heading, description);
}

bool NewElementDialog::canBeInside(const core::ProjectElement& element) const
{
    const core::KindRef chosen = kind();
    return m_registry && chosen &&
           core::BookProject::formOf(*m_registry, element) == core::ElementForm::Group &&
           chosen.kind->allowsInside(element.kind.kindId);
}

void NewElementDialog::updatePlaces()
{
    m_places.clear();
    m_place = -1;
    m_firstOption = -1;
    m_groupOption = -1;

    const core::KindRef chosen = kind();
    const core::KindPosition position =
        chosen ? chosen.kind->position : core::KindPosition::Any;
    if (m_registry && position != core::KindPosition::Any) {
        const bool start = position == core::KindPosition::Start;

        // The places in reading order: before each element of the body and, in a group that
        // can have the element, before each of its elements and at its end; then the end
        const core::ProjectElement* firstGroup = nullptr;
        const core::ProjectElement* lastGroup = nullptr;
        for (qsizetype i = 0; i <= m_body.size(); ++i) {
            m_places.append({QString(), i});
            if (i == m_body.size() || !canBeInside(m_body.at(i))) {
                continue;
            }
            const core::ProjectElement& group = m_body.at(i);
            if (!firstGroup) {
                firstGroup = &group;
            }
            lastGroup = &group;
            for (qsizetype j = 0; j <= group.elements.size(); ++j) {
                m_places.append({group.id, j});
            }
        }

        // The start of the body and of its first group, or the end of the body and of its
        // last group
        m_firstOption = start ? 0 : m_places.size() - 1;
        m_firstButton->setText(start ? tr("At the start of the body of the book")
                                     : tr("At the end of the body of the book"));
        if (const core::ProjectElement* group = start ? firstGroup : lastGroup) {
            m_groupOption = m_places.indexOf(
                NewElementPlace{group->id, start ? 0 : group->elements.size()});
            m_groupButton->setText(start ? tr("First in \"%1\"").arg(group->title)
                                         : tr("Last in \"%1\"").arg(group->title));
        }

        // The dialog starts with the start (or the end) of the group it was opened on
        m_place = m_firstOption;
        for (const core::ProjectElement& element : std::as_const(m_body)) {
            if (element.id == m_openedOn && canBeInside(element)) {
                m_place = m_places.indexOf(
                    NewElementPlace{element.id, start ? 0 : element.elements.size()});
            }
        }
    }

    const bool shown = !m_places.isEmpty();
    m_groupButton->setVisible(m_groupOption >= 0);
    m_placeBox->setVisible(shown);
    contentLayout()->setStretch(m_placeStretch, shown ? 0 : 1);
    if (shown) {
        showPlace();
    } else {
        m_placeList->clear();
        m_newItem = nullptr;
    }
    updateDescription();

    // A dialog on the screen takes the height of what it shows now
    if (isVisible()) {
        adjustSize();
    }
}

void NewElementDialog::showPlace()
{
    auto& artProvider = core::ArtProvider::getInstance();
    const NewElementPlace chosen = m_places.at(m_place);
    const core::KindRef newKind = kind();

    m_placeList->clear();
    m_newItem = nullptr;

    // The new element, in the font of the list made bold, and selected
    const auto addNew = [&](QTreeWidgetItem* parent) {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_placeList);
        item->setText(0, title());
        const QString iconId = newKind && !newKind.kind->icon.isEmpty()
                                   ? newKind.kind->icon
                                   : QStringLiteral("template.chapter");
        item->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));
        QFont font = m_placeList->font();
        font.setBold(true);
        item->setFont(0, font);
        item->setData(0, PLACE_ROLE, -1);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_newItem = item;
    };

    // An element of the book: clicking it puts the new element before it, if it can go there
    const auto addElement = [&](QTreeWidgetItem* parent, const core::ProjectElement& element,
                                const NewElementPlace& before) {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_placeList);
        item->setText(0, element.title);
        item->setIcon(0, artProvider.getIcon(NavigatorPanel::iconIdOf(*m_registry, element),
                                             core::IconContext::TreeView));
        item->setData(0, PLACE_ROLE, static_cast<qlonglong>(m_places.indexOf(before)));
        item->setFlags(Qt::ItemIsEnabled);
        return item;
    };

    for (qsizetype i = 0; i <= m_body.size(); ++i) {
        if (chosen == NewElementPlace{QString(), i}) {
            addNew(nullptr);
        }
        if (i == m_body.size()) {
            break;
        }
        const core::ProjectElement& element = m_body.at(i);
        QTreeWidgetItem* item = addElement(nullptr, element, {QString(), i});
        for (qsizetype j = 0; j <= element.elements.size(); ++j) {
            if (chosen == NewElementPlace{element.id, j}) {
                addNew(item);
            }
            if (j < element.elements.size()) {
                addElement(item, element.elements.at(j), {element.id, j});
            }
        }
    }
    m_placeList->expandAll();
    if (m_newItem) {
        m_placeList->setCurrentItem(m_newItem);
        m_placeList->scrollToItem(m_newItem);
    }

    // The option of the place: one of the first two, or any other place
    {
        const QSignalBlocker first(m_firstButton);
        const QSignalBlocker group(m_groupButton);
        const QSignalBlocker elsewhere(m_elsewhereButton);
        if (m_place == m_firstOption) {
            m_firstButton->setChecked(true);
        } else if (m_place == m_groupOption) {
            m_groupButton->setChecked(true);
        } else {
            m_elsewhereButton->setChecked(true);
        }
    }
    m_upButton->setEnabled(m_place > 0);
    m_downButton->setEnabled(m_place < m_places.size() - 1);
}

void NewElementDialog::moveTo(qsizetype index)
{
    if (index < 0 || index >= m_places.size() || index == m_place) {
        return;
    }
    m_place = index;
    showPlace();
}

void NewElementDialog::onElementClicked(QTreeWidgetItem* item)
{
    if (item && item != m_newItem) {
        moveTo(item->data(0, PLACE_ROLE).toLongLong());
    }
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
