#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/string.h>
#include <kern/pci.h>
#include <kern/pmap.h>

volatile uint32_t *reg;
struct tx_desc *txdscs;
struct e1000_tdh *tdh;
struct e1000_tdt *tdt;
uintptr_t buf_addrs[NTXDESCS];

#define TX_CMD_EOP 1<<0 // End Of Packet
#define TX_CMD_RS 1<<3 // Report Status
#define TX_CMD_RPS 1<<4 // Report Packet Sent
#define TX_CMD_IDE 1<<7 // Interrupt Delay Enable
#define TX_STATUS_DD 0x01 // Descriptor Done

static void
transmit_init() {
	struct e1000_tctl *tctl;
	struct e1000_tipg *tipg;
	struct e1000_tdl *tdl;
	uint32_t *tdbal;
	uint32_t *tdbah;

	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	txdscs = page2kva(pp);
	tdbal = (uint32_t *)E1000REG(reg, E1000_TDBAL);
	tdbah = (uint32_t *)E1000REG(reg, E1000_TDBAH);
	*tdbal = page2pa(pp);
	*tdbah = 0;
	int i;
	for (i = 0; i < NTXDESCS; i++) {
		struct PageInfo *bufpg = page_alloc(ALLOC_ZERO);
		buf_addrs[i] = page2pa(bufpg);
		txdscs[i].addr = 0;
		txdscs[i].cmd = 0;
	}

	tdl = (struct e1000_tdl *)E1000REG(reg, E1000_TDL);
	// uint16_t tdl_len = sizeof(struct tx_desc) * NTXDESCS;
	// assert(tdl_len % 128 == 0);
	// tdl->len = tdl_len;
	tdl->len = NTXDESCS; // NOT a byte size !!

	tdh = (struct e1000_tdh *)E1000REG(reg, E1000_TDH);
	tdh->tdh = 0;

	tdt = (struct e1000_tdt *)E1000REG(reg, E1000_TDT);
	tdt->tdt = 0;

	tctl = (struct e1000_tctl *)E1000REG(reg, E1000_TCTL);
	tctl->en = 1;
	tctl->psp = 1;
	tctl->ct = 0x10;
	tctl->cold = 0x40;

	// See the table 13-77 of section 13.4.34 on Developer's Manual.
	tipg = (struct e1000_tipg *)E1000REG(reg, E1000_TIPG);
	tipg->ipgt = 10;
	tipg->ipgr1 = 4;
	tipg->ipgr2 = 6;
}


int
e1000_transmit(void *addr, size_t size) {
	static uint32_t current = 0;
	txdscs[current].addr = buf_addrs[current];
	txdscs[current].length = size;
	txdscs[current].cmd = TX_CMD_EOP | TX_CMD_IDE | TX_CMD_RPS;
	memcpy(KADDR(buf_addrs[current]), addr, size);
	uint32_t next = (current + 1) % NTXDESCS;
	tdt->tdt = next; // NOT a byte offset !!
	while(!(txdscs[current].status & 0xf))
		;
	current = next;
	return 0;
}

int
e1000_attachfn(struct pci_func *pcif) {
	pci_func_enable(pcif);
	reg = mmio_map_region((physaddr_t)pcif->reg_base[0],
			pcif->reg_size[0]);

	uint32_t *status = (uint32_t *)E1000REG(reg, E1000_STATUS);
	cprintf("device status: %x\n", *status);

	transmit_init();
	return 0; // What shoud I return?
}

