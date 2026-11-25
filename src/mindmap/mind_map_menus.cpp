// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_menus.cpp
/// @brief What the map and the list of a mind map share

#include <kalahari/mindmap/mind_map_menus.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/mindmap/mind_map_document.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

#include <array>

namespace kalahari::mindmap {

namespace {

/// The size of the swatches of the colors in the menus
constexpr int SWATCH = 12;
constexpr double ICON_OPACITY = 0.8;
constexpr double ICON_LINE = 1.3;

/// A swatch of color @p color, framed with @p frame
QIcon swatch(const QColor& color, const QColor& frame) {
    QPixmap pixmap(SWATCH, SWATCH);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(frame);
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(0.5, 0.5, SWATCH - 1.0, SWATCH - 1.0), 2.0, 2.0);
    return QIcon(pixmap);
}

}  // namespace

void MindMapMenus::addMarkers(QMenu& menu, MindMapDocument& document, const MindMapLook& look,
                              const QString& id) {
    const MindMapNode* node = document.map().node(id);
    //: In the menu of a node of a mind map. In Polish: Znaczniki
    QMenu* markers = menu.addMenu(tr("Markers"));
    const std::array<QString, 5> names{
        //: A marker of a node. In Polish: Pytanie
        tr("Question"),
        //: A marker of a node. In Polish: Ważne
        tr("Important"),
        //: A marker of a node. In Polish: Pomysł
        tr("Idea"),
        //: A marker of a node. In Polish: Zrobione
        tr("Done"),
        //: A marker of a node. In Polish: Do sprawdzenia
        tr("To check")};
    const std::array<const char*, 5>& ids = markerIds();
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const QString marker = QString::fromLatin1(ids.at(i));
        QAction* action =
            markers->addAction(swatch(look.markerColor(marker), look.mid), names.at(i));
        action->setCheckable(true);
        action->setChecked(node != nullptr && node->markers.contains(marker));
        QObject::connect(action, &QAction::triggered, &document,
                         [&document, id, marker]() { document.toggleMarker(id, marker); });
    }
}

void MindMapMenus::addColors(QMenu& menu, MindMapDocument& document, const MindMapLook& look,
                             const QString& id) {
    const MindMapNode* node = document.map().node(id);
    //: In the menu of a node of a mind map. In Polish: Kolor gałęzi
    QMenu* colors = menu.addMenu(tr("Color of the branch"));
    //: The color of a branch: the one it takes from the branch above. In Polish: Z gałęzi
    QAction* inherited = colors->addAction(tr("From its branch"));
    inherited->setCheckable(true);
    inherited->setChecked(node != nullptr && node->color.isEmpty());
    QObject::connect(inherited, &QAction::triggered, &document,
                     [&document, id]() { document.setColor(id, QString()); });
    colors->addSeparator();
    for (std::size_t i = 0; i < look.palette.size(); ++i) {
        const QString key = QString::number(i + 1);
        //: A color of the palette of the branches. In Polish: Kolor %1
        QAction* action =
            colors->addAction(swatch(look.palette.at(i), look.mid), tr("Color %1").arg(key));
        action->setCheckable(true);
        action->setChecked(node != nullptr && node->color == key);
        QObject::connect(action, &QAction::triggered, &document,
                         [&document, id, key]() { document.setColor(id, key); });
    }
}

bool MindMapMenus::remove(MindMapDocument& document, const QString& id, QString* message) {
    if (document.map().node(id) == nullptr) {
        return false;
    }
    if (id == document.map().root()) {
        //: In Polish: Temat główny zostaje: to tytuł mapy. Usuń jego gałęzie albo całą mapę w
        //: Nawigatorze.
        *message = tr("The main topic stays: it is the title of the map. Delete its branches, "
                      "or the whole map in the Navigator.");
        return false;
    }
    const QString text = document.shownText(id);
    const int removed = document.removeNode(id);
    if (removed == 1) {
        //: In Polish: Usunięto „%1”.
        *message = tr("Deleted “%1”.").arg(text);
    } else if (removed > 1) {
        //: In Polish: Usunięto „%1” z gałęzią: %n węzeł / węzły / węzłów.
        *message = tr("Deleted “%1” with its branch: %n node(s).", nullptr, removed).arg(text);
    }
    return removed > 0;
}

QString MindMapMenus::copy(const MindMapDocument& document, const QString& id) {
    if (document.map().node(id) == nullptr) {
        return QString();
    }
    const QString outline = document.map().outlineOf(
        id, [&document](const MindMapNode& node) { return document.shownText(node); });
    QGuiApplication::clipboard()->setText(outline);
    const auto lines = static_cast<int>(outline.count(QLatin1Char('\n')) + 1);
    //: In Polish: Skopiowano „%1” jako listę z wcięciami: %n wiersz / wiersze / wierszy.
    return tr("Copied “%1” as an outline: %n line(s).", nullptr, lines)
        .arg(document.shownText(id));
}

bool MindMapMenus::canPaste() {
    return !QGuiApplication::clipboard()->text().trimmed().isEmpty();
}

QStringList MindMapMenus::paste(MindMapDocument& document, const QString& parent,
                                QString* message) {
    const QString text = QGuiApplication::clipboard()->text();
    QStringList lines = text.split(QLatin1Char('\n'));
    lines.removeIf([](const QString& line) { return line.trimmed().isEmpty(); });
    if (lines.isEmpty() || document.map().node(parent) == nullptr) {
        return {};
    }
    const bool tree =
        core::SettingsManager::getInstance().get<bool>(MindMapSettings::PASTE_TREE, true);
    QStringList added;
    if (tree && lines.size() > 1) {
        added = document.pasteOutline(parent, -1, text);
    } else {
        QStringList trimmed;
        for (const QString& line : std::as_const(lines)) {
            trimmed.append(line.trimmed());
        }
        const QString one = trimmed.join(QLatin1Char('\n'));
        //: What Undo undoes in a mind map. In Polish: wklejenie
        document.change(tr("pasting"), [&parent, &one, &added](MindMap& map) {
            const QString id = map.addChild(parent, one);
            if (!id.isEmpty()) {
                added.append(id);
            }
            return !id.isEmpty();
        });
        if (!added.isEmpty()) {
            document.select(added.first());
        }
    }
    if (!added.isEmpty()) {
        //: In Polish: Wklejono do „%1”: %n węzeł / węzły / węzłów.
        *message = tr("Pasted into “%1”: %n node(s).", nullptr, static_cast<int>(added.size()))
                       .arg(document.shownText(parent));
    }
    return added;
}

/// A link: two rings of a chain, across the square @p rect
void MindMapMenus::paintLinkIcon(QPainter& painter, const QRectF& rect, const QColor& color) {
    painter.save();
    painter.setOpacity(painter.opacity() * ICON_OPACITY);
    QPen pen(color, ICON_LINE);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const double size = rect.width();
    painter.translate(rect.center());
    painter.rotate(-45.0);
    const QRectF ring(-size * 0.5, -size * 0.2, size * 0.55, size * 0.4);
    painter.drawRoundedRect(ring, size * 0.2, size * 0.2);
    painter.drawRoundedRect(ring.translated(size * 0.45, 0.0), size * 0.2, size * 0.2);
    painter.restore();
}

/// A note: a sheet with lines, in the square @p rect
void MindMapMenus::paintNoteIcon(QPainter& painter, const QRectF& rect, const QColor& color) {
    painter.save();
    painter.setOpacity(painter.opacity() * ICON_OPACITY);
    QPen pen(color, ICON_LINE);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF sheet = rect.adjusted(rect.width() * 0.18, rect.height() * 0.08,
                                       -rect.width() * 0.18, -rect.height() * 0.08);
    painter.drawRoundedRect(sheet, 1.5, 1.5);
    for (int line = 1; line <= 3; ++line) {
        const double y = sheet.top() + sheet.height() * line / 4.0;
        painter.drawLine(QPointF(sheet.left() + sheet.width() * 0.22, y),
                         QPointF(sheet.right() - sheet.width() * 0.22, y));
    }
    painter.restore();
}

}  // namespace kalahari::mindmap
