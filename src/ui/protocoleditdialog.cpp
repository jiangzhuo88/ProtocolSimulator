#include "protocoleditdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTimer>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMimeData>
#include <QScrollArea>
#include <QApplication>
#include <QListWidget>
#include "collapsiblegroupbox.h"

ProtocolEditDialog::ProtocolEditDialog(ProtocolConfig &proto, QVector<ProtocolConfig> *allProtocols, QWidget *parent)
    : QDialog(parent), m_proto(proto), m_allProtocols(allProtocols), m_loading(false)
{
    setupUi();
    loadProtocol();
    updatePreview();
}

void ProtocolEditDialog::setupUi()
{
    setWindowTitle("协议编辑");
    resize(1100, 700);

    auto mainLayout = new QVBoxLayout(this);

    // 基本信息 + 主动上报
    auto infoLayout = new QHBoxLayout;
    infoLayout->addWidget(new QLabel("协议名称:"));
    m_nameEdit = new QLineEdit;
    infoLayout->addWidget(m_nameEdit);
    infoLayout->addWidget(new QLabel("描述:"));
    m_descEdit = new QLineEdit;
    infoLayout->addWidget(m_descEdit);
    mainLayout->addLayout(infoLayout);

    auto pushLayout = new QHBoxLayout;
    m_activePushCheck = new QCheckBox("主动上报(心跳/状态信息, 连接后自动定时发送)");
    m_pushIntervalSpin = new QSpinBox;
    m_pushIntervalSpin->setRange(10, 999999);
    m_pushIntervalSpin->setValue(1000);
    m_pushIntervalSpin->setEnabled(false);
    pushLayout->addWidget(m_activePushCheck);
    pushLayout->addWidget(new QLabel("周期(ms):"));
    pushLayout->addWidget(m_pushIntervalSpin);
    pushLayout->addWidget(new QLabel("  固定帧长度:"));
    m_fixedFrameLenSpin = new QSpinBox;
    m_fixedFrameLenSpin->setRange(0, 999999);
    m_fixedFrameLenSpin->setValue(0);
    m_fixedFrameLenSpin->setToolTip("0=根据参数自动计算帧长度\n>0=手动指定整帧字节数(数据区未配齐时使用)");
    pushLayout->addWidget(m_fixedFrameLenSpin);
    pushLayout->addStretch();
    mainLayout->addLayout(pushLayout);

    connect(m_activePushCheck, &QCheckBox::toggled, [this](bool checked) {
        m_pushIntervalSpin->setEnabled(checked);
        onParamChanged();
    });
    connect(m_pushIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    connect(m_fixedFrameLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &ProtocolEditDialog::onParamChanged);
    connect(m_descEdit, &QLineEdit::textChanged, this, &ProtocolEditDialog::onParamChanged);

    // Tab
    auto tabWidget = new QTabWidget;

    // ====== Tab1: 接收协议 ======
    auto recvTab = new QWidget;
    auto recvLayout = new QVBoxLayout(recvTab);

    // 帧头参数
    auto hdrGroup = new CollapsibleGroupBox("帧头参数 (右键支持复制/粘贴)");
    auto hdrContent = new QWidget;
    auto hdrLayout = new QVBoxLayout(hdrContent);
    m_headerTable = new QTableWidget(0, 11);
    m_headerTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","匹配","匹配模式","匹配值","匹配值2"});
    for (int i = 0; i < 11; ++i)
        m_headerTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_headerTable->setContextMenuPolicy(Qt::CustomContextMenu);
    hdrLayout->addWidget(m_headerTable);
    auto hdrBtnLayout = new QHBoxLayout;
    auto btnAddHdr = new QPushButton("添加帧头参数");
    auto btnDelHdr = new QPushButton("删除选中");
    hdrBtnLayout->addWidget(btnAddHdr);
    hdrBtnLayout->addWidget(btnDelHdr);
    hdrBtnLayout->addStretch();
    hdrLayout->addLayout(hdrBtnLayout);
    hdrGroup->setContentWidget(hdrContent);
    recvLayout->addWidget(hdrGroup);

    // 数据区参数
    auto dataGroup = new CollapsibleGroupBox("数据区参数 (右键支持复制/粘贴)");
    auto dataContent = new QWidget;
    auto dataLayout = new QVBoxLayout(dataContent);
    m_dataTable = new QTableWidget(0, 11);
    m_dataTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","匹配","匹配模式","匹配值","匹配值2"});
    for (int i = 0; i < 11; ++i)
        m_dataTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_dataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    dataLayout->addWidget(m_dataTable);
    auto dataBtnLayout = new QHBoxLayout;
    auto btnAddData = new QPushButton("添加数据区参数");
    auto btnDelData = new QPushButton("删除选中");
    dataBtnLayout->addWidget(btnAddData);
    dataBtnLayout->addWidget(btnDelData);
    dataBtnLayout->addStretch();
    dataLayout->addLayout(dataBtnLayout);
    dataGroup->setContentWidget(dataContent);
    recvLayout->addWidget(dataGroup);

    auto copyBtnLayout = new QHBoxLayout;
    auto btnCopyHeader = new QPushButton("复制帧头到回复帧头");
    auto btnCopyData = new QPushButton("复制数据区到回复数据区");
    auto btnCopyAll = new QPushButton("复制全部接收配置到回复");
    copyBtnLayout->addWidget(btnCopyHeader);
    copyBtnLayout->addWidget(btnCopyData);
    copyBtnLayout->addWidget(btnCopyAll);
    copyBtnLayout->addStretch();
    recvLayout->addLayout(copyBtnLayout);

    auto utilBtnLayout = new QHBoxLayout;
    auto btnSwapByteOrder = new QPushButton("一键切换所有字段大小端");
    utilBtnLayout->addWidget(btnSwapByteOrder);
    utilBtnLayout->addStretch();
    recvLayout->addLayout(utilBtnLayout);

    tabWidget->addTab(recvTab, "接收协议配置");

    // ====== Tab2: 操作配置 ======
    auto actionTab = new QWidget;
    auto actionLayout = new QVBoxLayout(actionTab);

    m_stopAllCheck = new QCheckBox("匹配成功时停止所有周期回复");
    m_stopAllCheck->setToolTip("勾选后，该协议匹配成功时会停止所有正在运行的周期回复");
    actionLayout->addWidget(m_stopAllCheck);

    actionLayout->addWidget(new QLabel("选择要停止的周期回复协议:"));
    m_stopList = new QListWidget;
    m_stopList->setSelectionMode(QAbstractItemView::NoSelection);
    m_stopList->setAlternatingRowColors(true);
    actionLayout->addWidget(m_stopList);

    auto actionBtnLayout = new QHBoxLayout;
    auto btnSelectAll = new QPushButton("全选");
    auto btnDeselectAll = new QPushButton("全不选");
    actionBtnLayout->addWidget(btnSelectAll);
    actionBtnLayout->addWidget(btnDeselectAll);
    actionBtnLayout->addStretch();
    actionLayout->addLayout(actionBtnLayout);

    tabWidget->addTab(actionTab, "操作配置");

    connect(btnSelectAll, &QPushButton::clicked, [this]() {
        for (int i = 0; i < m_stopList->count(); ++i)
            m_stopList->item(i)->setCheckState(Qt::Checked);
        onParamChanged();
    });
    connect(btnDeselectAll, &QPushButton::clicked, [this]() {
        for (int i = 0; i < m_stopList->count(); ++i)
            m_stopList->item(i)->setCheckState(Qt::Unchecked);
        onParamChanged();
    });
    connect(m_stopAllCheck, &QCheckBox::toggled, this, &ProtocolEditDialog::onParamChanged);
    connect(m_stopList, &QListWidget::itemChanged, this, [this](QListWidgetItem *) { onParamChanged(); });

    connect(btnAddHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onAddHeaderParam);
    connect(btnDelHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveHeaderParam);
    connect(btnAddData, &QPushButton::clicked, this, &ProtocolEditDialog::onAddDataParam);
    connect(btnDelData, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveDataParam);
    connect(btnCopyHeader, &QPushButton::clicked, this, &ProtocolEditDialog::onCopyHeaderToReply);
    connect(btnCopyData, &QPushButton::clicked, this, &ProtocolEditDialog::onCopyDataToReply);
    connect(btnCopyAll, &QPushButton::clicked, this, &ProtocolEditDialog::onCopyRecvToReply);
    connect(btnSwapByteOrder, &QPushButton::clicked, this, &ProtocolEditDialog::onSwapByteOrder);

    connect(m_headerTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onHeaderTableContextMenu);
    connect(m_dataTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onDataTableContextMenu);

    // ====== Tab2: 回复配置 ======
    auto replyTab = new QWidget;
    auto replyLayout = new QVBoxLayout(replyTab);

    auto modeLayout = new QHBoxLayout;
    modeLayout->addWidget(new QLabel("回复模式:"));
    m_replyModeCombo = new QComboBox;
    m_replyModeCombo->addItems({"不回复", "回复一次", "1秒周期回复", "5秒周期回复", "自定义周期回复"});
    modeLayout->addWidget(m_replyModeCombo);
    m_replyIntervalLabel = new QLabel("周期(ms):");
    m_replyIntervalSpin = new QSpinBox;
    m_replyIntervalSpin->setRange(10, 999999);
    m_replyIntervalSpin->setValue(1000);
    modeLayout->addWidget(m_replyIntervalLabel);
    modeLayout->addWidget(m_replyIntervalSpin);
    modeLayout->addStretch();
    replyLayout->addLayout(modeLayout);

    // 回复帧头参数
    auto rplHdrGroup = new CollapsibleGroupBox("回复帧头参数 (右键支持复制/粘贴)");
    auto rplHdrContent = new QWidget;
    auto rplHdrLayout = new QVBoxLayout(rplHdrContent);
    m_replyHeaderTable = new QTableWidget(0, 11);
    m_replyHeaderTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","随机","随机最小","随机最大","随机长度"});
    for (int i = 0; i < 11; ++i)
        m_replyHeaderTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_replyHeaderTable->setContextMenuPolicy(Qt::CustomContextMenu);
    rplHdrLayout->addWidget(m_replyHeaderTable);
    auto rplHdrBtnLayout = new QHBoxLayout;
    auto btnAddRplHdr = new QPushButton("添加回复帧头参数");
    auto btnDelRplHdr = new QPushButton("删除选中");
    rplHdrBtnLayout->addWidget(btnAddRplHdr);
    rplHdrBtnLayout->addWidget(btnDelRplHdr);
    rplHdrBtnLayout->addStretch();
    rplHdrLayout->addLayout(rplHdrBtnLayout);
    rplHdrGroup->setContentWidget(rplHdrContent);
    replyLayout->addWidget(rplHdrGroup);

    // 回复数据区参数
    auto rplDataGroup = new CollapsibleGroupBox("回复数据区参数 (右键支持复制/粘贴)");
    auto rplDataContent = new QWidget;
    auto rplDataLayout = new QVBoxLayout(rplDataContent);
    m_replyDataTable = new QTableWidget(0, 11);
    m_replyDataTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","随机","随机最小","随机最大","随机长度"});
    for (int i = 0; i < 11; ++i)
        m_replyDataTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_replyDataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    rplDataLayout->addWidget(m_replyDataTable);
    auto rplDataBtnLayout = new QHBoxLayout;
    auto btnAddRplData = new QPushButton("添加回复数据区参数");
    auto btnDelRplData = new QPushButton("删除选中");
    rplDataBtnLayout->addWidget(btnAddRplData);
    rplDataBtnLayout->addWidget(btnDelRplData);
    rplDataBtnLayout->addStretch();
    rplDataLayout->addLayout(rplDataBtnLayout);
    rplDataGroup->setContentWidget(rplDataContent);
    replyLayout->addWidget(rplDataGroup);

    auto rplUtilLayout = new QHBoxLayout;
    auto btnRplSwap = new QPushButton("一键切换所有字段大小端");
    rplUtilLayout->addWidget(btnRplSwap);
    rplUtilLayout->addStretch();
    replyLayout->addLayout(rplUtilLayout);

    tabWidget->addTab(replyTab, "回复配置");

    connect(btnAddRplHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onAddReplyHeaderParam);
    connect(btnDelRplHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveReplyHeaderParam);
    connect(btnAddRplData, &QPushButton::clicked, this, &ProtocolEditDialog::onAddReplyDataParam);
    connect(btnDelRplData, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveReplyDataParam);
    connect(m_replyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolEditDialog::onReplyModeChanged);
    connect(m_replyIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    connect(btnRplSwap, &QPushButton::clicked, this, &ProtocolEditDialog::onSwapByteOrder);

    connect(m_replyHeaderTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onReplyHeaderTableContextMenu);
    connect(m_replyDataTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onReplyDataTableContextMenu);

    mainLayout->addWidget(tabWidget);

    // ====== 帧预览 ======
    auto previewGroup = new CollapsibleGroupBox("帧预览");
    auto previewContent = new QWidget;
    auto previewLayout = new QVBoxLayout(previewContent);
    previewLayout->setContentsMargins(0, 0, 0, 0);

    auto scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumHeight(300);
    auto scrollContent = new QWidget;
    auto scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);

    m_previewLabel = new QLabel;
    m_previewLabel->setStyleSheet("font-family: monospace; font-size: 13px; background: #f8f8f8; padding: 8px;");
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    scrollLayout->addWidget(new QLabel("接收帧(组包结果):"));
    scrollLayout->addWidget(m_previewLabel);
    m_replyPreviewLabel = new QLabel;
    m_replyPreviewLabel->setStyleSheet("font-family: monospace; font-size: 13px; background: #f8f8f8; padding: 8px;");
    m_replyPreviewLabel->setWordWrap(true);
    m_replyPreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    scrollLayout->addWidget(new QLabel("回复帧(组包结果):"));
    scrollLayout->addWidget(m_replyPreviewLabel);

    scrollArea->setWidget(scrollContent);
    previewLayout->addWidget(scrollArea);
    previewGroup->setContentWidget(previewContent);
    mainLayout->addWidget(previewGroup);

    // 按钮
    auto btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    auto btnOk = new QPushButton("确定");
    auto btnCancel = new QPushButton("取消");
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, [this]() { saveProtocol(); accept(); });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // 定时刷新预览
    auto *timer = new QTimer(this);
    timer->start(500);
    connect(timer, &QTimer::timeout, this, &ProtocolEditDialog::onPreviewTimer);

    installEventFilter(this);
}

void ProtocolEditDialog::loadProtocol()
{
    m_loading = true;

    m_nameEdit->setText(m_proto.name);
    m_descEdit->setText(m_proto.description);
    m_activePushCheck->setChecked(m_proto.isActivePush);
    m_pushIntervalSpin->setValue(m_proto.pushIntervalMs);
    m_pushIntervalSpin->setEnabled(m_proto.isActivePush);
    m_fixedFrameLenSpin->setValue(m_proto.fixedFrameLength);
    m_stopAllCheck->setChecked(m_proto.stopAllPeriodicOnMatch);

    // 填充操作配置列表(场景中其他有周期回复的协议)
    m_stopList->clear();
    if (m_allProtocols) {
        for (const auto &proto : *m_allProtocols) {
            if (proto.name == m_proto.name) continue; // 跳过自身
            if (proto.replyConfig.mode == ReplyMode::Periodic1s ||
                proto.replyConfig.mode == ReplyMode::Periodic5s ||
                proto.replyConfig.mode == ReplyMode::PeriodicCustom) {
                auto item = new QListWidgetItem(proto.name);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(m_proto.stopPeriodicProtocolNames.contains(proto.name)
                                    ? Qt::Checked : Qt::Unchecked);
                m_stopList->addItem(item);
            }
        }
    }

    // 帧头参数
    m_headerTable->setRowCount(0);
    for (const auto &p : m_proto.headerParams) {
        int row = m_headerTable->rowCount();
        m_headerTable->insertRow(row);
        populateParamRow(m_headerTable, row, p, false);
    }

    // 数据区参数
    m_dataTable->setRowCount(0);
    for (const auto &p : m_proto.dataParams) {
        int row = m_dataTable->rowCount();
        m_dataTable->insertRow(row);
        populateParamRow(m_dataTable, row, p, false);
    }

    // 回复模式
    int modeIdx = (int)m_proto.replyConfig.mode;
    m_replyModeCombo->setCurrentIndex(modeIdx);
    m_replyIntervalSpin->setValue(m_proto.replyConfig.customIntervalMs);
    onReplyModeChanged(modeIdx);

    // 回复帧头参数
    m_replyHeaderTable->setRowCount(0);
    for (const auto &p : m_proto.replyConfig.headerParams) {
        int row = m_replyHeaderTable->rowCount();
        m_replyHeaderTable->insertRow(row);
        populateParamRow(m_replyHeaderTable, row, p, true);
    }

    // 回复数据区参数
    m_replyDataTable->setRowCount(0);
    for (const auto &p : m_proto.replyConfig.dataParams) {
        int row = m_replyDataTable->rowCount();
        m_replyDataTable->insertRow(row);
        populateParamRow(m_replyDataTable, row, p, true);
    }

    m_loading = false;

    updateMatchHighlight(m_headerTable);
    updateMatchHighlight(m_dataTable);
}

static bool isRawByteType(ParamType type)
{
    return type == ParamType::Hex || type == ParamType::Bytes ||
           type == ParamType::String || type == ParamType::StringUtf8;
}

void ProtocolEditDialog::populateParamRow(QTableWidget *table, int row, const ProtocolParam &param, bool isReplyTable)
{
    // 名称 (0)
    auto *itemName = new QTableWidgetItem(param.name);
    table->setItem(row, 0, itemName);

    // 字节序/长度 (2) -- 数值类型显示字节序, Hex/Bytes/String显示长度
    auto setupOrderOrLength = [this, table, row](ParamType t, ByteOrder order, int len) {
        if (isRawByteType(t)) {
            auto lenSpin = new QSpinBox;
            lenSpin->setRange(0, 9999);
            lenSpin->setValue(len);
            lenSpin->setToolTip("0=自动(根据默认值推导)");
            table->setCellWidget(row, 2, lenSpin);
            connect(lenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
        } else {
            auto orderCombo = new QComboBox;
            orderCombo->setFocusPolicy(Qt::StrongFocus);
            orderCombo->addItem("BigEndian");
            orderCombo->addItem("LittleEndian");
            orderCombo->setCurrentIndex((int)order);
            table->setCellWidget(row, 2, orderCombo);
            connect(orderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolEditDialog::onParamChanged);
        }
    };
    setupOrderOrLength(param.type, param.byteOrder, param.userLength);

    // 类型 (1)
    auto typeCombo = new QComboBox;
    typeCombo->setFocusPolicy(Qt::StrongFocus);
    for (int i = 0; i <= (int)ParamType::Hex; ++i)
        typeCombo->addItem(ProtocolParam::typeToString((ParamType)i));
    typeCombo->setCurrentIndex((int)param.type);
    table->setCellWidget(row, 1, typeCombo);

    // 类型变化时切换字节序/长度控件，然后通知参数变化
    connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, table, row, setupOrderOrLength](int idx) {
        ParamType t = (ParamType)idx;
        ByteOrder currOrder = ByteOrder::BigEndian;
        int currLen = 0;
        if (auto oldCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 2)))
            currOrder = (ByteOrder)oldCombo->currentIndex();
        if (auto oldSpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 2)))
            currLen = oldSpin->value();
        setupOrderOrLength(t, currOrder, currLen);
        onParamChanged();
    });

    // 数组 (3)
    auto arraySpin = new QSpinBox;
    arraySpin->setRange(1, 99999);
    arraySpin->setValue(param.arrayCount);
    arraySpin->setToolTip("1=单个值, >1=同类型数组(如1600个Int16)");
    table->setCellWidget(row, 3, arraySpin);
    connect(arraySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);

    // 默认值 (4)
    auto *itemDefault = new QTableWidgetItem(param.defaultValue);
    table->setItem(row, 4, itemDefault);

    // 动态类型 (5)
    auto dynCombo = new QComboBox;
    dynCombo->setFocusPolicy(Qt::StrongFocus);
    for (int i = 0; i <= (int)DynamicType::Sequence; ++i)
        dynCombo->addItem(ProtocolParam::dynamicTypeToString((DynamicType)i));
    dynCombo->setCurrentIndex((int)param.dynamicType);
    table->setCellWidget(row, 5, dynCombo);
    connect(dynCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolEditDialog::onParamChanged);

    // 动态参数 (6)
    auto dynSpin = new QSpinBox;
    dynSpin->setRange(0, 999999);
    dynSpin->setValue(param.dynamicParam);
    table->setCellWidget(row, 6, dynSpin);
    connect(dynSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);

    // 根据动态类型更新动态参数提示
    auto updateDynToolTip = [dynSpin](int dynIdx) {
        DynamicType dt = (DynamicType)dynIdx;
        switch (dt) {
        case DynamicType::None:     dynSpin->setToolTip(""); break;
        case DynamicType::Timestamp: dynSpin->setToolTip("0=秒 1=毫秒"); break;
        case DynamicType::Length:   dynSpin->setToolTip("0=数据区长度 1=总帧长度(包头+数据区)"); break;
        case DynamicType::Checksum: dynSpin->setToolTip("校验和计算起始偏移"); break;
        case DynamicType::Sequence: dynSpin->setToolTip("保留"); break;
        }
    };
    updateDynToolTip((int)param.dynamicType);
    connect(dynCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [updateDynToolTip](int idx) {
        updateDynToolTip(idx);
    });

    if (isReplyTable) {
        // 随机 (7)
        auto randCheck = new QCheckBox;
        randCheck->setChecked(param.isRandom);
        table->setCellWidget(row, 7, randCheck);
        connect(randCheck, &QCheckBox::toggled, this, &ProtocolEditDialog::onParamChanged);

        // 随机最小 (8)
        auto *itemRandMin = new QTableWidgetItem(param.randomMin);
        table->setItem(row, 8, itemRandMin);

        // 随机最大 (9)
        auto *itemRandMax = new QTableWidgetItem(param.randomMax);
        table->setItem(row, 9, itemRandMax);

        // 随机长度 (10)
        auto randLenSpin = new QSpinBox;
        randLenSpin->setRange(1, 9999);
        randLenSpin->setValue(param.randomLength);
        table->setCellWidget(row, 10, randLenSpin);
        connect(randLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    } else {
        // 匹配 (7)
        auto matchCheck = new QCheckBox;
        matchCheck->setChecked(param.matchEnabled);
        table->setCellWidget(row, 7, matchCheck);
        connect(matchCheck, &QCheckBox::toggled, [this, table](bool) {
            updateMatchHighlight(table);
            onParamChanged();
        });

        // 匹配模式 (8)
        auto matchModeCombo = new QComboBox;
        matchModeCombo->setFocusPolicy(Qt::StrongFocus);
        for (int i = 0; i <= (int)MatchMode::Any; ++i)
            matchModeCombo->addItem(ProtocolParam::matchModeToString((MatchMode)i));
        matchModeCombo->setCurrentIndex((int)param.matchMode);
        table->setCellWidget(row, 8, matchModeCombo);
        connect(matchModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolEditDialog::onParamChanged);

        // 匹配值 (9)
        auto *itemMatchVal = new QTableWidgetItem(param.matchValue);
        table->setItem(row, 9, itemMatchVal);

        // 匹配值2 (10)
        auto *itemMatchVal2 = new QTableWidgetItem(param.matchValue2);
        table->setItem(row, 10, itemMatchVal2);
    }
}

ProtocolParam ProtocolEditDialog::readParamRow(QTableWidget *table, int row, bool isReplyTable)
{
    ProtocolParam param;

    param.name = table->item(row, 0) ? table->item(row, 0)->text() : "";

    auto typeCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 1));
    if (typeCombo) param.type = (ParamType)typeCombo->currentIndex();

    if (isRawByteType(param.type)) {
        auto lenSpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 2));
        if (lenSpin) param.userLength = lenSpin->value();
    } else {
        auto orderCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 2));
        if (orderCombo) param.byteOrder = (ByteOrder)orderCombo->currentIndex();
    }

    auto arraySpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 3));
    if (arraySpin) param.arrayCount = arraySpin->value();

    param.defaultValue = table->item(row, 4) ? table->item(row, 4)->text() : "";

    auto dynCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 5));
    if (dynCombo) param.dynamicType = (DynamicType)dynCombo->currentIndex();

    auto dynSpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 6));
    if (dynSpin) param.dynamicParam = dynSpin->value();

    if (isReplyTable) {
        auto randCheck = qobject_cast<QCheckBox*>(table->cellWidget(row, 7));
        if (randCheck) param.isRandom = randCheck->isChecked();

        param.randomMin = table->item(row, 8) ? table->item(row, 8)->text() : "";
        param.randomMax = table->item(row, 9) ? table->item(row, 9)->text() : "";

        auto randLenSpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 10));
        if (randLenSpin) param.randomLength = randLenSpin->value();
    } else {
        auto matchCheck = qobject_cast<QCheckBox*>(table->cellWidget(row, 7));
        if (matchCheck) param.matchEnabled = matchCheck->isChecked();

        auto matchModeCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 8));
        if (matchModeCombo) param.matchMode = (MatchMode)matchModeCombo->currentIndex();

        param.matchValue = table->item(row, 9) ? table->item(row, 9)->text() : "";
        param.matchValue2 = table->item(row, 10) ? table->item(row, 10)->text() : "";
    }

    return param;
}

void ProtocolEditDialog::addParamRow(QTableWidget *table, bool isReplyTable)
{
    int row = table->rowCount();
    table->insertRow(row);
    ProtocolParam param;
    param.name = QString("参数%1").arg(row + 1);
    populateParamRow(table, row, param, isReplyTable);
}

void ProtocolEditDialog::saveProtocol()
{
    m_proto.name = m_nameEdit->text().trimmed();
    m_proto.description = m_descEdit->text().trimmed();
    m_proto.isActivePush = m_activePushCheck->isChecked();
    m_proto.pushIntervalMs = m_pushIntervalSpin->value();
    m_proto.fixedFrameLength = m_fixedFrameLenSpin->value();
    m_proto.stopAllPeriodicOnMatch = m_stopAllCheck->isChecked();

    // 保存操作配置中选中的协议
    m_proto.stopPeriodicProtocolNames.clear();
    for (int i = 0; i < m_stopList->count(); ++i) {
        auto item = m_stopList->item(i);
        if (item->checkState() == Qt::Checked)
            m_proto.stopPeriodicProtocolNames.append(item->text());
    }

    // 保存帧头参数
    m_proto.headerParams.clear();
    for (int i = 0; i < m_headerTable->rowCount(); ++i)
        m_proto.headerParams.append(readParamRow(m_headerTable, i, false));

    // 保存数据区参数
    m_proto.dataParams.clear();
    for (int i = 0; i < m_dataTable->rowCount(); ++i)
        m_proto.dataParams.append(readParamRow(m_dataTable, i, false));

    // 保存回复配置
    m_proto.replyConfig.mode = (ReplyMode)m_replyModeCombo->currentIndex();
    m_proto.replyConfig.customIntervalMs = m_replyIntervalSpin->value();

    m_proto.replyConfig.headerParams.clear();
    for (int i = 0; i < m_replyHeaderTable->rowCount(); ++i)
        m_proto.replyConfig.headerParams.append(readParamRow(m_replyHeaderTable, i, true));

    m_proto.replyConfig.dataParams.clear();
    for (int i = 0; i < m_replyDataTable->rowCount(); ++i)
        m_proto.replyConfig.dataParams.append(readParamRow(m_replyDataTable, i, true));
}

ProtocolConfig ProtocolEditDialog::getProtocol() const
{
    return m_proto;
}

void ProtocolEditDialog::onAddHeaderParam()
{
    addParamRow(m_headerTable, false);
    updatePreview();
}

void ProtocolEditDialog::onRemoveHeaderParam()
{
    int row = m_headerTable->currentRow();
    if (row >= 0) m_headerTable->removeRow(row);
    updatePreview();
}

void ProtocolEditDialog::onAddDataParam()
{
    addParamRow(m_dataTable, false);
    updatePreview();
}

void ProtocolEditDialog::onRemoveDataParam()
{
    int row = m_dataTable->currentRow();
    if (row >= 0) m_dataTable->removeRow(row);
    updatePreview();
}

void ProtocolEditDialog::onAddReplyHeaderParam()
{
    addParamRow(m_replyHeaderTable, true);
    updatePreview();
}

void ProtocolEditDialog::onRemoveReplyHeaderParam()
{
    int row = m_replyHeaderTable->currentRow();
    if (row >= 0) m_replyHeaderTable->removeRow(row);
    updatePreview();
}

void ProtocolEditDialog::onAddReplyDataParam()
{
    addParamRow(m_replyDataTable, true);
    updatePreview();
}

void ProtocolEditDialog::onRemoveReplyDataParam()
{
    int row = m_replyDataTable->currentRow();
    if (row >= 0) m_replyDataTable->removeRow(row);
    updatePreview();
}

void ProtocolEditDialog::onReplyModeChanged(int index)
{
    bool isCustom = (index == (int)ReplyMode::PeriodicCustom);
    m_replyIntervalLabel->setVisible(isCustom);
    m_replyIntervalSpin->setVisible(isCustom);
}

void ProtocolEditDialog::onPreviewTimer()
{
    updatePreview();
}

bool ProtocolEditDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        QWidget *focus = QApplication::focusWidget();
        if (qobject_cast<QComboBox*>(focus)) {
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

void ProtocolEditDialog::onParamChanged()
{
    if (m_loading) return;
    updateMatchHighlight(m_headerTable);
    updateMatchHighlight(m_dataTable);
    updatePreview();
}

void ProtocolEditDialog::onCopyHeaderToReply()
{
    m_loading = true;
    m_replyHeaderTable->setRowCount(0);
    for (int i = 0; i < m_headerTable->rowCount(); ++i) {
        ProtocolParam p = readParamRow(m_headerTable, i, false);
        p.matchEnabled = false;
        p.matchMode = MatchMode::Exact;
        p.matchValue = "";
        p.matchValue2 = "";
        p.isRandom = false;
        p.randomMin = "";
        p.randomMax = "";
        p.randomLength = 1;
        int row = m_replyHeaderTable->rowCount();
        m_replyHeaderTable->insertRow(row);
        populateParamRow(m_replyHeaderTable, row, p, true);
    }
    m_loading = false;
    updatePreview();
}

void ProtocolEditDialog::onCopyDataToReply()
{
    m_loading = true;
    m_replyDataTable->setRowCount(0);
    for (int i = 0; i < m_dataTable->rowCount(); ++i) {
        ProtocolParam p = readParamRow(m_dataTable, i, false);
        p.matchEnabled = false;
        p.matchMode = MatchMode::Exact;
        p.matchValue = "";
        p.matchValue2 = "";
        p.isRandom = false;
        p.randomMin = "";
        p.randomMax = "";
        p.randomLength = 1;
        int row = m_replyDataTable->rowCount();
        m_replyDataTable->insertRow(row);
        populateParamRow(m_replyDataTable, row, p, true);
    }
    m_loading = false;
    updatePreview();
}

void ProtocolEditDialog::onCopyRecvToReply()
{
    onCopyHeaderToReply();
    onCopyDataToReply();
}

void ProtocolEditDialog::onSwapByteOrder()
{
    m_loading = true;
    auto swapTable = [](QTableWidget *table) {
        for (int i = 0; i < table->rowCount(); ++i) {
            auto combo = qobject_cast<QComboBox*>(table->cellWidget(i, 2));
            if (combo) {
                int idx = combo->currentIndex();
                combo->setCurrentIndex(idx == 0 ? 1 : 0);
            }
        }
    };
    swapTable(m_headerTable);
    swapTable(m_dataTable);
    swapTable(m_replyHeaderTable);
    swapTable(m_replyDataTable);
    m_loading = false;
    updatePreview();
}

// ================== 右键菜单 ==================

void ProtocolEditDialog::onHeaderTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_headerTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_headerTable, false);
    else if (selected == actCopyAll) copyTableAll(m_headerTable, false);
    else if (selected == actPaste) pasteToTable(m_headerTable, false);
    else if (selected == actClear) { m_headerTable->setRowCount(0); updatePreview(); }
    else if (selected == actDel) onRemoveHeaderParam();
}

void ProtocolEditDialog::onDataTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_dataTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_dataTable, false);
    else if (selected == actCopyAll) copyTableAll(m_dataTable, false);
    else if (selected == actPaste) pasteToTable(m_dataTable, false);
    else if (selected == actClear) { m_dataTable->setRowCount(0); updatePreview(); }
    else if (selected == actDel) onRemoveDataParam();
}

void ProtocolEditDialog::onReplyHeaderTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_replyHeaderTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_replyHeaderTable, true);
    else if (selected == actCopyAll) copyTableAll(m_replyHeaderTable, true);
    else if (selected == actPaste) pasteToTable(m_replyHeaderTable, true);
    else if (selected == actClear) { m_replyHeaderTable->setRowCount(0); updatePreview(); }
    else if (selected == actDel) onRemoveReplyHeaderParam();
}

void ProtocolEditDialog::onReplyDataTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_replyDataTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_replyDataTable, true);
    else if (selected == actCopyAll) copyTableAll(m_replyDataTable, true);
    else if (selected == actPaste) pasteToTable(m_replyDataTable, true);
    else if (selected == actClear) { m_replyDataTable->setRowCount(0); updatePreview(); }
    else if (selected == actDel) onRemoveReplyDataParam();
}

// ================== 复制粘贴实现 ==================

QByteArray ProtocolEditDialog::paramsToClipboardData(const QVector<ProtocolParam> &params, bool isReplyTable)
{
    QJsonArray arr;
    for (const auto &p : params) {
        QJsonObject obj;
        obj["name"] = p.name;
        obj["type"] = (int)p.type;
        obj["byteOrder"] = (int)p.byteOrder;
        obj["userLength"] = p.userLength;
        obj["arrayCount"] = p.arrayCount;
        obj["defaultValue"] = p.defaultValue;
        obj["dynamicType"] = (int)p.dynamicType;
        obj["dynamicParam"] = p.dynamicParam;
        obj["matchEnabled"] = p.matchEnabled;
        obj["matchMode"] = (int)p.matchMode;
        obj["matchValue"] = p.matchValue;
        obj["matchValue2"] = p.matchValue2;
        obj["isRandom"] = p.isRandom;
        obj["randomMin"] = p.randomMin;
        obj["randomMax"] = p.randomMax;
        obj["randomLength"] = p.randomLength;
        arr.append(obj);
    }
    QJsonObject root;
    root["version"] = 1;
    root["isReplyTable"] = isReplyTable;
    root["params"] = arr;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QVector<ProtocolParam> ProtocolEditDialog::paramsFromClipboardData(const QByteArray &data, bool &outIsReplyTable)
{
    outIsReplyTable = false;
    QVector<ProtocolParam> result;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return result;
    QJsonObject root = doc.object();
    outIsReplyTable = root["isReplyTable"].toBool();
    QJsonArray arr = root["params"].toArray();
    for (const auto &v : arr) {
        if (!v.isObject()) continue;
        QJsonObject obj = v.toObject();
        ProtocolParam p;
        p.name = obj["name"].toString();
        p.type = (ParamType)obj["type"].toInt();
        p.byteOrder = (ByteOrder)obj["byteOrder"].toInt();
        p.userLength = obj["userLength"].toInt(0);
        p.arrayCount = obj["arrayCount"].toInt(1);
        p.defaultValue = obj["defaultValue"].toString();
        p.dynamicType = (DynamicType)obj["dynamicType"].toInt();
        p.dynamicParam = obj["dynamicParam"].toInt();
        p.matchEnabled = obj["matchEnabled"].toBool();
        p.matchMode = (MatchMode)obj["matchMode"].toInt();
        p.matchValue = obj["matchValue"].toString();
        p.matchValue2 = obj["matchValue2"].toString();
        p.isRandom = obj["isRandom"].toBool();
        p.randomMin = obj["randomMin"].toString();
        p.randomMax = obj["randomMax"].toString();
        p.randomLength = obj["randomLength"].toInt(1);
        result.append(p);
    }
    return result;
}

void ProtocolEditDialog::copyTableSelection(QTableWidget *table, bool isReplyTable)
{
    QVector<ProtocolParam> params;
    QSet<int> selectedRows;
    for (const auto &item : table->selectedItems())
        selectedRows.insert(item->row());
    QList<int> rows = selectedRows.values();
    std::sort(rows.begin(), rows.end());
    for (int r : rows)
        params.append(readParamRow(table, r, isReplyTable));

    if (params.isEmpty()) return;
    QByteArray data = paramsToClipboardData(params, isReplyTable);
    QMimeData *mime = new QMimeData;
    mime->setData("application/x-protocolsim-params", data);
    mime->setText(QString::fromUtf8(data));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void ProtocolEditDialog::copyTableAll(QTableWidget *table, bool isReplyTable)
{
    QVector<ProtocolParam> params;
    for (int i = 0; i < table->rowCount(); ++i)
        params.append(readParamRow(table, i, isReplyTable));
    if (params.isEmpty()) return;
    QByteArray data = paramsToClipboardData(params, isReplyTable);
    QMimeData *mime = new QMimeData;
    mime->setData("application/x-protocolsim-params", data);
    mime->setText(QString::fromUtf8(data));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void ProtocolEditDialog::pasteToTable(QTableWidget *table, bool isReplyTable)
{
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime) return;

    QByteArray data;
    if (mime->hasFormat("application/x-protocolsim-params"))
        data = mime->data("application/x-protocolsim-params");
    else if (mime->hasText())
        data = mime->text().toUtf8();
    else
        return;

    bool srcIsReply = false;
    QVector<ProtocolParam> params = paramsFromClipboardData(data, srcIsReply);
    if (params.isEmpty()) return;

    m_loading = true;
    for (const auto &p : params) {
        ProtocolParam np = p;
        // 如果源和目标表类型不同，做字段适配
        if (isReplyTable && !srcIsReply) {
            // 从接收表粘贴到回复表：清除匹配字段
            np.matchEnabled = false;
            np.matchMode = MatchMode::Exact;
            np.matchValue = "";
            np.matchValue2 = "";
        } else if (!isReplyTable && srcIsReply) {
            // 从回复表粘贴到接收表：清除随机字段
            np.isRandom = false;
            np.randomMin = "";
            np.randomMax = "";
            np.randomLength = 1;
        }
        int row = table->rowCount();
        table->insertRow(row);
        populateParamRow(table, row, np, isReplyTable);
    }
    m_loading = false;
    updatePreview();
}

void ProtocolEditDialog::updateMatchHighlight(QTableWidget *table)
{
    for (int i = 0; i < table->rowCount(); ++i) {
        auto check = qobject_cast<QCheckBox*>(table->cellWidget(i, 7));
        bool matched = check && check->isChecked();
        QColor bg = matched ? QColor(200, 255, 200) : QColor(Qt::white);
        for (int c = 0; c < table->columnCount(); ++c) {
            if (auto item = table->item(i, c))
                item->setBackground(bg);
        }
    }
}

void ProtocolEditDialog::updatePreview()
{
    saveProtocol();

    // 构建接收帧预览 (HTML格式)
    QByteArray frame = m_proto.buildFrame(0);
    QString hex = QString::fromLatin1(frame.toHex(' '));

    QString detail = "<pre style='font-family: monospace; font-size: 13px; margin: 0;'>";
    int offset = 0;
    detail += "<b>帧头:</b><br>";
    for (const auto &p : m_proto.headerParams) {
        int sz = p.byteSize();
        QByteArray fieldData = frame.mid(offset, sz);
        QString fieldHex = QString::fromLatin1(fieldData.toHex(' '));

        QString arrayInfo;
        if (p.arrayCount > 1)
            arrayInfo = QString("<font color='teal'> (%1[%2]) </font>")
                        .arg(ProtocolParam::typeToString(p.type)).arg(p.arrayCount);

        QString matchInfo;
        QString bgStyle;
        if (p.matchEnabled) {
            bgStyle = " style='background-color: #E8F5E9;'";
            QString matchModeStr = ProtocolParam::matchModeToString(p.matchMode);
            matchInfo = QString("<font color='green'> [匹配:%1]</font>").arg(matchModeStr);
            if (p.matchMode == MatchMode::Range) {
                matchInfo += QString("<font color='blue'> 期望范围:[%2, %3]</font>")
                             .arg(p.matchValue).arg(p.matchValue2);
            } else if (p.matchMode == MatchMode::Mask) {
                matchInfo += QString("<font color='blue'> 掩码:%2 期望:%3</font>")
                             .arg(p.matchValue).arg(p.matchValue2);
            } else if (!p.matchValue.isEmpty()) {
                matchInfo += QString("<font color='blue'> 期望:%2</font>").arg(p.matchValue);
            }
        }

        detail += QString("<span%1>  偏移:%2  %3 (%4)  %5字节  值:%6%7%8</span><br>")
                  .arg(bgStyle).arg(offset).arg(p.name)
                  .arg(ProtocolParam::typeToString(p.type))
                  .arg(sz).arg(fieldHex).arg(arrayInfo).arg(matchInfo);
        offset += sz;
    }
    detail += "<b>数据区:</b><br>";
    for (const auto &p : m_proto.dataParams) {
        int sz = p.byteSize();
        QByteArray fieldData = frame.mid(offset, sz);
        QString fieldHex = QString::fromLatin1(fieldData.toHex(' '));

        QString arrayInfo;
        if (p.arrayCount > 1)
            arrayInfo = QString("<font color='teal'> (%1[%2]) </font>")
                        .arg(ProtocolParam::typeToString(p.type)).arg(p.arrayCount);

        QString matchInfo;
        QString bgStyle;
        if (p.matchEnabled) {
            bgStyle = " style='background-color: #E8F5E9;'";
            QString matchModeStr = ProtocolParam::matchModeToString(p.matchMode);
            matchInfo = QString("<font color='green'> [匹配:%1]</font>").arg(matchModeStr);
            if (p.matchMode == MatchMode::Range) {
                matchInfo += QString("<font color='blue'> 期望范围:[%2, %3]</font>")
                             .arg(p.matchValue).arg(p.matchValue2);
            } else if (p.matchMode == MatchMode::Mask) {
                matchInfo += QString("<font color='blue'> 掩码:%2 期望:%3</font>")
                             .arg(p.matchValue).arg(p.matchValue2);
            } else if (!p.matchValue.isEmpty()) {
                matchInfo += QString("<font color='blue'> 期望:%2</font>").arg(p.matchValue);
            }
        }

        detail += QString("<span%1>  偏移:%2  %3 (%4)  %5字节  值:%6%7%8</span><br>")
                  .arg(bgStyle).arg(offset).arg(p.name)
                  .arg(ProtocolParam::typeToString(p.type))
                  .arg(sz).arg(fieldHex).arg(arrayInfo).arg(matchInfo);
        offset += sz;
    }
    detail += QString("<br><b>完整帧 (%1字节):</b> %2").arg(frame.size()).arg(hex);
    if (m_proto.fixedFrameLength > 0)
        detail += QString("<br><font color='orange'>固定帧长度: %1字节 (匹配时按此长度消费buffer)</font>").arg(m_proto.fixedFrameLength);
    detail += "</pre>";
    m_previewLabel->setText(detail);

    // 构建回复帧预览 (HTML格式)
    QByteArray replyFrame = m_proto.buildReplyFrame(0);
    QString replyHex = QString::fromLatin1(replyFrame.toHex(' '));

    QString replyDetail = "<pre style='font-family: monospace; font-size: 13px; margin: 0;'>";
    int roffset = 0;
    for (const auto &p : m_proto.replyConfig.headerParams) {
        int sz = p.isRandom ? p.randomLength : p.byteSize();
        if (sz <= 0) sz = p.byteSize();
        QByteArray fieldData = replyFrame.mid(roffset, sz);

        QString arrayInfo;
        if (p.arrayCount > 1)
            arrayInfo = QString("<font color='teal'> (%1[%2]) </font>")
                        .arg(ProtocolParam::typeToString(p.type)).arg(p.arrayCount);

        QString randInfo;
        QString bgStyle;
        if (p.isRandom) {
            bgStyle = " style='background-color: #E3F2FD;'";
            randInfo = QString("<font color='orange'> [随机]</font>");
            if (!p.randomMin.isEmpty() || !p.randomMax.isEmpty()) {
                randInfo += QString("<font color='purple'> 范围:[%1, %2]</font>")
                            .arg(p.randomMin.isEmpty() ? "0" : p.randomMin)
                            .arg(p.randomMax.isEmpty() ? "max" : p.randomMax);
            }
        }

        replyDetail += QString("<span%1>  偏移:%2  %3  值:%4%5%6</span><br>")
                       .arg(bgStyle).arg(roffset).arg(p.name)
                       .arg(QString::fromLatin1(fieldData.toHex(' '))).arg(arrayInfo).arg(randInfo);
        roffset += sz;
    }
    for (const auto &p : m_proto.replyConfig.dataParams) {
        int sz = p.isRandom ? p.randomLength : p.byteSize();
        if (sz <= 0) sz = p.byteSize();
        QByteArray fieldData = replyFrame.mid(roffset, sz);

        QString arrayInfo;
        if (p.arrayCount > 1)
            arrayInfo = QString("<font color='teal'> (%1[%2]) </font>")
                        .arg(ProtocolParam::typeToString(p.type)).arg(p.arrayCount);

        QString randInfo;
        QString bgStyle;
        if (p.isRandom) {
            bgStyle = " style='background-color: #E3F2FD;'";
            randInfo = QString("<font color='orange'> [随机]</font>");
            if (!p.randomMin.isEmpty() || !p.randomMax.isEmpty()) {
                randInfo += QString("<font color='purple'> 范围:[%1, %2]</font>")
                            .arg(p.randomMin.isEmpty() ? "0" : p.randomMin)
                            .arg(p.randomMax.isEmpty() ? "max" : p.randomMax);
            }
        }

        replyDetail += QString("<span%1>  偏移:%2  %3  值:%4%5%6</span><br>")
                       .arg(bgStyle).arg(roffset).arg(p.name)
                       .arg(QString::fromLatin1(fieldData.toHex(' '))).arg(arrayInfo).arg(randInfo);
        roffset += sz;
    }
    replyDetail += QString("<br><b>完整回复帧 (%1字节):</b> %2").arg(replyFrame.size()).arg(replyHex);
    replyDetail += "</pre>";
    m_replyPreviewLabel->setText(replyDetail);
}
