#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QVector>
#include "protocoltypes.h"

// 配置管理器: 管理多个场景配置文件
class ConfigManager
{
public:
    ConfigManager();

    // 场景管理
    bool loadScenes(const QString &dir);
    bool saveScenes(const QString &dir) const;
    bool loadScene(const QString &filepath);
    bool saveScene(const QString &filepath, const SceneConfig &scene) const;

    // 内存中的场景列表
    QVector<SceneConfig> &scenes();
//    const QVector<SceneConfig> &scenes() const;

    void addScene(const SceneConfig &scene);
    void removeScene(int index);
    SceneConfig *getScene(int index);
    SceneConfig *getSceneByName(const QString &name);

    QString configDir() const;
    void setConfigDir(const QString &dir);

private:
    QVector<SceneConfig> m_scenes;
    QString m_configDir;
};

#endif // CONFIGMANAGER_H
