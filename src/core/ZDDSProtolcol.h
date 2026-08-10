#ifndef ZDDSPROTOLCOL_H
#define ZDDSPROTOLCOL_H

#include <QObject>
const uint32_t PLAT_TOPIC_SIMULATION_CTRL = 0x00C00022;     //猎人课题增加 模拟场景切换控制  //注意该消息和接口54所、30所要接收，不能任意调整
//主题域结构定义------------------------------
typedef struct _PLT_DMHEAD
{
    uint32_t		unMsgCode;				//数据指令编码	unMsgCode		无符号4字节整数	系统统一规划定义。
    uint32_t		unMsgLen;				//数据包总长度	unMsgLen		无符号4字节整数	报文头和数据内容的总长度，取值范围[20, 8192]。
    uint32_t		unSrcID;				//源地址		unSrcID			无符号4字节整数	报文发送方唯一标识ID。软件地址编码=计算机ID*65536+软件配置项ID*256+软件编号
        uint32_t		unDestID;				//目的地址	unDestID		无符号4字节整数	软件地址编码=计算机ID*65536+软件配置项ID*256+软件编号
                                                                                                                //1、0：即计算机ID、软件配置项ID、软件编号均为0无特定接收方
                                                                                                                //2、计算机ID不等于0，软件配置项ID、软件编号均为0：特定席位上软件接收
                                                                                                                //3、计算机ID不等于0，软件配置项ID不等于0，软件编号为0：特定席位上特定配置项软件接收；
                                                                                                                //4、计算机ID不等于0、软件配置项ID不等于0、软件编号不等于0：特定席位上特定配置项的特定软件进程接收；

    uint32_t		unMsgOrder;				//数据流水编号	unMsgOrder		无符号4字节整数	发起方对发送数据的流水编号。

    _PLT_DMHEAD()
    {
                memset(this, 0,sizeof(*this));
    }
}stDMHead;
const uint16_t PLAT_DMHEAD_LEN	= sizeof(stDMHead);
typedef struct _BOM_SCENE_STATUS_CTRL_INFO
{
    stDMHead    DMHead;
    char        cSceneID[40];       //场景名称
    uint32_t    unSceneID;          //场景ID 暂定取值范围1~3
    uint8_t     ucSceneRunMode;     //运行控制 0-停止 1-启动
    uint8_t     cBackup[2048];      //备份

    _BOM_SCENE_STATUS_CTRL_INFO()
    {
        memset(this, 0, sizeof(*this));
        DMHead.unMsgCode = PLAT_TOPIC_SIMULATION_CTRL;
        DMHead.unMsgLen = sizeof(*this);
    }
}stSceneStatusCtrlInfo;
#endif // ZDDSPROTOLCOL_H
