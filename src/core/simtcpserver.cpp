#include "simtcpserver.h"
#include "simconnection.h"
#include <QHostAddress>

SimTcpServer::SimTcpServer(QObject *parent)
    : QObject(parent), m_server(nullptr)
{
}

SimTcpServer::~SimTcpServer()
{
    stop();
}

bool SimTcpServer::start(int port)
{
    if (m_server) return false;
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &SimTcpServer::onNewConnection);
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("[错误] 无法监听端口 %1: %2").arg(port).arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }
    emit logMessage(QString("[服务] TCP服务已启动, 监听端口 %1").arg(port));
    return true;
}

void SimTcpServer::stop()
{
    if (!m_server) return;
    for (auto *conn : m_connections) {
        if (conn->socket()) {
            QString addr = conn->socket()->peerAddress().toString() + ":" + QString::number(conn->socket()->peerPort());
            emit clientDisconnected(addr);
        }
        conn->deleteLater();
    }
    m_connections.clear();
    m_server->close();
    delete m_server;
    m_server = nullptr;
}

bool SimTcpServer::isRunning() const
{
    return m_server && m_server->isListening();
}

void SimTcpServer::setProtocols(const QVector<ProtocolConfig> &protocols)
{
    m_protocols = protocols;
    for (auto *conn : m_connections)
        conn->setProtocols(protocols);
}

int SimTcpServer::clientCount() const
{
    return m_connections.size();
}

void SimTcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        SimConnection *conn = new SimConnection(socket, this);
        conn->setProtocols(m_protocols);

        connect(conn, &SimConnection::logMessage, this, &SimTcpServer::logMessage);
        connect(conn, &SimConnection::dataReceived, this, &SimTcpServer::dataReceived);
        connect(conn, &SimConnection::dataSent, this, &SimTcpServer::dataSent);
        connect(conn, &QObject::destroyed, this, [this, conn](QObject*) {
            m_connections.removeOne(conn);
        });

        m_connections.append(conn);

        QString addr = socket->peerAddress().toString() + ":" + QString::number(socket->peerPort());
        emit clientConnected(addr);
        emit logMessage(QString("[连接] 客户端已连接: %1").arg(addr));
    }
}
