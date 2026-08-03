#include "simconnection.h"
#include <QHostAddress>

SimConnection::SimConnection(QTcpSocket *socket, QObject *parent)
    : QObject(parent), m_socket(socket), m_seqNumber(0)
{
    m_socket->setParent(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &SimConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
}

SimConnection::~SimConnection()
{
    stopAllPeriodicReplies();
}

void SimConnection::setProtocols(const QVector<ProtocolConfig> &protocols)
{
    m_protocols = protocols;
    stopAllPeriodicReplies();

    // 启动主动上报(心跳)定时器
    for (int i = 0; i < m_protocols.size(); ++i) {
        if (m_protocols[i].isActivePush && m_protocols[i].pushIntervalMs > 0) {
            startPeriodicReply(i, m_protocols[i].pushIntervalMs);
        }
    }
}

void SimConnection::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    m_rxBuffer += data;

    QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
    emit dataReceived(data, addr);
    emit logMessage(QString("[收] %1: %2").arg(addr).arg(QString::fromLatin1(data.toHex(' '))));

    tryMatch();
}

void SimConnection::tryMatch()
{
    // 遍历所有有效协议，尝试匹配
    bool anyMatched = false;
    int maxMatchedSize = 0;
    for (int i = 0; i < m_protocols.size(); ++i) {
        const ProtocolConfig &proto = m_protocols[i];
        if (proto.isActivePush) continue; // 主动上报协议不参与匹配
        if (!proto.isValid()) continue; // 无效协议跳过

        // 计算帧头总长度
        int headerSize = 0;
        for (const auto &p : proto.headerParams)
            headerSize += p.byteSize();

        // 计算数据区长度(固定长度)
        int dataSize = 0;
        for (const auto &p : proto.dataParams)
            dataSize += p.byteSize();

        int totalSize = headerSize + dataSize;
        if (m_rxBuffer.size() < totalSize) continue;

        // 提取每个字段的字节进行匹配
        bool matched = true;
        int offset = 0;

        // 匹配帧头参数
        for (const ProtocolParam &p : proto.headerParams) {
            int sz = p.byteSize();
            QByteArray fieldData = m_rxBuffer.mid(offset, sz);
            if (p.matchEnabled && !p.match(fieldData)) {
                matched = false;
                break;
            }
            offset += sz;
        }

        // 匹配数据区参数
        if (matched) {
            for (const auto &p : proto.dataParams) {
                int sz = p.byteSize();
                QByteArray fieldData = m_rxBuffer.mid(offset, sz);
                if (p.matchEnabled && !p.match(fieldData)) {
                    matched = false;
                    break;
                }
                offset += sz;
            }
        }

        if (matched) {
            QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
            emit logMessage(QString("[匹配] 协议 '%1' 匹配成功").arg(proto.name));

            if(!proto.stopPeriodicProtocolNames.isEmpty())
            {
                for(const QString &name : proto.stopPeriodicProtocolNames)
                {
                    emit logMessage(QString("[停止] 协议'%1'触发停止指令，停止协议'%2'的周期回复").arg(proto.name).arg(name));
                    stopPeriodicReplyByName(name);
                }
            }
            anyMatched = true;
            // 执行回复
            const ReplyConfig &reply = proto.replyConfig;
            if (reply.mode == ReplyMode::Once) {
                QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
                m_socket->write(replyData);
                emit dataSent(replyData, addr);
                emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
            } else if (reply.mode == ReplyMode::Periodic1s) {
                startPeriodicReply(i, 1000);
            } else if (reply.mode == ReplyMode::Periodic5s) {
                startPeriodicReply(i, 5000);
            } else if (reply.mode == ReplyMode::PeriodicCustom) {
                startPeriodicReply(i, reply.customIntervalMs);
            }

//            // 消费已匹配的数据
//            m_rxBuffer.remove(0, totalSize);
//            // 继续尝试匹配剩余数据
//            if (!m_rxBuffer.isEmpty())
//                tryMatch();
//            return;

            if(totalSize > maxMatchedSize)
            {
                maxMatchedSize = totalSize;
            }
        }
    }
    if(anyMatched)
    {
        m_rxBuffer.remove(0,maxMatchedSize);
        if(!m_rxBuffer.isEmpty())
        {
            tryMatch();
        }
    }
    return;
}

void SimConnection::startPeriodicReply(int protoIndex, int intervalMs)
{
    // 检查是否已经存在该协议的定时器
    for (const auto &pr : m_periodicTimers) {
        if (pr.protocolIndex == protoIndex) return; // 已经在运行
    }

    QTimer *timer = new QTimer(this);
    timer->setProperty("protoIndex", protoIndex);
    connect(timer, &QTimer::timeout, this, &SimConnection::onPeriodicReply);
    timer->start(intervalMs);

    m_periodicTimers.append({protoIndex, timer});

    // 立即发送一次
    onPeriodicReply();

    emit logMessage(QString("[周期回复] 协议 '%1' 开始周期回复, 间隔%2ms")
                    .arg(m_protocols[protoIndex].name).arg(intervalMs));
    return;
}

void SimConnection::onPeriodicReply()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    int protoIndex = timer->property("protoIndex").toInt();
    if (protoIndex < 0 || protoIndex >= m_protocols.size()) return;

    const ProtocolConfig &proto = m_protocols[protoIndex];
    QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
    qint64 result = m_socket->write(replyData);

    QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
    emit dataSent(replyData, addr);
//    emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
    emit logMessage(QString("[发] %1: %2").arg(addr).arg(proto.name));
}

void SimConnection::stopAllPeriodicReplies()
{
    for (auto &pr : m_periodicTimers) {
        if (pr.timer) {
            pr.timer->stop();
            delete pr.timer;
        }
    }
    m_periodicTimers.clear();
}

void SimConnection::stopPeriodicReplyByName(const QString &name)
{
    int protoIndex = -1;
    for(int i = 0;i < m_protocols.size();++i)
    {
        if(m_protocols[i].name == name)
        {
            protoIndex = i;
            break;
        }
    }
    if(protoIndex < 0) return;
    for(int i = m_periodicTimers.size() - 1;i >= 0;--i)
    {
        if(m_periodicTimers[i].protocolIndex == protoIndex)
        {
            if(m_periodicTimers[i].timer)
            {
                m_periodicTimers[i].timer->stop();
                delete m_periodicTimers[i].timer;
            }
            m_periodicTimers.removeAt(i);
        }
    }
}
