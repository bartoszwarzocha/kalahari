/// @file access_keys.cpp
/// @brief Implementation of the access keys of menu texts

#include "kalahari/gui/utils/access_keys.h"

#include <QAction>
#include <QHash>
#include <QList>
#include <QMenu>

namespace kalahari {
namespace gui {

namespace {

constexpr QChar MARK = QLatin1Char('&');

/// A key the keyboard gives with Alt alone: a letter or a digit of the Latin alphabet (Alt
/// with "ą" is AltGr on the Polish keyboard, which types the letter)
bool canBeAccessKey(QChar c) {
    return c.unicode() < 0x80 && c.isLetterOrNumber();
}

/// The places in a name of the characters that can be its access key, in the order they are
/// wanted: the first letters of its words, then its other letters and digits
QList<qsizetype> keyPlaces(const QString& name) {
    QList<qsizetype> wordStarts;
    QList<qsizetype> others;
    for (qsizetype i = 0; i < name.size(); ++i) {
        if (!canBeAccessKey(name.at(i))) {
            continue;
        }
        const bool startsWord = i == 0 || !name.at(i - 1).isLetterOrNumber();
        (startsWord ? wordStarts : others).append(i);
    }
    return wordStarts + others;
}

/// The access keys of names, in their order: each takes the first of its keys that is free.
/// Where none is, a name that has one of its keys takes another of its own and leaves it that
/// one, and so on along a path (as a maximum matching is found): no name that could have a
/// key is left without one.
class KeyChoice {
public:
    KeyChoice(const QStringList& names, const QSet<QChar>& used)
        : m_names(names), m_used(used), m_chosen(names.size(), -1) {
        for (const QString& name : names) {
            m_places.append(keyPlaces(name));
        }
        for (qsizetype index = 0; index < names.size(); ++index) {
            QSet<QChar> tried;
            take(index, tried);
        }
    }

    /// The place of each name's key in it; -1 for a name without one
    [[nodiscard]] const QList<qsizetype>& chosen() const { return m_chosen; }

private:
    [[nodiscard]] QChar keyAt(qsizetype index, qsizetype place) const {
        return m_names.at(index).at(place).toUpper();
    }

    /// Give a name a key: a free one, else one another name can leave it; whether it got one
    bool take(qsizetype index, QSet<QChar>& tried) {
        for (const qsizetype place : m_places.at(index)) {
            const QChar key = keyAt(index, place);
            if (!m_used.contains(key) && !m_owners.contains(key)) {
                give(index, place, key);
                return true;
            }
        }
        for (const qsizetype place : m_places.at(index)) {
            const QChar key = keyAt(index, place);
            if (m_used.contains(key) || tried.contains(key)) {
                continue;
            }
            tried.insert(key);
            if (take(m_owners.value(key), tried)) {
                give(index, place, key);
                return true;
            }
        }
        return false;
    }

    void give(qsizetype index, qsizetype place, QChar key) {
        m_owners.insert(key, index);
        m_chosen[index] = place;
    }

    QStringList m_names;
    QSet<QChar> m_used;                     ///< The keys of the menu's other items
    QList<QList<qsizetype>> m_places;       ///< keyPlaces() of each name
    QHash<QChar, qsizetype> m_owners;       ///< The name that has each key given
    QList<qsizetype> m_chosen;
};

}  // namespace

QString withoutAccessKey(const QString& text) {
    QString plain;
    plain.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) != MARK) {
            plain += text.at(i);
        } else if (i + 1 < text.size() && text.at(i + 1) == MARK) {
            plain += MARK;
            ++i;
        }
    }
    return plain;
}

QString literalMenuText(const QString& text) {
    return QString(text).replace(MARK, QStringLiteral("&&"));
}

QChar accessKeyOf(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) != MARK) {
            continue;
        }
        if (text.at(i + 1) == MARK) {
            ++i;  // an ampersand
            continue;
        }
        return text.at(i + 1).toUpper();
    }
    return {};
}

QSet<QChar> accessKeysIn(const QMenu* menu, const QAction* except) {
    QSet<QChar> keys;
    for (const QAction* action : menu->actions()) {
        const QChar key = accessKeyOf(action->text());
        if (action != except && !key.isNull()) {
            keys.insert(key);
        }
    }
    return keys;
}

QString withFreeAccessKey(const QString& name, QSet<QChar>& used) {
    return withFreeAccessKeys(QStringList{name}, used).constFirst();
}

QStringList withFreeAccessKeys(const QStringList& names, QSet<QChar>& used) {
    const KeyChoice choice(names, used);
    QStringList texts;
    texts.reserve(names.size());
    for (qsizetype index = 0; index < names.size(); ++index) {
        const QString& name = names.at(index);
        const qsizetype place = choice.chosen().at(index);
        if (place < 0) {
            texts.append(literalMenuText(name));
            continue;
        }
        used.insert(name.at(place).toUpper());
        texts.append(literalMenuText(name.left(place)) + MARK + literalMenuText(name.mid(place)));
    }
    return texts;
}

void giveFreeAccessKeys(QMenu* menu) {
    // The menu's own items without a key: a command of the window keeps its text
    QList<QAction*> keyless;
    QStringList names;
    for (QAction* action : menu->actions()) {
        const QString text = action->text();
        if (action->isSeparator() || !action->isEnabled() || !accessKeyOf(text).isNull()) {
            continue;
        }
        const QMenu* submenu = action->menu();
        const QObject* owner = submenu != nullptr ? submenu->parent() : action->parent();
        if (owner == menu) {
            keyless.append(action);
            names.append(withoutAccessKey(text.section(QLatin1Char('\t'), 0, 0)));
        }
    }
    if (keyless.isEmpty()) {
        return;
    }

    QSet<QChar> used = accessKeysIn(menu);
    const QStringList keyed = withFreeAccessKeys(names, used);
    for (qsizetype index = 0; index < keyless.size(); ++index) {
        if (accessKeyOf(keyed.at(index)).isNull()) {
            continue;  // each of its letters taken
        }
        // The keys of the item, after a tab, stay
        const QString text = keyless.at(index)->text();
        const qsizetype tab = text.indexOf(QLatin1Char('\t'));
        keyless.at(index)->setText(tab < 0 ? keyed.at(index) : keyed.at(index) + text.mid(tab));
    }
}

QMenu* menuWithAccessKeys(const QMenu& menu, QWidget* parent) {
    QStringList names;
    for (const QAction* item : menu.actions()) {
        if (!item->isSeparator()) {
            names.append(item->text());
        }
    }
    QSet<QChar> used;
    const QStringList texts = withFreeAccessKeys(names, used);

    auto* copy = new QMenu(parent);
    qsizetype next = 0;
    for (QAction* item : menu.actions()) {
        if (item->isSeparator()) {
            copy->addSeparator();
            continue;
        }
        QAction* own = copy->addAction(item->icon(), texts.at(next++));
        own->setCheckable(item->isCheckable());
        own->setChecked(item->isChecked());
        own->setEnabled(item->isEnabled());
        QObject::connect(own, &QAction::triggered, item, &QAction::trigger);
    }
    return copy;
}

} // namespace gui
} // namespace kalahari
