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
#include "H5Aprivate.h"  /* Attributes                           */
#include "H5Dprivate.h"  /* Datasets                             */
#include "H5Eprivate.h"  /* Error handling                       */
#include "H5Fprivate.h"  /* Files                                */
#include "H5Gprivate.h"  /* Groups                               */
#include "H5Iprivate.h"  /* IDs                                  */
#ifdef H5_HAVE_MAP_API
#include "H5Mprivate.h"  /* Maps                                 */
#endif /*  H5_HAVE_MAP_API */
#include "H5Tprivate.h"  /* Datatypes                            */
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

/* Package initialization variable */
hbool_t H5_PKG_INIT_VAR = FALSE;

/*****************************/
/* Library Private Variables */
/*****************************/

/*******************/
/* Local Variables */
/*******************/

/* VOL ID class */
static const H5I_class_t H5I_VOL_CLS[1] = {{
    H5I_VOL,                   /* ID class value */
    0,                         /* Class flags */
    0,                         /* # of reserved IDs for class */
    (H5I_free_t)H5VL__conn_close /* Callback routine for closing objects of this class */
}};

/*-------------------------------------------------------------------------
 * Function:    H5VL_init_phase1
 *
 * Purpose:     Initialize the interface from some other package.  This should
 *		be followed with a call to H5VL_init_phase2 after the H5P
 *		interface is completely set up, finish setting up the H5VL
 *		information.
 *
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_init_phase1(void)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* FUNC_ENTER() does all the work */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_init_phase1() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_init_phase2
 *
 * Purpose:     Finish initializing the interface from some other package.
 *
 * Note:	This is broken out as a separate routine to avoid a circular
 *		reference with the H5P package.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_init_phase2(void)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Initialize all packages for VOL-managed objects */
    if (H5T_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize datatype interface")
    if (H5D_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize dataset interface")
    if (H5F_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize file interface")
    if (H5G_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize group interface")
    if (H5A_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize attribute interface")
#ifdef H5_HAVE_MAP_API
    if (H5M_init() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize map interface")
#endif /*  H5_HAVE_MAP_API */

    /* Set up the default VOL connector in the default FAPL */
    if (H5VL__set_def_conn() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "unable to set default VOL connector")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_init_phase2() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__init_package
 *
 * Purpose:     Initialize interface-specific information
 *
 * Return:      Success:    Non-negative
 *
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__init_package(void)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Initialize the ID group for the VL IDs */
    if (H5I_register_type(H5I_VOL_CLS) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to initialize H5VL interface")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__init_package() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_term_package
 *
 * Purpose:     Terminate various H5VL objects
 *
 * Return:      Success:    Positive if anything was done that might
 *                          affect other interfaces; zero otherwise.
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
int
H5VL_term_package(void)
{
    int n = 0;

    FUNC_ENTER_NOAPI_NOINIT_NOERR

    if (H5_PKG_INIT_VAR) {
        if (H5VL_def_conn_g.connector_id > 0) {
            /* Release the default VOL connector */
            (void)H5VL_conn_prop_free(&H5VL_def_conn_g);
            n++;
        } /* end if */
        else {
            if (H5I_nmembers(H5I_VOL) > 0) {
                /* Unregister all VOL connectors */
                (void)H5I_clear_type(H5I_VOL, TRUE, FALSE);
                n++;
            } /* end if */
            else {
                if (H5VL__num_opt_operation() > 0) {
                    /* Unregister all dynamically registered optional operations */
                    (void)H5VL__term_opt_operation();
                    n++;
                } /* end if */
                else {
                    /* Destroy the VOL connector ID group */
                    n += (H5I_dec_type_ref(H5I_VOL) > 0);

                    /* Mark interface as closed */
                    if (0 == n)
                        H5_PKG_INIT_VAR = FALSE;
                } /* end else */
            }     /* end else */
        }         /* end else */
    }             /* end if */

    FUNC_LEAVE_NOAPI(n)
} /* end H5VL_term_package() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_fapl_is_native
 *
 * Purpose:     Query if a FAPL will use the native VOL connector.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *              Jun 17, 2021
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_fapl_is_native(hid_t fapl_id, hbool_t *is_native)
{
    H5P_genplist_t *      fapl_plist;     /* Property list pointer                    */
    H5VL_connector_prop_t connector_prop; /* Property for VOL connector ID & info     */
    H5VL_connector_t *        connector;            /* VOL class structure for callback info    */
    const H5VL_connector_t *native_connector;     /* Native VOL connector class structs */
    int                   cmp_value;      /* Comparison result */
    herr_t                ret_value = SUCCEED;

    FUNC_ENTER_NOAPI(FAIL)

    /* Check for default property list value */
    if (H5P_DEFAULT == fapl_id)
        fapl_id = H5P_FILE_ACCESS_DEFAULT;

    /* Get the VOL info from the fapl */
    if (NULL == (fapl_plist = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a file access property list")
    if (H5P_peek(fapl_plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector info")

    /* Get the connector */
    if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Retrieve the native connector */
    if (NULL == (native_connector = H5I_object_verify(H5VL_NATIVE, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't retrieve native VOL connector class")

    /* Compare connector classes */
    if (H5VL__cmp_connector_cls(&cmp_value, connector->cls, native_connector->cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector classes")

    /* If classes compare equal, then the object is / is in a native connector's file */
    *is_native = (cmp_value == 0);

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_fapl_is_native() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_file_is_same
 *
 * Purpose:     Query if two files are the same.
 *
 * Return:      SUCCEED/FAIL
 *
 * Programmer:  Quincey Koziol
 *              December 14, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_is_same(const H5VL_object_t *vol_obj1, const H5VL_object_t *vol_obj2, hbool_t *same_file)
{
    const H5VL_class_t *cls1;                /* VOL connector class struct for first object */
    const H5VL_class_t *cls2;                /* VOL connector class struct for second object */
    int                 cmp_value;           /* Comparison result */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check arguments */
    HDassert(vol_obj1);
    HDassert(vol_obj2);
    HDassert(same_file);

    /* Retrieve the terminal connectors for each object */
    cls1 = NULL;
    if (H5VL_introspect_get_conn_cls(vol_obj1, H5VL_GET_CONN_LVL_TERM, &cls1) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector class")
    cls2 = NULL;
    if (H5VL_introspect_get_conn_cls(vol_obj2, H5VL_GET_CONN_LVL_TERM, &cls2) < 0)
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

        /* Get unwrapped (terminal) object for vol_obj2 */
        if (NULL == (obj2 = H5VL_object_data(vol_obj2)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get unwrapped object")

        /* Set up VOL callback arguments */
        vol_cb_args.op_type                 = H5VL_FILE_IS_EQUAL;
        vol_cb_args.args.is_equal.obj2      = obj2;
        vol_cb_args.args.is_equal.same_file = same_file;

        /* Make 'are files equal' callback */
        if (H5VL_file_specific(vol_obj1, &vol_cb_args, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file specific failed")
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_is_same() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_id_to_obj_type
 *
 * Purpose:     Get the VOL object type for an ID type.
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_id_to_obj_type(H5I_type_t id_type, H5VL_obj_type_t *vol_obj_type)
{
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Check args */
    if (id_type < H5I_FILE || id_type > H5I_EVENTSET)
        HGOTO_ERROR(H5E_VOL, H5E_BADRANGE, FAIL, "ID type is out of range")
    if (0 == H5VL_id_to_obj_g[id_type])
        HGOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "ID type does not map to VOL object type")

    /* Set VOL object type */
    *vol_obj_type = H5VL_id_to_obj_g[id_type];

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL_id_to_obj_type() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__update_fapl_vol
 *
 * Purpose:     Update VOL information in FAPL
 *
 * Return:      SUCCEED/FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__update_fapl_vol(hid_t fapl_id, const H5VL_container_t *container)
{
    H5P_genplist_t *fapl_plist;       /* Property list pointer */
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Get the file access property list */
    if (NULL == (fapl_plist = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a file access property list")

    /* Set the VOL connector property */
    if (H5P_set(fapl_plist, H5F_ACS_VOL_CONN_NAME, &container->conn_prop) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTSET, FAIL, "can't set VOL connector ID & info")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* H5VL__update_fapl_vol() */

