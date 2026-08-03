#include "configmanager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>

ConfigManager::ConfigManager()
{
    m_configDir = "scenes";
}

// 场景目录结构:
// scenes/
//   场景名/
//     scene.json       -- 场景元数据 (名称, TCP端口)
//     协议A.json       -- 协议配置
//     协议B.json

bool ConfigManager::loadScenes(const QString &dir)
{
    m_configDir = dir;
    m_scenes.clear();

    QDir rootDir(dir);
    if (!rootDir.exists()) return false;

    // 遍历子目录，每个子目录是一个场景
    QFileInfoList entries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        if (!entry.isDir()) continue;

        QString sceneDir = entry.absoluteFilePath();
        QString sceneJsonPath = sceneDir + "/scene.json";

        // 读取场景元数据
        SceneConfig scene;
        if (QFile::exists(sceneJsonPath)) {
            QFile f(sceneJsonPath);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                f.close();
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    scene.name = obj["name"].toString();
                    scene.tcpPort = obj["tcpPort"].toInt(8080);
                }
            }
        }
        if (scene.name.isEmpty())
            scene.name = entry.fileName();

        // 加载该场景下的所有协议文件
        QDir sDir(sceneDir);
        QStringList filters;
        filters << "*.json";
        QFileInfoList protoFiles = sDir.entryInfoList(filters, QDir::Files);
        for (const auto &pf : protoFiles) {
            if (pf.fileName() == "scene.json") continue;
            QFile f(pf.absoluteFilePath());
            if (!f.open(QIODevice::ReadOnly)) continue;
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            if (doc.isObject()) {
                ProtocolConfig proto;
                proto.fromJson(doc.object());
                if (!proto.name.isEmpty())
                    scene.protocols.append(proto);
            }
        }

        m_scenes.append(scene);
    }

    return !m_scenes.isEmpty();
}

bool ConfigManager::saveScenes(const QString &dir) const
{
    QDir rootDir(dir);
    if (!rootDir.exists()) rootDir.mkpath(".");

    for (const auto &scene : m_scenes) {
        QString scenePath = rootDir.absoluteFilePath(scene.name);
        QDir sDir(scenePath);
        if (!sDir.exists()) sDir.mkpath(".");

        // 保存场景元数据
        QJsonObject meta;
        meta["name"] = scene.name;
        meta["tcpPort"] = scene.tcpPort;
        QFile sf(scenePath + "/scene.json");
        if (sf.open(QIODevice::WriteOnly)) {
            sf.write(QJsonDocument(meta).toJson());
            sf.close();
        }

        // 清理旧的协议文件(保留scene.json)
        QStringList filters;
        filters << "*.json";
        QFileInfoList oldFiles = sDir.entryInfoList(filters, QDir::Files);
        for (const auto &of : oldFiles) {
            if (of.fileName() == "scene.json") continue;
            QFile::remove(of.absoluteFilePath());
        }

        // 保存每个协议为独立文件
        for (const auto &proto : scene.protocols) {
            QString protoFile = scenePath + "/" + proto.name + ".json";
            QFile f(protoFile);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(QJsonDocument(proto.toJson()).toJson());
                f.close();
            }
        }
    }
    return true;
}

bool ConfigManager::loadScene(const QString &filepath)
{
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;

    // 兼容旧格式: 如果包含protocols数组则是旧格式单文件场景
    QJsonObject obj = doc.object();
    if (obj.contains("protocols")) {
        SceneConfig scene;
        scene.fromJson(obj);
        m_scenes.append(scene);
        return true;
    }
    return false;
}

bool ConfigManager::saveScene(const QString &filepath, const SceneConfig &scene) const
{
    QFile f(filepath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(scene.toJson());
    f.write(doc.toJson());
    f.close();
    return true;
}

QVector<SceneConfig> &ConfigManager::scenes() { return m_scenes; }
const QVector<SceneConfig> &ConfigManager::scenes() const { return m_scenes; }

void ConfigManager::addScene(const SceneConfig &scene) { m_scenes.append(scene); }

void ConfigManager::removeScene(int index)
{
    if (index >= 0 && index < m_scenes.size())
        m_scenes.removeAt(index);
}

SceneConfig *ConfigManager::getScene(int index)
{
    if (index >= 0 && index < m_scenes.size())
        return &m_scenes[index];
    return nullptr;
}

SceneConfig *ConfigManager::getSceneByName(const QString &name)
{
    for (auto &s : m_scenes)
        if (s.name == name) return &s;
    return nullptr;
}

QString ConfigManager::configDir() const { return m_configDir; }
void ConfigManager::setConfigDir(const QString &dir) { m_configDir = dir; }
