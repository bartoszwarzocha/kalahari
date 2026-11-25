// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_spread.cpp
/// @brief A spread: its settings in the .klh file, a new one as the Settings say, its changes
///        saved, and its status

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/spread.h>

#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <string>

using kalahari::core::BookPlace;
using kalahari::core::KindRef;
using kalahari::core::ProjectManager;
using kalahari::core::SpreadData;
using kalahari::test::ChangedSetting;

namespace {

QJsonObject jsonOf(const char* text) {
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

/// The kind of the spreads of the base package
KindRef spreadKind(const ProjectManager& pm) {
    return pm.bookTypes().findKind(QString::fromLatin1(ProjectManager::BASE_PACKAGE),
                                   QString::fromLatin1(SpreadData::KIND));
}

}  // namespace

TEST_CASE("Spread: its settings in the book file", "[core][spread]") {
    SECTION("All of them, and the fields of a later version kept") {
        const SpreadData data = SpreadData::fromJson(jsonOf(R"({
            "picture": "e-31", "area": "edge", "fit": "fill",
            "crop": { "x": 0.5, "y": -0.2, "zoom": 120 }, "gutter": 4,
            "caption": { "label": "Map", "text": "The route of the caravan" },
            "pageNumbers": true, "inContents": true, "later": 7 })"));
        CHECK(data.picture == QStringLiteral("e-31"));
        CHECK(data.toEdges());
        CHECK(data.fills());
        CHECK(data.cropX == 0.5);
        CHECK(data.cropY == -0.2);
        CHECK(data.zoom == 120);
        CHECK(data.gutter == 4.0);
        CHECK(data.caption);
        CHECK(data.captionLabel == QStringLiteral("Map"));
        CHECK(data.captionText == QStringLiteral("The route of the caravan"));
        CHECK(data.pageNumbers);
        CHECK(data.inContents);
        CHECK(data.extra.value(QStringLiteral("later")).toInt() == 7);
        CHECK(SpreadData::fromJson(data.toJson()) == data);
    }

    SECTION("Values out of their range are clamped, unknown ones are the defaults") {
        const SpreadData data = SpreadData::fromJson(jsonOf(R"({
            "area": "everywhere", "fit": "stretch", "gutter": 40,
            "crop": { "x": 3, "y": -9, "zoom": 900 } })"));
        CHECK(data.area == QLatin1String(SpreadData::MARGINS));
        CHECK(data.fit == QLatin1String(SpreadData::WHOLE));
        CHECK(data.gutter == SpreadData::MAX_GUTTER);
        CHECK(data.cropX == 1.0);
        CHECK(data.cropY == -1.0);
        CHECK(data.zoom == SpreadData::MAX_ZOOM);
        CHECK_FALSE(data.caption);
    }

    SECTION("The crop of a picture that does not fill its area is not saved") {
        SpreadData data;
        data.cropX = 0.4;
        const QJsonObject json = data.toJson();
        CHECK_FALSE(json.contains(QStringLiteral("crop")));
        CHECK_FALSE(json.contains(QStringLiteral("caption")));
        CHECK_FALSE(json.contains(QStringLiteral("picture")));
    }
}

TEST_CASE("Spread: a new one as the Settings say, its changes saved", "[core][spread]") {
    const ChangedSetting<std::string> area(SpreadData::AREA_KEY, SpreadData::EDGE);
    const ChangedSetting<double> gutter(SpreadData::GUTTER_KEY, 3.0);
    const ChangedSetting<bool> caption(SpreadData::CAPTION_KEY, true);
    const ChangedSetting<std::string> label(SpreadData::CAPTION_LABEL_KEY, "Map");

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    auto& pm = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(pm.createProject(dir.path(), QStringLiteral("Caravan"), QStringLiteral("Author"),
                             QStringLiteral("en"), false, QStringLiteral("kalahari.novel")));
    const KindRef kind = spreadKind(pm);
    REQUIRE(kind);
    CHECK(ProjectManager::canAdd(kind));

    // A spread between the chapters: no file, the settings of the Settings, a draft
    const QString chapter = pm.addElement(pm.chapterKindFor(BookPlace::Main),
                                          QStringLiteral("One"), BookPlace::Main);
    const QString id = pm.addElement(kind, QStringLiteral("The Route"), BookPlace::Main);
    REQUIRE_FALSE(id.isEmpty());
    const kalahari::core::ProjectElement* element = pm.findElement(id);
    REQUIRE(element != nullptr);
    CHECK(pm.isSpread(*element));
    CHECK_FALSE(pm.isSpread(*pm.findElement(chapter)));
    CHECK(element->file.isEmpty());
    SpreadData data = ProjectManager::spreadOf(*element);
    CHECK(data.toEdges());
    CHECK(data.gutter == 3.0);
    CHECK(data.caption);
    CHECK(data.captionLabel == QStringLiteral("Map"));
    CHECK(ProjectManager::statusOf(*element) == QStringLiteral("draft"));

    // Its settings changed: saved and told, clamped; the same again tells nothing
    int changed = 0;
    const QMetaObject::Connection told = QObject::connect(
        &pm, &ProjectManager::spreadChanged, [&changed](const QString&) { ++changed; });
    data.fit = QString::fromLatin1(SpreadData::FILL);
    data.zoom = 500;
    data.captionText = QStringLiteral("Through the dunes");
    REQUIRE(pm.setSpread(id, data));
    CHECK(changed == 1);
    CHECK(ProjectManager::spreadOf(*pm.findElement(id)).zoom == SpreadData::MAX_ZOOM);
    REQUIRE(pm.setSpread(id, data));
    CHECK(changed == 1);
    CHECK_FALSE(pm.setSpread(chapter, data));
    REQUIRE(pm.setStatus(id, QStringLiteral("final")));
    QObject::disconnect(told);

    // The book opened again has them
    const QString manifest = QDir(pm.getProjectPath()).filePath(QStringLiteral("Caravan.klh"));
    pm.closeProject(false);
    REQUIRE(pm.openProject(manifest));
    const kalahari::core::ProjectElement* again = pm.findElement(id);
    REQUIRE(again != nullptr);
    const SpreadData read = ProjectManager::spreadOf(*again);
    CHECK(read.fills());
    CHECK(read.zoom == SpreadData::MAX_ZOOM);
    CHECK(read.captionText == QStringLiteral("Through the dunes"));
    CHECK(ProjectManager::statusOf(*again) == QStringLiteral("final"));
    pm.closeProject(false);
}
