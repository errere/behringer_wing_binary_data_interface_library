#include <stdio.h>
#include <winsock2.h>
#include <stdint.h>
#include <WS2tcpip.h>

#include "elog.h"

#include "nrpc.h"

#include "wing_bin_intf.h"



#pragma comment(lib, "ws2_32.lib")  // 链接Winsock库

#define WINSOCK_DEPRECATED_NO_WARNINGS

#define DEFAULT_SERVER "192.168.31.234"
#define DEFAULT_PORT 2222
#define BUFFER_SIZE 4096

WSADATA wsaData;
SOCKET clientSocket = INVALID_SOCKET;
struct sockaddr_in serverAddr;
int result;

wing_bin_socket_intf_t wing_intf;
wing_bin_handle_t wing_decode;

uint8_t sendBuf[] = { 0xdf ,0xd1, 0xda ,0xdd };//4
uint8_t sendBuf2[] = { 0xdf ,0xd1, 0xd7 ,0x7e, 0x46 ,0x34, 0x74, 0xdd };//8
uint8_t sendBuf3[] = { 0xdf ,0xd1, 0xd7, 0x38, 0xae, 0x75 ,0xc2 ,0xd5, 0xc1, 0xc2 ,0x2c, 0x8c };//12 
uint8_t sendBuf4[] = { 0xdf ,0xd1, 0xd7, 0x38, 0xae, 0x75 ,0xc2 ,0xdc };//8

uint8_t sendBuf_get_all[] = { 0xdf ,0xd1, 0xda ,0xdc };//4

uint8_t recvBuf[BUFFER_SIZE];
int recvLen;

static const char TAG[] = "msvc_app";

wing_bin_err_t wing_tx(uint8_t* tx_bytes, size_t len, void* user) {
	// 5. 发送数据
	result = send(clientSocket, (const char*)tx_bytes, len, 0);
	if (result == SOCKET_ERROR) {
		elog_i("app", "send failed: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return WING_BIN_INTF_ERR;
	}
	elog_hexdump("send", 8, tx_bytes, len);
	return WING_BIN_INTF_OK;
}//wing_tx

void wing_frame_done(wing_bin_decoded_frame_t* decoded, void* user)
{

	switch (decoded->type) {
	case WING_BIN_FRAME_TYPE_BOOL:
		elog_i(TAG, "hash %08x  get bool %d", decoded->hash, decoded->boolen);
		break;//WING_BIN_FRAME_TYPE_BOOL

	case WING_BIN_FRAME_TYPE_INT_IN_TOKEN:
	case WING_BIN_FRAME_TYPE_INT16:
	case WING_BIN_FRAME_TYPE_INT32:
		elog_i(TAG, "hash %08x  get int(type=%d) %d", decoded->hash, decoded->type, decoded->intx);
		break;//WING_BIN_FRAME_TYPE_INT_IN_TOKEN

	case WING_BIN_FRAME_TYPE_NODE_INDEX_IN_TOKEN:
	case WING_BIN_FRAME_TYPE_NODE_INDEX:
		elog_i(TAG, "hash %08x  get node(type=%d) index %d", decoded->hash, decoded->type, decoded->node_index);
		break;//WING_BIN_FRAME_TYPE_NODE_INDEX_IN_TOKEN

	case WING_BIN_FRAME_TYPE_STRING:
		elog_i(TAG, "hash %08x  get string , len=%d , txt = %.*s",
			decoded->hash, decoded->string.len,
			decoded->string.len,
			decoded->string.txt);
		break;//WING_BIN_FRAME_TYPE_STRING

	case WING_BIN_FRAME_TYPE_NODE_NAME:
		elog_i(TAG, "hash %08x  get node name , len=%d , txt = %.*s",
			decoded->hash, decoded->string.len,
			decoded->string.len,
			decoded->string.txt);
		break;//WING_BIN_FRAME_TYPE_NODE_NAME

	case WING_BIN_FRAME_TYPE_FLOAT32:
		elog_i(TAG, "hash %08x  get float %f", decoded->hash, decoded->f32);
		break;//WING_BIN_FRAME_TYPE_FLOAT32

	case WING_BIN_FRAME_TYPE_NDR:
		elog_i(TAG, "------------------------>token = %02x", decoded->token);
		elog_i(TAG, "parent:%08lx", decoded->ndr.parent);
		elog_i(TAG, "hash:%08lx", decoded->ndr.hash);
		elog_i(TAG, "index:%04x", decoded->ndr.index);
		elog_i(TAG, "name_len:%d", decoded->ndr.name_len);
		elog_i(TAG, "name:%.*s", decoded->ndr.name_len, decoded->ndr.name);
		elog_i(TAG, "longname_len:%d", decoded->ndr.longname_len);
		elog_i(TAG, "longname:%.*s", decoded->ndr.longname_len, decoded->ndr.longname);
		elog_i(TAG, "flag:%04x", decoded->ndr.flag);
		if (decoded->ndr.flag > 0) {
			elog_i(TAG, "flag->unit:%x", decoded->ndr.flag_bits.unit);
			elog_i(TAG, "flag->type:%x", decoded->ndr.flag_bits.type);
			elog_i(TAG, "flag->ro:%x", decoded->ndr.flag_bits.is_readonly);
		}
		elog_i(TAG, "------------------------>token %02x end", decoded->token);
		break;//WING_BIN_FRAME_TYPE_NDR

	default:
		break;//default

	}//switch (decoded->type)

}//wing_frame_done

int tcpLoop() {

	/*wing_tx(sendBuf4, 8, NULL);*/
	wing_tx(sendBuf_get_all, 4, NULL);

	// 6. 接收服务器响应
	recvLen = recv(clientSocket, (char*)recvBuf, BUFFER_SIZE - 1, 0);
	if (recvLen > 0) {
		wing_bin_rx_data(&wing_decode, recvBuf, recvLen);
	}
	else if (recvLen == 0) {
		elog_i("app", "Connection closed by server.\n");
	}
	else {
		elog_i("app", "recv failed: %d\n", WSAGetLastError());
	}

	Sleep(5000);
}//tcpLoop

int main() {


	elog_init();
	/* set EasyLogger log format */
	elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
	elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
	elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
	elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
	elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));
	elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL & ~(ELOG_FMT_FUNC | ELOG_FMT_T_INFO | ELOG_FMT_P_INFO));

	elog_set_text_color_enabled(true);

	/* start EasyLogger */
	elog_start();


	// 1. 初始化Winsock
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		elog_i("app", "WSAStartup failed: %d\n", result);
		return 1;
	}

	// 2. 创建socket
	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (clientSocket == INVALID_SOCKET) {
		elog_i("app", "socket failed: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// 3. 设置服务器地址结构
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(DEFAULT_PORT);
	inet_pton(AF_INET, DEFAULT_SERVER, &serverAddr.sin_addr);
	if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
		elog_i("app", "Invalid address: %s\n", DEFAULT_SERVER);
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}

	// 4. 连接到服务器
	result = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR) {
		elog_i("app", "connect failed: %d\n", WSAGetLastError());
		closesocket(clientSocket);
		WSACleanup();
		return 1;
	}
	elog_i("app", "Connected to %s:%d\n", DEFAULT_SERVER, DEFAULT_PORT);


	wing_bin_decode_init(&wing_decode);

	//wing_intf.tx_evt = wing_tx;
	wing_intf.frame_done = wing_frame_done;
	wing_bin_decode_link_intf(&wing_decode, wing_intf);

	for (;;) {

		tcpLoop();

	}//for

	// 7. 清理
	closesocket(clientSocket);
	WSACleanup();
	return 0;
}
