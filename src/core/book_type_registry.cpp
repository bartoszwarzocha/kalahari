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

/// Packages whose kinds @p package names in its lists, "primary" and "start", each with the
/// first field that names one of its kinds
QList<std::pair<QString, QString>> namedPackages(const BookTypePackage& package) {
    QList<std::pair<QString, QString>> named;
    const auto add = [&named, &package](const QString& field, const QString& kindName) {
        const std::optional<KindReference> reference = KindReference::parse(kindName);
        if (!reference || reference->packageId.isEmpty() || reference->packageId == package.id) {
            return;
        }
        const bool known =
            std::any_of(named.cbegin(), named.cend(), [&reference](const auto& entry) {
                return entry.second == reference->packageId;
            });
        if (!known) {
            named.append({field, reference->packageId});
        }
    };
    for (const BookPlace place : ALL_PLACES) {
        for (const QString& kindName : package.kindsIn(place)) {
            add(bookPlaceName(place), kindName);
        }
    }
    add(QStringLiteral("primary"), package.primaryKind);
    for (const QString& kindName : package.startKinds) {
        add(QStringLiteral("start"), kindName);
    }
    return named;
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

QString KindRef::reference() const {
    return package && kind ? KindReference{package->id, kind->id}.toString() : QString();
}

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
    dropDependents(states);
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

void BookTypeRegistry::dropDependents(QHash<QString, CheckState>& states) {
    // A package that names kinds of another one is checked without waiting for it, so that
    // two packages can name each other's kinds. Dropping a package can take down others, so
    // this goes on until nothing changes.
    for (bool dropped = true; dropped;) {
        dropped = false;
        for (const auto& package : m_packages) {
            if (states.value(package->id) != CheckState::Valid) {
                continue;
            }
            QStringList problems;
            for (const QString& usedId : package->uses) {
                if (states.value(usedId) != CheckState::Valid) {
                    problems << QStringLiteral("uses: package '%1' has problems").arg(usedId);
                }
            }
            for (const auto& [field, packageId] : namedPackages(*package)) {
                if (states.value(packageId) != CheckState::Valid) {
                    problems
                        << QStringLiteral("%1: package '%2' has problems").arg(field, packageId);
                }
            }
            if (!problems.isEmpty()) {
                for (const QString& message : problems) {
                    m_problems.append({package->directory, message});
                }
                states.insert(package->id, CheckState::Invalid);
                dropped = true;
            }
        }
    }
}

QStringList BookTypeRegistry::check(const BookTypePackage& package) const {
    QStringList problems;
    const auto add = [&problems](const QString& field, const QString& message) {
        problems << field + QStringLiteral(": ") + message;
    };
    // Why a named kind was not found: no such kind, or no such package
    const auto notFound = [this](const QString& kindName) {
        const std::optional<KindReference> reference = KindReference::parse(kindName);
        if (reference && !reference->packageId.isEmpty() &&
            !m_packagesById.contains(reference->packageId)) {
            return QStringLiteral("kind '%1' is in package '%2', which is not installed or could "
                                  "not be loaded")
                .arg(kindName, reference->packageId);
        }
        return QStringLiteral("unknown kind '%1'").arg(kindName);
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
        QList<const ElementKind*> listed;
        for (const QString& kindId : package.kindsIn(place)) {
            const KindRef entry = findKind(package, kindId);
            if (!entry) {
                add(field, notFound(kindId));
                continue;
            }
            // "chapter" and "kalahari.base:chapter" can name one kind
            if (listed.contains(entry.kind)) {
                add(field,
                    QStringLiteral("'%1' is a kind that is already in the list").arg(kindId));
                continue;
            }
            listed << entry.kind;
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
            if (entry.kind->form == ElementForm::Group &&
                kindsInside(package, entry.kind->id).isEmpty()) {
                add(field, QStringLiteral("no kind of this package can be inside group '%1'")
                               .arg(kindId));
            }
        }
    }

    // Main text kind
    if (!package.primaryKind.isEmpty()) {
        const KindRef primary = findKind(package, package.primaryKind);
        if (!primary) {
            add(QStringLiteral("primary"), notFound(package.primaryKind));
        } else if (primary.kind->form != ElementForm::Text) {
            add(QStringLiteral("primary"),
                QStringLiteral("'%1' is not a text kind").arg(package.primaryKind));
        } else if (!namesKind(package, package.mainKinds, primary.kind)) {
            add(QStringLiteral("primary"),
                QStringLiteral("'%1' is not in the list of the main part")
                    .arg(package.primaryKind));
        }
    }

    // Elements a new book starts with
    const QStringList listed = listedKinds(package, ALL_PLACES);
    QHash<const ElementKind*, int> counts;
    for (const QString& kindId : package.startKinds) {
        const KindRef start = findKind(package, kindId);
        if (!start) {
            add(QStringLiteral("start"), notFound(kindId));
        } else if (!namesKind(package, listed, start.kind)) {
            add(QStringLiteral("start"),
                QStringLiteral("kind '%1' is in none of the lists front, main, back, workshop")
                    .arg(kindId));
        } else if (start.kind->form == ElementForm::Group) {
            add(QStringLiteral("start"),
                QStringLiteral("'%1' is a group; a group appears with its first element")
                    .arg(kindId));
        } else if (start.kind->limit > 0 && ++counts[start.kind] == start.kind->limit + 1) {
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

KindRef BookTypeRegistry::findKind(const BookTypePackage& package,
                                   const QString& kindName) const {
    const std::optional<KindReference> reference = KindReference::parse(kindName);
    if (!reference) {
        return {};
    }
    const BookTypePackage* owner = &package;
    if (!reference->packageId.isEmpty() && reference->packageId != package.id) {
        owner = m_packagesById.value(reference->packageId);
        if (!owner) {
            return {};
        }
    }
    for (const BookTypePackage* candidate : lineage(*owner)) {
        if (const ElementKind* kind = candidate->ownKind(reference->kindId)) {
            return {candidate, kind};
        }
    }
    return {};
}

bool BookTypeRegistry::namesKind(const BookTypePackage& package, const QStringList& kindNames,
                                 const ElementKind* kind) const {
    return std::any_of(kindNames.cbegin(), kindNames.cend(), [&](const QString& kindName) {
        return findKind(package, kindName).kind == kind;
    });
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
                                         [this, found, &element](BookPlace candidate) {
                                             return namesKind(*found, found->kindsIn(candidate),
                                                              element.kind.kind);
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
        QList<const ElementKind*> order;
        for (const QString& kindName : listedKinds(*package, ALL_PLACES)) {
            const KindRef listed = findKind(*package, kindName);
            if (listed.package == package.get() && !order.contains(listed.kind)) {
                order << listed.kind;
            }
        }
        for (const ElementKind& kind : package->kinds) {
            if (!order.contains(&kind)) {
                order << &kind;
            }
        }
        for (const ElementKind* kind : order) {
            kinds.append({package.get(), kind});
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
