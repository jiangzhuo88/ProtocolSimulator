#ifndef SIMTCPSERVER_H
#define SIMTCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QVector>
#include "protocoltypes.h"

class SimConnection;

class SimTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit SimTcpServer(QObject *parent = nullptr);
    ~SimTcpServer();

    bool start(int port);
    void stop();
    bool isRunning() const;

    void setProtocols(const QVector<ProtocolConfig> &protocols);
    int clientCount() const;

signals:
    void logMessage(const QString &msg);
    void dataReceived(const QByteArray &data, const QString &clientAddr);
    void dataSent(const QByteArray &data, const QString &clientAddr);
    void clientConnected(const QString &addr);
    void clientDisconnected(const QString &addr);

private slots:
    void onNewConnection();

private:
    QTcpServer *m_server;
    QVector<SimConnection*> m_connections;
    QVector<ProtocolConfig> m_protocols;
};

#endif // SIMTCPSERVER_H
