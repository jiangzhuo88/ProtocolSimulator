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
            // 旧MultiPacket配置已无独立UI, 统一按"发送区+分包区"处理: 视为Once
            ReplyMode effMode = reply.mode;
            if (effMode == ReplyMode::MultiPacket) effMode = ReplyMode::Once;

            if (effMode == ReplyMode::Once) {
                // 1. 发送发送区(主回复帧)
                QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
                m_socket->write(replyData);
                emit dataSent(replyData, addr);
                emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
                // 2. 分包区: 发送区帧发送后, 按模板拆分负载逐包下发
                if (reply.splitConfig.enabled && !reply.splitConfig.payloads.isEmpty()) {
                    auto config = QSharedPointer<PacketSplitConfig>::create(reply.splitConfig);
                    emit logMessage(QString("[分包] 协议 '%1' 发送区已发, 开始分包下发 %2 个负载")
                                    .arg(proto.name).arg(config->payloads.size()));
                    sendSplitPackets(config, 0, 0, m_seqNumber);
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
        // 1. 发送发送区(主回复帧)
        QByteArray replyData = proto.buildReplyFrame(m_seqNumber++);
        m_socket->write(replyData);
        QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
        emit dataSent(replyData, addr);
        emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::fromLatin1(replyData.toHex(' '))));
        // 2. 分包区: 发送区帧发送后, 按模板拆分负载逐包下发
        if (proto.replyConfig.splitConfig.enabled && !proto.replyConfig.splitConfig.payloads.isEmpty()) {
            auto config = QSharedPointer<PacketSplitConfig>::create(proto.replyConfig.splitConfig);
            emit logMessage(QString("[分包] 协议 '%1' 周期发送区已发, 开始分包下发 %2 个负载")
                            .arg(proto.name).arg(config->payloads.size()));
            sendSplitPackets(config, 0, 0, m_seqNumber);
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

void SimConnection::sendSplitPackets(const QSharedPointer<PacketSplitConfig> config,
                                     int payloadIndex, int packetIndex, quint64 seq)
{
    if (!config) return;
    if (payloadIndex < 0 || payloadIndex >= config->payloads.size()) return;
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;

    const QByteArray &payload = config->payloads[payloadIndex];
    int chunkSize = config->effectiveChunkSize();
    if (chunkSize <= 0 || payload.isEmpty()) return;

    int totalPackets = config->calcPacketCount(payload);
    if (totalPackets <= 0) return;
    if (packetIndex < 0 || packetIndex >= totalPackets) return;

    // 切片当前包负载
    int start = packetIndex * chunkSize;
    int len = qMin(chunkSize, payload.size() - start);
    if (len <= 0) return;
    QByteArray chunk = payload.mid(start, len);

    // 按模板构建当前分包帧
    QByteArray frame = config->buildPacket(packetIndex, totalPackets, chunk, seq);
    m_socket->write(frame);

    QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
    emit dataSent(frame, addr);
    emit logMessage(QString("[分包发 %1/%2] 负载%3/%4: %5")
                    .arg(packetIndex + 1).arg(totalPackets)
                    .arg(payloadIndex + 1).arg(config->payloads.size())
                    .arg(QString::fromLatin1(frame.toHex(' '))));

    // 序列号递增(供下一包使用)
    m_seqNumber = seq + 1;

    // 调度下一包
    if (packetIndex + 1 < totalPackets) {
        // 同一负载的下一包: 按 intervalMs 间隔
        int delay = config->intervalMs;
        if (delay < 0) delay = 0;
        quint64 nextSeq = seq + 1;
        QTimer::singleShot(delay, this, [this, config, payloadIndex, packetIndex, nextSeq]() {
            sendSplitPackets(config, payloadIndex, packetIndex + 1, nextSeq);
        });
    } else if (config->cycleEnabled && config->payloads.size() > 1) {
        // 当前负载已发完, 切换到下一个负载循环
        int nextPayload = (payloadIndex + 1) % config->payloads.size();
        int delay = config->cycleIntervalMs;
        if (delay < 0) delay = 0;
        quint64 nextSeq = seq + 1;
        QTimer::singleShot(delay, this, [this, config, nextPayload, nextSeq]() {
            sendSplitPackets(config, nextPayload, 0, nextSeq);
        });
    }
}
