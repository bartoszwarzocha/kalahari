/// @file network_page.cpp
/// @brief Settings page: Network > Materials
///
/// Whether the program connects to the web address of a material for the title of its page or
/// the copy of its picture: when the box of the Add Element window, empty each time, asks for
/// it (the usual one), at every new material with an address, or never. The program reads
/// the setting each time a material with an address is added.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/material_downloader.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>

namespace kalahari {
namespace gui {

namespace {

/// The values of MaterialDownloader::SETTING
constexpr const char* ASK = "ask";
constexpr const char* ALWAYS = "always";
constexpr const char* NEVER = "never";

} // namespace

NetworkMaterialsPage::NetworkMaterialsPage(QWidget* parent)
    : SettingsPage(parent)
{
    //: The group of the page Network > Materials. In Polish: Materiały z adresem
    QFormLayout* group = addGroup(tr("Materials with an Address"));
    QComboBox* downloads = addChoice(
        group,
        //: In Polish: Pobieranie tytułu strony i kopii obrazu:
        tr("Downloading the title of a page and the copy of a picture:"),
        //: In Polish: Na życzenie
        {{tr("On request"), QLatin1String(ASK),
          //: In Polish: Pobieranie włącza pole w oknie „Dodaj element”, za każdym razem puste.
          //: Bez niego program nie łączy się z siecią, a materiał zachowuje sam adres.
          tr("The box in the Add Element window, empty each time, turns it on. Without it the "
             "program does not connect to the network, and the material keeps only its "
             "address.")},
         //: In Polish: Zawsze
         {tr("Always"), QLatin1String(ALWAYS),
          //: In Polish: Każdy nowy materiał z adresem, także upuszczony na Nawigator albo
          //: wklejony w nim, dostaje tytuł strony albo kopię obrazu.
          tr("Every new material with an address, also one dropped on the Navigator or pasted "
             "in it, gets the title of its page or a copy of its picture.")},
         //: In Polish: Nigdy
         {tr("Never"), QLatin1String(NEVER),
          //: In Polish: Program nie łączy się z adresami materiałów: tytuł podpowiada koniec
          //: adresu, a obraz z sieci nie ma kopii.
          tr("The program does not connect to the addresses of materials: the end of the "
             "address gives the title, and a picture on the web has no copy.")}},
        std::string());
    downloads->setObjectName(QStringLiteral("networkDownloads"));
    if (auto* label = qobject_cast<QLabel*>(group->labelForField(downloads))) {
        label->setWordWrap(true);
    }
    bindChoice(downloads, MaterialDownloader::SETTING, ASK);
    //: In Polish: Program pobiera tylko adres materiału: stronę do jej tytułu albo obraz, którego
    //: kopię zapisuje w folderze „workshop” książki.
    addNote(group, tr("The program downloads only the address of the material: a page up to its "
                      "title, or a picture, whose copy it saves in the book's \"workshop\" "
                      "folder."));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
