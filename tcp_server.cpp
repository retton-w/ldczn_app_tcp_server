/**
 * @file	TcpServer.cpp
 * @brief	Tcp服务端类实现
 * @author	hrh <huangrh@landuntec.com>
 * @version 	1.0.0
 * @date 	2011-12-07
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


#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "socket.h"
#include "tcp_client.h"
#include "tcp_server.h"
#include "sensor.h"
#include "gpio.h"
#include "debug.h"
#include "parameters.h"
#include "uart.h"
#include "util.h" 
#include "peripherral_manage.h"


#define RECV_BUF_LENGTH 1024

static inline void fill_std_header(struct header_std *head,
                                   char encoding,
                                   char *auth_code,
                                   char msg_type,
                                   unsigned int msg_size)
{
        const char magic[] = PROTOCOL_MAGIC;
        memcpy(head->magic, magic, sizeof(magic));
        head->protocol_major = PROTOCOL_MAJOR;
        head->protocol_minor = PROTOCOL_MINOR;
        head->encoding       = encoding;
        memcpy(head->auth_code, auth_code, sizeof(head->auth_code));
        head->msg_type       = msg_type;
        head->msg_size       = msg_size;
}

TcpServer::TcpServer(TcpClient *client)
{
	server_sock = -1;
	clnt_sock = -1;

	_tcp_client = client;
	InitClient();
}


TcpServer::~TcpServer()
{
	server_sock = -1;
	clnt_sock = -1;

	_tcp_client = NULL;
	Debug("deconstruct");
}


void TcpServer::InitClient()
{
	UploadParam param = Parameters::GetInstance()->GetUploadParam();
	struct _ClientInfo client_info; 
	memcpy(client_info.addr, param.upload_server, sizeof(client_info.addr));
	_tcp_client->SetClient(&client_info);
}

/**
 * @function	void Init()
 * @brief	初始化tcp server，并监听39002端口
 *
 */
void TcpServer::Init()
{
	server_sock = Socket::CreateTcp();
	if (server_sock < 0) {
		Debug("Create Nonblock Tcp failed");
		return;
	}

	Socket::SetNonblock(server_sock);
	int ret = Socket::Bind(server_sock, 39002);
	if (ret < 0) {
		Debug("Bind port 39002 failed");
		Socket::Close(server_sock);
		return;
	}

	ret = Socket::Listen(server_sock, 5);
	if (ret < 0) {
		Debug("Listen port failed");
		Socket::Close(server_sock);
		return;
	}
}

void TcpServer::Run()
{
	Init();

	char buf[RECV_BUF_LENGTH];
	while (!IsTerminated()) {
		clnt_sock = Socket::Accept(server_sock, 10000);
		if (clnt_sock < 0) {
			usleep(100000);	
			continue;
		}

		int ret = Socket::Read(clnt_sock, buf, RECV_BUF_LENGTH, 5000); 
		if (ret <= 0) {
			Debug("remote is not alive");
		} else if ((unsigned int)ret < sizeof(PacketRequest)) {
			Debug("got %d packet, buf too less packet", ret);
		} else {
			ParsePacket(buf, ret);
		}
		Socket::Close(clnt_sock);
		clnt_sock = -1;
		usleep(100000);	
	}
	
	if (clnt_sock >= 0) {
		Socket::Close(clnt_sock);
	}
	Socket::Close(server_sock);
}


static inline int get_auth_code(char *auth_code, int len)
{
	auth_code = auth_code;
	len = len;
	return 0;
}

static inline int match_auth_code(const char *auth_code, int len)
{
	auth_code = auth_code;
	len = len;
	return 0;
}


int TcpServer::SendToClient(char *buf, int len)
{
	Debug();
	return Socket::Writen(clnt_sock, buf, len, 2000);
}


int TcpServer::ParsePacket(char *buf, int len)
{
	if ((unsigned int)len < sizeof(struct header_std)) {
		return -1;
	}
	
	struct header_std *head = (struct header_std *)buf;
	if (ProcessHeader(head, len)) {
		return -1;
	}

	switch (head->msg_type) {
	case MESSAGE_TYPE_REQ:
		ProcessRequest((struct payload_req *)(buf + sizeof(*head)),
				buf + sizeof(*head) + sizeof(struct payload_req));
		break;
	case MESSAGE_TYPE_ACK:
		ProcessAck((struct payload_ack *)(buf + sizeof(*head)),
				buf + sizeof(*head) + sizeof(struct payload_ack));
		break;
	default:
		break;
	}

	return len - head->msg_size;
}

int TcpServer::ProcessHeader(struct header_std *head, int packet_len)
{
	const char magic[sizeof(head->magic)] = PROTOCOL_MAGIC;
	Debug();

	if (memcmp(head->magic, magic, sizeof(magic)) != 0) {
		return -1;
	}

	if ((head->protocol_major != PROTOCOL_MAJOR) ||
		(head->protocol_minor != PROTOCOL_MINOR)) {
		return -1;
	}

	if (head->encoding != ENCODING_TYPE_RAW) {
		return -1;
	}

	if (match_auth_code(head->auth_code, sizeof(head->auth_code)) < 0) {
		return -1;
	}

	if (head->msg_size > (unsigned int)packet_len) {
		return -1;
	}

	return 0;
}

int TcpServer::ProcessRequest(struct payload_req *req, char *buf)
{
	Debug();
	switch (req->type & 0xFF000000) {

	case REQ_TYPE_HEARTBEAT:
		return ProcessHeartBeat(req, buf);
		break;
	case REQ_TYPE_MANUFACTURE:
		return ProcessMannufacture(req, buf);
		break;
	case REQ_TYPE_CONTROL:
		return ProcessControl(req);
		break;
	case REQ_TYPE_SET_PARAMETER:
		return ProcessSetParameter(req, buf);
		break;
	case REQ_TYPE_GET_PARAMETER:
		return ProcessGetParameter(req);
		break;
	default:
		break;
	}

	return -1;
}

int TcpServer::ProcessAck(struct payload_ack *ack, char *buf)
{
	ack = ack;
	buf = buf;
	return 0;
}

int TcpServer::ProcessHeartBeat(struct payload_req *req, char *buf)
{
	Debug();
	req = req;
	buf = buf;
	return 0;
}


int TcpServer::ProcessControl(struct payload_req *req)
{
	Debug();
	switch (req->type & 0x00FF0000) {
	case CTL_TYPE_VIDEO:
		Sensor::GetInstance()->SetSensorVideo();
		PeripherralManage::DisableRecv();
		break;
	case CTL_TYPE_CAPTURE:
		Sensor::GetInstance()->SetSensorCapture();
		PeripherralManage::EnableRecv();
		break;
	case CTL_TYPE_MANNUAL_SNAP:
		GpioCtl::MannualSnap();
		break;
	case CTL_TYPE_REBOOT:
		Util::Reboot();
		break;
	default:
		break;
	}
	
	ReturnAck(req);
	return 0;
}


int TcpServer::ProcessSetParameter(struct payload_req *req, char *buf)
{
	Debug();
	switch (req->type & REQ_TYPE_SUB_MASK) {
	case PARAM_TYPE_CAMERA:
		ProcessSetCameraParameter(req, buf);
		break;
	case PARAM_TYPE_NETWORK:
		ProcessSetNetworkParam(buf);
		break;
	case PARAM_TYPE_UPLOAD:
		ProcessSetUploadParam(buf);
		break;
	case PARAM_TYPE_TIME://添加校时模块
		ProcessCalibrateTime(req, buf);
		break;
	case PARAM_TYPE_FLASH:
		ProcessSetFlashParam(buf);
		break;
	case PARAM_TYPE_PLATE:
		break;
	case PARAM_TYPE_VIDEO_DETECT:
		break;
	case PARAM_TYPE_MANNUFACTURE:
		break;
	case PARAM_TYPE_DEVICE_INFO:
		ProcessSetDeviceInfo(buf);
		break;
	default :
		break;
        }
	
	ReturnAck(req);
        return 0;
}

int TcpServer::ProcessSetCameraParameter(struct payload_req *req, char *buf)
{
	Debug();
	CameraParam *setting = (CameraParam *)buf;
	Parameters::GetInstance()->SetCameraParam(setting);

	switch (req->type & REQ_TYPE_CMD_MASK) {
	case PARAM_CAMERA_DEFAULT_GAIN:
		Sensor::GetInstance()->SetSensorGain(setting->default_gain);
		break;
	case PARAM_CAMERA_MIN_GAIN:
		Sensor::GetInstance()->SetSensorMinGain(setting->min_gain);
		break;
	case PARAM_CAMERA_MAX_GAIN:
		Sensor::GetInstance()->SetSensorMaxGain(setting->max_gain);
		break;
	case PARAM_CAMERA_DEFAULT_EXPOSURE:
		Sensor::GetInstance()->SetSensorExposure(setting->default_exposure);
		break;
	case PARAM_CAMERA_MIN_EXPOSURE:
		Sensor::GetInstance()->SetSensorMinExp(setting->min_exposure);
		break;
	case PARAM_CAMERA_MAX_EXPOSURE:
		Sensor::GetInstance()->SetSensorMaxExp(setting->max_exposure);
		break;
	case PARAM_CAMERA_RED_GAIN:
		Sensor::GetInstance()->SetSensorRgain(setting->red_gain);
		break;
	case PARAM_CAMERA_BLUE_GAIN:
		Sensor::GetInstance()->SetSensorBgain(setting->blue_gain);
		break;
	case PARAM_CAMERA_VIDEO_TARGET:
		Sensor::GetInstance()->SetSensorVideoTargetGray(setting->video_target_gray);
		break;
	case PARAM_CAMERA_CAPTURE_TARGET:
		Sensor::GetInstance()->SetSensorCaptureTargetGray(setting->trigger_target_gray);
		break;
	case PARAM_CAMERA_AEW_MODE:
		switch (setting->aew_mode) {
		case AEWMODE_DISABLE:
			Sensor::GetInstance()->SetSensorAEManual();
			break;
		case AEWMODE_GAIN:
			Sensor::GetInstance()->SetSensorAEAuto();
			Sensor::GetInstance()->SetSensorAEMethod(0x01);
			break;
		case AEWMODE_EXP:
			Sensor::GetInstance()->SetSensorAEAuto();
			Sensor::GetInstance()->SetSensorAEMethod(0x02);
			break;
		case AEWMODE_AUTO:
			Sensor::GetInstance()->SetSensorAEAuto();
			Sensor::GetInstance()->SetSensorAEMethod(0x00);
			break;
		default:
			break;
		}
		break;
	default :
		switch (setting->aew_mode) {
			case AEWMODE_DISABLE:
				Sensor::GetInstance()->SetSensorAEManual();
				break;
			case AEWMODE_GAIN:
				Sensor::GetInstance()->SetSensorAEAuto();
				Sensor::GetInstance()->SetSensorAEMethod(0x01);
				break;
			case AEWMODE_EXP:
				Sensor::GetInstance()->SetSensorAEAuto();
				Sensor::GetInstance()->SetSensorAEMethod(0x02);
				break;
			case AEWMODE_AUTO:
				Sensor::GetInstance()->SetSensorAEAuto();
				Sensor::GetInstance()->SetSensorAEMethod(0x00);
				break;
			default:
				break;
		}

		Sensor::GetInstance()->SetSensorExposure(setting->default_exposure);
		Sensor::GetInstance()->SetSensorMinExp(setting->min_exposure);
		Sensor::GetInstance()->SetSensorMaxExp(setting->max_exposure);
		Sensor::GetInstance()->SetSensorMinGain(setting->min_gain);
		Sensor::GetInstance()->SetSensorMaxGain(setting->max_gain);
		Sensor::GetInstance()->SetSensorGain(setting->default_gain);
		Sensor::GetInstance()->SetSensorBgain(setting->blue_gain);
		Sensor::GetInstance()->SetSensorRgain(setting->red_gain);
		Sensor::GetInstance()->SetSensorVideoTargetGray(setting->video_target_gray);
		Sensor::GetInstance()->SetSensorCaptureTargetGray(setting->trigger_target_gray);
		Sensor::GetInstance()->SetSensorAEZone(setting->ae_zone);
		break;
	}

	return 0;
}

int TcpServer::ProcessSetFlashParam(char *buf)
{
	Debug();
	FlashParam *setting = (FlashParam *)buf;
	Parameters::GetInstance()->SetFlashParam(setting);

	unsigned int mode = 0;
	if (setting->flash_mode)
		mode |= 0x01;
	if (setting->led_mode)
		mode |= 0x02;
	if (setting->continuous_light)
		mode |= 0x04;
	Sensor::GetInstance()->SetSensorFlashMode(mode);

	if (setting->redlight_sync_mode) {
		Sensor::GetInstance()->SetSensorSyncOn();
	} else {
		Sensor::GetInstance()->SetSensorSyncOff();
	}

	Sensor::GetInstance()->SetSensorFlashDelay(setting->flash_delay);
	Sensor::GetInstance()->SetSensorRedLightDelay(setting->redlight_delay);
	Sensor::GetInstance()->SetSensorRedLightEfficient(setting->redlight_efficient);
	Sensor::GetInstance()->SetSensorLEDMultiple(setting->led_mutiple);

	return 0;
}

int TcpServer::ProcessSetDeviceInfo(char *buf)
{
	Debug();
	DeviceInfo *setting = (DeviceInfo *)buf;
	Parameters::GetInstance()->SetDeviceInfo(setting);

	return 0;
}

int TcpServer::ProcessSetNetworkParam(char *buf)
{
	Debug();
	
	NetworkParam *setting = (NetworkParam *)buf;
	Parameters::GetInstance()->SetNetworkParam(setting);
	return 0;
}

int TcpServer::ProcessSetUploadParam(char *buf)
{
	Debug();
	
	UploadParam *setting = (UploadParam *)buf;
	Parameters::GetInstance()->SetUploadParam(setting);

	InitClient();

	return 0;
}

int TcpServer::ProcessCalibrateTime(struct payload_req *req, char *buf)
{
	Ldczn_time *ldczn_time = (Ldczn_time *)buf;
	char timestr[20];
	bzero(timestr, 20);
	/*int year 	= (int)ldczn_time->year;
	int mon		= (int)ldczn_time->mon;
	int day		= (int)ldczn_time->day;
	int hour	= (int)ldczn_time->hour;
	int min		= (int)ldczn_time->min;
	int sec		= (int)ldczn_time->sec;
	sprintf(timestr, "%04d-%02d-%02d %02d:%02d:%02d",
			year, mon, day, hour, min, sec);*/
	sprintf(timestr, "%4d-%2d-%2d %2d:%2d:%2d",
		(int)ldczn_time->year,
		(int)ldczn_time->mon,
		(int)ldczn_time->day,
		(int)ldczn_time->hour,
		(int)ldczn_time->min,
		(int)ldczn_time->sec
		);
	Util::CalibrateTime(timestr);
	
	struct packet_man_comp_time_ack packet;
	char auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, 
			auth_code, MESSAGE_TYPE_ACK, sizeof(packet));
	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;
	packet.time		= *ldczn_time;
	SendToClient((char *)&packet, sizeof(packet));
	
	return 0;	
}

int TcpServer::ProcessGetParameter(struct payload_req *req)
{
	Debug();
	switch (req->type & 0x00FF0000) {
	case PARAM_TYPE_CAMERA:
		ProcessGetCameraParameter(req);
		break;
	case PARAM_TYPE_NETWORK:
		ProcessGetNetworkParameter(req);
		break;
	case PARAM_TYPE_UPLOAD:
		ProcessGetUploadParam(req);
		break;
	case PARAM_TYPE_TIME:
		break;
	case PARAM_TYPE_FLASH:
		ProcessGetFlashParam(req);
		break;
	case PARAM_TYPE_PLATE:
		break;
	case PARAM_TYPE_VIDEO_DETECT:
		break;
	case PARAM_TYPE_MANNUFACTURE:
		break;
	case PARAM_TYPE_DEVICE_INFO:
		ProcessGetDeviceInfo(req);
		break;
	case PARAM_TYPE_TRAFFIC:
		ProcessGetTrafficParam(req);
		break;
	default :
		break;
        }
        return 0;
}

int TcpServer::ReturnAck(struct payload_req *req)
{
	struct packet_img_gparm_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= req->id;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	SendToClient((char *)&packet, sizeof(packet));
	return 0;
}

int TcpServer::ProcessGetCameraParameter(struct payload_req *req)
{
	Debug();
	CameraParam params = Parameters::GetInstance()->GetCameraParam();

	struct packet_img_gparm_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	packet.parameter.default_exposure	= params.default_exposure;
	packet.parameter.min_exposure		= params.min_exposure;
	packet.parameter.max_exposure		= params.max_exposure;
	packet.parameter.default_gain		= params.default_gain;
	packet.parameter.min_gain		= params.min_gain;
	packet.parameter.max_gain		= params.max_gain;
	packet.parameter.red_gain		= params.red_gain;
	packet.parameter.blue_gain		= params.blue_gain;
	packet.parameter.video_target_gray	= params.video_target_gray;
	packet.parameter.trigger_target_gray	= params.trigger_target_gray;
	packet.parameter.ae_zone 		= params.ae_zone;
	packet.parameter.aew_mode		= params.aew_mode;

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}

int TcpServer::ProcessGetDeviceInfo(struct payload_req *req)
{
	Debug();
	DeviceInfo params = Parameters::GetInstance()->GetDeviceInfo();

	struct packet_deviceinfo_gparam_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	memcpy(&packet.info, &params, sizeof(DeviceInfo));

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}

int TcpServer::ProcessGetNetworkParameter(struct payload_req *req)
{
	Debug();
	NetworkParam params = Parameters::GetInstance()->GetNetworkParam();

	struct packet_networparam_gparam_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	memcpy(&packet.parameter, &params, sizeof(NetworkParam));

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}

int TcpServer::ProcessGetUploadParam(struct payload_req *req)
{
	Debug();
	UploadParam params = Parameters::GetInstance()->GetUploadParam();

	struct packet_uploadinfo_gparam_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	memcpy(&packet.parameter, &params, sizeof(UploadParam));

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}


int TcpServer::ProcessGetTrafficParam(struct payload_req *req)
{
	Debug();
	TrafficParam params = Parameters::GetInstance()->GetTrafficParam();

	struct packet_deviceinfo_gparam_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	memcpy(&packet.info, &params, sizeof(TrafficParam));

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}

int TcpServer::ProcessGetFlashParam(struct payload_req *req)
{
	Debug();
	FlashParam params = Parameters::GetInstance()->GetFlashParam();

	struct packet_flash_gparam_ack packet;
	char	auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, auth_code,
			MESSAGE_TYPE_ACK, sizeof(packet));

	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;

	memcpy(&packet.parameter, &params, sizeof(FlashParam));

	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}

int TcpServer::ProcessMannufacture(struct payload_req *req, char *buf)
{
	Debug();
	switch(req->type & 0X00FF0000) {
	case REQ_MAN_FMT:
		break;
	case REQ_MAN_UPG:
		return ProcessUpgrade(req, buf);
		break;
	case REQ_MAN_RESET:
		break;
	case REQ_MAN_CLR:
		break;
	default:
		break;
	}

	return -1;
}

int TcpServer::ProcessUpgrade(struct payload_req *req, char *buf)
{
	Debug();

	switch(req->type & 0X000000FF) {

	case REQ_MAN_UPG_BACK:
		break;
	case REQ_MAN_UPG_APP:
		return ProcessUpgradeApp(req, buf);
		break;
	case REQ_MAN_UPG_PREF:
		break;
	case REQ_MAN_UPG_DRV:
		break;
	case REQ_MAN_UPG_ALG_MOD:
		break;
	case REQ_MAN_UPG_LIB:
		break;
	case REQ_MAN_UPG_ALG:
		break;
	case REQ_MAN_UPG_MON:
		break;
	case REQ_MAN_UPG_MCU:
		break;
	case REQ_MAN_UPG_FPGA:
		break;
	default:
		break;
	}
	
	return -1;
}

int TcpServer::ProcessUpgradeApp(struct payload_req *req, char *buf)
{
	Debug();
	
	payload_man_upgrade *upd_camera = (payload_man_upgrade *)buf;
	int file_length = upd_camera->total_length;
	char *file_name = upd_camera->file_name;

	char directory[512] = "/data/";
	file_name = strcat(directory, file_name);
	FILE *fp = fopen(file_name, "w");
	char buffer[RECV_BUF_LENGTH];
	int rec_length = 0;
	while (1) {
		bzero(buffer, RECV_BUF_LENGTH);
		int ret = Socket::Read(clnt_sock, buffer, RECV_BUF_LENGTH, 5000);
		if (ret > 0) {
			int nwrite = fwrite(buf, sizeof(char), ret, fp);
			if (nwrite < ret)
				Debug("Write data to file failed");
			rec_length += ret;
		}
		else if(ret == -1) {
			Debug("read sock failed");
		}
		if (rec_length < file_length)
			continue;
		fclose(fp);
		break;
	}

	struct packet_man_upgrade_ack packet;
	char auth_code[sizeof(packet.ack.head)];
	fill_std_header(&packet.ack.head, ENCODING_TYPE_RAW, 
			auth_code, MESSAGE_TYPE_ACK, sizeof(packet));
	packet.ack.ack.id	= 0;
	packet.ack.ack.type	= req->type;
	packet.ack.ack.status	= ACK_SUCCESS;
	SendToClient((char *)&packet, sizeof(packet));

	return 0;
}
