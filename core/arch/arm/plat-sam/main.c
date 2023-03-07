// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2017 Timesys Corporation.
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

#include <arm32.h>
#include <console.h>
#include <drivers/atmel_saic.h>
#include <drivers/atmel_uart.h>
#include <io.h>
#include <kernel/boot.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/tz_ssvce_def.h>
#include <matrix.h>
#include <mm/core_mmu.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#ifdef OPTEE_SAMA7G5
#include <sama7g5.h>
#include <drivers/gic.h>
#else
#include <sama5d2.h>
#endif
#include <sam_sfr.h>
#include <stdint.h>
#include <sm/optee_smc.h>
#include <tz_matrix.h>

static struct atmel_uart_data console_data;

register_phys_mem_pgdir(MEM_AREA_IO_SEC, CONSOLE_UART_BASE,
			CORE_MMU_PGDIR_SIZE);

void console_init(void)
{
#ifdef CONSOLE_UART_BASE
	atmel_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
#endif
}

#ifdef OPTEE_SAMA7G5
#define APB0_BASE 0xe0000000
#define APB1_BASE 0xe1000000
#define APB2_BASE 0xe1800000
#define APB3_BASE 0xe2000000
#define APB4_BASE 0xe2800000
#define APB5_BASE 0xe3000000
#define APB6_BASE 0xe0800000
#define APB7_BASE 0xe3800000
#define APB0_SIZE 0x00800000
#define APB1_SIZE 0x00800000
#define APB2_SIZE 0x00800000
#define APB3_SIZE 0x00800000
#define APB4_SIZE 0x00800000
#define APB5_SIZE 0x00800000
#define APB6_SIZE 0x00800000
#define APB7_SIZE 0x00005000
/*
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB0_BASE, APB0_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB1_BASE, APB1_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB2_BASE, APB2_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB3_BASE, APB3_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB4_BASE, APB4_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB5_BASE, APB5_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB6_BASE, APB6_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB7_BASE, APB7_SIZE);
*/
// register_phys_mem_pgdir(MEM_AREA_IO_NSEC, AHB4_BASE, AHB4_SIZE);
// register_phys_mem_pgdir(MEM_AREA_IO_NSEC, AHB5_BASE, AHB5_SIZE);

// register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB3_BASE, APB4_SIZE);
// register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB5_BASE, APB5_SIZE);
// register_phys_mem_pgdir(MEM_AREA_IO_SEC, AHB4_BASE, AHB4_SIZE);
// register_phys_mem_pgdir(MEM_AREA_IO_SEC, AHB5_BASE, AHB5_SIZE);
// register_phys_mem_pgdir(MEM_AREA_IO_SEC, GIC_BASE, GIC_SIZE);

//register_ddr(DDR_CS_ADDR, CFG_DRAM_SIZE);

register_phys_mem_pgdir(MEM_AREA_IO_SEC, MATRIX_BASE_ADDRESS,
			CORE_MMU_PGDIR_SIZE);

vaddr_t matrix_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(MATRIX_BASE_ADDRESS, MEM_AREA_IO_SEC,
					  1);
		return (vaddr_t)va;
	}
	return MATRIX_BASE_ADDRESS;
}
static void matrix_configure_slave(void)
{
	unsigned int i;
	// PROC_DEBUG_MODE[2:0] = b100
	*((volatile unsigned int *)(SECUMOD_BASE_ADDRESS + 0x70)) = 0x08;
	*((volatile unsigned int *)(SECUMOD_BASE_ADDRESS + 0x7C)) = 0xFFF;
	*((volatile unsigned int *)(SECUMOD_BASE_ADDRESS + 0x88)) = 0;

	// TZPM_KEY
	*((volatile unsigned int *)(TZPM_BASE_ADDRESS + 0x04)) = 0x12AC4B5D;
	*((volatile unsigned int *)(TZPM_BASE_ADDRESS + 0x08)) = 0xFFFFFFFF;
	*((volatile unsigned int *)(TZPM_BASE_ADDRESS + 0x0C)) = 0xFFFFFFFF;
	*((volatile unsigned int *)(TZPM_BASE_ADDRESS + 0x10)) = 0xFFFFFFFF;
	*((volatile unsigned int *)(TZPM_BASE_ADDRESS + 0x14)) = 0xFFFFFFFF;

	// MATRIX_MCFGx
	for (i = 0; i < 14; i++)
		*((volatile unsigned int *)(matrix_base() + 0x4 * i)) = 0x00;

	//MATRIX_SSRx
	for (i = 0; i < 9; i++)
		*((volatile unsigned int *)(matrix_base() + 0x200 + 0x4 * i)) = 0x00FFFF00;
	// MATRIX_SPSELRx
	for (i = 0; i < 3; i++)
		*((volatile unsigned int *)(matrix_base() + 0x2C0 + 0x4 * i)) = 0xFFFFFFFF;
}
#else
register_phys_mem_pgdir(MEM_AREA_IO_SEC, AT91C_BASE_MATRIX32,
			CORE_MMU_PGDIR_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, AT91C_BASE_MATRIX64,
			CORE_MMU_PGDIR_SIZE);

vaddr_t matrix32_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(AT91C_BASE_MATRIX32, MEM_AREA_IO_SEC,
					  1);
		return (vaddr_t)va;
	}
	return AT91C_BASE_MATRIX32;
}

vaddr_t matrix64_base(void)
{
	static void *va;

	if (cpu_mmu_enabled()) {
		if (!va)
			va = phys_to_virt(AT91C_BASE_MATRIX64, MEM_AREA_IO_SEC,
					  1);
		return (vaddr_t)va;
	}
	return AT91C_BASE_MATRIX64;
}

static void matrix_configure_slave_h64mx(void)
{
	unsigned int ddr_port;
	unsigned int ssr_setting;
	unsigned int sasplit_setting;
	unsigned int srtop_setting;

	/*
	 * 0: Bridge from H64MX to AXIMX
	 * (Internal ROM, Crypto Library, PKCC RAM): Always Secured
	 */

	/* 1: H64MX Peripheral Bridge: SDMMC0, SDMMC1 Non-Secure */
	srtop_setting =	MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_128M)
			| MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_128M)
			| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2));
	matrix_configure_slave_security(matrix64_base(),
					H64MX_SLAVE_PERI_BRIDGE,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/*
	 * Matrix DDR configuration is hardcoded here and is difficult to
	 * generate at runtime. Since this configuration expect the secure
	 * DRAM to be at start of RAM and 8M of size, enforce it here.
	 */
	COMPILE_TIME_ASSERT(CFG_TZDRAM_START == AT91C_BASE_DDRCS);
	COMPILE_TIME_ASSERT(CFG_TZDRAM_SIZE == 0x800000);

	/* 2 ~ 9 DDR2 Port1 ~ 7: Non-Secure, except op-tee tee/ta memory */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_8M)
				| MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_128M)
				| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_128M)
				| MATRIX_SASPLIT(3, MATRIX_SASPLIT_VALUE_128M));
	ssr_setting = (MATRIX_LANSECH_S(0)
			| MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_LANSECH_NS(3)
			| MATRIX_RDNSECH_S(0)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_RDNSECH_NS(3)
			| MATRIX_WRNSECH_S(0)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2)
			| MATRIX_WRNSECH_NS(3));
	/* DDR port 0 not used from NWd */
	for (ddr_port = 1; ddr_port < 8; ddr_port++) {
		matrix_configure_slave_security(matrix64_base(),
					(H64MX_SLAVE_DDR2_PORT_0 + ddr_port),
					srtop_setting,
					sasplit_setting,
					ssr_setting);
	}

	/*
	 * 10: Internal SRAM 128K:
	 * - First 64K are reserved for suspend code in Secure World
	 * - Last 64K are for Non-Secure world (used by CAN)
	 */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_64K);
	ssr_setting = (MATRIX_LANSECH_S(0) | MATRIX_RDNSECH_S(0) |
		       MATRIX_WRNSECH_S(0));
	matrix_configure_slave_security(matrix64_base(),
					H64MX_SLAVE_INTERNAL_SRAM,
					srtop_setting, sasplit_setting,
					ssr_setting);

	/* 11:  Internal SRAM 128K (Cache L2): Default */

	/* 12:  QSPI0: Normal world */
	/* 13:  QSPI1: Normal world */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = MATRIX_LANSECH_NS(0) | MATRIX_RDNSECH_NS(0) |
		      MATRIX_WRNSECH_NS(0);

	matrix_configure_slave_security(matrix64_base(), H64MX_SLAVE_QSPI0,
					srtop_setting, sasplit_setting,
					ssr_setting);
	matrix_configure_slave_security(matrix64_base(), H64MX_SLAVE_QSPI1,
					srtop_setting, sasplit_setting,
					ssr_setting);
	/* 14:  AESB: Default */
}

static void matrix_configure_slave_h32mx(void)
{
	unsigned int ssr_setting;
	unsigned int sasplit_setting;
	unsigned int srtop_setting;

	/* 0: Bridge from H32MX to H64MX: Not Secured */
	/* 1: H32MX Peripheral Bridge 0: Not Secured */
	/* 2: H32MX Peripheral Bridge 1: Not Secured */

	/*
	 * 3: External Bus Interface
	 * EBI CS0 Memory(256M) ----> Slave Region 0, 1
	 * EBI CS1 Memory(256M) ----> Slave Region 2, 3
	 * EBI CS2 Memory(256M) ----> Slave Region 4, 5
	 * EBI CS3 Memory(128M) ----> Slave Region 6
	 * NFC Command Registers(128M) -->Slave Region 7
	 * NANDFlash(EBI CS3) --> Slave Region 6: Non-Secure
	 */
	srtop_setting =	MATRIX_SRTOP(6, MATRIX_SRTOP_VALUE_128M);
	srtop_setting |= MATRIX_SRTOP(7, MATRIX_SRTOP_VALUE_128M);
	sasplit_setting = MATRIX_SASPLIT(6, MATRIX_SASPLIT_VALUE_128M);
	sasplit_setting |= MATRIX_SASPLIT(7, MATRIX_SASPLIT_VALUE_128M);
	ssr_setting = (MATRIX_LANSECH_NS(6)
			| MATRIX_RDNSECH_NS(6)
			| MATRIX_WRNSECH_NS(6));
	ssr_setting |= (MATRIX_LANSECH_NS(7)
			| MATRIX_RDNSECH_NS(7)
			| MATRIX_WRNSECH_NS(7));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_EXTERNAL_EBI,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 4: NFC SRAM (4K): Non-Secure */
	srtop_setting = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_8K);
	sasplit_setting = MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_8K);
	ssr_setting = (MATRIX_LANSECH_NS(0)
			| MATRIX_RDNSECH_NS(0)
			| MATRIX_WRNSECH_NS(0));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_NFC_SRAM,
					srtop_setting,
					sasplit_setting,
					ssr_setting);

	/* 5:
	 * USB Device High Speed Dual Port RAM (DPR): 1M
	 * USB Host OHCI registers: 1M
	 * USB Host EHCI registers: 1M
	 */
	srtop_setting = (MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_1M)
			| MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_1M)
			| MATRIX_SRTOP(2, MATRIX_SRTOP_VALUE_1M));
	sasplit_setting = (MATRIX_SASPLIT(0, MATRIX_SASPLIT_VALUE_1M)
			| MATRIX_SASPLIT(1, MATRIX_SASPLIT_VALUE_1M)
			| MATRIX_SASPLIT(2, MATRIX_SASPLIT_VALUE_1M));
	ssr_setting = (MATRIX_LANSECH_NS(0)
			| MATRIX_LANSECH_NS(1)
			| MATRIX_LANSECH_NS(2)
			| MATRIX_RDNSECH_NS(0)
			| MATRIX_RDNSECH_NS(1)
			| MATRIX_RDNSECH_NS(2)
			| MATRIX_WRNSECH_NS(0)
			| MATRIX_WRNSECH_NS(1)
			| MATRIX_WRNSECH_NS(2));
	matrix_configure_slave_security(matrix32_base(),
					H32MX_USB,
					srtop_setting,
					sasplit_setting,
					ssr_setting);
}
#endif

#ifdef OPTEE_SAMA7G5
static unsigned int security_ps_peri_id[] = {
	ID_DWDT_SW,
	ID_DWDT_NSW,
	ID_DWDT_NSW_ALARM,
	ID_SCKC,
	ID_SHDWC,
	ID_RSTC,
	ID_RTC,
	ID_RTT,
	ID_CHIPID,
	ID_PMC,
	ID_PIOA,
	ID_PIOB,
	ID_PIOC,
	ID_PIOD,
	ID_PIOE,
	ID_SECUMOD,
	ID_SECURAM,
	ID_SFR,
	ID_SFRBU,
	ID_HSMC,
	ID_XDMAC0,
	ID_XDMAC1,
	ID_XDMAC2,
	ID_ACC,
	ID_ADC,
	ID_AES,
	ID_TZAESBASC,
	ID_ASRC,
	ID_CPKCC,
	ID_CSI,
	ID_CSI2DC,
	ID_DDRPUBL,
	ID_DDRUMCTL,
	ID_EIC,
	ID_FLEXCOM0,
	ID_FLEXCOM1,
	ID_FLEXCOM2,
	ID_FLEXCOM3,
	ID_FLEXCOM4,
	ID_FLEXCOM5,
	ID_FLEXCOM6,
	ID_FLEXCOM7,
	ID_FLEXCOM8,
	ID_FLEXCOM9,
	ID_FLEXCOM10,
	ID_FLEXCOM11,
	ID_GMAC0,
	ID_GMAC1,
	ID_GMAC0_TSU,
	ID_GMAC1_TSU,
	ID_ICM,
	ID_ISC,
	ID_I2SMCC0,
	ID_I2SMCC1,
	ID_MATRIX,
	ID_MCAN0,
	ID_MCAN1,
	ID_MCAN2,
	ID_MCAN3,
	ID_MCAN4,
	ID_MCAN5,
	ID_OTPC,
	ID_PDMC0,
	ID_PDMC1,
	ID_PIT64B0,
	ID_PIT64B1,
	ID_PIT64B2,
	ID_PIT64B3,
	ID_PIT64B4,
	ID_PIT64B5,
	ID_PWM,
	ID_QSPI0,
	ID_QSPI1,
	ID_SDMMC0,
	ID_SDMMC1,
	ID_SDMMC2,
	ID_SHA,
	ID_SPDIFRX,
	ID_SPDIFTX,
	ID_SSC0,
	ID_SSC1,
	ID_TC0_CHANNEL0,
	ID_TC0_CHANNEL1,
	ID_TC0_CHANNEL2,
	ID_TC1_CHANNEL0,
	ID_TC1_CHANNEL1,
	ID_TC1_CHANNEL2,
	ID_TCPCA,
	ID_TCPCB,
	ID_TDES,
	ID_TRNG,
	ID_TZAESB_NS,
	ID_TZAESB_NS_SINT,
	ID_TZAESB_S,
	ID_TZAESB_S_SINT,
	ID_TZC,
	ID_TZPM,
	ID_UDPHSA,
	ID_UDPHSB,
	ID_UHPHS,
	ID_XDMAC0_SINT,
	ID_XDMAC1_SINT,
	ID_XDMAC2_SINT,
	ID_AES_SINT,
	ID_GMAC0_Q1,
	ID_GMAC0_Q2,
	ID_GMAC0_Q3,
	ID_GMAC0_Q4,
	ID_GMAC0_Q5,
	ID_GMAC1_Q1,
	ID_ICM_SINT,
	ID_MCAN0_INT1,
	ID_MCAN1_INT1,
	ID_MCAN2_INT1,
	ID_MCAN3_INT1,
	ID_MCAN4_INT1,
	ID_MCAN5_INT1,
	ID_PIOA_SINT,
	ID_PIOB_SINT,
	ID_PIOC_SINT,
	ID_PIOD_SINT,
	ID_PIOE_SINT,
	ID_PIT64B0_SINT,
	ID_PIT64B1_SINT,
	ID_PIT64B2_SINT,
	ID_PIT64B3_SINT,
	ID_PIT64B4_SINT,
	ID_PIT64B5_SINT,
	ID_SDMMC0_TIMER,
	ID_SDMMC1_TIMER,
	ID_SDMMC2_TIMER,
	ID_SHA_SINT,
	ID_TC0_SINT0,
	ID_TC0_SINT1,
	ID_TC0_SINT2,
	ID_TC1_SINT0,
	ID_TC1_SINT1,
	ID_TC1_SINT2,
	ID_TDES_SINT,
	ID_TRNG_SINT,
	ID_EXT_IRQ0,
	ID_EXT_IRQ1,
};
#else
static unsigned int security_ps_peri_id[] = {
	AT91C_ID_PMC,
	AT91C_ID_ARM,
	AT91C_ID_PIT,
	AT91C_ID_WDT,
	AT91C_ID_GMAC,
	AT91C_ID_XDMAC0,
	AT91C_ID_XDMAC1,
	AT91C_ID_ICM,
	AT91C_ID_AES,
	AT91C_ID_AESB,
	AT91C_ID_TDES,
	AT91C_ID_SHA,
	AT91C_ID_MPDDRC,
	AT91C_ID_HSMC,
	AT91C_ID_FLEXCOM0,
	AT91C_ID_FLEXCOM1,
	AT91C_ID_FLEXCOM2,
	AT91C_ID_FLEXCOM3,
	AT91C_ID_FLEXCOM4,
	AT91C_ID_UART0,
	AT91C_ID_UART1,
	AT91C_ID_UART2,
	AT91C_ID_UART3,
	AT91C_ID_UART4,
	AT91C_ID_TWI0,
	AT91C_ID_TWI1,
	AT91C_ID_SDMMC0,
	AT91C_ID_SDMMC1,
	AT91C_ID_SPI0,
	AT91C_ID_SPI1,
	AT91C_ID_TC0,
	AT91C_ID_TC1,
	AT91C_ID_PWM,
	AT91C_ID_ADC,
	AT91C_ID_UHPHS,
	AT91C_ID_UDPHS,
	AT91C_ID_SSC0,
	AT91C_ID_SSC1,
	AT91C_ID_LCDC,
	AT91C_ID_ISI,
	AT91C_ID_TRNG,
	AT91C_ID_PDMIC,
	AT91C_ID_SFC,
	AT91C_ID_QSPI0,
	AT91C_ID_QSPI1,
	AT91C_ID_I2SC0,
	AT91C_ID_I2SC1,
	AT91C_ID_CAN0_INT0,
	AT91C_ID_CAN1_INT0,
	AT91C_ID_CLASSD,
	AT91C_ID_SFR,
	AT91C_ID_L2CC,
	AT91C_ID_CAN0_INT1,
	AT91C_ID_CAN1_INT1,
	AT91C_ID_GMAC_Q1,
	AT91C_ID_GMAC_Q2,
	AT91C_ID_SDMMC0_TIMER,
	AT91C_ID_SDMMC1_TIMER,
	AT91C_ID_SYS,
	AT91C_ID_ACC,
	AT91C_ID_RXLP,
	AT91C_ID_SFRBU,
	AT91C_ID_CHIPID,
};
#endif

static int matrix_init(void)
{
#ifdef OPTEE_SAMA7G5
	matrix_write_protect_disable(matrix_base());
	matrix_configure_slave();
#else
	matrix_write_protect_disable(matrix64_base());
	matrix_write_protect_disable(matrix32_base());

	matrix_configure_slave_h64mx();
	matrix_configure_slave_h32mx();
#endif

	return matrix_configure_periph_non_secure(security_ps_peri_id,
					      ARRAY_SIZE(security_ps_peri_id));
}

void plat_primary_init_early(void)
{
	matrix_init();
}

#ifdef OPTEE_SAMA7G5
static struct gic_data gic_data;
register_phys_mem_pgdir(MEM_AREA_IO_SEC, GIC_INTERFACE_BASE, GICC_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, GIC_DISTRIBUTOR_BASE, GICD_SIZE);
#endif

void itr_core_handler(void)
{
#ifdef OPTEE_SAMA7G5
	gic_it_handle(&gic_data);
#else
	atmel_saic_it_handle();
#endif
}

void main_init_gic(void)
{
#ifdef OPTEE_SAMA7G5
	vaddr_t gicc_base;
	vaddr_t gicd_base;

	assert(cpu_mmu_enabled());

	gicc_base = (vaddr_t)phys_to_virt(GIC_INTERFACE_BASE, MEM_AREA_IO_SEC,
					  GICC_SIZE);
	gicd_base = (vaddr_t)phys_to_virt(GIC_DISTRIBUTOR_BASE, MEM_AREA_IO_SEC,
					  GICD_SIZE);
	if (!gicc_base || !gicd_base)
		panic();

	gic_init(&gic_data, gicc_base, gicd_base);
	itr_init(&gic_data.chip);
#else
	if (atmel_saic_setup())
		panic("Failed to init interrupts\n");

	matrix_interrupt_init();
#endif
}
