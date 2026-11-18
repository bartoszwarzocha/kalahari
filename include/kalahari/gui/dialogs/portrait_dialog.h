// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file portrait_dialog.h
/// @brief The window Portrait: a portrait of a card put together from ready parts

#ifndef KALAHARI_GUI_DIALOGS_PORTRAIT_DIALOG_H
#define KALAHARI_GUI_DIALOGS_PORTRAIT_DIALOG_H

#include "kalahari/core/card_data.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QString>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QToolButton;
class QWidget;

namespace kalahari::gui::dialogs {

/// @brief The card the window Portrait puts portraits together for
struct PortraitSubject {
    QString name;                         ///< The name of the card, in the heading
    QString gender;                       ///< The choice of the field Gender: "woman", "man", ...
    QString age;                          ///< The text of the field Age
    bool hasAppearance = false;           ///< Whether the card has the field Appearance
    QString appearance;                   ///< Its text
    QColor color;                         ///< The color of the card, for the background
    QList<core::CardPortrait> portraits;  ///< All the portraits of the card, the main one first
    qsizetype current = -1;  ///< The portrait to open; -1 or a picture: a new one
};

/// @brief The window Portrait: a portrait put together from ready parts, without drawing
///
/// On the left the preview, its frame (the bust of the card or the whole figure) and style
/// (flat or line), Randomize (Alt+R), Undo (Ctrl+Z) and Start Over (the portrait the fields
/// Gender and Age suggest). In the middle the groups of the parts (Up and Down choose one)
/// and the portraits of the card with their labels; the main one, on the card, has a star.
/// On the right the tiles of the parts of the group, each the portrait with that value, and
/// the palettes of its colors with Other..., then the description of the look, which can go at
/// the end of the field Appearance, and the choice to save the portrait as a picture in the
/// materials. Every change shows at once in the preview; the card changes on OK.
class PortraitDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window for @p subject, with the portrait it names, or a new one
    /// @param subject The card and its portraits
    /// @param parent Parent widget
    explicit PortraitDialog(PortraitSubject subject, QWidget* parent = nullptr);

    /// @brief The portraits of the card as the writer left them, the main one first
    [[nodiscard]] const QList<core::CardPortrait>& portraits() const {
        return m_subject.portraits;
    }

    /// @brief The portrait shown, in portraits()
    [[nodiscard]] qsizetype current() const { return m_subject.current; }

    /// @brief The description of the look of the portrait shown
    [[nodiscard]] QString description() const;

    /// @brief Whether the description goes at the end of the field Appearance
    [[nodiscard]] bool appendsDescription() const;

    /// @brief Whether the portrait shown is also saved as a picture in the materials
    [[nodiscard]] bool savesPicture() const;

    /// @brief The frame shown
    [[nodiscard]] AvatarFrame frame() const { return m_frame; }

    /// @brief The portrait shown as a picture to save, in the frame shown (pictureOf())
    [[nodiscard]] QImage picture() const;

    /// @brief Portrait @p portrait of a card as a picture to save in the materials: 800 pixels
    /// high, in @p frame, on @p card, the color of the card, with the sign of @p gender when it
    /// has the sign
    [[nodiscard]] static QImage pictureOf(const core::CardPortrait& portrait, AvatarFrame frame,
                                          const QColor& card, const QString& gender);

    /// @brief The groups of the parts
    [[nodiscard]] QListWidget* groupList() const { return m_groups; }

    /// @brief The portraits of the card made of parts
    [[nodiscard]] QListWidget* portraitList() const { return m_strip; }

    /// @brief The preview
    [[nodiscard]] QLabel* preview() const { return m_preview; }

    /// @brief The label of the portrait shown
    [[nodiscard]] QLineEdit* labelEdit() const { return m_label; }

    /// @brief Randomize, Undo and Start Over
    [[nodiscard]] QPushButton* randomButton() const { return m_randomButton; }
    [[nodiscard]] QPushButton* undoButton() const { return m_undoButton; }
    [[nodiscard]] QPushButton* resetButton() const { return m_resetButton; }

    /// @brief New: a new portrait of the card, a copy of the one shown
    [[nodiscard]] QToolButton* newButton() const { return m_newButton; }

    /// @brief The choice of the frame: Bust (0) and Whole Figure (1)
    [[nodiscard]] QButtonGroup* frameButtons() const { return m_frameButtons; }

    /// @brief The choice of the style: Flat (0) and Line (1)
    [[nodiscard]] QButtonGroup* styleButtons() const { return m_styleButtons; }

    /// @brief The tile of value @p value of part @p key in the group shown; nullptr: none
    [[nodiscard]] QToolButton* tile(const QString& key, const QString& value) const;

    /// @brief The swatch of @p color of color @p key in the group shown; nullptr: none
    [[nodiscard]] QToolButton* swatch(const QString& key, const QString& color) const;

    /// @brief Other...: the color window for color @p key in the group shown; nullptr: none
    [[nodiscard]] QPushButton* otherColorButton(const QString& key) const;

    /// @brief The sign of the gender in the corner (in the group Background)
    [[nodiscard]] QCheckBox* signBox() const { return m_signBox; }

    /// @brief Add the description at the end of the field Appearance
    [[nodiscard]] QCheckBox* appendBox() const { return m_appendBox; }

    /// @brief Also save the portrait as a picture in the materials
    [[nodiscard]] QCheckBox* pictureBox() const { return m_pictureBox; }

    /// @brief The description of the look shown
    [[nodiscard]] QLabel* descriptionLabel() const;

    /// @brief Draw by chance with @p seed from now on (the tests: the same portraits each time)
    void setRandomSeed(quint32 seed) { m_random.seed(seed); }

protected:
    /// @brief The sizes of the tiles and of the preview follow the screen
    void changeEvent(QEvent* event) override;

private:
    /// @brief The portraits and the one shown, for Undo
    struct Step {
        QList<core::CardPortrait> portraits;
        qsizetype current = -1;
    };

    /// @brief The left side: the preview, the frame, the style, Randomize, Undo, Start Over
    [[nodiscard]] QWidget* previewSide();

    /// @brief The middle: the groups and the portraits of the card
    [[nodiscard]] QWidget* groupSide();

    /// @brief The right side: the tiles and the palettes of the group, the description
    [[nodiscard]] QWidget* optionSide();

    /// @brief A row of joined buttons of a choice, numbered from 0, in @p group
    [[nodiscard]] QWidget* choiceRow(const QString& label, QButtonGroup* group,
                                     const QStringList& texts);

    /// @brief The portrait shown
    [[nodiscard]] core::CardPortrait& shown() { return m_subject.portraits[m_subject.current]; }
    [[nodiscard]] const core::CardPortrait& shown() const {
        return m_subject.portraits.at(m_subject.current);
    }

    /// @brief A new portrait: the one the fields Gender and Age suggest, in the style of new
    /// portraits
    [[nodiscard]] core::CardPortrait proposal() const;

    /// @brief Keep the portraits for Undo before a change
    void keepForUndo();

    /// @brief The portrait shown changed: draw it again, its tiles, its strip and description
    void changed();

    /// @brief Choose value @p value of part @p key; a part of many takes it or leaves it
    void choose(const QString& key, const QString& value);

    /// @brief Give color @p key the color @p color
    void setColor(const QString& key, const QString& color);

    /// @brief Choose color @p key in the color window of the program
    void chooseOtherColor(const QString& key);

    /// @brief Draw the parts and the natural colors by chance (Randomize)
    void randomize();

    /// @brief Back one step (Undo)
    void undo();

    /// @brief Back to the portrait the fields Gender and Age suggest (Start Over)
    void startOver();

    /// @brief A new portrait of the card, a copy of the one shown
    void addPortrait();

    /// @brief Show portrait @p index of the card
    void showPortrait(qsizetype index);

    /// @brief Show frame @p frame
    void setFrame(AvatarFrame frame);

    /// @brief Show group @p row of the list
    void showGroup(int row);

    /// @brief The tiles and the palettes of the group shown, again
    void buildOptions();

    /// @brief buildOptions() when a color of the portrait has no swatch in the group shown
    void refreshOptions();

    /// @brief The buttons @p T of the tiles and the palettes of the group shown
    template <typename T>
    [[nodiscard]] QList<T*> optionButtons() const {
        QWidget* page = m_options->widget();
        return page != nullptr ? page->findChildren<T*>() : QList<T*>();
    }

    /// @brief The tiles of part @p key
    void addTiles(const Avatar::Part& part);

    /// @brief The palette of color @p key
    void addPalette(const Avatar::Color& color);

    /// @brief The swatches of @p colors of color @p key in @p row
    void addSwatches(QWidget* row, const QString& key, const QStringList& colors);

    /// @brief Draw the tiles of the group shown again, checked as the portrait has them
    void showTiles();

    /// @brief Draw the preview again
    void showPreview();

    /// @brief The strip of the portraits of the card again
    void showStrip();

    /// @brief @p avatar drawn in @p frame, @p side pixels high (the device's pixels as well)
    [[nodiscard]] QPixmap drawn(const QJsonObject& avatar, AvatarFrame frame, int side,
                                bool withSign) const;

    PortraitSubject m_subject;
    QList<Step> m_undo;
    AvatarFrame m_frame = AvatarFrame::Bust;
    QRandomGenerator m_random;

    QLabel* m_preview = nullptr;
    QButtonGroup* m_frameButtons = nullptr;
    QButtonGroup* m_styleButtons = nullptr;
    QPushButton* m_randomButton = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QListWidget* m_groups = nullptr;
    QListWidget* m_strip = nullptr;
    QToolButton* m_newButton = nullptr;
    QLineEdit* m_label = nullptr;
    QScrollArea* m_options = nullptr;  ///< The tiles and the palettes of the group shown
    QCheckBox* m_signBox = nullptr;
    QLabel* m_description = nullptr;
    QCheckBox* m_appendBox = nullptr;
    QCheckBox* m_pictureBox = nullptr;
};

}  // namespace kalahari::gui::dialogs

#endif  // KALAHARI_GUI_DIALOGS_PORTRAIT_DIALOG_H
