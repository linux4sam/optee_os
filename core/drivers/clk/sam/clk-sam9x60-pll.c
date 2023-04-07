// SPDX-License-Identifier: GPL-2.0+
/*
 *  Copyright (C) 2019 Microchip Technology Inc.
 *
 */

#define ERANGE 1

#define mult_frac(x, numer, denom)( 		\
{							\
	typeof(x) quot = (x) / (denom); 		\
	typeof(x) rem  = (x) % (denom); 		\
	(quot * (numer)) + ((rem * (numer)) / (denom)); \
}							\
)

#define PMC_PLL_CTRL0_DIV_MSK 0xf
#define PMC_PLL_CTRL0_DIV_POS 0
#define	PMC_PLL_CTRL1_MUL_MSK 0xf
#define	PMC_PLL_CTRL1_MUL_POS 24
#define	PMC_PLL_CTRL1_FRACR_MSK	0x3fffff
#define	PMC_PLL_CTRL1_FRACR_POS	0

#include <io.h>
#include <kernel/delay.h>
#include <kernel/panic.h>
#include <util.h>
#include <mm/core_memprot.h>
#include <types_ext.h>

#include "at91_clk.h"

#define u8 uint8_t
#define u32 uint32_t

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
	u8 div;
	u8 safe_div;
};

static struct sam9x60_div *notifier_div;

static inline bool sam9x60_pll_ready(vaddr_t base, int id)
{
	unsigned int status = io_read32(base + AT91_PMC_PLL_ISR0);

	return !!(status & BIT(id));
}

static int sam9x60_frac_pll_is_prepared(struct clk *hw)
{
	struct sam9x60_frac *frac = hw->priv;
	struct sam9x60_pll_core *core = &frac->core;

	return sam9x60_pll_ready(core->base, core->id);
}

static long sam9x60_frac_pll_compute_mul_frac(struct sam9x60_frac *frac,
					      unsigned long rate,
					      unsigned long parent_rate,
					      bool update)
{
	unsigned long tmprate, remainder;
	unsigned long nmul = 0;
	unsigned long nfrac = 0;

	if (rate < frac->core.charac->core_output[0].min ||
	    rate > frac->core.charac->core_output[0].max)
		return -ERANGE;

	/*
	 * Calculate the multiplier associated with the current
	 * divider that provide the closest rate to the requested one.
	 */
	nmul = mult_frac(rate, 1, parent_rate);
	tmprate = mult_frac(parent_rate, nmul, 1);
	remainder = rate - tmprate;

	if (remainder) {
		nfrac = DIV_ROUND_NEAREST((uint64_t)remainder * (1 << 22),
					      parent_rate);

		tmprate += DIV_ROUND_NEAREST((uint64_t)nfrac * parent_rate,
						 (1 << 22));
	}

	/* Check if resulted rate is a valid.  */
	if (tmprate < frac->core.charac->core_output[0].min ||
	    tmprate > frac->core.charac->core_output[0].max)
		return -ERANGE;

	if (update) {
		frac->mul = nmul - 1;
		frac->frac = nfrac;
	}

	return tmprate;
}

static long sam9x60_frac_pll_round_rate(struct clk *hw, unsigned long rate,
					unsigned long *parent_rate)
{
	struct sam9x60_frac *frac = hw->priv;

	return sam9x60_frac_pll_compute_mul_frac(frac, rate, *parent_rate, false);
}

static int sam9x60_frac_pll_set_rate(struct clk *hw, unsigned long rate,
				     unsigned long parent_rate)
{
	struct sam9x60_frac *frac = hw->priv;

	return sam9x60_frac_pll_compute_mul_frac(frac, rate, parent_rate, true);
}

static int sam9x60_frac_pll_set_rate_chg(struct clk *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	int ret;
	// TODO
	return ret;
}

static const struct clk_ops sam9x60_frac_pll_ops = {
	.set_rate = sam9x60_frac_pll_set_rate,
};

static const struct clk_ops sam9x60_frac_pll_ops_chg = {
	.set_rate = sam9x60_frac_pll_set_rate_chg,
};


static const struct clk_ops sam9x60_div_pll_ops = {
};

static const struct clk_ops sam9x60_div_pll_ops_chg = {
};

static const struct clk_ops sam9x60_fixed_div_pll_ops = {
};

static TEE_Result sama7g5_pll_enable(struct clk *clk)
{

}

static void sama7g5_pll_disable(struct clk *clk)
{
}

static unsigned long sama7g5_pll_get_rate(struct clk *clk,
				      unsigned long parent_rate)
{
}

static long sama7g5_pll_get_best_div_mul(struct clk_pll *pll, unsigned long rate,
				     unsigned long parent_rate,
				     uint32_t *div, uint32_t *mul,
				     uint32_t *index)
{
}

static TEE_Result sama7g5_pll_set_rate(struct clk *clk, unsigned long rate,
				   unsigned long parent_rate)
{
#if 0
	struct clk_pll *pll = clk->priv;
	long ret = -1;
	uint32_t div = 1;
	uint32_t mul = 0;
	uint32_t index = 0;

	ret = clk_pll_get_best_div_mul(pll, rate, parent_rate,
				       &div, &mul, &index);
	if (ret < 0)
		return TEE_ERROR_BAD_PARAMETERS;

	pll->range = index;
	pll->div = div;
	pll->mul = mul;

	return TEE_SUCCESS;
#endif
}

static const struct clk_ops sama7g5_pll_ops = {
	.enable = sama7g5_pll_enable,
	.disable = sama7g5_pll_disable,
	.get_rate = sama7g5_pll_get_rate,
	.set_rate = sama7g5_pll_set_rate,
};

struct clk *
sam9x60_clk_register_frac_pll(struct pmc_data *pmc,
			      const char *name, struct clk *parent, uint8_t id,
			      const struct clk_pll_charac *characteristics,
			      const struct clk_pll_layout *layout, u32 flags)
{
	struct sam9x60_frac *frac;
	struct clk *hw;
	unsigned long parent_rate, irqflags;
	unsigned int val;
	int ret;

//	if (!name || !parent || id > PLL_ID_MAX)
//		return NULL;

	frac = calloc(1, sizeof(*frac));
	if (!frac)
		return NULL;

	if (flags & CLK_SET_RATE_GATE)
		hw = clk_alloc(name, &sam9x60_frac_pll_ops, &parent, 1);
	else
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
							characteristics->core_output[0].min,
							parent_rate, true);
		if (ret <= 0) {
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
			     const char *name, struct clk *parent, u8 id,
			     const struct clk_pll_charac *characteristics,
			     const struct clk_pll_layout *layout, u32 flags,
			     u32 safe_div)
{
	struct sam9x60_div *div;
	struct clk *hw;
	unsigned long irqflags;
	unsigned int val;
	int ret;

	/* We only support one changeable PLL. */
//	if (id > PLL_ID_MAX || (safe_div && notifier_div))
//		return NULL;

	if (safe_div >= PLL_DIV_MAX)
		safe_div = PLL_DIV_MAX - 1;

	div = calloc(1, sizeof(*div));
	if (!div)
		return NULL;

	if (layout->div2) {
		hw = clk_alloc(name, &sam9x60_fixed_div_pll_ops, &parent, 1);
	} else {
		if (flags & CLK_SET_RATE_GATE)
			hw = clk_alloc(name, &sam9x60_div_pll_ops, &parent, 1);
		else
			hw = clk_alloc(name, &sam9x60_div_pll_ops_chg, &parent, 1);
	}
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
	} else if (div->safe_div) {
		notifier_div = div;
	}
	return hw;
}

