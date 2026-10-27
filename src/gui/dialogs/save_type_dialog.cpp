/// @file save_type_dialog.cpp
/// @brief Save as Type: the kinds of a book, saved as a book type of the writer's own

#include "kalahari/gui/dialogs/save_type_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"

#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {
namespace dialogs {

namespace {

constexpr int DESCRIPTION_LINES = 3;  ///< Of the field of the description
constexpr int MINIMUM_WIDTH = 520;    ///< A name and a sentence of the summary in a line

/// Icon of the writer's own types, as the New Book window shows them
constexpr const char* CUSTOM_TYPE_ICON = "template.custom";

} // anonymous namespace

SaveTypeDialog::SaveTypeDialog(const core::CustomBookType& type,
                               const core::BookTypeRegistry& registry, QWidget* parent)
    : KalahariDialog(parent)
    , m_type(type)
    , m_registry(registry)
    , m_folder(core::CustomBookTypes::folder())
{
    //: The window that saves the kinds of a book as a type of the writer's own. In Polish:
    //: Zapisz jako typ
    setHeading(tr("Save as Type"),
               //: In Polish: Typ pojawi się w oknie „Nowa książka”.
               tr("The type will be in the New Book window."));
    setHeadingIcon(QString::fromLatin1(CUSTOM_TYPE_ICON));
    setMinimumWidth(MINIMUM_WIDTH);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("saveTypeName"));
    m_nameEdit->setText(type.name);
    //: An example of a name of a type of the writer's own. In Polish: np. Powieść historyczna
    m_nameEdit->setPlaceholderText(tr("e.g. Historical novel"));
    //: In Polish: Nazwa
    addField(tr("Name"), m_nameEdit);

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setObjectName(QStringLiteral("saveTypeDescription"));
    m_descriptionEdit->setPlainText(type.description);
    // Tab goes on to the buttons, as in the other fields
    m_descriptionEdit->setTabChangesFocus(true);
    const QFontMetrics metrics(m_descriptionEdit->font());
    m_descriptionEdit->setFixedHeight(DESCRIPTION_LINES * metrics.lineSpacing() +
                                      2 * m_descriptionEdit->frameWidth() +
                                      static_cast<int>(2 * m_descriptionEdit->document()
                                                               ->documentMargin()));
    //: In Polish: Opis (nieobowiązkowy)
    addField(tr("Description (optional)"), m_descriptionEdit);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("saveTypeSummary"));
    m_summaryLabel->setTextFormat(Qt::RichText);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setText(summary());
    contentLayout()->addWidget(m_summaryLabel);
    contentLayout()->addStretch(1);

    //: The button that saves a type of the writer's own. In Polish: Zapisz
    setAcceptText(tr("Save"));
    const auto updateSave = [this]() {
        acceptButton()->setEnabled(!m_nameEdit->text().trimmed().isEmpty());
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this, updateSave);
    updateSave();
    m_nameEdit->setFocus();
}

void SaveTypeDialog::setFolder(const QString& folder)
{
    m_folder = folder;
}

QString SaveTypeDialog::summary() const
{
    const QString language = programLanguage();
    QString html;
    if (const core::BookTypePackage* package =
            m_type.isUserProject() ? nullptr : m_registry.package(m_type.typeId)) {
        QStringList taken;
        for (const core::KindRef& kind : m_type.kindsIn(m_registry)) {
            taken << QStringLiteral("%1 (%2)").arg(kind.kind->wordsIn(language).form(
                                                       QStringLiteral("singular")),
                                                   kind.package->name.text(language));
        }
        //: The book type of a type of the writer's own. In Polish: Typ:
        const QString label = tr("Type:");
        const QString name = package->name.text(language);
        //: %1: the book type, e.g. Novel. In Polish: %1, bez rodzajów innych typów.
        QString text = tr("%1, without the kinds of other types.").arg(name);
        if (!taken.isEmpty()) {
            //: %1: the book type; %2: the kinds it takes from other types. In Polish: %1,
            //: z rodzajami innych typów: %2.
            text = tr("%1, with the kinds of other types: %2.")
                       .arg(name, taken.join(QStringLiteral(", ")));
        }
        html = QStringLiteral("<b>%1</b> %2").arg(label.toHtmlEscaped(), text.toHtmlEscaped());
    } else {
        //: A type of the writer's own without a book type; %1: the number of its kinds. In
        //: Polish: Bez typu książki (projekt użytkownika), rodzaje: %1.
        html = tr("Without a book type (a user project), kinds: %1.")
                   .arg(m_type.kinds.size())
                   .toHtmlEscaped();
    }
    const QString titles = startTitles(m_type.startElementsIn(m_registry), language);
    //: What a new book of the type starts with. In Polish: Na start:
    const QString startLabel = tr("To start with:");
    //: A new book that starts without elements. In Polish: pusta książka
    const QString start = titles.isEmpty() ? tr("an empty book") : titles;
    html += QStringLiteral("<br/><b>%1</b> %2").arg(startLabel.toHtmlEscaped(),
                                                    start.toHtmlEscaped());
    return html;
}

void SaveTypeDialog::accept()
{
    core::CustomBookType type = m_type;
    type.name = m_nameEdit->text().trimmed();
    type.description = m_descriptionEdit->toPlainText().trimmed();
    if (type.name.isEmpty()) {
        return;
    }

    // A type of the same name is replaced in its file, when the writer agrees
    const QList<core::CustomBookType> types = core::CustomBookTypes::load(m_folder);
    if (const core::CustomBookType* same = core::CustomBookTypes::named(types, type.name);
        same && same->id != type.id) {
        if (!MessageDialog::confirm(this, windowTitle(),
                                    //: %1: the name of a type of the writer's own. In Polish:
                                    //: Typ „%1” już jest. Zastąpić go?
                                    tr("The type \"%1\" is already there. Replace it?")
                                        .arg(same->name),
                                    //: In Polish: &Zastąp
                                    tr("&Replace"), MessageDialog::Kind::Warning)) {
            return;
        }
        type.id = same->id;
    }

    QString problem;
    if (!core::CustomBookTypes::save(type, m_folder, &problem)) {
        MessageDialog::error(this, windowTitle(),
                             //: %1: the name of the type. In Polish: Nie udało się zapisać typu
                             //: „%1”.
                             tr("The type \"%1\" could not be saved.").arg(type.name), problem);
        return;
    }
    m_type = type;
    KalahariDialog::accept();
}

} // namespace dialogs
} // namespace gui
} // namespace kalahari
