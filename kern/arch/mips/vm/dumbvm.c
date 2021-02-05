/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "opt-A3.h"
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>


/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3
static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
paddr_t  coremap_start;
paddr_t  coremap_end;
paddr_t * vm_start;
unsigned long coremap_npages;
unsigned long npages_available;
bool coremap_ready = false;
#endif


void
vm_bootstrap(void)
{	
	// get the available physical length after bootstrap
	ram_getsize(&coremap_start, &coremap_end);

	npages_available = (coremap_end - coremap_start) / PAGE_SIZE;

	coremap_npages = ((npages_available * (sizeof(int))) + PAGE_SIZE) / PAGE_SIZE;

	npages_available = npages_available - coremap_npages;

	for (unsigned long i = 0; i < npages_available; i++){

		((int *) PADDR_TO_KVADDR(coremap_start))[i] = 0;

	}

	coremap_ready= true;

	/* Do nothing. */
}

# if OPT_A3

// kalloc still allocate continuous pages of phy mem
// the address struc for every process  will not be continuous based on our method
paddr_t vm_stealmem(unsigned long npages) {

	unsigned long page_start = 0;

	int alloced;

	unsigned long npage_can_allocate = 0;

	//search for if we can find continuous space for the npages;
	for (unsigned long cur_page = 0; cur_page < npages_available; cur_page ++) {


		alloced = ((int *) PADDR_TO_KVADDR(coremap_start))[cur_page];

		//alloced = * (int *)(coremap_start + (cur_page * sizeof(int)));

		if (alloced == 0){

			if (npage_can_allocate == 0){

				page_start = cur_page + 1;
				npage_can_allocate ++;

			} else {
				npage_can_allocate ++;
			}
			

		} else {

			page_start = 0;
			npage_can_allocate = 0;

		}

		if (npage_can_allocate == npages){
			break;
		}

	}

	if (npage_can_allocate == 0){
		return 0;
	} else {

		for (unsigned long cur_page = 0; cur_page < npages_available; cur_page ++){

			if ((cur_page + 1) == page_start){

				for (unsigned long npages_to_allocate = 0; npages_to_allocate < npage_can_allocate; npages_to_allocate++){

					((int *) PADDR_TO_KVADDR(coremap_start))[cur_page + npages_to_allocate] = npages_to_allocate + 1;
					//*(int *)(coremap_start + (npages_to_allocate * sizeof(int))) = npages_to_allocate + 1;

				}

			}
			break;


		}

		return (paddr_t)(coremap_start + (coremap_npages * PAGE_SIZE) + ((page_start - 1) * PAGE_SIZE));


	}

}

#endif

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	#if OPT_A3

	if (!coremap_ready){

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);
		
		spinlock_release(&stealmem_lock);

	} else {

		spinlock_acquire(&coremap_lock);

		addr = vm_stealmem(npages);
		
		spinlock_release(&coremap_lock);

	}


	#else

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);
	
	spinlock_release(&stealmem_lock);

	#endif
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */

	#if OPT_A3

	paddr_t phaddr = addr - MIPS_KSEG0;

	unsigned long page_belong = ((phaddr - (coremap_start + (coremap_npages * PAGE_SIZE)))/PAGE_SIZE);
	unsigned long cur_page = page_belong;
	unsigned long alloced = ((int *) PADDR_TO_KVADDR(coremap_start))[cur_page];

	while (alloced == (cur_page + 1)){

		((int *) PADDR_TO_KVADDR(coremap_start))[cur_page] = 0;
		//* (int *)(coremap_start + cur_page * sizeof(int)) = 0;

		cur_page ++;
		alloced = ((int *) PADDR_TO_KVADDR(coremap_start))[cur_page];
		//alloced = * (int *)(coremap_start + cur_page * sizeof(int));

	}


	#else 

	(void)addr;

	#endif

	
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
	#if OPT_A3
	    return EFAULT;

	#else
	    /* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");

	#endif


	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */

	#if OPT_A3
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != NULL);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != NULL);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != NULL);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	//KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	//KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	//KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	#else
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
	#endif

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;


	#if OPT_A3

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = * (paddr_t *)as->as_pbase1[(faultaddress - vbase1) / PAGE_SIZE];
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = * (paddr_t *)as->as_pbase2[(faultaddress - vbase2) / PAGE_SIZE];
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = * (paddr_t *)as->as_stackpbase [ (faultaddress - stackbase) / PAGE_SIZE];
	}
	else {
		return EFAULT;
	}

	#else

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	#endif
	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}

	#if OPT_A3

		if (faultaddress >= vbase1 && faultaddress < vtop1 && as -> in_load_elf == false){


			ehi = faultaddress;
			elo = paddr | TLBLO_VALID;
			DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
			tlb_write(ehi, elo, i);
			splx(spl);
			return 0;

		}

		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;

	#else
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;

	#endif

	}
#if OPT_A3

	if (faultaddress >= vbase1 && faultaddress < vtop1 && as -> in_load_elf == false){

		ehi = faultaddress;
		elo = paddr | TLBLO_VALID;
		//DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_random(ehi, elo);
		splx(spl);
		return 0;

	}
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	//DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	tlb_random(ehi, elo);
	splx(spl);
	return 0;

#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
#endif
}

// struct addrspace *
// as_create(void)
// {
// 	struct addrspace *as = kmalloc(sizeof(struct addrspace));
// 	if (as==NULL) {
// 		return NULL;
// 	}

// 	#if OPT_A3

// 	as->as_vbase1 = 0;
// 	as->as_pbase1 = NULL;
// 	as->as_npages1 = 0;
// 	as->as_vbase2 = 0;
// 	as->as_pbase2 = NULL;
// 	as->as_npages2 = 0;
// 	as->as_stackpbase = NULL;

// 	#else

// 	as->as_vbase1 = 0;
// 	as->as_pbase1 = 0;
// 	as->as_npages1 = 0;
// 	as->as_vbase2 = 0;
// 	as->as_pbase2 = 0;
// 	as->as_npages2 = 0;
// 	as->as_stackpbase = 0;

// 	#endif

// 	return as;
// }

// void
// as_destroy(struct addrspace *as)
// {
// 	#if OPT_A3
// 	for (size_t i = 0; i < as -> as_npages1; i++){

// 		free_kpages(as -> as_pbase1[i]); 

// 	}

// 	for (size_t i = 0; i < as -> as_npages2; i++){

// 		free_kpages(as -> as_pbase2[i]); 

// 	}

// 	for (size_t i = 0; i < DUMBVM_STACKPAGES; i++){

// 		free_kpages(as -> as_stackpbase[i]); 

// 	}

// 	kfree(as);
// 	#else
// 	kfree(as);
// 	#endif
// }

// void
// as_activate(void)
// {
// 	int i, spl;
// 	struct addrspace *as;

// 	as = curproc_getas();
// #ifdef UW
//         /* Kernel threads don't have an address spaces to activate */
// #endif
// 	if (as == NULL) {
// 		return;
// 	}

// 	/* Disable interrupts on this CPU while frobbing the TLB. */
// 	spl = splhigh();

// 	for (i=0; i<NUM_TLB; i++) {
// 		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
// 	}

// 	splx(spl);
// }

// void
// as_deactivate(void)
// {
// 	/* nothing */
// }

// int
// as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
// 		 int readable, int writeable, int executable)
// {
// 	size_t npages; 

// 	/* Align the region. First, the base... */
// 	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
// 	vaddr &= PAGE_FRAME;

// 	/* ...and now the length. */
// 	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

// 	npages = sz / PAGE_SIZE;

// 	/* We don't use these - all pages are read-write */
// 	(void)readable;
// 	(void)writeable;
// 	(void)executable;

// 	if (as->as_vbase1 == 0) {
// 		as->as_vbase1 = vaddr;
// 		as->as_npages1 = npages;

// 		#if OPT_A3
// 		as->as_pbase1 = kmalloc(sizeof(paddr_t) * npages);
// 		#endif

// 		return 0;
// 	}

// 	if (as->as_vbase2 == 0) {
// 		as->as_vbase2 = vaddr;
// 		as->as_npages2 = npages;

// 		#if OPT_A3
// 		as->as_pbase2 = kmalloc(sizeof(paddr_t) * npages);
// 		#endif

// 		return 0;
// 	}

// 	/*
// 	 * Support for more than two regions is not available.
// 	 */
// 	kprintf("dumbvm: Warning: too many regions\n");
// 	return EUNIMP;
// }

// static
// void
// as_zero_region(paddr_t paddr, unsigned npages)
// {
// 	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
// }

// int
// as_prepare_load(struct addrspace *as)
// {

// 	#if OPT_A3

// 	// KASSERT(as->as_pbase1 == NULL);
// 	// KASSERT(as->as_pbase2 == NULL);
// 	// KASSERT(as->as_stackppbase == NULL);

// 	#else

// 	KASSERT(as->as_pbase1 == 0);
// 	KASSERT(as->as_pbase2 == 0);
// 	KASSERT(as->as_stackpbase == 0);
// 	#endif


// 	#if OPT_A3

// 	for (size_t i = 0; i < as -> as_npages1; i ++){

// 		as->as_pbase1[i] = getppages(1);
// 		if (as->as_pbase1[i] == 0) {
// 			return ENOMEM;
// 		}
// 		as_zero_region(as->as_pbase1[i], 1);

// 	}


// 	for (size_t i = 0; i < as -> as_npages2; i ++){

// 		as->as_pbase2[i] = getppages(1);
// 		if (as->as_pbase2[i] == 0) {
// 			return ENOMEM;
// 		}
// 		as_zero_region(as->as_pbase2[i], 1);

// 	}
		
// 	as->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);
		
// 	for (size_t i = 0; i < DUMBVM_STACKPAGES; i ++){

// 		as->as_stackpbase[i] = getppages(1);
// 		if (as->as_stackpbase[i] == 0) {
// 			return ENOMEM;
// 		}
// 		as_zero_region(as->as_stackpbase[i], 1);

// 	}


// 	#else

// 	as->as_pbase1 = getppages(as->as_npages1);
// 	if (as->as_pbase1 == 0) {
// 		return ENOMEM;
// 	}

// 	as->as_pbase2 = getppages(as->as_npages2);
// 	if (as->as_pbase2 == 0) {
// 		return ENOMEM;
// 	}

// 	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
// 	if (as->as_stackpbase == 0) {
// 		return ENOMEM;
// 	}
	
// 	as_zero_region(as->as_pbase1, as->as_npages1);
// 	as_zero_region(as->as_pbase2, as->as_npages2);
// 	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

// 	#endif

// 	#if OPT_A3
// 	as -> in_load_elf = true;
// 	#endif

// 	return 0;
// }

// int
// as_complete_load(struct addrspace *as)
// {
// 	(void)as;
// 	#if OPT_A3
// 	as -> in_load_elf = false;
// 	#endif
// 	return 0;
// }

// int
// as_define_stack(struct addrspace *as, vaddr_t *stackptr)
// {
// 	#if OPT_A3
// 	KASSERT(as->as_stackpbase != NULL);
// 	#else
// 	KASSERT(as->as_stackpbase != 0);
// 	#endif

// 	*stackptr = USERSTACK;
// 	return 0;
// }

// int
// as_copy(struct addrspace *old, struct addrspace **ret)
// {
// 	struct addrspace *new;

// 	new = as_create();
// 	if (new==NULL) {
// 		return ENOMEM;
// 	}

// 	new->as_vbase1 = old->as_vbase1;
// 	new->as_npages1 = old->as_npages1;
// 	new->as_vbase2 = old->as_vbase2;
// 	new->as_npages2 = old->as_npages2;
// 	#if OPT_A3
// 	new->as_pbase1 = kmalloc(sizeof(paddr_t) * old->as_npages1);
// 	new->as_pbase2 = kmalloc(sizeof(paddr_t) * old->as_npages2);
// 	new->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);

// 	#endif

// 	/* (Mis)use as_prepare_load to allocate some physical memory. */
// 	if (as_prepare_load(new)) {
// 		as_destroy(new);
// 		return ENOMEM;
// 	}

// 	#if OPT_A3

// 	KASSERT(new->as_pbase1 != NULL);
// 	KASSERT(new->as_pbase2 != NULL);
// 	KASSERT(new->as_stackpbase != NULL);

// 	for (unsigned int i = 0; i < old->as_npages1; i++) {
// 	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1[i]),
// 	  (const void *)PADDR_TO_KVADDR(old->as_pbase1[i]),PAGE_SIZE);
// 	}
// 	for (unsigned int i = 0; i < old->as_npages2; i++) {
// 	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2[i]),
// 	  (const void *)PADDR_TO_KVADDR(old->as_pbase2[i]),PAGE_SIZE);
// 	}
// 	for (unsigned int i = 0; i < DUMBVM_STACKPAGES; i++) {
// 	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase[i]),
// 	  (const void *)PADDR_TO_KVADDR(old->as_stackpbase[i]),PAGE_SIZE);
// 	}
	
// 	*ret = new;
// 	return 0;


// 	#else

// 	KASSERT(new->as_pbase1 != 0);
// 	KASSERT(new->as_pbase2 != 0);
// 	KASSERT(new->as_stackpbase != 0);

// 	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
// 		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
// 		old->as_npages1*PAGE_SIZE);

// 	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
// 		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
// 		old->as_npages2*PAGE_SIZE);

// 	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
// 		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
// 		DUMBVM_STACKPAGES*PAGE_SIZE);
	
// 	*ret = new;
// 	return 0;
// 	#endif
// }

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

#if OPT_A3
	as->as_vbase1 = 0;
	as->as_pbase1 = NULL;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = NULL;
	as->as_npages2 = 0;
	as->as_stackpbase = NULL;
  as->loadelf_complete = false;
#else
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
#endif

	return as;
}

void
as_destroy(struct addrspace *as)
{
#if OPT_A3
  for (unsigned int i = 0; i < as->as_npages1; i++) {
    free_kpages(PADDR_TO_KVADDR(as->as_pbase1[i]));
  }
  for (unsigned int i = 0; i < as->as_npages2; i++) {
    free_kpages(PADDR_TO_KVADDR(as->as_pbase2[i]));
  }
  for (unsigned int i = 0; i < DUMBVM_STACKPAGES; i++) {
    free_kpages(PADDR_TO_KVADDR(as->as_stackpbase[i]));
  }
  /* free_kpages(PADDR_TO_KVADDR(as->as_pbase1)); */
  /* free_kpages(PADDR_TO_KVADDR(as->as_pbase2)); */
  /* free_kpages(PADDR_TO_KVADDR(as->as_stackpbase)); */
#endif
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
#if OPT_A3
    as->as_pbase1 = kmalloc(sizeof(paddr_t) * npages);
#endif
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
#if OPT_A3
    as->as_pbase2 = kmalloc(sizeof(paddr_t) * npages);
#endif
		as->as_npages2 = npages;
		return 0;
	}
	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	/* KASSERT(as->as_pbase1 == NULL); */
	/* KASSERT(as->as_pbase2 == NULL); */
	/* KASSERT(as->as_stackpbase == NULL); */

#if OPT_A3
  for (unsigned int i = 0; i < as->as_npages1; i++) {
    paddr_t adr = getppages(1);
    if (adr == 0) return ENOMEM;
    as->as_pbase1[i] = adr;
    as_zero_region(as->as_pbase1[i], 1);
  }
  for (unsigned int i = 0; i < as->as_npages2; i++) {
    paddr_t adr = getppages(1);
    if (adr == 0) return ENOMEM;
    as->as_pbase2[i] = adr;
    as_zero_region(as->as_pbase2[i], 1);
  }
	as->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);
	if (as->as_stackpbase == NULL) {
		return ENOMEM;
	}
  for (unsigned int i = 0; i < DUMBVM_STACKPAGES; i++) {
    paddr_t adr = getppages(1);
    if (adr == 0) return ENOMEM;
    as->as_stackpbase[i] = adr;
    as_zero_region(as->as_stackpbase[i], 1);
  }
#else
	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

#endif


	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

#if OPT_A3
  new->as_pbase1 = kmalloc(sizeof(paddr_t) * old->as_npages1);
  new->as_pbase2 = kmalloc(sizeof(paddr_t) * old->as_npages2);
  new->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);

#endif

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != NULL);
	KASSERT(new->as_pbase2 != NULL);
	KASSERT(new->as_stackpbase != NULL);

#if OPT_A3
  for (unsigned int i = 0; i < old->as_npages1; i++) {
    memmove((void *)PADDR_TO_KVADDR(new->as_pbase1[i]),
      (const void *)PADDR_TO_KVADDR(old->as_pbase1[i]),PAGE_SIZE);
  }
  for (unsigned int i = 0; i < old->as_npages2; i++) {
    memmove((void *)PADDR_TO_KVADDR(new->as_pbase2[i]),
      (const void *)PADDR_TO_KVADDR(old->as_pbase2[i]),PAGE_SIZE);
  }
  for (unsigned int i = 0; i < DUMBVM_STACKPAGES; i++) {
    memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase[i]),
      (const void *)PADDR_TO_KVADDR(old->as_stackpbase[i]),PAGE_SIZE);
  }
#else
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
#endif
	*ret = new;
	return 0;
}

