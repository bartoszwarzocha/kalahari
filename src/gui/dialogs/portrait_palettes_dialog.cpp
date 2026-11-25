// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file portrait_palettes_dialog.cpp
/// @brief The window Portrait Palettes: the colors the window Portrait chooses from

#include "kalahari/gui/dialogs/portrait_palettes_dialog.h"

#include "kalahari/gui/avatar.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace kalahari::gui::dialogs {

PortraitPalettesDialog::PortraitPalettesDialog(const AvatarPalettes& palettes, QWidget* parent)
    : KalahariDialog(parent) {
    //: Settings > Cards > Portrait Palettes... In Polish: Palety portretów / Kolory, z których
    //: wybiera okno „Portret”. Kolory spoza natury – skóra elfa, oczy wampira – są osobno.
    setHeading(tr("Portrait Palettes"),
               tr("The colors the window Portrait chooses from. The colors that are not "
                  "natural – the skin of an elf, the eyes of a vampire – are apart."));
    setHeadingIcon(QStringLiteral("template.character"));
    setCompactHeading(true);
    setAcceptText(tr("OK"));

    // A row of each natural palette, with the one of the colors that are not natural beside it
    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 3);
    grid->setColumnStretch(3, 1);
    const QString other = QStringLiteral("Other");
    int row = -1;
    for (const QString& name : AvatarPalettes::names()) {
        const QString title = AvatarPalettes::title(name);
        //: The title of the color window of a swatch; %1: the palette, such as "Hair". In
        //: Polish: Kolor palety portretów: %1
        auto* edit = new PaletteEdit(tr("A color of the portrait palette: %1").arg(title), this);
        edit->setObjectName(QStringLiteral("portraitPalette_") + name);
        m_edits << edit;
        const bool natural = !name.endsWith(other);
        row += natural ? 1 : 0;
        //: %1: the name of a palette, such as "Hair". In Polish: %1:
        QString text = tr("%1:").arg(title);
        if (!natural) {
            //: Beside a palette of colors that are not natural, such as the skin of an elf.
            //: In Polish: Spoza natury:
            text = tr("Not natural:");
        }
        auto* label = new QLabel(text, this);
        label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        const int column = natural ? 0 : 2;
        grid->addWidget(label, row, column);
        grid->addWidget(edit, row, column + 1, Qt::AlignTop);
    }
    contentLayout()->addLayout(grid);
    // Each palette of the program in one line; a longer one goes on in the next
    showPalettes(AvatarPalettes::defaults());
    const QStringList names = AvatarPalettes::names();
    for (qsizetype i = 0; i < m_edits.size(); ++i) {
        const int column = names.at(i).endsWith(other) ? 3 : 1;
        grid->setColumnMinimumWidth(column, std::max(grid->columnMinimumWidth(column),
                                                     m_edits.at(i)->sizeHint().width()));
    }
    //: A color without a name is left out of the description of the look. In Polish: Okno
    //: „Portret” opisuje wygląd nazwami kolorów programu; kolor dodany tutaj nie ma nazwy,
    //: więc opis go pomija.
    auto* note = new QLabel(tr("The window Portrait describes the look with the names of the "
                               "colors of the program; a color added here has no name, so the "
                               "description leaves it out."),
                            this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(note);

    //: In Polish: &Przywróć domyślne
    m_restore = addSideButton(tr("&Restore Defaults"));
    m_restore->setObjectName(QStringLiteral("portraitPalettesRestore"));
    connect(m_restore, &QPushButton::clicked, this,
            [this]() { showPalettes(AvatarPalettes::defaults()); });
    showPalettes(palettes);
}

AvatarPalettes PortraitPalettesDialog::palettes() const {
    AvatarPalettes palettes;
    const QStringList names = AvatarPalettes::names();
    for (qsizetype i = 0; i < m_edits.size(); ++i) {
        palettes.setColors(names.at(i), m_edits.at(i)->colors());
    }
    return palettes;
}

PaletteEdit* PortraitPalettesDialog::paletteEdit(const QString& name) const {
    const qsizetype index = AvatarPalettes::names().indexOf(name);
    return index < 0 ? nullptr : m_edits.at(index);
}

void PortraitPalettesDialog::showPalettes(const AvatarPalettes& palettes) {
    const QStringList names = AvatarPalettes::names();
    for (qsizetype i = 0; i < m_edits.size(); ++i) {
        m_edits.at(i)->setColors(palettes.colors(names.at(i)));
    }
}

std::optional<AvatarPalettes> editPortraitPalettes(const AvatarPalettes& palettes,
                                                   QWidget* parent) {
    PortraitPalettesDialog dialog(palettes, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return dialog.palettes();
}

}  // namespace kalahari::gui::dialogs
