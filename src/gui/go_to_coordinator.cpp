/// @file go_to_coordinator.cpp
/// @brief Edit > Go To...: the window of the places of the text

#include "kalahari/gui/go_to_coordinator.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/edit_dialogs.h"

#include <QApplication>
#include <QLineEdit>

#include <utility>

namespace kalahari::gui {

GoToCoordinator::GoToCoordinator(TextWithKeys textWithKeys, QWidget* window, QObject* parent)
    : QObject(parent),
      m_textWithKeys(std::move(textWithKeys)),
      m_window(window)
{
    // The text with the keys, also when the keys go to the window
    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        connect(app, &QApplication::focusChanged, this, [this]() {
            followKeys();
            if (m_dialog != nullptr && m_dialog->isVisible()) {
                m_dialog->updateTarget();
            }
        });
    }
}

void GoToCoordinator::connectCommands()
{
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND)) {
        command->execute = [this]() { open(); };
        registry.updateActionState(COMMAND);
    }
}

void GoToCoordinator::followKeys()
{
    editor::BookEditor* text = m_textWithKeys ? m_textWithKeys() : nullptr;
    if (text == nullptr || text == m_lastText) {
        return;
    }
    QObject::disconnect(m_lastTextGone);
    m_lastText = text;
    m_lastTextGone = connect(text, &QObject::destroyed, this, [this]() { m_lastText = nullptr; });
}

editor::BookEditor* GoToCoordinator::text() const
{
    editor::BookEditor* text = m_textWithKeys ? m_textWithKeys() : nullptr;
    return text != nullptr ? text : m_lastText;
}

void GoToCoordinator::open()
{
    followKeys();
    if (m_dialog == nullptr) {
        m_dialog = new dialogs::GoToDialog([this]() { return text(); }, m_window);
        connect(m_dialog, &QObject::destroyed, this, [this]() { m_dialog = nullptr; });
    }
    m_dialog->show();
    m_dialog->raise();
    m_dialog->activateWindow();
    if (auto* number = m_dialog->findChild<QLineEdit*>()) {
        number->setFocus(Qt::ShortcutFocusReason);
        number->selectAll();
    }
}

}  // namespace kalahari::gui
