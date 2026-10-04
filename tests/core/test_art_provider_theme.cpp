/// @file test_art_provider_theme.cpp
/// @brief Regression tests for the icon stack's theme-change ordering (BUG-1).
///
/// Background: toolbar and menu icons kept their old colors after a theme switch.
/// Root cause was an ORDERING defect, not a color-computation one. ArtProvider and
/// IconRegistry were each connected to ThemeManager::themeChanged independently;
/// same-thread direct connections fire in registration order, and ArtProvider was
/// registered first. So ArtProvider refreshed every managed QAction while
/// IconRegistry still held the previous theme's colors and a warm cache -- the
/// refresh re-applied stale-coloured icons, and IconRegistry's later update had
/// nothing left to re-trigger.
///
/// The fix makes ArtProvider::onThemeChanged drive IconRegistry itself, so the
/// order is explicit in code rather than implicit in connect() registration order.
/// These tests pin that ordering.
///
/// A SECOND, independent defect had the same symptom: CommandRegistry created every
/// toolbar/menu action via ArtProvider::createAction (which stores a {"cmdId",
/// "context"} map in the action's data for refreshAction to read), then immediately
/// overwrote that data with a bare command-id QString. refreshAction then read an
/// empty map and bailed, so toolbar/menu icons never re-themed regardless of the
/// ordering fix. The last test pins that CommandRegistry actions keep the map.

#include <catch2/catch_test_macros.hpp>

#include <kalahari/core/art_provider.h>
#include <kalahari/core/icon_registry.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/gui/command.h>
#include <kalahari/gui/command_registry.h>

#include <QAction>
#include <QColor>
#include <QObject>
#include <QVariant>
#include <QVariantMap>

#include <string>

using kalahari::core::ArtProvider;
using kalahari::core::IconRegistry;
using kalahari::core::ThemeManager;

// Without this, a failed color comparison reports "{?} == {?}", which says nothing
// about which color was actually applied.
namespace Catch {
template <>
struct StringMaker<QColor> {
    static std::string convert(const QColor& color) {
        return color.isValid() ? color.name(QColor::HexRgb).toStdString() : "<invalid QColor>";
    }
};
} // namespace Catch

TEST_CASE("IconRegistry holds the new theme colors before icons refresh",
          "[core][art_provider][theme][regression]") {
    // Distinct from any theme default, so a stale value is unambiguous.
    const QColor TEST_PRIMARY("#ff00aa");
    const QColor TEST_SECONDARY("#00ffcc");

    // resetSingletons() runs before every test case (see tests/test_main.cpp) and
    // re-runs ArtProvider::initialize(), so the themeChanged connection exists and
    // IconRegistry is synchronized with the current theme.
    auto& art = ArtProvider::getInstance();
    auto& registry = IconRegistry::getInstance();
    auto& themes = ThemeManager::getInstance();

    REQUIRE(registry.getThemeConfig().primaryColor != TEST_PRIMARY);

    // Capture what IconRegistry holds AT THE MOMENT icons are told to refresh.
    // This is the whole bug: the value seen here used to be the OLD color.
    int refreshCount = 0;
    QColor primarySeenByRefresh;
    QColor secondarySeenByRefresh;

    QMetaObject::Connection conn = QObject::connect(
        &art, &ArtProvider::resourcesChanged, &art, [&]() {
            ++refreshCount;
            primarySeenByRefresh = registry.getThemeConfig().primaryColor;
            secondarySeenByRefresh = registry.getThemeConfig().secondaryColor;
        });

    themes.applyColorOverrides({{"primary", TEST_PRIMARY}, {"secondary", TEST_SECONDARY}});

    QObject::disconnect(conn);

    SECTION("a theme change reaches ArtProvider") {
        REQUIRE(refreshCount >= 1);
    }

    SECTION("icons are refreshed against the new colors, not the previous ones") {
        REQUIRE(refreshCount >= 1);
        CHECK(primarySeenByRefresh == TEST_PRIMARY);
        CHECK(secondarySeenByRefresh == TEST_SECONDARY);
    }

    SECTION("the registry keeps the new colors after the change settles") {
        CHECK(registry.getThemeConfig().primaryColor == TEST_PRIMARY);
        CHECK(registry.getThemeConfig().secondaryColor == TEST_SECONDARY);
    }

    themes.resetColorOverrides();
}

TEST_CASE("CommandRegistry actions keep ArtProvider's icon-refresh data",
          "[core][art_provider][theme][regression]") {
    // resetSingletons() runs CommandRegistry::clear() before every test case, so
    // this registration does not leak into other tests.
    auto& registry = kalahari::gui::CommandRegistry::getInstance();

    kalahari::gui::Command cmd;
    cmd.id = "test.regression.themedAction";
    cmd.label = "Themed Action";
    registry.registerCommand(cmd);

    QAction* action = registry.getAction(QString::fromStdString(cmd.id));
    REQUIRE(action != nullptr);

    // refreshAction repaints the icon on resourcesChanged by reading a QVariantMap
    // {"cmdId", "context"} out of the action's data. If CommandRegistry (or anyone)
    // overwrites that data with a bare value, refreshAction reads an empty map and
    // bails -- the icon never re-themes. Pin that the map survives action creation.
    const QVariantMap data = action->data().toMap();
    CHECK(data.value("cmdId").toString() == QString::fromStdString(cmd.id));
    CHECK(data.contains("context"));
}
