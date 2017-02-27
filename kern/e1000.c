#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <kern/pci.h>
#include <kern/pmap.h>

volatile uint32_t *reg;

int
e1000_attachfn(struct pci_func *pcif) {
	pci_func_enable(pcif);
	reg = mmio_map_region((physaddr_t)pcif->reg_base[0],
			pcif->reg_size[0]);

	cprintf("device status: %x\n", reg[2]);

	return 0; // What shoud I return?
}
