#include <kern/e1000.h>
#include <kern/pmap.h>

// LAB 6: Your driver code here
#define TX_RING_LEN 64
#define ETH_MAX_LEN 1518

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
} __attribute__((packed));

struct tx_desc tx_d[TX_RING_LEN] __attribute__((aligned(16))) = {{0,0,0,0,0,0}};
//This 64-bit address is aligned on a 16-byte boundary 
//Length 128 byte aligned. 8*16B，至少8项，8的倍数，自定义64用于测试溢出

uint8_t tx_buf[TX_RING_LEN][ETH_MAX_LEN] = {{0}};
//The maximum size of an Ethernet packet is 1518 bytes

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

int pci_e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	
	e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

	cprintf("PCI function %02x:%02x.%d (%04x:%04x) enabled\n",
		pcif->bus->busno, pcif->dev, pcif->func,
		PCI_VENDOR(pcif->dev_id), PCI_PRODUCT(pcif->dev_id));

	e1000_tx_init();
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
	return 0;
}

