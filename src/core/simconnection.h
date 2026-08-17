#ifndef SIMCONNECTION_H
#define SIMCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QSharedPointer>
#include <QMutex>
#include <QQueue>
#include <atomic>
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
        int protocolIndex = -1;
        QTimer *timer = nullptr;
        bool mpRoundInProgress = false;   // 多包循环: 本轮多包是否正在发送(发完才允许下一轮计时)
        // 触发本次周期回复的请求帧上下文(用于EchoRequest回显请求字段)
        // 匹配启动周期回复时保存, 后续每次周期发数据时都回显同样的请求字段
        QByteArray requestFrame;
        QVector<ProtocolParam> requestParams;
        // 异步预构建帧队列: worker线程提前build好, 定时器到期直接取发, 不阻塞
        QQueue<QByteArray> readyFrames;
        int buildInFlight = 0;             // 正在worker线程构建中的帧数
    };
    QVector<PeriodicReply> m_periodicTimers;
    QMutex m_frameMutex;                    // 保护 readyFrames / buildInFlight
    std::atomic<quint64> m_nextBuildSeq{0}; // 全局seq分配器(线程安全)

    void tryMatch();
    void startPeriodicReply(int protoIndex, int intervalMs,
                            const QByteArray &requestFrame = QByteArray(),
                            const QVector<ProtocolParam> &requestParams = QVector<ProtocolParam>());
    void stopAllPeriodicReplies();
    void stopPeriodicReplyByName(const QString &name);
    // 异步预构建: 在worker线程提前buildReplyFrame, 放入readyFrames队列
    // 定时器到期时直接取已构建的帧发送, 不被buildReplyFrame阻塞
    void requestPrebuild(int protoIndex, const QByteArray &reqFrame,
                         const QVector<ProtocolParam> &reqParams, int targetDepth = 3);

    // 多包下发: 每包=发送区回复帧+本包多包帧, 一起发; 按列表顺序逐包间隔发送
    // proto: 协议配置(用于构建发送区回复帧)
    // packets: 多包列表(共享指针保活)
    // startIndex: 当前发送的包索引
    // seq: 当前包序列号
    // intervalMs: 默认包间间隔(单包delayMs<=0时用此值)
    // protoIndex: 周期回复协议索引(cycleReschedule=true时用于找到对应定时器重启)
    // cycleReschedule: true=最后一包发完后重启周期定时器(多包循环模式: 等本轮发完再开始计时下一轮)
    void sendMultiPackets(const ProtocolConfig &proto,
                          const QSharedPointer<QVector<MultiPacketItem>> packets,
                          int startIndex, quint64 seq, int intervalMs,
                          int protoIndex = -1, bool cycleReschedule = false,
                          const QByteArray &requestFrame = QByteArray(),
                          const QVector<ProtocolParam> &requestParams = QVector<ProtocolParam>());
};

#endif // SIMCONNECTION_H
