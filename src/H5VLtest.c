/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------
 *
 * Created:		H5VLtest.c
 *			Jan  3 2021
 *			Quincey Koziol
 *
 * Purpose:		Virtual Object Layer (VOL) testing routines.
 *
 *-------------------------------------------------------------------------
 */

/****************/
/* Module Setup */
/****************/

#include "H5VLmodule.h" /* This source code file is part of the H5VL module */
#define H5VL_TESTING    /* Suppress warning about H5VL testing funcs        */

/***********/
/* Headers */
/***********/
#include "H5private.h"  /* Generic Functions                        */
#include "H5Eprivate.h" /* Error handling                           */
#include "H5Fprivate.h" /* Files                                    */
#include "H5Iprivate.h" /* IDs                                      */
#include "H5Pprivate.h" /* Property lists                           */
#include "H5VLpkg.h"    /* Virtual Object Layer                     */

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
 * Function:    H5VL__reparse_def_vol_conn_variable_test
 *
 * Purpose:     Re-parse the default VOL connector environment variable.
 *
 *              Since getenv(3) is fairly expensive, we only parse it once,
 *              when the library opens. This test function is used to
 *              re-parse the environment variable after we've changed it
 *              with setenv(3).
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *              Feb 3, 2021
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__reparse_def_vol_conn_variable_test(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Re-check for the HDF5_VOL_CONNECTOR environment variable */
    if (H5VL__set_def_conn() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize default VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__reparse_def_vol_conn_variable_test() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__register_using_vol_id
 *
 * Purpose:     Utility function to create an object ID for a fake VOL object.
 *
 * Return:      Success:    A valid HDF5 ID
 *              Failure:    H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__register_using_vol_id(H5VL_obj_type_t obj_type, void *obj, hid_t connector_id)
{
    H5VL_container_t *container = NULL; /* Temporary VOL container for object */
    H5VL_connector_t *connector;        /* VOL connector */
    H5VL_connector_prop_t conn_prop;    /* VOL connector property */
    hid_t   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Initialize the connector property */
    conn_prop.connector_id = connector_id;
    conn_prop.connector_info = NULL;

    /* Get the connector */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, H5I_INVALID_HID, "not a VOL connector ID")

    /* Create container, if file object */
    if (H5VL_OBJ_FILE == obj_type) {
        /* Create fake container for the object */
        if (NULL == (container = H5VL_create_container(obj, connector, &conn_prop)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "VOL container create failed")

        /* Get an ID for the object */
        if ((ret_value = H5VL_register(obj_type, NULL, container, TRUE)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to get an ID for the object")
    } /* end if */
    else {
        /* Create fake container for the object */
        if (NULL == (container = H5VL_create_container(NULL, connector, &conn_prop)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "VOL container create failed")

        /* Get an ID for the object */
        if ((ret_value = H5VL_register(obj_type, obj, container, TRUE)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to get an ID for the object")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__register_using_vol_id() */

