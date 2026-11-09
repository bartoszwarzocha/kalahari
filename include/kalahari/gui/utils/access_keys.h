/// @file access_keys.h
/// @brief The access keys of menu texts
///
/// A menu text marks its access key with '&' before the letter ("Save &As..."): on Windows
/// and Linux Alt with the letter chooses the item (the letter is underlined), and "&&"
/// stands for an ampersand. macOS has no access keys and shows the text without the marks.

#pragma once

#include <QChar>
#include <QSet>
#include <QString>
#include <QStringList>

class QAction;
class QMenu;
class QWidget;

namespace kalahari {
namespace gui {

/// @brief A menu text as lists and tooltips show it: without its access key
///        ("Save &As..." → "Save As...", "Find && Rep&lace..." → "Find & Replace...")
[[nodiscard]] QString withoutAccessKey(const QString& text);

/// @brief A text shown in a menu as it is, with no access key ("Tom & Jerry" →
///        "Tom && Jerry"), for a name the user gave (a book's, a toolbar's)
[[nodiscard]] QString literalMenuText(const QString& text);

/// @brief The access key of a menu text, in upper case; a null QChar for a text without one
[[nodiscard]] QChar accessKeyOf(const QString& text);

/// @brief The access keys of the items of a menu (upper case)
/// @param except An item left out: one whose text changes
[[nodiscard]] QSet<QChar> accessKeysIn(const QMenu* menu, const QAction* except = nullptr);

/// @brief A name the user gave with an access key no other item of its menu has: the first
///        letter of one of its words, or else another letter or digit of it; none when the
///        menu has each of them already
/// @param used The access keys of the other items of the menu (upper case); gets the new
///        one
[[nodiscard]] QString withFreeAccessKey(const QString& name, QSet<QChar>& used);

/// @brief Names a menu shows as they are, each with an access key no other item of the menu
///        has, as withFreeAccessKey() gives it to each in their order
///
/// Where the letters and digits of a name are all taken, a name before it takes another of
/// its own and leaves it one (the panels and the toolbars of a window share a menu, and
/// their names share letters): as many names as can have a key get one.
/// @param used The access keys of the other items of the menu (upper case); gets the new
///        ones
[[nodiscard]] QStringList withFreeAccessKeys(const QStringList& names, QSet<QChar>& used);

/// @brief Give each item of a menu without an access key a free one (withFreeAccessKeys()):
///        the words and the names a menu shows as they are, such as the words for a
///        misspelled one
///
/// The commands of the program have keys of their own. An item the menu shares with other
/// menus (a command of the window) keeps its text, and so does a disabled item without a
/// key: a line that only tells something ("(No suggestions)"). The keys of an item, after a
/// tab, stay as they are.
void giveFreeAccessKeys(QMenu* menu);

/// @brief A copy of a menu that names other things (Qt's menu of the panels and the toolbars
///        of a window, named after their titles), with an access key in each item
///
/// The items of the copy are its own and choose the items of @p menu; the things keep their
/// names elsewhere. Hidden items of @p menu are left out.
/// @param parent The parent of the copy
[[nodiscard]] QMenu* menuWithAccessKeys(const QMenu& menu, QWidget* parent);

} // namespace gui
} // namespace kalahari
