#include "protocoltypes.h"
#include <QDataStream>
#include <QRandomGenerator>
#include <cmath>

// ==================== ProtocolParam ====================

int ProtocolParam::byteSize() const
{
    switch (type) {
    case ParamType::UInt8:  case ParamType::Int8:   return 1;
    case ParamType::UInt16: case ParamType::Int16:  return 2;
    case ParamType::UInt32: case ParamType::Int32:  case ParamType::Float32: return 4;
    case ParamType::UInt64: case ParamType::Int64:  case ParamType::Float64: return 8;
    case ParamType::String:     return defaultValue.toLatin1().size();
    case ParamType::StringUtf8: return defaultValue.toUtf8().size();
    case ParamType::Bytes:      return defaultValue.size() / 2; // hex string
    case ParamType::Hex: {
        QString s = defaultValue;
        s.remove(' ');
        return s.size() / 2;
    }
    }
    return 0;
}

QByteArray ProtocolParam::toBytes(quint64 seq, int dataAreaLen, const QByteArray &fullFrame) const
{
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

    // 非动态，使用默认值
    switch (type) {
    case ParamType::UInt8: {
        bool ok;
        quint8 v = defaultValue.toUInt(&ok, 0);
        return QByteArray(1, (char)v);
    }
    case ParamType::Int8: {
        bool ok;
        qint8 v = defaultValue.toInt(&ok, 0);
        return QByteArray(1, (char)v);
    }
    case ParamType::UInt16: {
        bool ok;
        quint16 v = defaultValue.toUInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int16: {
        bool ok;
        qint16 v = defaultValue.toInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::UInt32: {
        bool ok;
        quint32 v = defaultValue.toUInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int32: {
        bool ok;
        qint32 v = defaultValue.toInt(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::UInt64: {
        bool ok;
        quint64 v = defaultValue.toULongLong(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Int64: {
        bool ok;
        qint64 v = defaultValue.toLongLong(&ok, 0);
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Float32: {
        float v = defaultValue.toFloat();
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::Float64: {
        double v = defaultValue.toDouble();
        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        ds << v;
        return data;
    }
    case ParamType::String:
        return defaultValue.toLatin1();
    case ParamType::StringUtf8:
        return defaultValue.toUtf8();
    case ParamType::Bytes:
    case ParamType::Hex: {
        QString s = defaultValue;
        s.remove(' ').remove('\n').remove('\t');
        return QByteArray::fromHex(s.toLatin1());
    }
    }
    return QByteArray();
}

QByteArray ProtocolParam::toRandomBytes() const
{
    if (!isRandom) return toBytes();

    if (type == ParamType::Bytes || type == ParamType::Hex) {
        int len = randomLength;
        if (len <= 0) len = byteSize();
        QByteArray data(len, 0);
        for (int i = 0; i < len; ++i)
            data[i] = (char)QRandomGenerator::global()->bounded(256);
        return data;
    }

    if (type == ParamType::Float32 || type == ParamType::Float64) {
        double minVal = randomMin.isEmpty() ? 0.0 : randomMin.toDouble();
        double maxVal = randomMax.isEmpty() ? 1.0 : randomMax.toDouble();
        double r = QRandomGenerator::global()->generateDouble();
        double val = minVal + r * (maxVal - minVal);

        QByteArray data;
        QDataStream ds(&data, QIODevice::WriteOnly);
        ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);
        if (type == ParamType::Float32) {
            ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
            ds << (float)val;
        } else {
            ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
            ds << val;
        }
        return data;
    }

    // 整数类型随机
    bool ok;
    qint64 minVal = randomMin.toLongLong(&ok, 0);
    if (!ok) minVal = 0;
    qint64 maxVal = randomMax.toLongLong(&ok, 0);
    if (!ok) maxVal = 65535;
    if (maxVal < minVal) std::swap(minVal, maxVal);

    quint64 range = (quint64)(maxVal - minVal);
    quint64 val = (quint64)minVal + (range > 0 ? QRandomGenerator::global()->bounded((quint32)(range + 1)) : 0);

    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds.setByteOrder(byteOrder == ByteOrder::BigEndian ? QDataStream::BigEndian : QDataStream::LittleEndian);

    int sz = byteSize();
    if (sz <= 1) ds << (quint8)val;
    else if (sz <= 2) ds << (quint16)val;
    else if (sz <= 4) ds << (quint32)val;
    else ds << (quint64)val;
    return data;
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
    o["isRandom"] = isRandom;
    o["randomMin"] = randomMin;
    o["randomMax"] = randomMax;
    o["randomLength"] = randomLength;
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
    isRandom = o["isRandom"].toBool(false);
    randomMin = o["randomMin"].toString();
    randomMax = o["randomMax"].toString();
    randomLength = o["randomLength"].toInt(8);
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
    }
    return "None";
}

DynamicType ProtocolParam::stringToDynamicType(const QString &s)
{
    if (s == "Timestamp") return DynamicType::Timestamp;
    if (s == "Length") return DynamicType::Length;
    if (s == "Checksum") return DynamicType::Checksum;
    if (s == "Sequence") return DynamicType::Sequence;
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
}

QString ReplyConfig::modeToString(ReplyMode m)
{
    switch (m) {
    case ReplyMode::None: return "None";
    case ReplyMode::Once: return "Once";
    case ReplyMode::Periodic1s: return "Periodic1s";
    case ReplyMode::Periodic5s: return "Periodic5s";
    case ReplyMode::PeriodicCustom: return "PeriodicCustom";
    }
    return "None";
}

ReplyMode ReplyConfig::stringToMode(const QString &s)
{
    if (s == "Once") return ReplyMode::Once;
    if (s == "Periodic1s") return ReplyMode::Periodic1s;
    if (s == "Periodic5s") return ReplyMode::Periodic5s;
    if (s == "PeriodicCustom") return ReplyMode::PeriodicCustom;
    return ReplyMode::None;
}

// ==================== ProtocolConfig ====================

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

    // 构建帧头(需要数据区长度)
    QByteArray header;
    for (const auto &p : headerParams)
        header += p.toBytes(seq, dataArea.size());

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

QByteArray ProtocolConfig::buildReplyFrame(quint64 seq) const
{
    // 构建回复数据区(支持随机值)
    QByteArray dataArea;
    for (const auto &p : replyConfig.dataParams) {
        if (p.isRandom)
            dataArea += p.toRandomBytes();
        else
            dataArea += p.toBytes(seq, 0);
    }

    // 构建回复帧头
    QByteArray header;
    for (const auto &p : replyConfig.headerParams) {
        if (p.isRandom)
            header += p.toRandomBytes();
        else
            header += p.toBytes(seq, dataArea.size());
    }

    QByteArray frame = header + dataArea;

    // 第二遍: 校验和
    QByteArray finalFrame;
    int hdrOffset = 0;
    for (const auto &p : replyConfig.headerParams) {
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
