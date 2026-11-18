// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_templates_dialog.cpp
/// @brief Templates of Texts for Publishers: the templates of the book types and the writer's
///        own

#include "kalahari/gui/dialogs/publishing_templates_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/localized_text.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

using core::LocalizedText;
using core::PublishingTemplate;
using core::PublishingTemplateField;
using core::PublishingTemplatePoint;
using core::PublishingTexts;

/// The template of a row of the list (PublishingTemplate::reference()); none for a group
constexpr int TEMPLATE_ROLE = Qt::UserRole;
/// The book type of a box of the types
constexpr const char* TYPE_PROPERTY = "publishingType";
/// The width of the list of the templates, in average characters
constexpr int LIST_CHARACTERS = 26;
/// The width of the column of the names of the points, in average characters
constexpr int POINT_CHARACTERS = 22;
/// The width of the column of the names of the fields, in average characters
constexpr int FIELD_CHARACTERS = 18;
/// The largest target length
constexpr int MAX_TARGET = 100000;

/// What the keys of the list and of the points do
enum class TemplateKey { None, Add, Remove, Up, Down };

TemplateKey templateKeyOf(QKeyCombination keys) {
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    if (modifiers == Qt::AltModifier) {
        if (keys.key() == Qt::Key_Up) {
            return TemplateKey::Up;
        }
        return keys.key() == Qt::Key_Down ? TemplateKey::Down : TemplateKey::None;
    }
    if (modifiers != Qt::NoModifier) {
        return TemplateKey::None;
    }
    if (keys.key() == Qt::Key_Insert) {
        return TemplateKey::Add;
    }
    return keys.key() == Qt::Key_Delete ? TemplateKey::Remove : TemplateKey::None;
}

/// A text of the writer's own template: none for an empty one
LocalizedText ownText(const QString& text) {
    return text.isEmpty() ? LocalizedText() : LocalizedText::plain(text);
}

/// @p text in the program's language
QString shown(const LocalizedText& text) {
    return text.text(programLanguage());
}

/// @p templ as the writer's own template keeps it: one text each, in the program's language
PublishingTemplate ownCopy(const PublishingTemplate& templ) {
    PublishingTemplate copy = templ;
    copy.package.clear();
    copy.extra = {};
    copy.name = ownText(shown(templ.name));
    copy.hint = ownText(shown(templ.hint));
    copy.pattern = ownText(shown(templ.pattern));
    for (PublishingTemplatePoint& point : copy.points) {
        point.name = ownText(shown(point.name));
        point.hint = ownText(shown(point.hint));
    }
    for (PublishingTemplateField& field : copy.fields) {
        field.name = ownText(shown(field.name));
        field.hint = ownText(shown(field.hint));
    }
    return copy;
}

/// A new id of a point, unique among @p points: "p1", "p2"...
QString newPointId(const QList<PublishingTemplatePoint>& points) {
    for (int number = 1;; ++number) {
        const QString id = QStringLiteral("p%1").arg(number);
        if (std::none_of(points.cbegin(), points.cend(),
                         [&id](const PublishingTemplatePoint& point) { return point.id == id; })) {
            return id;
        }
    }
}

/// A cell of a table that the writer can change (@p editable) or not
QTableWidgetItem* cell(const QString& text, bool editable) {
    auto* item = new QTableWidgetItem(text);
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (editable) {
        flags |= Qt::ItemIsEditable;
    }
    item->setFlags(flags);
    item->setToolTip(text);  // the whole text in a narrow column
    return item;
}

/// The rows a table of the window asks for: it takes more where the window has room
constexpr int TABLE_ROWS = 3;

/// A table of the window, as high as a few rows unless the window has room for more
class TemplateTable : public QTableWidget {
public:
    using QTableWidget::QTableWidget;

    [[nodiscard]] QSize sizeHint() const override {
        return {QTableWidget::sizeHint().width(),
                horizontalHeader()->sizeHint().height() +
                    verticalHeader()->defaultSectionSize() * TABLE_ROWS + 2 * frameWidth()};
    }
};

/// Widgets in lines (FlowLayout) that ask only for the width of the widest of them: the lines
/// the width they get needs come from the layout's height for that width
class WrappingRow : public QWidget {
public:
    using QWidget::QWidget;

    [[nodiscard]] QSize sizeHint() const override {
        return {minimumSizeHint().width(), QWidget::sizeHint().height()};
    }
};

/// A table of the window with the columns @p headers
QTableWidget* newTable(const QStringList& headers, const char* name, QWidget* parent) {
    auto* table = new TemplateTable(0, static_cast<int>(headers.size()), parent);
    table->setObjectName(QString::fromLatin1(name));
    table->setHorizontalHeaderLabels(headers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setTabKeyNavigation(false);
    table->setWordWrap(false);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
}

/// A button of the window with its icon, which Enter in a field does not press
QPushButton* makeButton(const QString& text, const char* name, const char* iconId,
                       QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoDefault(false);
    card::keepIcon(button, QString::fromLatin1(iconId), core::IconContext::Toolbar);
    return button;
}

/// A sentence under a field: wrapped, in the color of a placeholder
QLabel* hintLabel(const QString& text, QWidget* parent) {
    auto* hint = new QLabel(text, parent);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    return hint;
}

/// The list of the templates, whose keys pass over the names of the groups
class TemplateList : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    /// The template nearest to the row a key goes to, past the names of the groups: Home and
    /// End go to the first and the last template, Page Up and Page Down stop at a template
    QModelIndex moveCursor(CursorAction action, Qt::KeyboardModifiers modifiers) override {
        const QModelIndex next = QListWidget::moveCursor(action, modifiers);
        if (!next.isValid() || next.flags().testFlag(Qt::ItemIsEnabled)) {
            return next;
        }
        const int step = movesForward(action) ? 1 : -1;
        for (const int direction : {step, -step}) {
            if (const int row = templateRow(next.row() + direction, direction); row >= 0) {
                return indexFromItem(item(row));
            }
        }
        return currentIndex();
    }

private:
    /// Whether @p action goes down the list
    static bool movesForward(CursorAction action) {
        return action == MoveDown || action == MovePageDown || action == MoveEnd ||
               action == MoveNext;
    }

    /// The first row of a template from @p row on, going by @p step; -1 for none
    [[nodiscard]] int templateRow(int row, int step) const {
        for (; row >= 0 && row < count(); row += step) {
            if (item(row)->flags().testFlag(Qt::ItemIsEnabled)) {
                return row;
            }
        }
        return -1;
    }
};

/// The name of a group of the list in @p list
void addGroupRow(QListWidget* list, const QString& name) {
    auto* row = new QListWidgetItem(name, list);
    row->setFlags(Qt::NoItemFlags);  // the arrows go past it
    QFont font = list->font();
    font.setBold(true);
    row->setFont(font);
}

/// The row of template @p templ in @p list
void addTemplateRow(QListWidget* list, const PublishingTemplate& templ) {
    auto* row = new QListWidgetItem(shown(templ.name), list);
    row->setData(TEMPLATE_ROLE, templ.reference());
    row->setIcon(core::ArtProvider::getInstance().getIcon(PublishingNames::partIconId(templ.part),
                                                          core::IconContext::Menu));
    row->setToolTip(PublishingNames::partName(templ.part));
}

/// What a key of the points does
void runPointKey(PublishingTemplatesDialog& dialog, TemplateKey key) {
    switch (key) {
        case TemplateKey::Add:
            dialog.addPoint();
            break;
        case TemplateKey::Remove:
            dialog.removePoint();
            break;
        case TemplateKey::Up:
            dialog.movePoint(-1);
            break;
        default:  // TemplateKey::Down
            dialog.movePoint(1);
            break;
    }
}

}  // namespace

// =============================================================================
// The window
// =============================================================================

PublishingTemplatesDialog::PublishingTemplatesDialog(QList<PublishingTemplateGroup> builtIn,
                                                     QList<PublishingBookType> types,
                                                     QString bookType,
                                                     QList<std::pair<QString, QString>> texts,
                                                     QWidget* parent)
    : KalahariDialog(parent)
    , m_builtIn(std::move(builtIn))
    , m_types(std::move(types))
    , m_bookType(std::move(bookType))
    , m_texts(std::move(texts))
    , m_own(PublishingTexts::ownTemplates()) {
    setObjectName(QStringLiteral("publishingTemplatesDialog"));
    setHeadingIcon(QStringLiteral("tools.publishing.templates"));
    setCompactHeading(true);
    const auto type = std::find_if(m_types.cbegin(), m_types.cend(),
                                   [this](const PublishingBookType& each) {
                                       return each.id == m_bookType;
                                   });
    //: The title of the window. In Polish: Szablony tekstów dla wydawnictwa
    const QString title = tr("Templates of Texts for Publishers");
    if (type != m_types.cend()) {
        //: %1: the book type of the book, "Novel". In Polish: Szablony typu „%1” i Twoje
        //: własne, zapisywane od razu, we wszystkich Twoich książkach. Szablon podpowiada, co
        //: napisać, i ustala docelową długość.
        setHeading(title, tr("The templates of the type \"%1\" and your own, kept at once, in "
                             "all your books. A template suggests what to write and sets the "
                             "target length.")
                              .arg(type->name));
    } else {
        //: In Polish: Szablony typów książek i Twoje własne, zapisywane od razu, we wszystkich
        //: Twoich książkach. Szablon podpowiada, co napisać, i ustala docelową długość.
        setHeading(title, tr("The templates of the book types and your own, kept at once, in "
                             "all your books. A template suggests what to write and sets the "
                             "target length."));
    }
    auto* row = new QHBoxLayout();
    row->addWidget(createList());
    row->addWidget(createForm(), 1);
    contentLayout()->addLayout(row, 1);
    //: The button that closes the window of the templates, whose changes are kept at once.
    //: In Polish: Zamknij
    setAcceptText(tr("Close"));
    cancelButton()->hide();
    fillList(QString());
}

QList<PublishingTemplateGroup> PublishingTemplatesDialog::builtInOf(
    const core::ProjectManager* book) {
    const core::BookTypeRegistry& registry = core::ProjectManager::installedBookTypes();
    const QString language = programLanguage();
    const core::BookProject* project = book != nullptr ? book->project() : nullptr;
    if (project != nullptr && project->type.has_value()) {
        const core::BookTypePackage* type = registry.package(project->type->id);
        const QString name = type != nullptr ? type->name.text(language) : project->type->id;
        return {{name, registry.publishingTemplates(project->type->id)}};
    }
    QList<PublishingTemplateGroup> groups;
    for (const core::BookTypePackage* type : registry.bookTypes()) {
        if (!type->publishingTemplates.isEmpty()) {
            groups.append({type->name.text(language), type->publishingTemplates});
        }
    }
    return groups;
}

QList<PublishingBookType> PublishingTemplatesDialog::installedTypes() {
    QList<PublishingBookType> types;
    const QString language = programLanguage();
    for (const core::BookTypePackage* type :
         core::ProjectManager::installedBookTypes().bookTypes()) {
        types.append({type->id, type->name.text(language)});
    }
    return types;
}

bool PublishingTemplatesDialog::editTemplates(const core::ProjectManager* book,
                                              QWidget* parent) {
    const core::BookProject* project = book != nullptr ? book->project() : nullptr;
    const QString type = project != nullptr && project->type.has_value() ? project->type->id
                                                                          : QString();
    PublishingTemplatesDialog dialog(builtInOf(book), installedTypes(), type,
                                     book != nullptr ? book->mainTexts()
                                                     : QList<std::pair<QString, QString>>(),
                                     parent);
    dialog.exec();
    return dialog.changed();
}

QWidget* PublishingTemplatesDialog::createList() {
    auto* column = new QWidget(this);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    //: The list of the templates. In Polish: &Szablony:
    auto* label = new QLabel(tr("&Templates:"), column);
    m_list = new TemplateList(column);
    m_list->setObjectName(QStringLiteral("publishingTemplatesList"));
    m_list->setMinimumWidth(fontMetrics().averageCharWidth() * LIST_CHARACTERS);
    m_list->installEventFilter(this);
    label->setBuddy(m_list);
    layout->addWidget(label);
    layout->addWidget(m_list, 1);

    auto* buttons = new WrappingRow(column);
    auto* flow = new FlowLayout(buttons);
    flow->setContentsMargins(0, 0, 0, 0);
    //: Makes a new template of the writer. In Polish: &Nowy szablon
    m_newButton = makeButton(tr("&New Template"), "publishingTemplatesNew", "common.add", buttons);
    m_newButton->setToolTip(ShortcutRules::keysText(Qt::Key_Insert));
    //: Makes the template chosen a template of the writer. In Polish: Utwórz &kopię
    m_copyButton = makeButton(tr("Make a &Copy"), "publishingTemplatesCopy", "edit.copy", buttons);
    //: Deletes the writer's template chosen. In Polish: &Usuń
    m_deleteButton = makeButton(tr("&Delete"), "publishingTemplatesDelete", "common.delete",
                               buttons);
    m_deleteButton->setToolTip(ShortcutRules::keysText(Qt::Key_Delete));
    flow->addWidget(m_newButton);
    flow->addWidget(m_copyButton);
    flow->addWidget(m_deleteButton);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](const QListWidgetItem* item) {
        if (item != nullptr) {
            m_current = item->data(TEMPLATE_ROLE).toString();
            showTemplate();
        }
    });
    connect(m_newButton, &QPushButton::clicked, this, &PublishingTemplatesDialog::newTemplate);
    connect(m_copyButton, &QPushButton::clicked, this, &PublishingTemplatesDialog::copyTemplate);
    connect(m_deleteButton, &QPushButton::clicked, this,
            &PublishingTemplatesDialog::deleteTemplate);
    return column;
}

QWidget* PublishingTemplatesDialog::createForm() {
    auto* side = new QWidget(this);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    //: Where the fields of a template are while none is chosen. In Polish: Wybierz szablon
    //: z listy albo utwórz nowy.
    m_empty = hintLabel(tr("Choose a template in the list, or make a new one."), side);
    m_empty->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_form = new QWidget(side);
    auto* fields = new QFormLayout(m_form);
    fields->setContentsMargins(0, 0, 0, 0);
    createBasics(fields);
    createPoints(fields);
    createPattern(fields);
    sideLayout->addWidget(m_empty, 1);
    sideLayout->addWidget(m_form, 1);
    return side;
}

void PublishingTemplatesDialog::createBasics(QFormLayout* fields) {
    m_nameEdit = new QLineEdit(m_form);
    m_nameEdit->setObjectName(QStringLiteral("publishingTemplateName"));
    m_partBox = new QComboBox(m_form);
    m_partBox->setObjectName(QStringLiteral("publishingTemplatePart"));
    for (const char* part : {PublishingTexts::SYNOPSIS, PublishingTexts::PITCH}) {
        m_partBox->addItem(PublishingNames::partName(QString::fromLatin1(part)),
                           QString::fromLatin1(part));
    }
    //: What a template is for: a synopsis or a sentence about the book. In Polish: &Rodzaj
    //: tekstu:
    auto* partLabel = new QLabel(tr("&Part:"), m_form);
    partLabel->setBuddy(m_partBox);
    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(m_nameEdit, 1);
    nameRow->addWidget(partLabel);
    nameRow->addWidget(m_partBox);
    //: The name of a template. In Polish: N&azwa:
    auto* nameLabel = new QLabel(tr("N&ame:"), m_form);
    nameLabel->setBuddy(m_nameEdit);
    fields->addRow(nameLabel, nameRow);
    //: The book types a template is for. In Polish: &Typy książek:
    auto* typesLabel = new QLabel(tr("Book t&ypes:"), m_form);
    fields->addRow(typesLabel, createTypes(m_form));
    typesLabel->setBuddy(m_typeChecks.value(0));
    //: The target length of a template. In Polish: Dług&ość:
    auto* lengthLabel = new QLabel(tr("&Length:"), m_form);
    fields->addRow(lengthLabel, createLength(m_form));
    lengthLabel->setBuddy(m_charactersBox);

    connect(m_nameEdit, &QLineEdit::textEdited, this, &PublishingTemplatesDialog::onNameEdited);
    connect(m_partBox, &QComboBox::currentIndexChanged, this,
            &PublishingTemplatesDialog::onPartChanged);
}

QWidget* PublishingTemplatesDialog::createTypes(QWidget* form) {
    auto* types = new QWidget(form);
    auto* layout = new QVBoxLayout(types);
    layout->setContentsMargins(0, 0, 0, 0);
    m_typeLabel = new QLabel(types);
    m_typeLabel->setObjectName(QStringLiteral("publishingTemplateTypeLabel"));
    m_typeLabel->setWordWrap(true);
    layout->addWidget(m_typeLabel);
    m_typeBoxes = new WrappingRow(types);
    auto* flow = new FlowLayout(m_typeBoxes);
    flow->setContentsMargins(0, 0, 0, 0);
    for (const PublishingBookType& type : std::as_const(m_types)) {
        auto* box = new QCheckBox(type.name, m_typeBoxes);
        box->setObjectName(QStringLiteral("publishingTemplateType_") + type.id);
        box->setProperty(TYPE_PROPERTY, type.id);
        flow->addWidget(box);
        m_typeChecks << box;
        connect(box, &QCheckBox::toggled, this, &PublishingTemplatesDialog::onTypesChanged);
    }
    //: After the boxes of the book types of the writer's template. In Polish: (bez
    //: zaznaczenia: każdy typ)
    QLabel* every = hintLabel(tr("(none ticked: every type)"), m_typeBoxes);
    every->setWordWrap(false);
    flow->addWidget(every);
    layout->addWidget(m_typeBoxes);
    return types;
}

QWidget* PublishingTemplatesDialog::createLength(QWidget* form) {
    auto* length = new QWidget(form);
    auto* layout = new QVBoxLayout(length);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout();
    auto spin = [length](const char* name) {
        auto* box = new QSpinBox(length);
        box->setObjectName(QString::fromLatin1(name));
        box->setRange(0, MAX_TARGET);
        //: A target length of 0: the text takes the length of the Settings. In Polish: brak
        box->setSpecialValueText(tr("none"));
        return box;
    };
    m_charactersBox = spin("publishingTemplateCharacters");
    m_wordsBox = spin("publishingTemplateWords");
    //: After the target length in characters. In Polish: znaków ze spacjami
    auto* characters = new QLabel(tr("characters with spaces"), length);
    //: After the target length in words. In Polish: słó&w
    auto* words = new QLabel(tr("&words"), length);
    words->setBuddy(m_wordsBox);
    row->addWidget(m_charactersBox);
    row->addWidget(characters);
    row->addWidget(m_wordsBox);
    row->addWidget(words);
    row->addStretch();
    layout->addLayout(row);
    m_lengthNote = hintLabel(QString(), length);
    m_lengthNote->setObjectName(QStringLiteral("publishingTemplateLengthNote"));
    layout->addWidget(m_lengthNote);
    for (QSpinBox* box : {m_charactersBox, m_wordsBox}) {
        connect(box, &QSpinBox::valueChanged, this, &PublishingTemplatesDialog::onLengthChanged);
    }
    return length;
}

void PublishingTemplatesDialog::createPoints(QFormLayout* fields) {
    m_pointsSection = new QWidget(m_form);
    auto* row = new QHBoxLayout(m_pointsSection);
    row->setContentsMargins(0, 0, 0, 0);
    m_pointsTable = newTable({//: The column of the names of the points. In Polish: Punkt
                              tr("Point"),
                              //: The column of the hints of the points. In Polish: Podpowiedź
                              tr("Hint")},
                             "publishingTemplatePoints", m_pointsSection);
    m_pointsTable->setColumnWidth(POINT_NAME_COLUMN,
                                  fontMetrics().averageCharWidth() * POINT_CHARACTERS);
    m_pointsTable->installEventFilter(this);
    auto* column = new QVBoxLayout();
    column->addWidget(m_pointsTable, 1);
    m_pointsNote = hintLabel(QString(), m_pointsSection);
    column->addWidget(m_pointsNote);
    row->addLayout(column, 1);

    m_pointButtons = new QWidget(m_pointsSection);
    auto* buttons = new QVBoxLayout(m_pointButtons);
    buttons->setContentsMargins(0, 0, 0, 0);
    //: In Polish: Doda&j punkt
    m_addPointButton = makeButton(tr("Add P&oint"), "publishingTemplateAddPoint", "common.add",
                                  m_pointButtons);
    m_addPointButton->setToolTip(ShortcutRules::keysText(Qt::Key_Insert));
    //: In Polish: Usuń &punkt
    m_removePointButton = makeButton(tr("R&emove Point"), "publishingTemplateRemovePoint",
                                     "common.remove", m_pointButtons);
    m_removePointButton->setToolTip(ShortcutRules::keysText(Qt::Key_Delete));
    //: In Polish: W &górę
    m_pointUpButton = makeButton(tr("Move &Up"), "publishingTemplatePointUp",
                                 "common.arrowUpward", m_pointButtons);
    m_pointUpButton->setToolTip(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Up));
    //: In Polish: W &dół
    m_pointDownButton = makeButton(tr("Mo&ve Down"), "publishingTemplatePointDown",
                                   "common.arrowDownward", m_pointButtons);
    m_pointDownButton->setToolTip(ShortcutRules::keysText(Qt::AltModifier | Qt::Key_Down));
    for (QPushButton* button :
         {m_addPointButton, m_removePointButton, m_pointUpButton, m_pointDownButton}) {
        buttons->addWidget(button);
    }
    buttons->addStretch();
    row->addWidget(m_pointButtons);
    //: The points of a template of a synopsis. In Polish: Punkt&y:
    auto* label = new QLabel(tr("Po&ints:"), m_form);
    label->setBuddy(m_pointsTable);
    fields->addRow(label, m_pointsSection);

    connect(m_pointsTable, &QTableWidget::cellChanged, this,
            &PublishingTemplatesDialog::onPointChanged);
    connect(m_pointsTable, &QTableWidget::currentCellChanged, this,
            &PublishingTemplatesDialog::updateButtons);
    connect(m_addPointButton, &QPushButton::clicked, this, &PublishingTemplatesDialog::addPoint);
    connect(m_removePointButton, &QPushButton::clicked, this,
            &PublishingTemplatesDialog::removePoint);
    connect(m_pointUpButton, &QPushButton::clicked, this, [this]() { movePoint(-1); });
    connect(m_pointDownButton, &QPushButton::clicked, this, [this]() { movePoint(1); });
}

void PublishingTemplatesDialog::createPattern(QFormLayout* fields) {
    m_patternSection = new QWidget(m_form);
    auto* layout = new QVBoxLayout(m_patternSection);
    layout->setContentsMargins(0, 0, 0, 0);
    m_patternEdit = new QLineEdit(m_patternSection);
    m_patternEdit->setObjectName(QStringLiteral("publishingTemplatePattern"));
    //: In Polish: Na przykład: Gdy {event}, {hero} musi {goal}, zanim {stakes}.
    m_patternEdit->setPlaceholderText(
        tr("For example: When {event}, {hero} must {goal}, before {stakes}."));
    layout->addWidget(m_patternEdit);
    //: Under the pattern. In Polish: Pola wzoru są w nawiasach klamrowych: {hero}. Każde ma
    //: swoje miejsce na karcie zdania.
    layout->addWidget(hintLabel(tr("The fields go in braces: {hero}. Each gets a box on the "
                                   "card of the sentence."),
                                m_patternSection));
    //: The pattern of a sentence about the book. In Polish: W&zór:
    auto* patternLabel = new QLabel(tr("Patte&rn:"), m_form);
    patternLabel->setBuddy(m_patternEdit);
    fields->addRow(patternLabel, m_patternSection);

    m_fieldsTable = newTable({//: The column of the fields as the pattern names them. In
                              //: Polish: We wzorze
                              tr("In the Pattern"),
                              //: The column of the names of the fields. In Polish: Nazwa
                              tr("Name"),
                              //: The column of the hints of the fields. In Polish: Podpowiedź
                              tr("Hint")},
                             "publishingTemplateFields", m_form);
    m_fieldsTable->verticalHeader()->hide();
    m_fieldsTable->horizontalHeader()->setSectionResizeMode(FIELD_ID_COLUMN,
                                                            QHeaderView::ResizeToContents);
    m_fieldsTable->setColumnWidth(FIELD_NAME_COLUMN,
                                  fontMetrics().averageCharWidth() * FIELD_CHARACTERS);
    //: The fields of the pattern. In Polish: Po&la:
    auto* label = new QLabel(tr("&Fields:"), m_form);
    label->setBuddy(m_fieldsTable);
    fields->addRow(label, m_fieldsTable);

    connect(m_patternEdit, &QLineEdit::textEdited, this,
            &PublishingTemplatesDialog::onPatternEdited);
    connect(m_fieldsTable, &QTableWidget::cellChanged, this,
            &PublishingTemplatesDialog::onFieldChanged);
}

// =============================================================================
// What it shows
// =============================================================================

QString PublishingTemplatesDialog::currentTemplate() const {
    return templateOf(m_current) != nullptr ? m_current : QString();
}

void PublishingTemplatesDialog::setCurrentTemplate(const QString& reference) {
    if (templateOf(reference) != nullptr) {
        fillList(reference);
    }
}

QCheckBox* PublishingTemplatesDialog::typeBox(const QString& id) const {
    const auto found = std::find_if(m_typeChecks.cbegin(), m_typeChecks.cend(),
                                    [&id](const QCheckBox* box) {
                                        return box->property(TYPE_PROPERTY).toString() == id;
                                    });
    return found != m_typeChecks.cend() ? *found : nullptr;
}

void PublishingTemplatesDialog::fillList(const QString& current) {
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const PublishingTemplateGroup& group : std::as_const(m_builtIn)) {
            //: A group of the list of the templates: those of a book type. In Polish: %1 –
            //: wbudowane
            addGroupRow(m_list, tr("%1 – Built-in").arg(group.name));
            for (const PublishingTemplate& templ : group.templates) {
                addTemplateRow(m_list, templ);
            }
        }
        //: The group of the writer's own templates. In Polish: Moje szablony
        addGroupRow(m_list, tr("My Templates"));
        for (const PublishingTemplate& templ : std::as_const(m_own)) {
            addTemplateRow(m_list, templ);
        }
    }
    QListWidgetItem* chosen = nullptr;
    for (int row = 0; row < m_list->count() && chosen == nullptr; ++row) {
        QListWidgetItem* item = m_list->item(row);
        const QString reference = item->data(TEMPLATE_ROLE).toString();
        if (!reference.isEmpty() && (reference == current || current.isEmpty())) {
            chosen = item;
        }
    }
    m_current = chosen != nullptr ? chosen->data(TEMPLATE_ROLE).toString() : QString();
    {
        const QSignalBlocker blocker(m_list);
        m_list->setCurrentItem(chosen);
    }
    showTemplate();
}

void PublishingTemplatesDialog::showTemplate() {
    const PublishingTemplate* templ = templateOf(m_current);
    m_form->setVisible(templ != nullptr);
    m_empty->setVisible(templ == nullptr);
    if (templ != nullptr) {
        const bool own = templ->isOwn();
        const QScopedValueRollback<bool> showing(m_showing, true);
        m_nameEdit->setText(shown(templ->name));
        m_nameEdit->setReadOnly(!own);
        m_partBox->setCurrentIndex(m_partBox->findData(templ->part));
        m_partBox->setEnabled(own);
        showTypes(*templ, own);
        showLength(*templ, own);
        const bool points = PublishingTexts::hasPoints(templ->part);
        auto* fields = static_cast<QFormLayout*>(m_form->layout());
        fields->setRowVisible(m_pointsSection, points);
        fields->setRowVisible(m_patternSection, !points);
        fields->setRowVisible(m_fieldsTable, !points);
        m_patternEdit->setText(shown(templ->pattern));
        m_patternEdit->setReadOnly(!own);
        fillPoints(*templ, own);
        fillFields(*templ, own);
    }
    updateButtons();
}

void PublishingTemplatesDialog::showTypes(const PublishingTemplate& templ, bool own) {
    m_typeLabel->setVisible(!own);
    m_typeBoxes->setVisible(own);
    if (!own) {
        const auto type = std::find_if(m_types.cbegin(), m_types.cend(),
                                       [&templ](const PublishingBookType& each) {
                                           return each.id == templ.package;
                                       });
        //: Where the book types of a template of a book type are. %1: the type, "Novel". In
        //: Polish: %1 – szablon typu książki, który się nie zmienia. Utwórz kopię, by go
        //: zmienić.
        m_typeLabel->setText(tr("%1 – a template of the book type, which does not change. Make "
                                "a Copy to change it.")
                                 .arg(type != m_types.cend() ? type->name : templ.package));
        return;
    }
    for (QCheckBox* box : std::as_const(m_typeChecks)) {
        box->setChecked(templ.types.contains(box->property(TYPE_PROPERTY).toString()));
    }
}

void PublishingTemplatesDialog::showLength(const PublishingTemplate& templ, bool own) {
    for (QSpinBox* box : {m_charactersBox, m_wordsBox}) {
        box->setReadOnly(!own);
        box->setButtonSymbols(own ? QAbstractSpinBox::UpDownArrows
                                  : QAbstractSpinBox::NoButtons);
    }
    m_charactersBox->setValue(templ.targetCharacters);
    m_wordsBox->setValue(templ.targetWords);
    m_lengthNote->setVisible(templ.eachText);
    const auto texts = static_cast<int>(m_texts.size());
    if (texts == 0) {
        //: The length of a template with a point for each text, without a book. In Polish:
        //: Dla każdego tekstu głównej części książki.
        m_lengthNote->setText(tr("For each text of the main part of the book."));
        return;
    }
    const QLocale locale;
    //: The length of a template with a point for each text. %1, %2: the length of all of
    //: them. In Polish: Dla każdego tekstu; ta książka ma %n tekst/teksty/tekstów, razem %1
    //: znaków (%2 słów).
    m_lengthNote->setText(tr("For each text; this book has %n texts, %1 characters (%2 words) "
                             "in all.",
                             nullptr, texts)
                              .arg(locale.toString(templ.targetCharacters * texts),
                                   locale.toString(templ.targetWords * texts)));
}

void PublishingTemplatesDialog::fillPoints(const PublishingTemplate& templ, bool own) {
    const QSignalBlocker blocker(m_pointsTable);
    const QList<PublishingTemplatePoint> points = templ.pointsFor(m_texts);
    const bool editable = own && !templ.eachText;
    m_pointsTable->setRowCount(static_cast<int>(points.size()));
    for (int row = 0; row < points.size(); ++row) {
        m_pointsTable->setItem(row, POINT_NAME_COLUMN, cell(shown(points[row].name), editable));
        m_pointsTable->setItem(row, POINT_HINT_COLUMN, cell(shown(points[row].hint), editable));
    }
    m_pointsTable->setEditTriggers(editable ? QAbstractItemView::DoubleClicked |
                                                  QAbstractItemView::EditKeyPressed |
                                                  QAbstractItemView::AnyKeyPressed
                                            : QAbstractItemView::NoEditTriggers);
    m_pointButtons->setVisible(editable);
    m_pointsNote->setVisible(templ.eachText);
    //: Under the points of a template with a point for each text. In Polish: Punkty to tytuły
    //: tekstów głównej części książki, więc zmieniają się razem z nimi.
    m_pointsNote->setText(tr("The points are the titles of the texts of the main part of the "
                             "book, so they change with them."));
    if (m_pointsTable->currentRow() < 0 && !points.isEmpty()) {
        m_pointsTable->setCurrentCell(0, POINT_NAME_COLUMN);
    }
}

void PublishingTemplatesDialog::fillFields(const PublishingTemplate& templ, bool own) {
    const QSignalBlocker blocker(m_fieldsTable);
    m_fieldsTable->setRowCount(static_cast<int>(templ.fields.size()));
    for (int row = 0; row < templ.fields.size(); ++row) {
        const PublishingTemplateField& field = templ.fields[row];
        m_fieldsTable->setItem(row, FIELD_ID_COLUMN,
                               cell(QLatin1Char('{') + field.id + QLatin1Char('}'), false));
        m_fieldsTable->setItem(row, FIELD_NAME_COLUMN, cell(shown(field.name), own));
        m_fieldsTable->setItem(row, FIELD_HINT_COLUMN, cell(shown(field.hint), own));
    }
    m_fieldsTable->setEditTriggers(own ? QAbstractItemView::DoubleClicked |
                                             QAbstractItemView::EditKeyPressed |
                                             QAbstractItemView::AnyKeyPressed
                                       : QAbstractItemView::NoEditTriggers);
}

void PublishingTemplatesDialog::updateButtons() {
    const PublishingTemplate* templ = templateOf(m_current);
    const bool own = templ != nullptr && templ->isOwn();
    m_copyButton->setEnabled(templ != nullptr);
    m_deleteButton->setEnabled(own);
    const bool points = own && !templ->eachText;
    const int row = m_pointsTable->currentRow();
    const int count = m_pointsTable->rowCount();
    m_removePointButton->setEnabled(points && row >= 0 && count > 1);
    m_pointUpButton->setEnabled(points && row > 0);
    m_pointDownButton->setEnabled(points && row >= 0 && row < count - 1);
}

const PublishingTemplate* PublishingTemplatesDialog::templateOf(const QString& reference) const {
    if (reference.isEmpty()) {
        return nullptr;
    }
    auto matches = [&reference](const PublishingTemplate& templ) {
        return templ.reference() == reference;
    };
    if (const auto own = std::find_if(m_own.cbegin(), m_own.cend(), matches);
        own != m_own.cend()) {
        return &*own;
    }
    for (const PublishingTemplateGroup& group : m_builtIn) {
        const auto found = std::find_if(group.templates.cbegin(), group.templates.cend(), matches);
        if (found != group.templates.cend()) {
            return &*found;
        }
    }
    return nullptr;
}

PublishingTemplate* PublishingTemplatesDialog::ownCurrent() {
    const auto found = std::find_if(m_own.begin(), m_own.end(),
                                    [this](const PublishingTemplate& templ) {
                                        return templ.reference() == m_current;
                                    });
    return found != m_own.end() ? &*found : nullptr;
}

// =============================================================================
// The changes
// =============================================================================

void PublishingTemplatesDialog::keep(bool listChanged) {
    PublishingTexts::setOwnTemplates(m_own);
    m_changed = true;
    if (listChanged) {
        fillList(m_current);
    }
    emit templatesChanged();
}

QString PublishingTemplatesDialog::freeName(const QString& base) const {
    auto taken = [this](const QString& name) {
        return std::any_of(m_own.cbegin(), m_own.cend(), [&name](const PublishingTemplate& t) {
            return shown(t.name) == name;
        });
    };
    QString name = base;
    for (int number = 2; taken(name); ++number) {
        name = QStringLiteral("%1 %2").arg(base).arg(number);
    }
    return name;
}

void PublishingTemplatesDialog::newTemplate() {
    const PublishingTemplate* chosen = templateOf(m_current);
    PublishingTemplate templ;
    templ.id = PublishingTexts::newOwnTemplateId(m_own);
    templ.part = chosen != nullptr ? chosen->part : QString::fromLatin1(PublishingTexts::SYNOPSIS);
    //: The name of a new template of the writer. In Polish: Nowy szablon
    templ.name = ownText(freeName(tr("New template")));
    if (!m_bookType.isEmpty()) {
        templ.types << m_bookType;
    }
    templ.targetCharacters = PublishingTexts::defaultTargetOf(templ.part, false);
    templ.targetWords = PublishingTexts::defaultTargetOf(templ.part, true);
    if (PublishingTexts::hasPoints(templ.part)) {
        //: The name of a new point of a template. In Polish: Punkt %1
        templ.points.append({QStringLiteral("p1"), ownText(tr("Point %1").arg(1)), {}});
    }
    m_own.append(templ);
    m_current = templ.reference();
    keep(true);
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->selectAll();
}

void PublishingTemplatesDialog::copyTemplate() {
    const PublishingTemplate* chosen = templateOf(m_current);
    if (chosen == nullptr) {
        return;
    }
    PublishingTemplate copy = ownCopy(*chosen);
    copy.id = PublishingTexts::newOwnTemplateId(m_own);
    //: The name of a copy of a template. In Polish: %1 – kopia
    copy.name = ownText(freeName(tr("%1 – copy").arg(shown(chosen->name))));
    if (!chosen->isOwn()) {
        copy.types = {m_bookType.isEmpty() ? chosen->package : m_bookType};
    }
    m_own.append(copy);
    m_current = copy.reference();
    keep(true);
    m_nameEdit->setFocus(Qt::OtherFocusReason);
    m_nameEdit->selectAll();
}

void PublishingTemplatesDialog::deleteTemplate() {
    const auto found = std::find_if(m_own.cbegin(), m_own.cend(),
                                    [this](const PublishingTemplate& templ) {
                                        return templ.reference() == m_current;
                                    });
    if (found == m_own.cend()) {
        return;
    }
    //: %1: the name of the template. In Polish: Usunąć szablon „%1”? Teksty, które z niego
    //: korzystają, zostają i nie mają już szablonu.
    const QString question = tr("Delete the template \"%1\"? The texts written with it stay, "
                                "without a template.")
                                 .arg(shown(found->name));
    //: The title of the question. In Polish: Usuń szablon
    if (!MessageDialog::confirm(this, tr("Delete Template"), question, tr("&Delete"),
                                MessageDialog::Kind::Warning)) {
        return;
    }
    const auto index = static_cast<int>(found - m_own.cbegin());
    m_own.removeAt(index);
    const int next = std::min(index, static_cast<int>(m_own.size()) - 1);
    m_current = next >= 0 ? m_own[next].reference() : QString();
    keep(true);
    m_list->setFocus(Qt::OtherFocusReason);
}

void PublishingTemplatesDialog::onNameEdited(const QString& name) {
    PublishingTemplate* templ = ownCurrent();
    if (m_showing || templ == nullptr) {
        return;
    }
    templ->name = ownText(name);
    if (QListWidgetItem* row = m_list->currentItem()) {
        row->setText(name);
    }
    keep(false);
}

void PublishingTemplatesDialog::onPartChanged(int index) {
    PublishingTemplate* templ = ownCurrent();
    if (m_showing || templ == nullptr || index < 0) {
        return;
    }
    templ->part = m_partBox->itemData(index).toString();
    if (PublishingTexts::hasPoints(templ->part) && templ->points.isEmpty()) {
        templ->points.append({QStringLiteral("p1"), ownText(tr("Point %1").arg(1)), {}});
    }
    keep(true);
    m_partBox->setFocus(Qt::OtherFocusReason);
}

void PublishingTemplatesDialog::onTypesChanged() {
    PublishingTemplate* templ = ownCurrent();
    if (m_showing || templ == nullptr) {
        return;
    }
    templ->types.clear();
    for (const QCheckBox* box : std::as_const(m_typeChecks)) {
        if (box->isChecked()) {
            templ->types << box->property(TYPE_PROPERTY).toString();
        }
    }
    keep(false);
}

void PublishingTemplatesDialog::onLengthChanged() {
    PublishingTemplate* templ = ownCurrent();
    if (m_showing || templ == nullptr) {
        return;
    }
    templ->targetCharacters = m_charactersBox->value();
    templ->targetWords = m_wordsBox->value();
    keep(false);
}

void PublishingTemplatesDialog::onPointChanged(int row, int column) {
    PublishingTemplate* templ = ownCurrent();
    if (templ == nullptr || row < 0 || row >= templ->points.size()) {
        return;
    }
    const LocalizedText text = ownText(m_pointsTable->item(row, column)->text());
    PublishingTemplatePoint& point = templ->points[row];
    (column == POINT_NAME_COLUMN ? point.name : point.hint) = text;
    keep(false);
}

void PublishingTemplatesDialog::onPatternEdited(const QString& pattern) {
    PublishingTemplate* templ = ownCurrent();
    if (m_showing || templ == nullptr) {
        return;
    }
    templ->pattern = ownText(pattern);
    // The fields the pattern names, those it had before with their names and hints
    QList<PublishingTemplateField> fields;
    for (const QString& id : PublishingTexts::fieldsOf(pattern)) {
        const auto before = std::find_if(templ->fields.cbegin(), templ->fields.cend(),
                                         [&id](const PublishingTemplateField& field) {
                                             return field.id == id;
                                         });
        fields.append(before != templ->fields.cend() ? *before
                                                     : PublishingTemplateField{id, ownText(id),
                                                                               {}});
    }
    templ->fields = fields;
    fillFields(*templ, true);
    keep(false);
}

void PublishingTemplatesDialog::onFieldChanged(int row, int column) {
    PublishingTemplate* templ = ownCurrent();
    if (templ == nullptr || row < 0 || row >= templ->fields.size()) {
        return;
    }
    const LocalizedText text = ownText(m_fieldsTable->item(row, column)->text());
    PublishingTemplateField& field = templ->fields[row];
    (column == FIELD_NAME_COLUMN ? field.name : field.hint) = text;
    keep(false);
}

void PublishingTemplatesDialog::addPoint() {
    PublishingTemplate* templ = ownCurrent();
    if (templ == nullptr || templ->eachText || !PublishingTexts::hasPoints(templ->part)) {
        return;
    }
    const int current = m_pointsTable->currentRow();
    const int row = current < 0 ? static_cast<int>(templ->points.size()) : current + 1;
    PublishingTemplatePoint point;
    point.id = newPointId(templ->points);
    point.name = ownText(tr("Point %1").arg(row + 1));
    templ->points.insert(row, point);
    fillPoints(*templ, true);
    keep(false);
    m_pointsTable->setFocus(Qt::OtherFocusReason);
    m_pointsTable->setCurrentCell(row, POINT_NAME_COLUMN);
    m_pointsTable->editItem(m_pointsTable->item(row, POINT_NAME_COLUMN));
}

void PublishingTemplatesDialog::removePoint() {
    PublishingTemplate* templ = ownCurrent();
    const int row = m_pointsTable->currentRow();
    if (templ == nullptr || templ->eachText || row < 0 || templ->points.size() < 2) {
        return;
    }
    templ->points.removeAt(row);
    fillPoints(*templ, true);
    keep(false);
    m_pointsTable->setCurrentCell(std::min(row, static_cast<int>(templ->points.size()) - 1),
                                  POINT_NAME_COLUMN);
}

void PublishingTemplatesDialog::movePoint(int step) {
    PublishingTemplate* templ = ownCurrent();
    const int row = m_pointsTable->currentRow();
    const int target = row + step;
    if (templ == nullptr || templ->eachText || row < 0 || target < 0 ||
        target >= templ->points.size()) {
        return;
    }
    templ->points.swapItemsAt(row, target);
    const int column = std::max(m_pointsTable->currentColumn(), 0);
    fillPoints(*templ, true);
    keep(false);
    m_pointsTable->setCurrentCell(target, column);
}

bool PublishingTemplatesDialog::eventFilter(QObject* watched, QEvent* event) {
    const bool keys =
        event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress;
    const bool points = watched == m_pointsTable;
    if (keys && (points || watched == m_list)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (runKey(ShortcutRules::keysOf(key), points, event->type() == QEvent::KeyPress)) {
            key->accept();
            return true;
        }
    }
    return KalahariDialog::eventFilter(watched, event);
}

bool PublishingTemplatesDialog::runKey(QKeyCombination keys, bool points, bool run) {
    const TemplateKey key = templateKeyOf(keys);
    const bool moves = key == TemplateKey::Up || key == TemplateKey::Down;
    if (key == TemplateKey::None || (moves && !points)) {
        return false;
    }
    if (!run) {
        return true;
    }
    if (points) {
        runPointKey(*this, key);
    } else if (key == TemplateKey::Add) {
        newTemplate();
    } else {
        deleteTemplate();
    }
    return true;
}

}  // namespace kalahari::gui::dialogs
