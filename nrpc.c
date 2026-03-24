#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "nrpc.h"


/* 初始化上下文 */
void nrpc_init(NRPCContext_t* ctx) {
	ctx->escf = 0;
	ctx->ch_id_rx = -1;
	ctx->ch_id_tx = -1;
	ctx->tx_byte = NULL;
	ctx->flush = NULL;
	ctx->rx_data = NULL;
	ctx->user = NULL;
}//nrpc_init

void nrpc_set_user_data(NRPCContext_t* ctx, void* user) {
	ctx->user = user;
}//nrpc_set_user_data

/* 注册发送字节回调 */
void nrpc_set_tx_byte_callback(NRPCContext_t* ctx, nrpc_tx_byte_func tx) {
	ctx->tx_byte = tx;
}

/* 注册刷新回调（可选） */
void nrpc_set_flush_callback(NRPCContext_t* ctx, nrpc_flush_func flush) {
	ctx->flush = flush;
}

/* 注册接收数据回调 */
void nrpc_set_rx_data_callback(NRPCContext_t* ctx, nrpc_rx_data_func rx) {
	ctx->rx_data = rx;
}

/*------------------------------------------------------------------------------
 * 接收例程：每次收到一个字节时调用
 *------------------------------------------------------------------------------*/
void nrpc_data_rx(NRPCContext_t* ctx, uint8_t db) {
	if (db == NRP_ESCAPE_CODE && !ctx->escf) {
		/* 第一个转义字节，进入转义状态 */
		ctx->escf = 1;
	}
	else {
		// db != NRP_ESCAPE_CODE  || escf==true
		if (ctx->escf) {
			/* 前一个字节是转义字节，处理当前字节 */
			if (db != NRP_ESCAPE_CODE)
			{
				ctx->escf = 0;  /* 清除转义标志 */

				// de->df
				if (db == NRP_ESCAPE_CODE - 1)
				{
					db = NRP_ESCAPE_CODE;
				} // db == NRP_ESCAPE_CODE - 1

				 // in channel id (d0 < db < de)
				else if (db >= NRP_CHANNEL_ID_BASE &&
					db < NRP_CHANNEL_ID_BASE + NRP_NUM_CHANNELS)
				{
					if (ctx->ch_id_rx != db - NRP_CHANNEL_ID_BASE)
					{
						ctx->ch_id_rx = db - NRP_CHANNEL_ID_BASE;
					}
					return;
				} // else if
				else if (ctx->ch_id_rx >= 0 && ctx->rx_data)
				{
					ctx->rx_data(ctx->ch_id_rx, NRP_ESCAPE_CODE, ctx->user);
				} // ch_id_rx >= 0

			}//if (db != NRP_ESCAPE_CODE)
		}//if (ctx->escf)
		/* 普通数据字节（或转义后需要继续处理的字节） */
		if (ctx->ch_id_rx >= 0 && ctx->rx_data) {
			ctx->rx_data(ctx->ch_id_rx, db, ctx->user);
		}//if (ctx->ch_id_rx >= 0 && ctx->rx_data) 
	}//!(db == NRP_ESCAPE_CODE && !escf)
}//nrpc_data_rx

/*------------------------------------------------------------------------------
 * 发送例程：发送指定通道的数据
 * 参数：
 *   ctx    - 上下文
 *   ch_id  - 目标通道 (1..14)
 *   data   - 待发送数据缓冲区
 *   len    - 数据长度
 *------------------------------------------------------------------------------*/
void nrpc_data_tx(NRPCContext_t* ctx, int ch_id, const void* data, int len) {
	assert(ch_id >= 1 && ch_id <= NRP_NUM_CHANNELS);
	assert(ctx->tx_byte != NULL);   /* 必须注册发送回调 */

	/* 如果需要切换通道 */
	if (ctx->ch_id_tx != ch_id) {
		/* 可选：刷新之前的发送缓冲区 */
		if (ctx->flush) {
			ctx->flush(ctx->user);
		}
		/* 发送通道切换序列：0xdf + (ch_id + 0xd0) */
		ctx->tx_byte(NRP_ESCAPE_CODE, ctx->user);
		ctx->tx_byte((uint8_t)(ch_id + NRP_CHANNEL_ID_BASE), ctx->user);
		ctx->ch_id_tx = ch_id;
	}//if (ctx->ch_id_tx != ch_id) 

	int esc = 0;                /* 本地转义标志 */
	const uint8_t* dpp = (const uint8_t*)data;

	while (len-- > 0) {
		uint8_t db = *dpp;
		dpp++;
		if (db == NRP_ESCAPE_CODE) {
			esc = 1;            /* 遇到 0xdf，暂不发送，等待下一个字节 */
		}
		else {
			if (esc && db >= NRP_CHANNEL_ID_BASE &&
				db <= NRP_CHANNEL_ID_BASE + NRP_NUM_CHANNELS) {
				//last byte = NRP_ESCAPE_CODE  &&  this byte in channels id
				db = NRP_ESCAPE_CODE - 1;
				//数据指针回退一位,增加剩余长度
				dpp--;
				len++;
			}
			esc = false;
		}//else
		ctx->tx_byte(db, ctx->user);
	}//while

	/* 如果最后一个字节是 0xdf，则它尚未发送，需要补发 0xde */
	if (esc) {
		ctx->tx_byte(NRP_ESCAPE_CODE - 1, ctx->user);
	}
}

void nrpc_reset_stm(NRPCContext_t* ctx) {
	ctx->escf = 0;
	ctx->ch_id_rx = -1;
	ctx->ch_id_tx = -1;
}//nrpc_reset_stm