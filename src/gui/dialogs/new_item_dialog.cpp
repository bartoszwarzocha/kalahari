/// @file new_item_dialog.cpp
/// @brief Implementation of NewItemDialog
///
/// OpenSpec #00033: Project File System - Phase C

#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPalette>
#include <QPixmap>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>

#include <algorithm>
#include <string>
#include <vector>

using namespace kalahari::gui::dialogs;
using kalahari::gui::ProgramFolders;

namespace {

constexpr int MINIMUM_WIDTH = 700;          ///< The description and two templates in a row
constexpr QSize DEFAULT_SIZE{850, 620};     ///< The templates and the details, under the heading

/// The writer's last choice in the Sections field for a template that shows sections
constexpr const char* SECTIONS_SETTING = "project.sectionSet";

/// The writer's own names of the sections, when the last choice was own names
constexpr const char* SECTION_NAMES_SETTING = "project.sectionNames";

/// The book types of the program, in the order the New Book window lists them; the types of
/// other packages follow by name
constexpr const char* TYPE_ORDER[] = {"kalahari.novel", "kalahari.short_stories",
                                      "kalahari.nonfiction", "kalahari.screenplay",
                                      "kalahari.poetry"};

/// The places of a book and its Workshop, in the order the description names them
constexpr kalahari::core::BookPlace DESCRIBED_PLACES[] = {
    kalahari::core::BookPlace::Front, kalahari::core::BookPlace::Main,
    kalahari::core::BookPlace::Back, kalahari::core::BookPlace::Workshop};

/// Icon of the writer's own types
constexpr const char* CUSTOM_TYPE_ICON = "template.custom";

/// Icon of a user project
constexpr const char* USER_PROJECT_ICON = "template.empty";

/// Link of the description that deletes the writer's own type
constexpr const char* DELETE_TYPE_LINK = "delete-type";

/// Whether @p kinds has @p kind
bool hasKind(const QList<kalahari::core::KindRef>& kinds, const kalahari::core::KindRef& kind) {
    return std::any_of(kinds.cbegin(), kinds.cend(),
                       [&kind](const kalahari::core::KindRef& other) {
                           return other.kind == kind.kind;
                       });
}

/// Set of names @p id, or the first set when the program has no such set
const kalahari::core::SectionNameSet& sectionNameSet(const QString& id) {
    const QList<kalahari::core::SectionNameSet>& sets =
        kalahari::core::ProjectBook::sectionNameSets();
    const auto found = std::find_if(sets.cbegin(), sets.cend(),
                                    [&id](const kalahari::core::SectionNameSet& set) {
                                        return set.id == id;
                                    });
    return found != sets.cend() ? *found : sets.first();
}

}  // namespace

// ============================================================================
// Constructor
// ============================================================================

NewItemDialog::NewItemDialog(NewItemMode mode, QWidget* parent)
    : KalahariDialog(parent)
    , m_mode(mode)
    , m_iconLabel(nullptr)
    , m_titleLabel(nullptr)
    , m_descriptionLabel(nullptr)
    , m_templateList(nullptr)
    , m_searchEdit(nullptr)
    , m_nameEdit(nullptr)
    , m_authorEdit(nullptr)
    , m_languageCombo(nullptr)
    , m_locationEdit(nullptr)
    , m_browseBtn(nullptr)
    , m_subfolderCheck(nullptr)
    , m_authorLabel(nullptr)
    , m_languageLabel(nullptr)
    , m_locationLabel(nullptr)
    , m_folderIcon(nullptr)
    , m_folderLabel(nullptr)
{
    // The heading: the title of the window and what it makes
    if (m_mode == NewItemMode::Project) {
        setHeading(tr("New Book"),
                   //: The heading of the New Book window. In Polish: Wybierz typ książki, a
                   //: potem tytuł, autora, język i folder książki.
                   tr("Choose a book type, then the title, author, language and folder of the "
                      "book."));
        setHeadingIcon(QStringLiteral("file.new.project"));
        //: The gear of the New Book window: Settings > Files > Folders, with the folder of the
        //: books. In Polish: Ustawienia folderów
        addSettingsButton(QString::fromLatin1(kalahari::gui::SettingsDialog::FOLDERS_PAGE),
                          tr("Folder Settings"));
    } else {
        setHeading(tr("New File"));
        setHeadingIcon(QStringLiteral("file.new"));
    }
    // A low heading leaves the room to the templates
    setCompactHeading(true);

    // As wide as the description and two templates in a row; the height of the content can
    // be lower: on a small screen (1366x768 at 150%) the content scrolls under the buttons
    setMinimumWidth(MINIMUM_WIDTH);

    // Initialize result structure
    m_result.mode = m_mode;
    m_result.createSubfolder = true;

    setupUI();
    createConnections();
    loadDefaults();
    populateTemplates();

    // Select first template if available
    if (m_templateList->count() > 0) {
        m_templateList->setCurrentRow(0);
    }

    validateInput();

    // The size of a big screen, higher by the heading, or less on a smaller one
    kalahari::gui::utils::resizeWithinScreen(this, withHeading(DEFAULT_SIZE));
}

// ============================================================================
// Public Methods
// ============================================================================

NewItemResult NewItemDialog::result() const {
    return m_result;
}

void NewItemDialog::setOpeningQuestion(NewBookQuestion question) {
    m_openingQuestion = std::move(question);
}

bool NewItemDialog::isUserProject(const QString& templateId) {
    return templateId == QLatin1String(USER_PROJECT);
}

// ============================================================================
// UI Setup
// ============================================================================

void NewItemDialog::setupUI() {
    // The templates and the details, which scroll in a window lower than they are; the
    // buttons stay under them
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(11);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create splitter for left/right panels
    QSplitter* splitter = new QSplitter(Qt::Horizontal, content);
    splitter->setChildrenCollapsible(false);

    // Add description panel (LEFT) and template grid (RIGHT)
    splitter->addWidget(createDescriptionPanel());
    splitter->addWidget(createTemplateGrid());

    // Set initial sizes: 250px for description, rest for templates
    splitter->setSizes({250, 500});

    layout->addWidget(splitter, 1);

    // Add details group (BOTTOM)
    layout->addWidget(createDetailsGroup(), 0);
    contentLayout()->addWidget(content, 1);

    // Dialog buttons
    setAcceptText((m_mode == NewItemMode::Project) ? tr("Create Book") : tr("Create"));
    acceptButton()->setEnabled(false);
}

QWidget* NewItemDialog::createDescriptionPanel() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setSpacing(11);
    layout->setContentsMargins(11, 11, 11, 11);

    // Fixed width for description panel
    panel->setFixedWidth(250);

    // Large icon (64x64)
    m_iconLabel = new QLabel(panel);
    m_iconLabel->setFixedSize(64, 64);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(true);
    layout->addWidget(m_iconLabel, 0, Qt::AlignHCenter);

    // Template name (bold)
    m_titleLabel = new QLabel(panel);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    // Separator line
    QFrame* separator = new QFrame(panel);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    // Description text (rich text); it scrolls when the panel is too low for the features
    m_descriptionLabel = new QLabel(panel);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_descriptionLabel->setTextFormat(Qt::RichText);
    m_descriptionLabel->setObjectName(QStringLiteral("newBookDescription"));
    // The link that deletes the writer's own type, with the mouse or with Tab and Enter
    m_descriptionLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse |
                                                Qt::LinksAccessibleByKeyboard);
    connect(m_descriptionLabel, &QLabel::linkActivated, this, [this](const QString& link) {
        if (link == QLatin1String(DELETE_TYPE_LINK)) {
            removeChosenCustomType();
        }
    });
    QScrollArea* descriptionScroll = new QScrollArea(panel);
    descriptionScroll->setFrameShape(QFrame::NoFrame);
    descriptionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    descriptionScroll->setWidgetResizable(true);
    descriptionScroll->setWidget(m_descriptionLabel);
    layout->addWidget(descriptionScroll, 1);

    return panel;
}

QWidget* NewItemDialog::createTemplateGrid() {
    QWidget* panel = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setSpacing(6);
    layout->setContentsMargins(0, 0, 0, 0);

    // Group box
    //: The group of the list of the New Book window: the book types. In Polish: Typy książek
    QGroupBox* group = new QGroupBox(m_mode == NewItemMode::Project ? tr("Book Types")
                                                                    : tr("Templates"),
                                     panel);
    QVBoxLayout* groupLayout = new QVBoxLayout(group);

    // Search filter (for future use)
    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setPlaceholderText(tr("Search templates..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->hide(); // Hidden for now, enable when many templates available
    groupLayout->addWidget(m_searchEdit);

    // Template list with icon mode
    m_templateList = new QListWidget(group);
    m_templateList->setViewMode(QListView::IconMode);
    m_templateList->setIconSize(QSize(48, 48));
    m_templateList->setSpacing(10);
    m_templateList->setResizeMode(QListView::Adjust);
    m_templateList->setMovement(QListView::Static);
    m_templateList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateList->setFlow(QListView::LeftToRight);
    m_templateList->setWrapping(true);
    m_templateList->setGridSize(QSize(110, 96));
    m_templateList->setWordWrap(true);  // Translated names may need two lines
    groupLayout->addWidget(m_templateList, 1);

    layout->addWidget(group);
    return panel;
}

QWidget* NewItemDialog::createDetailsGroup() {
    QGroupBox* group = new QGroupBox(tr("Details"), this);
    QGridLayout* layout = new QGridLayout(group);
    layout->setSpacing(6);
    layout->setContentsMargins(11, 11, 11, 11);

    int row = 0;

    // Name/Title
    QString nameLabel = (m_mode == NewItemMode::Project) ? tr("Book Title:") : tr("File Name:");
    QLabel* nameLbl = new QLabel(nameLabel, group);
    layout->addWidget(nameLbl, row, 0);

    m_nameEdit = new QLineEdit(group);
    m_nameEdit->setPlaceholderText((m_mode == NewItemMode::Project)
        ? tr("Enter book title...")
        : tr("Enter file name..."));
    m_nameEdit->setToolTip((m_mode == NewItemMode::Project)
        ? tr("The title of your new book")
        : tr("The name of the new file"));
    layout->addWidget(m_nameEdit, row, 1, 1, 2);
    row++;

    // Project-only fields
    if (m_mode == NewItemMode::Project) {
        // Author
        m_authorLabel = new QLabel(tr("Author:"), group);
        layout->addWidget(m_authorLabel, row, 0);

        m_authorEdit = new QLineEdit(group);
        m_authorEdit->setPlaceholderText(tr("Enter author name..."));
        m_authorEdit->setToolTip(tr("The author name for the book metadata"));
        layout->addWidget(m_authorEdit, row, 1, 1, 2);
        row++;

        // Language
        m_languageLabel = new QLabel(tr("Language:"), group);
        layout->addWidget(m_languageLabel, row, 0);

        m_languageCombo = new QComboBox(group);
        m_languageCombo->setToolTip(tr("Primary language for the book content"));
        // Add common languages
        m_languageCombo->addItem(tr("English"), "en");
        m_languageCombo->addItem(tr("Polish"), "pl");
        m_languageCombo->addItem(tr("German"), "de");
        m_languageCombo->addItem(tr("French"), "fr");
        m_languageCombo->addItem(tr("Spanish"), "es");
        m_languageCombo->addItem(tr("Italian"), "it");
        m_languageCombo->addItem(tr("Portuguese"), "pt");
        m_languageCombo->addItem(tr("Russian"), "ru");
        m_languageCombo->addItem(tr("Chinese"), "zh");
        m_languageCombo->addItem(tr("Japanese"), "ja");
        layout->addWidget(m_languageCombo, row, 1, 1, 2);
        row++;

        // Sections: the names of the three sections of the book, in its language, or none
        QLabel* sectionsLabel = new QLabel(tr("Sections:"), group);
        layout->addWidget(sectionsLabel, row, 0);

        m_sectionsCombo = new SectionsComboBox(group);
        m_sectionsCombo->setObjectName(QStringLiteral("newBookSectionsCombo"));
        sectionsLabel->setBuddy(m_sectionsCombo);
        layout->addWidget(m_sectionsCombo, row, 1, 1, 2);
        row++;

        // The writer's own names of the sections
        m_sectionNamesLabel = new QLabel(tr("Section names:"), group);
        layout->addWidget(m_sectionNamesLabel, row, 0);

        m_sectionNames = new SectionNamesEdit(Qt::Horizontal, group);
        m_sectionNames->setObjectName(QStringLiteral("newBookSectionNames"));
        m_sectionNamesLabel->setBuddy(m_sectionNames->firstField());
        layout->addWidget(m_sectionNames, row, 1, 1, 2);
        m_sectionNamesLabel->hide();
        m_sectionNames->hide();
        row++;

        // Location
        m_locationLabel = new QLabel(tr("Location:"), group);
        layout->addWidget(m_locationLabel, row, 0);

        m_locationEdit = new QLineEdit(group);
        m_locationEdit->setPlaceholderText(tr("Select book folder..."));
        m_locationEdit->setToolTip(tr("The folder where the book will be created"));
        layout->addWidget(m_locationEdit, row, 1);

        m_browseBtn = new QPushButton(tr("Browse..."), group);
        m_browseBtn->setToolTip(tr("Choose the folder in the system window"));
        layout->addWidget(m_browseBtn, row, 2);
        row++;

        // Create subfolder checkbox
        m_subfolderCheck = new QCheckBox(tr("Create subfolder with book name"), group);
        m_subfolderCheck->setChecked(true);
        m_subfolderCheck->setToolTip(tr("When checked, creates a new folder named after the book inside the selected location"));
        layout->addWidget(m_subfolderCheck, row, 1, 1, 2);
        row++;

        // The folder of the new book, or why the book cannot be made there
        QWidget* folderLine = new QWidget(group);
        QHBoxLayout* folderLayout = new QHBoxLayout(folderLine);
        folderLayout->setContentsMargins(0, 0, 0, 0);
        m_folderIcon = new QLabel(folderLine);
        m_folderIcon->setFixedSize(16, 16);
        folderLayout->addWidget(m_folderIcon, 0, Qt::AlignTop);
        m_folderLabel = new QLabel(folderLine);
        m_folderLabel->setObjectName(QStringLiteral("newBookFolderLabel"));
        m_folderLabel->setTextFormat(Qt::PlainText);
        m_folderLabel->setWordWrap(true);
        folderLayout->addWidget(m_folderLabel, 1);
        layout->addWidget(folderLine, row, 1, 1, 2);
    }

    // Make columns stretch properly
    layout->setColumnStretch(1, 1);

    return group;
}

void NewItemDialog::createConnections() {
    // Template selection
    connect(m_templateList, &QListWidget::currentItemChanged,
            this, &NewItemDialog::onTemplateSelected);

    // Name change
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, &NewItemDialog::onTitleChanged);

    // Project mode connections
    if (m_mode == NewItemMode::Project) {
        connect(m_browseBtn, &QPushButton::clicked,
                this, &NewItemDialog::onBrowseLocation);
        connect(m_locationEdit, &QLineEdit::textChanged,
                this, [this](const QString&) { validateInput(); });
        connect(m_subfolderCheck, &QCheckBox::toggled,
                this, [this](bool) { validateInput(); });

        // The names of the sets are in the language of the book
        connect(m_languageCombo, &QComboBox::currentIndexChanged,
                this, [this](int) { populateSections(); });
        // The description names the kinds in the language of the book, and the sections as
        // the Sections field names them
        const auto describeAgain = [this]() {
            if (const QListWidgetItem* current = m_templateList->currentItem()) {
                updateDescription(current->data(Qt::UserRole).toString());
            }
        };
        connect(m_languageCombo, &QComboBox::currentIndexChanged, this, describeAgain);
        connect(m_sectionsCombo, &QComboBox::currentIndexChanged, this, describeAgain);
        connect(m_sectionNames, &SectionNamesEdit::edited, this, describeAgain);
        connect(m_sectionsCombo, &QComboBox::currentIndexChanged,
                this, [this](int) { onSectionsChosen(); });
        connect(m_sectionNames, &SectionNamesEdit::edited,
                this, [this]() { m_sectionNamesTyped = true; });
    }

    // The settings that the gear opened
    connect(this, &KalahariDialog::settingsClosed, this, &NewItemDialog::onSettingsClosed);

    // Theme changes
    connect(&kalahari::core::ArtProvider::getInstance(),
            &kalahari::core::ArtProvider::resourcesChanged,
            this, &NewItemDialog::onThemeChanged);
}

void NewItemDialog::populateTemplates() {
    const QString chosen =
        m_templateList->currentItem()
            ? m_templateList->currentItem()->data(Qt::UserRole).toString()
            : QString();
    {
        const QSignalBlocker blocker(m_templateList);
        m_templateList->clear();

        if (m_mode == NewItemMode::Project) {
            // The book types of the installed packages: the program's in its order, the
            // others by name. A type the program does not have is not offered at all.
            const kalahari::core::BookTypeRegistry& installed =
                kalahari::core::ProjectManager::getInstance().installedBookTypes();
            const QString language = kalahari::gui::programLanguage();
            QList<const kalahari::core::BookTypePackage*> types = installed.bookTypes();
            const auto rank = [](const kalahari::core::BookTypePackage* type) {
                const auto* found = std::find_if(
                    std::begin(TYPE_ORDER), std::end(TYPE_ORDER),
                    [type](const char* id) { return type->id == QLatin1String(id); });
                return static_cast<int>(found - std::begin(TYPE_ORDER));
            };
            std::stable_sort(types.begin(), types.end(),
                             [&rank, &language](const kalahari::core::BookTypePackage* one,
                                                const kalahari::core::BookTypePackage* other) {
                                 if (rank(one) != rank(other)) {
                                     return rank(one) < rank(other);
                                 }
                                 return QString::localeAwareCompare(one->name.text(language),
                                                                    other->name.text(language)) <
                                        0;
                             });
            for (const kalahari::core::BookTypePackage* type : types) {
                addTemplate(type->id, type->name.text(language),
                            type->description.text(language), type->icon);
            }

            // The writer's own types, by name; one of a type the program does not have is
            // not offered
            m_customTypes = kalahari::core::CustomBookTypes::load();
            for (const kalahari::core::CustomBookType& custom : std::as_const(m_customTypes)) {
                if (!custom.isUserProject() && !installed.package(custom.typeId)) {
                    continue;
                }
                addTemplate(QLatin1String(CUSTOM_PREFIX) + custom.id, custom.name,
                            custom.description, QString::fromLatin1(CUSTOM_TYPE_ICON));
            }

            //: The last item of the New Book window: a book without a type. In Polish: Projekt
            //: użytkownika
            addTemplate(QString::fromLatin1(USER_PROJECT), tr("User Project"),
                        //: The tooltip of the user project in the New Book window. In Polish:
                        //: Książka bez typu, z rodzajami, które wybierzesz.
                        tr("A book without a type, with the kinds you choose."),
                        QString::fromLatin1(USER_PROJECT_ICON));
        } else {
            for (const TemplateInfo& tmpl : TemplateRegistry::getInstance().getFileTemplates()) {
                addTemplate(tmpl.id, tmpl.name, tmpl.description, tmpl.iconId);
            }
        }
    }

    // The template chosen before, if it is still there
    for (int row = 0; row < m_templateList->count(); ++row) {
        if (m_templateList->item(row)->data(Qt::UserRole).toString() == chosen) {
            m_templateList->setCurrentRow(row);
            return;
        }
    }
    if (!chosen.isEmpty() && m_templateList->count() > 0) {
        m_templateList->setCurrentRow(0);
    }
}

void NewItemDialog::addTemplate(const QString& id, const QString& name, const QString& tooltip,
                                const QString& iconId) {
    auto* item = new QListWidgetItem(m_templateList);
    item->setText(name);
    item->setData(Qt::UserRole, id);
    item->setToolTip(tooltip);
    // 48x48 for the grid
    const QIcon icon =
        kalahari::core::ArtProvider::getInstance().getIcon(iconId,
                                                            kalahari::core::IconContext::Dialog);
    if (!icon.isNull()) {
        item->setIcon(icon);
    }
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

const kalahari::core::CustomBookType* NewItemDialog::customTypeOf(
    const QString& templateId) const {
    if (!templateId.startsWith(QLatin1String(CUSTOM_PREFIX))) {
        return nullptr;
    }
    const QString id = templateId.mid(qsizetype(qstrlen(CUSTOM_PREFIX)));
    for (const kalahari::core::CustomBookType& custom : m_customTypes) {
        if (custom.id == id) {
            return &custom;
        }
    }
    return nullptr;
}

QString NewItemDialog::typeIdOf(const QString& templateId) const {
    if (isUserProject(templateId)) {
        return QString();
    }
    if (templateId.startsWith(QLatin1String(CUSTOM_PREFIX))) {
        const kalahari::core::CustomBookType* custom = customTypeOf(templateId);
        return custom ? custom->typeId : QString();
    }
    return templateId;
}

QString NewItemDialog::placeName(kalahari::core::BookPlace place) const {
    if (place == kalahari::core::BookPlace::Workshop) {
        //: The Workshop of a book in the description of a book type. In Polish: Warsztat
        return tr("Workshop");
    }
    const kalahari::core::BookSections sections = chosenSections();
    if (!sections.shown) {
        return SectionWords::groupName(place);
    }
    kalahari::core::ProjectBook book;
    book.language = m_languageCombo->currentData().toString();
    book.setSections(sections);
    return book.sectionName(place);
}

QString NewItemDialog::describe(const QString& templateId) const {
    using kalahari::core::BookPlace;
    using kalahari::core::KindRef;
    const kalahari::core::BookTypeRegistry& installed =
        kalahari::core::ProjectManager::getInstance().installedBookTypes();
    const QString programLanguage = kalahari::gui::programLanguage();
    const QString language = m_languageCombo->currentData().toString();
    const QString grey = palette().color(QPalette::PlaceholderText).name();
    const auto paragraph = [](const QString& text) {
        return QStringLiteral("<p>") + text + QStringLiteral("</p>");
    };
    const auto note = [&grey](const QString& text) {
        return QStringLiteral("<p style=\"color:%1\">%2</p>").arg(grey, text);
    };

    if (isUserProject(templateId)) {
        //: The description of a user project in the New Book window. In Polish: Książka bez
        //: typu. W następnym oknie wybierzesz rodzaje elementów spośród rodzajów wszystkich
        //: typów, a przycisk „Pomiń” utworzy książkę z rodzajami „Podstawy”.
        return paragraph(tr("A book without a type. In the next window you choose its kinds of "
                            "elements from the kinds of all types, and Skip creates the book "
                            "with the kinds of \"Base\".")
                             .toHtmlEscaped()) +
               //: In Polish: Rodzaje elementów zmienisz później we „Właściwościach
               //: projektu”.
               note(tr("You can change the kinds of elements later in Project Properties.")
                        .toHtmlEscaped());
    }

    const kalahari::core::CustomBookType* custom = customTypeOf(templateId);
    const QString typeId = typeIdOf(templateId);
    const kalahari::core::BookTypePackage* type =
        typeId.isEmpty() ? nullptr : installed.package(typeId);
    if (!custom && !type) {
        return QString();
    }

    QString html;
    // The sentence of the type, or the writer's own and what their type is based on
    if (custom) {
        if (!custom->description.trimmed().isEmpty()) {
            html += paragraph(custom->description.toHtmlEscaped());
        }
        if (type) {
            //: %1: the book type a type of the writer's own is based on, e.g. Novel. In
            //: Polish: Własny typ na podstawie typu „%1”.
            html += paragraph(tr("Your own type, based on the type \"%1\".")
                                  .arg(type->name.text(programLanguage))
                                  .toHtmlEscaped());
        } else {
            //: In Polish: Własny typ bez typu książki: projekt użytkownika z wybranymi
            //: rodzajami.
            html += paragraph(tr("Your own type without a book type: a user project with the "
                                 "kinds you chose.")
                                  .toHtmlEscaped());
        }
    } else {
        html += paragraph(type->description.text(programLanguage).toHtmlEscaped());
    }

    // The kinds that a type of the writer's own takes from other types, and all the kinds of
    // one without a type
    const QList<KindRef> customKinds = custom ? custom->kindsIn(installed) : QList<KindRef>{};

    // What the book starts with, titled in the language of the book
    kalahari::core::CustomBookType made;
    if (custom) {
        made = *custom;
    } else {
        made.typeId = type->id;
    }
    const QString titles =
        kalahari::gui::startTitles(made.startElementsIn(installed), language);
    //: What a new book of the type starts with. In Polish: Na start:
    const QString startLabel = tr("To start with:");
    //: A new book that starts without elements. In Polish: pusta książka
    const QString start = titles.isEmpty() ? tr("an empty book") : titles;
    html += paragraph(QStringLiteral("<b>%1</b> %2").arg(startLabel.toHtmlEscaped(),
                                                         start.toHtmlEscaped()));

    // The kinds of each section and of the Workshop: those the program can make, and in a line
    // of their own those it cannot make yet; a kind taken from another type names it
    for (const BookPlace place : DESCRIBED_PLACES) {
        QList<KindRef> kinds = type ? installed.kindsIn(type->id, place) : QList<KindRef>{};
        QList<KindRef> taken;
        for (const KindRef& kind : customKinds) {
            if (kind.offeredIn(place) && !hasKind(kinds, kind)) {
                kinds.append(kind);
                if (type) {
                    taken.append(kind);
                }
            }
        }
        if (kinds.isEmpty()) {
            continue;
        }
        QStringList ready;
        QStringList notYet;
        for (const KindRef& kind : kinds) {
            QString word = kind.kind->wordsIn(language).form(QStringLiteral("singular"));
            if (hasKind(taken, kind)) {
                word += QStringLiteral(" (%1)").arg(kind.package->name.text(programLanguage));
            }
            (kalahari::core::ProjectManager::canAdd(kind) ? ready : notYet) << word;
        }
        QString row = QStringLiteral("<b>%1:</b> %2").arg(
            placeName(place).toHtmlEscaped(),
            ready.isEmpty() ? QStringLiteral("—")
                            : ready.join(QStringLiteral(", ")).toHtmlEscaped());
        if (!notYet.isEmpty()) {
            //: The kinds of a section that the program cannot make yet, under those it can.
            //: In Polish: jeszcze niedostępne: %1
            row += QStringLiteral("<br/><i style=\"color:%1\">%2</i>")
                       .arg(grey, tr("not available yet: %1")
                                      .arg(notYet.join(QStringLiteral(", ")))
                                      .toHtmlEscaped());
        }
        html += paragraph(row);
    }

    if (type) {
        //: In Polish: Typu książki nie można później zmienić. Rodzaje innych typów dodasz we
        //: „Właściwościach projektu”.
        html += note(tr("The type of a book cannot be changed later. You can take the kinds of "
                        "other types in Project Properties.")
                         .toHtmlEscaped());
    } else {
        html += note(tr("You can change the kinds of elements later in Project Properties.")
                         .toHtmlEscaped());
    }
    if (custom) {
        //: The link under the description of a type of the writer's own. In Polish: Usuń ten
        //: typ...
        html += QStringLiteral("<p><a href=\"%1\">%2</a></p>")
                    .arg(QLatin1String(DELETE_TYPE_LINK),
                         tr("Delete This Type...").toHtmlEscaped());
    }
    return html;
}

void NewItemDialog::removeChosenCustomType() {
    const QListWidgetItem* current = m_templateList->currentItem();
    const kalahari::core::CustomBookType* custom =
        current ? customTypeOf(current->data(Qt::UserRole).toString()) : nullptr;
    if (!custom) {
        return;
    }
    const QString name = custom->name;
    //: The title of the question and of the message about deleting a type of the writer's
    //: own. In Polish: Usuń typ
    const QString title = tr("Delete Type");
    if (!MessageDialog::confirm(this, title,
                                //: %1: the name of a type of the writer's own. In Polish: Usunąć
                                //: typ „%1”? Książki utworzone z tego typu zostają bez zmian.
                                tr("Delete the type \"%1\"? The books created of it stay as "
                                   "they are.")
                                    .arg(name),
                                //: In Polish: &Usuń
                                tr("&Delete"), MessageDialog::Kind::Warning)) {
        return;
    }
    if (!kalahari::core::CustomBookTypes::remove(*custom)) {
        MessageDialog::error(this, title,
                             //: In Polish: Nie udało się usunąć typu „%1”. Szczegóły są w
                             //: dzienniku programu.
                             tr("The type \"%1\" could not be deleted. The program's log has "
                                "the details.")
                                 .arg(name));
        return;
    }
    // The list without it; the first template is chosen
    {
        const QSignalBlocker blocker(m_templateList);
        m_templateList->setCurrentRow(-1);
    }
    populateTemplates();
    if (m_templateList->count() > 0) {
        m_templateList->setCurrentRow(0);
    }
}

void NewItemDialog::updateDescription(const QString& templateId) {
    if (templateId.isEmpty()) {
        m_iconLabel->clear();
        m_titleLabel->clear();
        m_descriptionLabel->clear();
        return;
    }

    QString iconId;
    QString name;
    QString html;
    if (m_mode == NewItemMode::Project) {
        // The type's package, the writer's own type or the user project
        const kalahari::core::CustomBookType* custom = customTypeOf(templateId);
        const kalahari::core::BookTypePackage* type =
            custom || isUserProject(templateId)
                ? nullptr
                : kalahari::core::ProjectManager::getInstance().installedBookTypes().package(
                      templateId);
        if (custom) {
            iconId = QString::fromLatin1(CUSTOM_TYPE_ICON);
            name = custom->name;
        } else if (type) {
            iconId = type->icon;
            name = type->name.text(kalahari::gui::programLanguage());
        } else if (isUserProject(templateId)) {
            iconId = QString::fromLatin1(USER_PROJECT_ICON);
            name = tr("User Project");
        } else {
            return;
        }
        html = describe(templateId);
    } else {
        const TemplateInfo info = TemplateRegistry::getInstance().getTemplate(templateId);
        if (!info.isValid()) {
            return;
        }
        iconId = info.iconId;
        name = info.name;
        html += "<p>" +
                info.description.toHtmlEscaped().replace("\n\n", "</p><p>").replace("\n", "<br/>") +
                "</p>";
        if (!info.features.isEmpty()) {
            html += "<p><b>" + tr("Features:") + "</b></p><ul>";
            for (const QString& feature : info.features) {
                html += "<li>" + feature.toHtmlEscaped() + "</li>";
            }
            html += "</ul>";
        }
    }

    // Update icon (64x64) using getThemedIcon for auto-refresh support
    QIcon icon = kalahari::core::ArtProvider::getInstance().getThemedIcon(iconId);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(64, 64));
    } else {
        m_iconLabel->clear();
    }
    m_titleLabel->setText(name);
    m_descriptionLabel->setText(html);
}

void NewItemDialog::validateInput() {
    bool valid = true;

    // Name is always required
    if (m_nameEdit->text().trimmed().isEmpty()) {
        valid = false;
    }

    // For project mode, location is also required, and the book's folder must be new or empty
    if (m_mode == NewItemMode::Project) {
        const QString location = m_locationEdit->text().trimmed();
        if (location.isEmpty()) {
            valid = false;
        }

        const bool subfolder = m_subfolderCheck->isChecked();
        const QString folder =
            location.isEmpty() ? QString()
                               : kalahari::core::ProjectManager::newProjectFolder(
                                     location, m_nameEdit->text(), subfolder);
        QString folderName = QFileInfo(folder).fileName();
        if (folderName.isEmpty()) {
            folderName = QDir::toNativeSeparators(folder);  // a drive or the root
        }
        QString warningIcon;
        if (folder.isEmpty()) {
            m_folderLabel->clear();
        } else if (kalahari::core::ProjectManager::canHoldNewProject(folder)) {
            m_folderLabel->setText(tr("The book will be created in the folder '%1'.")
                                       .arg(folderName));
        } else {
            valid = false;
            warningIcon = QStringLiteral("common.warning");
            m_folderLabel->setText(
                subfolder ? tr("The folder '%1' is already in this location and is not empty. "
                               "Change the title or the location.").arg(folderName)
                          : tr("The folder '%1' is not empty. Choose another folder or create "
                               "a subfolder with the book name.").arg(folderName));
        }
        m_folderIcon->setPixmap(
            warningIcon.isEmpty()
                ? QPixmap()
                : kalahari::core::ArtProvider::getInstance().getIcon(warningIcon).pixmap(16, 16));
    }

    // Template must be selected
    if (!m_templateList->currentItem()) {
        valid = false;
    }

    acceptButton()->setEnabled(valid);
}

void NewItemDialog::loadDefaults() {
    auto& settings = kalahari::core::SettingsManager::getInstance();

    if (m_mode == NewItemMode::Project) {
        // Load default author
        std::string defaultAuthor = settings.get<std::string>("project.defaultAuthor");
        if (!defaultAuthor.empty()) {
            m_authorEdit->setText(QString::fromStdString(defaultAuthor));
        }

        // The language of new books from the settings, or else the language of the program,
        // so that the names of the sections are in it too; English when the list does not
        // have it
        QString language =
            QString::fromStdString(settings.get<std::string>("project.defaultLanguage"));
        if (m_languageCombo->findData(language) < 0) {
            language = kalahari::gui::programLanguage();
        }
        const int languageIndex = m_languageCombo->findData(language);
        if (languageIndex >= 0) {
            m_languageCombo->setCurrentIndex(languageIndex);
        }

        // The folder of the last new book, at first the folder of the books from the settings
        m_suggestedLocation = QDir::toNativeSeparators(
            ProgramFolders::startFolder(ProgramFolders::Operation::NewBook));
        m_locationEdit->setText(m_suggestedLocation);

        // Sections: the writer's last choice for a template that shows them, none for the
        // others
        populateSections();
        m_sectionsShown = QString::fromStdString(settings.get<std::string>(SECTIONS_SETTING));
        if (m_sectionsCombo->findData(m_sectionsShown) < 0) {
            m_sectionsShown = kalahari::core::ProjectBook::sectionNameSets().first().id;
        }
        m_sectionsHidden = QString::fromLatin1(SectionsComboBox::NO_SECTIONS);
        m_namesSet = SectionsComboBox::isNameSet(m_sectionsShown)
                         ? m_sectionsShown
                         : kalahari::core::ProjectBook::sectionNameSets().first().id;
        if (m_sectionsShown == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
            QStringList names;
            for (const std::string& name :
                 settings.get<std::vector<std::string>>(SECTION_NAMES_SETTING)) {
                names << QString::fromStdString(name);
            }
            m_sectionNames->setNames(names);
            m_sectionNamesTyped = true;
        }
    }
}

void NewItemDialog::populateSections() {
    if (!m_sectionsCombo) {
        return;
    }
    const QString language = m_languageCombo->currentData().toString();
    m_sectionsCombo->setLanguage(language);
    m_sectionNames->setLanguage(language);
    fillSectionNames();
}

bool NewItemDialog::showsSections(const QString& templateId) const {
    const QString typeId = typeIdOf(templateId);
    if (typeId.isEmpty()) {
        return false;  // a user project, or a file template
    }
    const kalahari::core::BookTypePackage* type =
        kalahari::core::ProjectManager::getInstance().installedBookTypes().package(typeId);
    return type != nullptr && type->partsLayer;
}

void NewItemDialog::showSectionsChoice() {
    if (!m_sectionsCombo) {
        return;
    }
    const QListWidgetItem* current = m_templateList->currentItem();
    const bool shown = current && showsSections(current->data(Qt::UserRole).toString());
    {
        const QSignalBlocker blocker(m_sectionsCombo);
        m_sectionsCombo->choose(shown ? m_sectionsShown : m_sectionsHidden);
    }
    onSectionsChosen();
}

void NewItemDialog::onSectionsChosen() {
    const QString chosen = m_sectionsCombo->choice();
    const QListWidgetItem* current = m_templateList->currentItem();
    if (current && showsSections(current->data(Qt::UserRole).toString())) {
        m_sectionsShown = chosen;
    } else {
        m_sectionsHidden = chosen;
    }
    if (SectionsComboBox::isNameSet(chosen)) {
        m_namesSet = chosen;
    }

    const bool custom = chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS);
    m_sectionNamesLabel->setVisible(custom);
    m_sectionNames->setVisible(custom);
    fillSectionNames();
}

void NewItemDialog::fillSectionNames() {
    if (m_sectionNamesTyped || !m_sectionsCombo) {
        return;
    }
    const QString language = m_languageCombo->currentData().toString();
    const kalahari::core::SectionNameSet& set = sectionNameSet(m_namesSet);
    QStringList names;
    for (kalahari::core::BookPlace place : SECTION_PLACES) {
        names << set.name(place, language);
    }
    m_sectionNames->setNames(names);
}

kalahari::core::BookSections NewItemDialog::chosenSections() const {
    kalahari::core::BookSections sections;
    const QString chosen = m_sectionsCombo->choice();
    if (chosen == QLatin1String(SectionsComboBox::NO_SECTIONS)) {
        sections.shown = false;  // with the names of the first set, if the writer shows them
        return sections;
    }
    sections.set = chosen;
    if (chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
        sections.names = m_sectionNames->names();
    }
    return sections;
}

void NewItemDialog::saveSectionsChoice() const {
    const QListWidgetItem* current = m_templateList->currentItem();
    if (!current || !showsSections(current->data(Qt::UserRole).toString())) {
        return;  // the others start without sections
    }
    auto& settings = kalahari::core::SettingsManager::getInstance();
    const QString chosen = m_sectionsCombo->choice();
    settings.set<std::string>(SECTIONS_SETTING, chosen.toStdString());
    if (chosen == QLatin1String(kalahari::core::ProjectBook::CUSTOM_SECTIONS)) {
        std::vector<std::string> names;
        for (const QString& name : m_sectionNames->names()) {
            names.push_back(name.toStdString());
        }
        settings.set<std::vector<std::string>>(SECTION_NAMES_SETTING, names);
    }
}

// ============================================================================
// Slots
// ============================================================================

void NewItemDialog::onTemplateSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    Q_UNUSED(previous)

    if (!current) {
        updateDescription(QString());
        validateInput();
        return;
    }

    QString templateId = current->data(Qt::UserRole).toString();
    updateDescription(templateId);
    showSectionsChoice();
    updateCreateButton();
    validateInput();
}

void NewItemDialog::updateCreateButton() {
    if (m_mode != NewItemMode::Project) {
        return;
    }
    const QListWidgetItem* current = m_templateList->currentItem();
    if (current && isUserProject(current->data(Qt::UserRole).toString())) {
        //: The button of the New Book window for a user project, whose kinds the writer
        //: chooses in the next window. In Polish: Dalej
        setAcceptText(tr("Next"));
    } else {
        setAcceptText(tr("Create Book"));
    }
}

void NewItemDialog::onBrowseLocation() {
    // The folder in the field; the folder the field started with is created at its first use,
    // and a typed folder that does not exist yet opens the window in the nearest one that does
    const QString location = m_locationEdit->text().trimmed();
    QString start = ProgramFolders::samePath(
                        location, ProgramFolders::startFolder(ProgramFolders::Operation::NewBook))
                        ? ProgramFolders::windowFolder(ProgramFolders::Operation::NewBook)
                        : ProgramFolders::existingFolderAt(location);
    if (start.isEmpty()) {
        start = ProgramFolders::windowFolder(ProgramFolders::Operation::NewBook);
    }

    QString folder = QFileDialog::getExistingDirectory(
        this,
        tr("Select Book Location"),
        start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!folder.isEmpty()) {
        m_locationEdit->setText(QDir::toNativeSeparators(folder));
    }
}

void NewItemDialog::onTitleChanged(const QString& text) {
    Q_UNUSED(text)
    validateInput();
}

void NewItemDialog::accept() {
    // Populate result structure
    m_result.mode = m_mode;
    m_result.title = m_nameEdit->text().trimmed();

    // Get selected template
    QListWidgetItem* current = m_templateList->currentItem();
    if (current) {
        m_result.templateId = current->data(Qt::UserRole).toString();
    }

    // Project-specific fields
    if (m_mode == NewItemMode::Project) {
        m_result.typeId = typeIdOf(m_result.templateId);
        if (const kalahari::core::CustomBookType* custom = customTypeOf(m_result.templateId)) {
            m_result.customType = *custom;
        } else {
            m_result.customType.reset();
        }
        m_result.author = m_authorEdit->text().trimmed();
        m_result.language = m_languageCombo->currentData().toString();
        m_result.location = m_locationEdit->text().trimmed();
        m_result.createSubfolder = m_subfolderCheck->isChecked();
        m_result.sections = chosenSections();

        // A book of a type is created now: when a book is open, the writer says whether the
        // new one takes its place, and Cancel returns to this window. A user project asks in
        // its next window.
        m_result.open = true;
        if (m_openingQuestion && !isUserProject(m_result.templateId)) {
            switch (m_openingQuestion(this, m_result.title)) {
                case NewBookOpening::Open:
                    break;
                case NewBookOpening::DontOpen:
                    m_result.open = false;
                    break;
                case NewBookOpening::Cancel:
                    return;
            }
        }
        saveSectionsChoice();
    }

    KalahariDialog::accept();
}

void NewItemDialog::showEvent(QShowEvent* event) {
    KalahariDialog::showEvent(event);
    // Back from the window of the kinds of a user project: the types the writer saved there
    // are on the list too
    if (m_mode == NewItemMode::Project && m_shown && !event->spontaneous()) {
        populateTemplates();
    }
    m_shown = true;
}

void NewItemDialog::onSettingsClosed() {
    if (m_mode != NewItemMode::Project) {
        return;
    }
    // A new folder of the books takes the place of the one the field suggested, unless the
    // writer typed another
    const QString suggested = QDir::toNativeSeparators(
        ProgramFolders::startFolder(ProgramFolders::Operation::NewBook));
    if (m_locationEdit->text().trimmed() == m_suggestedLocation &&
        suggested != m_suggestedLocation) {
        m_locationEdit->setText(suggested);
    }
    m_suggestedLocation = suggested;
}

void NewItemDialog::onThemeChanged() {
    // Refresh template icons
    populateTemplates();

    // Refresh the icon of the folder line
    validateInput();

    // Refresh description icon
    QListWidgetItem* current = m_templateList->currentItem();
    if (current) {
        QString templateId = current->data(Qt::UserRole).toString();
        updateDescription(templateId);
    }
}
