#pragma once

#include <stdint.h>

#include "nrpc.h"

#define WING_BIN_INTF_RX_BUFFER_SIZE (1024)
#define WING_BIN_INTF_TX_BUFFER_SIZE (1024)

typedef enum
{
	WING_BIN_INTF_OK = 0,
	WING_BIN_INTF_ERR = 1,
}wing_bin_err_t;

typedef enum
{
	WING_BIN_FRAME_TYPE_BOOL = 0,
	WING_BIN_FRAME_TYPE_INT_IN_TOKEN,
	WING_BIN_FRAME_TYPE_NODE_INDEX_IN_TOKEN,
	WING_BIN_FRAME_TYPE_NODE_INDEX,
	WING_BIN_FRAME_TYPE_STRING,//string in token , string in payload , empty string
	WING_BIN_FRAME_TYPE_NODE_NAME,
	WING_BIN_FRAME_TYPE_INT16,
	WING_BIN_FRAME_TYPE_INT32,
	WING_BIN_FRAME_TYPE_FLOAT32,
	WING_BIN_FRAME_TYPE_NDR,

	WING_BIN_FRAME_TYPE_UNKNOWN,

	WING_BIN_FRAME_TYPE_MAX,
}wing_bin_decoder_frame_type_t;

/*==============================interface==============================*/

typedef struct
{
	uint16_t len;
	const uint8_t* txt;
}wing_bin_decoded_string_t;

typedef struct
{
	uint32_t parent;
	uint32_t hash;
	uint16_t index;
	uint8_t name_len;
	const uint8_t* name;
	uint8_t longname_len;
	const uint8_t* longname;
	union
	{
		uint16_t flag;
		struct NDR_flag_bits_t {
			uint8_t unit : 4;
			uint8_t type : 4;
			uint8_t : 1;
			uint8_t is_readonly : 1;
			uint8_t : 6;
		}flag_bits;
	};

}wing_bin_decoder_node_definition_response_t;

typedef struct
{
	uint8_t token;
	wing_bin_decoder_frame_type_t type;
	uint32_t hash;
	union {
		wing_bin_decoder_node_definition_response_t ndr;
		wing_bin_decoded_string_t string;

		uint8_t boolen;
		uint16_t node_index;
		int32_t intx;
		float f32;
	};

}wing_bin_decoded_frame_t;

typedef struct
{
	uint8_t token;
	int32_t payload_len;//for frame decode
	uint8_t buffer[WING_BIN_INTF_RX_BUFFER_SIZE];

	wing_bin_decoded_frame_t decoded;

}wing_bin_frame_t;

/*==============================ext interface==============================*/

//typedef wing_bin_err_t(*wing_tx_callback_event_t)(uint8_t* tx_bytes, size_t len, void* user);
typedef int (*wing_rx_callback_event_t)(wing_bin_decoded_frame_t* decoded, void* user);

typedef struct
{
	//wing_tx_callback_event_t tx_evt;
	wing_rx_callback_event_t frame_done;
}wing_bin_socket_intf_t;

/*==============================handle==============================*/

typedef struct
{
	wing_bin_socket_intf_t intf;
	NRPCContext_t nrpc_ctx;

	wing_bin_frame_t rx;//for frame decode
	size_t rx_buffer_index;

	uint8_t tx_buffer[WING_BIN_INTF_TX_BUFFER_SIZE];
	size_t tx_buffer_index;

	struct wing_bin_decode_stm_t {
		uint8_t state;
		int32_t token_type;
		int32_t token_payload_length;//for frame recv

	}decode_stm;

	void* user;

}wing_bin_handle_t;

wing_bin_err_t wing_bin_decode_init(wing_bin_handle_t* handle);
wing_bin_err_t wing_bin_decode_link_intf(wing_bin_handle_t* handle, wing_bin_socket_intf_t intf);
wing_bin_err_t wing_bin_rx_data(wing_bin_handle_t* handle, uint8_t* rx_bytes, size_t len);

void wing_bin_set_user_data(wing_bin_handle_t* handle, void* user);

void wing_bin_reset_rx_stm(wing_bin_handle_t* handle);
//eof