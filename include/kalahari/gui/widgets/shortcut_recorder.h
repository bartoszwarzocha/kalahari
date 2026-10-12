/// @file shortcut_recorder.h
/// @brief A field that shows keys and, when asked, records the next key press

#pragma once

#include <QKeyCombination>
#include <QLineEdit>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief A field that records key presses (Settings > Keyboard Shortcuts)
///
/// Outside recording it is an ordinary field. While recording it takes every key, also
/// Tab, Enter and the keys of the window's shortcuts and mnemonics, and shows the
/// modifiers held down; Esc alone stops recording. It records one key press and stops,
/// or keeps recording until Esc (search by keys). Leaving the field stops recording too.
class ShortcutRecorder : public QLineEdit {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit ShortcutRecorder(QWidget* parent = nullptr);

    /// @brief Record key presses
    /// @param prompt Shown until a key is pressed ("Press the new shortcut... (Esc: cancel)")
    /// @param keepRecording Record until Esc instead of one key press
    void startRecording(const QString& prompt, bool keepRecording = false);

    /// @brief Stop recording without a key; restores the field
    void stopRecording();

    /// @brief Whether it records keys now
    [[nodiscard]] bool isRecording() const { return m_recording; }

signals:
    /// @brief A key press was recorded (ShortcutRules::keysOf())
    void recorded(QKeyCombination keys);

    /// @brief Recording stopped without a key: Esc, or the field was left
    /// @param escape Esc stopped it
    void recordingStopped(bool escape);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// @brief Show the modifiers held down, or the prompt
    void showModifiers(Qt::KeyboardModifiers modifiers);

    /// @brief End recording and restore the field
    void finish();

    bool m_recording = false;
    bool m_keepRecording = false;
    bool m_wasReadOnly = false;
    QString m_placeholder;  ///< The field's own placeholder, restored afterwards
    QString m_text;         ///< The field's own text, restored when nothing was recorded
    QString m_recordedText; ///< The keys recorded last while recording until Esc
};

} // namespace gui
} // namespace kalahari
