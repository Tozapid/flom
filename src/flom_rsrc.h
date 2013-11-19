/*
 * Copyright (c) 2013, Christian Ferrari <tiian@users.sourceforge.net>
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
#ifndef FLOM_RSRC_H
# define FLOM_RSRC_H



#include <config.h>



#ifdef HAVE_REGEX_H
# include <regex.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif



#include "flom_msg.h"
#include "flom_trace.h"



/* save old FLOM_TRACE_MODULE and set a new value */
#ifdef FLOM_TRACE_MODULE
# define FLOM_TRACE_MODULE_SAVE FLOM_TRACE_MODULE
# undef FLOM_TRACE_MODULE
#else
# undef FLOM_TRACE_MODULE_SAVE
#endif /* FLOM_TRACE_MODULE */
#define FLOM_TRACE_MODULE      FLOM_TRACE_MOD_RSRC



/**
 * Type of resource that must be locked (enum)
 */
enum flom_rsrc_type_e {
    /**
     * Null resource type
     */
    FLOM_RSRC_TYPE_NULL,
    /**
     * Simple resource type (a single non numerical resource)
     */
    FLOM_RSRC_TYPE_SIMPLE,
    /**
     * Number of managed resource types
     */
    FLOM_RSRC_TYPE_N
};
/**
 * Type of resource that must be locked
 */
typedef enum flom_rsrc_type_e flom_rsrc_type_t;



/**
 * Resource data for type "simple" @ref FLOM_RSRC_TYPE_SIMPLE
 */
struct flom_rsrc_data_simple_s {
    /**
     * Type of lock currently applied to the resource
     */
    flom_lock_type_t        current_lock;
};



/* necessary to declare flom_resource_t used inside the struct ("class")
   definition */
struct flom_resource_s;
/**
 * Resource type: a resource is an object that can be locked!
 */
typedef struct flom_resource_s flom_resource_t;
/**
 * Base struct for resource object
 */
struct flom_resource_s {
    /**
     * Resource type
     */
    flom_rsrc_type_t          type;
    /**
     * Resource name (allocated by g_strdup)
     */
    gchar                    *name;
    /**
     * Locking data associated with a resource (it depends from type)
     */
    union {
        struct flom_rsrc_data_simple_s       simple;
    } data;
    /**
     * Method called to process incoming messages (it depends from resource
     * type)
     */
    int    (*inmsg)   (flom_resource_t *, struct flom_msg_s *);
};



/**
 * This is a global static object shared by all modules and contain the
 * precompiled regular expression used to parse resource names and check if
 * they are valid resource names
 */
extern regex_t global_res_name_preg[];



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


    
    /**
     * Initialize the precompiled regular expression @ref global_res_name_preg
     * @return a reason code
     */
    int global_res_name_preg_init();
    
    

    /**
     * Retrieve the type of the resource from its name
     * @param resource_name IN resource name
     * @return resource type @ref flom_rsrc_type_t;
     *     @ref FLOM_RSRC_TYPE_NULL means the name is not valid for
     *      any resource type
     */
    flom_rsrc_type_t flom_rsrc_get_type(const gchar *resource_name);

    

    /**
     * Initialize a resource
     * @param resource IN/OUT object to be initialized
     * @param type IN resource type
     * @param name IN resource name
     * @return a reason code
     */
    int flom_resource_init(flom_resource_t *resource,
                           flom_rsrc_type_t type, const gchar *name);



    /**
     * Free all dynamically allocated memory
     * @param resource IN/OUT object to release
     */
    void flom_resource_free(flom_resource_t *resource);


    
    /**
     * Get the name of a resource
     * @param resource IN referente to resource object
     * @return the name of the resource
     */
    static inline const gchar *flom_resource_get_name(
        const flom_resource_t *resource) {
        return resource->name;
    }



    /**
     * Get the type of a resource
     * @param resource IN referente to resource object
     * @return the type of the resource
     */
    static inline flom_rsrc_type_t flom_resource_get_type(
        const flom_resource_t *resource) {
        return resource->type;
    }

    
    
    /**
     * Manage an incoming message for a "simple" resource
     * @param resource IN/OUT reference to resource object
     * @param msg IN reference to incoming message
     * @return a reason code
     */
    int flom_resource_simple_inmsg(flom_resource_t *resource,
                                   struct flom_msg_s *msg);


    
#ifdef __cplusplus
}
#endif /* __cplusplus */



/* restore old value of FLOM_TRACE_MODULE */
#ifdef FLOM_TRACE_MODULE_SAVE
# undef FLOM_TRACE_MODULE
# define FLOM_TRACE_MODULE FLOM_TRACE_MODULE_SAVE
# undef FLOM_TRACE_MODULE_SAVE
#endif /* FLOM_TRACE_MODULE_SAVE */



#endif /* FLOM_RSRC_H */