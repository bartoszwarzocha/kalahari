/// @file paragraph_marks_coordinator.cpp
/// @brief The paperclips beside the paragraphs of the chapters that materials are linked with

#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QPointer>
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

}  // namespace

ParagraphMarksCoordinator::ParagraphMarksCoordinator(QTabWidget* centralTabs,
                                                     MaterialShower showMaterial, QObject* parent)
    : QObject(parent)
    , m_centralTabs(centralTabs)
    , m_showMaterial(std::move(showMaterial))
{
    // A tab that comes gets its text before the event loop runs: its paperclips after it
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0);
    connect(m_refreshTimer, &QTimer::timeout, this, &ParagraphMarksCoordinator::refresh);
    connect(m_centralTabs, &QTabWidget::currentChanged, m_refreshTimer,
            qOverload<>(&QTimer::start));

    auto& projects = core::ProjectManager::getInstance();
    connect(&projects, &core::ProjectManager::linksChanged, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&projects, &core::ProjectManager::projectOpened, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&projects, &core::ProjectManager::projectClosed, m_refreshTimer,
            qOverload<>(&QTimer::start));
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            m_refreshTimer, qOverload<>(&QTimer::start));

    // The setting may change on another thread: the paperclips change on this one
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [timer = QPointer<QTimer>(m_refreshTimer)](const std::string& key) {
            if (key == core::LinkRoles::PARAGRAPHS_SETTING && timer != nullptr) {
                QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection);
            }
        });
}

ParagraphMarksCoordinator::~ParagraphMarksCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    disconnect(&core::ProjectManager::getInstance(), nullptr, m_refreshTimer, nullptr);
}

QList<editor::ParagraphMark> ParagraphMarksCoordinator::marksOf(const QString& elementId,
                                                                const editor::BookEditor& editor) {
    QList<editor::ParagraphMark> marks;
    const QTextDocument* document = editor.textDocument();
    if (elementId.isEmpty() || document == nullptr || !core::LinkRoles::paragraphsShown()) {
        return marks;
    }
    QStringList paragraphs;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        paragraphs << block.text();
    }
    for (const core::ProjectElement* material :
         core::ProjectManager::getInstance().materialsLinkedTo(elementId)) {
        const core::ElementLink* link = material->linkTo(elementId);
        const int number = link != nullptr ? core::LinkRoles::paragraphOf(paragraphs, *link) : 0;
        if (number <= 0) {
            continue;
        }
        const auto same = std::find_if(marks.begin(), marks.end(),
                                       [number](const editor::ParagraphMark& mark) {
                                           return mark.paragraph == number - 1;
                                       });
        if (same != marks.end()) {
            same->ids << material->id;
            same->titles << material->title;
        } else {
            marks.append(editor::ParagraphMark{number - 1, {material->id}, {material->title}});
        }
    }
    std::sort(marks.begin(), marks.end(),
              [](const editor::ParagraphMark& a, const editor::ParagraphMark& b) {
                  return a.paragraph < b.paragraph;
              });
    return marks;
}

void ParagraphMarksCoordinator::refresh() {
    m_refreshTimer->stop();
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        if (auto* panel = qobject_cast<EditorPanel*>(m_centralTabs->widget(i))) {
            refreshPanel(panel);
        }
    }
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
                [this](const QString& id) {
                    if (m_showMaterial) {
                        m_showMaterial(id);
                    }
                });
    }
    // A chapter of the book: a file of its own has no element
    const QString elementId = panel->property("elementId").toString();
    editor->setParagraphMarks(
        marksOf(elementId, *editor),
        core::ArtProvider::getInstance().getIcon(QStringLiteral("structure.materials")));
}

}  // namespace kalahari::gui
