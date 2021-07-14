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
#include "H5Iprivate.h"  /* IDs                                  */
#include "H5MMprivate.h" /* Memory management                    */
#include "H5Pprivate.h"  /* Property lists                       */
#include "H5PLprivate.h" /* Plugins                              */
#include "H5VLpkg.h"     /* Virtual Object Layer                 */

/* VOL connectors */
#include "H5VLpassthru.h" /* Pass-through VOL connector           */

/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

/* Information needed for iterating over the registered VOL connector hid_t IDs.
 * The name or value of the new VOL connector that is being registered is
 * stored in the name (or value) field and the found_id field is initialized to
 * H5I_INVALID_HID (-1).  If we find a VOL connector with the same name / value,
 * we set the found_id field to the existing ID for return to the function.
 */
typedef struct {
    /* IN */
    H5VL_get_connector_kind_t kind; /* Which kind of connector search to make */
    union {
        const char *       name;  /* The name of the VOL connector to check */
        H5VL_class_value_t value; /* The value of the VOL connector to check */
    } u;

    /* OUT */
    hid_t found_id; /* The connector ID, if we found a match */
} H5VL_get_connector_ud_t;

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/
static int H5VL__get_connector_cb(void *obj, hid_t id, void *_op_data);

/*********************/
/* Package Variables */
/*********************/

/* Default VOL connector */
H5VL_connector_prop_t H5VL_def_conn_g = {-1, NULL};

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5VL_connector_t struct */
H5FL_DEFINE_STATIC(H5VL_connector_t);

/*-------------------------------------------------------------------------
 * Function:    H5VL__conn_close
 *
 * Purpose:     Frees a file VOL class struct and returns an indication of
 *              success. This function is used as the free callback for the
 *              virtual object layer connectors.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__conn_close(H5VL_connector_t *connector, void H5_ATTR_UNUSED **request)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(connector);

    /* Decrement refcount on connector */
    if (H5VL__conn_dec_rc(connector) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to decrement ref count on VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__conn_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__set_def_conn
 *
 * Purpose:     Parses a string that contains the default VOL connector for
 *              the library.
 *
 * Note:	Usually from the environment variable "HDF5_VOL_CONNECTOR",
 *		but could be from elsewhere.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 * Programmer:  Jordan Henderson
 *              November 2018
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__set_def_conn(void)
{
    H5P_genplist_t *def_fapl;               /* Default file access property list */
    H5P_genclass_t *def_fapclass;           /* Default file access property class */
    const char *    env_var;                /* Environment variable for default VOL connector */
    char *          buf          = NULL;    /* Buffer for tokenizing string */
    hid_t           connector_id = -1;      /* VOL conntector ID */
    void *          vol_info     = NULL;    /* VOL connector info */
    herr_t          ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Reset default VOL connector, if it's set already */
    /* (Can happen during testing -QAK) */
    if (H5VL_def_conn_g.connector_id > 0)
        /* Release the default VOL connector */
        (void)H5VL_conn_prop_free(&H5VL_def_conn_g);

    /* Check for environment variable set */
    env_var = HDgetenv("HDF5_VOL_CONNECTOR");

    /* Only parse the string if it's set */
    if (env_var && *env_var) {
        char *      lasts = NULL;            /* Context pointer for strtok_r() call */
        const char *tok   = NULL;            /* Token from strtok_r call */
        htri_t      connector_is_registered; /* Whether connector is already registered */

        /* Duplicate the string to parse, as it is modified as we go */
        if (NULL == (buf = H5MM_strdup(env_var)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "can't allocate memory for environment variable string")

        /* Get the first 'word' of the environment variable.
         * If it's nothing (environment variable was whitespace) return error.
         */
        if (NULL == (tok = HDstrtok_r(buf, " \t\n\r", &lasts)))
            HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "VOL connector environment variable set empty?")

        /* First, check to see if the connector is already registered */
        if ((connector_is_registered = H5VL__is_connector_registered_by_name(tok)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't check if VOL connector already registered")
        else if (connector_is_registered) {
            /* Retrieve the ID of the already-registered VOL connector */
            if ((connector_id = H5VL__get_connector_id_by_name(tok, FALSE)) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector ID")
        } /* end else-if */
        else {
            /* Check for VOL connectors that ship with the library */
            if (!HDstrcmp(tok, "native")) {
                connector_id = H5VL_NATIVE;
                if (H5I_inc_ref(connector_id, FALSE) < 0)
                    HGOTO_ERROR(H5E_VOL, H5E_CANTINC, FAIL, "can't increment VOL connector refcount")
            } /* end if */
            else if (!HDstrcmp(tok, "pass_through")) {
                connector_id = H5VL_PASSTHRU;
                if (H5I_inc_ref(connector_id, FALSE) < 0)
                    HGOTO_ERROR(H5E_VOL, H5E_CANTINC, FAIL, "can't increment VOL connector refcount")
            } /* end else-if */
            else {
                /* Register the VOL connector */
                /* (NOTE: No provisions for vipl_id currently) */
                if ((connector_id = H5VL__register_connector_by_name(tok, H5P_VOL_INITIALIZE_DEFAULT)) < 0)
                    HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, FAIL, "can't register connector")
            } /* end else */
        }     /* end else */

        /* Was there any connector info specified in the environment variable? */
        if (NULL != (tok = HDstrtok_r(NULL, " \t\n\r", &lasts)))
            if (H5VL__connector_str_to_info(tok, connector_id, &vol_info) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTDECODE, FAIL, "can't deserialize connector info")

        /* Set the default VOL connector */
        H5VL_def_conn_g.connector_id   = connector_id;
        H5VL_def_conn_g.connector_info = vol_info;
    } /* end if */
    else {
        /* Set the default VOL connector */
        H5VL_def_conn_g.connector_id   = H5_DEFAULT_VOL;
        H5VL_def_conn_g.connector_info = NULL;

        /* Increment the ref count on the default connector */
        if (H5I_inc_ref(H5VL_def_conn_g.connector_id, FALSE) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTINC, FAIL, "can't increment VOL connector refcount")
    } /* end else */

    /* Get default file access pclass */
    if (NULL == (def_fapclass = (H5P_genclass_t *)H5I_object(H5P_FILE_ACCESS)))
        HGOTO_ERROR(H5E_VOL, H5E_BADID, FAIL, "can't find object for default file access property class ID")

    /* Change the default VOL for the default file access pclass */
    if (H5P_reset_vol_class(def_fapclass, &H5VL_def_conn_g) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL,
                    "can't set default VOL connector for default file access property class")

    /* Get default file access plist */
    if (NULL == (def_fapl = (H5P_genplist_t *)H5I_object(H5P_FILE_ACCESS_DEFAULT)))
        HGOTO_ERROR(H5E_VOL, H5E_BADID, FAIL, "can't find object for default fapl ID")

    /* Change the default VOL for the default FAPL */
    if (H5P_set_vol(def_fapl, H5VL_def_conn_g.connector_id, H5VL_def_conn_g.connector_info) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set default VOL connector for default FAPL")

done:
    /* Clean up on error */
    if (ret_value < 0) {
        if (vol_info)
            if (H5VL_free_connector_info(connector_id, vol_info) < 0)
                HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "can't free VOL connector info")
        if (connector_id >= 0)
            /* The H5VL_connector_t struct will be freed by this function */
            if (H5I_dec_ref(connector_id) < 0)
                HDONE_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to unregister VOL connector")
    } /* end if */

    /* Clean up */
    H5MM_xfree(buf);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__set_def_conn() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__conn_inc_rc
 *
 * Purpose:     Wrapper to increment the ref. count on a connector.
 *
 * Return:      Current ref. count (can't fail)
 *
 * Programmer:  Quincey Koziol
 *              February 23, 2019
 *
 *-------------------------------------------------------------------------
 */
size_t
H5VL__conn_inc_rc(H5VL_connector_t *connector)
{
    FUNC_ENTER_PACKAGE_NOERR

    /* Check arguments */
    HDassert(connector);

    /* Increment refcount for connector */
    connector->rc++;

    FUNC_LEAVE_NOAPI(connector->rc)
} /* end H5VL__conn_inc_rc() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__conn_dec_rc
 *
 * Purpose:     Wrapper to decrement the ref. count on a connector.
 *
 * Return:      Current ref. count (>=0) on success, <0 on failure
 *
 * Programmer:  Quincey Koziol
 *              February 23, 2019
 *
 *-------------------------------------------------------------------------
 */
ssize_t
H5VL__conn_dec_rc(H5VL_connector_t *connector)
{
    int64_t ret_value = -1; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    HDassert(connector);

    /* Decrement refcount for connector */
    connector->rc--;

    /* Check for last reference */
    if (0 == connector->rc) {
        if (H5VL__free_cls(connector->cls) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTFREE, FAIL, "unable to free VOL connector class")
        H5FL_FREE(H5VL_connector_t, connector);

        /* Set return value */
        ret_value = 0;
    } /* end if */
    else
        /* Set return value */
        ret_value = (ssize_t)connector->rc;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__conn_dec_rc() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__register_connector
 *
 * Purpose:     Registers a new VOL connector as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A VOL connector ID which is good until the
 *                          library is closed or the connector is unregistered.
 *
 *              Failure:    H5I_INVALID_HID
 *
 * Programmer:  Dana Robinson
 *              June 22, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__register_connector(const H5VL_class_t *cls, hid_t vipl_id)
{
    H5VL_connector_t *connector = NULL; /* Pointer to connector */
    H5VL_class_t *    new_cls   = NULL;
    hid_t             ret_value = H5I_INVALID_HID;

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    HDassert(cls);

    /* Allocate and initialize new VOL class struct */
    if (NULL == (new_cls = H5VL__new_cls(cls, vipl_id)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "can't create new VOL class struct")

    /* Create connector object */
    if (NULL == (connector = H5FL_MALLOC(H5VL_connector_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, H5I_INVALID_HID,
                    "memory allocation failed for VOL connector struct")

    /* Initialize connector */
    connector->rc  = 1;
    connector->cls = new_cls;

    /* Create the new class ID */
    if ((ret_value = H5I_register(H5I_VOL, connector, TRUE)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register VOL connector ID")

done:
    if (ret_value < 0) {
        if (new_cls && H5VL__free_cls(new_cls) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, H5I_INVALID_HID, "can't free VOL class struct")
        if (connector)
            H5FL_FREE(H5VL_connector_t, connector);
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__register_connector() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_connector_cb
 *
 * Purpose:     Callback routine to search through registered VOLs
 *
 * Return:      Success:    H5_ITER_STOP if the class and op_data name
 *                          members match. H5_ITER_CONT otherwise.
 *              Failure:    Can't fail
 *
 * Programmer:  Dana Robinson
 *              June 22, 2017
 *
 *-------------------------------------------------------------------------
 */
static int
H5VL__get_connector_cb(void *obj, hid_t id, void *_op_data)
{
    H5VL_get_connector_ud_t *op_data   = (H5VL_get_connector_ud_t *)_op_data; /* User data for callback */
    H5VL_connector_t *       connector = (H5VL_connector_t *)obj;
    int                      ret_value = H5_ITER_CONT; /* Callback return value */

    FUNC_ENTER_STATIC_NOERR

    if (H5VL_GET_CONNECTOR_BY_NAME == op_data->kind) {
        if (0 == HDstrcmp(connector->cls->name, op_data->u.name)) {
            op_data->found_id = id;
            ret_value         = H5_ITER_STOP;
        } /* end if */
    }     /* end if */
    else {
        HDassert(H5VL_GET_CONNECTOR_BY_VALUE == op_data->kind);
        if (connector->cls->value == op_data->u.value) {
            op_data->found_id = id;
            ret_value         = H5_ITER_STOP;
        } /* end if */
    }     /* end else */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_connector_cb() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__register_connector_by_class
 *
 * Purpose:     Registers a new VOL connector as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A VOL connector ID which is good until the
 *                          library is closed or the connector is
 *                          unregistered.
 *
 *              Failure:    H5I_INVALID_HID
 *
 * Programmer:  Dana Robinson
 *              June 22, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__register_connector_by_class(const H5VL_class_t *cls, hid_t vipl_id)
{
    H5VL_get_connector_ud_t op_data;                     /* Callback info for connector search */
    hid_t                   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    if (!cls)
        HGOTO_ERROR(H5E_ARGS, H5E_UNINITIALIZED, H5I_INVALID_HID,
                    "VOL connector class pointer cannot be NULL")
    if (H5VL_VERSION != cls->version)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "VOL connector has incompatible version")
    if (!cls->name)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID,
                    "VOL connector class name cannot be the NULL pointer")
    if (0 == HDstrlen(cls->name))
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID,
                    "VOL connector class name cannot be the empty string")
    if (cls->info_cls.copy && !cls->info_cls.free)
        HGOTO_ERROR(
            H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID,
            "VOL connector must provide free callback for VOL info objects when a copy callback is provided")
    if (cls->wrap_cls.get_wrap_ctx && !cls->wrap_cls.free_wrap_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID,
                    "VOL connector must provide free callback for object wrapping contexts when a get "
                    "callback is provided")

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_NAME;
    op_data.u.name   = cls->name;
    op_data.found_id = H5I_INVALID_HID;

    /* Check if connector is already registered */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL IDs")

    /* Increment the ref count on the existing VOL connector ID, if it's already registered */
    if (op_data.found_id != H5I_INVALID_HID) {
        if (H5I_inc_ref(op_data.found_id, TRUE) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTINC, H5I_INVALID_HID,
                        "unable to increment ref count on VOL connector")
        ret_value = op_data.found_id;
    } /* end if */
    else {
        /* Create a new class ID */
        if ((ret_value = H5VL__register_connector(cls, vipl_id)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register VOL connector")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__register_connector_by_class() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__register_connector_by_name
 *
 * Purpose:     Registers a new VOL connector as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A VOL connector ID which is good until the
 *                          library is closed or the connector is
 *                          unregistered.
 *
 *              Failure:    H5I_INVALID_HID
 *
 * Programmer:  Dana Robinson
 *              June 22, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__register_connector_by_name(const char *name, hid_t vipl_id)
{
    H5VL_get_connector_ud_t op_data;                     /* Callback info for connector search */
    hid_t                   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_NAME;
    op_data.u.name   = name;
    op_data.found_id = H5I_INVALID_HID;

    /* Check if connector is already registered */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL ids")

    /* If connector alread registered, increment ref count on ID and return ID */
    if (op_data.found_id != H5I_INVALID_HID) {
        if (H5I_inc_ref(op_data.found_id, TRUE) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTINC, H5I_INVALID_HID,
                        "unable to increment ref count on VOL connector")
        ret_value = op_data.found_id;
    } /* end if */
    else {
        H5PL_key_t          key;
        const H5VL_class_t *cls;

        /* Try loading the connector */
        key.vol.kind   = H5VL_GET_CONNECTOR_BY_NAME;
        key.vol.u.name = name;
        if (NULL == (cls = (const H5VL_class_t *)H5PL_load(H5PL_TYPE_VOL, &key)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, H5I_INVALID_HID, "unable to load VOL connector")

        /* Register the connector we loaded */
        if ((ret_value = H5VL__register_connector(cls, vipl_id)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register VOL connector ID")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__register_connector_by_name() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__register_connector_by_value
 *
 * Purpose:     Registers a new VOL connector as a member of the virtual object
 *              layer class.
 *
 * Return:      Success:    A VOL connector ID which is good until the
 *                          library is closed or the connector is
 *                          unregistered.
 *
 *              Failure:    H5I_INVALID_HID
 *
 * Programmer:  Dana Robinson
 *              June 22, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__register_connector_by_value(H5VL_class_value_t value, hid_t vipl_id)
{
    H5VL_get_connector_ud_t op_data;                     /* Callback info for connector search */
    hid_t                   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_VALUE;
    op_data.u.value  = value;
    op_data.found_id = H5I_INVALID_HID;

    /* Check if connector is already registered */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL ids")

    /* If connector alread registered, increment ref count on ID and return ID */
    if (op_data.found_id != H5I_INVALID_HID) {
        if (H5I_inc_ref(op_data.found_id, TRUE) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTINC, H5I_INVALID_HID,
                        "unable to increment ref count on VOL connector")
        ret_value = op_data.found_id;
    } /* end if */
    else {
        H5PL_key_t          key;
        const H5VL_class_t *cls;

        /* Try loading the connector */
        key.vol.kind    = H5VL_GET_CONNECTOR_BY_VALUE;
        key.vol.u.value = value;
        if (NULL == (cls = (const H5VL_class_t *)H5PL_load(H5PL_TYPE_VOL, &key)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, H5I_INVALID_HID, "unable to load VOL connector")

        /* Register the connector we loaded */
        if ((ret_value = H5VL__register_connector(cls, vipl_id)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register VOL connector ID")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__register_connector_by_value() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__is_connector_registered_by_name
 *
 * Purpose:     Checks if a connector with a particular name is registered.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 * Programmer:  Dana Robinson
 *              June 17, 2017
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5VL__is_connector_registered_by_name(const char *name)
{
    H5VL_get_connector_ud_t op_data;           /* Callback info for connector search */
    htri_t                  ret_value = FALSE; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_NAME;
    op_data.u.name   = name;
    op_data.found_id = H5I_INVALID_HID;

    /* Find connector with name */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, FAIL, "can't iterate over VOL connectors")

    /* Found a connector with that name */
    if (op_data.found_id != H5I_INVALID_HID)
        ret_value = TRUE;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__is_connector_registered_by_name() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__is_connector_registered_by_value
 *
 * Purpose:     Checks if a connector with a particular value (ID) is
 *              registered.
 *
 * Return:      Success:    0
 *              Failure:    -1
 *
 *-------------------------------------------------------------------------
 */
htri_t
H5VL__is_connector_registered_by_value(H5VL_class_value_t value)
{
    H5VL_get_connector_ud_t op_data;           /* Callback info for connector search */
    htri_t                  ret_value = FALSE; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_VALUE;
    op_data.u.value  = value;
    op_data.found_id = H5I_INVALID_HID;

    /* Find connector with value */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, FAIL, "can't iterate over VOL connectors")

    /* Found a connector with that name */
    if (op_data.found_id != H5I_INVALID_HID)
        ret_value = TRUE;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__is_connector_registered_by_value() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_connector_id
 *
 * Purpose:     Retrieves the VOL connector ID for a given object ID.
 *
 * Return:      Positive if the VOL class has been registered
 *              Negative on error (if the class is not a valid class or not registered)
 *
 * Programmer:  Dana Robinson
 *              June 17, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__get_connector_id(hid_t obj_id, hbool_t is_api)
{
    H5VL_object_t *vol_obj   = NULL;
    hid_t          ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Get the underlying VOL object for the object ID */
    if (NULL == (vol_obj = H5VL_vol_object(obj_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5I_INVALID_HID, "invalid location identifier")

    /* Return the VOL object's VOL class ID */
    ret_value = vol_obj->container->conn_prop.connector_id;
    if (H5I_inc_ref(ret_value, is_api) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTINC, H5I_INVALID_HID, "unable to increment ref count on VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_connector_id() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_connector_id_by_name
 *
 * Purpose:     Retrieves the ID for a registered VOL connector.
 *
 * Return:      Positive if the VOL class has been registered
 *              Negative on error (if the class is not a valid class or not registered)
 *
 * Programmer:  Dana Robinson
 *              June 17, 2017
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__get_connector_id_by_name(const char *name, hbool_t is_api)
{
    hid_t ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Find connector with name */
    if ((ret_value = H5VL__peek_connector_id_by_name(name)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't find VOL connector")

    /* Found a connector with that name */
    if (H5I_inc_ref(ret_value, is_api) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTINC, H5I_INVALID_HID, "unable to increment ref count on VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_connector_id_by_name() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_connector_id_by_value
 *
 * Purpose:     Retrieves the ID for a registered VOL connector.
 *
 * Return:      Positive if the VOL class has been registered
 *              Negative on error (if the class is not a valid class or
 *                not registered)
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__get_connector_id_by_value(H5VL_class_value_t value, hbool_t is_api)
{
    hid_t ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Find connector with value */
    if ((ret_value = H5VL__peek_connector_id_by_value(value)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't find VOL connector")

    /* Found a connector with that value */
    if (H5I_inc_ref(ret_value, is_api) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTINC, H5I_INVALID_HID, "unable to increment ref count on VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_connector_id_by_value() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__peek_connector_id_by_name
 *
 * Purpose:     Retrieves the ID for a registered VOL connector.  Does not
 *              increment the ref count
 *
 * Return:      Positive if the VOL class has been registered
 *              Negative on error (if the class is not a valid class or
 *                not registered)
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__peek_connector_id_by_name(const char *name)
{
    H5VL_get_connector_ud_t op_data;                     /* Callback info for connector search */
    hid_t                   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_NAME;
    op_data.u.name   = name;
    op_data.found_id = H5I_INVALID_HID;

    /* Find connector with name */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL connectors")

    /* Set return value */
    ret_value = op_data.found_id;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__peek_connector_id_by_name() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__peek_connector_id_by_value
 *
 * Purpose:     Retrieves the ID for a registered VOL connector.  Does not
 *              increment the ref count
 *
 * Return:      Positive if the VOL class has been registered
 *              Negative on error (if the class is not a valid class or
 *                not registered)
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL__peek_connector_id_by_value(H5VL_class_value_t value)
{
    H5VL_get_connector_ud_t op_data;                     /* Callback info for connector search */
    hid_t                   ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Set up op data for iteration */
    op_data.kind     = H5VL_GET_CONNECTOR_BY_VALUE;
    op_data.u.value  = value;
    op_data.found_id = H5I_INVALID_HID;

    /* Find connector with value */
    if (H5I_iterate(H5I_VOL, H5VL__get_connector_cb, &op_data, TRUE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_BADITER, H5I_INVALID_HID, "can't iterate over VOL connectors")

    /* Set return value */
    ret_value = op_data.found_id;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__peek_connector_id_by_value() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_connector_name
 *
 * Purpose:     Private version of H5VLget_connector_name
 *
 * Return:      Success:        The length of the connector name
 *              Failure:        Negative
 *
 *-------------------------------------------------------------------------
 */
ssize_t
H5VL__get_connector_name(hid_t id, char *name /*out*/, size_t size)
{
    H5VL_object_t *     vol_obj;
    const H5VL_class_t *cls;
    size_t              len;
    ssize_t             ret_value = -1;

    FUNC_ENTER_PACKAGE

    /* get the object pointer */
    if (NULL == (vol_obj = H5VL_vol_object(id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "invalid VOL identifier")

    cls = vol_obj->container->connector->cls;

    len = HDstrlen(cls->name);
    if (name) {
        HDstrncpy(name, cls->name, MIN(len + 1, size));
        if (len >= size)
            name[size - 1] = '\0';
    } /* end if */

    /* Set the return value for the API call */
    ret_value = (ssize_t)len;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_connector_name() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__cmp_connector_cls
 *
 * Purpose:     Compare VOL class for a connector
 *
 * Note:        Sets *cmp_value positive if VALUE1 is greater than VALUE2,
 *		negative if VALUE2 is greater than VALUE1, and zero if VALUE1
 *              and VALUE2 are equal (like strcmp).
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__cmp_connector_cls(int *cmp_value, const H5VL_class_t *cls1, const H5VL_class_t *cls2)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity checks */
    HDassert(cls1);
    HDassert(cls2);

    /* If the pointers are the same the classes are the same */
    if (cls1 == cls2) {
        *cmp_value = 0;
        HGOTO_DONE(SUCCEED);
    } /* end if */

    /* Compare connector "values" */
    if (cls1->value < cls2->value) {
        *cmp_value = -1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    if (cls1->value > cls2->value) {
        *cmp_value = 1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    HDassert(cls1->value == cls2->value);

    /* Compare connector names */
    if (cls1->name == NULL && cls2->name != NULL) {
        *cmp_value = -1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    if (cls1->name != NULL && cls2->name == NULL) {
        *cmp_value = 1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    if (0 != (*cmp_value = HDstrcmp(cls1->name, cls2->name)))
        HGOTO_DONE(SUCCEED)

    /* Compare connector VOL API versions */
    if (cls1->version < cls2->version) {
        *cmp_value = -1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    if (cls1->version > cls2->version) {
        *cmp_value = 1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    HDassert(cls1->version == cls2->version);

    /* Compare connector info */
    if (cls1->info_cls.size < cls2->info_cls.size) {
        *cmp_value = -1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    if (cls1->info_cls.size > cls2->info_cls.size) {
        *cmp_value = 1;
        HGOTO_DONE(SUCCEED)
    } /* end if */
    HDassert(cls1->info_cls.size == cls2->info_cls.size);

    /* Set comparison value to 'equal' */
    *cmp_value = 0;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__cmp_connector_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_cmp_connector
 *
 * Purpose:     Compare VOL class for a connector
 *
 * Note:        Sets *cmp_value positive if VALUE1 is greater than VALUE2,
 *		negative if VALUE2 is greater than VALUE1, and zero if VALUE1
 *              and VALUE2 are equal (like strcmp).
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_cmp_connector(int *cmp_value, const H5VL_connector_t *conn1, const H5VL_connector_t *conn2)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(conn1);
    HDassert(conn2);

    /* Invoke class comparison routine */
    if (H5VL__cmp_connector_cls(cmp_value, conn1->cls, conn2->cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector classes")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_cmp_connector() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_cmp_connector_info
 *
 * Purpose:     Compare VOL class info for a connector
 *
 * Note:        Sets *cmp_value positive if INFO1 is greater than INFO2,
 *		negative if INFO2 is greater than INFO1, and zero if INFO1
 *              and INFO2 are equal (like strcmp).
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_cmp_connector_info(const H5VL_connector_t *connector, int *cmp_value, const void *info1,
                        const void *info2)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(connector);
    HDassert(cmp_value);

    /* Invoke class info comparison routine */
    if (H5VL__cmp_connector_info_cls(connector->cls, cmp_value, info1, info2) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector class info")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_cmp_connector_info() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__is_default_conn
 *
 * Purpose:     Check if the default connector will be used for a container.
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
void
H5VL__is_default_conn(hid_t fapl_id, hid_t connector_id, hbool_t *is_default)
{
    FUNC_ENTER_PACKAGE_NOERR

    /* Sanity checks */
    HDassert(is_default);

    /* Determine if the default VOL connector will be used, based on non-default
     * values in the FAPL, connector ID, or the HDF5_VOL_CONNECTOR environment
     * variable being set.
     */
    *is_default = (H5VL_def_conn_g.connector_id == H5_DEFAULT_VOL) &&
                  ((H5P_FILE_ACCESS_DEFAULT == fapl_id) || connector_id == H5_DEFAULT_VOL);

    FUNC_LEAVE_NOAPI_VOID
} /* end H5VL__is_default_conn() */
