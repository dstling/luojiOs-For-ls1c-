
#include <buildconfig.h>
#include <types.h>

#include <stdio.h>
#include "rtdef.h"
#include "shell.h"

#include "filesys.h"

#include "usb.h"
#include "usb_defs.h"
#include "usb_ohci.h"

//#define DEBUG
//#define SHOW_INFO
//#define USB_DEBUG
//#define USB_HUB_DEBUG

//#define USB_ATTACH_INFO_BUG//打印usb探测信息

#ifdef	USB_ATTACH_INFO_BUG
#define	USB_ATTACH_INFO(format,args...) printf("attach info: " format "", ## args)
#else
#define	USB_ATTACH_INFO(fmt,args...)
#endif



#define err(format, arg...) printf("ERROR: " format "\n", ## arg)

#ifdef DEBUG
#define dbg(format, arg...) printf("DEBUG: " format "\n", ## arg)
#else
#define dbg(format, arg...) 
#endif /* DEBUG */

#ifdef SHOW_INFO
#define info(format, arg...) printf("INFO: " format "\n", ## arg)
#else
#define info(format, arg...) 
#endif

#ifdef	USB_DEBUG
#define	USB_PRINTF(fmt,args...)			printf (fmt ,##args)
#else
#define USB_PRINTF(fmt,args...)
#endif

#ifdef	USB_HUB_DEBUG
#define	USB_HUB_PRINTF(fmt,args...)		printf (fmt ,##args)
#else
#define	USB_HUB_PRINTF(fmt,args...)
#endif




#define USB_BUFSIZ	512
#define	UNCONF	1		/* print " not configured\n" */
#define config_found(d, a, p)	config_found_sm((d), (a), (p), NULL)

struct usb_device usb_dev[USB_MAX_DEVICE];//usb.c中定义
static int asynch_allowed;
static struct devrequest setup_packet;

SLIST_HEAD(usb_found_devs, usb_found_dev) usb_found_devs = SLIST_HEAD_INITIALIZER(usb_found_devs);//所有发现的可以使用的usb设备，usb0,usb1

//struct hostcontroller host_controller;//usb.c中定义

SLIST_HEAD(usbdriver_list, usb_driver) usbdrivers= SLIST_HEAD_INITIALIZER(usbdrivers);//usb.c中定义

void usb_disable_asynch(int disable)
{
	asynch_allowed=!disable;
}


/*===========================================================================
*
*FUNTION: usb_set_interface
*
*DESCRIPTION: This function is used to set interface number to interface.
*
*PARAMETERS:
*          [IN] dev: a pointer to the USB device information struct.
*          [IN] interface: the number of interface.
*          [IN] alternate: used to set the value field of setup packet
*
*RETURN VALUE: 0 if ok,< 0 if error.
*
*===========================================================================*/
int usb_set_interface(struct usb_device *dev, int interface, int alternate)
{
	struct usb_interface_descriptor *if_face = NULL;
	int ret, i;

	for (i = 0; i < dev->config.bNumInterfaces; i++) {
		if (dev->config.if_desc[i].bInterfaceNumber == interface) {
			if_face = &dev->config.if_desc[i];
			break;
		}
	}
	if (!if_face) {
		printf("selecting invalid interface %d", interface);
		return -1;
	}

	if ((ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
	    USB_REQ_SET_INTERFACE, USB_RECIP_INTERFACE, alternate,
	    interface, NULL, 0, USB_CNTL_TIMEOUT * 5)) < 0)
		return ret;

	return 0;
}

int submit_int_msg(struct usb_device *dev, unsigned long pipe, void *buffer, int transfer_len, int interval)
{
	struct usb_hc *hc = dev->hc_private;

	return hc->uop->submit_int_msg(dev, pipe, buffer, transfer_len, interval);
}

/*===========================================================================
*
*FUNTION: usb_clear_halt
*
*DESCRIPTION: This function is used to clear an endpoint.
*
*PARAMETERS:
*          [IN] dev:  a pointer to the USB device information struct.
*          [IN] pipe:  describe the property of a pipe,more details about it,
*                      please see the usb.h. 
*
*RETURN VALUE: 0 means ok -1 if failed.
*
*===========================================================================*/
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe)|(usb_pipein(pipe)<<7);

	result = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0, endp, NULL, 0, USB_CNTL_TIMEOUT * 3);

	/* don't clear if failed */
	if (result < 0)
		return result;

	/*
	 * NOTE: we do not get status and verify reset was successful
	 * as some devices are reported to lock up upon this check..
	 */

	usb_endpoint_running(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));

	/* toggle is reset on clear */
	usb_settoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe), 0);
	return 0;
}

/*===========================================================================
*
*FUNTION: submit_bulk_msg
*
*DESCRIPTION: This function is used to submit a Bulk Message and wait for 
*             completion by calling into specific host controller.This is a
*             function with synchronous behevior. 
*
*PARAMETERS:
*          [IN] dev:  a pointer to the USB device the message belongs to.
*          [IN] pipe:  describe the property of a pipe,more details about it,
*                      please see the usb.h. 
*          [IN] buffer:an all-purpose pointer to the data that would be returned 
*                      through usb channel.
*          [IN] transfer_len: the length of data to be transfered.
*
*RETURN VALUE: returns 0 if Ok or -1 if Error.
*
*===========================================================================*/
int submit_bulk_msg(struct usb_device *dev, unsigned long pipe, void *buffer,int transfer_len)
{
	struct usb_hc *hc= dev->hc_private;

	return hc->uop->submit_bulk_msg(dev, pipe, buffer, transfer_len);
}

/*===========================================================================
*
*FUNTION: usb_bulk_msg
*
*DESCRIPTION: This function is used to submit a Bulk Message by wrapping
*             submit_bulk_msg.
*
*PARAMETERS:
*          [IN] dev:  a pointer to the USB device the message belongs to.
*          [IN] pipe: describe the property of a pipe,more details about it,
*                      please see the usb.h. 
*          [IN] data: an all-purpose pointer to the data that would be returned 
*                      through usb channel.
*          [IN] len: the length of data to be transfered.
*          [OUT] actual_length: the actual length of data transfered. 
*          [IN] timeout: the number of ms during which to check whether the 
*                       submited control message has been finished.
*RETURN VALUE: same as the return of submit_int_msg.
*
*===========================================================================*/
int usb_bulk_msg(struct usb_device *dev, unsigned int pipe,
			void *data, int len, int *actual_length, int timeout)
{
	if (len < 0)
		return -1;
	//assert(actual_length != NULL);
	dev->status=USB_ST_NOT_PROC; /*not yet processed */
	submit_bulk_msg(dev,pipe,data,len);


	/*while(timeout--) {
		if(!((volatile unsigned long)dev->status & USB_ST_NOT_PROC))
			break;
		wait_ms(1);
	}*/
  
	//assert(actual_length != NULL);
	*actual_length=dev->act_len;
	if(dev->status==0)
		return 0;
	else
		return -1;
}

int usb_maxpacket(struct usb_device *dev,unsigned long pipe)
{
	if((pipe & USB_DIR_IN)==0) /* direction is out -> use emaxpacket out */
		return(dev->epmaxpacketout[((pipe>>15) & 0xf)]);
	else
		return(dev->epmaxpacketin[((pipe>>15) & 0xf)]);
}

int dev_index=0;
struct usb_device * usb_alloc_new_device(void *hc_private)
{
	int i;
	USB_PRINTF("New Device %d\n",dev_index);
	if(dev_index==USB_MAX_DEVICE) 
	{
		printf("ERROR, too many USB Devices, max=%d\n",USB_MAX_DEVICE);
		return NULL;
	}
	usb_dev[dev_index].devnum=dev_index+1; /* default Address is 0, real addresses start with 1 */
	usb_dev[dev_index].maxchild=0;
	for(i=0;i<USB_MAXCHILDREN;i++)
		usb_dev[dev_index].children[i]=NULL;//它自己的子设备全部设为空
	usb_dev[dev_index].parent=NULL;
	dev_index++;
	USB_PRINTF("usb_alloc_new_device,hc_private:0x%08x\n",hc_private);
	usb_dev[dev_index-1].hc_private = hc_private; //device is linked to this controller 
	return &usb_dev[dev_index-1];
}

int usbprint(void *aux, const char *pnp)
{
	struct usb_device *dev = aux;
	if (pnp)
		printf("drive at %s", pnp);
	printf(" devnum %d, Product %s\n", dev->devnum, dev->prod);
	return (UNCONF);
}

int usb_get_descriptor(struct usb_device *dev, unsigned char type, unsigned char index, void *buf, int size)
{
	int res;
	USB_PRINTF("usb_get_descriptor \n");	
 	res = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(type << 8) + index, 0,
			buf, size, USB_CNTL_TIMEOUT);
	return res;
}

int usb_control_msg(struct usb_device *dev, unsigned int pipe,
			unsigned char request, unsigned char requesttype,
			unsigned short value, unsigned short index,
			void *data, unsigned short size, int timeout)
{
	if((timeout==0)&&(!asynch_allowed)) /* request for a asynch control pipe is not allowed */
		return -1;

	/* set setup command */
	setup_packet.requesttype = requesttype;	//USB_DIR_IN
	setup_packet.request = request;			//USB_REQ_GET_DESCRIPTOR
	setup_packet.value = swap_16(value);
	setup_packet.index = swap_16(index);
	setup_packet.length = swap_16(size);
 	//USB_PRINTF("usb_control_msg: request: 0x%X, requesttype: 0x%X\nvalue 0x%X index 0x%X length 0x%X\n",
		//request,requesttype,value,index,size);
	dev->status=USB_ST_NOT_PROC; /*not yet processed */

	submit_control_msg(dev,pipe,data,size,&setup_packet);
	/*
	if(data!=NULL)
		dataHexPrint2(data,size,0,"usb_control_msg data");
	*/
	if(timeout==0) 
		return (int)size;

	while(timeout--) 
	{
		if(!((volatile unsigned long)dev->status & USB_ST_NOT_PROC))
			break;
		wait_ms(1);
	} 
    
	if(dev->status==0)
		return dev->act_len;
	else {
		return -1;
	}
}

int submit_control_msg(struct usb_device *dev, unsigned long pipe,
				void * buffer, int transfer_len, struct devrequest *setup)
{
	struct usb_hc *hc = dev->hc_private;
	assert(hc!=NULL);
	assert(hc->uop!=NULL);
	return hc->uop->submit_control_msg(dev, pipe, buffer, transfer_len, setup);
}

int usb_set_address(struct usb_device *dev)
{
	int res;

	USB_PRINTF("set address %d\n",dev->devnum);
	res=usb_control_msg(dev, usb_snddefctrl(dev),
		USB_REQ_SET_ADDRESS, 0,
		(dev->devnum),0,
		NULL,0, USB_CNTL_TIMEOUT);
	return res;
}

int usb_get_configuration_no(struct usb_device *dev,unsigned char *buffer,int cfgno)
{
 	int result;
	unsigned int tmp;
	struct usb_config_descriptor *config;


	config=(struct usb_config_descriptor *)&buffer[0];
	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, 8);
	if (result < 8) {
		if (result < 0)
			printf("unable to get descriptor, error %lX\n",dev->status);
		else
			printf("config descriptor too short (expected %i, got %i)\n",8,result);
		return -1;
	}
	tmp=swap_16(config->wTotalLength);

	if (tmp > USB_BUFSIZ) {
		USB_PRINTF("usb_get_configuration_no: failed to get descriptor - too long: %d\n",
			tmp);
		return -1;
	}

	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, tmp);
	USB_PRINTF("get_conf_no %d Result %d, wLength %d\n",cfgno,result,tmp);
	return result;
}

int usb_set_maxpacket(struct usb_device *dev)
{
	int i,ii,b;
	struct usb_endpoint_descriptor *ep;

	for(i=0; i<dev->config.bNumInterfaces;i++) {
		for(ii=0; ii<dev->config.if_desc[i].bNumEndpoints; ii++) {
			ep=&dev->config.if_desc[i].ep_desc[ii];
			b=ep->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;

			if((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)==USB_ENDPOINT_XFER_CONTROL) {	/* Control => bidirectional */
				dev->epmaxpacketout[b] = ep->wMaxPacketSize;
				dev->epmaxpacketin [b] = ep->wMaxPacketSize;
				USB_PRINTF("##Control EP epmaxpacketout/in[%d] = %d\n",b,dev->epmaxpacketin[b]);
			}
			else {
				if ((ep->bEndpointAddress & 0x80)==0) { /* OUT Endpoint */
					if(ep->wMaxPacketSize > dev->epmaxpacketout[b]) {
						dev->epmaxpacketout[b] = ep->wMaxPacketSize;
						USB_PRINTF("##EP epmaxpacketout[%d] = %d\n",b,dev->epmaxpacketout[b]);
					}
				}
				else  { /* IN Endpoint */
					if(ep->wMaxPacketSize > dev->epmaxpacketin[b]) {
						dev->epmaxpacketin[b] = ep->wMaxPacketSize;
						USB_PRINTF("##EP epmaxpacketin[%d] = %d\n",b,dev->epmaxpacketin[b]);
					}
				} /* if out */
			} /* if control */
		} /* for each endpoint */
	}
	return 0;
}

int usb_parse_config(struct usb_device *dev, unsigned char *buffer, int cfgno)
{
	struct usb_descriptor_header *head;
	int index,ifno,epno;
	ifno=-1;
	epno=-1;

	dev->configno=cfgno;
	head =(struct usb_descriptor_header *)&buffer[0];
	if(head->bDescriptorType!=USB_DT_CONFIG) {
		int i;
		printf(" ERROR: NOT USB_CONFIG_DESC %x\n",head->bDescriptorType);
		for(i=0; i<sizeof(struct usb_config_descriptor); i++)	
		{
			printf("%02x ", buffer[i]);
			if((i+1) % 8 == 0)
				printf("\n");	
		}
		printf("\n");
		return -1;
	}
	memcpy(&dev->config,buffer,buffer[0]);
	dev->config.wTotalLength=swap_16(dev->config.wTotalLength);
	dev->config.no_of_if=0;

	index=dev->config.bLength;
	/* Ok the first entry must be a configuration entry, now process the others */
	head=(struct usb_descriptor_header *)&buffer[index];
	while(index+1 < dev->config.wTotalLength) {
		switch(head->bDescriptorType) {
			case USB_DT_INTERFACE:
				ifno=dev->config.no_of_if;
				dev->config.no_of_if++; /* found an interface desc, increase numbers */
				memcpy(&dev->config.if_desc[ifno],&buffer[index],buffer[index]); /* copy new desc */
				dev->config.if_desc[ifno].no_of_ep=0;

				break;
			case USB_DT_ENDPOINT:
				epno=dev->config.if_desc[ifno].no_of_ep;
				dev->config.if_desc[ifno].no_of_ep++; /* found an endpoint */
				memcpy(&dev->config.if_desc[ifno].ep_desc[epno],&buffer[index],buffer[index]);
				dev->config.if_desc[ifno].ep_desc[epno].wMaxPacketSize
					=swap_16(dev->config.if_desc[ifno].ep_desc[epno].wMaxPacketSize);
				USB_PRINTF("if %d, ep %d\n",ifno,epno);
				break;
			default:
				if(head->bLength==0)
					return 1;
				USB_PRINTF("unknown Description Type : %x\n",head->bDescriptorType);
				{
					int i;
					unsigned char *ch;
					ch=(unsigned char *)head;
					for(i=0;i<head->bLength; i++)
						USB_PRINTF("%02X ",*ch++);
					USB_PRINTF("\n\n\n");
				}
				break;
		}
		index+=head->bLength;
		head=(struct usb_descriptor_header *)&buffer[index];
	}
	return 1;
}


int usb_set_configuration(struct usb_device *dev, int configuration)
{
	int res;
	USB_PRINTF("set configuration %d\n",configuration);
	/* set setup command */
	res=usb_control_msg(dev, usb_sndctrlpipe(dev,0),
		USB_REQ_SET_CONFIGURATION, 0,
		configuration,0,
		NULL,0, USB_CNTL_TIMEOUT);
	if(res==0) {
		dev->toggle[0] = 0;
		dev->toggle[1] = 0;
		return 0;
	}
	else
		return -1;
}


static struct usb_hub_device hub_dev[USB_MAX_HUB];
static int usb_hub_index;

struct usb_hub_device *usb_hub_allocate(void)
{
	if(usb_hub_index<USB_MAX_HUB) {
		return &hub_dev[usb_hub_index++];
	}
	printf("ERROR: USB_MAX_HUB (%d) reached\n",USB_MAX_HUB);
	return NULL;
}

#define MAX_TRIES 10



int usb_hub_probe(struct usb_device *dev, int ifnum)
{
	struct usb_interface_descriptor *iface;
	struct usb_endpoint_descriptor *ep;
	int ret;

	iface = &dev->config.if_desc[ifnum];
	/* Is it a hub? */
	if (iface->bInterfaceClass != USB_CLASS_HUB)
		return -1;
	/* Some hubs have a subclass of 1, which AFAICT according to the */
	/*  specs is not defined, but it works */
	if ((iface->bInterfaceSubClass != 0) &&(iface->bInterfaceSubClass != 1))
		return -1;
	/* Multiple endpoints? What kind of mutant ninja-hub is this? */
	if (iface->bNumEndpoints != 1)
		return -1;
	ep = &iface->ep_desc[0];
	/* Output endpoint? Curiousier and curiousier.. */
	if (!(ep->bEndpointAddress & USB_DIR_IN))
		return -1;
	/* If it's not an interrupt endpoint, we'd better punt! */
	if ((ep->bmAttributes & 3) != 3)
		return -1;
	/* We found a hub */
	ret=usb_hub_configure(dev);
	if(ret==0)
		USB_ATTACH_INFO("USB hub found.\n");
	else
		USB_ATTACH_INFO("USB hub check error.\n");
	return ret;
}

static void usb_hub_power_on(struct usb_hub_device *hub)
{
	int i;
	struct usb_device *dev;

	dev=hub->pusb_dev;
	/* Enable power to the ports */
	USB_HUB_PRINTF("enabling power on all ports\n");
	for (i = 0; i < dev->maxchild; i++) {
		usb_set_port_feature(dev, i + 1, USB_PORT_FEAT_POWER);
		USB_HUB_PRINTF("port %d returns %lX\nhub->desc.bPwrOn2PwrGood * 2:%d\n",i+1,dev->status,hub->desc.bPwrOn2PwrGood * 2);
		wait_ms(hub->desc.bPwrOn2PwrGood * 2);//
	}
}


/*===========================================================================
*
*FUNTION: usb_port_reset
*
*DESCRIPTION: This function is used to USB hub ports.
*
*PARAMETERS:
*          [IN] dev: a pointer to the USB device information struct.
*          [IN] port: the index number of USB hub port 
*          [OUT] portstat: a pointer to the USB hub port status information.
*
*RETURN VALUE: none.
*
*===========================================================================*/
static int hub_port_reset(struct usb_device *dev, int port,
			unsigned short *portstat)
{
	int tries;
	struct usb_port_status portsts;
	unsigned short portstatus, portchange;


	USB_HUB_PRINTF("hub_port_reset: resetting port %d...\n", port);
	for(tries=0;tries<MAX_TRIES;tries++) 
	{

		usb_set_port_feature(dev, port + 1, USB_PORT_FEAT_RESET);
		wait_ms(200);

		if (usb_get_port_status(dev, port + 1, &portsts)<0) {
			USB_HUB_PRINTF("get_port_status failed status %lX\n",dev->status);
			return -1;
		}
		portstatus = swap_16(portsts.wPortStatus);
		portchange = swap_16(portsts.wPortChange);
		USB_HUB_PRINTF("portstatus %x, change %x, %s\n", portstatus ,portchange,
			portstatus&(1<<USB_PORT_FEAT_LOWSPEED) ? "Low Speed" : "High Speed");
		USB_HUB_PRINTF("STAT_C_CONNECTION = %d STAT_CONNECTION = %d  USB_PORT_STAT_ENABLE %d\n",
			(portchange & USB_PORT_STAT_C_CONNECTION) ? 1 : 0,
			(portstatus & USB_PORT_STAT_CONNECTION) ? 1 : 0,
			(portstatus & USB_PORT_STAT_ENABLE) ? 1 : 0);
		if ((portchange & USB_PORT_STAT_C_CONNECTION) ||
		    !(portstatus & USB_PORT_STAT_CONNECTION))
			return -1;

		if (portstatus & USB_PORT_STAT_ENABLE) 
			break;
		wait_ms(200);
	}

	if (tries==MAX_TRIES) 
	{
		USB_HUB_PRINTF("Cannot enable port %i after %i retries, disabling port.\n", port+1, MAX_TRIES);
		USB_HUB_PRINTF("Maybe the USB cable is bad?\n");
		return -1;
	}

	usb_clear_port_feature(dev, port + 1, USB_PORT_FEAT_C_RESET);
	wait_ms(200);
	*portstat = portstatus;
	//USB_HUB_PRINTF("hub_port_reset: resetting port %d...end\n", port);
	//没有这句话好像延时不够啊
	//delay_ms(1000);
	return 0;
}

//设置探测到的新设备 会再次调用usb_new_device
struct usb_device *usb_hub_port_connect_change(struct usb_device *dev, int port)
{
	struct usb_device *usb=NULL;
	struct usb_port_status portsts;
	unsigned short portstatus, portchange;

	/* Check status */
	if (usb_get_port_status(dev, port + 1, &portsts)<0) {
		USB_HUB_PRINTF("get_port_status failed\n");
		return NULL;
	}

	portstatus = swap_16(portsts.wPortStatus);
	portchange = swap_16(portsts.wPortChange);
	USB_HUB_PRINTF("portstatus %x, change %x, %s\n", portstatus, portchange,portstatus&(1<<USB_PORT_FEAT_LOWSPEED) ? "Low Speed" : "High Speed");

	/* Clear the connection change status */
	usb_clear_port_feature(dev, port + 1, USB_PORT_FEAT_C_CONNECTION);

	/* Disconnect any existing devices under this port */
	if (((!(portstatus & USB_PORT_STAT_CONNECTION)) &&(!(portstatus & USB_PORT_STAT_ENABLE)))|| (dev->children[port])) 
	{
		USB_HUB_PRINTF("usb_disconnect(&hub->children[port]);\n");
		/* Return now if nothing is connected */
		if (!(portstatus & USB_PORT_STAT_CONNECTION))
			return NULL;
	}
	wait_ms(10);
	/* Reset the port */
	if (hub_port_reset(dev, port, &portstatus) < 0) 
	{
		printf("cannot reset port %i!?\n", port + 1);
		return NULL;
	}
	wait_ms(10);
	/* Allocate a new device struct for it */
	assert(dev->hc_private!=NULL);
	usb=usb_alloc_new_device(dev->hc_private);
	usb->slow = (portstatus & USB_PORT_STAT_LOW_SPEED) ? 1 : 0;

	dev->children[port] = usb;
	usb->parent=dev;

	/* hack to rename usb device*/
	usb->port = port;
	/* Run it through the hoops (find a driver, etc) */
	//list_thread();
	if (usb_new_device(usb)) 
	{
		/* Woops, disable the port */
		USB_HUB_PRINTF("hub: disabling port %d\n", port + 1);
		usb_clear_port_feature(dev, port + 1, USB_PORT_FEAT_ENABLE);
	}
	
	return usb;
}

int usb_hub_configure(struct usb_device *dev)
{
	unsigned char buffer[USB_BUFSIZ];
	unsigned char *bitmap;
	struct usb_hub_descriptor *descriptor;
	struct usb_hub_status *hubsts;
	int i;
	struct usb_hub_device *hub;
	struct usb_hc *hc = dev->hc_private;

	memset(buffer,0,USB_BUFSIZ);
	
	/* "allocate" Hub device */
	hub=usb_hub_allocate();
	if(hub==NULL)
		return -1;
	hub->pusb_dev=dev;
	
	descriptor = (struct usb_hub_descriptor *)buffer;
	/* Get the the hub descriptor */
	if (usb_get_hub_descriptor(dev, buffer, 4) < 0) 
	{
		USB_HUB_PRINTF("usb_hub_configure: failed to get hub descriptor, giving up %lX\n",dev->status);
		return -1;
	}

	/* silence compiler warning if USB_BUFSIZ is > 256 [= sizeof(char)] */
	i = descriptor->bLength;
	if (i > USB_BUFSIZ) 
	{
		USB_HUB_PRINTF("usb_hub_configure: failed to get hub descriptor - too long: %d\n",descriptor->bLength);
		return -1;
	}
	if (usb_get_hub_descriptor(dev, buffer, descriptor->bLength) < 0) 
	{
		USB_HUB_PRINTF("usb_hub_configure: failed to get hub descriptor 2nd giving up %lX\n",dev->status);
		return -1;
	}

	memcpy((unsigned char *)&hub->desc,buffer,descriptor->bLength);
	/* adjust 16bit values */
	hub->desc.wHubCharacteristics=swap_16(descriptor->wHubCharacteristics);

	/* set the bitmap */
	bitmap=(unsigned char *)&hub->desc.DeviceRemovable[0];
	memset(bitmap,0xff,(USB_MAXCHILDREN+1+7)/8); /* devices not removable by default */

	bitmap=(unsigned char *)&hub->desc.PortPowerCtrlMask[0];
	memset(bitmap,0xff,(USB_MAXCHILDREN+1+7)/8); /* PowerMask = 1B */

	for(i=0;i<((hub->desc.bNbrPorts + 1 + 7)/8);i++) 
	{
		hub->desc.DeviceRemovable[i]=descriptor->DeviceRemovable[i];
	}
	
	//dataHexPrint2(&hub->desc,sizeof(struct usb_hub_descriptor),0,"usb_hub_configure hub->desc3");
	
	for(i=0;i<((hub->desc.bNbrPorts + 1 + 7)/8);i++) 
	{
		hub->desc.DeviceRemovable[i]=descriptor->PortPowerCtrlMask[i];
	}
	dev->maxchild = descriptor->bNbrPorts;
	USB_ATTACH_INFO("%s have %d ports detected\n",dev->prod,dev->maxchild);

	switch (hub->desc.wHubCharacteristics & HUB_CHAR_LPSM) 
	{
		case 0x00:
			USB_HUB_PRINTF("ganged power switching\n");
			break;
		case 0x01:
			USB_HUB_PRINTF("individual port power switching\n");
			break;
		case 0x02:
		case 0x03:
			USB_HUB_PRINTF("unknown reserved power switching mode\n");
			break;
	}

	if (hub->desc.wHubCharacteristics & HUB_CHAR_COMPOUND)
		USB_HUB_PRINTF("part of a compound device\n");
	else
		USB_HUB_PRINTF("standalone hub\n");

	switch (hub->desc.wHubCharacteristics & HUB_CHAR_OCPM)
	{
		case 0x00:
			USB_HUB_PRINTF("global over-current protection\n");
			break;
		case 0x08:
			USB_HUB_PRINTF("individual port over-current protection\n");
			break;
		case 0x10:
		case 0x18:
			USB_HUB_PRINTF("no over-current protection\n");
      break;
	}
	USB_HUB_PRINTF("power on to power good time: %dms\n", descriptor->bPwrOn2PwrGood * 2);
	USB_HUB_PRINTF("hub controller current requirement: %dmA\n", descriptor->bHubContrCurrent);

	//dataHexPrint2(&hub->desc,sizeof(struct usb_hub_descriptor),0,"usb_hub_configure hub->desc5");
	for (i = 0; i < dev->maxchild; i++)
		USB_HUB_PRINTF("port %d is%s removable\n", i + 1,hub->desc.DeviceRemovable[(i + 1)/8] & (1 << ((i + 1)%8)) ? " not" : "");
	if (sizeof(struct usb_hub_status) > USB_BUFSIZ) 
	{
		USB_HUB_PRINTF("usb_hub_configure: failed to get Status - too long: %d\n",descriptor->bLength);
		return -1;
	}

	if (usb_get_hub_status(dev, buffer) < 0) 
	{
		USB_HUB_PRINTF("usb_hub_configure: failed to get Status %lX\n",dev->status);
		return -1;
	}
	hubsts = (struct usb_hub_status *)buffer;
	USB_HUB_PRINTF("get_hub_status returned status %X, change %X\n",swap_16(hubsts->wHubStatus),swap_16(hubsts->wHubChange));
	USB_HUB_PRINTF("local power source is %s\n",(swap_16(hubsts->wHubStatus) & HUB_STATUS_LOCAL_POWER) ? "lost (inactive)" : "good");
	USB_HUB_PRINTF("%sover-current condition exists\n",(swap_16(hubsts->wHubStatus) & HUB_STATUS_OVERCURRENT) ? "" : "no ");

	usb_hub_power_on(hub);
	for (i = 0; i < dev->maxchild; i++)//检测hub的每一个usb插口是否有设备连接 
	{
		struct usb_port_status portsts;
		unsigned short portstatus, portchange;
		USB_HUB_PRINTF("hc->port_mask:%d\n",hc->port_mask);
		if((hc->port_mask & (1 << i)))//端口对应关系
			continue;
		if (usb_get_port_status(dev, i + 1, &portsts) < 0) 
		{
			USB_HUB_PRINTF("get_port_status failed\n");
			continue;
		}
		portstatus = swap_16(portsts.wPortStatus);
		portchange = swap_16(portsts.wPortChange);
		USB_HUB_PRINTF("Port %d Status %X Change %X\n",i+1,portstatus,portchange);
		
		if (portchange & USB_PORT_STAT_C_CONNECTION) 
		{
			//设置探测到的新设备 会再次调用usb_new_device
			struct usb_device * new_usb_device= usb_hub_port_connect_change(dev, i);
			if(new_usb_device!=NULL)
				USB_ATTACH_INFO("%s's port %d had connected one usb device.new usb device:%s\n",dev->prod, i + 1,new_usb_device->prod);
			else
				USB_ATTACH_INFO("%s's port %d had connected one usb device check error.\n",dev->prod, i + 1);
		}
		
		if (portchange & USB_PORT_STAT_C_ENABLE) 
		{
			USB_HUB_PRINTF("port %d enable change, status %x\n", i + 1, portstatus);
			usb_clear_port_feature(dev, i + 1, USB_PORT_FEAT_C_ENABLE);

			/* EM interference sometimes causes bad shielded USB devices to
			 * be shutdown by the hub, this hack enables them again.
			 * Works at least with mouse driver */
			if (!(portstatus & USB_PORT_STAT_ENABLE) &&(portstatus & USB_PORT_STAT_CONNECTION) && (dev->children[i])) 
			{
				USB_HUB_PRINTF("already running port %i disabled by hub (EMI?), re-enabling...\n",i + 1);
				usb_hub_port_connect_change(dev, i);
			}
		}
		if (portstatus & USB_PORT_STAT_SUSPEND) 
		{
			USB_HUB_PRINTF("port %d suspend change\n", i + 1);
			usb_clear_port_feature(dev, i + 1,  USB_PORT_FEAT_SUSPEND);
		}

		if (portchange & USB_PORT_STAT_C_OVERCURRENT) 
		{
			USB_HUB_PRINTF("port %d over-current change\n", i + 1);
			usb_clear_port_feature(dev, i + 1, USB_PORT_FEAT_C_OVER_CURRENT);
			usb_hub_power_on(hub);
		}

		if (portchange & USB_PORT_STAT_C_RESET) 
		{
			USB_HUB_PRINTF("port %d reset change\n", i + 1);
			usb_clear_port_feature(dev, i + 1, USB_PORT_FEAT_C_RESET);
		}
	} /* end for i all ports */

	return 0;
}

int usb_get_hub_descriptor(struct usb_device *dev, void *data, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
		USB_REQ_GET_DESCRIPTOR, USB_DIR_IN | USB_RT_HUB,
		USB_DT_HUB << 8, 0, data, size, USB_CNTL_TIMEOUT);
}

int usb_get_hub_status(struct usb_device *dev, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_HUB, 0, 0,
			data, sizeof(struct usb_hub_status), USB_CNTL_TIMEOUT);
}

int usb_set_port_feature(struct usb_device *dev, int port, int feature)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_SET_FEATURE, USB_RT_PORT, feature, port, NULL, 0, USB_CNTL_TIMEOUT);
}


int usb_clear_port_feature(struct usb_device *dev, int port, int feature)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
		USB_REQ_CLEAR_FEATURE, USB_RT_PORT, feature, port, NULL, 0, USB_CNTL_TIMEOUT);
}


static int isprint (unsigned char ch)
{
	if (ch >= 32 && ch < 127)
		return (1);

	return (0);
}

static void usb_try_string_workarounds(unsigned char *buf, int *length)
{
	int newlength, oldlength = *length;

	for (newlength = 2; newlength + 1 < oldlength; newlength += 2)
		if (!isprint(buf[newlength]) || buf[newlength + 1])
			break;

	if (newlength > 2) {
		buf[0] = newlength;
		*length = newlength;
	}
}
//								dev, 							0,					 0, 			buf, 	255
int usb_get_string(struct usb_device *dev, unsigned short langid, unsigned char index, void *buf, int size)
{
	int i;
	int result;

	for (i = 0; i < 3; ++i) {
		/* some devices are flaky */
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(USB_DT_STRING << 8) + index, langid, buf, size,
			USB_CNTL_TIMEOUT);

		if (result > 0)
			break;
	}

	return result;
}

static int usb_string_sub(struct usb_device *dev, unsigned int langid,
		unsigned int index, unsigned char *buf)
{
	int rc;

	/* Try to read the string descriptor by asking for the maximum
	 * possible number of bytes */
	 //						dev, 0, 0, tbuf
	rc = usb_get_string(dev, langid, index, buf, 255);

	/* If that failed try to read the descriptor length, then
	 * ask for just that many bytes */
	if (rc < 2) {
		rc = usb_get_string(dev, langid, index, buf, 2);
		if (rc == 2)
			rc = usb_get_string(dev, langid, index, buf, buf[0]);
	}

	if (rc >= 2) {
		if (!buf[0] && !buf[1])
			usb_try_string_workarounds(buf, &rc);

		/* There might be extra junk at the end of the descriptor */
		if (buf[0] < rc)
			rc = buf[0];

		rc = rc - (rc & 1); /* force a multiple of two */
	}

	if (rc < 2)
		rc = -1;

	return rc;
}

int usb_string(struct usb_device *dev, int index, char *buf, int size)
{
	unsigned char mybuf[USB_BUFSIZ];
	unsigned char *tbuf;
	int err;
	unsigned int u, idx;

	if (size <= 0 || !buf || !index)
		return -1;
	buf[0] = 0;
	tbuf=&mybuf[0];
	
	memset(tbuf, 0, USB_BUFSIZ);

	/* get langid for strings if it's not yet known */
	if (!dev->have_langid) {
		err = usb_string_sub(dev, 0, 0, tbuf);
		if (err < 0) {
			USB_PRINTF("error getting string descriptor 0 (error=%x)\n",dev->status);
			return -1;
		} else if (tbuf[0] < 4) {
			USB_PRINTF("string descriptor 0 too short\n");
			return -1;
		} else {
			dev->have_langid = -1;
			dev->string_langid = tbuf[2] | (tbuf[3]<< 8);
				/* always use the first langid listed */
			USB_PRINTF("USB device number %d default language ID 0x%x\n",
				dev->devnum, dev->string_langid);
		}
		wait_ms(10);
	}

	err = usb_string_sub(dev, dev->string_langid, index, tbuf);
	if (err < 0)
		return err;

	size--;		/* leave room for trailing NULL char in output buffer */
	for (idx = 0, u = 2; u < err; u += 2) {
		if (idx >= size)
			break;
		if (tbuf[u+1])			/* high byte */
			buf[idx++] = '?';  /* non-ASCII character */
		else
			buf[idx++] = tbuf[u];
	}
	buf[idx] = 0;
	err = idx;
	return err;
}



int usb_get_port_status(struct usb_device *dev, int port, void *data)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_STATUS, USB_DIR_IN | USB_RT_PORT, 0, port,
			data, sizeof(struct usb_hub_status), USB_CNTL_TIMEOUT);
}




void usb_find_drivers(struct usb_device *dev)
{
	struct usb_driver *p;
	
	SLIST_FOREACH(p, &usbdrivers, d_list)
	{
		if(p->probe(dev, 0) == 1)
			break;	
	}
}

void usb_storage_notify(struct usb_device *dev)
{
	struct usb_hc *hc = dev->hc_private;

	if(hc->notify)
		hc->notify(dev, dev->port);
}


/*
void init_controller_list(void)
{
	TAILQ_INIT(&host_controller);
}
*/
extern struct rt_thread *rt_current_thread;

void usb_new_device_insert_list(struct usb_found_dev *new_usb_dev)
{
	SLIST_INSERT_HEAD(&usb_found_devs, new_usb_dev, i_next);
}

#include "../filesys/devfs.h"
int find_usb_device(char*name,struct biodev *biodevThis)
{
	struct usb_found_dev *usbDev;
	SLIST_FOREACH(usbDev, &usb_found_devs, i_next)
	{
		if(striequ(name, usbDev->name))
		{
			usbDev->biodevThis=biodevThis;
			biodevThis->dev_type=BLOCK_DEV_USB_STOR_FLAG;
			biodevThis->dev_index=usbDev->index;
			return usbDev->index;
		}
	}
	return -1;
}

int usb_new_device(struct usb_device *dev)
{
	int addr, err,tmp;
	unsigned char tmpbuf[USB_BUFSIZ];
	register long level;
/*	
	if(rt_current_thread!=NULL)//thread系统调度start
	{
		level = rt_hw_interrupt_disable();
	}
//*/
	memset(tmpbuf, 0, USB_BUFSIZ);
	USB_PRINTF("usb_new_device add dev addr:0x%08x\n",dev);
	
	dev->descriptor.bMaxPacketSize0 = 8;  /* Start off at 8 bytes  */
	dev->maxpacketsize = 0;		/* Default to 8 byte max packet size */
	dev->epmaxpacketin [0] = 8;
	dev->epmaxpacketout[0] = 8;

	/* We still haven't set the Address yet */
	addr = dev->devnum;
	dev->devnum = 0;

	/* and this is the old and known way of initializing devices */
	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev->descriptor, 8);
	if (err < 8) 
	{
		printf("\n      USB device not responding, giving up (status=%lX)\n",dev->status);
		return 1;
	}

	dev->epmaxpacketin [0] = dev->descriptor.bMaxPacketSize0;
	dev->epmaxpacketout[0] = dev->descriptor.bMaxPacketSize0;
	switch (dev->descriptor.bMaxPacketSize0) 
	{
		case 8: dev->maxpacketsize = 0; break;
		case 16: dev->maxpacketsize = 1; break;
		case 32: dev->maxpacketsize = 2; break;
		case 64: dev->maxpacketsize = 3; break;
	}
	dev->devnum = addr;
#ifdef USB_DEBUG
	//printf(" bLength = %x\n", ((struct usb_device_descriptor *)CACHED_TO_UNCACHED(&dev->descriptor))->bLength);
	printf(" bLength = %x\n", dev->descriptor.bLength);
	printf(" bDescriptorType =%x\n", dev->descriptor.bDescriptorType);
	printf(" bcdUSB = %#4x\n", dev->descriptor.bcdUSB);
	printf(" bDeviceClass =%x\n", dev->descriptor.bDeviceClass);
	printf(" bDeviceSubClass =%x\n", dev->descriptor.bDeviceSubClass);
	printf(" bDeviceProtocol =%x\n", dev->descriptor.bDeviceProtocol);
	printf(" bMaxPacketSize0 =%x\n", dev->descriptor.bMaxPacketSize0);
#endif

	err = usb_set_address(dev); /* set address */
	if (err < 0) 
	{
		printf("\n      USB device not accepting new address (error=%lX)\n", dev->status);
		return 1;
	}

	wait_ms(10);	/* Let the SET_ADDRESS settle */

	tmp = sizeof(dev->descriptor);

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev->descriptor, sizeof(dev->descriptor));
	if (err < tmp) 
	{
		if (err < 0)
			printf("unable to get device descriptor (error=%d)\n",err);
		else
			printf("USB device descriptor short read (expected %i, got %i)\n",tmp,err);
		return 1;
	}
#ifdef USB_DEBUG
	//printf(" bLength = %x\n", ((struct usb_device_descriptor *)CACHED_TO_UNCACHED(&dev->descriptor))->bLength);
	printf(" bLength = %x\n", dev->descriptor.bLength);
	printf(" bDescriptorType =%x\n", dev->descriptor.bDescriptorType);
	printf(" bcdUSB = %#4x\n", dev->descriptor.bcdUSB);
	printf(" bDeviceClass =%x\n", dev->descriptor.bDeviceClass);
	printf(" bDeviceSubClass =%x\n", dev->descriptor.bDeviceSubClass);
	printf(" bDeviceProtocol =%x\n", dev->descriptor.bDeviceProtocol);
	printf(" bMaxPacketSize0 =%x\n", dev->descriptor.bMaxPacketSize0);
	printf(" idVendor =%x\n",   dev->descriptor.idVendor);
	printf(" idProduct =%x\n", dev->descriptor.idProduct);
	printf(" bcdDevice =%x\n", dev->descriptor.bcdDevice);
	printf(" iManufacturer=%x\n", dev->descriptor.iManufacturer);
	printf(" iProduct =%x\n", dev->descriptor.iProduct);
	printf(" iSerialNumber=%x\n", dev->descriptor.iSerialNumber);
	printf(" bNumConfigurations=%x\n", dev->descriptor.bNumConfigurations);
#endif

	/* correct le values */
	dev->descriptor.bcdUSB=swap_16(dev->descriptor.bcdUSB);
	dev->descriptor.idVendor=swap_16(dev->descriptor.idVendor);
	dev->descriptor.idProduct=swap_16(dev->descriptor.idProduct);
	dev->descriptor.bcdDevice=swap_16(dev->descriptor.bcdDevice);
	/* only support for one config for now */
	memset(tmpbuf, 0, sizeof(tmpbuf));
	usb_get_configuration_no(dev,&tmpbuf[0],0);
	usb_parse_config(dev,&tmpbuf[0],0);
#ifdef USB_DEBUG
	{
		struct usb_config_descriptor *p =  tmpbuf;
		int i;

		printf("bLength=%x\n",  p->bLength);
		printf("bDescriptorType=%x\n", p->bDescriptorType);
		printf("wTotalLength=%x\n",    p->wTotalLength);
		printf("bNumInterfaces=%x\n",  p->bNumInterfaces);
		printf("bConfigurationValue=%x\n", p->bConfigurationValue);
		printf("iConfiguration=%x\n",  p->iConfiguration);
		printf("bmAttributes=%x\n",    p->bmAttributes);
		printf("MaxPower=%x\n",  p->MaxPower);
		for(i=p->bLength; i<p->wTotalLength; i++){
				printf("%02x ", tmpbuf[i]);
		}	
		printf("\n");
	}
#endif
	usb_set_maxpacket(dev);
	/* we set the default configuration here */
	if (usb_set_configuration(dev, dev->config.bConfigurationValue)) //dstling
	{
		printf("failed to set default configuration len %d, status %lX\n",dev->act_len,dev->status);
		return -1;
	}
	USB_PRINTF("new device strings: Mfr=%d, Product=%d, SerialNumber=%d\n",
		dev->descriptor.iManufacturer, dev->descriptor.iProduct, dev->descriptor.iSerialNumber);
	memset(dev->mf, '\0', sizeof(dev->mf));
	memset(dev->prod, '\0', sizeof(dev->prod));
	memset(dev->serial, '\0', sizeof(dev->serial));
	if (dev->descriptor.iManufacturer)
		usb_string(dev, dev->descriptor.iManufacturer, dev->mf, sizeof(dev->mf));
	if (dev->descriptor.iProduct)//hub new device strings: Mfr=0, Product=1, SerialNumber=0
		usb_string(dev, dev->descriptor.iProduct, dev->prod, sizeof(dev->prod));
	if (dev->descriptor.iSerialNumber)
		usb_string(dev, dev->descriptor.iSerialNumber, dev->serial, sizeof(dev->serial));

	if(dev->parent==NULL)
		USB_ATTACH_INFO("\nNew root usb device found.\n");
	else
		USB_ATTACH_INFO("\nNew child usb device found at parent:%s\n",dev->parent->prod);
	
	USB_ATTACH_INFO("Manufacturer :%s\n", dev->mf);		//制造商
	USB_ATTACH_INFO("Product      :%s\n", dev->prod);	//产品名称
	USB_ATTACH_INFO("SerialNumber :%s\n", dev->serial);//序列号
	
	/* now prode if the device is a hub */
	USB_ATTACH_INFO("usb_new_device:%s probe\n",dev->prod);
	//优先检测是不是个hub
	//hub下挂设备循环探测 会再次调用这个函数
	//会循环探测hub下挂载的其他usb设备
	int ret=usb_hub_probe(dev,0);
	if(ret==0)
		;
	else//添加usb设备
	{
		
	}
	
	//printf("usb_new_device:%s match\n",dev->prod);
	//if(usb_storage_match(dev->hc_private, NULL, dev))
	if(add_usb_storage_device(dev))//尝试识别usb storage设备
	{
		usb_storage_attach(dev->hc_private,NULL, dev);//
		usb_find_drivers(dev);
	}
	else
	{
		
	}
	
	//printf("usb_new_device:%s end.\n",dev->prod);

/*
	if(rt_current_thread!=NULL)//thread系统调度未start
	{
		rt_hw_interrupt_enable(level);
	}
//*/

	return 0;
}

char * usb_get_class_desc(unsigned char dclass)
{
	switch(dclass) {
		case USB_CLASS_PER_INTERFACE:
			return("See Interface");
		case USB_CLASS_AUDIO:
			return("Audio");
		case USB_CLASS_COMM:
			return("Communication");
		case USB_CLASS_HID:
			return("Human Interface");
		case USB_CLASS_PRINTER:
			return("Printer");
		case USB_CLASS_MASS_STORAGE:
			return("Mass Storage");
		case USB_CLASS_HUB:
			return("Hub");
		case USB_CLASS_DATA:
			return("CDC Data");
		case USB_CLASS_VENDOR_SPEC:
			return("Vendor specific");
		default :
			return("");
	}
}

/* shows the device tree recursively */
void usb_show_tree_graph(struct usb_device *dev,char *pre)
{
	int i,index;
	int has_child,last_child,port;

	index=strlen(pre);
	printf(" %s",pre);
	/* check if the device has connected children */
	has_child=0;
	for(i=0;i<dev->maxchild;i++) 
	{
		if (dev->children[i]!=NULL)
			has_child=1;
	}
	/* check if we are the last one */
	last_child=1;
	if (dev->parent!=NULL) {
		for(i=0;i<dev->parent->maxchild;i++) 
		{
			/* search for children */
			if (dev->parent->children[i]==dev) 
			{
				/* found our pointer, see if we have a little sister */
				port=i;
				while(i++<dev->parent->maxchild) 
				{
					if (dev->parent->children[i]!=NULL) 
					{
						/* found a sister */
						last_child=0;
						break;
					} /* if */
				} /* while */
			} /* device found */
		} /* for all children of the parent */
		printf("\b+-");
		/* correct last child */
		if (last_child) 
		{
			pre[index-1]=' ';
		}
	} /* if not root hub */
	else
		printf(" ");
	printf("%d ",dev->devnum);
	pre[index++]=' ';
	pre[index++]= has_child ? '|' : ' ';
	pre[index]=0;
	printf(" %s (%s, %dmA)\n",usb_get_class_desc(dev->config.if_desc[0].bInterfaceClass),
		dev->slow ? "1.5MBit/s" : "12MBit/s",dev->config.MaxPower * 2);
	if (strlen(dev->mf) ||
	   strlen(dev->prod) ||
	   strlen(dev->serial))
		printf(" %s  %s %s %s\n",pre,dev->mf,dev->prod,dev->serial);
	printf(" %s\n",pre);
	if (dev->maxchild>0) 
	{
		for(i=0;i<dev->maxchild;i++) 
		{
			if (dev->children[i]!=NULL) 
			{
				usb_show_tree_graph(dev->children[i],pre);
				pre[index]=0;
			}
		}
	}
}

void usb_show_tree(struct usb_device *dev)
{
	char preamble[32];

	if(dev == NULL)
		return;	
	memset(preamble,0,32);
	usb_show_tree_graph(dev,&preamble[0]);
}

void usb_tree(void)
{
	usb_show_tree(usb_get_dev_index(0));
}
FINSH_FUNCTION_EXPORT(usb_tree,list all usb device);


