/*
 * Copyright (C) 2021-2026 Bytedance Ltd. and/or its affiliates
 */
#include "lib.h"
#include "lib.daemonInfo.h"

#define MAX_SHOWCONF_REQUIREMENTS 2

struct showconf_requirement {
    const char *paramName;
    const char *paramValue;
};

struct showconf_param {
    struct config_param paramInfo;
    int daemonMask;
    struct showconf_requirement requirements[MAX_SHOWCONF_REQUIREMENTS];
};

/*
 * Static showconf catalog.
 * paramInfo.paramValue is the displayed fallback value. initShowconfValues()
 * overlays configured values onto this table after each daemon reads lsf.conf.
 * daemonMask controls which daemon's showconf output includes the parameter.
 */
static struct showconf_param showconfParams[] = {
    {{"LSB_CONFDIR", "LSF_CONFDIR/lsbatch"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_DEBUG", "Undefined"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_DEBUG_MBD", "Undefined"}, SHOWCONF_MBD},
    {{"LSB_DEBUG_SBD", "Undefined"}, SHOWCONF_SBD},
    {{"LSB_JOB_CPULIMIT", "Undefined"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_JOB_MEMLIMIT", "Undefined"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_MAILPROG", "/usr/lib/sendmail"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_MAILSERVER", "Undefined"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_MAILSIZE_LIMIT", "Undefined"}, SHOWCONF_SBD},
    {{"LSB_MAILTO", "!U"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_MBD_PORT", "6881"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_MEMLIMIT_ENFORCE", "N"}, SHOWCONF_SBD},
    {{"LSB_MIG2PEND", "0"}, SHOWCONF_MBD},
    {{"LSB_MOD_ALL_JOBS", "N"}, SHOWCONF_MBD},
    {{"LSB_PACK_SKIP_ERROR", "N"}, SHOWCONF_MBD},
    {{"LSB_QMBD_ALIVE_TIME", "5"}, SHOWCONF_MBD,
     {{"LSB_QMBD_PORT", NULL}}},
    {{"LSB_QMBD_MAX_TASK_NUM", "2000"}, SHOWCONF_MBD,
     {{"LSB_QMBD_PORT", NULL}}},
    {{"LSB_QMBD_PORT", "Undefined"}, SHOWCONF_MBD},
    {{"LSB_QMBD_JOB_SYNC_MODE", "socket"}, SHOWCONF_MBD,
     {{"LSB_QMBD_PORT", NULL}}},
    {{"LSB_QMBD_SYNC_SHM_SIZE", "1024"}, SHOWCONF_MBD,
     {{"LSB_QMBD_PORT", NULL}, {"LSB_QMBD_JOB_SYNC_MODE", "shm"}}},
    {{"LSB_QMBD_THREAD_NUM", "online CPU cores"}, SHOWCONF_MBD,
     {{"LSB_QMBD_PORT", NULL}}},
    {{"LSB_REQUEUE_TO_BOTTOM", "0"}, SHOWCONF_MBD},
    {{"LSB_SBD_PORT", "6882"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_SET_TMPDIR", "n"}, SHOWCONF_SBD},
    {{"LSB_SHAREDIR", "LSF_INDEP/work"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSB_SIGSTOP", "Undefined"}, SHOWCONF_SBD},
    {{"LSB_TIME_MBD", "Undefined"}, SHOWCONF_MBD},
    {{"LSB_TIME_SBD", "Undefined"}, SHOWCONF_SBD},
    {{"LSF_API_CONNTIMEOUT", "5"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_API_RECVTIMEOUT", "20"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_AUTH", "eauth"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_AUTH_DAEMONS", "Undefined"}, SHOWCONF_SBD},
    {{"LSF_BINDIR", "LSF_MACHDEP/bin"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_CONF_RETRY_INT", "30"}, SHOWCONF_LIM},
    {{"LSF_CONF_RETRY_MAX", "0"}, SHOWCONF_LIM},
    {{"LSF_CONFDIR", "LSF_INDEP/conf"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_DEBUG_LIM", "Undefined"}, SHOWCONF_LIM},
    {{"LSF_ENVDIR", "/etc"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_LIBDIR", "LSF_MACHDEP/lib"}, SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_LIM_DEBUG", "Undefined"}, SHOWCONF_LIM},
    {{"LSF_LIM_PORT", "6879"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_LOG_MASK", "LOG_WARNING"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_LOGDIR", "syslog"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_MASTER_LIST", "Undefined"}, SHOWCONF_LIM},
    {{"LSF_NON_PRIVILEGED_PORTS", "Y"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_RES_PORT", "6878"}, SHOWCONF_LIM},
    {{"LSF_SERVERDIR", "Undefined"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{"LSF_UNIT_FOR_LIMITS", "MB"}, SHOWCONF_LIM | SHOWCONF_MBD | SHOWCONF_SBD},
    {{NULL, NULL}, 0}
};

static time_t showconfConfigTime = 0;

static struct showconf_param *findShowconfParam(const char *);
static int showconfParamIsVisible(const struct showconf_param *, int);

/*
 * Find a showconf catalog entry by parameter name
 * @param[in] name: Configuration parameter name to look up
 * @return: Pointer to the matching table entry, or NULL if the name is not
 *          tracked by showconf
 */
static struct showconf_param *
findShowconfParam(const char *name)
{
    int i;

    if (name == NULL)
        return NULL;

    for (i = 0; showconfParams[i].paramInfo.paramName != NULL; i++) {
        if (strcmp(showconfParams[i].paramInfo.paramName, name) == 0)
            return &showconfParams[i];
    }

    return NULL;
}

/*
 * Check whether a catalog entry applies to a daemon and its requirements.
 * A requirement without paramValue only requires the parameter to be set.
 * A requirement with paramValue also requires a case-insensitive value match.
 */
static int
showconfParamIsVisible(const struct showconf_param *param, int daemonMask)
{
    struct showconf_param *requiredParam;
    int i;

    if (param == NULL || !(param->daemonMask & daemonMask))
        return FALSE;

    for (i = 0; i < MAX_SHOWCONF_REQUIREMENTS; i++) {
        if (param->requirements[i].paramName == NULL)
            break;

        requiredParam = findShowconfParam(param->requirements[i].paramName);
        if (requiredParam == NULL
            || requiredParam->paramInfo.paramValue == NULL
            || requiredParam->paramInfo.paramValue[0] == '\0'
            || strcmp(requiredParam->paramInfo.paramValue, "Undefined") == 0)
            return FALSE;

        if (param->requirements[i].paramValue != NULL
            && strcasecmp(requiredParam->paramInfo.paramValue,
                          param->requirements[i].paramValue) != 0)
            return FALSE;
    }

    return TRUE;
}

/*
 * XDR encode or decode one showconf name/value entry
 * @param[in,out] xdrs: XDR stream
 * @param[in,out] param: Parameter entry to transfer
 * @param[in] hdr: LSF message header, unused here but required by the array
 *                 element callback signature
 * @return: TRUE on success, FALSE on string transfer failure
 */
static bool_t
xdr_showConfParam(XDR *xdrs, struct config_param *param,
                  struct LSFHeader *hdr)
{
    if (!xdr_var_string(xdrs, &param->paramName)
        || !xdr_var_string(xdrs, &param->paramValue))
        return FALSE;

    return TRUE;
}

/*
 * Estimate the encoded size of a showconf reply
 * The daemon callers allocate an exact response buffer before XDR encoding.
 * @param[in] reply: Reply to size; NULL returns the minimum header overhead
 * @return: Word-aligned encoded size in bytes
 */
int
xdrShowConfReplySize(const struct showConfReply *reply)
{
    int size = LSF_HEADER_LEN + 2 * NET_INTSIZE_ + NET_INTSIZE_ + 64;
    int i;

    if (reply == NULL)
        return size;

    /* Each entry is encoded as an array element plus two variable strings. */
    for (i = 0; i < reply->entryCount; i++) {
        size += NET_INTSIZE_;
        size += NET_INTSIZE_ + getXdrStrlen(reply->entries[i].paramName);
        size += NET_INTSIZE_ + getXdrStrlen(reply->entries[i].paramValue);
    }

    return ALIGNWORD_(size);
}

/*
 * XDR encode, decode, or free a showconf reply
 * @param[in,out] xdrs: XDR stream controlling ENCODE, DECODE, or FREE mode
 * @param[in,out] reply: Reply object to encode, populate, or free
 * @param[in] hdr: LSF message header passed through xdr_arrayElement()
 * @return: TRUE on success, FALSE on malformed input or allocation failure
 */
bool_t
xdr_showConfReply(XDR *xdrs, struct showConfReply *reply,
                  struct LSFHeader *hdr)
{
    int i;
    long configTime;

    if (xdrs->x_op == XDR_FREE) {
        if (reply != NULL) {
            if (reply->entries != NULL) {
                for (i = 0; i < reply->entryCount; i++) {
                    FREEUP(reply->entries[i].paramName);
                    FREEUP(reply->entries[i].paramValue);
                }
                FREEUP(reply->entries);
            }
            reply->entryCount = 0;
        }
        return TRUE;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (reply->entries != NULL) {
            for (i = 0; i < reply->entryCount; i++) {
                FREEUP(reply->entries[i].paramName);
                FREEUP(reply->entries[i].paramValue);
            }
            FREEUP(reply->entries);
        }
        reply->entryCount = 0;
    }

    /* XDR carries time_t as the existing LSF long wire type. */
    if (xdrs->x_op == XDR_ENCODE)
        configTime = (long)reply->configTime;

    if (!xdr_LSFlong(xdrs, &configTime))
        return FALSE;

    if (xdrs->x_op == XDR_DECODE)
        reply->configTime = (time_t)configTime;

    if (xdrs->x_op == XDR_DECODE) {
        if (!xdr_int(xdrs, &reply->entryCount))
            return FALSE;

        /* Reject suspicious counts before allocating the decoded array. */
        if (reply->entryCount < 0 || reply->entryCount > 10000)
            return FALSE;

        if (reply->entryCount == 0) {
            reply->entries = NULL;
            return TRUE;
        }

        reply->entries = calloc(reply->entryCount, sizeof(struct config_param));
        if (reply->entries == NULL) {
            reply->entryCount = 0;
            return FALSE;
        }
    } else if (!xdr_int(xdrs, &reply->entryCount)) {
        return FALSE;
    }

    /* xdr_showConfParam owns the per-entry name/value string transfer. */
    for (i = 0; i < reply->entryCount; i++) {
        if (!xdr_arrayElement(xdrs, (char *)&reply->entries[i], hdr,
                              xdr_showConfParam, NULL)) {
            if (xdrs->x_op == XDR_DECODE) {
                if (reply->entries != NULL) {
                    for (i = 0; i < reply->entryCount; i++) {
                        FREEUP(reply->entries[i].paramName);
                        FREEUP(reply->entries[i].paramValue);
                    }
                    FREEUP(reply->entries);
                }
                reply->entryCount = 0;
            }
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * Overlay configured daemon parameters onto the showconf catalog
 * The daemon parameter arrays contain only values read from lsf.conf or the
 * daemon environment. Parameters left unset keep the fallback value from
 * showconfParams.
 * @param[in] params: NULL-terminated daemon parameter array
 * @return: 0 on success
 */
int
initShowconfValues(struct config_param *params)
{
    struct showconf_param *showParam;
    char *envDir;
    int i;

    if (params != NULL) {
        for (i = 0; params[i].paramName != NULL; i++) {
            if (params[i].paramValue == NULL)
                continue;

            /* Only tracked parameters override the display fallback. */
            showParam = findShowconfParam(params[i].paramName);
            if (showParam != NULL)
                showParam->paramInfo.paramValue = params[i].paramValue;
        }
    }

    /* LSF_ENVDIR can come from the process environment rather than lsf.conf. */
    envDir = getenv("LSF_ENVDIR");
    if (envDir != NULL) {
        showParam = findShowconfParam("LSF_ENVDIR");
        if (showParam != NULL)
            showParam->paramInfo.paramValue = envDir;
    }

    /* Preserve the first initialization time as the configuration timestamp. */
    if (showconfConfigTime == 0)
        showconfConfigTime = time(NULL);
    return 0;
}

/*
 * Release strings owned by a showconf reply
 * @param[in,out] reply: Reply object previously filled by makeShowConfReply()
 */
void
freeShowConfReply(struct showConfReply *reply)
{
    int i;

    if (reply == NULL)
        return;

    if (reply->entries != NULL) {
        for (i = 0; i < reply->entryCount; i++) {
            FREEUP(reply->entries[i].paramName);
            FREEUP(reply->entries[i].paramValue);
        }
        FREEUP(reply->entries);
    }
    reply->entryCount = 0;
}

/*
 * Build a daemon-specific showconf reply
 * The caller owns reply->entries and must release it with freeShowConfReply().
 * @param[in] daemonMask: SHOWCONF_MBD, SHOWCONF_SBD, or SHOWCONF_LIM
 * @param[out] reply: Reply object populated with copied parameter names/values
 * @return: 0 on success, -1 on allocation or invalid-argument failure
 */
int
makeShowConfReply(int daemonMask, struct showConfReply *reply)
{
    int i;
    int out = 0;
    int count = 0;

    if (reply == NULL)
        return -1;

    memset(reply, 0, sizeof(*reply));
    reply->configTime = showconfConfigTime ? showconfConfigTime : time(NULL);

    /* Count first so the wire reply can use a compact contiguous array. */
    for (i = 0; showconfParams[i].paramInfo.paramName != NULL; i++) {
        if (showconfParamIsVisible(&showconfParams[i], daemonMask))
            count++;
    }

    reply->entryCount = count;
    if (reply->entryCount == 0)
        return 0;

    reply->entries = calloc(reply->entryCount, sizeof(struct config_param));
    if (reply->entries == NULL) {
        reply->entryCount = 0;
        return -1;
    }

    /* Copy strings because the receiver and XDR free path own the reply. */
    for (i = 0; showconfParams[i].paramInfo.paramName != NULL; i++) {
        if (!showconfParamIsVisible(&showconfParams[i], daemonMask))
            continue;

        reply->entries[out].paramName =
            putstr_(showconfParams[i].paramInfo.paramName);
        reply->entries[out].paramValue =
            putstr_(showconfParams[i].paramInfo.paramValue ?
                    showconfParams[i].paramInfo.paramValue : "");
        if (reply->entries[out].paramName == NULL
            || reply->entries[out].paramValue == NULL) {
            freeShowConfReply(reply);
            return -1;
        }
        out++;
    }

    return 0;
}

/*
 * Print a decoded showconf reply in command output format
 * @param[in] daemonName: Display name such as "MBD", "SBD", or "LIM"
 * @param[in] host: Optional host name for host-scoped daemon output
 * @param[in] reply: Decoded showconf reply to print
 */
void
printShowConfReply(const char *daemonName, const char *host,
                   const struct showConfReply *reply)
{
    char timeBuf[64];
    time_t configTime;
    int i;

    if (daemonName == NULL || reply == NULL)
        return;

    /* Be defensive for old or failed replies that carry no timestamp. */
    configTime = reply->configTime ? reply->configTime : time(NULL);
    ctime_r(&configTime, timeBuf);
    timeBuf[strcspn(timeBuf, "\n")] = '\0';

    if (host != NULL)
        fprintf(stdout, "%s configuration for host <%s> at %s\n",
                daemonName, host, timeBuf);
    else
        fprintf(stdout, "%s configuration at %s\n", daemonName, timeBuf);

    for (i = 0; i < reply->entryCount; i++) {
        if (reply->entries[i].paramValue != NULL
            && strcmp(reply->entries[i].paramValue, "Undefined") == 0)
            continue;

        fprintf(stdout, "\t%s = %s\n",
                reply->entries[i].paramName,
                reply->entries[i].paramValue ? reply->entries[i].paramValue : "");
    }
    fprintf(stdout, "\n");
}
