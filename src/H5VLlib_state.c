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
 * Function:    H5VL_retrieve_lib_state
 *
 * Purpose:     Retrieve the state of the library.
 *
 * Note:        Currently just retrieves the API context state, but could be
 *		expanded in the future.
 *
 * Return:      Success:    Non-negative, *state set
 *              Failure:    Negative, *state unset
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_retrieve_lib_state(void **state)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(state);

    /* Retrieve the API context state */
    if (H5CX_retrieve_state((H5CX_state_t **)state) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get API context state")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_retrieve_lib_state() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_start_lib_state
 *
 * Purpose:     Opens a new internal state for the HDF5 library.
 *
 * Note:        Currently just pushes a new API context state, but could be
 *		expanded in the future.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *              Friday, February 5, 2021
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_start_lib_state(void)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Push a new API context on the stack */
    if (H5CX_push() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't push API context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_start_lib_state() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_restore_lib_state
 *
 * Purpose:     Restore the state of the library.
 *
 * Note:        Currently just restores the API context state, but could be
 *		expanded in the future.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, January 10, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_restore_lib_state(const void *state)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(state);

    /* Restore the API context state */
    if (H5CX_restore_state((const H5CX_state_t *)state) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set API context state")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_restore_lib_state() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_finish_lib_state
 *
 * Purpose:     Closes the state of the library, undoing affects of
 *		H5VL_start_lib_state.
 *
 * Note:        Currently just resets the API context state, but could be
 *		expanded in the future.
 *
 * Note:	This routine must be called as a "pair" with
 * 		H5VL_start_lib_state.  It can be called before / after /
 * 		independently of H5VL_free_lib_state.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Saturday, February 23, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_finish_lib_state(void)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Pop the API context off the stack */
    if (H5CX_pop(FALSE) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't pop API context")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_finish_lib_state() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_free_lib_state
 *
 * Purpose:     Free a library state.
 *
 * Note:	This routine must be called as a "pair" with
 * 		H5VL_retrieve_lib_state.
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, January 10, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_free_lib_state(void *state)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(state);

    /* Free the API context state */
    if (H5CX_free_state((H5CX_state_t *)state) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "can't free API context state")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_free_lib_state() */
