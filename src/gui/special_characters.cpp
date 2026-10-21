/// @file special_characters.cpp
/// @brief The characters of Insert > Special Character...: their groups, their names in the
///        program's language and in English, and finding them by a name or a code

#include "kalahari/gui/special_characters.h"

#include <QChar>
#include <QStringList>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief The context of the names in the translations
constexpr const char* CONTEXT = "kalahari::gui::SpecialCharacters";

/// @brief A character with a name of its own
struct Named {
    char32_t code;
    const char* name;           ///< In English, translated in CONTEXT
    const char* words;          ///< Other words to find it by, in English; none: nullptr
    const char* mark = nullptr; ///< What the window shows for a character that cannot be seen
};

/// @brief A mark of a letter, as a decomposed letter has it after the letter
struct Mark {
    char32_t code;
    const char* name;  ///< "%1 with ogonek": %1 is the letter
};

/// @brief A letter of the Greek alphabet
struct Greek {
    char32_t code;  ///< The small letter
    const char* name;
};

// The names are those of Unicode, shorter where they are long, in sentence case

const Named PUNCTUATION[] = {
    {0x2014,  // —
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Em dash"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "dash dialogue")},
    {0x2013,  // –
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "En dash"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "dash range")},
    {0x2026,  // …
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ellipsis"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "dots")},
    {0x201E,  // „
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Low double opening quotation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote")},
    {0x201D,  // ”
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Double closing quotation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote")},
    {0x201A,  // ‚
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Low single opening quotation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote")},
    {0x2019,  // ’
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Apostrophe"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "single closing quotation mark")},
    {0x201C,  // “
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Double opening quotation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote English")},
    {0x2018,  // ‘
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Single opening quotation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote English")},
    {0x00AB,  // «
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Opening guillemet"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote French")},
    {0x00BB,  // »
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Closing guillemet"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote French")},
    {0x2039,  // ‹
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Single opening guillemet"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote French")},
    {0x203A,  // ›
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Single closing guillemet"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "quote French")},
    {0x00B7,  // ·
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Middle dot"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "point")},
    {0x2022,  // •
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Bullet"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "point list")},
    {0x00A7,  // §
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Section sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "paragraph law")},
    {0x00B6,  // ¶
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Pilcrow"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "paragraph mark")},
    {0x2020,  // †
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Dagger"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "cross obelisk died")},
    {0x2021,  // ‡
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Double dagger"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "cross")},
    {0x00B0,  // °
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Degree sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "temperature angle")},
    {0x2032,  // ′
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Prime"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "minute foot")},
    {0x2033,  // ″
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Double prime"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "second inch")},
    {0x2030,  // ‰
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Per mille sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "thousand")},
    {0x2116,  // №
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Numero sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "number")},
    {0x00A9,  // ©
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Copyright sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "rights")},
    {0x00AE,  // ®
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Registered sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "registered trademark")},
    {0x2122,  // ™
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Trade mark sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "trademark")},
    {0x00A1,  // ¡
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Inverted exclamation mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Spanish")},
    {0x00BF,  // ¿
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Inverted question mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Spanish question")},
    {0x203D,  // ‽
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Interrobang"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "question exclamation")},
    {0x2042,  // ⁂
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Asterism"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "three asterisks scene break")},
    {0x203B,  // ※
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Reference mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "note")},
};

const Named SPACES[] = {
    {0x00A0,  // U+00A0
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "No-break space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "hard nbsp"),
         "°"},
    {0x202F,  // U+202F
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Narrow no-break space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "thin hard"),
         "NNBSP"},
    {0x2009,  // U+2009
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Thin space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "narrow"),
         "THSP"},
    {0x2002,  // U+2002
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "En space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "wide"),
         "ENSP"},
    {0x2003,  // U+2003
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Em space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "wide"),
         "EMSP"},
    {0x2011,  // ‑
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Non-breaking hyphen"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "hard dash")},
    {0x00AD,  // U+00AD
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Optional hyphen"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "soft shy hyphenation"),
         "¬"},
    {0x2010,  // ‐
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Hyphen"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "dash")},
    {0x200B,  // U+200B
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Zero width space"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "invisible break"),
         "ZWSP"},
};

const Named ARROWS[] = {
    {0x2190,  // ←
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Leftwards arrow"),
         nullptr},
    {0x2192,  // →
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Rightwards arrow"),
         nullptr},
    {0x2191,  // ↑
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Upwards arrow"),
         nullptr},
    {0x2193,  // ↓
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Downwards arrow"),
         nullptr},
    {0x2194,  // ↔
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Left right arrow"),
         nullptr},
    {0x2195,  // ↕
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Up down arrow"),
         nullptr},
    {0x21D0,  // ⇐
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Leftwards double arrow"),
         nullptr},
    {0x21D2,  // ⇒
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Rightwards double arrow"),
         nullptr},
    {0x21D4,  // ⇔
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Left right double arrow"),
         nullptr},
    {0x2196,  // ↖
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "North west arrow"),
         nullptr},
    {0x2197,  // ↗
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "North east arrow"),
         nullptr},
    {0x2198,  // ↘
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "South east arrow"),
         nullptr},
    {0x2199,  // ↙
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "South west arrow"),
         nullptr},
    {0x21A9,  // ↩
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Leftwards arrow with hook"),
         nullptr},
    {0x2794,  // ➔
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Heavy rightwards arrow"),
         nullptr},
};

const Named MATHEMATICS[] = {
    {0x00B1,  // ±
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Plus-minus sign"),
         nullptr},
    {0x00D7,  // ×
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Multiplication sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "times")},
    {0x00F7,  // ÷
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Division sign"),
         nullptr},
    {0x2212,  // −
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Minus sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "subtraction")},
    {0x2248,  // ≈
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Almost equal to"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "about")},
    {0x2260,  // ≠
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Not equal to"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "unequal")},
    {0x2264,  // ≤
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Less-than or equal to"),
         nullptr},
    {0x2265,  // ≥
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Greater-than or equal to"),
         nullptr},
    {0x221E,  // ∞
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Infinity"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "endless")},
    {0x221A,  // √
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Square root"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "root")},
    {0x2211,  // ∑
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Summation"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "sigma")},
    {0x220F,  // ∏
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Product"),
         nullptr},
    {0x222B,  // ∫
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Integral"),
         nullptr},
    {0x2202,  // ∂
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Partial differential"),
         nullptr},
    {0x2206,  // ∆
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Increment"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "delta")},
    {0x2207,  // ∇
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Nabla"),
         nullptr},
    {0x2208,  // ∈
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Element of"),
         nullptr},
    {0x2209,  // ∉
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Not an element of"),
         nullptr},
    {0x2229,  // ∩
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Intersection"),
         nullptr},
    {0x222A,  // ∪
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Union"),
         nullptr},
    {0x2282,  // ⊂
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Subset of"),
         nullptr},
    {0x2200,  // ∀
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "For all"),
         nullptr},
    {0x2203,  // ∃
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "There exists"),
         nullptr},
    {0x00AC,  // ¬
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Not sign"),
         nullptr},
    {0x2220,  // ∠
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Angle"),
         nullptr},
    {0x22A5,  // ⊥
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Perpendicular"),
         nullptr},
    {0x2225,  // ∥
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Parallel to"),
         nullptr},
};

const Named CURRENCIES[] = {
    {0x20AC,  // €
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Euro sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x0024,  // $
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Dollar sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x00A3,  // £
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Pound sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x00A5,  // ¥
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Yen sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x00A2,  // ¢
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Cent sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20BD,  // ₽
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ruble sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20B4,  // ₴
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Hryvnia sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20B9,  // ₹
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Indian rupee sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20A9,  // ₩
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Won sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20AA,  // ₪
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "New shekel sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20BA,  // ₺
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Turkish lira sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20A3,  // ₣
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "French franc sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x20BF,  // ₿
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Bitcoin sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
    {0x00A4,  // ¤
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Currency sign"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "currency money")},
};

const Named FRACTIONS[] = {
    {0x00BD,  // ½
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "One half"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction half")},
    {0x2153,  // ⅓
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "One third"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x2154,  // ⅔
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Two thirds"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x00BC,  // ¼
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "One quarter"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction quarter")},
    {0x00BE,  // ¾
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Three quarters"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x2155,  // ⅕
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "One fifth"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x215B,  // ⅛
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "One eighth"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x215C,  // ⅜
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Three eighths"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x215D,  // ⅝
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Five eighths"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
    {0x215E,  // ⅞
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Seven eighths"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fraction")},
};

const Named ORNAMENTS[] = {
    {0x2766,  // ❦
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Floral heart"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fleuron scene break")},
    {0x2767,  // ❧
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Rotated floral heart"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fleuron")},
    {0x2619,  // ☙
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Reversed rotated floral heart"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "fleuron")},
    {0x2756,  // ❖
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Diamond of four diamonds"),
         nullptr},
    {0x2731,  // ✱
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Heavy asterisk"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "star")},
    {0x2605,  // ★
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Black star"),
         nullptr},
    {0x2606,  // ☆
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "White star"),
         nullptr},
    {0x2660,  // ♠
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Spade suit"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "card")},
    {0x2663,  // ♣
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Club suit"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "card")},
    {0x2665,  // ♥
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Heart suit"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "card heart")},
    {0x2666,  // ♦
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Diamond suit"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "card")},
    {0x266A,  // ♪
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Eighth note"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "music")},
    {0x266B,  // ♫
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Beamed eighth notes"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "music")},
    {0x2713,  // ✓
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Check mark"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "tick")},
    {0x2717,  // ✗
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ballot X"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "cross x")},
    {0x2610,  // ☐
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ballot box"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "check box")},
    {0x2611,  // ☑
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ballot box with check"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "check box")},
    {0x271D,  // ✝
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Latin cross"),
         nullptr},
    {0x263C,  // ☼
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Sun"),
         nullptr},
    {0x263E,  // ☾
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Moon"),
         nullptr},
    {0x2709,  // ✉
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Envelope"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "letter mail")},
    {0x260E,  // ☎
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Telephone"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "phone")},
    {0x2702,  // ✂
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Scissors"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "cut")},
    {0x270D,  // ✍
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Writing hand"),
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "write")},
};

const Named OWN_LETTERS[] = {
    {0x00E6,  // æ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ligature ae"),
         nullptr},
    {0x00C6,  // Æ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ligature AE"),
         nullptr},
    {0x0153,  // œ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ligature oe"),
         nullptr},
    {0x0152,  // Œ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Ligature OE"),
         nullptr},
    {0x00F8,  // ø
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "o with stroke"),
         nullptr},
    {0x00D8,  // Ø
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "O with stroke"),
         nullptr},
    {0x0111,  // đ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "d with stroke"),
         nullptr},
    {0x0110,  // Đ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "D with stroke"),
         nullptr},
    {0x0142,  // ł
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "l with stroke"),
         nullptr},
    {0x0141,  // Ł
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "L with stroke"),
         nullptr},
    {0x00DF,  // ß
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Sharp s (eszett)"),
         nullptr},
    {0x00FE,  // þ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Thorn"),
         nullptr},
    {0x00DE,  // Þ
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Thorn (capital letter)"),
         nullptr},
    {0x00F0,  // ð
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Eth"),
         nullptr},
    {0x00D0,  // Ð
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Eth (capital letter)"),
         nullptr},
    {0x0131,  // ı
         QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "Dotless i"),
         nullptr},
};

const Mark MARKS[] = {
    {0x0301, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with acute")},
    {0x0300, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with grave")},
    {0x0302, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with circumflex")},
    {0x0308, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with diaeresis")},
    {0x0303, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with tilde")},
    {0x030A, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with ring above")},
    {0x0304, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with macron")},
    {0x0306, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with breve")},
    {0x0328, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with ogonek")},
    {0x0327, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with cedilla")},
    {0x030C, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with caron")},
    {0x0307, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with dot above")},
    {0x030B, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with double acute")},
    {0x0326, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "%1 with comma below")},
};

const Greek GREEK[] = {
    {0x03B1, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "alpha")},  // α
    {0x03B2, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "beta")},  // β
    {0x03B3, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "gamma")},  // γ
    {0x03B4, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "delta")},  // δ
    {0x03B5, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "epsilon")},  // ε
    {0x03B6, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "zeta")},  // ζ
    {0x03B7, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "eta")},  // η
    {0x03B8, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "theta")},  // θ
    {0x03B9, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "iota")},  // ι
    {0x03BA, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "kappa")},  // κ
    {0x03BB, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "lambda")},  // λ
    {0x03BC, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "mu")},  // μ
    {0x03BD, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "nu")},  // ν
    {0x03BE, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "xi")},  // ξ
    {0x03BF, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "omicron")},  // ο
    {0x03C0, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "pi")},  // π
    {0x03C1, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "rho")},  // ρ
    {0x03C3, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "sigma")},  // σ
    {0x03C4, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "tau")},  // τ
    {0x03C5, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "upsilon")},  // υ
    {0x03C6, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "phi")},  // φ
    {0x03C7, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "chi")},  // χ
    {0x03C8, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "psi")},  // ψ
    {0x03C9, QT_TRANSLATE_NOOP("kalahari::gui::SpecialCharacters", "omega")},  // ω
};

/// @brief The letters with accents of the European languages written in the Latin alphabet;
///        the window has their capitals after them
constexpr char32_t ACCENTED_LETTERS[] = {
    U'á', U'à', U'â', U'ä', U'ã', U'å', U'ā', U'ă', U'ą', U'æ', U'ç', U'ć', U'č', U'ď',
    U'đ', U'é', U'è', U'ê', U'ë', U'ě', U'ę', U'ė', U'ē', U'ğ', U'í', U'ì', U'î', U'ï',
    U'ī', U'ı', U'ł', U'ľ', U'ĺ', U'ń', U'ň', U'ñ', U'ó', U'ò', U'ô', U'ö', U'õ', U'ő',
    U'ø', U'œ', U'ŕ', U'ř', U'ś', U'š', U'ş', U'ș', U'ß', U'ť', U'ţ', U'ț', U'ú', U'ù',
    U'û', U'ü', U'ů', U'ű', U'ū', U'ý', U'ÿ', U'ź', U'ż', U'ž', U'þ', U'ð'};

/// @brief The superscript digits ⁰ to ⁹ and the subscript ones ₀ to ₉
constexpr char32_t SUPERSCRIPTS[] = {U'⁰', U'¹', U'²', U'³', U'⁴', U'⁵', U'⁶', U'⁷', U'⁸', U'⁹'};
constexpr char32_t SUBSCRIPTS[] = {U'₀', U'₁', U'₂', U'₃', U'₄', U'₅', U'₆', U'₇', U'₈', U'₉'};

/// @brief A name in the program's language
QString translated(const char* text) {
    return text != nullptr ? QCoreApplication::translate(CONTEXT, text) : QString();
}

/// @brief A text without its case and its accents, for finding: "Półpauza" is "polpauza"
QString folded(const QString& text) {
    const QString decomposed = text.toLower().normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(decomposed.size());
    for (QChar ch : decomposed) {
        if (ch.isMark()) {
            continue;
        }
        if (ch == u'ł') {
            ch = u'l';  // a letter of its own in Unicode, not l with a mark
        }
        result += ch;
    }
    return result;
}

/// @brief The text with its first letter a capital
QString withCapital(const QString& text) {
    return text.isEmpty() ? text : text.at(0).toUpper() + text.mid(1);
}

}  // namespace

SpecialCharacters::SpecialCharacters() {
    const auto addGroup = [this](const char* id, const QString& name) {
        m_groups.append({QString::fromLatin1(id), name, {}});
    };
    const auto addNamed = [this](const Named& named) {
        const QString englishWords = QString::fromUtf8(named.words);
        const QString words = translated(named.words);
        add({named.code, translated(named.name), QString::fromUtf8(named.name),
             words == englishWords ? words : words + QLatin1Char(' ') + englishWords,
             QString::fromUtf8(named.mark), QString()});
    };

    addGroup("punctuation", tr("Punctuation and Typography"));
    for (const Named& named : PUNCTUATION) {
        addNamed(named);
    }
    addGroup("spaces", tr("Spaces and Hyphens"));
    for (const Named& named : SPACES) {
        addNamed(named);
    }

    // The letters with accents: named from the letter and its mark, unless they have a name
    // of their own (ł, ø, æ: Unicode does not decompose them)
    addGroup("accents", tr("Accented Letters"));
    const QString letterWords = tr("letter accent") + QStringLiteral(" letter accent");
    const auto addLetter = [this, &letterWords](char32_t code) {
        for (const Named& own : OWN_LETTERS) {
            if (own.code == code) {
                add({code, translated(own.name), QString::fromUtf8(own.name), letterWords, {},
                     {}});
                return;
            }
        }
        const QString decomposed = QString::fromUcs4(&code, 1).normalized(
            QString::NormalizationForm_D);
        if (decomposed.size() != 2) {
            return;
        }
        for (const Mark& mark : MARKS) {
            if (decomposed.at(1).unicode() == mark.code) {
                const QString letter = decomposed.left(1);
                add({code, translated(mark.name).arg(letter),
                     QString::fromUtf8(mark.name).arg(letter), letterWords, {}, {}});
                return;
            }
        }
    };
    for (const char32_t letter : ACCENTED_LETTERS) {
        addLetter(letter);
        const char32_t capital = QChar::toUpper(letter);
        if (capital != letter && capital > 0x7F) {
            addLetter(capital);
        }
    }

    addGroup("greek", tr("Greek Alphabet"));
    const QString greekWords = tr("greek") + QStringLiteral(" greek");
    for (const Greek& greek : GREEK) {
        const QString name = translated(greek.name);
        const QString english = QString::fromUtf8(greek.name);
        add({greek.code, tr("%1 (small letter)").arg(name),
             QStringLiteral("%1 (small letter)").arg(english), greekWords, {}, {}});
        add({QChar::toUpper(greek.code), tr("%1 (capital letter)").arg(withCapital(name)),
             QStringLiteral("%1 (capital letter)").arg(withCapital(english)), greekWords, {},
             {}});
    }

    addGroup("arrows", tr("Arrows"));
    for (const Named& named : ARROWS) {
        addNamed(named);
    }
    addGroup("math", tr("Mathematics"));
    for (const Named& named : MATHEMATICS) {
        addNamed(named);
    }
    addGroup("currencies", tr("Currencies"));
    for (const Named& named : CURRENCIES) {
        addNamed(named);
    }
    addGroup("fractions", tr("Fractions and Indices"));
    for (const Named& named : FRACTIONS) {
        addNamed(named);
    }
    const QString powerWords = tr("power") + QStringLiteral(" power");
    for (int digit = 0; digit < 10; ++digit) {
        add({SUPERSCRIPTS[digit], tr("Superscript %1").arg(digit),
             QStringLiteral("Superscript %1").arg(digit), powerWords, {}, {}});
    }
    for (int digit = 0; digit < 10; ++digit) {
        add({SUBSCRIPTS[digit], tr("Subscript %1").arg(digit),
             QStringLiteral("Subscript %1").arg(digit), QString(), {}, {}});
    }
    addGroup("ornaments", tr("Ornaments"));
    for (const Named& named : ORNAMENTS) {
        addNamed(named);
    }
}

void SpecialCharacters::add(SpecialCharacter character) {
    if (m_groups.isEmpty() || m_characters.contains(character.code)) {
        return;
    }
    character.group = m_groups.last().id;
    m_groups.last().characters.append(character.code);
    m_searchTexts.insert(character.code,
                         folded(character.name + QLatin1Char(' ') + character.englishName +
                                QLatin1Char(' ') + character.words));
    m_characters.insert(character.code, character);
}

SpecialCharacter SpecialCharacters::character(char32_t code) const {
    if (const auto found = m_characters.constFind(code); found != m_characters.constEnd()) {
        return *found;
    }
    //: %1 is the code of a character, as U+2603
    return {code, tr("Character %1").arg(codeText(code)),
            QStringLiteral("Character %1").arg(codeText(code)), {}, {}, {}};
}

QList<char32_t> SpecialCharacters::find(const QString& text) const {
    const QString query = text.trimmed();
    QList<char32_t> found;
    if (query.isEmpty()) {
        return found;
    }

    // The character itself, and the character of a code
    if (const QList<uint> typed = query.toUcs4();
        typed.size() == 1 && m_characters.contains(typed.first())) {
        found.append(typed.first());
    }
    if (const std::optional<char32_t> code = codeOf(query); code && !found.contains(*code)) {
        found.append(*code);
    }

    // Every word in the names or the other words
    const QStringList words = folded(query).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const SpecialCharacterGroup& group : m_groups) {
        for (const char32_t code : group.characters) {
            const QString& searched = m_searchTexts[code];
            if (!found.contains(code) &&
                std::all_of(words.begin(), words.end(),
                            [&searched](const QString& word) { return searched.contains(word); })) {
                found.append(code);
            }
        }
    }
    return found;
}

QString SpecialCharacters::codeText(char32_t code) {
    return QStringLiteral("U+%1").arg(static_cast<uint>(code), 4, 16, QLatin1Char('0')).toUpper();
}

std::optional<char32_t> SpecialCharacters::codeOf(const QString& text) {
    QString digits = text.trimmed();
    const bool prefixed = digits.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
                          digits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive);
    if (prefixed) {
        digits = digits.mid(2);
    }
    const auto isDecimal = [](QChar ch) { return ch >= u'0' && ch <= u'9'; };
    const auto isHexadecimal = [&isDecimal](QChar ch) {
        return isDecimal(ch) || (ch.toLower() >= u'a' && ch.toLower() <= u'f');
    };
    if (digits.isEmpty() || digits.size() > 6 ||
        !std::all_of(digits.begin(), digits.end(), isHexadecimal)) {
        return std::nullopt;
    }
    // Without "U+": four digits or more, one of them decimal; a shorter number, or a word of
    // the letters a to f ("face"), is more likely a part of a name
    if (!prefixed &&
        (digits.size() < 4 || std::none_of(digits.begin(), digits.end(), isDecimal))) {
        return std::nullopt;
    }
    const uint code = digits.toUInt(nullptr, 16);
    if (code > 0x10FFFF) {
        return std::nullopt;
    }
    switch (QChar::category(code)) {
        case QChar::Other_Surrogate:
        case QChar::Other_NotAssigned:
        case QChar::Other_Control:
            return std::nullopt;
        default:
            return code;
    }
}

} // namespace gui
} // namespace kalahari
