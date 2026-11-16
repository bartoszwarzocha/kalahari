// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_preview_highlights.cpp
/// @brief The highlights of PdfPreviewPanel: text of a page marked in a color, with a note,
/// kept in the book; the PDF file stays as it is

#include "kalahari/gui/panels/pdf_preview_panel.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/panels/pdf_pages_view.h"
#include "kalahari/gui/pdf_files.h"
#include "kalahari/gui/widgets/color_menu.h"

#include <QAction>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <cmath>
#include <functional>

namespace kalahari::gui {

namespace {

/// The columns of the highlights
constexpr int PAGE_COLUMN = 0;
constexpr int TEXT_COLUMN = 1;
constexpr int NOTE_COLUMN = 2;

/// The side of the swatch of the color of a highlight in its list
constexpr int SWATCH_SIDE = 12;

/// A swatch of @p color, framed with @p frame
QIcon swatchOf(const QColor& color, const QColor& frame) {
    QPixmap pixmap(SWATCH_SIDE, SWATCH_SIDE);
    pixmap.fill(color);
    QPainter painter(&pixmap);
    painter.setPen(frame);
    painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
    return {pixmap};
}

/// The places of a highlight are kept to a hundredth of a point, as the book keeps them
constexpr double POINT_HUNDREDTHS = 100.0;

/// @p area to a hundredth of a point
QRectF rounded(const QRectF& area) {
    const auto round = [](double value) {
        return std::round(value * POINT_HUNDREDTHS) / POINT_HUNDREDTHS;
    };
    return {round(area.x()), round(area.y()), round(area.width()), round(area.height())};
}

/// The place of all the areas of @p highlight on its page, in points
QRectF placeOf(const core::PdfHighlight& highlight) {
    QRectF place;
    for (const QRectF& area : highlight.areas) {
        place = place.united(area);
    }
    return place;
}

/// Gives @p highlights the highlight of @p areas of @p page in the color @p color: the text
/// highlighted already gets the color, other text a new highlight with @p text; its index
qsizetype putHighlight(QList<core::PdfHighlight>& highlights, int page,
                       const QList<QRectF>& areas, const QString& text, const QString& color) {
    const auto same = std::ranges::find_if(highlights, [page, &areas](const auto& each) {
        return each.page == page && each.areas == areas;
    });
    if (same != highlights.end()) {
        same->color = color;
        return same - highlights.begin();
    }
    highlights.append(core::PdfHighlight{.page = page,
                                         .areas = areas,
                                         .text = text,
                                         .color = color,
                                         .note = {},
                                         .extra = {}});
    return highlights.size() - 1;
}

}  // namespace

void PdfPreviewPanel::buildHighlightsTab(
    const std::function<QWidget*(QWidget*, QLabel*, QWidget*)>& page) {
    m_highlightsList = new QTreeWidget();
    m_highlightsList->setObjectName(QStringLiteral("pdfPreviewHighlights"));
    m_highlightsList->setColumnCount(3);
    m_highlightsList->setHeaderLabels({
        //: The columns of the highlights of the preview. In Polish: Strona
        tr("Page"),
        //: In Polish: Tekst
        tr("Text"),
        //: In Polish: Notatka
        tr("Note")});
    m_highlightsList->setRootIsDecorated(false);
    m_highlightsList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_highlightsList->header()->setSectionResizeMode(PAGE_COLUMN, QHeaderView::ResizeToContents);
    m_highlightsList->header()->setStretchLastSection(true);
    //: In Polish: Zakreślenia: Enter przechodzi do tekstu, Insert zakreśla zaznaczenie, F2
    //: zmienia notatkę, Delete usuwa
    m_highlightsList->setAccessibleName(tr("Highlights: Enter goes to the text, Insert "
                                           "highlights the selection, F2 changes the note, "
                                           "Delete takes it away"));
    //: In Polish: Bez zakreśleń. Zaznacz tekst na stronie i wybierz „Zakreśl” na pasku nad
    //: zaznaczeniem albo w jego menu.
    m_highlightsEmpty = new QLabel(tr("No highlights. Select text on a page and choose "
                                      "Highlight on the bar over it or in its menu."));
    // The colors to highlight with, after the list they look at: the editor's highlight
    // colors, without No Color
    m_newHighlightColors = new ColorMenu(ColorMenu::Kind::Highlight, {}, this);
    //: The colors to highlight the selection in. In Polish: Zakreśl kolorem
    m_newHighlightColors->setTitle(tr("Highlight in Color"));
    m_newHighlightColors->noColorAction()->setVisible(false);
    m_firstHighlightColor = m_newHighlightColors->lastColor();
    m_highlightColors = new ColorMenu(
        ColorMenu::Kind::Highlight,
        [this]() {
            const core::ProjectElement* element = m_book.findElement(elementId());
            const int index = highlightTarget();
            return element != nullptr && index >= 0 && index < element->highlights.size()
                       ? colorOf(element->highlights.at(index))
                       : QColor();
        },
        this);
    //: The color of a highlight there is. In Polish: Kolor zakreślenia
    m_highlightColors->setTitle(tr("Highlight Color"));
    m_highlightColors->noColorAction()->setVisible(false);

    //: The color of the current highlight. In Polish: Kolor
    m_highlightColorButton = new QPushButton(tr("Color"));
    m_highlightColorButton->setObjectName(QStringLiteral("pdfPreviewHighlightColor"));
    //: In Polish: Zmienia kolor zakreślenia
    m_highlightColorButton->setToolTip(tr("Changes the color of the highlight"));
    m_highlightColorButton->setMenu(m_highlightColors);
    m_side->addTab(page(m_highlightsList, m_highlightsEmpty, m_highlightColorButton), QString());
    //: In Polish: Zakreślenia
    m_side->setTabToolTip(HighlightsTab, tr("Highlights"));
}

void PdfPreviewPanel::connectHighlights() {
    connect(m_highlightsList, &QTreeWidget::itemActivated, this,
            &PdfPreviewPanel::goToHighlight);
    connect(m_highlightsList, &QTreeWidget::itemClicked, this, &PdfPreviewPanel::goToHighlight);
    connect(m_highlightsList, &QTreeWidget::itemChanged, this,
            &PdfPreviewPanel::saveHighlightNote);
    connect(m_highlightsList, &QTreeWidget::currentItemChanged, this,
            &PdfPreviewPanel::updateCommands);
    connect(m_newHighlightColors, &ColorMenu::colorChosen, this, &PdfPreviewPanel::highlightIn);
    connect(m_highlightColors, &ColorMenu::colorChosen, this,
            &PdfPreviewPanel::setHighlightColor);
}

void PdfPreviewPanel::goToHighlight(QTreeWidgetItem* item) {
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int index = m_highlightsList->indexOfTopLevelItem(item);
    if (element != nullptr && index >= 0 && index < element->highlights.size()) {
        const core::PdfHighlight& highlight = element->highlights.at(index);
        m_view->showPlace(highlight.page - 1, placeOf(highlight));
    }
}

void PdfPreviewPanel::saveHighlightNote(QTreeWidgetItem* item, int column) {
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int index = m_highlightsList->indexOfTopLevelItem(item);
    if (m_filling || column != NOTE_COLUMN || element == nullptr || index < 0 ||
        index >= element->highlights.size()) {
        return;
    }
    QList<core::PdfHighlight> highlights = element->highlights;
    highlights[index].note = item->text(NOTE_COLUMN).trimmed();
    if (highlights != element->highlights) {
        saveHighlights(highlights);
    }
}

void PdfPreviewPanel::addHighlightCommands(QMenu& menu) {
    if (!PdfFiles::highlightsOn()) {
        return;
    }
    if (m_menuHighlight >= 0) {
        menu.addSeparator();
        menu.addMenu(m_highlightColors);
        menu.addAction(m_highlightNoteAction);
        menu.addAction(m_removeHighlightAction);
        return;
    }
    menu.addAction(m_highlightAction);
    menu.addMenu(m_newHighlightColors);
}

void PdfPreviewPanel::highlight() {
    highlightIn(m_newHighlightColors->lastColor());
}

void PdfPreviewPanel::highlightIn(const QColor& color) {
    const core::ProjectElement* element = m_book.findElement(elementId());
    QList<QRectF> areas;
    for (const QRectF& area : m_view->selectionAreas()) {
        areas << rounded(area);
    }
    if (element == nullptr || m_book.isReadOnly() || !hasDocument() ||
        !PdfFiles::highlightsOn() || areas.isEmpty()) {
        return;
    }
    const int page = m_view->selectionPage() + 1;
    const QString name = (color.isValid() ? color : m_firstHighlightColor).name().toUpper();
    QList<core::PdfHighlight> highlights = element->highlights;
    const qsizetype index =
        putHighlight(highlights, page, areas, m_view->selectedText().simplified(), name);
    saveHighlights(highlights);
    // The highlight shows in place of the selection
    m_view->clearSelection();
    if (QTreeWidgetItem* item = m_highlightsList->topLevelItem(static_cast<int>(index))) {
        m_highlightsList->setCurrentItem(item);
    }
}

void PdfPreviewPanel::setHighlightColor(const QColor& color) {
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int index = highlightTarget();
    if (element == nullptr || m_book.isReadOnly() || index < 0 ||
        index >= element->highlights.size() || !color.isValid()) {
        return;
    }
    QList<core::PdfHighlight> highlights = element->highlights;
    highlights[index].color = color.name().toUpper();
    if (highlights != element->highlights) {
        saveHighlights(highlights);
    }
}

void PdfPreviewPanel::removeHighlight() {
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int index = highlightTarget();
    if (element == nullptr || m_book.isReadOnly() || index < 0 ||
        index >= element->highlights.size()) {
        return;
    }
    // A note is the writer's words: asked first
    const core::PdfHighlight& highlight = element->highlights.at(index);
    const QTreeWidgetItem* row = m_highlightsList->topLevelItem(index);
    if (!highlight.note.trimmed().isEmpty() &&
        //: In Polish: Usuń zakreślenie
        !dialogs::MessageDialog::confirm(
            this, tr("Delete Highlight"),
            //: %1: the page. In Polish: Usunąć zakreślenie ze strony %1 razem z notatką?
            tr("Delete the highlight of page %1 with its note?")
                .arg(row != nullptr ? row->text(PAGE_COLUMN) : QString::number(highlight.page)),
            tr("&Delete"))) {
        return;
    }
    QList<core::PdfHighlight> highlights = element->highlights;
    highlights.removeAt(index);
    saveHighlights(highlights);
    if (const int rows = m_highlightsList->topLevelItemCount(); rows > 0) {
        m_highlightsList->setCurrentItem(m_highlightsList->topLevelItem(std::min(index, rows - 1)));
    }
}

void PdfPreviewPanel::editHighlightNote() {
    const int index = highlightTarget();
    QTreeWidgetItem* item = m_highlightsList->topLevelItem(index);
    if (item == nullptr || m_book.isReadOnly()) {
        return;
    }
    if (m_side->isHidden()) {
        m_sideAction->setChecked(true);
    }
    m_side->setCurrentIndex(HighlightsTab);
    m_highlightsList->setCurrentItem(item);
    m_highlightsList->setFocus(Qt::OtherFocusReason);
    m_highlightsList->editItem(item, NOTE_COLUMN);
}

bool PdfPreviewPanel::highlightKey(int key) {
    switch (key) {
    case Qt::Key_Insert:
        highlight();
        return true;
    case Qt::Key_Delete:
        removeHighlight();
        return true;
    case Qt::Key_F2:
        editHighlightNote();
        return true;
    default:
        return false;
    }
}

void PdfPreviewPanel::fillHighlights() {
    m_filling = true;
    // The current row stays, so that the list shown again after a change keeps its place
    const int current = m_highlightsList->currentIndex().row();
    m_highlightsList->clear();
    const core::ProjectElement* element = m_book.findElement(elementId());
    const int count = hasDocument() ? m_document->pageCount() : 0;
    QList<PdfPagesView::Highlight> marks;
    if (element != nullptr && PdfFiles::highlightsOn()) {
        const QColor frame = palette().color(QPalette::Mid);
        for (const core::PdfHighlight& highlight : element->highlights) {
            const QString page = highlight.page >= 1 && highlight.page <= count
                                     ? pageLabel(highlight.page - 1)
                                     : QString::number(highlight.page);
            auto* item = new QTreeWidgetItem(m_highlightsList,
                                             {page, highlight.text, highlight.note});
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            const QColor color = colorOf(highlight);
            item->setIcon(TEXT_COLUMN, swatchOf(color, frame));
            item->setToolTip(TEXT_COLUMN, highlight.text);
            marks << PdfPagesView::Highlight{
                .page = highlight.page - 1, .areas = highlight.areas, .color = color};
        }
    }
    m_view->setHighlights(marks);
    const int rows = m_highlightsList->topLevelItemCount();
    if (current >= 0 && rows > 0) {
        m_highlightsList->setCurrentItem(
            m_highlightsList->topLevelItem(std::min(current, rows - 1)));
    }
    const bool none = rows == 0;
    m_highlightsList->setVisible(!none);
    m_highlightsEmpty->setVisible(none);
    m_filling = false;
    updateCommands();
}

void PdfPreviewPanel::saveHighlights(const QList<core::PdfHighlight>& highlights) {
    if (m_book.setHighlights(elementId(), highlights)) {
        fillHighlights();
        emit materialChanged(elementId());
    }
}

void PdfPreviewPanel::applyHighlightsSetting() {
    const bool on = PdfFiles::highlightsOn();
    if (!on && m_side->currentIndex() == HighlightsTab) {
        m_side->setCurrentIndex(BookmarksTab);
    }
    m_side->setTabVisible(HighlightsTab, on);
    m_highlightAction->setVisible(on);
    m_newHighlightColors->menuAction()->setVisible(on);
    m_highlightButton->setVisible(on);
    fillHighlights();
}

int PdfPreviewPanel::highlightTarget() const {
    if (m_menuHighlight >= 0) {
        return m_menuHighlight;
    }
    if (m_highlightsList == nullptr || !PdfFiles::highlightsOn()) {
        return -1;
    }
    return m_highlightsList->indexOfTopLevelItem(m_highlightsList->currentItem());
}

QColor PdfPreviewPanel::colorOf(const core::PdfHighlight& highlight) const {
    const QColor color(highlight.color);
    return color.isValid() ? color : m_firstHighlightColor;
}

}  // namespace kalahari::gui
