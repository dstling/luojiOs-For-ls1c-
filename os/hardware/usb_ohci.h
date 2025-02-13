/*
 * URB OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2001 David Brownell <dbrownell@users.sourceforge.net>
 *
 * usb-ohci.h
 */

#ifndef __USB_OHCI_H 
#define __USB_OHCI_H

struct usb_found_dev{
	char 	name[32];
	int		index;
	struct usb_device *dev;
	struct biodev *biodevThis;//usb可用设备被打开时 对应的biodev
	SLIST_ENTRY(usb_found_dev) i_next;
};


#define UNCACHED_MEMORY_ADDR    0xa0000000
#define CACHED_TO_PHYS(x)       ((unsigned)(x) & 0x7fffffff)
#define PHYS_TO_CACHED(x)       ((unsigned)(x) | CACHED_MEMORY_ADDR)
#define UNCACHED_TO_PHYS(x)     ((unsigned)(x) & 0x1fffffff)
#define PHYS_TO_UNCACHED(x)     ((unsigned)(x) | UNCACHED_MEMORY_ADDR)
#define VA_TO_CINDEX(x)         ((unsigned)(x) & 0xffffff | CACHED_MEMORY_ADDR)
#define CACHED_TO_UNCACHED(x)   (PHYS_TO_UNCACHED(CACHED_TO_PHYS(x)))
#define readl(addr) *(volatile u32*)(((u32)(addr)) | 0xa0000000)
#define writel(val, addr) *(volatile u32*)(((u32)(addr)) | 0xa0000000) = (val)

typedef unsigned char uint8_t;
typedef unsigned long long  u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed int s32;

typedef u_int32_t bus_addr_t;
typedef	unsigned long	vm_offset_t;
//typedef u_int32_t pcireg_t;		/* configuration space register XXX */
typedef struct arch_pci_chipset *pci_chipset_tag_t;//dstling
typedef u_int32_t bus_space_handle_t;//dstling

struct tgt_bus_space {
	u_int32_t	bus_base;
	u_int32_t	bus_reverse;	/* Reverse bus ops (dummy) */
};
typedef struct tgt_bus_space *bus_space_tag_t;

static int cc_to_error[16] = {

/* mapping of the OHCI CC status to error codes */
	/* No  Error  */               0,
	/* CRC Error  */               USB_ST_CRC_ERR,
	/* Bit Stuff  */               USB_ST_BIT_ERR,
	/* Data Togg  */               USB_ST_CRC_ERR,
	/* Stall      */               USB_ST_STALLED,
	/* DevNotResp */               -1,
	/* PIDCheck   */               USB_ST_BIT_ERR,
	/* UnExpPID   */               USB_ST_BIT_ERR,
	/* DataOver   */               USB_ST_BUF_ERR,
	/* DataUnder  */               USB_ST_BUF_ERR,
	/* reservd    */               -1,
	/* reservd    */               -1,
	/* BufferOver */               USB_ST_BUF_ERR,
	/* BuffUnder  */               USB_ST_BUF_ERR,
	/* Not Access */               -1,
	/* Not Access */               -1
};

/* ED States */

#define ED_NEW 		0x00
#define ED_UNLINK 	0x01
#define ED_OPER		0x02
#define ED_DEL		0x04
#define ED_URB_DEL  	0x08

/* usb_ohci_ed */
struct ed {
	volatile u32 hwINFO;
	volatile u32 hwTailP;
	volatile u32 hwHeadP;
	volatile u32 hwNextED;

	struct ed *ed_prev;
	struct ed *ed_next;
	u32 oINFO;
	u8 int_period;
	u8 int_branch;
	u8 int_load;
	u8 int_interval;
	u8 state;
	u8 type;
	u16 last_iso;
	struct ed *ed_rm_list;

	struct usb_device *usb_dev;
	u32 unused[5];
} __attribute((aligned(32)));
typedef struct ed ed_t;


/* TD info field */
#define TD_CC       0xf0000000
#define TD_CC_GET(td_p) ((td_p >>28) & 0x0f)
#define TD_CC_SET(td_p, cc) (td_p) = ((td_p) & 0x0fffffff) | (((cc) & 0x0f) << 28)
#define TD_EC       0x0C000000
#define TD_T        0x03000000
#define TD_T_DATA0  0x02000000
#define TD_T_DATA1  0x03000000
#define TD_T_TOGGLE 0x00000000
#define TD_R        0x00040000
#define TD_DI       0x00E00000
#define TD_DI_SET(X) (((X) & 0x07)<< 21)
#define TD_DP       0x00180000
#define TD_DP_SETUP 0x00000000
#define TD_DP_IN    0x00100000
#define TD_DP_OUT   0x00080000

#define TD_ISO	    0x00010000
#define TD_DEL      0x00020000

/* CC Codes */
#define TD_CC_NOERROR      0x00
#define TD_CC_CRC          0x01
#define TD_CC_BITSTUFFING  0x02
#define TD_CC_DATATOGGLEM  0x03
#define TD_CC_STALL        0x04
#define TD_DEVNOTRESP      0x05
#define TD_PIDCHECKFAIL    0x06
#define TD_UNEXPECTEDPID   0x07
#define TD_DATAOVERRUN     0x08
#define TD_DATAUNDERRUN    0x09
#define TD_BUFFEROVERRUN   0x0C
#define TD_BUFFERUNDERRUN  0x0D
#define TD_NOTACCESSED     0x0F


#define MAXPSW 1

struct td {
	u32 hwINFO;
  	u32 hwCBP;		/* Current Buffer Pointer */
  	u32 hwNextTD;   /* Next TD Pointer */
  	u32 hwBE;		/* Memory Buffer End Pointer */

  	u16 hwPSW[MAXPSW];
  	u8 unused;
  	u8 index;
  	struct ed *ed;
  	struct td *next_dl_td;
	struct usb_device *usb_dev;
	int transfer_len;
	u32 data;

	unsigned char *retbuf;
	u32 unused2[1];
} __attribute((aligned(32)));
typedef struct td td_t;

#define OHCI_ED_SKIP	(1 << 14)

/*
 * The HCCA (Host Controller Communications Area) is a 256 byte
 * structure defined in the OHCI spec. that the host controller is
 * told the base address of.  It must be 256-byte aligned.
 */

#define NUM_INTS 32	/* part of the OHCI standard */
struct ohci_hcca {
	volatile u32	int_table[NUM_INTS];	/* Interrupt ED table */
	volatile u16	frame_no;		/* current frame number */
	volatile u16	pad1;			/* set to 0 on each frame_no change */
	volatile u32	done_head;		/* info returned for an interrupt */
	u8		reserved_for_hc[116];
} __attribute((aligned(256)));


/*
 * Maximum number of root hub ports.
 */
#define MAX_ROOT_PORTS	15	/* maximum OHCI root hub ports */

/*
 * This is the structure of the OHCI controller's memory mapped I/O
 * region.  This is Memory Mapped I/O.  You must use the readl() and
 * writel() macros defined in asm/io.h to access these!!
 */
struct ohci_regs {
	/* control and status registers */
	u32	revision;
	u32	control;
	u32	cmdstatus;
	u32	intrstatus;
	u32	intrenable;
	u32	intrdisable;
	/* memory pointers */
	u32	hcca;
	u32	ed_periodcurrent;
	u32	ed_controlhead;
	u32	ed_controlcurrent;
	u32	ed_bulkhead;
	u32	ed_bulkcurrent;
	u32	donehead;
	/* frame counters */
	u32	fminterval;
	u32	fmremaining;
	u32	fmnumber;
	u32	periodicstart;
	u32	lsthresh;
	/* Root hub ports */
	struct	ohci_roothub_regs {
		u32	a;
		u32	b;
		u32	status;
		u32	portstatus[MAX_ROOT_PORTS];
	} roothub;
} __attribute((aligned(32)));


/* OHCI CONTROL AND STATUS REGISTER MASKS */

/*
 * HcControl (control) register masks
 */
#define OHCI_CTRL_CBSR	(3 << 0)	/* control/bulk service ratio */
#define OHCI_CTRL_PLE	(1 << 2)	/* periodic list enable */
#define OHCI_CTRL_IE	(1 << 3)	/* isochronous enable */
#define OHCI_CTRL_CLE	(1 << 4)	/* control list enable */
#define OHCI_CTRL_BLE	(1 << 5)	/* bulk list enable */
#define OHCI_CTRL_HCFS	(3 << 6)	/* host controller functional state */
#define OHCI_CTRL_IR	(1 << 8)	/* interrupt routing */
#define OHCI_CTRL_RWC	(1 << 9)	/* remote wakeup connected */
#define OHCI_CTRL_RWE	(1 << 10)	/* remote wakeup enable */

/* pre-shifted values for HCFS */
#	define OHCI_USB_RESET	(0 << 6)
#	define OHCI_USB_RESUME	(1 << 6)
#	define OHCI_USB_OPER	(2 << 6)
#	define OHCI_USB_SUSPEND	(3 << 6)

/*
 * HcCommandStatus (cmdstatus) register masks
 */
#define OHCI_HCR	(1 << 0)	/* host controller reset */
#define OHCI_CLF  	(1 << 1)	/* control list filled */
#define OHCI_BLF  	(1 << 2)	/* bulk list filled */
#define OHCI_OCR  	(1 << 3)	/* ownership change request */
#define OHCI_SOC  	(3 << 16)	/* scheduling overrun count */

/*
 * masks used with interrupt registers:
 * HcInterruptStatus (intrstatus)
 * HcInterruptEnable (intrenable)
 * HcInterruptDisable (intrdisable)
 */
#define OHCI_INTR_SO	(1 << 0)	/* scheduling overrun */
#define OHCI_INTR_WDH	(1 << 1)	/* writeback of done_head */
#define OHCI_INTR_SF	(1 << 2)	/* start frame */
#define OHCI_INTR_RD	(1 << 3)	/* resume detect */
#define OHCI_INTR_UE	(1 << 4)	/* unrecoverable error */
#define OHCI_INTR_FNO	(1 << 5)	/* frame number overflow */
#define OHCI_INTR_RHSC	(1 << 6)	/* root hub status change */
#define OHCI_INTR_OC	(1 << 30)	/* ownership change */
#define OHCI_INTR_MIE	(1 << 31)	/* master interrupt enable */


/* Virtual Root HUB */
struct virt_root_hub {
	int devnum; /* Address of Root Hub endpoint */
	void *dev;  /* was urb */
	void *int_addr;
	int send;
	int interval;
};

/* USB HUB CONSTANTS (not OHCI-specific; see hub.h) */

/* destination of request */
#define RH_INTERFACE               0x01
#define RH_ENDPOINT                0x02
#define RH_OTHER                   0x03

#define RH_CLASS                   0x20
#define RH_VENDOR                  0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS           0x0080
#define RH_CLEAR_FEATURE        0x0100
#define RH_SET_FEATURE          0x0300
#define RH_SET_ADDRESS		0x0500
#define RH_GET_DESCRIPTOR	0x0680
#define RH_SET_DESCRIPTOR       0x0700
#define RH_GET_CONFIGURATION	0x0880
#define RH_SET_CONFIGURATION	0x0900
#define RH_GET_STATE            0x0280
#define RH_GET_INTERFACE        0x0A80
#define RH_SET_INTERFACE        0x0B00
#define RH_SYNC_FRAME           0x0C80
/* Our Vendor Specific Request */
#define RH_SET_EP               0x2000


/* Hub port features */
#define RH_PORT_CONNECTION         0x00
#define RH_PORT_ENABLE             0x01
#define RH_PORT_SUSPEND            0x02
#define RH_PORT_OVER_CURRENT       0x03
#define RH_PORT_RESET              0x04
#define RH_PORT_POWER              0x08
#define RH_PORT_LOW_SPEED          0x09

#define RH_C_PORT_CONNECTION       0x10
#define RH_C_PORT_ENABLE           0x11
#define RH_C_PORT_SUSPEND          0x12
#define RH_C_PORT_OVER_CURRENT     0x13
#define RH_C_PORT_RESET            0x14

/* Hub features */
#define RH_C_HUB_LOCAL_POWER       0x00
#define RH_C_HUB_OVER_CURRENT      0x01

#define RH_DEVICE_REMOTE_WAKEUP    0x00
#define RH_ENDPOINT_STALL          0x01

#define RH_ACK                     0x01
#define RH_REQ_ERR                 -1
#define RH_NACK                    0x00


/* OHCI ROOT HUB REGISTER MASKS */

/* roothub.portstatus [i] bits */
#define RH_PS_CCS            0x00000001   	/* current connect status */
#define RH_PS_PES            0x00000002   	/* port enable status*/
#define RH_PS_PSS            0x00000004   	/* port suspend status */
#define RH_PS_POCI           0x00000008   	/* port over current indicator */
#define RH_PS_PRS            0x00000010  	/* port reset status */
#define RH_PS_PPS            0x00000100   	/* port power status */
#define RH_PS_LSDA           0x00000200    	/* low speed device attached */
#define RH_PS_CSC            0x00010000 	/* connect status change */
#define RH_PS_PESC           0x00020000   	/* port enable status change */
#define RH_PS_PSSC           0x00040000    	/* port suspend status change */
#define RH_PS_OCIC           0x00080000    	/* over current indicator change */
#define RH_PS_PRSC           0x00100000   	/* port reset status change */

/* roothub.status bits */
#define RH_HS_LPS	     0x00000001		/* local power status */
#define RH_HS_OCI	     0x00000002		/* over current indicator */
#define RH_HS_DRWE	     0x00008000		/* device remote wakeup enable */
#define RH_HS_LPSC	     0x00010000		/* local power status change */
#define RH_HS_OCIC	     0x00020000		/* over current indicator change */
#define RH_HS_CRWE	     0x80000000		/* clear remote wakeup enable */

/* roothub.b masks */
#define RH_B_DR		0x0000ffff		/* device removable flags */
#define RH_B_PPCM	0xffff0000		/* port power control mask */

/* roothub.a masks */
#define	RH_A_NDP	(0xff << 0)		/* number of downstream ports */
#define	RH_A_PSM	(1 << 8)		/* power switching mode */
#define	RH_A_NPS	(1 << 9)		/* no power switching */
#define	RH_A_DT		(1 << 10)		/* device type (mbz) */
#define	RH_A_OCPM	(1 << 11)		/* over current protection mode */
#define	RH_A_NOCP	(1 << 12)		/* no over current protection */
#define	RH_A_POTPGT	(0xff << 24)		/* power on to power good time */

/* urb */
#define N_URB_TD 32
typedef struct
{
	ed_t *ed;
	u16 length;	/* number of tds associated with this request */
	u16 td_cnt;	/* number of tds already serviced */
	int   state;
	unsigned long pipe;
	int actual_length;
	int trans_length;
	unsigned char *trans_buffer, *setup_buffer;
	unsigned char *dev_trans_buffer;
	td_t *td[N_URB_TD];	/* list pointer to all corresponding TDs associated with this request */
	//unsigned char *bufs[N_URB_TD];
} urb_priv_t;
#define URB_DEL 1

/*
 * This is the full ohci controller description
 *
 * Note how the "proper" USB information is just
 * a subset of what the full implementation needs. (Linus)
 */


typedef struct ohci {
	struct usb_hc hc;
	//struct pci_attach_args pa;
    void *sc_ih;            /* interrupt handler cookie */
	//bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	unsigned long cpu_membase;  //for cpu access	
    pci_chipset_tag_t sc_pc;    /* chipset handle needed by mips */

	struct ohci_hcca *hcca;		/* hcca */
	/*dma_addr_t hcca_dma;*/

	int irq;
	int disabled;			/* e.g. got a UE, we're hung */
	int sleeping;
	unsigned long flags;		/* for HC bugs */

	struct ohci_regs *regs;	/* OHCI controller's memory */
	ed_t       *periodic [NUM_INTS];

	ed_t *ed_rm_list[2];     /* lists of all endpoints to be removed */
	ed_t *ed_bulktail;       /* last endpoint of bulk list */
	ed_t *ed_controltail;    /* last endpoint of control list */
	int intrstatus;
	u32 hc_control;		/* copy of the hc control reg */
	
	//struct usb_device *dev[32];
	struct virt_root_hub rh;
	struct usb_device *child_dev;//该ohci接口下下挂的usb设备
	
	int         load [NUM_INTS];
	const char	*slot_name;
	unsigned char *setup;
	unsigned char *control_buf;

	struct ohci_device *ohci_dev;
	void *lmem;
	bus_addr_t lmem_paddr;
	unsigned long free;
	td_t *gtd;

#define MAX_INTS 5
	struct usb_device *g_pInt_dev[MAX_INTS];
	urb_priv_t *g_pInt_urb_priv[MAX_INTS];
	ed_t *g_pInt_ed[MAX_INTS];

	unsigned int transfer_lock;

#ifdef CONFIG_SM502_USB_HCD
	/*only for sm502 usb only*/
#define MAX_SM502_BUFS 5 //This is sufficient
	void *sm502_bufs[MAX_SM502_BUFS];
	int  sm502_buf_use[MAX_SM502_BUFS];
#endif
} ohci_t;

#define NUM_EDS 32		/* num of preallocated endpoint descriptors */

struct ohci_device {
	ed_t 	ed[NUM_EDS];
	ed_t 	*cpu_ed;
	int ed_cnt;
	u32	unused[6];
}__attribute__((aligned(32)));

/* hcd */
/* endpoint */
static int ep_link(ohci_t * ohci, ed_t * ed);
static int ep_unlink(ohci_t * ohci, ed_t * ed);
static ed_t * ep_add_ed(struct usb_device * usb_dev, unsigned long pipe);

/*-------------------------------------------------------------------------*/

/* we need more TDs than EDs */
#define NUM_TD 64

/* +1 so we can align the storage */
//td_t gtd[NUM_TD+1];
/* pointers to aligned storage */
//td_t *ptd;

/* TDs ... */
static inline struct td *
td_alloc (struct usb_device *usb_dev)
{
	int i;
	struct td	*td;
	ohci_t *ohci = usb_dev->hc_private;
	td_t *ptd;

	ptd = ohci->gtd;
	td = NULL;
	for (i = 0; i < NUM_TD; i++) {
		if (ptd[i].usb_dev == NULL) {
			td = &ptd[i];
			memset(td, 0, sizeof(*td));
			td->usb_dev = usb_dev;
			break;
		}
	}
	if(td == NULL){
		printf("!!! td_alloc failed\n");	
		printf("dump td:\n");
		for(i=0; i<NUM_TD; i++){
			printf(" %d: usb_dev=%x, data=%x, index=%x\n", 
							i, ptd->usb_dev, ptd->data, ptd->index);
		}
	}
	return td;
}

static inline void
ed_free (struct ed *ed)
{
	ed->usb_dev = NULL;
}
#endif
