// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_editor.cpp
/// @brief The tab of a mind map

#include <kalahari/mindmap/mind_map_editor.h>

#include <kalahari/core/art_provider.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/gui/dialogs/element_choice_dialog.h>
#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/gui/widgets/info_bar.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_list.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_view.h>

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
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
constexpr int BAR_SPACING = 6;
constexpr int GROUP_SPACING = 14;
constexpr int NOTE_LINES = 8;

/// A label of the bar, its buddy @p field
QLabel* barLabel(const QString& text, QWidget* field, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

}  // namespace

MindMapEditor::MindMapEditor(const core::WindowEditorContext& context, QWidget* parent)
    : QWidget(parent),
      m_book(context.book),
      m_elementId(context.elementId),
      m_openSettings(context.openSettings),
      m_openElement(context.openElement) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    m_document = new MindMapDocument(element != nullptr ? m_book.filePathOf(*element) : QString(),
                                     this);
    m_document->setShownText([this](const MindMapNode& node) {
        if (!node.text.isEmpty()) {
            return node.text;
        }
        if (!node.link.isEmpty()) {
            const QString title = titleOf(node.link);
            //: A node about an element of the book that was deleted. In Polish: (element
            //: usunięty)
            return title.isEmpty() ? tr("(a deleted element)") : title;
        }
        return node.id == m_document->map().root() ? titleOf(m_elementId) : QString();
    });
    QString error;
    if (!m_document->load(&error)) {
        core::Logger::getInstance().warn("MindMapEditor: {}", error.toStdString());
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* bar = new QWidget(this);
    buildBar(bar);
    layout->addWidget(bar);

    m_stack = new QStackedWidget(this);
    m_view = new MindMapView(*m_document, m_stack);
    m_list = new MindMapList(*m_document, m_stack);
    m_list->setElementTitle([this](const QString& id) { return titleOf(id); });
    m_stack->addWidget(m_view);
    m_stack->addWidget(m_list);
    layout->addWidget(m_stack, 1);

    m_info = new gui::InfoBar(this);
    m_info->hide();
    layout->addWidget(m_info);
    m_infoTimer = new QTimer(this);
    m_infoTimer->setSingleShot(true);
    m_infoTimer->setInterval(MESSAGE_TIME);
    connect(m_infoTimer, &QTimer::timeout, m_info, &QWidget::hide);
    connect(m_info, &gui::InfoBar::dismissed, m_info, &QWidget::hide);
    connect(m_info, &gui::InfoBar::actionClicked, this, [this]() {
        m_info->hide();
        const QString what = m_document->undoText();
        m_document->undo();
        if (!what.isEmpty()) {
            tell(tr("Undone: %1.").arg(what), false);
        }
    });

    connect(m_view, &MindMapView::told, this, &MindMapEditor::tell);
    connect(m_list, &MindMapList::told, this, &MindMapEditor::tell);
    connect(m_view, &MindMapView::noteRequested, this, &MindMapEditor::editNote);
    connect(m_list, &MindMapList::noteRequested, this, &MindMapEditor::editNote);
    connect(m_view, &MindMapView::linkRequested, this, &MindMapEditor::chooseLink);
    connect(m_list, &MindMapList::linkRequested, this, &MindMapEditor::chooseLink);
    connect(m_view, &MindMapView::openElementRequested, this, &MindMapEditor::openElement);
    connect(m_list, &MindMapList::openElementRequested, this, &MindMapEditor::openElement);
    connect(m_view, &MindMapView::zoomChanged, this, &MindMapEditor::updateBar);
    connect(m_document, &MindMapDocument::changed, this, &MindMapEditor::updateBar);
    connect(m_document, &MindMapDocument::saveError, this, [this](const QString& why) {
        //: In Polish: Nie udało się zapisać mapy: %1
        tell(tr("The map could not be saved: %1").arg(why), false);
    });

    // The titles of the elements, the settings and the theme
    connect(&m_book, &core::ProjectManager::elementRenamed, this, [this]() {
        m_view->refresh();
        m_list->refresh();
    });
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

    // The view it opens on
    const OpenView open = MindMapLook::openView();
    const MindMapMode mode = open == OpenView::Map    ? MindMapMode::Map
                             : open == OpenView::List ? MindMapMode::List
                                                      : m_document->map().mode();
    showMode(mode, false);
    updateBar();
    setFocusProxy(mode == MindMapMode::List ? static_cast<QWidget*>(m_list) : m_view);
}

MindMapEditor::~MindMapEditor() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

// =============================================================================
// The bar above the map
// =============================================================================

void MindMapEditor::buildBar(QWidget* bar) {
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

    // Layout
    m_layout = new QComboBox(bar);
    //: A layout of a mind map. In Polish: Na obie strony
    m_layout->addItem(tr("Both sides"), MindMap::nameOf(MindMapLayout::BothSides));
    //: A layout of a mind map. In Polish: W prawo
    m_layout->addItem(tr("To the right"), MindMap::nameOf(MindMapLayout::Right));
    //: A layout of a mind map: a tree going down. In Polish: W dół
    m_layout->addItem(tr("Down"), MindMap::nameOf(MindMapLayout::Down));
    //: A layout of a mind map: where the writer puts the nodes. In Polish: Swobodny
    m_layout->addItem(tr("Free"), MindMap::nameOf(MindMapLayout::Free));
    //: In the bar of a mind map. In Polish: Układ:
    row->addWidget(barLabel(tr("Layout:"), m_layout, bar));
    row->addWidget(m_layout);
    connect(m_layout, &QComboBox::activated, this, [this]() {
        const auto layout = MindMap::layoutOf(m_layout->currentData().toString());
        if (layout && m_document->setLayout(*layout)) {
            //: In Polish: Układ mapy: %1.
            tell(tr("The layout of the map: %1.").arg(m_layout->currentText()), true);
        }
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
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::IDEA)) +
                             QLatin1Char(' ') + tr("ideas"),
                         QString::fromLatin1(MindMapNode::IDEA));
    //: Highlight in a mind map. In Polish: zrobione
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::DONE)) +
                             QLatin1Char(' ') + tr("done"),
                         QString::fromLatin1(MindMapNode::DONE));
    //: Highlight in a mind map. In Polish: do sprawdzenia
    m_highlight->addItem(markerSign(QString::fromLatin1(MindMapNode::CHECK)) +
                             QLatin1Char(' ') + tr("to check"),
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
    row->addSpacing(GROUP_SPACING);

    // Zoom, in the map only
    m_zoomBox = new QWidget(bar);
    auto* zoom = new QHBoxLayout(m_zoomBox);
    zoom->setContentsMargins(0, 0, 0, 0);
    zoom->setSpacing(2);
    auto* out = new QToolButton(m_zoomBox);
    out->setText(QStringLiteral("−"));
    //: In Polish: Pomniejsz (Ctrl+−)
    out->setToolTip(tr("Zoom Out (Ctrl+-)"));
    //: In Polish: Pomniejsz
    out->setAccessibleName(tr("Zoom Out"));
    m_zoomLabel = new QLabel(m_zoomBox);
    m_zoomLabel->setMinimumWidth(m_zoomLabel->fontMetrics().horizontalAdvance(
        QStringLiteral("400%")));
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    auto* in = new QToolButton(m_zoomBox);
    in->setText(QStringLiteral("+"));
    //: In Polish: Powiększ (Ctrl++)
    in->setToolTip(tr("Zoom In (Ctrl++)"));
    //: In Polish: Powiększ
    in->setAccessibleName(tr("Zoom In"));
    auto* reset = new QToolButton(m_zoomBox);
    //: In Polish: 100%
    reset->setText(tr("100%"));
    //: In Polish: Powiększenie 100% (Ctrl+0)
    reset->setToolTip(tr("Zoom 100% (Ctrl+0)"));
    zoom->addWidget(out);
    zoom->addWidget(m_zoomLabel);
    zoom->addWidget(in);
    zoom->addWidget(reset);
    row->addWidget(m_zoomBox);
    connect(out, &QToolButton::clicked, this, [this]() { m_view->zoomBy(-1); });
    connect(in, &QToolButton::clicked, this, [this]() { m_view->zoomBy(1); });
    connect(reset, &QToolButton::clicked, this, [this]() { m_view->resetZoom(); });

    row->addStretch(1);
    m_counts = new QLabel(bar);
    row->addWidget(m_counts);

    // The gear: Settings > Mind Maps
    m_gear = new QToolButton(bar);
    m_gear->setAutoRaise(true);
    m_gear->setIcon(core::ArtProvider::getInstance().getIcon(QStringLiteral("dock.settings")));
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
    m_zoomBox->setVisible(!list);
    setFocusProxy(list ? static_cast<QWidget*>(m_list) : m_view);
    if (keep && m_document->map().mode() != mode) {
        m_document->changeView([mode](MindMap& map) { map.setMode(mode); });
    }
    if (keep) {
        m_stack->currentWidget()->setFocus(Qt::OtherFocusReason);
    }
}

void MindMapEditor::updateBar() {
    const MindMap& map = m_document->map();
    const int index = m_layout->findData(MindMap::nameOf(map.layout()));
    if (index >= 0) {
        m_layout->setCurrentIndex(index);
    }
    m_zoomLabel->setText(QStringLiteral("%1%").arg(std::lround(map.zoom() * 100.0)));
    const auto nodes = static_cast<int>(map.size());
    const auto connections = static_cast<int>(map.connections().size());
    //: The numbers in the bar of a mind map. In Polish: %n węzeł / węzły / węzłów
    const QString nodeText = tr("%n node(s)", nullptr, nodes);
    //: In Polish: %n połączenie / połączenia / połączeń
    const QString connectionText = tr("%n connection(s)", nullptr, connections);
    m_counts->setText(nodeText + QStringLiteral(" · ") + connectionText);
}

void MindMapEditor::applyLook() {
    const MindMapLook look = MindMapLook::current();
    m_view->setLook(look);
    m_list->setLook(look);
}

void MindMapEditor::tell(const QString& message, bool undoable) {
    if (message.isEmpty()) {
        return;
    }
    m_info->setMessage(message);
    //: The button of the bar under a mind map. In Polish: Cofnij
    m_info->setActionText(undoable && m_document->canUndo() ? tr("Undo") : QString());
    m_info->show();
    m_infoTimer->start();
}

// =============================================================================
// The note, the element of the book
// =============================================================================

QString MindMapEditor::titleOf(const QString& elementId) const {
    const core::ProjectElement* element = m_book.findElement(elementId);
    return element != nullptr ? element->title : QString();
}

void MindMapEditor::editNote(const QString& id) {
    const MindMapNode* node = m_document->map().node(id);
    if (node == nullptr) {
        return;
    }
    gui::dialogs::KalahariDialog dialog(this);
    //: The window of the note of a node of a mind map. %1: the text of the node. In Polish:
    //: Notatka węzła / Notatka do „%1”. Widać ją w podpowiedzi i na liście.
    dialog.setHeading(tr("Note of the Node"),
                      tr("The note of “%1”. It shows in the tooltip and in the list.")
                          .arg(m_document->shownText(*node)));
    dialog.setHeadingIcon(QStringLiteral("insert.note"));
    auto* text = new QPlainTextEdit(&dialog);
    text->setPlainText(node->note);
    text->setTabChangesFocus(true);
    text->setMinimumHeight(text->fontMetrics().lineSpacing() * NOTE_LINES);
    //: In Polish: Notatka
    dialog.addField(tr("Note"), text);
    //: In Polish: Zapisz
    dialog.setAcceptText(tr("Save"));
    text->setFocus();
    if (dialog.exec() == QDialog::Accepted) {
        m_document->setNote(id, text->toPlainText().trimmed());
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

}  // namespace kalahari::mindmap
