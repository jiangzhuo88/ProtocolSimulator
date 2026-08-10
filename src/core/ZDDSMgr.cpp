#include "ZDDSMgr.h"
//ZDDSInterface* ZDDSMgr::m_zddsInterface = nullptr;//类外初始化静态成员变量，否则报错未定义引用

ZDDSMgr* ZDDSMgr::m_instance = nullptr;
ZDDSMgr::ZDDSMgr(QObject *parent):
    QObject(parent)
  ,m_zddsinterface(nullptr)
{
//    if(nullptr ==  m_zddsInterface )
//    {
        //29订阅接口
        m_zddsinterface = getInterfaceInstance();
        m_zddsinterface->regRecvCallbackFunc(GlobalOnrecvDataFromZDDS);
        m_zddsinterface->startZDDS();
//    }
}

ZDDSInterface* ZDDSMgr::getInterfaceInstance()//第一次调用接口实例时就会调用管理类对象
{
    if (nullptr == m_zddsinterface){
        m_zddsinterface = ZDDSInterface::createZDDSInterface();
    }
    return m_zddsinterface;
}

void ZDDSMgr::registerStateCtrl(const char *cDomainName, const char *cTopicName, func callback)
{
    m_zddsinterface->subMessage(cDomainName,cTopicName);
    m_map[cDomainName][cTopicName].push_back(callback);
}

ZDDSMgr* ZDDSMgr::getInstance()
{
    if (m_instance ==nullptr){
        m_instance = new ZDDSMgr();
    }
    return m_instance;
}

void ZDDSMgr::initZDDS()
{

}

void ZDDSMgr::GlobalOnrecvDataFromZDDS(const char* cDomainName,const char* cTopicName,const char* data,size_t nDataLen,void *pContext)
{
    ZDDSMgr* pInstance = ZDDSMgr::getInstance();
    if (pInstance == nullptr || data == nullptr) return;
    std::list<func> fList = pInstance->m_map.at(cDomainName).at(cTopicName);
    for(func f : fList)
    {
        f(data,nDataLen);
    }

}

