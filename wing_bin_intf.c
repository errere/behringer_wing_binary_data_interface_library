#include "wing_bin_intf.h"

#include <string.h>
#include <stdio.h>


#include "elog.h"

static const char TAG[] = "wing_intf";

#define WING_BIN_INTF_ASSERT(x) (assert(x))

#define WING_BIN_AUDIO_ENGINE_CONTROL_CHANNEL_ID (1)


#define WING_BIN_TOKEN_NORMAL (0)

#define WING_BIN_TOKEN_UNUSED (1<<16)//0xe0…0xff 
#define WING_BIN_TOKEN_LEN_VARABLE (2<<16)//0xdf 
#define WING_BIN_TOKEN_BOOL_VALUE (3<<16)//0x00…0x01
#define WING_BIN_TOKEN_INT_DATA (4<<16)//0x02…0x3f
#define WING_BIN_TOKEN_NODE_INDEX (5<<16)//0x40…0x7f
#define WING_BIN_TOKEN_STRINGS (6<<16)//0x80…0xbf
#define WING_BIN_TOKEN_NODE_NAME (7<<16)//0xc0…0xcf
#define WING_BIN_TOKEN_EMPTY_STRING (8<<16)//0xd0
#define WING_BIN_TOKEN_STRING (9<<16) //0xd1
#define WING_BIN_TOKEN_END_OF_DATA (10<<16)//0xde


#define WING_BIN_TOKEN_TYPE_MASK (0xffff0000)
#define WING_BIN_TOKEN_LENGTH_MASK (0xffff)

/*========================================locals=====================================================*/

static int32_t wing_bin_token_get_idal_payload_length(uint8_t token) {
	if (token == 0x00 || token == 0x01) return WING_BIN_TOKEN_BOOL_VALUE;//false; off; 0  |  true; on; 1
	else if (token >= 0x02 && token <= 0x3f) return WING_BIN_TOKEN_INT_DATA;//int 2…63
	else if (token >= 0x40 && token <= 0x7f) return WING_BIN_TOKEN_NODE_INDEX;//node index 1…64
	else if (token >= 0x80 && token <= 0xbf) return WING_BIN_TOKEN_STRINGS;//string[1…64]
	else if (token >= 0xc0 && token <= 0xcf) return WING_BIN_TOKEN_NODE_NAME;//node name[1…16]
	else if (token == 0xd0) return WING_BIN_TOKEN_EMPTY_STRING;//empty string
	else if (token == 0xd1) return WING_BIN_TOKEN_STRING;//string[1…256]
	else if (token == 0xd2) return 2;//node index 1…65536
	else if (token == 0xd3) return 2;//int16
	else if (token == 0xd4) return 4;//int32
	else if (token == 0xd5) return 4;//float32
	else if (token == 0xd6) return 4;//raw float32 (0.0…1.0)
	else if (token == 0xd7) return 4;//node hash
	else if (token == 0xd8) return 0;//click (toggle)
	else if (token == 0xd9) return 1;//step (inc/dec)
	else if (token == 0xda) return 0;//node tree: goto root node
	else if (token == 0xdb) return 0;//node tree: 1 level up
	else if (token == 0xdc) return 0;//data request
	else if (token == 0xdd) return 0;//request node definition (current node)
	else if (token == 0xde) return WING_BIN_TOKEN_END_OF_DATA;//end of data/def request
	else if (token == 0xdf) return WING_BIN_TOKEN_LEN_VARABLE;//node definition response (word: data length in bytes)
	else  return WING_BIN_TOKEN_UNUSED;//not used
}//wing_bin_token_get_idal_payload_length

static uint32_t wing_bin_pack_u32(uint8_t* buf) {
	//wing big endian pack
	return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3]);
}//wing_bin_pack_u32

static uint16_t wing_bin_pack_u16(uint8_t* buf) {
	//wing big endian pack
	return ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
}//wing_bin_pack_u16

static int32_t wing_bin_pack_i32(uint8_t* buf) {
	//wing big endian pack
	return ((int32_t)buf[0] << 24) | ((int32_t)buf[1] << 16) | ((int32_t)buf[2] << 8) | ((int32_t)buf[3]);
}//wing_bin_pack_i32

static int16_t wing_bin_pack_i16(uint8_t* buf) {
	//wing big endian pack
	return ((int32_t)buf[0] << 8) | ((int32_t)buf[1]);
}//wing_bin_pack_i16

static float wing_bin_pack_f32(uint8_t* buf) {
	//wing big endian pack
	// 将大端字节顺序组合成一个 32 位无符号整数
	uint32_t n = ((uint32_t)buf[0] << 24) |
		((uint32_t)buf[1] << 16) |
		((uint32_t)buf[2] << 8) |
		((uint32_t)buf[3]);

	float f;
	// 将整数的位模式按原样复制到 float 变量中
	memcpy(&f, &n, sizeof(f));
	return f;
}//wing_bin_pack_f32

static wing_bin_err_t wing_bin_frame_decode(wing_bin_handle_t* handle) {
	wing_bin_err_t err = 0;
	uint8_t* bufferx = handle->rx.buffer;

	handle->rx.decoded.token = handle->rx.token;

	//call token event
	if (handle->intf.frame_get) {
		err |= handle->intf.frame_get(handle->rx.decoded.token, handle->rx.payload_len, bufferx, handle->user);
	}

	switch (handle->rx.decoded.token)
	{
	case 0xd7:
		//node hash
		elog_d(TAG, "hash %08x", handle->rx.decoded.hash);
		handle->rx.decoded.hash = wing_bin_pack_u32(bufferx);
		goto exit;
		break;//0xd7

	case 0x00:
	case 0x01:
		//bool
		elog_d(TAG, "hash %08x  get bool %d", handle->rx.decoded.hash, handle->rx.decoded.token);
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_BOOL;
		handle->rx.decoded.boolen = handle->rx.decoded.token;
		break;//0x00,0x01

	case 0xd0:
		//empty str
		elog_d(TAG, "hash %08x  get empty string", handle->rx.decoded.hash);
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_STRING;
		handle->rx.decoded.string.len = 0;
		handle->rx.decoded.string.txt = bufferx;
		break;//0xd5

	case 0xd1:
		//string[1…256]
		elog_d(TAG, "hash %08x  get string , len=%d , txt = %.*s",
			handle->rx.decoded.hash, handle->rx.payload_len,
			handle->rx.payload_len,
			bufferx);
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_STRING;
		handle->rx.decoded.string.len = handle->rx.payload_len;
		handle->rx.decoded.string.txt = bufferx;
		break;//0xd1

	case 0xd2:
		//word node index 1…65536
		elog_d(TAG, "hash %08x  get node index %d", handle->rx.decoded.hash, wing_bin_pack_u16(bufferx));
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_NODE_INDEX;
		handle->rx.decoded.node_index = wing_bin_pack_u16(bufferx);
		break;//0xd2

	case 0xd3:
		//word int16
		elog_d(TAG, "hash %08x  get i16 %d", handle->rx.decoded.hash, wing_bin_pack_i16(bufferx));
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_INT16;
		handle->rx.decoded.intx = wing_bin_pack_i16(bufferx);
		break;//0xd3

	case 0xd4:
		//long int32
		elog_d(TAG, "hash %08x  get i32 %d", handle->rx.decoded.hash, wing_bin_pack_i32(bufferx));
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_INT32;
		handle->rx.decoded.intx = wing_bin_pack_i32(bufferx);
		break;//0xd4

	case 0xd5:
		//long float32
		elog_d(TAG, "hash %08x  get float %f", handle->rx.decoded.hash, wing_bin_pack_f32(bufferx));
		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_FLOAT32;
		handle->rx.decoded.f32 = wing_bin_pack_f32(bufferx);
		break;//0xd5

	case 0xdf:
		//node definition response

		handle->rx.decoded.type = WING_BIN_FRAME_TYPE_NDR;
		handle->rx.decoded.ndr.parent = wing_bin_pack_u32(bufferx);
		bufferx += 4;
		handle->rx.decoded.ndr.hash = wing_bin_pack_u32(bufferx);
		bufferx += 4;
		handle->rx.decoded.ndr.index = wing_bin_pack_u16(bufferx);
		bufferx += 2;
		handle->rx.decoded.ndr.name_len = *(bufferx);
		bufferx += 1;
		handle->rx.decoded.ndr.name = bufferx;
		bufferx += handle->rx.decoded.ndr.name_len;
		handle->rx.decoded.ndr.longname_len = *bufferx;
		bufferx += 1;
		handle->rx.decoded.ndr.longname = bufferx;
		bufferx += handle->rx.decoded.ndr.longname_len;
		handle->rx.decoded.ndr.flag = wing_bin_pack_u16(bufferx);

		elog_d(TAG, "------------------------>token = %02x", handle->rx.decoded.token);
		elog_d(TAG, "parent:%08lx", handle->rx.decoded.ndr.parent);
		elog_d(TAG, "hash:%08lx", handle->rx.decoded.ndr.hash);
		elog_d(TAG, "index:%04x", handle->rx.decoded.ndr.index);
		elog_d(TAG, "name_len:%d", handle->rx.decoded.ndr.name_len);
		elog_d(TAG, "name:%.*s", handle->rx.decoded.ndr.name_len, handle->rx.decoded.ndr.name);
		elog_d(TAG, "longname_len:%d", handle->rx.decoded.ndr.longname_len);
		elog_d(TAG, "longname:%.*s", handle->rx.decoded.ndr.longname_len, handle->rx.decoded.ndr.longname);
		elog_d(TAG, "flag:%04x", handle->rx.decoded.ndr.flag);
		if (handle->rx.decoded.ndr.flag > 0) {
			elog_d(TAG, "flag->unit:%x", handle->rx.decoded.ndr.flag_bits.unit);
			elog_d(TAG, "flag->type:%x", handle->rx.decoded.ndr.flag_bits.type);
			elog_d(TAG, "flag->ro:%x", handle->rx.decoded.ndr.flag_bits.is_readonly);
		}
		elog_d(TAG, "------------------------>done of %08x", handle->rx.decoded.token);

		break;//0xdf


	default:
		//range & unhandled
		if (handle->rx.decoded.token >= 0x02 && handle->rx.decoded.token <= 0x3f) {
			//0x02…0x3f int 2…63
			elog_d(TAG, "hash %08x  get token int %d", handle->rx.decoded.hash, handle->rx.decoded.token);
			handle->rx.decoded.type = WING_BIN_FRAME_TYPE_INT_IN_TOKEN;
			handle->rx.decoded.intx = handle->rx.decoded.token;
		}//0x02…0x3f int 2…63
		else if (handle->rx.decoded.token >= 0x40 && handle->rx.decoded.token <= 0x7f) {
			//0x40…0x7f node index 1…64
			elog_d(TAG, "hash %08x  get node idx %d", handle->rx.decoded.hash, (handle->rx.decoded.token - 0x3f));
			handle->rx.decoded.type = WING_BIN_FRAME_TYPE_NODE_INDEX_IN_TOKEN;
			handle->rx.decoded.node_index = (handle->rx.decoded.token - 0x3f);
		}//0x40…0x7f node index 1…64
		else if (handle->rx.decoded.token >= 0x80 && handle->rx.decoded.token <= 0xbf) {
			//0x80…0xbf string[1…64]
			elog_d(TAG, "hash %08x  get string , len=%d , txt = %.*s",
				handle->rx.decoded.hash, (handle->rx.decoded.token - 0x7f),
				(handle->rx.decoded.token - 0x7f),
				bufferx);
			handle->rx.decoded.type = WING_BIN_FRAME_TYPE_STRING;
			handle->rx.decoded.string.len = (handle->rx.decoded.token - 0x7f);
			handle->rx.decoded.string.txt = bufferx;
		}//0x80…0xbf string[1…64]
		else if (handle->rx.decoded.token >= 0xc0 && handle->rx.decoded.token <= 0xcf) {
			//0xc0…0xcf node name[1…16]
			elog_d(TAG, "hash %08x  get node name , len=%d , txt = %.*s",
				handle->rx.decoded.hash, (handle->rx.decoded.token - 0xbf),
				(handle->rx.decoded.token - 0xbf),
				bufferx);
			handle->rx.decoded.type = WING_BIN_FRAME_TYPE_NODE_NAME;
			handle->rx.decoded.string.len = (handle->rx.decoded.token - 0xbf);
			handle->rx.decoded.string.txt = bufferx;
		}//0xc0…0xcf node name[1…16]
		else {
			//unhandled
			elog_w(TAG, "hash %08x  unhandled token = %02x", handle->rx.decoded.hash, handle->rx.decoded.token);
			elog_hexdump("frame", 8, bufferx, handle->rx.payload_len);
			handle->rx.decoded.type = WING_BIN_FRAME_TYPE_UNKNOWN;
			handle->rx.decoded.string.len = handle->rx.payload_len;
			handle->rx.decoded.string.txt = bufferx;

			err |= WING_BIN_INTF_ERR_UNHANDLED_TOKEN;
		}//unhandled
		break;//default
	}//switch (handle->rx.decoded.token)

	//call done event
	if (handle->intf.frame_done) {
		err |= handle->intf.frame_done(&(handle->rx.decoded), handle->user);
	}

exit:
	return err;
}//frame_decode

/*=============================================nrpc callbacks================================================*/

static int on_rx_byte_event_cb(int channel, uint8_t data, void* user) {
	wing_bin_err_t err = 0;
	//func wing_bin_rx_data call this
	wing_bin_handle_t* this = (wing_bin_handle_t*)user;
	if (channel == WING_BIN_AUDIO_ENGINE_CONTROL_CHANNEL_ID) {
		//printf("%02x ", data);
		switch (this->decode_stm.state)
		{
			/*
			========================================state 0========================================
			init , decode token , change state
			*/
		case 0:
			//init frame struct
			this->rx_buffer_index = 0;
			this->rx.payload_len = 0;
			this->decode_stm.token_payload_length = 0;
			this->decode_stm.token_type = 0;
			//put token to struct
			this->rx.token = data;
			//process token
			this->decode_stm.token_type = wing_bin_token_get_idal_payload_length(data) & WING_BIN_TOKEN_TYPE_MASK;
			this->decode_stm.token_payload_length = wing_bin_token_get_idal_payload_length(data) & WING_BIN_TOKEN_LENGTH_MASK;
			this->rx.payload_len = this->decode_stm.token_payload_length;//init rx.payload_len
			//elog_i(TAG, "type = %08x , len=%08x", this->decode_stm.token_type, this->decode_stm.token_payload_length);
			if (this->decode_stm.token_type == WING_BIN_TOKEN_NORMAL) {
				//change state to recv data
				this->decode_stm.state = 10;
			}
			else if (this->decode_stm.token_type == WING_BIN_TOKEN_LEN_VARABLE) {
				//change state to calc recv length
				this->decode_stm.state = 1;
			}
			else if (this->decode_stm.token_type == WING_BIN_TOKEN_STRINGS) {
				this->decode_stm.token_payload_length = data - 0x7f;
				this->rx.payload_len = this->decode_stm.token_payload_length;//update rx.payload_len
				//change state to recv data
				this->decode_stm.state = 10;
			}
			else if (this->decode_stm.token_type == WING_BIN_TOKEN_NODE_NAME) {

				this->decode_stm.token_payload_length = data - 0xbf;
				this->rx.payload_len = this->decode_stm.token_payload_length;//update rx.payload_len
				//change state to calc recv length
				this->decode_stm.state = 10;
			}
			else if (this->decode_stm.token_type == WING_BIN_TOKEN_STRING) {
				//change state to calc recv length 1 byte
				this->decode_stm.state = 7;
			}
			//other
			else if (this->decode_stm.token_type == WING_BIN_TOKEN_END_OF_DATA) {
				//end of data , reset stm
				wing_bin_reset_rx_stm(this);
				goto exit;
			}
			else {
				//elog_w(TAG, "token%02x not have payload , type = %04x", data, this->decode_stm.token_type);
				err |= wing_bin_frame_decode(this);
			}
			break;//state 0

			/*
			========================================state 1 & 2========================================
			16 bit word length , with Node Definition Response long length auto process
			*/
		case 1:
			//calc recv length H
			this->decode_stm.token_payload_length = ((uint16_t)data) << 8;
			//change state to calc recv length L
			this->decode_stm.state = 2;
			break;//state 1
		case 2:
			//calc recv length L
			this->decode_stm.token_payload_length |= ((uint16_t)data);

			//in Node Definition Response If len.w==0 then len.l is used
			if (this->decode_stm.token_payload_length == 0 && this->rx.token == 0xdf) {
				//change state to get long len
				elog_w(TAG, "long len used");
				this->decode_stm.state = 3;
			}

			this->rx.payload_len = this->decode_stm.token_payload_length;//update rx.payload_len

			//change state to recv data
			this->decode_stm.state = 10;
			break;//state 2

			/*
			========================================state 3...6========================================
			32 bit word length
			*/
		case 3:
			//calc recv long length H24
			this->decode_stm.token_payload_length = ((uint32_t)data) << 24;
			//change state to recv long length H16
			this->decode_stm.state = 4;
			break;//state 3
		case 4:
			//recv long length H16
			this->decode_stm.token_payload_length |= ((uint32_t)data) << 16;
			//change state to long length H8
			this->decode_stm.state = 5;
			break;//state 4
		case 5:
			//recv long length H8
			this->decode_stm.token_payload_length |= ((uint32_t)data) << 8;
			//change state to recv long length L8
			this->decode_stm.state = 6;
			break;//state 5
		case 6:
			//recv long length L8
			this->decode_stm.token_payload_length |= ((uint32_t)data) & 0xff;

			this->rx.payload_len = this->decode_stm.token_payload_length;//update rx.payload_len

			elog_w(TAG, "long len =%ld", this->rx.payload_len);

			//change state to recv data
			this->decode_stm.state = 10;
			break;//state 6

			/*
			========================================state 7========================================
			8 bit word length
			*/
		case 7:
			//calc recv length 1 byte
			this->decode_stm.token_payload_length = data;
			//change state to recv data
			this->decode_stm.state = 10;
			break;//state 7

			/*
			========================================state 10========================================
			recv data use this->decode_stm.token_payload_length
			*/
		case 10:
			//recv data
			if (this->decode_stm.token_payload_length >= WING_BIN_INTF_RX_BUFFER_SIZE) {
				elog_e(TAG, "data length bigger than buffer size");
				//reset recv stm
				wing_bin_reset_rx_stm(this);
				err |= WING_BIN_INTF_ERR_BUFFER_OVERFLOW;
				goto exit;//exit func
			}

			this->rx.buffer[this->rx_buffer_index] = data;
			this->rx_buffer_index++;

			this->decode_stm.token_payload_length--;
			if (this->decode_stm.token_payload_length == 0) {
				err |= wing_bin_frame_decode(this);
				//change state start
				this->decode_stm.state = 0;
			}
			break;//state 10

		default:
			break;//default
		}//switch (this->decode_stm.state)
	}//WING_BIN_AUDIO_ENGINE_CONTROL_CHANNEL_ID
exit:
	return (int)err;
}//on_rx_byte_event_cb

static int on_tx_bytes(uint8_t byte, void* user) {
	wing_bin_handle_t* this = (wing_bin_handle_t*)user;
}//on_tx_bytes

static int on_tx_flush(void* user) {
	wing_bin_handle_t* this = (wing_bin_handle_t*)user;
}//on_tx_flush

/*=================================================APIs============================================*/

wing_bin_err_t wing_bin_decode_init(wing_bin_handle_t* handle) {
	WING_BIN_INTF_ASSERT(handle);

	memset(handle, 0x00, sizeof(wing_bin_handle_t));

	nrpc_init(&(handle->nrpc_ctx));
	nrpc_set_user_data(&(handle->nrpc_ctx), (void*)handle);
	nrpc_set_rx_data_callback(&(handle->nrpc_ctx), on_rx_byte_event_cb);
	nrpc_set_tx_byte_callback(&(handle->nrpc_ctx), on_tx_bytes);
	nrpc_set_flush_callback(&(handle->nrpc_ctx), on_tx_flush);

	return WING_BIN_INTF_OK;

}//wing_bin_decode_init

wing_bin_err_t wing_bin_decode_link_intf(wing_bin_handle_t* handle, wing_bin_socket_intf_t intf) {
	WING_BIN_INTF_ASSERT(handle);

	handle->intf = intf;

	return WING_BIN_INTF_OK;
}//wing_bin_decode_init

wing_bin_err_t wing_bin_rx_data(wing_bin_handle_t* handle, uint8_t* rx_bytes, size_t len) {
	WING_BIN_INTF_ASSERT(handle);

	wing_bin_err_t err = 0;

	for (size_t i = 0; i < len; i++)
	{
		err |= nrpc_data_rx(&(handle->nrpc_ctx), rx_bytes[i]);
	}

	if (err) {
		elog_e(TAG, "err = %ld", err);
	}
	return err;
}//wing_bin_rx_data

void wing_bin_set_user_data(wing_bin_handle_t* handle, void* user) {
	WING_BIN_INTF_ASSERT(handle);

	handle->user = user;
}//wing_bin_set_user_data

void wing_bin_reset_rx_stm(wing_bin_handle_t* handle) {

	WING_BIN_INTF_ASSERT(handle);
	//init frame struct
	memset(&(handle->rx), 0x00, sizeof(wing_bin_frame_t));
	handle->rx_buffer_index = 0;
	memset(&(handle->decode_stm), 0x00, sizeof(handle->decode_stm));
	nrpc_reset_rx_stm(&(handle->nrpc_ctx));
}//wing_bin_reset_rx_stm
//eof