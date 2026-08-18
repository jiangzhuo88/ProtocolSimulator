#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QAction>
#include <QActionGroup>
#include "../core/configmanager.h"
#include "../core/simtcpserver.h"
#include "ZDDSProtolcol.h"
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onSceneManagement();
    void onAddProtocol();
    void onEditProtocol();
    void onDeleteProtocol();
    void onCopyProtocol();
    void onSceneSelectionChanged();
    void onStartService();
    void onStartService(int index);
    void onStopService();
    void onLog(const QString &msg);
    void onProtocolDoubleClicked(int row, int col);
    void onTestBtnClicked();

protected:
    // 运行时语言切换: 安装/卸载QTranslator后收到此事件
    void changeEvent(QEvent *e) override;

private:
    void setupUi();
    void retranslateUi();   // 重设所有已创建界面上的用户可见文本(切语言时会再调)
    void refreshSceneList();
    void refreshProtocolTable();
    void updateServiceStatus();
    void autoSave();
    void autoLoad();
    void controlSceneStatus(QString sceneName, bool openScene);

    ConfigManager m_config;
    SimTcpServer *m_server;
    int m_currentSceneIndex;

    // UI
    QListWidget *m_sceneList;
    QTableWidget *m_protocolTable;
    QTextEdit *m_logEdit;
    QPushButton *m_btnStart;
    QPushButton *m_btnStop;
    QLineEdit   *m_testEdit;
    QCheckBox   *m_testBox;
    QPushButton *m_btnTest;
    QPushButton *m_btnSceneMgmt;
    QPushButton *m_btnAddProto;
    QPushButton *m_btnEditProto;
    QPushButton *m_btnCopyProto;
    QPushButton *m_btnDelProto;
    QLabel      *m_statusLabel;
    QGroupBox   *m_sceneGroup;
    QGroupBox   *m_protoGroup;
    QGroupBox   *m_logGroup;

    // 语言菜单
    QAction     *m_actLangZh;
    QAction     *m_actLangEn;
    QActionGroup *m_langGroup;  // 让中英文选项互斥选中
};

#endif // MAINWINDOW_H

