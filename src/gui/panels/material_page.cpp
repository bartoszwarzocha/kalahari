// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file material_page.cpp
/// @brief The short card of a material in the Properties panel, and the materials of an
/// element of the book

#include "kalahari/gui/panels/material_page.h"

#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/source_boxes.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/source_names.h"
#include "kalahari/gui/volume_names.h"
#include "kalahari/gui/widgets/own_fields_box.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/volume_materials.h"

#include <QAbstractItemDelegate>
#include <QAction>
#include <QClipboard>
#include <QComboBox>
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
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace kalahari::gui {

namespace {

/// The link of a row of the links, and the material of a row of the materials
constexpr int LINK_ROLE = Qt::UserRole;

/// Size of the pictures of the materials of an element
constexpr int MATERIAL_ICON = 40;

/// Space between the groups of the card
constexpr int GROUP_SPACING = 11;

/// Size the picture of the card is read at, before it is fitted to the panel
const QSize THUMBNAIL_BOUNDS(960, 2 * MaterialPage::MAXIMUM_THUMBNAIL_HEIGHT);

using card::buttonRow;
using card::cutOffChildren;

/// The titles of the elements @p ids of @p book, in their order
QStringList titlesOf(const core::ProjectManager& book, const QStringList& ids) {
    QStringList titles;
    for (const QString& id : ids) {
        if (const core::ProjectElement* element = book.findElement(id)) {
            titles << element->title;
        }
    }
    return titles;
}

using card::hintLabel;
using card::isListKey;
using card::isOpenKey;
using card::keepIcon;
using card::listButton;
using card::listHeight;

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

/// Show @p box with @p element when @p shown, hide it otherwise; @p box may be null (a group
/// only the full card has)
template <typename Box>
void showBoxIf(Box* box, bool shown, const core::ProjectElement& element) {
    if (box == nullptr) {
        return;
    }
    box->setVisible(shown);
    if (shown) {
        box->showMaterial(element);
    }
}

} // namespace

// =============================================================================
// MaterialPage
// =============================================================================

MaterialPage::MaterialPage(core::ProjectManager& book, QWidget* parent, bool full)
    : QWidget(parent), m_full(full), m_book(book) {
    setObjectName(full ? QStringLiteral("materialFullCard") : QStringLiteral("materialPage"));

    // The picture or the source, the file and the ways of opening it; the title, the file and
    // the address
    m_infoGroup = new QGroupBox(this);
    auto* infoLayout = new QVBoxLayout(m_infoGroup);
    infoLayout->setContentsMargins(11, 11, 11, 11);
    infoLayout->setSpacing(6);
    // A picture, not a button: no frame or background under the mouse or pressed; the keys
    // open the preview with Preview
    m_thumbnail = new QToolButton(m_infoGroup);
    m_thumbnail->setObjectName(QStringLiteral("materialThumbnail"));
    m_thumbnail->setAutoRaise(true);
    m_thumbnail->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_thumbnail->setFocusPolicy(Qt::NoFocus);
    m_thumbnail->setStyleSheet(QStringLiteral(
        "QToolButton#materialThumbnail, QToolButton#materialThumbnail:hover, "
        "QToolButton#materialThumbnail:pressed "
        "{ border: none; background: transparent; padding: 0px; }"));
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

    // A material that can be a source: on the full card its bibliographic description and its
    // entry in the bibliography above its description
    if (full) {
        m_sourceBox = new SourceDescriptionBox(m_book, this);
        m_entryBox = new BibliographyEntryBox(m_book, this);
    }

    // The description, saved when the field loses the keys
    //: The description of a material in the Properties panel. In Polish: Opis
    m_descriptionGroup = new QGroupBox(tr("Description"), this);
    QGroupBox* descriptionGroup = m_descriptionGroup;
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
    descriptionLayout->addWidget(m_descriptionEdit, 1);

    // A text of the cover: its characters and words, its target and the bar of its length
    m_counter = new QWidget(descriptionGroup);
    auto* counterLayout = new QVBoxLayout(m_counter);
    counterLayout->setContentsMargins(0, 0, 0, 0);
    counterLayout->setSpacing(3);
    auto* countsRow = new QHBoxLayout();
    m_countsLabel = new QLabel(m_counter);
    m_countsLabel->setObjectName(QStringLiteral("coverCountsLabel"));
    m_targetLabel = new QLabel(m_counter);
    m_targetLabel->setObjectName(QStringLiteral("coverTargetLabel"));
    m_targetLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    countsRow->addWidget(m_countsLabel, 1);
    countsRow->addWidget(m_targetLabel);
    counterLayout->addLayout(countsRow);
    m_targetBar = new QProgressBar(m_counter);
    m_targetBar->setObjectName(QStringLiteral("coverTargetBar"));
    m_targetBar->setTextVisible(false);
    m_targetBar->setMaximumHeight(std::max(6, fontMetrics().height() / 2));
    //: In Polish: Długość względem celu
    m_targetBar->setAccessibleName(tr("Length against the target"));
    counterLayout->addWidget(m_targetBar);
    if (full) {
        // The own target of the material, and how the cover prints an opinion
        m_targetRow = new QWidget(m_counter);
        auto* targetLayout = new QHBoxLayout(m_targetRow);
        targetLayout->setContentsMargins(0, 3, 0, 0);
        //: The own target length of one text of the cover. In Polish: Docelowa dłu&gość:
        auto* targetCaption = new QLabel(tr("Target le&ngth:"), m_targetRow);
        m_targetSpin = new QSpinBox(m_targetRow);
        m_targetSpin->setObjectName(QStringLiteral("coverTargetSpin"));
        m_targetSpin->setRange(0, 100000);
        m_targetSpin->setKeyboardTracking(false);
        targetCaption->setBuddy(m_targetSpin);
        m_targetUnit = new QLabel(m_targetRow);
        auto* targetNote = hintLabel(
            m_targetRow,
            //: In Polish: Dotyczy tylko tego materiału; domyślną długość rodzaju ustawisz w
            //: Ustawieniach > Nawigator.
            tr("Of this material only; the default of its kind is in Settings > Navigator."),
            "coverTargetNote");
        targetLayout->addWidget(targetCaption);
        targetLayout->addWidget(m_targetSpin);
        targetLayout->addWidget(m_targetUnit);
        targetLayout->addSpacing(8);
        targetLayout->addWidget(targetNote, 1);
        counterLayout->addWidget(m_targetRow);
        m_printedLabel = hintLabel(m_counter, QString(), "coverPrintedLabel");
        m_printedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        counterLayout->addWidget(m_printedLabel);
        connect(m_targetSpin, &QSpinBox::valueChanged, this, &MaterialPage::saveTarget);
    }
    descriptionLayout->addWidget(m_counter);
    m_counter->hide();
    // A note about the author of the cover can start from the note of the book
    m_bioRow = new QWidget(descriptionGroup);
    m_bioRow->setObjectName(QStringLiteral("coverBioRow"));
    auto* bioLayout = new QVBoxLayout(m_bioRow);
    bioLayout->setContentsMargins(0, 3, 0, 0);
    bioLayout->setSpacing(3);
    //: Copies the text of the note About the Author of the back part of the book into a note
    //: about the author of the cover. In Polish: Weź tekst z noty o autorze
    m_bioButton = new QPushButton(tr("Take the Text of About the Author"), m_bioRow);
    m_bioButton->setObjectName(QStringLiteral("coverBioButton"));
    m_bioButton->setIcon(
        core::ArtProvider::getInstance().getIcon("edit.copy", core::IconContext::Button));
    bioLayout->addWidget(m_bioButton, 0, Qt::AlignLeft);
    bioLayout->addWidget(hintLabel(
        m_bioRow,
        //: Under the button that takes the text of the note About the Author. In Polish:
        //: Kopiuje treść „Noty o autorze” z części końcowej; dalej oba teksty zmienia się
        //: osobno.
        tr("Copies the text of \"About the Author\" of the back part; from then on the two "
           "texts are changed apart."),
        "coverBioNote"));
    descriptionLayout->addWidget(m_bioRow);
    m_bioRow->hide();
    connect(m_bioButton, &QPushButton::clicked, this, &MaterialPage::takeAboutAuthor);
    // A note About the Author added or deleted in the Navigator while the card is open
    connect(&m_book, &core::ProjectManager::projectChanged, this, [this]() {
        if (!m_bioRow->isHidden()) {
            updateBioButton();
        }
    });
    connect(m_descriptionEdit, &QPlainTextEdit::textChanged, this, &MaterialPage::updateCounter);

    // A quote: where it comes from, under its words; on the short card the entry of a source
    // in the bibliography of the book
    m_quoteBox = new QuoteSourceBox(m_book, this);
    if (!full) {
        m_entryBox = new BibliographyEntryBox(m_book, this, true);
    }

    // The data of a material of the cover, between its text and its own fields
    createCoverGroup();

    // The bookmarks of a PDF, and on the full card the quotes from a source
    m_bookmarksBox = new BookmarksBox(m_book, this);
    if (full) {
        m_quotesBox = new SourceQuotesBox(m_book, this);
    }
    connectSourceParts();

    // The writer's own fields
    //: In Polish: Własne pola materiału
    m_fieldsBox = new OwnFieldsBox(m_book, QStringLiteral("material"),
                                   tr("Own fields of the material"), full, this);
    connect(m_fieldsBox, &OwnFieldsBox::changed, this, &MaterialPage::changed);

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
        // The description of a source, its entry, the description and where a quote comes
        // from beside the data of the cover, the bookmarks, the quotes and the own fields; the
        // links under them
        auto* grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setSpacing(GROUP_SPACING);
        grid->addWidget(m_infoGroup, 0, 0, 1, 2);
        auto* left = new QVBoxLayout();
        left->setSpacing(GROUP_SPACING);
        left->addWidget(m_sourceBox);
        left->addWidget(m_entryBox);
        left->addWidget(descriptionGroup, 1);
        left->addWidget(m_quoteBox);
        auto* right = new QVBoxLayout();
        right->setSpacing(GROUP_SPACING);
        right->addWidget(m_coverGroup);
        right->addWidget(m_bookmarksBox);
        right->addWidget(m_quotesBox);
        right->addWidget(m_fieldsBox, 1);
        grid->addLayout(left, 1, 0);
        grid->addLayout(right, 1, 1);
        grid->addWidget(linksGroup, 2, 0, 1, 2);
        grid->setColumnStretch(0, 3);
        grid->setColumnStretch(1, 2);
        grid->setRowStretch(1, 1);
        grid->setRowStretch(3, 1);
    } else {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(GROUP_SPACING);
        layout->addWidget(m_infoGroup);
        layout->addWidget(descriptionGroup);
        layout->addWidget(m_quoteBox);
        layout->addWidget(m_entryBox);
        layout->addWidget(m_coverGroup);
        layout->addWidget(m_bookmarksBox);
        layout->addWidget(m_fieldsBox);
        layout->addWidget(linksGroup);
        layout->addStretch(1);
    }

    // The icons of the kinds, of a file that is no picture and of the links
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            &MaterialPage::refresh);
    // The links changed elsewhere, or their paragraphs moved when a chapter was saved
    connect(&m_book, &core::ProjectManager::linksChanged, this,
            [this]() {
                if (const core::ProjectElement* element =
                        m_book.findElement(m_elementId)) {
                    fillLinks(*element);
                }
            });
    // The paragraphs of the links shown or not (LinkRoles::PARAGRAPHS_SETTING): the setting
    // may change on another thread, the list changes on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [page = QPointer<MaterialPage>(this)](const std::string& key) {
            if (page == nullptr) {
                return;
            }
            // The unit and the targets of the texts of the cover
            if (key.rfind("materials.cover.", 0) == 0) {
                QMetaObject::invokeMethod(
                    page,
                    [page]() {
                        if (page == nullptr) {
                            return;
                        }
                        if (const core::ProjectElement* element =
                                page->m_book.findElement(page->m_elementId)) {
                            page->showCover(*element);
                        }
                    },
                    Qt::QueuedConnection);
                return;
            }
            // The style of the bibliography and the form of the references of the Settings
            if (key.starts_with("materials.sources.")) {
                QMetaObject::invokeMethod(
                    page,
                    [page]() {
                        if (page != nullptr) {
                            page->refresh();
                        }
                    },
                    Qt::QueuedConnection);
                return;
            }
            if (key != core::LinkRoles::PARAGRAPHS_SETTING) {
                return;
            }
            QMetaObject::invokeMethod(
                page,
                [page]() {
                    if (page == nullptr) {
                        return;
                    }
                    if (const core::ProjectElement* element =
                            page->m_book.findElement(page->m_elementId)) {
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
    const auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    m_updating = true;
    const QHash<QString, core::KindWords> nouns{
        {QStringLiteral("kind"), wordsOf(m_book, *element)}};
    //: In Polish: Informacje o {kind:locative}
    m_infoGroup->setTitle(core::fillWords(tr("{Kind} Information"), nouns));

    m_cover = CoverNames::isCoverMaterial(m_book, *element);
    showSource(*element);

    if (!m_titleEdit->hasFocus()) {
        m_titleEdit->setText(element->title);
    }
    if (!m_descriptionEdit->hasFocus()) {
        m_descriptionEdit->setPlainText(element->description);
    }
    m_fieldsBox->showFields(m_elementId, element->fields);
    fillLinks(*element);
    showCover(*element);
    showSourceParts(*element);

    const bool readOnly = pm.isReadOnly();
    m_titleEdit->setReadOnly(readOnly);
    m_descriptionEdit->setReadOnly(readOnly);
    static_cast<LinkList*>(m_linksList)->takesDrops = !readOnly;
    m_updating = false;
    updateButtons();
}

void MaterialPage::showSource(const core::ProjectElement& element) {
    const auto& pm = m_book;
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
    // A material of the cover says what it is; its text is written under it
    if (m_cover) {
        if (m_source == MaterialSource::None) {
            problem.clear();
        }
        if (line.isEmpty()) {
            line = CoverNames::partName(element.cover.part);
        }
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
    QString onlyPictures;
    if (file.pdf) {
        //: The tooltip of Preview for a PDF file in a program built without the preview of
        //: PDF files. In Polish: Ta wersja programu nie ma podglądu plików PDF; otwiera je
        //: program systemowy
        onlyPictures = tr("This version of the program has no preview of PDF files; the "
                          "system program opens them");
    } else if (PdfFiles::previewAvailable()) {
        //: The tooltip of Preview for a file that is no picture nor PDF. In Polish: Program
        //: pokazuje tylko obrazy i pliki PDF
        onlyPictures = tr("The program shows only pictures and PDF files");
    } else {
        //: The tooltip of Preview for a file that is no picture. In Polish: Program pokazuje
        //: tylko obrazy
        onlyPictures = tr("The program shows only pictures");
    }
    //: In Polish: Otwórz podgląd
    m_previewButton->setToolTip(m_picture           ? tr("Open the Preview")
                                : problem.isEmpty() ? onlyPictures
                                                    : problem);
    // A text of the book, like a clipping, is a file of the program: no system program opens it
    const bool bookText = pm.formOf(element) == core::ElementForm::Text;
    m_systemButton->setVisible(fromFile && !bookText);
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

void MaterialPage::connectSourceParts() {
    const auto changedHere = [this](const QString& elementId) { emit changed(elementId); };
    const auto messageHere = [this](const QString& text) { emit message(text); };
    if (m_sourceBox != nullptr) {
        connect(m_sourceBox, &SourceDescriptionBox::changed, this, changedHere);
        connect(m_sourceBox, &SourceDescriptionBox::message, this, messageHere);
    }
    connect(m_entryBox, &BibliographyEntryBox::changed, this, changedHere);
    connect(m_entryBox, &BibliographyEntryBox::message, this, messageHere);
    connect(m_entryBox, &BibliographyEntryBox::requestSettings, this,
            &MaterialPage::requestSourceSettings);
    connect(m_bookmarksBox, &BookmarksBox::changed, this, changedHere);
    connect(m_bookmarksBox, &BookmarksBox::requestShowPage, this,
            &MaterialPage::requestShowPage);
    connect(m_quoteBox, &QuoteSourceBox::changed, this, changedHere);
    // The source of a quote opens on its card, or at the page of the quote in its preview
    connect(m_quoteBox, &QuoteSourceBox::requestOpenSource, this,
            [this](const QString& sourceId, const QString& page) {
                if (page.isEmpty()) {
                    emit requestOpenCard(sourceId);
                } else {
                    emit requestShowPage(sourceId, page);
                }
            });
    if (m_quotesBox != nullptr) {
        connect(m_quotesBox, &SourceQuotesBox::requestNewQuote, this,
                &MaterialPage::requestNewQuote);
        connect(m_quotesBox, &SourceQuotesBox::requestOpenQuote, this,
                &MaterialPage::requestOpenCard);
    }
}

void MaterialPage::showSourceParts(const core::ProjectElement& element) {
    // A material that can be a source: its description on the full card; its entry there,
    // and on the short card once it is a source; the quotes from it once it is one or has some
    const bool describable = SourceNames::canDescribe(m_book, element);
    const bool source = describable && !element.source.isEmpty();
    showBoxIf(m_sourceBox, describable, element);
    showBoxIf(m_entryBox, m_full ? describable : source, element);
    showBoxIf(m_quotesBox,
              m_quotesBox != nullptr && (source || !m_book.quotesOf(element.id).isEmpty()),
              element);
    // The bookmarks of a PDF, or of a file that has some
    const bool pdf =
        QFileInfo(element.file).suffix().compare(QLatin1String("pdf"), Qt::CaseInsensitive) == 0;
    showBoxIf(m_bookmarksBox, !element.bookmarks.isEmpty() || (!element.file.isEmpty() && pdf),
              element);
    // A quote: where it comes from, under its words
    showBoxIf(m_quoteBox, SourceNames::isQuote(m_book, element), element);
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
        shown = linkIcon(m_book, m_elementId, core::IconContext::Dialog).pixmap(iconSize * 2);
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

void MaterialPage::fillLinks(const core::ProjectElement& element) {
    const QString current =
        m_linksList->currentItem() ? m_linksList->currentItem()->data(0, LINK_ROLE).toString()
                                   : QString();
    m_linksList->clear();
    const QColor grey = palette().color(QPalette::PlaceholderText);
    const core::BookProject* project = m_book.project();
    bool details = false;  // Whether a row has the role or the paragraph under its title
    for (const core::ElementLink& link : element.links) {
        // A link that names nothing of the book any more is not shown
        if (project == nullptr || !project->hasLinkTarget(link.to)) {
            continue;
        }
        auto* item = new QTreeWidgetItem(m_linksList);
        const QString title = linkTitle(m_book, link.to);
        const QString how = linkDetails(link);
        item->setText(0, how.isEmpty() ? title : title + QLatin1Char('\n') + how);
        item->setIcon(0, linkIcon(m_book, link.to, core::IconContext::TreeView));
        item->setText(1, linkPlace(m_book, link.to));
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
    const bool readOnly = m_book.isReadOnly();
    m_fieldsBox->updateButtons();
    m_addLinkButton->setEnabled(!readOnly);
    m_removeLinkButton->setEnabled(!readOnly && m_linksList->currentItem() != nullptr);
}

void MaterialPage::saveTitle() {
    if (m_updating || m_elementId.isEmpty()) {
        return;
    }
    auto& pm = m_book;
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
    auto& pm = m_book;
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

QTableWidget* MaterialPage::fieldsTable() const {
    return m_fieldsBox->table();
}

QToolButton* MaterialPage::addFieldButton() const {
    return m_fieldsBox->addButton();
}

QToolButton* MaterialPage::removeFieldButton() const {
    return m_fieldsBox->removeButton();
}

void MaterialPage::addField() {
    m_fieldsBox->addField();
}

void MaterialPage::removeField() {
    m_fieldsBox->removeField();
}

void MaterialPage::editLinks() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const QTreeWidgetItem* item = m_linksList->currentItem();
    if (const std::optional<QList<core::ElementLink>> links = dialogs::chooseLinks(
            m_book, m_elementId, item != nullptr ? item->data(0, LINK_ROLE).toString() : QString(),
            window())) {
        saveLinks(*links);
    }
}

void MaterialPage::addLinks(const QStringList& links) {
    auto& pm = m_book;
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
        m_book.findElement(m_elementId);
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
    auto& pm = m_book;
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

QGroupBox* MaterialPage::createCoverGroup() {
    //: The data of a material of the cover: its kind, place and state, and of an opinion its
    //: author and source. In Polish: Dane okładki
    m_coverGroup = new QGroupBox(tr("Cover Data"), this);
    m_coverGroup->setObjectName(QStringLiteral("coverDataGroup"));
    m_coverForm = new QFormLayout(m_coverGroup);
    m_coverForm->setContentsMargins(11, 11, 11, 11);
    m_coverForm->setSpacing(6);
    const auto box = [this](const char* name) {
        auto* combo = new QComboBox(m_coverGroup);
        combo->setObjectName(QString::fromLatin1(name));
        return combo;
    };
    // A field saved when it loses the keys or with Enter
    const auto edit = [this](const char* name, QString core::CoverData::*field) {
        auto* line = new QLineEdit(m_coverGroup);
        line->setObjectName(QString::fromLatin1(name));
        connect(line, &QLineEdit::editingFinished, this, [this, line, field]() {
            const QString text = line->text().trimmed();
            saveCover([&text, field](core::CoverData& cover) { cover.*field = text; });
        });
        return line;
    };
    m_coverVolumeBox = box("coverVolumeBox");
    //: The volume of a series a material of the cover is for, or the whole series. In Polish:
    //: &Tom:
    m_coverForm->addRow(tr("&Volume:"), m_coverVolumeBox);
    m_coverPartBox = box("coverPartBox");
    //: The kind of a material of the cover: a description, a tagline... In Polish: &Rodzaj:
    m_coverForm->addRow(tr("&Kind:"), m_coverPartBox);
    m_coverPlaceBox = box("coverPlaceBox");
    //: Where a material goes on the cover. In Polish: &Miejsce na okładce:
    m_coverForm->addRow(tr("&Place on the cover:"), m_coverPlaceBox);
    m_coverStateBox = box("coverStateBox");
    //: Draft, chosen for the cover or rejected. In Polish: &Stan:
    m_coverForm->addRow(tr("S&tate:"), m_coverStateBox);
    m_coverAuthorEdit = edit("coverAuthorEdit", &core::CoverData::author);
    //: The author of an opinion or of a design. In Polish: &Autor:
    m_coverForm->addRow(tr("&Author:"), m_coverAuthorEdit);
    m_coverRoleEdit = edit("coverRoleEdit", &core::CoverData::role);
    //: Who the author of an opinion is: a critic, a writer. In Polish: &Kim jest:
    m_coverForm->addRow(tr("&Who they are:"), m_coverRoleEdit);
    m_coverSourceEdit = edit("coverSourceEdit", &core::CoverData::source);
    //: Where an opinion was printed or said. In Polish: Źródł&o:
    m_coverForm->addRow(tr("S&ource:"), m_coverSourceEdit);
    m_coverDateEdit = edit("coverDateEdit", &core::CoverData::date);
    //: In Polish: &Data:
    m_coverForm->addRow(tr("&Date:"), m_coverDateEdit);
    m_coverConsentBox = box("coverConsentBox");
    //: Whether the author of an opinion lets it be printed on the cover. In Polish: &Zgoda na
    //: druk:
    m_coverForm->addRow(tr("&Consent to print:"), m_coverConsentBox);
    m_coverLicenseEdit = edit("coverLicenseEdit", &core::CoverData::license);
    //: The license of a reference picture. In Polish: &Licencja:
    m_coverForm->addRow(tr("&License:"), m_coverLicenseEdit);
    m_coverAboutEdit = edit("coverAboutEdit", &core::CoverData::about);
    //: What a reference picture shows for the cover: colors, a typeface. In Polish: Dot&yczy:
    m_coverForm->addRow(tr("A&bout:"), m_coverAboutEdit);

    connect(m_coverPartBox, &QComboBox::activated, this, &MaterialPage::savePart);
    connect(m_coverStateBox, &QComboBox::activated, this, &MaterialPage::saveState);
    connect(m_coverVolumeBox, &QComboBox::activated, this, &MaterialPage::saveVolume);
    connect(m_coverPlaceBox, &QComboBox::activated, this, [this]() {
        const QString place = m_coverPlaceBox->currentData().toString();
        saveCover([&place](core::CoverData& cover) { cover.place = place; });
    });
    connect(m_coverConsentBox, &QComboBox::activated, this, [this]() {
        const QString consent = m_coverConsentBox->currentData().toString();
        saveCover([&consent](core::CoverData& cover) { cover.consent = consent; });
    });
    m_coverGroup->hide();
    return m_coverGroup;
}

void MaterialPage::showCover(const core::ProjectElement& element) {
    using core::CoverMaterial;
    m_cover = CoverNames::isCoverMaterial(m_book, element);
    m_coverData = m_cover ? element.cover : core::CoverData{};
    const QString& part = m_coverData.part;
    const bool text = m_cover && CoverMaterial::isText(part);

    // The description of a text of the cover is its text
    if (text) {
        //: The text of a material of the cover, written in the program. In Polish: Treść
        m_descriptionGroup->setTitle(tr("Text"));
        //: In Polish: Treść materiału okładki
        m_descriptionEdit->setAccessibleName(tr("Text of the cover material"));
        //: In Polish: Wpisz treść...
        m_descriptionEdit->setPlaceholderText(tr("Enter the text..."));
    } else if (SourceNames::isQuote(m_book, element)) {
        // The description of a quote is its words
        //: The words of a quote, in the field of the description of its card. In Polish: Cytat
        m_descriptionGroup->setTitle(tr("Quote"));
        //: In Polish: Słowa cytatu
        m_descriptionEdit->setAccessibleName(tr("Words of the quote"));
        //: In Polish: Wpisz albo wklej słowa cytatu...
        m_descriptionEdit->setPlaceholderText(tr("Type or paste the words of the quote..."));
    } else {
        m_descriptionGroup->setTitle(tr("Description"));
        m_descriptionEdit->setAccessibleName(tr("Description of the material"));
        m_descriptionEdit->setPlaceholderText(tr("Enter a description..."));
    }
    m_coverGroup->setVisible(m_cover);
    if (!m_cover) {
        updateCounter();
        return;
    }

    // The parts among the texts or among the pictures; an unknown part may take any
    const QSignalBlocker partBlocker(m_coverPartBox);
    m_coverPartBox->clear();
    const bool known = CoverMaterial::parts().contains(part);
    for (const QString& each : CoverMaterial::parts()) {
        if (!known || CoverMaterial::canChange(part, each)) {
            m_coverPartBox->addItem(
                core::ArtProvider::getInstance().getIcon(CoverNames::partIconId(each),
                                                         core::IconContext::Menu),
                CoverNames::partName(each), each);
        }
    }
    if (!known) {
        m_coverPartBox->insertItem(0, CoverNames::groupName(part), part);
    }
    m_coverPartBox->setCurrentIndex(m_coverPartBox->findData(part));

    fillPlaces(part, m_coverData.place);

    const QSignalBlocker stateBlocker(m_coverStateBox);
    m_coverStateBox->clear();
    for (const char* state : {"", CoverMaterial::CHOSEN, CoverMaterial::REJECTED}) {
        m_coverStateBox->addItem(CoverNames::stateName(QString::fromLatin1(state)),
                                 QString::fromLatin1(state));
    }
    if (m_coverStateBox->findData(m_coverData.state) < 0) {
        m_coverStateBox->addItem(m_coverData.state, m_coverData.state);
    }
    m_coverStateBox->setCurrentIndex(m_coverStateBox->findData(m_coverData.state));

    const QSignalBlocker consentBlocker(m_coverConsentBox);
    m_coverConsentBox->clear();
    for (const char* consent :
         {"", CoverMaterial::GRANTED, CoverMaterial::ASKED, CoverMaterial::REFUSED}) {
        m_coverConsentBox->addItem(CoverNames::consentName(QString::fromLatin1(consent)),
                                   QString::fromLatin1(consent));
    }
    if (m_coverConsentBox->findData(m_coverData.consent) < 0) {
        m_coverConsentBox->addItem(m_coverData.consent, m_coverData.consent);
    }
    m_coverConsentBox->setCurrentIndex(m_coverConsentBox->findData(m_coverData.consent));

    // The fields keep what is typed in the one with the keys
    const auto show = [](QLineEdit* line, const QString& value) {
        if (!line->hasFocus()) {
            line->setText(value);
        }
    };
    show(m_coverAuthorEdit, m_coverData.author);
    show(m_coverRoleEdit, m_coverData.role);
    show(m_coverSourceEdit, m_coverData.source);
    show(m_coverDateEdit, m_coverData.date);
    show(m_coverLicenseEdit, m_coverData.license);
    show(m_coverAboutEdit, m_coverData.about);

    // The rows of its part
    const bool opinion = part == QLatin1String(CoverMaterial::OPINION);
    const bool design = part == QLatin1String(CoverMaterial::DESIGN);
    const bool reference = part == QLatin1String(CoverMaterial::REFERENCE);
    const bool photo = part == QLatin1String(CoverMaterial::AUTHOR_PHOTO);
    const bool onCover = CoverMaterial::goesOnCover(part);
    m_coverForm->setRowVisible(m_coverPlaceBox, onCover);
    m_coverForm->setRowVisible(m_coverStateBox, onCover);
    m_coverForm->setRowVisible(m_coverAuthorEdit, opinion || design || reference || photo);
    m_coverForm->setRowVisible(m_coverRoleEdit, opinion);
    m_coverForm->setRowVisible(m_coverSourceEdit, opinion);
    m_coverForm->setRowVisible(m_coverDateEdit, opinion || design);
    m_coverForm->setRowVisible(m_coverConsentBox, opinion || photo);
    m_coverForm->setRowVisible(m_coverLicenseEdit, reference);
    m_coverForm->setRowVisible(m_coverAboutEdit, reference);
    if (auto* caption = qobject_cast<QLabel*>(m_coverForm->labelForField(m_coverAuthorEdit))) {
        if (reference) {
            //: Who made a reference picture, or where it comes from. In Polish: &Autor albo
            //: źródło:
            caption->setText(tr("&Author or source:"));
        } else if (photo) {
            //: Who took the photo of the author. In Polish: &Fotograf:
            caption->setText(tr("&Photographer:"));
        } else {
            caption->setText(tr("&Author:"));
        }
    }
    m_bioRow->setVisible(part == QLatin1String(CoverMaterial::BIO));
    if (part == QLatin1String(CoverMaterial::BIO)) {
        updateBioButton();
    }

    showVolume(element);
    const bool readOnly = m_book.isReadOnly();
    for (QComboBox* combo : {m_coverVolumeBox, m_coverPartBox, m_coverPlaceBox, m_coverStateBox,
                             m_coverConsentBox}) {
        combo->setEnabled(!readOnly);
    }
    for (QLineEdit* line : {m_coverAuthorEdit, m_coverRoleEdit, m_coverSourceEdit,
                            m_coverDateEdit, m_coverLicenseEdit, m_coverAboutEdit}) {
        line->setReadOnly(readOnly);
    }

    // The own target, in the unit of the settings; 0: the target of its part
    if (m_targetSpin != nullptr) {
        const bool words = CoverMaterial::countsWords();
        const QSignalBlocker targetBlocker(m_targetSpin);
        m_targetSpin->setValue(words ? m_coverData.targetWords : m_coverData.targetCharacters);
        //: The own target length of a text of the cover when it has none: the default of its
        //: kind; %1 its length. In Polish: jak w Ustawieniach (%1)
        m_targetSpin->setSpecialValueText(
            tr("as in the settings (%1)")
                .arg(QLocale().toString(CoverMaterial::defaultTargetOf(part, words))));
        m_targetSpin->setReadOnly(readOnly);
        m_targetUnit->setText(CoverNames::unitName(words));
    }
    updateCounter();
}

void MaterialPage::updateCounter() {
    using core::CoverMaterial;
    const bool text = m_cover && CoverMaterial::isText(m_coverData.part);
    m_counter->setVisible(text);
    if (!text) {
        return;
    }
    const QString body = m_descriptionEdit->toPlainText();
    const int characters = CoverMaterial::lengthOf(body, false);
    const int words = CoverMaterial::lengthOf(body, true);
    m_countsLabel->setText(CoverNames::countsText(characters, words));

    const bool inWords = CoverMaterial::countsWords();
    const int target = CoverMaterial::targetOf(m_coverData, inWords);
    m_targetLabel->setVisible(target > 0);
    m_targetBar->setVisible(target > 0);
    if (m_targetRow != nullptr) {
        m_targetRow->setVisible(CoverMaterial::hasTarget(m_coverData.part));
    }
    if (target > 0) {
        const int length = inWords ? words : characters;
        const bool over = length > target;
        m_targetBar->setRange(0, target);
        m_targetBar->setValue(std::min(length, target));
        // Past the target the bar and its words take the warning color of the theme
        QPalette warning;
        if (over) {
            const QColor color = core::ThemeManager::getInstance().getCurrentTheme().log.warning;
            warning.setColor(QPalette::Highlight, color);
            warning.setColor(QPalette::WindowText, color);
        }
        m_targetBar->setPalette(warning);
        m_targetLabel->setPalette(warning);
        QFont font = m_targetLabel->font();
        font.setBold(over);
        m_targetLabel->setFont(font);
        m_targetLabel->setText(over ? CoverNames::overText(length - target, inWords)
                                    : CoverNames::targetText(target, inWords));
        m_targetBar->setAccessibleDescription(m_targetLabel->text());
    }
    if (m_printedLabel != nullptr) {
        const bool opinion = m_coverData.part == QLatin1String(CoverMaterial::OPINION) &&
                             !body.trimmed().isEmpty();
        m_printedLabel->setVisible(opinion);
        if (opinion) {
            //: How the cover prints an opinion, under its text; %1 the opinion in quotes with
            //: its author. In Polish: Na okładce: %1
            m_printedLabel->setText(tr("On the cover: %1")
                                        .arg(CoverNames::printedOpinion(body, m_coverData)));
        }
    }
}

void MaterialPage::saveCover(const std::function<void(core::CoverData&)>& change) {
    if (m_updating || m_elementId.isEmpty() || !m_cover) {
        return;
    }
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(m_elementId);
    if (element == nullptr || pm.isReadOnly()) {
        return;
    }
    core::CoverData cover = element->cover;
    change(cover);
    if (cover == element->cover) {
        return;
    }
    if (!pm.setCover(m_elementId, cover)) {
        //: In Polish: Nie zapisano danych okładki
        dialogs::MessageDialog::warning(this, tr("Cover Data Not Saved"),
                                        tr("Failed to save changes."));
        // The project was put back as it was: the element is found again
        if (const core::ProjectElement* kept = pm.findElement(m_elementId)) {
            showCover(*kept);
        }
        return;
    }
    if (const core::ProjectElement* saved = pm.findElement(m_elementId)) {
        showCover(*saved);
    }
    emit changed(m_elementId);
}

void MaterialPage::savePart() {
    const QString part = m_coverPartBox->currentData().toString();
    const core::CoverParts parts = m_book.coverParts();
    saveCover([&part, &parts](core::CoverData& cover) {
        using core::CoverMaterial;
        if (cover.part == part) {
            return;
        }
        // A note has no place and state; a text that comes on the cover goes to the place of
        // its part; a design goes on the whole cover or jacket, and the rest not
        const bool known = CoverMaterial::places().contains(cover.place);
        const bool whole = CoverMaterial::isWhole(cover.place);
        if (!CoverMaterial::goesOnCover(part)) {
            cover.place.clear();
            cover.state.clear();
        } else if (!CoverMaterial::goesOnCover(cover.part)) {
            cover.place = CoverMaterial::defaultPlaceOf(part, parts);
        } else if (known && whole != (part == QLatin1String(CoverMaterial::DESIGN))) {
            cover.place.clear();
        }
        cover.part = part;
    });
}

void MaterialPage::fillPlaces(const QString& part, const QString& place) {
    using core::CoverMaterial;
    const QSignalBlocker placeBlocker(m_coverPlaceBox);
    m_coverPlaceBox->clear();
    auto* model = qobject_cast<QStandardItemModel*>(m_coverPlaceBox->model());
    const core::CoverParts parts = m_book.coverParts();
    const bool design = part == QLatin1String(CoverMaterial::DESIGN);
    const bool known = CoverMaterial::parts().contains(part);
    // The places of the cover, then those of the jacket, each under the name of its part
    for (const bool jacket : {false, true}) {
        const QStringList& places = jacket ? CoverMaterial::jacketPlaces()
                                           : CoverMaterial::coverPlaces();
        const bool off = jacket && !parts.jacket;
        QString heading = CoverNames::placeGroupName(places.constFirst());
        if (off) {
            //: The heading of the places of the jacket in the box of the place, while the
            //: book has no jacket: %1 the name of the jacket. In Polish: %1 (wyłączona)
            heading = tr("%1 (turned off)", "the jacket").arg(heading);
        }
        m_coverPlaceBox->addItem(heading);
        if (model != nullptr) {
            QStandardItem* item = model->item(model->rowCount() - 1);
            item->setFlags(Qt::NoItemFlags);
            QFont bold = m_coverPlaceBox->font();
            bold.setBold(true);
            item->setFont(bold);
        }
        for (const QString& each : places) {
            // A design on the whole, the rest on its parts; a part this version does not
            // know takes any
            if (known && CoverMaterial::isWhole(each) != design && each != place) {
                continue;
            }
            QString name = CoverNames::placeName(each);
            const bool on = CoverMaterial::isOn(each, parts);
            if (!on && !off) {
                //: A place on a part of the cover turned off, in the box of the place: %1 the
                //: place. In Polish: %1 (wyłączone)
                name = tr("%1 (turned off)", "a place").arg(name);
            }
            m_coverPlaceBox->addItem(name, each);
            if (!on && each != place && model != nullptr) {
                model->item(model->rowCount() - 1)->setEnabled(false);
            }
        }
    }
    m_coverPlaceBox->addItem(CoverNames::placeName(QString()), QString());
    if (m_coverPlaceBox->findData(place) < 0) {
        // A place this version does not know stays as it is
        m_coverPlaceBox->addItem(place, place);
    }
    m_coverPlaceBox->setCurrentIndex(m_coverPlaceBox->findData(place));
}

void MaterialPage::updateBioButton() {
    const core::ProjectBook* current = m_book.book();
    const bool has =
        current != nullptr && core::PublishingTexts::aboutAuthorOf(*current) != nullptr;
    m_bioButton->setEnabled(has && !m_book.isReadOnly());
    m_bioButton->setToolTip(
        has ? QString()
            //: The button that takes the text of the note About the Author, in a book
            //: without one. In Polish: Książka nie ma „Noty o autorze” w części końcowej.
            : tr("The book has no \"About the Author\" in its back part."));
}

void MaterialPage::takeAboutAuthor() {
    const core::ProjectBook* current = m_book.book();
    const core::ProjectElement* note =
        current != nullptr ? core::PublishingTexts::aboutAuthorOf(*current) : nullptr;
    if (note == nullptr || m_book.isReadOnly()) {
        return;
    }
    m_descriptionEdit->setPlainText(m_book.publishingTextOf(note->id).trimmed());
    saveDescription();
    //: The status bar after the text of the note About the Author is taken for a note about
    //: the author of the cover. In Polish: Skopiowano treść „Noty o autorze” z części końcowej.
    emit message(tr("The text of \"About the Author\" of the back part was copied."));
}

void MaterialPage::saveState() {
    if (m_updating || m_elementId.isEmpty() || !m_cover) {
        return;
    }
    auto& pm = m_book;
    const core::ProjectElement* element = pm.findElement(m_elementId);
    const QString state = m_coverStateBox->currentData().toString();
    if (element == nullptr || pm.isReadOnly() || element->cover.state == state) {
        return;
    }
    const QString title = element->title;
    QStringList drafts;
    if (!pm.setCoverState(m_elementId, state, &drafts)) {
        //: In Polish: Nie zapisano stanu
        dialogs::MessageDialog::warning(this, tr("State Not Saved"),
                                        tr("Failed to save changes."));
        if (const core::ProjectElement* kept = pm.findElement(m_elementId)) {
            showCover(*kept);
        }
        return;
    }
    emit message(CoverNames::stateMessage(title, state, titlesOf(pm, drafts)));
    emit changed(m_elementId);
    for (const QString& id : drafts) {
        emit changed(id);
    }
}

void MaterialPage::showVolume(const core::ProjectElement& element) {
    const core::BookProject* project = m_book.project();
    const bool series = project != nullptr && m_book.isSeries();
    m_coverForm->setRowVisible(m_coverVolumeBox, series);
    if (series) {
        VolumeNames::fillBox(*m_coverVolumeBox, m_book,
                             core::VolumeMaterials::volumeOf(*project, element));
    }
}

void MaterialPage::saveVolume() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (m_updating || !m_cover || element == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QString volume = m_coverVolumeBox->currentData().toString();
    if (core::VolumeMaterials::volumeOf(*m_book.project(), *element) == volume) {
        return;
    }
    const QString title = element->title;
    QStringList drafts;
    if (!m_book.setMaterialBook(m_elementId, volume, &drafts)) {
        //: In Polish: Nie zapisano tomu
        dialogs::MessageDialog::warning(this, tr("Volume Not Saved"),
                                        tr("Failed to save changes."));
        if (const core::ProjectElement* kept = m_book.findElement(m_elementId)) {
            showCover(*kept);
        }
        return;
    }
    emit message(VolumeNames::changeMessage(title, m_coverVolumeBox->currentText(),
                                            titlesOf(m_book, drafts)));
    emit changed(m_elementId);
    for (const QString& id : drafts) {
        emit changed(id);
    }
}

void MaterialPage::saveTarget() {
    if (m_targetSpin == nullptr) {
        return;
    }
    const int value = m_targetSpin->value();
    const bool words = core::CoverMaterial::countsWords();
    saveCover([value, words](core::CoverData& cover) {
        (words ? cover.targetWords : cover.targetCharacters) = value;
    });
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
    if (watched == m_linksList &&
        (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (event->type() == QEvent::KeyPress && isOpenKey(key)) {
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
            insert ? editLinks() : removeLink();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// MaterialCardPanel
// =============================================================================

MaterialCardPanel::MaterialCardPanel(core::ProjectManager& book, const QString& elementId,
                                     QWidget* parent)
    : QScrollArea(parent), m_book(book) {
    setObjectName(QStringLiteral("materialCardPanel"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    // The card fills the tab, however wide it is, with a margin around it
    auto* content = new QWidget(this);
    auto* row = new QHBoxLayout(content);
    row->setContentsMargins(16, 16, 16, 16);
    m_page = new MaterialPage(m_book, content, true);
    row->addWidget(m_page);
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
    const auto& pm = m_book;
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

LinkedMaterialsBox::LinkedMaterialsBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_menu(new QMenu(this)), m_book(book) {
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
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list);
    m_emptyLabel = hintLabel(this, QString(), "linkedMaterialsEmpty");
    m_menu->setObjectName(QStringLiteral("linkedMaterialsMenu"));
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        fillMenu();
        m_menu->popup(m_list->viewport()->mapToGlobal(at));
    });
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
    connect(&m_book, &core::ProjectManager::linksChanged, this,
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
    const auto& pm = m_book;
    const QString current =
        m_list->currentItem() ? m_list->currentItem()->data(LINK_ROLE).toString() : QString();
    m_list->clear();
    for (const core::ProjectElement* material : pm.materialsLinkedTo(m_link)) {
        QImage picture = readPicture(pm.filePathOf(*material), material->rotation,
                                     QSize(MATERIAL_ICON, MATERIAL_ICON));
        const QIcon icon = picture.isNull()
                               ? linkIcon(m_book, material->id, core::IconContext::Dialog)
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
    const bool readOnly = m_book.isReadOnly();
    m_addButton->setEnabled(!readOnly && !m_link.isEmpty());
    m_removeButton->setEnabled(!readOnly && m_list->currentItem() != nullptr);
}

void LinkedMaterialsBox::fillMenu() {
    m_menu->clear();
    const QListWidgetItem* item = m_list->currentItem();
    const QString material = item != nullptr ? item->data(LINK_ROLE).toString() : QString();
    const bool writable = !m_book.isReadOnly() && !m_link.isEmpty();
    //: The menu of a material linked with a chapter or a card. In Polish: Otwórz materiał
    QAction* open = m_menu->addAction(tr("Open the Material"));
    open->setEnabled(!material.isEmpty());
    connect(open, &QAction::triggered, this,
            [this, material]() { emit requestOpenMaterial(material); });
    QAction* remove = m_menu->addAction(tr("Remove Link (Delete)"));
    remove->setEnabled(writable && !material.isEmpty());
    connect(remove, &QAction::triggered, this, &LinkedMaterialsBox::removeMaterial);
    m_menu->addSeparator();
    QAction* add = m_menu->addAction(tr("Link Material... (Insert)"));
    add->setEnabled(writable);
    connect(add, &QAction::triggered, this, &LinkedMaterialsBox::editMaterials);
    emit menuOpening(m_menu, material);
}

void LinkedMaterialsBox::editMaterials() {
    auto& pm = m_book;
    if (m_link.isEmpty() || pm.isReadOnly()) {
        return;
    }
    dialogs::LinkDialog dialog(m_book, dialogs::LinkDialog::Mode::Materials, m_link, window());
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
    auto& pm = m_book;
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

} // namespace kalahari::gui
