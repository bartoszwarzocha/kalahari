// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_page.h
/// @brief The card of a character, a location or an item of the Workshop: short in the
/// Properties panel, full on a tab beside the chapters (CardPanel)

#ifndef KALAHARI_GUI_PANELS_CARD_PAGE_H
#define KALAHARI_GUI_PANELS_CARD_PAGE_H

#include <QHash>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace kalahari::core {
class ProjectManager;
struct CardData;
struct CardField;
struct ElementKind;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui {

class OwnFieldsBox;

/// @brief The card of a character, a location or an item (core::ElementKind::isCard()): short
/// in the Properties panel, what the writer checks and changes while writing; full on a tab of
/// its own, the place to work on it
///
/// - Its portrait: the first of its portraits (core::CardData::portraits), a picture of the
///   materials of the book, else its initials on its color. A click, Space, Enter or the menu
///   key opens the menu of the portrait: a picture of the materials, a picture from a file (it
///   becomes a material linked with the card) or no portrait (the picture stays in the
///   materials).
/// - Its name (the title of the element) and its short description ("In short", the
///   description of the element).
/// - The fields of its kind (core::ElementKind::fields): those shown on the card on the full
///   card, those shown in the Properties panel on the short one, the field that stands beside
///   the kind (core::CardField::subtitle) right under the name. A text has a line, a longer
///   text a few lines, a list its values, a card the names of the cards of its kind suggested.
/// - The full card has a bar with the gear (Settings > Cards) over its data, which scroll: the
///   kind with that field and the color of the card (a click opens the color window), the
///   fields, the writer's own fields (OwnFieldsBox) and, beside them, the long description,
///   the text of the card (setDescriptionWidget()).
/// - The short card has the gear at the title of its group, and Open Card, which opens the
///   full card.
///
/// Every change is saved at once in the .klh file: a line when it loses the keys or Enter is
/// pressed, a list when a value is chosen. The card shows a change made elsewhere at once
/// (core::ProjectManager::cardChanged(), elementRenamed()); the field with the keys keeps
/// what is typed in it. A book open read-only shows it all and changes nothing.
class CardPage : public QWidget {
    Q_OBJECT

public:
    /// @brief Side of the portrait of the short card
    static constexpr int SHORT_PORTRAIT = 96;
    /// @brief Side of the portrait of the full card
    static constexpr int FULL_PORTRAIT = 128;

    /// @brief The card; @p full: the full card, else the short one
    /// @param book The book of its window
    /// @param full Whether it is the full card
    /// @param parent Parent widget
    CardPage(core::ProjectManager& book, bool full, QWidget* parent = nullptr);
    /// @note What a field does while the card is destroyed (a line losing the keys) does not
    /// reach the card
    ~CardPage() override;

    CardPage(const CardPage&) = delete;
    CardPage& operator=(const CardPage&) = delete;
    CardPage(CardPage&&) = delete;
    CardPage& operator=(CardPage&&) = delete;

    /// @brief Whether it is the full card
    [[nodiscard]] bool isFull() const { return m_full; }

    /// @brief Show card @p elementId of the open book
    void showCard(const QString& elementId);

    /// @brief The card it shows
    [[nodiscard]] QString elementId() const { return m_elementId; }

    /// @brief Show the card again as the book has it; the field with the keys keeps what is
    /// typed in it
    void refresh();

    /// @brief Give the keys to the name, selected
    void focusName();

    /// @brief Full card: put @p description, the editor of the long description, beside the
    /// data; the card owns it then
    void setDescriptionWidget(QWidget* description);

    /// @brief The portrait of @p element at @p side pixels: its picture, cut to a square, or
    /// its initials on its color
    [[nodiscard]] static QPixmap portraitOf(const core::ProjectManager& book,
                                            const core::ProjectElement& element, int side,
                                            qreal pixelRatio);

    /// @brief The initials of @p title: the first letters of its first two words, in capitals
    [[nodiscard]] static QString initialsOf(const QString& title);

    /// @brief The portrait, a button with its menu
    [[nodiscard]] QToolButton* portraitButton() const { return m_portrait; }
    /// @brief The name
    [[nodiscard]] QLineEdit* nameEdit() const { return m_nameEdit; }
    /// @brief In short
    [[nodiscard]] QPlainTextEdit* shortEdit() const { return m_shortEdit; }
    /// @brief The kind with the value of the field beside it: "Character · main"; full card
    [[nodiscard]] QLabel* subtitleLabel() const { return m_subtitleLabel; }
    /// @brief The color of the card; full card
    [[nodiscard]] QToolButton* colorButton() const { return m_colorButton; }
    /// @brief The gear: Settings > Cards
    [[nodiscard]] QToolButton* settingsButton() const { return m_settingsButton; }
    /// @brief Open Card; short card
    [[nodiscard]] QPushButton* openButton() const { return m_openButton; }
    /// @brief The writer's own fields; full card
    [[nodiscard]] OwnFieldsBox* ownFields() const { return m_ownFields; }
    /// @brief The group of the long description; full card
    [[nodiscard]] QGroupBox* descriptionGroup() const { return m_descriptionGroup; }
    /// @brief The group of the short card, "Character Information"; short card
    [[nodiscard]] QGroupBox* infoGroup() const { return m_infoGroup; }
    /// @brief The control of field @p fieldId; nullptr when the card does not show it
    [[nodiscard]] QWidget* fieldEditor(const QString& fieldId) const {
        return m_editors.value(fieldId);
    }
    /// @brief The menu of the portrait, filled when it opens
    [[nodiscard]] QMenu* portraitMenu() const { return m_portraitMenu; }

public slots:
    /// @brief Make picture material @p imageId the portrait of the card, first of its portraits,
    /// and link it with the card
    void setPortrait(const QString& imageId);

    /// @brief Add the picture of file @p path to the materials, linked with the card, and make
    /// it the portrait
    void addPortraitFile(const QString& path);

    /// @brief Take out the portrait shown; its picture stays in the materials
    void removePortrait();

    /// @brief Open the color window for the color of the card
    void chooseColor();

signals:
    /// @brief Open the full card of @p elementId
    void requestOpenCard(const QString& elementId);

    /// @brief Open Settings > Cards
    void requestSettings();

    /// @brief The name of card @p elementId changed: the Navigator and the tabs show it
    void titleChanged(const QString& elementId);

    /// @brief Material @p materialId was added for the card (its portrait from a file): the
    /// Navigator shows it
    void materialAdded(const QString& materialId);

protected:
    /// @brief The keys of the portrait, the gear at the title of the group, the lines saved
    /// when they lose the keys, the names suggested when a line gets them
    bool eventFilter(QObject* watched, QEvent* event) override;
    /// @brief Another font: the name of the full card follows it
    void changeEvent(QEvent* event) override;

private:
    /// @brief The header of the full card: the portrait, the name, the kind and the color, In
    /// short; the bar with the gear over the data, which scroll
    void createFullLayout();

    /// @brief The group of the short card: the portrait, the name and the field beside the
    /// kind, In short, the fields; the gear at its title, Open Card under it
    void createShortLayout();

    /// @brief The name, In short, the kind and the color of @p element, or the title of the
    /// group of the short card
    void showHeader(const core::ProjectElement& element);

    /// @brief The icon of the kind of @p element, beside the kind; full card
    void showKindIcon(const core::ProjectElement& element);

    /// @brief The fields of @p kind in the form, those that this card shows
    void buildFields(const core::ElementKind& kind);

    /// @brief No fields: those of another kind go
    void clearFields();

    /// @brief The keys go through the card as it reads: the gear, the portrait, the name, the
    /// color or @p besides (the field beside the kind), In short, @p fields, the own fields
    void orderKeys(const QList<QWidget*>& besides, const QList<QWidget*>& fields);

    /// @brief The control of @p field, which saves its value
    QWidget* createEditor(const core::CardField& field);

    /// @brief Show the values of @p element in the controls of the fields; the one with the
    /// keys keeps what is typed in it
    void fillFields(const core::ProjectElement& element);

    /// @brief Show the portrait of @p element; read again only when it changed
    void showPortrait(const core::ProjectElement& element);

    /// @brief Show the color of @p element on its button
    void showColor(const core::ProjectElement& element);

    /// @brief Fill the menu of the portrait as it opens
    void fillPortraitMenu();

    /// @brief Fill @p pictures with the pictures of the materials, those of the card first;
    /// @p current, the portrait shown, is checked
    void fillPictures(QMenu* pictures, const QString& current);

    /// @brief Choose the file of a new portrait in the system window
    void choosePortraitFile();

    /// @brief Link picture material @p imageId with the card, when it is not
    /// @return false when it is no material, or the link could not be saved (said so)
    bool linkWithCard(const QString& imageId);

    /// @brief Save the name from its field
    void saveName();

    /// @brief Save the short description from its field
    void saveShort();

    /// @brief Save @p value as the value of field @p fieldId
    void saveValue(const QString& fieldId, const QString& value);

    /// @brief Save what @p watched, which lost the keys, has: In short or a field of lines
    void saveFocused(const QObject* watched);

    /// @brief Give the line @p watched of a field of the type "card", which got the keys, the
    /// names of the cards as they are now
    void fillSuggestions(const QObject* watched);

    /// @brief Give the card the data @p card, saying so when it cannot be saved
    void saveCard(const core::CardData& card);

    /// @brief The kind and the value of the field beside it: "Character · main"
    [[nodiscard]] QString subtitleOf(const core::ProjectElement& element) const;

    /// @brief The name of the kind of @p element, in capitals: "Character"
    [[nodiscard]] QString kindNameOf(const core::ProjectElement& element) const;

    /// @brief The names of the cards that the line of field @p fieldId, of the type "card",
    /// suggests: the cards of its kind, without this one
    [[nodiscard]] QStringList suggestedCards(const QString& fieldId) const;

    /// @brief The name of the full card larger than the text, in the font of the card
    void fitNameFont();

    bool m_full = false;
    QString m_elementId;
    QString m_kindKey;          ///< The kind whose fields the form has: "kalahari.base:item"
    QString m_portraitKey;      ///< What the portrait shown was drawn from
    bool m_updating = false;    ///< The fields are being filled: no saving
    QHash<QString, QWidget*> m_editors;  ///< Field id -> its control
    QHash<const QObject*, QString> m_fieldOf;  ///< A control of lines or of a card -> its field

    QGroupBox* m_infoGroup = nullptr;      ///< Short card: its group, "Character Information"
    QToolButton* m_portrait = nullptr;
    QMenu* m_portraitMenu = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLabel* m_kindIcon = nullptr;          ///< Full card: the icon of the kind
    QLabel* m_subtitleLabel = nullptr;
    QToolButton* m_colorButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QPlainTextEdit* m_shortEdit = nullptr;
    QVBoxLayout* m_subtitleLayout = nullptr;  ///< Short card: the field beside the kind
    QFormLayout* m_form = nullptr;            ///< The fields
    QWidget* m_fieldsBox = nullptr;           ///< What holds the fields: "Data" on the full card
    OwnFieldsBox* m_ownFields = nullptr;
    QPushButton* m_openButton = nullptr;
    QGroupBox* m_descriptionGroup = nullptr;  ///< Full card: where the long description goes
    core::ProjectManager& m_book;             ///< The book of its window
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_CARD_PAGE_H
