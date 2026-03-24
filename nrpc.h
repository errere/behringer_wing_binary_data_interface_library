#pragma once
/* nrpc.h */

#include <stdint.h>
#include <stdbool.h>

#define NRP_ESCAPE_CODE         0xdf
#define NRP_CHANNEL_ID_BASE     0xd0
#define NRP_NUM_CHANNELS        14

//extern "C"
//
//{
/* 回调函数类型定义 */
typedef int (*nrpc_tx_byte_func)(uint8_t byte, void* user);      /* 发送单个字节 */
typedef int (*nrpc_flush_func)(void* user);                /* 刷新发送缓冲区（可选） */
typedef int (*nrpc_rx_data_func)(int channel, uint8_t data, void* user); /* 接收数据字节 */

/* 上下文结构体 */
typedef struct {
	/* 接收状态 */
	int escf;               /* 转义标志，0 或 1 */
	int ch_id_rx;           /* 当前接收通道，-1 表示未初始化 */

	/* 发送状态 */
	int ch_id_tx;           /* 当前发送通道，-1 表示未初始化 */

	/* 回调函数指针 */
	nrpc_tx_byte_func tx_byte;
	nrpc_flush_func    flush;
	nrpc_rx_data_func  rx_data;

	void* user;
} NRPCContext_t;



void nrpc_init(NRPCContext_t* ctx);

void nrpc_set_user_data(NRPCContext_t* ctx, void* user);

void nrpc_set_tx_byte_callback(NRPCContext_t* ctx, nrpc_tx_byte_func tx);

void nrpc_set_flush_callback(NRPCContext_t* ctx, nrpc_flush_func flush);

void nrpc_set_rx_data_callback(NRPCContext_t* ctx, nrpc_rx_data_func rx);

int nrpc_data_rx(NRPCContext_t* ctx, uint8_t db);

int nrpc_data_tx(NRPCContext_t* ctx, int ch_id, const void* data, int len);

void nrpc_reset_rx_stm(NRPCContext_t* ctx);
void nrpc_reset_tx_stm(NRPCContext_t* ctx);

//}