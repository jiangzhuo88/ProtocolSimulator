#include "mainwindow.h"
#include "protocoleditdialog.h"
#include "scenemanagedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QScrollBar>
#include <QEvent>
#include <QMenuBar>
#include <QTranslator>
#include "ZTextEdit.h"
#include "ZDDSMgr.h"

extern QTranslator *g_appTranslator;   // main.cpp中定义
extern void switchAppLanguage(const QString &langCode); // main.cpp中定义
extern QString currentAppLanguage();    // main.cpp中定义

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_server(nullptr), m_currentSceneIndex(-1),
      m_sceneGroup(nullptr), m_protoGroup(nullptr), m_logGroup(nullptr)
//      ,m_actLangZh(nullptr), m_actLangEn(nullptr), m_langGroup(nullptr)
{
    setupUi();
    setWindowTitle(tr("模拟训练软件"));
    resize(1200, 800);

    m_server = new SimTcpServer(this);
    connect(m_server, &SimTcpServer::logMessage, this, &MainWindow::onLog);
    ZDDSMgr::getInstance()->registerStateCtrl("platSystemCtrl","SIMULATOR_CTRL_TOPIC",[this](const char* buffer,size_t len)
    {
        stSceneStatusCtrlInfo comJamStatusControl;
        memcpy(&comJamStatusControl, buffer, len);
        QString sceneName = QString::fromLatin1(comJamStatusControl.cSceneID);
        controlSceneStatus(sceneName,comJamStatusControl.ucSceneRunMode == 0?false:true);
    });

    autoLoad();

    if (m_config.scenes().isEmpty()) {
        SceneConfig scene;
        scene.name = tr("默认场景");
        scene.tcpPort = 8080;
        m_config.addScene(scene);
        autoSave();
    }
    refreshSceneList();
}

void MainWindow::setupUi()
{
    auto central = new QWidget;
    auto mainLayout = new QVBoxLayout(central);
//    switchAppLanguage("en");
    // 语言菜单栏(即使当前不是翻译环境, 也预留入口; 切语言后会重写文字)
//    auto *langMenu = menuBar()->addMenu(tr("语言"));
//    m_langGroup = new QActionGroup(this);
//    m_langGroup->setExclusive(true);
//    m_actLangZh = langMenu->addAction(tr("中文"));
//    m_actLangEn = langMenu->addAction(tr("English"));
//    m_actLangZh->setCheckable(true);
//    m_actLangEn->setCheckable(true);
//    m_langGroup->addAction(m_actLangZh);
//    m_langGroup->addAction(m_actLangEn);
//    m_actLangZh->setChecked(true);
//    connect(m_actLangZh, &QAction::triggered, this, []() {
//        switchAppLanguage("zh");
//    });
//    connect(m_actLangEn, &QAction::triggered, this, []() {
//        switchAppLanguage("en");
//    });

    // 工具栏
    auto toolbarLayout = new QHBoxLayout;
    m_btnSceneMgmt = new QPushButton;
    m_btnAddProto   = new QPushButton;
    m_btnEditProto  = new QPushButton;
    m_btnCopyProto  = new QPushButton;
    m_btnDelProto   = new QPushButton;
    m_btnTest       = new QPushButton;
    m_testEdit      = new QLineEdit("Scene1");
    m_testBox       = new QCheckBox;
    m_btnStart      = new QPushButton;
    m_btnStop       = new QPushButton;
    m_btnStop->setEnabled(false);

    toolbarLayout->addWidget(m_btnSceneMgmt);
    toolbarLayout->addWidget(m_btnAddProto);
    toolbarLayout->addWidget(m_btnEditProto);
    toolbarLayout->addWidget(m_btnCopyProto);
    toolbarLayout->addWidget(m_btnDelProto);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_testEdit);
    toolbarLayout->addWidget(m_testBox);
    toolbarLayout->addWidget(m_btnTest);
    toolbarLayout->addWidget(m_btnStart);
    toolbarLayout->addWidget(m_btnStop);
    mainLayout->addLayout(toolbarLayout);

    m_statusLabel = new QLabel;
    mainLayout->addWidget(m_statusLabel);

    auto splitter = new QSplitter(Qt::Horizontal);

    m_sceneGroup = new QGroupBox;
    auto sceneLayout = new QVBoxLayout(m_sceneGroup);
    m_sceneList = new QListWidget;
    m_sceneList->setMinimumWidth(200);
    sceneLayout->addWidget(m_sceneList);
    splitter->addWidget(m_sceneGroup);

    m_protoGroup = new QGroupBox;
    auto protoLayout = new QVBoxLayout(m_protoGroup);
    m_protocolTable = new QTableWidget(0, 6);
    m_protocolTable->setAlternatingRowColors(true);
    m_protocolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_protocolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_protocolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    protoLayout->addWidget(m_protocolTable);
    splitter->addWidget(m_protoGroup);

    splitter->setSizes({250, 950});
    mainLayout->addWidget(splitter, 3);

    m_logGroup = new QGroupBox;
    auto logLayout = new QVBoxLayout(m_logGroup);
    m_logEdit = new ZTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(200);
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(m_logGroup, 1);

    setCentralWidget(central);

    connect(m_btnSceneMgmt, &QPushButton::clicked, this, &MainWindow::onSceneManagement);
    connect(m_btnAddProto, &QPushButton::clicked, this, &MainWindow::onAddProtocol);
    connect(m_btnEditProto, &QPushButton::clicked, this, &MainWindow::onEditProtocol);
    connect(m_btnCopyProto, &QPushButton::clicked, this, &MainWindow::onCopyProtocol);
    connect(m_btnDelProto, &QPushButton::clicked, this, &MainWindow::onDeleteProtocol);
    connect(m_btnTest, &QPushButton::clicked, this, &MainWindow::onTestBtnClicked);
    connect(m_btnStart, &QPushButton::clicked, this, QOverload<>::of(&MainWindow::onStartService));
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::onStopService);
    connect(m_btnTest, &QPushButton::clicked, this, &MainWindow::onTestBtnClicked);
    connect(m_sceneList, &QListWidget::currentRowChanged, this, &MainWindow::onSceneSelectionChanged);
    connect(m_protocolTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::onProtocolDoubleClicked);

    retranslateUi();
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("模拟训练软件"));
    if (auto *bar = menuBar()) {
        // 语言菜单项在第一个菜单
        auto actions = bar->actions();
        if (!actions.isEmpty())
            actions.first()->setText(tr("语言"));
    }
//    if (m_actLangZh) m_actLangZh->setText(tr("中文"));
//    if (m_actLangEn) m_actLangEn->setText(tr("English"));
    // 更新语言菜单勾选状态 (当前语言选项被check)
//    const QString curLang = currentAppLanguage();
    //    if (m_actLangZh && m_actLangEn) {
    //        const QSignalBlocker b1(m_actLangZh);
    //        const QSignalBlocker b2(m_actLangEn);
    //        m_actLangZh->setChecked(curLang == "zh");
    //        m_actLangEn->setChecked(curLang == "en");
    //    }

    m_btnSceneMgmt->setText(tr("场景管理"));
    m_btnAddProto  ->setText(tr("添加协议"));
    m_btnEditProto ->setText(tr("编辑协议"));
    m_btnCopyProto ->setText(tr("复制协议"));
    m_btnDelProto  ->setText(tr("删除协议"));
    m_btnTest      ->setText(tr("测试"));
    m_btnStart     ->setText(tr("启动服务"));
    m_btnStop      ->setText(tr("停止服务"));

    if (m_sceneGroup) m_sceneGroup->setTitle(tr("场景列表"));
    if (m_protoGroup) m_protoGroup->setTitle(tr("协议列表 (双击编辑)"));
    if (m_logGroup)   m_logGroup  ->setTitle(tr("日志"));

    m_protocolTable->setHorizontalHeaderLabels(
        QStringList() << tr("协议名称") << tr("类型") << tr("状态")
                      << tr("匹配字段") << tr("回复模式") << tr("描述"));

    // 刷新表格内文字(类型/状态/匹配字段/回复模式)
    refreshProtocolTable();
    updateServiceStatus();
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
}

void MainWindow::refreshSceneList()
{
    m_sceneList->clear();
    for (const SceneConfig &scene : m_config.scenes()) {
        QString text = tr("%1 (端口:%2, 协议:%3)")
                       .arg(scene.name).arg(scene.tcpPort).arg(scene.protocols.size());
        QListWidgetItem* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole,scene.name);
        m_sceneList->addItem(item);
    }
    if (m_sceneList->count() > 0 && m_currentSceneIndex < 0)
        m_sceneList->setCurrentRow(0);
}

void MainWindow::refreshProtocolTable()
{
    m_protocolTable->setRowCount(0);
    if (m_currentSceneIndex < 0 || m_currentSceneIndex >= m_config.scenes().size())
        return;

    const SceneConfig &scene = m_config.scenes()[m_currentSceneIndex];
    m_protocolTable->setRowCount(scene.protocols.size());

    for (int i = 0; i < scene.protocols.size(); ++i) {
        const ProtocolConfig &proto = scene.protocols[i];
        bool valid = proto.isValid();

        m_protocolTable->setItem(i, 0, new QTableWidgetItem(proto.name));

        QString typeStr = proto.isActivePush ? tr("主动上报") : tr("接收匹配");
        m_protocolTable->setItem(i, 1, new QTableWidgetItem(typeStr));

        m_protocolTable->setItem(i, 2, new QTableWidgetItem(valid ? tr("有效") : tr("无效(无匹配)")));
        m_protocolTable->item(i, 2)->setForeground(valid ? QColor(Qt::green) : QColor(Qt::red));

        QStringList matchFields;
        for (const auto &p : proto.headerParams)
            if (p.matchEnabled) matchFields << p.name;
        for (const auto &p : proto.dataParams)
            if (p.matchEnabled) matchFields << p.name;
        m_protocolTable->setItem(i, 3, new QTableWidgetItem(matchFields.isEmpty() ? tr("无") : matchFields.join(", ")));

        QString replyStr;
        if (proto.isActivePush) {
            replyStr = tr("定时上报(%1ms)").arg(proto.pushIntervalMs);
        } else {
            ReplyMode m = proto.replyConfig.mode;
            if (m == ReplyMode::MultiPacket) {
                replyStr = ReplyConfig::modeToString(ReplyMode::Once);
            } else {
                replyStr = ReplyConfig::modeToString(m);
            }
            if (m == ReplyMode::PeriodicCustom)
                replyStr += tr("(%1ms)").arg(proto.replyConfig.customIntervalMs);
            if (!proto.replyConfig.multiPackets.isEmpty())
                replyStr += tr("+拼包%1").arg(proto.replyConfig.multiPackets.size());
        }
        m_protocolTable->setItem(i, 4, new QTableWidgetItem(replyStr));
        m_protocolTable->setItem(i, 5, new QTableWidgetItem(proto.description));

        if (!valid) {
            for (int c = 0; c < 6; ++c) {
                m_protocolTable->item(i, c)->setBackground(QColor(255, 240, 240));
            }
        }
    }
}

void MainWindow::onSceneSelectionChanged()
{
    m_currentSceneIndex = m_sceneList->currentRow();
    refreshProtocolTable();
    updateServiceStatus();
}

void MainWindow::onSceneManagement()
{
    SceneManageDialog dlg(&m_config, this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshSceneList();
        autoSave();
    }
}

void MainWindow::onAddProtocol()
{
    if (m_currentSceneIndex < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个场景"));
        return;
    }
    ProtocolConfig proto;
    proto.name = tr("新协议");
    auto &protocols = m_config.scenes()[m_currentSceneIndex].protocols;

    ProtocolEditDialog dlg(proto, &protocols,this);
    if (dlg.exec() == QDialog::Accepted) {
        proto = dlg.getProtocol();
        m_config.scenes()[m_currentSceneIndex].protocols.append(proto);
        refreshProtocolTable();
        autoSave();
        if (m_server && m_server->isRunning())
            m_server->setProtocols(m_config.scenes()[m_currentSceneIndex].protocols);
    }
}

void MainWindow::onEditProtocol()
{
    int row = m_protocolTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个协议"));
        return;
    }
    ProtocolConfig proto = m_config.scenes()[m_currentSceneIndex].protocols[row];

    auto &protocols = m_config.scenes()[m_currentSceneIndex].protocols;

    ProtocolEditDialog dlg(proto, &protocols,this);
    if (dlg.exec() == QDialog::Accepted) {
        proto = dlg.getProtocol();
        m_config.scenes()[m_currentSceneIndex].protocols[row] = proto;
        refreshProtocolTable();
        autoSave();
        if (m_server && m_server->isRunning())
            m_server->setProtocols(m_config.scenes()[m_currentSceneIndex].protocols);
    }
}

void MainWindow::onProtocolDoubleClicked(int row, int)
{
    if (m_currentSceneIndex < 0 || row < 0) return;
    ProtocolConfig proto = m_config.scenes()[m_currentSceneIndex].protocols[row];

    auto &protocols = m_config.scenes()[m_currentSceneIndex].protocols;

    ProtocolEditDialog dlg(proto, &protocols,this);
    if (dlg.exec() == QDialog::Accepted) {
        proto = dlg.getProtocol();
        m_config.scenes()[m_currentSceneIndex].protocols[row] = proto;
        refreshProtocolTable();
        autoSave();
        if (m_server && m_server->isRunning())
            m_server->setProtocols(m_config.scenes()[m_currentSceneIndex].protocols);
    }
}

void MainWindow::onTestBtnClicked()
{
    controlSceneStatus(m_testEdit->text(),m_testBox->isChecked());
}

void MainWindow::onCopyProtocol()
{
    int row = m_protocolTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个协议"));
        return;
    }
    ProtocolConfig proto = m_config.scenes()[m_currentSceneIndex].protocols[row];
    proto.name += tr(" (副本)");
    m_config.scenes()[m_currentSceneIndex].protocols.append(proto);
    refreshProtocolTable();
    autoSave();
    if (m_server && m_server->isRunning())
        m_server->setProtocols(m_config.scenes()[m_currentSceneIndex].protocols);
}

void MainWindow::onDeleteProtocol()
{
    int row = m_protocolTable->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个协议"));
        return;
    }
    auto ret = QMessageBox::question(this, tr("确认"), tr("确定删除选中的协议?"));
    if (ret == QMessageBox::Yes) {
        m_config.scenes()[m_currentSceneIndex].protocols.removeAt(row);
        refreshProtocolTable();
        autoSave();
        if (m_server && m_server->isRunning())
            m_server->setProtocols(m_config.scenes()[m_currentSceneIndex].protocols);
    }
}

void MainWindow::onStartService()
{
    if (m_currentSceneIndex < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择一个场景"));
        return;
    }
    onStartService(m_currentSceneIndex);
}

void MainWindow::onStartService(int index)
{
    if (index < 0) return;
    SceneConfig &scene = m_config.scenes()[index];
    scene.isRunning = true;
    if (m_server->start(scene.tcpPort)) {
        m_server->setProtocols(scene.protocols);
        m_btnStart->setEnabled(false);
        m_btnStop->setEnabled(true);
        updateServiceStatus();
    }
}

void MainWindow::onStopService()
{
    for(SceneConfig &scene :m_config.scenes())
    {
        scene.isRunning = false;
    }
    m_server->stop();
    onLog(tr("[服务] TCP服务已停止"));
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    updateServiceStatus();
}

void MainWindow::autoSave()
{
    QString dir = QApplication::applicationDirPath() + "/scenes";
    QDir d(dir);
    if (!d.exists()) d.mkpath(".");
    if (m_config.saveScenes(dir)) {
        onLog(tr("[配置] 已自动保存到 %1").arg(dir));
    }
}

void MainWindow::autoLoad()
{
    QString dir = QApplication::applicationDirPath() + "/scenes";
    if (m_config.loadScenes(dir)) {
        refreshSceneList();
        onLog(tr("[配置] 已从 %1 自动加载").arg(dir));
    }
}

void MainWindow::controlSceneStatus(QString sceneName,bool openScene)
{
    for(int i = 0;i<m_config.scenes().count();i++)
    {
        QString name = m_config.scenes()[i].name;
        qDebug()<<"var.name"<<name;
        if(name == sceneName)
        {
            m_currentSceneIndex = i;
            break;
        }
    }
    onStopService();
    if(openScene)
    {
        onStartService();
    }
}

void MainWindow::onLog(const QString &msg)
{
    QString time = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logEdit->append(QString("[%1] %2").arg(time).arg(msg));

    const int maxLines = 500;
    QTextDocument *doc = m_logEdit->document();
    int excess = doc->blockCount() - maxLines;
    if (excess > 0) {
        QTextCursor c(doc);
        c.movePosition(QTextCursor::Start);
        c.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, excess);
        c.removeSelectedText();
    }
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
}

void MainWindow::updateServiceStatus()
{
    if(m_currentSceneIndex < 0 || m_config.scenes().count() <= m_currentSceneIndex)
    {
        return;
    }
    const SceneConfig &scene = m_config.scenes()[m_currentSceneIndex];
    if (scene.isRunning == true) {
        m_statusLabel->setText(tr("服务运行中 | 场景: %1 | 端口: %2 | 客户端: %3")
                                .arg(scene.name).arg(scene.tcpPort).arg(m_server->clientCount()));
    } else {
        m_statusLabel->setText(tr("服务未启动"));
    }
}
