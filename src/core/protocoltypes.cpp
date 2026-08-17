#include "protocoltypes.h"
#include <QDataStream>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <cmath>

// ==================== PreviewModeSentry ====================
// 预览模式全局标志 — 仅 UI 预览期间为 true, 真实发送(toRandomBytes)跳过随机数生成改为0xA5占位pattern
// 显著降低大数组预览开销(例如2000频点随机生成从~100ms → <1ms)
static bool g_previewMode = false;
static int g_previewNest  = 0; // 支持嵌套(比如预览里又递归构帧)
PreviewModeSentry::PreviewModeSentry() { ++g_previewNest; g_previewMode = true; }
PreviewModeSentry::~PreviewModeSentry() { --g_previewNest; if (g_previewNest <= 0) { g_previewNest = 0; g_previewMode = false; } }
bool PreviewModeSentry::isActive() { return g_previewMode; }

// ==================== ProtocolParam ====================

int ProtocolParam::byteSize() const
{
    // 结构体数组: 单结构体大小(subFields总和) × 结构体个数(arrayCount)
    if (type == ParamType::StructArray) {
        int singleSize = 0;
        for (const auto &sf : subFields)
            singleSize += sf.byteSize();
        return singleSize * (arrayCount > 0 ? arrayCount : 1);
    }

    int singleSize = 0;
    // 数组且defaultValue为JSON数组时, 取第一个元素计算单元素大小
    QString val = defaultValue;
    if (arrayCount > 1 && val.trimmed().startsWith('[')) {
        QStringList vals = parseDefaultValues();
        if (!vals.isEmpty()) val = vals.first();
    }
    switch (type) {
    case ParamType::UInt8:  case ParamType::Int8:   singleSize = 1; break;
    case ParamType::UInt16: case ParamType::Int16:  singleSize = 2; break;
    case ParamType::UInt32: case ParamType::Int32:  case ParamType::Float32: singleSize = 4; break;
    case ParamType::UInt64: case ParamType::Int64:  case ParamType::Float64: singleSize = 8; break;
    case ParamType::String:     singleSize = userLength > 0 ? userLength : val.toLatin1().size(); break;
    case ParamType::StringUtf8: singleSize = userLength > 0 ? userLength : val.toUtf8().size(); break;
    case ParamType::Bytes:      singleSize = userLength > 0 ? userLength : (val.size() / 2); break;
    case ParamType::Hex: {
        if (userLength > 0) { singleSize = userLength; break; }
        QString s = val;
        s.remove(' ');
        singleSize = s.size() / 2;
        break;
    }
    case ParamType::StructArray: break; // 已在函数开头提前return, 不会到达
    }
    // 数组: 单元素大小 × 元素个数
    return singleSize * (arrayCount > 0 ? arrayCount : 1);
}

QByteArray ProtocolParam::toBytes(quint64 seq, int dataAreaLen, const QByteArray &fullFrame,
                                  int packetIndex, int packetSize, int totalPackets,
                                  const QMap<QString, QByteArray> *echoMap) const
{
    // 结构体数组: 交错布局 a0 b0 a1 b1 ...
    // 每个结构体实例遍历subFields, isRandom的用toRandomBytes, 否则用toBytes(各自defaultValue)
    if (type == ParamType::StructArray) {
        int count = arrayCount > 0 ? arrayCount : 1;
        QByteArray result;
        for (int i = 0; i < count; ++i) {
            for (const auto &sf : subFields) {
                if (sf.isRandom)
                    result += sf.toRandomBytes();
                else
                    result += sf.toBytes(seq, 0, QByteArray(), packetIndex, packetSize, totalPackets, echoMap);
            }
        }
        return result;
    }

    // 回显请求帧字段: 从echoMap中按echoRefName取出对应字段的字节原样返回
    // 找不到(比如主动上报/周期模式无请求上下文)或字节数不一致 → 回退defaultValue
    if (dynamicType == DynamicType::EchoRequest) {
        if (echoMap && !echoRefName.isEmpty()) {
            auto it = echoMap->find(echoRefName);
            if (it != echoMap->end()) {
                const QByteArray &raw = it.value();
                int mySize = byteSize();
                // 字节数吻合: 原样返回
                if (raw.size() == mySize || raw.isEmpty())
                    return raw;
                // 字节数不吻合: 截断或零填充到本参数字节数(避免错位)
                QByteArray out = raw;
                if (out.size() > mySize) out.truncate(mySize);
                while (out.size() < mySize) out.append('\0');
                return out;
            }
        }
        // 无请求上下文或找不到: 回退默认值(走下面的默认值构建逻辑)
    }

    // 处理动态类型
    if (dynamicType == DynamicType::Timestamp) {
        quint64 ts;
        if (dynamicParam == 0) // 秒
            ts = (quint64)QDateTime::currentDateTime().toTime_t();
        else // 毫秒
            ts = QDateTime::currentDateTime().toMSecsSinceEpoch();

        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        if (byteOrder == ByteOrder::BigEndian)
            ds.setByteOrder(QDataStream::BigEndian);
        else
            ds.setByteOrder(QDataStream::LittleEndian);

        int sz = byteSize();
        if (sz <= 2) ds << (quint16)ts;
        else if (sz <= 4) ds << (quint32)ts;
        else ds << ts;
        return data;
    }

    if (dynamicType == DynamicType::Length) {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        if (byteOrder == ByteOrder::BigEndian)
            ds.setByteOrder(QDataStream::BigEndian);
        else
            ds.setByteOrder(QDataStream::LittleEndian);

        int sz = byteSize();
        if (sz <= 1) ds << (quint8)dataAreaLen;
        else if (sz <= 2) ds << (quint16)dataAreaLen;
        else if (sz <= 4) ds << (quint32)dataAreaLen;
        else ds << (quint64)dataAreaLen;
        return data;
    }

    if (dynamicType == DynamicType::Sequence) {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        if (byteOrder == ByteOrder::BigEndian)
            ds.setByteOrder(QDataStream::BigEndian);
        else
            ds.setByteOrder(QDataStream::LittleEndian);

        int sz = byteSize();
        if (sz <= 1) ds << (quint8)seq;
        else if (sz <= 2) ds << (quint16)seq;
        else if (sz <= 4) ds << (quint32)seq;
        else ds << seq;
        return data;
    }

    // 分包序号
    if (dynamicType == DynamicType::PacketIndex) {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        int sz = byteSize();
        quint64 val = (quint64)qMax(0, packetIndex);
        if (sz <= 1) ds << (quint8)val;
        else if (sz <= 2) ds << (quint16)val;
        else if (sz <= 4) ds << (quint32)val;
        else ds << val;
        return data;
    }

    // 分包大小(当前包负载字节数)
    if (dynamicType == DynamicType::PacketSize) {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        int sz = byteSize();
        quint64 val = (quint64)qMax(0, packetSize);
        if (sz <= 1) ds << (quint8)val;
        else if (sz <= 2) ds << (quint16)val;
        else if (sz <= 4) ds << (quint32)val;
        else ds << val;
        return data;
    }

    // 总包数
    if (dynamicType == DynamicType::TotalPackets) {
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        int sz = byteSize();
        quint64 val = (quint64)qMax(0, totalPackets);
        if (sz <= 1) ds << (quint8)val;
        else if (sz <= 2) ds << (quint16)val;
        else if (sz <= 4) ds << (quint32)val;
        else ds << val;
        return data;
    }

    if (dynamicType == DynamicType::Checksum) {
        // 计算fullFrame中从dynamicParam开始到末尾的校验和
        quint8 sum = 0;
        if (!fullFrame.isEmpty()) {
            for (int i = dynamicParam; i < fullFrame.size(); ++i)
                sum += (quint8)fullFrame[i];
        }
        QByteArray data(1, (char)sum);
        return data;
    }

    // 非动态: 数组且defaultValue为JSON数组时, 逐元素生成
    if (arrayCount > 1 && defaultValue.trimmed().startsWith('[')) {
        QStringList vals = parseDefaultValues();
        QByteArray result;
        for (const auto &v : vals)
            result += singleElementToBytes(v);
        // 不足arrayCount个时用0补齐
        int singleSz = vals.isEmpty() ? 1 : (result.size() / vals.size());
        if (singleSz <= 0) singleSz = 1;
        while (result.size() < singleSz * arrayCount)
            result += singleElementToBytes("0");
        return result;
    }

    // 单值或旧式数组(重复单值)
    QByteArray singleData = singleElementToBytes(defaultValue);
    if (arrayCount <= 1) return singleData;
    QByteArray result;
    result.reserve(singleData.size() * arrayCount);
    for (int i = 0; i < arrayCount; ++i)
        result += singleData;
    return result;
}

QByteArray ProtocolParam::singleElementToBytes(const QString &val) const
{
    QByteArray singleData;
    switch (type) {
    case ParamType::UInt8: {
        bool ok;
        quint8 v = val.toUInt(&ok, 0);
        singleData = QByteArray(1, (char)v);
        break;
    }
    case ParamType::Int8: {
        bool ok;
        qint8 v = val.toInt(&ok, 0);
        singleData = QByteArray(1, (char)v);
        break;
    }
    case ParamType::UInt16: {
        bool ok;
        quint16 v = val.toUInt(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::Int16: {
        bool ok;
        qint16 v = val.toInt(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::UInt32: {
        bool ok;
        quint32 v = val.toUInt(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::Int32: {
        bool ok;
        qint32 v = val.toInt(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::UInt64: {
        bool ok;
        quint64 v = val.toULongLong(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::Int64: {
        bool ok;
        qint64 v = val.toLongLong(&ok, 0);
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::Float32: {
        float v = val.toFloat();
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::Float64: {
        double v = val.toDouble();
        QDataStream ds(&singleData, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        break;
    }
    case ParamType::String: {
        singleData = val.toLatin1();
        if (userLength > 0) singleData.resize(userLength);
        break;
    }
    case ParamType::StringUtf8: {
        singleData = val.toUtf8();
        if (userLength > 0) singleData.resize(userLength);
        break;
    }
    case ParamType::Bytes:
    case ParamType::Hex: {
        QString s = val;
        s.remove(' ').remove('\n').remove('\t');
        if (s.startsWith("0x", Qt::CaseInsensitive))
            s.remove(0, 2);
        singleData = QByteArray::fromHex(s.toLatin1());
        if (userLength > 0) singleData.resize(userLength);
        break;
    }
    case ParamType::StructArray: break; // 结构体数组走toBytes/toRandomBytes, 不走单元素转换
    }
    return singleData;
}

QStringList ProtocolParam::parseDefaultValuesFromString(const QString &defaultValue)
{
    QStringList result;
    QString s = defaultValue.trimmed();
    if (s.startsWith('[')) {
        QJsonDocument doc = QJsonDocument::fromJson(s.toUtf8());
        if (doc.isArray()) {
            for (const auto &v : doc.array())
                result.append(v.toString());
        }
    }
    if (result.isEmpty())
        result.append(defaultValue);
    return result;
}

QStringList ProtocolParam::parseDefaultValues() const
{
    return parseDefaultValuesFromString(defaultValue);
}

// 随机范围生成: 支持64位范围, 避免原来 quint32(range+1) 的溢出截断
static inline quint64 randRange64(quint64 lo, quint64 hi) {
    if (hi <= lo) return lo;
    quint64 range = hi - lo;
    if (range <= 0xFFFFFFFFULL) {
        // 32位范围内直接用bounded(quint32) — QRandomGenerator原生支持, 无偏差
        return lo + QRandomGenerator::global()->bounded((quint32)(range + 1));
    }
    // >32位: 用双生成 + 取模(有轻微偏差但64位范围工程上可接受, 且比quint32截断正确)
    quint64 r64 = ((quint64)QRandomGenerator::global()->generate() << 32)
                   | (quint64)QRandomGenerator::global()->generate();
    return lo + r64 % (range + 1);
}

// 小端写入辅助: 直接写原始字节, 免去每个元素构造QDataStream的开销
static inline void writeIntLE(uchar *p, quint64 val, int elemSize) {
    for (int k = 0; k < elemSize; ++k) { p[k] = (uchar)(val & 0xFF); val >>= 8; }
}
static inline void writeIntBE(uchar *p, quint64 val, int elemSize) {
    for (int k = elemSize - 1; k >= 0; --k) { p[k] = (uchar)(val & 0xFF); val >>= 8; }
}

QByteArray ProtocolParam::toRandomBytes() const
{
    // 预览模式快速路径: 不生成真随机, 返回 0xA5 占位pattern.
    // 2000频点/大数组场景下预览帧生成从~100ms降到<1ms, 用户仍可从预览看出长度/结构/偏移是否正确.
    if (Q_UNLIKELY(PreviewModeSentry::isActive())) {
        int totalBytes = byteSize();
        if (totalBytes <= 0) return QByteArray();
        QByteArray out(totalBytes, 0);
        uchar *p = (uchar*)out.data();
        // 用 0xA5 / 0x5A 交替 pattern — 非全0也非全FF, 看一眼就知道这是"预览占位值"
        for (int i = 0; i < totalBytes; ++i)
            p[i] = (i & 1) ? 0x5Au : 0xA5u;
        return out;
    }

    // 结构体数组: 随机性由各子字段独立决定(子字段isRandom控制), 外层isRandom无意义
    // 交错布局: field0[0] field1[0] field0[1] field1[1] ...
    if (type == ParamType::StructArray) {
        int count = arrayCount > 0 ? arrayCount : 1;
        if (count <= 0) return QByteArray();
        // --- 快速预览模式: 整体一块0xA5/0x5A pattern, 跳过所有子字段遍历 ---
        if (Q_UNLIKELY(PreviewModeSentry::isActive())) {
            int perStructSize = 0;
            for (const auto &sf : subFields) perStructSize += sf.byteSize();
            int total = count * perStructSize;
            QByteArray out(total, 0);
            uchar *p = (uchar*)out.data();
            for (int i = 0; i < total; ++i) p[i] = (i & 1) ? 0x5Au : 0xA5u;
            return out;
        }

        // --- 正常模式: 一次性"每个子字段信息"预解析, 然后count次循环内联写 ---
        // 避免原来 count×|subFields| 次递归调用 sf.toRandomBytes()/sf.toBytes()
        // — 原本会带来 O(N) 次QByteArray小分配 + O(N) 次randomMin/randomMax字符串重复解析
        struct SfSpec {
            int  elemSize;             // 本字段每实例字节数 (sf.byteSize(), subField的arrayCount固定=1)
            bool isRandom;             // 是否生成随机值(否则取defaultValue的固定字节)
            bool bigEnd;

            // 非随机固定字节缓存(长度=elemSize) — 避免每个实例都调用sf.toBytes()
            QByteArray fixedBytes;

            // 随机分支: 类型分类(INT / FLOAT / BYTES)
            enum class Kind { Int, Float, Bytes };
            Kind kind;

            // INT类随机: loU/hiU为随机范围(uint64形式), mask截断到elemSize位宽
            quint64 loU, hiU, mask;
            // FLOAT类随机: minF/maxF
            double  minF, maxF;
            // BYTES类随机: rawLen = elemSize(randomLength<=0时) 或 randomLength
            int rawLen;
        };

        QVarLengthArray<SfSpec, 8> specs;
        specs.resize(subFields.size());
        int perStructSize = 0;

        for (int k = 0; k < subFields.size(); ++k) {
            const auto &sf = subFields[k];
            SfSpec &s = specs[k];
            s.elemSize = sf.byteSize();
            if (s.elemSize <= 0) s.elemSize = 1;
            perStructSize += s.elemSize;
            s.isRandom = sf.isRandom;
            s.bigEnd   = (sf.byteOrder == ByteOrder::BigEndian);

            if (!s.isRandom) {
                s.fixedBytes = sf.toBytes(); // 非随机: 只算一次并缓存
                // 对齐到 elemSize(防止返回不一致, 虽然toBytes正常时应等于byteSize)
                if (s.fixedBytes.size() != s.elemSize) {
                    if (s.fixedBytes.size() > s.elemSize) s.fixedBytes.truncate(s.elemSize);
                    else while (s.fixedBytes.size() < s.elemSize) s.fixedBytes.append('\0');
                }
                continue;
            }

            // --- 随机: 分类并一次性解析范围 ---
            if (sf.type == ParamType::Bytes || sf.type == ParamType::Hex) {
                s.kind   = SfSpec::Kind::Bytes;
                s.rawLen = sf.randomLength;
                if (s.rawLen <= 0) s.rawLen = s.elemSize;
            } else if (sf.type == ParamType::Float32 || sf.type == ParamType::Float64) {
                s.kind   = SfSpec::Kind::Float;
                s.minF = sf.randomMin.isEmpty() ? 0.0 : sf.randomMin.toDouble();
                s.maxF = sf.randomMax.isEmpty() ? 1.0 : sf.randomMax.toDouble();
                if (s.maxF < s.minF) std::swap(s.minF, s.maxF);
            } else {
                // 整型: UInt8/16/32/64, Int8/16/32/64
                s.kind = SfSpec::Kind::Int;
                bool okMin, okMax;
                qint64 minV = sf.randomMin.toLongLong(&okMin, 0);
                if (!okMin) minV = 0;
                qint64 maxV = sf.randomMax.toLongLong(&okMax, 0);
                if (!okMax) maxV = 65535;
                if (maxV < minV) std::swap(minV, maxV);
                s.loU  = (quint64)minV;
                s.hiU  = (quint64)maxV;
                s.mask = (s.elemSize >= 8) ? 0xFFFFFFFFFFFFFFFFULL
                                           : ((1ULL << (s.elemSize * 8)) - 1ULL);
            }
        }

        int total = count * perStructSize;
        QByteArray result(total, 0);
        uchar *dst = (uchar*)result.data();
        QRandomGenerator *rng = QRandomGenerator::global();

        for (int i = 0; i < count; ++i) {
            for (int k = 0; k < specs.size(); ++k) {
                SfSpec &s = specs[k];
                uchar *wp = dst;
                int sz = s.elemSize;

                if (!s.isRandom) {
                    // 非随机: 直接memcpy缓存好的固定字节
                    if (Q_LIKELY(sz == s.fixedBytes.size()))
                        memcpy(wp, s.fixedBytes.constData(), sz);
                    dst += sz;
                    continue;
                }

                switch (s.kind) {
                case SfSpec::Kind::Bytes: {
                    int len = s.rawLen;
                    if (len > sz) len = sz;
                    // 批量填4字节对齐 + 尾字节
                    rng->fillRange((quint32*)wp, len / 4);
                    int tail = len & 3;
                    if (tail) {
                        uchar *tp = wp + (len - tail);
                        quint32 v = rng->generate();
                        for (int m = 0; m < tail; ++m) { tp[m] = (uchar)(v & 0xFF); v >>= 8; }
                    }
                    // 剩余超过rawLen的字节(只有rawLen<elemSize时)填0
                    if (len < sz) memset(wp + len, 0, sz - len);
                    dst += sz;
                    break;
                }
                case SfSpec::Kind::Float: {
                    double r = rng->generateDouble();
                    double val = s.minF + r * (s.maxF - s.minF);
                    if (sz <= 4) {
                        float fv = (float)val;
                        quint32 bits; memcpy(&bits, &fv, 4);
                        if (s.bigEnd) writeIntBE(wp, bits, 4);
                        else          writeIntLE(wp, bits, 4);
                    } else {
                        quint64 bits; memcpy(&bits, &val, 8);
                        if (s.bigEnd) writeIntBE(wp, bits, 8);
                        else          writeIntLE(wp, bits, 8);
                    }
                    dst += sz;
                    break;
                }
                default: // SfSpec::Kind::Int
                {
                    quint64 v = randRange64(s.loU, s.hiU);
                    v &= s.mask;
                    if (s.bigEnd) writeIntBE(wp, v, sz);
                    else          writeIntLE(wp, v, sz);
                    dst += sz;
                    break;
                }
                }
            }
        }
        return result;
    }

    if (!isRandom) return toBytes();

    int count = arrayCount > 0 ? arrayCount : 1;
    if (count <= 0) count = 1;

    // --- 路径1: Bytes/Hex raw bytes 批量填随机 ---
    if (type == ParamType::Bytes || type == ParamType::Hex) {
        int len = randomLength;
        if (len <= 0) len = byteSize();
        QByteArray data(len, 0);
        if (len > 0) {
            // QRandomGenerator::fillRange 填整个buffer — 比逐字节bounded(256)快1个数量级
            QRandomGenerator::global()->fillRange((quint32*)data.data(), len / 4);
            int tail = len & 3;
            if (tail) {
                uchar *tp = (uchar*)data.data() + (len - tail);
                quint32 v = QRandomGenerator::global()->generate();
                for (int k = 0; k < tail; ++k) { tp[k] = (uchar)(v & 0xFF); v >>= 8; }
            }
        }
        return data;
    }

    bool bigEnd = (byteOrder == ByteOrder::BigEndian);

    // --- 路径2: 浮点 Float32/Float64 ---
    if (type == ParamType::Float32 || type == ParamType::Float64) {
        double minVal = randomMin.isEmpty() ? 0.0 : randomMin.toDouble();
        double maxVal = randomMax.isEmpty() ? 1.0 : randomMax.toDouble();
        if (maxVal < minVal) std::swap(minVal, maxVal);
        int elemSize = (type == ParamType::Float32) ? 4 : 8;
        int total = elemSize * count;
        QByteArray result(total, 0);
        uchar *p = (uchar*)result.data();
        QRandomGenerator *rng = QRandomGenerator::global();
        for (int i = 0; i < count; ++i) {
            double r = rng->generateDouble();
            double val = minVal + r * (maxVal - minVal);
            if (type == ParamType::Float32) {
                float fv = (float)val;
                // 通过 memcpy 到 uchar* 保持与平台无关, 避免QDataStream开销
                quint32 bits;
                memcpy(&bits, &fv, 4);
                if (bigEnd) writeIntBE(p + i*4, bits, 4);
                else        writeIntLE(p + i*4, bits, 4);
            } else {
                quint64 bits;
                memcpy(&bits, &val, 8);
                if (bigEnd) writeIntBE(p + i*8, bits, 8);
                else        writeIntLE(p + i*8, bits, 8);
            }
        }
        return result;
    }

    // --- 路径3: 整型 (UInt8/16/32/64, Int8/16/32/64) — 热点路径, 2000频点的情况 ---
    // 只解析一次min/max字符串, 而非每个元素都解析
    bool okMin, okMax;
    qint64 minVal = randomMin.toLongLong(&okMin, 0);
    if (!okMin) minVal = 0;
    qint64 maxVal = randomMax.toLongLong(&okMax, 0);
    if (!okMax) maxVal = 65535;
    if (maxVal < minVal) std::swap(minVal, maxVal);
    quint64 loU = (quint64)minVal;
    quint64 hiU = (quint64)maxVal;

    int elemSize = byteSize() / count;
    if (elemSize <= 0) elemSize = 1;
    int total = elemSize * count;
    QByteArray result(total, 0);
    uchar *p = (uchar*)result.data();
    // elemSize 截断掩码: 比如 elemSize=2 就 mask=0xFFFF, 防止超出类型位宽
    quint64 mask = (elemSize >= 8) ? 0xFFFFFFFFFFFFFFFFULL
                                   : ((1ULL << (elemSize*8)) - 1ULL);
    if (bigEnd) {
        for (int i = 0; i < count; ++i) {
            quint64 v = randRange64(loU, hiU) & mask;
            writeIntBE(p + i*elemSize, v, elemSize);
        }
    } else {
        for (int i = 0; i < count; ++i) {
            quint64 v = randRange64(loU, hiU) & mask;
            writeIntLE(p + i*elemSize, v, elemSize);
        }
    }
    return result;
}

QString ProtocolParam::fromBytes(const QByteArray &data, ParamType type, ByteOrder order)
{
    if (data.isEmpty()) return QString();

    QDataStream ds(data);
    ds.setByteOrder(order == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);

    switch (type) {
    case ParamType::UInt8: { quint8 v; ds >> v; return QString::number(v); }
    case ParamType::Int8: { qint8 v; ds >> v; return QString::number(v); }
    case ParamType::UInt16: { quint16 v; ds >> v; return QString::number(v); }
    case ParamType::Int16: { qint16 v; ds >> v; return QString::number(v); }
    case ParamType::UInt32: { quint32 v; ds >> v; return QString::number(v); }
    case ParamType::Int32: { qint32 v; ds >> v; return QString::number(v); }
    case ParamType::UInt64: { quint64 v; ds >> v; return QString::number(v); }
    case ParamType::Int64: { qint64 v; ds >> v; return QString::number(v); }
    case ParamType::Float32: { float v; ds.setFloatingPointPrecision(QDataStream::SinglePrecision); ds >> v; return QString::number(v); }
    case ParamType::Float64: { double v; ds.setFloatingPointPrecision(QDataStream::DoublePrecision); ds >> v; return QString::number(v); }
    case ParamType::String: return QString::fromLatin1(data);
    case ParamType::StringUtf8: return QString::fromUtf8(data);
    case ParamType::Bytes:
    case ParamType::Hex: return QString::fromLatin1(data.toHex(' '));
    case ParamType::StructArray: return QString::fromLatin1(data.toHex(' ')); // 结构体数组不参与单值解码, 返回hex
    }
    return QString();
}

QByteArray ProtocolParam::matchValueToBytes(const QString &value) const
{
    // 数值类型: 按字段类型和字节序序列化(与defaultValue处理方式一致)
    switch (type) {
    case ParamType::UInt8: {
        bool ok;
        quint8 v = value.toUInt(&ok, 0);
        return QByteArray(1, (char)v);
    }
    case ParamType::Int8: {
        bool ok;
        qint8 v = value.toInt(&ok, 0);
        return QByteArray(1, (char)v);
    }
    case ParamType::UInt16: {
        bool ok;
        quint16 v = value.toUInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int16: {
        bool ok;
        qint16 v = value.toInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::UInt32: {
        bool ok;
        quint32 v = value.toUInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int32: {
        bool ok;
        qint32 v = value.toInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::UInt64: {
        bool ok;
        quint64 v = value.toULongLong(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int64: {
        bool ok;
        qint64 v = value.toLongLong(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Float32: {
        float v = value.toFloat();
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Float64: {
        double v = value.toDouble();
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::String:
        return value.toLatin1();
    case ParamType::StringUtf8:
        return value.toUtf8();
    case ParamType::Bytes:
    case ParamType::Hex: {
        // 去掉0x/0X前缀, 再fromHex
        QString s = value;
        s.remove(' ').remove('\n').remove('\t');
        if (s.startsWith("0x", Qt::CaseInsensitive))
            s.remove(0, 2);
        return QByteArray::fromHex(s.toLatin1());
    }
    case ParamType::StructArray: return QByteArray(); // 结构体数组不参与匹配值转换
    }
    return QByteArray();
}

bool ProtocolParam::match(const QByteArray &received) const
{
    if (!matchEnabled) return true; // 未启用匹配，默认通过

    if (matchMode == MatchMode::Any) return true;

    if (matchMode == MatchMode::Range) {
        // 范围匹配: 对数值类型, 将received解码为数值后比较
        QString recvStr = fromBytes(received, type, byteOrder);
        bool ok1, ok2;
        qint64 recvVal = recvStr.toLongLong(&ok1, 0);
        qint64 minVal = matchValue.toLongLong(&ok1, 0);
        qint64 maxVal = matchValue2.toLongLong(&ok2, 0);
        if (!ok1 || !ok2) return false;
        return recvVal >= minVal && recvVal <= maxVal;
    }

    // 精确/前缀/掩码匹配: 用matchValueToBytes转换(自动处理0x前缀和字节序)
    QByteArray expected = matchValueToBytes(matchValue);

    if (matchMode == MatchMode::Exact) {
        return received == expected;
    }

    if (matchMode == MatchMode::Prefix) {
        return received.startsWith(expected);
    }

    if (matchMode == MatchMode::Mask) {
        QByteArray mask = expected;
        QByteArray expVal = matchValueToBytes(matchValue2);
        if (received.size() != mask.size()) return false;
        for (int i = 0; i < received.size(); ++i) {
            if ((quint8)(received[i] & mask[i]) != (quint8)expVal[i])
                return false;
        }
        return true;
    }

    return false;
}

bool ProtocolParam::match(const uchar *data, int off, int size) const
{
    if (!matchEnabled) return true;
    if (matchMode == MatchMode::Any) return true;
    if (!data || off < 0 || size <= 0) return false;
    const uchar *base = data + off;

    if (matchMode == MatchMode::Range) {
        // 范围匹配: 将size字节整数按字节序解析
        bool be = (byteOrder == ByteOrder::BigEndian);
        quint64 vU = 0;
        if (size >= 8) {
            for (int k = 0; k < 8; ++k) {
                int idx = be ? k : (7 - k);
                vU = (vU << 8) | (quint64)base[idx];
            }
        } else {
            for (int k = 0; k < size; ++k) {
                int idx = be ? k : (size - 1 - k);
                vU = (vU << 8) | (quint64)base[idx];
            }
            // 按类型位宽做符号扩展(比较qint64时用)
            int bit = size * 8;
            quint64 sign = 1ULL << (bit - 1);
            if (vU & sign) vU |= ~((1ULL << bit) - 1ULL);
        }
        qint64 recvVal = (qint64)vU;
        bool ok1, ok2;
        qint64 minVal = matchValue.toLongLong(&ok1, 0);
        qint64 maxVal = matchValue2.toLongLong(&ok2, 0);
        if (!ok1 || !ok2) return false;
        return recvVal >= minVal && recvVal <= maxVal;
    }

    // Exact/Prefix/Mask: 直接拿matchValue转成expected(字节序列)做比较
    QByteArray expected = matchValueToBytes(matchValue);
    const char *exp = expected.constData();
    int esz = expected.size();

    if (matchMode == MatchMode::Exact) {
        if (esz != size) return false;
        return memcmp(base, exp, size) == 0;
    }
    if (matchMode == MatchMode::Prefix) {
        if (esz > size) return false;
        return memcmp(base, exp, esz) == 0;
    }
    if (matchMode == MatchMode::Mask) {
        if (esz != size) return false;
        QByteArray expVal = matchValueToBytes(matchValue2);
        if (expVal.size() != size) return false;
        const uchar *mv = (const uchar*)exp;
        const uchar *ev = (const uchar*)expVal.constData();
        for (int k = 0; k < size; ++k) {
            if ((base[k] & mv[k]) != (ev[k] & mv[k])) return false;
        }
        return true;
    }
    return false;
}

QJsonObject ProtocolParam::toJson() const
{
    QJsonObject o;
    o["name"] = name;
    o["type"] = typeToString(type);
    o["byteOrder"] = byteOrderToString(byteOrder);
    o["defaultValue"] = defaultValue;
    o["dynamicType"] = dynamicTypeToString(dynamicType);
    o["dynamicParam"] = dynamicParam;
    o["matchEnabled"] = matchEnabled;
    o["matchMode"] = matchModeToString(matchMode);
    o["matchValue"] = matchValue;
    o["matchValue2"] = matchValue2;
    o["userLength"] = userLength;
    o["arrayCount"] = arrayCount;
    o["isRandom"] = isRandom;
    o["randomMin"] = randomMin;
    o["randomMax"] = randomMax;
    o["randomLength"] = randomLength;
    // EchoRequest回显引用参数名
    if (!echoRefName.isEmpty())
        o["echoRefName"] = echoRefName;
    // 结构体数组子字段(仅StructArray类型有内容)
    if (type == ParamType::StructArray && !subFields.isEmpty()) {
        QJsonArray sfArr;
        for (const auto &sf : subFields) sfArr.append(sf.toJson());
        o["subFields"] = sfArr;
    }
    return o;
}

void ProtocolParam::fromJson(const QJsonObject &o)
{
    name = o["name"].toString();
    type = stringToType(o["type"].toString());
    byteOrder = stringToByteOrder(o["byteOrder"].toString());
    defaultValue = o["defaultValue"].toString();
    dynamicType = stringToDynamicType(o["dynamicType"].toString());
    dynamicParam = o["dynamicParam"].toInt(0);
    matchEnabled = o["matchEnabled"].toBool(false);
    matchMode = stringToMatchMode(o["matchMode"].toString());
    matchValue = o["matchValue"].toString();
    matchValue2 = o["matchValue2"].toString();
    userLength = o["userLength"].toInt(0);
    arrayCount = o["arrayCount"].toInt(1);
    isRandom = o["isRandom"].toBool(false);
    randomMin = o["randomMin"].toString();
    randomMax = o["randomMax"].toString();
    randomLength = o["randomLength"].toInt(8);
    echoRefName = o["echoRefName"].toString();
    // 结构体数组子字段
    subFields.clear();
    QJsonArray sfArr = o["subFields"].toArray();
    for (const auto &v : sfArr) { ProtocolParam sf; sf.fromJson(v.toObject()); subFields.append(sf); }
}

QString ProtocolParam::typeToString(ParamType t)
{
    switch (t) {
    case ParamType::UInt8: return "UInt8";
    case ParamType::UInt16: return "UInt16";
    case ParamType::UInt32: return "UInt32";
    case ParamType::UInt64: return "UInt64";
    case ParamType::Int8: return "Int8";
    case ParamType::Int16: return "Int16";
    case ParamType::Int32: return "Int32";
    case ParamType::Int64: return "Int64";
    case ParamType::Float32: return "Float32";
    case ParamType::Float64: return "Float64";
    case ParamType::String: return "String";
    case ParamType::StringUtf8: return "StringUtf8";
    case ParamType::Bytes: return "Bytes";
    case ParamType::Hex: return "Hex";
    case ParamType::StructArray: return "StructArray";
    }
    return "UInt16";
}

ParamType ProtocolParam::stringToType(const QString &s)
{
    if (s == "UInt8") return ParamType::UInt8;
    if (s == "UInt16") return ParamType::UInt16;
    if (s == "UInt32") return ParamType::UInt32;
    if (s == "UInt64") return ParamType::UInt64;
    if (s == "Int8") return ParamType::Int8;
    if (s == "Int16") return ParamType::Int16;
    if (s == "Int32") return ParamType::Int32;
    if (s == "Int64") return ParamType::Int64;
    if (s == "Float32") return ParamType::Float32;
    if (s == "Float64") return ParamType::Float64;
    if (s == "String") return ParamType::String;
    if (s == "StringUtf8") return ParamType::StringUtf8;
    if (s == "Bytes") return ParamType::Bytes;
    if (s == "Hex") return ParamType::Hex;
    if (s == "StructArray") return ParamType::StructArray;
    return ParamType::UInt16;
}

QString ProtocolParam::byteOrderToString(ByteOrder o)
{
    return o == ByteOrder::BigEndian ? "BigEndian" : "LittleEndian";
}

ByteOrder ProtocolParam::stringToByteOrder(const QString &s)
{
    return s == "LittleEndian" ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
}

QString ProtocolParam::dynamicTypeToString(DynamicType d)
{
    switch (d) {
    case DynamicType::None: return "None";
    case DynamicType::Timestamp: return "Timestamp";
    case DynamicType::Length: return "Length";
    case DynamicType::Checksum: return "Checksum";
    case DynamicType::Sequence: return "Sequence";
    case DynamicType::PacketIndex: return "PacketIndex";
    case DynamicType::PacketSize: return "PacketSize";
    case DynamicType::TotalPackets: return "TotalPackets";
    case DynamicType::EchoRequest: return "EchoRequest";
    }
    return "None";
}

DynamicType ProtocolParam::stringToDynamicType(const QString &s)
{
    if (s == "Timestamp") return DynamicType::Timestamp;
    if (s == "Length") return DynamicType::Length;
    if (s == "Checksum") return DynamicType::Checksum;
    if (s == "Sequence") return DynamicType::Sequence;
    if (s == "PacketIndex") return DynamicType::PacketIndex;
    if (s == "PacketSize") return DynamicType::PacketSize;
    if (s == "TotalPackets") return DynamicType::TotalPackets;
    if (s == "EchoRequest") return DynamicType::EchoRequest;
    return DynamicType::None;
}

QString ProtocolParam::matchModeToString(MatchMode m)
{
    switch (m) {
    case MatchMode::Exact: return "Exact";
    case MatchMode::Prefix: return "Prefix";
    case MatchMode::Mask: return "Mask";
    case MatchMode::Range: return "Range";
    case MatchMode::Any: return "Any";
    }
    return "Exact";
}

MatchMode ProtocolParam::stringToMatchMode(const QString &s)
{
    if (s == "Prefix") return MatchMode::Prefix;
    if (s == "Mask") return MatchMode::Mask;
    if (s == "Range") return MatchMode::Range;
    if (s == "Any") return MatchMode::Any;
    return MatchMode::Exact;
}

// ==================== MultiPacketItem ====================

QByteArray MultiPacketItem::buildFrame(quint64 seq, int packetIndex, int totalPackets,
                                       const QMap<QString, QByteArray> *echoMap) const
{
    // 先构建数据区
    QByteArray dataArea;
    for (const auto &p : dataParams) {
        if (p.isRandom)
            dataArea += p.toRandomBytes();
        else
            dataArea += p.toBytes(seq, 0, QByteArray(), packetIndex, dataArea.size(), totalPackets, echoMap);
    }

    int pktSize = dataArea.size(); // 本包数据区字节数(PacketSize动态字段用)

    // 计算帧头总长度
    int headerSize = 0;
    for (const auto &p : headerParams)
        headerSize += p.byteSize();

    // 构建帧头
    QByteArray header;
    for (const auto &p : headerParams) {
        if (p.isRandom) {
            header += p.toRandomBytes();
        } else {
            int len = dataArea.size();
            if (p.dynamicType == DynamicType::Length)
                len = dataArea.size() + headerSize;
            header += p.toBytes(seq, len, QByteArray(), packetIndex, pktSize, totalPackets, echoMap);
        }
    }

    QByteArray frame = header + dataArea;

    // 第二遍: 校验和字段
    QByteArray finalFrame;
    int hdrOffset = 0;
    for (const auto &p : headerParams) {
        if (p.dynamicType == DynamicType::Checksum) {
            finalFrame += p.toBytes(seq, dataArea.size(), frame, packetIndex, pktSize, totalPackets, echoMap);
        } else {
            finalFrame += frame.mid(hdrOffset, p.byteSize());
        }
        hdrOffset += p.byteSize();
    }
    finalFrame += dataArea;

    return finalFrame;
}

QJsonObject MultiPacketItem::toJson() const
{
    QJsonObject o;
    o["name"] = name;
    o["delayMs"] = delayMs;

    QJsonArray hdr;
    for (const auto &p : headerParams) hdr.append(p.toJson());
    o["headerParams"] = hdr;

    QJsonArray dat;
    for (const auto &p : dataParams) dat.append(p.toJson());
    o["dataParams"] = dat;
    return o;
}

void MultiPacketItem::fromJson(const QJsonObject &o)
{
    name = o["name"].toString();
    delayMs = o["delayMs"].toInt(0);

    headerParams.clear();
    QJsonArray hdr = o["headerParams"].toArray();
    for (const auto &v : hdr) { ProtocolParam p; p.fromJson(v.toObject()); headerParams.append(p); }

    dataParams.clear();
    QJsonArray dat = o["dataParams"].toArray();
    for (const auto &v : dat) { ProtocolParam p; p.fromJson(v.toObject()); dataParams.append(p); }
}

// ==================== ReplyConfig ====================

QJsonObject ReplyConfig::toJson() const
{
    QJsonObject o;
    o["mode"] = modeToString(mode);
    o["customIntervalMs"] = customIntervalMs;

    QJsonArray hdr;
    for (const auto &p : headerParams) hdr.append(p.toJson());
    o["headerParams"] = hdr;

    QJsonArray dat;
    for (const auto &p : dataParams) dat.append(p.toJson());
    o["dataParams"] = dat;

    o["multiPacketIntervalMs"] = multiPacketIntervalMs;
    o["multiPacketCycle"] = multiPacketCycle;
    QJsonArray mpArr;
    for (const auto &mp : multiPackets) mpArr.append(mp.toJson());
    o["multiPackets"] = mpArr;
    return o;
}

void ReplyConfig::fromJson(const QJsonObject &o)
{
    mode = stringToMode(o["mode"].toString());
    customIntervalMs = o["customIntervalMs"].toInt(1000);
    headerParams.clear();
    dataParams.clear();
    QJsonArray hdr = o["headerParams"].toArray();
    for (const auto &v : hdr) { ProtocolParam p; p.fromJson(v.toObject()); headerParams.append(p); }
    QJsonArray dat = o["dataParams"].toArray();
    for (const auto &v : dat) { ProtocolParam p; p.fromJson(v.toObject()); dataParams.append(p); }

    multiPacketIntervalMs = o["multiPacketIntervalMs"].toInt(100);
    multiPacketCycle = o["multiPacketCycle"].toBool(false);
    multiPackets.clear();
    QJsonArray mpArr = o["multiPackets"].toArray();
    for (const auto &v : mpArr) { MultiPacketItem mp; mp.fromJson(v.toObject()); multiPackets.append(mp); }
}

QString ReplyConfig::modeToString(ReplyMode m)
{
    switch (m) {
    case ReplyMode::None: return "None";
    case ReplyMode::Once: return "Once";
    case ReplyMode::Periodic1s: return "Periodic1s";
    case ReplyMode::Periodic5s: return "Periodic5s";
    case ReplyMode::PeriodicCustom: return "PeriodicCustom";
    case ReplyMode::MultiPacket: return "MultiPacket";
    }
    return "None";
}

ReplyMode ReplyConfig::stringToMode(const QString &s)
{
    if (s == "Once") return ReplyMode::Once;
    if (s == "Periodic1s") return ReplyMode::Periodic1s;
    if (s == "Periodic5s") return ReplyMode::Periodic5s;
    if (s == "PeriodicCustom") return ReplyMode::PeriodicCustom;
    if (s == "MultiPacket") return ReplyMode::MultiPacket;
    return ReplyMode::None;
}

// ==================== ProtocolConfig ====================

void ProtocolConfig::recalcCachedSizes()
{
    int hs = 0, ds = 0, rphs = 0, rpds = 0;
    for (const auto &p : headerParams)          hs  += p.byteSize();
    for (const auto &p : dataParams)            ds  += p.byteSize();
    for (const auto &p : replyConfig.headerParams) rphs += p.byteSize();
    for (const auto &p : replyConfig.dataParams)   rpds += p.byteSize();
    cachedHeaderSize = hs;
    cachedDataSize   = ds;
    cachedReplyHeaderSize = rphs;
    cachedReplyDataSize   = rpds;
    int total = hs + ds;
    if (fixedFrameLength > 0) total = fixedFrameLength;
    cachedTotalFrameSize = total;
    // minNeeded用于resync: buffer够大才允许丢第一个字节重试
    cachedMinFrameSize   = total > 0 ? total : 1;
}

bool ProtocolConfig::isValid() const
{
    if (isActivePush) return true;
    for (const auto &p : headerParams)
        if (p.matchEnabled) return true;
    for (const auto &p : dataParams)
        if (p.matchEnabled) return true;
    return false;
}

QVector<const ProtocolParam*> ProtocolConfig::matchParams() const
{
    QVector<const ProtocolParam*> result;
    for (const auto &p : headerParams)
        if (p.matchEnabled) result.append(&p);
    for (const auto &p : dataParams)
        if (p.matchEnabled) result.append(&p);
    return result;
}

QByteArray ProtocolConfig::buildFrame(quint64 seq) const
{
    // 先构建数据区
    QByteArray dataArea;
    for (const auto &p : dataParams)
        dataArea += p.toBytes(seq, 0);

    // 计算帧头总长度
    int headerSize = 0;
    for (const auto &p : headerParams)
        headerSize += p.byteSize();

    // 构建帧头(需要数据区长度)
    QByteArray header;
    for (const auto &p : headerParams) {
        int len = dataArea.size();
        if (p.dynamicType == DynamicType::Length)
            len = dataArea.size() + headerSize;
        header += p.toBytes(seq, len);
    }

    // 重新构建含有校验和的字段
    QByteArray frame = header + dataArea;

    // 第二遍: 处理校验和字段(需要完整帧)
    QByteArray finalFrame;
    int hdrOffset = 0;
    for (const auto &p : headerParams) {
        if (p.dynamicType == DynamicType::Checksum) {
            finalFrame += p.toBytes(seq, dataArea.size(), frame);
        } else {
            finalFrame += frame.mid(hdrOffset, p.byteSize());
        }
        hdrOffset += p.byteSize();
    }
    finalFrame += dataArea;

    return finalFrame;
}

// 辅助: 根据requestParams参数定义 + requestFrame原始字节, 解析出"参数名→字节"映射供EchoRequest查用
static QMap<QString, QByteArray> buildEchoMap(const QVector<ProtocolParam> *requestParams,
                                               const QByteArray &requestFrame)
{
    QMap<QString, QByteArray> m;
    if (!requestParams || requestFrame.isEmpty()) return m;
    int off = 0;
    for (const auto &p : *requestParams) {
        int sz = p.byteSize();
        if (off + sz <= requestFrame.size() && !p.name.isEmpty()) {
            m.insert(p.name, requestFrame.mid(off, sz));
        }
        off += sz;
    }
    return m;
}

QByteArray ProtocolConfig::buildReplyFrame(quint64 seq, int extraLen,
                                           const QByteArray &requestFrame,
                                           const QVector<ProtocolParam> *requestParams) const
{
    // 构建回显映射(EchoRequest动态字段查找)
    QMap<QString, QByteArray> localEchoMap = buildEchoMap(requestParams, requestFrame);
    const QMap<QString, QByteArray> *echoMap = localEchoMap.isEmpty() ? nullptr : &localEchoMap;

    // 主动上报兼容: 如果replyConfig.headerParams/dataParams都为空, 回退用headerParams/dataParams作为发送集
    // — 用户心智: "主动上报就是把这些字段按周期发出去", 不用区分"接收参数表/回复参数表"
    //   这样用户在任何一张参数表里配的字段(含随机/Length/Checksum动态/StructArray数组随机)都能正确发送
    const QVector<ProtocolParam> *sendHeaders = &replyConfig.headerParams;
    const QVector<ProtocolParam> *sendData    = &replyConfig.dataParams;
    if (sendHeaders->isEmpty() && sendData->isEmpty()) {
        sendHeaders = &headerParams;
        sendData    = &dataParams;
    }

    // 构建回复数据区(支持随机值)
    // 优先用cached sizes, 否则fallback累加 (setProtocols/fromJson都已调recalcCachedSizes)
    int dataSize;
    int headerSize;
    if (sendData == &dataParams && sendHeaders == &headerParams) {
        dataSize   = cachedDataSize;
        headerSize = cachedHeaderSize;
    } else {
        dataSize   = cachedReplyDataSize;
        headerSize = cachedReplyHeaderSize;
    }
    if (dataSize <= 0) {
        dataSize = 0;
        for (const auto &p : *sendData) dataSize += p.byteSize();
    }
    if (headerSize <= 0) {
        headerSize = 0;
        for (const auto &p : *sendHeaders) headerSize += p.byteSize();
    }

    QByteArray dataArea;
    dataArea.reserve(dataSize + 16);
    for (const auto &p : *sendData) {
        if (p.isRandom)
            dataArea.append(p.toRandomBytes());
        else
            dataArea.append(p.toBytes(seq, 0, QByteArray(), -1, -1, -1, echoMap));
    }

    // 构建回复帧头
    QByteArray header;
    header.reserve(headerSize + 16);
    for (const ProtocolParam &p : *sendHeaders) {
        if (p.isRandom)
            header.append(p.toRandomBytes());
        else {
            // Length动态字段:
            //   dynamicParam==0 → 数据区长度
            //   dynamicParam==1 → 包头+数据区(整帧)
            //   分包模式(extraLen>0) → 上述基础值 + 本包帧字节数(本包包头+数据区)
            int len = dataArea.size();
            if (p.dynamicType == DynamicType::Length)
                len = dataArea.size() + headerSize;
            if (p.dynamicType == DynamicType::Length && extraLen > 0)
                len += extraLen;
            header.append(p.toBytes(seq, len, QByteArray(), -1, -1, -1, echoMap));
        }
    }

    QByteArray frame;
    frame.reserve(headerSize + dataSize + 16);
    frame.append(header);
    frame.append(dataArea);

    // 第二遍: 校验和
    // 预分配finalFrame: headerSize + dataSize, 避免append时realloc
    QByteArray finalFrame;
    finalFrame.reserve(headerSize + dataSize + 16);
    int hdrOffset = 0;
    for (const auto &p : *sendHeaders) {
        if (p.dynamicType == DynamicType::Checksum) {
            finalFrame += p.toBytes(seq, dataArea.size(), frame, -1, -1, -1, echoMap);
        } else {
            finalFrame += frame.mid(hdrOffset, p.byteSize());
        }
        hdrOffset += p.byteSize();
    }
    finalFrame += dataArea;

    return finalFrame;
}

QJsonObject ProtocolConfig::toJson() const
{
    QJsonObject o;
    o["name"] = name;
    o["description"] = description;

    QJsonArray hdr;
    for (const auto &p : headerParams) hdr.append(p.toJson());
    o["headerParams"] = hdr;

    QJsonArray dat;
    for (const auto &p : dataParams) dat.append(p.toJson());
    o["dataParams"] = dat;

    o["replyConfig"] = replyConfig.toJson();
    o["isActivePush"] = isActivePush;
    o["pushIntervalMs"] = pushIntervalMs;
    o["fixedFrameLength"] = fixedFrameLength;
    o["stopAllPeriodicOnMatch"] = stopAllPeriodicOnMatch;
    QJsonArray stopArr;
    for (const auto &n : stopPeriodicProtocolNames) stopArr.append(n);
    o["stopPeriodicProtocolNames"] = stopArr;
    return o;
}

void ProtocolConfig::fromJson(const QJsonObject &o)
{
    name = o["name"].toString();
    description = o["description"].toString();
    headerParams.clear();
    dataParams.clear();
    QJsonArray hdr = o["headerParams"].toArray();
    for (const auto &v : hdr) { ProtocolParam p; p.fromJson(v.toObject()); headerParams.append(p); }
    QJsonArray dat = o["dataParams"].toArray();
    for (const auto &v : dat) { ProtocolParam p; p.fromJson(v.toObject()); dataParams.append(p); }
    replyConfig.fromJson(o["replyConfig"].toObject());
    isActivePush = o["isActivePush"].toBool(false);
    pushIntervalMs = o["pushIntervalMs"].toInt(1000);
    fixedFrameLength = o["fixedFrameLength"].toInt(0);
    stopAllPeriodicOnMatch = o["stopAllPeriodicOnMatch"].toBool(false);
    stopPeriodicProtocolNames.clear();
    QJsonArray stopArr = o["stopPeriodicProtocolNames"].toArray();
    for (const auto &v : stopArr) stopPeriodicProtocolNames.append(v.toString());
    // 字段全改完: 刷一遍cached sizes给tryMatch/buildReplyFrame用
    recalcCachedSizes();
}

// ==================== SceneConfig ====================

QJsonObject SceneConfig::toJson() const
{
    QJsonObject o;
    o["name"] = name;
    o["tcpPort"] = tcpPort;
    QJsonArray arr;
    for (const auto &p : protocols) arr.append(p.toJson());
    o["protocols"] = arr;
    return o;
}

void SceneConfig::fromJson(const QJsonObject &o)
{
    name = o["name"].toString();
    tcpPort = o["tcpPort"].toInt(8080);
    protocols.clear();
    QJsonArray arr = o["protocols"].toArray();
    for (const auto &v : arr) {
        ProtocolConfig p;
        p.fromJson(v.toObject());
        protocols.append(p);
    }
}
