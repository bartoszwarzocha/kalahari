/// @file test_style_resolver.cpp
/// @brief Unit tests for StyleResolver (Sub-Project C WS4.4)
///
/// Tier A covers behavior that needs no database (defaults, format conversion,
/// null-DB safety, cache/reload signals). Tier B uses a temporary on-disk
/// ProjectDatabase to validate inheritance, caching and circular-inheritance
/// protection. Tier A is intentionally independent of Tier B.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <kalahari/editor/style_resolver.h>
#include <kalahari/core/project_database.h>
#include <kalahari/core/database_types.h>

#include <QColor>
#include <QDir>
#include <QFont>
#include <QObject>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QUuid>
#include <QVariantMap>

#include <filesystem>

using Catch::Approx;
using namespace kalahari::editor;
using kalahari::core::CharacterStyle;
using kalahari::core::ParagraphStyle;
using kalahari::core::ProjectDatabase;

namespace {

namespace fs = std::filesystem;

/// @brief Temporary project directory for ProjectDatabase-backed tests.
///
/// Named distinctly to avoid colliding with the TempProjectDir defined in
/// test_project_database.cpp (both compile into the same test binary).
class StyleResolverTempDir {
public:
    StyleResolverTempDir() {
        m_path = fs::temp_directory_path() /
                 ("kalahari_styleres_" +
                  QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
        fs::create_directories(m_path);
    }

    ~StyleResolverTempDir() {
        if (fs::exists(m_path)) {
            std::error_code ec;
            fs::remove_all(m_path, ec);
        }
    }

    QString path() const { return QString::fromStdString(m_path.string()); }

private:
    fs::path m_path;
};

}  // namespace

// =============================================================================
// Tier A - no database
// =============================================================================

TEST_CASE("StyleResolver default paragraph style", "[editor][style][resolver]") {
    StyleResolver resolver;
    ResolvedParagraphStyle style = resolver.defaultParagraphStyle();

    REQUIRE(style.fontFamily == QString("Segoe UI"));
    REQUIRE(style.fontSize == 12);
    REQUIRE_FALSE(style.bold);
    REQUIRE_FALSE(style.italic);
    REQUIRE(style.alignment == Qt::AlignLeft);
}

TEST_CASE("StyleResolver default character style", "[editor][style][resolver]") {
    StyleResolver resolver;
    ResolvedCharacterStyle style = resolver.defaultCharacterStyle();

    REQUIRE(style.fontFamily == QString("Segoe UI"));
    REQUIRE(style.fontSize == 12);
    REQUIRE(style.textColor == QColor(Qt::black));
    REQUIRE(style.backgroundColor == QColor(Qt::transparent));
}

TEST_CASE("StyleResolver resolves unknown ids to defaults with no database", "[editor][style][resolver]") {
    StyleResolver resolver;

    ResolvedParagraphStyle para = resolver.resolveParagraphStyle("does.not.exist");
    REQUIRE(para.fontFamily == QString("Segoe UI"));
    REQUIRE(para.fontSize == 12);

    ResolvedCharacterStyle chr = resolver.resolveCharacterStyle("does.not.exist");
    REQUIRE(chr.fontFamily == QString("Segoe UI"));
    REQUIRE(chr.fontSize == 12);
}

TEST_CASE("ResolvedParagraphStyle toFont maps font attributes", "[editor][style][resolver]") {
    ResolvedParagraphStyle style;
    style.fontFamily = "Times New Roman";
    style.fontSize = 18;
    style.bold = true;
    style.italic = true;
    style.underline = true;

    QFont font = style.toFont();
    REQUIRE(font.family() == QString("Times New Roman"));
    REQUIRE(font.pointSize() == 18);
    REQUIRE(font.bold());
    REQUIRE(font.italic());
    REQUIRE(font.underline());
}

TEST_CASE("ResolvedParagraphStyle toBlockFormat maps paragraph attributes", "[editor][style][resolver]") {
    ResolvedParagraphStyle style;
    style.alignment = Qt::AlignHCenter;
    style.firstLineIndent = 10.0;
    style.leftMargin = 5.0;
    style.rightMargin = 7.0;
    style.spaceBefore = 3.0;
    style.spaceAfter = 4.0;
    style.lineHeight = 1.5;

    QTextBlockFormat fmt = style.toBlockFormat();
    REQUIRE(fmt.alignment() == Qt::AlignHCenter);
    REQUIRE(fmt.textIndent() == Approx(10.0));
    REQUIRE(fmt.leftMargin() == Approx(5.0));
    REQUIRE(fmt.rightMargin() == Approx(7.0));
    REQUIRE(fmt.topMargin() == Approx(3.0));
    REQUIRE(fmt.bottomMargin() == Approx(4.0));
    REQUIRE(fmt.lineHeightType() == QTextBlockFormat::ProportionalHeight);
    REQUIRE(fmt.lineHeight() == Approx(150.0));
}

TEST_CASE("ResolvedParagraphStyle toCharFormat applies foreground and font", "[editor][style][resolver]") {
    ResolvedParagraphStyle style;
    style.textColor = QColor(Qt::red);
    style.fontFamily = "Georgia";
    style.fontSize = 14;

    QTextCharFormat fmt = style.toCharFormat();
    REQUIRE(fmt.foreground().color() == QColor(Qt::red));
    REQUIRE(fmt.font().family() == QString("Georgia"));
    REQUIRE(fmt.font().pointSize() == 14);
}

TEST_CASE("ResolvedCharacterStyle toCharFormat applies foreground and background", "[editor][style][resolver]") {
    ResolvedCharacterStyle style;
    style.textColor = QColor(Qt::blue);
    style.backgroundColor = QColor(Qt::yellow);

    QTextCharFormat fmt = style.toCharFormat();
    REQUIRE(fmt.foreground().color() == QColor(Qt::blue));
    REQUIRE(fmt.background().color() == QColor(Qt::yellow));
}

TEST_CASE("StyleResolver setDatabase(nullptr) is safe", "[editor][style][resolver]") {
    StyleResolver resolver;
    resolver.setDatabase(nullptr);
    REQUIRE(resolver.database() == nullptr);

    // Resolving with a null database returns defaults and does not crash.
    ResolvedParagraphStyle para = resolver.resolveParagraphStyle("anything");
    REQUIRE(para.fontFamily == QString("Segoe UI"));
}

TEST_CASE("StyleResolver cache/reload with null database emits stylesChanged", "[editor][style][resolver]") {
    StyleResolver resolver;

    // Manual signal counter (the test target does not link Qt6::Test/QSignalSpy).
    int emitted = 0;
    QObject::connect(&resolver, &StyleResolver::stylesChanged,
                     [&emitted]() { ++emitted; });

    // invalidateCache() must not crash with a null database.
    resolver.invalidateCache();

    // reloadFromDatabase() always emits stylesChanged, even without a database.
    resolver.reloadFromDatabase();
    REQUIRE(emitted == 1);
}

// =============================================================================
// Tier B - with an on-disk ProjectDatabase
// =============================================================================

TEST_CASE("StyleResolver resolves a stored paragraph style", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));
    REQUIRE(db.isOpen());

    ParagraphStyle base;
    base.id = "base";
    base.name = "Base";
    base.properties.insert("fontFamily", "Georgia");
    base.properties.insert("fontSize", 20);
    base.properties.insert("bold", true);
    db.saveParagraphStyle(base);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    ResolvedParagraphStyle resolved = resolver.resolveParagraphStyle("base");
    REQUIRE(resolved.id == QString("base"));
    REQUIRE(resolved.fontFamily == QString("Georgia"));
    REQUIRE(resolved.fontSize == 20);
    REQUIRE(resolved.bold);
}

TEST_CASE("StyleResolver applies single-level inheritance with child override", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));

    ParagraphStyle base;
    base.id = "base";
    base.name = "Base";
    base.properties.insert("fontFamily", "Georgia");
    base.properties.insert("fontSize", 20);
    base.properties.insert("bold", true);
    db.saveParagraphStyle(base);

    ParagraphStyle heading;
    heading.id = "heading1";
    heading.name = "Heading 1";
    heading.baseStyle = "base";
    heading.properties.insert("italic", true);
    heading.properties.insert("fontSize", 30);  // overrides base
    db.saveParagraphStyle(heading);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    ResolvedParagraphStyle resolved = resolver.resolveParagraphStyle("heading1");
    REQUIRE(resolved.fontFamily == QString("Georgia"));  // inherited from base
    REQUIRE(resolved.bold);                               // inherited from base
    REQUIRE(resolved.italic);                             // from child
    REQUIRE(resolved.fontSize == 30);                     // child override wins
}

TEST_CASE("StyleResolver flattens a multi-level inheritance chain", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));

    // C (grandparent) <- B (parent) <- A (child)
    ParagraphStyle styleC;
    styleC.id = "C";
    styleC.name = "C";
    styleC.properties.insert("fontFamily", "Courier");
    styleC.properties.insert("fontSize", 9);
    db.saveParagraphStyle(styleC);

    ParagraphStyle styleB;
    styleB.id = "B";
    styleB.name = "B";
    styleB.baseStyle = "C";
    styleB.properties.insert("bold", true);
    db.saveParagraphStyle(styleB);

    ParagraphStyle styleA;
    styleA.id = "A";
    styleA.name = "A";
    styleA.baseStyle = "B";
    styleA.properties.insert("italic", true);
    db.saveParagraphStyle(styleA);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    ResolvedParagraphStyle resolved = resolver.resolveParagraphStyle("A");
    REQUIRE(resolved.fontFamily == QString("Courier"));  // from C
    REQUIRE(resolved.fontSize == 9);                      // from C
    REQUIRE(resolved.bold);                               // from B
    REQUIRE(resolved.italic);                             // from A
    REQUIRE(resolved.id == QString("A"));
}

TEST_CASE("StyleResolver terminates on circular inheritance", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));

    // A -> B -> A circular chain
    ParagraphStyle styleA;
    styleA.id = "A";
    styleA.name = "A";
    styleA.baseStyle = "B";
    styleA.properties.insert("bold", true);
    db.saveParagraphStyle(styleA);

    ParagraphStyle styleB;
    styleB.id = "B";
    styleB.name = "B";
    styleB.baseStyle = "A";
    styleB.properties.insert("italic", true);
    db.saveParagraphStyle(styleB);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    // Must not hang; the visited-set guard breaks the cycle.
    ResolvedParagraphStyle resolved = resolver.resolveParagraphStyle("A");
    REQUIRE(resolved.id == QString("A"));
    REQUIRE(resolved.bold);
}

TEST_CASE("StyleResolver caches resolved styles (stale until invalidated)", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));

    ParagraphStyle base;
    base.id = "base";
    base.name = "Base";
    base.properties.insert("fontSize", 20);
    db.saveParagraphStyle(base);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    ResolvedParagraphStyle first = resolver.resolveParagraphStyle("base");
    REQUIRE(first.fontSize == 20);

    // Change the DB WITHOUT invalidating the cache.
    ParagraphStyle updated;
    updated.id = "base";
    updated.name = "Base";
    updated.properties.insert("fontSize", 40);
    db.saveParagraphStyle(updated);

    // Cache hit: still the old value.
    ResolvedParagraphStyle second = resolver.resolveParagraphStyle("base");
    REQUIRE(second.fontSize == 20);

    // After invalidation the new value is reflected.
    resolver.invalidateCache();
    ResolvedParagraphStyle third = resolver.resolveParagraphStyle("base");
    REQUIRE(third.fontSize == 40);
}

TEST_CASE("StyleResolver resolves a stored character style", "[editor][style][resolver]") {
    StyleResolverTempDir tempDir;
    ProjectDatabase db;
    REQUIRE(db.open(tempDir.path()));

    CharacterStyle emph;
    emph.id = "emph";
    emph.name = "Emphasis";
    emph.properties.insert("italic", true);
    emph.properties.insert("textColor", "#FF0000");
    db.saveCharacterStyle(emph);

    StyleResolver resolver;
    resolver.setDatabase(&db);

    ResolvedCharacterStyle resolved = resolver.resolveCharacterStyle("emph");
    REQUIRE(resolved.id == QString("emph"));
    REQUIRE(resolved.italic);
    REQUIRE(resolved.textColor == QColor("#FF0000"));
}
