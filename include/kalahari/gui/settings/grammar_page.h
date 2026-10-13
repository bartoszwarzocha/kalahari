/// @file grammar_page.h
/// @brief Settings > Editor > Grammar: grammar as you type, where the text is checked, the
///        preparation of LanguageTool and the rules ignored
///
/// The coordinator of the grammar (GrammarCoordinator) follows the settings by itself.

#pragma once

#include "kalahari/gui/grammar_components.h"
#include "kalahari/gui/settings/settings_page.h"

#include <QHash>
#include <QString>

class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QToolButton;
class QWidget;

namespace kalahari {
namespace editor {
class GrammarCheckService;
}
namespace gui {

class GrammarCoordinator;

/// @brief Settings > Editor > Grammar
///
/// A card at the top says what grammar as you type can do now, from what the page shows:
/// nothing is prepared yet (with the button that prepares it), the components the program
/// downloaded are gone, the preparation goes on, ready, or checked on a server in the
/// internet. The text goes to a server in the internet only after the writer agreed to it.
///
/// The advanced settings, for LanguageTool installed by the writer, give the java program,
/// the folder of LanguageTool, the port and a server started outside the program; they also
/// tell the state of the server and remove the components the program downloaded.
class EditorGrammarPage : public SettingsPage {
    Q_OBJECT

public:
    /// @brief What the card at the top says
    enum class Card {
        NotPrepared,  ///< Nothing prepared yet
        Missing,      ///< The components set are not there any more
        Preparing,    ///< The preparation goes on
        Ready,        ///< Ready, on this computer
        Remote        ///< On a server in the internet
    };

    /// @param grammar The grammar of the editors: its preparation and its server (none: the
    ///        page cannot prepare it or tell the state of the server)
    /// @param parent Parent widget
    explicit EditorGrammarPage(GrammarCoordinator* grammar = nullptr, QWidget* parent = nullptr);

    /// @brief Write the changed values; the server can be checked with them then
    std::vector<std::string> apply() override;

    /// @brief What the card says now
    [[nodiscard]] Card card() const { return m_card; }

    /// @name The controls, for the tests
    /// @{
    [[nodiscard]] QCheckBox* enabledBox() const { return m_enabled; }
    [[nodiscard]] QRadioButton* localButton() const { return m_local; }
    [[nodiscard]] QRadioButton* remoteButton() const { return m_remote; }
    [[nodiscard]] QPushButton* prepareButton() const { return m_prepare; }
    [[nodiscard]] QLabel* cardTitle() const { return m_cardTitle; }
    [[nodiscard]] QLabel* cardText() const { return m_cardText; }
    [[nodiscard]] QGroupBox* remoteGroup() const { return m_remoteGroup; }
    [[nodiscard]] QComboBox* serverBox() const { return m_server; }
    [[nodiscard]] QLineEdit* usernameField() const { return m_username; }
    [[nodiscard]] QLineEdit* apiKeyField() const { return m_apiKey; }
    [[nodiscard]] QPushButton* checkRemoteButton() const { return m_checkRemote; }
    [[nodiscard]] QLabel* remoteState() const { return m_remoteState; }
    [[nodiscard]] QListWidget* ignoredRulesList() const { return m_rules; }
    [[nodiscard]] QPushButton* restoreButton() const { return m_restore; }
    [[nodiscard]] QPushButton* restoreAllButton() const { return m_restoreAll; }
    [[nodiscard]] QToolButton* advancedToggle() const { return m_advancedToggle; }
    [[nodiscard]] QWidget* advancedSettings() const { return m_advanced; }
    [[nodiscard]] QLineEdit* javaField() const { return m_java; }
    [[nodiscard]] QLabel* javaStatus() const { return m_javaStatus; }
    [[nodiscard]] QLineEdit* folderField() const { return m_folder; }
    [[nodiscard]] QLabel* folderStatus() const { return m_folderStatus; }
    [[nodiscard]] QSpinBox* portBox() const { return m_port; }
    [[nodiscard]] QCheckBox* externalBox() const { return m_external; }
    [[nodiscard]] QLabel* serverState() const { return m_serverState; }
    [[nodiscard]] QPushButton* checkServerButton() const { return m_checkServer; }
    [[nodiscard]] QPushButton* removeButton() const { return m_remove; }
    /// @}

    /// @brief Choose where to check without the question of the consent (the tests); in the
    ///        program the writer agrees to the internet in a question
    void setRemoteWithoutAsking(bool remote);

private:
    /// @brief The group of the checking: the switch, the card and where to check
    void createChecking();

    /// @brief The group of the server in the internet
    void createRemote();

    /// @brief The group of the rules ignored
    void createIgnoredRules();

    /// @brief The advanced settings, under their button
    void createAdvanced();

    /// @brief The writer chose the server in the internet: ask whether to send the text
    void askForRemote();

    /// @brief The card, the switch and the groups for what the page shows
    void updateCard();

    /// @brief What the java program in the field is
    void updateJavaStatus();

    /// @brief What the folder in the field is
    void updateFolderStatus();

    /// @brief The state of the server on this computer
    void updateServerState();

    /// @brief The buttons of the rules ignored
    void updateRuleButtons();

    /// @brief The removal of the components downloaded: shown when there are any, with
    ///        how much they take
    void updateRemoveRow();

    /// @brief Ask the server in the internet to check a text, and say how it went
    void checkRemote();

    /// @brief Start the server on this computer again, with the settings applied
    void checkServer();

    /// @brief Show the preparation (its window)
    void prepare();

    /// @brief Remove the components the program downloaded, after a question
    void removeDownloaded();

    /// @brief Show again the settings the preparation or the removal changed
    void reloadPrepared();

    /// @brief What a java program is, found once per path
    GrammarComponents::JavaInfo javaInfo(const QString& java);

    /// @brief What a folder of LanguageTool is, found once per path
    GrammarComponents::LanguageToolInfo languageToolInfo(const QString& folder);

    /// @brief Whether the settings of the server on this computer differ from those applied
    bool serverSettingsChanged() const;

    GrammarCoordinator* m_grammar;
    Card m_card{Card::NotPrepared};
    bool m_asking{false};  ///< The radio buttons are set by the page, not by the writer

    // The checking
    QCheckBox* m_enabled{nullptr};
    QFrame* m_cardFrame{nullptr};
    QLabel* m_cardIcon{nullptr};
    QLabel* m_cardTitle{nullptr};
    QLabel* m_cardText{nullptr};
    QLabel* m_cardNote{nullptr};
    QProgressBar* m_cardProgress{nullptr};
    QPushButton* m_prepare{nullptr};
    QPushButton* m_showProgress{nullptr};
    QRadioButton* m_local{nullptr};
    QRadioButton* m_remote{nullptr};

    // The server in the internet
    QGroupBox* m_remoteGroup{nullptr};
    QComboBox* m_server{nullptr};
    QLineEdit* m_username{nullptr};
    QLineEdit* m_apiKey{nullptr};
    QPushButton* m_checkRemote{nullptr};
    QLabel* m_remoteState{nullptr};
    editor::GrammarCheckService* m_tester{nullptr};  ///< Checks the text of the test

    // The rules ignored
    QListWidget* m_rules{nullptr};
    QLabel* m_noRules{nullptr};
    QPushButton* m_restore{nullptr};
    QPushButton* m_restoreAll{nullptr};

    // The advanced settings
    QToolButton* m_advancedToggle{nullptr};
    QWidget* m_advancedRow{nullptr};
    QWidget* m_advanced{nullptr};
    QLineEdit* m_java{nullptr};
    QLabel* m_javaStatus{nullptr};
    QLineEdit* m_folder{nullptr};
    QLabel* m_folderStatus{nullptr};
    QSpinBox* m_port{nullptr};
    QCheckBox* m_external{nullptr};
    QLabel* m_serverState{nullptr};
    QPushButton* m_checkServer{nullptr};
    QPushButton* m_serverDetails{nullptr};
    QWidget* m_removeRow{nullptr};
    QPushButton* m_remove{nullptr};
    QLabel* m_removeNote{nullptr};

    // The bindings shown again after the preparation (references into a deque stay valid)
    Binding* m_enabledBinding{nullptr};
    Binding* m_modeBinding{nullptr};
    Binding* m_javaBinding{nullptr};
    Binding* m_folderBinding{nullptr};
    Binding* m_portBinding{nullptr};
    Binding* m_externalBinding{nullptr};

    QHash<QString, GrammarComponents::JavaInfo> m_javaInfos;  ///< By path
    QHash<QString, GrammarComponents::LanguageToolInfo> m_languageToolInfos;  ///< By path
};

}  // namespace gui
}  // namespace kalahari
