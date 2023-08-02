// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Microchip
 */

#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <io.h>
#include <libfdt.h>
#include <kernel/boot.h>
#include <kernel/time_source.h>
#include <matrix.h>
#ifdef OPTEE_SAMA7G5
#include <sama7g5.h>
#define AT91C_BASE_TC0 TC0_BASE_ADDRESS
#define AT91C_ID_TC0     ID_TC0_CHANNEL0
#define AT91C_ID_TC1     ID_TC1_CHANNEL0
#else
#include <sama5d2.h>
#endif
#include <tee_api_defines.h>

#define TCB_CHAN(chan)	(chan * 0x40)

#define TCB_CCR(chan)		(0x0 + TCB_CHAN(chan))
#define  TCB_CCR_SWTRG		0x4
#define  TCB_CCR_CLKEN		0x1

#define TCB_CMR(chan)		(0x4 + TCB_CHAN(chan))
#define  TCB_CMR_WAVE		BIT32(15)
#define  TCB_CMR_TIMER_CLOCK5	4
#define  TCB_CMR_XC1		6
#define  TCB_CMR_ACPA_SET	(1 << 16)
#define  TCB_CMR_ACPC_CLEAR	(2 << 18)

#define TCB_CV(chan)		(0x10 + TCB_CHAN(chan))

#define TCB_RA(chan)		(0x14 + TCB_CHAN(chan))
#define TCB_RB(chan)		(0x18 + TCB_CHAN(chan))
#define TCB_RC(chan)		(0x1c + TCB_CHAN(chan))

#define TCB_IER(chan)		(0x24 + TCB_CHAN(chan))
#define  TCB_IER_COVFS		0x1

#define TCB_SR(chan)		(0x20 + TCB_CHAN(chan))
#define  TCB_SR_COVFS		0x1

#define TCB_IDR(chan)		(0x28 + TCB_CHAN(chan))

#define TCB_BCR			0xc0
#define  TCB_BCR_SYNC		0x1

#define TCB_BMR			0xc4
#define  TCB_BMR_TC1XC1S_TIOA0	(2 << 2)

#define TCB_WPMR		0xe4
#define  TCB_WPMR_WAKEY		0x54494d

#ifdef OPTEE_SAMA7G5
static const char *tcb_clocks[] = {"t0_clk", "t1_clk", "t2_clk", "md_slck"};
#else
static const char *tcb_clocks[] = {"t0_clk", "gclk", "slow_clk"};
#endif
static vaddr_t tcb_base;
static uint32_t tcb_rate;

static TEE_Result atmel_tcb_enable_clocks(const void *fdt, int node)
{
	unsigned int i = 0;
	struct clk *clk = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;

	for (i = 0; i < ARRAY_SIZE(tcb_clocks); i++) {
		res = clk_dt_get_by_name(fdt, node, tcb_clocks[i], &clk);
		if (res)
			return res;

		clk_enable(clk);
	}

	return TEE_SUCCESS;
}

static TEE_Result atmel_tcb_get_sys_time(TEE_Time *time)
{
	uint64_t cv0 = 0;
	uint64_t cv1 = 0;

	if (!tcb_base)
		return TEE_ERROR_BAD_STATE;

	do {
		cv1 = io_read32(tcb_base + TCB_CV(1));
		cv0 = io_read32(tcb_base + TCB_CV(0));
	} while (io_read32(tcb_base + TCB_CV(1)) != cv1);

	cv0 |= cv1 << 32;

	time->seconds = cv0 / tcb_rate;
	time->millis = (cv0 % tcb_rate) / (tcb_rate / TEE_TIME_MILLIS_BASE);

	return TEE_SUCCESS;
}

static const struct time_source atmel_tcb_time_source = {
	.name = "atmel_tcb",
	.protection_level = 1000,
	.get_sys_time = atmel_tcb_get_sys_time,
};

REGISTER_TIME_SOURCE(atmel_tcb_time_source)

static void atmel_tcb_configure(void)
{
	/* Disable write protection */
	io_write32(tcb_base + TCB_WPMR, TCB_WPMR_WAKEY);

	/* Disable all irqs for both channel 0 & 1 */
	io_write32(tcb_base + TCB_IDR(0), 0xff);
	io_write32(tcb_base + TCB_IDR(1), 0xff);

	/*
	 * In order to avoid wrapping, use a 64 bit counter by chaining
	 * two channels. We use the slow_clk which runs at 32K an is sufficient
	 * for the millisecond precision, this will wrap in approximately
	 * 17851025 years so no worries here.
	 *
	 * Channel 0 is configured to generate a clock on TIOA0 which is cleared
	 * when reaching 0x80000000 and set when reaching 0. 
	 */
	io_write32(tcb_base + TCB_CMR(0),
		   TCB_CMR_TIMER_CLOCK5 | TCB_CMR_WAVE | TCB_CMR_ACPA_SET
		   | TCB_CMR_ACPC_CLEAR);
	io_write32(tcb_base + TCB_RC(0), 0x80000000);
	io_write32(tcb_base + TCB_RA(0), 0x1);
	io_write32(tcb_base + TCB_CCR(0), TCB_CCR_CLKEN);

	/* Channel 1 is configured to use TIOA0 as input */
	io_write32(tcb_base + TCB_CMR(1), TCB_CMR_XC1 | TCB_CMR_WAVE);
	io_write32(tcb_base + TCB_CCR(1), TCB_CCR_CLKEN);

	/* Set XC1 input to be TIOA0 (ie output of Channel 0) */
	io_write32(tcb_base + TCB_BMR, TCB_BMR_TC1XC1S_TIOA0);

	/* Sync & start all timers */
	io_write32(tcb_base + TCB_BCR, TCB_BCR_SYNC);

	/* Enable write protection */
	io_write32(tcb_base + TCB_WPMR, TCB_WPMR_WAKEY | 1);
}

static TEE_Result atmel_tcb_setup(const void *fdt, int nodeoffset, int status)
{
	size_t size = 0;
	TEE_Result res = TEE_ERROR_GENERIC;
	unsigned int peri_id = AT91C_ID_TC0;
	struct clk *clk = NULL;

	res = atmel_tcb_enable_clocks(fdt, nodeoffset);
	if (res)
		return res;

	if (tcb_base)
		return TEE_SUCCESS;

	if (status != DT_STATUS_OK_SEC)
		return TEE_SUCCESS;

#ifdef OPTEE_SAMA7G5
	res = clk_dt_get_by_name(fdt, nodeoffset, "md_slck", &clk);
#else
	res = clk_dt_get_by_name(fdt, nodeoffset, "slow_clk", &clk);
#endif
	if (res)
		return res;

	if (dt_map_dev(fdt, nodeoffset, &tcb_base, &size) < 0)
		return TEE_ERROR_GENERIC;

	peri_id = (tcb_base == AT91C_BASE_TC0 ? AT91C_ID_TC0 : AT91C_ID_TC1);

	matrix_configure_periph_secure(peri_id);

	tcb_rate = clk_get_rate(clk);

	atmel_tcb_configure();

	return TEE_SUCCESS;
}

static TEE_Result atmel_tcb_probe(const void *fdt, int node,
				   const void *compat_data __unused)

{
	int status = _fdt_get_status(fdt, node);

	return atmel_tcb_setup(fdt, node, status);
}

static const struct dt_device_match atmel_tcb_match_table[] = {
	{ .compatible = "atmel,sama5d2-tcb" },
	{ }
};

DEFINE_DT_DRIVER(atmel_tcb_dt_driver) = {
	.name = "atmel_tcb",
	.type = DT_DRIVER_NOTYPE,
	.match_table = atmel_tcb_match_table,
	.probe = atmel_tcb_probe,
};
