/// @file annotations_page.cpp
/// @brief Settings page: Annotations
///
/// The comments, to-dos and notes: who new ones are by, how large their marks in the text are,
/// their cards under the paragraphs (the editors follow these by themselves: EditorPanel
/// subscribes to them) and their colors (they reach the editors and the panel through the
/// theme: ThemeManager follows them).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/annotation.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// The size of the cards' text, in percent of the chapter's text
constexpr int MIN_CARD_FONT = 60;
constexpr int MAX_CARD_FONT = 130;
constexpr int CARD_FONT_STEP = 5;

/// How much of the kind's color the cards have, in percent of the usual
constexpr int MIN_CARD_TINT = 50;
constexpr int MAX_CARD_TINT = 200;
constexpr int CARD_TINT_STEP = 10;

} // namespace

AnnotationsPage::AnnotationsPage(QWidget* parent)
    : SettingsPage(parent)
{
    // New ones only: an annotation keeps the author it was written by
    QFormLayout* added = addGroup(tr("New Annotations"));
    auto* author = new QLineEdit();
    author->setObjectName(QStringLiteral("annotationsAuthor"));
    author->setPlaceholderText(tr("Your name or pen name"));
    addField(added, tr("Author:"), author, "annotations.author");
    addNote(added, tr("The name shown on new annotations. Without it, they have no author."));

    QFormLayout* marks = addGroup(tr("Marks in the Text"));
    auto* markSize = new QSpinBox();
    markSize->setObjectName(QStringLiteral("annotationsMarkSize"));
    markSize->setRange(50, 300);
    markSize->setSingleStep(10);
    markSize->setSuffix(tr(" %"));
    markSize->setToolTip(tr("Size of the marks of comments, to-dos and notes in the text "
                            "(100% suits the text's font)"));
    addField(marks, tr("Mark Size:"), markSize, "editor.annotationMarkSize");

    addCardSettings();

    // One color for each kind, kept for each theme
    QFormLayout* colors = addGroup(tr("Colors"));
    addNote(colors, tr("The colors of the annotations' marks in the text, of their cards under "
                       "the paragraphs, of the frame an annotation is written in on the paper and "
                       "of the cards in the Annotations panel. Each theme has its own colors: "
                       "these are the colors of the current theme."));
    using editor::AnnotationKind;
    addThemeColor(colors, tr("Comment"), editor::annotationColorKey(AnnotationKind::Comment));
    addThemeColor(colors, tr("To-do"), editor::annotationColorKey(AnnotationKind::Todo));
    addThemeColor(colors, tr("Note"), editor::annotationColorKey(AnnotationKind::Note));
    addOptimizeColorsCheckBox(colors);
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

void AnnotationsPage::addCardSettings()
{
    // A second way of showing the annotations in the text: a card under the paragraph of
    // each one's mark, in the continuous view
    QFormLayout* cards = addGroup(tr("Cards Under Paragraphs (Continuous View)"));
    QCheckBox* show = addCheckBox(cards, tr("Show annotation cards under paragraphs"),
                                  "editor.annotationCards.show");
    show->setObjectName(QStringLiteral("annotationCardsShow"));
    alignWithText(addNote(cards, tr("Same as View > Annotation Cards Under Paragraphs. In the "
                                    "page view annotations are always marks only.")),
                  show);

    // Of which kinds: one under another, as the three fit no narrow window side by side
    auto* kinds = new QWidget();
    auto* kindsLayout = new QVBoxLayout(kinds);
    kindsLayout->setContentsMargins(0, 0, 0, 0);
    const auto addKind = [this, kindsLayout](const QString& text, const QString& name,
                                             const std::string& key) {
        auto* box = new QCheckBox(text);
        box->setObjectName(name);
        kindsLayout->addWidget(box);
        bind(box, key);
        return box;
    };
    QCheckBox* comments = addKind(tr("comments"), QStringLiteral("annotationCardsComments"),
                                  "editor.annotationCards.comments");
    addKind(tr("to-dos"), QStringLiteral("annotationCardsTodos"), "editor.annotationCards.todos");
    addKind(tr("notes"), QStringLiteral("annotationCardsNotes"), "editor.annotationCards.notes");
    auto* kindsLabel = new QLabel(tr("Cards for:"));
    kindsLabel->setBuddy(comments);
    cards->addRow(kindsLabel, kinds);

    // Two looks of a card, two of a collapsed one: a setting each, one of two words. Short
    // texts with a note under each, which wraps in a narrow window
    struct Choice {
        const char* value;  ///< The setting's word
        QString text;       ///< The button
        QString note;       ///< What it means, under the button
    };
    auto& settings = core::SettingsManager::getInstance();
    const auto addChoice = [this, cards, &settings](const QString& label, const std::string& key,
                                                    const Choice& firstChoice,
                                                    const Choice& secondChoice,
                                                    const QString& name) {
        auto* choices = new QWidget();
        auto* layout = new QVBoxLayout(choices);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* first = new QRadioButton(firstChoice.text);
        first->setObjectName(name + QStringLiteral("First"));
        auto* second = new QRadioButton(secondChoice.text);
        second->setObjectName(name + QStringLiteral("Second"));
        layout->addWidget(first);
        alignWithText(addNote(layout, firstChoice.note), first);
        layout->addWidget(second);
        alignWithText(addNote(layout, secondChoice.note), second);
        auto* choicesLabel = new QLabel(label);
        choicesLabel->setBuddy(first);
        cards->addRow(choicesLabel, choices);

        const char* firstValue = firstChoice.value;
        const char* secondValue = secondChoice.value;
        Binding binding;
        binding.key = [key]() { return key; };
        binding.stored = [&settings, key, firstValue]() {
            return json(settings.get<std::string>(key, firstValue));
        };
        binding.shown = [second, firstValue, secondValue]() {
            return json(second->isChecked() ? secondValue : firstValue);
        };
        binding.show = [first, second, secondValue](const json& value) {
            const bool isSecond = value.is_string() && value.get<std::string>() == secondValue;
            (isSecond ? second : first)->setChecked(true);
        };
        binding.store = [&settings, key](const json& value) {
            settings.set<std::string>(key, value.get<std::string>());
        };
        bindCustom(std::move(binding));
    };
    addChoice(tr("Card look:"), "editor.annotationCards.look",
              {"full", tr("Full"), tr("The kind, the author and the date above the text.")},
              {"compact", tr("Compact"), tr("The text alone, the author and the date at its end.")},
              QStringLiteral("annotationCardsLook"));
    addChoice(tr("Collapsed annotation:"), "editor.annotationCards.collapsedLook",
              {"mark", tr("Mark only"), tr("Its mark in the text alone, as without the cards.")},
              {"line", tr("One line"), tr("A line with the start of its text under the paragraph.")},
              QStringLiteral("annotationCardsCollapsed"));

    auto* afterOpening = new QComboBox();
    afterOpening->setObjectName(QStringLiteral("annotationCardsAfterOpening"));
    afterOpening->addItem(tr("expanded"), QStringLiteral("expanded"));
    afterOpening->addItem(tr("collapsed"), QStringLiteral("collapsed"));
    addField(cards, tr("After a chapter opens, annotations are:"), afterOpening,
             "editor.annotationCards.afterOpening");

    auto* longLines = new QComboBox();
    longLines->setObjectName(QStringLiteral("annotationCardsLongLines"));
    longLines->addItem(tr("show whole"), 0);
    longLines->addItem(tr("cut short to 3 lines"), 3);
    longLines->addItem(tr("cut short to 6 lines"), 6);
    addField(cards, tr("Long annotations:"), longLines, "editor.annotationCards.longLines");

    auto* fontSize = new QSpinBox();
    fontSize->setObjectName(QStringLiteral("annotationCardsFontSize"));
    fontSize->setRange(MIN_CARD_FONT, MAX_CARD_FONT);
    fontSize->setSingleStep(CARD_FONT_STEP);
    fontSize->setSuffix(tr(" % of the chapter's text"));
    addField(cards, tr("Card text size:"), fontSize, "editor.annotationCards.fontSize");

    // How much of the kind's color the cards have: from subtle to strong
    auto* tintRow = new QWidget();
    auto* tintLayout = new QHBoxLayout(tintRow);
    tintLayout->setContentsMargins(0, 0, 0, 0);
    auto* tint = new QSlider(Qt::Horizontal);
    tint->setObjectName(QStringLiteral("annotationCardsTint"));
    tint->setRange(MIN_CARD_TINT, MAX_CARD_TINT);
    tint->setSingleStep(CARD_TINT_STEP);
    tint->setPageStep(CARD_TINT_STEP);
    auto* subtle = new QLabel(tr("subtle"));
    auto* strong = new QLabel(tr("strong"));
    tintLayout->addWidget(subtle);
    tintLayout->addWidget(tint, 1);
    tintLayout->addWidget(strong);
    auto* tintLabel = new QLabel(tr("Card background:"));
    tintLabel->setBuddy(tint);
    tint->setAccessibleName(tr("Card background"));
    tint->setAccessibleDescription(tr("From subtle to strong"));
    cards->addRow(tintLabel, tintRow);
    Binding tintBinding;
    tintBinding.key = []() { return std::string("editor.annotationCards.tint"); };
    tintBinding.stored = [&settings]() {
        return json(settings.get<int>("editor.annotationCards.tint", 100));
    };
    tintBinding.shown = [tint]() { return json(tint->value()); };
    tintBinding.show = [tint](const json& value) {
        if (value.is_number()) {
            tint->setValue(static_cast<int>(value.get<double>()));
        }
    };
    tintBinding.store = [&settings](const json& value) {
        settings.set<int>("editor.annotationCards.tint", value.get<int>());
    };
    bindCustom(std::move(tintBinding));

    QCheckBox* highlight =
        addCheckBox(cards, tr("Highlight the fragment when pointing at a card"),
                    "editor.annotationCards.highlightFragment");
    highlight->setObjectName(QStringLiteral("annotationCardsHighlight"));

    auto* distractionFree = new QComboBox();
    distractionFree->setObjectName(QStringLiteral("annotationCardsDistractionFree"));
    distractionFree->addItem(tr("show the cards"), QStringLiteral("show"));
    distractionFree->addItem(tr("hide the cards"), QStringLiteral("hide"));
    addField(cards, tr("In Distraction-Free:"), distractionFree,
             "editor.annotationCards.distractionFree");
}

} // namespace gui
} // namespace kalahari
