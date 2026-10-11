/// @file add_to_book_dialog.cpp
/// @brief Add to Book: a file as a new text element, of the kind, with the title and at the
/// place the writer chooses

#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/standalone_file.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/section_words.h"

#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

/// The kind that the name of file @p path says, as the program names the files of the
/// elements: "chapter" for "chapter_004.kchapter"; empty for another name
QString kindIdOf(const QString& path) {
    static const QRegularExpression numbered(QStringLiteral("^(.+)_\\d+$"));
    const QRegularExpressionMatch match = numbered.match(QFileInfo(path).completeBaseName());
    return match.hasMatch() ? match.captured(1) : QString();
}

/// Index of the first of @p choices that @p fits; -1 when none does
int indexOf(const QList<NewElementChoice>& choices,
            const std::function<bool(const NewElementChoice&)>& fits) {
    const auto found = std::find_if(choices.cbegin(), choices.cend(), fits);
    return found != choices.cend() ? static_cast<int>(found - choices.cbegin()) : -1;
}

} // anonymous namespace

AddToBookDialog::Book AddToBookDialog::openBook(const QString& path) {
    Book book;
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    const core::ProjectBook* projectBook = pm.book();
    if (!project || !projectBook) {
        return book;
    }

    // Each text kind the book has a place for, once, in reading order, with the place it takes
    // in the first part of the book or group that can have it
    const auto addKinds = [&pm, &book](core::BookPlace place, const QString& groupId) {
        for (const core::KindRef& kind : pm.textKindsFor(place, groupId)) {
            const auto same = [&kind](const NewElementChoice& choice) {
                return choice.kind.kind == kind.kind;
            };
            if (std::none_of(book.choices.cbegin(), book.choices.cend(), same)) {
                book.choices.append({kind, QString(), pm.newPlaceOf(kind, place, groupId)});
            }
        }
    };
    const std::function<void(core::BookPlace, const QList<core::ProjectElement>&)> addGroups =
        [&pm, &addKinds, &addGroups](core::BookPlace place,
                                     const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                if (pm.formOf(element) == core::ElementForm::Group) {
                    addKinds(place, element.id);
                    addGroups(place, element.elements);
                }
            }
        };
    for (const core::BookPlace place :
         {core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back}) {
        const QList<core::ProjectElement>& elements = project->elementsIn(place);
        addKinds(place, QString());
        addGroups(place, elements);

        // A book without sections has no rows of its parts
        book.sections.append({place,
                              projectBook->partsLayer ? projectBook->sectionName(place)
                                                      : QString(),
                              SectionWords::forPart(projectBook, place).inPart, elements});
    }

    // The kind the file's name says, else the main texts of the book
    const QString named = kindIdOf(path);
    int current = named.isEmpty() ? -1
                                  : indexOf(book.choices, [&named](const NewElementChoice& choice) {
                                        return choice.kind.kind->id == named;
                                    });
    if (current < 0) {
        const core::KindRef main = pm.mainTextKind();
        current = indexOf(book.choices, [&main](const NewElementChoice& choice) {
            return choice.kind.kind == main.kind;
        });
    }
    book.current = std::max(current, 0);
    return book;
}

AddToBookDialog::AddToBookDialog(const File& file, const Book& book,
                                 const core::BookTypeRegistry& registry, QWidget* parent)
    : KalahariDialog(parent)
    , m_file(file)
    , m_book(book)
    , m_current(book.choices.isEmpty()
                    ? -1
                    : qBound(0, book.current, static_cast<int>(book.choices.size()) - 1))
{
    // What becomes of the file: a file of the book goes back to it, a chapter or a text from
    // outside it becomes a new element
    QString description;
    if (m_file.inBook) {
        description = tr("The file goes back to the book as an element, with the title, the "
                         "status and the notes saved in it.");
    } else if (core::StandaloneFile::typeOf(m_file.path) ==
               core::StandaloneFile::Type::PlainText) {
        description = tr("The text of the file becomes a new element of the book; its lines "
                         "become paragraphs.");
    } else {
        description = tr("The file becomes an element of the book, with the title, the status "
                         "and the notes saved in it.");
    }
    setHeading(tr("Add the File to the Book"), description);
    setWindowTitle(tr("Add to Book"));
    setHeadingIcon(QStringLiteral("book.newItem"));

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // The file; one from outside the book is copied or moved to it, as the writer chooses
    auto* fileField = new QLineEdit(QDir::toNativeSeparators(m_file.path), this);
    fileField->setReadOnly(true);
    fileField->setToolTip(fileField->text());
    fileField->setCursorPosition(static_cast<int>(fileField->text().size()));
    if (m_file.inBook) {
        form->addRow(tr("File:"), fileField);
    } else {
        auto* fileColumn = new QVBoxLayout();
        fileColumn->setSpacing(4);
        fileColumn->addWidget(fileField);
        //: The book gets a chapter of its own; the file the writer chose stays
        m_copyButton = new QRadioButton(tr("&Copy to the book – the original stays where it is"),
                                        this);
        //: The book gets a chapter of its own; the file the writer chose is deleted
        m_moveButton = new QRadioButton(tr("&Move to the book – the original is deleted"), this);
        m_copyButton->setChecked(true);
        fileColumn->addWidget(m_copyButton);
        fileColumn->addWidget(m_moveButton);
        form->addRow(tr("File:"), fileColumn);
    }

    // The kinds, named in the program's language
    m_kindBox = new QComboBox(this);
    const QString language = programLanguage();
    for (const NewElementChoice& choice : std::as_const(m_book.choices)) {
        m_kindBox->addItem(choice.kind ? choice.kind.kind->name.text(language) : QString());
    }
    m_kindBox->setCurrentIndex(m_current);
    m_kindBox->setEnabled(m_book.choices.size() > 1);
    form->addRow(tr("&Kind:"), m_kindBox);

    // The title in the file, else the file's name
    const QString startTitle =
        m_file.title.trimmed().isEmpty() ? QFileInfo(m_file.path).completeBaseName()
                                         : m_file.title.trimmed();
    m_titleEdit = new QLineEdit(startTitle, this);
    form->addRow(tr("&Title:"), m_titleEdit);

    // The status of the file, when it is one the elements can have; else a draft
    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setText(core::ProjectManager::isTextStatus(m_file.status)
                               //: %1: the status, e.g. Draft
                               ? tr("%1 (from the file)").arg(statusName(m_file.status))
                               : statusName(QStringLiteral("draft")));
    form->addRow(tr("Status:"), m_statusLabel);
    contentLayout()->addLayout(form);

    // The place: the book with the element in it
    m_picker = new ElementPlacePicker(this);
    m_picker->setBook(m_book.sections, registry);
    contentLayout()->addWidget(m_picker, 1);

    connect(m_kindBox, &QComboBox::currentIndexChanged, this, &AddToBookDialog::onKindChanged);
    connect(m_titleEdit, &QLineEdit::textChanged, this, [this]() {
        m_picker->setTitle(title());
        updateAcceptButton();
    });
    connect(m_picker, &ElementPlacePicker::placeChanged, this,
            &AddToBookDialog::updateAcceptButton);

    setAcceptText(tr("Add"));
    onKindChanged();

    // Typing replaces the title
    m_titleEdit->selectAll();
    m_titleEdit->setFocus();
}

core::KindRef AddToBookDialog::kind() const
{
    return m_current >= 0 && m_current < m_book.choices.size() ? m_book.choices.at(m_current).kind
                                                               : core::KindRef{};
}

QString AddToBookDialog::title() const
{
    return m_titleEdit->text().trimmed();
}

core::ElementPlace AddToBookDialog::place() const
{
    return m_picker->place();
}

bool AddToBookDialog::copy() const
{
    return !m_copyButton || m_copyButton->isChecked();
}

QString AddToBookDialog::statusName(const QString& status)
{
    if (status == QLatin1String("draft")) {
        return tr("Draft");
    }
    if (status == QLatin1String("revision")) {
        return tr("Revision");
    }
    if (status == QLatin1String("final")) {
        return tr("Final");
    }
    QString name = status;
    if (!name.isEmpty()) {
        name[0] = name[0].toUpper();
    }
    return name;
}

void AddToBookDialog::onKindChanged()
{
    m_current = m_kindBox->currentIndex();
    if (const core::KindRef chosen = kind()) {
        m_picker->setElement(chosen, title());
        m_picker->setPlace(m_book.choices.at(m_current).place);
    }
    updateAcceptButton();
}

void AddToBookDialog::updateAcceptButton()
{
    acceptButton()->setEnabled(!title().isEmpty() && kind() && !m_picker->places().isEmpty());
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
