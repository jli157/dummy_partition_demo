/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <psa/crypto.h>
#include <stdbool.h>
#include <stdint.h>
#include "tfm_secure_api.h"
#include "tfm_api.h"
#include "tfm_sp_log.h"
#include "dummy_partition_defs.h"
#include "tfm_plat_test.h"

#define NUM_SECRETS 5

struct dp_secret {
	uint8_t secret[16];
};

struct dp_secret secrets[NUM_SECRETS] = {
	{ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
	{ { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
	{ { 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
	{ { 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
	{ { 4, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
};

typedef void (*psa_write_callback_t)(void *handle, uint8_t *digest,
				     uint32_t digest_size);

static psa_status_t tfm_dp_secret_digest(uint32_t secret_index,
					 size_t digest_size, size_t *p_digest_size,
					 psa_write_callback_t callback, void *handle)
{
	uint8_t digest[32];
	psa_status_t status;

	/* Check that secret_index is valid. */
	if (secret_index >= NUM_SECRETS) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	/* Check that digest_size is valid. */
	if (digest_size != sizeof(digest)) {
		return PSA_ERROR_INVALID_ARGUMENT;
	}

	status = psa_hash_compute(PSA_ALG_SHA_256, secrets[secret_index].secret,
				  sizeof(secrets[secret_index].secret), digest,
				  digest_size, p_digest_size);

	if (status != PSA_SUCCESS) {
		return status;
	}
	if (*p_digest_size != digest_size) {
		return PSA_ERROR_PROGRAMMER_ERROR;
	}

	callback(handle, digest, digest_size);

	return PSA_SUCCESS;
}

#include "psa/service.h"
#include "psa_manifest/tfm_dummy_partition.h"

typedef psa_status_t (*dp_func_t)(psa_msg_t *);

static void psa_write_digest(void *handle, uint8_t *digest,
			     uint32_t digest_size)
{
	psa_write((psa_handle_t)handle, 0, digest, digest_size);
}

static psa_status_t tfm_dp_secret_digest_ipc(psa_msg_t *msg)
{
	size_t num = 0;
	uint32_t secret_index;

	LOG_INFFMT("Generate secret_digest...\r\n");
	if (msg->in_size[0] != sizeof(secret_index)) {
		/* The size of the argument is incorrect */
		return PSA_ERROR_PROGRAMMER_ERROR;
	}

	num = psa_read(msg->handle, 0, &secret_index, msg->in_size[0]);
	if (num != msg->in_size[0]) {
		return PSA_ERROR_PROGRAMMER_ERROR;
	}

	return tfm_dp_secret_digest(secret_index, msg->out_size[0],
				    &msg->out_size[0], psa_write_digest,
				    (void *)msg->handle);
}

static psa_status_t tfm_dp_timer_cb_ipc(void)
{

	LOG_INFFMT("Timer0 timeout...\r\n");
	tfm_plat_test_secure_timer_stop();
	psa_irq_disable(TFM_TIMER0_IRQ_SIGNAL);
	psa_eoi(TFM_TIMER0_IRQ_SIGNAL);

#if 0
	psa_signal_t signals = 0;
	signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);
	if (signals & TFM_DP_FETCH_RESP_SIGNAL) {
	}
#endif
	return PSA_SUCCESS;
}

static psa_status_t tfm_dp_timer_test_ipc(psa_msg_t *msg)
{
	LOG_INFFMT("Start timer ...\r\n");
	psa_irq_enable(TFM_TIMER0_IRQ_SIGNAL);
	tfm_plat_test_secure_timer_start();

	return PSA_SUCCESS;
}


static void dp_signal_handle(psa_signal_t signal)
{
	psa_status_t status;
	psa_msg_t msg;

	status = psa_get(signal, &msg);
	switch (msg.type) {
	case TFM_DP_SECRET_DIGEST:
		status = tfm_dp_secret_digest_ipc(&msg);
		psa_reply(msg.handle, status);
		break;
	case TFM_DP_TIMER_TEST:
		status = tfm_dp_timer_test_ipc(&msg);
		psa_reply(msg.handle, status);
		break;
	default:
		psa_panic();
	}
}

psa_status_t tfm_dp_req_mngr_init(void)
{
	psa_signal_t signals = 0;

	LOG_INFFMT("tfm_dp_req_mngr_init...\r\n");
	while (1) {
		signals = psa_wait(PSA_WAIT_ANY, PSA_BLOCK);
		if (signals & TFM_DP_SERVICE_SIGNAL) {
			dp_signal_handle(TFM_DP_SERVICE_SIGNAL);

		} else if (signals & TFM_TIMER0_IRQ_SIGNAL) {
			tfm_dp_timer_cb_ipc();
		} else {
			psa_panic();
		}
	}

	return PSA_ERROR_SERVICE_FAILURE;
}
