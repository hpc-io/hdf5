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
#include "H5Fprivate.h"  /* Files                                */
#include "H5FLprivate.h" /* Free lists                           */
#include "H5Iprivate.h"  /* IDs                                  */
#include "H5Oprivate.h"  /* Object headers                       */
#include "H5Pprivate.h"  /* Property lists                       */
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

/* Declare a free list to manage the H5VL_container_t struct */
H5FL_DEFINE_STATIC(H5VL_container_t);

/*-------------------------------------------------------------------------
 * Function:    H5VL_create_container
 *
 * Purpose:     Creates a new VOL container for accessing an HDF5 container.
 *
 * Return:      Success:    A valid VOL container
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_container_t *
H5VL_create_container(void *object, H5VL_connector_t *connector, H5VL_connector_prop_t *conn_prop)
{
    H5VL_container_t *new_container = NULL; /* Pointer to new container object */
    H5VL_container_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Check arguments */
    HDassert(connector);
    HDassert(conn_prop);

    /* Set up VOL object for the passed-in data */
    if (NULL == (new_container = H5FL_CALLOC(H5VL_container_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "can't allocate memory for VOL object")

    /* Set non-zero fields */
    new_container->connector = connector;
    new_container->object    = object;

    /* Make copy of VOL connector ID & info */
    HDmemcpy(&new_container->conn_prop, conn_prop, sizeof(*conn_prop));
    if (H5VL_conn_prop_copy(&new_container->conn_prop) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, NULL, "can't copy VOL connector")

    /* Bump the reference count on the VOL connector */
    H5VL__conn_inc_rc(new_container->connector);

    /* Set return value */
    ret_value = new_container;

done:
    /* Clean up on error */
    if(NULL == ret_value)
        if(new_container)
            new_container = H5FL_FREE(H5VL_container_t, new_container);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_create_container() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_inc_rc
 *
 * Purpose:     Wrapper to increment the ref. count on a container.
 *
 * Return:      Current ref. count (can't fail)
 *
 * Programmer:  Quincey Koziol
 *              February 23, 2019
 *
 *-------------------------------------------------------------------------
 */
size_t
H5VL_container_inc_rc(H5VL_container_t *container)
{
    size_t ret_value = 0;       /* Return value */

    FUNC_ENTER_NOAPI(0)

    /* Check arguments */
    HDassert(container);

    /* Increment refcount for container */
    ret_value = ++container->rc;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_container_inc_rc() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_dec_rc
 *
 * Purpose:     Wrapper to decrement the ref. count on a container.
 *
 * Return:      Current ref. count (>=0) on success, <0 on failure
 *
 * Programmer:  Quincey Koziol
 *              February 23, 2019
 *
 *-------------------------------------------------------------------------
 */
ssize_t
H5VL_container_dec_rc(H5VL_container_t *container)
{
    ssize_t ret_value = (ssize_t)-1; /* Return value */

    FUNC_ENTER_NOAPI(-1)

    /* Check arguments */
    HDassert(container);

    /* Decrement refcount for container */
    container->rc--;

    /* Check for last reference */
    if (0 == container->rc) {
        if (H5VL__conn_dec_rc(container->connector) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to decrement ref count on VOL connector")
        if (H5VL_conn_prop_free(&container->conn_prop) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTFREE, FAIL, "unable to free VOL connector property")
        H5FL_FREE(H5VL_container_t, container);

        /* Set return value */
        ret_value = 0;
    } /* end if */
    else
        /* Set return value */
        ret_value = (ssize_t)container->rc;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_container_dec_rc() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_get
 *
 * Purpose:     Performs a file 'get' callback, using the container info
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_container_get(H5VL_container_t *container, H5VL_file_get_args_t *args)
{
    H5VL_object_t tmp_vol_obj;                  /* Temporary VOL object for re-opening the file */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(container);
    HDassert(args);

    /* Set up temporary file VOL object */
    tmp_vol_obj.obj_type = H5VL_OBJ_FILE;
    tmp_vol_obj.object = NULL;
    tmp_vol_obj.container = container;

    /* Call the corresponding internal VOL routine */
    if (H5VL_file_get(&tmp_vol_obj, args, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "file 'get' operation failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_container_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_specific
 *
 * Purpose:     Performs a file 'specific' callback, using the container info
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_container_specific(H5VL_container_t *container, H5VL_file_specific_args_t *args,
    void **request)
{
    H5VL_object_t tmp_vol_obj;                  /* Temporary VOL object for re-opening the file */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(container);
    HDassert(args);

    /* Set up temporary file VOL object */
    tmp_vol_obj.obj_type = H5VL_OBJ_FILE;
    tmp_vol_obj.object = NULL;
    tmp_vol_obj.container = container;

    /* Call the corresponding internal VOL routine */
    if (H5VL_file_specific(&tmp_vol_obj, args, H5P_DATASET_XFER_DEFAULT, request) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file 'specific' operation failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_container_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_is_native
 *
 * Purpose:     Query if a container will use the native VOL connector.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_container_is_native(const H5VL_container_t *container, H5VL_get_conn_lvl_t lvl, hbool_t *is_native)
{
    const H5VL_class_t *cls;                 /* VOL connector class structs for object */
    const H5VL_connector_t *native_connector;          /* Native VOL connector class structs */
    int                 cmp_value;           /* Comparison result */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(container);
    HDassert(is_native);

    /* Retrieve the terminal connector class for the object */
    cls = NULL;
    if (H5VL__introspect_get_conn_cls(container->object, container->connector->cls, lvl, &cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector class")

    /* Retrieve the native connector */
    if (NULL == (native_connector = H5I_object_verify(H5VL_NATIVE, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't retrieve native VOL connector class")

    /* Compare connector classes */
    if (H5VL__cmp_connector_cls(&cmp_value, cls, native_connector->cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector classes")

    /* If classes compare equal, then the object is / is in a native connector's file */
    *is_native = (cmp_value == 0);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_container_is_native() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_container_object
 *
 * Purpose:     Correctly retrieve the 'object' field for a VOL container,
 *              even for nested / stacked VOL connectors.
 *
 * Return:      Success:        Unwrapped object pointer
 *              Failure:        NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL_container_object(const H5VL_container_t *container)
{
    void *ret_value = NULL;     /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Check for 'get_object' callback in connector */
    if (container->connector->cls->wrap_cls.get_object)
        ret_value = (container->connector->cls->wrap_cls.get_object)(container->object);
    else
        ret_value = container->object;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__container_object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_file_is_same_as_container
 *
 * Purpose:     Query if a file and a container are the same.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_is_same_as_container(const H5VL_object_t *vol_obj, const H5VL_container_t *container,
                                hbool_t *same_file)
{
    const H5VL_class_t *cls1;                /* VOL connector class struct for first object */
    const H5VL_class_t *cls2;                /* VOL connector class struct for second object */
    int                 cmp_value;           /* Comparison result */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(vol_obj);
    HDassert(container);
    HDassert(same_file);

    /* Retrieve the terminal connectors for each object */
    cls1 = NULL;
    if (H5VL_introspect_get_conn_cls(vol_obj, H5VL_GET_CONN_LVL_TERM, &cls1) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector class")
    cls2 = NULL;
    if (H5VL__introspect_get_conn_cls(container->object, container->connector->cls, H5VL_GET_CONN_LVL_TERM, &cls2) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector class")

    /* Compare connector classes */
    if (H5VL__cmp_connector_cls(&cmp_value, cls1, cls2) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector classes")

    /* If the connector classes are different, the files are different */
    if (cmp_value)
        *same_file = FALSE;
    else {
        void *                    obj2;        /* Terminal object for second file */
        H5VL_file_specific_args_t vol_cb_args; /* Arguments to VOL callback */

        /* Get unwrapped (terminal) object for container */
        if (NULL == (obj2 = H5VL_container_object(container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get unwrapped object")

        /* Set up VOL callback arguments */
        vol_cb_args.op_type                 = H5VL_FILE_IS_EQUAL;
        vol_cb_args.args.is_equal.obj2      = obj2;
        vol_cb_args.args.is_equal.same_file = same_file;

        /* Make 'are files equal' callback */
        if (H5VL_file_specific(vol_obj, &vol_cb_args, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file specific failed")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_is_same_as_container() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_container_for_obj
 *
 * Purpose:     Determines if an object is opened through an external link
 *              and creates a new container for those that are.and creates a new container for ha
 *
 * Note:        Relies on the assumption that external links can only be opened
 *              when the VOL connector stack is the trivial "native only"
 *              connector stack.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
H5VL_container_t *
H5VL__get_container_for_obj(void *obj, H5I_type_t obj_type, H5VL_container_t *orig_container)
{
    hbool_t is_native = FALSE;
    H5VL_container_t *ret_value = NULL;         /* Return value */

    FUNC_ENTER_PACKAGE

    /* Check if current container is using the "native only" connector stack */
    if (H5VL_container_is_native(orig_container, H5VL_GET_CONN_LVL_CURR, &is_native) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't query about native VOL connector")
    if(is_native) {
        H5F_t *f;               /* File pointer for object */

        /* Retrieve the native file pointer */
        if (NULL == (f = H5O_fileof(obj, obj_type)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't retrieve native file pointer for object")

        /* Check if the object is not in the same file as the container used to open it */
        if (f != orig_container->object) {
            H5VL_connector_t *native_connector;     /* Native VOL connector class structs */
            H5VL_connector_prop_t conn_prop = {H5VL_NATIVE, NULL};      /* Connector property for native VOL connector */

            /* Retrieve the native connector */
            if (NULL == (native_connector = H5I_object_verify(H5VL_NATIVE, H5I_VOL)))
                HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't retrieve native VOL connector")

            /* Create new container for externally-linked object */
            if (NULL == (ret_value = H5VL_create_container(f, native_connector, &conn_prop)))
                HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL container for external object")
        } /* end if */
        else
            ret_value = orig_container;
    } /* end if */
    else
        ret_value = orig_container;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__get_container_for_obj() */


