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
#include "collapsiblegroupbox.h"
#include "../core/protocoltypes.h"

class ProtocolEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProtocolEditDialog(ProtocolConfig &proto, QWidget *parent = nullptr);
    ProtocolConfig getProtocol() const;

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

    // 右键菜单槽
    void onHeaderTableContextMenu(const QPoint &pos);
    void onDataTableContextMenu(const QPoint &pos);
    void onReplyHeaderTableContextMenu(const QPoint &pos);
    void onReplyDataTableContextMenu(const QPoint &pos);

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

    // 复制粘贴辅助函数
    void copyTableSelection(QTableWidget *table, bool isReplyTable);
    void copyTableAll(QTableWidget *table, bool isReplyTable);
    void pasteToTable(QTableWidget *table, bool isReplyTable);
    static QByteArray paramsToClipboardData(const QVector<ProtocolParam> &params, bool isReplyTable);
    static QVector<ProtocolParam> paramsFromClipboardData(const QByteArray &data, bool &outIsReplyTable);

    ProtocolConfig &m_proto;

    // 基本信息控件
    QLineEdit *m_nameEdit;
    QLineEdit *m_descEdit;
    QCheckBox *m_activePushCheck;
    QSpinBox *m_pushIntervalSpin;

    // 接收协议Tab
    QTableWidget *m_headerTable;
    QTableWidget *m_dataTable;

    // 回复配置Tab
    QComboBox *m_replyModeCombo;
    QSpinBox *m_replyIntervalSpin;
    QLabel *m_replyIntervalLabel;
    QTableWidget *m_replyHeaderTable;
    QTableWidget *m_replyDataTable;

    // 预览
    QLabel *m_previewLabel;
    QLabel *m_replyPreviewLabel;

    bool m_loading; // 加载中标志, 防止onParamChanged覆盖数据
};

#endif // PROTOCOLEDITDIALOG_H
