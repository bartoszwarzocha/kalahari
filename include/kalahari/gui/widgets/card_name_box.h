// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_name_box.h
/// @brief The declension of the name of a card of the Workshop: its forms and its other names,
///        which the book's dictionary keeps (core/card_names.h)

#ifndef KALAHARI_GUI_WIDGETS_CARD_NAME_BOX_H
#define KALAHARI_GUI_WIDGETS_CARD_NAME_BOX_H

#include <QGroupBox>
#include <QList>
#include <QString>

class QButtonGroup;
class QEvent;
class QHBoxLayout;
class QLabel;
class QToolButton;
class QVBoxLayout;

namespace kalahari::core {
class ProjectManager;
struct DictionaryName;
struct NameVariant;
}  // namespace kalahari::core

namespace kalahari::gui {

class NameFormsTable;

/// @brief The group "Declension" of a card: the forms of its name and its other names, as the
///        book's dictionary has them for the card
///
/// The short card in the Properties panel shows the forms in one line and the other names
/// under them; Change... opens the window "Name" of the book's dictionary
/// (requestNameWindow()). The full card shows the table of the forms by case of the name or
/// of the other name chosen above it, which the writer corrects right there (F2 in a cell),
/// and "+" (Insert) adds another name (requestOtherName()); the plural and the models of
/// declension are changed in the window "Name". Both follow the dictionary: a change in one
/// place is at once in the other. A card whose name the dictionary does not have yet says so,
/// and the window adds it. A book open read-only shows them, changing nothing.
class CardNameBox : public QGroupBox {
    Q_OBJECT

public:
    /// @brief The group of the short card (@p full false) or of the full card
    /// @param book The book of the card's window
    /// @param parent Parent widget
    CardNameBox(core::ProjectManager& book, bool full, QWidget* parent = nullptr);

    /// @brief Show the name of card @p cardId
    void showCard(const QString& cardId);

    /// @brief Show the name of the card anew, as the dictionary has it now
    void refresh();

    /// @brief Change... of the short card, Name... of the full one: the window "Name"
    [[nodiscard]] QToolButton* nameButton() const { return m_nameButton; }

    /// @brief The short card: the forms of the name in one line
    [[nodiscard]] QLabel* formsLabel() const { return m_formsLabel; }

    /// @brief The other names of the card ("Other names: Anna, Ania"); on the full card the
    ///        sentence of a card without a name in the dictionary
    [[nodiscard]] QLabel* otherNamesLabel() const { return m_otherNamesLabel; }

    /// @brief The full card: the table of the forms of the one chosen; null before a name
    ///        is shown and on the short card
    [[nodiscard]] NameFormsTable* formsTable() const { return m_table; }

    /// @brief The full card: the buttons of the name itself and of its other names, which
    ///        choose whose forms the table shows
    [[nodiscard]] QList<QToolButton*> variantButtons() const;

    /// @brief The full card: "+", another name
    [[nodiscard]] QToolButton* addButton() const { return m_addButton; }

    /// @brief The full card: the models of declension of the one chosen
    [[nodiscard]] QLabel* modelLabel() const { return m_modelLabel; }

    /// @brief What takes the keys, in the order of Tab: on the full card the buttons of the
    ///        names, "+" and the table, then the button of the window "Name"
    [[nodiscard]] QList<QWidget*> keyParts() const;

signals:
    /// @brief The window "Name" of the book's dictionary for card @p cardId
    void requestNameWindow(const QString& cardId);

    /// @brief Another name for card @p cardId
    void requestOtherName(const QString& cardId);

protected:
    /// @brief Insert in the group adds another name
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief The parts of the short card: Change... and the lines
    void createShortLayout(QVBoxLayout* layout);

    /// @brief The parts of the full card: the buttons of the names, the table, the models
    void createFullLayout(QVBoxLayout* layout);

    /// @brief Show @p name on the short card
    void showShort(const core::DictionaryName& name);

    /// @brief Show @p name on the full card: its buttons and the forms of the one chosen
    void showFull(const core::DictionaryName& name);

    /// @brief The buttons of the name itself and of its other names, with the texts of
    ///        @p name
    void fillVariantButtons(const core::DictionaryName& name);

    /// @brief The tooltip of the button of @p variant, at @p index of the variants: the name
    ///        itself, or who added the other name
    [[nodiscard]] static QString variantTip(qsizetype index, const core::NameVariant& variant);

    /// @brief The table for the cases of the language of the book; anew for another language
    void prepareTable();

    /// @brief The writer corrected a form in the table: it goes to the dictionary
    void saveForms();

    /// @brief What the parts say for a card whose name the dictionary does not have
    void showNoName();

    core::ProjectManager& m_book;  ///< The book of the card's window
    bool m_full = false;
    QString m_cardId;
    QString m_language;    ///< The language the table has its cases for
    int m_variant = 0;     ///< The full card: the one whose forms the table shows
    bool m_showing = false;  ///< The parts are being filled, not changed

    QToolButton* m_nameButton = nullptr;
    QLabel* m_formsLabel = nullptr;
    QLabel* m_otherNamesLabel = nullptr;
    QHBoxLayout* m_variantRow = nullptr;  ///< The full card: the buttons of the names
    QButtonGroup* m_variantGroup = nullptr;
    QToolButton* m_addButton = nullptr;
    QVBoxLayout* m_tableLayout = nullptr;  ///< The full card: where the table goes
    NameFormsTable* m_table = nullptr;
    QLabel* m_modelLabel = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_WIDGETS_CARD_NAME_BOX_H
