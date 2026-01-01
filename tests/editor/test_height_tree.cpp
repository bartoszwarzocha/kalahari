/// @file test_height_tree.cpp
/// @brief Unit tests for HeightTree (Fenwick tree for paragraph heights)

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <kalahari/editor/height_tree.h>

using namespace kalahari::editor;
using Catch::Matchers::WithinRel;
using Catch::Matchers::WithinAbs;

// =============================================================================
// Construction and Basic Operations
// =============================================================================

TEST_CASE("HeightTree construction", "[editor][height_tree]") {
    SECTION("Default constructor creates empty tree") {
        HeightTree tree;
        REQUIRE(tree.empty());
        REQUIRE(tree.size() == 0);
        REQUIRE(tree.totalHeight() == 0.0);
    }

    SECTION("Construct with size and default height") {
        HeightTree tree(100, 24.0);
        REQUIRE(tree.size() == 100);
        REQUIRE_FALSE(tree.empty());
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(2400.0, 0.001));
    }

    SECTION("Construct with size and zero height") {
        HeightTree tree(50, 0.0);
        REQUIRE(tree.size() == 50);
        REQUIRE(tree.totalHeight() == 0.0);
    }
}

TEST_CASE("HeightTree resize", "[editor][height_tree]") {
    SECTION("Resize from empty") {
        HeightTree tree;
        tree.resize(10, 20.0);
        REQUIRE(tree.size() == 10);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(200.0, 0.001));
    }

    SECTION("Resize to larger") {
        HeightTree tree(5, 10.0);
        tree.resize(10, 15.0);
        REQUIRE(tree.size() == 10);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(150.0, 0.001));
    }

    SECTION("Resize to smaller") {
        HeightTree tree(10, 10.0);
        tree.resize(5, 20.0);
        REQUIRE(tree.size() == 5);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(100.0, 0.001));
    }

    SECTION("Resize to zero") {
        HeightTree tree(10, 10.0);
        tree.resize(0, 0.0);
        REQUIRE(tree.empty());
        REQUIRE(tree.totalHeight() == 0.0);
    }
}

TEST_CASE("HeightTree clear", "[editor][height_tree]") {
    HeightTree tree(100, 24.0);
    tree.clear();
    REQUIRE(tree.empty());
    REQUIRE(tree.size() == 0);
    REQUIRE(tree.totalHeight() == 0.0);
}

// =============================================================================
// Height Get/Set Operations
// =============================================================================

TEST_CASE("HeightTree height operations", "[editor][height_tree]") {
    SECTION("Get individual heights") {
        HeightTree tree(5, 10.0);
        for (size_t i = 0; i < 5; ++i) {
            REQUIRE_THAT(tree.height(i), WithinAbs(10.0, 0.001));
        }
    }

    SECTION("Set height updates correctly") {
        HeightTree tree(5, 10.0);
        tree.setHeight(2, 30.0);

        REQUIRE_THAT(tree.height(0), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.height(1), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.height(2), WithinAbs(30.0, 0.001));
        REQUIRE_THAT(tree.height(3), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.height(4), WithinAbs(10.0, 0.001));
    }

    SECTION("Set height updates total") {
        HeightTree tree(5, 10.0);  // Total = 50
        tree.setHeight(2, 30.0);   // Total = 10+10+30+10+10 = 70
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(70.0, 0.001));
    }

    SECTION("Set height to zero") {
        HeightTree tree(5, 10.0);
        tree.setHeight(2, 0.0);
        REQUIRE_THAT(tree.height(2), WithinAbs(0.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(40.0, 0.001));
    }

    SECTION("Height out of range throws") {
        HeightTree tree(5, 10.0);
        REQUIRE_THROWS_AS(tree.height(5), std::out_of_range);
        REQUIRE_THROWS_AS(tree.height(100), std::out_of_range);
    }

    SECTION("SetHeight out of range throws") {
        HeightTree tree(5, 10.0);
        REQUIRE_THROWS_AS(tree.setHeight(5, 10.0), std::out_of_range);
        REQUIRE_THROWS_AS(tree.setHeight(100, 10.0), std::out_of_range);
    }
}

// =============================================================================
// Prefix Sum Tests
// =============================================================================

TEST_CASE("HeightTree prefixSum", "[editor][height_tree]") {
    SECTION("Prefix sum of empty tree") {
        HeightTree tree;
        REQUIRE(tree.prefixSum(0) == 0.0);
        REQUIRE(tree.prefixSum(1) == 0.0);
    }

    SECTION("Prefix sum boundaries") {
        HeightTree tree(5, 10.0);
        REQUIRE(tree.prefixSum(0) == 0.0);          // Sum of nothing
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(10.0, 0.001));  // height[0]
        REQUIRE_THAT(tree.prefixSum(5), WithinAbs(50.0, 0.001));  // All
    }

    SECTION("Prefix sum equals totalHeight for size()") {
        HeightTree tree(100, 24.0);
        REQUIRE_THAT(tree.prefixSum(100), WithinAbs(tree.totalHeight(), 0.001));
    }

    SECTION("Prefix sum with varying heights") {
        HeightTree tree(5, 0.0);
        tree.setHeight(0, 10.0);
        tree.setHeight(1, 20.0);
        tree.setHeight(2, 30.0);
        tree.setHeight(3, 40.0);
        tree.setHeight(4, 50.0);

        REQUIRE(tree.prefixSum(0) == 0.0);
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(10.0, 0.001));   // 10
        REQUIRE_THAT(tree.prefixSum(2), WithinAbs(30.0, 0.001));   // 10+20
        REQUIRE_THAT(tree.prefixSum(3), WithinAbs(60.0, 0.001));   // 10+20+30
        REQUIRE_THAT(tree.prefixSum(4), WithinAbs(100.0, 0.001));  // 10+20+30+40
        REQUIRE_THAT(tree.prefixSum(5), WithinAbs(150.0, 0.001));  // all
    }

    SECTION("Prefix sum beyond size returns total") {
        HeightTree tree(5, 10.0);
        REQUIRE_THAT(tree.prefixSum(10), WithinAbs(50.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(100), WithinAbs(50.0, 0.001));
    }

    SECTION("Prefix sum after setHeight") {
        HeightTree tree(5, 10.0);
        tree.setHeight(2, 100.0);

        REQUIRE(tree.prefixSum(0) == 0.0);
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(2), WithinAbs(20.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(3), WithinAbs(120.0, 0.001));  // 10+10+100
        REQUIRE_THAT(tree.prefixSum(5), WithinAbs(140.0, 0.001));  // 10+10+100+10+10
    }
}

// =============================================================================
// findIndexForY Tests
// =============================================================================

TEST_CASE("HeightTree findIndexForY", "[editor][height_tree]") {
    SECTION("Empty tree returns 0") {
        HeightTree tree;
        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(100.0) == 0);
    }

    SECTION("Negative Y returns 0") {
        HeightTree tree(5, 10.0);
        REQUIRE(tree.findIndexForY(-10.0) == 0);
        REQUIRE(tree.findIndexForY(-0.001) == 0);
    }

    SECTION("Y at 0 returns 0") {
        HeightTree tree(5, 10.0);
        REQUIRE(tree.findIndexForY(0.0) == 0);
    }

    SECTION("Y beyond total returns size()") {
        HeightTree tree(5, 10.0);  // Total = 50
        REQUIRE(tree.findIndexForY(50.0) == 5);
        REQUIRE(tree.findIndexForY(100.0) == 5);
        REQUIRE(tree.findIndexForY(1000.0) == 5);
    }

    SECTION("Uniform heights") {
        HeightTree tree(10, 10.0);

        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(5.0) == 0);     // Within first paragraph
        REQUIRE(tree.findIndexForY(9.9) == 0);
        REQUIRE(tree.findIndexForY(10.0) == 1);    // Start of second
        REQUIRE(tree.findIndexForY(15.0) == 1);
        REQUIRE(tree.findIndexForY(50.0) == 5);
        REQUIRE(tree.findIndexForY(99.9) == 9);
    }

    SECTION("Varying heights") {
        HeightTree tree(5, 0.0);
        tree.setHeight(0, 10.0);   // Y: 0-10
        tree.setHeight(1, 20.0);   // Y: 10-30
        tree.setHeight(2, 30.0);   // Y: 30-60
        tree.setHeight(3, 40.0);   // Y: 60-100
        tree.setHeight(4, 50.0);   // Y: 100-150

        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(5.0) == 0);
        REQUIRE(tree.findIndexForY(10.0) == 1);    // Boundary
        REQUIRE(tree.findIndexForY(15.0) == 1);
        REQUIRE(tree.findIndexForY(30.0) == 2);    // Boundary
        REQUIRE(tree.findIndexForY(45.0) == 2);
        REQUIRE(tree.findIndexForY(60.0) == 3);    // Boundary
        REQUIRE(tree.findIndexForY(100.0) == 4);   // Boundary
        REQUIRE(tree.findIndexForY(125.0) == 4);
        REQUIRE(tree.findIndexForY(150.0) == 5);   // Beyond end
    }

    SECTION("Single element") {
        HeightTree tree(1, 100.0);
        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(50.0) == 0);
        REQUIRE(tree.findIndexForY(99.9) == 0);
        REQUIRE(tree.findIndexForY(100.0) == 1);
    }

    SECTION("Zero height elements") {
        HeightTree tree(5, 0.0);
        tree.setHeight(2, 100.0);  // Only middle has height

        REQUIRE(tree.findIndexForY(0.0) == 2);     // First non-zero is index 2
        REQUIRE(tree.findIndexForY(50.0) == 2);
        REQUIRE(tree.findIndexForY(100.0) == 5);   // Beyond
    }
}

// =============================================================================
// Insert/Remove Tests
// =============================================================================

TEST_CASE("HeightTree insert", "[editor][height_tree]") {
    SECTION("Insert at beginning") {
        HeightTree tree(3, 10.0);
        tree.insert(0, 50.0);

        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.height(0), WithinAbs(50.0, 0.001));
        REQUIRE_THAT(tree.height(1), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(80.0, 0.001));
    }

    SECTION("Insert in middle") {
        HeightTree tree(3, 10.0);
        tree.insert(1, 50.0);

        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.height(0), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.height(1), WithinAbs(50.0, 0.001));
        REQUIRE_THAT(tree.height(2), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.height(3), WithinAbs(10.0, 0.001));
    }

    SECTION("Insert at end") {
        HeightTree tree(3, 10.0);
        tree.insert(3, 50.0);

        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.height(3), WithinAbs(50.0, 0.001));
    }

    SECTION("Insert into empty") {
        HeightTree tree;
        tree.insert(0, 100.0);

        REQUIRE(tree.size() == 1);
        REQUIRE_THAT(tree.height(0), WithinAbs(100.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(100.0, 0.001));
    }

    SECTION("Insert preserves prefix sums") {
        HeightTree tree(3, 10.0);
        tree.insert(1, 50.0);

        REQUIRE_THAT(tree.prefixSum(0), WithinAbs(0.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(2), WithinAbs(60.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(4), WithinAbs(80.0, 0.001));
    }

    SECTION("Insert out of range throws") {
        HeightTree tree(3, 10.0);
        REQUIRE_THROWS_AS(tree.insert(5, 10.0), std::out_of_range);
    }
}

TEST_CASE("HeightTree remove", "[editor][height_tree]") {
    SECTION("Remove from beginning") {
        HeightTree tree(3, 10.0);
        tree.setHeight(0, 50.0);
        tree.remove(0);

        REQUIRE(tree.size() == 2);
        REQUIRE_THAT(tree.height(0), WithinAbs(10.0, 0.001));
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(20.0, 0.001));
    }

    SECTION("Remove from middle") {
        HeightTree tree(5, 10.0);
        tree.setHeight(2, 50.0);
        tree.remove(2);

        REQUIRE(tree.size() == 4);
        for (size_t i = 0; i < 4; ++i) {
            REQUIRE_THAT(tree.height(i), WithinAbs(10.0, 0.001));
        }
    }

    SECTION("Remove from end") {
        HeightTree tree(3, 10.0);
        tree.setHeight(2, 50.0);
        tree.remove(2);

        REQUIRE(tree.size() == 2);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(20.0, 0.001));
    }

    SECTION("Remove last element") {
        HeightTree tree(1, 100.0);
        tree.remove(0);

        REQUIRE(tree.empty());
        REQUIRE(tree.totalHeight() == 0.0);
    }

    SECTION("Remove preserves prefix sums") {
        HeightTree tree(5, 10.0);
        tree.setHeight(2, 50.0);
        tree.remove(1);  // Remove height[1], shift down

        REQUIRE(tree.size() == 4);
        REQUIRE_THAT(tree.prefixSum(0), WithinAbs(0.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(1), WithinAbs(10.0, 0.001));   // height[0]
        REQUIRE_THAT(tree.prefixSum(2), WithinAbs(60.0, 0.001));   // height[0] + height[2] (was 50)
    }

    SECTION("Remove out of range throws") {
        HeightTree tree(3, 10.0);
        REQUIRE_THROWS_AS(tree.remove(3), std::out_of_range);
        REQUIRE_THROWS_AS(tree.remove(100), std::out_of_range);
    }

    SECTION("Remove from empty throws") {
        HeightTree tree;
        REQUIRE_THROWS_AS(tree.remove(0), std::out_of_range);
    }
}

// =============================================================================
// Large Scale Tests
// =============================================================================

TEST_CASE("HeightTree large scale", "[editor][height_tree]") {
    SECTION("1000 elements with uniform heights") {
        HeightTree tree(1000, 24.0);

        REQUIRE(tree.size() == 1000);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(24000.0, 0.001));
        REQUIRE_THAT(tree.prefixSum(500), WithinAbs(12000.0, 0.001));

        // Find paragraph at various positions
        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(12000.0) == 500);
        REQUIRE(tree.findIndexForY(23999.9) == 999);
    }

    SECTION("Multiple updates") {
        HeightTree tree(100, 20.0);

        // Update every 10th element
        for (size_t i = 0; i < 100; i += 10) {
            tree.setHeight(i, 50.0);
        }

        // Total = 10 * 50 + 90 * 20 = 500 + 1800 = 2300
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(2300.0, 0.001));

        // Verify findIndexForY still works correctly
        REQUIRE(tree.findIndexForY(0.0) == 0);
        REQUIRE(tree.findIndexForY(50.0) == 1);  // After first element (50px)
    }

    SECTION("Sequential operations maintain consistency") {
        HeightTree tree(50, 10.0);

        // Insert 10 elements
        for (int i = 0; i < 10; ++i) {
            tree.insert(static_cast<size_t>(i * 2), 25.0);
        }

        REQUIRE(tree.size() == 60);

        // Remove 5 elements
        for (int i = 0; i < 5; ++i) {
            tree.remove(0);
        }

        REQUIRE(tree.size() == 55);

        // Verify consistency
        double sum = 0.0;
        for (size_t i = 0; i < tree.size(); ++i) {
            sum += tree.height(i);
        }
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(sum, 0.001));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("HeightTree edge cases", "[editor][height_tree]") {
    SECTION("Very small heights") {
        HeightTree tree(100, 0.001);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(0.1, 0.0001));
        // y=0.05 is exactly at boundary of paragraph 49/50 (prefixSum(50) = 0.05)
        // Allow for floating-point tolerance at exact boundaries
        auto result = tree.findIndexForY(0.05);
        REQUIRE((result == 49 || result == 50));  // Either is acceptable at boundary
    }

    SECTION("Very large heights") {
        HeightTree tree(10, 1e6);
        REQUIRE_THAT(tree.totalHeight(), WithinAbs(1e7, 1.0));
        REQUIRE(tree.findIndexForY(5e6) == 5);
    }

    SECTION("Mixed zero and non-zero heights") {
        HeightTree tree(10, 0.0);
        tree.setHeight(3, 100.0);
        tree.setHeight(7, 200.0);

        REQUIRE_THAT(tree.totalHeight(), WithinAbs(300.0, 0.001));
        REQUIRE(tree.findIndexForY(0.0) == 3);    // First non-zero
        REQUIRE(tree.findIndexForY(50.0) == 3);
        REQUIRE(tree.findIndexForY(100.0) == 7);  // Second non-zero
        REQUIRE(tree.findIndexForY(150.0) == 7);
    }

    SECTION("Power of 2 sizes") {
        for (size_t size : {1, 2, 4, 8, 16, 32, 64, 128, 256}) {
            HeightTree tree(size, 10.0);
            REQUIRE(tree.size() == size);
            REQUIRE_THAT(tree.totalHeight(), WithinAbs(size * 10.0, 0.001));
            REQUIRE(tree.findIndexForY(size * 5.0) == size / 2);
        }
    }

    SECTION("Non-power of 2 sizes") {
        for (size_t size : {3, 5, 7, 13, 17, 31, 63, 127, 255}) {
            HeightTree tree(size, 10.0);
            REQUIRE(tree.size() == size);
            REQUIRE_THAT(tree.totalHeight(), WithinAbs(size * 10.0, 0.001));
        }
    }
}
