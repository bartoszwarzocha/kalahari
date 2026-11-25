// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_spread_card.cpp
/// @brief The card of a spread: its settings saved, its picture and its quality for print, its
///        keys, its place among the pages of the book, and the Properties panel of it

#include <catch2/catch_test_macros.hpp>

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/material_kinds.h"
#include "kalahari/core/page_start.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/spread.h"
#include "kalahari/gui/book_page_numbers.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/spread_card.h"
#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/gui/document_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>

#include <optional>
#include <string>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::SpreadData;
using gui::SpreadCard;
using test::OpenNovel;
using test::Window;

namespace {

/// The settings a new spread and the pages take, as the test gives them, and back afterwards
struct SpreadSettings {
    test::ChangedSetting<std::string> area{SpreadData::AREA_KEY, SpreadData::MARGINS};
    test::ChangedSetting<std::string> fit{SpreadData::FIT_KEY, SpreadData::WHOLE};
    test::ChangedSetting<double> gutter{SpreadData::GUTTER_KEY, 0.0};
    test::ChangedSetting<bool> caption{SpreadData::CAPTION_KEY, false};
    test::ChangedSetting<bool> numbers{SpreadData::PAGE_NUMBERS_KEY, false};
    test::ChangedSetting<bool> contents{SpreadData::IN_CONTENTS_KEY, true};
    test::ChangedSetting<bool> mirror{"editor.margins.mirrorEnabled", false};
    test::ChangedSetting<std::string> numbering{gui::BookPageNumbers::NUMBERING_KEY,
                                                gui::BookPageNumbers::NUMBERING_BOOK};
    test::ChangedSetting<std::string> start{core::TEXTS_START_KEY, core::PAGE_START_RIGHT};
};

/// The book of the test only: its own chapter, with a short text; the texts the book type
/// adds taken out
void keepOnly(OpenNovel& novel) {
    core::ProjectManager& pm = novel.pm;
    const core::ProjectBook* book = pm.book();
    REQUIRE(book != nullptr);
    QStringList others;
    for (const QList<core::ProjectElement>* list :
         {&book->frontElements, &book->mainElements, &book->backElements}) {
        for (const core::ProjectElement& element : *list) {
            if (element.id != novel.chapterId) {
                others << element.id;
            }
        }
    }
    for (const QString& id : others) {
        const std::optional<core::ProjectElement> removed = pm.removeElement(id);
        REQUIRE(removed.has_value());
        for (const QString& file : pm.filesOf(*removed)) {
            QFile::remove(file);
        }
    }
    pm.setChapterContent(novel.chapterId, QStringLiteral("<kml><p>A short storm.</p></kml>"));
    REQUIRE(pm.saveChapterContent(novel.chapterId));
}

/// The kind of the pictures of the Workshop
core::KindRef pictureKindOf(const core::ProjectManager& pm) {
    const core::BookProject* project = pm.project();
    REQUIRE(project != nullptr);
    std::optional<core::KindRef> found;
    for (const core::KindRef& kind : project->kindsIn(pm.bookTypes(), core::BookPlace::Workshop)) {
        if (kind.kind->isMaterial() &&
            kind.kind->id == QLatin1String(core::MaterialKinds::PICTURE)) {
            found = kind;
            break;
        }
    }
    if (!found.has_value()) {
        FAIL("The Workshop has no pictures");
    }
    return *found;
}

/// The settings of spread @p id, as the book has them
SpreadData spreadIn(const core::ProjectManager& pm, const QString& id) {
    const core::ProjectElement* element = pm.findElement(id);
    REQUIRE(element != nullptr);
    return core::ProjectManager::spreadOf(*element);
}

/// Press and release @p key with @p modifiers on @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent down(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(widget, &down);
    QKeyEvent up(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(widget, &up);
}

/// Write @p text in @p field, as the writer does
void type(QLineEdit* field, const QString& text) {
    REQUIRE(field != nullptr);
    field->setText(text);
    emit field->textEdited(text);
}

}  // namespace

TEST_CASE("Spread card: its settings saved, its picture, its keys and its place in the book",
          "[gui][spread]") {
    gui::registerAllIcons();
    const SpreadSettings settings;
    QApplication::processEvents();

    OpenNovel novel(QStringLiteral("Spreads"));
    keepOnly(novel);
    core::ProjectManager& pm = novel.pm;
    const core::KindRef kind = pm.bookTypes().findKind(
        QString::fromLatin1(core::ProjectManager::BASE_PACKAGE), QString::fromLatin1(SpreadData::KIND));
    REQUIRE(kind);
    const QString id = pm.addElement(kind, QStringLiteral("The Route"), core::BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());

    Window window;
    window.navigator.onElementSelected(id, QStringLiteral("The Route"));
    auto* card = qobject_cast<SpreadCard*>(window.tabs->currentWidget());
    REQUIRE(card != nullptr);
    CHECK(card->elementId() == id);
    CHECK(card->marginsButton()->isChecked());
    CHECK(card->wholeButton()->isChecked());
    CHECK(card->qualityText().isEmpty());

    SECTION("Each field saves its setting; the gutter and the caption once they are done") {
        card->edgeButton()->setChecked(true);
        CHECK(spreadIn(pm, id).toEdges());
        card->fillButton()->setChecked(true);
        CHECK(spreadIn(pm, id).fills());
        card->gutterBox()->setStoredValue(5.0);
        CHECK(spreadIn(pm, id).gutter == 0.0);
        card->saveNow();
        CHECK(spreadIn(pm, id).gutter == 5.0);
        card->captionBox()->setChecked(true);
        type(card->captionField(), QStringLiteral("Through the dunes"));
        card->saveNow();
        card->numbersBox()->setChecked(true);
        card->contentsBox()->setChecked(false);

        const SpreadData saved = spreadIn(pm, id);
        CHECK(saved.caption);
        CHECK(saved.captionText == QStringLiteral("Through the dunes"));
        CHECK(saved.pageNumbers);
        CHECK_FALSE(saved.inContents);

        // The Properties panel shows them in short
        window.properties->showChapterProperties(id);
        auto* group = window.properties->findChild<QGroupBox*>(QStringLiteral("spreadGroup"));
        REQUIRE(group != nullptr);
        CHECK_FALSE(group->isHidden());
        auto* firstPage = window.properties->findChild<QComboBox*>(QStringLiteral("spreadFirstPage"));
        REQUIRE(firstPage != nullptr);
        CHECK_FALSE(firstPage->isHidden());
        CHECK_FALSE(firstPage->isEnabled());
        auto* picture = window.properties->findChild<QLabel*>(QStringLiteral("spreadPictureLabel"));
        REQUIRE(picture != nullptr);
        CHECK(picture->text() == QStringLiteral("none"));
    }

    SECTION("A picture of the Workshop: its quality for print; the keys move it and zoom it") {
        QTemporaryDir files;
        REQUIRE(files.isValid());
        QImage image(600, 200, QImage::Format_RGB32);
        image.fill(Qt::darkGreen);
        const QString source = QDir(files.path()).filePath(QStringLiteral("route.png"));
        REQUIRE(image.save(source));
        const QString material = pm.addMaterial(source, pictureKindOf(pm), QStringLiteral("Route"));
        REQUIRE_FALSE(material.isEmpty());
        card->refresh();

        const int index = card->pictureBox()->findData(material);
        REQUIRE(index > 0);
        card->pictureBox()->setCurrentIndex(index);
        CHECK(spreadIn(pm, id).picture == material);
        // 600 pixels over two pages: too few
        CHECK(card->qualityText().contains(QStringLiteral(" dpi – too little for print")));
        CHECK(card->qualityText().contains(QStringLiteral("blurred")));

        // The whole picture does not move; filling the pages it does
        press(card->view(), Qt::Key_Right);
        card->saveNow();
        CHECK(spreadIn(pm, id).cropX == 0.0);
        card->edgeButton()->setChecked(true);
        card->fillButton()->setChecked(true);
        press(card->view(), Qt::Key_Right);
        CHECK(spreadIn(pm, id).cropX == 0.0);  // saved once the moves stop
        card->saveNow();
        CHECK(spreadIn(pm, id).cropX < 0.0);
        press(card->view(), Qt::Key_BracketRight, Qt::ControlModifier);
        card->saveNow();
        CHECK(spreadIn(pm, id).zoom == SpreadData::MIN_ZOOM + SpreadCard::ZOOM_STEP);
        press(card->view(), Qt::Key_Home);
        CHECK(spreadIn(pm, id).cropX == 0.0);
        CHECK(spreadIn(pm, id).zoom == SpreadData::MIN_ZOOM);

        window.properties->showChapterProperties(id);
        auto* picture = window.properties->findChild<QLabel*>(QStringLiteral("spreadPictureLabel"));
        REQUIRE(picture != nullptr);
        CHECK(picture->text() == QStringLiteral("Route"));

        // A move still waiting is saved before the book closes
        press(card->view(), Qt::Key_Left);
        const QString manifest =
            QDir(pm.getProjectPath()).filePath(QStringLiteral("Spreads.klh"));
        pm.closeProject(false);
        REQUIRE(pm.openProject(manifest));
        CHECK(spreadIn(pm, id).cropX > 0.0);
    }

    SECTION("Its place among the pages: a left page and the right one, moved as in the Navigator") {
        gui::BookPageNumbers& numbers = window.navigator.pageNumbers();
        numbers.countNow();
        // The Storm on page 1, the spread on pages 2 and 3
        const std::optional<gui::TextFirstPage> first = numbers.firstPageOf(id);
        REQUIRE(first.has_value());
        CHECK(first->index == 1);
        CHECK(card->headText() == QStringLiteral("The Route · spread · pages 2–3"));
        CHECK(card->placeText().startsWith(QStringLiteral("pages 2–3")));
        CHECK(card->placeText().contains(QStringLiteral("no empty pages beside it")));

        window.properties->showChapterProperties(id);
        auto* pages = window.properties->findChild<QLabel*>(QStringLiteral("chapterPagesLabel"));
        REQUIRE(pages != nullptr);
        CHECK(pages->text() == QStringLiteral("2–3"));

        // Earlier: before The Storm, the first page of the book left empty before it
        REQUIRE(card->earlierButton()->isEnabled());
        CHECK_FALSE(card->laterButton()->isEnabled());
        card->earlierButton()->click();
        REQUIRE(pm.book() != nullptr);
        CHECK(pm.book()->mainElements.front().id == id);
        CHECK_FALSE(card->earlierButton()->isEnabled());
        CHECK(card->laterButton()->isEnabled());
        numbers.countNow();
        CHECK(numbers.firstPageOf(id)->index == 1);
        CHECK(card->placeText().contains(QStringLiteral("before it the empty page 1")));
    }
}
