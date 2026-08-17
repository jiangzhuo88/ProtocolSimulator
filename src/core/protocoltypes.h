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
    Hex,         // 十六进制字节序列
    StructArray  // 结构体数组: 每元素由subFields定义的多个字段组成, 内存交错布局(a0 b0 a1 b1...)
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
    TotalPackets, // 总包数
    EchoRequest   // 回显请求字段: 从匹配到的请求帧中按echoRefName取对应字段的字节原样返回(找不到则回退defaultValue)
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

    // 结构体数组的子字段定义(仅type==StructArray时用)
    // 每个子字段是完整的ProtocolParam(有独立type/byteOrder/isRandom/randomMin/randomMax等)
    // arrayCount表示结构体个数, 内存布局: field0[0] field1[0] field0[1] field1[1] ... (交错)
    // 子字段的arrayCount固定为1(不支持嵌套结构体数组)
    QVector<ProtocolParam> subFields;

    // 回显引用的请求参数名(仅dynamicType==EchoRequest时用)
    // 在回复参数中设置后: 匹配到请求帧时, 原样返回请求帧中同名参数的字节
    QString echoRefName;

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
    // echoMap: 请求帧的参数字节映射(name→bytes, 为空表示无请求上下文如主动上报/周期模式)
    QByteArray toBytes(quint64 seq = 0, int dataAreaLen = 0,
                       const QByteArray &fullFrame = QByteArray(),
                       int packetIndex = -1, int packetSize = -1, int totalPackets = -1,
                       const QMap<QString, QByteArray> *echoMap = nullptr) const;
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

// 预览模式RAII哨兵: 构造时进入预览模式, 析构时退出
// 预览模式下 ProtocolParam::toRandomBytes 不会生成真随机字节, 而是返回0xA5占位pattern
// — 对超大数组(如2000频点)的预览性能影响巨大: 从 ~100ms 降至 <1ms, 真实发送路径不受影响
class PreviewModeSentry {
public:
    PreviewModeSentry();
    ~PreviewModeSentry();
    PreviewModeSentry(const PreviewModeSentry&) = delete;
    PreviewModeSentry& operator=(const PreviewModeSentry&) = delete;
    // 判断当前是否处于预览模式(toRandomBytes内部调用)
    static bool isActive();
};

// 多包项(每包独立配置字段; MultiPacket回复模式使用)
// 每个包有自己的帧头/数据区参数; 字段可设动态类型=PacketIndex(包序号)/TotalPackets(总包数)/PacketSize(本包字节数)
// 发送时按列表顺序逐包发送, 动态字段自动填充, 非动态字段用默认值
struct MultiPacketItem {
    QString name;                          // 包名称(列表显示用)
    QVector<ProtocolParam> headerParams;   // 包帧头参数
    QVector<ProtocolParam> dataParams;     // 包数据区参数
    int delayMs;                           // 本包发送前的延迟(ms, <=0=用ReplyConfig.multiPacketIntervalMs)

    MultiPacketItem() : delayMs(0) {}

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
    // 构建本包帧; packetIndex/totalPackets用于动态字段自动填充
    // echoMap: 请求帧参数字节映射(用于EchoRequest字段回显)
    QByteArray buildFrame(quint64 seq, int packetIndex, int totalPackets,
                          const QMap<QString, QByteArray> *echoMap = nullptr) const;
};

// 回复配置
struct ReplyConfig {
    ReplyMode mode;                         // 回复模式
    int customIntervalMs;                   // 自定义周期(毫秒)
    QVector<ProtocolParam> headerParams;    // 回复帧头参数(发送区)
    QVector<ProtocolParam> dataParams;      // 回复数据区参数(发送区)
    QVector<MultiPacketItem> multiPackets;  // 多包列表(MultiPacket模式: 发送区帧后依次发送)
    int multiPacketIntervalMs;              // 多包默认发送间隔(ms)
    bool multiPacketCycle;                  // 多包循环(配合回复周期: 周期模式下每轮回到包1重发)

    ReplyConfig() : mode(ReplyMode::None), customIntervalMs(1000)
        , multiPacketIntervalMs(100), multiPacketCycle(false) {}

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
    // extraLen: 分包模式下, Length动态字段需额外包含的本包帧字节数(=本包包头+数据区)
    //           非分包场景传0, Length=回复区(包头+数据区)
    // requestFrame: 匹配到的请求帧原始字节(有请求上下文时传入, 用于EchoRequest回显)
    // requestParams: 请求帧的参数列表(帧头+数据区, 和requestFrame配合解析出各字段字节用于回显)
    //                传空则无法回显, EchoRequest字段回退defaultValue
    QByteArray buildReplyFrame(quint64 seq = 0, int extraLen = 0,
                               const QByteArray &requestFrame = QByteArray(),
                               const QVector<ProtocolParam> *requestParams = nullptr) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
};

// 场景配置
struct SceneConfig {
    QString name;                           // 场景名称
    int tcpPort;                            // TCP端口
    QVector<ProtocolConfig> protocols;      // 协议列表
    bool isRunning = false;

    SceneConfig() : tcpPort(8080) {}

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);
};

#endif // PROTOCOLTYPES_H
