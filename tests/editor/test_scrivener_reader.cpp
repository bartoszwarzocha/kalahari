// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_scrivener_reader.cpp
/// @brief Import of a project of Scrivener: its binder and metadata, the texts of its draft

#include <catch2/catch_test_macros.hpp>
#include <kalahari/editor/scrivener_import.h>
#include "test_support/scrivener_project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimeZone>

using namespace kalahari::editor;
using kalahari::test::makeScrivenerProject;

namespace {

void write(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(data);
}

QString textOf(const ImportDocument& document) {
    QStringList lines;
    for (const ImportBlock& block : document.blocks) {
        lines << importPlainText(block);
    }
    return lines.join(QLatin1Char('\n'));
}

}  // namespace

TEST_CASE("Import Scrivener: the project file of its folder", "[import][scrivener]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString project = makeScrivenerProject(folder.path());
    const QString file = QDir(project).filePath(QStringLiteral("Karawana.scrivx"));
    CHECK(scrivenerProjectFile(project) == file);
    CHECK(scrivenerProjectFile(file) == file);
    CHECK(scrivenerProjectFile(folder.path()).isEmpty());

    ScrivenerProject read;
    ImportProblem problem;
    CHECK_FALSE(readScrivenerProject(folder.path(), &read, &problem));
    CHECK(problem.kind == ImportProblem::Kind::NotScrivener);

    // A file of the binder that is not XML of a binder
    const QString damaged = QDir(folder.path()).filePath(QStringLiteral("Zepsuty.scriv"));
    write(QDir(damaged).filePath(QStringLiteral("Zepsuty.scrivx")), "<ScrivenerProject><Binder>");
    CHECK_FALSE(readScrivenerProject(damaged, &read, &problem));
    CHECK(problem.kind == ImportProblem::Kind::Damaged);
    write(QDir(damaged).filePath(QStringLiteral("Zepsuty.scrivx")), "<Other/>");
    CHECK_FALSE(readScrivenerProject(damaged, &read, &problem));
    CHECK(problem.kind == ImportProblem::Kind::Damaged);
}

TEST_CASE("Import Scrivener: the binder and the metadata of its documents", "[import][scrivener]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = makeScrivenerProject(folder.path());
    ScrivenerProject project;
    ImportProblem problem;
    REQUIRE(readScrivenerProject(path, &project, &problem));

    CHECK(project.title == QStringLiteral("Karawana"));
    CHECK(project.author == QStringLiteral("Jan Kowalski"));
    CHECK(project.draftName == QStringLiteral("Manuscript"));
    CHECK(project.trash == 1);
    REQUIRE(project.draft.size() == 3);

    const ScrivenerItem& part = project.draft[0];
    CHECK(part.kind == ScrivenerItem::Kind::Folder);
    CHECK(part.title == QStringLiteral("Część pierwsza"));
    CHECK(part.label == QStringLiteral("Blue"));
    CHECK(part.status.isEmpty());
    CHECK(part.hasText());
    REQUIRE(part.children.size() == 2);

    const ScrivenerItem& chapter = part.children[0];
    CHECK(chapter.synopsis == QStringLiteral("Rozdział o wyjeździe."));
    CHECK_FALSE(chapter.hasText());
    REQUIRE(chapter.children.size() == 3);
    const ScrivenerItem& scene = chapter.children[0];
    CHECK(scene.kind == ScrivenerItem::Kind::Text);
    CHECK(scene.label == QStringLiteral("Red"));
    CHECK(scene.status == QStringLiteral("Revised Draft"));
    CHECK(scene.keywords == QStringList{QStringLiteral("pustynia"), QStringLiteral("noc")});
    CHECK(scene.synopsis == QStringLiteral("Karawana wyrusza."));
    CHECK(scene.notes == QStringLiteral("Notatka do sceny.\nDruga linia."));
    CHECK(scene.compiled);
    CHECK_FALSE(chapter.children[2].compiled);

    // The comment and the footnote the text links to
    REQUIRE(scene.text.annotations.size() == 1);
    CHECK(scene.text.annotations[0].text == QStringLiteral("Sprawdź datę."));
    REQUIRE(scene.text.notes.size() == 1);
    CHECK(importPlainText(scene.text.notes[0].paragraphs.front()) == QStringLiteral("Przypis sceny."));

    // The snapshots, from the oldest, each with the file of its time
    const ScrivenerItem& second = part.children[1];
    CHECK(second.status == QStringLiteral("Final Draft"));
    REQUIRE(second.snapshots.size() == 2);
    CHECK(second.snapshots[0].title == QStringLiteral("Pierwsza wersja"));
    CHECK(textOf(second.snapshots[0].text) == QStringLiteral("Stary tekst."));
    CHECK(second.snapshots[0].created ==
          QDateTime(QDate(2025, 10, 27), QTime(2, 20, 40), QTimeZone(-7 * 3600)));
    CHECK(second.snapshots[1].title == QStringLiteral("Po redakcji"));
    CHECK(textOf(second.snapshots[1].text) == QStringLiteral("Nowszy tekst."));

    // The research, with the other folders at the top of the binder
    REQUIRE(project.research.size() == 3);
    const ScrivenerItem& map = project.research[0];
    CHECK(map.kind == ScrivenerItem::Kind::File);
    CHECK(map.type == QStringLiteral("Image"));
    CHECK(map.file.endsWith(QStringLiteral("Files/Data/IMG/content.png")));
    CHECK(textOf(project.research[1].text) == QStringLiteral("Oaza leży na południu."));
    CHECK(project.research[2].title == QStringLiteral("Characters"));
    REQUIRE(project.research[2].children.size() == 1);
    CHECK(project.research[2].children[0].title == QStringLiteral("Anna"));

    // The .scrivx file read as its folder
    ScrivenerProject again;
    REQUIRE(readScrivenerProject(QDir(path).filePath(QStringLiteral("Karawana.scrivx")), &again, &problem));
    CHECK(again.title == QStringLiteral("Karawana"));
    CHECK(again.draft.size() == 3);
}

TEST_CASE("Import Scrivener: the texts and parts of the draft", "[import][scrivener]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    ScrivenerProject project;
    ImportProblem problem;
    REQUIRE(readScrivenerProject(makeScrivenerProject(folder.path()), &project, &problem));

    const ScrivenerDraft draft = scrivenerDraft(project, ScrivenerDraftOptions());
    CHECK(draft.document.title == QStringLiteral("Karawana"));
    CHECK(draft.excluded == 1);
    CHECK(draft.sceneSnapshots == 0);
    CHECK(textOf(draft.document) ==
          QStringLiteral("Część pierwsza\nWstęp do części.\nRozdział 1\nPierwsza scena.\n\nPodtytuł\n"
                         "Druga scena.\nRozdział 2\nTekst rozdziału drugiego.\nEpilog\nKoniec."));
    REQUIRE(draft.document.blocks.size() == 11);
    CHECK(draft.document.blocks[0].headingLevel == 1);
    CHECK(draft.document.blocks[2].headingLevel == 2);
    CHECK(draft.document.blocks[4].kind == ImportBlock::Kind::SceneBreak);
    CHECK(draft.document.headingLevelOf(draft.document.blocks[5]) == 3);  // the scene's own
    CHECK(draft.document.blocks[7].headingLevel == 2);

    REQUIRE(draft.texts.size() == 5);
    const ScrivenerText& part = draft.texts[0];
    CHECK(part.piece.part);
    CHECK(part.piece.title == QStringLiteral("Część pierwsza"));
    CHECK(part.label == QStringLiteral("Blue"));

    const ScrivenerText& intro = draft.texts[1];
    CHECK_FALSE(intro.piece.part);
    CHECK(intro.piece.inPart);
    CHECK(intro.piece.title == QStringLiteral("Część pierwsza"));
    CHECK(intro.piece.from == 1);
    CHECK(intro.piece.to == 2);
    CHECK(intro.label.isEmpty());

    // A text of scenes: the synopsis of its folder, the least done status of its scenes
    const ScrivenerText& first = draft.texts[2];
    CHECK(first.piece.inPart);
    CHECK(first.piece.title == QStringLiteral("Rozdział 1"));
    CHECK(first.piece.from == 3);
    CHECK(first.piece.to == 7);
    CHECK(first.piece.words == 7);  // with the words of the footnote
    CHECK(first.synopsis == QStringLiteral("Rozdział o wyjeździe."));
    CHECK(first.status == QStringLiteral("First Draft"));
    REQUIRE(first.scenes.size() == 2);
    CHECK(first.scenes[0].title == QStringLiteral("Scena 1"));
    CHECK(first.scenes[0].synopsis == QStringLiteral("Karawana wyrusza."));
    CHECK(first.scenes[0].keywords.size() == 2);
    CHECK(first.snapshots.empty());
    CHECK(draft.document.annotations.size() == 1);
    CHECK(draft.document.notes.size() == 1);
    CHECK(draft.document.style(QStringLiteral("s1")) != nullptr);

    const ScrivenerText& second = draft.texts[3];
    CHECK(second.piece.title == QStringLiteral("Rozdział 2"));
    CHECK(second.status == QStringLiteral("Final Draft"));
    CHECK(second.snapshots.size() == 2);
    CHECK(second.scenes.empty());

    // A text at the top after the part is not in it; an empty folder is left out
    const ScrivenerText& epilogue = draft.texts[4];
    CHECK(epilogue.piece.title == QStringLiteral("Epilog"));
    CHECK_FALSE(epilogue.piece.inPart);
    CHECK(epilogue.piece.to == draft.document.blocks.size());

    // All the documents, also those not Included in Compile
    ScrivenerDraftOptions all;
    all.compiledOnly = false;
    const ScrivenerDraft whole = scrivenerDraft(project, all);
    CHECK(whole.excluded == 0);
    REQUIRE(whole.texts.size() == 5);
    CHECK(whole.texts[2].scenes.size() == 3);
}

TEST_CASE("Import Scrivener: the statuses of the book", "[import][scrivener]") {
    CHECK(scrivenerBookStatus(QStringLiteral("To Do")) == QStringLiteral("draft"));
    CHECK(scrivenerBookStatus(QStringLiteral("First Draft")) == QStringLiteral("draft"));
    CHECK(scrivenerBookStatus(QStringLiteral(" revised  draft ")) == QStringLiteral("revision"));
    CHECK(scrivenerBookStatus(QStringLiteral("Final Draft")) == QStringLiteral("final"));
    CHECK(scrivenerBookStatus(QStringLiteral("Done")) == QStringLiteral("final"));
    CHECK(scrivenerBookStatus(QStringLiteral("No Status")).isEmpty());
    CHECK(scrivenerBookStatus(QStringLiteral("Do sprawdzenia")).isEmpty());
}

TEST_CASE("Import Scrivener: a document appended to another", "[import][scrivener]") {
    const auto documentOf = [](const QString& styleName, const QString& text) {
        ImportDocument document;
        ImportStyle style;
        style.key = QStringLiteral("s1");
        style.name = styleName;
        style.paragraphs = 1;
        document.styles.push_back(style);
        ImportBlock block;
        block.style = QStringLiteral("s1");
        block.list = 0;
        ImportRun run;
        run.text = text;
        run.annotations = {0};
        block.runs.push_back(run);
        ImportRun note;
        note.kind = ImportRun::Kind::Note;
        note.index = 0;
        block.runs.push_back(note);
        ImportRun image;
        image.kind = ImportRun::Kind::Image;
        image.index = 0;
        block.runs.push_back(image);
        document.blocks.push_back(block);
        document.notes.emplace_back();
        document.images.emplace_back();
        document.annotations.emplace_back();
        document.lists.push_back(ListDefinition::bulleted());
        document.omissions[ImportOmission::Shape] = 1;
        return document;
    };
    ImportDocument into = documentOf(QStringLiteral("Cytat"), QStringLiteral("A"));
    appendImportDocument(into, documentOf(QStringLiteral("Cytat"), QStringLiteral("B")));
    appendImportDocument(into, documentOf(QStringLiteral("Motto"), QStringLiteral("C")));

    REQUIRE(into.blocks.size() == 3);
    REQUIRE(into.styles.size() == 2);
    CHECK(into.styles[0].paragraphs == 2);
    CHECK(into.styles[1].key == QStringLiteral("s1_2"));
    CHECK(into.styles[1].name == QStringLiteral("Motto"));
    CHECK(into.blocks[1].style == QStringLiteral("s1"));
    CHECK(into.blocks[2].style == QStringLiteral("s1_2"));
    const ImportBlock& last = into.blocks[2];
    CHECK(last.list == 2);
    CHECK(last.runs[0].annotations == std::vector<int>{2});
    CHECK(last.runs[1].index == 2);
    CHECK(last.runs[2].index == 2);
    CHECK(into.notes.size() == 3);
    CHECK(into.images.size() == 3);
    CHECK(into.annotations.size() == 3);
    CHECK(into.lists.size() == 3);
    CHECK(into.omissions.at(ImportOmission::Shape) == 3);
}

TEST_CASE("Import Scrivener: the scenes divided by their titles or not at all", "[import][scrivener]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    ScrivenerProject project;
    ImportProblem problem;
    REQUIRE(readScrivenerProject(makeScrivenerProject(folder.path()), &project, &problem));

    // The titles of the scenes as headings of level 3, their own headings a level lower
    ScrivenerDraftOptions titles;
    titles.scenes = ScrivenerDraftOptions::Scenes::Titles;
    const ScrivenerDraft titled = scrivenerDraft(project, titles);
    CHECK(textOf(titled.document) ==
          QStringLiteral("Część pierwsza\nWstęp do części.\nRozdział 1\nScena 1\nPierwsza scena.\n"
                         "Scena 2\nPodtytuł\nDruga scena.\nRozdział 2\nTekst rozdziału drugiego.\n"
                         "Epilog\nKoniec."));
    REQUIRE(titled.document.blocks.size() == 12);
    CHECK(titled.document.blocks[3].headingLevel == 3);
    CHECK(titled.document.blocks[5].headingLevel == 3);
    CHECK(titled.document.headingLevelOf(titled.document.blocks[6]) == 4);
    CHECK(titled.document.blocks[8].headingLevel == 2);
    REQUIRE(titled.texts.size() == 5);
    CHECK(titled.texts[2].piece.from == 3);
    CHECK(titled.texts[2].piece.to == 8);

    // Nothing between the scenes
    ScrivenerDraftOptions none;
    none.scenes = ScrivenerDraftOptions::Scenes::None;
    const ScrivenerDraft joined = scrivenerDraft(project, none);
    CHECK(textOf(joined.document) ==
          QStringLiteral("Część pierwsza\nWstęp do części.\nRozdział 1\nPierwsza scena.\nPodtytuł\n"
                         "Druga scena.\nRozdział 2\nTekst rozdziału drugiego.\nEpilog\nKoniec."));
    for (const ImportBlock& block : joined.document.blocks) {
        CHECK(block.kind != ImportBlock::Kind::SceneBreak);
    }
    CHECK(joined.document.headingLevelOf(joined.document.blocks[4]) == 3);
}

TEST_CASE("Import Scrivener: the headings of a snapshot as low as those of its text",
          "[import][scrivener]") {
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString path = makeScrivenerProject(folder.path());
    // A snapshot of "Rozdział 2" with a heading of its own
    write(QDir(path).filePath(QStringLiteral("Snapshots/C2.snapshots/2025-10-27-02-20-40-0700.rtf")),
          R"RTF({\rtf1\ansi{\stylesheet{\s0 Normal;}{\s1\b heading 1;}}\pard\s1 Stary podtytu\'b3\par\pard Stary tekst.\par})RTF");
    ScrivenerProject project;
    ImportProblem problem;
    REQUIRE(readScrivenerProject(path, &project, &problem));

    const ScrivenerDraft draft = scrivenerDraft(project, ScrivenerDraftOptions());
    REQUIRE(draft.texts.size() == 5);
    const std::vector<ScrivenerSnapshot>& snapshots = draft.texts[3].snapshots;
    REQUIRE(snapshots.size() == 2);
    const ImportDocument& old = snapshots[0].text;
    REQUIRE(old.blocks.size() == 2);
    CHECK(old.headingLevelOf(old.blocks[0]) == 3);

    ScrivenerDraftOptions titles;
    titles.scenes = ScrivenerDraftOptions::Scenes::Titles;
    const ScrivenerDraft titled = scrivenerDraft(project, titles);
    const ImportDocument& lower = titled.texts[3].snapshots[0].text;
    CHECK(lower.headingLevelOf(lower.blocks[0]) == 4);
}
