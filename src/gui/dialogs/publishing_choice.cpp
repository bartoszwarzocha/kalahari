// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file publishing_choice.cpp
/// @brief What a new text for publishers is in Add Element: its part and its template

#include "kalahari/gui/dialogs/publishing_choice.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/cover_material.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/cover_names.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/publishing_names.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <optional>
#include <utility>

namespace kalahari::gui::dialogs {

namespace {

using core::PublishingTexts;
using nlohmann::json;

/// Between the parts, their lines and the rows of the window
constexpr int SPACING = 6;

/// The pattern of @p templ with the names of its fields in brackets: "When [event], [hero]
/// must..."
QString patternText(const core::PublishingTemplate& templ) {
    const QString language = programLanguage();
    QString text = templ.pattern.text(language);
    for (const core::PublishingTemplateField& field : templ.fields) {
        text.replace(QStringLiteral("{%1}").arg(field.id),
                     QStringLiteral("[%1]").arg(field.name.text(language)));
    }
    return text;
}

/// The names of the points of @p templ, between dots
QString pointsText(const core::PublishingTemplate& templ) {
    QStringList names;
    for (const core::PublishingTemplatePoint& point : templ.points) {
        names << point.name.text(programLanguage());
    }
    return names.join(QStringLiteral(" · "));
}

/// The template chosen last for @p part (PublishingChoice::TEMPLATE_SETTING); nullopt: none
/// kept, empty: no template
std::optional<QString> keptTemplateOf(const QString& part) {
    const json kept = core::SettingsManager::getInstance().get<json>(
        PublishingChoice::TEMPLATE_SETTING, json::object());
    const std::string key = part.toStdString();
    if (!kept.is_object() || !kept.contains(key) || !kept.at(key).is_string()) {
        return std::nullopt;
    }
    return QString::fromStdString(kept.at(key).get<std::string>());
}

}  // namespace

PublishingChoice::PublishingChoice(QList<core::PublishingTemplate> templates, bool cover,
                                   QWidget* parent)
    : QGroupBox(parent),
      m_templates(std::move(templates)),
      m_cover(cover),
      m_partButtons(new QButtonGroup(this)) {
    setObjectName(QStringLiteral("publishingChoice"));
    //: The parts of a new text for publishers, buttons that the arrows go through, and its
    //: template. In Polish: Rodzaj tekstu
    setTitle(tr("Kind of Text"));
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(SPACING);
    auto* chips = new QWidget(this);
    auto* chipsLayout = new FlowLayout(chips, SPACING, SPACING);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    const QStringList all = parts();
    auto& art = core::ArtProvider::getInstance();
    for (qsizetype index = 0; index < all.size(); ++index) {
        const QString& part = all.at(index);
        auto* button = new QToolButton(chips);
        button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        //: A part of a new text for publishers that goes to the cover; %1: what it is. In
        //: Polish: %1 – do Okładki
        button->setText(PublishingTexts::parts().contains(part)
                            ? PublishingNames::partName(part)
                            : tr("%1 – to the Cover").arg(PublishingNames::partName(part)));
        button->setProperty("iconId", PublishingNames::partIconId(part));
        button->setIcon(art.getIcon(PublishingNames::partIconId(part), core::IconContext::Menu));
        m_partButtons->addButton(button, static_cast<int>(index));
        chipsLayout->addWidget(button);
    }
    layout->addWidget(chips);

    auto* templateRow = new QHBoxLayout();
    templateRow->setSpacing(SPACING);
    //: The list of the templates of a new text for publishers. In Polish: Sza&blon:
    m_templateLabel = new QLabel(tr("Te&mplate:"), this);
    m_templateBox = new QComboBox(this);
    m_templateBox->setObjectName(QStringLiteral("publishingTemplate"));
    m_templateLabel->setBuddy(m_templateBox);
    templateRow->addWidget(m_templateLabel);
    templateRow->addWidget(m_templateBox, 1);
    layout->addLayout(templateRow);
    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("publishingNote"));
    m_note->setWordWrap(true);
    layout->addWidget(m_note);

    // The part chosen last, else a sentence
    QString start = QString::fromStdString(core::SettingsManager::getInstance().get<std::string>(
        PART_SETTING, PublishingTexts::PITCH));
    if (!all.contains(start)) {
        start = QString::fromLatin1(PublishingTexts::PITCH);
    }
    m_part = start;
    m_partButtons->button(static_cast<int>(all.indexOf(start)))->setChecked(true);
    fillTemplates();
    connect(m_partButtons, &QButtonGroup::idToggled, this, [this](int index, bool checked) {
        if (checked) {
            m_part = parts().value(index);
            fillTemplates();
            emit partChanged(m_part);
        }
    });
    connect(m_templateBox, &QComboBox::currentIndexChanged, this, [this]() {
        m_chosen.insert(m_part, m_templateBox->currentData().toString());
        updateNote();
    });
    // New icon colors or sizes redraw the icons
    connect(&art, &core::ArtProvider::resourcesChanged, this, [this]() {
        for (QAbstractButton* button : m_partButtons->buttons()) {
            button->setIcon(core::ArtProvider::getInstance().getIcon(
                button->property("iconId").toString(), core::IconContext::Menu));
        }
    });
}

bool PublishingChoice::coverPart() const {
    return !PublishingTexts::parts().contains(m_part);
}

QString PublishingChoice::templateReference() const {
    const core::PublishingTemplate* chosen = templateAt(m_templateBox->currentIndex());
    return chosen != nullptr && !coverPart() ? chosen->reference() : QString();
}

void PublishingChoice::setPart(const QString& part) {
    if (const qsizetype index = parts().indexOf(part); index >= 0) {
        m_partButtons->button(static_cast<int>(index))->setChecked(true);
    }
}

void PublishingChoice::setTemplate(const QString& reference) {
    for (int index = 0; index < m_templateBox->count(); ++index) {
        if (m_templateBox->itemData(index).toString() == reference) {
            m_templateBox->setCurrentIndex(index);
            return;
        }
    }
}

void PublishingChoice::keep() const {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>(PART_SETTING, m_part.toStdString());
    if (coverPart()) {
        return;
    }
    json kept = settings.get<json>(TEMPLATE_SETTING, json::object());
    if (!kept.is_object()) {
        kept = json::object();
    }
    kept[m_part.toStdString()] = templateReference().toStdString();
    settings.set<json>(TEMPLATE_SETTING, kept);
}

QStringList PublishingChoice::parts() const {
    QStringList all = PublishingTexts::parts();
    if (m_cover) {
        all << QString::fromLatin1(PublishingTexts::BLURB)
            << QString::fromLatin1(PublishingTexts::TAGLINE);
    }
    return all;
}

void PublishingChoice::fillTemplates() {
    const bool cover = coverPart();
    m_templateLabel->setVisible(!cover);
    m_templateBox->setVisible(!cover);
    {
        const QSignalBlocker blocker(m_templateBox);
        m_templateBox->clear();
        for (const core::PublishingTemplate& templ : std::as_const(m_templates)) {
            if (templ.part != m_part) {
                continue;
            }
            const QString name = templ.name.text(programLanguage());
            //: A template of the writer's own in the list of Add Element; %1 its name. In
            //: Polish: %1 – Twój szablon
            m_templateBox->addItem(templ.isOwn() ? tr("%1 – your template").arg(name) : name,
                                   templ.reference());
        }
        m_templateBox->addItem(PublishingNames::noTemplateName(), QString());
        // The template chosen for the part in the window, else the one chosen last, else the
        // first one
        const std::optional<QString> kept =
            m_chosen.contains(m_part) ? m_chosen.value(m_part) : keptTemplateOf(m_part);
        const int index = kept ? m_templateBox->findData(*kept) : -1;
        m_templateBox->setCurrentIndex(std::max(0, index));
    }
    updateNote();
}

void PublishingChoice::updateNote() {
    const bool words = core::CoverMaterial::countsWords();
    if (coverPart()) {
        //: Where a new description or tagline of the cover goes; %1: what it is. In Polish:
        //: %1 trafi do Okładki jako materiał okładki; teksty dla wydawnictwa pokażą go
        //: stamtąd, bez drugiej kopii. Docelowa długość jest z Okładki.
        m_note->setText(tr("%1 goes to the Cover as a material of the cover; the texts for "
                           "publishers show it from there, without a second copy. Its target "
                           "length is that of the Cover.")
                            .arg(PublishingNames::partName(m_part)));
        return;
    }
    const core::PublishingTemplate* chosen = templateAt(m_templateBox->currentIndex());
    if (chosen == nullptr) {
        //: The note about a new text for publishers without a template; %1: the length. In
        //: Polish: Samo pole tekstu; docelowa długość z Ustawień: %1
        m_note->setText(tr("Only the text; the target length from the Settings: %1")
                            .arg(CoverNames::lengthText(
                                PublishingTexts::defaultTargetOf(m_part, words), words)));
        return;
    }
    const int target = words ? chosen->targetWords : chosen->targetCharacters;
    QString what;
    QString length = CoverNames::lengthText(target, words);
    if (chosen->eachText) {
        //: The points of a template of a synopsis with a point for each text. In Polish:
        //: Punkt dla każdego tekstu książki
        what = tr("A point for each text of the book");
        //: The target length of a template of each text; %1: the length. In Polish: %1 na
        //: każdy tekst
        length = tr("%1 for each text").arg(length);
    } else if (PublishingTexts::hasPattern(m_part)) {
        //: The pattern of a template of a sentence; %1: the pattern. In Polish: Wzór: %1
        what = tr("Pattern: %1").arg(patternText(*chosen));
    } else {
        //: The points of a template of a synopsis; %1: their names. In Polish: Punkty: %1
        what = tr("Points: %1").arg(pointsText(*chosen));
    }
    //: The note about the template of a new text for publishers: %1 its pattern or its
    //: points, %2 the target length. In Polish: %1 · docelowo %2
    m_note->setText(target > 0 ? tr("%1 · target: %2").arg(what, length) : what);
}

const core::PublishingTemplate* PublishingChoice::templateAt(int index) const {
    const QString reference = m_templateBox->itemData(index).toString();
    if (reference.isEmpty()) {
        return nullptr;
    }
    for (const core::PublishingTemplate& templ : m_templates) {
        if (templ.reference() == reference) {
            return &templ;
        }
    }
    return nullptr;
}

}  // namespace kalahari::gui::dialogs
