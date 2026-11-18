// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pitch_panel.cpp
/// @brief The card of a sentence about the book on a tab of its own: its template and the
///        fields of its pattern, the sentence in the editor under them, its length and state

#include "kalahari/gui/panels/pitch_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/widgets/flow_layout.h"
#include "kalahari/gui/widgets/length_meter.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// How long after the last letter typed in a field the fields are saved
constexpr int SAVE_DELAY_MS = 700;

/// The largest own target length
constexpr int MAX_TARGET = 100000;

/// The margins of the card and the gaps between its parts
constexpr int CARD_MARGIN_X = 12;
constexpr int CARD_MARGIN_Y = 10;
constexpr int CARD_SPACING = 8;

/// The title of the sentence, larger than the text around it
constexpr double TITLE_SCALE = 1.2;

/// Between the words and the fields of the pattern, and between their lines
constexpr int PATTERN_SPACING = 4;
constexpr int PATTERN_LINE_SPACING = 6;

/// Before whether the set has the sentence, after its state
constexpr int IN_SET_GAP = 16;

/// The narrowest field of the pattern, in characters
constexpr int FIELD_MIN_CHARACTERS = 12;

/// A heading of a section of the card
QLabel* headingOf(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

/// A hint under a part of the card, in the color of the hints
QLabel* hintOf(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

}  // namespace

PitchPanel::PitchPanel(core::ProjectManager& book, CommandRegistry& commands,
                       const QString& elementId, QWidget* parent)
    : EditorPanel(book, commands, parent), m_book(book), m_elementId(elementId) {
    setObjectName(QStringLiteral("pitchPanel"));
    if (const core::ProjectElement* element = m_book.findElement(elementId)) {
        m_data = element->publishing;
    }
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(SAVE_DELAY_MS);
    connect(m_saveTimer, &QTimer::timeout, this, &PitchPanel::saveFields);
    buildCard();
    showData();
    connect(this, &EditorPanel::contentChanged, this, &PitchPanel::onTextChanged);
    // A change made elsewhere: the state chosen in the list, the set
    connect(&m_book, &core::ProjectManager::projectChanged, this, &PitchPanel::showData);
    connect(&m_book, &core::ProjectManager::publishingSetChanged, this, &PitchPanel::showData);
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged, this,
            [this]() {
                if (auto* icon = findChild<QLabel*>(QStringLiteral("pitchIcon"))) {
                    auto& art = core::ArtProvider::getInstance();
                    const int size = art.getIconSize(core::IconContext::Panel);
                    icon->setPixmap(art.getIcon(PublishingNames::partIconId(m_data.part),
                                                core::IconContext::Panel)
                                        .pixmap(size, size));
                }
            });
}

QLineEdit* PitchPanel::field(const QString& fieldId) const {
    return m_fields.value(fieldId, nullptr);
}

void PitchPanel::buildCard() {
    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("pitchCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(CARD_MARGIN_X, CARD_MARGIN_Y, CARD_MARGIN_X, CARD_MARGIN_Y);
    layout->setSpacing(CARD_SPACING);

    // The header: the icon, the title and what the card is
    auto* header = new QHBoxLayout();
    auto* icon = new QLabel(card);
    icon->setObjectName(QStringLiteral("pitchIcon"));
    auto& art = core::ArtProvider::getInstance();
    const int size = art.getIconSize(core::IconContext::Panel);
    icon->setPixmap(art.getIcon(PublishingNames::partIconId(m_data.part), core::IconContext::Panel)
                        .pixmap(size, size));
    auto* names = new QVBoxLayout();
    names->setSpacing(0);
    m_titleEdit = new QLineEdit(card);
    m_titleEdit->setObjectName(QStringLiteral("pitchTitle"));
    //: The title of a sentence about the book on its card. In Polish: Tytuł
    m_titleEdit->setAccessibleName(tr("Title"));
    QFont titleFont = m_titleEdit->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * TITLE_SCALE);
    m_titleEdit->setFont(titleFont);
    m_titleEdit->setFrame(false);
    auto* what = hintOf(card);
    //: Under the title of a sentence about the book on its card; %1 what it is. In Polish: %1 ·
    //: Teksty dla wydawnictwa
    what->setText(tr("%1 · Texts for Publishers")
                      .arg(PublishingNames::partName(QString::fromLatin1(PublishingTexts::PITCH))));
    names->addWidget(m_titleEdit);
    names->addWidget(what);
    header->addWidget(icon, 0, Qt::AlignTop);
    header->addLayout(names, 1);
    layout->addLayout(header);

    layout->addWidget(createPattern(card));
    layout->addWidget(createSentence(card), 1);
    layout->addWidget(createData(card));

    // The editor of the sentence goes on the card, under the pattern
    editor::BookEditor* sentence = getBookEditor();
    QLayout* own = this->layout();
    own->removeWidget(sentence);
    own->addWidget(card);
    m_sentenceLayout->insertWidget(1, sentence, 1);
    //: The editor of a sentence about the book, for screen readers. In Polish: Zdanie o
    //: książce
    sentence->setAccessibleName(tr("Sentence about the book"));
    connect(m_titleEdit, &QLineEdit::editingFinished, this, &PitchPanel::saveTitle);
}

QWidget* PitchPanel::createPattern(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* top = new QHBoxLayout();
    //: The section of the card of a sentence with the fields of its pattern. In Polish: Wzór
    top->addWidget(headingOf(tr("Pattern"), section));
    top->addStretch(1);
    //: The template of a sentence about the book on its card. In Polish: Sza&blon:
    auto* templateLabel = new QLabel(tr("Te&mplate:"), section);
    m_templateBox = new QComboBox(section);
    m_templateBox->setObjectName(QStringLiteral("pitchTemplate"));
    templateLabel->setBuddy(m_templateBox);
    for (const core::PublishingTemplate& templ : m_book.publishingTemplates()) {
        if (templ.part == QLatin1String(PublishingTexts::PITCH)) {
            m_templateBox->addItem(templ.name.text(programLanguage()), templ.reference());
        }
    }
    m_templateBox->addItem(PublishingNames::noTemplateName(), QString());
    top->addWidget(templateLabel);
    top->addWidget(m_templateBox);
    layout->addLayout(top);
    m_patternArea = new QWidget(section);
    m_patternArea->setObjectName(QStringLiteral("pitchPattern"));
    m_patternLayout = new FlowLayout(m_patternArea, PATTERN_SPACING, PATTERN_LINE_SPACING);
    m_patternLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_patternArea);
    m_patternHint = hintOf(section);
    layout->addWidget(m_patternHint);
    connect(m_templateBox, &QComboBox::activated, this, &PitchPanel::saveTemplate);
    return section;
}

QWidget* PitchPanel::createSentence(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QVBoxLayout(section);
    m_sentenceLayout = layout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    //: The section of the card of a sentence about the book with the sentence. In Polish:
    //: Zdanie
    layout->addWidget(headingOf(tr("Sentence"), section));
    // The editor comes here (buildCard())
    m_meter = new LengthMeter(section);
    layout->addWidget(m_meter);

    m_manualNote = new QWidget(section);
    m_manualNote->setObjectName(QStringLiteral("pitchManual"));
    auto* manual = new QHBoxLayout(m_manualNote);
    manual->setContentsMargins(0, 0, 0, 0);
    //: A sentence about the book changed by hand. In Polish: Zdanie poprawione ręcznie – pola
    //: wzoru już go nie zmieniają.
    auto* manualText = new QLabel(
        tr("The sentence was changed by hand – the fields of the pattern no longer change it."),
        m_manualNote);
    manualText->setWordWrap(true);
    //: Composes the sentence about the book of the fields of its pattern again. In Polish:
    //: Złóż zdanie z &pól
    m_composeButton = new QPushButton(tr("Compose from the &Fields"), m_manualNote);
    m_composeButton->setObjectName(QStringLiteral("pitchCompose"));
    manual->addWidget(manualText, 1);
    manual->addWidget(m_composeButton);
    layout->addWidget(m_manualNote);
    connect(m_composeButton, &QPushButton::clicked, this, [this]() {
        core::PublishingData data = m_data;
        data.manual = false;
        if (save(data)) {
            compose();
        }
    });

    auto* footer = new QHBoxLayout();
    m_targetSource = hintOf(section);
    m_targetSource->setObjectName(QStringLiteral("pitchTargetSource"));
    //: The own target length of a sentence about the book. In Polish: Docelowa dłu&gość:
    auto* targetLabel = new QLabel(tr("Target le&ngth:"), section);
    m_targetSpin = new QSpinBox(section);
    m_targetSpin->setObjectName(QStringLiteral("pitchTarget"));
    m_targetSpin->setRange(0, MAX_TARGET);
    m_targetSpin->setKeyboardTracking(false);
    //: The own target length of a text when it has none, which takes that of its template or
    //: of the Settings. In Polish: –
    m_targetSpin->setSpecialValueText(tr("–"));
    targetLabel->setBuddy(m_targetSpin);
    m_targetUnit = new QLabel(section);
    footer->addWidget(m_targetSource, 1);
    footer->addWidget(targetLabel);
    footer->addWidget(m_targetSpin);
    footer->addWidget(m_targetUnit);
    layout->addLayout(footer);
    connect(m_targetSpin, &QSpinBox::valueChanged, this, &PitchPanel::saveTarget);
    return section;
}

QWidget* PitchPanel::createData(QWidget* parent) {
    auto* section = new QWidget(parent);
    auto* layout = new QHBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    //: The state of a text for publishers on its card. In Polish: &Stan:
    auto* stateLabel = new QLabel(tr("&State:"), section);
    m_stateBox = new QComboBox(section);
    m_stateBox->setObjectName(QStringLiteral("pitchState"));
    stateLabel->setBuddy(m_stateBox);
    for (const char* state : {"", PublishingTexts::CHOSEN, PublishingTexts::REJECTED}) {
        m_stateBox->addItem(PublishingNames::stateName(QString::fromLatin1(state)),
                            QString::fromLatin1(state));
    }
    m_inSetLabel = new QLabel(section);
    m_inSetLabel->setObjectName(QStringLiteral("pitchInSet"));
    layout->addWidget(stateLabel);
    layout->addWidget(m_stateBox);
    layout->addSpacing(IN_SET_GAP);
    layout->addWidget(m_inSetLabel, 1);
    connect(m_stateBox, &QComboBox::activated, this, &PitchPanel::saveState);
    return section;
}

void PitchPanel::buildFields() {
    while (QLayoutItem* item = m_patternLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_fields.clear();
    // The pattern in the language of the book, which the sentence is written in; none without
    // a template
    const core::PublishingTemplate templ = currentTemplate().value_or(core::PublishingTemplate{});
    const QString pattern = templ.pattern.text(m_book.bookLanguage());
    m_patternArea->setVisible(!pattern.isEmpty());
    if (pattern.isEmpty()) {
        //: The card of a sentence about the book without a template. In Polish: Bez szablonu
        //: – zdanie piszesz od razu w polu niżej.
        m_patternHint->setText(tr("Without a template – you write the sentence in the field "
                                  "below."));
        return;
    }
    //: Under the fields of the pattern of a sentence about the book. In Polish: Wpisz pola –
    //: zdanie niżej składa się od razu. Pola przechodzi się klawiszem Tab.
    m_patternHint->setText(tr("Type the fields – the sentence below is composed at once. Tab "
                              "goes through the fields."));
    const bool readOnly = m_book.isReadOnly();
    static const QRegularExpression fieldPattern(QStringLiteral("(\\{\\w+\\})"));
    qsizetype from = 0;
    QRegularExpressionMatchIterator found = fieldPattern.globalMatch(pattern);
    const auto addWords = [this](const QString& words) {
        if (!words.trimmed().isEmpty()) {
            m_patternLayout->addWidget(new QLabel(words.trimmed(), m_patternArea));
        }
    };
    QWidget* previous = m_templateBox;
    while (found.hasNext()) {
        const QRegularExpressionMatch match = found.next();
        addWords(pattern.mid(from, match.capturedStart() - from));
        from = match.capturedEnd();
        const QString id = match.captured(1).mid(1, match.capturedLength(1) - 2);
        const auto definition =
            std::find_if(templ.fields.cbegin(), templ.fields.cend(),
                         [&id](const core::PublishingTemplateField& each) {
                             return each.id == id;
                         });
        if (definition == templ.fields.cend() || m_fields.contains(id)) {
            continue;
        }
        auto* box = new QWidget(m_patternArea);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(0);
        auto* edit = new QLineEdit(box);
        edit->setObjectName(QStringLiteral("pitchField_") + id);
        edit->setPlaceholderText(definition->hint.text(programLanguage()));
        edit->setAccessibleName(definition->name.text(programLanguage()));
        edit->setText(m_data.fields.value(id));
        edit->setCursorPosition(0);  // a value longer than the field shows from its start
        edit->setReadOnly(readOnly);
        edit->setMinimumWidth(edit->fontMetrics().averageCharWidth() * FIELD_MIN_CHARACTERS);
        auto* caption = hintOf(box);
        caption->setText(definition->name.text(programLanguage()));
        boxLayout->addWidget(edit);
        boxLayout->addWidget(caption);
        m_patternLayout->addWidget(box);
        m_fields.insert(id, edit);
        setTabOrder(previous, edit);
        previous = edit;
        connect(edit, &QLineEdit::textEdited, this, [this, id](const QString& text) {
            m_data.fields.insert(id, text);
            if (!m_data.manual) {
                compose();
            }
            m_saveTimer->start();
        });
        connect(edit, &QLineEdit::editingFinished, this, &PitchPanel::saveFields);
    }
    addWords(pattern.mid(from));
}

void PitchPanel::showData() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    const bool readOnly = m_book.isReadOnly();
    // The fields typed and not yet saved stay
    const QMap<QString, QString> typed = m_data.fields;
    const QString shownTemplate = m_data.templateId;
    m_data = element->publishing;
    if (m_saveTimer->isActive()) {
        m_data.fields = typed;
    }
    if (!m_titleEdit->hasFocus() && m_titleEdit->text() != element->title) {
        m_titleEdit->setText(element->title);
    }
    m_titleEdit->setReadOnly(readOnly);
    showPattern(shownTemplate);
    showState();
    updateLength();
}

void PitchPanel::showPattern(const QString& shownTemplate) {
    {
        const QSignalBlocker blocker(m_templateBox);
        int index = m_templateBox->findData(m_data.templateId);
        if (index < 0) {
            // A template the book does not have: named as it is written
            m_templateBox->addItem(m_data.templateId, m_data.templateId);
            index = m_templateBox->count() - 1;
        }
        m_templateBox->setCurrentIndex(index);
    }
    m_templateBox->setEnabled(!m_book.isReadOnly());
    if (m_fields.isEmpty() || shownTemplate != m_data.templateId) {
        buildFields();
    } else {
        for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) {
            const QString value = m_data.fields.value(it.key());
            if (!it.value()->hasFocus() && it.value()->text() != value) {
                it.value()->setText(value);
                it.value()->setCursorPosition(0);
            }
        }
    }
}

void PitchPanel::showState() {
    const bool readOnly = m_book.isReadOnly();
    {
        const QSignalBlocker blocker(m_stateBox);
        const int index = m_stateBox->findData(m_data.state);
        if (index < 0) {
            m_stateBox->addItem(PublishingNames::stateName(m_data.state), m_data.state);
        }
        m_stateBox->setCurrentIndex(std::max(0, m_stateBox->findData(m_data.state)));
    }
    m_stateBox->setEnabled(!readOnly);
    const bool words = core::CoverMaterial::countsWords();
    {
        const QSignalBlocker blocker(m_targetSpin);
        m_targetSpin->setValue(words ? m_data.targetWords : m_data.targetCharacters);
    }
    m_targetSpin->setReadOnly(readOnly);
    m_targetUnit->setText(CoverNames::unitName(words));
    const QList<core::PublishingSetPart> parts = m_book.publishingSetParts();
    const bool inSet = std::any_of(parts.cbegin(), parts.cend(), [this](const auto& part) {
        return part.elementId == m_elementId;
    });
    //: Whether a text for publishers is in the set. In Polish: W zestawie: tak – ta wersja /
    //: W zestawie: nie
    m_inSetLabel->setText(inSet ? tr("In the set: yes – this version")
                                : tr("In the set: no"));
}

void PitchPanel::updateLength() {
    const QString text = getBookEditor()->plainText();
    const bool words = core::CoverMaterial::countsWords();
    const int target = m_book.publishingTargetOf(m_elementId, words);
    m_meter->setLength(PublishingTexts::lengthOf(text, false),
                       PublishingTexts::lengthOf(text, true), target, words);
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    const bool pattern = templ && !templ->pattern.text(m_book.bookLanguage()).isEmpty();
    m_manualNote->setVisible(pattern && m_data.manual);
    m_composeButton->setEnabled(!m_book.isReadOnly());
    // Where the target comes from
    m_targetSource->setText(PublishingNames::targetSourceText(
        m_data, templ ? &*templ : nullptr, target, words));
}

std::optional<core::PublishingTemplate> PitchPanel::currentTemplate() const {
    if (m_data.templateId.isEmpty()) {
        return std::nullopt;
    }
    return m_book.publishingTemplate(m_data.templateId);
}

QString PitchPanel::composed() const {
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    if (!templ) {
        return {};
    }
    const bool any = std::any_of(m_data.fields.cbegin(), m_data.fields.cend(),
                                 [](const QString& value) { return !value.trimmed().isEmpty(); });
    return any ? PublishingTexts::composePitch(templ->pattern.text(m_book.bookLanguage()),
                                               m_data.fields)
               : QString();
}

void PitchPanel::compose() {
    const QString sentence = composed();
    if (sentence.isEmpty() || getBookEditor()->plainText().trimmed() == sentence) {
        updateLength();
        return;
    }
    m_composing = true;
    getBookEditor()->replaceWithKml(editor::ClipboardHandler::textToKml(sentence));
    m_composing = false;
    updateLength();
}

void PitchPanel::onTextChanged() {
    updateLength();
    if (m_composing || m_data.manual || m_book.isReadOnly()) {
        return;
    }
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    if (!templ || templ->pattern.text(m_book.bookLanguage()).isEmpty()) {
        return;
    }
    const QString text = getBookEditor()->plainText().trimmed();
    if (text == composed()) {
        return;
    }
    // Changed by hand: the fields no longer make it
    core::PublishingData data = m_data;
    data.manual = true;
    save(data);
    updateLength();
}

bool PitchPanel::save(const core::PublishingData& data) {
    if (m_book.isReadOnly()) {
        return false;
    }
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return false;
    }
    m_data = data;
    if (element->publishing == data) {
        return true;
    }
    if (!m_book.setPublishing(m_elementId, data)) {
        return false;
    }
    emit publishingChanged(m_elementId);
    return true;
}

void PitchPanel::saveFields() {
    m_saveTimer->stop();
    save(m_data);
}

void PitchPanel::saveTitle() {
    const QString title = m_titleEdit->text().trimmed();
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || title.isEmpty() || title == element->title ||
        m_book.isReadOnly()) {
        if (element != nullptr && title.isEmpty()) {
            m_titleEdit->setText(element->title);
        }
        return;
    }
    if (m_book.renameElement(m_elementId, title)) {
        emit titleChanged(m_elementId);
    }
}

void PitchPanel::saveTemplate() {
    core::PublishingData data = m_data;
    data.templateId = m_templateBox->currentData().toString();
    if (data.templateId == m_data.templateId) {
        return;
    }
    // The values of the fields stay, for the fields of the same names
    if (save(data)) {
        buildFields();
        if (!m_data.manual) {
            compose();
        }
        updateLength();
    }
}

void PitchPanel::saveTarget() {
    core::PublishingData data = m_data;
    (core::CoverMaterial::countsWords() ? data.targetWords : data.targetCharacters) =
        m_targetSpin->value();
    save(data);
    updateLength();
}

void PitchPanel::saveState() {
    const QString state = m_stateBox->currentData().toString();
    if (state == m_data.state || m_book.isReadOnly()) {
        return;
    }
    if (m_book.setPublishingState(m_elementId, state)) {
        emit publishingChanged(m_elementId);
    }
    showData();
}

}  // namespace kalahari::gui
