#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__

#define ETHERNET_MTU		1500

#define ETHIF_LINK_AUTOUP	0x0000
#define ETHIF_LINK_PHYUP	0x0100

#define MAX_ADDR_LEN 6

struct eth_device
{
	unsigned char name[RT_NAME_MAX];
    /* inherit from rt_device */
    //struct rt_device parent;

    /* network interface for lwip */
    struct netif *netif;
    struct rt_semaphore tx_ack;

    unsigned int flags;
    unsigned char  link_changed;
    unsigned char  link_status;

    /* eth device interface */
    struct pbuf* (*eth_rx)(struct eth_device* dev);
    long (*eth_tx)(struct eth_device* dev, struct pbuf* p);
	
    unsigned char dev_addr[MAX_ADDR_LEN];//mac地址
	
    struct rt_timer link_timer;
    struct rt_timer rx_poll_timer;
	
    void *priv;//adapter
};

#endif

