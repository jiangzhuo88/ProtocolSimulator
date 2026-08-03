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

    // 分包下发: 拆分负载→逐包构建→间隔发送→多负载循环
    // config: 分包配置(共享指针保活)
    // payloadIndex: 当前发送的负载索引
    // packetIndex: 当前负载内的包索引
    // seq: 当前包序列号
    void sendSplitPackets(const QSharedPointer<PacketSplitConfig> config,
                          int payloadIndex, int packetIndex, quint64 seq);
};

#endif // SIMCONNECTION_H
