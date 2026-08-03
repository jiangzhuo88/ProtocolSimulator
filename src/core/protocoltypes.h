#ifndef PROTOCOLTYPES_H
#define PROTOCOLTYPES_H

#include <QString>
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
    None,       // 非动态，使用默认值
    Timestamp,  // 当前时间戳(秒或毫秒)
    Length,     // 数据区长度
    Checksum,   // 校验和
    Sequence    // 自增序列号
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
    PeriodicCustom  // 自定义周期回复
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
    int userLength;            // 用户显式指定的长度
    int arrayCount;            // 数组元素个数（1=单个值，>1=同类型数组）

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
    QByteArray toBytes(quint64 seq = 0, int dataAreaLen = 0, const QByteArray &fullFrame = QByteArray()) const;
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

// 回复配置
struct ReplyConfig {
    ReplyMode mode;                         // 回复模式
    int customIntervalMs;                   // 自定义周期(毫秒)
    QVector<ProtocolParam> headerParams;    // 回复帧头参数
    QVector<ProtocolParam> dataParams;      // 回复数据区参数

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
    QStringList stopPeriodicProtocolNames;  //匹配成功时要停止的周期回复协议

    ProtocolConfig() : isActivePush(false), pushIntervalMs(1000) {}

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
