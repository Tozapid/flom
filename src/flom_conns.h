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
#ifndef FLOM_CONNS_H
# define FLOM_CONNS_H



#include <config.h>



#ifdef HAVE_GLIB_H
# include <glib.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif



#include "flom_config.h"
#include "flom_msg.h"
#include "flom_tcp.h"



/* save old FLOM_TRACE_MODULE and set a new value */
#ifdef FLOM_TRACE_MODULE
# define FLOM_TRACE_MODULE_SAVE FLOM_TRACE_MODULE
# undef FLOM_TRACE_MODULE
#else
# undef FLOM_TRACE_MODULE_SAVE
#endif /* FLOM_TRACE_MODULE */
#define FLOM_TRACE_MODULE      FLOM_TRACE_MOD_CONNS



/**
 * Default allocation size for @ref flom_conns_t objects
 */
#define FLOM_CONNS_DEFAULT_ALLOCATION  10
/**
 * Expansion allocation step
 */
#define FLOM_CONNS_PERCENT_ALLOCATION  20



/**
 * Possible state of a connection
 */
typedef enum flom_conn_state_e {
    /**
     * The connection is managed by main daemon thread (the listener thread)
     */
    FLOM_CONN_STATE_DAEMON,
    /**
     * The connection is managed by a locker thread (the resource manager
     * threads)
     */
    FLOM_CONN_STATE_LOCKER,
    /**
     * The connection is no more useful and must be eliminated by
     * @ref flom_conns_clean
     */
    FLOM_CONN_STATE_REMOVE
} flom_conn_state_t;



/**
 * Class of objects used to store connection data
 */
typedef struct {
    /**
     * Connection state
     */
    flom_conn_state_t     state;
    /**
     * The connection is related to a client waiting for resource creation
     * (it's logically parked inside a virtual incubator); this is a boolean
     * property
     */
    int                   wait;
    /**
     * TCP/IP connection data
     */
    flom_tcp_t            tcp;
    /**
     * Last received message (allocated by malloc)
     */
    struct flom_msg_s     *msg;
    /**
     * Step of the last sent/received  message
     */
    int                    last_step;
    /**
     * GMarkup Parser context (allocated by g_markup_parse_context_new)
     */
    GMarkupParseContext   *gmpc;
} flom_conn_t;



/**
 * A structured object used to register connections
 */
struct flom_conns_s {
    /**
     * Array used for poll function (it must be re-generated before poll
     * function)
     */
    struct pollfd *poll_array;
    /**
     * Connection domain as specified when calling socket function
     */
    int domain;
    /**
     * Array of connection data (it's an array of pointer for @ref
     * flom_conn_t)
     */
    GPtrArray     *array;
};
    


typedef struct flom_conns_s flom_conns_t;



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



    /**
     * Check if n field of the object and the real size of the underlying
     * array match
     * @param conns IN connections object
     * @return a boolean value: TRUE OK, FALSE KO
     */
    int flom_conns_check_n(flom_conns_t *conns);
    


    /**
     * Initialize a new object
     * @param conns IN/OUT object to be initialized
     * @param domain IN socket domain for all the connections managed by this
     *                  object
     */
    void flom_conns_init(flom_conns_t *conns, int domain);



    /**
     * Add a new connection
     * @param conns IN/OUT connections object
     * @param fd IN file descriptor
     * @param type IN socket type associated to file descriptor (SOCK_STREAM,
     *                SOCK_DGRAM)
     * @param addr_len IN lenght of address
     * @param sa IN address
     * @param main_thread IN thread is asking the connection: TRUE = father
     *        (listener) thread, FALSE = child (locker) thread
     * @return a reason code
     */
    int flom_conns_add(flom_conns_t *conns, int fd, int type,
                       socklen_t addr_len, const struct sockaddr *sa,
                       int main_thread);



    
    /**
     * Import a connection: the imported connection (@ref flom_conn_t)
     * must not be freed by the caller because the import does not make a copy
     * of the structure, it picks up the passed reference
     * @param conns IN/OUT connections object
     * @param fd IN file descriptor
     * @param conn IN connection object
     */
    void flom_conns_import(flom_conns_t *conns, int fd, flom_conn_t *conn);

    

    /**
     * Return the socket domain associated with all the connection
     * @return socket domain
     */
    static inline int flom_conns_get_domain(const flom_conns_t *conns) {
        return conns->domain;
    }



    /**
     * Set the socket domain of an already initialized connections object;
     * pay attention this may invalidate the already stored addresses and
     * should be used only when domain is not available at initialization
     * time
     * @param conns IN/OUT connections object
     * @param domain IN the new domain of the connections object
     */
    static inline void flom_conns_set_domain(flom_conns_t *conns,
                                             int domain) {
        conns->domain = domain;
    }

    

    /**
     * Return the number of active connections managed by the object
     * @param conns IN connections object
     * @return the number of active connections
     */
    static inline guint flom_conns_get_used(const flom_conns_t *conns) {
        return conns->array->len;
    }

    

    /**
     * Return the file descriptor associated to a connection
     * @param conns IN connections object
     * @param id IN identificator (position in array) of the connection
     * @return the associated file descriptor or @ref FLOM_NULL_FD if any error
     *         happens
     */
    static inline int flom_conns_get_fd(const flom_conns_t *conns, guint id) {
        if (id < conns->array->len)
            return flom_tcp_get_sockfd(
                &((flom_conn_t *)g_ptr_array_index(conns->array, id))->tcp);
        else
            return FLOM_NULL_FD;
    }



    /**
     * Return the socket type related to a file descriptor
     * @param conns IN connections object
     * @param id IN identificator (position in array) of the connection
     * @return SOCK_STREAM or SOCK_DGRAM or 0 (error condition)
     */
    static inline int flom_conns_get_type(
        const flom_conns_t *conns, guint id) {
        if (id < conns->array->len)
            return flom_tcp_get_socket_type(
                &((flom_conn_t *)g_ptr_array_index(conns->array, id))->tcp);
        else
            return 0;
    }



    /**
     * Return a reference (read-only pointer) to the struct containing
     * connection data of a specific connection
     * @param conns IN connections object
     * @param id IN identificator (position in array) of the connection
     * @return a reference to the asked structure or NULL
     */
    static inline flom_conn_t *flom_conns_get_conn(
        const flom_conns_t *conns, guint id) {
        if (id < conns->array->len)
            return ((flom_conn_t *)g_ptr_array_index(conns->array, id));
        else
            return NULL;
    }



    /**
     * Build a new array for poll function and return it
     * @param conns IN connections object
     * @return the fds array to be used with poll function or NULL if an
     *         error happens
     */
    struct pollfd *flom_conns_get_fds(flom_conns_t *conns);



    /**
     * Return the message associated to a connection
     * @param conns IN connections object
     * @param id IN identificator (position in array) of the connection
     * @return the associated message or NULL if any error happens
     */
    static inline struct flom_msg_s *flom_conns_get_msg(
        flom_conns_t *conns, guint id) {
        if (id < conns->array->len)
            return ((flom_conn_t *)g_ptr_array_index(conns->array, id))->msg;
        else
            return NULL;
    }



    /**
     * Return the GMarkupParseContext associated to a connection
     * @param conns IN connections object
     * @param id IN identificator (position in array) of the connection
     * @return the associated GMarkupParseContext or NULL if any error happens
     */
    static inline GMarkupParseContext *flom_conns_get_gmpc(        
        flom_conns_t *conns, guint id) {
        if (id < conns->array->len)
            return ((flom_conn_t *)g_ptr_array_index(conns->array, id))->gmpc;
        else
            return NULL;
    }



    /**
     * Set events field for every connection in the object
     * NOTE: it must be called after @ref flom_conns_get_fds because it
     *       resize the poll array if necessary
     * @param conns IN/OUT connections object
     * @param events IN new value for every events field
     * @return a reason code
     */
    int flom_conns_set_events(flom_conns_t *conns, short events);
    


    /**
     * Close a file descriptor and set it to @ref FLOM_NULL_FD; use
     * @ref flom_conns_clean to remove the connections associated to closed
     * file descriptors
     * @param conns IN/OUT connections object
     * @param id IN connection must be closed
     * @return a reason code
     */
    int flom_conns_close_fd(flom_conns_t *conns, guint id);



    /**
     * Mark as "transferred to another thread" a connection and detach it
     * from this connections object
     * @param conns IN/OUT connections object
     * @param id IN connection must be marked
     * @return a reason code
     */
    int flom_conns_trns_fd(flom_conns_t *conns, guint id);

    

    /**
     * Remove connections with invalid (closed) file descriptor
     * @param conns IN/OUT connections object
     * @return a reason code
     */
    int flom_conns_clean(flom_conns_t *conns);

    

    /**
     * Free all the memory allocated by connections object
     * @param conns IN/OUT connections object
     */
    void flom_conns_free(flom_conns_t *conns);



    /**
     * Trace the content of a connection data struct
     * @param conn IN connection object to trace
     */
    void flom_conn_trace(const flom_conn_t *conn);


    
    /**
     * Trace the content of a connections object
     * @param conns IN connections object to trace
     */
    void flom_conns_trace(const flom_conns_t *conns);



    /**
     * Set SO_KEEPALIVE and correlated parameters for the socket associated
     * to the passed file descriptor
     * @param config IN configuration object, NULL for global config
     * @param fd IN/OUT socket file descriptor
     * @return a reason code
     */
    int flom_conn_set_keepalive(flom_config_t *config, int fd);


    
#ifdef __cplusplus
}
#endif /* __cplusplus */



/* restore old value of FLOM_TRACE_MODULE */
#ifdef FLOM_TRACE_MODULE_SAVE
# undef FLOM_TRACE_MODULE
# define FLOM_TRACE_MODULE FLOM_TRACE_MODULE_SAVE
# undef FLOM_TRACE_MODULE_SAVE
#endif /* FLOM_TRACE_MODULE_SAVE */



#endif /* FLOM_CONNS_H */
