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
 * Function:    H5VL_setup_args
 *
 * Purpose:     Set up arguments to access an object
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_args(hid_t loc_id, H5I_type_t id_type, H5VL_object_t **vol_obj)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);

    /* Get attribute pointer */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5I_object_verify(loc_id, id_type)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not the correct type of ID")

    /* Set up collective metadata (if appropriate) */
    if (H5CX_set_loc(loc_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set collective metadata read")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_loc_args
 *
 * Purpose:     Set up arguments to access an object
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_loc_args(hid_t loc_id, H5VL_object_t **vol_obj, H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not the correct type of ID")

    /* Set up collective metadata (if appropriate */
    if (H5CX_set_loc(loc_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set collective metadata read")

    /* Set location parameters */
    loc_params->type     = H5VL_OBJECT_BY_SELF;
    loc_params->obj_type = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_loc_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_acc_args
 *
 * Purpose:     Set up arguments to access an object
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_acc_args(hid_t loc_id, const H5P_libclass_t *libclass, hbool_t is_collective, hid_t *acspl_id,
                    H5VL_object_t **vol_obj, H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(libclass);
    HDassert(acspl_id);
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Verify access property list and set up collective metadata if appropriate */
    if (H5CX_set_apl(acspl_id, libclass, loc_id, is_collective) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set access property list info")

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")

    /* Set location parameters */
    loc_params->type     = H5VL_OBJECT_BY_SELF;
    loc_params->obj_type = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_acc_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_self_args
 *
 * Purpose:     Set up arguments to access an object "by self"
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_self_args(hid_t loc_id, H5VL_object_t **vol_obj, H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")

    /* Set location parameters */
    loc_params->type     = H5VL_OBJECT_BY_SELF;
    loc_params->obj_type = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_self_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_name_args
 *
 * Purpose:     Set up arguments to access an object "by name"
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_name_args(hid_t loc_id, const char *name, hbool_t is_collective, hid_t lapl_id,
                     H5VL_object_t **vol_obj, H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Check args */
    if (!name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "name parameter cannot be NULL")
    if (!*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "name parameter cannot be an empty string")

    /* Verify access property list and set up collective metadata if appropriate */
    if (H5CX_set_apl(&lapl_id, H5P_CLS_LACC, loc_id, is_collective) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set access property list info")

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")

    /* Set up location parameters */
    loc_params->type                         = H5VL_OBJECT_BY_NAME;
    loc_params->loc_data.loc_by_name.name    = name;
    loc_params->loc_data.loc_by_name.lapl_id = lapl_id;
    loc_params->obj_type                     = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_name_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_idx_args
 *
 * Purpose:     Set up arguments to access an object "by idx"
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_idx_args(hid_t loc_id, const char *name, H5_index_t idx_type, H5_iter_order_t order, hsize_t n,
                    hbool_t is_collective, hid_t lapl_id, H5VL_object_t **vol_obj,
                    H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Check args */
    if (!name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "name parameter cannot be NULL")
    if (!*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "name parameter cannot be an empty string")
    if (idx_type <= H5_INDEX_UNKNOWN || idx_type >= H5_INDEX_N)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid index type specified")
    if (order <= H5_ITER_UNKNOWN || order >= H5_ITER_N)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid iteration order specified")

    /* Verify access property list and set up collective metadata if appropriate */
    if (H5CX_set_apl(&lapl_id, H5P_CLS_LACC, loc_id, is_collective) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set access property list info")

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")

    /* Set location parameters */
    loc_params->type                         = H5VL_OBJECT_BY_IDX;
    loc_params->loc_data.loc_by_idx.name     = name;
    loc_params->loc_data.loc_by_idx.idx_type = idx_type;
    loc_params->loc_data.loc_by_idx.order    = order;
    loc_params->loc_data.loc_by_idx.n        = n;
    loc_params->loc_data.loc_by_idx.lapl_id  = lapl_id;
    loc_params->obj_type                     = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_idx_args() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_setup_token_args
 *
 * Purpose:     Set up arguments to access an object by token
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_setup_token_args(hid_t loc_id, H5O_token_t *obj_token, H5VL_object_t **vol_obj,
                      H5VL_loc_params_t *loc_params)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);
    HDassert(loc_params);

    /* Get the location object */
    if (NULL == (*vol_obj = (H5VL_object_t *)H5VL_vol_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")

    /* Set location parameters */
    loc_params->type                        = H5VL_OBJECT_BY_TOKEN;
    loc_params->loc_data.loc_by_token.token = obj_token;
    loc_params->obj_type                    = H5I_get_type(loc_id);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_setup_token_args() */
