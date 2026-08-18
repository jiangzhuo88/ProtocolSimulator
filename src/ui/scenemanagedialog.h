#ifndef SCENEMANAGEDIALOG_H
#define SCENEMANAGEDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include "../core/configmanager.h"

class SceneManageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SceneManageDialog(ConfigManager *config, QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *e) override;

private slots:
    void onAddScene();
    void onRemoveScene();
    void onSceneSelectionChanged();
    void onApplyChanges();

private:
    void setupUi();
    void refreshList();
    void retranslateUi();

    ConfigManager *m_config;
    QListWidget *m_list;
    QLineEdit *m_nameEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_btnAdd;
    QPushButton *m_btnRemove;
    QPushButton *m_btnApply;
    QGroupBox *m_listGroup;
    QGroupBox *m_editGroup;
    QLabel *m_nameLabel;
    QLabel *m_portLabel;
};

#endif // SCENEMANAGEDIALOG_H
