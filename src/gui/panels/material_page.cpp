/// @file material_page.cpp
/// @brief The short card of a material in the Properties panel, and the materials of an
/// element of the book

#include "kalahari/gui/panels/material_page.h"

#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"

#include <QAbstractItemDelegate>
#include <QClipboard>
#include <QGridLayout>
#include <QGuiApplication>
#include <QTextCursor>
#include <QUrl>
#include <QCompleter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace kalahari {
namespace gui {

namespace {

/// The link of a row of the links, and the material of a row of the materials
constexpr int LINK_ROLE = Qt::UserRole;

/// Rows of the fields and of the links shown at least and at most; the others scroll
constexpr int MINIMUM_ROWS = 2;
constexpr int MAXIMUM_ROWS = 8;

/// Size of the pictures of the materials of an element
constexpr int MATERIAL_ICON = 40;

/// Size the picture of the card is read at, before it is fitted to the panel
const QSize THUMBNAIL_BOUNDS(960, 2 * MaterialPage::MAXIMUM_THUMBNAIL_HEIGHT);

/// Give @p button icon @p iconId, also after a change of the theme or of the icons
template <typename Button>
void keepIcon(Button* button, const QString& iconId, core::IconContext context) {
    auto& art = core::ArtProvider::getInstance();
    const auto update = [button, iconId, context]() {
        button->setIcon(core::ArtProvider::getInstance().getIcon(iconId, context));
    };
    update();
    QObject::connect(&art, &core::ArtProvider::resourcesChanged, button, update);
}

/// A small button of a list: "+" or "-"
QToolButton* listButton(QWidget* parent, const char* iconId, const QString& tooltip,
                        const char* name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(name));
    button->setAutoRaise(true);
    keepIcon(button, QString::fromLatin1(iconId), core::IconContext::Menu);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    return button;
}

/// A row of "+" and "-" at the right, over a list
QHBoxLayout* buttonRow(QToolButton* add, QToolButton* remove) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(2);
    row->addStretch();
    row->addWidget(add);
    row->addWidget(remove);
    return row;
}

/// A grey sentence of a list, which wraps
QLabel* hintLabel(QWidget* parent, const QString& text, const char* name) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// Height of @p rows rows of @p view, between MINIMUM_ROWS and MAXIMUM_ROWS, with the header
int listHeight(const QAbstractItemView* view, int rows, int rowHeight, int header) {
    const int shown = std::clamp(rows, MINIMUM_ROWS, MAXIMUM_ROWS);
    const int frame = view->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    return shown * rowHeight + header + 2 * frame + 2;
}

/// Whether @p event is a key of a list that its own handler takes: Insert or Delete without
/// modifiers
bool isListKey(const QKeyEvent* key, Qt::Key which) {
    return key->key() == which &&
           (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
}

/// Whether @p key is Enter without modifiers, which opens the current row of a list also on
/// macOS, where a list edits it
bool isOpenKey(const QKeyEvent* key) {
    return isListKey(key, Qt::Key_Return) || isListKey(key, Qt::Key_Enter);
}

/// The names of the fields: the names of the fields of the book suggested as they are typed
class FieldDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        auto* line = qobject_cast<QLineEdit*>(editor);
        if (line != nullptr && index.column() == 0) {
            auto* completer =
                new QCompleter(core::ProjectManager::getInstance().fieldNames(), line);
            completer->setCaseSensitivity(Qt::CaseInsensitive);
            completer->setFilterMode(Qt::MatchStartsWith);
            line->setCompleter(completer);
        }
        return editor;
    }
};

/// The links of a material: a node of the Navigator dropped on it is linked
class LinkList : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    /// Called with the links of the nodes dropped
    std::function<void(const QStringList&)> onDrop;

    /// Whether drops are taken: not in a book open read-only
    bool takesDrops = true;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        accept(event, droppedLinks(event).isEmpty());
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        accept(event, droppedLinks(event).isEmpty());
    }

    void dropEvent(QDropEvent* event) override {
        const QStringList links = droppedLinks(event);
        if (links.isEmpty()) {
            event->ignore();
            return;
        }
        accept(event, false);
        if (onDrop) {
            onDrop(links);
        }
    }

private:
    /// Copied where the drag lets it be, else moved: the Navigator moves only its own nodes,
    /// so that it keeps them either way
    static void accept(QDropEvent* event, bool none) {
        if (none) {
            event->ignore();
            return;
        }
        event->setDropAction(event->possibleActions().testFlag(Qt::CopyAction) ? Qt::CopyAction
                                                                               : Qt::MoveAction);
        event->accept();
    }

    /// The links of the nodes of the Navigator dragged over the list
    QStringList droppedLinks(const QDropEvent* event) const {
        if (!takesDrops) {
            return {};
        }
        return NavigatorPanel::linksOf(event->mimeData());
    }
};

/// Nothing of the children of @p widget reaches it any more. QWidget's destructor deletes them
/// after the class of @p widget is gone, and a field with the keys finishes its editing then.
void cutOffChildren(QWidget* widget) {
    for (QObject* child : widget->findChildren<QObject*>()) {
        child->disconnect(widget);
    }
}

} // namespace

// =============================================================================
// MaterialPage
// =============================================================================

MaterialPage::MaterialPage(QWidget* parent, bool full) : QWidget(parent), m_full(full) {
    setObjectName(full ? QStringLiteral("materialFullCard") : QStringLiteral("materialPage"));

    // The picture or the source, the file and the ways of opening it; the title, the file and
    // the address
    m_infoGroup = new QGroupBox(this);
    auto* infoLayout = new QVBoxLayout(m_infoGroup);
    infoLayout->setContentsMargins(11, 11, 11, 11);
    infoLayout->setSpacing(6);
    m_thumbnail = new QToolButton(m_infoGroup);
    m_thumbnail->setObjectName(QStringLiteral("materialThumbnail"));
    m_thumbnail->setAutoRaise(true);
    m_thumbnail->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_thumbnail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_thumbnail->installEventFilter(this);
    connect(m_thumbnail, &QToolButton::clicked, this, [this]() {
        if (m_picture) {
            emit requestPreview(m_elementId);
        }
    });
    infoLayout->addWidget(m_thumbnail);
    m_thumbnailProblem = hintLabel(m_infoGroup, QString(), "materialThumbnailProblem");
    m_thumbnailProblem->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(m_thumbnailProblem);
    m_fileLabel = new QLabel(m_infoGroup);
    m_fileLabel->setObjectName(QStringLiteral("materialFileLabel"));
    m_fileLabel->setWordWrap(true);
    m_fileLabel->setAlignment(Qt::AlignCenter);
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(m_fileLabel);

    auto* openRow = new QHBoxLayout();
    //: The button that opens the preview of a material in the program. In Polish: Podgląd
    m_previewButton = new QPushButton(tr("Preview"), m_infoGroup);
    m_previewButton->setObjectName(QStringLiteral("materialPreviewButton"));
    keepIcon(m_previewButton, QStringLiteral("common.preview"), core::IconContext::Button);
    //: The button that opens a material in the program the system has for its file. In
    //: Polish: Program systemowy
    m_systemButton = new QPushButton(tr("System Program"), m_infoGroup);
    m_systemButton->setObjectName(QStringLiteral("materialSystemButton"));
    keepIcon(m_systemButton, QStringLiteral("common.openExternal"), core::IconContext::Button);
    //: In Polish: Otwórz w programie systemowym
    m_systemButton->setToolTip(tr("Open in the System Program"));
    //: The button that opens the web address of a material in the system's browser.
    //: In Polish: Otwórz w przeglądarce
    m_browserButton = new QPushButton(tr("Open in Browser"), m_infoGroup);
    m_browserButton->setObjectName(QStringLiteral("materialBrowserButton"));
    keepIcon(m_browserButton, QStringLiteral("material.page"), core::IconContext::Button);
    //: In Polish: Kopiuj adres
    m_copyAddressButton = new QPushButton(tr("Copy Address"), m_infoGroup);
    m_copyAddressButton->setObjectName(QStringLiteral("materialCopyAddressButton"));
    keepIcon(m_copyAddressButton, QStringLiteral("edit.copy"), core::IconContext::Button);
    //: The button that downloads the picture of a material's web address and saves its copy
    //: in the book. In Polish: Zapisz kopię
    m_saveCopyButton = new QPushButton(tr("Save Copy"), m_infoGroup);
    m_saveCopyButton->setObjectName(QStringLiteral("materialSaveCopyButton"));
    keepIcon(m_saveCopyButton, QStringLiteral("common.download"), core::IconContext::Button);
    //: In Polish: Pobierz obraz z sieci i zapisz jego kopię w folderze „workshop” książki
    m_saveCopyButton->setToolTip(tr("Download the picture from the web and save its copy in "
                                    "the book's folder \"workshop\""));
    openRow->addWidget(m_saveCopyButton);
    openRow->addWidget(m_previewButton);
    openRow->addWidget(m_systemButton);
    openRow->addWidget(m_browserButton);
    openRow->addWidget(m_copyAddressButton);
    infoLayout->addLayout(openRow);
    connect(m_previewButton, &QPushButton::clicked, this,
            [this]() { emit requestPreview(m_elementId); });
    connect(m_systemButton, &QPushButton::clicked, this,
            [this]() { emit requestOpenWithSystem(m_elementId); });
    connect(m_browserButton, &QPushButton::clicked, this,
            [this]() { emit requestOpenInBrowser(m_elementId); });
    connect(m_copyAddressButton, &QPushButton::clicked, this, &MaterialPage::copyAddress);
    connect(m_saveCopyButton, &QPushButton::clicked, this,
            [this]() { emit requestSaveCopy(m_elementId); });

    m_form = new QFormLayout();
    m_form->setSpacing(6);
    m_titleEdit = new QLineEdit(m_infoGroup);
    m_titleEdit->setObjectName(QStringLiteral("materialTitleEdit"));
    m_form->addRow(tr("Title:"), m_titleEdit);
    m_pathLabel = new QLabel(m_infoGroup);
    m_pathLabel->setObjectName(QStringLiteral("materialPathLabel"));
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    //: The file of a material, in the folder of the book. In Polish: Plik:
    m_form->addRow(tr("File:"), m_pathLabel);
    m_addressLabel = new QLabel(m_infoGroup);
    m_addressLabel->setObjectName(QStringLiteral("materialAddressLabel"));
    m_addressLabel->setTextFormat(Qt::PlainText);
    m_addressLabel->setWordWrap(true);
    m_addressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                            Qt::TextSelectableByKeyboard);
    // A long address without spaces wraps anywhere, not past the card
    m_addressLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    //: The web address of a material. In Polish: Adres:
    m_form->addRow(tr("Address:"), m_addressLabel);
    infoLayout->addLayout(m_form);
    connect(m_titleEdit, &QLineEdit::editingFinished, this, &MaterialPage::saveTitle);

    // The description, saved when the field loses the keys
    //: The description of a material in the Properties panel. In Polish: Opis
    auto* descriptionGroup = new QGroupBox(tr("Description"), this);
    auto* descriptionLayout = new QVBoxLayout(descriptionGroup);
    descriptionLayout->setContentsMargins(11, 11, 11, 11);
    m_descriptionEdit = new QPlainTextEdit(descriptionGroup);
    m_descriptionEdit->setObjectName(QStringLiteral("materialDescriptionEdit"));
    //: In Polish: Opis materiału
    m_descriptionEdit->setAccessibleName(tr("Description of the material"));
    //: In Polish: Wpisz opis...
    m_descriptionEdit->setPlaceholderText(tr("Enter a description..."));
    m_descriptionEdit->setTabChangesFocus(true);
    // On the full card the description is the place to write: a quotation, notes of a talk
    m_descriptionEdit->setMinimumHeight(full ? 180 : 80);
    if (!full) {
        m_descriptionEdit->setMaximumHeight(200);
    }
    m_descriptionEdit->installEventFilter(this);
    descriptionLayout->addWidget(m_descriptionEdit);

    // The writer's own fields
    //: The writer's own fields of a material: a name and a value each. In Polish: Własne pola
    auto* fieldsGroup = new QGroupBox(tr("Own Fields"), this);
    auto* fieldsLayout = new QVBoxLayout(fieldsGroup);
    fieldsLayout->setContentsMargins(11, 6, 11, 11);
    fieldsLayout->setSpacing(4);
    //: In Polish: Dodaj pole (Insert)
    m_addFieldButton = listButton(fieldsGroup, "common.add", tr("Add Field (Insert)"),
                                  "materialAddField");
    //: In Polish: Usuń pole (Delete)
    m_removeFieldButton = listButton(fieldsGroup, "common.remove", tr("Delete Field (Delete)"),
                                     "materialRemoveField");
    fieldsLayout->addLayout(buttonRow(m_addFieldButton, m_removeFieldButton));
    m_fieldsTable = new QTableWidget(0, 2, fieldsGroup);
    m_fieldsTable->setObjectName(QStringLiteral("materialFieldsTable"));
    //: The columns of the own fields of a material. In Polish: Nazwa
    m_fieldsTable->setHorizontalHeaderLabels({tr("Name"),
                                              //: In Polish: Wartość
                                              tr("Value")});
    //: In Polish: Własne pola materiału
    m_fieldsTable->setAccessibleName(tr("Own fields of the material"));
    m_fieldsTable->verticalHeader()->hide();
    m_fieldsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_fieldsTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_fieldsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fieldsTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                   QAbstractItemView::EditKeyPressed |
                                   QAbstractItemView::AnyKeyPressed);
    m_fieldsTable->setItemDelegate(new FieldDelegate(m_fieldsTable));
    m_fieldsTable->setWordWrap(false);
    m_fieldsTable->installEventFilter(this);
    fieldsLayout->addWidget(m_fieldsTable);
    //: In Polish: Brak pól. „+” albo Insert dodaje pole.
    m_noFieldsLabel = hintLabel(fieldsGroup, tr("No fields. \"+\" or Insert adds a field."),
                                "materialNoFields");
    fieldsLayout->addWidget(m_noFieldsLabel);
    if (full) {
        fieldsLayout->addStretch(1);
    }
    connect(m_addFieldButton, &QToolButton::clicked, this, &MaterialPage::addField);
    connect(m_removeFieldButton, &QToolButton::clicked, this, &MaterialPage::removeField);
    connect(m_fieldsTable, &QTableWidget::itemChanged, this, [this]() {
        if (!m_updating) {
            saveFields();
        }
    });
    connect(m_fieldsTable, &QTableWidget::currentCellChanged, this, &MaterialPage::updateButtons);
    // A new field left without a name and a value goes
    connect(m_fieldsTable->itemDelegate(), &QAbstractItemDelegate::closeEditor, this,
            [this](QWidget*, QAbstractItemDelegate::EndEditHint hint) {
                if (hint == QAbstractItemDelegate::EditNextItem ||
                    hint == QAbstractItemDelegate::EditPreviousItem) {
                    return;
                }
                const int row = m_fieldsTable->currentRow();
                if (row < 0 || row >= m_fieldsTable->rowCount()) {
                    return;
                }
                const QTableWidgetItem* name = m_fieldsTable->item(row, 0);
                const QTableWidgetItem* value = m_fieldsTable->item(row, 1);
                if ((name == nullptr || name->text().trimmed().isEmpty()) &&
                    (value == nullptr || value->text().isEmpty())) {
                    m_fieldsTable->removeRow(row);
                    if (row < m_fields.size()) {
                        m_fields.removeAt(row);
                    }
                    saveFields();
                }
            });

    // The links: what the material is about
    //: The elements and sections a material is about. In Polish: Powiązania
    auto* linksGroup = new QGroupBox(tr("Links"), this);
    auto* linksLayout = new QVBoxLayout(linksGroup);
    linksLayout->setContentsMargins(11, 6, 11, 11);
    linksLayout->setSpacing(4);
    //: In Polish: Powiąż... (Insert)
    m_addLinkButton = listButton(linksGroup, "common.add", tr("Link... (Insert)"),
                                 "materialAddLink");
    //: In Polish: Usuń powiązanie (Delete)
    m_removeLinkButton = listButton(linksGroup, "common.remove", tr("Remove Link (Delete)"),
                                    "materialRemoveLink");
    linksLayout->addLayout(buttonRow(m_addLinkButton, m_removeLinkButton));
    auto* links = new LinkList(linksGroup);
    links->onDrop = [this](const QStringList& dropped) { addLinks(dropped); };
    m_linksList = links;
    m_linksList->setObjectName(QStringLiteral("materialLinksList"));
    //: In Polish: Powiązania materiału
    m_linksList->setAccessibleName(tr("Links of the material"));
    m_linksList->setColumnCount(2);
    m_linksList->setHeaderHidden(true);
    m_linksList->setRootIsDecorated(false);
    m_linksList->setUniformRowHeights(true);
    m_linksList->header()->setStretchLastSection(false);
    m_linksList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_linksList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_linksList->setAcceptDrops(true);
    m_linksList->setDragDropMode(QAbstractItemView::DropOnly);
    m_linksList->setDropIndicatorShown(false);
    m_linksList->installEventFilter(this);
    linksLayout->addWidget(m_linksList);
    //: In Polish: Brak powiązań. Przeciągnij tu węzeł z Nawigatora albo kliknij „+”.
    m_noLinksLabel = hintLabel(linksGroup,
                               tr("No links. Drag a node of the Navigator here or click "
                                  "\"+\"."),
                               "materialNoLinks");
    linksLayout->addWidget(m_noLinksLabel);
    connect(m_addLinkButton, &QToolButton::clicked, this, &MaterialPage::editLinks);
    connect(m_removeLinkButton, &QToolButton::clicked, this, &MaterialPage::removeLink);
    connect(m_linksList, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        emit requestOpenLink(item->data(0, LINK_ROLE).toString());
    });
    connect(m_linksList, &QTreeWidget::currentItemChanged, this, &MaterialPage::updateButtons);

    if (full) {
        // The description beside the own fields, the links under them
        auto* grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(11);
        grid->addWidget(m_infoGroup, 0, 0, 1, 2);
        grid->addWidget(descriptionGroup, 1, 0);
        grid->addWidget(fieldsGroup, 1, 1);
        grid->addWidget(linksGroup, 2, 0, 1, 2);
        grid->setColumnStretch(0, 3);
        grid->setColumnStretch(1, 2);
        grid->setRowStretch(3, 1);
    } else {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(11);
        layout->addWidget(m_infoGroup);
        layout->addWidget(descriptionGroup);
        layout->addWidget(fieldsGroup);
        layout->addWidget(linksGroup);
        layout->addStretch(1);
    }

    // The icons of the kinds, of a file that is no picture and of the links
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &MaterialPage::refresh);
    // The links changed elsewhere, or their paragraphs moved when a chapter was saved
    connect(&core::ProjectManager::getInstance(), &core::ProjectManager::linksChanged, this,
            [this]() {
                if (const core::ProjectElement* element =
                        core::ProjectManager::getInstance().findElement(m_elementId)) {
                    fillLinks(*element);
                }
            });
    // The paragraphs of the links shown or not (LinkRoles::PARAGRAPHS_SETTING): the setting
    // may change on another thread, the list changes on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [page = QPointer<MaterialPage>(this)](const std::string& key) {
            if (key != core::LinkRoles::PARAGRAPHS_SETTING || page == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                page,
                [page]() {
                    if (page == nullptr) {
                        return;
                    }
                    if (const core::ProjectElement* element =
                            core::ProjectManager::getInstance().findElement(page->m_elementId)) {
                        page->fillLinks(*element);
                    }
                },
                Qt::QueuedConnection);
        });
}

MaterialPage::~MaterialPage() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    cutOffChildren(this);
}

void MaterialPage::showMaterial(const QString& elementId) {
    m_elementId = elementId;
    refresh();
}

void MaterialPage::refresh() {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    m_updating = true;
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};
    //: In Polish: Informacje o {kind:locative}
    m_infoGroup->setTitle(core::fillWords(tr("{Kind} Information"), nouns));

    showSource(*element);

    if (!m_titleEdit->hasFocus()) {
        m_titleEdit->setText(element->title);
    }
    if (!m_descriptionEdit->hasFocus()) {
        m_descriptionEdit->setPlainText(element->description);
    }
    fillFields(*element);
    fillLinks(*element);

    const bool readOnly = pm.isReadOnly();
    m_titleEdit->setReadOnly(readOnly);
    m_descriptionEdit->setReadOnly(readOnly);
    m_fieldsTable->setEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                                            : QAbstractItemView::DoubleClicked |
                                                  QAbstractItemView::EditKeyPressed |
                                                  QAbstractItemView::AnyKeyPressed);
    static_cast<LinkList*>(m_linksList)->takesDrops = !readOnly;
    m_updating = false;
    updateButtons();
}

void MaterialPage::showSource(const core::ProjectElement& element) {
    const auto& pm = core::ProjectManager::getInstance();
    m_source = materialSourceOf(element);
    m_url = element.url;
    m_path = pm.filePathOf(element);
    m_rotation = element.rotation;
    const bool hasFile = !element.file.isEmpty();
    MaterialFile file = MaterialFile::of(m_path);
    m_picture = hasFile && file.readable;
    const QString readError = updateThumbnail();
    if (file.readable && !m_picture) {
        file.unreadable(readError);
    }

    // The sentence under the picture or the icon, and the line under it
    QString problem;
    QString line;
    switch (m_source) {
    case MaterialSource::File:
        problem = file.problem();
        line = file.summary();
        break;
    case MaterialSource::WebPicture:
        if (hasFile) {
            problem = file.problem();
            //: The line under the picture of a material with a copy of a picture on the web:
            //: %1 its name, format and size. In Polish: %1 · kopia z sieci
            line = tr("%1 · copy from the web").arg(file.summary());
        } else {
            //: In Polish: Program pokaże obraz, gdy w książce będzie jego kopia.
            problem = tr("The program shows the picture once the book has its copy.");
            line = QUrl(m_url).host();
        }
        break;
    case MaterialSource::Page:
        line = QUrl(m_url).host();
        break;
    case MaterialSource::None:
        //: In Polish: Bez pliku i adresu. Skąd pochodzi, mówią opis i własne pola.
        problem = tr("No file and no address. Its description and own fields tell where it "
                     "comes from.");
        break;
    }
    m_thumbnailProblem->setText(problem);
    m_thumbnailProblem->setVisible(!problem.isEmpty());
    m_fileLabel->setText(line);
    m_fileLabel->setVisible(!line.isEmpty());

    // The ways of opening it
    const bool fromFile = m_source == MaterialSource::File;
    const bool fromWeb = m_source == MaterialSource::Page || m_source == MaterialSource::WebPicture;
    m_previewButton->setVisible(fromFile || (m_source == MaterialSource::WebPicture && hasFile));
    m_previewButton->setEnabled(m_picture);
    //: The tooltip of Preview for a file that is no picture. In Polish: Program pokazuje
    //: tylko obrazy
    const QString onlyPictures = tr("The program shows only pictures");
    //: In Polish: Otwórz podgląd
    m_previewButton->setToolTip(m_picture           ? tr("Open the Preview")
                                : problem.isEmpty() ? onlyPictures
                                                    : problem);
    m_systemButton->setVisible(fromFile);
    m_systemButton->setEnabled(file.exists);
    m_browserButton->setVisible(fromWeb);
    //: The tooltip of Open in Browser: %1 the address. In Polish: Otwórz %1 w przeglądarce
    m_browserButton->setToolTip(tr("Open %1 in the browser").arg(m_url));
    m_copyAddressButton->setVisible(m_source == MaterialSource::Page);
    m_saveCopyButton->setVisible(m_source == MaterialSource::WebPicture && !hasFile &&
                                 MaterialDownloader::policy() !=
                                     MaterialDownloader::Policy::Never);
    const bool downloading = m_downloading.contains(m_elementId);
    m_saveCopyButton->setEnabled(!pm.isReadOnly() && !downloading);
    //: Save Copy while the picture is being downloaded. In Polish: Pobieranie...
    m_saveCopyButton->setText(downloading ? tr("Downloading...") : tr("Save Copy"));

    // The file is the copy of a picture on the web; the address of a page or of a picture
    m_pathLabel->setText(element.file);
    m_form->setRowVisible(m_pathLabel, hasFile);
    if (auto* caption = qobject_cast<QLabel*>(m_form->labelForField(m_pathLabel))) {
        //: The copy of a picture on the web in the book's folder. In Polish: Kopia:
        const QString copy = tr("Copy:");
        caption->setText(m_source == MaterialSource::WebPicture ? copy : tr("File:"));
    }
    m_addressLabel->setText(m_url);
    m_addressLabel->setToolTip(m_url);
    m_form->setRowVisible(m_addressLabel, fromWeb);
}

void MaterialPage::setDownloading(const QString& elementId, bool downloading) {
    if (m_downloading.contains(elementId) == downloading) {
        return;
    }
    if (downloading) {
        m_downloading.insert(elementId);
    } else {
        m_downloading.remove(elementId);
    }
    if (elementId == m_elementId) {
        refresh();
    }
}

void MaterialPage::copyAddress() {
    if (m_url.isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(m_url);
    //: %1: the address. In Polish: Skopiowano adres: %1
    emit message(tr("Address copied: %1").arg(m_url));
}

void MaterialPage::focusFirstField() {
    m_titleEdit->setFocus(Qt::OtherFocusReason);
    m_titleEdit->selectAll();
}

void MaterialPage::focusDescription() {
    m_descriptionEdit->setFocus(Qt::OtherFocusReason);
    m_descriptionEdit->moveCursor(QTextCursor::End);
}

QString MaterialPage::updateThumbnail() {
    const int width = std::max(1, m_thumbnail->width() - 8);
    QPixmap shown;
    QString readError;
    if (m_picture) {
        // The file is read again only for another file, turn or change of it, not when the
        // panel changes its width
        const QString key = QStringLiteral("%1|%2|%3")
                                .arg(m_path)
                                .arg(m_rotation)
                                .arg(QFileInfo(m_path).lastModified().toMSecsSinceEpoch());
        if (key != m_thumbnailKey) {
            m_thumbnailError.clear();
            m_thumbnailPicture =
                readPicture(m_path, m_rotation, THUMBNAIL_BOUNDS, &m_thumbnailError);
            m_thumbnailKey = key;
        }
        const QImage& picture = m_thumbnailPicture;
        if (picture.isNull()) {
            m_picture = false;
            readError = m_thumbnailError;
        } else {
            const QSize fitted = picture.size().scaled(
                QSize(width, m_full ? MAXIMUM_FULL_THUMBNAIL_HEIGHT : MAXIMUM_THUMBNAIL_HEIGHT),
                Qt::KeepAspectRatio);
            const QSize size = picture.width() <= fitted.width() &&
                                       picture.height() <= fitted.height()
                                   ? picture.size()
                                   : fitted;
            shown = QPixmap::fromImage(
                picture.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
    if (shown.isNull()) {
        // A file that is no picture, one the program cannot show, or no file: the icon of its
        // kind or of its source
        const int iconSize =
            core::ArtProvider::getInstance().getIconSize(core::IconContext::Dialog);
        shown = linkIcon(m_elementId, core::IconContext::Dialog).pixmap(iconSize * 2);
    }
    m_thumbnail->setIcon(QIcon(shown));
    m_thumbnail->setIconSize(shown.size());
    m_thumbnail->setFixedHeight(shown.height() + 8);
    m_thumbnail->setCursor(m_picture ? Qt::PointingHandCursor : Qt::ArrowCursor);
    //: The picture of a material in the Properties panel. In Polish: Otwórz podgląd
    m_thumbnail->setToolTip(m_picture ? tr("Open the Preview") : QString());
    m_thumbnail->setEnabled(true);
    m_thumbnail->setProperty("thumbnailWidth", m_thumbnail->width());
    return readError;
}

void MaterialPage::fillFields(const core::ProjectElement& element) {
    m_fields = element.fields;
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_fieldsTable->setRowCount(0);
    for (const core::ElementField& field : element.fields) {
        const int row = m_fieldsTable->rowCount();
        m_fieldsTable->insertRow(row);
        m_fieldsTable->setItem(row, 0, new QTableWidgetItem(field.name));
        m_fieldsTable->setItem(row, 1, new QTableWidgetItem(field.value));
    }
    const int rowHeight = m_fieldsTable->verticalHeader()->defaultSectionSize();
    const int header = m_fieldsTable->horizontalHeader()->sizeHint().height();
    m_fieldsTable->setFixedHeight(listHeight(
        m_fieldsTable, static_cast<int>(element.fields.size()), rowHeight, header));
    m_fieldsTable->setVisible(!element.fields.isEmpty());
    m_noFieldsLabel->setVisible(element.fields.isEmpty());
    m_updating = wasUpdating;
}

void MaterialPage::fillLinks(const core::ProjectElement& element) {
    const QString current =
        m_linksList->currentItem() ? m_linksList->currentItem()->data(0, LINK_ROLE).toString()
                                   : QString();
    m_linksList->clear();
    const QColor grey = palette().color(QPalette::PlaceholderText);
    const core::BookProject* project = core::ProjectManager::getInstance().project();
    bool details = false;  // Whether a row has the role or the paragraph under its title
    for (const core::ElementLink& link : element.links) {
        // A link that names nothing of the book any more is not shown
        if (project == nullptr || !project->hasLinkTarget(link.to)) {
            continue;
        }
        auto* item = new QTreeWidgetItem(m_linksList);
        const QString title = linkTitle(link.to);
        const QString how = linkDetails(link);
        item->setText(0, how.isEmpty() ? title : title + QLatin1Char('\n') + how);
        item->setIcon(0, linkIcon(link.to, core::IconContext::TreeView));
        item->setText(1, linkPlace(link.to));
        item->setForeground(1, grey);
        item->setData(0, LINK_ROLE, link.to);
        //: The tooltip of a link with a note: %1 the title and the role, %2 the note. In
        //: Polish: %1\nNotatka: %2
        item->setToolTip(0, link.note.isEmpty() ? item->text(0)
                                                : tr("%1\nNote: %2").arg(item->text(0), link.note));
        details = details || !how.isEmpty();
        if (link.to == current) {
            m_linksList->setCurrentItem(item);
        }
    }
    const int count = m_linksList->topLevelItemCount();
    const int lines = details ? 2 : 1;
    const int rowHeight = std::max(lines * m_linksList->fontMetrics().height() + 6,
                                   m_linksList->iconSize().height() + 4);
    m_linksList->setUniformRowHeights(!details);
    m_linksList->setFixedHeight(listHeight(m_linksList, count, std::max(rowHeight, 22), 0));
    m_linksList->setVisible(count > 0);
    m_noLinksLabel->setVisible(count == 0);
}

void MaterialPage::updateButtons() {
    const bool readOnly = core::ProjectManager::getInstance().isReadOnly();
    m_addFieldButton->setEnabled(!readOnly);
    m_removeFieldButton->setEnabled(!readOnly && m_fieldsTable->currentRow() >= 0 &&
                                    m_fieldsTable->rowCount() > 0);
    m_addLinkButton->setEnabled(!readOnly);
    m_removeLinkButton->setEnabled(!readOnly && m_linksList->currentItem() != nullptr);
}

void MaterialPage::saveTitle() {
    if (m_updating || m_elementId.isEmpty()) {
        return;
    }
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr || pm.isReadOnly()) {
        return;
    }
    const QString previous = element->title;
    const QString title = m_titleEdit->text().trimmed();
    if (title.isEmpty() || title == previous) {
        m_titleEdit->setText(previous);
        return;
    }
    if (!pm.renameElement(m_elementId, title)) {
        m_titleEdit->setText(previous);
        dialogs::MessageDialog::warning(this, tr("Rename Failed"), tr("Failed to save changes."));
        return;
    }
    emit titleChanged(m_elementId);
    emit changed(m_elementId);
}

void MaterialPage::saveDescription() {
    if (m_updating || m_elementId.isEmpty()) {
        return;
    }
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr || pm.isReadOnly() ||
        element->description == m_descriptionEdit->toPlainText()) {
        return;
    }
    if (!pm.setDescription(m_elementId, m_descriptionEdit->toPlainText())) {
        //: In Polish: Nie zapisano opisu
        dialogs::MessageDialog::warning(this, tr("Description Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    emit changed(m_elementId);
}

QList<core::ElementField> MaterialPage::tableFields() const {
    QList<core::ElementField> fields;
    for (int row = 0; row < m_fieldsTable->rowCount(); ++row) {
        core::ElementField field = row < m_fields.size() ? m_fields.at(row) : core::ElementField{};
        const QTableWidgetItem* name = m_fieldsTable->item(row, 0);
        const QTableWidgetItem* value = m_fieldsTable->item(row, 1);
        field.name = name ? name->text().trimmed() : QString();
        field.value = value ? value->text() : QString();
        fields << field;
    }
    return fields;
}

void MaterialPage::saveFields() {
    auto& pm = core::ProjectManager::getInstance();
    if (m_elementId.isEmpty() || pm.isReadOnly()) {
        return;
    }
    // A field without a name and a value is not saved
    QList<core::ElementField> fields = tableFields();
    fields.removeIf([](const core::ElementField& field) {
        return field.name.isEmpty() && field.value.isEmpty();
    });
    if (!pm.setFields(m_elementId, fields)) {
        //: In Polish: Nie zapisano pól
        dialogs::MessageDialog::warning(this, tr("Fields Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_fields = tableFields();
    updateButtons();
    emit changed(m_elementId);
}

void MaterialPage::addField() {
    if (m_elementId.isEmpty() || core::ProjectManager::getInstance().isReadOnly()) {
        return;
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    const int row = m_fieldsTable->rowCount();
    m_fieldsTable->insertRow(row);
    m_fieldsTable->setItem(row, 0, new QTableWidgetItem());
    m_fieldsTable->setItem(row, 1, new QTableWidgetItem());
    m_fields.append(core::ElementField{});
    m_updating = wasUpdating;
    const int rowHeight = m_fieldsTable->verticalHeader()->defaultSectionSize();
    const int header = m_fieldsTable->horizontalHeader()->sizeHint().height();
    m_fieldsTable->setFixedHeight(
        listHeight(m_fieldsTable, m_fieldsTable->rowCount(), rowHeight, header));
    m_fieldsTable->show();
    m_noFieldsLabel->hide();
    m_fieldsTable->setFocus(Qt::OtherFocusReason);
    m_fieldsTable->setCurrentCell(row, 0);
    m_fieldsTable->editItem(m_fieldsTable->item(row, 0));
    updateButtons();
}

void MaterialPage::removeField() {
    const int row = m_fieldsTable->currentRow();
    if (row < 0 || row >= m_fieldsTable->rowCount() ||
        core::ProjectManager::getInstance().isReadOnly()) {
        return;
    }
    const QTableWidgetItem* name = m_fieldsTable->item(row, 0);
    const QTableWidgetItem* value = m_fieldsTable->item(row, 1);
    if (value != nullptr && !value->text().isEmpty()) {
        //: %1: the name of the field. In Polish: Usunąć pole „%1” razem z jego wartością?
        const QString question = tr("Delete the field \"%1\" with its value?")
                                     .arg(name != nullptr ? name->text() : QString());
        //: In Polish: Usuń pole
        if (!dialogs::MessageDialog::confirm(this, tr("Delete Field"), question,
                                             //: In Polish: &Usuń
                                             tr("&Delete"))) {
            return;
        }
    }
    const bool wasUpdating = m_updating;
    m_updating = true;
    m_fieldsTable->removeRow(row);
    if (row < m_fields.size()) {
        m_fields.removeAt(row);
    }
    m_updating = wasUpdating;
    saveFields();
    refresh();
    if (m_fieldsTable->rowCount() > 0) {
        m_fieldsTable->setCurrentCell(std::min(row, m_fieldsTable->rowCount() - 1), 0);
        m_fieldsTable->setFocus(Qt::OtherFocusReason);
    }
}

void MaterialPage::editLinks() {
    if (m_elementId.isEmpty() || core::ProjectManager::getInstance().isReadOnly()) {
        return;
    }
    const QTreeWidgetItem* item = m_linksList->currentItem();
    if (const std::optional<QList<core::ElementLink>> links = dialogs::chooseLinks(
            m_elementId, item != nullptr ? item->data(0, LINK_ROLE).toString() : QString(),
            window())) {
        saveLinks(*links);
    }
}

void MaterialPage::addLinks(const QStringList& links) {
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    QList<core::ElementLink> all = element->links;
    for (const QString& link : links) {
        all << pm.newLink(m_elementId, link);
    }
    saveLinks(all);
}

void MaterialPage::removeLink() {
    const QTreeWidgetItem* item = m_linksList->currentItem();
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(m_elementId);
    if (item == nullptr || element == nullptr) {
        return;
    }
    const int index = m_linksList->indexOfTopLevelItem(const_cast<QTreeWidgetItem*>(item));
    const QString target = item->data(0, LINK_ROLE).toString();
    QList<core::ElementLink> links = element->links;
    links.removeIf([&target](const core::ElementLink& link) { return link.to == target; });
    saveLinks(links);
    // The next link is the current one
    if (m_linksList->topLevelItemCount() > 0) {
        m_linksList->setCurrentItem(
            m_linksList->topLevelItem(std::min(index, m_linksList->topLevelItemCount() - 1)));
        m_linksList->setFocus(Qt::OtherFocusReason);
    }
}

void MaterialPage::saveLinks(const QList<core::ElementLink>& links) {
    auto& pm = core::ProjectManager::getInstance();
    if (m_elementId.isEmpty() || pm.isReadOnly()) {
        return;
    }
    if (!pm.setLinks(m_elementId, links)) {
        //: In Polish: Nie zapisano powiązań
        dialogs::MessageDialog::warning(this, tr("Links Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
    emit changed(m_elementId);
}

bool MaterialPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_thumbnail && event->type() == QEvent::Resize &&
        m_thumbnail->property("thumbnailWidth").toInt() != m_thumbnail->width()) {
        updateThumbnail();
    }
    if (watched == m_descriptionEdit && event->type() == QEvent::FocusOut) {
        saveDescription();
    }
    // Insert and Delete of the lists; the same keys of the window's commands wait while a
    // list has the keys. A cell being edited has them itself.
    if ((watched == m_fieldsTable || watched == m_linksList) &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (watched == m_linksList && event->type() == QEvent::KeyPress && isOpenKey(key)) {
            if (const QTreeWidgetItem* item = m_linksList->currentItem()) {
                emit requestOpenLink(item->data(0, LINK_ROLE).toString());
            }
            return true;
        }
        const bool insert = isListKey(key, Qt::Key_Insert);
        const bool remove = isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            if (watched == m_fieldsTable) {
                insert ? addField() : removeField();
            } else {
                insert ? editLinks() : removeLink();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// MaterialCardPanel
// =============================================================================

MaterialCardPanel::MaterialCardPanel(const QString& elementId, QWidget* parent)
    : QScrollArea(parent) {
    setObjectName(QStringLiteral("materialCardPanel"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    // The card in the middle of a wide tab, at most MAXIMUM_WIDTH wide
    auto* content = new QWidget(this);
    auto* row = new QHBoxLayout(content);
    row->setContentsMargins(16, 16, 16, 16);
    m_page = new MaterialPage(content, true);
    m_page->setMaximumWidth(MAXIMUM_WIDTH);
    row->addStretch(1);
    row->addWidget(m_page, 100);
    row->addStretch(1);
    setWidget(content);
    setFocusProxy(m_page->titleEdit());
    m_page->showMaterial(elementId);
    setProperty("elementId", elementId);
    refresh();
}

QString MaterialCardPanel::elementId() const {
    return m_page->elementId();
}

void MaterialCardPanel::refresh() {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId());
    if (element == nullptr) {
        return;
    }
    setProperty("tabIconId", NavigatorPanel::iconIdOf(pm.bookTypes(), *element));
    m_page->refresh();
}

// =============================================================================
// LinkedMaterialsBox
// =============================================================================

LinkedMaterialsBox::LinkedMaterialsBox(QWidget* parent) : QGroupBox(parent) {
    setObjectName(QStringLiteral("linkedMaterials"));
    //: The materials linked with a chapter, in its page of the Properties panel. In Polish:
    //: Materiały
    setTitle(tr("Materials"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(11, 6, 11, 11);
    layout->setSpacing(4);
    //: In Polish: Powiąż materiał... (Insert)
    m_addButton = listButton(this, "common.add", tr("Link Material... (Insert)"),
                             "linkedMaterialsAdd");
    //: In Polish: Usuń powiązanie (Delete)
    m_removeButton = listButton(this, "common.remove", tr("Remove Link (Delete)"),
                                "linkedMaterialsRemove");
    layout->addLayout(buttonRow(m_addButton, m_removeButton));
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("linkedMaterialsList"));
    //: In Polish: Powiązane materiały
    m_list->setAccessibleName(tr("Linked materials"));
    m_list->setIconSize(QSize(MATERIAL_ICON, MATERIAL_ICON));
    m_list->setUniformItemSizes(true);
    m_list->setWordWrap(false);
    //: In Polish: Dwuklik otwiera materiał
    m_list->setToolTip(tr("A double-click opens the material"));
    m_list->installEventFilter(this);
    layout->addWidget(m_list);
    m_emptyLabel = hintLabel(this, QString(), "linkedMaterialsEmpty");
    layout->addWidget(m_emptyLabel);
    connect(m_addButton, &QToolButton::clicked, this, &LinkedMaterialsBox::editMaterials);
    connect(m_removeButton, &QToolButton::clicked, this, &LinkedMaterialsBox::removeMaterial);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        emit requestOpenMaterial(item->data(LINK_ROLE).toString());
    });
    connect(m_list, &QListWidget::currentItemChanged, this, &LinkedMaterialsBox::updateButtons);
    // The icons of the materials that are no pictures
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &LinkedMaterialsBox::refresh);
    // The links changed elsewhere, or their paragraphs moved when a chapter was saved
    connect(&core::ProjectManager::getInstance(), &core::ProjectManager::linksChanged, this,
            [this]() {
                if (!m_link.isEmpty()) {
                    refresh();
                }
            });
    // The paragraphs of the links shown or not (LinkRoles::PARAGRAPHS_SETTING): the setting
    // may change on another thread, the list changes on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [box = QPointer<LinkedMaterialsBox>(this)](const std::string& key) {
            if (key != core::LinkRoles::PARAGRAPHS_SETTING || box == nullptr) {
                return;
            }
            QMetaObject::invokeMethod(
                box,
                [box]() {
                    if (box != nullptr && !box->m_link.isEmpty()) {
                        box->refresh();
                    }
                },
                Qt::QueuedConnection);
        });
}

LinkedMaterialsBox::~LinkedMaterialsBox() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    cutOffChildren(this);
}

void LinkedMaterialsBox::showLinked(const QString& link, const QString& emptyText) {
    m_link = link;
    m_emptyText = emptyText;
    refresh();
}

void LinkedMaterialsBox::refresh() {
    const auto& pm = core::ProjectManager::getInstance();
    const QString current =
        m_list->currentItem() ? m_list->currentItem()->data(LINK_ROLE).toString() : QString();
    m_list->clear();
    for (const core::ProjectElement* material : pm.materialsLinkedTo(m_link)) {
        QImage picture = readPicture(pm.filePathOf(*material), material->rotation,
                                     QSize(MATERIAL_ICON, MATERIAL_ICON));
        const QIcon icon = picture.isNull()
                               ? linkIcon(material->id, core::IconContext::Dialog)
                               : QIcon(QPixmap::fromImage(picture));
        // The title, and under it the role and the paragraph of the link, else the first
        // field: "Scale: 1 : 50 000"
        QString text = material->title;
        const core::ElementLink* link = material->linkTo(m_link);
        const QString how = link != nullptr ? linkDetails(*link) : QString();
        if (!how.isEmpty()) {
            text += QLatin1Char('\n') + how;
        } else if (!material->fields.isEmpty()) {
            const core::ElementField& field = material->fields.first();
            //: The first own field of a material under its title: %1 the name, %2 the
            //: value. In Polish: %1: %2
            text += QLatin1Char('\n') + tr("%1: %2").arg(field.name, field.value);
        }
        auto* item = new QListWidgetItem(icon, text, m_list);
        item->setData(LINK_ROLE, material->id);
        QString tooltip = material->description.isEmpty() ? material->title
                                                           : material->description;
        if (link != nullptr && !link->note.isEmpty()) {
            //: The tooltip of a material with a note of its link: %1 its description, %2 the
            //: note. In Polish: %1\nNotatka: %2
            tooltip = tr("%1\nNote: %2").arg(tooltip, link->note);
        }
        item->setToolTip(tooltip);
        if (material->id == current) {
            m_list->setCurrentItem(item);
        }
    }
    const int count = m_list->count();
    const int rowHeight = std::max(MATERIAL_ICON + 6, 2 * m_list->fontMetrics().height() + 6);
    m_list->setFixedHeight(listHeight(m_list, count, rowHeight, 0));
    m_list->setVisible(count > 0);
    m_emptyLabel->setText(m_emptyText);
    m_emptyLabel->setVisible(count == 0);
    updateButtons();
}

void LinkedMaterialsBox::updateButtons() {
    const bool readOnly = core::ProjectManager::getInstance().isReadOnly();
    m_addButton->setEnabled(!readOnly && !m_link.isEmpty());
    m_removeButton->setEnabled(!readOnly && m_list->currentItem() != nullptr);
}

void LinkedMaterialsBox::editMaterials() {
    auto& pm = core::ProjectManager::getInstance();
    if (m_link.isEmpty() || pm.isReadOnly()) {
        return;
    }
    dialogs::LinkDialog dialog(dialogs::LinkDialog::Mode::Materials, m_link, window());
    if (const QListWidgetItem* item = m_list->currentItem()) {
        dialog.setCurrentLink(item->data(LINK_ROLE).toString());
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // Each material that changes gets the link as it is set in the window, or loses it; the
    // link is saved with it
    const QStringList checked = dialog.checkedLinks();
    const QStringList before = dialog.initialLinks();
    bool saved = true;
    for (const QString& material : checked) {
        const core::ProjectElement* element = pm.findElement(material);
        if (element == nullptr) {
            saved = false;
            continue;
        }
        const core::ElementLink link = dialog.linkOf(material);
        QList<core::ElementLink> links = element->links;
        const auto had =
            std::find_if(links.begin(), links.end(),
                         [this](const core::ElementLink& each) { return each.to == m_link; });
        if (had == links.end()) {
            links << link;
        } else if (*had != link) {
            *had = link;
        } else {
            continue;
        }
        saved = pm.setLinks(material, links) && saved;
    }
    for (const QString& material : before) {
        if (!checked.contains(material)) {
            const core::ProjectElement* element = pm.findElement(material);
            if (element == nullptr) {
                continue;
            }
            QList<core::ElementLink> links = element->links;
            links.removeIf([this](const core::ElementLink& each) { return each.to == m_link; });
            saved = pm.setLinks(material, links) && saved;
        }
    }
    if (!saved) {
        dialogs::MessageDialog::warning(this, tr("Links Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
}

void LinkedMaterialsBox::removeMaterial() {
    auto& pm = core::ProjectManager::getInstance();
    const QListWidgetItem* item = m_list->currentItem();
    if (item == nullptr || pm.isReadOnly()) {
        return;
    }
    const int row = m_list->row(item);
    const QString material = item->data(LINK_ROLE).toString();
    const core::ProjectElement* element = pm.findElement(material);
    if (element == nullptr) {
        return;
    }
    QList<core::ElementLink> links = element->links;
    links.removeIf([this](const core::ElementLink& each) { return each.to == m_link; });
    if (!pm.setLinks(material, links)) {
        dialogs::MessageDialog::warning(this, tr("Links Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
    if (m_list->count() > 0) {
        m_list->setCurrentRow(std::min(row, m_list->count() - 1));
        m_list->setFocus(Qt::OtherFocusReason);
    }
}

bool LinkedMaterialsBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::KeyPress && isOpenKey(key)) {
            if (const QListWidgetItem* item = m_list->currentItem()) {
                emit requestOpenMaterial(item->data(LINK_ROLE).toString());
            }
            return true;
        }
        const bool insert = isListKey(key, Qt::Key_Insert);
        const bool remove = isListKey(key, Qt::Key_Delete);
        if (insert || remove) {
            if (event->type() == QEvent::ShortcutOverride) {
                key->accept();
                return true;
            }
            insert ? editMaterials() : removeMaterial();
            return true;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

} // namespace gui
} // namespace kalahari
