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
    // 遍历所有有效协议，尝试匹配(多条协议可能同时匹配，都需回复)
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
        if (proto.fixedFrameLength > 0)
            totalSize = proto.fixedFrameLength;
        if (m_rxBuffer.size() < totalSize) continue;

        // 提取每个字段的字节进行匹配
        bool matched = true;
        int offset = 0;

        // 匹配帧头参数
        for (const auto &p : proto.headerParams) {
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

            // 如果该协议匹配时需要停止所有周期回复
            if (proto.stopAllPeriodicOnMatch) {
                emit logMessage(QString("[停止] 协议 '%1' 触发停止指令，停止所有周期回复").arg(proto.name));
                stopAllPeriodicReplies();
            }

            // 停止指定协议的周期回复
            if (!proto.stopPeriodicProtocolNames.isEmpty()) {
                for (const auto &name : proto.stopPeriodicProtocolNames) {
                    emit logMessage(QString("[停止] 协议 '%1' 触发停止指令，停止协议 '%2' 的周期回复").arg(proto.name).arg(name));
                    stopPeriodicReplyByName(name);
                }
            }

            // 执行回复
            const ReplyConfig &reply = proto.replyConfig;
            ReplyMode effMode = reply.mode;

            if (effMode == ReplyMode::Once || effMode == ReplyMode::MultiPacket) {
                // 多包闭环模式: 每包=发送区回复帧+本包多包帧, 一起发
                if (!reply.multiPackets.isEmpty()) {
                    auto packets = QSharedPointer<QVector<MultiPacketItem>>::create(reply.multiPackets);
                    emit logMessage(QString("[多包] 协议 '%1' 开始下发 %2 个闭环包")
                                    .arg(proto.name).arg(packets->size()));
                    sendMultiPackets(proto, packets, 0, m_seqNumber, reply.multiPacketIntervalMs);
                } else {
                    // 无多包配置: 仅发发送区回复帧
                    QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
                    m_socket->write(replyData);
                    emit dataSent(replyData, addr);
                    emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
                }
            } else if (effMode == ReplyMode::Periodic1s) {
                startPeriodicReply(i, 1000);
            } else if (effMode == ReplyMode::Periodic5s) {
                startPeriodicReply(i, 5000);
            } else if (effMode == ReplyMode::PeriodicCustom) {
                startPeriodicReply(i, reply.customIntervalMs);
            }

            anyMatched = true;
            if (totalSize > maxMatchedSize)
                maxMatchedSize = totalSize;
        }
    }

    if (anyMatched) {
        // 防止maxMatchedSize为0导致无限递归
        if (maxMatchedSize <= 0) maxMatchedSize = 1;
        // 消费已匹配的数据(取最大匹配长度)
        m_rxBuffer.remove(0, maxMatchedSize);
        // 继续尝试匹配剩余数据
        if (!m_rxBuffer.isEmpty())
            tryMatch();
    } else {
        // 没有协议匹配: 检查buffer是否已超过所有协议的最小totalSize
        // 如果超过,说明buffer中有残留数据顶住了后续协议,需要逐字节丢弃resync
        int minNeeded = INT_MAX;
        for (int i = 0; i < m_protocols.size(); ++i) {
            const ProtocolConfig &p = m_protocols[i];
            if (p.isActivePush || !p.isValid()) continue;
            int sz = 0;
            for (const auto &param : p.headerParams) sz += param.byteSize();
            for (const auto &param : p.dataParams) sz += param.byteSize();
            if (p.fixedFrameLength > 0) sz = p.fixedFrameLength;
            if (sz > 0 && sz < minNeeded) minNeeded = sz;
        }
        // buffer已足够大但没有协议匹配,丢弃首字节尝试resync
        if (minNeeded != INT_MAX && m_rxBuffer.size() > minNeeded) {
            m_rxBuffer.remove(0, 1);
            tryMatch();
        }
    }
}

void SimConnection::startPeriodicReply(int protoIndex, int intervalMs)
{
    // 检查是否已经存在该协议的定时器
    for (const auto &pr : m_periodicTimers) {
        if (pr.protocolIndex == protoIndex) return; // 已经在运行
    }

    auto doReply = [this, protoIndex]() {
        if (protoIndex < 0 || protoIndex >= m_protocols.size()) return;
        const ProtocolConfig &proto = m_protocols[protoIndex];
        QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
        // 多包循环模式: 开启循环且有多包 → 每周期发N个闭环(每包=回复帧+多包帧)
        // 否则 → 只发发送区回复帧
        if (proto.replyConfig.multiPacketCycle && !proto.replyConfig.multiPackets.isEmpty()) {
            auto packets = QSharedPointer<QVector<MultiPacketItem>>::create(proto.replyConfig.multiPackets);
            emit logMessage(QString("[多包循环] 协议 '%1' 周期触发, 发送 %2 个闭环包")
                            .arg(proto.name).arg(packets->size()));
            sendMultiPackets(proto, packets, 0, m_seqNumber, proto.replyConfig.multiPacketIntervalMs);
        } else {
            QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
            m_socket->write(replyData);
            emit dataSent(replyData, addr);
            emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
        }
    };

    QTimer *timer = new QTimer(this);
    timer->setProperty("protoIndex", protoIndex);
    connect(timer, &QTimer::timeout, doReply);
    timer->start(intervalMs);

    m_periodicTimers.append({protoIndex, timer});

    // 立即发送一次
    doReply();

    emit logMessage(QString("[周期回复] 协议 '%1' 开始周期回复, 间隔%2ms")
                    .arg(m_protocols[protoIndex].name).arg(intervalMs));
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
    // 查找协议名称对应的index
    int protoIndex = -1;
    for (int i = 0; i < m_protocols.size(); ++i) {
        if (m_protocols[i].name == name) {
            protoIndex = i;
            break;
        }
    }
    if (protoIndex < 0) return;

    // 停止对应的定时器
    for (int i = m_periodicTimers.size() - 1; i >= 0; --i) {
        if (m_periodicTimers[i].protocolIndex == protoIndex) {
            if (m_periodicTimers[i].timer) {
                m_periodicTimers[i].timer->stop();
                delete m_periodicTimers[i].timer;
            }
            m_periodicTimers.removeAt(i);
        }
    }
}

void SimConnection::sendMultiPackets(const ProtocolConfig &proto,
                                     const QSharedPointer<QVector<MultiPacketItem>> packets,
                                     int startIndex, quint64 seq, int intervalMs)
{
    if (!packets || startIndex < 0 || startIndex >= packets->size()) return;
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;

    int total = packets->size();
    QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());

    // 1. 发送发送区回复帧(每包都发, 构成完整闭环)
    QByteArray replyFrame = proto.buildReplyFrame(seq);
    m_socket->write(replyFrame);
    emit dataSent(replyFrame, addr);
    emit logMessage(QString("[多包闭环 %1/%2] 发送区回复帧: %3")
                    .arg(startIndex + 1).arg(total)
                    .arg(QString::fromLatin1(replyFrame.toHex(' '))));

    // 2. 发送本包多包帧(动态字段PacketIndex/TotalPackets/PacketSize自动填充; 包序号1-based)
    const MultiPacketItem &item = packets->at(startIndex);
    QByteArray pktFrame = item.buildFrame(seq, startIndex + 1, total);
    m_socket->write(pktFrame);
    emit dataSent(pktFrame, addr);
    emit logMessage(QString("[多包闭环 %1/%2] 本包多包帧: %3")
                    .arg(startIndex + 1).arg(total)
                    .arg(QString::fromLatin1(pktFrame.toHex(' '))));

    // 序列号递增(供下一包使用)
    m_seqNumber = seq + 1;

    // 调度下一包
    if (startIndex + 1 < total) {
        int delay = item.delayMs > 0 ? item.delayMs : intervalMs;
        if (delay < 0) delay = 0;
        quint64 nextSeq = seq + 1;
        QTimer::singleShot(delay, this, [this, proto, packets, startIndex, nextSeq, intervalMs]() {
            sendMultiPackets(proto, packets, startIndex + 1, nextSeq, intervalMs);
        });
    }
}
