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

    // 多包下发: 按列表顺序逐包构建→间隔发送(发完最后一包即停; 循环由回复周期驱动)
    void sendMultiPackets(const QSharedPointer<QVector<MultiPacketItem>> packets,
                          int startIndex, quint64 seq, int intervalMs);
};

#endif // SIMCONNECTION_H
