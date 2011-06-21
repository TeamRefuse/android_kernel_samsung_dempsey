/*
 * include/linux/mfd/cg2900_audio.h
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Authors:
 * Par-Gunnar Hjalmdahl (par-gunnar.p.hjalmdahl@stericsson.com) for ST-Ericsson.
 * Kjell Andersson (kjell.k.andersson@stericsson.com) for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Linux Bluetooth Audio Driver for ST-Ericsson controller.
 */

#ifndef _CG2900_AUDIO_H_
#define _CG2900_AUDIO_H_

#include <linux/types.h>

/*
 * Digital Audio Interface configuration types
 */

/** CG2900_A2DP_MAX_AVDTP_HDR_LEN - Max length of a AVDTP header.
 * Max length of a AVDTP header for an A2DP packet.
 */
#define CG2900_A2DP_MAX_AVDTP_HDR_LEN	25

/**
 * enum cg2900_dai_dir - Contains the DAI port directions alternatives.
 * @DAI_DIR_B_RX_A_TX: Port B as Rx and port A as Tx.
 * @DAI_DIR_B_TX_A_RX: Port B as Tx and port A as Rx.
 */
enum cg2900_dai_dir {
	DAI_DIR_B_RX_A_TX = 0x00,
	DAI_DIR_B_TX_A_RX = 0x01
};

/**
 * enum cg2900_dai_mode - DAI mode alternatives.
 * @DAI_MODE_SLAVE: Slave.
 * @DAI_MODE_MASTER: Master.
 */
enum cg2900_dai_mode {
	DAI_MODE_SLAVE = 0x00,
	DAI_MODE_MASTER = 0x01
};

/**
 * enum cg2900_dai_stream_ratio - Voice stream ratio alternatives.
 * @STREAM_RATIO_FM16_VOICE16:	FM 16kHz, Voice 16kHz.
 * @STREAM_RATIO_FM16_VOICE8:	FM 16kHz, Voice 8kHz.
 * @STREAM_RATIO_FM48_VOICE16:	FM 48kHz, Voice 16Khz.
 * @STREAM_RATIO_FM48_VOICE8:	FM 48kHz, Voice 8kHz.
 *
 * Contains the alternatives for the voice stream ratio between the Audio stream
 * sample rate and the Voice stream sample rate.
 */
enum cg2900_dai_stream_ratio {
	STREAM_RATIO_FM16_VOICE16 = 0x01,
	STREAM_RATIO_FM16_VOICE8 = 0x02,
	STREAM_RATIO_FM48_VOICE16 = 0x03,
	STREAM_RATIO_FM48_VOICE8 = 0x06
};

/**
 * enum cg2900_dai_fs_duration - Frame sync duration alternatives.
 * @SYNC_DURATION_8: 8 frames sync duration.
 * @SYNC_DURATION_16: 16 frames sync duration.
 * @SYNC_DURATION_24: 24 frames sync duration.
 * @SYNC_DURATION_32: 32 frames sync duration.
 * @SYNC_DURATION_48: 48 frames sync duration.
 * @SYNC_DURATION_50: 50 frames sync duration.
 * @SYNC_DURATION_64: 64 frames sync duration.
 * @SYNC_DURATION_75: 75 frames sync duration.
 * @SYNC_DURATION_96: 96 frames sync duration.
 * @SYNC_DURATION_125: 125 frames sync duration.
 * @SYNC_DURATION_128: 128 frames sync duration.
 * @SYNC_DURATION_150: 150 frames sync duration.
 * @SYNC_DURATION_192: 192 frames sync duration.
 * @SYNC_DURATION_250: 250 frames sync duration.
 * @SYNC_DURATION_256: 256 frames sync duration.
 * @SYNC_DURATION_300: 300 frames sync duration.
 * @SYNC_DURATION_384: 384 frames sync duration.
 * @SYNC_DURATION_500: 500 frames sync duration.
 * @SYNC_DURATION_512: 512 frames sync duration.
 * @SYNC_DURATION_600: 600 frames sync duration.
 * @SYNC_DURATION_768: 768 frames sync duration.
 *
 * This parameter sets the PCM frame sync duration. It is calculated as the
 * ratio between the bit clock and the frame rate. For example, if the bit
 * clock is 512 kHz and the stream sample rate is 8 kHz, the PCM frame sync
 * duration is 512 / 8 = 64.
 */
enum cg2900_dai_fs_duration {
	SYNC_DURATION_8 = 8,
	SYNC_DURATION_16 = 16,
	SYNC_DURATION_24 = 24,
	SYNC_DURATION_32 = 32,
	SYNC_DURATION_48 = 48,
	SYNC_DURATION_50 = 50,
	SYNC_DURATION_64 = 64,
	SYNC_DURATION_75 = 75,
	SYNC_DURATION_96 = 96,
	SYNC_DURATION_125 = 125,
	SYNC_DURATION_128 = 128,
	SYNC_DURATION_150 = 150,
	SYNC_DURATION_192 = 192,
	SYNC_DURATION_250 = 250,
	SYNC_DURATION_256 = 256,
	SYNC_DURATION_300 = 300,
	SYNC_DURATION_384 = 384,
	SYNC_DURATION_500 = 500,
	SYNC_DURATION_512 = 512,
	SYNC_DURATION_600 = 600,
	SYNC_DURATION_768 = 768
};

/**
 * enum cg2900_dai_bit_clk - Bit Clock alternatives.
 * @BIT_CLK_128:	128 Kbits clock.
 * @BIT_CLK_256:	256 Kbits clock.
 * @BIT_CLK_512:	512 Kbits clock.
 * @BIT_CLK_768:	768 Kbits clock.
 * @BIT_CLK_1024:	1024 Kbits clock.
 * @BIT_CLK_1411_76:	1411.76 Kbits clock.
 * @BIT_CLK_1536:	1536 Kbits clock.
 * @BIT_CLK_2000:	2000 Kbits clock.
 * @BIT_CLK_2048:	2048 Kbits clock.
 * @BIT_CLK_2400:	2400 Kbits clock.
 * @BIT_CLK_2823_52:	2823.52 Kbits clock.
 * @BIT_CLK_3072:	3072 Kbits clock.
 *
 *  This parameter sets the bit clock speed. This is the clocking of the actual
 *  data. A usual parameter for eSCO voice is 512 kHz.
 */
enum cg2900_dai_bit_clk {
	BIT_CLK_128 = 0x00,
	BIT_CLK_256 = 0x01,
	BIT_CLK_512 = 0x02,
	BIT_CLK_768 = 0x03,
	BIT_CLK_1024 = 0x04,
	BIT_CLK_1411_76 = 0x05,
	BIT_CLK_1536 = 0x06,
	BIT_CLK_2000 = 0x07,
	BIT_CLK_2048 = 0x08,
	BIT_CLK_2400 = 0x09,
	BIT_CLK_2823_52 = 0x0A,
	BIT_CLK_3072 = 0x0B
};

/**
 * enum cg2900_dai_sample_rate - Sample rates alternatives.
 * @SAMPLE_RATE_8:	8 kHz sample rate.
 * @SAMPLE_RATE_16:	16 kHz sample rate.
 * @SAMPLE_RATE_44_1:	44.1 kHz sample rate.
 * @SAMPLE_RATE_48:	48 kHz sample rate.
 */
enum cg2900_dai_sample_rate {
	SAMPLE_RATE_8 = 0x00,
	SAMPLE_RATE_16 = 0x01,
	SAMPLE_RATE_44_1 = 0x02,
	SAMPLE_RATE_48 = 0x04
};

/**
 * enum cg2900_dai_port_protocol - Port protocol alternatives.
 * @PORT_PROTOCOL_PCM: Protocol PCM.
 * @PORT_PROTOCOL_I2S: Protocol I2S.
 */
enum cg2900_dai_port_protocol {
	PORT_PROTOCOL_PCM = 0x00,
	PORT_PROTOCOL_I2S = 0x01
};

/**
 * enum cg2900_dai_channel_sel - The channel selection alternatives.
 * @CHANNEL_SELECTION_RIGHT: Right channel used.
 * @CHANNEL_SELECTION_LEFT: Left channel used.
 * @CHANNEL_SELECTION_BOTH: Both channels used.
 */
enum cg2900_dai_channel_sel {
	CHANNEL_SELECTION_RIGHT = 0x00,
	CHANNEL_SELECTION_LEFT = 0x01,
	CHANNEL_SELECTION_BOTH = 0x02
};

/**
 * struct cg2900_dai_conf_i2s_pcm - Port configuration structure.
 * @mode:		Operational mode of the port configured.
 * @i2s_channel_sel:	I2S channels used. Only valid if used in I2S mode.
 * @slot_0_used:	True if SCO slot 0 is used.
 * @slot_1_used:	True if SCO slot 1 is used.
 * @slot_2_used:	True if SCO slot 2 is used.
 * @slot_3_used:	True if SCO slot 3 is used.
 * @slot_0_dir:		Direction of slot 0.
 * @slot_1_dir:		Direction of slot 1.
 * @slot_2_dir:		Direction of slot 2.
 * @slot_3_dir:		Direction of slot 3.
 * @slot_0_start:	Slot 0 start (relative to the PCM frame sync).
 * @slot_1_start:	Slot 1 start (relative to the PCM frame sync)
 * @slot_2_start:	Slot 2 start (relative to the PCM frame sync)
 * @slot_3_start:	Slot 3 start (relative to the PCM frame sync)
 * @ratio:		Voice stream ratio between the Audio stream sample rate
 *			and the Voice stream sample rate.
 * @protocol:		Protocol used on port.
 * @duration:		Frame sync duration.
 * @clk:		Bit clock.
 * @sample_rate:	Sample rate.
 */
struct cg2900_dai_conf_i2s_pcm {
	enum cg2900_dai_mode mode;
	enum cg2900_dai_channel_sel i2s_channel_sel;
	bool slot_0_used;
	bool slot_1_used;
	bool slot_2_used;
	bool slot_3_used;
	enum cg2900_dai_dir slot_0_dir;
	enum cg2900_dai_dir slot_1_dir;
	enum cg2900_dai_dir slot_2_dir;
	enum cg2900_dai_dir slot_3_dir;
	__u8 slot_0_start;
	__u8 slot_1_start;
	__u8 slot_2_start;
	__u8 slot_3_start;
	enum cg2900_dai_stream_ratio ratio;
	enum cg2900_dai_port_protocol protocol;
	enum cg2900_dai_fs_duration duration;
	enum cg2900_dai_bit_clk clk;
	enum cg2900_dai_sample_rate sample_rate;
};

/**
 * enum cg2900_dai_half_period - Half period duration alternatives.
 * @HALF_PER_DUR_8:	8 Bits.
 * @HALF_PER_DUR_16:	16 Bits.
 * @HALF_PER_DUR_24:	24 Bits.
 * @HALF_PER_DUR_25:	25 Bits.
 * @HALF_PER_DUR_32:	32 Bits.
 * @HALF_PER_DUR_48:	48 Bits.
 * @HALF_PER_DUR_64:	64 Bits.
 * @HALF_PER_DUR_75:	75 Bits.
 * @HALF_PER_DUR_96:	96 Bits.
 * @HALF_PER_DUR_128:	128 Bits.
 * @HALF_PER_DUR_150:	150 Bits.
 * @HALF_PER_DUR_192:	192 Bits.
 *
 * This parameter sets the number of bits contained in each I2S half period,
 * i.e. each channel slot. A usual value is 16 bits.
 */
enum cg2900_dai_half_period {
	HALF_PER_DUR_8 = 0x00,
	HALF_PER_DUR_16 = 0x01,
	HALF_PER_DUR_24 = 0x02,
	HALF_PER_DUR_25 = 0x03,
	HALF_PER_DUR_32 = 0x04,
	HALF_PER_DUR_48 = 0x05,
	HALF_PER_DUR_64 = 0x06,
	HALF_PER_DUR_75 = 0x07,
	HALF_PER_DUR_96 = 0x08,
	HALF_PER_DUR_128 = 0x09,
	HALF_PER_DUR_150 = 0x0A,
	HALF_PER_DUR_192 = 0x0B
};

/**
 * enum cg2900_dai_word_width - Word width alternatives.
 * @WORD_WIDTH_16: 16 bits words.
 * @WORD_WIDTH_32: 32 bits words.
 */
enum cg2900_dai_word_width {
	WORD_WIDTH_16 = 0x00,
	WORD_WIDTH_32 = 0x01
};

/**
 * struct cg2900_dai_conf_i2s - Port configuration struct for I2S.
 * @mode:		Operational mode of the port.
 * @half_period:	Half period duration.
 * @channel_sel:	Channel selection.
 * @sample_rate:	Sample rate.
 * @word_width:		Word width.
 */
struct cg2900_dai_conf_i2s {
	enum cg2900_dai_mode			mode;
	enum cg2900_dai_half_period		half_period;
	enum cg2900_dai_channel_sel		channel_sel;
	enum cg2900_dai_sample_rate		sample_rate;
	enum cg2900_dai_word_width		word_width;
};

/**
 * union cg2900_dai_port_conf - DAI port configuration union.
 * @i2s: The configuration struct for a port supporting only I2S.
 * @i2s_pcm: The configuration struct for a port supporting both PCM and I2S.
 */
union cg2900_dai_port_conf {
	struct cg2900_dai_conf_i2s i2s;
	struct cg2900_dai_conf_i2s_pcm i2s_pcm;
};

/**
 * enum cg2900_dai_ext_port_id - DAI external port id alternatives.
 * @PORT_0_I2S: Port id is 0 and it supports only I2S.
 * @PORT_1_I2S_PCM: Port id is 1 and it supports both I2S and PCM.
 */
enum cg2900_dai_ext_port_id {
	PORT_0_I2S,
	PORT_1_I2S_PCM
};

/**
 * enum cg2900_audio_endpoint_id - Audio endpoint id alternatives.
 * @ENDPOINT_PORT_0_I2S:	Internal audio endpoint of the external I2S
 *				interface.
 * @ENDPOINT_PORT_1_I2S_PCM:	Internal audio endpoint of the external I2S/PCM
 *				interface.
 * @ENDPOINT_SLIMBUS_VOICE:	Internal audio endpoint of the external Slimbus
 *				voice interface. (Currently not supported)
 * @ENDPOINT_SLIMBUS_AUDIO:	Internal audio endpoint of the external Slimbus
 *				audio interface. (Currently not supported)
 * @ENDPOINT_BT_SCO_INOUT:	Bluetooth SCO bidirectional.
 * @ENDPOINT_BT_A2DP_SRC:	Bluetooth A2DP source.
 * @ENDPOINT_BT_A2DP_SNK:	Bluetooth A2DP sink.
 * @ENDPOINT_FM_RX:		FM receive.
 * @ENDPOINT_FM_TX:		FM transmit.
 * @ENDPOINT_ANALOG_OUT:	Analog out.
 * @ENDPOINT_DSP_AUDIO_IN:	DSP audio in.
 * @ENDPOINT_DSP_AUDIO_OUT:	DSP audio out.
 * @ENDPOINT_DSP_VOICE_IN:	DSP voice in.
 * @ENDPOINT_DSP_VOICE_OUT:	DSP voice out.
 * @ENDPOINT_DSP_TONE_IN:	DSP tone in.
 * @ENDPOINT_BURST_BUFFER_IN:	Burst buffer in.
 * @ENDPOINT_BURST_BUFFER_OUT:	Burst buffer out.
 * @ENDPOINT_MUSIC_DECODER:	Music decoder.
 * @ENDPOINT_HCI_AUDIO_IN:	HCI audio in.
 */
enum cg2900_audio_endpoint_id {
	ENDPOINT_PORT_0_I2S,
	ENDPOINT_PORT_1_I2S_PCM,
	ENDPOINT_SLIMBUS_VOICE,
	ENDPOINT_SLIMBUS_AUDIO,
	ENDPOINT_BT_SCO_INOUT,
	ENDPOINT_BT_A2DP_SRC,
	ENDPOINT_BT_A2DP_SNK,
	ENDPOINT_FM_RX,
	ENDPOINT_FM_TX,
	ENDPOINT_ANALOG_OUT,
	ENDPOINT_DSP_AUDIO_IN,
	ENDPOINT_DSP_AUDIO_OUT,
	ENDPOINT_DSP_VOICE_IN,
	ENDPOINT_DSP_VOICE_OUT,
	ENDPOINT_DSP_TONE_IN,
	ENDPOINT_BURST_BUFFER_IN,
	ENDPOINT_BURST_BUFFER_OUT,
	ENDPOINT_MUSIC_DECODER,
	ENDPOINT_HCI_AUDIO_IN
};

/**
 * struct cg2900_dai_config - Configuration struct for Digital Audio Interface.
 * @port: The port id to configure. Acts as a discriminator for @conf parameter
 *	  which is a union.
 * @conf: The configuration union that contains the parameters for the port.
 */
struct cg2900_dai_config {
	enum cg2900_dai_ext_port_id	port;
	union cg2900_dai_port_conf	conf;
};

/*
 * Endpoint configuration types
 */

/**
 * enum cg2900_endpoint_sample_rate - Audio endpoint configuration sample rate alternatives.
 * @ENDPOINT_SAMPLE_RATE_8_KHZ: 8 kHz sample rate.
 * @ENDPOINT_SAMPLE_RATE_16_KHZ: 16 kHz sample rate.
 * @ENDPOINT_SAMPLE_RATE_44_1_KHZ: 44.1 kHz sample rate.
 * @ENDPOINT_SAMPLE_RATE_48_KHZ: 48 kHz sample rate.
 */
enum cg2900_endpoint_sample_rate {
	ENDPOINT_SAMPLE_RATE_8_KHZ = 0x01,
	ENDPOINT_SAMPLE_RATE_16_KHZ = 0x02,
	ENDPOINT_SAMPLE_RATE_44_1_KHZ = 0x04,
	ENDPOINT_SAMPLE_RATE_48_KHZ = 0x05
};


/**
 * struct cg2900_endpoint_config_a2dp_src - A2DP source audio endpoint configurations.
 * @sample_rate: Sample rate.
 * @channel_count: Number of channels.
 */
struct cg2900_endpoint_config_a2dp_src {
	enum cg2900_endpoint_sample_rate	sample_rate;
	unsigned int				channel_count;
};

/**
 * struct cg2900_endpoint_config_fm - Configuration parameters for an FM endpoint.
 * @sample_rate: The sample rate alternatives for the FM audio endpoints.
 */
struct cg2900_endpoint_config_fm {
	enum cg2900_endpoint_sample_rate	sample_rate;
};


/**
 * struct cg2900_endpoint_config_sco_in_out - SCO audio endpoint configuration structure.
 * @sample_rate: Sample rate, valid values are
 *		 * ENDPOINT_SAMPLE_RATE_8_KHZ
 *		 * ENDPOINT_SAMPLE_RATE_16_KHZ.
 */
struct cg2900_endpoint_config_sco_in_out {
	enum cg2900_endpoint_sample_rate	sample_rate;
};

/**
 * union cg2900_endpoint_config - Different audio endpoint configurations.
 * @sco:	SCO audio endpoint configuration structure.
 * @a2dp_src:	A2DP source audio endpoint configuration structure.
 * @fm:		FM audio endpoint configuration structure.
 */
union cg2900_endpoint_config_union {
	struct cg2900_endpoint_config_sco_in_out	sco;
	struct cg2900_endpoint_config_a2dp_src		a2dp_src;
	struct cg2900_endpoint_config_fm		fm;
};

/**
 * struct cg2900_endpoint_config - Audio endpoint configuration.
 * @endpoint_id:	Identifies the audio endpoint. Works as a discriminator
 *			for the config union.
 * @config:		Union holding the configuration parameters for
 *			the endpoint.
 */
struct cg2900_endpoint_config {
	enum cg2900_audio_endpoint_id		endpoint_id;
	union cg2900_endpoint_config_union	config;
};

/*
 * ST-Ericsson CG2900 audio control driver interfaces methods
 */

/**
 * cg2900_audio_open() - Opens a session to the ST-Ericsson CG2900 Audio control interface.
 * @session:	[out] Address where to store the session identifier.
 *		Allocated by caller, must not be NULL.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -ENOMEM upon allocation failure.
 *   -EMFILE if no more user session could be opened.
 *   -EIO upon failure to register to CG2900.
 */
int cg2900_audio_open(unsigned int *session);

/**
 * cg2900_audio_close() - Closes an opened session to the ST-Ericsson CG2900 audio control interface.
 * @session:	[in_out] Pointer to session identifier to close.
 *		Will be 0 after this call.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -EIO if driver has not been opened.
 *   -EACCES if session has not opened.
 */
int cg2900_audio_close(unsigned int *session);

/**
 * cg2900_audio_set_dai_config() -  Sets the Digital Audio Interface configuration.
 * @session:	Session identifier this call is related to.
 * @config:	Pointer to the configuration to set.
 *		Allocated by caller, must not be NULL.
 *
 * Sets the Digital Audio Interface (DAI) configuration. The DAI is the external
 * interface between the combo chip and the platform.
 * For example the PCM or I2S interface.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -EIO if driver has not been opened.
 *   -ENOMEM upon allocation failure.
 *   -EACCES if trying to set unsupported configuration.
 *   Errors from @receive_bt_cmd_complete.
 */
int cg2900_audio_set_dai_config(unsigned int session,
				struct cg2900_dai_config *config);

/**
 * cg2900_audio_get_dai_config() - Gets the current Digital Audio Interface configuration.
 * @session:	Session identifier this call is related to.
 * @config:	[out] Pointer to the configuration to get.
 *		Allocated by caller, must not be NULL.
 *
 * Gets the current Digital Audio Interface configuration. Currently this method
 * can only be called after some one has called
 * cg2900_audio_set_dai_config(), there is today no way of getting
 * the static settings file parameters from this method.
 * Note that the @port parameter within @config must be set when calling this
 * function so that the ST-Ericsson CG2900 Audio driver will know which
 * configuration to return.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -EIO if driver has not been opened or configuration has not been set.
 */
int cg2900_audio_get_dai_config(unsigned int session,
				struct cg2900_dai_config *config);

/**
 * cg2900_audio_config_endpoint() - Configures one endpoint in the combo chip's audio system.
 * @session:	Session identifier this call is related to.
 * @config:	Pointer to the endpoint's configuration structure.
 *
 * Configures one endpoint in the combo chip's audio system.
 * Supported @endpoint_id values are:
 *  * ENDPOINT_BT_SCO_INOUT
 *  * ENDPOINT_BT_A2DP_SRC
 *  * ENDPOINT_FM_RX
 *  * ENDPOINT_FM_TX
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -EIO if driver has not been opened.
 *   -EACCES if supplied cg2900_dai_config struct contains not supported
 *   endpoint_id.
 */
int cg2900_audio_config_endpoint(unsigned int session,
				 struct cg2900_endpoint_config *config);

/**
 * cg2900_audio_start_stream() - Connects two endpoints and starts the audio stream.
 * @session:		Session identifier this call is related to.
 * @ep_1:		One of the endpoints, no relation to direction or role.
 * @ep_2:		The other endpoint, no relation to direction or role.
 * @stream_handle:	Pointer where to store the stream handle.
 *			Allocated by caller, must not be NULL.
 *
 * Connects two endpoints and starts the audio stream.
 * Note that the endpoints need to be configured before the stream is started;
 * DAI endpoints, such as ENDPOINT_PORT_0_I2S, are
 * configured through @cg2900_audio_set_dai_config() while other
 * endpoints are configured through @cg2900_audio_config_endpoint().
 *
 * Supported @endpoint_id values are:
 *  * ENDPOINT_PORT_0_I2S
 *  * ENDPOINT_PORT_1_I2S_PCM
 *  * ENDPOINT_BT_SCO_INOUT
 *  * ENDPOINT_FM_RX
 *  * ENDPOINT_FM_TX
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter or unsupported configuration.
 *   -EIO if driver has not been opened.
 *   Errors from @conn_start_i2s_to_fm_rx, @conn_start_i2s_to_fm_tx, and
 *   @conn_start_pcm_to_sco.
 */
int cg2900_audio_start_stream(unsigned int session,
			      enum cg2900_audio_endpoint_id ep_1,
			      enum cg2900_audio_endpoint_id ep_2,
			      unsigned int *stream_handle);

/**
 * cg2900_audio_stop_stream() - Stops a stream and disconnects the endpoints.
 * @session:		Session identifier this call is related to.
 * @stream_handle:	Handle to the stream to stop.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL upon bad input parameter.
 *   -EIO if driver has not been opened.
 */
int cg2900_audio_stop_stream(unsigned int session,
			     unsigned int stream_handle);

#endif /* _CG2900_AUDIO_H_ */
