/*
 * Copyright (C) 2016 Red Hat, Inc.  All rights reserved.
 *
 * Authors: Fabio M. Di Nitto <fabbione@kronosnet.org>
 *
 * This software licensed under GPL-2.0+, LGPL-2.0+
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libknet.h"

#include "internals.h"
#include "test-common.h"

static int private_data;

static int dhost_filter(void *pvt_data,
			const unsigned char *outdata,
			ssize_t outdata_len,
			uint8_t tx_rx,
			uint8_t this_host_id,
			uint8_t src_host_id,
			int8_t *dst_channel,
			uint8_t *dst_host_ids,
			size_t *dst_host_ids_entries)
{
	return 0;
}

static void test(void)
{
	knet_handle_t knet_h;
	int logfds[2];

	printf("Test knet_handle_enable_filter incorrect knet_h\n");

	if ((!knet_handle_enable_filter(NULL, NULL, dhost_filter)) || (errno != EINVAL)) {
		printf("knet_handle_enable_filter accepted invalid knet_h or returned incorrect error: %s\n", strerror(errno));
		exit(FAIL);
	}

	setup_logpipes(logfds);

	knet_h = knet_handle_new(1, logfds[1], KNET_LOG_DEBUG);

	if (!knet_h) {
		printf("knet_handle_new failed: %s\n", strerror(errno));
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);
	}

	printf("Test knet_handle_enable_filter with no private_data\n");

	if (knet_handle_enable_filter(knet_h, NULL, dhost_filter) < 0) {
		printf("knet_handle_enable_filter failed: %s\n", strerror(errno));
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);
	}

	if (knet_h->dst_host_filter_fn_private_data != NULL) {
		printf("knet_handle_enable_filter failed to unset private_data");
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);

	}

	flush_logs(logfds[0], stdout);

	printf("Test knet_handle_enable_filter with private_data\n");

	if (knet_handle_enable_filter(knet_h, &private_data, NULL) < 0) {
		printf("knet_handle_enable_filter failed: %s\n", strerror(errno));
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);
	}

	if (knet_h->dst_host_filter_fn_private_data != &private_data) {
		printf("knet_handle_enable_filter failed to set private_data");
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);

	}

	flush_logs(logfds[0], stdout);

	printf("Test knet_handle_enable_filter with no dhost_filter fn\n");

	if (knet_handle_enable_filter(knet_h, NULL, NULL) < 0) {
		printf("knet_handle_enable_filter failed: %s\n", strerror(errno));
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);
	}

	if (knet_h->dst_host_filter_fn != NULL) {
		printf("knet_handle_enable_filter failed to unset dhost_filter fn");
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);

	}

	flush_logs(logfds[0], stdout);

	printf("Test knet_handle_enable_filter with dhost_filter fn\n");

	if (knet_handle_enable_filter(knet_h, NULL, dhost_filter) < 0) {
		printf("knet_handle_enable_filter failed: %s\n", strerror(errno));
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);
	}

	if (knet_h->dst_host_filter_fn != &dhost_filter) {
		printf("knet_handle_enable_filter failed to set dhost_filter fn");
		knet_handle_free(knet_h);
		flush_logs(logfds[0], stdout);
		close_logpipes(logfds);
		exit(FAIL);

	}

	flush_logs(logfds[0], stdout);


	knet_host_remove(knet_h, 1);
	knet_handle_free(knet_h);
	flush_logs(logfds[0], stdout);
	close_logpipes(logfds);
}

int main(int argc, char *argv[])
{
	need_root();

	test();

	return PASS;
}
