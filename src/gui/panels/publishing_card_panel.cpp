// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_card_panel.cpp
/// @brief The card For Publishers on a tab of its own: the set on top, the versions of the part
///        chosen, the text of the version written in the card and its template beside it

#include "kalahari/gui/panels/publishing_card_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/publishing_text_box.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/publishing_set.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTime>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// How long after the last key the text is saved, and the fields of a sentence
constexpr int SAVE_DELAY_MS = 700;

/// The margins of the card and the gaps between its parts
constexpr int CARD_MARGIN = 10;
constexpr int CARD_SPACING = 8;

/// The title of the card, larger than the text around it
constexpr double TITLE_SCALE = 1.25;

/// The tiles of the set: their width and the gaps between them
constexpr int TILE_MIN_WIDTH = 150;
constexpr int TILE_SPACING = 6;
constexpr int TILE_PADDING = 6;

/// The widths of the versions and of the box beside the text when the card opens
constexpr int VERSIONS_WIDTH = 200;
constexpr int TEXT_WIDTH = 460;
constexpr int BOX_WIDTH = 340;

/// Between the words and the fields of the pattern, and between their lines
constexpr int PATTERN_SPACING = 4;
constexpr int PATTERN_LINE_SPACING = 6;

/// The narrowest field of the pattern, in characters
constexpr int FIELD_MIN_CHARACTERS = 12;

/// The pages of the text: the editor, a text from elsewhere, a part without versions
constexpr int EDITOR_PAGE = 0;
constexpr int OUTSIDE_PAGE = 1;
constexpr int EMPTY_PAGE = 2;

/// The pages of the body: the versions with the text, the preview of the set
constexpr int VERSIONS_PAGE = 0;
constexpr int PREVIEW_PAGE = 1;

/// The role of the versions with their ids
constexpr int ID_ROLE = Qt::UserRole;

/// The property of a tile with its part
constexpr const char* PART_PROPERTY = "publishingPart";

/// Whether @p part is a description or a tagline, which the cover has
bool ofCover(const QString& part) {
    return part == QLatin1String(PublishingTexts::TAGLINE) ||
           part == QLatin1String(PublishingTexts::BLURB);
}

/// A hint under a part of the card, in the color of the hints
QLabel* hintOf(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

/// Whether @p event is the key of a menu: the menu key or Shift+F10
bool isMenuKey(const QKeyEvent& event) {
    return event.key() == Qt::Key_Menu ||
           (event.key() == Qt::Key_F10 && event.modifiers() == Qt::ShiftModifier);
}

}  // namespace

PublishingCardPanel::PublishingCardPanel(core::ProjectManager& book, CommandRegistry& commands,
                                         QWidget* parent)
    : EditorPanel(book, commands, parent), m_book(book) {
    setObjectName(QStringLiteral("publishingCard"));
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(SAVE_DELAY_MS);
    connect(m_saveTimer, &QTimer::timeout, this, &PublishingCardPanel::saveNow);
    m_fieldsTimer = new QTimer(this);
    m_fieldsTimer->setSingleShot(true);
    m_fieldsTimer->setInterval(SAVE_DELAY_MS);
    connect(m_fieldsTimer, &QTimer::timeout, this, [this]() { savePublishing(m_data); });
    buildCard();
    connect(this, &EditorPanel::contentChanged, this, &PublishingCardPanel::onTextChanged);
    // A change made elsewhere: a version added or renamed, its state, the set
    connect(&m_book, &core::ProjectManager::projectChanged, this, &PublishingCardPanel::refresh);
    connect(&m_book, &core::ProjectManager::publishingSetChanged, this,
            &PublishingCardPanel::refresh);
    showPart(QString(), false);
}

PublishingCardPanel::~PublishingCardPanel() = default;

// =============================================================================
// The card
// =============================================================================

void PublishingCardPanel::buildCard() {
    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("publishingCardBody"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(CARD_MARGIN, CARD_MARGIN, CARD_MARGIN, CARD_MARGIN);
    layout->setSpacing(CARD_SPACING);

    // The title and the commands
    auto* header = new QHBoxLayout();
    //: The title of the card of the texts for publishers and the set for a publisher. In
    //: Polish: Dla wydawnictwa
    auto* title = new QLabel(tr("For Publishers"), card);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * TITLE_SCALE);
    title->setFont(titleFont);
    header->addWidget(title);
    header->addStretch(1);
    layout->addLayout(header);
    layout->addWidget(createCommands(card));

    // The tiles of the set
    m_tilesArea = new QWidget(card);
    m_tilesArea->setObjectName(QStringLiteral("publishingTiles"));
    //: The tiles of the parts of the set for a publisher, for screen readers. In Polish:
    //: Zestaw dla wydawnictwa
    m_tilesArea->setAccessibleName(tr("Set for the Publisher"));
    auto* tiles = new FlowLayout(m_tilesArea, TILE_SPACING, TILE_SPACING);
    tiles->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tilesArea);

    // The versions, the text and its box; or the preview of the set
    m_body = new QStackedWidget(card);
    auto* split = new QSplitter(Qt::Horizontal, m_body);
    split->setObjectName(QStringLiteral("publishingSplit"));
    split->setChildrenCollapsible(false);
    m_versions = new QListWidget(split);
    m_versions->setObjectName(QStringLiteral("publishingVersions"));
    //: The versions of the part of the set chosen on the card For Publishers, for screen
    //: readers. In Polish: Wersje
    m_versions->setAccessibleName(tr("Versions"));
    m_versions->setContextMenuPolicy(Qt::CustomContextMenu);
    m_versions->setEditTriggers(QAbstractItemView::EditKeyPressed);
    m_versions->installEventFilter(this);
    split->addWidget(m_versions);
    split->addWidget(createText(split));
    auto* boxArea = new QScrollArea(split);
    boxArea->setWidgetResizable(true);
    boxArea->setFrameShape(QFrame::NoFrame);
    boxArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // its lines wrap
    auto* boxPage = new QWidget(boxArea);
    auto* boxLayout = new QVBoxLayout(boxPage);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    m_textBox = new PublishingTextBox(m_book, boxPage);
    boxLayout->addWidget(m_textBox);
    boxLayout->addStretch(1);
    boxArea->setWidget(boxPage);
    split->addWidget(boxArea);
    split->setStretchFactor(1, 1);
    split->setSizes({VERSIONS_WIDTH, TEXT_WIDTH, BOX_WIDTH});
    m_body->insertWidget(VERSIONS_PAGE, split);
    auto* previewArea = new QScrollArea(m_body);
    previewArea->setWidgetResizable(true);
    m_preview = new PublishingSetView(previewArea);
    m_preview->setObjectName(QStringLiteral("publishingPreview"));
    previewArea->setWidget(m_preview);
    m_body->insertWidget(PREVIEW_PAGE, previewArea);
    layout->addWidget(m_body, 1);

    m_savedLabel = hintOf(card);
    m_savedLabel->setObjectName(QStringLiteral("publishingSaved"));
    layout->addWidget(m_savedLabel);
    showSaved();

    // The editor of the version goes in the card, under the pattern of a sentence
    editor::BookEditor* text = getBookEditor();
    QLayout* own = this->layout();
    own->removeWidget(text);
    own->addWidget(card);
    auto* editorPage = qobject_cast<QVBoxLayout*>(m_textStack->widget(EDITOR_PAGE)->layout());
    editorPage->insertWidget(1, text, 1);
    //: The editor of a text for publishers on its card, for screen readers. In Polish: Tekst
    //: wersji
    text->setAccessibleName(tr("Text of the version"));

    connect(m_versions, &QListWidget::currentItemChanged, this,
            [this](const QListWidgetItem* item) {
                if (item != nullptr && !m_loading) {
                    openVersion(item->data(ID_ROLE).toString());
                }
            });
    connect(m_versions, &QListWidget::itemChanged, this, [this](const QListWidgetItem* item) {
        // F2: the title typed
        const QString id = item->data(ID_ROLE).toString();
        const core::ProjectElement* element = m_book.findElement(id);
        const QString title = item->text().trimmed();
        if (m_loading || element == nullptr || title.isEmpty() || title == element->title) {
            return;
        }
        if (m_book.renameElement(id, title)) {
            emit titleChanged(id);
        }
    });
    connect(m_versions, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showVersionMenu(m_versions->viewport()->mapToGlobal(pos)); });
    connect(m_textBox, &PublishingTextBox::publishingChanged, this,
            [this](const QString& elementId) {
                // Its template, its target, its state: the card follows the book (refresh());
                // a template changed in the box gives a sentence other fields
                showSaved();
                if (elementId == m_versionId) {
                    openVersion(elementId);
                }
                emit publishingChanged(elementId);
            });
    connect(m_textBox, &PublishingTextBox::settingsRequested, this,
            &PublishingCardPanel::settingsRequested);
    connect(m_textBox, &PublishingTextBox::statusMessage, this,
            &PublishingCardPanel::statusMessage);
    connect(m_preview, &PublishingSetView::activated, this, [this](const QString& part) {
        // A part of the preview: back to its versions
        showPreview(false);
        showPart(part, true);
    });
}

QToolButton* PublishingCardPanel::addCommand(QHBoxLayout* bar, const QString& name,
                                             const QString& iconId, const QString& tip) {
    auto* button = new QToolButton(bar->parentWidget());
    button->setObjectName(QStringLiteral("publishingCommand_") + name);
    button->setIcon(core::ArtProvider::getInstance().getIcon(iconId));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setAutoRaise(true);
    bar->addWidget(button);
    m_buttons.insert(name, button);
    return button;
}

QWidget* PublishingCardPanel::createCommands(QWidget* parent) {
    auto* commands = new QWidget(parent);
    commands->setObjectName(QStringLiteral("publishingCommands"));
    auto* bar = new QHBoxLayout(commands);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(2);
    //: The command bar of the card For Publishers. In Polish: Nowa wersja (Insert)
    connect(addCommand(bar, QStringLiteral("new"), QStringLiteral("common.add"),
                       tr("New Version (Insert)")),
            &QToolButton::clicked, this, [this]() { addVersion(false); });
    //: In Polish: Kopia wersji
    connect(addCommand(bar, QStringLiteral("copy"), QStringLiteral("edit.copy"),
                       tr("Copy of the Version")),
            &QToolButton::clicked, this, [this]() { addVersion(true); });
    //: In Polish: Wybierz do zestawu (Spacja)
    connect(addCommand(bar, QStringLiteral("choose"), QStringLiteral("cover.choose"),
                       tr("Choose for the Set (Space)")),
            &QToolButton::clicked, this, &PublishingCardPanel::toggleVersionInSet);
    //: In Polish: Odrzuć
    connect(addCommand(bar, QStringLiteral("reject"), QStringLiteral("cover.reject"),
                       tr("Reject")),
            &QToolButton::clicked, this, &PublishingCardPanel::toggleRejected);
    //: In Polish: Kopiuj tekst
    connect(addCommand(bar, QStringLiteral("copyText"), QStringLiteral("edit.copy"),
                       tr("Copy the Text")),
            &QToolButton::clicked, this, &PublishingCardPanel::copyText);
    bar->addSpacing(CARD_SPACING);
    //: In Polish: Wcześniej w zestawie (Alt+←)
    connect(addCommand(bar, QStringLiteral("earlier"), QStringLiteral("navigation.up"),
                       tr("Earlier in the Set (Alt+Left)")),
            &QToolButton::clicked, this, [this]() { movePart(m_part, -1); });
    //: In Polish: Później w zestawie (Alt+→)
    connect(addCommand(bar, QStringLiteral("later"), QStringLiteral("navigation.down"),
                       tr("Later in the Set (Alt+Right)")),
            &QToolButton::clicked, this, [this]() { movePart(m_part, 1); });
    //: In Polish: Kopiuj wszystko
    connect(addCommand(bar, QStringLiteral("copyAll"), QStringLiteral("materials.publishingSet"),
                       tr("Copy All")),
            &QToolButton::clicked, this, [this]() {
                QString back;
                if (!PublishingSetContents::confirmPoints(m_book, this, false, &back)) {
                    if (!back.isEmpty()) {
                        showVersion(back, true);
                    }
                    return;
                }
                const QString said = PublishingSetContents::copyAll(m_book);
                if (!said.isEmpty()) {
                    emit statusMessage(said);
                }
            });
    //: In Polish: Eksportuj zestaw...
    connect(addCommand(bar, QStringLiteral("export"), QStringLiteral("file.export.docx"),
                       tr("Export Set...")),
            &QToolButton::clicked, this, [this]() {
                QString back;
                if (PublishingSetContents::confirmPoints(m_book, this, true, &back)) {
                    emit requestExportSet();
                } else if (!back.isEmpty()) {
                    showVersion(back, true);
                }
            });
    //: In Polish: Drukuj zestaw...
    connect(addCommand(bar, QStringLiteral("print"), QStringLiteral("file.print"),
                       tr("Print Set...")),
            &QToolButton::clicked, this, &PublishingCardPanel::requestPrintSet);
    //: In Polish: Podgląd zestawu
    QToolButton* preview = addCommand(bar, QStringLiteral("preview"),
                                      QStringLiteral("common.preview"), tr("Preview of the Set"));
    preview->setCheckable(true);
    connect(preview, &QToolButton::toggled, this, &PublishingCardPanel::showPreview);
    bar->addSpacing(CARD_SPACING);
    //: In Polish: Cofnij (Ctrl+Z)
    connect(addCommand(bar, QStringLiteral("undo"), QStringLiteral("edit.undo"),
                       tr("Undo (Ctrl+Z)")),
            &QToolButton::clicked, this, [this]() { getBookEditor()->undo(); });
    //: In Polish: Ponów (Ctrl+Y)
    connect(addCommand(bar, QStringLiteral("redo"), QStringLiteral("edit.redo"),
                       tr("Redo (Ctrl+Y)")),
            &QToolButton::clicked, this, [this]() { getBookEditor()->redo(); });
    bar->addStretch(1);
    //: The gear of the card For Publishers: Settings > Navigator > Texts for Publishers. In
    //: Polish: Ustawienia tekstów dla wydawnictwa
    connect(addCommand(bar, QStringLiteral("settings"), QStringLiteral("dock.settings"),
                       tr("Texts for Publishers Settings")),
            &QToolButton::clicked, this, &PublishingCardPanel::settingsRequested);
    return commands;
}

QWidget* PublishingCardPanel::createText(QWidget* parent) {
    m_textStack = new QStackedWidget(parent);
    m_textStack->setObjectName(QStringLiteral("publishingText"));

    // The editor, with the pattern of a sentence over it
    auto* editorPage = new QWidget(m_textStack);
    auto* editorLayout = new QVBoxLayout(editorPage);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(4);
    auto* pattern = new QWidget(editorPage);
    auto* patternLayout = new QVBoxLayout(pattern);
    patternLayout->setContentsMargins(0, 0, 0, 0);
    m_patternArea = new QWidget(pattern);
    m_patternArea->setObjectName(QStringLiteral("publishingPattern"));
    m_patternLayout = new FlowLayout(m_patternArea, PATTERN_SPACING, PATTERN_LINE_SPACING);
    m_patternLayout->setContentsMargins(0, 0, 0, 0);
    patternLayout->addWidget(m_patternArea);
    m_patternHint = hintOf(pattern);
    patternLayout->addWidget(m_patternHint);
    editorLayout->addWidget(pattern);  // the editor comes after it (buildCard())

    m_manualNote = new QWidget(editorPage);
    m_manualNote->setObjectName(QStringLiteral("publishingManual"));
    auto* manual = new QHBoxLayout(m_manualNote);
    manual->setContentsMargins(0, 0, 0, 0);
    //: A sentence about the book changed by hand. In Polish: Zdanie poprawione ręcznie – pola
    //: wzoru już go nie zmieniają.
    auto* manualText = new QLabel(
        tr("The sentence was changed by hand – the fields of the pattern no longer change it."),
        m_manualNote);
    manualText->setWordWrap(true);
    //: Composes the sentence about the book of the fields of its pattern again. In Polish:
    //: Złóż zdanie z &pól
    m_composeButton = new QPushButton(tr("Compose from the &Fields"), m_manualNote);
    m_composeButton->setObjectName(QStringLiteral("publishingCompose"));
    manual->addWidget(manualText, 1);
    manual->addWidget(m_composeButton);
    editorLayout->addWidget(m_manualNote);
    connect(m_composeButton, &QPushButton::clicked, this, [this]() {
        core::PublishingData changed = m_data;
        changed.manual = false;
        if (savePublishing(changed)) {
            compose();
        }
    });
    m_textStack->insertWidget(EDITOR_PAGE, editorPage);

    // A tagline, a description or the note about the author: written elsewhere
    auto* outside = new QWidget(m_textStack);
    auto* outsideLayout = new QVBoxLayout(outside);
    m_outsideSource = hintOf(outside);
    m_outsideText = new QLabel(outside);
    m_outsideText->setObjectName(QStringLiteral("publishingOutsideText"));
    m_outsideText->setWordWrap(true);
    m_outsideText->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                           Qt::TextSelectableByKeyboard);
    m_outsideText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_outsideButton = new QPushButton(outside);
    m_outsideButton->setObjectName(QStringLiteral("publishingOutsideOpen"));
    outsideLayout->addWidget(m_outsideSource);
    outsideLayout->addWidget(m_outsideText, 1);
    outsideLayout->addWidget(m_outsideButton, 0, Qt::AlignLeft);
    connect(m_outsideButton, &QPushButton::clicked, this, [this]() {
        if (ofCover(m_part)) {
            emit requestShowInCover(m_versionId);
        } else {
            emit requestOpen(m_versionId);
        }
    });
    m_textStack->insertWidget(OUTSIDE_PAGE, outside);

    // A part without a version yet
    auto* empty = new QWidget(m_textStack);
    auto* emptyLayout = new QVBoxLayout(empty);
    m_emptyText = hintOf(empty);
    m_emptyText->setObjectName(QStringLiteral("publishingEmpty"));
    emptyLayout->addWidget(m_emptyText);
    emptyLayout->addStretch(1);
    m_textStack->insertWidget(EMPTY_PAGE, empty);
    return m_textStack;
}

// =============================================================================
// The set
// =============================================================================

QCheckBox* PublishingCardPanel::tileCheck(const QString& part) const {
    const QFrame* tile = m_tiles.value(part);
    return tile != nullptr ? tile->findChild<QCheckBox*>() : nullptr;
}

QFrame* PublishingCardPanel::createTile(const QString& part) {
    auto* tile = new QFrame(m_tilesArea);
    tile->setObjectName(QStringLiteral("publishingTile_") + part);
    tile->setProperty(PART_PROPERTY, part);
    tile->setFrameShape(QFrame::StyledPanel);
    tile->setFocusPolicy(Qt::StrongFocus);
    tile->setMinimumWidth(TILE_MIN_WIDTH);
    tile->installEventFilter(this);
    auto* tileLayout = new QVBoxLayout(tile);
    tileLayout->setContentsMargins(TILE_PADDING, TILE_PADDING, TILE_PADDING, TILE_PADDING);
    tileLayout->setSpacing(2);
    auto* check = new QCheckBox(tile);
    check->setFocusPolicy(Qt::NoFocus);  // Space on the tile
    QFont bold = check->font();
    bold.setBold(true);
    check->setFont(bold);
    connect(check, &QCheckBox::clicked, this, [this, part]() { togglePart(part); });
    tileLayout->addWidget(check);
    auto* version = hintOf(tile);
    version->setObjectName(QStringLiteral("publishingTileVersion"));
    tileLayout->addWidget(version);
    auto* length = hintOf(tile);
    length->setObjectName(QStringLiteral("publishingTileLength"));
    tileLayout->addWidget(length);
    return tile;
}

void PublishingCardPanel::fillTiles() {
    // The tiles stay while their parts do: a key on a tile changes the set
    auto* layout = static_cast<FlowLayout*>(m_tilesArea->layout());
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item;  // the tile stays
    }
    QHash<QString, QFrame*> gone = m_tiles;
    m_tiles.clear();
    m_order.clear();
    const bool readOnly = m_book.isReadOnly();
    for (const PublishingSetItem& item : PublishingSetContents::itemsOf(m_book)) {
        QFrame* tile = gone.take(item.part);
        if (tile == nullptr) {
            tile = createTile(item.part);
        }
        tile->setAccessibleName(item.name);
        auto* check = tile->findChild<QCheckBox*>();
        check->setText(item.name);
        check->setChecked(item.on);
        check->setEnabled(!readOnly);
        const auto version = std::find_if(item.versions.cbegin(), item.versions.cend(),
                                          [&item](const PublishingSetVersion& each) {
                                              return each.id == item.elementId;
                                          });
        //: A tile of a part of the set without a version. In Polish: Brak wersji
        tile->findChild<QLabel*>(QStringLiteral("publishingTileVersion"))
            ->setText(version != item.versions.cend() ? version->title : tr("No version"));
        tile->findChild<QLabel*>(QStringLiteral("publishingTileLength"))->setText(item.length);
        layout->addWidget(tile);
        tile->show();
        m_tiles.insert(item.part, tile);
        m_order.append(item.part);
    }
    for (QFrame* tile : std::as_const(gone)) {
        tile->hide();
        tile->deleteLater();
    }
    // The keys stay on the tile of their part, wherever it is now
    for (int i = 1; i < m_order.size(); ++i) {
        setTabOrder(m_tiles.value(m_order.at(i - 1)), m_tiles.value(m_order.at(i)));
    }
    markTiles();
}

void PublishingCardPanel::markTiles() {
    for (auto it = m_tiles.cbegin(); it != m_tiles.cend(); ++it) {
        const bool current = it.key() == m_part;
        // The part chosen has a frame in the color of the selection; the tile alone, not the
        // labels in it
        const QString look = current ? QStringLiteral("QFrame#%1 { border: 2px solid "
                                                      "palette(highlight); border-radius: 4px; }")
                                     : QStringLiteral("QFrame#%1 { border: 1px solid palette(mid); "
                                                      "border-radius: 4px; }");
        it.value()->setStyleSheet(look.arg(it.value()->objectName()));
        it.value()->setAccessibleDescription(
            //: A tile of the set chosen, for screen readers. In Polish: wybrana część
            current ? tr("chosen part") : QString());
    }
}

void PublishingCardPanel::showPart(const QString& part, bool keys) {
    fillTiles();
    QString chosen = part;
    if (!m_order.contains(chosen)) {
        chosen = m_order.contains(m_part) ? m_part : m_order.value(0);
    }
    m_part.clear();  // shown again
    choosePart(chosen, keys);
}

void PublishingCardPanel::choosePart(const QString& part, bool keys) {
    if (part != m_part) {
        saveNow();
        m_part = part;
        m_versionId.clear();
        markTiles();
        fillVersions();
        updateCommands();
    }
    if (keys && m_tiles.contains(part)) {
        m_tiles.value(part)->setFocus(Qt::OtherFocusReason);
    }
}

void PublishingCardPanel::togglePart(const QString& part) {
    if (m_book.isReadOnly() || !m_order.contains(part)) {
        return;
    }
    const core::PublishingSet set = m_book.publishingSet();
    if (m_book.setPublishingSet(set.withPart(part, !set.includes(part)))) {
        showSaved();
    }
    showPart(m_part, m_tilesArea->isAncestorOf(QApplication::focusWidget()));
}

void PublishingCardPanel::movePart(const QString& part, int step) {
    if (m_book.isReadOnly() || !m_order.contains(part)) {
        return;
    }
    const bool keys = m_tilesArea->isAncestorOf(QApplication::focusWidget());
    if (m_book.setPublishingSet(m_book.publishingSet().withPartMoved(part, step))) {
        showSaved();
    }
    showPart(part, keys);
}

bool PublishingCardPanel::tileKey(const QString& part, const QKeyEvent& event) {
    const qsizetype index = m_order.indexOf(part);
    const bool alt = event.modifiers() == Qt::AltModifier;
    const bool none = event.modifiers() == Qt::NoModifier || event.modifiers() == Qt::KeypadModifier;
    switch (event.key()) {
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const int step = event.key() == Qt::Key_Left ? -1 : 1;
        if (alt) {
            movePart(part, step);
            return true;
        }
        if (none) {
            const qsizetype next = std::clamp<qsizetype>(index + step, 0, m_order.size() - 1);
            choosePart(m_order.value(next), true);
            return true;
        }
        return false;
    }
    case Qt::Key_Home:
    case Qt::Key_End:
        if (none && !m_order.isEmpty()) {
            choosePart(event.key() == Qt::Key_Home ? m_order.first() : m_order.last(), true);
            return true;
        }
        return false;
    case Qt::Key_Space:
        if (none) {
            togglePart(part);
            return true;
        }
        return false;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (none) {
            choosePart(part, false);
            m_versions->setFocus(Qt::OtherFocusReason);
            return true;
        }
        return false;
    default:
        return false;
    }
}

// =============================================================================
// The versions
// =============================================================================

void PublishingCardPanel::fillVersions() {
    m_loading = true;
    m_versions->clear();
    const core::BookProject* project = m_book.project();
    QString inSet;
    for (const core::PublishingSetPart& each : m_book.publishingSetParts()) {
        if (each.part == m_part) {
            inSet = each.elementId;
        }
    }
    QString shown = m_versionId;
    if (project != nullptr) {
        for (const core::ProjectElement* version :
             PublishingTexts::versionsOf(*project, m_part, m_book.book())) {
            auto* item = new QListWidgetItem(version->title, m_versions);
            item->setData(ID_ROLE, version->id);
            const bool rejected =
                core::ProjectManager::isPublishingText(*version)
                    ? version->publishing.state == QLatin1String(PublishingTexts::REJECTED)
                    : version->cover.state == QLatin1String(core::CoverMaterial::REJECTED);
            if (version->id == inSet) {
                //: A version in the set on the card For Publishers: %1 its title. In Polish:
                //: %1 ★
                item->setText(tr("%1 ★").arg(version->title));
                //: For screen readers. In Polish: w zestawie
                item->setData(Qt::AccessibleDescriptionRole, tr("in the set"));
            }
            if (rejected) {
                QFont struck = item->font();
                struck.setStrikeOut(true);
                item->setFont(struck);
                //: For screen readers. In Polish: odrzucona
                item->setData(Qt::AccessibleDescriptionRole, tr("rejected"));
            }
            if (writtenHere() && !m_book.isReadOnly()) {
                item->setFlags(item->flags() | Qt::ItemIsEditable);
            }
            if (shown.isEmpty() && version->id == inSet) {
                shown = version->id;
            }
        }
    }
    // The note about the author: the one of the back part, which the set has
    if (m_versions->count() == 0 && !inSet.isEmpty()) {
        if (const core::ProjectElement* element = m_book.findElement(inSet)) {
            auto* item = new QListWidgetItem(tr("%1 ★").arg(element->title), m_versions);
            item->setData(ID_ROLE, element->id);
        }
    }
    int row = -1;
    for (int i = 0; i < m_versions->count(); ++i) {
        if (m_versions->item(i)->data(ID_ROLE).toString() == shown) {
            row = i;
        }
    }
    if (row < 0 && m_versions->count() > 0) {
        row = 0;
    }
    m_versions->setCurrentRow(row);
    m_loading = false;
    openVersion(row >= 0 ? m_versions->item(row)->data(ID_ROLE).toString() : QString());
}

bool PublishingCardPanel::showVersion(const QString& elementId, bool keys) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr) {
        return false;
    }
    QString part = element->publishing.part;
    if (part.isEmpty() && ofCover(element->cover.part)) {
        part = element->cover.part;
    }
    if (part.isEmpty()) {
        for (const core::PublishingSetPart& each : m_book.publishingSetParts()) {
            if (each.elementId == elementId) {
                part = each.part;
            }
        }
    }
    if (part.isEmpty()) {
        return false;
    }
    showPreview(false);
    fillTiles();
    if (part != m_part) {
        saveNow();
        m_part = part;
        markTiles();
    }
    m_versionId = elementId;
    fillVersions();
    updateCommands();
    if (keys) {
        if (writtenHere()) {
            getBookEditor()->setFocus(Qt::OtherFocusReason);
        } else {
            m_versions->setFocus(Qt::OtherFocusReason);
        }
    }
    return true;
}

void PublishingCardPanel::refresh() {
    if (!m_book.isProjectOpen()) {
        return;
    }
    // The version shown may be gone
    if (!m_versionId.isEmpty() && m_book.findElement(m_versionId) == nullptr) {
        m_versionId.clear();
        setProperty("elementId", QString());
    }
    fillTiles();
    if (!m_order.contains(m_part)) {
        m_part = m_order.value(0);
        markTiles();
    }
    const bool keys = m_versions->hasFocus();
    fillVersions();
    if (keys) {
        m_versions->setFocus(Qt::OtherFocusReason);
    }
    if (m_preview->isVisible()) {
        showPreview(true);
    }
    updateCommands();
}

void PublishingCardPanel::openVersion(const QString& elementId) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element != nullptr && elementId == m_versionId &&
        property("elementId").toString() == elementId) {
        // The same version: its data again (with the fields typed and not yet saved), the
        // text as it is; another template has other fields
        const QString shownTemplate = m_data.templateId;
        const QString shownPattern = m_fieldsPattern;
        const QMap<QString, QString> typed = m_data.fields;
        m_data = element->publishing;
        if (m_fieldsTimer->isActive()) {
            m_data.fields = typed;
        }
        const std::optional<core::PublishingTemplate> templ = currentTemplate();
        const QString pattern = templ ? templ->pattern.text(m_book.bookLanguage()) : QString();
        if (writtenHere() && (shownTemplate != m_data.templateId || shownPattern != pattern)) {
            buildFields();
            if (!m_data.manual) {
                compose();
            }
        }
        if (writtenHere()) {
            m_textBox->showText(elementId, getBookEditor());
        }
        updateManualNote();
        updateCommands();
        return;
    }
    saveNow();
    m_versionId = element != nullptr ? elementId : QString();
    m_data = element != nullptr ? element->publishing : core::PublishingData{};
    const bool here = element != nullptr && core::ProjectManager::isPublishingText(*element);
    // The tab is the editor of the version written here, for Save, spelling and the menus
    setProperty("elementId", here ? elementId : QString());
    if (here) {
        m_loading = true;
        // A text with changes not saved (Replace All in Book) has them in the editor, saved
        setContent(m_book.loadChapterContent(elementId));
        if (m_book.hasUnsavedText(elementId)) {
            getBookEditor()->replaceWithKml(m_book.chapterText(elementId));
        }
        getBookEditor()->setReadOnly(m_book.isReadOnly());
        setImagesFolder(m_book.imagesBaseFolder());
        m_loading = false;
        m_textStack->setCurrentIndex(EDITOR_PAGE);
        buildFields();
        m_textBox->showText(elementId, getBookEditor());
        if (m_book.hasUnsavedText(elementId)) {
            saveNow();
        }
    } else {
        m_textBox->showText(QString(), nullptr);
        if (element != nullptr) {
            m_textStack->setCurrentIndex(OUTSIDE_PAGE);
            m_outsideText->setText(m_book.publishingTextOf(elementId).trimmed());
            if (ofCover(m_part)) {
                //: Over a tagline or a description on the card For Publishers. In Polish:
                //: Pochodzi z Okładki – tam się go zmienia.
                m_outsideSource->setText(tr("It comes from the Cover – it is changed there."));
                //: In Polish: Pokaż w &Okładce
                m_outsideButton->setText(tr("Show in the &Cover"));
            } else {
                //: Over the note about the author on the card For Publishers. In Polish:
                //: Pochodzi z sekcji końcowej książki – tam się ją zmienia.
                m_outsideSource->setText(
                    tr("It comes from the back part of the book – it is changed there."));
                //: In Polish: &Otwórz
                m_outsideButton->setText(tr("&Open"));
            }
        } else {
            m_textStack->setCurrentIndex(EMPTY_PAGE);
            m_emptyText->setText(
                writtenHere()
                    //: A part of the set without versions on the card For Publishers. In
                    //: Polish: Ta część nie ma jeszcze wersji. Insert albo „Nowa wersja”
                    //: dodaje pierwszą.
                    ? tr("This part has no version yet. Insert or New Version adds the first.")
                    //: In Polish: Ta część nie ma jeszcze tekstu.
                    : tr("This part has no text yet."));
        }
    }
    updateManualNote();
    updateCommands();
    emit versionShown(m_versionId);
}

bool PublishingCardPanel::versionKey(const QKeyEvent& event) {
    const bool none = event.modifiers() == Qt::NoModifier || event.modifiers() == Qt::KeypadModifier;
    if (isMenuKey(event)) {
        const QListWidgetItem* item = m_versions->currentItem();
        const QRect rect = item != nullptr ? m_versions->visualItemRect(item) : QRect();
        showVersionMenu(m_versions->viewport()->mapToGlobal(rect.bottomLeft()));
        return true;
    }
    if (!none) {
        return false;
    }
    switch (event.key()) {
    case Qt::Key_Space:
        toggleVersionInSet();
        return true;
    case Qt::Key_Insert:
        addVersion(false);
        return true;
    case Qt::Key_Delete:
        if (writtenHere() && !m_versionId.isEmpty() && !m_book.isReadOnly()) {
            emit requestDelete(m_versionId);
        }
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_textStack->currentIndex() == EDITOR_PAGE) {
            getBookEditor()->setFocus(Qt::OtherFocusReason);
        } else if (m_textStack->currentIndex() == OUTSIDE_PAGE) {
            m_outsideButton->setFocus(Qt::OtherFocusReason);
        }
        return true;
    case Qt::Key_Escape:
        if (m_tiles.contains(m_part)) {
            m_tiles.value(m_part)->setFocus(Qt::OtherFocusReason);
            return true;
        }
        return false;
    default:
        return false;
    }
}

QString PublishingCardPanel::addVersion(bool copy) {
    if (!writtenHere() || m_book.isReadOnly()) {
        if (!m_book.isReadOnly()) {
            //: Insert on a tagline or a description on the card For Publishers. In Polish:
            //: Hasło i opis pisze się w Okładce.
            emit statusMessage(tr("A tagline and a description are written in the Cover."));
        }
        return {};
    }
    saveNow();
    // What the copy takes, before the book has one more element
    const core::ProjectElement* source = copy ? m_book.findElement(m_versionId) : nullptr;
    if (copy && source == nullptr) {
        return {};
    }
    const core::PublishingData sourceData =
        source != nullptr ? source->publishing : core::PublishingData{};
    const QString sourceText = source != nullptr ? m_book.chapterText(source->id) : QString();
    QString title = PublishingNames::newTitle(m_book, m_part);
    if (source != nullptr) {
        //: The title of a copy of a version on the card For Publishers: %1 its title. In
        //: Polish: %1 – kopia
        title = tr("%1 – copy").arg(source->title);
    }
    const QString id = m_book.addPublishingText(
        m_part, source != nullptr ? sourceData.templateId : m_data.templateId, title);
    if (id.isEmpty()) {
        return {};
    }
    if (copy) {
        // The copy has the text and what the version says, as a draft
        core::PublishingData data = sourceData;
        data.state.clear();
        m_book.setPublishing(id, data);
        m_book.setChapterContent(id, sourceText);
        m_book.saveChapterContent(id);
    }
    emit publishingChanged(id);
    showVersion(id, true);
    showSaved();
    return id;
}

void PublishingCardPanel::toggleVersionInSet() {
    if (m_versionId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    saveNow();
    bool inSet = false;
    bool picked = false;
    for (const core::PublishingSetPart& each : m_book.publishingSetParts()) {
        if (each.part == m_part && each.elementId == m_versionId) {
            inSet = true;
            picked = each.picked;
        }
    }
    const core::ProjectElement* element = m_book.findElement(m_versionId);
    bool done = false;
    if (!inSet && element != nullptr && core::ProjectManager::isPublishingText(*element)) {
        // Chosen for the set, in place of the version chosen before
        done = m_book.setPublishingState(m_versionId,
                                         QString::fromLatin1(PublishingTexts::CHOSEN));
    } else if (!inSet) {
        done = m_book.pickForPublishingSet(m_versionId);
    } else if (picked) {
        core::PublishingSet set = m_book.publishingSet();
        set.picks.remove(m_part);
        done = m_book.setPublishingSet(set);
    } else if (element != nullptr && core::ProjectManager::isPublishingText(*element)) {
        // Chosen for the set: a draft again
        done = m_book.setPublishingState(m_versionId, QString());
    }
    if (done) {
        emit publishingChanged(m_versionId);
        showSaved();
    }
    refresh();
}

void PublishingCardPanel::toggleRejected() {
    const core::ProjectElement* element = m_book.findElement(m_versionId);
    if (element == nullptr || !core::ProjectManager::isPublishingText(*element) ||
        m_book.isReadOnly()) {
        return;
    }
    const bool rejected = element->publishing.state == QLatin1String(PublishingTexts::REJECTED);
    if (m_book.setPublishingState(m_versionId, rejected
                                                   ? QString()
                                                   : QString::fromLatin1(PublishingTexts::REJECTED))) {
        emit publishingChanged(m_versionId);
        showSaved();
    }
    refresh();
}

void PublishingCardPanel::copyText() {
    const QString text = m_textStack->currentIndex() == EDITOR_PAGE
                             ? getBookEditor()->plainText().trimmed()
                             : m_outsideText->text();
    if (text.isEmpty()) {
        return;
    }
    QApplication::clipboard()->setText(text);
    //: The status bar after Copy the Text on the card For Publishers. In Polish: Tekst
    //: skopiowany do schowka
    emit statusMessage(tr("The text is copied to the clipboard"));
}

void PublishingCardPanel::showVersionMenu(const QPoint& globalPos) {
    if (m_versionId.isEmpty()) {
        return;
    }
    QMenu menu(this);
    fillVersionMenu(menu);
    menu.exec(globalPos);
}

void PublishingCardPanel::fillVersionMenu(QMenu& menu) {
    const bool readOnly = m_book.isReadOnly();
    auto& art = core::ArtProvider::getInstance();
    //: The menu of a version on the card For Publishers. In Polish: Wybierz do &zestawu
    QAction* choose = menu.addAction(art.getIcon(QStringLiteral("cover.choose")),
                                     tr("Choose for the &Set"));
    choose->setEnabled(!readOnly);
    connect(choose, &QAction::triggered, this, &PublishingCardPanel::toggleVersionInSet);
    if (writtenHere()) {
        const bool rejected = m_data.state == QLatin1String(PublishingTexts::REJECTED);
        QAction* reject = menu.addAction(art.getIcon(QStringLiteral("cover.reject")),
                                         rejected
                                             //: In Polish: Przywróć jako &roboczą
                                             ? tr("Restore as a &Draft")
                                             //: In Polish: &Odrzuć
                                             : tr("&Reject"));
        reject->setEnabled(!readOnly);
        connect(reject, &QAction::triggered, this, &PublishingCardPanel::toggleRejected);
        //: In Polish: Zmień &nazwę (F2)
        QAction* rename = menu.addAction(art.getIcon(QStringLiteral("edit.rename")),
                                         tr("Re&name (F2)"));
        rename->setEnabled(!readOnly);
        connect(rename, &QAction::triggered, this, [this]() {
            if (QListWidgetItem* item = m_versions->currentItem()) {
                m_versions->editItem(item);
            }
        });
        //: In Polish: &Kopia wersji
        QAction* copy = menu.addAction(art.getIcon(QStringLiteral("edit.copy")),
                                       tr("Cop&y of the Version"));
        copy->setEnabled(!readOnly);
        connect(copy, &QAction::triggered, this, [this]() { addVersion(true); });
    }
    //: In Polish: Kopiuj &tekst
    QAction* copyText = menu.addAction(art.getIcon(QStringLiteral("edit.copy")),
                                       tr("Copy the &Text"));
    connect(copyText, &QAction::triggered, this, &PublishingCardPanel::copyText);
    if (ofCover(m_part)) {
        menu.addSeparator();
        //: In Polish: Pokaż w &Okładce
        QAction* cover = menu.addAction(art.getIcon(QStringLiteral("material.cover")),
                                        tr("Show in the &Cover"));
        connect(cover, &QAction::triggered, this,
                [this]() { emit requestShowInCover(m_versionId); });
    }
    if (writtenHere()) {
        menu.addSeparator();
        //: In Polish: &Usuń (Delete)
        QAction* remove = menu.addAction(art.getIcon(QStringLiteral("edit.delete")),
                                         tr("&Delete (Delete)"));
        remove->setEnabled(!readOnly);
        connect(remove, &QAction::triggered, this,
                [this]() { emit requestDelete(m_versionId); });
    }
}

void PublishingCardPanel::updateCommands() {
    const bool readOnly = m_book.isReadOnly();
    const bool here = writtenHere();
    const bool version = !m_versionId.isEmpty();
    m_buttons.value(QStringLiteral("new"))->setEnabled(here && !readOnly);
    m_buttons.value(QStringLiteral("copy"))->setEnabled(here && version && !readOnly);
    m_buttons.value(QStringLiteral("choose"))->setEnabled(version && !readOnly);
    m_buttons.value(QStringLiteral("reject"))->setEnabled(here && version && !readOnly);
    m_buttons.value(QStringLiteral("copyText"))->setEnabled(version);
    const qsizetype index = m_order.indexOf(m_part);
    m_buttons.value(QStringLiteral("earlier"))->setEnabled(!readOnly && index > 0);
    m_buttons.value(QStringLiteral("later"))
        ->setEnabled(!readOnly && index >= 0 && index + 1 < m_order.size());
    m_buttons.value(QStringLiteral("undo"))->setEnabled(here && version && !readOnly);
    m_buttons.value(QStringLiteral("redo"))->setEnabled(here && version && !readOnly);
    if (QToolButton* reject = m_buttons.value(QStringLiteral("reject"))) {
        const bool rejected = m_data.state == QLatin1String(PublishingTexts::REJECTED);
        reject->setToolTip(rejected ? tr("Restore as a &Draft").remove(QLatin1Char('&'))
                                    : tr("Reject"));
    }
}

void PublishingCardPanel::showPreview(bool shown) {
    QToolButton* button = m_buttons.value(QStringLiteral("preview"));
    if (button != nullptr && button->isChecked() != shown) {
        const QSignalBlocker blocker(button);
        button->setChecked(shown);
    }
    if (shown) {
        saveNow();
        m_preview->setHeader(PublishingSetContents::titleOf(m_book),
                             PublishingSetContents::aboutOf(m_book));
        m_preview->setItems(PublishingSetContents::itemsOf(m_book));
        m_preview->setCurrent(m_part);
    }
    m_body->setCurrentIndex(shown ? PREVIEW_PAGE : VERSIONS_PAGE);
}

// =============================================================================
// The text of the version
// =============================================================================

bool PublishingCardPanel::writtenHere() const {
    return m_part == QLatin1String(PublishingTexts::PITCH) ||
           m_part == QLatin1String(PublishingTexts::SYNOPSIS);
}

std::optional<core::PublishingTemplate> PublishingCardPanel::currentTemplate() const {
    if (m_data.templateId.isEmpty()) {
        return std::nullopt;
    }
    return m_book.publishingTemplate(m_data.templateId);
}

void PublishingCardPanel::buildFields() {
    while (QLayoutItem* item = m_patternLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_fields.clear();
    const bool pitch = m_data.part == QLatin1String(PublishingTexts::PITCH);
    // The pattern in the language of the book, which the sentence is written in
    const core::PublishingTemplate templ = currentTemplate().value_or(core::PublishingTemplate{});
    const QString pattern = pitch ? templ.pattern.text(m_book.bookLanguage()) : QString();
    m_fieldsPattern = pattern;
    m_patternArea->setVisible(!pattern.isEmpty());
    m_patternHint->setVisible(pitch);
    if (pattern.isEmpty()) {
        //: The card For Publishers, a sentence about the book without a template. In Polish:
        //: Bez szablonu – zdanie piszesz od razu w polu niżej.
        m_patternHint->setText(tr("Without a template – you write the sentence in the field "
                                  "below."));
        return;
    }
    //: Under the fields of the pattern of a sentence about the book. In Polish: Wpisz pola –
    //: zdanie niżej składa się od razu. Pola przechodzi się klawiszem Tab.
    m_patternHint->setText(tr("Type the fields – the sentence below is composed at once. Tab "
                              "goes through the fields."));
    const bool readOnly = m_book.isReadOnly();
    static const QRegularExpression fieldPattern(QStringLiteral("(\\{\\w+\\})"));
    qsizetype from = 0;
    QRegularExpressionMatchIterator found = fieldPattern.globalMatch(pattern);
    const auto addWords = [this](const QString& words) {
        if (!words.trimmed().isEmpty()) {
            m_patternLayout->addWidget(new QLabel(words.trimmed(), m_patternArea));
        }
    };
    while (found.hasNext()) {
        const QRegularExpressionMatch match = found.next();
        addWords(pattern.mid(from, match.capturedStart() - from));
        from = match.capturedEnd();
        const QString id = match.captured(1).mid(1, match.capturedLength(1) - 2);
        const auto definition = std::find_if(
            templ.fields.cbegin(), templ.fields.cend(),
            [&id](const core::PublishingTemplateField& each) { return each.id == id; });
        if (definition == templ.fields.cend() || m_fields.contains(id)) {
            continue;
        }
        auto* box = new QWidget(m_patternArea);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(0);
        auto* edit = new QLineEdit(box);
        edit->setObjectName(QStringLiteral("publishingField_") + id);
        edit->setPlaceholderText(definition->hint.text(programLanguage()));
        edit->setAccessibleName(definition->name.text(programLanguage()));
        edit->setText(m_data.fields.value(id));
        edit->setCursorPosition(0);  // a value longer than the field shows from its start
        edit->setReadOnly(readOnly);
        edit->setMinimumWidth(edit->fontMetrics().averageCharWidth() * FIELD_MIN_CHARACTERS);
        auto* caption = hintOf(box);
        caption->setText(definition->name.text(programLanguage()));
        boxLayout->addWidget(edit);
        boxLayout->addWidget(caption);
        m_patternLayout->addWidget(box);
        m_fields.insert(id, edit);
        connect(edit, &QLineEdit::textEdited, this, [this, id](const QString& text) {
            m_data.fields.insert(id, text);
            if (!m_data.manual) {
                compose();
            }
            m_fieldsTimer->start();
        });
        connect(edit, &QLineEdit::editingFinished, this, [this]() {
            if (m_fieldsTimer->isActive()) {
                m_fieldsTimer->stop();
                savePublishing(m_data);
            }
        });
    }
    addWords(pattern.mid(from));
}

QString PublishingCardPanel::composed() const {
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    if (!templ || m_data.part != QLatin1String(PublishingTexts::PITCH)) {
        return {};
    }
    const bool any = std::any_of(m_data.fields.cbegin(), m_data.fields.cend(),
                                 [](const QString& value) { return !value.trimmed().isEmpty(); });
    return any ? PublishingTexts::composePitch(templ->pattern.text(m_book.bookLanguage()),
                                               m_data.fields)
               : QString();
}

void PublishingCardPanel::compose() {
    const QString sentence = composed();
    if (sentence.isEmpty() || getBookEditor()->plainText().trimmed() == sentence) {
        return;
    }
    m_composing = true;
    getBookEditor()->replaceWithKml(editor::ClipboardHandler::textToKml(sentence));
    m_composing = false;
}

void PublishingCardPanel::onTextChanged() {
    if (m_loading || m_versionId.isEmpty()) {
        return;
    }
    m_textBox->updateLength();
    m_saveTimer->start();
    if (m_composing || m_data.manual || m_book.isReadOnly() || composed().isEmpty()) {
        return;
    }
    if (getBookEditor()->plainText().trimmed() == composed()) {
        return;
    }
    // Changed by hand: the fields no longer make it
    core::PublishingData changed = m_data;
    changed.manual = true;
    savePublishing(changed);
}

bool PublishingCardPanel::savePublishing(const core::PublishingData& publishing) {
    m_fieldsTimer->stop();
    const core::ProjectElement* element = m_book.findElement(m_versionId);
    if (element == nullptr || m_book.isReadOnly()) {
        return false;
    }
    m_data = publishing;
    if (element->publishing == publishing) {
        updateManualNote();
        return true;
    }
    if (!m_book.setPublishing(m_versionId, publishing)) {
        return false;
    }
    updateManualNote();
    showSaved();
    m_textBox->showText(m_versionId, getBookEditor());
    emit publishingChanged(m_versionId);
    return true;
}

void PublishingCardPanel::updateManualNote() {
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    const bool pattern = m_data.part == QLatin1String(PublishingTexts::PITCH) && templ &&
                         !templ->pattern.text(m_book.bookLanguage()).isEmpty();
    m_manualNote->setVisible(pattern && m_data.manual);
    m_composeButton->setEnabled(!m_book.isReadOnly());
}

bool PublishingCardPanel::saveNow() {
    m_saveTimer->stop();
    if (m_fieldsTimer->isActive()) {
        savePublishing(m_data);
    }
    const QString id = property("elementId").toString();
    if (id.isEmpty() || !isModified() || m_book.isReadOnly()) {
        return true;
    }
    m_book.setChapterContent(id, getContent());
    if (!m_book.saveChapterContent(id)) {
        //: The status bar when the text on the card For Publishers cannot be saved. In
        //: Polish: Nie można zapisać tekstu wersji.
        emit statusMessage(tr("The text of the version cannot be saved."));
        return false;
    }
    setUnmodified();
    showSaved();
    fillTiles();  // its length on its tile
    return true;
}

void PublishingCardPanel::showSaved() {
    if (m_book.isReadOnly()) {
        //: The footer of the card For Publishers in a book open read-only. In Polish: Książka
        //: jest otwarta tylko do odczytu.
        m_savedLabel->setText(tr("The book is open read-only."));
        return;
    }
    const bool first = m_savedAt.isEmpty() && m_tiles.isEmpty();
    if (!first) {
        m_savedAt = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    }
    m_savedLabel->setText(
        m_savedAt.isEmpty()
            //: The footer of the card For Publishers. In Polish: Wszystko w tej karcie
            //: zapisuje się od razu.
            ? tr("Everything in this card is saved at once.")
            //: The footer of the card For Publishers: %1 the time. In Polish: Wszystko w tej
            //: karcie zapisuje się od razu · zapisano o %1
            : tr("Everything in this card is saved at once · saved at %1").arg(m_savedAt));
}

// =============================================================================
// Events
// =============================================================================

bool PublishingCardPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto& key = static_cast<const QKeyEvent&>(*event);
        if (watched == m_versions) {
            // While a title is edited (F2) the keys go to its field, not to the list
            if (versionKey(key)) {
                return true;
            }
        } else if (const auto* tile = qobject_cast<QFrame*>(watched);
                   tile != nullptr && m_tiles.contains(tile->property(PART_PROPERTY).toString())) {
            if (tileKey(tile->property(PART_PROPERTY).toString(), key)) {
                return true;
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        if (const auto* tile = qobject_cast<QFrame*>(watched);
            tile != nullptr && m_tiles.contains(tile->property(PART_PROPERTY).toString())) {
            choosePart(tile->property(PART_PROPERTY).toString(), true);
        }
    }
    return EditorPanel::eventFilter(watched, event);
}

void PublishingCardPanel::hideEvent(QHideEvent* event) {
    saveNow();
    EditorPanel::hideEvent(event);
}

}  // namespace kalahari::gui
