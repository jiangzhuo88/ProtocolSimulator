#include "simconnection.h"
#include <QHostAddress>
#include <QElapsedTimer>
#include <QtConcurrent>

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

            // --- EchoRequest回显上下文 ---
            // matchedFrame: 匹配到的请求帧原始字节(按前面offset累加得到的完整帧长截取)
            QByteArray matchedFrame = m_rxBuffer.left(offset);
            // mergedRequestParams: 帧头+数据区参数合并(供buildReplyFrame内部解析每个字段的字节偏移)
            QVector<ProtocolParam> mergedRequestParams;
            mergedRequestParams.reserve(proto.headerParams.size() + proto.dataParams.size());
            for (const auto &p : proto.headerParams) mergedRequestParams.append(p);
            for (const auto &p : proto.dataParams) mergedRequestParams.append(p);
            const QVector<ProtocolParam> *rpPtr = mergedRequestParams.isEmpty() ? nullptr : &mergedRequestParams;

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
                    sendMultiPackets(proto, packets, 0, m_seqNumber, reply.multiPacketIntervalMs,
                                     -1, false, matchedFrame, mergedRequestParams);
                } else {
                    // 无多包配置: 仅发发送区回复帧
                    QByteArray replyData = proto.buildReplyFrame(m_seqNumber++, 0, matchedFrame, rpPtr);
                    m_socket->write(replyData);
                    emit dataSent(replyData, addr);
                    emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::number(replyData.count())));
                }
            } else if (effMode == ReplyMode::Periodic1s) {
                startPeriodicReply(i, 1000, matchedFrame, mergedRequestParams);
            } else if (effMode == ReplyMode::Periodic5s) {
                startPeriodicReply(i, 5000, matchedFrame, mergedRequestParams);
            } else if (effMode == ReplyMode::PeriodicCustom) {
                startPeriodicReply(i, reply.customIntervalMs, matchedFrame, mergedRequestParams);
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

void SimConnection::startPeriodicReply(int protoIndex, int intervalMs,
                                        const QByteArray &requestFrame,
                                        const QVector<ProtocolParam> &requestParams)
{
    // 检查是否已经存在该协议的定时器
    for (const auto &pr : m_periodicTimers) {
        if (pr.protocolIndex == protoIndex) return; // 已经在运行
    }

    auto doReply = [this, protoIndex]() {
        if (protoIndex < 0 || protoIndex >= m_protocols.size()) return;
        const ProtocolConfig &proto = m_protocols[protoIndex];
        QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());
        // 从PeriodicReply里取出"启动时保存的请求上下文"(用于EchoRequest回显)
        QByteArray savedReqFrame;
        QVector<ProtocolParam> savedReqParams;
        const QVector<ProtocolParam> *rpPtr = nullptr;
        PeriodicReply *prEntry = nullptr;
        for (auto &entry : m_periodicTimers)
            if (entry.protocolIndex == protoIndex) { prEntry = &entry; break; }
        if (prEntry) {
            savedReqFrame = prEntry->requestFrame;
            savedReqParams = prEntry->requestParams;
            if (!savedReqParams.isEmpty()) rpPtr = &savedReqParams;
        }
        // 多包循环模式: 开启循环且有多包 → 每轮发N个闭环(每包=回复帧+多包帧)
        // 关键: 一轮所有包发完后再开始计时下一轮, 避免上轮未发完下轮又开始导致错包
        // 否则 → 只发发送区回复帧
        if (proto.replyConfig.multiPacketCycle && !proto.replyConfig.multiPackets.isEmpty()) {
            // 找到本协议的PeriodicReply条目
            PeriodicReply *pr = prEntry;
            // 防重入: 本轮还在发(理论上不会, 因为发前已停定时器), 跳过
            if (pr && pr->mpRoundInProgress) return;
            if (pr) {
                pr->mpRoundInProgress = true;
                pr->timer->stop();   // 停掉定时器, 等本轮发完再重启计时
            }
            auto packets = QSharedPointer<QVector<MultiPacketItem>>::create(proto.replyConfig.multiPackets);
            emit logMessage(QString("[多包循环] 协议 '%1' 周期触发, 发送 %2 个闭环包")
                            .arg(proto.name).arg(packets->size()));
            sendMultiPackets(proto, packets, 0, m_seqNumber, proto.replyConfig.multiPacketIntervalMs,
                             protoIndex, true, savedReqFrame, savedReqParams);
        } else {
            // --- 异步预构建帧队列: 从readyFrames取帧, 不阻塞定时器 ---
            QByteArray replyData;
            bool fromPrebuilt = false;
            {
                QMutexLocker l(&m_frameMutex);
                for (auto &entry : m_periodicTimers) {
                    if (entry.protocolIndex == protoIndex && !entry.readyFrames.isEmpty()) {
                        replyData = entry.readyFrames.dequeue();
                        fromPrebuilt = true;
                        break;
                    }
                }
            }

            qint64 buildNs = 0;
            if (!fromPrebuilt) {
                // 队列空: 降级同步构建
                quint64 seq = m_nextBuildSeq.fetch_add(1);
                QElapsedTimer tb; tb.start();
                replyData = proto.buildReplyFrame(seq, 0, savedReqFrame, rpPtr);
                buildNs = tb.nsecsElapsed();
            }

            // 补充预构建队列(异步)
            requestPrebuild(protoIndex, savedReqFrame, savedReqParams);

            QElapsedTimer twrite; twrite.start();
            m_socket->write(replyData);
            qint64 writeNs = twrite.nsecsElapsed();
            emit dataSent(replyData, addr);
            if (replyData.size() > 512 || buildNs > 1000000LL || !fromPrebuilt) {
                emit logMessage(QString("[性能] '%1' %2B  构建:%3μs  write:%4μs  %5")
                                .arg(proto.name)
                                .arg(replyData.size())
                                .arg(fromPrebuilt ? 0 : buildNs / 1000)
                                .arg(writeNs / 1000)
                                .arg(fromPrebuilt ? QStringLiteral("(预构建)") : QStringLiteral("(同步降级)")));
            }
            emit logMessage(QString("[发] %1: %2").arg(addr).arg(QString::number(replyData.count())));
        }
    };

    QTimer *timer = new QTimer(this);
    timer->setProperty("protoIndex", protoIndex);
    // 短间隔必须用PreciseTimer, 否则Qt默认CoarseTimer会把100ms合并成500ms级调度
    timer->setTimerType(Qt::PreciseTimer);
    connect(timer, &QTimer::timeout, doReply);
    timer->start(intervalMs);

    PeriodicReply reply;
    reply.protocolIndex = protoIndex;
    reply.timer = timer;
    reply.requestFrame = requestFrame;
    reply.requestParams = requestParams;
    m_periodicTimers.append(reply);

    // 启动前预构建2帧, 填充队列(异步, 不阻塞)
    requestPrebuild(protoIndex, requestFrame, requestParams);

    // 立即发送一次
    doReply();

    emit logMessage(QString("[周期回复] 协议 '%1' 开始周期回复, 间隔%2ms")
                    .arg(m_protocols[protoIndex].name).arg(intervalMs));
}

void SimConnection::requestPrebuild(int protoIndex, const QByteArray &reqFrame,
                                     const QVector<ProtocolParam> &reqParams, int targetDepth)
{
    // 在mutex内提交build任务, 保证readyFrames/buildInFlight的线程安全
    QMutexLocker l(&m_frameMutex);
    for (auto &entry : m_periodicTimers) {
        if (entry.protocolIndex != protoIndex) continue;
        while (entry.readyFrames.size() + entry.buildInFlight < targetDepth) {
            entry.buildInFlight++;
            quint64 buildSeq = m_nextBuildSeq.fetch_add(1);
            int idx = protoIndex;
            QByteArray rf = reqFrame;
            QVector<ProtocolParam> rp = reqParams;
            // worker线程: 构建帧 → 加锁入队 → buildInFlight--
            QtConcurrent::run([this, idx, buildSeq, rf, rp]() {
                QByteArray frame;
                if (idx >= 0 && idx < m_protocols.size()) {
                    const ProtocolConfig &proto = m_protocols[idx];
                    const QVector<ProtocolParam> *rpPtr = rp.isEmpty() ? nullptr : &rp;
                    frame = proto.buildReplyFrame(buildSeq, 0, rf, rpPtr);
                }
                QMutexLocker l(&m_frameMutex);
                for (auto &e : m_periodicTimers) {
                    if (e.protocolIndex == idx) {
                        if (!frame.isEmpty())
                            e.readyFrames.enqueue(frame);
                        e.buildInFlight--;
                        break;
                    }
                }
            });
        }
        break;
    }
}



void SimConnection::stopAllPeriodicReplies()
{
    // 加锁: 防止worker线程回来入队时m_periodicTimers已被修改
    QMutexLocker l(&m_frameMutex);
    for (auto &pr : m_periodicTimers) {
        if (pr.timer) {
            pr.timer->stop();
            delete pr.timer;
            pr.timer = nullptr;
        }
        pr.readyFrames.clear();
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

    // 停止对应的定时器(加锁: 与worker线程的入队操作互斥)
    QMutexLocker l(&m_frameMutex);
    for (int i = m_periodicTimers.size() - 1; i >= 0; --i) {
        if (m_periodicTimers[i].protocolIndex == protoIndex) {
            if (m_periodicTimers[i].timer) {
                m_periodicTimers[i].timer->stop();
                delete m_periodicTimers[i].timer;
                m_periodicTimers[i].timer = nullptr;
            }
            m_periodicTimers[i].readyFrames.clear();
            m_periodicTimers.removeAt(i);
        }
    }
}

// 辅助: 在simconnection层也复用buildEchoMap便于MultiPacketItem::buildFrame传echoMap
static QMap<QString, QByteArray> scBuildEchoMap(const QVector<ProtocolParam> *requestParams,
                                                 const QByteArray &requestFrame)
{
    QMap<QString, QByteArray> m;
    if (!requestParams || requestFrame.isEmpty()) return m;
    int off = 0;
    // requestParams在这里实际是headerParams+dataParams的合并(由调用方提前准备好)
    for (const auto &p : *requestParams) {
        int sz = p.byteSize();
        if (off + sz <= requestFrame.size() && !p.name.isEmpty()) {
            m.insert(p.name, requestFrame.mid(off, sz));
        }
        off += sz;
    }
    return m;
}

void SimConnection::sendMultiPackets(const ProtocolConfig &proto,
                                     const QSharedPointer<QVector<MultiPacketItem>> packets,
                                     int startIndex, quint64 seq, int intervalMs,
                                     int protoIndex, bool cycleReschedule,
                                     const QByteArray &requestFrame,
                                     const QVector<ProtocolParam> &requestParams)
{
    // 提前返回时(cycle模式): 恢复标志并重启定时器, 避免循环静默停滞
    auto abortCycleSafe = [this, protoIndex]() {
        for (auto &entry : m_periodicTimers) {
            if (entry.protocolIndex == protoIndex) {
                entry.mpRoundInProgress = false;
                if (entry.timer) entry.timer->start();
                break;
            }
        }
    };
    if (!packets || startIndex < 0 || startIndex >= packets->size()) {
        if (cycleReschedule) abortCycleSafe();
        return;
    }
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        // socket断开: SimConnection会被deleteLater销毁, 无需重启定时器
        return;
    }

    int total = packets->size();
    QString addr = m_socket->peerAddress().toString() + ":" + QString::number(m_socket->peerPort());

    // 构建回显映射(EchoRequest): requestParams是headerParams+dataParams合并
    const QVector<ProtocolParam> *rpPtr = requestParams.isEmpty() ? nullptr : &requestParams;
    QMap<QString, QByteArray> localEchoMap = scBuildEchoMap(rpPtr, requestFrame);
    const QMap<QString, QByteArray> *echoMap = localEchoMap.isEmpty() ? nullptr : &localEchoMap;

    // 2. 先构建本包多包帧(动态字段PacketIndex/TotalPackets/PacketSize自动填充; 包序号1-based)
    //    为什么要先建包帧: 回复帧的Length动态字段在分包模式下需包含本包帧长度, 故先算出包帧大小
    const MultiPacketItem &item = packets->at(startIndex);
    QByteArray pktFrame = item.buildFrame(seq, startIndex + 1, total, echoMap);

    // 1. 发送发送区回复帧(每包都发, 构成完整闭环)
    //    分包模式: Length动态字段 = 回复区(包头+数据区) + 本包(包头+数据区); 不分包时extraLen=0
    QByteArray replyFrame = proto.buildReplyFrame(seq, pktFrame.size(), requestFrame, requestParams);
//    m_socket->write(replyFrame);
//    emit dataSent(replyFrame, addr);
//    emit logMessage(QString("[多包闭环 %1/%2] 发送区回复帧: %3")
//                    .arg(startIndex + 1).arg(total)
//                    .arg(QString::fromLatin1(replyFrame.toHex(' '))));
    QByteArray byteResult = replyFrame + pktFrame;
    m_socket->write(byteResult);
    emit dataSent(byteResult, addr);
    emit logMessage(QString("[多包闭环 %1/%2] 本包多包帧: %3")
                    .arg(startIndex + 1).arg(total)
                    .arg(QString::number(byteResult.count())));
//                    .arg(QString::fromLatin1(pktFrame.toHex(' '))));

    // 序列号递增(供下一包使用)
    m_seqNumber = seq + 1;

    // 调度下一包(捕获requestFrame/requestParams按值, 保证异步触发时数据仍有效)
    if (startIndex + 1 < total) {
        int delay = item.delayMs > 0 ? item.delayMs : intervalMs;
        if (delay < 0) delay = 0;
        quint64 nextSeq = seq + 1;
        QTimer::singleShot(delay, this, [this, proto, packets, startIndex, nextSeq, intervalMs,
                                         protoIndex, cycleReschedule, requestFrame, requestParams]() {
            sendMultiPackets(proto, packets, startIndex + 1, nextSeq, intervalMs,
                             protoIndex, cycleReschedule, requestFrame, requestParams);
        });
    } else {
        // 本轮所有包已发完
        if (cycleReschedule && protoIndex >= 0) {
            // 多包循环模式: 重启周期定时器, 间隔从"本轮发完"开始计时, 避免上轮未完下轮又起
            for (auto &entry : m_periodicTimers) {
                if (entry.protocolIndex == protoIndex) {
                    entry.mpRoundInProgress = false;
                    if (entry.timer) entry.timer->start();   // 用原interval重启
                    break;
                }
            }
        }
    }
}
