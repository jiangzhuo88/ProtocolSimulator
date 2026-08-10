#ifndef PROTOCOLEDITDIALOG_H
#define PROTOCOLEDITDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QListWidget>
#include "collapsiblegroupbox.h"
#include "WheelEventFilter.h"
#include "../core/protocoltypes.h"

class ProtocolEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProtocolEditDialog(ProtocolConfig &proto, QVector<ProtocolConfig> *allProtocols, QWidget *parent = nullptr);
    ProtocolConfig getProtocol() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onAddHeaderParam();
    void onRemoveHeaderParam();
    void onAddDataParam();
    void onRemoveDataParam();
    void onAddReplyHeaderParam();
    void onRemoveReplyHeaderParam();
    void onAddReplyDataParam();
    void onRemoveReplyDataParam();
    void onReplyModeChanged(int index);
    void onPreviewTimer();
    void onParamChanged();
    void onCopyHeaderToReply();
    void onCopyDataToReply();
    void onCopyRecvToReply();
    void onSwapByteOrder();

    // 多包配置槽
    void onAddMultiPacket();
    void onRemoveMultiPacket();
    void onMoveMultiPacketUp();
    void onMoveMultiPacketDown();
    void onMultiPacketSelected(int index);
    void onAddMpHdr();
    void onRemoveMpHdr();
    void onAddMpData();
    void onRemoveMpData();

    // 右键菜单槽
    void onHeaderTableContextMenu(const QPoint &pos);
    void onDataTableContextMenu(const QPoint &pos);
    void onReplyHeaderTableContextMenu(const QPoint &pos);
    void onReplyDataTableContextMenu(const QPoint &pos);
    void onMpHdrTableContextMenu(const QPoint &pos);
    void onMpDataTableContextMenu(const QPoint &pos);

private:
    void setupUi();
    void loadProtocol();
    void saveProtocol();
    void updatePreview();
    void updateMatchHighlight(QTableWidget *table);
    // 标记预览脏, 由防抖定时器延迟刷新(避免每次单元格编辑都全量重建)
    void markPreviewDirty();

    // 参数表操作: isReplyTable=true表示回复表(无匹配列, 有随机列)
    void populateParamRow(QTableWidget *table, int row, const ProtocolParam &param, bool isReplyTable);
    ProtocolParam readParamRow(QTableWidget *table, int row, bool isReplyTable);
    void addParamRow(QTableWidget *table, bool isReplyTable);
    // 设置默认值列控件: 数组时用"编辑数组"按钮, 单值时用文本
    void setupDefaultValueCell(QTableWidget *table, int row, int arrayCount, const QString &defaultValue);
    // 弹出数组编辑对话框
    void editArrayValues(QTableWidget *table, int row);

    // 多包配置辅助
    void loadMultiPackets();
    void loadMultiPacketToTables(int index);
    void saveCurrentMultiPacket();
    void refreshMultiPacketList();

    // 检测Hex/Bytes参数"空值+userLength>0"会被0填充的情况; 返回非空HTML警告串(红色), 否则返回空串
    static QString zeroFillWarnHtml(const ProtocolParam &p, int byteCount);

    // 复制粘贴辅助函数
    void copyTableSelection(QTableWidget *table, bool isReplyTable);
    void copyTableAll(QTableWidget *table, bool isReplyTable);
    void pasteToTable(QTableWidget *table, bool isReplyTable);
    static QByteArray paramsToClipboardData(const QVector<ProtocolParam> &params, bool isReplyTable);
    static QVector<ProtocolParam> paramsFromClipboardData(const QByteArray &data, bool &outIsReplyTable);

    ProtocolConfig &m_proto;
    QVector<ProtocolConfig> *m_allProtocols;

    // 基本信息控件
    QLineEdit *m_nameEdit;
    QLineEdit *m_descEdit;
    QCheckBox *m_activePushCheck;
    QSpinBox *m_pushIntervalSpin;
    QSpinBox *m_fixedFrameLenSpin;

    // 操作配置Tab
    QCheckBox *m_stopAllCheck;
    QListWidget *m_stopList;

    // 接收协议Tab
    QTableWidget *m_headerTable;
    QTableWidget *m_dataTable;

    // 回复配置Tab
    QComboBox *m_replyModeCombo;
    QSpinBox *m_replyIntervalSpin;
    QLabel *m_replyIntervalLabel;
    QTableWidget *m_replyHeaderTable;
    QTableWidget *m_replyDataTable;
    CollapsibleGroupBox *m_rplHdrGroup;
    CollapsibleGroupBox *m_rplDataGroup;

    // 多包配置
    CollapsibleGroupBox *m_multiPktGroup;
    QSpinBox *m_mpIntervalSpin;
    QCheckBox *m_mpCycleCheck;
    QListWidget *m_mpList;
    QTableWidget *m_mpHdrTable;
    QTableWidget *m_mpDataTable;
    int m_mpCurrentIndex;

    // 预览
    QLabel *m_previewLabel;
    QLabel *m_replyPreviewLabel;
    QTimer *m_previewTimer;   // 防抖定时器: 编辑后延迟刷新预览
    bool m_previewDirty;      // 预览脏标志: true表示待刷新

    bool m_loading; // 加载中标志, 防止onParamChanged覆盖数据
    WheelEventFilter *wheelFilter;
};

#endif // PROTOCOLEDITDIALOG_H
