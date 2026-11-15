// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file clipping_origin_box.cpp
/// @brief The group Origin of a clipping in the Properties panel

#include "kalahari/gui/panels/clipping_origin_box.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/clipping.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/version_texts.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include <string>

namespace kalahari::gui {

namespace {

/// @brief The margins and the spacing of the group, as the other groups of the panel have
constexpr int MARGIN = 11;
constexpr int SPACING = 6;

/// @brief The link of the text a clipping comes from, in its label
constexpr const char* ORIGIN_LINK = "origin";

/// @brief Whether a clipping inserted stays in the Workshop (core::Clippings::AFTER_INSERT_KEEP)
bool keepsInserted() {
    using core::Clippings;
    return core::SettingsManager::getInstance().get<std::string>(
               Clippings::AFTER_INSERT_SETTING, Clippings::AFTER_INSERT_KEEP) ==
           Clippings::AFTER_INSERT_KEEP;
}

}  // namespace

ClippingOriginBox::ClippingOriginBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_book(book), m_layout(new QFormLayout(this)) {
    setObjectName(QStringLiteral("clippingOriginGroup"));
    //: A group of the Properties panel of a clipping: where it comes from. In Polish:
    //: Pochodzenie
    setTitle(tr("Origin"));
    m_layout->setSpacing(SPACING);
    m_layout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    //: The text a clipping was cut from. In Polish: Skąd:
    m_from = addValue(tr("From:"), "clippingOriginFrom");
    m_from->setTextFormat(Qt::RichText);
    m_from->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(m_from, &QLabel::linkActivated, this, [this]() {
        if (!m_clippingId.isEmpty()) {
            emit requestShowOrigin(m_clippingId);
        }
    });
    //: The paragraph a clipping was cut from. In Polish: Miejsce:
    m_place = addValue(tr("Place:"), "clippingOriginPlace");
    //: When a clipping was cut. In Polish: Wycięto:
    m_cut = addValue(tr("Cut:"), "clippingOriginCut");
    //: When a clipping was inserted into a text last. In Polish: Wstawiony:
    m_inserted = addValue(tr("Inserted:"), "clippingOriginInserted");

    auto* buttons = new QWidget(this);
    auto* row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(SPACING);
    //: Opens the text a clipping was cut from at the place. In Polish: Przejdź do miejsca
    m_go = new QPushButton(tr("Go to Its Place"), buttons);
    m_go->setObjectName(QStringLiteral("clippingOriginGo"));
    //: Inserts a clipping where it was cut from. In Polish: Przywróć na miejsce
    m_restore = new QPushButton(tr("Restore to Its Place"), buttons);
    m_restore->setObjectName(QStringLiteral("clippingOriginRestore"));
    row->addWidget(m_go);
    row->addWidget(m_restore);
    row->addStretch();
    m_layout->addRow(buttons);
    connect(m_go, &QPushButton::clicked, this, [this]() { emit requestShowOrigin(m_clippingId); });
    connect(m_restore, &QPushButton::clicked, this,
            [this]() { emit requestRestore(m_clippingId); });
    hide();
}

QLabel* ClippingOriginBox::addValue(const QString& label, const char* name) {
    auto* value = new QLabel(this);
    value->setObjectName(QString::fromLatin1(name));
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_layout->addRow(label, value);
    return value;
}

void ClippingOriginBox::showClipping(const QString& elementId) {
    const core::ProjectElement* element = m_book.findElement(elementId);
    if (element == nullptr || !core::ProjectManager::isClipping(*element)) {
        m_clippingId.clear();
        hide();
        return;
    }
    m_clippingId = elementId;
    const core::ClippingData& clipping = element->clipping;
    const ClippingFrom from = ClippingTexts::fromOf(m_book, clipping);
    if (from.live) {
        m_from->setText(QStringLiteral("<a href=\"%1\">%2</a>")
                            .arg(QString::fromLatin1(ORIGIN_LINK), from.title.toHtmlEscaped()));
    } else {
        //: Where a clipping comes from: none, it was added by hand. In Polish: dodany ręcznie
        m_from->setText(
            (from.title.isEmpty() ? tr("added by hand") : from.title).toHtmlEscaped());
    }
    const QString place = ClippingTexts::placeText(clipping.origin);
    m_place->setText(place);
    m_layout->setRowVisible(m_place, !place.isEmpty());
    m_cut->setText(clipping.origin.cut.isValid() ? versionTime(clipping.origin.cut) : QString());
    m_layout->setRowVisible(m_cut, clipping.origin.cut.isValid());
    const QString inserted = ClippingTexts::insertedText(clipping);
    //: A clipping never inserted into a text. In Polish: nie
    m_inserted->setText(inserted.isEmpty() ? tr("no") : inserted);
    m_layout->setRowVisible(m_inserted, keepsInserted() || !inserted.isEmpty());

    const QString why = ClippingTexts::restoreToolTip(from);
    m_go->setEnabled(from.live);
    //: %1: the title of the text. In Polish: Otwórz „%1” w miejscu, z którego go wycięto
    const QString go = tr("Open \"%1\" at the place it was cut from").arg(from.title);
    m_go->setToolTip(from.live ? go : why);
    m_restore->setEnabled(from.live && !m_book.isReadOnly());
    m_restore->setToolTip(why);
    show();
}

}  // namespace kalahari::gui
