/*
 * Copyright (c) 2015 Cray Inc. All rights reserved.
 * Copyright (c) 2015 Los Alamos National Security, LLC. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <time.h>
#include <string.h>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_endpoint.h>

#include "gnix_ep.h"

#include <criterion/criterion.h>

static struct fi_info *hints;
static struct fi_info *fi;
static struct fid_fabric *fab;
static struct fid_domain *dom;

static void setup(void)
{
	int ret;

	hints = fi_allocinfo();
	cr_assert(hints, "fi_allocinfo");

	hints->fabric_attr->name = strdup("gni");

	ret = fi_getinfo(FI_VERSION(1, 0), NULL, 0, 0, hints, &fi);
	cr_assert(!ret, "fi_getinfo");

	ret = fi_fabric(fi->fabric_attr, &fab, NULL);
	cr_assert(!ret, "fi_fabric");

	ret = fi_domain(fab, fi, &dom, NULL);
	cr_assert(!ret, "fi_domain");

}

static void teardown(void)
{
	int ret;

	ret = fi_close(&dom->fid);
	cr_assert(!ret, "fi_close domain");

	ret = fi_close(&fab->fid);
	cr_assert(!ret, "fi_close fabric");

	fi_freeinfo(fi);
	fi_freeinfo(hints);
}

TestSuite(endpoint, .init = setup, .fini = teardown);

Test(endpoint, open_close)
{
	int i, ret;
	const int num_eps = 61;
	struct fid_ep *eps[num_eps];

	memset(eps, 0, num_eps*sizeof(struct fid_ep *));

	for (i = 0; i < num_eps; i++) {
		ret = fi_endpoint(dom, fi, &eps[i], NULL);
		cr_assert(!ret, "fi_endpoint");
		struct gnix_fid_ep *ep = container_of(eps[i],
						      struct gnix_fid_ep,
						      ep_fid);
		cr_assert(ep, "endpoint not allcoated");

		/* Check fields (fill in as implemented) */
		cr_assert(ep->nic, "NIC not allocated");
		cr_assert(!_gnix_sfl_empty(&ep->fr_freelist),
			  "gnix_fab_req freelist empty");
	}

	for (i = num_eps-1; i >= 0; i--) {
		ret = fi_close(&eps[i]->fid);
		cr_assert(!ret, "fi_close endpoint");
	}

}

Test(endpoint, getsetopt)
{
	int ret;
	struct fid_ep *ep = NULL;
	uint64_t val;
	size_t len;

	ret = fi_endpoint(dom, fi, &ep, NULL);
	cr_assert(!ret, "fi_endpoint");

	/* Test bad params. */
	ret = fi_getopt(&ep->fid, !FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, &len);
	cr_assert(ret == -FI_ENOPROTOOPT, "fi_getopt");

	ret = fi_getopt(&ep->fid, FI_OPT_ENDPOINT, !FI_OPT_MIN_MULTI_RECV,
			(void *)&val, &len);
	cr_assert(ret == -FI_ENOPROTOOPT, "fi_getopt");

	ret = fi_getopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			NULL, &len);
	cr_assert(ret == -FI_EINVAL, "fi_getopt");

	ret = fi_getopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, NULL);
	cr_assert(ret == -FI_EINVAL, "fi_getopt");

	ret = fi_setopt(&ep->fid, !FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, sizeof(size_t));
	cr_assert(ret == -FI_ENOPROTOOPT, "fi_setopt");

	ret = fi_setopt(&ep->fid, FI_OPT_ENDPOINT, !FI_OPT_MIN_MULTI_RECV,
			(void *)&val, sizeof(size_t));
	cr_assert(ret == -FI_ENOPROTOOPT, "fi_setopt");

	ret = fi_setopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			NULL, sizeof(size_t));
	cr_assert(ret == -FI_EINVAL, "fi_setopt");

	ret = fi_setopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, sizeof(size_t) - 1);
	cr_assert(ret == -FI_EINVAL, "fi_setopt");

	/* Test update. */
	ret = fi_getopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, &len);
	fprintf(stderr, "ret is %d\n", ret);
	cr_assert(!ret, "fi_getopt");
	cr_assert(val == GNIX_OPT_MIN_MULTI_RECV_DEFAULT, "fi_getopt");
	cr_assert(len == sizeof(size_t), "fi_getopt");

	val = 128;
	ret = fi_setopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, sizeof(size_t));
	cr_assert(!ret, "fi_setopt");

	ret = fi_getopt(&ep->fid, FI_OPT_ENDPOINT, FI_OPT_MIN_MULTI_RECV,
			(void *)&val, &len);
	cr_assert(!ret, "fi_getopt");
	cr_assert(val == 128, "fi_getopt");
	cr_assert(len == sizeof(size_t), "fi_getopt");

	ret = fi_close(&ep->fid);
	cr_assert(!ret, "fi_close endpoint");
}

Test(endpoint, sizeleft)
{
	int ret;
	size_t sz;
	struct fid_ep *ep = NULL;

	ret = fi_endpoint(dom, fi, &ep, NULL);
	cr_assert(!ret, "fi_endpoint");

	/* Test default values. */
	sz = fi_rx_size_left(ep);
	cr_assert(sz == 64, "fi_rx_size_left");

	sz = fi_tx_size_left(ep);
	cr_assert(sz == 64, "fi_tx_size_left");

	ret = fi_close(&ep->fid);
	cr_assert(!ret, "fi_close endpoint");
}
