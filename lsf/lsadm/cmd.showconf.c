/* $Id: cmd.showconf.c $
 * Copyright (C) 2021-2026 Bytedance Ltd. and/or its affiliates
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include <stdio.h>
#include <string.h>

#include "../lsf.h"
#include "../lim/limout.h"
#include "../lib/lib.hdr.h"
#include "../lib/lib.daemonInfo.h"

extern int optind;
extern int initenv_(struct config_param *, char *);
extern int callLim_(enum limReqCode, void *, bool_t (*)(), void *,
                    bool_t (*)(), char *, int, struct LSFHeader *);

static int showConfHost(char *);

static int exitrc;

/*
 * Execute lsadmin showconf lim
 * With no host operand, query the local LIM. With "all", query every server
 * host returned by LIM. Otherwise query each host named on the command line.
 * @param[in] argc: Command argument count
 * @param[in] argv: Command argument vector
 * @return: 0 if all requested output succeeds, -1 on runtime failure,
 *          -2 on usage error
 */
int
limShowconf(int argc, char **argv)
{
    char *localHost;
    struct hostInfo *hostinfo;
    int numhosts = 0;
    int i;

    exitrc = 0;

    if (optind >= argc || strcmp(argv[optind], "lim") != 0)
        return -2;

    optind++;
    if (optind == argc) {
        if ((localHost = ls_getmyhostname()) == NULL) {
            ls_perror("ls_getmyhostname");
            return -1;
        }
        return showConfHost(localHost);
    }

    /* "all" is limited to server hosts because client-only hosts run no LIM. */
    if (optind == argc - 1 && strcmp(argv[optind], "all") == 0) {
        hostinfo = ls_gethostinfo("-:server", &numhosts, NULL, 0, LOCAL_ONLY);
        if (hostinfo == NULL) {
            ls_perror("ls_gethostinfo");
            return -1;
        }

        for (i = 0; i < numhosts; i++) {
            if (showConfHost(hostinfo[i].hostName) < 0)
                exitrc = -1;
        }
        return exitrc;
    }

    for (; optind < argc; optind++) {
        if (showConfHost(argv[optind]) < 0)
            exitrc = -1;
    }

    return exitrc;
}

/*
 * Request and print showconf output from one LIM host
 * @param[in] host: LIM host to query
 * @return: 0 on success, -1 on initialization, transport, or decode failure
 */
static int
showConfHost(char *host)
{
    struct showConfReply reply;
    struct LSFHeader hdr;
    char msg[MAXLINELEN];

    memset(&reply, 0, sizeof(reply));
    if (initenv_(NULL, NULL) < 0)
        return -1;

    /* callLim_ decodes the UDP reply directly into reply via xdr_showConfReply. */
    if (callLim_(LIM_SHOWCONF, NULL, NULL, &reply, xdr_showConfReply,
                 host, 0, &hdr) < 0) {
        snprintf(msg, sizeof(msg), "showconf lim <%s>", host);
        ls_perror(msg);
        freeShowConfReply(&reply);
        return -1;
    }

    printShowConfReply("LIM", host, &reply);
    freeShowConfReply(&reply);
    return 0;
}
