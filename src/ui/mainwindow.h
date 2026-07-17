#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "../core/configmanager.h"
#include "../core/simtcpserver.h"

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
    void onSceneSelectionChanged();
    void onStartService();
    void onStopService();
    void onLog(const QString &msg);
    void onProtocolDoubleClicked(int row, int col);

private:
    void setupUi();
    void refreshSceneList();
    void refreshProtocolTable();
    void updateServiceStatus();
    void autoSave();
    void autoLoad();

    ConfigManager m_config;
    SimTcpServer *m_server;
    int m_currentSceneIndex;

    // UI
    QListWidget *m_sceneList;
    QTableWidget *m_protocolTable;
    QTextEdit *m_logEdit;
    QPushButton *m_btnStart;
    QPushButton *m_btnStop;
    QPushButton *m_btnSceneMgmt;
    QPushButton *m_btnAddProto;
    QPushButton *m_btnEditProto;
    QPushButton *m_btnDelProto;
    QLabel *m_statusLabel;
};

#endif // MAINWINDOW_H
