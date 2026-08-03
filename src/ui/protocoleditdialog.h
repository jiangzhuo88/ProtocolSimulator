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

    // 分包配置槽
    void onAddSplitHdr();
    void onRemoveSplitHdr();
    void onAddSplitData();
    void onRemoveSplitData();
    void onSplitParamChanged();
    void onPayloadFieldChanged(int index);
    void onAddPayload();
    void onRemovePayload();
    void onLoadPayloadFromFile();
    void onPastePayloadHex();

    // 右键菜单槽
    void onHeaderTableContextMenu(const QPoint &pos);
    void onDataTableContextMenu(const QPoint &pos);
    void onReplyHeaderTableContextMenu(const QPoint &pos);
    void onReplyDataTableContextMenu(const QPoint &pos);
    void onSplitHdrTableContextMenu(const QPoint &pos);
    void onSplitDataTableContextMenu(const QPoint &pos);

private:
    void setupUi();
    void loadProtocol();
    void saveProtocol();
    void updatePreview();
    void updateMatchHighlight(QTableWidget *table);

    // 参数表操作: isReplyTable=true表示回复表(无匹配列, 有随机列)
    void populateParamRow(QTableWidget *table, int row, const ProtocolParam &param, bool isReplyTable);
    ProtocolParam readParamRow(QTableWidget *table, int row, bool isReplyTable);
    void addParamRow(QTableWidget *table, bool isReplyTable);
    // 设置默认值列控件: 数组时用"编辑数组"按钮, 单值时用文本
    void setupDefaultValueCell(QTableWidget *table, int row, int arrayCount, const QString &defaultValue);
    // 弹出数组编辑对话框
    void editArrayValues(QTableWidget *table, int row);

    // 分包配置辅助
    void loadSplitConfig();
    void saveSplitConfig();
    void refreshPayloadFieldCombo();

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

    // 分包配置
    CollapsibleGroupBox *m_splitGroup;
    QCheckBox *m_splitEnableCheck;
    QTableWidget *m_splitHdrTable;
    QTableWidget *m_splitDataTable;
    QComboBox *m_payloadFieldCombo;
    QSpinBox *m_chunkSizeSpin;
    QSpinBox *m_intervalSpin;
    QCheckBox *m_cycleCheck;
    QSpinBox *m_cycleIntervalSpin;
    QListWidget *m_payloadList;

    // 预览
    QLabel *m_previewLabel;
    QLabel *m_replyPreviewLabel;

    bool m_loading; // 加载中标志, 防止onParamChanged覆盖数据
    WheelEventFilter *wheelFilter;
};

#endif // PROTOCOLEDITDIALOG_H
