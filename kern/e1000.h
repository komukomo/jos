#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

#define VENDOR_ID_82540EM 0x8086
#define DEVICE_ID_82540EM 0x100e

#define E1000REG(reg, offset) (void *)((unsigned)reg + offset);

#define NTXDESCS 32 // the number of Transmit Descirptor

#define E1000_STATUS 0x00008

// Transmit Descriptor
struct tx_desc {
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

// Transmit Control Register
#define E1000_TCTL 0x00400
struct e1000_tctl {
	unsigned rsv1   : 1;
	unsigned en     : 1;
	unsigned rsv2   : 1;
	unsigned psp    : 1;
	unsigned ct     : 8;
	unsigned cold   : 10;
	unsigned swxoff : 1;
	unsigned rsv3   : 1;
	unsigned rtlc   : 1;
	unsigned nrtu   : 1;
	unsigned rsv4   : 6;
};

// Transmit IPG Register
#define E1000_TIPG 0x00410
struct e1000_tipg {
	unsigned ipgt  : 10;
	unsigned ipgr1 : 10;
	unsigned ipgr2 : 10;
	unsigned rsv   : 2;
};

// Transmit Descriptor Length
#define E1000_TDL 0x3808
struct e1000_tdl {
	unsigned zero : 7;
	unsigned len  : 13;
	unsigned rsv  : 12;
};

#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804

// Transmit Descriptor Head
#define E1000_TDH 0x03810
struct e1000_tdh {
	uint16_t tdh;
	uint16_t rsv;
};

// Transmit Descriptor Tail
#define E1000_TDT 0x03818
struct e1000_tdt {
	uint16_t tdt;
	uint16_t rsv;
};

int e1000_attachfn(struct pci_func *pcif);
int e1000_transmit(void *addr, size_t size);

#endif	// JOS_KERN_E1000_H
