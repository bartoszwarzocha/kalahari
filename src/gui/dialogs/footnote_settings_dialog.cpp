// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file footnote_settings_dialog.cpp
/// @brief Insert > Footnote Layout...: how the book numbers its notes and where its
///        endnotes are

#include "kalahari/gui/dialogs/footnote_settings_dialog.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/settings_dialog.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QStringList>

#include <algorithm>
#include <array>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The formats of the numbers, in the order of the lists (core::NoteNumbering::format)
constexpr std::array<const char*, 6> NOTE_FORMATS = {"1", "a", "A", "i", "I", "*"};

/// A list of the formats of the numbers, @p current chosen
QComboBox* formatList(const QString& current, QWidget* parent)
{
    auto* list = new QComboBox(parent);
    for (const char* code : NOTE_FORMATS) {
        const QString format = QString::fromLatin1(code);
        list->addItem(FootnoteSettingsDialog::formatSample(format), format);
    }
    list->setCurrentIndex(std::max(0, list->findData(current)));
    return list;
}

/// A list of the numberings: anew in each chapter or through the book
QComboBox* numberingList(bool perChapter, QWidget* parent)
{
    auto* list = new QComboBox(parent);
    //: The numbering of the notes. In Polish: W każdym rozdziale od 1
    list->addItem(FootnoteSettingsDialog::tr("From 1 in each chapter"),
                  true);
    //: In Polish: Ciągła w całej książce
    list->addItem(FootnoteSettingsDialog::tr("Continuous through the book"), false);
    list->setCurrentIndex(perChapter ? 0 : 1);
    return list;
}

/// A label of a field in a grid: its text, its buddy
QLabel* fieldLabel(const QString& text, QWidget* field, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setBuddy(field);
    return label;
}

} // namespace

FootnoteSettingsDialog::FootnoteSettingsDialog(const core::BookFootnotes& footnotes,
                                               QWidget* parent)
    : KalahariDialog(parent),
      m_footnotes(footnotes)
{
    //: Insert > Footnote Layout... In Polish: Ustawienia przypisów / Numeracja i miejsce
    //: przypisów w tej książce.
    setHeading(tr("Footnote Layout"),
               tr("How this book numbers its notes and where its endnotes are."));
    setHeadingIcon(QStringLiteral("insert.footnote"));
    setAcceptText(tr("OK"));
    //: The tooltip of the gear of the window of the notes' layout. In Polish: Ustawienia
    //: edytora
    addSettingsButton(QString::fromLatin1(SettingsDialog::WORD_COUNT_PAGE),
                      tr("Editor Settings"));

    //: In Polish: Przypisy dolne
    auto* footnoteGroup = new QGroupBox(tr("Footnotes"), this);
    auto* footnoteGrid = new QGridLayout(footnoteGroup);
    m_footnoteFormat = formatList(footnotes.footnotes.format, footnoteGroup);
    m_footnoteNumbering = numberingList(footnotes.footnotes.perChapter, footnoteGroup);
    //: In Polish: &Format
    footnoteGrid->addWidget(fieldLabel(tr("&Format"), m_footnoteFormat, footnoteGroup), 0, 0);
    footnoteGrid->addWidget(m_footnoteFormat, 0, 1);
    //: In Polish: &Numeracja
    footnoteGrid->addWidget(fieldLabel(tr("&Numbering"), m_footnoteNumbering, footnoteGroup),
                            1, 0);
    footnoteGrid->addWidget(m_footnoteNumbering, 1, 1);
    footnoteGrid->setColumnStretch(2, 1);
    contentLayout()->addWidget(footnoteGroup);

    //: In Polish: Przypisy końcowe
    auto* endnoteGroup = new QGroupBox(tr("Endnotes"), this);
    auto* endnoteGrid = new QGridLayout(endnoteGroup);
    m_endnotePlace = new QComboBox(endnoteGroup);
    //: The place of the endnotes. In Polish: Koniec rozdziału
    m_endnotePlace->addItem(tr("End of the chapter"), false);
    //: In Polish: Koniec książki
    m_endnotePlace->addItem(tr("End of the book"), true);
    m_endnotePlace->setCurrentIndex(footnotes.endnotesAtBookEnd ? 1 : 0);
    m_endnoteFormat = formatList(footnotes.endnotes.format, endnoteGroup);
    m_endnoteNumbering = numberingList(footnotes.endnotes.perChapter, endnoteGroup);
    //: In Polish: &Miejsce
    endnoteGrid->addWidget(fieldLabel(tr("&Place"), m_endnotePlace, endnoteGroup), 0, 0);
    endnoteGrid->addWidget(m_endnotePlace, 0, 1);
    //: The second Format, of the endnotes. In Polish: Fo&rmat
    endnoteGrid->addWidget(fieldLabel(tr("Fo&rmat"), m_endnoteFormat, endnoteGroup), 1, 0);
    endnoteGrid->addWidget(m_endnoteFormat, 1, 1);
    //: The second Numbering, of the endnotes. In Polish: Num&eracja
    endnoteGrid->addWidget(fieldLabel(tr("Nu&mbering"), m_endnoteNumbering, endnoteGroup), 2,
                           0);
    endnoteGrid->addWidget(m_endnoteNumbering, 2, 1);
    endnoteGrid->setColumnStretch(2, 1);
    contentLayout()->addWidget(endnoteGroup);

    //: In Polish: Przypisy końcowe z końca książki edytor pokazuje po ostatnim rozdziale.
    //: Ustawienia zapisują się w pliku książki.
    auto* hint = new QLabel(tr("The editor shows the endnotes at the end of the book after the "
                               "last chapter. The settings are saved in the book's file."),
                            this);
    hint->setWordWrap(true);
    hint->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(hint);
    contentLayout()->addStretch(1);

    m_footnoteFormat->setFocus();
}

core::BookFootnotes FootnoteSettingsDialog::footnotes() const
{
    core::BookFootnotes footnotes = m_footnotes;
    footnotes.footnotes.format = m_footnoteFormat->currentData().toString();
    footnotes.footnotes.perChapter = m_footnoteNumbering->currentData().toBool();
    footnotes.endnotes.format = m_endnoteFormat->currentData().toString();
    footnotes.endnotes.perChapter = m_endnoteNumbering->currentData().toBool();
    footnotes.endnotesAtBookEnd = m_endnotePlace->currentData().toBool();
    return footnotes;
}

QString FootnoteSettingsDialog::formatSample(const QString& format)
{
    const editor::NoteNumberFormat shown = editor::noteNumberFormatOf(format);
    QStringList numbers;
    for (int number = 1; number <= 3; ++number) {
        numbers << editor::noteNumberText(number, shown);
    }
    return numbers.join(QStringLiteral(", "));
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
