// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_publishing_set.cpp
/// @brief The set for a publisher as the fourth source of the export, with the headings of its
///        parts

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/gui/publishing_test_utils.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/editor/book_export.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::CoverMaterial;
using core::PublishingTexts;

namespace {

/// The texts of the set of the open book
struct SetTexts {
    QString first;     ///< The sentence chosen
    QString second;    ///< Another sentence
    QString synopsis;  ///< The synopsis chosen
    QString blurb;     ///< The description of the cover chosen for it
};

/// Two sentences, the first chosen, a synopsis and a description of the cover, both chosen;
/// no tagline and no note about the author
SetTexts addSetTexts(core::ProjectManager& pm) {
    SetTexts texts;
    texts.first = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 1"),
        QStringLiteral("When the well runs dry, Leila must find water."));
    texts.second = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 2"), QStringLiteral("Sand."));
    texts.synopsis = test::addPublishingText(
        PublishingTexts::SYNOPSIS, QStringLiteral("Synopsis – version 1"),
        QStringLiteral("Leila crosses the desert."));
    texts.blurb = test::addCoverText(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                     QStringLiteral("A road through the sand."));
    REQUIRE(pm.setPublishingState(texts.first, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setPublishingState(texts.synopsis, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setCoverState(texts.blurb, QString::fromLatin1(CoverMaterial::CHOSEN)));
    return texts;
}

}  // namespace

TEST_CASE("Set for the publisher: the fourth source of the export, with the headings of its parts",
          "[gui][publishing][export]") {
    using gui::dialogs::ExportBookDialog;
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    (void)addSetTexts(pm);
    test::Window window;
    const gui::BookExportCoordinator coordinator(
        pm, test::commands(), window.tabs, []() -> gui::EditorPanel* { return nullptr; },
        nullptr, nullptr, &window.window);

    // The window of the export: the set as one text, with the parts that have a text
    const ExportBookDialog::Book shown = coordinator.dialogBook();
    CHECK(shown.setTitle == QStringLiteral("Sand – set for the publisher"));
    CHECK(shown.setParts == 3);

    // The title of the book with its author, then each part under its name
    ExportBookDialog::Choice choice;
    choice.content = ExportBookDialog::Content::Set;
    choice.format = editor::ExportFormat::Markdown;
    const editor::BookExport book = coordinator.bookExport(choice);
    REQUIRE(book.items.size() == 4);
    CHECK(book.items[0].title == QStringLiteral("Sand"));
    CHECK(book.items[0].titleStyle == QStringLiteral("heading_1"));
    CHECK(book.items[0].kml.contains(QStringLiteral("Author")));
    CHECK(book.items[1].title == QStringLiteral("Sentence about the book"));
    CHECK(book.items[1].titleStyle == QStringLiteral("heading_2"));
    CHECK(book.items[1].kml.contains(
        QStringLiteral("When the well runs dry, Leila must find water.")));
    CHECK(book.items[2].title == QStringLiteral("Cover description"));
    CHECK(book.items[3].title == QStringLiteral("Synopsis"));

    // A Markdown file: the headings of the parts
    const QTemporaryDir dir;
    const QString path = QDir(dir.path()).filePath(QStringLiteral("set.md"));
    editor::ExportResult result;
    editor::ExportProblem problem;
    REQUIRE(editor::exportMarkdown(book, path, QString(), {}, &result, &problem));
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(file.readAll());
    INFO(text.toStdString());
    CHECK(text.startsWith(QStringLiteral("# Sand\n\nAuthor\n\n## Sentence about the book\n\n"
                                         "When the well runs dry, Leila must find water.\n")));
    CHECK(text.contains(QStringLiteral("## Cover description\n\nA road through the sand.\n")));
    CHECK(text.contains(QStringLiteral("## Synopsis\n\nLeila crosses the desert.\n")));

    // A part left out of the set is not in the file
    REQUIRE(pm.setPublishingSet(pm.publishingSet().withPart(QStringLiteral("synopsis"), false)));
    CHECK(coordinator.bookExport(choice).items.size() == 3);
    CHECK(coordinator.dialogBook().setParts == 2);
}
