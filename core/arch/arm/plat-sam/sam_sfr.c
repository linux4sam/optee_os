// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 Timesys Corporation.
 * Copyright (C) 2021 Bootlin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <io.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <kernel/tz_ssvce_pl310.h>
#include <libfdt.h>
#include <matrix.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <sm/sm.h>
#include <smc_ids.h>
#include <types_ext.h>
#ifdef OPTEE_SAMA7G5
#include <sama7g5.h>
#else
#include <sama5d2.h>
#include <sam_pl310.h>
#endif

#include <sam_sfr.h>

#define REGMAP_SMC_READ		0
#define REGMAP_SMC_WRITE	1

#define REG_ACCESS_FLAG_READ	BIT32(0)
#define REG_ACCESS_FLAG_WRITE	BIT32(1)
#define REG_ACCESS_FLAG_RW	(REG_ACCESS_FLAG_READ | REG_ACCESS_FLAG_WRITE)

struct register_access {
	uint32_t offset;
	uint8_t flags;
};

register_phys_mem_pgdir(MEM_AREA_IO_SEC, SFR_BASE, CORE_MMU_PGDIR_SIZE);

vaddr_t sam_sfr_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(SFR_BASE, MEM_AREA_IO_SEC, 1);
		return (vaddr_t)va;
	}
	return SFR_BASE;
}

static const struct register_access sfr_regs[] = {
	{ .offset = AT91_SFR_OHCIICR,	.flags = REG_ACCESS_FLAG_RW},
	{ .offset = AT91_SFR_SN0, 	.flags = REG_ACCESS_FLAG_READ},
	{ .offset = AT91_SFR_SN1, 	.flags = REG_ACCESS_FLAG_READ},
};

static const struct register_access *get_sfr_reg_access(uint32_t offset)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sfr_regs); i++) {
		if (sfr_regs[i].offset == offset)
			return &sfr_regs[i];
	}

	return NULL;
}

enum sm_handler_ret sam_sfr_access_reg(struct thread_smc_args *args)
{
	vaddr_t sfr = sam_sfr_base();
	const struct register_access *reg;

	reg = get_sfr_reg_access(args->a2);
	if (!reg)
		goto err_perm;

	switch (args->a1) {
	case REGMAP_SMC_READ:
		if (!(reg->flags & REG_ACCESS_FLAG_READ))
			goto err_perm;

		args->a1 = io_read32(sfr + reg->offset);
		break;
	case REGMAP_SMC_WRITE:
		if (!(reg->flags & REG_ACCESS_FLAG_WRITE))
			goto err_perm;

		io_write32(sfr + reg->offset, args->a3);
		break;
	default:
		goto err_perm;
	}

	args->a0 = SAMA5_SMC_SIP_RETURN_SUCCESS;

	return SM_HANDLER_SMC_HANDLED;

err_perm:
	args->a0 = SAMA5_SMC_SIP_RETURN_EPERM;

	return SM_HANDLER_SMC_HANDLED;
}

static TEE_Result sfr_set_secure(void)
{
	int node;
	void *fdt = get_embedded_dt();

	node = fdt_node_offset_by_compatible(fdt, 0, "atmel,sama5d2-sfr");
	if (node < 0)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (_fdt_get_status(fdt, node) == DT_STATUS_OK_SEC)
		matrix_configure_periph_secure(AT91C_ID_SFR);

	return TEE_SUCCESS;
}
driver_init(sfr_set_secure);

