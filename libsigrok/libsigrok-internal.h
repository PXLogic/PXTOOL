/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_SIGROK_INTERNAL_H
#define LIBSIGROK_SIGROK_INTERNAL_H

//#include <stdarg.h>
#include <glib.h>
#include <ds_types.h>
#include "config.h" /* Needed for HAVE_LIBUSB_1_0 and others. */
#include <libusb-1.0/libusb.h>
#include "libsigrok.h"

/**
 * @file
 *
 * libsigrok private header file, only to be used internally.
 */

/*--- Macros ----------------------------------------------------------------*/

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef ARRAY_AND_SIZE
#define ARRAY_AND_SIZE(a) (a), ARRAY_SIZE(a)
#endif

#ifdef __clang__
#define ALL_ZERO { }
#else
#define ALL_ZERO { 0 }
#endif

static inline uint16_t read_u16le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
#define RL16(x) read_u16le((const uint8_t *)(x))

static inline int16_t read_i16le(const uint8_t *p)
{
    return (int16_t)read_u16le(p);
}
#define RL16S(x) read_i16le((const uint8_t *)(x))

static inline void write_u16le(uint8_t *p, uint16_t x)
{
    p[0] = x & 0xff;
    x >>= 8;
    p[1] = x & 0xff;
}
#define WL16(p, x) write_u16le((uint8_t *)(p), (uint16_t)(x))

static inline uint32_t read_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
#define RL32(x) read_u32le((const uint8_t *)(x))

static inline int32_t read_i32le(const uint8_t *p)
{
    return (int32_t)read_u32le(p);
}
#define RL32S(x) read_i32le((const uint8_t *)(x))

static inline void write_u32le(uint8_t *p, uint32_t x)
{
    p[0] = x & 0xff;
    x >>= 8;
    p[1] = x & 0xff;
    x >>= 8;
    p[2] = x & 0xff;
    x >>= 8;
    p[3] = x & 0xff;
}
#define WL32(p, x) write_u32le((uint8_t *)(p), (uint32_t)(x))

#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

#define USB_EV_HOTPLUG_UNKNOW		0
#define USB_EV_HOTPLUG_ATTACH		1
#define USB_EV_HOTPLUG_DETTACH		2

#define safe_free(p) 			if((p)) free((p)); ((p)) = NULL;
#define g_safe_free_list(p) 	if((p)) g_slist_free((p)); ((p)) = NULL;

#define DS_VENDOR_ID	0x2A0E

enum device_transaction_types
{
	DEV_TRANS_NONE = 0,
	DEV_TRANS_OPEN = 1
};

/** global variable */
extern char DS_RES_PATH[500];
extern struct ds_trigger *trigger;

typedef void (*hotplug_event_callback)(struct libusb_context *ctx, struct libusb_device *dev, int event);

struct sr_context {
	libusb_context*			libusb_ctx;
	libusb_hotplug_callback_handle hotplug_handle;
	int 					hotplug_registered;
	hotplug_event_callback  hotplug_callback;
	struct 					timeval hotplug_tv;
	int (*listen_hotplug_ext)(struct sr_context *ctx);
	int (*close_hotplug_ext)(struct sr_context *ctx);
	int (*hotplug_wait_timout_ext)(struct sr_context *ctx);
};

static const struct sr_dev_mode sr_mode_list[] =
{
    {LOGIC,"Logic Analyzer","la"},
    {ANALOG, "Data Acquisition", "daq"},
    {DSO, "Oscilloscope", "osc"},
};

enum sr_dev_driver_type
{
	DRIVER_TYPE_DEMO = 0,
	DRIVER_TYPE_FILE = 1,
	DRIVER_TYPE_HARDWARE = 2
};

enum ds_device_source_kind
{
	DS_DEVICE_SOURCE_UNKNOWN = 0,
	DS_DEVICE_SOURCE_NATIVE = 1,
	DS_DEVICE_SOURCE_UPSTREAM_COMPAT = 2,
	DS_DEVICE_SOURCE_FILE = 3,
	DS_DEVICE_SOURCE_DEMO = 4
};

struct lang_text_map_item{
	int  config_id;
	int  id;
	const char *en_name;
	const char *cn_name;
};

struct sr_dev_driver {
	/* Driver-specific */
	char *name;
	char *longname;
	int api_version;
	int driver_type; // enum sr_dev_driver_type
	int (*init) (struct sr_context *sr_ctx);
	int (*cleanup) (void);
	GSList *(*scan) (GSList *options);
    const GSList *(*dev_mode_list) (const struct sr_dev_inst *sdi);

    int (*config_get) (int id, GVariant **data,
                       const struct sr_dev_inst *sdi,
                       const struct sr_channel *ch,
                       const struct sr_channel_group *cg);
    int (*config_set) (int id, GVariant *data,
                       struct sr_dev_inst *sdi,
                       struct sr_channel *ch,
                       struct sr_channel_group *cg);
    int (*config_list) (int info_id, GVariant **data,
                        const struct sr_dev_inst *sdi,
                        const struct sr_channel_group *cg);

	/* Device-specific */
	int (*dev_open) (struct sr_dev_inst *sdi);
	int (*dev_close) (struct sr_dev_inst *sdi);
	int (*dev_destroy) (struct sr_dev_inst *sdi);
    int (*dev_status_get) (const struct sr_dev_inst *sdi,
                           struct sr_status *status, gboolean prg);
    int (*dev_acquisition_start) (struct sr_dev_inst *sdi,
			void *cb_data);
    int (*dev_acquisition_stop) (const struct sr_dev_inst *sdi,
			void *cb_data);

	/* Dynamic */
	void *priv;
};

struct sr_dev_inst {
    /** Device driver. */
    struct sr_dev_driver *driver;
	
	/**Identity. */
    ds_device_handle handle;

	/** device name. */
	char *name;

	char *path;

	/** Device type:(demo,filelog,hardware). The type see enum sr_device_type. */
	int dev_type;

	/** Internal source kind: native, upstream-compat, file, or demo. */
	int source_kind;

    /** Index of device in driver. */
    int index;

    /** Device instance status. SR_ST_NOT_FOUND, etc. */
    int status;

    /** Device mode. LA/DAQ/OSC, etc. */
    int mode;

    /** Device vendor. */
    char *vendor;
 
    /** Device version. */
    char *version;

    /** List of channels. */
    GSList *channels;

    /** List of sr_channel_group structs. */
    GSList *channel_groups;
 
    /** Device instance connection data (used?) */
    void *conn;

    /** Device instance private data (used?) */
    void *priv;

	int actived_times;
};

struct sr_session 
{ 
    gboolean running;

	unsigned int num_sources;

	/*
	 * Both "sources" and "pollfds" are of the same size and contain pairs
	 * of descriptor and callback function. We can not embed the GPollFD
	 * into the source struct since we want to be able to pass the array
	 * of all poll descriptors to g_poll().
	 */
	struct source *sources;
	GPollFD *pollfds;
	int source_timeout;

	/*
	 * These are our synchronization primitives for stopping the session in
	 * an async fashion. We need to make sure the session is stopped from
	 * within the session thread itself.
	 */
    GMutex stop_mutex;
	gboolean abort_session;
};

struct sr_usb_dev_inst {
	uint8_t bus;
	uint8_t address;
	struct libusb_device_handle *devhdl;
	struct libusb_device *usb_dev;
};

#define SERIAL_PARITY_NONE 0
#define SERIAL_PARITY_EVEN 1
#define SERIAL_PARITY_ODD  2
struct sr_serial_dev_inst {
	char *port;
	char *serialcomm;
	int fd;
};

/* Private driver context. */
struct drv_context {
	struct sr_context *sr_ctx;
	GSList *instances;
};


/*
 * Oscilloscope
 */
#define MAX_TIMEBASE SR_SEC(10)
#define MIN_TIMEBASE SR_NS(10)

struct ds_trigger {
    uint16_t trigger_en;
    uint16_t trigger_mode;
    uint16_t trigger_pos;
    uint16_t trigger_stages;
    unsigned char trigger_logic[TriggerStages+1];
    unsigned char trigger0_inv[TriggerStages+1];
    unsigned char trigger1_inv[TriggerStages+1];
    char trigger0[TriggerStages+1][MaxTriggerProbes];
    char trigger1[TriggerStages+1][MaxTriggerProbes];
    uint32_t trigger0_count[TriggerStages+1];
    uint32_t trigger1_count[TriggerStages+1];
};


/*--- device.c --------------------------------------------------------------*/

SR_PRIV struct sr_channel *sr_channel_new(struct sr_dev_inst *sdi,
        int index, int type, gboolean enabled, const char *name);
SR_PRIV void sr_channel_free(struct sr_channel *channel);
SR_PRIV void sr_channel_free_cb(void *channel);
SR_PRIV gboolean sr_channels_differ(struct sr_channel *ch1,
        struct sr_channel *ch2);
SR_PRIV gboolean sr_channel_lists_differ(GSList *l1, GSList *l2);
SR_PRIV struct sr_channel_group *sr_channel_group_new(struct sr_dev_inst *sdi,
        const char *name, void *priv);
SR_PRIV void sr_channel_group_free(struct sr_channel_group *cg);
SR_PRIV void sr_channel_group_free_cb(void *cg);

SR_PRIV void sr_dev_probes_free(struct sr_dev_inst *sdi);

#ifdef DSVIEW_TESTING
SR_PRIV void sr_test_channel_lifecycle_reset(void);
SR_PRIV unsigned int sr_test_channel_free_count(void);
SR_PRIV unsigned int sr_test_channel_group_free_count(void);
#endif

SR_PRIV int sr_enable_device_channel(struct sr_dev_inst *sdi, const struct sr_channel *probe, gboolean enable);

SR_PRIV int sr_dev_probe_name_set(const struct sr_dev_inst *sdi,
		int probenum, const char *name);

SR_PRIV int sr_dev_probe_enable(const struct sr_dev_inst *sdi, int probenum,
		gboolean state);
		
SR_PRIV int sr_dev_trigger_set(const struct sr_dev_inst *sdi, uint16_t probenum,
		const char *trigger);

/* Generic device instances */
SR_PRIV struct sr_dev_inst *sr_dev_inst_new(int mode, int status,
                                            const char *vendor, const char *model, const char *version);
SR_PRIV void sr_dev_inst_free(struct sr_dev_inst *sdi);

/* USB-specific instances */
SR_PRIV struct sr_usb_dev_inst *sr_usb_dev_inst_new(uint8_t bus, uint8_t address);
SR_PRIV GSList *sr_usb_find_usbtmc(libusb_context *usb_ctx);
SR_PRIV void sr_usb_dev_inst_free(struct sr_usb_dev_inst *usb);

/* Serial-specific instances */
SR_PRIV struct sr_serial_dev_inst *sr_serial_dev_inst_new(const char *port,
		const char *serialcomm);
SR_PRIV void sr_serial_dev_inst_free(struct sr_serial_dev_inst *serial);


/*--- hwdriver.c ------------------------------------------------------------*/

typedef int (*sr_receive_data_callback_t)(int fd, int revents, const struct sr_dev_inst *sdi);

SR_PRIV void sr_hw_cleanup_all(void);

/*--- session.c -------------------------------------------------------------*/
 
SR_PRIV int usb_hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                                  libusb_hotplug_event event, void *user_data);

SR_PRIV int sr_session_source_add(gintptr poll_object, int events,
                                  int timeout,
                                  sr_receive_data_callback_t cb,
                                  const struct sr_dev_inst *sdi);
SR_PRIV int sr_session_source_remove(gintptr poll_object);
SR_PRIV int sr_session_send_meta(const struct sr_dev_inst *sdi,
        uint32_t key, GVariant *var);
SR_PRIV int sr_session_send(const struct sr_dev_inst *sdi,
        const struct sr_datafeed_packet *packet);

/*--- std.c -----------------------------------------------------------------*/

typedef int (*dev_close_t)(struct sr_dev_inst *sdi);

SR_PRIV int std_hw_init(struct sr_context *sr_ctx, struct sr_dev_driver *di,
		const char *prefix);
SR_PRIV int std_session_send_df_header(const struct sr_dev_inst *sdi);
SR_PRIV int std_session_send_df_end(const struct sr_dev_inst *sdi);
SR_PRIV int std_session_send_df_trigger(const struct sr_dev_inst *sdi);
SR_PRIV int std_session_send_df_frame_begin(const struct sr_dev_inst *sdi);
SR_PRIV int std_session_send_df_frame_end(const struct sr_dev_inst *sdi);

/*--- analog.c ------------------------------------------------------------*/

SR_PRIV int sr_analog_init(struct sr_datafeed_analog *analog,
        struct sr_analog_encoding *encoding,
        struct sr_analog_meaning *meaning,
        struct sr_analog_spec *spec, int digits);

/*--- strutil.c ------------------------------------------------------------*/

SR_PRIV int sr_atod_ascii(const char *str, double *ret);
SR_PRIV int sr_atod_ascii_digits(const char *str, double *ret, int *digits);
SR_PRIV int sr_atof_ascii(const char *str, float *ret);
SR_PRIV int sr_atof_ascii_digits(const char *str, float *ret, int *digits);

/*--- input/feed_queue.c ----------------------------------------------------*/

struct feed_queue_logic;
struct feed_queue_analog;

SR_API struct feed_queue_logic *feed_queue_logic_alloc(
        const struct sr_dev_inst *sdi, size_t sample_count, size_t unit_size);
SR_API int feed_queue_logic_submit_one(struct feed_queue_logic *q,
        const uint8_t *data, size_t repeat_count);
SR_API int feed_queue_logic_submit_many(struct feed_queue_logic *q,
        const uint8_t *data, size_t samples_count);
SR_API int feed_queue_logic_flush(struct feed_queue_logic *q);
SR_API int feed_queue_logic_send_trigger(struct feed_queue_logic *q);
SR_API void feed_queue_logic_free(struct feed_queue_logic *q);

SR_API struct feed_queue_analog *feed_queue_analog_alloc(
        const struct sr_dev_inst *sdi, size_t sample_count, int digits,
        struct sr_channel *ch);
SR_API int feed_queue_analog_mq_unit(struct feed_queue_analog *q,
        enum sr_mq mq, enum sr_mqflag mq_flag, enum sr_unit unit);
SR_API int feed_queue_analog_scale_offset(struct feed_queue_analog *q,
        const struct sr_rational *scale, const struct sr_rational *offset);
SR_API int feed_queue_analog_submit_one(struct feed_queue_analog *q,
        float data, size_t repeat_count);
SR_API int feed_queue_analog_flush(struct feed_queue_analog *q);
SR_API void feed_queue_analog_free(struct feed_queue_analog *q);
SR_PRIV int sr_count_digits(const char *str, int *digits);
SR_PRIV GString *sr_hexdump_new(const uint8_t *data, size_t len);
SR_PRIV void sr_hexdump_free(GString *text);

/*--- trigger.c -------------------------------------------------*/
SR_PRIV uint64_t sr_trigger_get_mask0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_mask1(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_value0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_value1(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_edge0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_edge1(uint16_t stage);

SR_PRIV uint16_t ds_trigger_get_mask0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_mask1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);

SR_PRIV int ds_trigger_init(void);
SR_PRIV int ds_trigger_destroy(void);

/*--- hardware/common/serial.c ----------------------------------------------*/

enum {
	SERIAL_RDWR = 1,
	SERIAL_RDONLY = 2,
	SERIAL_NONBLOCK = 4,
};

typedef gboolean (*packet_valid_t)(const uint8_t *buf);

SR_PRIV int serial_open(struct sr_serial_dev_inst *serial, int flags);
SR_PRIV int serial_close(struct sr_serial_dev_inst *serial);
SR_PRIV int serial_flush(struct sr_serial_dev_inst *serial);
SR_PRIV int serial_write(struct sr_serial_dev_inst *serial,
		const void *buf, size_t count);
SR_PRIV int serial_read(struct sr_serial_dev_inst *serial, void *buf,
		size_t count);
SR_PRIV int serial_set_params(struct sr_serial_dev_inst *serial, int baudrate,
		int bits, int parity, int stopbits, int flowcontrol, int rts, int dtr);
SR_PRIV int serial_set_paramstr(struct sr_serial_dev_inst *serial,
		const char *paramstr);
SR_PRIV int serial_readline(struct sr_serial_dev_inst *serial, char **buf,
		int *buflen, gint64 timeout_ms);
SR_PRIV int serial_stream_detect(struct sr_serial_dev_inst *serial,
				 uint8_t *buf, size_t *buflen,
				 size_t packet_size, packet_valid_t is_valid,
				 uint64_t timeout_ms, int baudrate);

/*--- hardware/common/ezusb.c -----------------------------------------------*/


SR_PRIV int ezusb_reset(struct libusb_device_handle *hdl, int set_clear);
SR_PRIV int ezusb_install_firmware(libusb_device_handle *hdl,
				   const char *filename);
SR_PRIV int ezusb_upload_firmware(libusb_device *dev, int configuration,
				  const char *filename);

/*--- hardware/common/usb.c -------------------------------------------------*/

SR_PRIV GSList *sr_usb_find(libusb_context *usb_ctx, const char *conn); 

/*--- backend.c -------------------------------------------------------------*/
SR_PRIV int sr_init(struct sr_context **ctx);
SR_PRIV int sr_exit(struct sr_context *ctx); 

SR_PRIV int sr_listen_hotplug(struct sr_context *ctx, hotplug_event_callback callback);
SR_PRIV int sr_close_hotplug(struct sr_context *ctx);
SR_PRIV void sr_hotplug_wait_timout(struct sr_context *ctx);

/**---------------driver ----*/
SR_PRIV struct sr_dev_driver **sr_driver_list(void);
SR_PRIV int sr_driver_init(struct sr_context *ctx,
		struct sr_dev_driver *driver);

/*----- Session ------*/ 
SR_PRIV int sr_session_run(void);
SR_PRIV int sr_session_stop(void); 
SR_PRIV struct sr_session *sr_session_new(void);
SR_PRIV int sr_session_destroy(void);

/**
 * Create a virtual deivce from file.
 */
SR_PRIV int sr_new_virtual_device(const char *filename, struct sr_dev_inst **out_di);


/*--- lib_main.c -------------------------------------------------*/
/**
 * Check whether the USB device is in the device list.
 */
SR_PRIV int sr_usb_device_is_exists(libusb_device *usb_dev);

SR_PRIV void ds_set_last_error(int error);

/**
 * Forward the data.
 */
SR_PRIV int ds_data_forward(const struct sr_dev_inst *sdi,
						const struct sr_datafeed_packet *packet);

SR_PRIV int current_device_acquisition_stop();

SR_PRIV int lib_extern_init(struct sr_context *ctx);

SR_PRIV int post_message_callback(int msg);

SR_PRIV void xsleep(int ms);

/*--- hwdriver.c ------------------------------------------------------------*/

SR_PRIV int sr_config_get(const struct sr_dev_driver *driver,
                         const struct sr_dev_inst *sdi,
                         const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data);
SR_PRIV int sr_config_set(struct sr_dev_inst *sdi,
                         struct sr_channel *ch,
                         struct sr_channel_group *cg,
                         int key, GVariant *data);
SR_PRIV int sr_config_list(const struct sr_dev_driver *driver,
                          const struct sr_dev_inst *sdi,
                          const struct sr_channel_group *cg,
                          int key, GVariant **data);
SR_PRIV const struct sr_config_info *sr_config_info_get(int key);
SR_PRIV int sr_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg);
SR_PRIV struct sr_config *sr_config_new(int key, GVariant *data);
SR_PRIV void sr_config_free(struct sr_config *src);
SR_PRIV int ds_scan_all_device_list(libusb_context *usb_ctx, struct libusb_device **list_buf, int size, int *count);

/*--- dsl.c ------------------------------------------------------------*/
SR_PRIV int sr_option_value_to_code(int config_id, const char *value, const struct lang_text_map_item *array, int num);

/*--- dslogic.c ------------------------------------------------------------*/
SR_PRIV int sr_dslogic_option_value_to_code(const struct sr_dev_inst *sdi, int config_id, const char *value);

/*--- dscope.c ------------------------------------------------------------*/
SR_PRIV int sr_dscope_option_value_to_code(const struct sr_dev_inst *sdi, int config_id, const char *value);
 
#endif
