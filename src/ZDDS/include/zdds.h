#pragma once
//Interface Version V1.2.0
#include <stddef.h>   //size_t define
#ifdef WIN32
	#ifdef ZDDS_LIB
		#define ZDDS_EXPORT __declspec(dllexport)
	#else
		#define ZDDS_EXPORT __declspec(dllimport)
	#endif
#else
	#ifdef ZDDS_LIB
		#define ZDDS_EXPORT
	#else
		#define ZDDS_EXPORT
	#endif
#endif

#ifndef NET_DISCOVER_TYPE_DEFINE
#define NET_DISCOVER_TYPE_DEFINE
enum NET_DISCOVER_TYPE
{
    ND_TYPE_MULTICAST = 1,	//ZDDS自行通过UDP组播进行网络发现
    ND_TYPE_CENTRALNODE,	//ZDDS通过与中心结点(CentralNode)连接进行网络发现
    ND_TYPE_HOSTPROXY		//ZDDS通过本机代理(ZDDSProxy)进行网络发现, 自身不实现网络发现功能
};
#endif

//接收数据回调函数
typedef void(* pfn_RecvDataCallbackFunc)(const char* cDomainName, const char* cTopicName, \
                                         const char* data, size_t nDatalen, void *pContext);

//ZDDS启动成功通知回调函数 add by yhh 20220727
typedef void(* pfn_StartSuccessCallbackFunc)(void *pUserContext);

class ZDDS_EXPORT ZDDSInterface
{

public:
    ZDDSInterface() {}
    virtual ~ZDDSInterface() {}

public:
    //创建ZDDS接口
    static ZDDSInterface* createZDDSInterface();
    //释放ZDDS接口
    static void releaseZDDSInterface(ZDDSInterface* pZddsInterface);

    //注册接收数据回调函数 modify by yhh 20220727 增加pZContext上下文
    virtual void regRecvCallbackFunc(pfn_RecvDataCallbackFunc ptrRecvCallbackFunc, void *pZContext=NULL) = 0;

    //注册ZDDS启动成功通知回调函数 add by yhh 20220727
    virtual void regZDDSStartSuccessNotify(pfn_StartSuccessCallbackFunc ptrStartSuccessNotify, void *pUserContext=NULL) = 0;

    //订阅消息
    //cDomainName: 订阅的域名称
    //cTopicName:  订阅的主题名称
    //返回值:void
    virtual void subMessage(const char* cDomainName, const char* cTopicName) = 0;

    //取消订阅消息
    //cDomainName: 取消订阅的域名称
    //cTopicName:  取消订阅的主题名称
    //返回值:void
    virtual void unSubMessage(const char* cDomainName, const char* cTopicName) = 0;

    //异步发送消息
    //cDomainName: 域名称
    //cTopicName:  主题名称
    //pData:       发送数据指针
    //nDatalen:    发送数据长度
    //返回值:void
    virtual void asySendMessage(const char* cDomainName, const char* cTopicName, \
                                const char* pData, size_t nDatalen) = 0;

    //启动ZDDS服务
    //返回值:bool, true:成功，false:失败
    virtual bool startZDDS() = 0;
};
