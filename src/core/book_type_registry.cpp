/// @file book_type_registry.cpp
/// @brief Book type packages loaded from folders, and what a book of each type offers

#include <kalahari/core/book_type_registry.h>

#include <kalahari/core/logger.h>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <functional>
#include <utility>

namespace kalahari::core {

namespace {

constexpr BookPlace ALL_PLACES[] = {BookPlace::Front, BookPlace::Main, BookPlace::Back,
                                    BookPlace::Workshop};

/// Groups are in the main part, so the kinds inside them come mostly from its list
constexpr BookPlace INSIDE_GROUP_ORDER[] = {BookPlace::Main, BookPlace::Front, BookPlace::Back,
                                            BookPlace::Workshop};

/// Kinds of the lists of @p package, each once, in the order of the places in @p order
template <std::size_t N>
QStringList listedKinds(const BookTypePackage& package, const BookPlace (&order)[N]) {
    QStringList kinds;
    for (const BookPlace place : order) {
        for (const QString& kindId : package.kindsIn(place)) {
            if (!kinds.contains(kindId)) {
                kinds << kindId;
            }
        }
    }
    return kinds;
}

/// Styles of @p lineage (most specific package first): the most general first, a style of a
/// more specific package replacing one of its id where it stands
template <typename Style>
QList<Style> mergeStyles(const QList<const BookTypePackage*>& lineage,
                         QList<Style> BookTypePackage::*styles) {
    QList<Style> merged;
    for (auto it = lineage.crbegin(); it != lineage.crend(); ++it) {
        for (const Style& style : (*it)->*styles) {
            const auto same = std::find_if(merged.begin(), merged.end(),
                                           [&style](const Style& other) {
                                               return other.id == style.id;
                                           });
            if (same == merged.end()) {
                merged.append(style);
            } else {
                *same = style;
            }
        }
    }
    return merged;
}

}  // namespace

// =============================================================================
// Loading
// =============================================================================

void BookTypeRegistry::load(const QStringList& directories) {
    m_packages.clear();
    m_packagesById.clear();
    m_problems.clear();
    auto& logger = Logger::getInstance();

    for (const QString& directory : directories) {
        const QDir root(directory);
        if (!root.exists()) {
            logger.debug("Book types: no folder {}", directory.toStdString());
            continue;
        }
        const QFileInfoList folders =
            root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& folder : folders) {
            const QString manifest = QDir(folder.filePath())
                                         .filePath(QString::fromLatin1(
                                             BookTypePackage::MANIFEST_FILE));
            if (!QFileInfo(manifest).isFile()) {
                continue;  // not a package
            }
            QStringList problems;
            std::optional<BookTypePackage> package =
                BookTypePackage::read(folder.absoluteFilePath(), problems);
            if (!package) {
                for (const QString& message : problems) {
                    m_problems.append({folder.absoluteFilePath(), message});
                }
                continue;
            }
            if (const BookTypePackage* first = m_packagesById.value(package->id)) {
                m_problems.append({package->directory,
                                   QStringLiteral("id: package '%1' is already loaded from %2")
                                       .arg(package->id, first->directory)});
                continue;
            }
            m_packages.push_back(std::make_unique<BookTypePackage>(std::move(*package)));
            m_packagesById.insert(m_packages.back()->id, m_packages.back().get());
        }
    }

    // A package is checked after the packages it uses, and only when they are valid
    QHash<QString, CheckState> states;
    for (const auto& package : m_packages) {
        validate(*package, states);
    }
    m_packages.erase(std::remove_if(m_packages.begin(), m_packages.end(),
                                    [&states](const std::unique_ptr<BookTypePackage>& package) {
                                        return states.value(package->id) != CheckState::Valid;
                                    }),
                     m_packages.end());
    m_packagesById.clear();
    for (const auto& package : m_packages) {
        m_packagesById.insert(package->id, package.get());
    }

    for (const BookTypeProblem& problem : m_problems) {
        logger.warn("Book type package {}: {}", problem.directory.toStdString(),
                    problem.message.toStdString());
    }
    logger.info("Book types: {} packages loaded, {} problems", m_packages.size(),
                m_problems.size());
}

bool BookTypeRegistry::validate(const BookTypePackage& package,
                                QHash<QString, CheckState>& states) {
    const CheckState state = states.value(package.id, CheckState::Unchecked);
    if (state != CheckState::Unchecked) {
        return state == CheckState::Valid;
    }
    states.insert(package.id, CheckState::Checking);

    QStringList problems;
    for (const QString& usedId : package.uses) {
        const BookTypePackage* used = m_packagesById.value(usedId);
        if (!used) {
            problems << QStringLiteral("uses: package '%1' is not installed or could not be "
                                       "loaded")
                            .arg(usedId);
        } else if (states.value(usedId, CheckState::Unchecked) == CheckState::Checking) {
            problems << QStringLiteral("uses: package '%1' uses this package, directly or "
                                       "through others")
                            .arg(usedId);
        } else if (!validate(*used, states)) {
            problems << QStringLiteral("uses: package '%1' has problems").arg(usedId);
        }
    }
    if (problems.isEmpty()) {
        problems = check(package);
    }

    for (const QString& message : problems) {
        m_problems.append({package.directory, message});
    }
    states.insert(package.id, problems.isEmpty() ? CheckState::Valid : CheckState::Invalid);
    return problems.isEmpty();
}

QStringList BookTypeRegistry::check(const BookTypePackage& package) const {
    QStringList problems;
    const auto add = [&problems](const QString& field, const QString& message) {
        problems << field + QStringLiteral(": ") + message;
    };

    // Groups that the package's own kinds can be inside
    for (const ElementKind& kind : package.kinds) {
        for (const QString& place : kind.places) {
            if (bookPlaceFromName(place)) {
                continue;
            }
            const KindRef group = findKind(package, place);
            const QString field = QStringLiteral("kinds.%1.places").arg(kind.id);
            if (!group) {
                add(field, QStringLiteral("'%1' is neither a place (front, main, back, "
                                          "workshop) nor a kind")
                               .arg(place));
            } else if (group.kind->form != ElementForm::Group) {
                add(field, QStringLiteral("'%1' is not a group kind, so nothing can be inside it")
                               .arg(place));
            }
        }
    }

    // Lists of the places
    for (const BookPlace place : ALL_PLACES) {
        const QString field = bookPlaceName(place);
        for (const QString& kindId : package.kindsIn(place)) {
            const KindRef entry = findKind(package, kindId);
            if (!entry) {
                add(field, QStringLiteral("unknown kind '%1'").arg(kindId));
                continue;
            }
            if (!entry.kind->allows(place)) {
                add(field, QStringLiteral("kind '%1' cannot be here; its places are: %2")
                               .arg(kindId, entry.kind->places.join(QStringLiteral(", "))));
            }
            // A kind of a used package may be inside groups which this package replaced
            for (const QString& groupId : entry.kind->places) {
                const KindRef group = bookPlaceFromName(groupId) ? KindRef{}
                                                                 : findKind(package, groupId);
                if (group && group.kind->form != ElementForm::Group) {
                    add(field, QStringLiteral("kind '%1' can be inside '%2', which is not a "
                                              "group kind in this package")
                                   .arg(kindId, groupId));
                }
            }
            if (entry.kind->form == ElementForm::Group && kindsInside(package, kindId).isEmpty()) {
                add(field, QStringLiteral("no kind of this package can be inside group '%1'")
                               .arg(kindId));
            }
        }
    }

    // Main text kind
    if (!package.primaryKind.isEmpty()) {
        const KindRef primary = findKind(package, package.primaryKind);
        if (!primary) {
            add(QStringLiteral("primary"),
                QStringLiteral("unknown kind '%1'").arg(package.primaryKind));
        } else if (primary.kind->form != ElementForm::Text) {
            add(QStringLiteral("primary"),
                QStringLiteral("'%1' is not a text kind").arg(package.primaryKind));
        } else if (!package.mainKinds.contains(package.primaryKind)) {
            add(QStringLiteral("primary"),
                QStringLiteral("'%1' is not in the list of the main part")
                    .arg(package.primaryKind));
        }
    }

    // Elements a new book starts with
    const QStringList listed = listedKinds(package, ALL_PLACES);
    QHash<QString, int> counts;
    for (const QString& kindId : package.startKinds) {
        const KindRef start = findKind(package, kindId);
        if (!start) {
            add(QStringLiteral("start"), QStringLiteral("unknown kind '%1'").arg(kindId));
        } else if (!listed.contains(kindId)) {
            add(QStringLiteral("start"),
                QStringLiteral("kind '%1' is in none of the lists front, main, back, workshop")
                    .arg(kindId));
        } else if (start.kind->form == ElementForm::Group) {
            add(QStringLiteral("start"),
                QStringLiteral("'%1' is a group; a group appears with its first element")
                    .arg(kindId));
        } else if (start.kind->limit > 0 && ++counts[kindId] == start.kind->limit + 1) {
            add(QStringLiteral("start"),
                QStringLiteral("more elements of kind '%1' than its limit of %2")
                    .arg(kindId)
                    .arg(start.kind->limit));
        }
    }

    // Styles of the package and of the packages it uses
    const QList<PackageParagraphStyle> paragraphs =
        mergeStyles(lineage(package), &BookTypePackage::paragraphStyles);
    const auto findParagraph = [&paragraphs](const QString& id) -> const PackageParagraphStyle* {
        const auto it = std::find_if(paragraphs.cbegin(), paragraphs.cend(),
                                     [&id](const PackageParagraphStyle& style) {
                                         return style.id == id;
                                     });
        return it == paragraphs.cend() ? nullptr : &*it;
    };
    for (const PackageParagraphStyle& style : package.paragraphStyles) {
        const QString field = QStringLiteral("paragraph_styles.") + style.id;
        if (!style.baseStyle.isEmpty() && !findParagraph(style.baseStyle)) {
            add(field + QStringLiteral(".base_style"),
                QStringLiteral("unknown paragraph style '%1'").arg(style.baseStyle));
        }
        if (!style.nextStyle.isEmpty() && !findParagraph(style.nextStyle)) {
            add(field + QStringLiteral(".next_style"),
                QStringLiteral("unknown paragraph style '%1'").arg(style.nextStyle));
        }
        // Inheritance has to end; a replaced style of a used package can close a circle
        QStringList chain{style.id};
        for (const PackageParagraphStyle* current = findParagraph(style.id);
             current && !current->baseStyle.isEmpty();
             current = findParagraph(current->baseStyle)) {
            const bool circle = chain.contains(current->baseStyle);
            chain << current->baseStyle;
            if (circle) {
                add(field + QStringLiteral(".base_style"),
                    QStringLiteral("inheritance goes round in a circle: %1")
                        .arg(chain.join(QStringLiteral(" > "))));
                break;
            }
        }
    }

    return problems;
}

// =============================================================================
// Resolution
// =============================================================================

QList<const BookTypePackage*> BookTypeRegistry::lineage(const BookTypePackage& package) const {
    QList<const BookTypePackage*> order;
    QSet<QString> visited;
    // After the packages it uses: reversed, a package comes before them
    const std::function<void(const BookTypePackage&)> visit =
        [&](const BookTypePackage& current) {
            if (visited.contains(current.id)) {
                return;
            }
            visited.insert(current.id);
            for (const QString& usedId : current.uses) {
                if (const BookTypePackage* used = m_packagesById.value(usedId)) {
                    visit(*used);
                }
            }
            order.append(&current);
        };
    visit(package);
    std::reverse(order.begin(), order.end());
    return order;
}

KindRef BookTypeRegistry::findKind(const BookTypePackage& package, const QString& kindId) const {
    for (const BookTypePackage* candidate : lineage(package)) {
        if (const ElementKind* kind = candidate->ownKind(kindId)) {
            return {candidate, kind};
        }
    }
    return {};
}

QList<KindRef> BookTypeRegistry::kindsInside(const BookTypePackage& package,
                                             const QString& groupKindId) const {
    QList<KindRef> kinds;
    for (const QString& kindId : listedKinds(package, INSIDE_GROUP_ORDER)) {
        const KindRef ref = findKind(package, kindId);
        if (ref && ref.kind->allowsInside(groupKindId)) {
            kinds.append(ref);
        }
    }
    return kinds;
}

// =============================================================================
// Queries
// =============================================================================

QList<const BookTypePackage*> BookTypeRegistry::packages() const {
    QList<const BookTypePackage*> list;
    for (const auto& package : m_packages) {
        list.append(package.get());
    }
    return list;
}

QList<const BookTypePackage*> BookTypeRegistry::bookTypes() const {
    QList<const BookTypePackage*> list;
    for (const auto& package : m_packages) {
        if (package->role == PackageRole::Type) {
            list.append(package.get());
        }
    }
    return list;
}

const BookTypePackage* BookTypeRegistry::package(const QString& id) const {
    return m_packagesById.value(id);
}

KindRef BookTypeRegistry::findKind(const QString& packageId, const QString& kindId) const {
    const BookTypePackage* found = package(packageId);
    return found ? findKind(*found, kindId) : KindRef{};
}

QList<KindRef> BookTypeRegistry::kindsIn(const QString& packageId, BookPlace place) const {
    QList<KindRef> kinds;
    if (const BookTypePackage* found = package(packageId)) {
        for (const QString& kindId : found->kindsIn(place)) {
            kinds.append(findKind(*found, kindId));
        }
    }
    return kinds;
}

QList<KindRef> BookTypeRegistry::kindsInside(const QString& packageId,
                                             const QString& groupKindId) const {
    const BookTypePackage* found = package(packageId);
    return found ? kindsInside(*found, groupKindId) : QList<KindRef>{};
}

QList<StartElement> BookTypeRegistry::startElements(const QString& packageId) const {
    QList<StartElement> elements;
    const BookTypePackage* found = package(packageId);
    if (!found) {
        return elements;
    }
    for (const QString& kindId : found->startKinds) {
        StartElement element;
        element.kind = findKind(*found, kindId);
        const auto* place = std::find_if(std::begin(ALL_PLACES), std::end(ALL_PLACES),
                                         [found, &kindId](BookPlace candidate) {
                                             return found->kindsIn(candidate).contains(kindId);
                                         });
        if (place != std::end(ALL_PLACES)) {
            element.place = *place;
        }
        elements.append(element);
    }
    return elements;
}

QList<KindRef> BookTypeRegistry::allKinds() const {
    QList<KindRef> kinds;
    for (const auto& package : m_packages) {
        QStringList order;
        for (const QString& kindId : listedKinds(*package, ALL_PLACES)) {
            if (package->ownKind(kindId)) {
                order << kindId;
            }
        }
        for (const ElementKind& kind : package->kinds) {
            if (!order.contains(kind.id)) {
                order << kind.id;
            }
        }
        for (const QString& kindId : order) {
            kinds.append({package.get(), package->ownKind(kindId)});
        }
    }
    return kinds;
}

QList<PackageParagraphStyle> BookTypeRegistry::paragraphStyles(const QString& packageId) const {
    const BookTypePackage* found = package(packageId);
    return found ? mergeStyles(lineage(*found), &BookTypePackage::paragraphStyles)
                 : QList<PackageParagraphStyle>{};
}

QList<PackageCharacterStyle> BookTypeRegistry::characterStyles(const QString& packageId) const {
    const BookTypePackage* found = package(packageId);
    return found ? mergeStyles(lineage(*found), &BookTypePackage::characterStyles)
                 : QList<PackageCharacterStyle>{};
}

}  // namespace kalahari::core
