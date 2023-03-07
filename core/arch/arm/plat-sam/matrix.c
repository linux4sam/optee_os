// SPDX-License-Identifier: BSD-Source-Code
/*
 * Copyright (c) 2013, Atmel Corporation
 * Copyright (c) 2017, Timesys Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <arm32.h>
#include <assert.h>
#include <config.h>
#include <drivers/atmel_saic.h>
#include <initcall.h>
#include <io.h>
#include <kernel/interrupt.h>
#include <kernel/pm.h>
#include <kernel/panic.h>
#include <matrix.h>
#include <platform_config.h>
#ifdef OPTEE_SAMA7G5
#include <sama7g5.h>
#else
#include <sama5d2.h>
#endif
#include <sm/sm.h>
#include <stdint.h>
#include <tz_matrix.h>
#include <trace.h>

#ifndef OPTEE_SAMA7G5
#define MATRIX_H64MX	0
#define MATRIX_H32MX	1
#define MATRIX_COUNT	2
#endif

#define SECURITY_TYPE_AS	1
#define SECURITY_TYPE_NS	2
#define SECURITY_TYPE_PS	3

#define WORLD_NON_SECURE	0
#define WORLD_SECURE		1

#define MATRIX_SPSELR_COUNT	3
#define MATRIX_SLAVE_COUNT	15

#define MATRIX_MASTER_COUNT	12

struct peri_security {
	unsigned int peri_id;
	unsigned int matrix;
	unsigned int security_type;
};

#ifdef OPTEE_SAMA7G5
static const struct peri_security peri_security_array[] = {
	{.peri_id = ID_DWDT_SW,        .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_DWDT_NSW,       .security_type = SECURITY_TYPE_NS, },
	{.peri_id = ID_DWDT_NSW_ALARM, .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_SCKC,           .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_SHDWC,          .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_RSTC,           .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_RTC,            .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_RTT,            .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_CHIPID,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PMC,            .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_PIOA,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOB,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOC,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOD,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOE,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SECUMOD,        .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_SECURAM,        .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_SFR,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SFRBU,          .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_HSMC,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC0,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC1,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC2,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_ACC,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_ADC,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_AES,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TZAESBASC,      .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_ASRC,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_CPKCC,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_CSI,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_CSI2DC,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_DDRPUBL,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_DDRUMCTL,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_EIC,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM0,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM1,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM2,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM3,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM4,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM5,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM6,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM7,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM8,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM9,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM10,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_FLEXCOM11,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC1,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_TSU,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC1_TSU,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_ICM,            .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_ISC,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_I2SMCC0,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_I2SMCC1,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MATRIX,         .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_MCAN0,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN1,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN2,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN3,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN4,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN5,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_OTPC,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PDMC0,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PDMC1,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B0,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B1,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B2,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B3,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B4,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B5,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PWM,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_QSPI0,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_QSPI1,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC0,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC1,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC2,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SHA,            .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SPDIFRX,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SPDIFTX,        .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SSC0,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SSC1,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC0_CHANNEL0,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC0_CHANNEL1,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC0_CHANNEL2,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC1_CHANNEL0,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC1_CHANNEL1,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TC1_CHANNEL2,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TCPCA,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TCPCB,          .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TDES,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TRNG,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TZAESB_NS,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TZAESB_NS_SINT, .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_TZAESB_S,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TZAESB_S_SINT,  .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_TZC,            .security_type = SECURITY_TYPE_AS, },
//	{.peri_id = ID_TZPM,           .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_UDPHSA,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_UDPHSB,         .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_UHPHS,          .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = AT91C_ID_ARM,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = AT91C_ID_ARM,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC0_SINT,    .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC1_SINT,    .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_XDMAC2_SINT,    .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_AES_SINT,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_Q1,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_Q2,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_Q3,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_Q4,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC0_Q5,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_GMAC1_Q1,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_ICM_SINT,       .security_type = SECURITY_TYPE_AS, },
	{.peri_id = ID_MCAN0_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN1_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN2_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN3_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN4_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_MCAN5_INT1,     .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOA_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOB_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOC_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOD_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIOE_SINT,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_PIT64B0_SINT,   .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_PIT64B1_SINT,   .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_PIT64B2_SINT,   .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_PIT64B3_SINT,   .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_PIT64B4_SINT,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_PIT64B5_SINT,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC0_TIMER,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC1_TIMER,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SDMMC2_TIMER,   .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_SHA_SINT,       .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC0_SINT0,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC0_SINT1,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC0_SINT2,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC1_SINT0,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC1_SINT1,      .security_type = SECURITY_TYPE_PS, },
//	{.peri_id = ID_TC1_SINT2,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TDES_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_TRNG_SINT,      .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_EXT_IRQ0,       .security_type = SECURITY_TYPE_PS, },
	{.peri_id = ID_EXT_IRQ1,       .security_type = SECURITY_TYPE_PS, },
};
#else
static const struct peri_security peri_security_array[] = {
	{
		.peri_id = AT91C_ID_PMC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ARM,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIT,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_WDT,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_XDMAC0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_XDMAC1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ICM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_AES,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_AESB,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TDES,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SHA,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_MPDDRC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_MATRIX1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_MATRIX0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_SECUMOD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_HSMC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIOA,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM3,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_FLEXCOM4,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART3,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UART4,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TWI0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TWI1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SPI0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SPI1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PWM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ADC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UHPHS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_UDPHS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SSC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SSC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_LCDC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ISI,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_TRNG,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PDMIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_IRQ,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_NS,
	},
	{
		.peri_id = AT91C_ID_SFC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SECURAM,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_QSPI0,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_QSPI1,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_I2SC0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_I2SC1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN0_INT0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN1_INT0,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CLASSD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SFR,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SAIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_AIC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_NS,
	},
	{
		.peri_id = AT91C_ID_L2CC,
		.matrix = MATRIX_H64MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN0_INT1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CAN1_INT1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC_Q1,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_GMAC_Q2,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_PIOB,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_PIOC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_PIOD,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_AS,
	},
	{
		.peri_id = AT91C_ID_SDMMC0_TIMER,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SDMMC1_TIMER,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SYS,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_ACC,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_RXLP,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_SFRBU,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
	{
		.peri_id = AT91C_ID_CHIPID,
		.matrix = MATRIX_H32MX,
		.security_type = SECURITY_TYPE_PS,
	},
};
#endif
static void matrix_write(unsigned int base,
			 unsigned int offset,
			 const unsigned int value)
{
	io_write32(offset + base, value);
}

static unsigned int matrix_read(int base, unsigned int offset)
{
	return io_read32(offset + base);
}

void matrix_write_protect_enable(unsigned int matrix_base)
{
	matrix_write(matrix_base, MATRIX_WPMR,
		    (MATRIX_WPMR_WPKEY_PASSWD | MATRIX_WPMR_WPEN_ENABLE));
}

void matrix_write_protect_disable(unsigned int matrix_base)
{
	matrix_write(matrix_base, MATRIX_WPMR, MATRIX_WPMR_WPKEY_PASSWD);
}
#ifdef OPTEE_SAMA7G5
#else
static vaddr_t matrix_get_base(unsigned int matrix)
{
	if (matrix == MATRIX_H32MX)
		return matrix32_base();
	else if (matrix == MATRIX_H64MX)
		return matrix64_base();
	else
		return 0;
}
#endif
static void matrix_disp_error(unsigned int matrix)
{
	uint8_t master = 0;
#ifdef OPTEE_SAMA7G5
	vaddr_t base = matrix_base();
#else
	vaddr_t base = matrix_get_base(matrix);
#endif
	uint32_t mesr = matrix_read(base, MATRIX_MESR);
	paddr_t addr = 0;
	struct sm_nsec_ctx *ctx = NULL;

	if (mesr == 0) {
		EMSG("Matrix %d it triggered but no error !", matrix);
		return;
	}

	master = __builtin_ctz(mesr);
	assert(master < MATRIX_MASTER_COUNT);

	addr = matrix_read(base, MATRIX_MEAR(master));
	ctx = sm_get_nsec_ctx();

	// EMSG("Matrix %d permission failure from master %d, address 0x%lx, mon_lr = 0x%"PRIx32,
	//      matrix, master, addr, ctx->mon_lr);
}

static enum itr_return matrix_it_handler(struct itr_handler *handler __unused)
{
	unsigned int matrix = (unsigned int) handler->data;

	matrix_disp_error(matrix);

	if (IS_ENABLED(CFG_AT91_MATRIX_PANIC_ON_VIOLATION))
		panic();

	return ITRR_HANDLED;
}

#ifdef OPTEE_SAMA7G5
static struct itr_handler matrix_itr_handler = {
	.it = ID_MATRIX,
	.handler = matrix_it_handler,
//	.data = (void *) MATRIX_H64MX,
};
#else
static struct itr_handler matrix_itr_handlers[MATRIX_COUNT] = {
	{
		.it = AT91C_ID_MATRIX0,
		.handler = matrix_it_handler,
		.data = (void *) MATRIX_H64MX,
	},
	{
		.it = AT91C_ID_MATRIX1,
		.handler = matrix_it_handler,
		.data = (void *) MATRIX_H32MX,
	}
};
#endif

void matrix_interrupt_init(void)
{
	int i = 0;
	vaddr_t base = 0;

#ifdef OPTEE_SAMA7G5
	itr_add_type_prio(&matrix_itr_handler, IRQ_TYPE_LEVEL_HIGH, 0);
	itr_enable(matrix_itr_handler.it);
	base = matrix_base();

	/* Enable errors interrupts for all masters */
	matrix_write(base, MATRIX_MEIER, MATRIX_MASTER_COUNT - 1);
	/* Unmask all masters */
	matrix_write(base, MATRIX_MEIMR, MATRIX_MASTER_COUNT - 1);
#else
	for (i = 0; i < MATRIX_COUNT; i++) {
		itr_add_type_prio(&matrix_itr_handlers[i], IRQ_TYPE_LEVEL_HIGH,
				  0);
		itr_enable(matrix_itr_handlers[i].it);
		base = matrix_get_base(i);

		/* Enable errors interrupts for all masters */
		matrix_write(base, MATRIX_MEIER, MATRIX_MASTER_COUNT - 1);
		/* Unmask all masters */
		matrix_write(base, MATRIX_MEIMR, MATRIX_MASTER_COUNT - 1);
	}
#endif
}

void matrix_configure_slave_security(unsigned int matrix_base,
				     unsigned int slave,
				     unsigned int srtop_setting,
				     unsigned int srsplit_setting,
				     unsigned int ssr_setting)
{
	matrix_write(matrix_base, MATRIX_SSR(slave), ssr_setting);
	matrix_write(matrix_base, MATRIX_SRTSR(slave), srtop_setting);
	matrix_write(matrix_base, MATRIX_SASSR(slave), srsplit_setting);
}

static const struct peri_security *get_peri_security(unsigned int peri_id)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(peri_security_array); i++) {
		if (peri_id == peri_security_array[i].peri_id)
			return &peri_security_array[i];
	}

	return NULL;
}

static int matrix_set_periph_world(unsigned int matrix, unsigned int peri_id,
				   unsigned int world)
{
	unsigned int base;
	unsigned int spselr;
	unsigned int idx;
	unsigned int bit;

	idx = peri_id / 32;
	if (idx > 3)
		return -1;

	bit = (0x01 << (peri_id % 32));

#ifdef OPTEE_SAMA7G5
	base = matrix_base();
#else
	if (matrix == MATRIX_H32MX)
		base = matrix32_base();
	else if (matrix == MATRIX_H64MX)
		base = matrix64_base();
	else
		return -1;
#endif

	spselr = matrix_read(base, MATRIX_SPSELR(idx));
	if (world == WORLD_SECURE)
		spselr &= ~bit;
	else
		spselr |= bit;
	matrix_write(base, MATRIX_SPSELR(idx), spselr);

	return 0;
}

int matrix_configure_periph_secure(unsigned int peri_id)
{
	const struct peri_security *psec = NULL;

	psec = get_peri_security(peri_id);
	if (!psec)
		return -1;

	return matrix_set_periph_world(psec->matrix, peri_id, WORLD_SECURE);
}

int matrix_configure_periph_non_secure(unsigned int *peri_id_array,
				       unsigned int size)
{
	unsigned int i;
	unsigned int *peri_id_p;
	unsigned int matrix;
	unsigned int peri_id;
	const struct peri_security *peripheral_sec;
	int ret;

	if (!peri_id_array || !size)
		return -1;

	peri_id_p = peri_id_array;
	for (i = 0; i < size; i++) {
		peripheral_sec = get_peri_security(*peri_id_p);
		if (!peripheral_sec)
			return -1;

		if (peripheral_sec->security_type != SECURITY_TYPE_PS)
			return -1;

		matrix = peripheral_sec->matrix;
		peri_id = *peri_id_p;
		ret = matrix_set_periph_world(matrix, peri_id,
					      WORLD_NON_SECURE);
		if (ret)
			return -1;

		peri_id_p++;
	}

	return 0;
}

#ifdef CFG_PM_ARM32
struct matrix_state {
	uint32_t spselr[MATRIX_SPSELR_COUNT];
	uint32_t ssr[MATRIX_SLAVE_COUNT];
	uint32_t srtsr[MATRIX_SLAVE_COUNT];
	uint32_t sassr[MATRIX_SLAVE_COUNT];
	uint32_t meier;
	uint32_t meimr;
};

static struct matrix_state matrix32_state;
static struct matrix_state matrix64_state;

static void matrix_save_regs(vaddr_t base, struct matrix_state *state)
{
	int idx = 0;

	for (idx = 0; idx < MATRIX_SPSELR_COUNT; idx++)
		state->spselr[idx] = matrix_read(base, MATRIX_SPSELR(idx));

	for (idx = 0; idx < MATRIX_SLAVE_COUNT; idx++) {
		state->ssr[idx] = matrix_read(base, MATRIX_SSR(idx));
		state->srtsr[idx] = matrix_read(base, MATRIX_SRTSR(idx));
		state->sassr[idx] = matrix_read(base, MATRIX_SASSR(idx));
	}

	state->meier = matrix_read(base, MATRIX_MEIER);
	state->meimr = matrix_read(base, MATRIX_MEIMR);
}

static void matrix_suspend(void)
{
#ifdef OPTEE_SAMA7G5
	matrix_save_regs(matrix_base(), &matrix32_state);
#else
	matrix_save_regs(matrix32_base(), &matrix32_state);
	matrix_save_regs(matrix64_base(), &matrix64_state);
#endif
}

static void matrix_restore_regs(vaddr_t base, struct matrix_state *state)
{
	int idx = 0;

	matrix_write_protect_disable(base);

	for (idx = 0; idx < MATRIX_SPSELR_COUNT; idx++)
		matrix_write(base, MATRIX_SPSELR(idx), state->spselr[idx]);

	for (idx = 0; idx < MATRIX_SLAVE_COUNT; idx++) {
		matrix_write(base, MATRIX_SSR(idx), state->ssr[idx]);
		matrix_write(base, MATRIX_SRTSR(idx), state->srtsr[idx]);
		matrix_write(base, MATRIX_SASSR(idx), state->sassr[idx]);
	}

	matrix_write(base, MATRIX_MEIER, state->meier);
	matrix_write(base, MATRIX_MEIMR, state->meimr);
}

static void matrix_resume(void)
{
#ifdef OPTEE_SAMA7G5
	matrix_restore_regs(matrix_base(), &matrix32_state);
#else
	matrix_restore_regs(matrix32_base(), &matrix32_state);
	matrix_restore_regs(matrix64_base(), &matrix64_state);
#endif
}

static TEE_Result matrix_pm(enum pm_op op, uint32_t pm_hint __unused,
			    const struct pm_callback_handle *hdl __unused)
{
	switch (op) {
	case PM_OP_RESUME:
		matrix_resume();
		break;
	case PM_OP_SUSPEND:
		matrix_suspend();
		break;
	default:
		panic("Invalid PM operation");
	}

	return TEE_SUCCESS;
}

static TEE_Result matrix_pm_init(void)
{
	/*
	 * We can't call matrix_register_pm in matrix_init since allocator is
	 * not ready yet so we just call it later in this driver init callback.
	 */
	register_pm_driver_cb(matrix_pm, NULL, "sam-matrix");

	return TEE_SUCCESS;
}
driver_init(matrix_pm_init);

#endif
