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

/* Declare a free list to manage the H5VL_request_t struct */
H5FL_DEFINE_STATIC(H5VL_request_t);

/*-------------------------------------------------------------------------
 * Function:    H5VL_create_request
 *
 * Purpose:     Creates a new VOL request object.
 *
 * Return:      Success:        VOL request pointer
 *              Failure:        NULL
 *
 * Programmer:	Quincey Koziol
 *		Friday, July  2, 2021
 *
 *-------------------------------------------------------------------------
 */
H5VL_request_t *
H5VL_create_request(void *token, H5VL_connector_t *connector)
{
    H5VL_request_t *request  = NULL;  /* Pointer to new VOL request object                    */
    H5VL_request_t *ret_value    = NULL;  /* Return value                                 */

    FUNC_ENTER_NOAPI(NULL)

    /* Check arguments */
    HDassert(token);
    HDassert(connector);

    /* Create the new VOL request object */
    if (NULL == (request = H5FL_CALLOC(H5VL_request_t)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "can't allocate memory for VOL request object")

    /* Increment refcount on connector */
    H5VL__conn_inc_rc(connector);

    /* Set fields */
    request->token = token;
    request->connector = connector;

    /* Set return value */
    ret_value = request;

done:
    /* Cleanup on error */
    if (NULL == ret_value)
        if (request)
            request = H5FL_FREE(H5VL_request_t, request);

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_create_request() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_free_token
 *
 * Purpose:     Special-purpose error-cleanup routine to release a VOL connector's
 *              token.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		Friday, July  2, 2021
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_free_token(void *token, H5VL_connector_t *connector)
{
    H5VL_request_t request;  /* Static VOL request object                    */
    herr_t         ret_value   = SUCCEED; /* Return value                                 */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(token);
    HDassert(connector);

    /* Set up request object */
    request.token = token;
    request.connector = connector;

    /* Free the VOL connector's token */
    if (H5VL_request_free(&request) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request free failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_free_token() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_free_request
 *
 * Purpose:     Release a VOL request object.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		Friday, July  2, 2021
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_free_request(H5VL_request_t *request)
{
    herr_t         ret_value   = SUCCEED; /* Return value                                 */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(request);

    /* Free the VOL connector's token */
    if (H5VL_request_free(request) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request free failed")

    /* Decrement refcount for using connector */
    if (H5VL__conn_dec_rc(request->connector) < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to decrement ref count on VOL connector")

    /* Free request */
    request = H5FL_FREE(H5VL_request_t, request);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_free_request() */

