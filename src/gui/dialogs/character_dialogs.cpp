/// @file character_dialogs.cpp
/// @brief The small windows of Insert > Special Character...: the keys of a character, and a
///        replacement of auto-correct that gives a character

#include "kalahari/gui/dialogs/character_dialogs.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/special_characters.h"
#include "kalahari/gui/widgets/shortcut_recorder.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// @brief A character as a window names it: its name and the character ("Em dash (—)")
QString characterTitle(char32_t code)
{
    const SpecialCharacter character = SpecialCharacters().character(code);
    const QString shown = character.mark.isEmpty() ? character.text() : character.mark;
    return QStringLiteral("%1 (%2)").arg(character.name, shown);
}

/// @brief A typed text in a font of the same widths, in which "->" stays two characters
QString codeHtml(const QString& text)
{
    return QStringLiteral("<code>%1</code>").arg(text.toHtmlEscaped());
}

/// @brief Keys as the system writes them
QString keysText(const KeyboardShortcut& keys)
{
    return keys.isEmpty()
               ? QString()
               : ShortcutRules::keysText(QKeyCombination::fromCombined(
                     keys.keyCode | static_cast<int>(keys.modifiers.toInt())));
}

/// @brief The name of a command, as its menu item says it
QString commandTitle(const std::string& id)
{
    const Command* command = CommandRegistry::getInstance().getCommand(id);
    return command != nullptr ? QString::fromStdString(command->label)
                              : QString::fromStdString(id);
}

} // namespace

// ============================================================================
// CharacterShortcutDialog
// ============================================================================

CharacterShortcutDialog::CharacterShortcutDialog(char32_t code, std::string commandId,
                                                 const KeyboardShortcut& keysNow, QWidget* parent)
    : KalahariDialog(parent)
    , m_commandId(std::move(commandId))
    , m_keysNow(keysNow)
    , m_keys(keysNow)
{
    setHeading(tr("Shortcut for a Character"),
               //: %1 is a character with its name, as "Em dash (—)"
               tr("Press the keys that are to insert %1.").arg(characterTitle(code)));
    setHeadingIcon(QStringLiteral("insert.specialChar"));
    setAcceptText(tr("Assign"));

    m_field = new ShortcutRecorder(this);
    m_field->setReadOnly(true);
    m_field->setPlaceholderText(tr("No shortcut"));
    m_field->setText(keysText(keysNow));
    addField(tr("Shortcut"), m_field);

    m_message = new QLabel(this);
    m_message->setWordWrap(true);
    m_message->setTextFormat(Qt::RichText);
    m_message->hide();
    contentLayout()->addWidget(m_message);

    auto* note = new QLabel(
        tr("The shortcut is also on the page Settings > Keyboard Shortcuts, in the group "
           "Characters, where it can be changed or removed."),
        this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(note);
    contentLayout()->addStretch(1);

    m_recordButton = addSideButton(tr("&Press Other Keys"));
    m_removeButton = addSideButton(tr("&Remove Shortcut"));
    m_removeButton->setVisible(!keysNow.isEmpty());
    connect(m_recordButton, &QPushButton::clicked, this,
            &CharacterShortcutDialog::startRecording);
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        m_keys = KeyboardShortcut();
        KalahariDialog::accept();
    });
    connect(m_field, &ShortcutRecorder::recorded, this, &CharacterShortcutDialog::record);
    connect(m_field, &ShortcutRecorder::recordingStopped, this, [this](bool) {
        m_recordButton->setEnabled(true);
    });

    acceptButton()->setEnabled(false);
    startRecording();
}

void CharacterShortcutDialog::startRecording()
{
    m_recordButton->setEnabled(false);
    m_field->setFocus(Qt::OtherFocusReason);
    m_field->startRecording(tr("Press the keys... (Esc – cancel)"));
}

void CharacterShortcutDialog::showMessage(const QString& html)
{
    m_message->setText(html);
    m_message->setVisible(!html.isEmpty());
    growForContent();
}

void CharacterShortcutDialog::record(QKeyCombination keys)
{
    m_recordButton->setEnabled(true);
    const KeyboardShortcut pressed(keys.toCombined() & ~int(Qt::KeyboardModifierMask),
                                   keys.keyboardModifiers());
    m_field->setText(keysText(pressed));

    const KeyCheck check = ShortcutRules().check(keys, m_commandId);
    if (check.result == KeyCheck::Result::Refused) {
        showMessage(check.reason.toHtmlEscaped());
        acceptButton()->setEnabled(false);
        // The next keys pressed are the next try
        startRecording();
        return;
    }

    m_keys = pressed;
    QStringList lines;
    const std::string owner = ownerOfKeys(pressed);
    if (!owner.empty() && owner != m_commandId) {
        //: %1 are keys, %2 the command that has them
        lines.append(tr("%1 belongs to <b>%2</b> now: if you assign it here, that command "
                        "will be left without a shortcut.")
                         .arg(keysText(pressed).toHtmlEscaped(),
                              commandTitle(owner).toHtmlEscaped()));
    }
    if (check.result == KeyCheck::Result::Warning) {
        lines.append(check.reason.toHtmlEscaped());
    }
    showMessage(lines.join(QStringLiteral("<br>")));
    acceptButton()->setEnabled(pressed != m_keysNow);
    acceptButton()->setFocus(Qt::OtherFocusReason);
}

// ============================================================================
// ReplacementDialog
// ============================================================================

ReplacementDialog::ReplacementDialog(char32_t code, QWidget* parent)
    : KalahariDialog(parent)
    , m_replacement(SpecialCharacters().character(code).text())
{
    setHeading(tr("New Replacement"),
               //: %1 is a character with its name, as "Rightwards arrow (→)"
               tr("Auto-correct replaces the word you type here with %1.")
                   .arg(characterTitle(code)));
    setHeadingIcon(QStringLiteral("insert.specialChar"));
    setAcceptText(tr("Add"));

    // In a font of the same widths: "->" stays two characters, where a font of the system
    // draws it as an arrow
    m_field = new QLineEdit(this);
    m_field->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_field->setPlaceholderText(tr("e.g. ->"));
    addField(tr("Replace"), m_field);

    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::RichText);
    contentLayout()->addWidget(m_note);
    contentLayout()->addStretch(1);

    connect(m_field, &QLineEdit::textChanged, this, &ReplacementDialog::updateNote);
    updateNote();
    m_field->setFocus(Qt::OtherFocusReason);
}

QString ReplacementDialog::word() const
{
    return m_field->text().trimmed();
}

void ReplacementDialog::updateNote()
{
    const QString typed = word();
    QString note;
    bool can = !typed.isEmpty();
    bool warning = true;
    const std::vector<ReplacementEntry> entries = replacementEntries();
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&typed](const ReplacementEntry& entry) {
                                        return entry.from == typed;
                                    });
    auto& settings = core::SettingsManager::getInstance();
    if (typed.contains(QRegularExpression(QStringLiteral("\\s")))) {
        note = tr("A replacement is one word: without spaces.").toHtmlEscaped();
        can = false;
    } else if (found != entries.end() && found->to == m_replacement && found->enabled) {
        note = tr("The replacement table has this pair already.").toHtmlEscaped();
        can = false;
    } else if (found != entries.end() && found->to != m_replacement) {
        //: %1 is a word, %2 what replaces it now, %3 the character
        note = tr("Auto-correct replaces “%1” with “%2” now: Add puts “%3” in its place.")
                   .toHtmlEscaped()
                   .arg(codeHtml(found->from), found->to.toHtmlEscaped(),
                        m_replacement.toHtmlEscaped());
    } else {
        warning = false;
        note = tr("The pair goes to the replacement table on the page Settings > Editor > "
                  "Auto-correct, and works after a space or a punctuation mark.")
                   .toHtmlEscaped();
    }
    if (can && (!settings.get<bool>("editor.autoCorrect.enabled", true) ||
                !settings.get<bool>("editor.autoCorrect.replacementTable", true))) {
        note += QLatin1Char(' ') +
                tr("The replacements of the table are turned off now (Settings > Editor > "
                   "Auto-correct).")
                    .toHtmlEscaped();
    }
    // A word that cannot be added, or a replacement it changes, in the color of the text;
    // a hint greyed out
    m_note->setForegroundRole(warning ? QPalette::WindowText : QPalette::PlaceholderText);
    m_note->setText(note);
    acceptButton()->setEnabled(can);
    growForContent();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
