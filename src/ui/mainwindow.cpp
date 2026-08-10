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
#include "ZTextEdit.h"
#include "ZDDSMgr.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_server(nullptr), m_currentSceneIndex(-1)
{
    setupUi();
    setWindowTitle("模拟训练软件");
    resize(1200, 800);

    m_server = new SimTcpServer(this);
    connect(m_server, &SimTcpServer::logMessage, this, &MainWindow::onLog);
    ZDDSMgr::getInstance()->registerStateCtrl("platSystemCtrl","SIMULATOR_CTRL_TOPIC",[this](const char* buffer,size_t len)
    {
//        QByteArray bytes(buffer,len);
        stSceneStatusCtrlInfo comJamStatusControl;
        memcpy(&comJamStatusControl, buffer, len);
        //接收到场景后，切换场景
        QString sceneName = QString::fromLatin1(comJamStatusControl.cSceneID);
        controlSceneStatus(sceneName,comJamStatusControl.ucSceneRunMode == 0?false:true);
//        for(const SceneConfig& var:m_config.scenes())

    });

    // 启动时自动加载配置
    autoLoad();

    // 如果没有场景，创建默认场景
    if (m_config.scenes().isEmpty()) {
        SceneConfig scene;
        scene.name = "默认场景";
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

    // 工具栏
    auto toolbarLayout = new QHBoxLayout;
    m_btnSceneMgmt = new QPushButton("场景管理");
    m_btnAddProto = new QPushButton("添加协议");
    m_btnEditProto = new QPushButton("编辑协议");
    m_btnCopyProto = new QPushButton("复制协议");
    m_btnDelProto = new QPushButton("删除协议");
    m_btnTest = new QPushButton("测试");
    m_testEdit = new QLineEdit("Scene1");
    m_testBox = new QCheckBox();
    m_btnStart = new QPushButton("启动服务");
    m_btnStop = new QPushButton("停止服务");
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

    // 状态栏
    m_statusLabel = new QLabel("服务未启动");
    mainLayout->addWidget(m_statusLabel);

    // 主区域: 左边场景列表, 右边协议表格
    auto splitter = new QSplitter(Qt::Horizontal);

    // 场景列表
    auto sceneGroup = new QGroupBox("场景列表");
    auto sceneLayout = new QVBoxLayout(sceneGroup);
    m_sceneList = new QListWidget;
    m_sceneList->setMinimumWidth(200);
    sceneLayout->addWidget(m_sceneList);
    splitter->addWidget(sceneGroup);

    // 协议表格
    auto protoGroup = new QGroupBox("协议列表 (双击编辑)");
    auto protoLayout = new QVBoxLayout(protoGroup);
    m_protocolTable = new QTableWidget(0, 6);
    m_protocolTable->setAlternatingRowColors(true);
    m_protocolTable->setHorizontalHeaderLabels({"协议名称", "类型", "状态", "匹配字段", "回复模式", "描述"});
    m_protocolTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_protocolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_protocolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    protoLayout->addWidget(m_protocolTable);
    splitter->addWidget(protoGroup);

    splitter->setSizes({250, 950});
    mainLayout->addWidget(splitter, 3);

    // 日志区
    auto logGroup = new QGroupBox("日志");
    auto logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new ZTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(200);
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(logGroup, 1);

    setCentralWidget(central);

    // 连接信号
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
}

void MainWindow::refreshSceneList()
{
    m_sceneList->clear();
    for (const SceneConfig &scene : m_config.scenes()) {
        QString text = QString("%1 (端口:%2, 协议:%3)")
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

        // 类型列
        QString typeStr = proto.isActivePush ? "主动上报" : "接收匹配";
        m_protocolTable->setItem(i, 1, new QTableWidgetItem(typeStr));

        m_protocolTable->setItem(i, 2, new QTableWidgetItem(valid ? "有效" : "无效(无匹配)"));
        m_protocolTable->item(i, 2)->setForeground(valid ? QColor(Qt::green) : QColor(Qt::red));

        // 列出匹配字段
        QStringList matchFields;
        for (const auto &p : proto.headerParams)
            if (p.matchEnabled) matchFields << p.name;
        for (const auto &p : proto.dataParams)
            if (p.matchEnabled) matchFields << p.name;
        m_protocolTable->setItem(i, 3, new QTableWidgetItem(matchFields.isEmpty() ? "无" : matchFields.join(", ")));

        // 回复模式
        QString replyStr;
        if (proto.isActivePush) {
            replyStr = QString("定时上报(%1ms)").arg(proto.pushIntervalMs);
        } else {
            // 旧MultiPacket配置显示为Once(已无独立UI)
            ReplyMode m = proto.replyConfig.mode;
            if (m == ReplyMode::MultiPacket) {
                replyStr = ReplyConfig::modeToString(ReplyMode::Once);
            } else {
                replyStr = ReplyConfig::modeToString(m);
            }
            if (m == ReplyMode::PeriodicCustom)
                replyStr += QString("(%1ms)").arg(proto.replyConfig.customIntervalMs);
            // 有拼包区则标注拼包数
            if (!proto.replyConfig.multiPackets.isEmpty())
                replyStr += QString("+拼包%1").arg(proto.replyConfig.multiPackets.size());
        }
        m_protocolTable->setItem(i, 4, new QTableWidgetItem(replyStr));

        m_protocolTable->setItem(i, 5, new QTableWidgetItem(proto.description));

        // 有效/无效行颜色
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
        QMessageBox::warning(this, "提示", "请先选择一个场景");
        return;
    }
    ProtocolConfig proto;
    proto.name = "新协议";
    auto &protocols = m_config.scenes()[m_currentSceneIndex].protocols;

//    ProtocolEditDialog dlg(proto, this);
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
        QMessageBox::warning(this, "提示", "请先选择一个协议");
        return;
    }
    ProtocolConfig proto = m_config.scenes()[m_currentSceneIndex].protocols[row];

    auto &protocols = m_config.scenes()[m_currentSceneIndex].protocols;

//    ProtocolEditDialog dlg(proto, this);
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

//    ProtocolEditDialog dlg(proto, this);
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
        QMessageBox::warning(this, "提示", "请先选择一个协议");
        return;
    }
    ProtocolConfig proto = m_config.scenes()[m_currentSceneIndex].protocols[row];
    proto.name += " (副本)";
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
        QMessageBox::warning(this, "提示", "请先选择一个协议");
        return;
    }
    auto ret = QMessageBox::question(this, "确认", "确定删除选中的协议?");
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
        QMessageBox::warning(this, "提示", "请先选择一个场景");
        return;
    }
    onStartService(m_currentSceneIndex);
}

void MainWindow::onStartService(int index)
{
    if (index < 0) {
//        QMessageBox::warning(this, "提示", "请先选择一个场景");
        return;
    }
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
    onLog("[服务] TCP服务已停止");
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
        onLog(QString("[配置] 已自动保存到 %1").arg(dir));
    }
}

void MainWindow::autoLoad()
{
    QString dir = QApplication::applicationDirPath() + "/scenes";
    if (m_config.loadScenes(dir)) {
        refreshSceneList();
        onLog(QString("[配置] 已从 %1 自动加载").arg(dir));
    }
}

void MainWindow::controlSceneStatus(QString sceneName,bool openScene)
{

    for(int i = 0;i<m_config.scenes().count();i++)
    {
//        SceneConfig var = m_config.scenes()[i];
        QString name = m_config.scenes()[i].name;
        qDebug()<<"var.name"<<name;
        if(name == sceneName)
        {
//            m_sceneList->setCurrentRow(i);
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

    // 限制日志最多显示500行: 超出则从顶部删除多余行, 避免长时间运行内存膨胀
    const int maxLines = 500;
    QTextDocument *doc = m_logEdit->document();
    int excess = doc->blockCount() - maxLines;
    if (excess > 0) {
        QTextCursor c(doc);
        c.movePosition(QTextCursor::Start);
        // 从开头向下选中excess个块(整块含分隔符), 一次性删除
        c.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, excess);
        c.removeSelectedText();
    }
    // 滚动到底部显示最新日志
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

        m_statusLabel->setText(QString("服务运行中 | 场景: %1 | 端口: %2 | 客户端: %3")
                                .arg(scene.name).arg(scene.tcpPort).arg(m_server->clientCount()));
    } else {
        m_statusLabel->setText("服务未启动");
    }
}
