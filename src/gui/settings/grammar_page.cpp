/// @file grammar_page.cpp
/// @brief Settings page: Editor > Grammar
///
/// The coordinator of the grammar (GrammarCoordinator) follows the settings by itself.

#include "kalahari/gui/settings/grammar_page.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/grammar_setup.h"
#include "kalahari/gui/language_tool_server.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <initializer_list>
#include <string>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

constexpr int ICON_SIZE = 16;
constexpr int MAX_PORT = 65535;
constexpr int RULE_ROWS = 6;  ///< The rows the list of the rules shows

/// @brief Where the writer gets Java and LanguageTool to install them alone
constexpr const char* TEMURIN_PAGE = "https://adoptium.net/temurin/releases/";
constexpr const char* LANGUAGE_TOOL_PAGE = "https://languagetool.org/download/";

/// @brief Put a note under a check box or a radio button in line with its text
void alignWithText(QLabel* note, const QAbstractButton* button) {
    const QStyle* style = button->style();
    const bool radio = qobject_cast<const QRadioButton*>(button) != nullptr;
    const int indicator =
        style->pixelMetric(radio ? QStyle::PM_ExclusiveIndicatorWidth : QStyle::PM_IndicatorWidth,
                           nullptr, button);
    const int spacing = style->pixelMetric(
        radio ? QStyle::PM_RadioButtonLabelSpacing : QStyle::PM_CheckBoxLabelSpacing, nullptr,
        button);
    note->setContentsMargins(indicator + spacing, 0, 0, 0);
}

/// @brief A label of plain text that wraps
QLabel* textLabel(const QString& text = QString()) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

/// @brief Put a small icon into a label
void setIcon(QLabel* label, const QString& iconId) {
    label->setPixmap(
        core::ArtProvider::getInstance().getIcon(iconId).pixmap(QSize(ICON_SIZE, ICON_SIZE)));
}

/// @brief A field with a button at its right, and a line of its state under them
QWidget* fieldWithButton(QWidget* field, QPushButton* button, QLabel* status = nullptr) {
    auto* widget = new QWidget();
    auto* column = new QVBoxLayout(widget);
    column->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout();
    row->addWidget(field, 1);
    row->addWidget(button);
    column->addLayout(row);
    if (status != nullptr) {
        column->addWidget(status);
    }
    return widget;
}

/// @brief The server a box of servers gives: the address of an item, or the text typed
QString serverOf(const QComboBox* box) {
    const QString text = box->currentText().trimmed();
    const int index = box->findText(text);
    return index >= 0 ? box->itemData(index).toString() : text;
}

/// @brief The rules ignored as a setting value: what they look for, by rule
json rulesValue(const QMap<QString, QString>& rules) {
    json value = json::object();
    for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
        value[it.key().toStdString()] = it.value().toStdString();
    }
    return value;
}

/// @brief The rules ignored from a setting value
QMap<QString, QString> rulesOf(const json& value) {
    QMap<QString, QString> rules;
    if (!value.is_object()) {
        return rules;
    }
    for (const auto& item : value.items()) {
        rules.insert(QString::fromStdString(item.key()),
                     item.value().is_string()
                         ? QString::fromStdString(item.value().get<std::string>())
                         : QString());
    }
    return rules;
}

/// @brief A Java as the writer reads it: Java 21 (Eclipse Temurin)
QString javaText(const GrammarComponents::JavaInfo& java) {
    if (!java.found) {
        return EditorGrammarPage::tr("Java");
    }
    if (java.vendor.isEmpty()) {
        return EditorGrammarPage::tr("Java %1").arg(java.feature);
    }
    return EditorGrammarPage::tr("Java %1 (%2)").arg(java.feature).arg(java.vendor);
}

/// @brief A LanguageTool as the writer reads it: LanguageTool 6.6
QString languageToolText(const GrammarComponents::LanguageToolInfo& languageTool) {
    return languageTool.version.isEmpty()
               ? EditorGrammarPage::tr("LanguageTool")
               : EditorGrammarPage::tr("LanguageTool %1").arg(languageTool.version);
}

}  // namespace

EditorGrammarPage::EditorGrammarPage(GrammarCoordinator* grammar, QWidget* parent)
    : SettingsPage(parent), m_grammar(grammar) {
    addNote(pageLayout(),
            tr("The program can check grammar, style and punctuation - in the language of the "
               "book, like the spelling. LanguageTool does it, a free program that runs on this "
               "computer."));
    createChecking();
    createRemote();
    createIgnoredRules();
    createAdvanced();
    pageLayout()->addStretch();

    if (m_grammar != nullptr) {
        connect(m_grammar->setup(), &GrammarSetup::progress, this,
                &EditorGrammarPage::updateCard);
        connect(m_grammar->setup(), &GrammarSetup::finished, this,
                &EditorGrammarPage::reloadPrepared);
        connect(m_grammar->server(), &LanguageToolServer::stateChanged, this,
                &EditorGrammarPage::updateServerState);
    }
    whenLoaded([this]() {
        updateJavaStatus();
        updateFolderStatus();
        updateRuleButtons();
        updateRemoveRow();
        updateServerState();
        updateCard();
    });
}

std::vector<std::string> EditorGrammarPage::apply() {
    std::vector<std::string> written = SettingsPage::apply();
    updateServerState();
    return written;
}

void EditorGrammarPage::setRemoteWithoutAsking(bool remote) {
    const bool asking = std::exchange(m_asking, true);
    (remote ? m_remote : m_local)->setChecked(true);
    m_asking = asking;
}

// =============================================================================
// The groups
// =============================================================================

void EditorGrammarPage::createChecking() {
    QFormLayout* form = addGroup(tr("Grammar as You Type"));
    m_enabled = addCheckBox(form, tr("Check &grammar as you type"),
                            GrammarCoordinator::ENABLED_KEY);
    m_enabledBinding = &lastBinding();

    // The card: what grammar as you type can do now
    m_cardFrame = new QFrame();
    m_cardFrame->setObjectName(QStringLiteral("grammarCard"));
    m_cardFrame->setFrameShape(QFrame::StyledPanel);
    auto* card = new QVBoxLayout(m_cardFrame);
    auto* titleRow = new QHBoxLayout();
    m_cardIcon = new QLabel();
    m_cardIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_cardTitle = textLabel();
    QFont bold = m_cardTitle->font();
    bold.setBold(true);
    m_cardTitle->setFont(bold);
    titleRow->addWidget(m_cardIcon, 0, Qt::AlignTop);
    titleRow->addWidget(m_cardTitle, 1);
    card->addLayout(titleRow);
    m_cardText = textLabel();
    card->addWidget(m_cardText);
    m_cardNote = addNote(card, QString());
    m_cardProgress = new QProgressBar();
    m_cardProgress->setRange(0, 100);
    m_cardProgress->setTextVisible(false);
    m_cardProgress->setAccessibleName(tr("Preparation of grammar checking"));
    card->addWidget(m_cardProgress);
    auto* buttons = new QHBoxLayout();
    m_prepare = new QPushButton(tr("&Prepare Grammar Checking..."));
    m_showProgress = new QPushButton(tr("&Show Progress"));
    buttons->addWidget(m_prepare);
    buttons->addWidget(m_showProgress);
    buttons->addStretch();
    card->addLayout(buttons);
    form->addRow(m_cardFrame);
    connect(m_prepare, &QPushButton::clicked, this, &EditorGrammarPage::prepare);
    connect(m_showProgress, &QPushButton::clicked, this, &EditorGrammarPage::prepare);

    // Where the text is checked
    m_local = new QRadioButton(tr("On this comp&uter (recommended)"));
    m_remote = new QRadioButton(tr("On a server in the &internet"));
    auto* where = new QWidget();
    auto* whereLayout = new QVBoxLayout(where);
    whereLayout->setContentsMargins(0, 0, 0, 0);
    whereLayout->addWidget(m_local);
    alignWithText(addNote(whereLayout, tr("The text of the book does not leave the computer.")),
                  m_local);
    whereLayout->addWidget(m_remote);
    alignWithText(addNote(whereLayout, tr("The text is sent through the internet.")), m_remote);
    auto* whereLabel = new QLabel(tr("Where to check:"));
    whereLabel->setBuddy(m_local);
    form->addRow(whereLabel, where);
    // The question comes after the click, not inside the switch of the buttons
    connect(m_remote, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked && !m_asking) {
            QMetaObject::invokeMethod(this, "askForRemote", Qt::QueuedConnection);
        }
    });
    connect(m_local, &QRadioButton::toggled, this, [this]() {
        updateCard();
        updateServerState();
    });

    auto& settings = core::SettingsManager::getInstance();
    Binding mode;
    mode.key = []() { return std::string(GrammarCoordinator::MODE_KEY); };
    mode.stored = [&settings]() {
        return json(settings.get<std::string>(GrammarCoordinator::MODE_KEY,
                                              GrammarCoordinator::LOCAL_MODE));
    };
    mode.shown = [this]() {
        return json(m_remote->isChecked() ? GrammarCoordinator::REMOTE_MODE
                                          : GrammarCoordinator::LOCAL_MODE);
    };
    mode.show = [this](const json& value) {
        setRemoteWithoutAsking(value.is_string() &&
                               value.get<std::string>() == GrammarCoordinator::REMOTE_MODE);
    };
    mode.store = [&settings](const json& value) {
        settings.set<std::string>(GrammarCoordinator::MODE_KEY,
                                  value.is_string() ? value.get<std::string>()
                                                    : std::string(GrammarCoordinator::LOCAL_MODE));
    };
    m_modeBinding = &bindCustom(std::move(mode));
}

void EditorGrammarPage::createRemote() {
    QFormLayout* form = addGroup(tr("Server in the Internet"));
    m_remoteGroup = qobject_cast<QGroupBox*>(form->parentWidget());

    auto* warning = new QWidget();
    auto* warningRow = new QHBoxLayout(warning);
    warningRow->setContentsMargins(0, 0, 0, 0);
    auto* warningIcon = new QLabel();
    setIcon(warningIcon, QStringLiteral("common.warning"));
    warningRow->addWidget(warningIcon, 0, Qt::AlignTop);
    warningRow->addWidget(
        textLabel(tr("The text of the paragraphs checked is sent through the internet to this "
                     "server. The free LanguageTool server limits the number and the length of "
                     "the requests.")),
        1);
    form->addRow(warning);

    m_server = new QComboBox();
    m_server->setEditable(true);
    m_server->setInsertPolicy(QComboBox::NoInsert);
    m_server->addItem(tr("api.languagetool.org (free)"),
                      QString::fromLatin1(GrammarCoordinator::FREE_SERVER));
    m_server->addItem(tr("api.languagetoolplus.com (Premium account)"),
                      QString::fromLatin1(GrammarCoordinator::PREMIUM_SERVER));
    m_checkRemote = new QPushButton(tr("C&heck"));
    auto* serverLabel = new QLabel(tr("Ser&ver:"));
    serverLabel->setBuddy(m_server);
    form->addRow(serverLabel, fieldWithButton(m_server, m_checkRemote));
    m_remoteState = textLabel();
    form->addRow(QString(), m_remoteState);
    addNote(form, tr("You can also type the address of another LanguageTool server."));

    auto& settings = core::SettingsManager::getInstance();
    Binding server;
    server.key = []() { return std::string(GrammarCoordinator::SERVER_KEY); };
    server.stored = [&settings]() {
        return json(settings.get<std::string>(GrammarCoordinator::SERVER_KEY,
                                              GrammarCoordinator::FREE_SERVER));
    };
    server.shown = [this]() { return json(serverOf(m_server).toStdString()); };
    server.show = [this](const json& value) {
        const QString address = QString::fromStdString(
            value.is_string() ? value.get<std::string>() : std::string());
        const int index = m_server->findData(address);
        if (index >= 0) {
            m_server->setCurrentIndex(index);
        } else {
            m_server->setEditText(address);
        }
    };
    server.store = [&settings](const json& value) {
        settings.set<std::string>(GrammarCoordinator::SERVER_KEY,
                                  value.is_string() ? value.get<std::string>() : std::string());
    };
    bindCustom(std::move(server));

    m_username = new QLineEdit();
    m_username->setPlaceholderText(tr("only with a Premium account (optional)"));
    addField(form, tr("Account &e-mail:"), m_username, GrammarCoordinator::USERNAME_KEY);
    lastBinding().shown = [this]() { return json(m_username->text().trimmed().toStdString()); };
    m_apiKey = new QLineEdit();
    m_apiKey->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_apiKey->setPlaceholderText(tr("only with a Premium account (optional)"));
    addField(form, tr("API &key:"), m_apiKey, GrammarCoordinator::API_KEY_KEY);
    lastBinding().shown = [this]() { return json(m_apiKey->text().trimmed().toStdString()); };

    // A test asked for before is dropped when what it tested changes
    const auto dropTest = [this]() {
        delete m_tester;
        m_tester = nullptr;
        m_remoteState->clear();
    };
    connect(m_checkRemote, &QPushButton::clicked, this, &EditorGrammarPage::checkRemote);
    connect(m_server, &QComboBox::currentTextChanged, this, [this, dropTest]() {
        dropTest();
        updateCard();
    });
    connect(m_username, &QLineEdit::textChanged, this, dropTest);
    connect(m_apiKey, &QLineEdit::textChanged, this, dropTest);
}

void EditorGrammarPage::createIgnoredRules() {
    QFormLayout* form = addGroup(tr("Ignored Rules"));
    m_rules = new QListWidget();
    m_rules->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_rules->setSortingEnabled(true);
    m_rules->setAccessibleName(tr("Ignored rules"));
    m_rules->setMaximumHeight(m_rules->fontMetrics().lineSpacing() * RULE_ROWS +
                              2 * m_rules->frameWidth());
    m_noRules = textLabel(tr("No rule is ignored."));
    m_restore = new QPushButton(tr("&Restore"));
    m_restoreAll = new QPushButton(tr("Restore &All"));
    auto* buttons = new QHBoxLayout();
    buttons->addWidget(m_restore);
    buttons->addWidget(m_restoreAll);
    buttons->addStretch();
    auto* rulesBox = new QWidget();
    auto* column = new QVBoxLayout(rulesBox);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(m_noRules);
    column->addWidget(m_rules);
    column->addLayout(buttons);
    form->addRow(rulesBox);
    addNote(form, tr("A rule ignored with \"Ignore This Rule\" in the context menu is not "
                     "checked in any book. A restored rule is checked again after OK or "
                     "Apply."));

    connect(m_rules, &QListWidget::itemSelectionChanged, this,
            &EditorGrammarPage::updateRuleButtons);
    connect(m_restore, &QPushButton::clicked, this, [this]() {
        qDeleteAll(m_rules->selectedItems());
        updateRuleButtons();
    });
    connect(m_restoreAll, &QPushButton::clicked, this, [this]() {
        m_rules->clear();
        updateRuleButtons();
    });

    Binding ignored;
    ignored.key = []() { return std::string(GrammarCoordinator::IGNORED_RULES_KEY); };
    ignored.stored = []() { return rulesValue(GrammarCoordinator::ignoredRules()); };
    ignored.shown = [this]() {
        QMap<QString, QString> listed;
        for (int row = 0; row < m_rules->count(); ++row) {
            const QListWidgetItem* item = m_rules->item(row);
            listed.insert(item->data(Qt::UserRole).toString(),
                          item->data(Qt::UserRole + 1).toString());
        }
        return rulesValue(listed);
    };
    ignored.show = [this](const json& value) {
        m_rules->clear();
        const QMap<QString, QString> rules = rulesOf(value);
        for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
            auto* item =
                new QListWidgetItem(it.value().isEmpty() ? it.key() : it.value(), m_rules);
            item->setData(Qt::UserRole, it.key());
            item->setData(Qt::UserRole + 1, it.value());
            item->setToolTip(it.key());
        }
        updateRuleButtons();
    };
    ignored.store = [](const json& value) {
        GrammarCoordinator::setIgnoredRules(rulesOf(value));
    };
    bindCustom(std::move(ignored));
}

void EditorGrammarPage::createAdvanced() {
    // The button that shows them, with whom they are for
    m_advancedRow = new QWidget();
    auto* toggleRow = new QHBoxLayout(m_advancedRow);
    toggleRow->setContentsMargins(0, 0, 0, 0);
    m_advancedToggle = new QToolButton();
    m_advancedToggle->setText(tr("Ad&vanced Settings"));
    m_advancedToggle->setCheckable(true);
    m_advancedToggle->setAutoRaise(true);
    m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_advancedToggle->setArrowType(Qt::RightArrow);
    toggleRow->addWidget(m_advancedToggle);
    QLabel* forWhom = addNote(
        toggleRow,
        tr("for those who installed LanguageTool themselves or run their own server"));
    toggleRow->setStretchFactor(forWhom, 1);
    pageLayout()->addWidget(m_advancedRow);

    auto* group = new QGroupBox();
    m_advanced = group;
    auto* form = new QFormLayout(group);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    pageLayout()->addWidget(group);
    group->hide();
    addNote(form, tr("The button \"Prepare Grammar Checking...\" fills these fields itself. "
                     "Change them only when LanguageTool is installed another way."));

    // The java program
    m_java = new QLineEdit();
    m_java->setAccessibleName(tr("Java"));
    auto* browseJava = new QPushButton(tr("&Browse..."));
    browseJava->setAccessibleName(tr("Browse for Java"));
    m_javaStatus = textLabel();
    auto* javaLabel = new QLabel(tr("&Java:"));
    javaLabel->setBuddy(m_java);
    form->addRow(javaLabel, fieldWithButton(m_java, browseJava, m_javaStatus));
    m_javaBinding = &bind(m_java, GrammarCoordinator::JAVA_KEY);
    m_javaBinding->shown = [this]() { return json(m_java->text().trimmed().toStdString()); };
    connect(browseJava, &QPushButton::clicked, this, [this]() {
        const QString current = m_java->text().trimmed();
        const QString start = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
#ifdef Q_OS_WIN
        const QString filter = tr("Java (java.exe javaw.exe)");
#else
        const QString filter = tr("Java (java)");
#endif
        const QString java = QFileDialog::getOpenFileName(this, tr("Choose Java"), start, filter);
        if (!java.isEmpty()) {
            m_java->setText(QDir::toNativeSeparators(java));
            updateJavaStatus();
        }
    });

    // The folder of LanguageTool
    m_folder = new QLineEdit();
    m_folder->setAccessibleName(tr("LanguageTool folder"));
    auto* browseFolder = new QPushButton(tr("Br&owse..."));
    browseFolder->setAccessibleName(tr("Browse for the folder of LanguageTool"));
    m_folderStatus = textLabel();
    auto* folderLabel = new QLabel(tr("LanguageTool &folder:"));
    folderLabel->setBuddy(m_folder);
    form->addRow(folderLabel, fieldWithButton(m_folder, browseFolder, m_folderStatus));
    m_folderBinding = &bind(m_folder, GrammarCoordinator::FOLDER_KEY);
    m_folderBinding->shown = [this]() {
        return json(m_folder->text().trimmed().toStdString());
    };
    connect(browseFolder, &QPushButton::clicked, this, [this]() {
        const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Choose the Folder of LanguageTool"), m_folder->text().trimmed());
        if (!folder.isEmpty()) {
            m_folder->setText(QDir::toNativeSeparators(folder));
            updateFolderStatus();
        }
    });

    for (QLineEdit* field : {m_java, m_folder}) {
        connect(field, &QLineEdit::textChanged, this, [this]() {
            updateCard();
            updateServerState();
        });
    }
    connect(m_java, &QLineEdit::editingFinished, this, &EditorGrammarPage::updateJavaStatus);
    connect(m_folder, &QLineEdit::editingFinished, this,
            &EditorGrammarPage::updateFolderStatus);

    // The port and a server started outside the program
    m_port = new QSpinBox();
    m_port->setRange(1, MAX_PORT);
    addField(form, tr("Por&t:"), m_port, GrammarCoordinator::PORT_KEY);
    m_portBinding = &lastBinding();
    connect(m_port, qOverload<int>(&QSpinBox::valueChanged), this,
            &EditorGrammarPage::updateServerState);
    m_external = addCheckBox(form, tr("Co&nnect to a server started outside the program"),
                             GrammarCoordinator::EXTERNAL_KEY);
    m_externalBinding = &lastBinding();
    alignWithText(addNote(form, tr("The program does not start or close such a server.")),
                  m_external);
    connect(m_external, &QCheckBox::toggled, this, [this]() {
        updateCard();
        updateServerState();
    });

    // The state of the server
    m_serverState = textLabel();
    m_checkServer = new QPushButton(tr("Chec&k"));
    m_serverDetails = new QPushButton(tr("&Details"));
    auto* state = new QWidget();
    auto* stateRow = new QHBoxLayout(state);
    stateRow->setContentsMargins(0, 0, 0, 0);
    stateRow->addWidget(m_serverState, 1);
    stateRow->addWidget(m_checkServer);
    stateRow->addWidget(m_serverDetails);
    form->addRow(tr("Server state:"), state);
    connect(m_checkServer, &QPushButton::clicked, this, &EditorGrammarPage::checkServer);
    connect(m_serverDetails, &QPushButton::clicked, this, [this]() {
        if (m_grammar != nullptr) {
            dialogs::MessageDialog::information(this, tr("LanguageTool Server"),
                                                m_serverState->text(),
                                                m_grammar->server()->details());
        }
    });

    // The removal of the components downloaded
    m_removeRow = new QWidget();
    auto* removeLayout = new QHBoxLayout(m_removeRow);
    removeLayout->setContentsMargins(0, 0, 0, 0);
    m_remove = new QPushButton(tr("Re&move Downloaded Components..."));
    removeLayout->addWidget(m_remove);
    m_removeNote = addNote(removeLayout, QString());
    removeLayout->setStretchFactor(m_removeNote, 1);
    form->addRow(m_removeRow);
    connect(m_remove, &QPushButton::clicked, this, &EditorGrammarPage::removeDownloaded);

    // Installing it alone
    addNote(form, tr("Installing it yourself, e.g. on a computer without the internet: install "
                     "Java %1 or newer, unpack the file LanguageTool-stable.zip and choose both "
                     "above.")
                      .arg(GrammarComponents::MIN_JAVA_VERSION));
    auto* links = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>&nbsp;&nbsp;&nbsp;"
                                            "<a href=\"%3\">%4</a>")
                                 .arg(QString::fromLatin1(TEMURIN_PAGE),
                                      tr("Eclipse Temurin website").toHtmlEscaped(),
                                      QString::fromLatin1(LANGUAGE_TOOL_PAGE),
                                      tr("LanguageTool download page").toHtmlEscaped()));
    links->setTextFormat(Qt::RichText);
    links->setTextInteractionFlags(Qt::TextBrowserInteraction);
    links->setOpenExternalLinks(true);
    form->addRow(links);

    connect(m_advancedToggle, &QToolButton::toggled, this, [this](bool open) {
        m_advancedToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        m_advanced->setVisible(open && !m_remote->isChecked());
    });
}

// =============================================================================
// What the page shows
// =============================================================================

void EditorGrammarPage::updateCard() {
    const bool remote = m_remote->isChecked();
    const bool external = m_external->isChecked();
    const GrammarSetup* setup = m_grammar != nullptr ? m_grammar->setup() : nullptr;
    const bool preparing = setup != nullptr && setup->isRunning();

    if (preparing && !remote) {
        m_card = Card::Preparing;
    } else {
        switch (GrammarCoordinator::readinessOf(remote, external, m_java->text(),
                                                m_folder->text())) {
        case GrammarCoordinator::Readiness::NotPrepared:
            m_card = Card::NotPrepared;
            break;
        case GrammarCoordinator::Readiness::Missing:
            m_card = Card::Missing;
            break;
        case GrammarCoordinator::Readiness::Ready:
            m_card = Card::Ready;
            break;
        case GrammarCoordinator::Readiness::Remote:
            m_card = Card::Remote;
            break;
        }
    }

    QString icon;
    QString title;
    QString text;
    QString note;
    bool busy = false;
    switch (m_card) {
    case Card::NotPrepared:
        icon = QStringLiteral("help.about");
        title = tr("Grammar checking is not prepared yet");
        text = tr("It needs two free components: LanguageTool and Java. The program downloads "
                  "and prepares them itself - just click the button. About %1 to download.")
                   .arg(GrammarComponents::downloadSizeText());
        if (GrammarComponents::javaDownloaded() && !GrammarComponents::languageToolDownloaded()) {
            note = tr("Java is downloaded already - LanguageTool is left to download (%1).")
                       .arg(GrammarComponents::sizeText(
                           GrammarComponents::LANGUAGE_TOOL_DOWNLOAD_BYTES));
        }
        break;
    case Card::Missing:
        icon = QStringLiteral("common.warning");
        title = tr("LanguageTool or Java was not found");
        text = tr("The files of LanguageTool or Java chosen for grammar checking were removed or "
                  "moved. Prepare grammar checking again, or choose them in the advanced "
                  "settings.");
        break;
    case Card::Preparing:
        icon = QStringLiteral("common.arrowForward");
        title = tr("Grammar checking is being prepared");
        switch (setup->step()) {
        case GrammarSetup::Step::Java:
            text = tr("Downloading Java - %1%").arg(setup->percent());
            break;
        case GrammarSetup::Step::LanguageTool:
            text = tr("Downloading LanguageTool - %1%").arg(setup->percent());
            break;
        case GrammarSetup::Step::Unpack:
            text = tr("Unpacking and checking the files");
            busy = true;
            break;
        case GrammarSetup::Step::FirstRun:
            text = tr("First start and a trial check");
            busy = true;
            break;
        }
        break;
    case Card::Ready:
        icon = QStringLiteral("common.checkCircle");
        title = tr("Grammar checking is ready");
        if (external) {
            text = tr("It connects to the LanguageTool server started outside the program, on "
                      "this computer - the text of the book does not leave the computer.");
        } else {
            text = tr("It works on this computer - the text of the book does not leave the "
                      "computer.");
            note = tr("%1 - %2", "LanguageTool 6.6 - Java 21 (Eclipse Temurin)")
                       .arg(languageToolText(languageToolInfo(m_folder->text().trimmed())),
                            javaText(javaInfo(m_java->text().trimmed())));
        }
        break;
    case Card::Remote: {
        icon = QStringLiteral("common.warning");
        title = tr("The grammar is checked on a server in the internet");
        const QString server = serverOf(m_server);
        const QString host = QUrl::fromUserInput(server).host();
        text = tr("The paragraphs checked are sent to the server %1.")
                   .arg(host.isEmpty() ? server : host);
        break;
    }
    }

    setIcon(m_cardIcon, icon);
    m_cardTitle->setText(title);
    m_cardText->setText(text);
    m_cardNote->setText(note);
    m_cardNote->setVisible(!note.isEmpty());
    m_cardProgress->setVisible(m_card == Card::Preparing);
    if (busy) {
        m_cardProgress->setRange(0, 0);
    } else {
        m_cardProgress->setRange(0, 100);
        m_cardProgress->setValue(setup != nullptr ? setup->percent() : 0);
    }
    m_prepare->setVisible(m_grammar != nullptr &&
                          (m_card == Card::NotPrepared || m_card == Card::Missing));
    m_showProgress->setVisible(m_card == Card::Preparing);
    m_enabled->setEnabled(m_card == Card::Ready || m_card == Card::Remote);

    m_remoteGroup->setVisible(remote);
    m_advancedRow->setVisible(!remote);
    m_advanced->setVisible(!remote && m_advancedToggle->isChecked());
    m_remove->setEnabled(!preparing);
}

void EditorGrammarPage::updateJavaStatus() {
    const QString java = m_java->text().trimmed();
    if (java.isEmpty()) {
        m_javaStatus->setText(tr("No Java chosen."));
        return;
    }
    const GrammarComponents::JavaInfo info = javaInfo(java);
    if (!info.found) {
        m_javaStatus->setText(tr("This is not Java of version %1 or newer.")
                                  .arg(GrammarComponents::MIN_JAVA_VERSION));
    } else if (info.feature < GrammarComponents::MIN_JAVA_VERSION) {
        m_javaStatus->setText(tr("Java %1 is too old: LanguageTool needs version %2 or newer.")
                                  .arg(info.feature)
                                  .arg(GrammarComponents::MIN_JAVA_VERSION));
    } else {
        m_javaStatus->setText(javaText(info));
    }
}

void EditorGrammarPage::updateFolderStatus() {
    const QString folder = m_folder->text().trimmed();
    if (folder.isEmpty()) {
        m_folderStatus->setText(tr("No folder chosen."));
        return;
    }
    const GrammarComponents::LanguageToolInfo info = languageToolInfo(folder);
    if (!info.found) {
        m_folderStatus->setText(tr("This folder has no LanguageTool (the file "
                                   "languagetool-server.jar). Choose the folder unpacked from "
                                   "the file LanguageTool-stable.zip."));
    } else {
        m_folderStatus->setText(languageToolText(info));
    }
}

void EditorGrammarPage::updateServerState() {
    if (m_grammar == nullptr) {
        m_serverState->setText(tr("The server does not run."));
        m_checkServer->hide();
        m_serverDetails->hide();
        return;
    }
    const LanguageToolServer* server = m_grammar->server();
    QString state;
    switch (server->state()) {
    case LanguageToolServer::State::Stopped:
        state = tr("The server does not run.");
        break;
    case LanguageToolServer::State::Starting:
        state = tr("Starting the server...");
        break;
    case LanguageToolServer::State::Running:
        state = server->startedByProgram() ? tr("The server runs - the program started it.")
                                           : tr("The server runs - started outside the program.");
        break;
    case LanguageToolServer::State::Failed:
        switch (server->failure()) {
        case LanguageToolServer::Failure::NotRunning:
            state = tr("No server answers on the port %1.").arg(server->port());
            break;
        case LanguageToolServer::Failure::OtherProgram:
            state = tr("Another program, not LanguageTool, answers on the port %1. Choose "
                       "another port.")
                        .arg(server->port());
            break;
        case LanguageToolServer::Failure::Stopped:
            state = tr("The server stopped.");
            break;
        case LanguageToolServer::Failure::NotStarted:
        case LanguageToolServer::Failure::None:
            state = tr("The server did not start.");
            break;
        }
        break;
    }
    m_serverState->setText(state);

    // The server is checked with the settings applied, ready, and while it is not starting
    const bool changed = serverSettingsChanged();
    const bool ready =
        GrammarCoordinator::readinessOf(m_remote->isChecked(), m_external->isChecked(),
                                        m_java->text(), m_folder->text()) ==
        GrammarCoordinator::Readiness::Ready;
    const bool idle = server->state() == LanguageToolServer::State::Stopped ||
                      server->state() == LanguageToolServer::State::Failed;
    m_checkServer->setEnabled(!changed && ready && idle && !m_grammar->setup()->isRunning());
    m_checkServer->setToolTip(changed ? tr("Apply the changes to check the server with them.")
                                      : QString());
    m_serverDetails->setVisible(server->state() == LanguageToolServer::State::Failed &&
                                !server->details().isEmpty());
}

void EditorGrammarPage::updateRuleButtons() {
    const bool any = m_rules->count() > 0;
    m_rules->setVisible(any);
    m_noRules->setVisible(!any);
    m_restore->setEnabled(!m_rules->selectedItems().isEmpty());
    m_restoreAll->setEnabled(any);
}

void EditorGrammarPage::updateRemoveRow() {
    const bool downloaded = m_grammar != nullptr && GrammarComponents::anythingDownloaded();
    m_removeRow->setVisible(downloaded);
    if (downloaded) {
        m_removeNote->setText(
            tr("Removes Java and LanguageTool downloaded by the program (about %1).")
                .arg(GrammarComponents::sizeText(GrammarComponents::downloadedBytes())));
    }
}

bool EditorGrammarPage::serverSettingsChanged() const {
    for (const Binding* binding : {m_modeBinding, m_javaBinding, m_folderBinding, m_portBinding,
                                   m_externalBinding}) {
        if (binding->shown() != binding->loaded) {
            return true;
        }
    }
    return false;
}

GrammarComponents::JavaInfo EditorGrammarPage::javaInfo(const QString& java) {
    // A Java not found is looked for again: it may be installed meanwhile
    const auto known = m_javaInfos.constFind(java);
    if (known != m_javaInfos.constEnd()) {
        return *known;
    }
    const GrammarComponents::JavaInfo info = GrammarComponents::inspectJava(java);
    if (info.found) {
        m_javaInfos.insert(java, info);
    }
    return info;
}

GrammarComponents::LanguageToolInfo EditorGrammarPage::languageToolInfo(const QString& folder) {
    const auto known = m_languageToolInfos.constFind(folder);
    if (known != m_languageToolInfos.constEnd()) {
        return *known;
    }
    const GrammarComponents::LanguageToolInfo info = GrammarComponents::inspectLanguageTool(folder);
    if (info.found) {
        m_languageToolInfos.insert(folder, info);
    }
    return info;
}

// =============================================================================
// What the writer does
// =============================================================================

void EditorGrammarPage::askForRemote() {
    // Gone back to this computer meanwhile, or agreed before (the setting says so)
    if (!m_remote->isChecked() ||
        m_modeBinding->loaded == json(GrammarCoordinator::REMOTE_MODE)) {
        return;
    }
    dialogs::MessageDialog dialog(
        dialogs::MessageDialog::Kind::Question,
        tr("Send the text of the book to a server in the internet?"),
        tr("The paragraphs checked will be sent through the internet to a LanguageTool server "
           "(by default api.languagetool.org). Choose this only if you agree to it.") +
            QStringLiteral("\n\n") + tr("Checking on this computer sends the text nowhere."),
        this);
    dialog.setWindowTitle(tr("Grammar"));
    dialog.setQuestionButtons(tr("&Send Text"));
    dialog.setDefaultButton(dialogs::MessageDialog::DefaultButton::Cancel);
    dialog.exec();
    if (dialog.answer() != dialogs::MessageDialog::Answer::Accept) {
        setRemoteWithoutAsking(false);
        return;
    }
    // Chosen to be checked there: the checking is turned on with it
    m_enabled->setChecked(true);
}

void EditorGrammarPage::checkRemote() {
    // A test asked for before is dropped
    delete m_tester;
    m_tester = new editor::GrammarCheckService(this);
    m_tester->setServer(serverOf(m_server));
    m_tester->setCredentials(m_username->text().trimmed(), m_apiKey->text().trimmed());
    m_tester->setLanguage(SpellingCoordinator::wantedLanguage());
    if (!m_tester->isConfigured()) {
        m_remoteState->setText(tr("This is not the address of a server."));
        return;
    }
    connect(m_tester, &editor::GrammarCheckService::textChecked, this,
            [this]() { m_remoteState->setText(tr("The server checks the text.")); });
    connect(m_tester, &editor::GrammarCheckService::serverError, this,
            [this](const QString& message) {
                switch (m_tester->lastFailure()) {
                case editor::GrammarCheckService::Failure::Connection:
                    m_remoteState->setText(
                        tr("The server did not check the text: it could not be connected to."));
                    break;
                case editor::GrammarCheckService::Failure::Limit:
                    m_remoteState->setText(tr("The server refused the request because the limit "
                                              "was exceeded. Try again in a moment."));
                    break;
                case editor::GrammarCheckService::Failure::Refused:
                case editor::GrammarCheckService::Failure::None:
                    m_remoteState->setText(
                        tr("The server did not check the text: %1").arg(message));
                    break;
                }
            });
    m_remoteState->setText(tr("Connecting..."));
    m_tester->check(QStringLiteral("Test."));
}

void EditorGrammarPage::checkServer() {
    if (m_grammar == nullptr || serverSettingsChanged()) {
        return;
    }
    m_grammar->server()->start();
}

void EditorGrammarPage::prepare() {
    if (m_grammar == nullptr) {
        return;
    }
    m_grammar->showSetup(window());
    updateCard();
}

void EditorGrammarPage::removeDownloaded() {
    if (m_grammar == nullptr) {
        return;
    }
    dialogs::MessageDialog dialog(
        dialogs::MessageDialog::Kind::Warning, tr("Remove the downloaded components?"),
        tr("Java and LanguageTool downloaded by the program will be removed from this computer "
           "(about %1). Grammar checking on this computer stops working until you prepare it "
           "again. Spelling goes on working.")
            .arg(GrammarComponents::sizeText(GrammarComponents::downloadedBytes())),
        this);
    dialog.setWindowTitle(tr("Grammar"));
    dialog.setQuestionButtons(tr("&Remove"));
    dialog.setDefaultButton(dialogs::MessageDialog::DefaultButton::Cancel);
    dialog.exec();
    if (dialog.answer() != dialogs::MessageDialog::Answer::Accept) {
        return;
    }
    if (!m_grammar->removeDownloaded()) {
        dialogs::MessageDialog::warning(
            this, tr("Not Everything Was Removed"),
            tr("Some files in the folder %1 are in use or protected. Close the programs that use "
               "them and try again.")
                .arg(QDir::toNativeSeparators(GrammarComponents::folder())));
    }

    // The coordinator cleared the settings that pointed at them
    m_javaInfos.clear();
    m_languageToolInfos.clear();
    for (Binding* binding : {m_enabledBinding, m_javaBinding, m_folderBinding}) {
        reload(*binding);
    }
    updateJavaStatus();
    updateFolderStatus();
    updateRemoveRow();
    updateServerState();
    updateCard();
}

void EditorGrammarPage::reloadPrepared() {
    // The coordinator wrote the settings of the components prepared before this
    if (m_grammar != nullptr && m_grammar->setup()->state() == GrammarSetup::State::Finished) {
        for (Binding* binding : {m_enabledBinding, m_modeBinding, m_javaBinding, m_folderBinding,
                                 m_portBinding, m_externalBinding}) {
            reload(*binding);
        }
        updateJavaStatus();
        updateFolderStatus();
    }
    updateRemoveRow();
    updateServerState();
    updateCard();
}

}  // namespace gui
}  // namespace kalahari
