// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright 2022 Microchip
 */

#include <drivers/wdt.h>
#include <sm/optee_smc.h>
#include <sm/psci.h>

enum smcwd_call {
	SMCWD_INIT		= 0,
	SMCWD_SET_TIMEOUT	= 1,
	SMCWD_ENABLE		= 2,
	SMCWD_PET		= 3,
	SMCWD_GET_TIMELEFT	= 4,
};

enum sm_handler_ret wdt_sm_handler(struct thread_smc_args *args)
{
	if (OPTEE_SMC_FUNC_NUM(args->a0) !=
	    OPTEE_SMC_FUNC_NUM(CFG_WDT_SM_HANDLER_ID))
		return SM_HANDLER_PENDING_SMC;

	switch (args->a1) {
	case SMCWD_INIT:
		unsigned long min_timeout = 0;
		unsigned long max_timeout = 0;

		if (watchdog_init(&min_timeout, &max_timeout)) {
			args->a0 = PSCI_RET_INTERNAL_FAILURE;
		} else {
			args->a0 = PSCI_RET_SUCCESS;
			args->a1 = min_timeout;
			args->a2 = max_timeout;
		}
		break;
	case SMCWD_SET_TIMEOUT:
		watchdog_settimeout(args->a2);
		args->a0 = PSCI_RET_SUCCESS;
		break;
	case SMCWD_ENABLE:
		if (args->a2 == 0) {
			watchdog_stop();
			args->a0 = PSCI_RET_SUCCESS;
		} else if (args->a2 == 1) {
			watchdog_start();
			args->a0 = PSCI_RET_SUCCESS;
		} else {
			args->a0 = PSCI_RET_INVALID_PARAMETERS;
		}
		break;
	case SMCWD_PET:
		watchdog_ping();
		args->a0 = PSCI_RET_SUCCESS;
		break;
	/* SMCWD_GET_TIMELEFT is optional */
	case SMCWD_GET_TIMELEFT:
	default:
		args->a0 = PSCI_RET_NOT_SUPPORTED;
	}

	return SM_HANDLER_SMC_HANDLED;
}
