#ifndef PROTOCOLTYPES_H
#define PROTOCOLTYPES_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

// 参数值类型
enum class ParamType {
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Int8,
    Int16,
    Int32,
    Int64,
    Float32,
    Float64,
    String,      // ASCII字符串
    StringUtf8,  // UTF-8字符串
    Bytes,       // 固定字节
    Hex          // 十六进制字节序列
};

// 字节序
enum class ByteOrder {
    BigEndian,
    LittleEndian
};

// 动态类型
enum class DynamicType {
    None,         // 非动态，使用默认值
    Timestamp,    // 当前时间戳(秒或毫秒)
    Length,       // 数据区长度
    Checksum,     // 校验和
    Sequence,     // 自增序列号
    PacketIndex,  // 分包序号(当前包在分包中的索引,0-based)
    PacketSize,   // 分包大小(当前包负载字节数)
    TotalPackets  // 总包数
};

// 匹配模式
enum class MatchMode {
    Exact,   // 精确匹配
    Prefix,  // 前缀匹配
    Mask,    // 掩码匹配 (value & mask == expected)
    Range,   // 范围匹配 [min, max]
    Any      // 任意值(通配)
};

// 回复模式
enum class ReplyMode {
    None,           // 不回复
    Once,           // 回复一次
    Periodic1s,     // 1秒周期回复
    Periodic5s,     // 5秒周期回复
    PeriodicCustom, // 自定义周期回复
    MultiPacket     // 多包回复(收到指令后连续发送多包)
};

// 单个协议参数
struct ProtocolParam {
    QString name;              // 参数名称
    ParamType type;            // 参数类型
    ByteOrder byteOrder;       // 字节序
    QString defaultValue;      // 默认值(支持int/hex/double等输入)
    DynamicType dynamicType;   // 动态类型
    int dynamicParam;          // 动态参数(如时间戳单位:0=秒 1=毫秒; 校验和范围起始)

    bool matchEnabled;         // 是否启用匹配
    MatchMode matchMode;       // 匹配模式
    QString matchValue;        // 匹配值
    QString matchValue2;       // 第二匹配值(范围匹配时的max, 掩码匹配时的expected)

    int userLength;            // 用户显式指定的长度(仅Hex/Bytes/String类型用,0=自动推导)
    int arrayCount;            // 数组元素个数(1=单个值, >1=同类型数组, 如1600个Int16)

    bool isRandom;             // 是否随机值(用于回复参数)
    QString randomMin;         // 随机最小值
    QString randomMax;         // 随机最大值
    int randomLength;          // 随机字节长度(Bytes/Hex类型用)

    ProtocolParam()
        : type(ParamType::UInt16)
        , byteOrder(ByteOrder::BigEndian)
        , dynamicType(DynamicType::None)
        , dynamicParam(0)
        , matchEnabled(false)
        , matchMode(MatchMode::Exact)
        , userLength(0)
        , arrayCount(1)
        , isRandom(false)
        , randomLength(8)
    {}

    // 获取参数占用的字节数
    int byteSize() const;
    // 将默认值转为字节序列
    // packetIndex/packetSize/totalPackets: 分包上下文(-1=非分包模式)
    QByteArray toBytes(quint64 seq = 0, int dataAreaLen = 0, 
                       const QByteArray &fullFrame = QByteArray(),
                       int packetIndex = -1, int packetSize = -1, int totalPackets = -1) const;
    // 单元素(非动态)按指定字符串值转字节(供数组逐元素生成)
    QByteArray singleElementToBytes(const QString &val) const;
    // 解析默认值字符串为字符串列表(JSON数组返回多个, 否则返回单个原值)
    static QStringList parseDefaultValuesFromString(const QString &defaultValue);
    // 解析本参数默认值为字符串列表(数组且为JSON数组时返回多个元素, 否则返回单个)
    QStringList parseDefaultValues() const;
    // 生成随机值字节
    QByteArray toRandomBytes() const;
    // 从字节序列解析值
    static QString fromBytes(const QByteArray &data, ParamType type, ByteOrder order);
    // 匹配检查: received为收到的该字段字节
    bool match(const QByteArray &received) const;
    // 将匹配值字符串转为字节序列(考虑字段类型和字节序)
    QByteArray matchValueToBytes(const QString &value) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    static QString typeToString(ParamType t);
    static ParamType stringToType(const QString &s);
    static QString byteOrderToString(ByteOrder o);
    static ByteOrder stringToByteOrder(const QString &s);
    static QString dynamicTypeToString(DynamicType d);
    static DynamicType stringToDynamicType(const QString &s);
    static QString matchModeToString(MatchMode m);
    static MatchMode stringToMatchMode(const QString &s);
};

// 分包配置(模板+负载+循环)
struct PacketSplitConfig {
    bool enabled;                          // 是否启用分包下发
    QVector<ProtocolParam> headerParams;   // 包模板帧头参数(可设PacketIndex/PacketSize/TotalPackets)
    QVector<ProtocolParam> dataParams;     // 包模板数据区参数(其中一个可标记为负载)
    int payloadFieldIndex;                 // 负载数据字段在dataParams中的索引(-1=无负载字段)
    int chunkSize;                         // 每包负载字节数(0=用负载字段userLength)
    int intervalMs;                        // 包间发送间隔(ms)
    bool cycleEnabled;                     // 是否多负载循环
    int cycleIntervalMs;                   // 循环间隔(一轮发完到下一轮,ms)
    QVector<QByteArray> payloads;          // 多个负载数据(如多张图片)

    PacketSplitConfig()
        : enabled(false), payloadFieldIndex(-1)
        , chunkSize(1024), intervalMs(100)
        , cycleEnabled(false), cycleIntervalMs(1000) {}

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
    // 构建第packetIndex个分包(基于chunk负载数据)
    QByteArray buildPacket(int packetIndex, int totalPackets,
                           const QByteArray &chunk, quint64 seq) const;
    // 计算给定负载的分包数
    int calcPacketCount(const QByteArray &payload) const;
    // 获取每包负载大小
    int effectiveChunkSize() const;
};

// 回复配置
struct ReplyConfig {
    ReplyMode mode;                         // 回复模式
    int customIntervalMs;                   // 自定义周期(毫秒)
    QVector<ProtocolParam> headerParams;    // 回复帧头参数(发送区)
    QVector<ProtocolParam> dataParams;      // 回复数据区参数(发送区)
    PacketSplitConfig splitConfig;          // 分包配置(发送区帧发送后, 按模板拆分负载下发)

    ReplyConfig() : mode(ReplyMode::None), customIntervalMs(1000) {}

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
    static QString modeToString(ReplyMode m);
    static ReplyMode stringToMode(const QString &s);
};

// 协议配置(一条完整的协议)
struct ProtocolConfig {
    QString name;                           // 协议名称
    QString description;                    // 描述
    QVector<ProtocolParam> headerParams;    // 帧头参数
    QVector<ProtocolParam> dataParams;      // 数据区参数
    ReplyConfig replyConfig;                // 回复配置
    bool isActivePush;                      // 是否主动上报(心跳/状态)
    int pushIntervalMs;                     // 主动上报周期(ms)
    int fixedFrameLength;                   // 固定帧总长度(0=根据参数自动计算, >0=手动指定)
    bool stopAllPeriodicOnMatch;            // 匹配成功时停止所有正在运行的周期回复
    QStringList stopPeriodicProtocolNames;  // 匹配成功时要停止的周期回复协议名称列表

    ProtocolConfig() : isActivePush(false), pushIntervalMs(1000), fixedFrameLength(0), stopAllPeriodicOnMatch(false) {}

    // 是否有效(主动上报始终有效, 或至少有一个参数启用了匹配)
    bool isValid() const;
    // 获取所有启用了匹配的参数列表
    QVector<const ProtocolParam*> matchParams() const;
    // 构建完整帧
    QByteArray buildFrame(quint64 seq = 0) const;
    // 构建回复帧
    QByteArray buildReplyFrame(quint64 seq = 0) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
};

// 场景配置
struct SceneConfig {
    QString name;                           // 场景名称
    int tcpPort;                            // TCP端口
    QVector<ProtocolConfig> protocols;      // 协议列表

    SceneConfig() : tcpPort(8080) {}

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
};

#endif // PROTOCOLTYPES_H
