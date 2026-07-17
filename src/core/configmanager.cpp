#include "configmanager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>

ConfigManager::ConfigManager()
{
    m_configDir = "scenes";
}

bool ConfigManager::loadScenes(const QString &dir)
{
    m_configDir = dir;
    m_scenes.clear();
    QDir d(dir);
    if (!d.exists()) return false;
    QStringList filters;
    filters << "*.json";
    d.setNameFilters(filters);
    QFileInfoList files = d.entryInfoList(QDir::Files);
    for (const auto &fi : files) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (doc.isObject()) {
            SceneConfig scene;
            scene.fromJson(doc.object());
            m_scenes.append(scene);
        }
    }
    return !m_scenes.isEmpty();
}

bool ConfigManager::saveScenes(const QString &dir) const
{
    QDir d(dir);
    if (!d.exists()) d.mkpath(".");
    for (const auto &scene : m_scenes) {
        QString filename = d.absoluteFilePath(scene.name + ".json");
        if (!saveScene(filename, scene)) return false;
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
    SceneConfig scene;
    scene.fromJson(doc.object());
    m_scenes.append(scene);
    return true;
}

bool ConfigManager::saveScene(const QString &filepath, const SceneConfig &scene) const
{
    QFile f(filepath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(scene.toJson());
    f.write(doc.toJson(QJsonDocument::Indented));
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
