/*
 * Copyright (c) 2013-2016, Christian Ferrari <tiian@users.sourceforge.net>
 * All rights reserved.
 *
 * This file is part of FLoM and libflom (FLoM API client library)
 *
 * FLoM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2.0 as
 * published by the Free Software Foundation.
 *
 * This file is part of libflom too and you can redistribute it and/or modify
 * it under the terms of one of the following licences:
 * - GNU General Public License version 2.0
 * - GNU Lesser General Public License version 2.1
 * as published by the Free Software Foundation.
 *
 * FLoM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and
 * GNU Lesser General Public License along with FLoM.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include <config.h>



#ifdef HAVE_ASSERT_H
# include <assert.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif



#include "flom_conns.h"
#include "flom_config.h"
#include "flom_errors.h"
#include "flom_trace.h"



/* set module trace flag */
#ifdef FLOM_TRACE_MODULE
# undef FLOM_TRACE_MODULE
#endif /* FLOM_TRACE_MODULE */
#define FLOM_TRACE_MODULE   FLOM_TRACE_MOD_CONNS



int flom_conns_check_n(flom_conns_t *conns)
{
    /* this is a dirty hack because this struct is opaque and this operation
       should not be done :( */
    typedef struct _GPtrArrayPriv {
        gpointer *pdata;
        guint len;
        guint size;
    } GPtrArrayPriv;
    GPtrArrayPriv *p = (GPtrArrayPriv *)conns->array;
    
    FLOM_TRACE(("flom_conns_check_n: p->len=%u, p->size=%u, "
                "conns->array->len=%u\n", p->len, p->size, conns->array->len));
    return conns->array->len == p->len;
}



void flom_conns_init(flom_conns_t *conns, int domain)
{
    FLOM_TRACE(("flom_conns_init\n"));
    conns->poll_array = NULL;
    conns->domain = domain;
    conns->array = g_ptr_array_new();
    FLOM_TRACE(("flom_conns_init: allocated array:%p\n", conns->array));
}



int flom_conns_add(flom_conns_t *conns, int fd, int type,
                   socklen_t addr_len, const struct sockaddr *sa,
                   int main_thread)
{
    enum Exception { NEW_OBJ
                     , INVALID_DOMAIN
                     , G_MARKUP_PARSE_CONTEXT_NEW_ERROR
                     , NONE } excp;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;

    flom_conn_t *tmp;
    
    FLOM_TRACE(("flom_conns_add\n"));
    TRY {
        GMarkupParseContext *tmp_parser;
        if (NULL == (tmp = flom_conn_new(NULL)))
            THROW(NEW_OBJ);
        FLOM_TRACE(("flom_conns_add: allocated a new connection (%p)\n", tmp));
        flom_tcp_init(flom_conn_get_tcp(tmp), NULL);
        flom_tcp_set_domain(flom_conn_get_tcp(tmp), conns->domain);
        switch (conns->domain) {
            case AF_UNIX:
                memcpy(flom_tcp_get_sa_un(flom_conn_get_tcp(tmp)), sa,
                       sizeof(struct sockaddr_un));
                break;
            case AF_INET:
                memcpy(flom_tcp_get_sa_in(flom_conn_get_tcp(tmp)), sa,
                       sizeof(struct sockaddr_in));
                break;
            case AF_INET6:
                memcpy(flom_tcp_get_sa_in6(flom_conn_get_tcp(tmp)), sa,
                       sizeof(struct sockaddr_in6));
                break;
            default:
                THROW(INVALID_DOMAIN);
        }
        flom_tcp_set_sockfd(flom_conn_get_tcp(tmp), fd);
        assert(SOCK_STREAM == type || SOCK_DGRAM == type);
        flom_tcp_set_socket_type(flom_conn_get_tcp(tmp), type);
        flom_conn_set_state(tmp, main_thread ?
                            FLOM_CONN_STATE_DAEMON : FLOM_CONN_STATE_LOCKER);
        flom_conn_set_wait(tmp, FALSE);
        flom_tcp_set_addrlen(flom_conn_get_tcp(tmp), addr_len);
        
        /* initialize the associated parser */
        if (NULL == (tmp_parser = g_markup_parse_context_new (
                         &flom_msg_parser, 0,
                         (gpointer)(flom_conn_get_msg(tmp)), NULL)))
            THROW(G_MARKUP_PARSE_CONTEXT_NEW_ERROR);
        flom_conn_set_parser(tmp, tmp_parser);
        g_ptr_array_add(conns->array, tmp);
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case NEW_OBJ:
                ret_cod = FLOM_RC_NEW_OBJ;
                break;
            case INVALID_DOMAIN:
                ret_cod = FLOM_RC_OBJ_CORRUPTED;
                break;
            case G_MARKUP_PARSE_CONTEXT_NEW_ERROR:
                ret_cod = FLOM_RC_G_MARKUP_PARSE_CONTEXT_NEW_ERROR;
                break;
            case NONE:
                ret_cod = FLOM_RC_OK;
                break;
            default:
                ret_cod = FLOM_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    FLOM_TRACE(("flom_conns_add: excp=%d\n", excp));
    if (NONE != excp) {
        if (NEW_OBJ < excp) /* release connection data */
            flom_conn_delete(tmp);
    }
    FLOM_TRACE(("flom_conns_add/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    return ret_cod;
}



void flom_conns_import(flom_conns_t *conns, int fd, flom_conn_t *conn)
{
    FLOM_TRACE(("flom_conns_import\n"));
    flom_conn_trace(conn);
    g_ptr_array_add(conns->array, conn);
}
    


struct pollfd *flom_conns_get_fds(flom_conns_t *conns)
{
    struct pollfd *tmp = NULL;
    guint i;
    
    FLOM_TRACE(("flom_conns_get_fds\n"));
    /* resize the previous poll array */
    if (NULL == (tmp = (struct pollfd *)realloc(
                     conns->poll_array,
                     (size_t)(conns->array->len*sizeof(struct pollfd))))) {
        conns->poll_array = tmp;
        return NULL;
    }    
    /* reset the array */
    memset(tmp, 0, (size_t)(conns->array->len*sizeof(struct pollfd)));
    /* fill the poll array with file descriptors */
    for (i=0; i<conns->array->len; ++i) {
        tmp[i].fd = flom_tcp_get_sockfd(
            flom_conn_get_tcp(
                (flom_conn_t *)g_ptr_array_index(conns->array, i)));
    }
    conns->poll_array = tmp;
    return conns->poll_array;
}




int flom_conns_set_events(flom_conns_t *conns, short events)
{
    enum Exception { OBJECT_CORRUPTED
                     , NONE } excp;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;
    
    FLOM_TRACE(("flom_conns_set_events\n"));
    TRY {
        guint i;
        for (i=0; i<conns->array->len; ++i) {
            flom_conn_t *c =
                (flom_conn_t *)g_ptr_array_index(conns->array, i);
            if (FLOM_NULL_FD != flom_tcp_get_sockfd(flom_conn_get_tcp(c)))
                conns->poll_array[i].events = events;
            else {
                FLOM_TRACE(("flom_conns_set_events: i=%u, "
                            "conns->poll_array[i].fd=%d\n", i,
                            conns->poll_array[i].fd));
                THROW(OBJECT_CORRUPTED);
            }
        }
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case OBJECT_CORRUPTED:
                ret_cod = FLOM_RC_OBJ_CORRUPTED;
                break;
            case NONE:
                ret_cod = FLOM_RC_OK;
                break;
            default:
                ret_cod = FLOM_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    FLOM_TRACE(("flom_conns_set_events/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    return ret_cod;
}



int flom_conns_close_fd(flom_conns_t *conns, guint id)
{
    enum Exception { OUT_OF_RANGE
                     , NULL_OBJECT
                     , TCP_CLOSE
                     , NONE } excp;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;
    
    FLOM_TRACE(("flom_conns_close_fd\n"));
    TRY {
        flom_conn_t *c;
        FLOM_TRACE(("flom_conns_close: closing connection id=%u\n", id));
        if (id >= conns->array->len)
            THROW(OUT_OF_RANGE);
        if (NULL == (c = (flom_conn_t *)
                     g_ptr_array_index(conns->array, id)))
            THROW(NULL_OBJECT);
        if (FLOM_CONN_STATE_REMOVE != flom_conn_get_state(c)) {
            flom_conn_set_state(c, FLOM_CONN_STATE_REMOVE);
            if (FLOM_NULL_FD == flom_tcp_get_sockfd(flom_conn_get_tcp(c))) {
                FLOM_TRACE(("flom_conns_close: connection id=%u already "
                            "closed, skipping...\n", id));
            } else {
                FLOM_TRACE(("flom_conns_close: closing fd=%d\n",
                            flom_tcp_get_sockfd(flom_conn_get_tcp(c))));
                if (FLOM_RC_OK != (ret_cod = flom_tcp_close(
                                       flom_conn_get_tcp(c))))
                    THROW(TCP_CLOSE);
            }
        } else {
            FLOM_TRACE(("flom_conns_close: connection id=%u already "
                        "in state %d, skipping...\n", id,
                        flom_conn_get_state(c)));
        } /* if (FLOM_CONN_STATE_REMOVE == flom_conn_get_state(c)) */
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case OUT_OF_RANGE:
                ret_cod = FLOM_RC_OUT_OF_RANGE;
                break;
            case NULL_OBJECT:
                ret_cod = FLOM_RC_NULL_OBJECT;
                break;
            case TCP_CLOSE:
                break;
            case NONE:
                ret_cod = FLOM_RC_OK;
                break;
            default:
                ret_cod = FLOM_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    FLOM_TRACE(("flom_conns_close_fd/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    return ret_cod;
}



int flom_conns_trns_fd(flom_conns_t *conns, guint id)
{
    enum Exception { OUT_OF_RANGE
                     , G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR
                     , NONE } excp;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;
    
    FLOM_TRACE(("flom_conns_trns_fd\n"));
    TRY {
        flom_conn_t *c;
        FLOM_TRACE(("flom_conns_trns: marking as transferred connection "
                    "id=%u\n", id));
        if (id >= conns->array->len)
            THROW(OUT_OF_RANGE);
        /* update connection state */
        c = (flom_conn_t *)g_ptr_array_index(conns->array, id);
        flom_conn_set_state(c, FLOM_CONN_STATE_LOCKER);
        /* detach the connection from this connections object (it
           will be attached by a locker connections object */
        if (NULL == g_ptr_array_remove_index_fast(conns->array, id)) {
            THROW(G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR);
        }
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case OUT_OF_RANGE:
                ret_cod = FLOM_RC_OUT_OF_RANGE;
                break;
            case G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR:
                ret_cod = FLOM_RC_G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR;
                break;
            case NONE:
                ret_cod = FLOM_RC_OK;
                break;
            default:
                ret_cod = FLOM_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    FLOM_TRACE(("flom_conns_trns_fd/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    return ret_cod;
}



int flom_conns_clean(flom_conns_t *conns)
{
    enum Exception { G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR
                     , NONE } excp;
    int ret_cod = FLOM_RC_INTERNAL_ERROR;
    guint i=0;
    
    FLOM_TRACE(("flom_conns_clean\n"));
    TRY {
        while (i<conns->array->len) {
            flom_conn_t *c =
                (flom_conn_t *)g_ptr_array_index(conns->array, i);
            FLOM_TRACE(("flom_conns_clean: i=%u, state=%d, wait=%d, "
                        "fd=%d %s\n",
                        i, flom_conn_get_state(c), flom_conn_get_wait(c),
                        flom_tcp_get_sockfd(flom_conn_get_tcp(c)),
                        FLOM_CONN_STATE_REMOVE == flom_conn_get_state(c) ?
                        "(removing...)" : FLOM_EMPTY_STRING));
            flom_conn_trace(c);
            if (FLOM_CONN_STATE_REMOVE == flom_conn_get_state(c)) {
                /* connections with this state are no more valid and must be
                   removed and destroyed */
                /* removing message object */
                /* removing parser object */
                flom_conn_free_parser(c);
                /* removing from array */
                if (NULL == g_ptr_array_remove_index_fast(conns->array, i)) {
                    THROW(G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR);
                }
                /* release connection */
                FLOM_TRACE(("flom_conns_clean: releasing connection %p\n", c));
                flom_conn_delete(c);
            } else i++;
        } /* while (i<conns->n) */
        
        THROW(NONE);
    } CATCH {
        switch (excp) {
            case NONE:
                ret_cod = FLOM_RC_OK;
                break;
            case G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR:
                ret_cod = FLOM_RC_G_PTR_ARRAY_REMOVE_INDEX_FAST_ERROR;
                break;
            default:
                ret_cod = FLOM_RC_INTERNAL_ERROR;
        } /* switch (excp) */
    } /* TRY-CATCH */
    FLOM_TRACE(("flom_conns_clean/excp=%d/"
                "ret_cod=%d/errno=%d\n", excp, ret_cod, errno));
    return ret_cod;
}



void flom_conns_free(flom_conns_t *conns)
{
    guint i;
    FLOM_TRACE(("flom_conns_free: starting...\n"));
    for (i=0; i<conns->array->len; ++i)
        flom_conns_close_fd(conns, i);
    flom_conns_clean(conns);
    if (NULL != conns->poll_array) {
        FLOM_TRACE(("flom_conns_free: releasing poll_array:%p\n",
                    conns->poll_array));
        free(conns->poll_array);
        conns->poll_array = NULL;
    }
    assert(flom_conns_check_n(conns));
    if (NULL != conns->array) {
        FLOM_TRACE(("flom_conns_free: releasing array:%p\n",
                    conns->array));
        g_ptr_array_free(conns->array, TRUE);
        conns->array = NULL;
    }
    FLOM_TRACE(("flom_conns_free: completed\n"));
}



void flom_conns_trace(const flom_conns_t *conns)
{
    FLOM_TRACE(("flom_conns_trace: object=%p\n", conns));
    FLOM_TRACE(("flom_conns_trace: domain=%d, len=%u, array=%p, "
                "poll_array=%p\n",
                conns->domain, conns->array->len, conns->array,
                conns->poll_array));
}
