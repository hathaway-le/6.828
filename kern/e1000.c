#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here

//82540EM-A

static volatile uint32_t *e1000;//防止编译器优化，扰乱顺序单线程内顺序，不用寄存器（eax这些）缓存（lock指令）

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
} __attribute__((packed));//length在小地址处，小端

struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t checksum;
	uint8_t status;
	uint8_t error;
	uint16_t special;
}__attribute__((packed));

struct tx_desc tx_d[TX_RING_LEN] __attribute__((aligned(16))) = {{0}};
//This 64-bit address is aligned on a 16-byte boundary 
//Length 128 byte aligned. 8*16B，至少8项，8的倍数，自定义64用于测试溢出

uint8_t tx_buf[TX_RING_LEN][ETH_MAX_LEN] = {{0}};
//The maximum size of an Ethernet packet is 1518 bytes

struct rx_desc rx_d[RX_RING_LEN] __attribute__((aligned(16))) = {{0}};

uint8_t rx_buf[RX_RING_LEN][RX_BUF_LEN] = {{0}};//网卡对与接受buffer大小提供了几个档位

void e1000_tx_init()
{
	size_t ii;
	for(ii = 0;ii < sizeof(tx_d) / sizeof(struct tx_desc);ii++)
	{
		tx_d[ii].addr = PADDR(tx_buf[ii]);
		tx_d[ii].status = E1000_TXD_STAT_DD;
        tx_d[ii].cmd = ((E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP) >> 24) & 0xff;
		//Hardware only sets the DD bit for descriptors with RS set.
		//E1000_TXD_CMD_EOP 每个队列项保存一个包，若多个队列项保存一个包，最后一个项置此位
	}

	*(uint32_t *)((uint8_t *)e1000 + E1000_TDBAL) = PADDR(tx_d);
	*(uint32_t *)((uint8_t *)e1000 + E1000_TDBAH) = 0;
	*(uint32_t *)((uint8_t *)e1000 + E1000_TDLEN) = sizeof(tx_d);		
	*(uint32_t *)((uint8_t *)e1000 + E1000_TDH) = 0;	
	*(uint32_t *)((uint8_t *)e1000 + E1000_TDT) = 0;
	//you should see an "e1000: tx disabled" message when you set the TDT register (since this happens before you set TCTL.EN)

	*(uint32_t *)((uint8_t *)e1000 + E1000_TCTL) = E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_CT & (0x10 << 4)) | (E1000_TCTL_COLD & (0x40 << 12));	
	*(uint32_t *)((uint8_t *)e1000 + E1000_TIPG) = 10 | (8 << 10) | (12 << 20);
}

void e1000_rx_init()
{
	uint32_t mac_low,mac_high;
	size_t ii;
 
	mac_low = 0x12005452;
	mac_high = 0x00005634;

	*(uint32_t *)((uint8_t *)e1000 + E1000_RAL) = mac_low;
	*(uint32_t *)((uint8_t *)e1000 + E1000_RAH) = mac_high | E1000_RAH_AV;//不赋值也能过

	for(ii = 0;ii < sizeof(rx_d) / sizeof(struct rx_desc);ii++)
	{
		rx_d[ii].addr = PADDR(rx_buf[ii]);//DD与tx相反
	}
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDBAL) = PADDR(rx_d);
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDBAH) = 0;
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDLEN) = sizeof(rx_d);	
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDH) = 0;//E1000_RDH硬件自增，E1000_RDH和E1000_RDT之间的非DD为硬件可写，软件接受一次，RDT+1，因为是循环队列，相当于可写区域扩大
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDT) = RX_RING_LEN -1;//队列全满

	*(uint32_t *)((uint8_t *)e1000 + E1000_RCTL) = E1000_RCTL_EN | E1000_RCTL_SECRC | E1000_RCTL_BAM | E1000_RCTL_SZ_2048;

}

int pci_e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	
	e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	cprintf("PCI function %02x:%02x.%d (%04x:%04x) enabled\n",
		pcif->bus->busno, pcif->dev, pcif->func,
		PCI_VENDOR(pcif->dev_id), PCI_PRODUCT(pcif->dev_id));

	e1000_tx_init();
	e1000_rx_init();
	cprintf("device status:[%08x]\n", *(uint32_t *)((uint8_t *)e1000 + E1000_STATUS));
	return 0;
}
 
int e1000_tx(uint8_t *data, uint32_t len)//raw data
{
	if (len > ETH_MAX_LEN)
	{
		cprintf("e1000 tx candidate is too long\n");
		return -1;
	}

	uint32_t ii = *(uint32_t *)((uint8_t *)e1000 + E1000_TDT);
	if ((tx_d[ii].status & E1000_TXD_STAT_DD) == 0)
	{
		cprintf("e1000 tx ring is full\n");
		return -1;
	}

	memmove(tx_buf[ii], data, len);
	tx_d[ii].length = len;
	tx_d[ii].status &= ~E1000_TXD_STAT_DD;

	*(uint32_t *)((uint8_t *)e1000 + E1000_TDT) = (ii+1) % TX_RING_LEN;
	//Hardware attempts to transmit all packets referenced by descriptors between head and tail
	//E1000_TDH由硬件自增，直到等于E1000_TDT，队列为空
	return 0;
}

int e1000_rx(uint8_t *data, uint32_t *len)//raw data
{
	uint32_t ii = ((*(uint32_t *)((uint8_t *)e1000 + E1000_RDT)) + 1) % RX_RING_LEN;//初始化rail在head后面（环形，-1）
	if (!(rx_d[ii].status & E1000_RXD_STAT_DD))
	{
		//cprintf("e1000 rx ring is empty\n");
		return -1;
	}
	*len = rx_d[ii].length;
	memmove(data, rx_buf[ii], *len);
	rx_d[ii].status &= ~E1000_RXD_STAT_DD;
	*(uint32_t *)((uint8_t *)e1000 + E1000_RDT) = ii;
	return 0;
}
