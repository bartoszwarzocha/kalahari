// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_avatar.cpp
/// @brief Portraits put together from parts: the parts, the drawings of their values, the
/// palettes, the drawing by chance and the description of the look

#include "kalahari_tests/changed_setting.h"
#include "kalahari_tests/polish.h"

#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSize>

#include <nlohmann/json_fwd.hpp>

using kalahari::gui::Avatar;
using kalahari::gui::AvatarArt;
using kalahari::gui::AvatarFrame;
using kalahari::gui::AvatarLook;
using kalahari::gui::AvatarPalettes;

namespace {

/// The values of the parts of @p avatar that the program draws, without the colors
QJsonObject partsOf(const QJsonObject& avatar) {
    QJsonObject parts;
    for (const Avatar::Part& each : Avatar::parts()) {
        parts.insert(each.key, avatar.value(each.key));
    }
    return parts;
}

}  // namespace

TEST_CASE("Avatar: the parts, the colors and the groups go together", "[gui][avatar]") {
    QStringList grouped;
    QStringList groupedColors;
    for (const Avatar::Group& group : Avatar::groups()) {
        grouped << group.parts;
        groupedColors << group.colors;
        CHECK(Avatar::groupName(group.id) != group.id);
    }
    QStringList keys;
    for (const Avatar::Part& each : Avatar::parts()) {
        keys << each.key;
        CHECK(Avatar::part(each.key) == &each);
        CHECK(Avatar::partName(each.key) != each.key);
        CHECK((each.many || each.values.contains(each.fallback)));
        for (const QString& value : each.values) {
            CHECK(Avatar::valueName(each.key, value) != QString());
        }
    }
    CHECK(grouped == keys);
    QStringList colorKeys;
    for (const Avatar::Color& each : Avatar::colors()) {
        colorKeys << each.key;
        CHECK(Avatar::color(each.key) == &each);
        CHECK(Avatar::colorName(each.key) != each.key);
        CHECK(AvatarPalettes::names().contains(each.palette));
    }
    CHECK(groupedColors == colorKeys);
    CHECK(Avatar::part(QStringLiteral("tail")) == nullptr);
    CHECK(Avatar::color(QStringLiteral("tail")) == nullptr);
    CHECK(Avatar::valueName(QStringLiteral("hair"), QStringLiteral("fringe")) ==
          QStringLiteral("with a fringe"));
    CHECK(Avatar::signOf(QStringLiteral("woman")) == QStringLiteral("♀"));
    CHECK(Avatar::signOf(QStringLiteral("man")) == QStringLiteral("♂"));
    CHECK(Avatar::signOf(QStringLiteral("other")) == QStringLiteral("⚲"));
}

TEST_CASE("Avatar: a new portrait, the one the card suggests and one with unknown values",
          "[gui][avatar]") {
    const QJsonObject defaults = Avatar::defaults();
    CHECK(defaults.value(QStringLiteral("mouth")).toString() == QStringLiteral("neutral"));
    CHECK(defaults.value(QStringLiteral("extras")).toArray().isEmpty());
    CHECK(defaults.value(QLatin1String(Avatar::SIGN)).toBool() == false);
    CHECK(Avatar::resolved(defaults) == defaults);

    const QJsonObject woman = Avatar::proposal(QStringLiteral("woman"), QStringLiteral(" 67 "));
    CHECK(woman.value(QStringLiteral("hair")).toString() == QStringLiteral("shoulder"));
    CHECK(woman.value(QStringLiteral("lashes")).toString() == QStringLiteral("long"));
    CHECK(woman.value(QStringLiteral("age")).toString() == QStringLiteral("old"));
    const QJsonObject man = Avatar::proposal(QStringLiteral("man"), QStringLiteral("17"));
    CHECK(man.value(QStringLiteral("hair")).toString() == QStringLiteral("short"));
    CHECK(man.value(QStringLiteral("age")).toString() == QStringLiteral("young"));
    CHECK(Avatar::proposal(QString(), QStringLiteral("45")).value(QStringLiteral("age")) ==
          QStringLiteral("middle"));
    CHECK(Avatar::proposal(QString(), QStringLiteral("about thirty"))
              .value(QStringLiteral("age")) == QStringLiteral("adult"));

    QJsonObject later = defaults;
    later.insert(QStringLiteral("hair"), QStringLiteral("mohawk"));
    later.insert(QStringLiteral("extras"),
                 QJsonArray{QStringLiteral("scar"), QStringLiteral("wings"),
                            QStringLiteral("scar")});
    later.insert(QStringLiteral("skin"), QStringLiteral("not a color"));
    later.insert(QStringLiteral("hairColor"), QStringLiteral("#ABCDEF"));
    later.insert(QStringLiteral("tail"), QStringLiteral("long"));
    later.remove(QStringLiteral("nose"));
    const QJsonObject drawn = Avatar::resolved(later);
    CHECK(drawn.value(QStringLiteral("hair")).toString() == QStringLiteral("short"));
    CHECK(drawn.value(QStringLiteral("extras")).toArray() == QJsonArray{QStringLiteral("scar")});
    CHECK(drawn.value(QStringLiteral("skin")) == defaults.value(QStringLiteral("skin")));
    CHECK(drawn.value(QStringLiteral("hairColor")).toString() == QStringLiteral("#abcdef"));
    CHECK(drawn.value(QStringLiteral("nose")).toString() == QStringLiteral("straight"));
    CHECK(drawn.value(QStringLiteral("tail")).toString() == QStringLiteral("long"));
}

TEST_CASE("Avatar: a portrait drawn by chance suits the gender and keeps a calm face",
          "[gui][avatar]") {
    const AvatarPalettes palettes = AvatarPalettes::defaults();
    QJsonObject given = Avatar::defaults();
    given.insert(QStringLiteral("background"), QStringLiteral("gradient"));
    given.insert(QLatin1String(Avatar::SIGN), true);
    given.insert(QStringLiteral("age"), QStringLiteral("old"));
    QRandomGenerator random(7);
    QStringList beards;
    QStringList clothes;
    for (int round = 0; round < 60; ++round) {
        const QString gender = round % 2 == 0 ? QStringLiteral("woman") : QStringLiteral("man");
        const QJsonObject drawn = Avatar::randomized(given, gender, palettes, random);
        INFO("round " << round);
        CHECK(Avatar::resolved(drawn) == drawn);
        CHECK(QStringList{QStringLiteral("neutral"), QStringLiteral("serious")}.contains(
            drawn.value(QStringLiteral("mouth")).toString()));
        CHECK(drawn.value(QStringLiteral("background")).toString() ==
              QStringLiteral("gradient"));
        CHECK(drawn.value(QLatin1String(Avatar::SIGN)).toBool());
        CHECK(drawn.value(QStringLiteral("age")).toString() == QStringLiteral("old"));
        for (const Avatar::Color& color : Avatar::colors()) {
            CHECK(palettes.colors(color.palette).contains(drawn.value(color.key).toString()));
        }
        const QString beard = drawn.value(QStringLiteral("beard")).toString();
        const QString garment = drawn.value(QStringLiteral("clothes")).toString();
        if (gender == QLatin1String("woman")) {
            CHECK(beard == QStringLiteral("none"));
            CHECK(drawn.value(QStringLiteral("hair")).toString() != QStringLiteral("bald"));
            clothes << garment;
        } else {
            CHECK(garment != QStringLiteral("dress"));
            CHECK(drawn.value(QStringLiteral("lashes")).toString() == QStringLiteral("plain"));
            CHECK(drawn.value(QStringLiteral("bottom")).toString() == QStringLiteral("trousers"));
            beards << beard;
        }
    }
    beards.removeDuplicates();
    clothes.removeDuplicates();
    CHECK(beards.size() > 2);
    CHECK(clothes.size() > 2);

    QRandomGenerator again(7);
    QRandomGenerator same(7);
    CHECK(Avatar::randomized(given, QString(), palettes, again) ==
          Avatar::randomized(given, QString(), palettes, same));
}

TEST_CASE("Avatar: the look in words", "[gui][avatar]") {
    QJsonObject avatar = Avatar::defaults();
    CHECK(Avatar::described(avatar, false) ==
          QStringLiteral("Oval face, fair skin, round brown eyes, straight nose, short brown "
                         "hair. Clothes: grey shirt."));
    CHECK(Avatar::described(avatar, true) ==
          QStringLiteral("Oval face, fair skin, round brown eyes, straight nose, short brown "
                         "hair. Clothes: grey shirt, navy trousers. Of medium height, average "
                         "build."));

    avatar.insert(QStringLiteral("hair"), QStringLiteral("bald"));
    avatar.insert(QStringLiteral("beard"), QStringLiteral("full"));
    avatar.insert(QStringLiteral("glasses"), QStringLiteral("monocle"));
    avatar.insert(QStringLiteral("extras"),
                  QJsonArray{QStringLiteral("scar"), QStringLiteral("wings")});
    avatar.insert(QStringLiteral("eyeColor"), QStringLiteral("#123456"));
    avatar.insert(QStringLiteral("clothes"), QStringLiteral("robe"));
    avatar.insert(QStringLiteral("age"), QStringLiteral("old"));
    avatar.insert(QStringLiteral("height"), QStringLiteral("tall"));
    CHECK(Avatar::described(avatar, true) ==
          QStringLiteral("Oval face, fair skin, round eyes, straight nose, bald head, beard and "
                         "moustache, a monocle, a scar on the cheek, deep wrinkles. Clothes: "
                         "grey robe. Tall, average build."));

    avatar.insert(QStringLiteral("clothes"), QStringLiteral("armor"));
    avatar.insert(QStringLiteral("bottom"), QStringLiteral("skirt"));
    avatar.insert(QStringLiteral("bottomColor"), QStringLiteral("#6e2237"));
    CHECK(Avatar::described(avatar, true).contains(QStringLiteral("Clothes: armor, ")));
}

TEST_CASE("Avatar: the look in words in Polish", "[gui][avatar]") {
    const kalahari::test::Polish polish;
    REQUIRE(polish.loaded());
    QJsonObject avatar = Avatar::defaults();
    avatar.insert(QStringLiteral("clothes"), QStringLiteral("dress"));
    avatar.insert(QStringLiteral("clothesColor"), QStringLiteral("#6e2237"));
    avatar.insert(QStringLiteral("lashes"), QStringLiteral("long"));
    CHECK(Avatar::described(avatar, false) ==
          QStringLiteral("Owalna twarz, jasna cera, okrągłe brązowe oczy, długie rzęsy, prosty "
                         "nos, krótkie brązowe włosy. Strój: bordowa sukienka."));
    avatar.insert(QStringLiteral("clothes"), QStringLiteral("sweater"));
    avatar.insert(QStringLiteral("bottom"), QStringLiteral("skirt"));
    avatar.insert(QStringLiteral("bottomColor"), QStringLiteral("#2c3e5c"));
    CHECK(Avatar::described(avatar, true)
              .endsWith(QStringLiteral("Strój: bordowy sweter, granatowa spódnica. Średniego "
                                       "wzrostu, przeciętna budowa.")));
    avatar.insert(QStringLiteral("clothes"), QStringLiteral("shirt"));
    avatar.insert(QStringLiteral("bottom"), QStringLiteral("trousers"));
    CHECK(Avatar::described(avatar, true)
              .contains(QStringLiteral("Strój: bordowa koszula, granatowe spodnie.")));
    CHECK(Avatar::partName(QStringLiteral("hair")) == QStringLiteral("Fryzura"));
}

TEST_CASE("Avatar: the palettes have names for their colors and keep what the writer sets",
          "[gui][avatar]") {
    const AvatarPalettes defaults = AvatarPalettes::defaults();
    for (const QString& name : AvatarPalettes::names()) {
        INFO(name.toStdString());
        CHECK(AvatarPalettes::title(name) != name);
        CHECK_FALSE(defaults.colors(name).isEmpty());
        for (const QString& color : defaults.colors(name)) {
            INFO(color.toStdString());
            CHECK_FALSE(AvatarPalettes::colorName(name, color).isEmpty());
            if (name == QLatin1String("clothes")) {
                CHECK_FALSE(AvatarPalettes::colorName(name, color, QStringLiteral("f")).isEmpty());
                CHECK_FALSE(
                    AvatarPalettes::colorName(name, color, QStringLiteral("pl")).isEmpty());
            }
        }
    }
    for (const Avatar::Color& each : Avatar::colors()) {
        CHECK(defaults.colors(each.palette).contains(Avatar::fallbackOf(each)));
    }
    CHECK(AvatarPalettes::colorName(QStringLiteral("hair"), QStringLiteral("#123456")).isEmpty());

    const kalahari::test::KeptSetting<nlohmann::json> kept(AvatarPalettes::SETTING);
    kalahari::core::SettingsManager::getInstance().set<nlohmann::json>(
        AvatarPalettes::SETTING, nlohmann::json{{"hair", {"#ff0000", "not a color"}},
                                                {"eyes", nlohmann::json::array()}});
    AvatarPalettes current = AvatarPalettes::current();
    CHECK(current.colors(QStringLiteral("hair")) == QStringList{QStringLiteral("#ff0000")});
    CHECK(current.colors(QStringLiteral("eyes")) == defaults.colors(QStringLiteral("eyes")));
    CHECK(current.colors(QStringLiteral("skin")) == defaults.colors(QStringLiteral("skin")));

    current.setColors(QStringLiteral("clothes"), {QStringLiteral("#00ff00")});
    current.save();
    const AvatarPalettes saved = AvatarPalettes::current();
    CHECK(saved == current);
    CHECK_FALSE(saved == defaults);
}

namespace {

/// Whether value @p value of part @p key is drawn by the layers of the other values ("none"),
/// or by the fit of the layers (the build, the height), and so has no layer of its own
bool drawnByOthers(const QString& key, const QString& value) {
    static const QStringList fitted = {QStringLiteral("build"), QStringLiteral("height"),
                                       QStringLiteral("age")};
    static const QStringList others = {QStringLiteral("lashes=plain"),
                                       QStringLiteral("bottom=trousers")};
    return value == QLatin1String("none") || fitted.contains(key) ||
           others.contains(key + QLatin1Char('=') + value);
}

}  // namespace

TEST_CASE("Avatar: the drawings of the program have every value of the parts",
          "[gui][avatar]") {
    const AvatarArt& art = AvatarArt::drawn();
    REQUIRE_FALSE(art.isEmpty());
    const auto drawnValues = art.drawnValues();
    for (const Avatar::Part& each : Avatar::parts()) {
        for (const QString& value : each.values) {
            if (drawnByOthers(each.key, value)) {
                continue;
            }
            INFO(each.key.toStdString() << " = " << value.toStdString());
            CHECK(drawnValues.value(each.key).contains(value));
        }
    }
    for (const AvatarFrame frame : {AvatarFrame::Bust, AvatarFrame::Full, AvatarFrame::Face}) {
        CHECK(art.view(frame).isValid());
    }
    CHECK(art.view(AvatarFrame::Full).height() > art.view(AvatarFrame::Bust).height());
}

TEST_CASE("Avatar: a portrait drawn as an image, in both styles and every frame",
          "[gui][avatar]") {
    const AvatarArt& art = AvatarArt::drawn();
    QJsonObject avatar = Avatar::defaults();
    avatar.insert(QStringLiteral("background"), QStringLiteral("none"));
    AvatarLook look;
    look.card = QColor(QStringLiteral("#d9e4f0"));
    const QSize pixels(120, 120);

    const QImage flat = art.image(avatar, look, pixels);
    REQUIRE(flat.size() == pixels);
    CHECK(flat.pixelColor(2, 2).alpha() == 0);
    CHECK(flat.pixelColor(60, 60).alpha() == 255);

    look.style = QString::fromLatin1(AvatarLook::LINE);
    const QImage line = art.image(avatar, look, pixels);
    CHECK(line.pixelColor(60, 60).alpha() == 255);
    CHECK(line != flat);
    CHECK(art.svg(avatar, look).contains(QStringLiteral("stroke=\"#2b2b2b\"")));

    look.style = QString::fromLatin1(AvatarLook::FLAT);
    avatar.insert(QStringLiteral("background"), QStringLiteral("card"));
    look.sign = Avatar::signOf(QStringLiteral("woman"));
    const QString svg = art.svg(avatar, look);
    AvatarLook otherCard = look;
    otherCard.card = QColor(QStringLiteral("#f0d9d9"));
    CHECK(art.svg(avatar, otherCard) != svg);
    CHECK(svg.contains(QStringLiteral("♀")));
    CHECK(art.image(avatar, look, pixels).pixelColor(2, 2).alpha() == 255);

    look.sign.clear();
    look.frame = AvatarFrame::Full;
    QJsonObject tall = avatar;
    tall.insert(QStringLiteral("height"), QStringLiteral("tall"));
    CHECK(art.svg(tall, look) != art.svg(avatar, look));
    look.frame = AvatarFrame::Face;
    CHECK_FALSE(art.image(avatar, look, pixels).isNull());
    CHECK(partsOf(avatar).size() == Avatar::parts().size());
}

TEST_CASE("Avatar: a set of drawings with mistakes says what is wrong", "[gui][avatar]") {
    QStringList problems;
    const AvatarArt empty = AvatarArt::fromJson(QJsonObject(), problems);
    CHECK(empty.isEmpty());
    CHECK_FALSE(problems.isEmpty());

    problems.clear();
    const QJsonObject json = {
        {QStringLiteral("frames"),
         QJsonObject{{QStringLiteral("bust"), QJsonArray{0, 0, 200, 200}},
                     {QStringLiteral("full"), QJsonArray{0, 0, 200, 400}},
                     {QStringLiteral("face"), QJsonArray{50, 50, 100, 100}}}},
        {QStringLiteral("layers"),
         QJsonArray{QJsonObject{{QStringLiteral("draw"),
                                 QJsonArray{QJsonObject{{QStringLiteral("circle"),
                                                         QJsonArray{100, 100, 40}},
                                                        {QStringLiteral("fill"),
                                                         QStringLiteral("{skin}")}}}}},
                    QStringLiteral("not a layer")}},
    };
    const AvatarArt art = AvatarArt::fromJson(json, problems);
    CHECK_FALSE(art.isEmpty());
    CHECK(problems.size() == 1);
    CHECK(art.svg(Avatar::defaults(), AvatarLook()).contains(QStringLiteral("#eec6a6")));

    problems.clear();
    CHECK(AvatarArt::fromFile(QStringLiteral("/no/such/file.json"), problems).isEmpty());
    CHECK(problems.size() == 1);
}
