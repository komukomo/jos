#include <kern/e1000.h>

// LAB 6: Your driver code here
#include <kern/pci.h>

int
e1000_attachfn(struct pci_func *pcif) {
	pci_func_enable(pcif);
	return 0; // What shoud I return?
}
