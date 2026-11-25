// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file paragraph_marks_coordinator.cpp
/// @brief The paperclips beside the paragraphs of the chapters that materials are linked with,
///        the signs of the cards linked with them, and the scissors beside those clippings
///        were cut from

#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QPointer>
#include <QString>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

/// @brief The property of an editor whose signals the coordinator has
constexpr const char* CONNECTED_PROPERTY = "paragraphMarksConnected";

/// Add @p element, linked with paragraph @p number (from 1), to the mark of @p icon of that
/// paragraph in @p marks, or a new mark
void addToMark(QList<editor::ParagraphMark>& marks, int number, const QString& icon,
               const core::ProjectElement& element) {
    const auto same = std::find_if(marks.begin(), marks.end(),
                                   [number, &icon](const editor::ParagraphMark& mark) {
                                       return mark.paragraph == number - 1 && mark.icon == icon;
                                   });
    if (same != marks.end()) {
        same->ids << element.id;
        same->titles << element.title;
        return;
    }
    editor::ParagraphMark mark{number - 1, {element.id}, {element.title}};
    mark.icon = icon;
    marks.append(mark);
}

/// What is linked with element @p elementId of @p book, with the icon of its mark: the
/// materials first, with the paperclip (none), next to the text, then the cards, with the signs
/// of their kinds, unless the cards have no signs
QList<std::pair<const core::ProjectElement*, QString>> linkedWithMarks(
    const core::ProjectManager& book, const QString& elementId) {
    QList<std::pair<const core::ProjectElement*, QString>> linked;
    for (const core::ProjectElement* material : book.materialsLinkedTo(elementId)) {
        linked.append({material, QString()});
    }
    if (!ParagraphMarksCoordinator::cardMarksShown()) {
        return linked;
    }
    for (const core::ProjectElement* card : book.cardsLinkedTo(elementId)) {
        linked.append({card, NavigatorPanel::iconIdOf(book.bookTypes(), *card)});
    }
    return linked;
}

}  // namespace

ParagraphMarksCoordinator::ParagraphMarksCoordinator(core::ProjectManager& book,
                                                     QTabWidget* centralTabs,
                                                     MaterialShower showMaterial,
                                                     CardOpener openCard, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_showMaterial(std::move(showMaterial)),
      m_openCard(std::move(openCard)),
      m_book(book) {
    // A tab that comes gets its text before the event loop runs: its paperclips after it
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &ParagraphMarksCoordinator::refresh);
    connect(m_centralTabs, &QTabWidget::currentChanged, m_refreshTimer,
            qOverload<>(&QTimer::start));

    auto& projects = m_book;
    connect(&projects, &core::ProjectManager::linksChanged, m_refreshTimer,
            qOverload<>(&QTimer::start));
    // The titles of the marks
    connect(&projects, &core::ProjectManager::elementRenamed, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&projects, &core::ProjectManager::projectOpened, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&projects, &core::ProjectManager::projectClosed, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            m_refreshTimer, qOverload<>(&QTimer::start));

    // The settings may change on another thread: the marks change on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [timer = QPointer<QTimer>(m_refreshTimer)](const std::string& key) {
            if ((key == core::LinkRoles::PARAGRAPHS_SETTING ||
                 key == core::Clippings::MARKS_SETTING || key == CARD_MARKS_SETTING) &&
                timer != nullptr) {
                QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection);
            }
        });
}

ParagraphMarksCoordinator::~ParagraphMarksCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    disconnect(&m_book, nullptr, m_refreshTimer, nullptr);
}

bool ParagraphMarksCoordinator::cardMarksShown() {
    return core::SettingsManager::getInstance().get<bool>(CARD_MARKS_SETTING, true);
}

QList<editor::ParagraphMark> ParagraphMarksCoordinator::marksOf(const core::ProjectManager& book,
                                                                const QString& elementId,
                                                                const editor::BookEditor& editor) {
    QList<editor::ParagraphMark> marks;
    const QTextDocument* document = editor.textDocument();
    if (elementId.isEmpty() || document == nullptr || !core::LinkRoles::paragraphsShown()) {
        return marks;
    }
    const QStringList paragraphs = paragraphsOf(*document);
    for (const auto& [element, icon] : linkedWithMarks(book, elementId)) {
        const core::ElementLink* link = element->linkTo(elementId);
        const int number = link != nullptr ? core::LinkRoles::paragraphOf(paragraphs, *link) : 0;
        if (number > 0) {
            addToMark(marks, number, icon, *element);
        }
    }
    std::stable_sort(marks.begin(), marks.end(),
                     [](const editor::ParagraphMark& a, const editor::ParagraphMark& b) {
                         return a.paragraph < b.paragraph;
                     });
    return marks;
}

QStringList ParagraphMarksCoordinator::paragraphsOf(const QTextDocument& document) {
    QStringList paragraphs;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        paragraphs << block.text();
    }
    return paragraphs;
}

void ParagraphMarksCoordinator::refresh() {
    m_refreshTimer->stop();
    // The texts of the book window's tabs and of the windows of their own
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page)) {
            refreshPanel(panel);
        }
    }
}

void ParagraphMarksCoordinator::refreshSoon() {
    m_refreshTimer->start();
}

void ParagraphMarksCoordinator::refreshPanel(EditorPanel* panel) {
    editor::BookEditor* editor = panel->getBookEditor();
    if (editor == nullptr) {
        return;
    }
    // A text loaded again has no paperclips; a paperclip chosen shows its material
    if (!editor->property(CONNECTED_PROPERTY).toBool()) {
        editor->setProperty(CONNECTED_PROPERTY, true);
        connect(editor, &editor::BookEditor::documentChanged, this,
                [this, panel = QPointer<EditorPanel>(panel)]() {
                    if (panel != nullptr) {
                        refreshPanel(panel);
                    }
                });
        connect(editor, &editor::BookEditor::paragraphMarkClicked, this,
                &ParagraphMarksCoordinator::open);
        connect(editor, &editor::BookEditor::clippingRestoreRequested, this,
                &ParagraphMarksCoordinator::clippingRestoreRequested);
    }
    // A chapter of the book: a file of its own has no element
    const QString elementId = panel->property("elementId").toString();
    const QList<editor::ParagraphMark> marks = marksOf(m_book, elementId, *editor);
    auto& art = core::ArtProvider::getInstance();
    QHash<QString, QIcon> cardIcons;
    for (const editor::ParagraphMark& mark : marks) {
        if (!mark.icon.isEmpty() && !cardIcons.contains(mark.icon)) {
            cardIcons.insert(mark.icon, art.getIcon(mark.icon));
        }
    }
    editor->setParagraphMarks(marks, art.getIcon(QStringLiteral("structure.materials")),
                              editor::ParagraphMarkKind::Materials, cardIcons);
    editor->setParagraphMarks(clippingMarksOf(m_book, elementId, *editor),
                              art.getIcon(QStringLiteral("edit.cut")),
                              editor::ParagraphMarkKind::Clippings);
}

void ParagraphMarksCoordinator::open(const QString& id) const {
    const core::ProjectElement* element = m_book.findElement(id);
    if (element != nullptr && m_book.isCard(*element)) {
        if (m_openCard) {
            m_openCard(id);
        }
    } else if (m_showMaterial) {
        m_showMaterial(id);
    }
}

}  // namespace kalahari::gui
