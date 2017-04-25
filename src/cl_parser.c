/*
 * Copyright 2016-2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * cl_parser.c -- Command-line parser for strace.ebpf
 */

#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>

#include "txt.h"
#include "strace.ebpf.h"
#include "utils.h"
#include "cl_parser.h"
#include "ebpf_syscalls.h"
#include "print_event_cb.h"

static inline int
check_optarg(const char * const optarg)
{
	if (optarg == NULL) {
		ERROR("missing mandatory option's argument");
		fprint_help(stderr);
		exit(EXIT_FAILURE);
	}
}

/*
 * cl_parser -- Tool's command-line options parser
 */
int
cl_parser(struct cl_options *const clo,
		const int argc, char *const argv[])
{
	while (1) {
		int c;
		int option_index = 0;

		static struct option long_options[] = {
			{"timestamp",		no_argument,	   0, 't'},
			{"failed",		no_argument,	   0, 'X'},
			{"help",		no_argument,	   0, 'h'},
			{"debug",		no_argument,	   0, 'd'},
			{"list",		no_argument,	   0, 'L'},
			{"ll-list",		no_argument,	   0, 'R'},
			{"builtin-list",	no_argument,	   0, 'B'},
			{"no-progress",		no_argument,	   0, 'r'},
			{"pid",			required_argument, 0, 'p'},
			{"format",		required_argument, 0, 'l'},
			{"string-args",		required_argument, 0, 's'},
			{"expr",		required_argument, 0, 'e'},
			{"output",		required_argument, 0, 'o'},
			{"ebpf-src-dir", 	required_argument, 0, 'N'},
			{"hex-separator", 	required_argument, 0, 'K'},
			{"full-follow-fork",	optional_argument, 0, 'f'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "+tXhdLRBrp:l:s:e:o:N:K:f::",
				long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			int res;

		case 'r':
			clo->do_not_print_progress = 1;
			break;

		case 't':
			clo->timestamp = true;
			break;

		case 'X':
			clo->failed = true;
			break;

		case 'h':
			fprint_help(stdout);
			exit(EXIT_SUCCESS);

		case 'd':
			clo->debug = true;
			break;

		case 'p':
			check_optarg(optarg);
			clo->pid = atoi(optarg);
			if (clo->pid < 1) {
				ERROR("wrong value for pid option is"
					" provided: '%s' => '%d'",
					optarg, clo->pid);
				exit(EXIT_FAILURE);
			}
			break;

		case 'o':
			check_optarg(optarg);
			clo->out_fn = optarg;
			break;

		case 'K':
			check_optarg(optarg);
			clo->out_lf_fld_sep_ch = *optarg;
			break;

		case 'N':
			check_optarg(optarg);
			clo->ebpf_src_dir = optarg;
			break;

		case 'e':
			check_optarg(optarg);
			if (!strcasecmp(optarg, "list") ||
			    !strcasecmp(optarg, "help")) {
				INFO("List of supported expressions: "
					"'help', 'list', 'trace=set'");
				INFO("For list of supported sets you should "
					"use 'trace=help' or 'trace=list'");
				exit(EXIT_SUCCESS);
			} else if (!strcasecmp(optarg, "trace=help") ||
					!strcasecmp(optarg, "trace=list")) {
				fprint_trace_list(stderr);
				INFO("You can combine sets by using comma.");
				exit(EXIT_SUCCESS);
			}
			clo->expr = optarg;
			break;

		case 'l':
			check_optarg(optarg);
			if (!strcasecmp(optarg, "list") ||
			    !strcasecmp(optarg, "help")) {
				INFO("List of supported formats: "
					"'bin', 'binary', 'hex', 'hex_raw', "
					"'hex_sl', 'strace', "
					"'list' & 'help'");
				exit(EXIT_SUCCESS);
			}
			clo->out_fmt_str = optarg;
			Out_lf_fmt = out_fmt_str2enum(clo->out_fmt_str);
			break;

		case 's':
			check_optarg(optarg);
			clo->fnr_mode = choose_fnr_mode(optarg);
			break;

		case 'L':
			get_sc_list(stdout, is_a_sc);
			exit(EXIT_SUCCESS);

		case 'R':
			get_sc_list(stdout, NULL);
			exit(EXIT_SUCCESS);

		case 'B':
			res = print_syscalls_table(stdout);

			switch (res) {
			case 1: exit(EXIT_SUCCESS);
			case 0: exit(EXIT_FAILURE);
			default: exit(res);
			}
			break;

		case 'f':
			clo->ff_mode = E_FF_FULL;
			if (optarg) {
				clo->ff_separate_logs = true;
			}
			break;

		case ':':
			ERROR("missing mandatory option's argument");
			fprint_help(stderr);
			exit(EXIT_FAILURE);

		default:
			ERROR("unknown option: '-%c'", c);
		case '?':
			fprint_help(stderr);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc)
		clo->command = true;

	return optind;
}
