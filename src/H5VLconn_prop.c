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
#include "H5Iprivate.h"  /* IDs                                  */
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

/*-------------------------------------------------------------------------
 * Function:    H5VL_conn_prop_copy
 *
 * Purpose:     Copy VOL connector ID & info.
 *
 * Note:        This is an "in-place" copy.
 *
 * Return:      Success:        Non-negative
 *              Failure:        Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_conn_prop_copy(H5VL_connector_prop_t *connector_prop)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    if (connector_prop) {
        /* Copy the connector ID & info, if there is one */
        if (connector_prop->connector_id > 0) {
            /* Increment the reference count on connector ID and copy connector info */
            if (H5I_inc_ref(connector_prop->connector_id, FALSE) < 0)
                HGOTO_ERROR(H5E_PLIST, H5E_CANTINC, FAIL, "unable to increment ref count on VOL connector ID")

            /* Copy connector info, if it exists */
            if (connector_prop->connector_info) {
                H5VL_connector_t *connector;                 /* Pointer to connector */
                void *        new_connector_info = NULL; /* Copy of connector info */

                /* Retrieve the connector for the ID */
                if (NULL == (connector = H5I_object(connector_prop->connector_id)))
                    HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "not a VOL connector ID")

                /* Allocate and copy connector info */
                if (H5VL_copy_connector_info(connector, &new_connector_info, connector_prop->connector_info) < 0)
                    HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, FAIL, "connector info copy failed")

                /* Set the connector info to the copy */
                connector_prop->connector_info = new_connector_info;
            } /* end if */
        }     /* end if */
    }         /* end if */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_conn_prop_copy() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_conn_prop_free
 *
 * Purpose:     Free VOL connector property's ID & info.
 *
 * Return:      Success:        Non-negative
 *              Failure:        Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_conn_prop_free(H5VL_connector_prop_t *connector_prop)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    if (connector_prop) {
        /* Free the connector info (if it exists) and decrement the ID */
        if (connector_prop->connector_id > 0) {
            if (connector_prop->connector_info) {
                /* Free the connector info */
                if (H5VL_free_connector_info(connector_prop->connector_id, connector_prop->connector_info) < 0)
                    HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL connector info object")
                connector_prop->connector_info = NULL;
            } /* end if */

            /* Decrement reference count for connector ID */
            if (H5I_dec_ref(connector_prop->connector_id) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "can't decrement reference count for connector ID")
            connector_prop->connector_id = H5I_INVALID_HID;
        }
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_conn_prop_free() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_get_cap_flags
 *
 * Purpose:     Query capability flags for connector property.
 *
 * Return:      Success:        Non-negative
 *              Failure:        Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_get_cap_flags(const H5VL_connector_prop_t *connector_prop, unsigned *cap_flags)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(connector_prop);

    /* Copy the connector ID & info, if there is one */
    if (connector_prop->connector_id > 0) {
        H5VL_connector_t *connector; /* Pointer to connector */

        /* Retrieve the connector for the ID */
        if (NULL == (connector = (H5VL_connector_t *)H5I_object(connector_prop->connector_id)))
            HGOTO_ERROR(H5E_PLIST, H5E_BADTYPE, FAIL, "not a VOL connector ID")

        /* Query the connector's capability flags */
        if (H5VL_introspect_get_cap_flags(connector_prop->connector_info, connector->cls, cap_flags) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector's capability flags")
    } /* end if */
    else
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "connector ID not set?")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_get_cap_flags() */
