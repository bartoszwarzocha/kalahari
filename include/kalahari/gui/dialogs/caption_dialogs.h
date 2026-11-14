// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file caption_dialogs.h
/// @brief The windows of the numbered captions: Insert Caption, Caption Numbering and Insert
///        Cross-Reference

#pragma once

#include "kalahari/editor/text_captions.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief Insert > Caption... (Ctrl+Shift+G): the caption of an image
///
/// The caption is a paragraph in the style Caption next to the image: its label and the number
/// the program gives ("Figure 3."), then the text the writer types. Each label has a numbering
/// of its own; Numbering... sets how the book counts the label chosen. Without a label and
/// number the caption is its text alone. The gear opens Settings > Editor > Images.
class InsertCaptionDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows, and how it asks the book
    struct Data {
        QStringList labels;        ///< The labels of the book, in their order
        QSet<QString> usedLabels;  ///< The labels the captions of the book have (not removable)
        editor::ImageCaption caption;  ///< The image's caption (its label: the one chosen first)
        /// The number the image's caption would have with a label ("3", "2.3")
        std::function<QString(const QString& label)> numberOf;
        /// How the book numbers a label
        std::function<editor::CaptionNumbering(const QString& label)> numberingOf;
        /// The window of the numbering of a label; whether it changed. None: a file outside a
        /// book, which the Settings number
        std::function<bool(const QString& label, QWidget* parent)> editNumbering;
    };

    explicit InsertCaptionDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The caption as the writer left it (not shown: none, the image loses its own)
    [[nodiscard]] editor::ImageCaption caption() const;

    /// @brief The labels of the book as the writer left them (new ones added, some removed)
    [[nodiscard]] QStringList labels() const { return m_labels; }

private:
    /// @brief Fill the list of labels, with @p current chosen
    void showLabels(const QString& current);

    /// @brief New Label...: a label of the book's own
    void addLabel();

    /// @brief Remove: a label no caption has
    void removeLabel();

    /// @brief Numbering...: the window of the numbering of the label chosen
    void editNumbering();

    /// @brief The fields follow the choices: the label and number before the text, the buttons
    ///        a label allows, how the label is numbered
    void updateFields();

    Data m_data;
    QStringList m_labels;
    bool m_removed = false;  ///< Remove Caption was pressed
    QLabel* m_prefix = nullptr;
    QLineEdit* m_text = nullptr;
    QComboBox* m_label = nullptr;
    QPushButton* m_newLabel = nullptr;
    QPushButton* m_removeLabel = nullptr;
    QComboBox* m_position = nullptr;
    QCheckBox* m_unnumbered = nullptr;
    QPushButton* m_numbering = nullptr;
    QLabel* m_numberingHint = nullptr;
};

/// @brief Caption Numbering...: how the book numbers the captions of a label
///
/// The format of the number, through the whole book or anew in each chapter, and then with
/// the chapter's number first ("2.3") and the separator between them.
class CaptionNumberingDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @param label The label ("Figure")
    /// @param numbering How the book numbers it now
    CaptionNumberingDialog(const QString& label, const editor::CaptionNumbering& numbering,
                           QWidget* parent = nullptr);

    /// @brief The numbering as the writer left it
    [[nodiscard]] editor::CaptionNumbering numbering() const;

    /// @brief How a numbering is said in a line: "1, 2, 3 · anew in each chapter, after the
    ///        chapter's number"
    [[nodiscard]] static QString describe(const editor::CaptionNumbering& numbering);

private:
    /// @brief The fields a choice allows, and the example
    void updateFields();

    QString m_label;
    QComboBox* m_format = nullptr;
    QRadioButton* m_continuous = nullptr;
    QRadioButton* m_perChapter = nullptr;
    QCheckBox* m_withChapter = nullptr;
    QLabel* m_separatorLabel = nullptr;
    QComboBox* m_separator = nullptr;
    QLabel* m_example = nullptr;
};

/// @brief Insert > Cross-Reference...: a field that shows the number of a caption of the book
///
/// The kind (the label) narrows the list of the captions; the field shows the number alone
/// (the writer writes "Fig." or "in figure" around it, in the right case), the label and the
/// number, the whole caption, or "above" or "below". As a link, Ctrl+click goes to the image.
class InsertCrossReferenceDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief A caption the cross-reference may name
    struct Target {
        QString id;          ///< The id of its number
        QString label;       ///< Its label
        QString number;      ///< Its number as its field shows it: "3", "2.3"
        QString text;        ///< The whole caption: "Figure 3. The bridge in 1931"
        QString where;       ///< The title of the text it is in; empty: the text in front
        bool before = false; ///< It is before the cursor
    };

    /// @brief What the window shows
    struct Data {
        QStringList labels;           ///< The labels of the targets, in the book's order
        std::vector<Target> targets;  ///< The captions of the book, in its order
        editor::ReferenceShows shows = editor::ReferenceShows::Number;  ///< Chosen first
    };

    explicit InsertCrossReferenceDialog(Data shown, QWidget* parent = nullptr);

    /// @brief The cross-reference chosen
    [[nodiscard]] editor::CrossReference reference() const;

private:
    /// @brief Fill the list with the captions of the kind chosen; @p chosen first (its id)
    void showTargets(const QString& chosen);

    /// @brief The choices of what the field shows name the caption chosen; Insert needs one
    void updateChoices();

    /// @brief The target of the item chosen; nullptr: none
    [[nodiscard]] const Target* chosenTarget() const;

    Data m_data;
    QComboBox* m_kind = nullptr;
    QComboBox* m_shows = nullptr;
    QListWidget* m_targets = nullptr;
    QCheckBox* m_link = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
