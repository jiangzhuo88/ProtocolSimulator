#ifndef SIMCONNECTION_H
#define SIMCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QSharedPointer>
#include "protocoltypes.h"

class SimConnection : public QObject
{
    Q_OBJECT
public:
    explicit SimConnection(QTcpSocket *socket, QObject *parent = nullptr);
    ~SimConnection();

    void setProtocols(const QVector<ProtocolConfig> &protocols);
    QTcpSocket *socket() const { return m_socket; }

signals:
    void logMessage(const QString &msg);
    void dataReceived(const QByteArray &data, const QString &clientAddr);
    void dataSent(const QByteArray &data, const QString &clientAddr);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QVector<ProtocolConfig> m_protocols;
    QByteArray m_rxBuffer;
    quint64 m_seqNumber;

    // 周期回复管理
    struct PeriodicReply {
        int protocolIndex;
        QTimer *timer;
    };
    QVector<PeriodicReply> m_periodicTimers;

    void tryMatch();
    void startPeriodicReply(int protoIndex, int intervalMs);
    void stopAllPeriodicReplies();
    void stopPeriodicReplyByName(const QString &name);

    // 多包下发: 每包=发送区回复帧+本包多包帧, 一起发; 按列表顺序逐包间隔发送
    // proto: 协议配置(用于构建发送区回复帧)
    // packets: 多包列表(共享指针保活)
    // startIndex: 当前发送的包索引
    // seq: 当前包序列号
    // intervalMs: 默认包间间隔(单包delayMs<=0时用此值)
    void sendMultiPackets(const ProtocolConfig &proto,
                          const QSharedPointer<QVector<MultiPacketItem>> packets,
                          int startIndex, quint64 seq, int intervalMs);
};

#endif // SIMCONNECTION_H
