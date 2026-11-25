// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_text_box.cpp
/// @brief The group of a text for publishers in the Properties panel

#include "kalahari/gui/panels/publishing_text_box.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/volume_materials.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_points.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/volume_names.h"
#include "kalahari/gui/widgets/length_meter.h"
#include "kalahari/gui/widgets/publishing_template_editor.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// The largest own target length
constexpr int MAX_TARGET = 100000;

/// The settings of the texts for publishers: the targets, the pages, the writer's templates
constexpr std::string_view PUBLISHING_SETTINGS = "materials.publishing.";

/// How far the hint of a point is from the left edge of its check box, and from the next point
constexpr int HINT_INDENT = 22;
constexpr int HINT_BOTTOM = 4;

/// The margins and the spacing of the group, as of the other groups of the panel
constexpr int MARGIN = 11;
constexpr int SPACING = 6;

/// The space over the template and over the data, and between the points
constexpr int SECTION_GAP = 4;
constexpr int POINT_SPACING = 2;

/// The side of the number of a point in its color, and the space after it
constexpr int SWATCH_SIZE = 18;
constexpr int SWATCH_GAP = 6;

/// A color of a point lighter than this (QColor::lightness()) has its number in the darker
/// of the colors of the text and of its background, a darker one in the lighter
constexpr int LIGHT_SWATCH = 150;

/// The data of New Template... on the list of the templates, which is no template
constexpr const char* NEW_TEMPLATE = "*new*";

/// The points a menu of Link to a Point gives a digit as its key
constexpr int DIGIT_KEYS = 9;

/// A hint in the color of the hints, its lines wrapped
QLabel* hintOf(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setWordWrap(true);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

}  // namespace

PublishingTextBox::PublishingTextBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent),
      m_book(book),
      m_meter(new LengthMeter(this)),
      m_targetSource(hintOf(this)),
      m_templateBox(new QComboBox(this)),
      m_changeTemplate(new QToolButton(this)),
      m_templateEditor(new PublishingTemplateEditor(this)),
      m_pointsArea(new QWidget(this)),
      m_pointsLayout(new QVBoxLayout(m_pointsArea)),
      m_pointsHint(hintOf(this)),
      m_volumeBox(new QComboBox(this)),
      m_stateBox(new QComboBox(this)),
      m_targetSpin(new QSpinBox(this)),
      m_targetUnit(new QLabel(this)),
      m_inSetLabel(new QLabel(this)) {
    setObjectName(QStringLiteral("publishingTextBox"));
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(SPACING);
    layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    layout->addWidget(m_meter);
    m_targetSource->setObjectName(QStringLiteral("publishingTargetSource"));
    layout->addWidget(m_targetSource);

    auto* templateRow = new QFormLayout();
    templateRow->setContentsMargins(0, SECTION_GAP, 0, 0);
    m_templateBox->setObjectName(QStringLiteral("publishingTemplate"));
    m_changeTemplate->setObjectName(QStringLiteral("publishingChangeTemplate"));
    m_changeTemplate->setAutoRaise(true);
    //: The pencil beside the template of a text for publishers: its name, length and points
    //: in place of the points. In Polish: Zmień szablon
    m_changeTemplate->setToolTip(tr("Change Template"));
    m_changeTemplate->setAccessibleName(m_changeTemplate->toolTip());
    card::keepIcon(m_changeTemplate, QStringLiteral("publishing.changeTemplate"),
                   core::IconContext::Menu);
    auto* templateLine = new QHBoxLayout();
    templateLine->addWidget(m_templateBox, 1);
    templateLine->addWidget(m_changeTemplate);
    //: The template of a text for publishers in the Properties panel. In Polish: Sza&blon:
    auto* templateLabel = new QLabel(tr("Te&mplate:"), this);
    templateLabel->setBuddy(m_templateBox);
    templateRow->addRow(templateLabel, templateLine);
    layout->addLayout(templateRow);
    m_templateEditor->hide();
    layout->addWidget(m_templateEditor);

    // Over the points: how many the text covers, Show in the Text and the gear
    m_pointsHeader = new QWidget(this);
    auto* header = new QHBoxLayout(m_pointsHeader);
    header->setContentsMargins(0, 0, 0, 0);
    m_pointsSummary = hintOf(m_pointsHeader);
    m_pointsSummary->setObjectName(QStringLiteral("publishingPointsSummary"));
    header->addWidget(m_pointsSummary, 1);
    m_showInText = new QToolButton(m_pointsHeader);
    m_showInText->setObjectName(QStringLiteral("publishingShowInText"));
    m_showInText->setCheckable(true);
    m_showInText->setAutoRaise(true);
    //: The button over the points of a synopsis that fills the fragments of all of them in the
    //: text while it is on. In Polish: Pokaż w tekście
    m_showInText->setToolTip(tr("Show in the Text"));
    m_showInText->setAccessibleName(m_showInText->toolTip());
    card::keepIcon(m_showInText, QStringLiteral("common.preview"), core::IconContext::Menu);
    header->addWidget(m_showInText);
    m_settingsButton = new QToolButton(m_pointsHeader);
    m_settingsButton->setObjectName(QStringLiteral("publishingPointsSettings"));
    m_settingsButton->setAutoRaise(true);
    //: The gear over the points of a synopsis. In Polish: Ustawienia tekstów dla wydawnictwa
    m_settingsButton->setToolTip(tr("Settings of Texts for Publishers"));
    m_settingsButton->setAccessibleName(m_settingsButton->toolTip());
    card::keepIcon(m_settingsButton, QStringLiteral("dock.settings"), core::IconContext::Menu);
    header->addWidget(m_settingsButton);
    layout->addWidget(m_pointsHeader);
    connect(m_showInText, &QToolButton::toggled, this, &PublishingTextBox::applyLook);
    connect(m_settingsButton, &QToolButton::clicked, this, &PublishingTextBox::settingsRequested);
    // The colors of the points follow the theme
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::editorColorsChanged, this,
            [this]() {
                updatePointStates();
                applyLook();
            });

    m_pointsArea->setObjectName(QStringLiteral("publishingPoints"));
    m_pointsLayout->setContentsMargins(0, 0, 0, 0);
    m_pointsLayout->setSpacing(POINT_SPACING);
    layout->addWidget(m_pointsArea);
    m_pointsHint->setObjectName(QStringLiteral("publishingPointsHint"));
    layout->addWidget(m_pointsHint);

    auto* fields = new QFormLayout();
    m_fields = fields;
    fields->setContentsMargins(0, SECTION_GAP, 0, 0);
    m_volumeBox->setObjectName(QStringLiteral("publishingVolume"));
    //: The volume of a series a text for publishers is for, or the whole series, in the
    //: Properties panel. In Polish: &Tom:
    fields->addRow(tr("&Volume:"), m_volumeBox);
    m_stateBox->setObjectName(QStringLiteral("publishingState"));
    for (const char* state : {"", PublishingTexts::CHOSEN, PublishingTexts::REJECTED}) {
        m_stateBox->addItem(PublishingNames::stateName(QString::fromLatin1(state)),
                            QString::fromLatin1(state));
    }
    //: The state of a text for publishers in the Properties panel. In Polish: &Stan:
    fields->addRow(tr("&State:"), m_stateBox);
    auto* target = new QHBoxLayout();
    m_targetSpin->setObjectName(QStringLiteral("publishingTarget"));
    m_targetSpin->setRange(0, MAX_TARGET);
    m_targetSpin->setKeyboardTracking(false);
    //: The own target length of a text when it has none, which takes that of its template or
    //: of the Settings. In Polish: –
    m_targetSpin->setSpecialValueText(tr("–"));
    target->addWidget(m_targetSpin, 1);
    target->addWidget(m_targetUnit);
    //: The own target length of a text for publishers in the Properties panel. In Polish:
    //: Docelowa dłu&gość:
    auto* targetLabel = new QLabel(tr("Target le&ngth:"));
    targetLabel->setBuddy(m_targetSpin);
    fields->addRow(targetLabel, target);
    m_inSetLabel->setObjectName(QStringLiteral("publishingInSet"));
    fields->addRow(m_inSetLabel);
    layout->addLayout(fields);

    connect(m_templateBox, &QComboBox::activated, this, &PublishingTextBox::saveTemplate);
    connect(m_stateBox, &QComboBox::activated, this, &PublishingTextBox::saveState);
    connect(m_volumeBox, &QComboBox::activated, this, &PublishingTextBox::saveVolume);
    connect(m_targetSpin, &QSpinBox::valueChanged, this, &PublishingTextBox::saveTarget);
    connect(m_changeTemplate, &QToolButton::clicked, this, &PublishingTextBox::changeTemplate);
    connect(m_templateEditor, &PublishingTemplateEditor::kept, this,
            &PublishingTextBox::keepTemplate);
    connect(m_templateEditor, &PublishingTemplateEditor::dropped, this,
            &PublishingTextBox::stopEditingTemplate);
    // The targets of the Settings and the writer's templates: the listener runs on the thread
    // that changes them
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.starts_with(PUBLISHING_SETTINGS)) {
                QMetaObject::invokeMethod(
                    this,
                    [this]() {
                        if (!m_elementId.isEmpty()) {
                            // The names of the templates and of their points too
                            fillTemplates();
                            m_pointIds.clear();
                            showData();
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    // A change made elsewhere: the state chosen in the list, the set, another template
    for (auto signal : {&core::ProjectManager::projectChanged,
                        &core::ProjectManager::publishingSetChanged}) {
        connect(&m_book, signal, this, [this]() {
            if (!m_elementId.isEmpty()) {
                showData();
            }
        });
    }
    hide();
}

PublishingTextBox::~PublishingTextBox() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
    followEditor(nullptr);
}

void PublishingTextBox::showText(const QString& elementId, editor::BookEditor* editor) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    m_editor = editor;
    if (elementId != m_elementId) {
        stopEditingTemplate();  // the changes of a template of another text are dropped
    }
    if (element == nullptr || element->publishing.part.isEmpty()) {
        m_elementId.clear();
        followEditor(nullptr);
        hide();
        return;
    }
    const bool other = elementId != m_elementId || element->publishing.part != m_data.part;
    m_elementId = elementId;
    m_data = element->publishing;
    if (other) {
        fillTemplates();
        m_nextFragment.clear();
    }
    followEditor(editor);
    showData();
    show();
}

void PublishingTextBox::fillTemplates() {
    const QSignalBlocker blocker(m_templateBox);
    m_templateBox->clear();
    for (const core::PublishingTemplate& templ : m_book.publishingTemplates()) {
        if (templ.part == m_data.part) {
            m_templateBox->addItem(templ.name.text(programLanguage()), templ.reference());
        }
    }
    m_templateBox->addItem(PublishingNames::noTemplateName(), QString());
    m_templateBox->insertSeparator(m_templateBox->count());
    //: The last item of the list of the templates of a text for publishers: a template of the
    //: writer from scratch. In Polish: Nowy szablon...
    m_templateBox->addItem(tr("New Template..."), QString::fromLatin1(NEW_TEMPLATE));
}

void PublishingTextBox::showData() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || element->publishing.part.isEmpty()) {
        m_elementId.clear();
        followEditor(nullptr);
        hide();
        return;
    }
    m_data = element->publishing;
    m_data.done = m_book.publishingPointsOf(m_elementId);  // also those waiting for the text
    setTitle(PublishingNames::partName(m_data.part));
    const bool readOnly = m_book.isReadOnly();
    {
        const QSignalBlocker blocker(m_templateBox);
        if (m_templateBox->findData(m_data.templateId) < 0) {
            // A template the book does not have: named as it is written
            m_templateBox->insertItem(m_templateBox->findData(QString()), m_data.templateId,
                                      m_data.templateId);
        }
        m_templateBox->setCurrentIndex(m_templateBox->findData(m_data.templateId));
    }
    m_templateBox->setEnabled(!readOnly && m_templateEditor->isHidden());
    m_changeTemplate->setEnabled(!readOnly && m_templateEditor->isHidden());
    showPoints();
    {
        const QSignalBlocker blocker(m_stateBox);
        if (m_stateBox->findData(m_data.state) < 0) {
            m_stateBox->addItem(PublishingNames::stateName(m_data.state), m_data.state);
        }
        m_stateBox->setCurrentIndex(m_stateBox->findData(m_data.state));
    }
    m_stateBox->setEnabled(!readOnly);
    showVolume(*element);
    const bool words = core::CoverMaterial::countsWords();
    {
        const QSignalBlocker blocker(m_targetSpin);
        m_targetSpin->setValue(words ? m_data.targetWords : m_data.targetCharacters);
    }
    m_targetSpin->setReadOnly(readOnly);
    m_targetUnit->setText(CoverNames::unitName(words));
    const QList<core::PublishingSetPart> parts = m_book.publishingSetPartsOf(m_elementId);
    const bool inSet = std::any_of(parts.cbegin(), parts.cend(), [this](const auto& part) {
        return part.elementId == m_elementId;
    });
    //: Whether a text for publishers is in the set. In Polish: W zestawie: tak – ta wersja /
    //: W zestawie: nie
    m_inSetLabel->setText(inSet ? tr("In the set: yes – this version") : tr("In the set: no"));
    updateLength();
}

void PublishingTextBox::showSaved(const QString& elementId) {
    if (!m_elementId.isEmpty() && elementId == m_elementId) {
        showData();
    }
}

void PublishingTextBox::showPoints() {
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    QList<core::PublishingTemplatePoint> points;
    if (templ && PublishingTexts::hasPoints(m_data.part)) {
        points = templ->pointsFor(m_book.mainTexts());
    }
    QStringList ids;
    for (const core::PublishingTemplatePoint& point : points) {
        ids << point.id;
    }
    if (ids != m_pointIds) {
        buildPoints(points, templ && templ->eachText);
        m_pointIds = ids;
        if (!m_pointIds.contains(m_chosenPoint)) {
            m_chosenPoint.clear();
        }
    }
    m_templatePoints = points;
    const bool readOnly = m_book.isReadOnly();
    for (qsizetype i = 0; i < m_points.size(); ++i) {
        const QSignalBlocker blocker(m_points.at(i));
        m_points.at(i)->setChecked(m_data.done.contains(m_pointIds.at(i)));
        m_points.at(i)->setEnabled(!readOnly);
    }
    // While its template is changed in their place, the points are hidden
    const bool shown = !m_points.isEmpty() && m_templateEditor->isHidden();
    m_pointsArea->setVisible(shown);
    m_pointsHeader->setVisible(shown);
    updatePointStates();
    applyLook();
    if (!PublishingTexts::hasPoints(m_data.part)) {
        //: Beside a sentence about the book on the card For Publishers. In Polish: Pola wzoru
        //: są nad zdaniem.
        m_pointsHint->setText(tr("The fields of its pattern are over the sentence."));
    } else if (m_points.isEmpty()) {
        //: The Properties panel of a synopsis without a template. In Polish: Tekst bez
        //: szablonu. Wybierz szablon, a jego punkty pokażą się tutaj.
        m_pointsHint->setText(
            tr("A text without a template. Choose a template and its points show here."));
    } else {
        //: Under the points of the template of a synopsis on the card For Publishers. In
        //: Polish: Zaznacz fragment tekstu i naciśnij Ctrl+Shift+D, aby powiązać go z punktem.
        //: Punkty odhaczasz sam (Spacja). W punkcie Enter wybiera jego następny fragment,
        //: Insert wiąże z nim zaznaczenie, Delete usuwa jego powiązania, a Esc wraca do tekstu.
        m_pointsHint->setText(
            tr("Select a fragment of the text and press Ctrl+Shift+D to link it to a point. "
               "You tick off the points yourself (Space). In a point Enter selects its next "
               "fragment, Insert links the selection to it, Delete takes its links away, and "
               "Esc takes the keys back to the text."));
    }
}

void PublishingTextBox::buildPoints(const QList<core::PublishingTemplatePoint>& points,
                                    bool eachText) {
    utils::clearLayout(m_pointsLayout);
    m_points.clear();
    m_pointStates.clear();
    m_pointSwatches.clear();
    m_linkButtons.clear();
    m_unlinkButtons.clear();
    // A point for each text has the hint of its template, once over them
    if (eachText && !points.isEmpty()) {
        QLabel* hint = hintOf(m_pointsArea);
        hint->setText(points.constFirst().hint.text(programLanguage()));
        m_pointsLayout->addWidget(hint);
    }
    for (qsizetype i = 0; i < points.size(); ++i) {
        const core::PublishingTemplatePoint& point = points.at(i);
        const QString id = point.id;
        const QString hint = point.hint.text(programLanguage());
        // Its number in its color, the check box of its name and its two buttons; under the
        // name its hint and its fragments in the text
        auto* grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, HINT_BOTTOM);
        grid->setHorizontalSpacing(SWATCH_GAP);
        grid->setVerticalSpacing(0);
        grid->setColumnStretch(1, 1);
        auto* swatch = new QLabel(QString::number(i + 1), m_pointsArea);
        swatch->setObjectName(QStringLiteral("publishingPointColor_") + id);
        swatch->setFixedSize(SWATCH_SIZE, SWATCH_SIZE);
        swatch->setAlignment(Qt::AlignCenter);
        grid->addWidget(swatch, 0, 0, Qt::AlignTop);
        auto* box = new QCheckBox(point.name.text(programLanguage()), m_pointsArea);
        box->setObjectName(QStringLiteral("publishingPoint_") + id);
        box->setAccessibleDescription(hint);
        box->installEventFilter(this);
        grid->addWidget(box, 0, 1);
        auto* link = new QToolButton(m_pointsArea);
        link->setObjectName(QStringLiteral("publishingPointLink_") + id);
        link->setAutoRaise(true);
        link->setFocusPolicy(Qt::NoFocus);  // Insert in the point does the same
        //: The button of a point of a synopsis. In Polish: Powiąż zaznaczony fragment tekstu z
        //: tym punktem (Insert)
        link->setToolTip(tr("Link the selected fragment of the text to this point (Insert)"));
        link->setAccessibleName(link->toolTip());
        card::keepIcon(link, QStringLiteral("tools.publishing.linkPoint"),
                       core::IconContext::Menu);
        grid->addWidget(link, 0, 2);
        auto* unlink = new QToolButton(m_pointsArea);
        unlink->setObjectName(QStringLiteral("publishingPointUnlink_") + id);
        unlink->setAutoRaise(true);
        unlink->setFocusPolicy(Qt::NoFocus);  // Delete in the point does the same
        //: The button of a point of a synopsis. In Polish: Usuń powiązania fragmentów tekstu z
        //: tym punktem; tekst zostaje (Delete)
        unlink->setToolTip(
            tr("Take the links of the fragments of the text to this point away; the text stays "
               "(Delete)"));
        unlink->setAccessibleName(unlink->toolTip());
        card::keepIcon(unlink, QStringLiteral("common.delete"), core::IconContext::Menu);
        grid->addWidget(unlink, 0, 3);
        int row = 1;
        if (!eachText && !hint.isEmpty()) {
            QLabel* label = hintOf(m_pointsArea);
            label->setText(hint);
            label->setContentsMargins(HINT_INDENT, 0, 0, 0);
            grid->addWidget(label, row++, 1, 1, 3);
        }
        QLabel* state = hintOf(m_pointsArea);
        state->setObjectName(QStringLiteral("publishingPointState_") + id);
        state->setContentsMargins(HINT_INDENT, 0, 0, 0);
        grid->addWidget(state, row, 1, 1, 3);
        m_pointsLayout->addLayout(grid);
        m_points << box;
        m_pointStates << state;
        m_pointSwatches << swatch;
        m_linkButtons << link;
        m_unlinkButtons << unlink;
        connect(box, &QCheckBox::toggled, this, [this, id](bool done) { savePoint(id, done); });
        connect(link, &QToolButton::clicked, this, [this, id]() { linkSelection(id); });
        connect(unlink, &QToolButton::clicked, this, [this, id]() { unlinkPoint(id); });
    }
}

QList<QColor> PublishingTextBox::pointColors() const {
    QList<QColor> colors;
    const core::ThemeManager& themes = core::ThemeManager::getInstance();
    for (const char* key : editor::TEMPLATE_POINT_COLOR_KEYS) {
        const QColor color = themes.editorColor(key, QColor());
        if (color.isValid()) {
            colors << color;
        }
    }
    if (colors.isEmpty()) {  // a theme of the writer without them: the color of the selection
        colors << palette().color(QPalette::Highlight);
    }
    return colors;
}

void PublishingTextBox::followEditor(editor::BookEditor* editor) {
    if (m_lookEditor == editor) {
        return;
    }
    for (const QMetaObject::Connection& connection : std::as_const(m_editorConnections)) {
        disconnect(connection);
    }
    m_editorConnections.clear();
    if (!m_lookEditor.isNull()) {
        m_lookEditor->setTemplatePointLook({});
    }
    m_lookEditor = editor;
    if (editor == nullptr) {
        return;
    }
    // The states follow the fragments of the text, the names in bold its cursor
    m_editorConnections << connect(editor, &editor::BookEditor::contentChanged, this,
                                   &PublishingTextBox::updatePointStates);
    m_editorConnections << connect(editor, &editor::BookEditor::cursorPositionChanged, this,
                                   &PublishingTextBox::showPointsAtCursor);
    m_editorConnections << connect(editor, &editor::BookEditor::selectionChanged, this,
                                   &PublishingTextBox::showPointsAtCursor);
}

void PublishingTextBox::applyLook() {
    if (m_lookEditor.isNull()) {
        return;
    }
    editor::TemplatePointLook look;
    if (!m_pointIds.isEmpty() && !m_elementId.isEmpty()) {
        look.order = m_pointIds;
        look.colors = pointColors();
        look.shown = m_showInText->isChecked();
        look.bars = PublishingTexts::showsPointBars();
        look.current = m_chosenPoint;
    }
    m_lookEditor->setTemplatePointLook(look);
}

void PublishingTextBox::updatePointStates() {
    if (m_points.isEmpty()) {
        return;
    }
    // The fragments of the text in its editor, else what the book keeps of it
    QHash<QString, QList<editor::PointFragment>> fragments;
    QStringList unlinked;
    const bool inEditor = !m_editor.isNull();
    if (inEditor) {
        fragments = m_editor->templatePointFragments();
    } else {
        for (const core::PublishingTemplatePoint& point :
             m_book.unlinkedPublishingPointsOf(m_elementId)) {
            unlinked << point.id;
        }
    }
    const QList<QColor> colors = pointColors();
    const QColor warningColor = core::ThemeManager::getInstance().getCurrentTheme().log.warning;
    int covered = 0;
    int ticked = 0;
    for (qsizetype i = 0; i < m_points.size(); ++i) {
        const QString& id = m_pointIds.at(i);
        const QList<editor::PointFragment> own = fragments.value(id);
        const bool inText = inEditor ? !own.isEmpty() : !unlinked.contains(id);
        const bool done = m_points.at(i)->isChecked();
        covered += inText ? 1 : 0;
        ticked += done ? 1 : 0;
        int characters = 0;
        for (const editor::PointFragment& fragment : own) {
            characters += fragment.characters;
        }
        QLabel* state = m_pointStates.at(i);
        const bool warning = done && !inText;
        if (warning) {
            //: Under a point of a synopsis ticked off without a fragment linked to it. In
            //: Polish: Odhaczony, a w tekście nie ma jego fragmentu
            state->setText(tr("Ticked off, but the text has no fragment of it"));
        } else if (inText && inEditor) {
            //: Under a point of a synopsis: its fragments in the text and their length. In
            //: Polish: W tekście: %1, %2 (2 fragmenty, 140 znaków)
            const auto count = static_cast<int>(own.size());
            state->setText(tr("In the text: %1, %2")
                               .arg(count == 1
                                        //: In Polish: 1 fragment
                                        ? tr("1 fragment")
                                        //: In Polish: %n fragment / fragmenty / fragmentów
                                        : tr("%n fragments", nullptr, count),
                                    characters == 1
                                        //: In Polish: 1 znak
                                        ? tr("1 character")
                                        //: In Polish: %n znak / znaki / znaków
                                        : tr("%n characters", nullptr, characters)));
        } else if (inText) {
            //: Under a point of a synopsis with a fragment linked to it. In Polish: W tekście
            state->setText(tr("In the text"));
        } else {
            //: Under a point of a synopsis without a fragment linked to it. In Polish: Brak w
            //: tekście
            state->setText(tr("Not in the text"));
        }
        QPalette look = palette();
        if (warning) {
            look.setColor(QPalette::WindowText, warningColor);
        }
        state->setPalette(look);
        state->setForegroundRole(warning ? QPalette::WindowText : QPalette::PlaceholderText);
        QFont font = state->font();
        font.setBold(warning);
        state->setFont(font);
        // Its number in its color, with the color of the text that stands out on it
        const QColor color = colors.at(i % colors.size());
        const QColor light = palette().color(QPalette::Base);
        const QColor dark = palette().color(QPalette::Text);
        const QColor lighter = light.lightness() > dark.lightness() ? light : dark;
        const QColor darker = light.lightness() > dark.lightness() ? dark : light;
        const QString style =
            QStringLiteral("QLabel { background: %1; color: %2; border-radius: 3px; }")
                .arg(color.name(), (color.lightness() > LIGHT_SWATCH ? darker : lighter).name());
        if (m_pointSwatches.at(i)->styleSheet() != style) {  // not again at every key
            m_pointSwatches.at(i)->setStyleSheet(style);
        }
        const bool editable = inEditor && !m_book.isReadOnly() && !m_editor->isReadOnly();
        m_linkButtons.at(i)->setEnabled(editable && m_editor->hasSelection());
        m_unlinkButtons.at(i)->setEnabled(editable && !own.isEmpty());
    }
    //: Over the points of a synopsis. In Polish: W tekście: %1 z %3 · Gotowe: %2 z %3
    m_pointsSummary->setText(tr("In the text: %1 of %3 · Done: %2 of %3")
                                 .arg(covered)
                                 .arg(ticked)
                                 .arg(m_points.size()));
    m_showInText->setEnabled(inEditor);
    showPointsAtCursor();
}

void PublishingTextBox::showPointsAtCursor() {
    const QStringList atCursor =
        !m_editor.isNull() ? m_editor->templatePointsAtCursor() : QStringList();
    const bool selection = !m_editor.isNull() && m_editor->hasSelection();
    const bool editable = !m_editor.isNull() && !m_book.isReadOnly() && !m_editor->isReadOnly();
    for (qsizetype i = 0; i < m_points.size(); ++i) {
        QFont font = m_points.at(i)->font();
        font.setBold(atCursor.contains(m_pointIds.at(i)));
        m_points.at(i)->setFont(font);
        m_linkButtons.at(i)->setEnabled(editable && selection);
    }
}

void PublishingTextBox::choosePoint(const QString& pointId) {
    if (m_chosenPoint == pointId) {
        return;
    }
    m_chosenPoint = pointId;
    applyLook();
}

bool PublishingTextBox::linkSelection(const QString& pointId) {
    if (m_editor.isNull() || m_book.isReadOnly()) {
        return false;
    }
    if (!m_editor->hasSelection()) {
        //: Link to a Point without a selection. In Polish: Zaznacz fragment tekstu, który chcesz
        //: powiązać z punktem.
        emit statusMessage(tr("Select the fragment of the text you want to link to a point."));
        return false;
    }
    return m_editor->linkToTemplatePoint(pointId, true);
}

bool PublishingTextBox::unlinkPoint(const QString& pointId) {
    if (m_editor.isNull() || m_book.isReadOnly()) {
        return false;
    }
    m_nextFragment.remove(pointId);
    return m_editor->unlinkTemplatePointEverywhere(pointId);
}

bool PublishingTextBox::goToNextFragment(const QString& pointId) {
    if (m_editor.isNull()) {
        return false;
    }
    const qsizetype count = m_editor->templatePointFragments().value(pointId).size();
    if (count == 0) {
        //: Enter in a point of a synopsis without a fragment. In Polish: Tekst nie ma jeszcze
        //: fragmentu tego punktu.
        emit statusMessage(tr("The text has no fragment of this point yet."));
        return false;
    }
    const int index = m_nextFragment.value(pointId) % static_cast<int>(count);
    m_nextFragment.insert(pointId, index + 1);
    return m_editor->goToTemplatePointFragment(pointId, index);
}

bool PublishingTextBox::showLinkMenu() {
    if (m_editor.isNull() || m_templatePoints.isEmpty() || !PublishingTexts::hasPoints(m_data.part)) {
        return false;
    }
    const bool selection = m_editor->hasSelection();
    const QStringList linked = m_editor->templatePointsAtCursor();
    if (!selection && linked.isEmpty()) {
        emit statusMessage(tr("Select the fragment of the text you want to link to a point."));
        return false;
    }
    // Each point with its number as its key and its color; without a selection only the
    // points of the fragment at the cursor, whose link the menu takes away
    QMenu menu(m_editor);
    menu.setObjectName(QStringLiteral("publishingLinkMenu"));
    const QList<QColor> colors = pointColors();
    for (qsizetype i = 0; i < m_templatePoints.size(); ++i) {
        const QString id = m_templatePoints.at(i).id;
        QString name = m_templatePoints.at(i).name.text(programLanguage());
        name.replace(QLatin1Char('&'), QStringLiteral("&&"));
        const QString number = i < DIGIT_KEYS ? QStringLiteral("&%1").arg(i + 1)
                                              : QString::number(i + 1);
        QPixmap swatch(SWATCH_SIZE / 2, SWATCH_SIZE / 2);
        swatch.fill(colors.at(i % colors.size()));
        QAction* action = menu.addAction(QIcon(swatch), number + QStringLiteral(". ") + name);
        action->setCheckable(true);
        action->setChecked(linked.contains(id));
        action->setEnabled(selection || linked.contains(id));
        action->setData(id);
    }
    const QRect cursor = m_editor->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    const QAction* chosen = menu.exec(m_editor->mapToGlobal(cursor.bottomLeft()));
    if (chosen == nullptr) {
        return true;
    }
    // A point ticked is taken away (the action is unticked by now)
    return m_editor->linkToTemplatePoint(chosen->data().toString(), chosen->isChecked());
}

void PublishingTextBox::updateLength() {
    if (m_elementId.isEmpty()) {
        return;
    }
    // The words in the editor, also those not saved yet
    const QString text =
        !m_editor.isNull() ? m_editor->plainText() : m_book.publishingTextOf(m_elementId);
    const bool words = core::CoverMaterial::countsWords();
    const int target = m_book.publishingTargetOf(m_elementId, words);
    const bool pages = m_data.part == QLatin1String(PublishingTexts::SYNOPSIS) &&
                       PublishingTexts::showsPages();
    m_meter->setLength(PublishingTexts::lengthOf(text, false),
                       PublishingTexts::lengthOf(text, true), target, words, pages);
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    m_targetSource->setText(
        PublishingNames::targetSourceText(m_data, templ ? &*templ : nullptr, target, words));
}

bool PublishingTextBox::eventFilter(QObject* watched, QEvent* event) {
    const qsizetype index = m_points.indexOf(qobject_cast<QCheckBox*>(watched));
    if (index >= 0) {
        switch (event->type()) {
        case QEvent::KeyPress:
            if (pointKey(index, *static_cast<QKeyEvent*>(event))) {
                return true;
            }
            break;
        case QEvent::FocusIn:
        case QEvent::Enter:
            choosePoint(m_pointIds.value(index));  // its fragments filled in the text
            break;
        case QEvent::FocusOut:
        case QEvent::Leave: {
            // Still chosen while the keys are in another point or in this one
            const auto* focused = qobject_cast<QCheckBox*>(QApplication::focusWidget());
            const qsizetype kept = m_points.indexOf(const_cast<QCheckBox*>(focused));
            choosePoint(kept >= 0 ? m_pointIds.value(kept) : QString());
            break;
        }
        default:
            break;
        }
    }
    return QGroupBox::eventFilter(watched, event);
}

bool PublishingTextBox::pointKey(qsizetype index, const QKeyEvent& event) {
    if (event.modifiers() & ~Qt::KeypadModifier) {
        return false;
    }
    const QString& id = m_pointIds.at(index);
    switch (event.key()) {
    case Qt::Key_Escape:
        if (m_editor.isNull()) {
            return false;
        }
        // Back to the text, also from a panel floating in a window of its own
        m_editor->window()->activateWindow();
        m_editor->setFocus(Qt::OtherFocusReason);
        return true;
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End: {
        const qsizetype last = m_points.size() - 1;
        qsizetype next = index;
        if (event.key() == Qt::Key_Up) {
            next = std::max<qsizetype>(0, index - 1);
        } else if (event.key() == Qt::Key_Down) {
            next = std::min(last, index + 1);
        } else {
            next = event.key() == Qt::Key_Home ? 0 : last;
        }
        m_points.at(next)->setFocus(Qt::TabFocusReason);
        return true;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        goToNextFragment(id);
        return true;
    case Qt::Key_Insert:
        linkSelection(id);
        return true;
    case Qt::Key_Delete:
        unlinkPoint(id);
        return true;
    default:
        return false;
    }
}

std::optional<core::PublishingTemplate> PublishingTextBox::currentTemplate() const {
    if (m_data.templateId.isEmpty()) {
        return std::nullopt;
    }
    return m_book.publishingTemplate(m_data.templateId);
}

bool PublishingTextBox::save(const core::PublishingData& publishing) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly()) {
        return false;
    }
    // The points ticked off go their own way, waiting for the text (savePoint())
    core::PublishingData saved = publishing;
    saved.done = element->publishing.done;
    m_data = publishing;
    if (element->publishing == saved) {
        return true;
    }
    if (!m_book.setPublishing(m_elementId, saved)) {
        return false;
    }
    emit publishingChanged(m_elementId);
    return true;
}

void PublishingTextBox::saveTemplate() {
    if (m_templateBox->currentData().toString() == QLatin1String(NEW_TEMPLATE)) {
        startNewTemplate();
        return;
    }
    core::PublishingData changed = m_data;
    changed.templateId = m_templateBox->currentData().toString();
    if (changed.templateId != m_data.templateId && save(changed)) {
        showPoints();
        updateLength();
    }
}

void PublishingTextBox::changeTemplate() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const std::optional<core::PublishingTemplate> templ = currentTemplate();
    editTemplate(templ ? *templ
                       : PublishingTemplateEditor::newTemplate(m_data.part,
                                                               PublishingTexts::ownTemplates()));
}

void PublishingTextBox::startNewTemplate() {
    if (m_elementId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    editTemplate(
        PublishingTemplateEditor::newTemplate(m_data.part, PublishingTexts::ownTemplates()));
}

void PublishingTextBox::editTemplate(const core::PublishingTemplate& templ) {
    // The template of the text stays chosen in the list while it is changed
    {
        const QSignalBlocker blocker(m_templateBox);
        m_templateBox->setCurrentIndex(m_templateBox->findData(m_data.templateId));
    }
    m_templateBox->setEnabled(false);
    m_changeTemplate->setEnabled(false);
    m_pointsHeader->hide();
    m_pointsArea->hide();
    m_pointsHint->hide();
    m_templateEditor->show();
    m_templateEditor->edit(templ, core::CoverMaterial::countsWords());
}

void PublishingTextBox::keepTemplate() {
    QList<core::PublishingTemplate> own = PublishingTexts::ownTemplates();
    const core::PublishingTemplate kept = m_templateEditor->keptTemplate(own);
    const auto same = std::find_if(own.begin(), own.end(), [&kept](const auto& templ) {
        return templ.id == kept.id;
    });
    if (same != own.end()) {
        *same = kept;
    } else {
        own.append(kept);
    }
    PublishingTexts::setOwnTemplates(own);
    stopEditingTemplate();
    fillTemplates();
    core::PublishingData changed = m_data;
    changed.templateId = kept.reference();
    if (changed.templateId == m_data.templateId) {
        emit publishingChanged(m_elementId);  // the same template, changed
    } else if (!save(changed)) {
        return;
    }
    m_pointIds.clear();  // the points of the template changed: built again
    showData();
    //: The status bar after a template was changed beside a text. In Polish: Zapisano szablon
    //: „%1” – jest dostępny we wszystkich książkach.
    emit statusMessage(tr("Template \"%1\" saved – it is available in all books.")
                           .arg(kept.name.text(programLanguage())));
}

void PublishingTextBox::stopEditingTemplate() {
    if (m_templateEditor->isHidden()) {
        return;
    }
    const bool keys = m_templateEditor->isAncestorOf(QApplication::focusWidget());
    m_templateEditor->hide();
    m_pointsHint->show();
    m_templateBox->setEnabled(!m_book.isReadOnly());
    m_changeTemplate->setEnabled(!m_book.isReadOnly());
    showPoints();
    if (keys) {
        m_templateBox->setFocus(Qt::OtherFocusReason);
    }
}

void PublishingTextBox::savePoint(const QString& pointId, bool done) {
    QStringList ticked = m_data.done;
    ticked.removeAll(pointId);
    if (done) {
        ticked << pointId;
    }
    // While the text has changes not saved, the points are saved and dropped with them
    const bool textChanged = m_editor != nullptr && m_editor->isModified();
    if (m_book.isReadOnly() || !m_book.setPublishingPoints(m_elementId, ticked, textChanged)) {
        showPoints();
        return;
    }
    m_data.done = ticked;
    updatePointStates();  // the warning of a point ticked off without a fragment
    emit publishingChanged(m_elementId);
}

void PublishingTextBox::saveState() {
    const QString state = m_stateBox->currentData().toString();
    if (state == m_data.state || m_book.isReadOnly()) {
        return;
    }
    if (m_book.setPublishingState(m_elementId, state)) {
        emit publishingChanged(m_elementId);
    }
    showData();
}

void PublishingTextBox::showVolume(const core::ProjectElement& element) {
    const core::BookProject* project = m_book.project();
    const bool series = project != nullptr && m_book.isSeries();
    m_fields->setRowVisible(m_volumeBox, series);
    if (series) {
        VolumeNames::fillBox(*m_volumeBox, m_book,
                             core::VolumeMaterials::volumeOf(*project, element));
    }
    m_volumeBox->setEnabled(!m_book.isReadOnly());
}

void PublishingTextBox::saveVolume() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const QString volume = m_volumeBox->currentData().toString();
    if (element == nullptr || m_book.isReadOnly() ||
        core::VolumeMaterials::volumeOf(*m_book.project(), *element) == volume) {
        return;
    }
    QStringList drafts;
    if (m_book.setMaterialBook(m_elementId, volume, &drafts)) {
        emit publishingChanged(m_elementId);
        for (const QString& draft : drafts) {
            emit publishingChanged(draft);
        }
    }
    showData();
}

void PublishingTextBox::saveTarget() {
    core::PublishingData changed = m_data;
    (core::CoverMaterial::countsWords() ? changed.targetWords : changed.targetCharacters) =
        m_targetSpin->value();
    save(changed);
    updateLength();
}

}  // namespace kalahari::gui
