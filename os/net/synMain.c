#include <stdio.h>
#include <ls1c_pin.h>

#include <rtdef.h>
#include <ls1c_irq.h>


#include "synopGMAC_Dev.h"
#include "mii.h"
#include "synopGMAC_Host.h"

#include <ethernetif.h>

#include <lwip/pbuf.h>

#define rt_kprintf printf

#define Gmac_base           0xbfe10000
u32 regbase = 0xbfe10000;
#define RMII

#define RT_USING_GMAC_INT_MODE


#define DEFAULT_MAC_ADDRESS {0x00, 0x55, 0x7B, 0xB5, 0x7D, 0xF7}

unsigned char default_MAC_addr[MAX_ADDR_LEN]=DEFAULT_MAC_ADDRESS;

struct eth_device  this_eth_dev;
char this_eth_dev_name[]="e0";
static u32 GMAC_Power_down;
static struct rt_semaphore sem_ack, sem_lock;

static int mdio_read(synopGMACPciNetworkAdapter *adapter, int addr, int reg)
{
    synopGMACdevice *gmacdev;
    u16 data;
    gmacdev = adapter->synopGMACdev;

    synopGMAC_read_phy_reg(gmacdev->MacBase, addr, reg, &data);
    return data;
}

static void mdio_write(synopGMACPciNetworkAdapter *adapter, int addr, int reg, int data)
{
    synopGMACdevice *gmacdev;
    gmacdev = adapter->synopGMACdev;
    synopGMAC_write_phy_reg(gmacdev->MacBase, addr, reg, data);
}



long rt_eth_tx(struct eth_device* device, struct pbuf *p)
{
    /* lock eth device */
    rt_sem_take(&sem_lock, RT_WAITING_FOREVER);

    DEBUG_MES("in %s\n", __FUNCTION__);

    s32 status;
    u32 pbuf;
    u64 dma_addr;
    u32 offload_needed = 0;
    u32 index;
    DmaDesc *dpr;
    struct eth_device *dev = (struct eth_device *) device;
    struct synopGMACNetworkAdapter *adapter;
    synopGMACdevice *gmacdev;
    adapter = (struct synopGMACNetworkAdapter *) dev->priv;
    if (adapter == NULL)
        return -1;

    gmacdev = (synopGMACdevice *) adapter->synopGMACdev;
    if (gmacdev == NULL)
        return -1;

    if (!synopGMAC_is_desc_owned_by_dma(gmacdev->TxNextDesc))
    {

        pbuf = (u32)plat_alloc_memory((u32)p->tot_len);
        //pbuf = (u32)pbuf_alloc(PBUF_LINK, p->len, PBUF_RAM);
        if (pbuf == 0)
        {
            printf("===error in alloc bf1\n");
            return -1;
        }

        DEBUG_MES("p->len = %d\n", p->len);
        pbuf_copy_partial(p, (void *)pbuf, p->tot_len, 0);
        dma_addr = plat_dma_map_single(gmacdev, (void *)pbuf, p->tot_len);

        status = synopGMAC_set_tx_qptr(gmacdev, dma_addr, p->tot_len, pbuf, 0, 0, 0, offload_needed, &index, dpr);
        if (status < 0)
        {
            printf("%s No More Free Tx Descriptors\n", __FUNCTION__);

            plat_free_memory((void *)pbuf);
            return -16;
        }
    }
    synopGMAC_resume_dma_tx(gmacdev);

    s32 desc_index;
    u32 data1, data2;
    u32 dma_addr1, dma_addr2;
    u32 length1, length2;
#ifdef ENH_DESC_8W
    u32 ext_status;
    u16 time_stamp_higher;
    u32 time_stamp_high;
    u32 time_stamp_low;
#endif
    do
    {
#ifdef ENH_DESC_8W
        desc_index = synopGMAC_get_tx_qptr(gmacdev, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2, &ext_status, &time_stamp_high, &time_stamp_low);
        synopGMAC_TS_read_timestamp_higher_val(gmacdev, &time_stamp_higher);
#else
        desc_index = synopGMAC_get_tx_qptr(gmacdev, &status, &dma_addr1, &length1, &data1, &dma_addr2, &length2, &data2);
#endif
        if (desc_index >= 0 && data1 != 0)
        {
#ifdef  IPC_OFFLOAD
            if (synopGMAC_is_tx_ipv4header_checksum_error(gmacdev, status))
            {
                rt_kprintf("Harware Failed to Insert IPV4 Header Checksum\n");
            }
            if (synopGMAC_is_tx_payload_checksum_error(gmacdev, status))
            {
                rt_kprintf("Harware Failed to Insert Payload Checksum\n");
            }
#endif

            plat_free_memory((void *)(data1));  //sw:   data1 = buffer1

            if (synopGMAC_is_desc_valid(status))
            {
                adapter->synopGMACNetStats.tx_bytes += length1;
                adapter->synopGMACNetStats.tx_packets++;
            }
            else
            {
                adapter->synopGMACNetStats.tx_errors++;
                adapter->synopGMACNetStats.tx_aborted_errors += synopGMAC_is_tx_aborted(status);
                adapter->synopGMACNetStats.tx_carrier_errors += synopGMAC_is_tx_carrier_error(status);
            }
        }
        adapter->synopGMACNetStats.collisions += synopGMAC_get_tx_collision_count(status);
    }
    while (desc_index >= 0);

    /* unlock eth device */
    rt_sem_release(&sem_lock);
//  rt_kprintf("output %d bytes\n", p->len);
    u32 test_data;
    test_data = synopGMACReadReg(gmacdev->DmaBase, DmaStatus);
    return RT_EOK;
}

struct pbuf *rt_eth_rx(struct eth_device* device)
{
    DEBUG_MES("in %s, name:%s\n", __FUNCTION__,device->name);
    struct eth_device *dev = device;
    struct synopGMACNetworkAdapter *adapter;
    synopGMACdevice *gmacdev;
//  struct PmonInet * pinetdev;
    s32 desc_index;
    int i;
    char *ptr;
    u32 bf1;
    u32 data1;
    u32 data2;
    u32 len;
    u32 status;
    u32 dma_addr1;
    u32 dma_addr2;
    struct pbuf *pbuf = RT_NULL;
    rt_sem_take(&sem_lock, RT_WAITING_FOREVER);

    adapter = (struct synopGMACNetworkAdapter *) dev->priv;
    if (adapter == NULL)
    {
        printf("%s : Unknown Device !!\n", __FUNCTION__);
        return NULL;
    }

    gmacdev = (synopGMACdevice *) adapter->synopGMACdev;
    if (gmacdev == NULL)
    {
        printf("%s : GMAC device structure is missing\n", __FUNCTION__);
        return NULL;
    }

    /*Handle the Receive Descriptors*/
    do
    {
        desc_index = synopGMAC_get_rx_qptr(gmacdev, &status, &dma_addr1, NULL, &data1, &dma_addr2, NULL, &data2);

        if (desc_index >= 0 && data1 != 0)
        {
            DEBUG_MES("Received Data at Rx Descriptor %d for skb 0x%08x whose status is %08x\n", desc_index, dma_addr1, status);

            if (synopGMAC_is_rx_desc_valid(status) || SYNOP_PHY_LOOPBACK)
            {
                pbuf = pbuf_alloc(PBUF_LINK, MAX_ETHERNET_PAYLOAD, PBUF_RAM);
                if (pbuf == 0) printf("===error in pbuf_alloc\n");


                dma_addr1 =  plat_dma_map_single(gmacdev, (void *)data1, RX_BUF_SIZE);
                len =  synopGMAC_get_rx_desc_frame_length(status); //Not interested in Ethernet CRC bytes
                memcpy(pbuf->payload, (char *)data1, len);
                DEBUG_MES("==get pkg len: %d\n", len);
            }
            else
            {
                DEBUG_MES("s: %08x\n", status);
                adapter->synopGMACNetStats.rx_errors++;
                adapter->synopGMACNetStats.collisions       += synopGMAC_is_rx_frame_collision(status);
                adapter->synopGMACNetStats.rx_crc_errors    += synopGMAC_is_rx_crc(status);
                adapter->synopGMACNetStats.rx_frame_errors  += synopGMAC_is_frame_dribbling_errors(status);
                adapter->synopGMACNetStats.rx_length_errors += synopGMAC_is_rx_frame_length_errors(status);
            }

            desc_index = synopGMAC_set_rx_qptr(gmacdev, dma_addr1, RX_BUF_SIZE, (u32)data1, 0, 0, 0);

            if (desc_index < 0)
            {
    #if SYNOP_RX_DEBUG
                printf("Cannot set Rx Descriptor for data1 %08x\n", (u32)data1);
    #endif
                plat_free_memory((void *)data1);
            }

        }
    }while(desc_index >= 0);
    rt_sem_release(&sem_lock);
    DEBUG_MES("%s : before return \n", __FUNCTION__);
    return pbuf;
}


static int rtl88e1111_config_init(synopGMACdevice *gmacdev)
{
    int retval, err;
    u16 data;

    DEBUG_MES("in %s\n", __FUNCTION__);
    synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x14, &data);
    data = data | 0x82;
    err = synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x14, data);
    synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x00, &data);
    data = data | 0x8000;
    err = synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x00, data);
#if SYNOP_PHY_LOOPBACK
    synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x14, &data);
    data = data | 0x70;
    data = data & 0xffdf;
    err = synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x14, data);
    data = 0x8000;
    err = synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x00, data);
    data = 0x5140;
    err = synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x00, data);
#endif
    if (err < 0)
        return err;
    return 0;
}

s32 synopGMAC_setup_tx_desc_queue(synopGMACdevice *gmacdev, u32 no_of_desc, u32 desc_mode)//no_of_desc=36
{
    s32 i;
    DmaDesc *bf1;

    DmaDesc *first_desc = NULL;

    dma_addr_t dma_addr;
    gmacdev->TxDescCount = 0;

	int memSize=sizeof(DmaDesc) * no_of_desc;
	//printf("synopGMAC_setup_tx_desc_queue memSize:%d\n",memSize);
    first_desc = (DmaDesc *)plat_alloc_consistent_dmaable_memory(gmacdev,memSize , &dma_addr);//36*DmaDesc
    if (first_desc == NULL)
    {
        rt_kprintf("Error in Tx Descriptors memory allocation\n");
        return -ESYNOPGMACNOMEM;
    }

    DEBUG_MES("tx_first_desc_addr = %p\n", first_desc);
    DEBUG_MES("dmaadr = %p\n", dma_addr);
    gmacdev->TxDescCount = no_of_desc;//36
    gmacdev->TxDesc      = first_desc;
    gmacdev->TxDescDma  = dma_addr;

    for (i = 0; i < gmacdev->TxDescCount; i++)//清零操作
    {
		//printf("synopGMAC_tx_desc_init_ring:%d\n",i);
		synopGMAC_tx_desc_init_ring(gmacdev->TxDesc + i, i == gmacdev->TxDescCount - 1);
#if SYNOP_TOP_DEBUG
        rt_kprintf("\n%02d %08x \n", i, (unsigned int)(gmacdev->TxDesc + i));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i))->status);
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->length));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->buffer1));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->buffer2));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->data1));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->data2));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->dummy1));
        rt_kprintf("%08x ", (unsigned int)((gmacdev->TxDesc + i)->dummy2));
#endif
    }

    gmacdev->TxNext = 0;
    gmacdev->TxBusy = 0;
    gmacdev->TxNextDesc = gmacdev->TxDesc;
    gmacdev->TxBusyDesc = gmacdev->TxDesc;
    gmacdev->BusyTxDesc  = 0;

    return -ESYNOPGMACNOERR;
}

s32 synopGMAC_setup_rx_desc_queue(synopGMACdevice *gmacdev, u32 no_of_desc, u32 desc_mode)
{
    s32 i;
    DmaDesc *bf1;
    DmaDesc *first_desc = NULL;

    dma_addr_t dma_addr;

    gmacdev->RxDescCount = 0;
    first_desc = (DmaDesc *)plat_alloc_consistent_dmaable_memory(gmacdev, sizeof(DmaDesc) * no_of_desc, &dma_addr);
    if (first_desc == NULL)
    {
        rt_kprintf("Error in Rx Descriptor Memory allocation in Ring mode\n");
        return -ESYNOPGMACNOMEM;
    }

    DEBUG_MES("rx_first_desc_addr = %p\n", first_desc);
    DEBUG_MES("dmaadr = %p\n", dma_addr);
    gmacdev->RxDescCount = no_of_desc;
    gmacdev->RxDesc      = (DmaDesc *)first_desc;
    gmacdev->RxDescDma   = dma_addr;

    for (i = 0; i < gmacdev->RxDescCount; i++)
    {
        synopGMAC_rx_desc_init_ring(gmacdev->RxDesc + i, i == gmacdev->RxDescCount - 1);

    }

    gmacdev->RxNext = 0;
    gmacdev->RxBusy = 0;
    gmacdev->RxNextDesc = gmacdev->RxDesc;
    gmacdev->RxBusyDesc = gmacdev->RxDesc;

    gmacdev->BusyRxDesc   = 0;

    return -ESYNOPGMACNOERR;
}


void synopGMAC_linux_cable_unplug_function(void *adaptr)
{
    s32 data;
    synopGMACPciNetworkAdapter *adapter = (synopGMACPciNetworkAdapter *)adaptr;
    synopGMACdevice            *gmacdev = adapter->synopGMACdev;
    struct ethtool_cmd cmd;

    //rt_kprintf("%s\n",__FUNCTION__);
    if (!mii_link_ok(&adapter->mii))
    {
        if (gmacdev->LinkState)
            printf("\r\nNo Link\r\n");
        gmacdev->DuplexMode = 0;
        gmacdev->Speed = 0;
        gmacdev->LoopBackMode = 0;
        gmacdev->LinkState = 0;
    }
    else
    {
        data = synopGMAC_check_phy_init(adapter);

        if (gmacdev->LinkState != data)
        {
            gmacdev->LinkState = data;
            synopGMAC_mac_init(gmacdev);
            printf("Link is up in %s mode\n", (gmacdev->DuplexMode == FULLDUPLEX) ? "FULL DUPLEX" : "HALF DUPLEX");
            if (gmacdev->Speed == SPEED1000)
                printf("Link is with 1000M Speed \r\n");
            if (gmacdev->Speed == SPEED100)
                printf("Link is with 100M Speed \n");
            if (gmacdev->Speed == SPEED10)
                printf("Link is with 10M Speed \n");
        }
    }
}

s32 synopGMAC_check_phy_init(synopGMACPciNetworkAdapter *adapter)
{
    struct ethtool_cmd cmd;
    synopGMACdevice            *gmacdev = adapter->synopGMACdev;

    if (!mii_link_ok(&adapter->mii))
    {
        gmacdev->DuplexMode = FULLDUPLEX;
        gmacdev->Speed      =   SPEED100;

        return 0;
    }
    else
    {
        mii_ethtool_gset(&adapter->mii, &cmd);

        gmacdev->DuplexMode = (cmd.duplex == DUPLEX_FULL)  ? FULLDUPLEX : HALFDUPLEX ;
        if (cmd.speed == SPEED_1000)
            gmacdev->Speed      =   SPEED1000;
        else if (cmd.speed == SPEED_100)
            gmacdev->Speed      =   SPEED100;
        else
            gmacdev->Speed      =   SPEED10;
    }

    return gmacdev->Speed | (gmacdev->DuplexMode << 4);
}


void eth_rx_irq(int irqno, void *param)//eth接收中断函数
{
    //struct rt_eth_dev *dev = &eth_dev;
	struct eth_device *eth_device =&this_eth_dev;
	
    struct synopGMACNetworkAdapter *adapter = eth_device->priv;
    DEBUG_MES("in %s\n", __FUNCTION__);
#ifdef RT_USING_GMAC_INT_MODE
    int i ;
    for (i = 0; i < 7200; i++)
        ;
#endif  /*RT_USING_GMAC_INT_MODE*/
    synopGMACdevice *gmacdev = (synopGMACdevice *)adapter->synopGMACdev;

    u32 interrupt, dma_status_reg;
    s32 status;
    u32 dma_addr;

    //rt_kprintf("irq i = %d\n", i++);
    dma_status_reg = synopGMACReadReg(gmacdev->DmaBase, DmaStatus);
    if (dma_status_reg == 0)
    {
        printf("dma_status ==0 \n");
        return;
    }

    //rt_kprintf("dma_status_reg is 0x%x\n", dma_status_reg);
    u32 gmacstatus;
    synopGMAC_disable_interrupt_all(gmacdev);
    gmacstatus = synopGMACReadReg(gmacdev->MacBase, GmacStatus);

    if (dma_status_reg & GmacPmtIntr)
    {
        printf("%s:: Interrupt due to PMT module\n", __FUNCTION__);
        //synopGMAC_linux_powerup_mac(gmacdev);
    }
    if (dma_status_reg & GmacMmcIntr)
    {
        printf("%s:: Interrupt due to MMC module\n", __FUNCTION__);
        DEBUG_MES("%s:: synopGMAC_rx_int_status = %08x\n", __FUNCTION__, synopGMAC_read_mmc_rx_int_status(gmacdev));
        DEBUG_MES("%s:: synopGMAC_tx_int_status = %08x\n", __FUNCTION__, synopGMAC_read_mmc_tx_int_status(gmacdev));
    }

    if (dma_status_reg & GmacLineIntfIntr)
    {
        printf("%s:: Interrupt due to GMAC LINE module\n", __FUNCTION__);
    }

    interrupt = synopGMAC_get_interrupt_type(gmacdev);
    //rt_kprintf("%s:Interrupts to be handled: 0x%08x\n",__FUNCTION__,interrupt);
    if (interrupt & synopGMACDmaError)
    {
        u8 mac_addr0[6];
        printf("%s::Fatal Bus Error Inetrrupt Seen\n", __FUNCTION__);

        memcpy(mac_addr0, eth_device->dev_addr, 6);
        synopGMAC_disable_dma_tx(gmacdev);
        synopGMAC_disable_dma_rx(gmacdev);

        synopGMAC_take_desc_ownership_tx(gmacdev);
        synopGMAC_take_desc_ownership_rx(gmacdev);

        synopGMAC_init_tx_rx_desc_queue(gmacdev);

        synopGMAC_reset(gmacdev);

        synopGMAC_set_mac_addr(gmacdev, GmacAddr0High, GmacAddr0Low, mac_addr0);
        synopGMAC_dma_bus_mode_init(gmacdev, DmaFixedBurstEnable | DmaBurstLength8 | DmaDescriptorSkip2);
        synopGMAC_dma_control_init(gmacdev, DmaStoreAndForward);
        synopGMAC_init_rx_desc_base(gmacdev);
        synopGMAC_init_tx_desc_base(gmacdev);
        synopGMAC_mac_init(gmacdev);
        synopGMAC_enable_dma_rx(gmacdev);
        synopGMAC_enable_dma_tx(gmacdev);

    }
    if (interrupt & synopGMACDmaRxNormal)
    {
        //DEBUG_MES("%s:: Rx Normal \n", __FUNCTION__);
        //synop_handle_received_data(netdev);
        eth_device_ready(eth_device);//在arch中定义
    }
    if (interrupt & synopGMACDmaRxAbnormal)
    {
        //rt_kprintf("%s::Abnormal Rx Interrupt Seen\n",__FUNCTION__);
        if (GMAC_Power_down == 0)
        {
            adapter->synopGMACNetStats.rx_over_errors++;
            synopGMACWriteReg(gmacdev->DmaBase, DmaStatus, 0x80);
            synopGMAC_resume_dma_rx(gmacdev);
        }
    }
    if (interrupt & synopGMACDmaRxStopped)
    {
        printf("%s::Receiver stopped seeing Rx interrupts\n", __FUNCTION__); //Receiver gone in to stopped state
    }

    if (interrupt & synopGMACDmaTxNormal)
    {
        DEBUG_MES("%s::Finished Normal Transmission \n", __FUNCTION__);
        //          synop_handle_transmit_over(netdev);
    }

    if (interrupt & synopGMACDmaTxAbnormal)
    {
        printf("%s::Abnormal Tx Interrupt Seen\n", __FUNCTION__);
    }
    if (interrupt & synopGMACDmaTxStopped)
    {
        TR("%s::Transmitter stopped sending the packets\n", __FUNCTION__);
        if (GMAC_Power_down == 0)    // If Mac is not in powerdown
        {
            synopGMAC_disable_dma_tx(gmacdev);
            synopGMAC_take_desc_ownership_tx(gmacdev);

            synopGMAC_enable_dma_tx(gmacdev);
            //      netif_wake_queue(netdev);
            TR("%s::Transmission Resumed\n", __FUNCTION__);
        }
    }
    /* Enable the interrrupt before returning from ISR*/
    synopGMAC_enable_interrupt(gmacdev, DmaIntEnable);

    return;
}


long eth_init()//rt_device_t device//在lwip tcpip thread中被调用
{
	struct eth_device *eth_device =&this_eth_dev;
    s32 status = 0;
    u64 dma_addr;
	
    struct synopGMACNetworkAdapter *adapter = eth_device->priv;
    synopGMACdevice *gmacdev = (synopGMACdevice *)adapter->synopGMACdev;
    synopGMAC_reset(gmacdev);
	
    synopGMAC_attach(gmacdev, (regbase + MACBASE), (regbase + DMABASE), DEFAULT_PHY_BASE, default_MAC_addr);

    synopGMAC_read_version(gmacdev);
    synopGMAC_set_mdc_clk_div(gmacdev, GmiiCsrClk3);
	
    gmacdev->ClockDivMdc = synopGMAC_get_mdc_clk_div(gmacdev);
    init_phy(adapter->synopGMACdev);

    DEBUG_MES("tx desc_queue\n");
    synopGMAC_setup_tx_desc_queue(gmacdev, TRANSMIT_DESC_SIZE, RINGMODE);
    synopGMAC_init_tx_desc_base(gmacdev);

    DEBUG_MES("rx desc_queue\n");
    synopGMAC_setup_rx_desc_queue(gmacdev, RECEIVE_DESC_SIZE, RINGMODE);
    synopGMAC_init_rx_desc_base(gmacdev);
    DEBUG_MES("DmaRxBaseAddr = %08x\n", synopGMACReadReg(gmacdev->DmaBase, DmaRxBaseAddr));

//  u32 dmaRx_Base_addr = synopGMACReadReg(gmacdev->DmaBase,DmaRxBaseAddr);
//  rt_kprintf("first_desc_addr = 0x%x\n", dmaRx_Base_addr);


    //synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength4 | DmaDescriptorSkip1);
    synopGMAC_dma_bus_mode_init(gmacdev, DmaBurstLength4 | DmaDescriptorSkip2);

    synopGMAC_dma_control_init(gmacdev, DmaStoreAndForward | DmaTxSecondFrame | DmaRxThreshCtrl128);

    status = synopGMAC_check_phy_init(adapter);
    synopGMAC_mac_init(gmacdev);

    synopGMAC_pause_control(gmacdev);

    u32 skb;
	//printf("eth_init 2\n");

    do
    {
        skb = (u32)plat_alloc_memory(RX_BUF_SIZE);      //should skb aligned here?
        if (skb == RT_NULL)
        {
            printf("ERROR in skb buffer allocation\n");
            break;
        }

        dma_addr = plat_dma_map_single(gmacdev, (void *)skb, RX_BUF_SIZE);  //获取 skb 的 dma 地址

        status = synopGMAC_set_rx_qptr(gmacdev, dma_addr, RX_BUF_SIZE, (u32)skb, 0, 0, 0);
        if (status < 0)
        {
            printf("status < 0!!\n");
            plat_free_memory((void *)skb);
        }
    }
    while (status >= 0 && (status < (RECEIVE_DESC_SIZE - 1)));
	//printf("eth_init 3\n");

    synopGMAC_clear_interrupt(gmacdev);

    synopGMAC_disable_mmc_tx_interrupt(gmacdev, 0xFFFFFFFF);
    synopGMAC_disable_mmc_rx_interrupt(gmacdev, 0xFFFFFFFF);
    synopGMAC_disable_mmc_ipc_rx_interrupt(gmacdev, 0xFFFFFFFF);

//  synopGMAC_disable_interrupt_all(gmacdev);
    synopGMAC_enable_interrupt(gmacdev, DmaIntEnable);
    synopGMAC_enable_dma_rx(gmacdev);
    synopGMAC_enable_dma_tx(gmacdev);

    plat_delay(DEFAULT_LOOP_VARIABLE);
    synopGMAC_check_phy_init(adapter);
    synopGMAC_mac_init(gmacdev);

    rt_timer_init(&eth_device->link_timer, "link_timer",
                  synopGMAC_linux_cable_unplug_function,
                  (void *)adapter,
                  RT_TICK_PER_SECOND,
                  2);//RT_TIMER_FLAG_PERIODIC

    rt_timer_start(&eth_device->link_timer);
    /* installl isr */
    DEBUG_MES("%s\n", __FUNCTION__);
    rt_hw_interrupt_install(LS1C_MAC_IRQ, eth_rx_irq, RT_NULL, "e0_isr");
    rt_hw_interrupt_umask(LS1C_MAC_IRQ);

    DEBUG_MES("eth_inited success!\n");

    return RT_EOK;
}




int init_phy(synopGMACdevice *gmacdev)
{
    u16 data=0;

    synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 2, &data);
	TR("init_phy DATA = %08x\n",data);
    /*set 88e1111 clock phase delay*/
    if (data == 0x141)
        rtl88e1111_config_init(gmacdev);
#if defined (RMII)
    else if (data == 0x8201)
    {
        //RTL8201
        data = 0x400;    // set RMII mode
        synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x19, data);
        synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x19, &data);
        TR("phy reg25 is %0x \n", data);

        data = 0x3100;    //set  100M speed
        synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x0, data);
    }
    else if (data == 0x0180 || data == 0x0181)
    {
        //DM9161
        synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x10, &data);
        data |= (1 << 8);  //set RMII mode
        synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x10, data); //set RMII mode
        synopGMAC_read_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x10, &data);
        TR("phy reg16 is 0x%0x \n", data);

        //  synopGMAC_read_phy_reg(gmacdev->MacBase,gmacdev->PhyBase,0x0,&data);
        //  data &= ~(1<<10);
        data = 0x3100;  //set auto-
        //data = 0x0100;    //set  10M speed
        synopGMAC_write_phy_reg(gmacdev->MacBase, gmacdev->PhyBase, 0x0, data);
    }
#endif

    return 0;
}
void hw_eth_init_thread_entry(void *parameter)
{
	u64 base_addr = Gmac_base;
	struct synopGMACNetworkAdapter *synopGMACadapter=NULL;
	static u8 mac_addr0[6] = DEFAULT_MAC_ADDRESS;
	int index;

	rt_sem_init(&sem_ack, "tx_ack", 1, RT_IPC_FLAG_FIFO);
	rt_sem_init(&sem_lock, "eth_lock", 1, RT_IPC_FLAG_FIFO);

	for (index = 21; index <= 30; index++)
	{
		pin_set_purpose(index, PIN_PURPOSE_OTHER);
		pin_set_remap(index, PIN_REMAP_DEFAULT);
	}
	pin_set_purpose(35, PIN_PURPOSE_OTHER);
	pin_set_remap(35, PIN_REMAP_DEFAULT);
	*((volatile unsigned int *)0xbfd00424) &= ~(7 << 28);
	*((volatile unsigned int *)0xbfd00424) |= (1 << 30); //wl rmii
	
	synopGMACadapter = (struct synopGMACNetworkAdapter *)plat_alloc_memory(sizeof(struct synopGMACNetworkAdapter));
	TR("synopGMACadapter addr:0x%08X\n",synopGMACadapter);
	if (!synopGMACadapter)
	{
		rt_kprintf("Error in Memory Allocataion, Founction : %s \n", __FUNCTION__);
	}
	memset((char *)synopGMACadapter, 0, sizeof(struct synopGMACNetworkAdapter));

	synopGMACadapter->synopGMACdev	  = NULL;

	synopGMACadapter->synopGMACdev = (synopGMACdevice *) plat_alloc_memory(sizeof(synopGMACdevice));
	TR("synopGMACadapter->synopGMACdev addr:0x%08X\n",synopGMACadapter->synopGMACdev);
	if (!synopGMACadapter->synopGMACdev)
	{
		rt_kprintf("Error in Memory Allocataion, Founction : %s \n", __FUNCTION__);
	}
	memset((char *)synopGMACadapter->synopGMACdev, 0, sizeof(synopGMACdevice));
	
	/*
	 * Attach the device to MAC struct This will configure all the required base addresses
	 * such as Mac base, configuration base, phy base address(out of 32 possible phys)
	 * */
	
	synopGMAC_attach(synopGMACadapter->synopGMACdev, (regbase + MACBASE), regbase + DMABASE, DEFAULT_PHY_BASE, mac_addr0);

	init_phy(synopGMACadapter->synopGMACdev);
	synopGMAC_reset(synopGMACadapter->synopGMACdev);

	/* MII setup */
	synopGMACadapter->mii.phy_id_mask = 0x1F;
	synopGMACadapter->mii.reg_num_mask = 0x1F;
	synopGMACadapter->mii.dev = synopGMACadapter;
	synopGMACadapter->mii.mdio_read = mdio_read;
	synopGMACadapter->mii.mdio_write = mdio_write;
	synopGMACadapter->mii.phy_id = synopGMACadapter->synopGMACdev->PhyBase;
	synopGMACadapter->mii.supports_gmii = mii_check_gmii_support(&synopGMACadapter->mii);
	
	/*
	memset(&eth_dev, 0, sizeof(eth_dev));
	eth_dev.iobase = base_addr;
	eth_dev.name = "e0";
	eth_dev.priv = synopGMACadapter;
	eth_dev.dev_addr[0] = mac_addr0[0];
	eth_dev.dev_addr[1] = mac_addr0[1];
	eth_dev.dev_addr[2] = mac_addr0[2];
	eth_dev.dev_addr[3] = mac_addr0[3];
	eth_dev.dev_addr[4] = mac_addr0[4];
	eth_dev.dev_addr[5] = mac_addr0[5];

	eth_dev.parent.parent.type			= RT_Device_Class_NetIf;
	eth_dev.parent.parent.init			= eth_init;
	eth_dev.parent.parent.open			= eth_open;
	eth_dev.parent.parent.close 		= eth_close;
	eth_dev.parent.parent.read			= eth_read;
	eth_dev.parent.parent.write 		= eth_write;
	eth_dev.parent.parent.control		= eth_control;
	eth_dev.parent.parent.user_data 	= RT_NULL;

	eth_dev.parent.eth_tx			 = rt_eth_tx;
	eth_dev.parent.eth_rx			 = rt_eth_rx;
	*/

	//eth_device_init(&(eth_dev.parent), "e0");

	//eth_device_linkchange(&eth_dev.parent, RT_TRUE);	 //linkup the e0 for lwip to check

	this_eth_dev.priv=synopGMACadapter;
	memcpy(this_eth_dev.dev_addr,default_MAC_addr,MAX_ADDR_LEN);//复制mac地址给设备

	unsigned char devname[]="e0Dstling";
	memcpy(this_eth_dev.name,devname,strlen(devname));
	this_eth_dev.eth_tx=rt_eth_tx;
	this_eth_dev.eth_rx=rt_eth_rx;

	eth_system_device_init();//提前初始换

	
	eth_device_init(&this_eth_dev, this_eth_dev_name);

	eth_device_linkchange(&this_eth_dev, 1);   //linkup the e0 for lwip to check
	
	
	//初始化lwip组件
	lwip_system_init();//lwip_init();//在lwip_system_init->tcpip_init中被初始化 

	return 0;
}


#include "shell.h"

int rt_hw_eth_init(void)
{
	thread_join_init("hw_eth_init_thread_entry",hw_eth_init_thread_entry,NULL,2048,13,100);
	return 0;
}
FINSH_FUNCTION_EXPORT(rt_hw_eth_init,rt_hw_eth_init in sys);


