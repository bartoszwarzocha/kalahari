/// @file paragraph_style_menu.cpp
/// @brief Implementation of ParagraphStyleMenu

#include "kalahari/gui/paragraph_style_menu.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/utils/access_keys.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

const std::array<StyleCommand, 7>& styleCommands() {
    static const std::array<StyleCommand, 7> commands = {{
        {"format.style.heading1", "heading_1"},
        {"format.style.heading2", "heading_2"},
        {"format.style.heading3", "heading_3"},
        {"format.style.body", editor::NORMAL_STYLE_ID},
        {"format.style.quote", "quote"},
        {"format.style.dialogue", "dialogue"},
        {"format.style.code", "code"},
    }};
    return commands;
}

namespace {

/// Whether a style has a fixed command
bool hasCommand(const QString& styleId) {
    const auto& commands = styleCommands();
    return std::any_of(commands.begin(), commands.end(), [&styleId](const StyleCommand& command) {
        return styleId == QLatin1String(command.styleId);
    });
}

}  // namespace

ParagraphStyleMenu::ParagraphStyleMenu(QMenu* menu, EditorInFront editorInFront,
                                       QObject* parent)
    : QObject(parent)
    , m_menu(menu)
    , m_editorInFront(std::move(editorInFront))
    , m_group(new QActionGroup(this)) {
    // One style at a time: the menu shows the items as choices of one
    m_group->setExclusive(true);
    CommandRegistry& registry = CommandRegistry::getInstance();
    for (const StyleCommand& command : styleCommands()) {
        if (QAction* action = registry.getAction(QString::fromLatin1(command.commandId))) {
            m_group->addAction(action);
        }
    }
    if (m_menu != nullptr) {
        connect(m_menu, &QMenu::aboutToShow, this, &ParagraphStyleMenu::rebuild);
    }
}

void ParagraphStyleMenu::rebuild() {
    for (QAction* action : std::as_const(m_bookStyles)) {
        m_group->removeAction(action);
        m_menu->removeAction(action);
        action->deleteLater();
    }
    m_bookStyles.clear();

    editor::BookEditor* text = m_editorInFront ? m_editorInFront() : nullptr;
    if (m_menu == nullptr || text == nullptr) {
        return;
    }

    QList<const editor::ParagraphStyle*> styles;
    QStringList names;
    for (const editor::ParagraphStyle& style : text->paragraphStyles().styles()) {
        if (!hasCommand(style.id)) {
            styles.append(&style);
            names.append(style.name.isEmpty() ? style.id : style.name);
        }
    }
    if (styles.isEmpty()) {
        return;
    }

    // After Code, before the line above Manage Styles...
    QAction* before = nullptr;
    const QList<QAction*> items = m_menu->actions();
    QAction* code = CommandRegistry::getInstance().getAction(
        QString::fromLatin1(styleCommands().back().commandId));
    const qsizetype codePlace = items.indexOf(code);
    if (codePlace >= 0 && codePlace + 1 < items.size()) {
        before = items.at(codePlace + 1);
    }

    QSet<QChar> used = accessKeysIn(m_menu);
    const QStringList texts = withFreeAccessKeys(names, used);
    const QString current = text->currentParagraphStyle();
    const bool readOnly = text->isReadOnly();
    for (qsizetype index = 0; index < styles.size(); ++index) {
        const QString styleId = styles.at(index)->id;
        auto* action = new QAction(texts.at(index), m_menu);
        action->setCheckable(true);
        action->setChecked(styleId == current);
        // A chapter of a book open read-only: greyed out, saying why
        action->setEnabled(!readOnly);
        if (readOnly) {
            action->setToolTip(readOnlyReason());
            action->setStatusTip(readOnlyReason());
        }
        m_group->addAction(action);
        connect(action, &QAction::triggered, this, [this, styleId]() {
            if (editor::BookEditor* inFront = m_editorInFront ? m_editorInFront() : nullptr) {
                inFront->applyParagraphStyle(styleId);
            }
        });
        m_menu->insertAction(before, action);
        m_bookStyles.append(action);
    }
}

}  // namespace kalahari::gui
