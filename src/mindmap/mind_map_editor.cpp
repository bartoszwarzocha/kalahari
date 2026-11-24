// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_editor.cpp
/// @brief The tab of a mind map: its bars, the map of nodes, the list and the messages

#include <kalahari/mindmap/mind_map_editor.h>

#include <kalahari/core/art_provider.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/gui/dialogs/element_choice_dialog.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_list.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_node_dialog.h>
#include <kalahari/mindmap/mind_map_relation_map.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace kalahari::mindmap {

namespace {

/// How long a message stays in the bar under the map
constexpr int MESSAGE_TIME = 10000;
constexpr int BAR_MARGIN = 6;
constexpr int BAR_SPACING = 4;
constexpr int GROUP_SPACING = 12;

/// A label of the bar, its buddy @p field
QLabel* barLabel(const QString& text, QWidget* field, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

/// A line between the groups of a bar
QWidget* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// The item of @p combo whose data is @p value, chosen without a signal
void choose(QComboBox* combo, const QString& value) {
    const int index = combo->findData(value);
    if (index >= 0 && index != combo->currentIndex()) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(index);
    }
}

}  // namespace

MindMapEditor::MindMapEditor(const core::WindowEditorContext& context, QWidget* parent)
    : QWidget(parent),
      m_book(context.book),
      m_elementId(context.elementId),
      m_openSettings(context.openSettings),
      m_openElement(context.openElement),
      m_content(std::make_unique<MindMapContent>(&context.book)) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    m_document = new MindMapDocument(element != nullptr ? m_book.filePathOf(*element) : QString(),
                                     this);
    m_document->setShownText([this](const MindMapNode& node) {
        return m_content->titleOf(m_document->map(), node, titleOf(m_elementId));
    });
    QString error;
    if (!m_document->load(&error)) {
        core::Logger::getInstance().warn("MindMapEditor: {}", error.toStdString());
    }
    const MindMapLook look = MindMapLook::current();
    m_document->setNewFolded(look.newFolded);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_commands = new QWidget(this);
    buildCommands(m_commands);
    layout->addWidget(m_commands);
    auto* viewBar = new QWidget(this);
    buildViewBar(viewBar);
    layout->addWidget(viewBar);

    m_stack = new QStackedWidget(this);
    m_view = new MindMapView(*m_document, *m_content, &m_book, m_stack);
    m_view->setMapTitle(titleOf(m_elementId));
    m_list = new MindMapList(*m_document, m_stack);
    m_list->setElementTitle([this](const QString& id) { return titleOf(id); });
    m_stack->addWidget(m_view);
    m_stack->addWidget(m_list);
    layout->addWidget(m_stack, 1);

    // The line under the map: what was done, else what the selection is
    m_info = new QWidget(this);
    m_info->setObjectName(QStringLiteral("mindMapInfoLine"));
    auto* infoRow = new QHBoxLayout(m_info);
    infoRow->setContentsMargins(BAR_MARGIN * 2, BAR_MARGIN, BAR_MARGIN, BAR_MARGIN);
    m_infoText = new QLabel(m_info);
    m_infoText->setWordWrap(true);
    m_infoText->setTextFormat(Qt::PlainText);
    infoRow->addWidget(m_infoText, 1);
    //: The button of the line under a mind map. In Polish: Cofnij
    m_infoUndo = new QPushButton(tr("Undo"), m_info);
    m_infoUndo->hide();
    infoRow->addWidget(m_infoUndo);
    layout->addWidget(m_info);
    m_infoTimer = new QTimer(this);
    m_infoTimer->setSingleShot(true);
    m_infoTimer->setInterval(MESSAGE_TIME);
    connect(m_infoTimer, &QTimer::timeout, this, &MindMapEditor::showHint);
    connect(m_infoUndo, &QPushButton::clicked, this, [this]() {
        const QString what = m_document->undoText();
        m_document->undo();
        if (what.isEmpty()) {
            showHint();
        } else {
            //: In the line under a mind map; %1 is what was undone: "dodanie węzła". In Polish:
            //: Cofnięto: %1.
            tell(tr("Undone: %1.").arg(what), false);
        }
        if (m_stack->currentWidget() == m_view) {
            m_view->setFocus(Qt::OtherFocusReason);
        }
    });

    connect(m_view, &MindMapView::told, this, &MindMapEditor::tell);
    connect(m_list, &MindMapList::told, this, &MindMapEditor::tell);
    connect(m_view, &MindMapView::nodeWindowRequested, this, &MindMapEditor::openNodeWindow);
    connect(m_list, &MindMapList::noteRequested, this, &MindMapEditor::openNodeWindow);
    connect(m_view, &MindMapView::linkRequested, this, &MindMapEditor::chooseLink);
    connect(m_list, &MindMapList::linkRequested, this, &MindMapEditor::chooseLink);
    connect(m_view, &MindMapView::openElementRequested, this, &MindMapEditor::openElement);
    connect(m_list, &MindMapList::openElementRequested, this, &MindMapEditor::openElement);
    connect(m_view, &MindMapView::zoomChanged, this, &MindMapEditor::updateBar);
    connect(m_view, &MindMapView::selectionChanged, this, &MindMapEditor::updateBar);
    connect(m_document, &MindMapDocument::changed, this, &MindMapEditor::updateBar);
    connect(m_document, &MindMapDocument::selectionChanged, this, &MindMapEditor::updateBar);
    connect(m_document, &MindMapDocument::saveError, this, [this](const QString& why) {
        //: In Polish: Nie udało się zapisać mapy: %1
        tell(tr("The map could not be saved: %1").arg(why), false);
    });
    connect(m_document, &MindMapDocument::bookStepFailed, this, [this](const QString& what) {
        //: In the bar under a mind map, when Undo or Redo cannot change the cards back. In
        //: Polish: Nie udało się zmienić kart: %1.
        tell(tr("The cards could not be changed: %1.").arg(what), false);
    });

    // What the nodes show of the book: read again after its changes, once
    m_bookTimer = new QTimer(this);
    m_bookTimer->setSingleShot(true);
    m_bookTimer->setInterval(0);
    connect(m_bookTimer, &QTimer::timeout, this, &MindMapEditor::bookChanged);
    for (const auto signal :
         {&core::ProjectManager::linksChanged, &core::ProjectManager::projectChanged}) {
        connect(&m_book, signal, m_bookTimer, qOverload<>(&QTimer::start));
    }
    for (const auto signal :
         {&core::ProjectManager::elementRenamed, &core::ProjectManager::cardChanged,
          &core::ProjectManager::cardAdded, &core::ProjectManager::cardFieldsChanged}) {
        connect(&m_book, signal, m_bookTimer, [this]() { m_bookTimer->start(); });
    }
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [guard = QPointer<MindMapEditor>(this)](const std::string& key) {
            if (key.rfind("mindMaps.", 0) == 0 || key.rfind("editor.fontFamily", 0) == 0) {
                QMetaObject::invokeMethod(
                    guard.data(),
                    [guard]() {
                        if (guard) {
                            guard->applyLook();
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &MindMapEditor::applyLook);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            &MindMapEditor::applyLook);

    syncRelationMap();

    // The view it opens on
    const OpenView open = MindMapLook::openView();
    const MindMapMode mode = open == OpenView::Map    ? MindMapMode::Map
                             : open == OpenView::List ? MindMapMode::List
                                                      : m_document->map().mode();
    showMode(mode, false);
    updateBar();
}

MindMapEditor::~MindMapEditor() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

void MindMapEditor::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The texts and the timelines may have changed while the tab was away
    bookChanged();
}

void MindMapEditor::bookChanged() {
    m_content->forget();
    m_view->setMapTitle(titleOf(m_elementId));
    m_view->refresh();
    m_list->refresh();
    updateBar();
}

// =============================================================================
// The bar of the commands
// =============================================================================

QToolButton* MindMapEditor::addCommand(QWidget* bar, const char* icon, const QString& name,
                                       const QString& keys, const std::function<void()>& run) {
    auto* button = new QToolButton(bar);
    button->setAutoRaise(true);
    button->setIcon(core::ArtProvider::getInstance().getIcon(QString::fromLatin1(icon)));
    button->setToolTip(keys.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, keys));
    button->setAccessibleName(name);
    button->setFocusPolicy(Qt::TabFocus);
    if (run) {
        connect(button, &QToolButton::clicked, this, [this, run]() {
            run();
            if (m_stack->currentWidget() == m_view && !m_view->isEditing()) {
                m_view->setFocus(Qt::OtherFocusReason);
            }
        });
    }
    bar->layout()->addWidget(button);
    return button;
}

void MindMapEditor::buildCommands(QWidget* bar) {
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, 0);
    row->setSpacing(BAR_SPACING);

    //: A command over a mind map. In Polish: Dodaj węzeł…
    m_add = addCommand(bar, MindMapIcons::ADD, tr("Add Node…"), QStringLiteral("Shift+A"), {});
    m_add->setText(tr("Add Node…"));
    m_add->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(m_add, &QToolButton::clicked, this, [this]() {
        showMode(MindMapMode::Map, true);
        m_view->openAddNode(std::nullopt, QString(),
                            m_add->mapToGlobal(QPoint(0, m_add->height() + 2)));
    });
    //: In Polish: Dodaj gałąź
    m_branch = addCommand(bar, MindMapIcons::BRANCH, tr("Add Branch"), QStringLiteral("Tab"),
                          [this]() { m_view->addBranch(); });
    m_branch->setText(tr("Add Branch"));
    m_branch->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    //: In Polish: Dodaj poniżej
    m_below = addCommand(bar, MindMapIcons::BELOW, tr("Add Below"),
                         //: The keys of Add Below, Add Above. In Polish: Enter; powyżej:
                         //: Shift+Enter
                         tr("Enter; above: Shift+Enter"), [this]() { m_view->addBelow(false); });
    m_below->setText(tr("Add Below"));
    m_below->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    row->addWidget(separator(bar));
    //: In Polish: Edytuj nazwę
    m_rename = addCommand(bar, MindMapIcons::RENAME, tr("Edit Name"), QStringLiteral("F2"),
                          [this]() {
                              if (!m_document->selected().isEmpty()) {
                                  m_view->startEditing(m_document->selected());
                              }
                          });
    //: In Polish: Zwiń / rozwiń
    m_fold = addCommand(bar, MindMapIcons::FOLD, tr("Fold / Unfold"), QStringLiteral("H"),
                        [this]() { m_view->foldSelected(); });
    //: In Polish: Ramka
    m_frame = addCommand(bar, MindMapIcons::FRAME, tr("Frame"), QStringLiteral("Ctrl+J"),
                         [this]() { m_view->frameSelected(); });
    //: In Polish: Uporządkuj
    m_arrange = addCommand(bar, MindMapIcons::ARRANGE, tr("Arrange"), QString(), [this]() {
        if (m_document->map().relationMap() && m_view->hasFamilies()) {
            m_view->arrangeGenerations();
        } else {
            m_view->arrange(MindMapLayout::Free);
        }
    });
    m_arrange->setText(tr("Arrange"));
    m_arrange->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_arrange->setPopupMode(QToolButton::MenuButtonPopup);
    auto* layouts = new QMenu(m_arrange);
    connect(layouts, &QMenu::aboutToShow, this, [this, layouts]() {
        layouts->clear();
        const MindMapLayout now = m_document->map().layout();
        const auto add = [this, layouts, now](const QString& text, MindMapLayout layout) {
            QAction* action = layouts->addAction(text);
            action->setCheckable(true);
            action->setChecked(now == layout);
            connect(action, &QAction::triggered, this, [this, layout]() {
                m_view->arrange(layout);
                m_view->setFocus(Qt::OtherFocusReason);
            });
        };
        //: A layout of a mind map. In Polish: Na obie strony
        add(tr("On Both Sides"), MindMapLayout::BothSides);
        //: In Polish: W prawo
        add(tr("To the Right"), MindMapLayout::Right);
        //: In Polish: W dół
        add(tr("Down"), MindMapLayout::Down);
        if (m_view->hasFamilies()) {
            layouts->addSeparator();
            //: In Polish: Rodzina w pokoleniach
            connect(layouts->addAction(tr("Family in Generations")), &QAction::triggered, this,
                    [this]() {
                        m_view->arrangeGenerations();
                        m_view->setFocus(Qt::OtherFocusReason);
                    });
        }
    });
    m_arrange->setMenu(layouts);
    //: In Polish: Usuń
    m_remove = addCommand(bar, MindMapIcons::REMOVE, tr("Delete"), QStringLiteral("Delete"),
                          [this]() { m_view->removeSelected(); });
    row->addWidget(separator(bar));
    //: In Polish: Cofnij
    m_undo = addCommand(bar, MindMapIcons::UNDO, tr("Undo"), QStringLiteral("Ctrl+Z"), [this]() {
        const QString what = m_document->undoText();
        m_document->undo();
        if (!what.isEmpty()) {
            tell(tr("Undone: %1.").arg(what), false);
        }
    });
    //: In Polish: Ponów
    m_redo = addCommand(bar, MindMapIcons::REDO, tr("Redo"), QStringLiteral("Ctrl+Y"), [this]() {
        const QString what = m_document->redoText();
        m_document->redo();
        if (!what.isEmpty()) {
            //: In Polish: Ponowiono: %1.
            tell(tr("Redone: %1.").arg(what), false);
        }
    });
    row->addStretch(1);
    //: In Polish: Pomniejsz
    addCommand(bar, MindMapIcons::ZOOM_OUT, tr("Zoom Out"), QStringLiteral("Ctrl+-"),
               [this]() { m_view->zoomBy(-1); });
    m_zoomLabel = new QLabel(bar);
    m_zoomLabel->setMinimumWidth(
        m_zoomLabel->fontMetrics().horizontalAdvance(QStringLiteral("400%")));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    //: The zoom of a mind map, in the bar. In Polish: Powiększenie (Ctrl+0: 100%)
    m_zoomLabel->setToolTip(tr("Zoom (Ctrl+0: 100%)"));
    row->addWidget(m_zoomLabel);
    //: In Polish: Powiększ
    addCommand(bar, MindMapIcons::ZOOM_IN, tr("Zoom In"), QStringLiteral("Ctrl++"),
               [this]() { m_view->zoomBy(1); });
    //: In Polish: Cała mapa
    addCommand(bar, MindMapIcons::FIT, tr("Whole Map"), QStringLiteral("Home"),
               [this]() { m_view->showAll(); });
}

// =============================================================================
// The bar of the view
// =============================================================================

void MindMapEditor::buildViewBar(QWidget* bar) {
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(BAR_MARGIN, BAR_MARGIN / 2, BAR_MARGIN, BAR_MARGIN / 2);
    row->setSpacing(BAR_SPACING);

    // View: the map or the list
    m_mapButton = new QToolButton(bar);
    //: The view of a mind map: its nodes and lines. In Polish: Mapa
    m_mapButton->setText(tr("Map"));
    m_listButton = new QToolButton(bar);
    //: The view of a mind map: an outline. In Polish: Lista
    m_listButton->setText(tr("List"));
    auto* views = new QButtonGroup(bar);
    for (QToolButton* button : {m_mapButton, m_listButton}) {
        button->setCheckable(true);
        button->setAutoRaise(false);
        views->addButton(button);
    }
    views->setExclusive(true);
    //: In the bar of a mind map. In Polish: Widok:
    row->addWidget(barLabel(tr("View:"), m_mapButton, bar));
    row->addWidget(m_mapButton);
    row->addWidget(m_listButton);
    connect(m_mapButton, &QToolButton::clicked, this,
            [this]() { showMode(MindMapMode::Map, true); });
    connect(m_listButton, &QToolButton::clicked, this,
            [this]() { showMode(MindMapMode::List, true); });
    row->addSpacing(GROUP_SPACING);

    // The background and the lines: the settings of all maps
    m_background = new QComboBox(bar);
    //: The background of the mind maps. In Polish: kratka
    m_background->addItem(tr("grid"), QStringLiteral("grid"));
    //: In Polish: kropki
    m_background->addItem(tr("dots"), QStringLiteral("dots"));
    //: In Polish: gładkie
    m_background->addItem(tr("plain"), QStringLiteral("theme"));
    //: In Polish: własny kolor
    m_background->addItem(tr("own color"), QStringLiteral("own"));
    //: In the bar of a mind map. In Polish: Tło:
    row->addWidget(barLabel(tr("Background:"), m_background, bar));
    row->addWidget(m_background);
    connect(m_background, &QComboBox::activated, this, [this]() {
        core::SettingsManager::getInstance().set<std::string>(
            MindMapSettings::BACKGROUND, m_background->currentData().toString().toStdString());
    });
    row->addSpacing(GROUP_SPACING);
    m_lines = new QComboBox(bar);
    //: The lines of the mind maps: their branches and other links. In Polish: krzywe
    m_lines->addItem(tr("curved"), QStringLiteral("curved"));
    //: In Polish: proste
    m_lines->addItem(tr("straight"), QStringLiteral("straight"));
    //: In Polish: łamane
    m_lines->addItem(tr("elbow"), QStringLiteral("elbow"));
    //: In the bar of a mind map. In Polish: Linie:
    row->addWidget(barLabel(tr("Lines:"), m_lines, bar));
    row->addWidget(m_lines);
    connect(m_lines, &QComboBox::activated, this, [this]() {
        core::SettingsManager::getInstance().set<std::string>(
            MindMapSettings::LINES, m_lines->currentData().toString().toStdString());
    });
    row->addSpacing(GROUP_SPACING);

    // Highlight
    m_highlight = new QComboBox(bar);
    //: Highlight in a mind map: nothing. In Polish: nic
    m_highlight->addItem(tr("nothing"), QString());
    //: Highlight in a mind map: the nodes with this marker. In Polish: pytania
    m_highlight->addItem(QStringLiteral("? ") + tr("questions"),
                         QString::fromLatin1(MindMapNode::QUESTION));
    //: Highlight in a mind map. In Polish: ważne
    m_highlight->addItem(QStringLiteral("! ") + tr("important"),
                         QString::fromLatin1(MindMapNode::IMPORTANT));
    //: Highlight in a mind map. In Polish: pomysły
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::IDEA)) + QLatin1Char(' ') +
                             tr("ideas"),
                         QString::fromLatin1(MindMapNode::IDEA));
    //: Highlight in a mind map. In Polish: zrobione
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::DONE)) + QLatin1Char(' ') +
                             tr("done"),
                         QString::fromLatin1(MindMapNode::DONE));
    //: Highlight in a mind map. In Polish: do sprawdzenia
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::CHECK)) + QLatin1Char(' ') +
                             tr("to check"),
                         QString::fromLatin1(MindMapNode::CHECK));
    //: Highlight in a mind map. In Polish: o elementach książki
    m_highlight->addItem(tr("about elements of the book"), QStringLiteral("link"));
    //: Highlight in a mind map. In Polish: z notatką
    m_highlight->addItem(tr("with a note"), QStringLiteral("note"));
    //: In the bar of a mind map. In Polish: Wyróżnij:
    row->addWidget(barLabel(tr("Highlight:"), m_highlight, bar));
    row->addWidget(m_highlight);
    connect(m_highlight, &QComboBox::activated, this, [this]() {
        const QString filter = m_highlight->currentData().toString();
        m_view->setHighlight(filter);
        m_list->setHighlight(filter);
    });

    row->addStretch(1);
    m_what = new QLabel(bar);
    row->addWidget(m_what);

    // The gear: Settings > Mind Maps
    m_gear = new QToolButton(bar);
    m_gear->setAutoRaise(true);
    m_gear->setIcon(
        core::ArtProvider::getInstance().getIcon(QString::fromLatin1(MindMapIcons::SETTINGS)));
    //: The gear of a mind map. In Polish: Ustawienia map myśli
    m_gear->setToolTip(tr("Mind Map Settings"));
    m_gear->setAccessibleName(m_gear->toolTip());
    row->addWidget(m_gear);
    connect(m_gear, &QToolButton::clicked, this, [this]() {
        if (m_openSettings) {
            m_openSettings();
        }
    });
}

void MindMapEditor::showMode(MindMapMode mode, bool keep) {
    const bool list = mode == MindMapMode::List;
    m_stack->setCurrentWidget(list ? static_cast<QWidget*>(m_list) : m_view);
    m_mapButton->setChecked(!list);
    m_listButton->setChecked(list);
    m_commands->setEnabled(!list);
    m_background->setEnabled(!list);
    m_lines->setEnabled(!list);
    setFocusProxy(list ? static_cast<QWidget*>(m_list) : m_view);
    if (keep && m_document->map().mode() != mode) {
        m_document->changeView([mode](MindMap& map) { map.setMode(mode); });
    }
    if (keep) {
        m_stack->currentWidget()->setFocus(Qt::OtherFocusReason);
    }
    showHint();
}

void MindMapEditor::updateBar() {
    const MindMap& map = m_document->map();
    m_zoomLabel->setText(QStringLiteral("%1%").arg(std::lround(m_view->zoom() * 100.0)));
    const bool selected = !m_document->selection().isEmpty();
    const bool one = m_document->selection().size() == 1;
    const bool something = selected || m_view->selectedLink() || !m_view->selectedFrame().isEmpty();
    m_branch->setEnabled(true);
    m_below->setEnabled(true);
    m_rename->setEnabled(
        one || !m_view->selectedFrame().isEmpty() ||
        (m_view->selectedLink() && m_view->selectedLink()->type == LinkLine::Type::Connection));
    m_fold->setEnabled(selected);
    m_frame->setEnabled(selected);
    m_remove->setEnabled(something &&
                         !(one && m_document->selected() == map.root() && !m_view->selectedLink() &&
                           m_view->selectedFrame().isEmpty()));
    m_undo->setEnabled(m_document->canUndo());
    m_undo->setToolTip(m_document->canUndo()
                           //: The tooltip of Undo; %1 is what it undoes. In Polish: Cofnij: %1
                           //: (Ctrl+Z)
                           ? tr("Undo: %1 (Ctrl+Z)").arg(m_document->undoText())
                           : tr("Undo") + QStringLiteral(" (Ctrl+Z)"));
    m_redo->setEnabled(m_document->canRedo());
    m_redo->setToolTip(m_document->canRedo()
                           //: In Polish: Ponów: %1 (Ctrl+Y)
                           ? tr("Redo: %1 (Ctrl+Y)").arg(m_document->redoText())
                           : tr("Redo") + QStringLiteral(" (Ctrl+Y)"));
    const MindMapLook& look = m_view->look();
    choose(m_background, look.background == MapBackground::Grid   ? QStringLiteral("grid")
                         : look.background == MapBackground::Dots ? QStringLiteral("dots")
                         : look.background == MapBackground::Own  ? QStringLiteral("own")
                                                                  : QStringLiteral("theme"));
    choose(m_lines, look.lines == LineStyle::Straight ? QStringLiteral("straight")
                    : look.lines == LineStyle::Elbow  ? QStringLiteral("elbow")
                                                      : QStringLiteral("curved"));
    // The nodes drawn: a map of relations does not draw its main topic
    const auto nodes = static_cast<int>(m_view->scene() != nullptr ? m_view->scene()->nodes().size()
                                                                   : map.size());
    const auto links = static_cast<int>(m_view->scene() != nullptr ? m_view->scene()->links().size()
                                                                   : 0);
    const QString kind = map.relationMap()
                             //: What a map is, in its bar. In Polish: Mapa relacji z kart
                             //: Warsztatu
                             ? tr("Relation map of the Workshop cards")
                             //: In Polish: Mapa myśli
                             : tr("Mind map");
    //: The numbers in the bar of a mind map. In Polish: %n węzeł / węzły / węzłów
    const QString nodeText = tr("%n node(s)", nullptr, nodes);
    //: In Polish: %n połączenie / połączenia / połączeń
    const QString linkText = tr("%n link(s)", nullptr, links);
    m_what->setText(kind + QStringLiteral(" · ") + nodeText + QStringLiteral(" · ") + linkText);
    showHint();
}

void MindMapEditor::applyLook() {
    const MindMapLook look = MindMapLook::current();
    m_document->setNewFolded(look.newFolded);
    m_view->setLook(look);
    m_list->setLook(look);
    updateBar();
}

void MindMapEditor::tell(const QString& message, bool undoable) {
    if (message.isEmpty()) {
        return;
    }
    m_infoText->setText(message);
    m_infoUndo->setVisible(undoable && m_document->canUndo());
    m_infoTimer->start();
}

void MindMapEditor::showHint() {
    if (m_infoTimer->isActive()) {
        return;  // A message is shown
    }
    m_infoUndo->hide();
    m_infoText->setText(m_stack->currentWidget() == m_list
                            //: The line under the list of a mind map. In Polish: Enter dodaje
                            //: węzeł poniżej, Tab przenosi węzeł poziom niżej, Shift+Tab – poziom
                            //: wyżej, Ctrl+strzałki – wśród rodzeństwa.
                            ? tr("Enter adds a node below, Tab puts a node a level down, "
                                 "Shift+Tab a level up, Ctrl+arrows among its siblings.")
                            : m_view->hint());
}

QString MindMapEditor::infoText() const {
    return m_infoText->text();
}

// =============================================================================
// The window of a node, the element of the book, a map of relations
// =============================================================================

QString MindMapEditor::titleOf(const QString& elementId) const {
    const core::ProjectElement* element = m_book.findElement(elementId);
    return element != nullptr ? element->title : QString();
}

void MindMapEditor::openNodeWindow(const QString& id) {
    const MindMapNode* node = m_document->map().node(id);
    if (node == nullptr) {
        return;
    }
    MindMapNodeDialog dialog(m_document->map(), *node, *m_content, titleOf(m_elementId), &m_book,
                             this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const MindMapNode changed = dialog.node();
    if (const MindMapNode* now = m_document->map().node(id); now != nullptr && changed != *now) {
        //: What Undo undoes in a mind map. In Polish: zmiana węzła
        if (m_document->setNode(changed, tr("changing a node"))) {
            //: In the bar under a mind map. In Polish: Zmieniono węzeł „%1”.
            tell(tr("The node “%1” is changed.").arg(m_document->shownText(changed)), true);
        }
    }
}

void MindMapEditor::chooseLink(const QString& id) {
    const MindMapNode* node = m_document->map().node(id);
    if (node == nullptr) {
        return;
    }
    gui::dialogs::ElementChoiceDialog dialog(
        m_book,
        //: The window that chooses what a node of a mind map is about. In Polish: Element
        //: książki
        tr("Element of the Book"),
        //: %1: the text of the node. In Polish: O czym jest „%1”? Węzeł bez własnego tekstu
        //: nosi tytuł elementu, a Ctrl+Enter go otwiera.
        tr("What is “%1” about? A node without its own text shows the title of the element, "
           "and Ctrl+Enter opens it.")
            .arg(m_document->shownText(*node)),
        node->link, {m_elementId}, this);
    if (dialog.exec() == QDialog::Accepted && !dialog.chosen().isEmpty()) {
        m_document->setLink(id, dialog.chosen());
    }
}

void MindMapEditor::openElement(const QString& elementId) {
    if (m_book.findElement(elementId) == nullptr) {
        //: In Polish: Tego elementu nie ma już w książce.
        tell(tr("This element is not in the book any more."), false);
        return;
    }
    if (m_openElement) {
        m_openElement(elementId);
    }
}

void MindMapEditor::syncRelationMap() {
    if (m_synced || !m_document->map().relationMap()) {
        return;
    }
    m_synced = true;
    // The cards that have new relations come to the map when it opens
    const QStringList missing = missingCards(m_book, m_document->map(),
                                             *m_document->map().relationMap());
    if (missing.isEmpty()) {
        return;
    }
    m_document->changeView([&missing](MindMap& map) { addCardNodes(map, missing); });
    m_view->refresh();
    //: In the bar under a map of relations, when it opens. In Polish: Doszły karty z nowymi
    //: relacjami: %1.
    tell(tr("Cards with new relations came to the map: %1.").arg(missing.size()), false);
}

}  // namespace kalahari::mindmap
