#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <inc/string.h>
#include <kern/pci.h>
#include <kern/pmap.h>

volatile uint32_t *reg;
struct tx_desc *txdscs;
struct rx_desc *rxdscs;
struct e1000_tdh *tdh;
struct e1000_tdt *tdt;
struct e1000_rdt *rdt;
uintptr_t buf_addrs[NTXDESCS];
void *recv_buf_addrs[NRXDESCS];

#define TX_CMD_EOP 1<<0 // End Of Packet
#define TX_CMD_RS 1<<3 // Report Status
#define TX_CMD_RPS 1<<4 // Report Packet Sent
#define TX_CMD_IDE 1<<7 // Interrupt Delay Enable
#define TX_STATUS_DD 0x01 // Descriptor Done
#define RX_STATUS_DD 0x01 // Descriptor Done
#define RX_STATUS_EOP 1<<1 // End of Packet

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

static void
set_receive_address(uint32_t *ra, char *hdaddr, uint32_t rah_flag) {
	/*
	 * If hdaddr is AA:BB:CC:DD:EE:FF, each register will
	 * be set as follows.
	 *      31           0
	 *  RAL: |DD|CC|BB|AA|
	 *  RAH: |flags|FF|EE|
	 */
	uint32_t low = 0, high = 0;
	int i;
	for (i = 0; i < 4; i++) {
		low |= hdaddr[i] << (8 * i);
	}
	for (i = 4; i < 6; i++) {
		high |= hdaddr[i] << (8 * i);
	}
	ra[0] = low;
	ra[1] = high | rah_flag;
}

static void
receive_init() {
	uint32_t *rdbal;
	uint32_t *rdbah;
	struct e1000_rdlen *rdlen;
	struct e1000_rdh *rdh;
	e1000_rctl *rctl;

	uint32_t *ra0 = (uint32_t *)E1000REG(reg, E1000_RA0);
	char hdaddr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
	set_receive_address(ra0, hdaddr, E1000_RAH_AV);

	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	assert(sizeof(struct rx_desc) * NRXDESCS < PGSIZE);
	rxdscs = page2kva(pp);
	rdbal = (uint32_t *)E1000REG(reg, E1000_RDBAL);
	rdbah = (uint32_t *)E1000REG(reg, E1000_RDBAH);
	*rdbal = page2pa(pp);
	*rdbah = 0;

	int i;
	for(i = 0; i < NRXDESCS; i++) {
		struct PageInfo *tmppg = page_alloc(ALLOC_ZERO);
		recv_buf_addrs[i] = page2kva(tmppg);
		rxdscs[i].addr = page2pa(tmppg);
	}

	rdlen = (struct e1000_rdlen *)E1000REG(reg, E1000_RDLEN);
	rdlen->len = NRXDESCS;
	rdh = (struct e1000_rdh *)E1000REG(reg, E1000_RDH);
	rdh->rdh = 0;
	rdt = (struct e1000_rdt *)E1000REG(reg, E1000_RDT);
	rdt->rdt = NRXDESCS - 1;

	rctl = (e1000_rctl *)E1000REG(reg, E1000_RCTL);
	*rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_RECRC;
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

enum {
	E_RECEIVE_RETRY = 1,
};


int
e1000_receive(void *addr, size_t *size) {
	static uint32_t next = 0;
	uint8_t errors;
	if (!(rxdscs[next].status & RX_STATUS_DD)) {
		return -E_RECEIVE_RETRY;
	}
	if (!(rxdscs[next].status & RX_STATUS_EOP)){
		return -E_RECEIVE_RETRY;
	}
	if ((errors = rxdscs[next].errors) != 0){
		cprintf("receive error: %x\n", errors);
		return -E_RECEIVE_RETRY;
	}
	*size = rxdscs[next].length;
	memcpy(addr, recv_buf_addrs[next], *size);
	rdt->rdt = next;
	next = (next + 1) % NRXDESCS;
	return 0;
}


int
e1000_attachfn(struct pci_func *pcif) {
	pci_func_enable(pcif);
	reg = mmio_map_region((physaddr_t)pcif->reg_base[0],
			pcif->reg_size[0]);

	uint32_t *status = (uint32_t *)E1000REG(reg, E1000_STATUS);
	cprintf("device status: %x\n", *status);

	receive_init();
	transmit_init();
	return 0; // What shoud I return?
}

