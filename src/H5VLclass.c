/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://www.hdfgroup.org/licenses.               *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:     The Virtual Object Layer as described in documentation.
 *              The purpose is to provide an abstraction on how to access the
 *              underlying HDF5 container, whether in a local file with
 *              a specific file format, or remotely on other machines, etc...
 */

/****************/
/* Module Setup */
/****************/

#include "H5VLmodule.h" /* This source code file is part of the H5VL module */

/***********/
/* Headers */
/***********/

#include "H5private.h"   /* Generic Functions                    */
#include "H5Eprivate.h"  /* Error handling                       */
#include "H5FLprivate.h" /* Free lists                           */
#include "H5MMprivate.h" /* Memory management                    */
#include "H5PLprivate.h" /* Plugins                              */
#include "H5VLpkg.h"     /* Virtual Object Layer                 */

/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5VL_class_t struct */
H5FL_DEFINE_STATIC(H5VL_class_t);

/*-------------------------------------------------------------------------
 * Function:    H5VL__new_cls
 *
 * Purpose:     Allocates and initializes a new VOL class struct.
 *
 * Return:      Success:    A pointer to a valid & initialized VOL class
 *              Failure:    NULL
 *
 * Programmer:  Quincey Koziol
 *              July 13, 2021
 *
 *-------------------------------------------------------------------------
 */
H5VL_class_t *
H5VL__new_cls(const H5VL_class_t *cls, hid_t vipl_id)
{
    H5VL_class_t *new_cls     = NULL;   /* Newly created VOL class */
    H5VL_class_t *ret_value = NULL;     /* Return value */

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    HDassert(cls);

    /* Copy the class structure so the caller can reuse or free it */
    if (NULL == (new_cls = H5FL_MALLOC(H5VL_class_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "memory allocation failed for VOL connector class struct")
    H5MM_memcpy(new_cls, cls, sizeof(*cls));
    if (NULL == (new_cls->name = H5MM_strdup(cls->name)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "memory allocation failed for VOL connector name")

    /* Initialize the VOL connector */
    if (new_cls->initialize && new_cls->initialize(vipl_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "unable to init VOL connector")

    /* Set return value */
    ret_value = new_cls;

done:
    if (NULL == ret_value) {
        if (new_cls) {
            if (new_cls->name)
                H5MM_xfree_const(new_cls->name);
            H5FL_FREE(H5VL_class_t, new_cls);
        } /* end if */
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__new_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__free_cls
 *
 * Purpose:     Frees a file VOL class struct.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__free_cls(H5VL_class_t *cls)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(cls);

    /* Shut down the VOL connector */
    if (cls->terminate && cls->terminate() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "VOL connector did not terminate cleanly")

    /* Release the class */
    H5MM_xfree_const(cls->name);
    H5FL_FREE(H5VL_class_t, cls);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__free_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_check_plugin_load
 *
 * Purpose:     Check if a VOL connector matches the search criteria, and
 *              can be loaded.
 *
 * Note:        Matching the connector's name / value, but the connector
 *              having an incompatible version is not an error, but means
 *              that the connector isn't a "match".  Setting the SUCCEED
 *              value to FALSE and not failing for that case allows the
 *              plugin framework to keep looking for other DLLs that match
 *              and have a compatible version.
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_check_plugin_load(const H5VL_class_t *cls, const H5PL_key_t *key, hbool_t *success)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(cls);
    HDassert(key);
    HDassert(success);

    /* Which kind of key are we looking for? */
    if (key->vol.kind == H5VL_GET_CONNECTOR_BY_NAME) {
        /* Check if plugin name matches VOL connector class name */
        if (cls->name && !HDstrcmp(cls->name, key->vol.u.name))
            *success = TRUE;
    } /* end if */
    else {
        /* Sanity check */
        HDassert(key->vol.kind == H5VL_GET_CONNECTOR_BY_VALUE);

        /* Check if plugin value matches VOL connector class value */
        if (cls->value == key->vol.u.value)
            *success = TRUE;
    } /* end else */

    /* Connector is a match, but might not be a compatible version */
    if (*success && cls->version != H5VL_VERSION)
        *success = FALSE;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_check_plugin_load() */

