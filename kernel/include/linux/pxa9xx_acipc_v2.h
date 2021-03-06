/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef _PXA9XX_ACIPC_V2_H_
#define _PXA9XX_ACIPC_V2_H_

#define API_ALIGNMENT

/* user level ioctl commands for accessing APIs */
#define ACIPC_SET_EVENT		0
#define ACIPC_GET_EVENT		1
#define ACIPC_SEND_DATA		2
#define ACIPC_READ_DATA		3
#define ACIPC_BIND_EVENT	4
#define ACIPC_UNBIND_EVENT	5
#define ACIPC_GET_BIND_EVENT_ARG	6

#define ACIPC_NUMBER_OF_EVENTS (10)
#define ACIPC_NUMBER_OF_INTERRUPTS (3)
#define ACIPC_INT0_EVENTS (0xff)

#define MAX_ACIPC_REACT_TIME 32500 /* 10msec in 3.25MHz clocl cycles */

#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA988) \
	|| defined(CONFIG_CPU_PXA1088)
#define ACIPC_INT1_EVENTS (ACIPC_SHM_PACKET_NOTIFY)
#define ACIPC_INT2_EVENTS (ACIPC_IPM)
#elif defined(CONFIG_PXA95x) || defined(CONFIG_PXA93x) \
	|| defined(CONFIG_CPU_PXA1986)
#define ACIPC_INT1_EVENTS (ACIPC_DDR_260_READY_REQ)
#define ACIPC_INT2_EVENTS (ACIPC_DDR_READY_REQ)
#endif

enum acipc_dev_type {
	ACSIPC_DEV, /*APPS-COMM-SEAGULL-IPC*/
#ifdef CONFIG_PXA9XX_ACMIPC_V2
	ACMIPC_DEV,/*APPS-COMM-MSA-IPC*/
#endif
	ACIPC_NUMBER_OF_DEVICES
};


/*
 * clients callback type
 */
#if defined(CONFIG_CPU_PXA1986) || defined(CONFIG_PXA95x)
typedef u32(*acipc_rec_event_callback) (u32 events_status,
					enum acipc_dev_type idx);
#else
typedef u32(*acipc_rec_event_callback) (u32 events_status);
#endif

typedef u32 acipc_data;

enum ddr_mode {
	DDR_NOREQ = 0,
	DDR_208MHZ = 0x1,
	DDR_260MHZ = 0x2,
};

struct ddr_status {
	int mode;
	int needed_modes;
};

/*
 * this enum define the event type
 */
enum acipc_events {
	/* pxa930 specific */
	ACIPC_DDR_RELQ_REQ = 0x00000001,
	ACIPC_DDR_RELQ_ACK = 0x00000001,
	ACIPC_DDR_260_RELQ_REQ = 0x00000002,
	ACIPC_DDR_260_RELQ_ACK = 0x00000002,
	ACIPC_MSL_SLEEP_ALLOW = 0x00000004,
	ACIPC_MSL_WAKEUP_ACK = 0x00000008,
	ACIPC_MSL_WAKEUP_REQ = 0x00000010,
	ACIPC_DATA_Q_ADRS = 0x00000020,
	ACIPC_DATA_IND = 0x00000040,
	ACIPC_SPARE_1 = 0x00000080,
	ACIPC_DDR_260_READY_REQ = 0x00000100,
	ACIPC_DDR_260_READY_ACK = 0x00000100,
	ACIPC_DDR_READY_REQ = 0x00000200,
	ACIPC_DDR_READY_ACK = 0x00000200,

#if defined(CONFIG_CPU_PXA910) || defined(CONFIG_CPU_PXA1986)
	ACIPC_SPARE = 0x00000000,
	ACIPC_RINGBUF_TX_STOP = 0x00000001,
	ACIPC_RINGBUF_TX_RESUME = 0x00000002,
	ACIPC_PORT_FLOWCONTROL = 0x00000004,
	ACIPC_SHM_PACKET_NOTIFY = 0x00000100,
	ACIPC_IPM = 0x00000200,
#elif defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)

	/* pxa988 specific */
	ACIPC_SPARE = 0x00000000,
	ACIPC_RINGBUF_TX_STOP = 0x00000001,
	ACIPC_RINGBUF_TX_RESUME = 0x00000002,
	ACIPC_PORT_FLOWCONTROL = 0x00000004,
	ACIPC_MODEM_DDR_UPDATE_REQ = 0x00000008,
	ACIPC_RINGBUF_PSD_TX_STOP = 0x00000010,
	ACIPC_RINGBUF_PSD_TX_RESUME = 0x00000020,
	ACIPC_SHM_PSD_PACKET_NOTIFY = 0x00000040,
	ACIPC_SHM_DIAG_PACKET_NOTIFY = 0x00000080,
	ACIPC_SHM_PACKET_NOTIFY = 0x00000100,
	ACIPC_IPM = 0x00000200,
#endif

#ifdef CONFIG_CPU_PXA978
	/* pxa978 TD specific */
	ACIPC_SPARE = 0x00000000,
	ACIPC_RINGBUF_TX_STOP = 0x00000004,
	ACIPC_RINGBUF_TX_RESUME = 0x00000008,
	ACIPC_PORT_FLOWCONTROL = 0x000000010,
	ACIPC_SHM_PACKET_NOTIFY = 0x00000020,
	ACIPC_SHM_PEER_SYNC = 0x00000040,
#endif
}; /* ACIPC_EventsE */

enum acipc_return_code {
	ACIPC_RC_OK = 0,
	ACIPC_HISTORICAL_EVENT_OCCUR,
	ACIPC_EVENT_ALREADY_BIND,
	ACIPC_RC_FAILURE,
	ACIPC_RC_API_FAILURE,
	ACIPC_RC_WRONG_PARAM
}; /* ACIPC_ReturnCodeE */

/*
 * used by clients when binding a callback to an event
 */
enum acipc_callback_mode {
	/* callback will be called only if the DDR available */
	ACIPC_CB_NORMAL = 0,
	/* called always ,even if the DDR is not available */
	ACIPC_CB_ALWAYS_NO_DDR
}; /* ACIPC_CBModeE */

struct acipc_database_cell {
	enum acipc_events iir_bit;
	enum acipc_callback_mode mode;
	/*
	 * add to support multiple events binding
	 * see ACIPCEventBind for more details
	 */
	u32 mask;
	acipc_rec_event_callback cb;
};

struct acipc_database {
	struct acipc_database_cell event_db[ACIPC_NUMBER_OF_EVENTS];
	enum acipc_callback_mode driver_mode;
	u32 int0_events_cnt;
	/*
	 * hold status of events that occur
	 * before the clients bind their callback
	 */
	u32 historical_event_status;
};

#ifdef ACIPC_DEBUG
#define IPCTRACE(format, args...) \
	pr_info(format, ## args)
#define	IPC_ENTER()	\
	pr_info("IPC: ENTER %s\n", __func__)
#define	IPC_LEAVE()	\
	pr_info("IPC: LEAVE %s\n", __func__)
#else
#define IPCTRACE(s...)	do {} while (0)
#define	IPC_ENTER()	do {} while (0)
#define	IPC_LEAVE()	do {} while (0)
#endif

u32 set_ddr_avail_flag(enum acipc_dev_type type);

struct acipc_ioctl_arg {
	u32 arg;
	enum acipc_events set_event;
	enum acipc_callback_mode cb_mode;
	enum acipc_dev_type idx;
};
extern int set_acipc_cp_enable(enum acipc_dev_type type, unsigned int pm_cp);

#if defined(CONFIG_CPU_PXA1986) || defined(CONFIG_PXA95x)
/*NEW API*/
extern enum acipc_return_code ACIPCEventBindExt(enum acipc_dev_type type,
		u32 user_event, acipc_rec_event_callback cb,
		enum acipc_callback_mode cb_mode, u32 *historical_event_status);

extern enum acipc_return_code ACIPCEventUnBindExt(enum acipc_dev_type type,
							u32 user_event);

extern enum acipc_return_code ACIPCEventSetExt(enum acipc_dev_type type,
						enum acipc_events user_event);

extern enum acipc_return_code ACIPCDataSendExt(enum acipc_dev_type type,
				enum acipc_events user_event, acipc_data data);

extern enum acipc_return_code ACIPCDataReadExt(enum acipc_dev_type type,
							acipc_data *data);

#else
/* OLD API */
extern enum acipc_return_code ACIPCEventBind(u32 user_event,
					acipc_rec_event_callback cb,
					enum acipc_callback_mode cb_mode,
					u32 *historical_event_status);

extern enum acipc_return_code ACIPCEventUnBind(u32 user_event);

extern enum acipc_return_code ACIPCEventSet(enum acipc_events user_event);

extern enum acipc_return_code ACIPCDataSend(enum acipc_events user_event,
						acipc_data data);

extern enum acipc_return_code ACIPCDataRead(acipc_data *data);
#endif

#endif	/* _PXA9XX_ACIPC_V2_H_ */
