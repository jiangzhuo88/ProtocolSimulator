#ifndef ZDDSMGR_H
#define ZDDSMGR_H

#include <QObject>
#include <functional>
#include <stddef.h>
#include "zdds.h"
class FormTransmitConotrl;//前向声明有这个类，防止循环调用头文件
class ZDDSMgr : public QObject
{
    Q_OBJECT
public:
    void initZDDS();
    static ZDDSMgr* getInstance();
    ZDDSInterface* getInterfaceInstance();
    typedef std::function<void(const char*,size_t)> func;
    void registerStateCtrl(const char* cDomainName, const char* cTopicName, func callback);
    //静态回调
    static void GlobalOnrecvDataFromZDDS(const char* cDomainName,const char* cTopicName,const char* data,size_t nDataLen,void *pContext);
private:
    explicit ZDDSMgr(QObject *parent = nullptr);
    static ZDDSMgr* m_instance;
    ZDDSInterface* m_zddsinterface = nullptr;

    std::map<std::string,std::map<std::string,std::list<func>>> m_map;
};

#endif // ZDDSMGR_H
