// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2019 Microchip.
 */
#include <assert.h>
#include <drivers/clk.h>
#include <drivers/clk_dt.h>
#include <drivers/pinctrl.h>
#include <initcall.h>
#include <io.h>
#include <libfdt.h>
#include <kernel/dt.h>
#include <malloc.h>
#include <matrix.h>
#include <platform_config.h>
#include <trace.h>
#include <util.h>

#define PIO_GROUP_COUNT		4
#define PIO_GROUP_OFFSET	0x40
#define PIO_REG(reg, group)	((reg) + ((group) * PIO_GROUP_OFFSET))
/* Mask register */
#define PIO_MSKR(group)		PIO_REG(0x0, group)
/* Configuration register */
#define PIO_CFGR(group)		PIO_REG(0x4, group)
#define PIO_CFGR_FUNC		GENMASK(2, 0)
#define PIO_CFGR_PUEN		BIT(9)
#define PIO_CFGR_PDEN		BIT(10)

/* Non-Secure configuration register */
#define PIO_SIONR(group)	PIO_REG(0x30, group)
/* Secure configuration register */
#define PIO_SIOSR(group)	PIO_REG(0x34, group)

#define DT_GET_PIN_NO(val)	((val) & 0xFF)
#define DT_GET_FUNC(val)	(((val) >> 16) & 0xF)

struct atmel_pio {
	vaddr_t base;
	struct pinctrl pinctrl;
};

static void pio_write(struct atmel_pio *pio, unsigned int offset, uint32_t val)
{
	io_write32(pio->base + offset, val);
}

static TEE_Result atmel_pinctrl_apply_state(struct pinctrl *pinctrl,
					    const void *fdt, int node)
{
	int func = 0;
	int group = 0;
	int pin_no = 0;
	uint32_t cfg = 0;
	unsigned int i = 0;
	int pio_group = -1;
	uint32_t pinmux = 0;
	uint32_t pin_mask = 0;
	uint32_t *configs = NULL;
	unsigned int prop_count = 0;
	const uint32_t *prop = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;
	struct atmel_pio *pio = container_of(pinctrl, struct atmel_pio, pinctrl);
	unsigned int configs_count = 0;

	prop = fdt_getprop(fdt, node, "pinmux", (int *)&prop_count);
	if (!prop)
		return TEE_ERROR_ITEM_NOT_FOUND;

	prop_count /= 4;
	for (i = 0; i < prop_count; i++) {
		pinmux = fdt32_to_cpu(prop[i]);

		pin_no = DT_GET_PIN_NO(pinmux);
		func = DT_GET_FUNC(pinmux);

		group = pin_no / 32;
		if (pio_group == -1)
			pio_group = group;
		else
			assert(group == pio_group);

		pin_mask |= BIT(pin_no % 32);
	}

	cfg = func;

	res = pinctrl_parse_dt_pin_mode_config(fdt, node, &configs,
					       &configs_count);
	if (res)
		return res;

	for (i = 0; i < configs_count; i++) {
		switch (configs[i]) {
			case PIN_MODE_BIAS_PULL_UP:
				cfg |= PIO_CFGR_PUEN;
				cfg &= (~PIO_CFGR_PDEN);
			break;
			case PIN_MODE_BIAS_PULL_DOWN:
				cfg |= PIO_CFGR_PDEN;
				cfg &= (~PIO_CFGR_PUEN);
			break;
			case PIN_MODE_BIAS_DISABLE:
			break;
			default:
				DMSG("Unhandled mode %"PRIu32"\n", configs[i]);
			break;
		}
	}

	free(configs);

	DMSG("Applying configuration on group %d, pins %x, cfg %x\n", pio_group, pin_mask, cfg);

	pio_write(pio, PIO_SIOSR(pio_group), pin_mask);
	pio_write(pio, PIO_MSKR(pio_group), pin_mask);
	pio_write(pio, PIO_CFGR(pio_group), cfg);

	return TEE_SUCCESS;
}

static void pio_init_hw(struct atmel_pio *pio)
{
	int i;

	/* Set all IOs as non-secure */
	for (i = 0; i < PIO_GROUP_COUNT; i++)
		pio_write(pio, PIO_SIONR(PIO_GROUP_COUNT), GENMASK_32(31, 0));
}

static TEE_Result pio_node_probe(const void *fdt, int node,
				 const void *compat_data __unused)
{
	size_t size = 0;
	struct clk *clk = NULL;
	TEE_Result res = TEE_ERROR_GENERIC;
	struct atmel_pio *pio = NULL;

	if (_fdt_get_status(fdt, node) != DT_STATUS_OK_SEC)
		return TEE_ERROR_BAD_STATE;

	pio = calloc(1, sizeof(*pio));
	if (!pio)
		return TEE_ERROR_OUT_OF_MEMORY;

	res = clk_dt_get_by_index(fdt, node, 0, &clk);
	if (res)
		goto free_pio;

	if (dt_map_dev(fdt, node, &pio->base, &size) < 0)
		goto free_pio;

	clk_enable(clk);

#ifdef OPTEE_SAMA7G5
	matrix_configure_periph_secure(ID_PIOA);
	matrix_configure_periph_secure(ID_PIOB);
	matrix_configure_periph_secure(ID_PIOC);
	matrix_configure_periph_secure(ID_PIOD);
	matrix_configure_periph_secure(ID_PIOE);
#else
	matrix_configure_periph_secure(AT91C_ID_PIOA);
	matrix_configure_periph_secure(AT91C_ID_PIOB);
	matrix_configure_periph_secure(AT91C_ID_PIOC);
	matrix_configure_periph_secure(AT91C_ID_PIOD);
#endif

	pio_init_hw(pio);

	pio->pinctrl.node = node;
	pio->pinctrl.apply_state = atmel_pinctrl_apply_state;

	pinctrl_register(&pio->pinctrl);

	return TEE_SUCCESS;

free_pio:
	free(pio);

	return res;
}

static const struct dt_device_match atmel_pio_match_table[] = {
	{ .compatible = "atmel,sama5d2-pinctrl" },
	{ .compatible = "microchip,sama7g5-pinctrl" },
	{ }
};

DEFINE_DT_DRIVER(atmel_pio_dt_driver) = {
	.name = "atmel_pio",
	.type = DT_DRIVER_NOTYPE,
	.match_table = atmel_pio_match_table,
	.probe = pio_node_probe,
};
