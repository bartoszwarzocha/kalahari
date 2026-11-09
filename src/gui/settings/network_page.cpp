/// @file network_page.cpp
/// @brief Settings page: Network > Materials
///
/// Whether the program connects to the web address of a material for the title of its page or
/// the copy of its picture: when the box of the Add Element window, empty each time, asks for
/// it (the usual one), at every new material with an address, or never. The program reads
/// the setting each time a material with an address is added.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/material_downloader.h"
#include "kalahari/core/settings_manager.h"

#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

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
    //: In Polish: Na życzenie
    m_ask = new QRadioButton(tr("On request"));
    m_ask->setObjectName(QStringLiteral("networkDownloadsAsk"));
    //: In Polish: Zawsze
    m_always = new QRadioButton(tr("Always"));
    m_always->setObjectName(QStringLiteral("networkDownloadsAlways"));
    //: In Polish: Nigdy
    m_never = new QRadioButton(tr("Never"));
    m_never->setObjectName(QStringLiteral("networkDownloadsNever"));
    auto* choices = new QWidget();
    auto* layout = new QVBoxLayout(choices);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_ask);
    //: In Polish: Pobieranie włącza pole w oknie „Dodaj element”, za każdym razem puste. Bez
    //: niego program nie łączy się z siecią, a materiał zachowuje sam adres.
    alignWithText(addNote(layout, tr("The box in the Add Element window, empty each time, turns "
                                     "it on. Without it the program does not connect to the "
                                     "network, and the material keeps only its address.")),
                  m_ask);
    layout->addWidget(m_always);
    //: In Polish: Każdy nowy materiał z adresem, także upuszczony na Nawigator albo wklejony w
    //: nim, dostaje tytuł strony albo kopię obrazu.
    alignWithText(addNote(layout, tr("Every new material with an address, also one dropped on "
                                     "the Navigator or pasted in it, gets the title of its page "
                                     "or a copy of its picture.")),
                  m_always);
    layout->addWidget(m_never);
    //: In Polish: Program nie łączy się z adresami materiałów: tytuł podpowiada koniec adresu,
    //: a obraz z sieci nie ma kopii.
    alignWithText(addNote(layout, tr("The program does not connect to the addresses of "
                                     "materials: the end of the address gives the title, and "
                                     "a picture on the web has no copy.")),
                  m_never);
    //: In Polish: Pobieranie tytułu strony i kopii obrazu:
    auto* label = new QLabel(tr("Downloading the title of a page and the copy of a picture:"));
    label->setWordWrap(true);
    label->setBuddy(m_ask);
    group->addRow(label, choices);
    //: In Polish: Program pobiera tylko adres materiału: stronę do jej tytułu albo obraz, którego
    //: kopię zapisuje w folderze „workshop” książki.
    addNote(group, tr("The program downloads only the address of the material: a page up to its "
                      "title, or a picture, whose copy it saves in the book's \"workshop\" "
                      "folder."));

    auto& settings = core::SettingsManager::getInstance();
    Binding downloads;
    downloads.key = []() { return std::string(MaterialDownloader::SETTING); };
    downloads.stored = [&settings]() {
        return json(settings.get<std::string>(MaterialDownloader::SETTING, ASK));
    };
    downloads.shown = [this]() {
        return json(m_always->isChecked() ? ALWAYS : m_never->isChecked() ? NEVER : ASK);
    };
    downloads.show = [this](const json& value) {
        const std::string chosen = value.is_string() ? value.get<std::string>() : ASK;
        (chosen == ALWAYS ? m_always : chosen == NEVER ? m_never : m_ask)->setChecked(true);
    };
    downloads.store = [&settings](const json& value) {
        settings.set<std::string>(MaterialDownloader::SETTING, value.get<std::string>());
    };
    bindCustom(std::move(downloads));

    pageLayout()->addStretch();
}

} // namespace gui
} // namespace kalahari
