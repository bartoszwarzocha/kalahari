// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_text_box.cpp
/// @brief The group of a text for publishers in the Properties panel

#include "kalahari/gui/panels/publishing_text_box.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/widgets/length_meter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// The largest own target length
constexpr int MAX_TARGET = 100000;

/// How far the hint of a point is from the left edge of its check box, and from the next point
constexpr int HINT_INDENT = 22;
constexpr int HINT_BOTTOM = 4;

/// The margins and the spacing of the group, as of the other groups of the panel
constexpr int MARGIN = 11;
constexpr int SPACING = 6;

/// The space over the template and over the data, and between the points
constexpr int SECTION_GAP = 4;
constexpr int POINT_SPACING = 2;

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
      m_pointsArea(new QWidget(this)),
      m_pointsLayout(new QVBoxLayout(m_pointsArea)),
      m_pointsHint(hintOf(this)),
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
    //: The template of a text for publishers in the Properties panel. In Polish: Sza&blon:
    templateRow->addRow(tr("Te&mplate:"), m_templateBox);
    layout->addLayout(templateRow);
    m_pointsArea->setObjectName(QStringLiteral("publishingPoints"));
    m_pointsLayout->setContentsMargins(0, 0, 0, 0);
    m_pointsLayout->setSpacing(POINT_SPACING);
    layout->addWidget(m_pointsArea);
    m_pointsHint->setObjectName(QStringLiteral("publishingPointsHint"));
    layout->addWidget(m_pointsHint);

    auto* data = new QFormLayout();
    data->setContentsMargins(0, SECTION_GAP, 0, 0);
    m_stateBox->setObjectName(QStringLiteral("publishingState"));
    for (const char* state : {"", PublishingTexts::CHOSEN, PublishingTexts::REJECTED}) {
        m_stateBox->addItem(PublishingNames::stateName(QString::fromLatin1(state)),
                            QString::fromLatin1(state));
    }
    //: The state of a text for publishers in the Properties panel. In Polish: &Stan:
    data->addRow(tr("&State:"), m_stateBox);
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
    data->addRow(targetLabel, target);
    m_inSetLabel->setObjectName(QStringLiteral("publishingInSet"));
    data->addRow(m_inSetLabel);
    layout->addLayout(data);

    connect(m_templateBox, &QComboBox::activated, this, &PublishingTextBox::saveTemplate);
    connect(m_stateBox, &QComboBox::activated, this, &PublishingTextBox::saveState);
    connect(m_targetSpin, &QSpinBox::valueChanged, this, &PublishingTextBox::saveTarget);
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

void PublishingTextBox::showText(const QString& elementId, editor::BookEditor* editor) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    m_editor = editor;
    if (element == nullptr || element->publishing.part.isEmpty()) {
        m_elementId.clear();
        hide();
        return;
    }
    const bool other = elementId != m_elementId || element->publishing.part != m_data.part;
    m_elementId = elementId;
    m_data = element->publishing;
    if (other) {
        fillTemplates();
    }
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
}

void PublishingTextBox::showData() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || element->publishing.part.isEmpty()) {
        m_elementId.clear();
        hide();
        return;
    }
    m_data = element->publishing;
    setTitle(PublishingNames::partName(m_data.part));
    const bool readOnly = m_book.isReadOnly();
    {
        const QSignalBlocker blocker(m_templateBox);
        if (m_templateBox->findData(m_data.templateId) < 0) {
            // A template the book does not have: named as it is written
            m_templateBox->addItem(m_data.templateId, m_data.templateId);
        }
        m_templateBox->setCurrentIndex(m_templateBox->findData(m_data.templateId));
    }
    m_templateBox->setEnabled(!readOnly);
    showPoints();
    {
        const QSignalBlocker blocker(m_stateBox);
        if (m_stateBox->findData(m_data.state) < 0) {
            m_stateBox->addItem(PublishingNames::stateName(m_data.state), m_data.state);
        }
        m_stateBox->setCurrentIndex(m_stateBox->findData(m_data.state));
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
    m_inSetLabel->setText(inSet ? tr("In the set: yes – this version") : tr("In the set: no"));
    updateLength();
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
    }
    const bool readOnly = m_book.isReadOnly();
    for (qsizetype i = 0; i < m_points.size(); ++i) {
        const QSignalBlocker blocker(m_points.at(i));
        m_points.at(i)->setChecked(m_data.done.contains(m_pointIds.at(i)));
        m_points.at(i)->setEnabled(!readOnly);
    }
    m_pointsArea->setVisible(!m_points.isEmpty());
    if (!PublishingTexts::hasPoints(m_data.part)) {
        //: The Properties panel of a sentence about the book. In Polish: Pola wzoru są na
        //: karcie zdania – otwiera ją Enter na liście albo w Nawigatorze.
        m_pointsHint->setText(tr("The fields of its pattern are on the card of the sentence – "
                                 "Enter in the list or the Navigator opens it."));
    } else if (m_points.isEmpty()) {
        //: The Properties panel of a synopsis without a template. In Polish: Tekst bez
        //: szablonu. Wybierz szablon, a jego punkty pokażą się tutaj.
        m_pointsHint->setText(
            tr("A text without a template. Choose a template and its points show here."));
    } else {
        //: Under the points of the template of a synopsis in the Properties panel. In Polish:
        //: Punkty odhaczasz sam (Spacja). Narzędzia > Teksty dla wydawnictwa > Punkty szablonu
        //: przenosi tu klawiaturę, a Esc wraca do tekstu.
        m_pointsHint->setText(
            tr("You tick off the points yourself (Space). Tools > Texts for Publishers > "
               "Template Points brings the keys here, and Esc takes them back to the text."));
    }
}

void PublishingTextBox::buildPoints(const QList<core::PublishingTemplatePoint>& points,
                                    bool eachText) {
    utils::clearLayout(m_pointsLayout);
    m_points.clear();
    // A point for each text has the hint of its template, once over them
    if (eachText && !points.isEmpty()) {
        QLabel* hint = hintOf(m_pointsArea);
        hint->setText(points.constFirst().hint.text(programLanguage()));
        m_pointsLayout->addWidget(hint);
    }
    for (const core::PublishingTemplatePoint& point : points) {
        const QString hint = point.hint.text(programLanguage());
        auto* box = new QCheckBox(point.name.text(programLanguage()), m_pointsArea);
        box->setObjectName(QStringLiteral("publishingPoint_") + point.id);
        box->setAccessibleDescription(hint);
        box->installEventFilter(this);
        m_pointsLayout->addWidget(box);
        if (!eachText && !hint.isEmpty()) {
            QLabel* label = hintOf(m_pointsArea);
            label->setText(hint);
            label->setContentsMargins(HINT_INDENT, 0, 0, HINT_BOTTOM);
            m_pointsLayout->addWidget(label);
        }
        m_points << box;
        const QString id = point.id;
        connect(box, &QCheckBox::toggled, this, [this, id](bool done) { savePoint(id, done); });
    }
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
    if (event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape && !m_editor.isNull() &&
        m_points.contains(qobject_cast<QCheckBox*>(watched))) {
        // Back to the text, also from a panel floating in a window of its own
        m_editor->window()->activateWindow();
        m_editor->setFocus(Qt::OtherFocusReason);
        return true;
    }
    return QGroupBox::eventFilter(watched, event);
}

std::optional<core::PublishingTemplate> PublishingTextBox::currentTemplate() const {
    if (m_data.templateId.isEmpty()) {
        return std::nullopt;
    }
    return m_book.publishingTemplate(m_data.templateId);
}

bool PublishingTextBox::save(const core::PublishingData& data) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly()) {
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

void PublishingTextBox::saveTemplate() {
    core::PublishingData data = m_data;
    data.templateId = m_templateBox->currentData().toString();
    if (data.templateId != m_data.templateId && save(data)) {
        showPoints();
        updateLength();
    }
}

void PublishingTextBox::savePoint(const QString& pointId, bool done) {
    core::PublishingData data = m_data;
    data.done.removeAll(pointId);
    if (done) {
        data.done << pointId;
    }
    save(data);
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

void PublishingTextBox::saveTarget() {
    core::PublishingData data = m_data;
    (core::CoverMaterial::countsWords() ? data.targetWords : data.targetCharacters) =
        m_targetSpin->value();
    save(data);
    updateLength();
}

}  // namespace kalahari::gui
