#include "udp_demo.h" 
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "lcd.h"
#include "malloc.h"
#include "stdio.h"
#include "string.h" 
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//UDP 测试代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/8/15
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved									  
//*******************************************************************************
//修改信息:
//2019.12.22 Zhejiang University 计算机网络与通信课程项目 UDP通信
////////////////////////////////////////////////////////////////////////////////// 	   
 
//UDP接收数据缓冲区
u8 udp_demo_recvbuf[UDP_DEMO_RX_BUFSIZE];

//UDP发送数据缓冲区
u8 udp_demo_sendbuf[UDP_DEMO_TX_BUFSIZE];
//UDP数据帧和应答帧相关定义
u8 FileBuff[100000]__attribute__((at(0x68040000)));	//外部RAM存放完整图片的缓存,200*240*2字节
u32 FileNum = 0;									//图片分片标志
u16 FrameByteNum = 0;								//标示当前接收到数据帧的哪个字节
//数据帧和应答帧相关定义
const u8 FrameFlag = 0x7E;				//帧头标识符，固定
const u8 SrcAdd = 0xff, DestAdd = 0xff;	//目标地址和原地址
u8 Command;							//命令
u8 LastFrameNum = 0;				//上一个接收到的正确的帧序号
u8 FrameNumber = 0;					//帧序号
u16 DataLength;						//数据长度
//u8 FrameData[1010];				//当前帧数据缓存,这里换成udp_demo_recvbuf
u16 CheckSum = 0;
u8 CheckSumH = 0, CheckSumL = 0;	//数据帧求和校验位,高低两个字节
u16 ACKCheckSum = 0;
u8 ACKCheckSumH = 0, ACKCheckSumL = 0;	//应答帧求和校验位,高低两个字节

//UDP 测试全局状态标记变量
//bit7:没有用到
//bit6:0,没有收到数据;1,收到数据了.
//bit5:0,没有连接上;1,连接上了.
//bit4~0:保留
u8 udp_demo_flag;

void LCD_display(void);

//设置远端IP地址
void udp_demo_set_remoteip(void)
{
	u8 *tbuf;
	//u16 xoff;
	//u8 key;
	LCD_Clear(WHITE);
	POINT_COLOR=RED;
	LCD_ShowString(30,30,200,16,16,"Explorer STM32F4");
	LCD_ShowString(30,50,200,16,16,"UDP Test");
	LCD_ShowString(30,70,200,16,16,"Remote IP Set");  
	// LCD_ShowString(30,90,200,16,16,"KEY0:+  KEY2:-");  
	// LCD_ShowString(30,110,200,16,16,"KEY_UP:OK");  

	tbuf=mymalloc(SRAMIN,100);	//申请内存
	if(tbuf==NULL)	return;

	//前三个IP保持和DHCP得到的IP一致
	//远端IP地址192.168.1.115
	lwipdev.remoteip[0]= lwipdev.ip[0];
	lwipdev.remoteip[1]= lwipdev.ip[1];
	lwipdev.remoteip[2]= lwipdev.ip[2]; 
	lwipdev.remoteip[3]= 115;

	//显示远端IP地址
	sprintf((char*)tbuf,"Remote IP:%d.%d.%d.%d",lwipdev.remoteip[0],\
			lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3]);
	LCD_ShowString(30,150,210,16,16,tbuf); 
	myfree(SRAMIN,tbuf); 
} 

//UDP测试
void udp_demo_test(void)
{
 	err_t err;
	struct udp_pcb *udppcb;  	//定义一个UDP协议控制块
	struct ip_addr rmtipaddr;  	//远端ip地址
 	
	u8 *tbuf;					//LCD显示缓存指针
 	u8 key;
	u8 res=0;		
	u8 t=0; 
	u32 i; 						//FileBuff按字节存储的循环标志
	u16 j;						//计算校验位CheckSum时用到的循环标志

	udp_demo_set_remoteip();	//设置远端(计算机)IP地址
	
	//LCD UI界面
	LCD_Clear(WHITE);	//清屏
	POINT_COLOR=RED; 	//红色字体
	LCD_ShowString(30,30,200,16,16,"Explorer STM32F4");
	LCD_ShowString(30,50,200,16,16,"UDP Test");
	LCD_ShowString(30,70,200,16,16,"ATOM@ALIENTEK");  
	LCD_ShowString(30,90,200,16,16,"KEY0:Send data");  
	LCD_ShowString(30,110,200,16,16,"KEY_UP:Quit");  
	tbuf=mymalloc(SRAMIN,200);	//申请内存
	if(tbuf==NULL)return;		//内存申请失败了,直接退出
	sprintf((char*)tbuf,"Local IP:%d.%d.%d.%d",lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);//服务器IP
	LCD_ShowString(30,130,210,16,16,tbuf);  
	sprintf((char*)tbuf,"Remote IP:%d.%d.%d.%d",lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3]);//远端IP
	LCD_ShowString(30,150,210,16,16,tbuf);  
	sprintf((char*)tbuf,"Remote Port:%d",UDP_DEMO_PORT);//客户端端口号
	LCD_ShowString(30,170,210,16,16,tbuf);
	POINT_COLOR=BLUE;
	LCD_ShowString(30,190,210,16,16,"STATUS:Disconnected"); 


	udppcb=udp_new();

	if(udppcb)//创建成功
	{
		//此时res=0
		IP4_ADDR(&rmtipaddr,lwipdev.remoteip[0],lwipdev.remoteip[1],lwipdev.remoteip[2],lwipdev.remoteip[3]);
		err=udp_connect(udppcb,&rmtipaddr,UDP_DEMO_PORT);//UDP客户端连接到指定IP地址和端口号的服务器
		if(err==ERR_OK)
		{
			err=udp_bind(udppcb,IP_ADDR_ANY,UDP_DEMO_PORT);//绑定本地IP地址与端口号
			if(err==ERR_OK)	//绑定完成
			{
				udp_recv(udppcb,udp_demo_recv,NULL);//注册接收回调函数 
				LCD_ShowString(30,190,210,16,16,"STATUS:Connected   ");//标记连接上了(UDP是非可靠连接,这里仅仅表示本地UDP已经准备好)
				POINT_COLOR=RED;
				LCD_ShowString(30,210,lcddev.width-30,lcddev.height-190,16,"Receive Data:");//提示消息		
				POINT_COLOR=BLUE;//蓝色字体
			}
			else 
				res=1;
		}
		else 
			res=1;		
	}
	else 
		res=1;


	while(res==0)
	{
		if(udp_demo_flag&1<<6)//是否收到数据?
		{
			//LCD_Fill(30,230,lcddev.width-1,lcddev.height-1,WHITE);//清上一次数据
			//LCD_ShowString(30,230,lcddev.width-30,lcddev.height-230,16,udp_demo_recvbuf);//显示接收到的数据			
			
			//已经接收到了一个完整的UDP数据帧,抓取帧头信息和数据,并进行校验,同时回送校验帧
			FrameByteNum = 0;
			Command = udp_demo_recvbuf[3];
			FrameNumber = udp_demo_recvbuf[4];
			DataLength = (((u16)udp_demo_recvbuf[5])<<8) + (u16)udp_demo_recvbuf[6];

			for (j = 0; j <= DataLength + 6; j++)
			{
				CheckSum += udp_demo_recvbuf[j];
			}
			CheckSumL = (u8)CheckSum;
			CheckSumH = (u8)(CheckSum >> 8);
		
			//判断求和校验和帧序号是否正确
			if((udp_demo_recvbuf[DataLength + 7] == CheckSumH) && (udp_demo_recvbuf[DataLength + 8] == CheckSumL) && \
		  	((FrameNumber == LastFrameNum)||(FrameNumber == LastFrameNum + 1)))  
			{
				//将数据存到FileBuff中
				for(i = FileNum; i < FileNum + DataLength; i++)
				{
					FileBuff[i] = udp_demo_recvbuf[i-FileNum+7];
				}
				FileNum += DataLength;
				LastFrameNum++;

				//应答帧校验
				ACKCheckSum = FrameFlag + SrcAdd + DestAdd + Command + FrameNumber;
				ACKCheckSumL = (u8)ACKCheckSum;
				ACKCheckSumH = (u8)(ACKCheckSum >> 8);
				//发送应答帧
				udp_demo_sendbuf[0] = FrameFlag;
				udp_demo_sendbuf[1] = SrcAdd;
				udp_demo_sendbuf[2] = DestAdd;
				udp_demo_sendbuf[3] = Command;
				udp_demo_sendbuf[4] = FrameNumber;
				udp_demo_sendbuf[5] = 0;
				udp_demo_sendbuf[6] = 0;
				udp_demo_sendbuf[7] = ACKCheckSumH;
				udp_demo_sendbuf[8] = ACKCheckSumL;

				udp_demo_senddata(udppcb);
			}

			//清除校验位、长度标示位
			CheckSumH = 0;
			CheckSumL = 0;
			CheckSum = 0;
			DataLength = 0;

			udp_demo_flag&=~(1<<6);//标记数据已经被处理了
			
		} 
		lwip_periodic_handle();
		delay_ms(2);
		t++;
		if(t==200)
		{
			t=0;
			LED0=!LED0;				//LED0标识UDP进程正在运行
		}
		if(Command == 0X02)
			break;
	}

	if(Command == 0X02)
	{
		LCD_display();
		Command = 0;
	}

	udp_demo_connection_close(udppcb); 
	myfree(SRAMIN,tbuf);
} 


//UDP服务器回调函数
void udp_demo_recv(void *arg,struct udp_pcb *upcb,struct pbuf *p,struct ip_addr *addr,u16_t port)
{
	u32 data_len = 0;
	struct pbuf *q;
	if(p != NULL)	//接收到不为空的数据时
	{
		memset(udp_demo_recvbuf,0,UDP_DEMO_RX_BUFSIZE);  //数据接收缓冲区清零
		for(q=p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
		{
			//判断要拷贝到UDP_DEMO_RX_BUFSIZE中的数据是否大于UDP_DEMO_RX_BUFSIZE的剩余空间，如果大于
			//的话就只拷贝UDP_DEMO_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
			if(q->len > (UDP_DEMO_RX_BUFSIZE-data_len)) memcpy(udp_demo_recvbuf+data_len,q->payload,(UDP_DEMO_RX_BUFSIZE-data_len));//拷贝数据
			else memcpy(udp_demo_recvbuf+data_len,q->payload,q->len);
			data_len += q->len;  	
			if(data_len > UDP_DEMO_RX_BUFSIZE) break; //超出UDP客户端接收数组,跳出	
		}
		upcb->remote_ip=*addr; 				//记录远程主机的IP地址
		upcb->remote_port=port;  			//记录远程主机的端口号
		lwipdev.remoteip[0]=upcb->remote_ip.addr&0xff; 		//IADDR4
		lwipdev.remoteip[1]=(upcb->remote_ip.addr>>8)&0xff; //IADDR3
		lwipdev.remoteip[2]=(upcb->remote_ip.addr>>16)&0xff;//IADDR2
		lwipdev.remoteip[3]=(upcb->remote_ip.addr>>24)&0xff;//IADDR1 
		udp_demo_flag|=1<<6;				//标记接收到数据了
		pbuf_free(p);//释放内存
	}
	else
	{
		udp_disconnect(upcb); 
	} 
} 
//UDP服务器发送数据,该函数用于发送应答帧
void udp_demo_senddata(struct udp_pcb *upcb)
{
	struct pbuf *ptr;
	ptr=pbuf_alloc(PBUF_TRANSPORT, 9, PBUF_POOL); //申请内存
	if(ptr)
	{
		ptr->payload=(void*)udp_demo_sendbuf; 	//发送的数据在这里
		udp_send(upcb,ptr);	//udp发送数据 
		pbuf_free(ptr);//释放内存
	} 
} 
//关闭UDP连接
void udp_demo_connection_close(struct udp_pcb *upcb)
{
	udp_disconnect(upcb); 
	udp_remove(upcb);		//断开UDP连接 
}

//LCD显示
void LCD_display(void)
{
	//i:行标
	//j:列标
	u8 i, j;
	for(j = 0; j < 240; j++)
		for(i = 0; i < 200; i++)
		{
			POINT_COLOR = (((u16)FileBuff[j*400+(i+64)*2+1]) << 8) + ((u16)FileBuff[j*400+(i+64)*2]);
			LCD_DrawPoint(j, i); 
		}
}
























