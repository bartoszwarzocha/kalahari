/// @file folder_field.h
/// @brief Field of a folder: its path, Choose... and Restore Default

#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

namespace kalahari {
namespace gui {

/// @brief Field of a folder: the path, Choose... (the system window choosing a folder) and
/// Restore Default, with a line under them that says why the program cannot use the folder,
/// or that the program creates the folder when it is first needed
///
/// An empty field means the default folder, which the field shows as its placeholder.
/// Restore Default is available while the field holds another folder.
class FolderField : public QWidget {
    Q_OBJECT

public:
    /// @brief Create the field without a folder
    explicit FolderField(QWidget* parent = nullptr);

    /// @brief Set the folder Restore Default puts in the field, also meant by an empty field
    void setDefaultFolder(const QString& folder);

    /// @brief The folder Restore Default puts in the field
    [[nodiscard]] QString defaultFolder() const { return m_defaultFolder; }

    /// @brief Set the title of the system window choosing the folder
    void setChooseTitle(const QString& title);

    /// @brief The folder of the field the way the settings keep it; the default folder when
    /// the field is empty
    [[nodiscard]] QString folder() const;

    /// @brief Show folder @p folder in the field
    void setFolder(const QString& folder);

    /// @brief Why the program cannot use the folder of the field; empty when it can
    [[nodiscard]] QString problem() const;

    /// @brief The field of the path, the buddy of a label
    [[nodiscard]] QLineEdit* lineEdit() const { return m_edit; }

    /// @brief The line under the field: a problem of the folder, or that it does not exist yet
    [[nodiscard]] QString stateText() const;

signals:
    /// @brief The folder in the field changed: typed, chosen or restored
    void folderChanged();

protected:
    /// @brief A disabled field says nothing about its folder
    void changeEvent(QEvent* event) override;

private:
    /// @brief Choose a folder in the system window, starting at the folder of the field
    void choose();

    /// @brief Make Restore Default available while the field holds another folder
    void updateRestoreButton();

    /// @brief Fill the line under the field
    void showState();

    QLineEdit* m_edit;
    QPushButton* m_chooseButton;
    QPushButton* m_restoreButton;
    QLabel* m_stateIcon;
    QLabel* m_stateText;
    QTimer* m_stateTimer;  ///< The line follows typing after a pause
    QString m_defaultFolder;
    QString m_chooseTitle;
};

}  // namespace gui
}  // namespace kalahari
