/*
 * Copyright (c) 2013-2014, Christian Ferrari <tiian@users.sourceforge.net>
 * All rights reserved.
 *
 * This file is part of FLOM.
 *
 * FLOM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * FLOM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FLOM.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <config.h>

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif
#ifdef HAVE_GLIB_H
# include <glib.h>
#endif



#define FLOM_TRACE_MODULE FLOM_TRACE_MOD_GENERIC



#include "flom_config.h"
#include "flom_client.h"
#include "flom_conns.h"
#include "flom_errors.h"
#include "flom_exec.h"
#include "flom_rsrc.h"
#include "flom_trace.h"



static gboolean print_version = FALSE;
static gboolean verbose = FALSE;
static char *config_file = NULL;
static gchar *resource_name = NULL;
static gchar *resource_wait = NULL;
static gint resource_timeout = FLOM_NETWORK_WAIT_TIMEOUT;
static gchar *lock_mode = NULL;
static gchar *command_trace_file = NULL;
static gchar *daemon_trace_file = NULL;
static gchar **command_argv = NULL;
/* command line options */
static GOptionEntry entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &print_version, "Print package info and exit", NULL },
    { "verbose", 'V', 0, G_OPTION_ARG_NONE, &verbose, "Activate verbose messages", NULL },
    { "config-file", 'c', 0, G_OPTION_ARG_STRING, &config_file, "User configuration file name", NULL },
    { "resource-name", 'r', 0, G_OPTION_ARG_STRING, &resource_name, "Specify the name of the resource to be locked", NULL },
    { "resource-wait", 'w', 0, G_OPTION_ARG_STRING, &resource_wait, "Specify if the command enques when the resource is already locked (accepted values 'yes', 'no')", NULL },
    { "resource-timeout", 'o', 0, G_OPTION_ARG_INT, &resource_timeout, "Specify maximum wait time (milliseconds) if a resource is already locked", NULL },
    { "lock-mode", 'l', 0, G_OPTION_ARG_STRING, &lock_mode, "Resource lock mode ('NL', 'CR', 'CW', 'PR', 'PW', 'EX')", NULL },
    { "command-trace-file", 'T', 0, G_OPTION_ARG_STRING, &command_trace_file, "Specify command (foreground process) trace file name (absolute path required)", NULL },
    { "daemon-trace-file", 't', 0, G_OPTION_ARG_STRING, &daemon_trace_file, "Specify daemon (background process) trace file name (absolute path required)", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &command_argv, "Command must be executed under flom control" },
    { NULL }
};



int main (int argc, char *argv[])
{
    GError *error = NULL;
    GOptionContext *option_context;
    int child_status = 0;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;
    struct flom_conn_data_s cd;

    option_context = g_option_context_new("-- command to execute");
    g_option_context_add_main_entries(option_context, entries, NULL);
    if (!g_option_context_parse(option_context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        exit(FLOM_ES_GENERIC_ERROR);
    }
    g_option_context_free(option_context);

    if (print_version) {
        g_print("FLOM: Free LOck Manager\n"
                "Copyright (c) 2013-2014, Christian Ferrari; "
                "all rights reserved.\n"
                "License: GPL (GNU Public License) version 2\n"
                "Package name: %s; package version: %s; release date: %s\n"
                "Access http://sourceforge.net/projects/flom/ for "
                "project community activities\n",
                FLOM_PACKAGE_NAME, FLOM_PACKAGE_VERSION, FLOM_PACKAGE_DATE);
        exit(FLOM_ES_OK);
    }

    /* initialize trace destination if necessary */
    FLOM_TRACE_INIT;
    
    /* initialize regular expression table */
    if (FLOM_RC_OK != (ret_cod = global_res_name_preg_init())) {
        g_print("global_res_name_preg_init: ret_cod=%d\n", ret_cod);
        exit(FLOM_ES_GENERIC_ERROR);
    }

    /* reset global configuration */
    flom_config_reset();
    /* initialize configuration with standard system, statndard user and
       user customized config files */
    if (FLOM_RC_OK != (ret_cod = flom_config_init(config_file))) {
        g_print("flom_config_init: ret_cod=%d\n", ret_cod);
        exit(FLOM_ES_GENERIC_ERROR);
    }
    /* overrides configuration with command line passed arguments */
    if (NULL != resource_name)
        if (FLOM_RC_OK != (ret_cod = flom_config_set_resource_name(
                               resource_name))) {
            g_print("flom_config_set_resource_name: ret_cod=%d\n", ret_cod);
            exit(FLOM_ES_GENERIC_ERROR);
        }
    if (NULL != resource_wait) {
        flom_bool_value_t fbv;
        if (FLOM_BOOL_INVALID == (
                fbv = flom_bool_value_retrieve(resource_wait))) {
            g_print("resource-wait: '%s' is an invalid value\n",
                    resource_wait);
            exit(FLOM_ES_GENERIC_ERROR);
        }
        flom_config_set_resource_wait(fbv);
    }
    if (FLOM_NETWORK_WAIT_TIMEOUT != resource_timeout) {
        /* timeout is useless if no wait was specified */
        if (FLOM_BOOL_NO == flom_config_get_resource_wait())
            g_warning("Timeout ignored because 'no wait' behavior was "
                      "specified\n");
        else
            flom_config_set_resource_timeout(resource_timeout);
    }
    if (NULL != lock_mode) {
        flom_lock_mode_t flm;
        if (FLOM_LOCK_MODE_INVALID == (
                flm = flom_lock_mode_retrieve(lock_mode))) {
            g_print("lock-mode: '%s' is an invalid value\n", lock_mode);
            exit(FLOM_ES_GENERIC_ERROR);
        }
        flom_config_set_lock_mode(flm);
    }
    if (NULL != daemon_trace_file)
        flom_config_set_daemon_trace_file(daemon_trace_file);
    if (NULL != command_trace_file) {
        flom_config_set_command_trace_file(command_trace_file);
        /* change trace destination if necessary */
        FLOM_TRACE_REOPEN(flom_config_get_command_trace_file());
    }

    /* print configuration */
    if (verbose)
        flom_config_print();
    
    /* check the command is not null */
    if (NULL == command_argv) {
        g_warning("No command to execute!\n");
        exit(FLOM_ES_UNABLE_TO_EXECUTE_COMMAND);        
    }

    /* open connection to a valid flom lock manager... */
    if (FLOM_RC_OK != (ret_cod = flom_client_connect(&cd))) {
        g_print("flom_client_connect: ret_cod=%d (%s)\n",
                ret_cod, flom_strerror(ret_cod));
        exit(FLOM_ES_GENERIC_ERROR);
    }

    /* sending lock command */
    ret_cod = flom_client_lock(&cd, flom_config_get_resource_timeout());
    switch (ret_cod) {
        case FLOM_RC_OK: /* OK, go on */
            break;
        case FLOM_RC_LOCK_BUSY: /* busy */
            g_print("Resource already locked, the lock cannot be obtained\n");
            /* gracefully disconnect from daemon */
            if (FLOM_RC_OK != (ret_cod = flom_client_disconnect(&cd))) {
                g_print("flom_client_unlock: ret_cod=%d (%s)\n",
                        ret_cod, flom_strerror(ret_cod));
            }
            exit(FLOM_ES_RESOURCE_BUSY);
            break;
        case FLOM_RC_NETWORK_TIMEOUT: /* timeout expired, busy resource */
            g_print("Resource already locked, the lock was not obtained "
                    "because timeout (%d milliseconds) expired\n",
                    flom_config_get_resource_timeout());
            /* gracefully disconnect from daemon */
            if (FLOM_RC_OK != (ret_cod = flom_client_disconnect(&cd))) {
                g_print("flom_client_unlock: ret_cod=%d (%s)\n",
                        ret_cod, flom_strerror(ret_cod));
            }
            exit(FLOM_ES_RESOURCE_BUSY);
            break;            
        default:
            g_print("flom_client_lock: ret_cod=%d (%s)\n",
                    ret_cod, flom_strerror(ret_cod));
            exit(FLOM_ES_GENERIC_ERROR);
    } /* switch (ret_cod) */

    /* execute the command */
    if (FLOM_RC_OK != (ret_cod = flom_exec(command_argv, &child_status))) {
        guint i, num;
        g_print("Unable to execute command: '");
        num = g_strv_length(command_argv);
        for (i=0; i<num; ++i)
            g_print("%s", command_argv[i]);
        g_print("'\n");
        exit(FLOM_ES_UNABLE_TO_EXECUTE_COMMAND);
    }
    
    /* sending unlock command */
    if (FLOM_RC_OK != (ret_cod = flom_client_unlock(&cd))) {
        g_print("flom_client_unlock: ret_cod=%d (%s)\n",
                ret_cod, flom_strerror(ret_cod));
        exit(FLOM_ES_GENERIC_ERROR);
    }

    /* gracefully disconnect from daemon */
    if (FLOM_RC_OK != (ret_cod = flom_client_disconnect(&cd))) {
        g_print("flom_client_unlock: ret_cod=%d (%s)\n",
                ret_cod, flom_strerror(ret_cod));
    }

    g_strfreev (command_argv);
    command_argv = NULL;

    /* release config data */
    flom_config_free();
    /* release regular expression data */
    global_res_name_preg_free();
    
	return child_status;
}
