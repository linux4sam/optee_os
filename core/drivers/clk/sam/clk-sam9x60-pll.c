// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2019 Microchip Technology Inc.
 *
 */

#include <io.h>
#include <kernel/delay.h>
#include <util.h>
#include <mm/core_memprot.h>
#include <types_ext.h>

#include "at91_clk.h"

#define mult_frac(x, numer, denom)( 		\
{							\
	typeof(x) quot = (x) / (denom); 		\
	typeof(x) rem  = (x) % (denom); 		\
	(quot * (numer)) + ((rem * (numer)) / (denom)); \
}							\
)

#define PMC_PLL_CTRL0_DIV_MSK 0xf
#define PMC_PLL_CTRL0_DIV_POS 0
#define	PMC_PLL_CTRL1_MUL_MSK 0xff
#define	PMC_PLL_CTRL1_MUL_POS 24
#define	PMC_PLL_CTRL1_FRACR_MSK	0x3fffff
#define	PMC_PLL_CTRL1_FRACR_POS	0

#define PLL_STATUS_MASK(id)	BIT(1 + (id))
#define PLL_REG(id)		(AT91_CKGR_PLLAR + ((id) * 4))
#define PLL_DIV_MASK		0xff
#define PLL_DIV_MAX		PLL_DIV_MASK
#define PLL_DIV(reg)		((reg) & PLL_DIV_MASK)
#define PLL_MUL(reg, layout) \
	({ \
		typeof(layout) __layout = layout; \
		\
		(((reg) >> (__layout)->mul_shift) & (__layout)->mul_mask); \
	})
#define PLL_MUL_MIN		2
#define PLL_MUL_MASK(layout)	((layout)->mul_mask)
#define PLL_MUL_MAX(layout)	(PLL_MUL_MASK(layout) + 1)
#define PLL_ICPR_SHIFT(id)	((id) * 16)
#define PLL_ICPR_MASK(id)	(0xffff << PLL_ICPR_SHIFT(id))
#define PLL_MAX_COUNT		0x3f
#define PLL_COUNT_SHIFT		8
#define PLL_OUT_SHIFT		14

struct sam9x60_pll_core {
	vaddr_t base;
	const struct clk_pll_charac *charac;
	const struct clk_pll_layout *layout;
	struct clk *hw;
	uint8_t id;
};

struct sam9x60_frac {
	struct sam9x60_pll_core core;
	uint32_t frac;
	uint16_t mul;
};

struct sam9x60_div {
	struct sam9x60_pll_core core;
	uint8_t div;
	uint8_t safe_div;
};

static inline bool sam9x60_pll_ready(vaddr_t base, int id)
{
	unsigned int status = io_read32(base + AT91_PMC_PLL_ISR0);

	return !!(status & BIT(id));
}

static bool sam9x60_frac_pll_ready(vaddr_t regmap, uint8_t id)
{
	return sam9x60_pll_ready(regmap, id);
}

static unsigned long sam9x60_frac_pll_recalc_rate(struct clk *hw,
						  unsigned long parent_rate)
{
	struct sam9x60_frac *frac = hw->priv;
	struct sam9x60_pll_core *core = &frac->core;
	unsigned long freq;

	freq = parent_rate * (frac->mul + 1) +
		UDIV_ROUND_NEAREST((unsigned long long)parent_rate * frac->frac, (1 << 22));

	if (core->layout->div2)
		freq >>= 1;

	return freq;
}

static TEE_Result sam9x60_frac_pll_set(struct sam9x60_frac *frac)
{
	struct sam9x60_pll_core *core = &frac->core;
	vaddr_t regmap = frac->core.base;
	unsigned int val, cfrac, cmul;

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_ID_MSK, core->id);
	val = io_read32(regmap + AT91_PMC_PLL_CTRL1);
	cmul = (val & core->layout->mul_mask) >> core->layout->mul_shift;
	cfrac = (val & core->layout->frac_mask) >> core->layout->frac_shift;

	if (sam9x60_frac_pll_ready(regmap, core->id) &&
	    (cmul == frac->mul && cfrac == frac->frac))
		return 0;

	/* Recommended value for PMC_PLL_ACR */
	if (core->charac->upll)
		val = AT91_PMC_PLL_ACR_DEFAULT_UPLL;
	else
		val = AT91_PMC_PLL_ACR_DEFAULT_PLLA;
	io_write32(regmap + AT91_PMC_PLL_ACR, val);

	io_write32(regmap + AT91_PMC_PLL_CTRL1,
		     (frac->mul << core->layout->mul_shift) |
		     (frac->frac << core->layout->frac_shift));

	if (core->charac->upll) {
		/* Enable the UTMI internal bandgap */
		val |= AT91_PMC_PLL_ACR_UTMIBG;
		io_write32(regmap + AT91_PMC_PLL_ACR, val);

		udelay(10);

		/* Enable the UTMI internal regulator */
		val |= AT91_PMC_PLL_ACR_UTMIVR;
		io_write32(regmap + AT91_PMC_PLL_ACR, val);

		udelay(10);
	}

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
			   AT91_PMC_PLL_UPDT_UPDATE | core->id);

	io_clrsetbits32(regmap + AT91_PMC_PLL_CTRL0,
			   AT91_PMC_PLL_CTRL0_ENLOCK | AT91_PMC_PLL_CTRL0_ENPLL,
			   AT91_PMC_PLL_CTRL0_ENLOCK | AT91_PMC_PLL_CTRL0_ENPLL);

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
			   AT91_PMC_PLL_UPDT_UPDATE | core->id);

	while (!sam9x60_pll_ready(regmap, core->id))
		;
	return 0;
}

static TEE_Result sam9x60_frac_pll_prepare(struct clk *hw)
{
	struct sam9x60_frac *frac = hw->priv;
	return sam9x60_frac_pll_set(frac);
}

static void sam9x60_frac_pll_unprepare(struct clk *hw)
{
	struct sam9x60_frac *frac = hw->priv;
	io_clrsetbits32(frac->core.base + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_ID_MSK, frac->core.id);

	io_clrsetbits32(frac->core.base + AT91_PMC_PLL_CTRL0, AT91_PMC_PLL_CTRL0_ENPLL, 0);

	if (frac->core.charac->upll)
		io_clrsetbits32(frac->core.base + AT91_PMC_PLL_ACR,
				   AT91_PMC_PLL_ACR_UTMIBG | AT91_PMC_PLL_ACR_UTMIVR, 0);

	io_clrsetbits32(frac->core.base + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
			   AT91_PMC_PLL_UPDT_UPDATE | frac->core.id);
}

static TEE_Result sam9x60_frac_pll_compute_mul_frac(struct sam9x60_frac *frac,
					      unsigned long rate,
					      unsigned long parent_rate,
					      bool update)
{
	unsigned long tmprate, remainder;
	unsigned long nmul = 0;
	unsigned long nfrac = 0;

	if (rate < frac->core.charac->output[0].min ||
	    rate > frac->core.charac->output[0].max)
		return TEE_ERROR_GENERIC;

	/*
	 * Calculate the multiplier associated with the current
	 * divider that provide the closest rate to the requested one.
	 */
	nmul = mult_frac(rate, 1, parent_rate);
	tmprate = mult_frac(parent_rate, nmul, 1);
	remainder = rate - tmprate;

	if (remainder) {
		nfrac = UDIV_ROUND_NEAREST((uint64_t)remainder * (1 << 22),
					      parent_rate);

		tmprate += UDIV_ROUND_NEAREST((uint64_t)nfrac * parent_rate,
						 (1 << 22));
	}

	/* Check if resulted rate is a valid.  */
	if (tmprate < frac->core.charac->output[0].min ||
	    tmprate > frac->core.charac->output[0].max)
		return TEE_ERROR_GENERIC;

	if (update) {
		frac->mul = nmul - 1;
		frac->frac = nfrac;
	}

	return TEE_SUCCESS;
}

static TEE_Result sam9x60_frac_pll_set_rate_chg(struct clk *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	TEE_Result ret;
	struct sam9x60_frac *frac = hw->priv;
	struct sam9x60_pll_core *core = &frac->core;
	vaddr_t regmap = core->base;

	ret = sam9x60_frac_pll_compute_mul_frac(frac, rate, parent_rate, true);
	if (ret == TEE_SUCCESS) {
		io_write32(regmap + AT91_PMC_PLL_CTRL1,
			     (frac->mul << core->layout->mul_shift) |
			     (frac->frac << core->layout->frac_shift));

		io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
				   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
				   AT91_PMC_PLL_UPDT_UPDATE | core->id);

		io_clrsetbits32(regmap + AT91_PMC_PLL_CTRL0,
				   AT91_PMC_PLL_CTRL0_ENLOCK | AT91_PMC_PLL_CTRL0_ENPLL,
				   AT91_PMC_PLL_CTRL0_ENLOCK |
				   AT91_PMC_PLL_CTRL0_ENPLL);

		io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
				   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
				   AT91_PMC_PLL_UPDT_UPDATE | core->id);

		while (!sam9x60_pll_ready(regmap, core->id))
			;
	}

	return ret;
}

static const struct clk_ops sam9x60_frac_pll_ops_chg = {
	.enable = sam9x60_frac_pll_prepare,
	.disable = sam9x60_frac_pll_unprepare,
	.get_rate = sam9x60_frac_pll_recalc_rate,
	.set_rate = sam9x60_frac_pll_set_rate_chg,
};

static void sam9x60_div_pll_set_div(struct sam9x60_pll_core *core, uint32_t div,
				    bool enable)
{
	vaddr_t regmap = core->base;
	uint32_t ena_msk = enable ? core->layout->endiv_mask : 0;
	uint32_t ena_val = enable ? (1 << core->layout->endiv_shift) : 0;

	io_clrsetbits32(regmap + AT91_PMC_PLL_CTRL0,
			   core->layout->div_mask | ena_msk,
			   (div << core->layout->div_shift) | ena_val);

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
			   AT91_PMC_PLL_UPDT_UPDATE | core->id);

	while (!sam9x60_pll_ready(regmap, core->id))
		;
}

static TEE_Result sam9x60_div_pll_set(struct sam9x60_div *div)
{
	struct sam9x60_pll_core *core = &div->core;
	vaddr_t regmap = core->base;
	unsigned int val, cdiv;
	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_ID_MSK, core->id);
	val = io_read32(regmap + AT91_PMC_PLL_CTRL0);
	cdiv = (val & core->layout->div_mask) >> core->layout->div_shift;

	/* Stop if enabled an nothing changed. */
	if (!!(val & core->layout->endiv_mask) && cdiv == div->div)
		return 0;

	sam9x60_div_pll_set_div(core, div->div, 1);
	return 0;
}

static TEE_Result sam9x60_div_pll_prepare(struct clk *hw)
{
	struct sam9x60_div *div = hw->priv;

	return sam9x60_div_pll_set(div);
}

static void sam9x60_div_pll_unprepare(struct clk *hw)
{
	struct sam9x60_div *div = hw->priv;
	struct sam9x60_pll_core *core = &div->core;
	vaddr_t regmap = core->base;
	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_ID_MSK, core->id);

	io_clrsetbits32(regmap + AT91_PMC_PLL_CTRL0,
			   core->layout->endiv_mask, 0);

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_UPDATE | AT91_PMC_PLL_UPDT_ID_MSK,
			   AT91_PMC_PLL_UPDT_UPDATE | core->id);
}

static unsigned long sam9x60_div_pll_recalc_rate(struct clk *hw,
						 unsigned long parent_rate)
{
	struct sam9x60_div *div = hw->priv;
	return UDIV_ROUND_NEAREST(parent_rate, (div->div + 1));
}

static TEE_Result sam9x60_div_pll_set_rate(struct clk *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct sam9x60_div *div = hw->priv;

	div->div = UDIV_ROUND_NEAREST(parent_rate, rate) - 1;

	return TEE_SUCCESS;
}

static TEE_Result sam9x60_div_pll_set_rate_chg(struct clk *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct sam9x60_div *div = hw->priv;
	struct sam9x60_pll_core *core = &div->core;
	vaddr_t regmap = core->base;
	unsigned int val, cdiv;
	div->div = UDIV_ROUND_NEAREST(parent_rate, rate) - 1;

	io_clrsetbits32(regmap + AT91_PMC_PLL_UPDT, AT91_PMC_PLL_UPDT_ID_MSK,
			   core->id);
	val = io_read32(regmap + AT91_PMC_PLL_CTRL0);
	cdiv = (val & core->layout->div_mask) >> core->layout->div_shift;

	/* Stop if nothing changed. */
	if (cdiv == div->div)
		return TEE_SUCCESS;

	sam9x60_div_pll_set_div(core, div->div, 0);
	return TEE_SUCCESS;
}

static const struct clk_ops sam9x60_div_pll_ops = {
	.enable = sam9x60_div_pll_prepare,
	.disable = sam9x60_div_pll_unprepare,
	.set_rate = sam9x60_div_pll_set_rate,
	.get_rate = sam9x60_div_pll_recalc_rate,
};

static const struct clk_ops sam9x60_div_pll_ops_chg = {
	.enable = sam9x60_div_pll_prepare,
	.disable = sam9x60_div_pll_unprepare,
	.set_rate = sam9x60_div_pll_set_rate_chg,
	.get_rate = sam9x60_div_pll_recalc_rate,
};

struct clk *
sam9x60_clk_register_frac_pll(struct pmc_data *pmc,
			      const char *name, struct clk *parent, uint8_t id,
			      const struct clk_pll_charac *characteristics,
			      const struct clk_pll_layout *layout, uint32_t flags)
{
	struct sam9x60_frac *frac;
	struct clk *hw;
	unsigned long parent_rate;
	unsigned int val;
	TEE_Result ret;

//	if (!name || !parent || id > PLL_ID_MAX)
//		return NULL;

	frac = calloc(1, sizeof(*frac));
	if (!frac)
		return NULL;

	hw = clk_alloc(name, &sam9x60_frac_pll_ops_chg, &parent, 1);
	if (!hw) {
		free(frac);
		return NULL;
	}

	hw->priv = frac;
	hw->flags = flags;
	frac->core.id = id;
	frac->core.charac = characteristics;
	frac->core.layout = layout;
	frac->core.base = pmc->base;

	if (sam9x60_pll_ready(pmc->base, id)) {
		io_clrsetbits32(frac->core.base + AT91_PMC_PLL_UPDT,
				   AT91_PMC_PLL_UPDT_ID_MSK, id);
		val = io_read32(pmc->base + AT91_PMC_PLL_CTRL1);
		frac->mul = (val >> PMC_PLL_CTRL1_MUL_POS) & PMC_PLL_CTRL1_MUL_MSK;
		frac->frac = (val >> PMC_PLL_CTRL1_FRACR_POS) & PMC_PLL_CTRL1_FRACR_MSK;
	} else {
		/*
		 * This means the PLL is not setup by bootloaders. In this
		 * case we need to set the minimum rate for it. Otherwise
		 * a clock child of this PLL may be enabled before setting
		 * its rate leading to enabling this PLL with unsupported
		 * rate. This will lead to PLL not being locked at all.
		 */
		parent_rate = clk_get_rate(parent);
		if (!parent_rate) {
			clk_free(hw);
			free(frac);
			return NULL;
		}

		ret = sam9x60_frac_pll_compute_mul_frac(frac,
							characteristics->output[0].min,
							parent_rate, true);
		if (ret != TEE_SUCCESS) {
			clk_free(hw);
			free(frac);
			return NULL;
		}
	}

	frac->core.hw = hw;
	if (clk_register(hw)) {
		clk_free(hw);
		free(frac);
		return NULL;
	}
	return hw;
}

struct clk *
sam9x60_clk_register_div_pll(struct pmc_data *pmc,
			     const char *name, struct clk *parent, uint8_t id,
			     const struct clk_pll_charac *characteristics,
			     const struct clk_pll_layout *layout, uint32_t flags,
			     uint32_t safe_div)
{
	struct sam9x60_div *div;
	struct clk *hw;
	unsigned int val;

	/* We only support one changeable PLL. */
//	if (id > PLL_ID_MAX || (safe_div && notifier_div))
//		return NULL;

	if (safe_div >= PLL_DIV_MAX)
		safe_div = PLL_DIV_MAX - 1;

	div = calloc(1, sizeof(*div));
	if (!div)
		return NULL;

	if (flags & CLK_SET_RATE_GATE)
		hw = clk_alloc(name, &sam9x60_div_pll_ops, &parent, 1);
	else
		hw = clk_alloc(name, &sam9x60_div_pll_ops_chg, &parent, 1);
	if (!hw) {
		free(div);
		return NULL;
	}

	hw->priv = div;
	hw->flags = flags;
	div->core.id = id;
	div->core.charac = characteristics;
	div->core.layout = layout;
	div->core.base = pmc->base;
	div->safe_div = safe_div;

	io_clrsetbits32(pmc->base + AT91_PMC_PLL_UPDT,
			   AT91_PMC_PLL_UPDT_ID_MSK, id);
	val = io_read32(pmc->base + AT91_PMC_PLL_CTRL0);
	div->div = (val >> PMC_PLL_CTRL0_DIV_POS) & PMC_PLL_CTRL0_DIV_MSK;

	div->core.hw = hw;
	if (clk_register(hw)) {
		clk_free(hw);
		free(div);
		return NULL;
	}

	return hw;
}

