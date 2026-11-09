/// @file versions_page.cpp
/// @brief Settings page: Editor > Versions
///
/// Save Version and the program read these settings each time they keep a version
/// (VersionsCoordinator, ProjectManager); the colors of the comparison reach the window of the
/// versions through the theme (ThemeManager::editorColor()), the place of the buttons the
/// toolbars (ToolbarManager).

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/chapter_versions.h"
#include "kalahari/editor/version_comparison.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/version_texts.h"
#include "kalahari/gui/widgets/flow_layout.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

namespace {

/// The minutes the writer can choose between the versions kept when a chapter is saved
constexpr int ON_SAVE_MINUTES[] = {5, 10, 15, 30, 60, 120};

/// At most this many versions kept by the program in each chapter
constexpr int MAX_KEPT = 500;

/// The place of the buttons of the versions is at least this many characters wide
constexpr int MIN_BUTTONS_CHARACTERS = 20;

/// A row of a form with @p widgets in one line, as a sentence; in more lines in a narrow window
QWidget* sentenceRow(std::initializer_list<QWidget*> widgets) {
    auto* row = new QWidget();
    auto* layout = new FlowLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* widget : widgets) {
        widget->setParent(row);
        layout->addWidget(widget);
    }
    return row;
}

}  // namespace

EditorVersionsPage::EditorVersionsPage(QWidget* parent)
    : SettingsPage(parent)
{
    using core::ChapterVersions;

    // Versions at all: off, the program keeps no new versions and the groups of how it keeps
    // them wait; the versions kept can be read and restored, the colors and the buttons stay
    //: The first group of Settings > Editor > Versions. In Polish: Wersje tekstów
    QFormLayout* versions = addGroup(tr("Versions of the Texts"));
    //: In Polish: Twórz wersje tekstów
    QCheckBox* enabled = addCheckBox(versions, tr("Create versions of the texts"),
                                     ChapterVersions::ENABLED_SETTING);
    enabled->setObjectName(QStringLiteral("versionsEnabled"));
    QLabel* enabledNote = addNote(versions, QString());
    enabledNote->setObjectName(QStringLiteral("versionsEnabledNote"));
    alignWithText(enabledNote, enabled);

    // Save Version: the small window of the description, or the version kept at once
    //: The group of Settings > Editor > Versions with Save Version. In Polish: Zapisywanie
    QFormLayout* saving = addGroup(tr("Saving"));
    //: In Polish: Pytaj o opis przy zapisie wersji
    QCheckBox* ask = addCheckBox(saving, tr("Ask for a description when a version is saved"),
                                 ChapterVersions::ASK_NAME_SETTING);
    ask->setObjectName(QStringLiteral("versionsAskName"));
    QLabel* askNote = addNote(saving, QString());
    alignWithText(askNote, ask);
    // With the keys Save Version has now (the user may change them)
    const auto showAskNote = [ask, askNote]() {
        const QString keys = saveVersionKeys();
        askNote->setText(
            ask->isChecked()
                //: %1: the keys of File > Save Version (Alt+Shift+S). In Polish: %1 otwiera małe
                //: okno z polem opisu; Enter zapisuje także bez opisu.
                ? tr("%1 opens a small window with a field for the description; Enter saves "
                     "also without one.")
                      .arg(keys)
                //: %1: the keys of File > Save Version (Alt+Shift+S). In Polish: %1 zapisuje od
                //: razu, a pasek stanu to potwierdza. Opis można dodać później.
                : tr("%1 saves at once, and the status bar says so. The description can be "
                     "added later.")
                      .arg(keys));
    };
    connect(ask, &QCheckBox::toggled, this, showAskNote);
    connect(&CommandRegistry::getInstance(), &CommandRegistry::shortcutsChanged, askNote,
            showAskNote);
    whenLoaded(showAskNote);

    // The versions the program keeps by itself, and how many of them stay
    //: In Polish: Wersje zapisywane samoczynnie
    QFormLayout* automatic = addGroup(tr("Versions Saved by the Program"));
    //: Followed by a number of minutes. In Polish: Przy zapisie rozdziału, najwyżej raz na
    auto* onSave = new QCheckBox(tr("When a chapter is saved, at most once in"));
    onSave->setObjectName(QStringLiteral("versionsOnSave"));
    auto* minutes = new QComboBox();
    minutes->setObjectName(QStringLiteral("versionsOnSaveMinutes"));
    for (const int count : ON_SAVE_MINUTES) {
        //: In Polish: %n minutę / minuty / minut
        minutes->addItem(tr("%n minute(s)", nullptr, count), count);
    }
    //: In Polish: Co ile minut najwyżej wersja przy zapisie rozdziału
    minutes->setAccessibleName(tr("Minutes between the versions saved with the chapter"));
    automatic->addRow(sentenceRow({onSave, minutes}));
    bind(onSave, ChapterVersions::ON_SAVE_SETTING);
    bind(minutes, ChapterVersions::ON_SAVE_MINUTES_SETTING);
    //: In Polish: Gdy tekst różni się od ostatniej wersji zapisanej w tym rozdziale. Wersja
    //: to cały tekst rozdziału: przy 5000 słów około 40 KB.
    alignWithText(addNote(automatic, tr("When the text differs from the last version saved in "
                                        "the chapter. A version is the whole text of the "
                                        "chapter: about 40 KB for 5,000 words.")),
                  onSave);
    const auto syncMinutes = [onSave, minutes]() { minutes->setEnabled(onSave->isChecked()); };
    connect(onSave, &QCheckBox::toggled, this, syncMinutes);
    whenLoaded(syncMinutes);

    //: In Polish: Przed „Zamień wszystkie” w rozdziale
    QCheckBox* beforeReplaceAll =
        addCheckBox(automatic, tr("Before Replace All in a chapter"),
                    ChapterVersions::BEFORE_REPLACE_ALL_SETTING);
    beforeReplaceAll->setObjectName(QStringLiteral("versionsBeforeReplaceAll"));
    //: In Polish: Opis podaje, co zamieniono na co i ile razy.
    alignWithText(addNote(automatic, tr("The description says what was replaced with what, and "
                                        "how many times.")),
                  beforeReplaceAll);
    //: In Polish: Przed przywróceniem innej wersji program zawsze zapisuje bieżący tekst, żeby
    //: można było do niego wrócić.
    addNote(automatic, tr("Before another version is restored, the program always keeps the "
                          "current text, so that it can be brought back."));

    //: Followed by a number. In Polish: Zachowuj w każdym rozdziale najwyżej
    auto* keepLabel = new QLabel(tr("Keep in each chapter at most"));
    auto* keep = new QSpinBox();
    keep->setObjectName(QStringLiteral("versionsKeepAutomatic"));
    keep->setRange(1, MAX_KEPT);
    keepLabel->setBuddy(keep);
    //: After a number. In Polish: wersji zapisanych samoczynnie
    auto* keepUnit = new QLabel(tr("versions saved by the program"));
    automatic->addRow(sentenceRow({keepLabel, keep, keepUnit}));
    bind(keep, ChapterVersions::KEEP_AUTOMATIC_SETTING);
    //: In Polish: Starsze usuwa program. Wersje zapisane przez Ciebie zostają, dopóki ich nie
    //: usuniesz.
    addNote(automatic, tr("The program deletes the older ones. The versions you save stay until "
                          "you delete them."));

    // Turned off, the program keeps no new version: the groups of how it keeps them wait, and
    // the note says what still works
    QWidget* savingGroup = saving->parentWidget();
    QWidget* automaticGroup = automatic->parentWidget();
    const auto syncEnabled = [enabled, enabledNote, savingGroup, automaticGroup]() {
        const bool on = enabled->isChecked();
        savingGroup->setEnabled(on);
        automaticGroup->setEnabled(on);
        //: In Polish: Wersja zachowuje cały tekst rozdziału, żeby można było do niego wrócić.
        const QString what = tr("A version keeps the whole text of a chapter, so that it can be "
                                "brought back.");
        enabledNote->setText(
            what + QLatin1Char(' ') +
            (on
                 //: In Polish: Wyłączone: program nie zapisuje nowych wersji – ani Twoich, ani
                 //: samoczynnych.
                 ? tr("Off: the program keeps no new versions, neither yours nor its own.")
                 //: In Polish: Program nie zapisuje teraz nowych wersji. Zapisane można dalej
                 //: czytać, porównywać i przywracać w historii; przed przywróceniem program
                 //: zachowuje bieżący tekst.
                 : tr("The program keeps no new versions now. Those saved can still be read, "
                      "compared and restored in the history; before a restore the program "
                      "keeps the current text.")));
    };
    connect(enabled, &QCheckBox::toggled, this, syncEnabled);
    whenLoaded(syncEnabled);

    // Where the buttons of the versions are: on a toolbar of their own, with Backup Now, or
    // on Quick Actions; the toolbars change at once when the setting is applied
    //: The group of Settings > Editor > Versions with the place of the buttons. In Polish:
    //: Pasek narzędzi
    QFormLayout* toolbar = addGroup(tr("Toolbar"));
    auto* buttons = new QComboBox();
    buttons->setObjectName(QStringLiteral("versionsButtons"));
    // As wide as the window lets it be: in the narrowest one the long names are cut, the list
    // shows them whole
    buttons->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    buttons->setMinimumContentsLength(MIN_BUTTONS_CHARACTERS);
    //: The toolbar View > Toolbars > Versions and Backups. In Polish: Na osobnym pasku „Wersje
    //: i kopie”
    buttons->addItem(tr("On a toolbar of their own, \"Versions and Backups\""),
                     QString::fromLatin1(ToolbarManager::VERSION_BUTTONS_OWN_TOOLBAR));
    //: In Polish: Na pasku „Szybki dostęp”
    buttons->addItem(tr("On the toolbar \"Quick Actions\""),
                     QString::fromLatin1(ToolbarManager::VERSION_BUTTONS_QUICK_ACTIONS));
    //: In Polish: Przyciski wersji:
    addField(toolbar, tr("Version buttons:"), buttons, ToolbarManager::VERSION_BUTTONS_KEY);
    //: In Polish: „Zapisz wersję” i „Historia wersji”. Osobny pasek można przesunąć albo ukryć
    //: w menu Widok > Paski narzędzi.
    addNote(toolbar, tr("\"Save Version\" and \"Version History\". The toolbar of their own can "
                        "be moved, or hidden in the menu View > Toolbars."));

    // The marks of the changes in the window of the versions, kept for each theme
    //: In Polish: Kolory porównania
    QFormLayout* colors = addGroup(tr("Colors of the Comparison"));
    //: In Polish: Kolory tekstu w trybie „Zmiany do bieżącego tekstu” w oknie „Historia wersji”.
    //: Każdy motyw ma swoje kolory: to kolory bieżącego motywu.
    addNote(colors, tr("The colors of the text in the mode \"Changes to the Current Text\" of "
                       "the window \"Version History\". Each theme has its own colors: these are "
                       "the colors of the current theme."));
    //: Struck out: in the version, not in the current text. In Polish: Usunięty tekst
    addThemeColor(colors, tr("Deleted text"), editor::DELETED_COLOR_KEY);
    //: Underlined: written later. In Polish: Dopisany tekst
    addThemeColor(colors, tr("Added text"), editor::ADDED_COLOR_KEY);
    addOptimizeColorsCheckBox(colors);
    addRestoreColorsButton(colors);

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
