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
#include "H5CXprivate.h" /* API Contexts                         */
#include "H5Eprivate.h"  /* Error handling                       */
#include "H5FLprivate.h" /* Free lists                           */
#include "H5Iprivate.h"  /* IDs                                  */
#ifdef H5_HAVE_MAP_API
#include "H5Mprivate.h"  /* Maps                                 */
#endif /*  H5_HAVE_MAP_API */
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
static void * H5VL__wrap_obj(void *obj, H5I_type_t obj_type);
static void * H5VL__object(hid_t id, H5I_type_t obj_type);

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5VL_object_t struct */
H5FL_DEFINE_STATIC(H5VL_object_t);

/* Mapping of VOL object types to ID types */
const H5I_type_t H5VL_obj_to_id_g[] = {
    H5I_BADID,          /* Invalid type: not defined */
    H5I_FILE,           /* H5VL_OBJ_FILE */
    H5I_GROUP,          /* H5VL_OBJ_GROUP */
    H5I_DATATYPE,       /* H5VL_OBJ_DATATYPE */
    H5I_DATASET,        /* H5VL_OBJ_DATASET */
#ifdef H5_HAVE_MAP_API
    H5I_MAP,            /* H5VL_OBJ_MAP */
#endif /*  H5_HAVE_MAP_API */
    H5I_ATTR            /* H5VL_OBJ_ATTR */
};

/* Mapping of ID types VOL object types */
const H5VL_obj_type_t H5VL_id_to_obj_g[] = {
    0,                  /* Invalid type: not defined */
    H5VL_OBJ_FILE,      /* H5I_FILE */
    H5VL_OBJ_GROUP,     /* H5I_GROUP */
    H5VL_OBJ_DATATYPE,  /* H5I_DATATYPE */
    0,                  /* Invalid type: H5I_DATASPACE */
    H5VL_OBJ_DATASET,   /* H5I_DATASET */
#ifdef H5_HAVE_MAP_API
    H5VL_OBJ_MAP,       /* H5I_MAP */
#else /*  H5_HAVE_MAP_API */
    0,                  /* Invalid type: H5I_MAP */
#endif /*  H5_HAVE_MAP_API */
    H5VL_OBJ_ATTR,      /* H5I_ATTR */
    0,                  /* Invalid type: H5I_VFL */
    0,                  /* Invalid type: H5I_VOL */
    0,                  /* Invalid type: H5I_GENPROP_CLS */
    0,                  /* Invalid type: H5I_GENPROP_LST */
    0,                  /* Invalid type: H5I_ERROR_CLASS */
    0,                  /* Invalid type: H5I_ERROR_MSG */
    0,                  /* Invalid type: H5I_ERROR_STACK */
    0,                  /* Invalid type: H5I_SPACE_SEL_ITER */
    0                   /* Invalid type: H5I_EVENTSET */
};

/*-------------------------------------------------------------------------
 * Function:    H5VL__wrap_obj
 *
 * Purpose:     Wraps a library object with possible VOL connector wrappers, to
 *		match the VOL connector stack for the file.
 *
 * Return:      Success:        Wrapped object pointer
 *              Failure:        NULL
 *
 * Programmer:	Quincey Koziol
 *		Friday, October  7, 2018
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__wrap_obj(void *obj, H5I_type_t obj_type)
{
    H5VL_container_ctx_t *container_ctx = NULL; /* Container context */
    void *           ret_value    = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check arguments */
    HDassert(obj);

    /* Retrieve the primary VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL container context")

    /* If there is a VOL object wrapping context, wrap the object */
    if (container_ctx && container_ctx->obj_wrap_ctx) {
        /* Wrap object, using the VOL callback */
        if (NULL == (ret_value = H5VL__wrap_object(container_ctx->container->connector, container_ctx->obj_wrap_ctx, obj, obj_type)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't wrap object")
    } /* end if */
    else
        ret_value = obj;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__wrap_obj() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__new_vol_obj
 *
 * Purpose:     Creates a new VOL object, to use when registering an ID.
 *
 * Return:      Success:        VOL object pointer
 *              Failure:        NULL
 *
 * Programmer:	Quincey Koziol
 *		Friday, October  7, 2018
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL__new_vol_obj(H5VL_obj_type_t type, void *object, H5VL_container_t *container)
{
    H5VL_object_t *vol_obj  = NULL;  /* Pointer to new VOL object                    */
    H5VL_object_t *ret_value    = NULL;  /* Return value                                 */

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    HDassert(container);

    /* Create the new VOL object */
    if (NULL == (vol_obj = H5VL__create_object(type, object, container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object")

    /* If this is a datatype, we have to hide the VOL object under the H5T_t pointer */
    if (H5VL_OBJ_DATATYPE == type) {
        if (NULL == (ret_value = (H5VL_object_t *)H5T_construct_datatype(vol_obj)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "can't construct datatype object")
    } /* end if */
    else
        ret_value = vol_obj;

done:
    /* Cleanup on error */
    if (NULL == ret_value)
        if (vol_obj && H5VL_free_object(vol_obj) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, NULL, "unable to free VOL object")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__new_vol_obj() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_register
 *
 * Purpose:     VOL-aware version of H5I_register. Constructs an H5VL_object_t
 *              from the passed-in object and registers that. Does the right
 *              thing with datatypes, which are complicated under the VOL.
 *
 * Note:        Does not wrap object, since it's from a VOL callback.
 *
 * Return:      Success:    A valid HDF5 ID
 *              Failure:    H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_register(H5VL_obj_type_t type, void *object, H5VL_container_t *container, hbool_t app_ref)
{
    H5VL_object_t *vol_obj   = NULL;            /* VOL object wrapper for library object */
    hid_t          ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    /* Check arguments */
    HDassert(container);

    /* Set up VOL object for the passed-in data */
    if (NULL == (vol_obj = H5VL__new_vol_obj(type, object, container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "can't create VOL object")

    /* Register VOL object as _object_ type, for future object API calls */
    if ((ret_value = H5I_register(H5VL_obj_to_id_g[type], vol_obj, app_ref)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register handle")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_register() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_register_using_existing_id
 *
 * Purpose:     Registers an OBJECT in a TYPE with the supplied ID for it.
 *              This routine will check to ensure the supplied ID is not already
 *              in use, and ensure that it is a valid ID for the given type,
 *              but will NOT check to ensure the OBJECT is not already
 *              registered (thus, it is possible to register one object under
 *              multiple IDs).
 *
 * NOTE:        Intended for use in refresh calls, where we have to close
 *              and re-open the underlying data, then hook the VOL object back
 *              up to the original ID.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_register_using_existing_id(H5I_type_t type, void *object, H5VL_container_t *container, hbool_t app_ref,
                                hid_t existing_id)
{
    H5VL_object_t *new_vol_obj = NULL;    /* Pointer to new VOL object                    */
    void *wrapped_object = NULL;          /* Pointer to wrapped object */
    herr_t         ret_value   = SUCCEED; /* Return value                                 */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(object);
    HDassert(container);

    /* Wrap object, since it's a native object */
    if (NULL == (wrapped_object = H5VL__wrap_obj(object, type)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't wrap library object")

    /* Set up VOL object for the wrapped object */
    if (NULL == (new_vol_obj = H5VL__new_vol_obj(H5VL_id_to_obj_g[type], wrapped_object, container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object")

    /* Call the underlying H5I function to complete the registration */
    if (H5I_register_using_existing_id(type, new_vol_obj, app_ref, existing_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, FAIL, "can't register object under existing ID")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_register_using_existing_id() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__create_object
 *
 * Purpose:     Similar to H5VL_register but does not create an ID.
 *              Creates a new VOL object for the provided generic object
 *              using the provided vol connector.  Should only be used for
 *              internal objects returned from the connector such as
 *              requests.
 *
 * Note:        'object' pointer must be NULL for file types, as the container
 *              holds the actual file object pointer.
 *
 * Return:      Success:    A valid VOL object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL__create_object(H5VL_obj_type_t type, void *object, H5VL_container_t *container)
{
    H5VL_object_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Check arguments */
    HDassert((type != H5VL_OBJ_FILE && object) || (type == H5VL_OBJ_FILE && object == NULL));
    HDassert(container);

    /* Set up VOL object for the passed-in data */
    if (NULL == (ret_value = H5FL_CALLOC(H5VL_object_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "can't allocate memory for VOL object")
    ret_value->obj_type  = type;
    ret_value->object    = object;
    ret_value->container = container;

    /* Bump the reference count on the VOL container */
    H5VL_container_inc_rc(container);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__create_object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_free_object
 *
 * Purpose:     Wrapper to unregister an object ID with a VOL aux struct
 *              and decrement ref count on VOL connector ID
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_free_object(H5VL_object_t *vol_obj)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(vol_obj);

    /* Decrement refcount on container */
    if (H5VL_container_dec_rc(vol_obj->container) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to decrement ref count on VOL container")

    vol_obj = H5FL_FREE(H5VL_object_t, vol_obj);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_free_object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object_is_native
 *
 * Purpose:     Query if an object is (if it's a file object) / is in (if its
 *              an object) a native connector's file.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:  Quincey Koziol
 *              December 14, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_is_native(const H5VL_object_t *obj, H5VL_get_conn_lvl_t lvl, hbool_t *is_native)
{
    const H5VL_class_t *cls;                 /* VOL connector class structs for object */
    const H5VL_connector_t *native_connector;          /* Native VOL connector class structs */
    int                 cmp_value;           /* Comparison result */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(obj);
    HDassert(is_native);

    /* Retrieve the terminal connector class for the object */
    cls = NULL;
    if (H5VL_introspect_get_conn_cls(obj, lvl, &cls) < 0)
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
} /* end H5VL_object_is_native() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_vol_object
 *
 * Purpose:     Utility function to return the object pointer associated with
 *              a hid_t. This routine is the same as H5I_object for all types
 *              except for named datatypes, where the vol_obj is returned that
 *              is attached to the H5T_t struct.
 *
 * Return:      Success:        object pointer
 *              Failure:        NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_vol_object(hid_t id)
{
    void *         obj = NULL;
    H5I_type_t     obj_type;
    H5VL_object_t *ret_value = NULL;

    FUNC_ENTER_NOAPI(NULL)

    obj_type = H5I_get_type(id);
    if (H5I_FILE == obj_type || H5I_GROUP == obj_type || H5I_ATTR == obj_type || H5I_DATASET == obj_type ||
        H5I_DATATYPE == obj_type || H5I_MAP == obj_type) {
        /* Get the object */
        if (NULL == (obj = H5I_object(id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "invalid identifier")

        /* If this is a datatype, get the VOL object attached to the H5T_t struct */
        if (H5I_DATATYPE == obj_type)
            if (NULL == (obj = H5T_get_named_type((H5T_t *)obj)))
                HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a named datatype")
    } /* end if */
    else
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "invalid identifier type to function")

    ret_value = (H5VL_object_t *)obj;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_vol_object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object_data
 *
 * Purpose:     Correctly retrieve the 'object' field for a VOL object,
 *              even for nested / stacked VOL connectors.
 *
 * Return:      Success:        object pointer
 *              Failure:        NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL_object_data(const H5VL_object_t *vol_obj)
{
    void *obj;                  /* Object to operate on */
    void *ret_value = NULL;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Check for 'get_object' callback in connector */
    if (vol_obj->container->connector->cls->wrap_cls.get_object)
        ret_value = (vol_obj->container->connector->cls->wrap_cls.get_object)(obj);
    else
        ret_value = obj;

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_data() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object_unwrap
 *
 * Purpose:     Correctly unwrap the 'data' field for a VOL object (H5VL_object),
 *              even for nested / stacked VOL connectors.
 *
 * Return:      Success:        Object pointer
 *              Failure:        NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL_object_unwrap(const H5VL_object_t *vol_obj)
{
    void *ret_value = NULL;

    FUNC_ENTER_NOAPI(NULL)

    if (NULL == (ret_value = H5VL__unwrap_object(vol_obj->container->connector, vol_obj->object)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't unwrap object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_unwrap() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object
 *
 * Purpose:     Internal function to return the VOL object pointer associated
 *              with an hid_t.
 *
 * Return:      Success:    object pointer
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__object(hid_t id, H5I_type_t obj_type)
{
    H5VL_object_t *vol_obj   = NULL;
    void *         ret_value = NULL;

    FUNC_ENTER_STATIC

    /* Get the underlying object */
    switch (obj_type) {
        case H5I_GROUP:
        case H5I_DATASET:
        case H5I_FILE:
        case H5I_ATTR:
        case H5I_MAP:
            /* get the object */
            if (NULL == (vol_obj = (H5VL_object_t *)H5I_object(id)))
                HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "invalid identifier")
            break;

        case H5I_DATATYPE: {
            H5T_t *dt = NULL;

            /* get the object */
            if (NULL == (dt = (H5T_t *)H5I_object(id)))
                HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "invalid identifier")

            /* Get the actual datatype object that should be the vol_obj */
            if (NULL == (vol_obj = H5T_get_named_type(dt)))
                HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a named datatype")
            break;
        }

        case H5I_UNINIT:
        case H5I_BADID:
        case H5I_DATASPACE:
        case H5I_VFL:
        case H5I_VOL:
        case H5I_GENPROP_CLS:
        case H5I_GENPROP_LST:
        case H5I_ERROR_CLASS:
        case H5I_ERROR_MSG:
        case H5I_ERROR_STACK:
        case H5I_SPACE_SEL_ITER:
        case H5I_EVENTSET:
        case H5I_NTYPES:
        default:
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "unknown data object type")
    } /* end switch */

    /* Set the return value */
    ret_value = H5VL_object_data(vol_obj);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object
 *
 * Purpose:     Utility function to return the VOL object pointer associated with
 *              a hid_t.
 *
 * Return:      Success:    object pointer
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL_object(hid_t id)
{
    void *ret_value = NULL;

    FUNC_ENTER_NOAPI(NULL)

    /* Get the underlying object */
    if (NULL == (ret_value = H5VL__object(id, H5I_get_type(id))))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't retrieve object for ID")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object_verify
 *
 * Purpose:     Utility function to return the VOL object pointer associated
 *              with an identifier.
 *
 * Return:      Success:    object pointer
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL_object_verify(hid_t id, H5I_type_t obj_type)
{
    void *ret_value = NULL;

    FUNC_ENTER_NOAPI(NULL)

    /* Check of ID of correct type */
    if (obj_type != H5I_get_type(id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "invalid identifier")

    /* Get the underlying object */
    if (NULL == (ret_value = H5VL__object(id, obj_type)))
        HGOTO_ERROR(H5E_ARGS, H5E_CANTGET, NULL, "can't retrieve object for ID")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_verify() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_wrap_register
 *
 * Purpose:     Wrap an object and register an ID for it
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_wrap_register(H5I_type_t type, void *obj, hbool_t app_ref)
{
    H5VL_container_ctx_t *container_ctx = NULL;         /* Container context */
    void *           wrapped_obj;                     /* Newly wrapped object */
    hid_t            ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_NOAPI(H5I_INVALID_HID)

    /* Sanity check */
    HDassert(obj);

    /* Retrieve the primary VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, H5I_INVALID_HID, "can't get VOL container context")
    if (NULL == container_ctx || NULL == container_ctx->container)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, H5I_INVALID_HID, "VOL container context or its container is NULL???")

    /* If the datatype is already VOL-managed, the datatype's vol_obj
     * field will get clobbered later, so disallow this.
     */
    if (type == H5I_DATATYPE)
        if (container_ctx->container->conn_prop.connector_id == H5VL_NATIVE)
            if (TRUE == H5T_already_vol_managed((const H5T_t *)obj))
                HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, H5I_INVALID_HID, "can't wrap an uncommitted datatype")

    /* Wrap the object with VOL connector info */
    if (NULL == (wrapped_obj = H5VL__wrap_obj(obj, type)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "can't wrap library object")

    /* Get an ID for the object */
    if ((ret_value = H5VL_register(H5VL_id_to_obj_g[type], wrapped_obj, container_ctx->container, app_ref)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to get an ID for the object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_wrap_register() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_object_close
 *
 * Purpose:     This is a helper routine for closing & freeing a arbitrary VOL object
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_close(H5VL_object_t *vol_obj)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check args */
    HDassert(vol_obj);

    /* Invoke correct close operation for the VOL object type */
    switch (vol_obj->obj_type) {
        case H5VL_OBJ_FILE:
            if (H5VL_file_close(vol_obj, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close file")
            break;

        case H5VL_OBJ_GROUP:
            if (H5VL_group_close(vol_obj, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close group")
            break;

        case H5VL_OBJ_DATATYPE:
            if (H5VL_datatype_close(vol_obj, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close named datatype")
            break;

        case H5VL_OBJ_DATASET:
            if (H5VL_dataset_close(vol_obj, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close dataset")
            break;

#ifdef H5_HAVE_MAP_API
        case H5VL_OBJ_MAP:
            if (H5M_close(vol_obj, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close map")
            break;
#endif /*  H5_HAVE_MAP_API */

        case H5VL_OBJ_ATTR:
            if (H5VL_attr_close(vol_obj, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unable to close attribute")
            break;

        default:
            HGOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "unknown object type!")
            break;
    } /* end switch */

    /* Free VOL object */
    if (H5VL_free_object(vol_obj) < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "can't free VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL_object_close() */

