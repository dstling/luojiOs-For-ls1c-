
#include <buildconfig.h>
#include <types.h>
#include <stdio.h>
#include "rtdef.h"
#include "shell.h"
#include "filesys.h"

#include "usb.h"
#include "usb_defs.h"
#include "usb_ohci.h"

#define VA_TO_PA(x)     UNCACHED_TO_PHYS(x)
#define	SYNC_R	0	/* Sync caches for reading data */
#define	SYNC_W	1	/* Sync caches for writing data */


extern struct usb_device usb_dev[];//usb.c中定义
extern struct hostcontroller host_controller;//usb.c中定义

void __inline__ wait_ms(unsigned long ms)
{
	delay_ms(ms);
}

//#define DEBUG
//#define USB_DEBUG
//#define SHOW_INFO

#ifdef DEBUG
#define dbg(format, arg...) printf("DEBUG: " format "\n", ## arg)
#else
#define dbg(format, arg...) 
#endif /* DEBUG */

#define err(format, arg...) printf("ERROR: " format "\n", ## arg)

#ifdef SHOW_INFO
#define info(format, arg...) printf("INFO: " format "\n", ## arg)
#else
#define info(format, arg...) 
#endif

#ifdef	USB_DEBUG
#define	USB_PRINTF(fmt,args...)	printf (fmt ,##args)
#else
#define USB_PRINTF(fmt,args...)
#endif

#ifdef DEBUG
#define WR_RH_STAT(x)		{printf("WR:status %#8x", (x));writel((x), &gohci->regs->roothub.status);}
#define WR_RH_PORTSTAT(x)	{printf("WR:portstatus[%d] %#8x", wIndex-1, (x));writel((x), &gohci->regs->roothub.portstatus[wIndex-1]);}
#else
#define WR_RH_STAT(x)		writel((x), &gohci->regs->roothub.status)
#define WR_RH_PORTSTAT(x)	writel((x), &gohci->regs->roothub.portstatus[wIndex-1])
#endif

#define RD_RH_STAT		roothub_status(gohci)
#define RD_RH_PORTSTAT		roothub_portstatus(gohci,wIndex-1)
#define OK(x)			len = (x); break
#define RD_RH_STAT		roothub_status(gohci)
#define RD_RH_PORTSTAT		roothub_portstatus(gohci,wIndex-1)

#define min_t(type,x,y) ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })

//#define	vtophys(p)	(0)		//_pci_dmamap((vm_offset_t)p, 1)//dstling
#define	vtophys(p)			_pci_dmamap((vm_offset_t)p, 1)

static pcireg_t pci_local_mem_pci_base= 0x00000000;;
vm_offset_t _pci_dmamap(vm_offset_t va, unsigned int len)
{
	return (pci_local_mem_pci_base + VA_TO_PA (va));
}

#define m16_swap(x) swap_16(x)
#define m32_swap(x) swap_32(x)

#define NOTUSBIRQ -0x100

#define  OHCI_PCI_MMBA 0x10

#define MAX_OHCI_C 4   /*In most case it is enough */

ohci_t *usb_ohci_dev[MAX_OHCI_C];


struct usb_device *devgone;

//static struct ohci ohci_root;

//int ohci_debug = 1;

#define OHCI_MAX_USBDEVICES 8
#define OHCI_MAX_ENDPOINTS_PER_DEVICE 32
urb_priv_t ohci_urb[OHCI_MAX_USBDEVICES][OHCI_MAX_ENDPOINTS_PER_DEVICE];

int got_rhsc;

#define OHCI_CONTROL_INIT \
	(OHCI_CTRL_CBSR & 0x3) | OHCI_CTRL_IE | OHCI_CTRL_PLE

static void urb_free_priv (urb_priv_t * urb)
{
	int		i;
	int		last;
	struct td	* td;

	last = urb->length - 1;
	if (last >= 0) {
		for (i = 0; i <= last; i++) {
			td = urb->td[i];
			if (td) {
				td->usb_dev = NULL;
				td->retbuf = NULL;
				urb->td[i] = NULL;
			}
		}
	}
}

void pci_sync_cache(void *p, vm_offset_t adr, int size, int rw)
{
	CPU_IOFlushDCache(adr, size, rw);
}

static void td_fill (ohci_t *ohci, unsigned int info,
	void *data, int len,
	struct usb_device *dev, int index, urb_priv_t *urb_priv, void*retbuf)
{
	volatile td_t  *td, *td_pt;

	if (index > urb_priv->length) {
		err("index > length \n");
		return;
	}

	if (index != urb_priv->length - 1) 
		info |= (7 << 21);

	/* use this td as the next dummy */
	{
		td_pt = (td_t *)CACHED_TO_UNCACHED(urb_priv->td[index]);
	}
	td_pt->hwNextTD = 0;

	/* fill the old dummy TD */
	{
		td = urb_priv->td[index]= (td_t *)(CACHED_TO_UNCACHED(urb_priv->ed->hwTailP) & ~0xf);
	}


	td->ed = urb_priv->ed;
	td->next_dl_td = NULL;
	td->index = index;
	td->data = (u32)(data);
	td->transfer_len = len; //for debug purpose
	td->retbuf = retbuf;

	if (!len)
		data = 0;

	td->hwINFO = m32_swap (info);
	
	if(len==0)
		td->hwCBP = 0; //take special care
	else{
			td->hwCBP = vtophys(data);
	}

	if (data){
			td->hwBE = vtophys(data+ len - 1);
	}
	else
		td->hwBE = 0;

	{
		td->hwNextTD = vtophys(m32_swap (td_pt));
	}
	td->hwPSW [0] = ((u32)data & 0x0FFF) | 0xE000;
	/* append to queue */
	wbflush();
	td->ed->hwTailP = td->hwNextTD;
}


static void td_submit_job (struct usb_device *dev, unsigned long pipe, void
				*buffer, int transfer_len, struct devrequest *setup, urb_priv_t
				*urb, int interval)
{
	ohci_t *ohci = dev->hc_private;
	int data_len = transfer_len;
	void *data;
	int cnt = 0;
	u32 info = 0; int periodic = 0;
	unsigned int toggle = 0;

	/* OHCI handles the DATA-toggles itself, we just use the USB-toggle bits for reseting */
	{
		pci_sync_cache(ohci->sc_pc, (vm_offset_t)buffer, transfer_len, SYNC_W);//dstling
	}

	if(usb_gettoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe))) {
		toggle = TD_T_TOGGLE;
	} else {
		toggle = TD_T_DATA0;
		usb_settoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe), 1);
	}
	urb->td_cnt = 0;
	if (data_len) 
	{
		{
			data = buffer; //XXX
		}
	}
   	else
	{
		data = 0;
	}

	switch (usb_pipetype (pipe)) {
	case PIPE_INTERRUPT:
		info = usb_pipeout (urb->pipe)?
			TD_CC | TD_DP_OUT | toggle: TD_CC | TD_R | TD_DP_IN | toggle;
		{
			td_fill (ohci, info, data, data_len, dev, cnt++, urb, NULL);
		}
		periodic = 1;
		break;
	case PIPE_BULK:
		info = usb_pipeout (pipe)?
			TD_CC | TD_DP_OUT : TD_CC | TD_DP_IN ;
		while(data_len > 4096) {
			td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, 4096, dev, cnt, urb, NULL);
			data += 4096; data_len -= 4096; cnt++;
		}
		info = usb_pipeout (pipe)?
			TD_CC | TD_DP_OUT : TD_CC | TD_R | TD_DP_IN ;
		td_fill (ohci, info | (cnt? TD_T_TOGGLE:toggle), data, data_len, dev, cnt, urb, NULL);
		cnt++;

		if (!ohci->sleeping)
			writel (OHCI_BLF, &ohci->regs->cmdstatus); /* start bulk list */
		break;

	case PIPE_CONTROL:
		info = TD_CC | TD_DP_SETUP | TD_T_DATA0;
		memcpy(ohci->setup, setup, 8);
		td_fill (ohci, info, (void *)ohci->setup, 8, dev, cnt++, urb, NULL);
		if (data_len > 0) {
			info = usb_pipeout (pipe)?
				TD_CC | TD_R | TD_DP_OUT | TD_T_DATA1 : TD_CC | TD_R | TD_DP_IN | TD_T_DATA1;
			/* NOTE:  mishandles transfers >8K, some >4K */
			if(usb_pipeout(pipe)){
				memcpy(ohci->control_buf, data, data_len);
				td_fill (ohci, info, ohci->control_buf, data_len, dev, cnt++, urb, NULL);
			} else {
				td_fill (ohci, info, ohci->control_buf, data_len, dev, cnt++, urb, buffer);
			}
		}
		info = (usb_pipeout (pipe) || data_len ==0)?
			TD_CC | TD_DP_IN | TD_T_DATA1: TD_CC | TD_DP_OUT | TD_T_DATA1;
		td_fill (ohci, info, data, 0, dev, cnt++, urb, NULL);
		if (!ohci->sleeping)
			writel (OHCI_CLF, &ohci->regs->cmdstatus); /* start Control list */
		break;
	}

	if (periodic) {
		ohci->hc_control |= OHCI_CTRL_PLE|OHCI_CTRL_IE;
		writel(ohci->hc_control, &ohci->regs->control);
	}

	if (urb->length != cnt)
		dbg("TD LENGTH %d != CNT %d", urb->length, cnt);
}


static void dl_transfer_length(td_t * td)
{
	u32 tdINFO, tdBE, tdCBP;
	urb_priv_t *lurb_priv = NULL;
	int length = 0;

	//QYL-2008-03-07
	u_int32_t dev_num,ed_num;
	struct usb_device *p_dev = NULL;
	ed_t *p_ed = NULL;

	tdINFO = m32_swap (td->hwINFO);
	tdBE   = m32_swap (td->hwBE);
	{
		tdCBP  = PHYS_TO_UNCACHED(m32_swap (td->hwCBP));
	}

	//QYL-2008-03-07
	if (td != NULL)
	{
		p_dev = td->usb_dev;
		for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
		{
			if (p_dev == &usb_dev[dev_num])
			{
				break;
			}
		}
		p_ed = td->ed;
		ed_num = (p_ed->hwINFO & 0x780) >> 7;
		lurb_priv = &ohci_urb[dev_num][ed_num];
	}

	if (!(usb_pipetype (lurb_priv->pipe) == PIPE_CONTROL &&
		((td->index == 0) || (td->index == lurb_priv->length - 1)))) {
		if (tdBE != 0) {
			if (td->hwCBP == 0){
				{
					length = PHYS_TO_UNCACHED(tdBE) - CACHED_TO_UNCACHED(td->data) + 1;
				}
				lurb_priv->actual_length += length;
				if(usb_pipecontrol(lurb_priv->pipe)&& usb_pipein(lurb_priv->pipe)){
					{
						memcpy((void *)td->retbuf, (void*)td->data, length);
					}
				}

			} else {
				{
					length = tdCBP - CACHED_TO_UNCACHED(td->data);
				}
				lurb_priv->actual_length += length;
				if(usb_pipein(lurb_priv->pipe) &&usb_pipecontrol(lurb_priv->pipe)){
					memcpy(td->retbuf, (void*)td->data, length);
				}
			}
		}
	}


}
static td_t * dl_reverse_done_list (ohci_t *ohci)
{
	u32 td_list_hc;
	td_t *td_rev = NULL;
	td_t *td_list = NULL;
	urb_priv_t *lurb_priv = NULL;

	//QYL-2008-03-07
	u_int32_t dev_num,ed_num;
	struct usb_device *p_dev = NULL;
	ed_t *p_ed = NULL;

	td_list_hc = ohci->hcca->done_head & 0xfffffff0;
	ohci->hcca->done_head = 0;

	while (td_list_hc) {

		{

			td_list = (td_t *)PHYS_TO_UNCACHED(td_list_hc & 0x1fffffff);
		}
		td_list->hwINFO |= TD_DEL;

		if (TD_CC_GET (m32_swap (td_list->hwINFO))) {
			p_dev = td_list->usb_dev;
			for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
			{
				if (p_dev == &usb_dev[dev_num])
				{
					break;
				}
			}
			p_ed = td_list->ed;
			ed_num = (p_ed->hwINFO & 0x780) >> 7;
			lurb_priv = &ohci_urb[dev_num][ed_num];
			
			if (td_list->ed->hwHeadP & m32_swap (0x1)) { //ED halted
				if (lurb_priv && ((td_list->index + 1) < lurb_priv->length)) {
					td_list->ed->hwHeadP =
						(lurb_priv->td[lurb_priv->length - 1]->hwNextTD & m32_swap (0xfffffff0)) |
									(td_list->ed->hwHeadP & m32_swap (0x2));
					lurb_priv->td_cnt += lurb_priv->length - td_list->index - 1;
				} else
					td_list->ed->hwHeadP &= m32_swap (0xfffffff2);
			}
		}

		td_list->next_dl_td = td_rev;
		td_rev = td_list;
		td_list_hc = m32_swap (td_list->hwNextTD) & 0xfffffff0;
	}
	return td_list;
}

//Felix-2008-05-05
static int dl_td_done_list (ohci_t *ohci, td_t *td_list)
{
	td_t *td_list_next = NULL;
	ed_t *ed;
	int cc = 0;
	int stat = 0;
	struct usb_device *dev = NULL;
	/* urb_t *urb; */
	urb_priv_t *lurb_priv = NULL;//&urb_priv;
	u32 tdINFO, edHeadP, edTailP;

    //QYL-2008-03-07
    u_int32_t dev_num,ed_num;
    struct usb_device *p_dev = NULL;
    ed_t *p_ed = NULL;

    //QYL-2008-03-07
    u_int32_t bPipeBulk = 0;
    urb_priv_t *pInt_urb_priv = NULL;
    struct usb_device *pInt_dev = NULL;
    ed_t *pInt_ed = NULL;
    
	while (td_list) {
        
		td_list_next = td_list->next_dl_td;

        p_dev = td_list->usb_dev;
        for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
        {
            if (p_dev == &usb_dev[dev_num])
            {
                break;
            }
        }
		if(dev_num == USB_MAX_DEVICE) {
			printf("Error not found device (%08x),%08x\n", td_list, p_dev);
			break;
		}
        p_ed = td_list->ed;
        ed_num = (p_ed->hwINFO & 0x780) >> 7;
        lurb_priv = &ohci_urb[dev_num][ed_num];

        //QYL-2008-03-07
        if (p_ed->type == PIPE_INTERRUPT)
        {
            pInt_ed = p_ed;
            pInt_urb_priv = lurb_priv;
            pInt_dev = p_ed->usb_dev;
        }
        if (p_ed->type == PIPE_BULK)
        {
            bPipeBulk = 1;
        }
        
		tdINFO = m32_swap (td_list->hwINFO);

		ed = td_list->ed;
		dev = ed->usb_dev;

		dl_transfer_length(td_list);

		/* error code of transfer */
		cc = TD_CC_GET (tdINFO);
		if (cc != 0) {
			err("ConditionCode %x/%x", cc, td_list);
			stat = cc_to_error[cc];
		}

		if (ed->state != ED_NEW) {
			edHeadP = m32_swap (ed->hwHeadP) & 0xfffffff0;
			edTailP = m32_swap (ed->hwTailP);

			/* unlink eds if they are not busy */
			if ((edHeadP == edTailP) && (ed->state == ED_OPER)){
				ep_unlink (ohci, ed);
			}
		}
		dev->status = stat; // FIXME, the transfer complete?

		td_list = td_list_next;
	}
    
    if ((NULL != pInt_urb_priv)) { /* FIXME */
		int i;
		for (i=0; i< MAX_INTS; i++) {
			if(ohci->g_pInt_dev[i] == NULL) {
				ohci->g_pInt_dev[i] = pInt_dev;
				ohci->g_pInt_ed[i] = pInt_ed;
				ohci->g_pInt_urb_priv[i] = pInt_urb_priv;
				break;
			}
		}
    }
	return stat;
}



#define OHCI_QUIRK_AMD756 0xabcd
#define read_roothub(hc, register, mask) ({ \
		u32 temp = readl (&hc->regs->roothub.register); \
		if (hc->flags & OHCI_QUIRK_AMD756) \
			while (temp & mask) \
				temp = readl (&hc->regs->roothub.register); \
		temp; })
static u32 roothub_a (struct ohci *hc)
	{ return read_roothub (hc, a, 0xfc0fe000); }
static inline u32 roothub_b (struct ohci *hc)
	{ return readl (&hc->regs->roothub.b); }
static inline u32 roothub_status (struct ohci *hc)
	{ return readl (&hc->regs->roothub.status); }
static u32 roothub_portstatus (struct ohci *hc, int i)
	{ return read_roothub (hc, portstatus [i], 0xffe0fce0); }

int rh_check_port_status(ohci_t *controller)
{
	u32 temp, ndp, i;
	int res;

	res = -1;
	temp = roothub_a (controller);
	ndp = (temp & RH_A_NDP);
	for (i = 0; i < ndp; i++) {
		temp = roothub_portstatus (controller, i);
		/* check for a device disconnect */
		if (((temp & (RH_PS_PESC | RH_PS_CSC)) ==
			(RH_PS_PESC | RH_PS_CSC)) &&
			((temp & RH_PS_CCS) == 0)) {
			res = i;
			break;
		}
	}
	return res;
}

static int hc_reset (ohci_t *ohci)
{
	int timeout = 30;
	int smm_timeout = 50; /* 0,5 sec */

	if (readl (&ohci->regs->control) & OHCI_CTRL_IR) { /* SMM owns the HC */
		writel (OHCI_OCR, &ohci->regs->cmdstatus); /* request ownership */
		info("USB HC TakeOver from SMM");
		while (readl (&ohci->regs->control) & OHCI_CTRL_IR) {
			wait_ms (10);
			if (--smm_timeout == 0) {
				err("USB HC TakeOver failed!");
				return -1;
			}
		}
	}

	/* Disable HC interrupts */
	writel (OHCI_INTR_MIE, &ohci->regs->intrdisable);

	dbg("USB HC reset_hc usb-%s: ctrl = 0x%X ;",
		ohci->slot_name,
		readl (&ohci->regs->control));

	/* Reset USB (needed by some controllers) */
	writel (0, &ohci->regs->control);

	/* HC Reset requires max 10 us delay */
	writel (OHCI_HCR,  &ohci->regs->cmdstatus);
	while ((readl (&ohci->regs->cmdstatus) & OHCI_HCR) != 0) {
		if (--timeout == 0) {
			err("USB HC reset timed out!");
			return -1;
		}
		wait_ms(1);
	}
	return 0;
}

static int balance (ohci_t *ohci, int interval, int load)
{
	int i, branch = -1;

	/* iso periods can be huge; iso tds specify frame numbers */
	if (interval > NUM_INTS)
		interval = NUM_INTS;

	/* search for the least loaded schedule branch of that period
	 *      * that has enough bandwidth left unreserved.
	 *           */
	for (i = 0; i < interval ; i++) {
		if (branch < 0 || ohci->load [branch] > ohci->load [i]) {
			int j;

			/* usb 1.1 says 90% of one frame */
			for (j = i; j < NUM_INTS; j += interval) {
				if ((ohci->load [j] + load) > 900)
					break;
			}
			if (j < NUM_INTS)
				continue;
			branch = i;
		}
	}
	return branch;
}

static void periodic_link (ohci_t *ohci, ed_t *ed)
{
	unsigned    i;
		 
	dbg("link %x branch %d [%dus.], interval %d\n",ed, ed->int_branch, ed->int_load, ed->int_interval);

	for (i = ed->int_branch; i < NUM_INTS; i += ed->int_interval) {
		ed_t   **prev = &ohci->periodic [i];
		volatile unsigned int *prev_p = &ohci->hcca->int_table [i];
		ed_t   *here = *prev;

		/* sorting each branch by period (slow before fast)
		 * lets us share the faster parts of the tree.
		 * (plus maybe: put interrupt eds before iso)
		 */
		while (here && ed != here) {
			if (ed->int_interval > here->int_interval)
				break;
			prev = &here->ed_next;
			prev_p = &here->hwNextED;
		}
		if (ed != here) {
			ed->ed_next = here;
			if (here)
				ed->hwNextED = *prev_p;
			*prev = ed;
			{
				*prev_p = vtophys(ed);
			}
		}
		ohci->load [i] += ed->int_load;
	}
}

static void periodic_unlink (ohci_t *ohci, ed_t *ed)
{
	int i;

	for (i = ed->int_branch; i < NUM_INTS; i += ed->int_interval) {
		ed_t   *temp;
		ed_t   **prev = &ohci->periodic[i];
		volatile unsigned int *prev_p = &ohci->hcca->int_table [i];

		while (*prev && (temp = *prev) != ed) {
			prev_p = &temp->hwNextED;
			prev = &temp->ed_next;
		}
		if (*prev) {
			*prev_p = ed->hwNextED;
			*prev = ed->ed_next;
		}
		ohci->load [i] -= ed->int_load;
	}

	dbg("unlink %p branch %d [%dus.], interval %d\n",ed, ed->int_branch, ed->int_load, ed->int_interval);	
}

static int ep_link (ohci_t *ohci, ed_t *edi)
{
	int branch = -1;
	volatile ed_t *ed = edi;

	ed->state = ED_OPER;
	dbg("ed->type:0x%08x\n",ed->type);
	switch (ed->type) {
	case PIPE_CONTROL:
		ed->hwNextED = 0;
		dbg("ep_link001,vtophys(ed):0x%08x\n",vtophys(ed));
		if (ohci->ed_controltail == NULL) 
		{
			dbg("ep_link000\n");
			writel (vtophys(ed), &ohci->regs->ed_controlhead);
		} 
		else 
		{
			dbg("ep_link001,ed_controltail:0x%o8x\n",ohci->ed_controltail);
			ohci->ed_controltail->hwNextED = m32_swap (vtophys(ed));
			dbg("ep_link001,ed_controltail:0x%o8x,ohci->ed_controltail->hwNextED:0x%08x\n",ohci->ed_controltail,ohci->ed_controltail->hwNextED);
		}
		dbg("ep_link1\n");
		ed->ed_prev = ohci->ed_controltail;
		dbg("ep_link2\n");
		if (!ohci->ed_controltail && !ohci->ed_rm_list[0] &&
			!ohci->ed_rm_list[1] && !ohci->sleeping) 
		{
			ohci->hc_control |= OHCI_CTRL_CLE;
			dbg("ep_link3\n");
			writel (ohci->hc_control, &ohci->regs->control);
			dbg("ep_link4\n");
		}
			dbg("ep_link5\n");
		ohci->ed_controltail = edi;
		break;

	case PIPE_BULK:
		ed->hwNextED = 0;
		if (ohci->ed_bulktail == NULL) {
			{
				writel ((long)vtophys(ed), &ohci->regs->ed_bulkhead);
			}
		} else {
			{
				ohci->ed_bulktail->hwNextED = vtophys(ed);
			}
		}
		ed->ed_prev = ohci->ed_bulktail;
		if (!ohci->ed_bulktail && !ohci->ed_rm_list[0] &&
			!ohci->ed_rm_list[1] && !ohci->sleeping) {
			ohci->hc_control |= OHCI_CTRL_BLE;
			writel (ohci->hc_control, &ohci->regs->control);
		}
		ohci->ed_bulktail = edi;
		break;
	case PIPE_INTERRUPT:
		ed->hwNextED = 0;
		branch = balance(ohci, ed->int_interval, ed->int_load);
		ed->int_branch = branch;
		periodic_link (ohci, (ed_t *)ed);
		break;
	}
	return 0;
}

static int ep_unlink (ohci_t *ohci, ed_t *ed)
{
	ed->hwINFO |= m32_swap (OHCI_ED_SKIP);

	switch (ed->type) {
	case PIPE_CONTROL:
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_CLE;
				writel (ohci->hc_control, &ohci->regs->control);
			}
			writel (m32_swap (*((u32 *)&ed->hwNextED)), &ohci->regs->ed_controlhead);
		} else {
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		if (ohci->ed_controltail == ed) {
			ohci->ed_controltail = ed->ed_prev;
		} else {
			{
				((ed_t *)CACHED_TO_UNCACHED(*((u32 *)&ed->hwNextED)))->ed_prev = ed->ed_prev;
			}
		}
		break;

	case PIPE_BULK:
		if (ed->ed_prev == NULL) {
			if (!ed->hwNextED) {
				ohci->hc_control &= ~OHCI_CTRL_BLE;
				writel (ohci->hc_control, &ohci->regs->control);
			}
			writel (m32_swap (*((u32 *)&ed->hwNextED)), &ohci->regs->ed_bulkhead);
		} else {
			ed->ed_prev->hwNextED = ed->hwNextED;
		}
		if (ohci->ed_bulktail == ed) {
			ohci->ed_bulktail = ed->ed_prev;
		} else {
			{
				((ed_t *)CACHED_TO_UNCACHED(ed->hwNextED))->ed_prev = ed->ed_prev;
			}
		}
		break;
	case PIPE_INTERRUPT:
		periodic_unlink (ohci, ed);
		break;
	}
	ed->state = ED_UNLINK;
	return 0;
}


long usb_calc_bus_time (int speed, int is_input, int isoc, int bytecount)
{
	unsigned long   tmp;

	switch (speed) {
		case USB_SPEED_LOW:     /* INTR only */
			if (is_input) {
				tmp = (67667L * (31L + 10L * BitTime (bytecount))) / 1000L;
				return (64060L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
			} else {
				tmp = (66700L * (31L + 10L * BitTime (bytecount))) / 1000L;
				return (64107L + (2 * BW_HUB_LS_SETUP) + BW_HOST_DELAY + tmp);
			}
		case USB_SPEED_FULL:    /* ISOC or INTR */
			if (isoc) {
				tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
				return (((is_input) ? 7268L : 6265L) + BW_HOST_DELAY + tmp);
			} else {
				tmp = (8354L * (31L + 10L * BitTime (bytecount))) / 1000L;
				return (9107L + BW_HOST_DELAY + tmp);
			}
		default:
			printf ("bogus device speed!\n");
			return -1;
	}
}

static ed_t * ep_add_ed (struct usb_device *usb_dev, unsigned long pipe)
{
	td_t *td;
	ed_t *ed_ret;
	volatile ed_t *ed;
	
	ohci_t *ohci = usb_dev->hc_private;
	struct ohci_device *ohci_dev = ohci->ohci_dev;

    //QYL-2008-03-07
    u_int32_t cpued_num = 0;

    cpued_num = ((usb_pipedevice(pipe)&0x3)<<3)|((usb_pipeendpoint(pipe)&0x3)<<1)|(usb_pipein(pipe));
    ed = ed_ret = &ohci_dev->cpu_ed[cpued_num];
    
	if ((ed->state & ED_DEL) || (ed->state & ED_URB_DEL)) {
		err("ep_add_ed: pending delete %x/%d\n", ed->state, 
				(usb_pipeendpoint(pipe) << 1) | (usb_pipecontrol (pipe)? 0: usb_pipeout (pipe)));
		/* pending delete request */
		return NULL;
	}

	if (ed->state == ED_NEW) {
		ed->hwINFO = m32_swap (OHCI_ED_SKIP); /* skip ed */
		/* dummy td; end of td list for ed */
		td = td_alloc (usb_dev);
		{
			ed->hwTailP = vtophys(td);
		}

		ed->hwHeadP = ed->hwTailP;
		ed->state = ED_UNLINK;
		ed->type = usb_pipetype (pipe);
		ohci_dev->ed_cnt++;
	}

	ed->hwINFO = m32_swap (usb_pipedevice (pipe)
			| usb_pipeendpoint (pipe) << 7
			| (usb_pipeisoc (pipe)? 0x8000: 0)
			| (usb_pipecontrol (pipe)? 0: (usb_pipeout (pipe)? 0x800: 0x1000))
			| usb_pipeslow (pipe) << 13
			| usb_maxpacket (usb_dev, pipe) << 16);
	ed->oINFO = ed->hwINFO;

	if(usb_pipetype(pipe) == PIPE_INTERRUPT) 
		ed->int_load = usb_calc_bus_time (
				USB_SPEED_LOW, !usb_pipeout(pipe), 0, 64) / 1000; /*FIXME*/
	return ed_ret;
}


static int hc_check_ohci_controller (void *hc_data)
{
	ohci_t *ohci = hc_data;
	struct ohci_regs *regs = ohci->regs;	
	urb_priv_t *lurb_priv = NULL;	 
	td_t *td = NULL;
	int ints;
	int stat = NOTUSBIRQ;

	u_int32_t dev_num, ed_num;
	struct usb_device *p_dev = NULL;
	ed_t *p_ed = NULL;

	if ((ohci->hcca->done_head != 0) &&
			!(m32_swap (ohci->hcca->done_head) & 0x01)) {
		ints =	OHCI_INTR_WDH;
		{
			td = (td_t *)CACHED_TO_UNCACHED(ohci->hcca->done_head);
		}
	} else {
		ints = readl (&regs->intrstatus);
		if (ints == ~0ul) 
			return 0;
		if (ints == 0)
			return 0;
	}

	if (ints & OHCI_INTR_RHSC) {
		writel(OHCI_INTR_RD | OHCI_INTR_RHSC, &regs->intrstatus);
		got_rhsc = 1;
	}

	if (got_rhsc) {
		int timeout;
#if 0
		ohci_dump_roothub (gohci, 1);
#endif
		got_rhsc = 0;
		/* abuse timeout */
		wait_ms(10);
		timeout = rh_check_port_status(ohci);
		if (timeout >= 0) {
			/*
			 * XXX
			 * This is potentially dangerous because it assumes
			 * that only one device is ever plugged in!
			 */
			printf("**hc_check_ohci_controller** device disconnected\n");
		}
	}


	if (ints & OHCI_INTR_UE) {
		ohci->disabled++;
		printf("**hc_check_ohci_controller** Unrecoverable Error, controller usb-%s disabled\n",
			ohci->slot_name);
		/* e.g. due to PCI Master/Target Abort */
#ifdef	DEBUG
		//ohci_dump (ohci, 1);
#endif
		/* FIXME: be optimistic, hope that bug won't repeat often. */
		/* Make some non-interrupt context restart the controller. */
		/* Count and limit the retries though; either hardware or */
		/* software errors can go forever... */
		hc_reset (ohci);
		ohci->disabled--;
		return -1;
	}

	if (ints & OHCI_INTR_WDH) {
		
		if (td == NULL) { 
			{
				td = (td_t *)CACHED_TO_UNCACHED(ohci->hcca->done_head & ~0x1f);
			}
		}

		if ((td != NULL)&&(td->ed != NULL)) { //&&(td->ed->type != PIPE_INTERRUPT)))
			writel (OHCI_INTR_WDH, &regs->intrdisable);
			p_dev = td->usb_dev;

			for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
			{
				if (p_dev == &usb_dev[dev_num])
				{
					break;
				}
			}
			p_ed = td->ed;
			ed_num = (p_ed->hwINFO & 0x780) >> 7;//See OHCI1.1 spec Page 17 Endpoint Descriptor Field Definitions
			lurb_priv= &ohci_urb[dev_num][ed_num];

			if (td->index != lurb_priv->length -1) {		   
				stat = dl_td_done_list (ohci, dl_reverse_done_list (ohci));
				printf("**hc_check** td index=%x/%x, p_dev %x\n", td->index, lurb_priv->length, p_dev);
			} else {
				stat = dl_td_done_list (ohci, dl_reverse_done_list (ohci));
			}
			writel (OHCI_INTR_WDH, &regs->intrenable);
		}
	}

	if (ints & OHCI_INTR_SO) {
		printf("USB Schedule overrun\n");
		writel (OHCI_INTR_SO, &regs->intrenable);
		stat = -1;
	}

	/* FIXME:  this assumes SOF (1/ms) interrupts don't get lost... */
	if (ints & OHCI_INTR_SF) {
		unsigned int frame = m16_swap (ohci->hcca->frame_no) & 1;
		writel (OHCI_INTR_SF, &regs->intrdisable);
		if (ohci->ed_rm_list[frame] != NULL)
			writel (OHCI_INTR_SF, &regs->intrenable);
		stat = 0xff;
	}
	writel (ints, &regs->intrstatus);
	(void)readl(&regs->control);

	return stat;
}





int sohci_submit_job(struct usb_device *dev, unsigned long pipe, void *buffer,int transfer_len, struct devrequest *setup, int interval)
{
	ohci_t *ohci;
	ed_t * ed;
	urb_priv_t *purb_priv;
	int i, size = 0;

    //QYL-2008-03-07
    u_int32_t dev_num,ed_num;

	ohci = dev->hc_private;

	/* when controller's hung, permit only roothub cleanup attempts
	 * such as powering down ports */
	if (ohci->disabled) {
		err("sohci_submit_job: EPIPE");
		return -1;
	}
	dbg("sohci_submit_job1\n");
	/* every endpoint has a ed, locate and fill it */
	if (!(ed = ep_add_ed (dev, pipe))) {
		err("sohci_submit_job: ENOMEM");
		return -1;
	}
	dbg("sohci_submit_job2\n");

	ed->usb_dev = dev;
	ed->int_interval = interval;

	/* for the private part of the URB we need the number of TDs (size) */
	switch (usb_pipetype (pipe)) {
		case PIPE_BULK: /* one TD for every 4096 Byte */
			size =  (transfer_len - 1) / 4096 + 1;
			break;
		case PIPE_CONTROL: /* 1 TD for setup, 1 for ACK and 1 for every 4096 B */
			size = (transfer_len == 0)? 2:
						(transfer_len - 1) / 4096 + 3;
			break;
		case PIPE_INTERRUPT:
			size =  (transfer_len - 1) / 4096 + 1;
			break;
		default:
			break;
	}

	if (size >= (N_URB_TD - 1)) {
		err("need %d TDs, only have %d", size, N_URB_TD);
		return -1;
	}

    for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
    {
        if (dev == &usb_dev[dev_num])
        {
            break;
        }
    }
	dbg("sohci_submit_job3\n");
    ed_num = usb_pipeendpoint(pipe);
    purb_priv = &ohci_urb[dev_num][ed_num];
    
	dbg("sohci_submit_job4\n");
	purb_priv->pipe = pipe;
	purb_priv->trans_buffer = buffer;
	purb_priv->setup_buffer = (unsigned char *)setup;

	/* fill the private part of the URB */
	purb_priv->length = size;
	purb_priv->ed = ed;
	purb_priv->actual_length = 0;
	purb_priv->trans_length = transfer_len;

	/* allocate the TDs */
	/* note that td[0] was allocated in ep_add_ed */
	for (i = 0; i < size; i++) {
		purb_priv->td[i] = td_alloc (dev);
		if (!purb_priv->td[i]) {
			purb_priv->length = i;
			urb_free_priv (purb_priv);
			err("sohci_submit_job: ENOMEM");
			return -1;
		}
	}

	if (ed->state == ED_NEW || (ed->state & ED_DEL)) {
		urb_free_priv (purb_priv);
		err("sohci_submit_job: EINVAL");
		return -1;
	}
	dbg("sohci_submit_job5\n");

	/* link the ed into a chain if is not already */
	if (ed->state != ED_OPER)
		ep_link (ohci, ed);
	dbg("sohci_submit_job6\n");
	/* fill the TDs and link it to the ed */
	td_submit_job(dev, pipe, buffer, transfer_len, setup, purb_priv, interval);
	dbg("sohci_submit_job7\n");

	return 0;
}


int submit_common_msg(struct usb_device *dev, unsigned long pipe, void *buffer,int transfer_len, struct devrequest *setup, int interval)
{
	int stat = 0;
	int maxsize = usb_maxpacket(dev, pipe);
	int timeout, i;
	struct ohci *gohci = dev->hc_private;
	struct usb_interface_descriptor *iface;
 
#ifdef LS1BSOC
   //liushiwei-2016-03-02  4个usb口，上面的2个，在pmon中跳过不用， 可以用来插一些会造成pmon死锁的usb设备。
   //u盘升级， 请使用下面的2个口， 靠近线路板的2个口
	unsigned char usbDeviceClass;
	usbDeviceClass = dev->descriptor.bDeviceClass;
	if(usbDeviceClass == 0) {
		//then see InterfaceClass
		iface = &dev->config.if_desc[0];
		usbDeviceClass=iface->bInterfaceClass;
	}

	if( usbDeviceClass != 9  && (dev->port == 1 || dev->port == 3)  ) { 
		printf("usb port=%d, DeviceClass=0x%x,skip\n",dev->port,usbDeviceClass);
		return -1;
	}
#endif
    //QYL-2008-03-07
    u_int32_t dev_num,ed_num;
    urb_priv_t *lurb_priv = NULL;

	/* device pulled? Shortcut the action. */
	if (devgone == dev) {
		dev->status = USB_ST_CRC_ERR;
		return 0;
	}
	dbg("submit_common_msg2,pipe:0x%08x,gohci:0x%08x,gohci->hc_control:0x%08x\n",pipe,gohci,gohci->hc_control);

	if (!maxsize) {
		err("submit_common_message: pipesize for pipe %lx is zero",
			pipe);
		return -1;
	}

	if(pipe != PIPE_INTERRUPT) {
		gohci->transfer_lock ++;

		gohci->hc_control &= ~OHCI_CTRL_PLE;
		writel(gohci->hc_control, &gohci->regs->control);
	}
	dbg("submit_common_msg3\n");

	if (sohci_submit_job(dev, pipe, buffer, transfer_len, setup, interval) < 0) {
		err("sohci_submit_job failed");
		return -1;
	}
	dbg("submit_common_msg4\n");
	/* allow more time for a BULK device to react - some are slow */
#define BULK_TO	 500	/* timeout in milliseconds */
	if (usb_pipetype (pipe) == PIPE_BULK)
		timeout = BULK_TO;
	else
		timeout = 2000;

	timeout *= 100;

	/* wait for it to complete */
	dbg("submit_common_msg5\n");
	while(--timeout > 0) {
		if(!(dev->status & USB_ST_NOT_PROC)){
			break;
		}
		wait_ms(1);
		hc_check_ohci_controller(gohci);
	}
	dbg("submit_common_msg6\n");
	if(pipe != PIPE_INTERRUPT) {
		gohci->transfer_lock --;
		gohci->hc_control |= OHCI_CTRL_PLE;
		writel(gohci->hc_control, &gohci->regs->control);
	}
	dbg("submit_common_msg7\n");

	for(i=0; i< MAX_INTS; i++) {
		struct usb_device *pInt_dev = NULL;
		urb_priv_t * pInt_urb_priv = NULL;
		ed_t * pInt_ed = NULL;

		pInt_dev = gohci->g_pInt_dev[i];
		pInt_urb_priv = gohci->g_pInt_urb_priv[i];
		pInt_ed = gohci->g_pInt_ed[i];

		if(pInt_dev == NULL || pInt_urb_priv == NULL|| pInt_ed == NULL)
			continue;

		if (pInt_dev->irq_handle) {
			pInt_dev->irq_status = 0;
			pInt_dev->irq_act_len = pInt_urb_priv->actual_length;
			pInt_dev->irq_handle(pInt_dev);
		}

		pInt_urb_priv->actual_length = 0;
		pInt_dev->irq_act_len = 0;
		pInt_ed->hwINFO = pInt_ed->oINFO;
		ep_link(gohci, pInt_ed);
		td_submit_job(pInt_ed->usb_dev, pInt_urb_priv->pipe, pInt_urb_priv->trans_buffer, pInt_urb_priv->trans_length, (struct devrequest *)pInt_urb_priv->setup_buffer, pInt_urb_priv, pInt_ed->int_interval);               

		gohci->g_pInt_dev[i] = NULL;
		gohci->g_pInt_urb_priv[i] = NULL;
		gohci->g_pInt_ed[i] = NULL;

	}
	
	dbg("submit_common_msg2,timeout:%d\n",timeout);
    //if (timeout == 0) 
	//	printf("USB timeout dev:0x%x,devnum=0x%x,port=0x%x,deviceclass=0x%x,interfacesClass=0x%x\n",(u_int32_t)dev,dev->devnum,dev->port,dev->descriptor.bDeviceClass,iface->bInterfaceClass);

	dbg("submit_common_msg8.1,got_rhsc:%d\n",got_rhsc);

	/* we got an Root Hub Status Change interrupt */
	if (got_rhsc) {
		got_rhsc = 0;
		/* abuse timeout */
	
		dbg("submit_common_msg8.2\n");
		timeout = rh_check_port_status(gohci);
		dbg("submit_common_msg8.3\n");
		if (timeout >= 0) 
		{
			printf("device disconnected\n");
			devgone = dev;
		}
	}
	dbg("submit_common_msg9\n");

    //QYL-2008-03-07
    for(dev_num = 0; dev_num < USB_MAX_DEVICE; dev_num++)
    {
        if (dev == &usb_dev[dev_num])
        {
            break;
        }
    }
	dbg("submit_common_msg10\n");
    ed_num = usb_pipeendpoint(pipe);
    dbg("submit_common_msg11\n");
	lurb_priv = &ohci_urb[dev_num][ed_num];

	if (usb_pipetype(pipe) != PIPE_INTERRUPT) { /*FIXME, might not done bulk*/
		dev->status = stat;
		dev->act_len = transfer_len;
		/* free TDs in urb_priv */
		urb_free_priv(lurb_priv);
		memset(lurb_priv, 0, sizeof(*lurb_priv));
	}
	dbg("submit_common_msg12\n");

	return 0;
}



/*-------------------------------------------------------------------------*
 * Virtual Root Hub
 *-------------------------------------------------------------------------*/
/* Device descriptor */
static u8 root_hub_dev_des[] =
{
	0x12,	    /*	__u8  bLength; */
	0x01,	    /*	__u8  bDescriptorType; Device */
	0x10,	    /*	__u16 bcdUSB; v1.1 */
	0x01,
	0x09,	    /*	__u8  bDeviceClass; HUB_CLASSCODE */
	0x00,	    /*	__u8  bDeviceSubClass; */
	0x00,	    /*	__u8  bDeviceProtocol; */
	0x08,	    /*	__u8  bMaxPacketSize0; 8 Bytes */
	0x00,	    /*	__u16 idVendor; */
	0x00,
	0x00,	    /*	__u16 idProduct; */
	0x00,
	0x00,	    /*	__u16 bcdDevice; */
	0x00,
	0x00,	    /*	__u8  iManufacturer; */
	0x01,	    /*	__u8  iProduct; */
	0x00,	    /*	__u8  iSerialNumber; */
	0x01	    /*	__u8  bNumConfigurations; */
};

/* Configuration descriptor */
static u8 root_hub_config_des[] =
{
	0x09,		/*	__u8  bLength; */
	0x02,		/*	__u8  bDescriptorType; Configuration */
	0x19,		/*	__u16 wTotalLength; */
	0x00,
	0x01,		/*	__u8  bNumInterfaces; */
	0x01,		/*	__u8  bConfigurationValue; */
	0x00,		/*	__u8  iConfiguration; */
	0x40,		/*	__u8  bmAttributes;
		 Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup, 4..0: resvd */
	0x00,		/*	__u8  MaxPower; */

	/* interface */
	0x09,		/*	__u8  if_bLength; */
	0x04,		/*	__u8  if_bDescriptorType; Interface */
	0x00,		/*	__u8  if_bInterfaceNumber; */
	0x00,		/*	__u8  if_bAlternateSetting; */
	0x01,		/*	__u8  if_bNumEndpoints; */
	0x09,		/*	__u8  if_bInterfaceClass; HUB_CLASSCODE */
	0x00,		/*	__u8  if_bInterfaceSubClass; */
	0x00,		/*	__u8  if_bInterfaceProtocol; */
	0x00,		/*	__u8  if_iInterface; */

	/* endpoint */
	0x07,		/*	__u8  ep_bLength; */
	0x05,		/*	__u8  ep_bDescriptorType; Endpoint */
	0x81,		/*	__u8  ep_bEndpointAddress; IN Endpoint 1 */
	0x03,		/*	__u8  ep_bmAttributes; Interrupt */
	0x02,		/*	__u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
	0x00,
	0xff		/*	__u8  ep_bInterval; 255 ms */
};

static unsigned char root_hub_str_index0[] =
{
	0x04,			/*	__u8  bLength; */
	0x03,			/*	__u8  bDescriptorType; String-descriptor */
	0x09,			/*	__u8  lang ID */
	0x04,			/*	__u8  lang ID */
};


static unsigned char root_hub_str_index1[] =
{
	28,			/*  __u8  bLength; */
	0x03,			/*  __u8  bDescriptorType; String-descriptor */
	'O',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'H',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'C',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'I',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	' ',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'R',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'o',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'o',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	't',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	' ',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'H',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'u',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
	'b',			/*  __u8  Unicode */
	0,				/*  __u8  Unicode */
};


static int ohci_submit_rh_msg(struct usb_device *dev, unsigned long pipe,
		void *buffer, int transfer_len, struct devrequest *cmd)
{
	void * data = buffer;
	int leni = transfer_len;
	int len = 0;
	int stat = 0;
	u32 datab[4];
	u8 *data_buf = (u8 *)datab;
	u16 bmRType_bReq;
	u16 wValue;
	u16 wIndex;
	u16 wLength;
	struct ohci *gohci = dev->hc_private;

	wait_ms(1);
	info("ohci_submit_rh_msgxxxxxxxxxxxxxxxxxxx\n");

	if ((pipe & PIPE_INTERRUPT) == PIPE_INTERRUPT) {
		info("Root-Hub submit IRQ: NOT implemented");
		return 0;
	}

	bmRType_bReq  = cmd->requesttype | (cmd->request << 8);
	wValue	      = m16_swap (cmd->value);
	wIndex	      = m16_swap (cmd->index);
	wLength	      = m16_swap (cmd->length);

	///*
	info("Root-Hub: adr: %2x cmd(%1x): %08x %04x %04x %04x",
		dev->devnum, 8, bmRType_bReq, wValue, wIndex, wLength);
	//*/

	//printf("ohci_submit_rh_msg,bmRType_bReq:0x%08x\n",bmRType_bReq);
	switch (bmRType_bReq) {
	/* Request Destination:
	   without flags: Device,
	   RH_INTERFACE: interface,
	   RH_ENDPOINT: endpoint,
	   RH_CLASS means HUB here,
	   RH_OTHER | RH_CLASS	almost ever means HUB_PORT here
	*/

	case RH_GET_STATUS:
			*(u16 *) data_buf = m16_swap (1); OK (2);
	case RH_GET_STATUS | RH_INTERFACE:
			*(u16 *) data_buf = m16_swap (0); OK (2);
	case RH_GET_STATUS | RH_ENDPOINT:
			*(u16 *) data_buf = m16_swap (0); OK (2);
	case RH_GET_STATUS | RH_CLASS:
			*(u32 *) data_buf = m32_swap (
				RD_RH_STAT & ~(RH_HS_CRWE | RH_HS_DRWE));
			OK (4);
	case RH_GET_STATUS | RH_OTHER | RH_CLASS:
			*(u32 *) data_buf = m32_swap (RD_RH_PORTSTAT); OK (4);

	case RH_CLEAR_FEATURE | RH_ENDPOINT:
		switch (wValue) {
			case (RH_ENDPOINT_STALL): OK (0);
		}
		break;

	case RH_CLEAR_FEATURE | RH_CLASS:
		switch (wValue) {
			case RH_C_HUB_LOCAL_POWER:
				OK(0);
			case (RH_C_HUB_OVER_CURRENT):
					//printf("ohci_submit_rh_msg dstling\n");
					WR_RH_STAT(RH_HS_OCIC); OK (0);
		}
		break;

	case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
			case (RH_PORT_ENABLE):
					WR_RH_PORTSTAT (RH_PS_CCS ); OK (0);
			case (RH_PORT_SUSPEND):
					WR_RH_PORTSTAT (RH_PS_POCI); OK (0);
			case (RH_PORT_POWER):
					WR_RH_PORTSTAT (RH_PS_LSDA); OK (0);
			case (RH_C_PORT_CONNECTION):
					WR_RH_PORTSTAT (RH_PS_CSC ); OK (0);
			case (RH_C_PORT_ENABLE):
					WR_RH_PORTSTAT (RH_PS_PESC); OK (0);
			case (RH_C_PORT_SUSPEND):
					WR_RH_PORTSTAT (RH_PS_PSSC); OK (0);
			case (RH_C_PORT_OVER_CURRENT):
					WR_RH_PORTSTAT (RH_PS_OCIC); OK (0);
			case (RH_C_PORT_RESET):
					WR_RH_PORTSTAT (RH_PS_PRSC); OK (0);
		}
		break;

	case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
		switch (wValue) {
			case (RH_PORT_SUSPEND):
					WR_RH_PORTSTAT (RH_PS_PSS ); OK (0);
			case (RH_PORT_RESET): /* BUG IN HUP CODE *********/
					if (RD_RH_PORTSTAT & RH_PS_CCS)
					    WR_RH_PORTSTAT (RH_PS_PRS);
					OK (0);
			case (RH_PORT_POWER):
					WR_RH_PORTSTAT (RH_PS_PPS ); OK (0);
			case (RH_PORT_ENABLE): /* BUG IN HUP CODE *********/
					if (RD_RH_PORTSTAT & RH_PS_CCS)
					    WR_RH_PORTSTAT (RH_PS_PES );
					OK (0);
		}
		break;

	case RH_SET_ADDRESS: gohci->rh.devnum = wValue; OK(0);

	case RH_GET_DESCRIPTOR://0x680
		switch ((wValue & 0xff00) >> 8) {
			case (0x01): /* device descriptor */
				len = min_t(unsigned int,
					  leni,
					  min_t(unsigned int,
					      sizeof (root_hub_dev_des),
					      wLength));
				data_buf = root_hub_dev_des; OK(len);
			case (0x02): /* configuration descriptor */
				len = min_t(unsigned int,
					  leni,
					  min_t(unsigned int,
					      sizeof (root_hub_config_des),
					      wLength));
				data_buf = root_hub_config_des; OK(len);
			case (0x03): /* string descriptors */	//到这里
				if(wValue==0x0300) {
					len = min_t(unsigned int,
						  leni,
						  min_t(unsigned int,
						      sizeof (root_hub_str_index0),
						      wLength));
					data_buf = root_hub_str_index0;
					OK(len);
				}
				if(wValue==0x0301) {
					len = min_t(unsigned int,
						  leni,
						  min_t(unsigned int,
						      sizeof (root_hub_str_index1),
						      wLength));
					data_buf = root_hub_str_index1;
					OK(len);
			}
			default:
				stat = USB_ST_STALLED;
		}
		break;

	case RH_GET_DESCRIPTOR | RH_CLASS:
	    {
		    u32 temp = roothub_a (gohci);

		    data_buf [0] = 9;		/* min length; */
		    data_buf [1] = 0x29;
		    data_buf [2] = temp & RH_A_NDP;
		    data_buf [3] = 0;
		    if (temp & RH_A_PSM)	/* per-port power switching? */
			data_buf [3] |= 0x1;
		    if (temp & RH_A_NOCP)	/* no overcurrent reporting? */
			data_buf [3] |= 0x10;
		    else if (temp & RH_A_OCPM)	/* per-port overcurrent reporting? */
			data_buf [3] |= 0x8;

		    /* corresponds to data_buf[4-7] */
		    datab [1] = 0;
		    data_buf [5] = (temp & RH_A_POTPGT) >> 24;
		    temp = roothub_b (gohci);
		    data_buf [7] = temp & RH_B_DR;
		    if (data_buf [2] < 7) {
			data_buf [8] = 0xff;
		    } else {
			data_buf [0] += 2;
			data_buf [8] = (temp & RH_B_DR) >> 8;
			data_buf [10] = data_buf [9] = 0xff;
		    }

		    len = min_t(unsigned int, leni,
			      min_t(unsigned int, data_buf [0], wLength));
		    OK (len);
		}

	case RH_GET_CONFIGURATION:	*(u8 *) data_buf = 0x01; OK (1);

	case RH_SET_CONFIGURATION:	
		//printf("ohci_submit_rh_msg dstling2\n");
		//WR_RH_STAT (0x10000); 
		{
			//printf("WR:status %#8x,gohci->regs:0x%08x", 0x10000,gohci->regs);
			//printf("WR:status %#8x,gohci->regs->roothub.status:0x%08x", 0x10000,gohci->regs->roothub.status);
			writel((0x10000), &gohci->regs->roothub.status);
		}
		len = 0; 
		break;

	default:
		dbg ("unsupported root hub command");
		stat = USB_ST_STALLED;
	}
#ifdef	DEBUG
	//ohci_dump_roothub (gohci, 1);
#else
	wait_ms(1);
#endif
	wait_ms(10);

	//printf("ohci_submit_rh_msg data:0x%08x, data_buf:0x%08x, len:0x%08x,leni:0x%08x\n",data, data_buf, len,leni);
	len = min_t(int, len, leni);
	if (data != data_buf)
	    memcpy (data, data_buf, len);
	dev->act_len = len;
	dev->status = stat;

#ifdef DEBUG
	//if (transfer_len)
	//	urb_priv.actual_length = transfer_len;
#else
	wait_ms(1);
#endif

	return stat;
}



static int ohci_submit_bulk_msg(struct usb_device *dev, unsigned long pipe, void *buffer,int transfer_len)
{
	int s;

	dbg("submit_bulk_msg %x/%d\n", buffer, transfer_len);
	if((u32)buffer & 0x0f)
		printf("bulk buffer %x/%d not aligned\n", buffer, transfer_len);	
	s = submit_common_msg(dev, pipe, buffer, transfer_len, NULL, 0);
	dbg("submit_bulk_msg END\n"); 

	return s;
}

static int ohci_submit_control_msg(struct usb_device *dev, unsigned long pipe,
				void *buffer, int transfer_len, struct devrequest *setup)
{
	int maxsize = usb_maxpacket(dev, pipe);
	struct ohci *gohci = dev->hc_private;

	dbg("submit_control_msg(%d) %x/%d,gohci->rh.devnum:%d\n", (pipe>>8)&0x7f, buffer, transfer_len,gohci->rh.devnum);
#if 1
	wait_ms(1);
#endif
	if (!maxsize) {
		err("submit_control_message: pipesize for pipe %lx is zero",
			pipe);
		return -1;
	}
	if (((pipe >> 8) & 0x7f) == gohci->rh.devnum) {
		gohci->rh.dev = dev;
		dbg("root hub - redirect,dev->hc_private:0x%08x\n",dev->hc_private);
		return ohci_submit_rh_msg(dev, pipe, buffer, transfer_len,
			setup);
	}

	return submit_common_msg(dev, pipe, buffer, transfer_len, setup, 0);//hub下面设备到这里了
}

static int ohci_submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer,
		int transfer_len, int interval)
{
	int s;

	dbg("submit int(%08x)\n", dev);
	s = submit_common_msg(dev, pipe, buffer, transfer_len, NULL, interval);
	return s;
}


struct usb_ops ohci_usb_op = {
	.submit_bulk_msg	= 	ohci_submit_bulk_msg,
	.submit_control_msg	= 	ohci_submit_control_msg,
	.submit_int_msg		= 	ohci_submit_int_msg,
};




static int hc_start (ohci_t * ohci)
{
	u32 mask;
	unsigned int fminterval;

	ohci->disabled = 1;

	/* Tell the controller where the control and bulk lists are
	 * The lists are empty now. */

	writel (0, &ohci->regs->ed_controlhead);
	writel (0, &ohci->regs->ed_bulkhead);
	writel (0, &ohci->regs->ed_periodcurrent);

	{
		writel ((u32)(vtophys(ohci->hcca)), &ohci->regs->hcca); /* a reset clears this */
	}

	fminterval = 0x2edf;
	writel ((fminterval * 8) / 10, &ohci->regs->periodicstart);    
    fminterval |= ((((fminterval - 210) * 6) / 7) << 16);
	writel (fminterval, &ohci->regs->fminterval);
	writel (0x628, &ohci->regs->lsthresh);

	/* start controller operations */
	ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
	ohci->disabled = 0;
	writel (ohci->hc_control, &ohci->regs->control);

	/* disable all interrupts */
	mask = (OHCI_INTR_SO | OHCI_INTR_WDH | OHCI_INTR_SF | OHCI_INTR_RD |
			OHCI_INTR_UE | OHCI_INTR_FNO | OHCI_INTR_RHSC |
			OHCI_INTR_OC | OHCI_INTR_MIE);
	writel (mask, &ohci->regs->intrdisable);

#ifdef	OHCI_USE_NPS
	/* required for AMD-756 and some Mac platforms */
	writel ((roothub_a (ohci) | RH_A_NPS) & ~RH_A_PSM,
		&ohci->regs->roothub.a);
	writel (RH_HS_LPSC, &ohci->regs->roothub.status);
#endif	/* OHCI_USE_NPS */

//#define mdelay(n) do {unsigned long msec=(n); while (msec--) udelay(1000);} while(0)
	/* POTPGT delay is bits 24-31, in 2 ms units. */
	//mdelay((roothub_a (ohci) >> 23) & 0x1fe);
	dbg("hc_start wait_ms:%d\n",(roothub_a (ohci) >> 23) & 0x1fe);
	wait_ms((roothub_a (ohci) >> 23) & 0x1fe);

	/* connect the virtual root hub */
	ohci->rh.devnum = 0;

	return 0;
}

static void hc_release_ohci (ohci_t *ohci)
{
	dbg ("USB HC release ohci usb-%s", ohci->slot_name);

	if (!ohci->disabled)
		hc_reset (ohci);
}




int usb_lowlevel_init(ohci_t *gohci)
{

	struct ohci_hcca *hcca = NULL;
	struct ohci_device *ohci_dev = NULL;
	td_t *gtd = NULL;
	unsigned char *tmpbuf;

	dbg("in usb_lowlevel_init\n");

	{
		//hcca = malloc(sizeof(*gohci->hcca));
		hcca = malloc_align(sizeof(*gohci->hcca),256);
		memset(hcca, 0, sizeof(*hcca));
		pci_sync_cache(gohci->sc_pc, (vm_offset_t)hcca, sizeof(*hcca), SYNC_W);//dstling
	}
	
	dbg("usb_lowlevel_init hcca:0x%08x\n",hcca);
	/* align the storage */
	//主机控制器通信区（HCCA）是一个256字节对齐的内存数据结构，被系统软件用于与HC进行通信
	//https://blog.csdn.net/weixin_33858336/article/details/89999349
	if ((u32)&hcca[0] & 0xff) 
	{
		err("HCCA not aligned!! %x\n", hcca);
		return -1;
	}

	{
		//ohci_dev = malloc(sizeof *ohci_dev);
		ohci_dev = malloc_align(sizeof *ohci_dev,32);
		memset(ohci_dev, 0, sizeof(struct ohci_device));
	}
	if ((u32)&ohci_dev->ed[0] & 0x1f) {
		err("EDs not aligned!!");
		return -1;
	}

	if((gohci->flags & 0x80)) 
	{
		ohci_dev->cpu_ed = ohci_dev->ed;
	} 
	else 
	{
		pci_sync_cache(gohci->sc_pc, (vm_offset_t)ohci_dev->ed, sizeof(ohci_dev->ed),SYNC_W);//dstling
		ohci_dev->cpu_ed = (ed_t *)CACHED_TO_UNCACHED(&ohci_dev->ed);
	}

	{
		gtd = malloc_align(sizeof(td_t) * (NUM_TD+1),32);
		memset(gtd, 0, sizeof(td_t) * (NUM_TD + 1));
		pci_sync_cache(gohci->sc_pc, (vm_offset_t)gtd, sizeof(td_t)*(NUM_TD+1), SYNC_W);//dstling
	}

	if ((u32)gtd & 0x0f) {
		err("TDs not aligned!!");
		return -1;
	}

	{
		gohci->hcca = (struct ohci_hcca*)CACHED_TO_UNCACHED(hcca);
		gohci->gtd = (td_t *)CACHED_TO_UNCACHED(gtd);
		gohci->ohci_dev = ohci_dev;
	}

	{
		tmpbuf = malloc_align(512,32);
		if(tmpbuf == NULL){
			printf("No mem for control buffer\n");
			goto errout;
		}	
		if((u32)tmpbuf & 0x1f)
			printf("Malloc return not cache line aligned\n");
		memset(tmpbuf, 0, 512);
		pci_sync_cache(gohci->sc_pc, (vm_offset_t)tmpbuf,  512, SYNC_W);//dstling
		gohci->control_buf = (unsigned char*)CACHED_TO_UNCACHED(tmpbuf);
	}

	{
		tmpbuf = malloc_align(64,32);//dstling
		if(tmpbuf == NULL){
			printf("No mem for setup buffer\n");
			goto errout;
		}	
		if((u32)tmpbuf & 0x1f)
			printf("Malloc return not cache line aligned\n");
		memset(tmpbuf, 0, 64);
		pci_sync_cache(tmpbuf, (vm_offset_t)tmpbuf, 64, SYNC_W);//dstling
		gohci->setup = (unsigned char *)CACHED_TO_UNCACHED(tmpbuf);
	}

	gohci->disabled = 1;
	gohci->sleeping = 0;
	gohci->irq = -1;
	gohci->regs = (struct ohci_regs *)(gohci->sc_sh | 0xA0000000);//dstling


	dbg("OHCI: regs base %x\n", gohci->regs);

	//gohci->flags = 0;
	gohci->slot_name = "Godson";

	dbg("OHCI revision: 0x%08x\n"
	       "  RH: a: 0x%08x b: 0x%08x\n",
	       readl(&gohci->regs->revision),
	       readl(&gohci->regs->roothub.a), readl(&gohci->regs->roothub.b));

	if (hc_reset (gohci) < 0)
		goto errout;

	/* FIXME this is a second HC reset; why?? */
	writel (gohci->hc_control = OHCI_USB_RESET, &gohci->regs->control);
	wait_ms (10);

	if (hc_start (gohci) < 0)
		goto errout;

#ifdef	DEBUG
	//ohci_dump (gohci, 1);
#else
	wait_ms(1);
#endif

	dbg("OHCI %x initialized ok\n", gohci);
	return 0;

errout:
	err("OHCI initialization error\n");
	hc_release_ohci (gohci);
	/* Initialization failed */
	return -1;
}



static struct ohci *ohci_root;
extern struct rt_thread *rt_current_thread;

//struct device *parent, struct device *self, void *aux
void lohci_attach_entry(void *parameter)
{
	/*
	register long level;
	if(rt_current_thread!=NULL)//说明此时线程已经运作
	{
		level=rt_hw_interrupt_disable();
		rt_current_thread=NULL;//无论如何要停止线程调度
	}
	*/
	printf("Usb lohci init.\n");
	
	ohci_root=malloc_align(sizeof(struct ohci)+256,256);
	memset(ohci_root,0,sizeof(struct ohci)+256);
	dbg("usb lohci init,ohci_root:0x%08x\n",ohci_root);
	
	struct ohci *ohci =(struct ohci*)ohci_root; //(struct ohci*)self;
	static int ohci_dev_index = 0;

	/*end usb reset*/
#if defined(LS1ASOC)
	/* enable USB */
	*(volatile int *)0xbfd00420 &= ~0x200000;
	/*ls1a usb reset stop*/
	*(volatile int *)0xbff10204 |= 0x40000000;
#elif defined(LS1BSOC) /* LS1BSOC */
	/* enable USB */
	*(volatile int *)0xbfd00424 &= ~0x800;
	/*ls1b usb reset stop*/
	*(volatile int *)0xbfd00424 |= 0x80000000;
#elif defined(LS1CSOC)
	*(volatile int *)0xbfd00424 &= ~(1 << 31);
	wait_ms(100);
	*(volatile int *)0xbfd00424 |= (1 << 31);//USBHOST模块软件复位
#endif

	/* Or we just return false in the match function */
	if(ohci_dev_index >= MAX_OHCI_C) {
		printf("Exceed max controller limits\n");
		return;
	}

	usb_ohci_dev[ohci_dev_index++] = ohci;
	
	//struct confargs *cf = aux;
	ohci->sc_sh = 0xbfe28000;//(bus_space_handle_t)cf->ca_baseaddr;
	//ohci->regs=0xbfe28000;
	usb_lowlevel_init(ohci);
	ohci->hc.uop = &ohci_usb_op;
	/*
	 * Now build the device tree
	 */
	//TAILQ_INSERT_TAIL(&host_controller, &ohci->hc, hc_list);

	//ohci->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, hc_interrupt, ohci,self->dv_xname);
	
	ohci->child_dev = usb_alloc_new_device(ohci);//先申请一个根usb_device空间 
	struct usb_hc *hc = ohci->child_dev->hc_private;
	dbg("lohci_attach,ohci:0x%08x,ohci->regs:0x%08x,hc->port_mask:0x%08x\n",ohci,ohci->regs,hc->port_mask);
	hc->port_mask=0;
	ohci->ed_controltail = NULL;

    /*do the enumeration of  the USB devices attached to the USB HUB(here root hub) 
    ports.*/
    usb_new_device(ohci->child_dev);
	usb_tree();
	THREAD_DEAD_EXIT;	
}







