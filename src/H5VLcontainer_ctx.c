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
static H5VL_container_ctx_t * H5VL__create_container_ctx(H5VL_container_t *container);
static herr_t H5VL__free_container_ctx(H5VL_container_ctx_t *container_ctx);

/*********************/
/* Package Variables */
/*********************/

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5VL_container_ctx_t struct */
H5FL_DEFINE_STATIC(H5VL_container_ctx_t);

/*-------------------------------------------------------------------------
 * Function:    H5VL__create_object_with_container_ctx
 *
 * Purpose:     Create an object using the container context.
 *
 * Return:      Success:    A valid VOL object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL__create_object_with_container_ctx(H5VL_obj_type_t type, void *object)
{
    H5VL_container_ctx_t *container_ctx = NULL; /* Container context */
    H5VL_object_t *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Retrieve the primary VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL container context")

    /* Check for valid, active VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "no VOL container context?")
    if (0 == container_ctx->rc)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "bad VOL container context refcount?")

    /* Create the new VOL object */
    if (NULL == (ret_value = H5VL__create_object(type, object, container_ctx->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__create_object_with_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_get_primary_container
 *
 * Purpose:     Retrieve the primary VOL container for an API operation.
 *
 * Return:      Success:    A valid VOL container
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_container_t *
H5VL_get_primary_container(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    H5VL_container_t *ret_value = NULL;     /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Retrieve the primary VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL container context")
    if (NULL == container_ctx || NULL == container_ctx->container)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "VOL container context or its container is NULL???")

    /* Set return value */
    ret_value = container_ctx->container;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_get_primary_container() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_get_src_container
 *
 * Purpose:     Retrieve the 'src' VOL container for an API operation.
 *
 * Return:      Success:    A valid VOL container
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_container_t *
H5VL_get_src_container(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    H5VL_container_t *ret_value = NULL;     /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Retrieve the 'src' VOL container context */
    if (H5CX_get_src_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL container context")
    if (NULL == container_ctx || NULL == container_ctx->container)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "VOL container context or its container is NULL???")

    /* Set return value */
    ret_value = container_ctx->container;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_get_src_container() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_get_dst_container
 *
 * Purpose:     Retrieve the 'dst' VOL container for an API operation.
 *
 * Return:      Success:    A valid VOL container
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_container_t *
H5VL_get_dst_container(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    H5VL_container_t *ret_value = NULL;     /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Retrieve the 'dst' VOL container context */
    if (H5CX_get_dst_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL container context")
    if (NULL == container_ctx || NULL == container_ctx->container)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "VOL container context or its container is NULL???")

    /* Set return value */
    ret_value = container_ctx->container;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_get_dst_container() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__create_container_ctx
 *
 * Purpose:     Create container context
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
static H5VL_container_ctx_t *
H5VL__create_container_ctx(H5VL_container_t *container)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    void *obj_wrap_ctx = NULL; /* VOL connector's wrapping context */
    H5VL_container_ctx_t *ret_value    = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(container);

    /* Check if the connector can create a wrap context */
    if (container->connector->cls->wrap_cls.get_wrap_ctx) {
        /* Sanity check */
        HDassert(container->connector->cls->wrap_cls.free_wrap_ctx);

        /* Get the wrap context from the connector */
        if ((container->connector->cls->wrap_cls.get_wrap_ctx)(container->object, &obj_wrap_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't retrieve VOL connector's object wrap context")
    } /* end if */

    /* Allocate VOL container context */
    if (NULL == (container_ctx = H5FL_MALLOC(H5VL_container_ctx_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "can't allocate VOL container context")

    /* Increment the outstanding objects that are using the container */
    H5VL_container_inc_rc(container);

    /* Set up VOL container context */
    container_ctx->rc           = 1;
    container_ctx->container    = container;
    container_ctx->obj_wrap_ctx = obj_wrap_ctx;

    /* Set return value */
    ret_value = container_ctx;

done:
    if (NULL == ret_value && container_ctx)
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, NULL, "unable to release VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__create_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_inc_container_ctx
 *
 * Purpose:     Increment refcount on container context
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, January  9, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_inc_container_ctx(void *_container_ctx)
{
    H5VL_container_ctx_t *container_ctx = (H5VL_container_ctx_t *)_container_ctx; /* VOL container context */
    herr_t           ret_value    = SUCCEED;                          /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check for valid, active VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "no VOL container context?")
    if (0 == container_ctx->rc)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "bad VOL container context refcount?")

    /* Increment ref count on container context */
    container_ctx->rc++;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_inc_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_dec_container_ctx
 *
 * Purpose:     Decrement refcount on container context, releasing it
 *		if the refcount drops to zero.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, January  9, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dec_container_ctx(void *_container_ctx)
{
    H5VL_container_ctx_t *container_ctx = (H5VL_container_ctx_t *)_container_ctx; /* VOL container context */
    herr_t           ret_value    = SUCCEED;                          /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check for valid, active VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "no VOL container context?")
    if (0 == container_ctx->rc)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "bad VOL container context refcount?")

    /* Decrement ref count on container context */
    container_ctx->rc--;

    /* Release context if the ref count drops to zero */
    if (0 == container_ctx->rc)
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dec_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__free_container_ctx
 *
 * Purpose:     Free container context for VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, January  9, 2019
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__free_container_ctx(H5VL_container_ctx_t *container_ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(container_ctx);
    HDassert(0 == container_ctx->rc);
    HDassert(container_ctx->container);
    HDassert(container_ctx->container->connector);
    HDassert(container_ctx->container->connector->cls);

    /* If there is a VOL connector object wrapping context, release it */
    if (container_ctx->obj_wrap_ctx)
        /* Release the VOL connector's object wrapping context */
        if ((*container_ctx->container->connector->cls->wrap_cls.free_wrap_ctx)(container_ctx->obj_wrap_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release connector's object wrapping context")

    /* Decrement refcount on container */
    if (H5VL_container_dec_rc(container_ctx->container) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to decrement ref count on VOL container")

    /* Release container context */
    H5FL_FREE(H5VL_container_ctx_t, container_ctx);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__free_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_set_primary_container_ctx
 *
 * Purpose:     Set up container context for primary VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_set_primary_container_ctx(const H5VL_object_t *vol_obj)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    hbool_t container_ctx_created = FALSE;      /* Whether container context was created */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);

    /* Retrieve the VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for existing container context */
    if (NULL == container_ctx) {
        if (NULL == (container_ctx = H5VL__create_container_ctx(vol_obj->container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create container context")
        container_ctx_created = TRUE;
    } /* end if */
    else
        /* Increment ref count on existing container context */
        container_ctx->rc++;

    /* Save the container context */
    if (H5CX_set_primary_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    if (ret_value < 0 && container_ctx && container_ctx_created)
        /* Release container context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_set_primary_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_reset_primary_container_ctx
 *
 * Purpose:     Reset container context for primary VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_reset_primary_container_ctx(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Retrieve the VOL container context */
    if (H5CX_get_primary_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "no VOL container context?")

    /* Decrement ref count on container context */
    container_ctx->rc--;

    /* Release context if the ref count drops to zero */
    if (0 == container_ctx->rc) {
        /* Release object wrapping context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")
        container_ctx = NULL;
    } /* end if */

    /* Save the updated container context */
    if (H5CX_set_primary_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_reset_primary_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__set_src_container_ctx
 *
 * Purpose:     Set up container context for 'src' VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__set_src_container_ctx(const H5VL_object_t *vol_obj)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    hbool_t container_ctx_created = FALSE;      /* Whether container context was created */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(vol_obj);

    /* Retrieve the 'src' VOL container context */
    if (H5CX_get_src_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for existing container context */
    if (NULL == container_ctx) {
        if (NULL == (container_ctx = H5VL__create_container_ctx(vol_obj->container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create container context")
        container_ctx_created = TRUE;
    } /* end if */
    else
        /* Increment ref count on existing container context */
        container_ctx->rc++;

    /* Save the container context */
    if (H5CX_set_src_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    if (ret_value < 0 && container_ctx && container_ctx_created)
        /* Release container context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__set_src_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__reset_src_container_ctx
 *
 * Purpose:     Reset container context for 'src' VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__reset_src_container_ctx(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Retrieve the 'src' VOL container context */
    if (H5CX_get_src_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "no VOL container context?")

    /* Decrement ref count on container context */
    container_ctx->rc--;

    /* Release context if the ref count drops to zero */
    if (0 == container_ctx->rc) {
        /* Release object wrapping context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")
        container_ctx = NULL;
    } /* end if */

    /* Save the updated container context */
    if (H5CX_set_src_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__reset_src_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__set_dst_container_ctx
 *
 * Purpose:     Set up container context for 'dst' VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__set_dst_container_ctx(const H5VL_object_t *vol_obj)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    hbool_t container_ctx_created = FALSE;      /* Whether container context was created */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(vol_obj);

    /* Retrieve the 'dst' VOL container context */
    if (H5CX_get_dst_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for existing container context */
    if (NULL == container_ctx) {
        if (NULL == (container_ctx = H5VL__create_container_ctx(vol_obj->container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create container context")
        container_ctx_created = TRUE;
    } /* end if */
    else
        /* Increment ref count on existing container context */
        container_ctx->rc++;

    /* Save the container context */
    if (H5CX_set_dst_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    if (ret_value < 0 && container_ctx && container_ctx_created)
        /* Release container context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__set_dst_container_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__reset_dst_container_ctx
 *
 * Purpose:     Reset container context for 'dst' VOL connector
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__reset_dst_container_ctx(void)
{
    H5VL_container_ctx_t *container_ctx = NULL;    /* Container context */
    herr_t           ret_value    = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Retrieve the 'dst' VOL container context */
    if (H5CX_get_dst_container_ctx((void **)&container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL container context")

    /* Check for VOL container context */
    if (NULL == container_ctx)
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "no VOL container context?")

    /* Decrement ref count on container context */
    container_ctx->rc--;

    /* Release context if the ref count drops to zero */
    if (0 == container_ctx->rc) {
        /* Release object wrapping context */
        if (H5VL__free_container_ctx(container_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL container context")
        container_ctx = NULL;
    } /* end if */

    /* Save the updated container context */
    if (H5CX_set_dst_container_ctx(container_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__reset_dst_container_ctx() */

