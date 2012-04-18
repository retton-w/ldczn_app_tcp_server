/**
 * @file	TcpServer.h
 * @brief	TCP服务类声明
 * @author	hrh <huangrh@landuntec.com>
 * @version	1.0.0
 * @date	2011-12-07
 *
 * @verbatim
 * ============================================================================
 * Copyright (c) Shenzhen Landun technology Co.,Ltd. 2011
 * All rights reserved. 
 * 
 * Use of this software is controlled by the terms and conditions found in the
 * license agreenment under which this software has been supplied or provided.
 * ============================================================================
 * 
 * @endverbatim
 * 
 */


#ifndef _TCPSERVER_H_
#define _TCPSERVER_H_

#include "thread.h"
//#include "tcp_client.h"
#include "ldczn_protocol.h"

class TcpClient;
class Uart;
class Util;

class TcpServer: public Thread
{

public:
	int Process(char *buf, int len);
	TcpServer(TcpClient *client);
	//TcpServer();
	~TcpServer();

protected:
	void Run();


private:
	int  server_sock;	//服务器socket
	int  clnt_sock;		//客户端socket
	
	TcpClient *_tcp_client;	//相机客户端线程对象指针
	//Uart *_signal_module;

	void Init();
	void InitClient();

	int ParsePacket(char *buf, int len);
	int ProcessHeartBeat(struct payload_req *req, char *buf);
	int ProcessRequest(struct payload_req *req, char *buf);
	int ProcessAck(struct payload_ack *ack, char *buf);
	int ProcessHeader(struct header_std *head, int packet_len);
	int ProcessMannufacture(struct payload_req *req, char *buf);
	int ProcessUpgrade(struct payload_req *req, char *buf);
	int ProcessUpgradeApp(struct payload_req *req, char *buf);

	int ProcessSetParameter(struct payload_req *req, char *buf);
	//int ProcessSetCameraParameter(char *buf);
	int ProcessSetCameraParameter(struct payload_req *req, char *buf);
	int ProcessSetNetworkParam(char *buf);
	int ProcessSetUploadParam(char *buf);
	int ProcessSetDeviceInfo(char *buf);
	int ProcessSetFlashParam(char *buf);
	int ProcessCalibrateTime(struct payload_req *req, char *buf);

	int ProcessGetParameter(struct payload_req *req);
	int ProcessGetCameraParameter(struct payload_req *req);
	int ProcessGetDeviceInfo(struct payload_req *req);
	int ProcessGetNetworkParameter(struct payload_req *req);
	int ProcessGetUploadParam(struct payload_req *req);
	int ProcessGetTrafficParam(struct payload_req *req);
	int ProcessGetFlashParam(struct payload_req *req);

	int ProcessControl(struct payload_req *req);
	
	int ReturnAck(struct payload_req *req);
	int SendToClient(char *buf, int len);
};

#endif

