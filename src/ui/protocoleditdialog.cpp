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
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTableWidgetItem>
#include <QInputDialog>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QFont>
#include "collapsiblegroupbox.h"

// ================== 数组编辑对话框(内部类, 无需Q_OBJECT) ==================
class ArrayEditDialog : public QDialog
{
public:
    ArrayEditDialog(int count, const QStringList &values, const QString &typeName, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QString("编辑数组值 (%1个 %2)").arg(count).arg(typeName));
        resize(420, 500);
        auto lay = new QVBoxLayout(this);

        lay->addWidget(new QLabel(QString("共 %1 个元素, 每行一个值:").arg(count)));

        m_table = new QTableWidget(count, 2);
        m_table->setHorizontalHeaderLabels({"索引", "值"});
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int i = 0; i < count; ++i) {
            auto idxItem = new QTableWidgetItem(QString::number(i));
            idxItem->setFlags(Qt::NoItemFlags);
            idxItem->setBackground(QColor(240, 240, 240));
            m_table->setItem(i, 0, idxItem);
            m_table->setItem(i, 1, new QTableWidgetItem(i < values.size() ? values[i] : ""));
        }
        lay->addWidget(m_table);

        auto fillLay = new QHBoxLayout;
        fillLay->addWidget(new QLabel("全部填充为:"));
        m_fillEdit = new QLineEdit;
        fillLay->addWidget(m_fillEdit);
        auto fillBtn = new QPushButton("填充");
        fillLay->addWidget(fillBtn);
        lay->addLayout(fillLay);
        connect(fillBtn, &QPushButton::clicked, [this]() {
            QString v = m_fillEdit->text();
            for (int i = 0; i < m_table->rowCount(); ++i)
                m_table->item(i, 1)->setText(v);
        });

        // 序列填充(等差/线性)
        auto seqLay = new QHBoxLayout;
        seqLay->addWidget(new QLabel("序列: 起="));
        m_seqStart = new QLineEdit; m_seqStart->setMaximumWidth(70);
        seqLay->addWidget(m_seqStart);
        seqLay->addWidget(new QLabel("步="));
        m_seqStep = new QLineEdit; m_seqStep->setMaximumWidth(70);
        seqLay->addWidget(m_seqStep);
        auto seqBtn = new QPushButton("生成序列");
        seqLay->addWidget(seqBtn);
        seqLay->addStretch();
        lay->addLayout(seqLay);
        connect(seqBtn, &QPushButton::clicked, [this]() {
            bool ok1, ok2;
            double start = m_seqStart->text().toDouble(&ok1);
            double step = m_seqStep->text().toDouble(&ok2);
            if (!ok1 || !ok2) return;
            for (int i = 0; i < m_table->rowCount(); ++i) {
                double v = start + step * i;
                m_table->item(i, 1)->setText(QString::number(v));
            }
        });

        auto btnLay = new QHBoxLayout;
        btnLay->addStretch();
        auto ok = new QPushButton("确定");
        auto cancel = new QPushButton("取消");
        btnLay->addWidget(ok);
        btnLay->addWidget(cancel);
        lay->addLayout(btnLay);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    QStringList values() const
    {
        QStringList r;
        for (int i = 0; i < m_table->rowCount(); ++i)
            r << m_table->item(i, 1)->text();
        return r;
    }

private:
    QTableWidget *m_table;
    QLineEdit *m_fillEdit;
    QLineEdit *m_seqStart;
    QLineEdit *m_seqStep;
};

// ================== 大数据编辑对话框(内部类, 无需Q_OBJECT) ==================
// 用于Hex/Bytes类型参数的大块数据编辑: 显示当前已有hex, 支持从文件加载并选择字节范围
// 典型场景: 分包负载每包8000字节, 从大文件中按 [起始, 结束) 截取本包数据
class LargeDataEditDialog : public QDialog
{
public:
    LargeDataEditDialog(const QByteArray &currentData, QWidget *parent = nullptr)
        : QDialog(parent), m_data(currentData)
    {
        setWindowTitle("大数据编辑 (文件加载/字节范围选择)");
        resize(720, 600);
        auto lay = new QVBoxLayout(this);

        // 当前Hex内容(hexdump格式: 偏移 | 16字节Hex | ASCII)
        lay->addWidget(new QLabel("当前数据 (偏移 | Hex字节 | ASCII):"));
        m_hexView = new QPlainTextEdit;
        m_hexView->setReadOnly(true);
        m_hexView->setFont(QFont("Monospace"));
        m_hexView->setLineWrapMode(QPlainTextEdit::NoWrap);
        lay->addWidget(m_hexView);

        // 文件加载按钮 + 文件信息
        auto fileLay = new QHBoxLayout;
        auto btnFile = new QPushButton("选择文件...");
        btnFile->setToolTip("从文件加载二进制数据, 加载后可在下方选择字节范围");
        m_fileLabel = new QLabel("未加载文件 (显示当前已有数据)");
        fileLay->addWidget(btnFile);
        fileLay->addWidget(m_fileLabel, 1);
        lay->addLayout(fileLay);

        // 字节范围选择
        auto rangeLay = new QHBoxLayout;
        rangeLay->addWidget(new QLabel("起始字节:"));
        m_startSpin = new QSpinBox;
        m_startSpin->setRange(0, 0);
        m_startSpin->setToolTip("选中范围的起始字节偏移(含此字节, 0-based)");
        rangeLay->addWidget(m_startSpin);
        rangeLay->addWidget(new QLabel("结束字节:"));
        m_endSpin = new QSpinBox;
        m_endSpin->setRange(0, 0);
        m_endSpin->setToolTip("选中范围的结束字节偏移(不含此字节, 即区间 [start, end))");
        rangeLay->addWidget(m_endSpin);
        auto btnAll = new QPushButton("全选");
        btnAll->setToolTip("选中整个数据范围 [0, 总大小)");
        rangeLay->addWidget(btnAll);
        rangeLay->addStretch();
        lay->addLayout(rangeLay);

        // 选中范围预览
        lay->addWidget(new QLabel("选中范围预览:"));
        m_rangePreview = new QPlainTextEdit;
        m_rangePreview->setReadOnly(true);
        m_rangePreview->setFont(QFont("Monospace"));
        m_rangePreview->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_rangePreview->setMaximumBlockCount(2000);
        lay->addWidget(m_rangePreview);

        // 统计信息
        m_statLabel = new QLabel;
        lay->addWidget(m_statLabel);

        // 按钮
        auto btnLay = new QHBoxLayout;
        btnLay->addStretch();
        auto ok = new QPushButton("确定");
        auto cancel = new QPushButton("取消");
        btnLay->addWidget(ok);
        btnLay->addWidget(cancel);
        lay->addLayout(btnLay);

        // 初始化显示
        refreshHexView();
        m_startSpin->setRange(0, m_data.size());
        m_endSpin->setRange(0, m_data.size());
        m_startSpin->setValue(0);
        m_endSpin->setValue(m_data.size());
        updateRangePreview();

        // 信号连接(lambda, 无需Q_OBJECT)
        connect(btnFile, &QPushButton::clicked, this, [this]() { onSelectFile(); });
        connect(m_startSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updateRangePreview(); });
        connect(m_endSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { updateRangePreview(); });
        connect(btnAll, &QPushButton::clicked, this, [this]() {
            m_startSpin->setValue(0);
            m_endSpin->setValue(m_data.size());
        });
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    }

    // 返回选中范围的Hex字符串(空格分隔), 供回填QLineEdit
    QString selectedHex() const {
        int start = qMin(m_startSpin->value(), m_endSpin->value());
        int end = qMax(m_startSpin->value(), m_endSpin->value());
        QByteArray sub = m_data.mid(start, end - start);
        return QString::fromLatin1(sub.toHex(' '));
    }

    // 返回选中字节数
    int selectedSize() const {
        return qAbs(m_endSpin->value() - m_startSpin->value());
    }

private:
    QByteArray m_data;
    QPlainTextEdit *m_hexView;
    QLabel *m_fileLabel;
    QSpinBox *m_startSpin;
    QSpinBox *m_endSpin;
    QPlainTextEdit *m_rangePreview;
    QLabel *m_statLabel;

    void onSelectFile() {
        QString path = QFileDialog::getOpenFileName(this, "选择数据文件", QString(),
                                                    "所有文件(*.*);;二进制(*.bin *.dat);;图片(*.png *.jpg *.bmp)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "加载失败",
                                 QString("无法打开文件:\n%1\n\n请检查文件是否存在或被占用。").arg(path));
            return;
        }
        QByteArray data = f.readAll();
        f.close();
        if (data.isEmpty()) {
            QMessageBox::warning(this, "加载失败",
                                 QString("文件为空:\n%1\n\n不会用0填充, 请选择有效数据文件。").arg(path));
            return;
        }
        m_data = data;
        m_fileLabel->setText(QString("已加载: %1 (总大小 %2字节)").arg(QFileInfo(path).fileName()).arg(data.size()));
        refreshHexView();
        m_startSpin->setRange(0, m_data.size());
        m_endSpin->setRange(0, m_data.size());
        m_startSpin->setValue(0);
        m_endSpin->setValue(m_data.size());
        updateRangePreview();
    }

    void refreshHexView() {
        m_hexView->setPlainText(formatHexDump(m_data));
    }

    void updateRangePreview() {
        int start = qMin(m_startSpin->value(), m_endSpin->value());
        int end = qMax(m_startSpin->value(), m_endSpin->value());
        QByteArray sub = m_data.mid(start, end - start);
        m_rangePreview->setPlainText(formatHexDump(sub));
        m_statLabel->setText(QString("数据总大小: %1字节 | 选中范围: [%2, %3) | 将填充字节数: %4")
                             .arg(m_data.size()).arg(start).arg(end).arg(sub.size()));
    }

    // hexdump格式: "0000: 41 54 2E ... |AT.|"
    // 超过4KB只显示前4KB避免卡顿, 末尾标注总字节数
    static QString formatHexDump(const QByteArray &data) {
        const int maxShow = 4096;
        int showBytes = qMin(data.size(), maxShow);
        QString result;
        result.reserve(showBytes * 4);
        for (int i = 0; i < showBytes; i += 16) {
            QString line = QString("%1: ").arg(i, 4, 16, QChar('0')).toUpper();
            for (int j = 0; j < 16; ++j) {
                if (i + j < showBytes)
                    line += QString("%1 ").arg((quint8)data[i+j], 2, 16, QChar('0')).toUpper();
                else
                    line += "   ";
            }
            line += "|";
            for (int j = 0; j < 16; ++j) {
                if (i + j < showBytes) {
                    char c = data[i+j];
                    line += (c >= 32 && c < 127) ? QChar::fromLatin1(c) : QChar('.');
                }
            }
            line += "|";
            result += line;
            result += "\n";
        }
        if (data.size() > maxShow)
            result += QString("... (共 %1 字节, 仅显示前 %2 字节)").arg(data.size()).arg(maxShow);
        return result;
    }
};

ProtocolEditDialog::ProtocolEditDialog(ProtocolConfig &proto, QVector<ProtocolConfig> *allProtocols, QWidget *parent)
    : QDialog(parent), m_proto(proto), m_allProtocols(allProtocols), m_loading(false), wheelFilter(new WheelEventFilter(this))
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
    m_replyModeCombo->addItems({"不回复", "回复一次", "1秒周期回复", "5秒周期回复", "自定义周期回复", "多包回复"});
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
    m_rplHdrGroup = new CollapsibleGroupBox("回复帧头参数 (右键支持复制/粘贴)");
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
    m_rplHdrGroup->setContentWidget(rplHdrContent);
    replyLayout->addWidget(m_rplHdrGroup);

    // 回复数据区参数
    m_rplDataGroup = new CollapsibleGroupBox("回复数据区参数 (右键支持复制/粘贴)");
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
    m_rplDataGroup->setContentWidget(rplDataContent);
    replyLayout->addWidget(m_rplDataGroup);

    auto rplUtilLayout = new QHBoxLayout;
    auto btnRplSwap = new QPushButton("一键切换所有字段大小端");
    rplUtilLayout->addWidget(btnRplSwap);
    rplUtilLayout->addStretch();
    replyLayout->addLayout(rplUtilLayout);

    // ====== 多包配置(多包回复模式: 发送区帧发送后, 按列表顺序逐包下发; 每包独立配置字段) ======
    m_multiPktGroup = new CollapsibleGroupBox("多包配置 (多包回复模式: 发送区帧后依次发送各包; 每包独立配置, 字段动态类型可设=包序号/总包数/包大小)");
    auto mpContent = new QWidget;
    auto mpLayout = new QVBoxLayout(mpContent);

    // 顶部: 包间间隔 + 循环开关
    auto mpTopLayout = new QHBoxLayout;
    mpTopLayout->addWidget(new QLabel("包间间隔(ms):"));
    m_mpIntervalSpin = new QSpinBox;
    m_mpIntervalSpin->setRange(0, 999999);
    m_mpIntervalSpin->setValue(100);
    m_mpIntervalSpin->setToolTip("相邻包发送间隔; 单包若设了delayMs则以包级延迟优先");
    mpTopLayout->addWidget(m_mpIntervalSpin);
    m_mpCycleCheck = new QCheckBox("多包循环(配合回复周期: 周期模式下每轮回到包1重发)");
    m_mpCycleCheck->setToolTip("勾选后, 在'1秒/5秒/自定义周期回复'模式下, 每个周期都会重新从包1发送一轮多包");
    mpTopLayout->addWidget(m_mpCycleCheck);
    mpTopLayout->addStretch();
    mpLayout->addLayout(mpTopLayout);

    // 中部: 左侧包列表 + 右侧当前包字段表
    auto mpMidLayout = new QHBoxLayout;
    // 左侧: 包列表
    auto mpLeftLayout = new QVBoxLayout;
    m_mpList = new QListWidget;
    m_mpList->setMaximumWidth(180);
    m_mpList->setAlternatingRowColors(true);
    mpLeftLayout->addWidget(m_mpList);
    auto mpListBtnLayout = new QHBoxLayout;
    auto btnAddMp = new QPushButton("添加包");
    auto btnDelMp = new QPushButton("删除包");
    mpListBtnLayout->addWidget(btnAddMp);
    mpListBtnLayout->addWidget(btnDelMp);
    mpLeftLayout->addLayout(mpListBtnLayout);
    auto mpMoveBtnLayout = new QHBoxLayout;
    auto btnMpUp = new QPushButton("上移");
    auto btnMpDown = new QPushButton("下移");
    mpMoveBtnLayout->addWidget(btnMpUp);
    mpMoveBtnLayout->addWidget(btnMpDown);
    mpLeftLayout->addLayout(mpMoveBtnLayout);
    mpLeftLayout->addStretch();
    mpMidLayout->addLayout(mpLeftLayout, 0);

    // 右侧: 当前选中包的帧头+数据区表格
    auto mpRightLayout = new QVBoxLayout;
    // 包帧头参数
    auto mpHdrGroup = new QGroupBox("当前包帧头参数 (右键复制/粘贴; 动态类型可设PacketIndex/TotalPackets/PacketSize)");
    auto mpHdrLayout = new QVBoxLayout(mpHdrGroup);
    m_mpHdrTable = new QTableWidget(0, 11);
    m_mpHdrTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","随机","随机最小","随机最大","随机长度"});
    for (int i = 0; i < 11; ++i)
        m_mpHdrTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_mpHdrTable->setContextMenuPolicy(Qt::CustomContextMenu);
    mpHdrLayout->addWidget(m_mpHdrTable);
    auto mpHdrBtnLayout = new QHBoxLayout;
    auto btnAddMpHdr = new QPushButton("添加帧头参数");
    auto btnDelMpHdr = new QPushButton("删除选中");
    mpHdrBtnLayout->addWidget(btnAddMpHdr);
    mpHdrBtnLayout->addWidget(btnDelMpHdr);
    mpHdrBtnLayout->addStretch();
    mpHdrLayout->addLayout(mpHdrBtnLayout);
    mpRightLayout->addWidget(mpHdrGroup);

    // 包数据区参数
    auto mpDataGroup = new QGroupBox("当前包数据区参数 (右键复制/粘贴)");
    auto mpDataLayout = new QVBoxLayout(mpDataGroup);
    m_mpDataTable = new QTableWidget(0, 11);
    m_mpDataTable->setHorizontalHeaderLabels({"名称","类型","字节序/长度","数组","默认值","动态类型","动态参数","随机","随机最小","随机最大","随机长度"});
    for (int i = 0; i < 11; ++i)
        m_mpDataTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    m_mpDataTable->setContextMenuPolicy(Qt::CustomContextMenu);
    mpDataLayout->addWidget(m_mpDataTable);
    auto mpDataBtnLayout = new QHBoxLayout;
    auto btnAddMpData = new QPushButton("添加数据区参数");
    auto btnDelMpData = new QPushButton("删除选中");
    mpDataBtnLayout->addWidget(btnAddMpData);
    mpDataBtnLayout->addWidget(btnDelMpData);
    mpDataBtnLayout->addStretch();
    mpDataLayout->addLayout(mpDataBtnLayout);
    mpRightLayout->addWidget(mpDataGroup);
    mpMidLayout->addLayout(mpRightLayout, 1);
    mpLayout->addLayout(mpMidLayout);

    m_multiPktGroup->setContentWidget(mpContent);
//    replyLayout->addWidget(m_multiPktGroup);

    m_mpCurrentIndex = -1;

    tabWidget->addTab(replyTab, "回复配置");
    tabWidget->addTab(m_multiPktGroup, "多包配置");

    connect(btnAddRplHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onAddReplyHeaderParam);
    connect(btnDelRplHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveReplyHeaderParam);
    connect(btnAddRplData, &QPushButton::clicked, this, &ProtocolEditDialog::onAddReplyDataParam);
    connect(btnDelRplData, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveReplyDataParam);
    connect(m_replyModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolEditDialog::onReplyModeChanged);
    connect(m_replyIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    connect(btnRplSwap, &QPushButton::clicked, this, &ProtocolEditDialog::onSwapByteOrder);

    connect(m_replyHeaderTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onReplyHeaderTableContextMenu);
    connect(m_replyDataTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onReplyDataTableContextMenu);

    // 多包配置信号连接
    connect(m_mpIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);
    connect(m_mpCycleCheck, &QCheckBox::toggled, this, &ProtocolEditDialog::onParamChanged);
    connect(btnAddMp, &QPushButton::clicked, this, &ProtocolEditDialog::onAddMultiPacket);
    connect(btnDelMp, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveMultiPacket);
    connect(btnMpUp, &QPushButton::clicked, this, &ProtocolEditDialog::onMoveMultiPacketUp);
    connect(btnMpDown, &QPushButton::clicked, this, &ProtocolEditDialog::onMoveMultiPacketDown);
    connect(m_mpList, &QListWidget::currentRowChanged, this, &ProtocolEditDialog::onMultiPacketSelected);
    connect(btnAddMpHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onAddMpHdr);
    connect(btnDelMpHdr, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveMpHdr);
    connect(btnAddMpData, &QPushButton::clicked, this, &ProtocolEditDialog::onAddMpData);
    connect(btnDelMpData, &QPushButton::clicked, this, &ProtocolEditDialog::onRemoveMpData);
    connect(m_mpHdrTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onMpHdrTableContextMenu);
    connect(m_mpDataTable, &QTableWidget::customContextMenuRequested, this, &ProtocolEditDialog::onMpDataTableContextMenu);
    m_mpHdrTable->installEventFilter(wheelFilter);
    m_mpDataTable->installEventFilter(wheelFilter);

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

    // 回复模式(MultiPacket为独立模式, 发送区帧+多包)
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

    // 多包配置加载
    loadMultiPackets();

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
            orderCombo->installEventFilter(wheelFilter);
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
    typeCombo->installEventFilter(wheelFilter);
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
    arraySpin->setToolTip("1=单个值, >1=同类型数组(如1600个Int16)\n数组>1时, 默认值列变为可编辑数组控件");
    table->setCellWidget(row, 3, arraySpin);
    // 数组个数变化时, 切换默认值列控件(单值文本<->数组按钮)
    connect(arraySpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, table, row](int newCount) {
        // 读取当前默认值
        QString curVal;
        auto btn = qobject_cast<QPushButton*>(table->cellWidget(row, 4));
        if (btn) curVal = btn->property("arrayData").toString();
        else if (auto it = table->item(row, 4)) curVal = it->text();

        if (newCount > 1) {
            // 从单值或已有数组转换为newCount个元素
            QStringList vals;
            if (curVal.trimmed().startsWith('[')) {
                vals = ProtocolParam::parseDefaultValuesFromString(curVal);
            } else if (!curVal.isEmpty()) {
                for (int i = 0; i < newCount; ++i) vals << curVal;
            }
            QString jsonData = QJsonDocument(QJsonArray::fromStringList(vals)).toJson(QJsonDocument::Compact);
            if (vals.isEmpty()) jsonData = "[]";
            setupDefaultValueCell(table, row, newCount, jsonData);
        } else {
            // 数组转单值: 取第一个元素
            QString singleVal = curVal;
            if (curVal.trimmed().startsWith('[')) {
                QStringList vals = ProtocolParam::parseDefaultValuesFromString(curVal);
                singleVal = vals.isEmpty() ? "" : vals.first();
            }
            setupDefaultValueCell(table, row, 1, singleVal);
        }
        onParamChanged();
    });
    connect(arraySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProtocolEditDialog::onParamChanged);

    // 默认值 (4): 根据数组个数选择控件
    setupDefaultValueCell(table, row, param.arrayCount, param.defaultValue);

    // 动态类型 (5)
    auto dynCombo = new QComboBox;
    dynCombo->setFocusPolicy(Qt::StrongFocus);
    dynCombo->installEventFilter(wheelFilter);
    for (int i = 0; i <= (int)DynamicType::TotalPackets; ++i)
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
        case DynamicType::None:         dynSpin->setToolTip(""); break;
        case DynamicType::Timestamp:    dynSpin->setToolTip("0=秒 1=毫秒"); break;
        case DynamicType::Length:       dynSpin->setToolTip("0=数据区长度 1=总帧长度(包头+数据区)"); break;
        case DynamicType::Checksum:     dynSpin->setToolTip("校验和计算起始偏移"); break;
        case DynamicType::Sequence:     dynSpin->setToolTip("保留"); break;
        case DynamicType::PacketIndex:  dynSpin->setToolTip("分包序号(自动填充当前包索引)"); break;
        case DynamicType::PacketSize:   dynSpin->setToolTip("分包大小(自动填充当前包负载字节数)"); break;
        case DynamicType::TotalPackets: dynSpin->setToolTip("总包数(自动填充)"); break;
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
        matchModeCombo->installEventFilter(wheelFilter);
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

    // 默认值列: 数组时是按钮(存arrayData), 单值时是容器(QLineEdit+文件按钮)
    auto defBtn = qobject_cast<QPushButton*>(table->cellWidget(row, 4));
    if (defBtn) {
        param.defaultValue = defBtn->property("arrayData").toString();
    } else {
        // 单值容器: 找里面的QLineEdit
        auto container = qobject_cast<QWidget*>(table->cellWidget(row, 4));
        if (container) {
            auto edit = container->findChild<QLineEdit*>();
            param.defaultValue = edit ? edit->text() : "";
        } else {
            param.defaultValue = table->item(row, 4) ? table->item(row, 4)->text() : "";
        }
    }

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

void ProtocolEditDialog::setupDefaultValueCell(QTableWidget *table, int row, int arrayCount, const QString &defaultValue)
{
    // 移除并释放旧的cellWidget
    QWidget *old = table->cellWidget(row, 4);
    if (old) {
        table->removeCellWidget(row, 4);
        delete old;
    }

    if (arrayCount > 1) {
        // 数组: 用按钮, property存JSON数组字符串
        QString data = defaultValue;
        if (!data.trimmed().startsWith('[')) {
            // 旧单值转成重复N个的数组
            QStringList vals;
            if (!data.isEmpty())
                for (int i = 0; i < arrayCount; ++i) vals << data;
            data = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(vals)).toJson(QJsonDocument::Compact));
        }
        // 校正数组元素个数与arrayCount一致(多截少补)
        QStringList vals = ProtocolParam::parseDefaultValuesFromString(data);
        while (vals.size() < arrayCount) vals << "";
        if (vals.size() > arrayCount) vals = vals.mid(0, arrayCount);
        data = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(vals)).toJson(QJsonDocument::Compact));

        auto btn = new QPushButton;
        btn->setFocusPolicy(Qt::StrongFocus);
        btn->setProperty("arrayData", data);
        int filled = 0;
        for (const auto &v : vals) if (!v.isEmpty()) ++filled;
        btn->setText(QString("编辑数组(%1项,已填%2)").arg(arrayCount).arg(filled));
        btn->setToolTip("点击编辑每个数组元素的值\n支持全部填充、序列生成");
        table->setCellWidget(row, 4, btn);
        // 占位item(保持背景一致/可选中)
        if (!table->item(row, 4)) table->setItem(row, 4, new QTableWidgetItem(""));
        table->item(row, 4)->setFlags(Qt::NoItemFlags);

        connect(btn, &QPushButton::clicked, this, [this, table, row]() {
            editArrayValues(table, row);
        });
    } else {
        // 单值: 用容器(QLineEdit + 文件加载按钮), 支持大块数据(如8000字节负载)
        QWidget *old = table->cellWidget(row, 4);
        if (old) { table->removeCellWidget(row, 4); delete old; }

        auto container = new QWidget;
        auto lay = new QHBoxLayout(container);
        lay->setContentsMargins(2, 0, 2, 0);
        lay->setSpacing(2);
        auto edit = new QLineEdit;
        edit->setText(defaultValue);
        edit->setPlaceholderText("Hex(如 41542E)或文本; 大数据点右侧按钮");
        lay->addWidget(edit);
        auto btnFile = new QToolButton();
        btnFile->setText("...");
        btnFile->setToolTip("打开大数据编辑对话框\n- 显示当前已有hex内容\n- 可从文件加载二进制数据\n- 可选择字节范围[起始,结束)\n- 确定后按选中范围回填Hex");
        btnFile->setMaximumWidth(80);
        lay->addWidget(btnFile);
        table->setCellWidget(row, 4, container);
        if (!table->item(row, 4)) table->setItem(row, 4, new QTableWidgetItem(""));
        table->item(row, 4)->setFlags(Qt::NoItemFlags);

        connect(edit, &QLineEdit::textChanged, this, &ProtocolEditDialog::onParamChanged);
        connect(btnFile, &QToolButton::clicked, this, [this, edit, btnFile]() {
            // 解析QLineEdit中当前的hex内容作为对话框初始数据
            QString curHex = edit->text();
            curHex.remove(' ').remove('\n').remove('\t');
            QByteArray curData;
            if (curHex.startsWith("0x", Qt::CaseInsensitive)) curHex = curHex.mid(2);
            if (!curHex.isEmpty()) {
                curData = QByteArray::fromHex(curHex.toLatin1());
                // 若hex解析为空(可能用户填的是文本), 回退为原始文本字节
                if (curData.isEmpty() && !edit->text().trimmed().isEmpty())
                    curData = edit->text().toLatin1();
            }
            // 弹出大数据编辑对话框: 显示当前hex + 可选文件 + 字节范围选择
            LargeDataEditDialog dlg(curData, this);
            if (dlg.exec() == QDialog::Accepted) {
                QString hex = dlg.selectedHex();
                int sz = dlg.selectedSize();
                edit->setText(hex);
                QString tip = QString("已加载范围: %1字节").arg(sz);
                edit->setToolTip(tip);
                btnFile->setText(QString("文件(%1B)").arg(sz));
                btnFile->setToolTip(tip + "\n点击重新打开编辑对话框");
            }
        });
    }
}

void ProtocolEditDialog::editArrayValues(QTableWidget *table, int row)
{
    auto btn = qobject_cast<QPushButton*>(table->cellWidget(row, 4));
    if (!btn) return;

    auto arraySpin = qobject_cast<QSpinBox*>(table->cellWidget(row, 3));
    int count = arraySpin ? arraySpin->value() : 1;
    if (count <= 1) return;

    QString data = btn->property("arrayData").toString();
    QStringList vals = ProtocolParam::parseDefaultValuesFromString(data);
    while (vals.size() < count) vals << "";

    // 取类型名用于提示
    QString typeName = "值";
    if (auto typeCombo = qobject_cast<QComboBox*>(table->cellWidget(row, 1)))
        typeName = typeCombo->currentText();

    ArrayEditDialog dlg(count, vals, typeName, this);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList newVals = dlg.values();
        while (newVals.size() < count) newVals << "";
        if (newVals.size() > count) newVals = newVals.mid(0, count);
        QString jsonData = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(newVals)).toJson(QJsonDocument::Compact));
        btn->setProperty("arrayData", jsonData);
        int filled = 0;
        for (const auto &v : newVals) if (!v.isEmpty()) ++filled;
        btn->setText(QString("编辑数组(%1项,已填%2)").arg(count).arg(filled));
        onParamChanged();
    }
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

    // 保存分包配置
    saveCurrentMultiPacket();
    m_proto.replyConfig.multiPacketIntervalMs = m_mpIntervalSpin->value();
    m_proto.replyConfig.multiPacketCycle = m_mpCycleCheck->isChecked();
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
    // 发送区(回复帧头/数据区)与多包配置始终显示, 适用于所有回复模式
    if (!m_loading) updatePreview();
}

// ================== 多包配置实现 ==================

void ProtocolEditDialog::refreshMultiPacketList()
{
    m_mpList->blockSignals(true);
    int prev = m_mpCurrentIndex;
    m_mpList->clear();
    for (int i = 0; i < m_proto.replyConfig.multiPackets.size(); ++i) {
        const auto &mp = m_proto.replyConfig.multiPackets[i];
        QString name = mp.name.isEmpty() ? QString("包%1").arg(i + 1) : mp.name;
        m_mpList->addItem(QString("%1: %2").arg(i + 1).arg(name));
    }
    if (prev >= 0 && prev < m_mpList->count())
        m_mpList->setCurrentRow(prev);
    m_mpList->blockSignals(false);
}

void ProtocolEditDialog::loadMultiPackets()
{
    m_loading = true;
    m_mpIntervalSpin->setValue(m_proto.replyConfig.multiPacketIntervalMs);
    m_mpCycleCheck->setChecked(m_proto.replyConfig.multiPacketCycle);
    refreshMultiPacketList();
    if (m_proto.replyConfig.multiPackets.isEmpty()) {
        m_mpCurrentIndex = -1;
        m_mpHdrTable->setRowCount(0);
        m_mpDataTable->setRowCount(0);
    } else {
        m_mpCurrentIndex = 0;
        m_mpList->setCurrentRow(0);
        loadMultiPacketToTables(0);
    }
    m_loading = false;
    updatePreview();
}

void ProtocolEditDialog::loadMultiPacketToTables(int index)
{
    if (index < 0 || index >= m_proto.replyConfig.multiPackets.size()) {
        m_mpHdrTable->setRowCount(0);
        m_mpDataTable->setRowCount(0);
        return;
    }
    const MultiPacketItem &mp = m_proto.replyConfig.multiPackets[index];
    m_mpHdrTable->setRowCount(0);
    for (const auto &p : mp.headerParams) {
        int row = m_mpHdrTable->rowCount();
        m_mpHdrTable->insertRow(row);
        populateParamRow(m_mpHdrTable, row, p, true);
    }
    m_mpDataTable->setRowCount(0);
    for (const auto &p : mp.dataParams) {
        int row = m_mpDataTable->rowCount();
        m_mpDataTable->insertRow(row);
        populateParamRow(m_mpDataTable, row, p, true);
    }
}

void ProtocolEditDialog::saveCurrentMultiPacket()
{
    if (m_mpCurrentIndex < 0 || m_mpCurrentIndex >= m_proto.replyConfig.multiPackets.size())
        return;
    MultiPacketItem &mp = m_proto.replyConfig.multiPackets[m_mpCurrentIndex];
    mp.headerParams.clear();
    for (int i = 0; i < m_mpHdrTable->rowCount(); ++i)
        mp.headerParams.append(readParamRow(m_mpHdrTable, i, true));
    mp.dataParams.clear();
    for (int i = 0; i < m_mpDataTable->rowCount(); ++i)
        mp.dataParams.append(readParamRow(m_mpDataTable, i, true));
}

void ProtocolEditDialog::onAddMultiPacket()
{
    if (!m_loading) saveCurrentMultiPacket();
    MultiPacketItem mp;
    mp.name = QString("包%1").arg(m_proto.replyConfig.multiPackets.size() + 1);
    m_proto.replyConfig.multiPackets.append(mp);
    m_mpCurrentIndex = m_proto.replyConfig.multiPackets.size() - 1;
    refreshMultiPacketList();
    m_mpList->setCurrentRow(m_mpCurrentIndex);
    loadMultiPacketToTables(m_mpCurrentIndex);
    updatePreview();
}

void ProtocolEditDialog::onRemoveMultiPacket()
{
    int row = m_mpList->currentRow();
    if (row < 0) return;
    m_proto.replyConfig.multiPackets.removeAt(row);
    m_mpCurrentIndex = qMin(row, m_proto.replyConfig.multiPackets.size() - 1);
    refreshMultiPacketList();
    if (m_mpCurrentIndex >= 0) {
        m_mpList->setCurrentRow(m_mpCurrentIndex);
        loadMultiPacketToTables(m_mpCurrentIndex);
    } else {
        m_mpHdrTable->setRowCount(0);
        m_mpDataTable->setRowCount(0);
    }
    updatePreview();
}

void ProtocolEditDialog::onMoveMultiPacketUp()
{
    int row = m_mpList->currentRow();
    if (row <= 0) return;
    saveCurrentMultiPacket();
    qSwap(m_proto.replyConfig.multiPackets[row], m_proto.replyConfig.multiPackets[row - 1]);
    m_mpCurrentIndex = row - 1;
    refreshMultiPacketList();
    m_mpList->setCurrentRow(m_mpCurrentIndex);
    loadMultiPacketToTables(m_mpCurrentIndex);
    updatePreview();
}

void ProtocolEditDialog::onMoveMultiPacketDown()
{
    int row = m_mpList->currentRow();
    if (row < 0 || row >= m_proto.replyConfig.multiPackets.size() - 1) return;
    saveCurrentMultiPacket();
    qSwap(m_proto.replyConfig.multiPackets[row], m_proto.replyConfig.multiPackets[row + 1]);
    m_mpCurrentIndex = row + 1;
    refreshMultiPacketList();
    m_mpList->setCurrentRow(m_mpCurrentIndex);
    loadMultiPacketToTables(m_mpCurrentIndex);
    updatePreview();
}

void ProtocolEditDialog::onMultiPacketSelected(int index)
{
    if (m_loading) return;
    saveCurrentMultiPacket();
    m_mpCurrentIndex = index;
    loadMultiPacketToTables(index);
    updatePreview();
}

void ProtocolEditDialog::onAddMpHdr()
{
    if (m_mpCurrentIndex < 0) { onAddMultiPacket(); return; }
    addParamRow(m_mpHdrTable, true);
    if (!m_loading) saveCurrentMultiPacket();
    updatePreview();
}

void ProtocolEditDialog::onRemoveMpHdr()
{
    int row = m_mpHdrTable->currentRow();
    if (row >= 0) m_mpHdrTable->removeRow(row);
    if (!m_loading) saveCurrentMultiPacket();
    updatePreview();
}

void ProtocolEditDialog::onAddMpData()
{
    if (m_mpCurrentIndex < 0) { onAddMultiPacket(); return; }
    addParamRow(m_mpDataTable, true);
    if (!m_loading) saveCurrentMultiPacket();
    updatePreview();
}

void ProtocolEditDialog::onRemoveMpData()
{
    int row = m_mpDataTable->currentRow();
    if (row >= 0) m_mpDataTable->removeRow(row);
    if (!m_loading) saveCurrentMultiPacket();
    updatePreview();
}

void ProtocolEditDialog::onMpHdrTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_mpHdrTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_mpHdrTable, true);
    else if (selected == actCopyAll) copyTableAll(m_mpHdrTable, true);
    else if (selected == actPaste) pasteToTable(m_mpHdrTable, true);
    else if (selected == actClear) { m_mpHdrTable->setRowCount(0); if (!m_loading) saveCurrentMultiPacket(); updatePreview(); }
    else if (selected == actDel) onRemoveMpHdr();
}

void ProtocolEditDialog::onMpDataTableContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    auto actCopySel = menu.addAction("复制选中行");
    auto actCopyAll = menu.addAction("复制整表");
    auto actPaste = menu.addAction("粘贴");
    auto actClear = menu.addAction("清空整表");
    auto actDel = menu.addAction("删除选中行");

    QAction *selected = menu.exec(m_mpDataTable->viewport()->mapToGlobal(pos));
    if (selected == actCopySel) copyTableSelection(m_mpDataTable, true);
    else if (selected == actCopyAll) copyTableAll(m_mpDataTable, true);
    else if (selected == actPaste) pasteToTable(m_mpDataTable, true);
    else if (selected == actClear) { m_mpDataTable->setRowCount(0); if (!m_loading) saveCurrentMultiPacket(); updatePreview(); }
    else if (selected == actDel) onRemoveMpData();
}

void ProtocolEditDialog::onPreviewTimer()
{
    updatePreview();
}

// 检测Hex/Bytes类型参数: defaultValue为空但占用字节>0时, 会被memset(0)填充发出
// 这种"无内容却发了0字节"在分包负载场景下是异常, 应提示用户从文件加载真实数据
QString ProtocolEditDialog::zeroFillWarnHtml(const ProtocolParam &p, int byteCount)
{
    // 只关心原始字节类型(Hex/Bytes/String/StringUtf8)且非动态、非随机、非数组
    if (p.dynamicType != DynamicType::None) return QString();
    if (p.isRandom) return QString();
    if (p.arrayCount > 1) return QString();
    if (byteCount <= 0) return QString();

    bool rawByte = (p.type == ParamType::Hex || p.type == ParamType::Bytes
                    || p.type == ParamType::String || p.type == ParamType::StringUtf8);
    if (!rawByte) return QString();

    // defaultValue去除空格/0x前缀后为空 → 实际内容为空, 会用0填充
    QString s = p.defaultValue.trimmed();
    if (s.startsWith("0x", Qt::CaseInsensitive)) s = s.mid(2);
    s.remove(' ').remove('\n').remove('\t');
    if (!s.isEmpty()) return QString();

    return QString("<font color='red'> [警告:未加载数据,%1字节将用0填充,请点\"文件\"按钮加载真实数据!]</font>")
           .arg(byteCount);
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
    // 多包配置每个包独立表格, 不参与一键切换大小端
    // swapTable(m_mpHdrTable);
    // swapTable(m_mpDataTable);
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

        detail += QString("<span%1>  偏移:%2  %3 (%4)  %5字节  值:%6%7%8%9</span><br>")
                  .arg(bgStyle).arg(offset).arg(p.name)
                  .arg(ProtocolParam::typeToString(p.type))
                  .arg(sz).arg(fieldHex).arg(arrayInfo).arg(matchInfo)
                  .arg(zeroFillWarnHtml(p, sz));
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

        detail += QString("<span%1>  偏移:%2  %3 (%4)  %5字节  值:%6%7%8%9</span><br>")
                  .arg(bgStyle).arg(offset).arg(p.name)
                  .arg(ProtocolParam::typeToString(p.type))
                  .arg(sz).arg(fieldHex).arg(arrayInfo).arg(matchInfo)
                  .arg(zeroFillWarnHtml(p, sz));
        offset += sz;
    }
    detail += QString("<br><b>完整帧 (%1字节):</b> %2").arg(frame.size()).arg(hex);
    if (m_proto.fixedFrameLength > 0)
        detail += QString("<br><font color='orange'>固定帧长度: %1字节 (匹配时按此长度消费buffer)</font>").arg(m_proto.fixedFrameLength);
    detail += "</pre>";
    m_previewLabel->setText(detail);

    // 构建回复帧预览 (HTML格式)
    QString replyDetail = "<pre style='font-family: monospace; font-size: 13px; margin: 0;'>";

    // 生成单个帧的字段明细HTML
    auto buildFrameDetail = [](const QByteArray &frame, const QVector<ProtocolParam> &hdrParams,
                               const QVector<ProtocolParam> &dataParams) -> QString {
        QString s;
        int roffset = 0;
        s += "<b>帧头:</b><br>";
        for (const auto &p : hdrParams) {
            int sz = p.isRandom ? p.randomLength : p.byteSize();
            if (sz <= 0) sz = p.byteSize();
            QByteArray fieldData = frame.mid(roffset, sz);

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

            s += QString("<span%1>  偏移:%2  %3  值:%4%5%6%7</span><br>")
                 .arg(bgStyle).arg(roffset).arg(p.name)
                 .arg(QString::fromLatin1(fieldData.toHex(' '))).arg(arrayInfo).arg(randInfo)
                 .arg(ProtocolEditDialog::zeroFillWarnHtml(p, sz));
            roffset += sz;
        }
        s += "<b>数据区:</b><br>";
        for (const auto &p : dataParams) {
            int sz = p.isRandom ? p.randomLength : p.byteSize();
            if (sz <= 0) sz = p.byteSize();
            QByteArray fieldData = frame.mid(roffset, sz);

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

            s += QString("<span%1>  偏移:%2  %3  值:%4%5%6%7</span><br>")
                 .arg(bgStyle).arg(roffset).arg(p.name)
                 .arg(QString::fromLatin1(fieldData.toHex(' '))).arg(arrayInfo).arg(randInfo)
                 .arg(ProtocolEditDialog::zeroFillWarnHtml(p, sz));
            roffset += sz;
        }
        s += QString("<b>完整帧 (%1字节):</b> %2").arg(frame.size())
             .arg(QString::fromLatin1(frame.toHex(' ')));
        return s;
    };

    // 发送区(主回复帧) - 所有模式都显示
    QByteArray replyFrame = m_proto.buildReplyFrame(0);
    replyDetail += QString("<b>发送区(主回复帧) %1字节:</b><br>").arg(replyFrame.size());
    replyDetail += buildFrameDetail(replyFrame, m_proto.replyConfig.headerParams, m_proto.replyConfig.dataParams);

    // 多包闭环区(每包=发送区回复帧+本包多包帧, 一起发)
    const auto &mps = m_proto.replyConfig.multiPackets;
    if (mps.isEmpty()) {
        replyDetail += "<br><font color='gray'>无多包配置, 仅发送发送区帧</font>";
    } else {
        int total = mps.size();
        replyDetail += QString("<br><b>多包闭环: 共%1包, 间隔%2ms (每包=发送区回复帧+本包多包帧)</b><br>")
                       .arg(total).arg(m_proto.replyConfig.multiPacketIntervalMs);
        if (m_proto.replyConfig.multiPacketCycle)
            replyDetail += "<font color='purple'><b>[多包循环已开启, 配合周期模式每轮重发]</b></font><br>";
        // 预览前3包
        int showCount = qMin(total, 3);
        for (int i = 0; i < showCount; ++i) {
            const auto &mp = mps[i];
            QByteArray pktFrame = mp.buildFrame(0, i + 1, total);
            // 本闭环回复帧: Length动态字段含本包帧大小(=回复区包头+数据区+本包包头+数据区)
            QByteArray clReplyFrame = m_proto.buildReplyFrame(0, pktFrame.size());
            QString name = mp.name.isEmpty() ? QString("包%1").arg(i + 1) : mp.name;
            replyDetail += QString("<font color='blue'><b>闭环%1: 回复帧(%2字节)+多包帧(%3字节)</b></font><br>")
                           .arg(name).arg(clReplyFrame.size()).arg(pktFrame.size());
            replyDetail += QString("<font color='green'>  └回复帧(Length含本包大小):</font><br>");
            replyDetail += buildFrameDetail(clReplyFrame, m_proto.replyConfig.headerParams, m_proto.replyConfig.dataParams);
            replyDetail += QString("<font color='green'>  └多包帧:</font><br>");
            replyDetail += buildFrameDetail(pktFrame, mp.headerParams, mp.dataParams);
            replyDetail += "<br>";
        }
        if (total > 3)
            replyDetail += QString("<font color='gray'>... (共%1包, 省略%2包)</font><br>")
                           .arg(total).arg(total - 3);
    }
    replyDetail += "</pre>";
    m_replyPreviewLabel->setText(replyDetail);
}
