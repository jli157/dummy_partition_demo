/*
 * Copyright (c) 2021 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dummy_partition.h"
#include "psa/client.h"
#include "psa_manifest/sid.h"
#include "dummy_partition_defs.h"

psa_status_t dp_secret_digest(uint32_t secret_index,
			      void *p_digest,
			      size_t digest_size)
{
	psa_status_t status;

	psa_invec in_vec[] = {
		{ .base = &secret_index, .len = sizeof(secret_index) },
	};

	psa_outvec out_vec[] = {
		{ .base = p_digest, .len = digest_size }
	};

	status = psa_call(TFM_DP_SERVICE_HANDLE, TFM_DP_SECRET_DIGEST,
			  in_vec, IOVEC_LEN(in_vec),
			  out_vec, IOVEC_LEN(out_vec));

	return status;
}
