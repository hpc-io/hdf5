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

#include "H5private.h"   /* Generic Functions                                */
#include "H5CXprivate.h" /* API Contexts                                     */
#include "H5Dprivate.h"  /* Datasets                                         */
#include "H5Eprivate.h"  /* Error handling                                   */
#include "H5ESprivate.h" /* Event Sets                                       */
#include "H5Fprivate.h"  /* File access				             */
#include "H5Iprivate.h"  /* IDs                                              */
#include "H5MMprivate.h" /* Memory management                                */
#include "H5Pprivate.h"  /* Property lists                                   */
#include "H5PLprivate.h" /* Plugins                                          */
#include "H5Tprivate.h"  /* Datatypes                                        */
#include "H5VLpkg.h"     /* Virtual Object Layer                             */

/****************/
/* Local Macros */
/****************/

/******************/
/* Local Typedefs */
/******************/

/* Structure used when trying to find a
 * VOL connector to open a given file with.
 */
typedef struct H5VL_file_open_find_connector_t {
    const char *           filename;
    const H5VL_class_t *   cls;
    H5VL_connector_prop_t *connector_prop;
    hid_t                  fapl_id;
} H5VL_file_open_find_connector_t;

/* Typedef for common callback form of registered optional operations */
typedef herr_t (*H5VL_reg_opt_oper_t)(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                      hid_t dxpl_id, void **req);

/* Typedef for common callback form of API operations */
typedef herr_t (*H5VL_api_oper_t)(void *obj, hbool_t new_api_ctx, void *ctx);

/*
 * Context data structures for common callbacks for API operations
 */

/* Attribute create "common" callback context data */
typedef struct H5VL_attr_create_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    type_id;
    hid_t                    space_id;
    hid_t                    acpl_id;
    hid_t                    aapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_attr_create_ctx_t;

/* Attribute open "common" callback context data */
typedef struct H5VL_attr_open_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    aapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_attr_open_ctx_t;

/* Attribute read "common" callback context data */
typedef struct H5VL_attr_read_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *cls;
    hid_t         mem_type_id;
    void *        buf;
    hid_t         dxpl_id;
    void **       req;
} H5VL_attr_read_ctx_t;

/* Attribute write "common" callback context data */
typedef struct H5VL_attr_write_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *cls;
    hid_t         mem_type_id;
    const void *  buf;
    hid_t         dxpl_id;
    void **       req;
} H5VL_attr_write_ctx_t;

/* Attribute get "common" callback context data */
typedef struct H5VL_attr_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_attr_get_args_t *args;
    hid_t                 dxpl_id;
    void **               req;
} H5VL_attr_get_ctx_t;

/* Attribute specific "common" callback context data */
typedef struct H5VL_attr_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *  loc_params;
    const H5VL_class_t *             cls;
    H5VL_attr_specific_args_t *args;
    hid_t                      dxpl_id;
    void **                    req;

    /* UP: API routine return value */
    herr_t ret_value;
} H5VL_attr_specific_ctx_t;

/* Attribute optional "common" callback context data */
typedef struct H5VL_attr_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_optional_args_t *args;
    hid_t                 dxpl_id;
    void **               req;

    /* UP: API routine return value */
    herr_t ret_value;
} H5VL_attr_optional_ctx_t;

/* Attribute close "common" callback context data */
typedef struct H5VL_attr_close_ctx_t {
    /* DOWN: API routine parameters */
    void *obj;
    const H5VL_class_t *cls;
    hid_t         dxpl_id;
    void **       req;
} H5VL_attr_close_ctx_t;

/* Dataset create "common" callback context data */
typedef struct H5VL_dataset_create_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    lcpl_id;
    hid_t                    type_id;
    hid_t                    space_id;
    hid_t                    dcpl_id;
    hid_t                    dapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_dataset_create_ctx_t;

/* Dataset open "common" callback context data */
typedef struct H5VL_dataset_open_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    dapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_dataset_open_ctx_t;

/* Dataset read "common" callback context data */
typedef struct H5VL_dataset_read_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *cls;
    hid_t         mem_type_id;
    hid_t         mem_space_id;
    hid_t         file_space_id;
    void *        buf;
    hid_t         dxpl_id;
    void **       req;
} H5VL_dataset_read_ctx_t;

/* Dataset write "common" callback context data */
typedef struct H5VL_dataset_write_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *cls;
    hid_t         mem_type_id;
    hid_t         mem_space_id;
    hid_t         file_space_id;
    const void *  buf;
    hid_t         dxpl_id;
    void **       req;
} H5VL_dataset_write_ctx_t;

/* Dataset get "common" callback context data */
typedef struct H5VL_dataset_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *           cls;
    H5VL_dataset_get_args_t *args;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_dataset_get_ctx_t;

/* Dataset specific "common" callback context data */
typedef struct H5VL_dataset_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *                cls;
    H5VL_dataset_specific_args_t *args;
    hid_t                         dxpl_id;
    void **                       req;
} H5VL_dataset_specific_ctx_t;

/* Dataset optional "common" callback context data */
typedef struct H5VL_dataset_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_optional_args_t *args;
    hid_t                 dxpl_id;
    void **               req;
} H5VL_dataset_optional_ctx_t;

/* Dataset close "common" callback context data */
typedef struct H5VL_dataset_close_ctx_t {
    /* DOWN: API routine parameters */
    void *obj;
    const H5VL_class_t *cls;
    hid_t         dxpl_id;
    void **       req;
} H5VL_dataset_close_ctx_t;

/* Named datatype commit "common" callback context data */
typedef struct H5VL_datatype_commit_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    type_id;
    hid_t                    lcpl_id;
    hid_t                    tcpl_id;
    hid_t                    tapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_datatype_commit_ctx_t;

/* Named datatype open "common" callback context data */
typedef struct H5VL_datatype_open_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    tapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_datatype_open_ctx_t;

/* Datatype get "common" callback context data */
typedef struct H5VL_datatype_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *            cls;
    H5VL_datatype_get_args_t *args;
    hid_t                     dxpl_id;
    void **                   req;
} H5VL_datatype_get_ctx_t;

/* Datatype specific "common" callback context data */
typedef struct H5VL_datatype_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *                 cls;
    H5VL_datatype_specific_args_t *args;
    hid_t                          dxpl_id;
    void **                        req;
} H5VL_datatype_specific_ctx_t;

/* Datatype optional "common" callback context data */
typedef struct H5VL_datatype_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_optional_args_t *args;
    hid_t                 dxpl_id;
    void **               req;
} H5VL_datatype_optional_ctx_t;

/* Datatype close "common" callback context data */
typedef struct H5VL_datatype_close_ctx_t {
    /* DOWN: API routine parameters */
    void *obj;
    const H5VL_class_t *cls;
    hid_t         dxpl_id;
    void **       req;
} H5VL_datatype_close_ctx_t;

/* File get "common" callback context data */
typedef struct H5VL_file_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_file_get_args_t *args;
    hid_t                 dxpl_id;
    void **               req;
} H5VL_file_get_ctx_t;

/* File specific "common" callback context data */
typedef struct H5VL_file_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *             cls;
    H5VL_file_specific_args_t *args;
    hid_t                      dxpl_id;
    void **                    req;
} H5VL_file_specific_ctx_t;

/* File optional "common" callback context data */
typedef struct H5VL_file_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_optional_args_t *args;
    hid_t                 dxpl_id;
    void **               req;
} H5VL_file_optional_ctx_t;

/* File close "common" callback context data */
typedef struct H5VL_file_close_ctx_t {
    /* DOWN: API routine parameters */
    void *obj;
    const H5VL_class_t *cls;
    hid_t         dxpl_id;
    void **       req;
} H5VL_file_close_ctx_t;

/* Group create "common" callback context data */
typedef struct H5VL_group_create_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    lcpl_id;
    hid_t                    gcpl_id;
    hid_t                    gapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_group_create_ctx_t;

/* Group open "common" callback context data */
typedef struct H5VL_group_open_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    const char *             name;
    hid_t                    gapl_id;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_group_open_ctx_t;

/* Group get "common" callback context data */
typedef struct H5VL_group_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *         cls;
    H5VL_group_get_args_t *args;
    hid_t                  dxpl_id;
    void **                req;
} H5VL_group_get_ctx_t;

/* Group specific "common" callback context data */
typedef struct H5VL_group_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *              cls;
    H5VL_group_specific_args_t *args;
    hid_t                       dxpl_id;
    void **                     req;
} H5VL_group_specific_ctx_t;

/* Group optional "common" callback context data */
typedef struct H5VL_group_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_class_t *        cls;
    H5VL_optional_args_t *args;
    hid_t                 dxpl_id;
    void **               req;

    /* UP: API routine return value */
    herr_t ret_value;
} H5VL_group_optional_ctx_t;

/* Group close "common" callback context data */
typedef struct H5VL_group_close_ctx_t {
    /* DOWN: API routine parameters */
    void *obj;
    const H5VL_class_t *cls;
    hid_t         dxpl_id;
    void **       req;
} H5VL_group_close_ctx_t;

/* Link create "common" callback context data */
typedef struct H5VL_link_create_ctx_t {
    /* DOWN: API routine parameters */
    H5VL_link_create_args_t *args;
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    hid_t                    lcpl_id;
    hid_t                    lapl_id;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_link_create_ctx_t;

/* Link copy "common" callback context data */
typedef struct H5VL_link_copy_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *src_loc_params;
    void *                   dst_obj;
    const H5VL_loc_params_t *dst_loc_params;
    const H5VL_class_t *           cls;
    hid_t                    lcpl_id;
    hid_t                    lapl_id;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_link_copy_ctx_t;

/* Link move "common" callback context data */
typedef struct H5VL_link_move_ctx_t {
    /* DOWN: API routine parameters */
    void *                   src_obj;
    const H5VL_loc_params_t *src_loc_params;
    void *                   dst_obj;
    const H5VL_loc_params_t *dst_loc_params;
    const H5VL_class_t *           cls;
    hid_t                    lcpl_id;
    hid_t                    lapl_id;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_link_move_ctx_t;

/* Link get "common" callback context data */
typedef struct H5VL_link_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    H5VL_link_get_args_t *   args;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_link_get_ctx_t;

/* Link specific "common" callback context data */
typedef struct H5VL_link_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *  loc_params;
    const H5VL_class_t *             cls;
    H5VL_link_specific_args_t *args;
    hid_t                      dxpl_id;
    void **                    req;

    /* UP: API routine return value */
    herr_t ret_value;
} H5VL_link_specific_ctx_t;

/* Link optional "common" callback context data */
typedef struct H5VL_link_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    H5VL_optional_args_t *   args;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_link_optional_ctx_t;

/* Object open "common" callback context data */
typedef struct H5VL_object_open_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    H5I_type_t *             opened_type;
    hid_t                    dxpl_id;
    void **                  req;

    /* UP: API routine return value */
    void *ret_value;
} H5VL_object_open_ctx_t;

/* Object copy "common" callback context data */
typedef struct H5VL_object_copy_ctx_t {
    /* DOWN: API routine parameters */
    void *                   src_obj;
    const H5VL_loc_params_t *src_loc_params;
    const char *             src_name;
    void *                   dst_obj;
    const H5VL_loc_params_t *dst_loc_params;
    const char *             dst_name;
    const H5VL_class_t *           cls;
    hid_t                    ocpypl_id;
    hid_t                    lcpl_id;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_object_copy_ctx_t;

/* Object get "common" callback context data */
typedef struct H5VL_object_get_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    H5VL_object_get_args_t * args;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_object_get_ctx_t;

/* Object specific "common" callback context data */
typedef struct H5VL_object_specific_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *    loc_params;
    const H5VL_class_t *               cls;
    H5VL_object_specific_args_t *args;
    hid_t                        dxpl_id;
    void **                      req;

    /* UP: API routine return value */
    herr_t ret_value;
} H5VL_object_specific_ctx_t;

/* Object optional "common" callback context data */
typedef struct H5VL_object_optional_ctx_t {
    /* DOWN: API routine parameters */
    const H5VL_loc_params_t *loc_params;
    const H5VL_class_t *           cls;
    H5VL_optional_args_t *   args;
    hid_t                    dxpl_id;
    void **                  req;
} H5VL_object_optional_ctx_t;

/********************/
/* Package Typedefs */
/********************/

/********************/
/* Local Prototypes */
/********************/
static herr_t H5VL__common_optional_op(hid_t id, H5I_type_t id_type, H5VL_reg_opt_oper_t reg_opt_op, H5VL_optional_args_t *args, hid_t dxpl_id, void **req, H5VL_object_t **_vol_obj_ptr);
static herr_t H5VL__common_api_op(void *obj, hid_t dxpl_id, H5VL_api_oper_t wrap_op, void *wrap_ctx);
static herr_t H5VL__get_wrap_ctx(const H5VL_connector_t *connector, void *obj, void **wrap_ctx);
static herr_t H5VL__free_wrap_ctx(const H5VL_connector_t *connector, void *wrap_ctx);
static void * H5VL__attr_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                const char *name, hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id,
                                hid_t dxpl_id, void **req);
static herr_t H5VL__attr_create_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__attr_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                              const char *name, hid_t aapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__attr_open_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_read(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, void *buf, hid_t dxpl_id,
                              void **req);
static herr_t H5VL__attr_read_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_write(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, const void *buf,
                               hid_t dxpl_id, void **req);
static herr_t H5VL__attr_write_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_get(void *obj, const H5VL_class_t *cls, H5VL_attr_get_args_t *args, hid_t dxpl_id,
                             void **req);
static herr_t H5VL__attr_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                  H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__attr_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                  hid_t dxpl_id, void **req);
static herr_t H5VL__attr_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__attr_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req);
static herr_t H5VL__attr_close_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__dataset_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                   const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id,
                                   hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_create_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                 const char *name, hid_t dapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_open_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_read(void *dset, const H5VL_class_t *cls, hid_t mem_type_id, hid_t mem_space_id,
                                 hid_t file_space_id, hid_t dxpl_id, void *buf, void **req);
static herr_t H5VL__dataset_read_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_write(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, hid_t mem_space_id,
                                  hid_t file_space_id, hid_t dxpl_id, const void *buf, void **req);
static herr_t H5VL__dataset_write_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_get(void *obj, const H5VL_class_t *cls, H5VL_dataset_get_args_t *args,
                                hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_specific(void *obj, const H5VL_class_t *cls, H5VL_dataset_specific_args_t *args,
                                     hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                     hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__dataset_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req);
static herr_t H5VL__dataset_close_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__datatype_commit(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                    const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id,
                                    hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_commit_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                  const char *name, hid_t tapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_open_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__datatype_get(void *obj, const H5VL_class_t *cls, H5VL_datatype_get_args_t *args,
                                 hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__datatype_specific(void *obj, const H5VL_class_t *cls, H5VL_datatype_specific_args_t *args,
                                      hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__datatype_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                      hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__datatype_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req);
static herr_t H5VL__datatype_close_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__file_create(const H5VL_class_t *cls, const char *name, unsigned flags, hid_t fcpl_id,
                                hid_t fapl_id, hid_t dxpl_id, void **req);
static void * H5VL__file_open(const H5VL_class_t *cls, const char *name, unsigned flags, hid_t fapl_id,
                              hid_t dxpl_id, void **req);
static herr_t H5VL__file_open_find_connector_cb(H5PL_type_t plugin_type, const void *plugin_info,
                                                void *op_data);
static herr_t H5VL__file_get(void *obj, const H5VL_class_t *cls, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__file_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__file_specific(void *obj, const H5VL_class_t *cls, H5VL_file_specific_args_t *args,
                                  hid_t dxpl_id, void **req);
static herr_t H5VL__file_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__file_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                  hid_t dxpl_id, void **req);
static herr_t H5VL__file_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__file_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req);
static herr_t H5VL__file_close_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__group_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                 const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id,
                                 void **req);
static herr_t H5VL__group_create_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__group_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                               const char *name, hid_t gapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__group_open_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__group_get(void *obj, const H5VL_class_t *cls, H5VL_group_get_args_t *args, hid_t dxpl_id,
                              void **req);
static herr_t H5VL__group_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__group_specific(void *obj, const H5VL_class_t *cls, H5VL_group_specific_args_t *args,
                                   hid_t dxpl_id, void **req);
static herr_t H5VL__group_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__group_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args,
                                   hid_t dxpl_id, void **req);
static herr_t H5VL__group_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__group_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req);
static herr_t H5VL__group_close_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                                const H5VL_class_t *cls, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id,
                                void **req);
static herr_t H5VL__link_create_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, void *dst_obj,
                              const H5VL_loc_params_t *dst_loc_params, const H5VL_class_t *cls, hid_t lcpl_id,
                              hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__link_copy_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_move(void *src_obj, const H5VL_loc_params_t *loc_params1, void *dst_obj,
                              const H5VL_loc_params_t *loc_params2, const H5VL_class_t *cls, hid_t lcpl_id,
                              hid_t lapl_id, hid_t dxpl_id, void **req);
static herr_t H5VL__link_move_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_get(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                             H5VL_link_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__link_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                  H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__link_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__link_optional(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                  H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__link_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static void * H5VL__object_open(void *obj, const H5VL_loc_params_t *params, const H5VL_class_t *cls,
                                H5I_type_t *opened_type, hid_t dxpl_id, void **req);
static herr_t H5VL__object_open_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name,
                                void *dst_obj, const H5VL_loc_params_t *dst_loc_params, const char *dst_name,
                                const H5VL_class_t *cls, hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id,
                                void **req);
static herr_t H5VL__object_copy_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__object_get(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                               H5VL_object_get_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__object_get_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__object_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                    H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__object_specific_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__object_optional(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                                    H5VL_optional_args_t *args, hid_t dxpl_id, void **req);
static herr_t H5VL__object_optional_api_op(void *obj, hbool_t new_api_ctx, void *ctx);
static herr_t H5VL__introspect_opt_query(void *obj, const H5VL_class_t *cls, H5VL_subclass_t subcls,
                                         int opt_type, uint64_t *flags);
static herr_t H5VL__request_wait(void *req, const H5VL_class_t *cls, uint64_t timeout,
                                 H5VL_request_status_t *status);
static herr_t H5VL__request_notify(void *req, const H5VL_class_t *cls, H5VL_request_notify_t cb, void *ctx);
static herr_t H5VL__request_cancel(void *req, const H5VL_class_t *cls, H5VL_request_status_t *status);
static herr_t H5VL__request_specific(void *req, const H5VL_class_t *cls, H5VL_request_specific_args_t *args);
static herr_t H5VL__request_optional(void *req, const H5VL_class_t *cls, H5VL_optional_args_t *args);
static herr_t H5VL__request_free(void *req, const H5VL_class_t *cls);
static herr_t H5VL__blob_put(void *obj, const H5VL_class_t *cls, const void *buf, size_t size, void *blob_id,
                             void *ctx);
static herr_t H5VL__blob_get(void *obj, const H5VL_class_t *cls, const void *blob_id, void *buf, size_t size,
                             void *ctx);
static herr_t H5VL__blob_specific(void *obj, const H5VL_class_t *cls, void *blob_id,
                                  H5VL_blob_specific_args_t *args);
static herr_t H5VL__blob_optional(void *obj, const H5VL_class_t *cls, void *blob_id,
                                  H5VL_optional_args_t *args);
static herr_t H5VL__token_cmp(void *obj, const H5VL_class_t *cls, const H5O_token_t *token1,
                              const H5O_token_t *token2, int *cmp_value);
static herr_t H5VL__token_to_str(void *obj, H5I_type_t obj_type, const H5VL_class_t *cls,
                                 const H5O_token_t *token, char **token_str);
static herr_t H5VL__token_from_str(void *obj, H5I_type_t obj_type, const H5VL_class_t *cls,
                                   const char *token_str, H5O_token_t *token);
static herr_t H5VL__optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id,
                             void **req);

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
 * Function:    H5VLinitialize
 *
 * Purpose:     Calls the connector-specific callback to initialize the connector.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLinitialize(hid_t connector_id, hid_t vipl_id)
{
    H5VL_connector_t *connector;      /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "ii", connector_id, vipl_id);

    /* Check args */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Invoke class' callback, if there is one */
    if (connector->cls->initialize && connector->cls->initialize(vipl_id) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "VOL connector did not initialize")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLinitialize() */

/*-------------------------------------------------------------------------
 * Function:    H5VLterminate
 *
 * Purpose:     Calls the connector-specific callback to terminate the connector.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLterminate(hid_t connector_id)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE1("e", "i", connector_id);

    /* Check args */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Invoke class' callback, if there is one */
    if (connector->cls->terminate && connector->cls->terminate() < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "VOL connector did not terminate cleanly")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLterminate() */

/*---------------------------------------------------------------------------
 * Function:    H5VLget_cap_flags
 *
 * Purpose:     Retrieves the capability flag for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLget_cap_flags(hid_t connector_id, unsigned *cap_flags /*out*/)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "ix", connector_id, cap_flags);

    /* Check args */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Retrieve capability flags */
    if (cap_flags)
        *cap_flags = connector->cls->cap_flags;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLget_cap_flags */

/*---------------------------------------------------------------------------
 * Function:    H5VLget_value
 *
 * Purpose:     Retrieves the 'value' for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLget_value(hid_t connector_id, H5VL_class_value_t *value /*out*/)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "ix", connector_id, value);

    /* Check args */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Retrieve connector value */
    if (value)
        *value = connector->cls->value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLget_value */

/*-------------------------------------------------------------------------
 * Function:    H5VL__common_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__common_optional_op(hid_t id, H5I_type_t id_type, H5VL_reg_opt_oper_t reg_opt_op,
                         H5VL_optional_args_t *args, hid_t dxpl_id, void **req, H5VL_object_t **_vol_obj_ptr)
{
    void *obj;                  /* Object to operate on */
    H5VL_object_t * tmp_vol_obj = NULL;                                         /* Object for id */
    H5VL_object_t **vol_obj_ptr = (_vol_obj_ptr ? _vol_obj_ptr : &tmp_vol_obj); /* Ptr to object ptr for id */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t          ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check ID type & get VOL object */
    if (NULL == (*vol_obj_ptr = H5I_object_verify(id, id_type)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid identifier")

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(*vol_obj_ptr) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = ((*vol_obj_ptr)->obj_type == H5VL_OBJ_FILE) ? (*vol_obj_ptr)->container->object : (*vol_obj_ptr)->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (*reg_opt_op)(obj, (*vol_obj_ptr)->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute generic 'optional' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__common_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__common_api_op
 *
 * Purpose:     Provide common wrapping for VOL callback API routines.
 *
 * Note:        When the 'new API context' property is set, the 'obj' pointer
 *              is actually a VOL object pointer (H5VL_object_t *).
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__common_api_op(void *obj, hid_t dxpl_id, H5VL_api_oper_t wrap_op, void *wrap_ctx)
{
    H5P_genplist_t *dxpl_plist  = NULL;    /* DXPL property list pointer */
    hbool_t         new_api_ctx = FALSE;   /* Whether to start a new API context */
    hbool_t         api_pushed  = FALSE;   /* Indicate that a new API context was pushed */
    hbool_t         prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t         new_api_ctx_prop_reset = FALSE; /* Whether the 'new API context' property was reset */
    herr_t          ret_value   = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check for non-default DXPL */
    if (!(H5P_DEFAULT == dxpl_id || H5P_DATASET_XFER_DEFAULT == dxpl_id)) {
        /* Check for 'new API context' flag */
        if (NULL == (dxpl_plist = H5P_object_verify(dxpl_id, H5P_DATASET_XFER)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a dataset transfer property list")
        if (H5P_get(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &new_api_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to get value")

        /* Start a new API context, if requested */
        if (new_api_ctx) {
            H5VL_object_t *vol_obj;             /* VOL object */
            hbool_t reset_api_ctx = FALSE; /* Flag to reset the 'new API context' */

            /* Push new API context */
            if (H5CX_push() < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set API context")
            api_pushed = TRUE;

            /* Object pointer is actually a VOL object pointer, when 'new API context' flag is used */
            vol_obj = obj;

            /* Set container info in API context */
            if (H5VL_set_primary_container_ctx(vol_obj) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
            prim_container_ctx_set = TRUE;

            /* Get actual object */
            obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

            /* Reset 'new API context' flag for next layer down */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &reset_api_ctx) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "unable to set value")
            new_api_ctx_prop_reset = TRUE;
        } /* end if */
    }     /* end if */

    /* Call the corresponding internal common wrapper routine */
    if ((*wrap_op)(obj, new_api_ctx, wrap_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation callback")

done:
    /* Undo earlier actions, if performed */
    if (new_api_ctx) {
        if (new_api_ctx_prop_reset) {
            hbool_t undo_api_ctx = TRUE; /* Flag to reset the 'new API context' */

            /* Undo change to 'new API context' flag */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &undo_api_ctx) < 0)
                HDONE_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "unable to set value")
        } /* end if */
        if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")
        if (api_pushed)
            (void)H5CX_pop(FALSE);
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__common_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_copy_connector_info
 *
 * Purpose:     Copy the VOL info for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_copy_connector_info(const H5VL_connector_t *connector, void **dst_info, const void *src_info)
{
    void * new_connector_info = NULL;    /* Copy of connector info */
    herr_t ret_value          = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(connector);

    /* Check for actual source info */
    if (src_info) {
        /* Allow the connector to copy or do it ourselves */
        if (connector->cls->info_cls.copy) {
            if (NULL == (new_connector_info = (connector->cls->info_cls.copy)(src_info)))
                HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "connector info copy callback failed")
        } /* end if */
        else if (connector->cls->info_cls.size > 0) {
            if (NULL == (new_connector_info = H5MM_malloc(connector->cls->info_cls.size)))
                HGOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "connector info allocation failed")
            H5MM_memcpy(new_connector_info, src_info, connector->cls->info_cls.size);
        } /* end else-if */
        else
            HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "no way to copy connector info")
    } /* end if */

    /* Set the connector info for the copy */
    *dst_info = new_connector_info;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_copy_connector_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VLcopy_connector_info
 *
 * Purpose:     Copies a VOL connector's info object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLcopy_connector_info(hid_t connector_id, void **dst_vol_info, void *src_vol_info)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "i**x*x", connector_id, dst_vol_info, src_vol_info);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Copy the VOL connector's info object */
    if (H5VL_copy_connector_info(connector, dst_vol_info, src_vol_info) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "unable to copy VOL connector info object")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLcopy_connector_info() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__cmp_connector_info_cls
 *
 * Purpose:     Compare VOL info for a connector.  Sets *cmp_value to
 *              positive if INFO1 is greater than INFO2, negative if
 *              INFO2 is greater than INFO1 and zero if INFO1 and
 *              INFO2 are equal.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__cmp_connector_info_cls(const H5VL_class_t *cls, int *cmp_value, const void *info1, const void *info2)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity checks */
    HDassert(cls);
    HDassert(cmp_value);

    /* Take care of cases where one or both pointers is NULL */
    if (info1 == NULL && info2 != NULL) {
        *cmp_value = -1;
        HGOTO_DONE(SUCCEED);
    } /* end if */
    if (info1 != NULL && info2 == NULL) {
        *cmp_value = 1;
        HGOTO_DONE(SUCCEED);
    } /* end if */
    if (info1 == NULL && info2 == NULL) {
        *cmp_value = 0;
        HGOTO_DONE(SUCCEED);
    } /* end if */

    /* Use the class's info comparison routine to compare the info objects,
     * if there is a a callback, otherwise just compare the info objects as
     * memory buffers
     */
    if (cls->info_cls.cmp) {
        if ((cls->info_cls.cmp)(cmp_value, info1, info2) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare connector info")
    } /* end if */
    else {
        HDassert(cls->info_cls.size > 0);
        *cmp_value = HDmemcmp(info1, info2, cls->info_cls.size);
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__cmp_connector_info_cls() */

/*---------------------------------------------------------------------------
 * Function:    H5VLcmp_connector_info
 *
 * Purpose:     Compares two connector info objects
 *
 * Note:	Both info objects must be from the same VOL connector class
 *
 * Return:      Success:    Non-negative, with *cmp set to positive if
 *			    info1 is greater than info2, negative if info2
 *			    is greater than info1 and zero if info1 and info2
 *			    are equal.
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLcmp_connector_info(int *cmp, hid_t connector_id, const void *info1, const void *info2)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE4("e", "*Isi*x*x", cmp, connector_id, info1, info2);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Compare the two VOL connector info objects */
    if (cmp)
        H5VL__cmp_connector_info_cls(connector->cls, cmp, info1, info2);

done:
    FUNC_LEAVE_API(ret_value)
} /* H5VLcmp_connector_info() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_free_connector_info
 *
 * Purpose:     Free VOL info for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_free_connector_info(hid_t connector_id, const void *info)
{
    H5VL_connector_t *connector;       /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(connector_id > 0);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Only free info object, if it's non-NULL */
    if (info) {
        /* Allow the connector to free info or do it ourselves */
        if (connector->cls->info_cls.free) {
            /* Cast through uintptr_t to de-const memory */
            if ((connector->cls->info_cls.free)((void *)(uintptr_t)info) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "connector info free request failed")
        }
        else
            H5MM_xfree_const(info);
    }

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_free_connector_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VLfree_connector_info
 *
 * Purpose:     Free VOL connector info object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLfree_connector_info(hid_t connector_id, void *info)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "i*x", connector_id, info);

    /* Free the VOL connector info object */
    if (H5VL_free_connector_info(connector_id, info) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL connector info object")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLfree_connector_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VLconnector_info_to_str
 *
 * Purpose:     Serialize a connector's info into a string
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLconnector_info_to_str(const void *info, hid_t connector_id, char **str)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xi**s", info, connector_id, str);

    /* Only serialize info object, if it's non-NULL */
    if (info) {
        H5VL_connector_t *connector; /* VOL connector's class struct */

        /* Check args and get class pointer */
        if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

        /* Allow the connector to serialize info */
        if (connector->cls->info_cls.to_str) {
            if ((connector->cls->info_cls.to_str)(info, str) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSERIALIZE, FAIL, "can't serialize connector info")
        } /* end if */
        else
            *str = NULL;
    } /* end if */
    else
        *str = NULL;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLconnector_info_to_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__connector_str_to_info
 *
 * Purpose:     Deserializes a string into a connector's info object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 * Programmer:  Quincey Koziol
 *              March 2, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__connector_str_to_info(const char *str, hid_t connector_id, void **info)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Only deserialize string, if it's non-NULL */
    if (str) {
        H5VL_connector_t *connector; /* VOL connector's class struct */

        /* Check args and get class pointer */
        if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a VOL connector ID")

        /* Allow the connector to deserialize info */
        if (connector->cls->info_cls.from_str) {
            if ((connector->cls->info_cls.from_str)(str, info) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTUNSERIALIZE, FAIL, "can't deserialize connector info")
        } /* end if */
        else
            *info = NULL;
    } /* end if */
    else
        *info = NULL;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__connector_str_to_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VLconnector_str_to_info
 *
 * Purpose:     Deserialize a string into a connector's info
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLconnector_str_to_info(const char *str, hid_t connector_id, void **info /*out*/)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*six", str, connector_id, info);

    /* Call internal routine */
    if (H5VL__connector_str_to_info(str, connector_id, info) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTDECODE, FAIL, "can't deserialize connector info")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLconnector_str_to_info() */

/*---------------------------------------------------------------------------
 * Function:    H5VLget_object
 *
 * Purpose:     Retrieves an underlying object.
 *
 * Return:      Success:    Non-NULL
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
void *
H5VLget_object(void *obj, hid_t connector_id)
{
    H5VL_connector_t *connector;    /* VOL connector struct */
    void *        ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE2("*x", "*xi", obj, connector_id);

    /* Check args */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Check for 'get_object' callback in connector */
    if (connector->cls->wrap_cls.get_object)
        ret_value = (connector->cls->wrap_cls.get_object)(obj);
    else
        ret_value = obj;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLget_object */

/*-------------------------------------------------------------------------
 * Function:    H5VL__get_wrap_ctx
 *
 * Purpose:     Retrieve the VOL object wrapping context for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__get_wrap_ctx(const H5VL_connector_t *connector, void *obj, void **wrap_ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity checks */
    HDassert(connector);
    HDassert(obj);
    HDassert(wrap_ctx);

    /* Allow the connector to copy or do it ourselves */
    if (connector->cls->wrap_cls.get_wrap_ctx) {
        /* Sanity check */
        HDassert(connector->cls->wrap_cls.free_wrap_ctx);

        /* Invoke connector's callback */
        if ((connector->cls->wrap_cls.get_wrap_ctx)(obj, wrap_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "connector wrap context callback failed")
    } /* end if */
    else
        *wrap_ctx = NULL;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__get_wrap_ctx() */

/*---------------------------------------------------------------------------
 * Function:    H5VLget_wrap_ctx
 *
 * Purpose:     Get a VOL connector's object wrapping context
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLget_wrap_ctx(void *obj, hid_t connector_id, void **wrap_ctx /*out*/)
{
    H5VL_connector_t *connector;                 /* VOL connector's class struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xix", obj, connector_id, wrap_ctx);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Get the VOL connector's object wrapper */
    if (H5VL__get_wrap_ctx(connector, obj, wrap_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to retrieve VOL connector object wrap context")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLget_wrap_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__wrap_object
 *
 * Purpose:     Wrap an object with connector
 *
 * Return:      Success:    Non-NULL
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL__wrap_object(const H5VL_connector_t *connector, void *wrap_ctx, void *obj, H5I_type_t obj_type)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity checks */
    HDassert(connector);
    HDassert(obj);

    /* Only wrap object if there's a wrap context */
    if (wrap_ctx) {
        /* Ask the connector to wrap the object */
        if (NULL == (ret_value = (connector->cls->wrap_cls.wrap_object)(obj, obj_type, wrap_ctx)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't wrap object")
    } /* end if */
    else
        ret_value = obj;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__wrap_object() */

/*---------------------------------------------------------------------------
 * Function:    H5VLwrap_object
 *
 * Purpose:     Asks a connector to wrap an underlying object.
 *
 * Return:      Success:    Non-NULL
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
void *
H5VLwrap_object(void *obj, H5I_type_t obj_type, hid_t connector_id, void *wrap_ctx)
{
    H5VL_connector_t *connector;    /* VOL connector struct */
    void *        ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE4("*x", "*xIti*x", obj, obj_type, connector_id, wrap_ctx);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Wrap the object */
    if (NULL == (ret_value = H5VL__wrap_object(connector, wrap_ctx, obj, obj_type)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "unable to wrap object")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLwrap_object */

/*-------------------------------------------------------------------------
 * Function:    H5VL__unwrap_object
 *
 * Purpose:     Unwrap an object from connector
 *
 * Return:      Success:    Non-NULL
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VL__unwrap_object(const H5VL_connector_t *connector, void *obj)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Sanity checks */
    HDassert(connector);
    HDassert(obj);

    /* Only unwrap object if there's an unwrap callback */
    if (connector->cls->wrap_cls.wrap_object) {
        /* Ask the connector to unwrap the object */
        if (NULL == (ret_value = (connector->cls->wrap_cls.unwrap_object)(obj)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't unwrap object")
    } /* end if */
    else
        ret_value = obj;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__unwrap_object() */

/*---------------------------------------------------------------------------
 * Function:    H5VLunwrap_object
 *
 * Purpose:     Unwrap an object from connector
 *
 * Return:      Success:    Non-NULL
 *              Failure:    NULL
 *
 *---------------------------------------------------------------------------
 */
void *
H5VLunwrap_object(void *obj, hid_t connector_id)
{
    H5VL_connector_t *connector;    /* VOL connector struct */
    void *        ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE2("*x", "*xi", obj, connector_id);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Unwrap the object */
    if (NULL == (ret_value = H5VL__unwrap_object(connector, obj)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "unable to unwrap object")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLunwrap_object */

/*-------------------------------------------------------------------------
 * Function:    H5VL__free_wrap_ctx
 *
 * Purpose:     Free object wrapping context for a connector
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__free_wrap_ctx(const H5VL_connector_t *connector, void *wrap_ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(connector);

    /* Only free wrap context, if it's non-NULL */
    if (wrap_ctx)
        /* Free the connector's object wrapping context */
        if ((connector->cls->wrap_cls.free_wrap_ctx)(wrap_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "connector wrap context free request failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__free_wrap_ctx() */

/*---------------------------------------------------------------------------
 * Function:    H5VLfree_wrap_ctx
 *
 * Purpose:     Release a VOL connector's object wrapping context
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLfree_wrap_ctx(void *wrap_ctx, hid_t connector_id)
{
    H5VL_connector_t *connector;                 /* VOL connector's class struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "*xi", wrap_ctx, connector_id);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Release the VOL connector's object wrapper */
    if (H5VL__free_wrap_ctx(connector, wrap_ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to release VOL connector object wrap context")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* H5VLfree_wrap_ctx() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_create
 *
 * Purpose:     Creates an attribute through the VOL
 *
 * Return:      Success: Pointer to the new attribute
 *              Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__attr_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                  hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.create)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'attr create' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->attr_cls.create)(obj, loc_params, name, type_id, space_id, acpl_id, aapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "attribute create failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_attr_create
 *
 * Purpose:     Creates an attribute through the VOL
 *
 * Return:      Success: Pointer to VOL object for new attribute
 *              Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_attr_create(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                 hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *attr = NULL;          /* Attribute object from VOL connector */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (NULL == (attr = H5VL__attr_create(obj, loc_params, vol_obj->container->connector->cls, name, type_id, space_id, acpl_id, aapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "attribute create failed")

    /* Create VOL object for attribute */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_ATTR, attr, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for attribute")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (attr && H5VL__attr_close(attr, vol_obj->container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "attribute close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_create_api_op
 *
 * Purpose:     Callback for common API wrapper to create an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_create_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_attr_create_ctx_t *ctx       = (H5VL_attr_create_ctx_t *)_ctx; /* Get pointer to context */
    void *attr;         /* Attribute from VOL connector */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (attr = H5VL__attr_create(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->type_id, ctx->space_id, ctx->acpl_id, ctx->aapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "unable to create attribute")

    /* Set up object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_ATTR, attr)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for attribute")
    } /* end if */
    else
        ctx->ret_value = attr;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_create_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_create
 *
 * Purpose:     Creates an attribute
 *
 * Return:      Success:    Pointer to the new attribute
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLattr_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id, hid_t dxpl_id,
                void **req /*out*/)
{
    H5VL_attr_create_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *         connector;              /* VOL connector struct */
    void *                 ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE10("*x", "*x*#i*siiiiix", obj, loc_params, connector_id, name, type_id, space_id, acpl_id, aapl_id,
              dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.type_id    = type_id;
    ctx.space_id   = space_id;
    ctx.acpl_id    = acpl_id;
    ctx.aapl_id    = aapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_create_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_open
 *
 * Purpose:	Opens an attribute through the VOL
 *
 * Return:      Success: Pointer to the attribute
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__attr_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'attr open' method")

    /* Call the corresponding VOL open callback */
    if (NULL == (ret_value = (cls->attr_cls.open)(obj, loc_params, name, aapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "attribute open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_open
 *
 * Purpose:	Opens an attribute through the VOL
 *
 * Return:      Success: Pointer to VOL object for the attribute
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_attr_open(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
               hid_t aapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *attr = NULL;          /* Attribute object from VOL connector */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (NULL == (attr = H5VL__attr_open(obj, loc_params, vol_obj->container->connector->cls, name, aapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "attribute open failed")

    /* Create VOL object for attribute */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_ATTR, attr, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for attribute")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (attr && H5VL__attr_close(attr, vol_obj->container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "attribute close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_open_api_op
 *
 * Purpose:     Callback for common API wrapper to open an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_open_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_attr_open_ctx_t *ctx       = (H5VL_attr_open_ctx_t *)_ctx; /* Get pointer to context */
    void *attr;         /* Attribute from VOL connector */
    herr_t                ret_value = SUCCEED;                      /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (attr = H5VL__attr_open(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->aapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, FAIL, "unable to open attribute")

    /* Set up attribute object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_ATTR, attr)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for attribute")
    } /* end if */
    else
        ctx->ret_value = attr;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_open_api_op() */

/*-------------------------------------------------------------------------
 * Function:	H5VLattr_open
 *
 * Purpose:     Opens an attribute
 *
 * Return:      Success:    Pointer to the attribute
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLattr_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
              hid_t aapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_attr_open_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *         connector;              /* VOL connector struct */
    void *               ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE7("*x", "*x*#i*siix", obj, loc_params, connector_id, name, aapl_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.aapl_id    = aapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_open_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_read
 *
 * Purpose:	Reads data from attr through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_read(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.read)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr read' method")

    /* Call the corresponding VOL callback */
    if ((cls->attr_cls.read)(obj, mem_type_id, buf, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "attribute read failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_read() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_read
 *
 * Purpose:	Reads data from attr through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_read(const H5VL_object_t *vol_obj, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_read(obj, vol_obj->container->connector->cls, mem_type_id, buf, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "attribute read failed")

done:
    /* Reset container info in API context */
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_read() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_read_api_op
 *
 * Purpose:     Callback for common API wrapper to read an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_read_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_attr_read_ctx_t *ctx       = (H5VL_attr_read_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                ret_value = SUCCEED;                      /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_read(obj, ctx->cls, ctx->mem_type_id, ctx->buf, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "unable to read attribute")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_read_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_read
 *
 * Purpose:     Reads data from an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_read(void *obj, hid_t connector_id, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_attr_read_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *   connector;                 /* VOL connector struct */
    herr_t               ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*xii*xix", obj, connector_id, mem_type_id, buf, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls         = connector->cls;
    ctx.mem_type_id = mem_type_id;
    ctx.buf         = buf;
    ctx.dxpl_id     = dxpl_id;
    ctx.req         = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_read_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_read() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_write
 *
 * Purpose:	Writes data to attr through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_write(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, const void *buf, hid_t dxpl_id,
                 void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.write)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr write' method")

    /* Call the corresponding VOL callback */
    if ((cls->attr_cls.write)(obj, mem_type_id, buf, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_WRITEERROR, FAIL, "write failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_write() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_write
 *
 * Purpose:	Writes data to attr through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_write(const H5VL_object_t *vol_obj, hid_t mem_type_id, const void *buf, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    if (H5VL__set_dst_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_write(obj, vol_obj->container->connector->cls, mem_type_id, buf, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_WRITEERROR, FAIL, "write failed")

done:
    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'dst' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_write() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_write_api_op
 *
 * Purpose:     Callback for common API wrapper to write an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_write_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_attr_write_ctx_t *ctx       = (H5VL_attr_write_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                 ret_value = SUCCEED;                       /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_write(obj, ctx->cls, ctx->mem_type_id, ctx->buf, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_WRITEERROR, FAIL, "unable to write attribute")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_write_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_write
 *
 * Purpose:     Writes data to an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_write(void *obj, hid_t connector_id, hid_t mem_type_id, const void *buf, hid_t dxpl_id,
               void **req /*out*/)
{
    H5VL_attr_write_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*xii*xix", obj, connector_id, mem_type_id, buf, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls         = connector->cls;
    ctx.mem_type_id = mem_type_id;
    ctx.buf         = buf;
    ctx.dxpl_id     = dxpl_id;
    ctx.req         = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_write_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_write() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_get
 *
 * Purpose:	Get information about the attribute through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_get(void *obj, const H5VL_class_t *cls, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr get' method")

    /* Call the corresponding VOL callback */
    if ((cls->attr_cls.get)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "attribute get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_get
 *
 * Purpose:	Get information about the attribute through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_get(const H5VL_object_t *vol_obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_get(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "attribute get failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_attr_get_ctx_t *ctx       = (H5VL_attr_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t               ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_get(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to get attribute information")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_get
 *
 * Purpose:     Gets information about the attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_get(void *obj, hid_t connector_id, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_attr_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")
    if (NULL == args)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid argument struct")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_specific
 *
 * Purpose:	Specific operation on attributes through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                    H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr specific' method")

    /* Call the corresponding VOL callback */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (cls->attr_cls.specific)(obj, loc_params, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'specific' callback");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_specific
 *
 * Purpose:	Specific operation on attributes through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_specific(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                   H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = H5VL__attr_specific(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'specific' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_specific_api_op
 *
 * Purpose:     Callback for common API wrapper for attribute specific operation
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_attr_specific_ctx_t *ctx = (H5VL_attr_specific_ctx_t *)_ctx; /* Get pointer to context */

    FUNC_ENTER_STATIC_NOERR

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    /* (Must capture return value from callback, for iterators) */
    ctx->ret_value = H5VL__attr_specific(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5VL__attr_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_specific
 *
 * Purpose:     Performs a connector-specific operation on an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                  H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_attr_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = -1;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

    /* Must return value from callback, for iterators */
    if ((ret_value = ctx.ret_value) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'specific' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__attr_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr optional' method")

    /* Call the corresponding VOL callback */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (cls->attr_cls.optional)(obj, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'optional' callback");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_attr_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = H5VL__attr_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'optional' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_optional_api_op
 *
 * Purpose:     Callback for common API wrapper for attribute optional operation
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_attr_optional_ctx_t *ctx = (H5VL_attr_optional_ctx_t *)_ctx; /* Get pointer to context */

    FUNC_ENTER_STATIC_NOERR

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    /* (Must capture return value from callback, for iterators) */
    ctx->ret_value = H5VL__attr_optional(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5VL__attr_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_optional
 *
 * Purpose:     Performs an optional connector-specific operation on an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                  void **req /*out*/)
{
    H5VL_attr_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls       = connector->cls;
    ctx.args      = args;
    ctx.dxpl_id   = dxpl_id;
    ctx.req       = req;
    ctx.ret_value = -1;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

    /* Must return value from callback, for iterators */
    if ((ret_value = ctx.ret_value) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute attribute 'optional' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t attr_id,
                     H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5VL_object_t *vol_obj   = NULL;            /* Attribute VOL object */
    void *         token     = NULL;            /* Request token for async operation        */
    void **        token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    herr_t         ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*s*sIui*!ii", app_file, app_func, app_line, attr_id, args, dxpl_id, es_id);

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Call the common VOL connector optional routine */
    if ((ret_value = H5VL__common_optional_op(attr_id, H5I_ATTR, H5VL__attr_optional, args, dxpl_id, token_ptr, &vol_obj)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute attribute 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token,
                        H5ARG_TRACE7(__func__, "*s*sIui*!ii", app_file, app_func, app_line, attr_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLattr_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_close
 *
 * Purpose:     Closes an attribute through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->attr_cls.close)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'attr close' method")

    /* Call the corresponding VOL callback */
    if ((cls->attr_cls.close)(obj, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "attribute close failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_attr_close
 *
 * Purpose:     Closes an attribute through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_attr_close(const H5VL_object_t *vol_obj, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_close(obj, vol_obj->container->connector->cls, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "attribute close failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_attr_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__attr_close_api_op
 *
 * Purpose:     Callback for common API wrapper to close an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__attr_close_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_attr_close_ctx_t *ctx       = (H5VL_attr_close_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                 ret_value = SUCCEED;                       /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__attr_close(obj, ctx->cls, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "unable to close attribute")

    /* Close VOL object, if API context */
    if(new_api_ctx)
        if (H5VL_free_object(ctx->obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to free attribute VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__attr_close_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLattr_close
 *
 * Purpose:     Closes an attribute
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLattr_close(void *obj, hid_t connector_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_attr_close_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiix", obj, connector_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.obj     = obj;
    ctx.cls     = connector->cls;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__attr_close_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLattr_close() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_create
 *
 * Purpose:	Creates a dataset through the VOL
 *
 * Return:      Success: Pointer to new dataset
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__dataset_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                     const char *name, hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id,
                     hid_t dapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.create)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'dataset create' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->dataset_cls.create)(obj, loc_params, name, lcpl_id, type_id, space_id, dcpl_id, dapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "dataset create failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_create
 *
 * Purpose:	Creates a dataset through the VOL
 *
 * Return:      Success: Pointer to VOL object for dataset
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_dataset_create(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                    hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id,
                    void **req)
{
    void *obj;                  /* Object to operate on */
    void *dset = NULL;          /* Dataset object from VOL connector */
    H5VL_container_t *dset_container = NULL;    /* Container for dataset */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    /* (For converting fill-values to disk form) */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    /* (For converting fill-values to disk form) */
    if (H5VL__set_dst_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for dataset */
    dset_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (dset = H5VL__dataset_create(obj, loc_params, vol_obj->container->connector->cls, name, lcpl_id, type_id, space_id, dcpl_id, dapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "dataset create failed")

    /* Get container for dataset (accommodates external links) */
    if (NULL == (dset_container = H5VL__get_container_for_obj(dset, H5I_DATASET, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for dataset")

    /* Create VOL object for dataset */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_DATASET, dset, dset_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for dataset")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (dset && dset_container && H5VL__dataset_close(dset, dset_container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "dataset close failed")

    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset 'dst' VOL container info")
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_create_api_op
 *
 * Purpose:     Callback for common API wrapper to create a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_create_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_dataset_create_ctx_t *ctx       = (H5VL_dataset_create_ctx_t *)_ctx; /* Get pointer to context */
    void *dset;         /* Dataset from VOL connector */
    herr_t                     ret_value = SUCCEED;                           /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (dset = H5VL__dataset_create(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->lcpl_id, ctx->type_id, ctx->space_id, ctx->dcpl_id, ctx->dapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "unable to create dataset")

    /* Set up object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_DATASET, dset)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for dataset")
    } /* end if */
    else
        ctx->ret_value = dset;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_create_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_create
 *
 * Purpose:     Creates a dataset
 *
 * Return:      Success:    Pointer to the new dataset
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdataset_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                   hid_t lcpl_id, hid_t type_id, hid_t space_id, hid_t dcpl_id, hid_t dapl_id, hid_t dxpl_id,
                   void **req /*out*/)
{
    H5VL_dataset_create_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *        connector;              /* VOL connector struct */
    void *                    ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE11("*x", "*x*#i*siiiiiix", obj, loc_params, connector_id, name, lcpl_id, type_id, space_id,
              dcpl_id, dapl_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.lcpl_id    = lcpl_id;
    ctx.type_id    = type_id;
    ctx.space_id   = space_id;
    ctx.dcpl_id    = dcpl_id;
    ctx.dapl_id    = dapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_create_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_open
 *
 * Purpose:	Opens a dataset through the VOL
 *
 * Return:      Success: Pointer to dataset
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                   hid_t dapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'dataset open' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->dataset_cls.open)(obj, loc_params, name, dapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "dataset open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_open
 *
 * Purpose:	Opens a dataset through the VOL
 *
 * Return:      Success: Pointer to VOL object for dataset
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_dataset_open(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                  hid_t dapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *dset = NULL;          /* Dataset object from VOL connector */
    H5VL_container_t *dset_container = NULL;    /* Container for dataset */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for dataset */
    dset_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (dset = H5VL__dataset_open(obj, loc_params, dset_container->connector->cls, name, dapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "dataset open failed")

    /* Get container for dataset (accommodates external links) */
    if (NULL == (dset_container = H5VL__get_container_for_obj(dset, H5I_DATASET, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for dataset")

    /* Create VOL object for dataset */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_DATASET, dset, dset_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for dataset")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (dset && dset_container && H5VL__dataset_close(dset, dset_container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "dataset close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_open_api_op
 *
 * Purpose:     Callback for common API wrapper to open a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_open_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_dataset_open_ctx_t *ctx       = (H5VL_dataset_open_ctx_t *)_ctx; /* Get pointer to context */
    void *dset = NULL;          /* Dataset object from VOL connector */
    herr_t                   ret_value = SUCCEED;                         /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (dset = H5VL__dataset_open(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->dapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, FAIL, "unable to open dataset")

    /* Set up dataset object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_DATASET, dset)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for dataset")
    } /* end if */
    else
        ctx->ret_value = dset;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_open_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_open
 *
 * Purpose:     Opens a dataset
 *
 * Return:      Success:    Pointer to the new dataset
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdataset_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                 hid_t dapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_dataset_open_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *      connector;              /* VOL connector struct */
    void *                  ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE7("*x", "*x*#i*siix", obj, loc_params, connector_id, name, dapl_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.dapl_id    = dapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_open_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_read
 *
 * Purpose:	Reads data from dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_read(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, hid_t mem_space_id,
                   hid_t file_space_id, hid_t dxpl_id, void *buf, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.read)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset read' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.read)(obj, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "dataset read failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_read() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_read
 *
 * Purpose:	Reads data from dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_read(const H5VL_object_t *vol_obj, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                  hid_t dxpl_id, void *buf, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    /* (For converting fill-values to disk form) */
    if (H5VL__set_dst_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_read(obj, vol_obj->container->connector->cls, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "dataset read failed")

done:
    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'dst' VOL container info")
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_read() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_read_api_op
 *
 * Purpose:     Callback for common API wrapper to read a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_read_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_dataset_read_ctx_t *ctx       = (H5VL_dataset_read_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                   ret_value = SUCCEED;                         /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_read(obj, ctx->cls, ctx->mem_type_id, ctx->mem_space_id, ctx->file_space_id, ctx->dxpl_id, ctx->buf, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to read dataset")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_read_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_read
 *
 * Purpose:     Reads data from a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_read(void *obj, hid_t connector_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                 hid_t dxpl_id, void *buf, void **req /*out*/)
{
    H5VL_dataset_read_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *      connector;                 /* VOL connector struct */
    herr_t                  ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE8("e", "*xiiiii*xx", obj, connector_id, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf,
             req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls           = connector->cls;
    ctx.mem_type_id   = mem_type_id;
    ctx.mem_space_id  = mem_space_id;
    ctx.file_space_id = file_space_id;
    ctx.buf           = buf;
    ctx.dxpl_id       = dxpl_id;
    ctx.req           = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_read_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_read() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_write
 *
 * Purpose:	Writes data from dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_write(void *obj, const H5VL_class_t *cls, hid_t mem_type_id, hid_t mem_space_id,
                    hid_t file_space_id, hid_t dxpl_id, const void *buf, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.write)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset write' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.write)(obj, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_WRITEERROR, FAIL, "dataset write failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_write() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_write
 *
 * Purpose:	Writes data from dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_write(const H5VL_object_t *vol_obj, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                   hid_t dxpl_id, const void *buf, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    /* (For converting fill-values to disk form) */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    if (H5VL__set_dst_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_write(obj, vol_obj->container->connector->cls, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_WRITEERROR, FAIL, "dataset write failed")

done:
    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'dst' VOL container info")
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_write() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_write_api_op
 *
 * Purpose:     Callback for common API wrapper to write a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_write_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_dataset_write_ctx_t *ctx       = (H5VL_dataset_write_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_write(obj, ctx->cls, ctx->mem_type_id, ctx->mem_space_id, ctx->file_space_id, ctx->dxpl_id, ctx->buf, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "unable to write dataset")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_write_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_write
 *
 * Purpose:     Writes data to a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_write(void *obj, hid_t connector_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id,
                  hid_t dxpl_id, const void *buf, void **req /*out*/)
{
    H5VL_dataset_write_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE8("e", "*xiiiii*xx", obj, connector_id, mem_type_id, mem_space_id, file_space_id, dxpl_id, buf,
             req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls           = connector->cls;
    ctx.mem_type_id   = mem_type_id;
    ctx.mem_space_id  = mem_space_id;
    ctx.file_space_id = file_space_id;
    ctx.buf           = buf;
    ctx.dxpl_id       = dxpl_id;
    ctx.req           = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_write_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_write() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_get
 *
 * Purpose:	Get specific information about the dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_get(void *obj, const H5VL_class_t *cls, H5VL_dataset_get_args_t *args, hid_t dxpl_id,
                  void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset get' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.get)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "dataset get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_get
 *
 * Purpose:	Get specific information about the dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_get(const H5VL_object_t *vol_obj, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    /* (For converting fill-values from disk form) */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_get(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "dataset 'get' operation failed")

done:
    /* Reset container info in API context */
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_dataset_get_ctx_t *ctx       = (H5VL_dataset_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_get(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute dataset 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_get
 *
 * Purpose:     Gets information about a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_get(void *obj, hid_t connector_id, H5VL_dataset_get_args_t *args, hid_t dxpl_id,
                void **req /*out*/)
{
    H5VL_dataset_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                 ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_specific
 *
 * Purpose:	Specific operation on datasets through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_specific(void *obj, const H5VL_class_t *cls, H5VL_dataset_specific_args_t *args, hid_t dxpl_id,
                       void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.specific)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_specific
 *
 * Purpose:	Specific operation on datasets through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_specific(const H5VL_object_t *vol_obj, H5VL_dataset_specific_args_t *args, hid_t dxpl_id,
                      void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    /* (For converting fill-values from disk form) */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    /* (For converting fill-values to disk form) */
    if (H5VL__set_dst_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_specific(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'specific' callback")

done:
    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'dst' VOL container info")
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to issue specific operations on a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_dataset_specific_ctx_t *ctx       = (H5VL_dataset_specific_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                       ret_value = SUCCEED;                             /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_specific(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_specific
 *
 * Purpose:     Performs a connector-specific operation on a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_specific(void *obj, hid_t connector_id, H5VL_dataset_specific_args_t *args, hid_t dxpl_id,
                     void **req /*out*/)
{
    H5VL_dataset_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                      ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__dataset_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id,
                       void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.optional)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_dataset_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    if (H5VL__set_src_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'optional' callback")

done:
    /* Reset container info in API context */
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to perform optional operation on a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_dataset_optional_ctx_t *ctx       = (H5VL_dataset_optional_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                       ret_value = SUCCEED;                             /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_optional(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_optional
 *
 * Purpose:     Performs an optional connector-specific operation on a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                     void **req /*out*/)
{
    H5VL_dataset_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                      ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t dset_id,
                        H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5VL_object_t *vol_obj   = NULL;            /* Dataset VOL object */
    void *         token     = NULL;            /* Request token for async operation        */
    void **        token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    herr_t         ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*s*sIui*!ii", app_file, app_func, app_line, dset_id, args, dxpl_id, es_id);

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Call the corresponding internal VOL routine */
    if (H5VL__common_optional_op(dset_id, H5I_DATASET, H5VL__dataset_optional, args, dxpl_id, token_ptr, &vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute dataset 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token,
                        H5ARG_TRACE7(__func__, "*s*sIui*!ii", app_file, app_func, app_line, dset_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdataset_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_close
 *
 * Purpose:     Closes a dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->dataset_cls.close)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'dataset close' method")

    /* Call the corresponding VOL callback */
    if ((cls->dataset_cls.close)(obj, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "dataset close failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_dataset_close
 *
 * Purpose:     Closes a dataset through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_dataset_close(const H5VL_object_t *vol_obj, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_close(obj, vol_obj->container->connector->cls, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "dataset close failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_dataset_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__dataset_close_api_op
 *
 * Purpose:     Callback for common API wrapper to close a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__dataset_close_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_dataset_close_ctx_t *ctx       = (H5VL_dataset_close_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__dataset_close(obj, ctx->cls, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "unable to close dataset")

    /* Close VOL object, if API context */
    if(new_api_ctx)
        if (H5VL_free_object(ctx->obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to free dataset VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__dataset_close_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdataset_close
 *
 * Purpose:     Closes a dataset
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdataset_close(void *obj, hid_t connector_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_dataset_close_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiix", obj, connector_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.obj     = obj;
    ctx.cls     = connector->cls;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__dataset_close_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdataset_close() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__datatype_commit
 *
 * Purpose:	Commits a datatype to the file through the VOL
 *
 * Return:      Success:    Pointer to the new datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__datatype_commit(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                      const char *name, hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id,
                      hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.commit)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'datatype commit' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->datatype_cls.commit)(obj, loc_params, name, type_id, lcpl_id, tcpl_id,
                                                        tapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "datatype commit failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_commit() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_datatype_commit
 *
 * Purpose:	Commits a datatype to the file through the VOL
 *
 * Return:      Success:    Pointer to VOL object for named datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_datatype_commit(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                     hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *type = NULL;          /* Named datatype object from VOL connector */
    H5VL_container_t *type_container = NULL;    /* Container for named datatype */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for named datatype */
    type_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (type = H5VL__datatype_commit(obj, loc_params, vol_obj->container->connector->cls, name, type_id, lcpl_id, tcpl_id, tapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "datatype commit failed")

    /* Get container for named datatype (accommodates external links) */
    if (NULL == (type_container = H5VL__get_container_for_obj(type, H5I_DATATYPE, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for named datatype")

    /* Create VOL object for named datatype */
    if (NULL == (ret_value = H5VL__create_object(H5VL_OBJ_DATATYPE, type, type_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for named datatype")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (type && type_container && H5VL__datatype_close(type, type_container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "named datatype close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_commit() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_commit_api_op
 *
 * Purpose:     Callback for common API wrapper to create an named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_commit_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_datatype_commit_ctx_t *ctx       = (H5VL_datatype_commit_ctx_t *)_ctx; /* Get pointer to context */
    void *type;         /* Named datatype from VOL connector */
    herr_t                      ret_value = SUCCEED;                            /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (type = H5VL__datatype_commit(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->type_id, ctx->lcpl_id, ctx->tcpl_id, ctx->tapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "unable to commit datatype")

    /* Set up object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_DATATYPE, type)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for named datatype")
    } /* end if */
    else
        ctx->ret_value = type;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_commit_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_commit
 *
 * Purpose:     Commits a datatype to the file
 *
 * Return:      Success:    Pointer to the new datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdatatype_commit(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                    hid_t type_id, hid_t lcpl_id, hid_t tcpl_id, hid_t tapl_id, hid_t dxpl_id,
                    void **req /*out*/)
{
    H5VL_datatype_commit_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *        connector;              /* VOL connector struct */
    void *                     ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE10("*x", "*x*#i*siiiiix", obj, loc_params, connector_id, name, type_id, lcpl_id, tcpl_id, tapl_id,
              dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.type_id    = type_id;
    ctx.lcpl_id    = lcpl_id;
    ctx.tcpl_id    = tcpl_id;
    ctx.tapl_id    = tapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_commit_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_commit() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__datatype_open
 *
 * Purpose:	Opens a named datatype through the VOL
 *
 * Return:      Success:    Pointer to the datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__datatype_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                    hid_t tapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "no datatype open callback")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->datatype_cls.open)(obj, loc_params, name, tapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "datatype open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_datatype_open
 *
 * Purpose:	Opens a named datatype through the VOL
 *
 * Return:      Success:    Pointer to VOL object for named datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_datatype_open(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                   hid_t tapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *type = NULL;          /* Named datatype object from VOL connector */
    H5VL_container_t *type_container = NULL;    /* Container for named datatype */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for named datatype */
    type_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (type = H5VL__datatype_open(obj, loc_params, type_container->connector->cls, name, tapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "named datatype open failed")

    /* Get container for named datatype (accommodates external links) */
    if (NULL == (type_container = H5VL__get_container_for_obj(type, H5I_DATATYPE, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for named datatype")

    /* Create VOL object for named datatype */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_DATATYPE, type, type_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for named datatype")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (type && type_container && H5VL__datatype_close(type, type_container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "named datatype close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_open_api_op
 *
 * Purpose:     Callback for common API wrapper to open a named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_open_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_datatype_open_ctx_t *ctx       = (H5VL_datatype_open_ctx_t *)_ctx; /* Get pointer to context */
    void *type = NULL;          /* Named datatype object from VOL connector */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (ctx->ret_value = H5VL__datatype_open(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->tapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, FAIL, "unable to open named datatype")

    /* Set up named datatype object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_DATATYPE, type)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for named datatype")
    } /* end if */
    else
        ctx->ret_value = type;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_open_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_open
 *
 * Purpose:     Opens a named datatype
 *
 * Return:      Success:    Pointer to the datatype
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLdatatype_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                  hid_t tapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_datatype_open_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *      connector;              /* VOL connector struct */
    void *                   ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE7("*x", "*x*#i*siix", obj, loc_params, connector_id, name, tapl_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.tapl_id    = tapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_open_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_get
 *
 * Purpose:     Get specific information about the datatype through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_get(void *obj, const H5VL_class_t *cls, H5VL_datatype_get_args_t *args, hid_t dxpl_id,
                   void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'datatype get' method")

    /* Call the corresponding VOL callback */
    if ((cls->datatype_cls.get)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "datatype 'get' failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_datatype_get
 *
 * Purpose:     Get specific information about the datatype through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_datatype_get(const H5VL_object_t *vol_obj, H5VL_datatype_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_get(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "named datatype get failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_datatype_get_ctx_t *ctx       = (H5VL_datatype_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                   ret_value = SUCCEED;                         /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_get(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute datatype 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_get
 *
 * Purpose:     Gets information about the datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_get(void *obj, hid_t connector_id, H5VL_datatype_get_args_t *args, hid_t dxpl_id,
                 void **req /*out*/)
{
    H5VL_datatype_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *      connector;                 /* VOL connector struct */
    herr_t                  ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__datatype_specific
 *
 * Purpose:	Specific operation on datatypes through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_specific(void *obj, const H5VL_class_t *cls, H5VL_datatype_specific_args_t *args,
                        hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'datatype specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->datatype_cls.specific)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute datatype 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_datatype_specific
 *
 * Purpose:	Specific operation on datatypes through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_datatype_specific(const H5VL_object_t *vol_obj, H5VL_datatype_specific_args_t *args, hid_t dxpl_id,
                       void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_specific(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute named datatype 'specific' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to perform a specific operation on a named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_datatype_specific_ctx_t *ctx = (H5VL_datatype_specific_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                        ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_specific(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute named datatype 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_specific
 *
 * Purpose:     Performs a connector-specific operation on a datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_specific(void *obj, hid_t connector_id, H5VL_datatype_specific_args_t *args, hid_t dxpl_id,
                      void **req /*out*/)
{
    H5VL_datatype_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                       ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__datatype_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id,
                        void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no named datatype 'optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->datatype_cls.optional)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute named datatype 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_datatype_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_datatype_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute named datatype 'optional' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_datatype_optional_op
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_datatype_optional_op(H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req,
                          H5VL_object_t **_vol_obj_ptr)
{
    void *obj;                  /* Object to operate on */
    H5VL_object_t * tmp_vol_obj = NULL;                                         /* Object for id */
    H5VL_object_t **vol_obj_ptr = (_vol_obj_ptr ? _vol_obj_ptr : &tmp_vol_obj); /* Ptr to object ptr for id */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t          ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(vol_obj);

    /* Set up vol_obj_ptr */
    *vol_obj_ptr = vol_obj;

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute named datatype 'optional' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to perform an optional operation on a named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_datatype_optional_ctx_t *ctx = (H5VL_datatype_optional_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                        ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_optional(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute datatype 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_optional
 *
 * Purpose:     Performs an optional connector-specific operation on a datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                      void **req /*out*/)
{
    H5VL_datatype_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                       ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t type_id,
                         H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5T_t *        dt;                          /* Datatype for this operation */
    H5VL_object_t *vol_obj   = NULL;            /* Datatype VOL object */
    void *         token     = NULL;            /* Request token for async operation        */
    void **        token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    herr_t         ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*s*sIui*!ii", app_file, app_func, app_line, type_id, args, dxpl_id, es_id);

    /* Check args */
    if (NULL == (dt = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype")

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Only invoke callback if VOL object is set for the datatype */
    if (H5T_invoke_vol_optional(dt, args, dxpl_id, token_ptr, &vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to invoke named datatype 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token,
                        H5ARG_TRACE7(__func__, "*s*sIui*!ii", app_file, app_func, app_line, type_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLdatatype_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_close
 *
 * Purpose:     Closes a datatype through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->datatype_cls.close)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'datatype close' method")

    /* Call the corresponding VOL callback */
    if ((cls->datatype_cls.close)(obj, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "datatype close failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_datatype_close
 *
 * Purpose:     Closes a datatype through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_datatype_close(const H5VL_object_t *vol_obj, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_close(obj, vol_obj->container->connector->cls, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "named datatype close failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_datatype_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__datatype_close_api_op
 *
 * Purpose:     Callback for common API wrapper to close a named datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__datatype_close_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_datatype_close_ctx_t *ctx       = (H5VL_datatype_close_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                     ret_value = SUCCEED;                           /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__datatype_close(obj, ctx->cls, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "unable to close datatype")

    /* Close VOL object, if API context */
    if(new_api_ctx)
        if (H5VL_free_object(ctx->obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to free named datatype VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__datatype_close_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLdatatype_close
 *
 * Purpose:     Closes a datatype
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLdatatype_close(void *obj, hid_t connector_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_datatype_close_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                    ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiix", obj, connector_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.obj     = obj;
    ctx.cls     = connector->cls;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__datatype_close_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLdatatype_close() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_create
 *
 * Purpose:	Creates a file through the VOL
 *
 * Note:	Does not have a 'static' version of the routine, since there's
 *		no objects in the container before this operation completes.
 *
 * Return:      Success: Pointer to new file
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__file_create(const H5VL_class_t *cls, const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id,
                  hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.create)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'file create' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->file_cls.create)(name, flags, fcpl_id, fapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "file create failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_file_create
 *
 * Purpose:	Creates a file through the VOL
 *
 * Return:      Success:    A file ID
 *              Failure:    H5I_INVALID_HID
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5VL_file_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id, void **req)
{
    H5P_genplist_t *      fapl_plist;       /* Property list pointer                    */
    H5VL_connector_t *    connector;        /* VOL connector    */
    H5VL_object_t *       vol_obj = NULL;   /* Temporary VOL object for file */
    H5VL_container_t *    container = NULL; /* VOL container for file */
    H5VL_connector_prop_t connector_prop;   /* Property for VOL connector ID & info     */
    void *                file      = NULL; /* New file created */
    hid_t                 ret_value = H5I_INVALID_HID; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Get the VOL info from the fapl */
    if (NULL == (fapl_plist = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, H5I_INVALID_HID, "not a file access property list")
    if (H5P_peek(fapl_plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, H5I_INVALID_HID, "can't get VOL connector info")

    /* Get the connector's class */
    if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, H5I_INVALID_HID, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (NULL == (file = H5VL__file_create(connector->cls, name, flags, fcpl_id, fapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "file create failed")

    /* Create container for the file */
    if (NULL == (container = H5VL_create_container(file, connector, &connector_prop)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, H5I_INVALID_HID, "VOL container create failed")

    /* Create a VOL object for the file */
    /* (Note that the 'object' pointer is passed as NULL, as the container holds the actual file object) */
    if (NULL == (vol_obj = H5VL__create_object(H5VL_OBJ_FILE, NULL, container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, H5I_INVALID_HID, "can't create VOL object")

    /* Register ID for VOL object, for future API calls */
    if ((ret_value = H5I_register(H5I_FILE, vol_obj, TRUE)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5I_INVALID_HID, "unable to register file ID")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_create
 *
 * Purpose:     Creates a file
 *
 * Return:      Success:    Pointer to the new file
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLfile_create(const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t dxpl_id,
                void **req /*out*/)
{
    H5P_genplist_t *      fapl_plist = NULL;   /* FAPL property list pointer */
    H5P_genplist_t *      dxpl_plist = NULL;   /* DXPL property list pointer */
    H5VL_object_t *       vol_obj    = NULL;   /* Temporary VOL object for file */
    H5VL_container_t *    container = NULL; /* VOL container for file */
    H5VL_connector_prop_t connector_prop;      /* Property for VOL connector ID & info */
    H5VL_connector_t *    connector;           /* VOL connector */
    void *                file      = NULL; /* New file created */
    hbool_t               new_api_ctx = FALSE; /* Whether to start a new API context */
    hbool_t               api_pushed  = FALSE; /* Indicate that a new API context was pushed */
    hbool_t             new_api_ctx_prop_reset = FALSE; /* Whether the 'new API context' property was reset */
    void *                ret_value   = NULL;  /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE6("*x", "*sIuiiix", name, flags, fcpl_id, fapl_id, dxpl_id, req);

    /* Get the VOL info from the fapl */
    if (NULL == (fapl_plist = H5P_object_verify(fapl_id, H5P_FILE_ACCESS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file access property list")
    if (H5P_peek(fapl_plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL connector info")

    /* Check for non-default DXPL */
    if (!(H5P_DEFAULT == dxpl_id || H5P_DATASET_XFER_DEFAULT == dxpl_id)) {
        /* Check for 'new API context' flag */
        if (NULL == (dxpl_plist = H5P_object_verify(dxpl_id, H5P_DATASET_XFER)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, NULL, "not a dataset transfer property list")
        if (H5P_get(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &new_api_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "unable to get value")

        /* Start a new API context, if requested */
        if (new_api_ctx) {
            hbool_t reset_api_ctx = FALSE; /* Flag to reset the 'new API context' */

            /* Push the API context */
            if (H5CX_push() < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set API context")
            api_pushed = TRUE;

            /* Reset 'new API context' flag for next layer down */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &reset_api_ctx) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "unable to set value")
            new_api_ctx_prop_reset = TRUE;
        } /* end if */
    }     /* end if */

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (NULL == (file = H5VL__file_create(connector->cls, name, flags, fcpl_id, fapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "unable to create file")

    /* Attempt 'post open' callback, for new API contexts */
    if (new_api_ctx) {
        /* Create container for the file */
        if (NULL == (container = H5VL_create_container(file, connector, &connector_prop)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "VOL container create failed")

        /* Create a VOL object for the file */
        /* (Note that the 'object' pointer is passed as NULL, as the container holds the actual file object) */
        if (NULL == (vol_obj = H5VL__create_object(H5VL_OBJ_FILE, NULL, container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "can't create VOL object")

        /* Set return value to VOL object */
        ret_value = vol_obj;
    }     /* end if */
    else
        /* Set return value to file object */
        ret_value = file;

done:
    /* Undo earlier actions, if performed */
    if (new_api_ctx) {
        if (new_api_ctx_prop_reset) {
            hbool_t undo_api_ctx = TRUE; /* Flag to reset the 'new API context' */

            /* Undo change to 'new API context' flag */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &undo_api_ctx) < 0)
                HDONE_ERROR(H5E_VOL, H5E_CANTSET, NULL, "unable to set value")
        } /* end if */

        if (api_pushed)
            (void)H5CX_pop(FALSE);
    } /* end if */

    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_open
 *
 * Purpose:	Opens a file through the VOL.
 *
 * Return:      Success: Pointer to file.
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__file_open(const H5VL_class_t *cls, const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id,
                void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'file open' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->file_cls.open)(name, flags, fapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_open_find_connector_cb
 *
 * Purpose:     Iteration callback for H5PL_iterate that tries to find the
 *              correct VOL connector to open a file with when
 *              H5VL_file_open fails for its given VOL connector. Iterates
 *              through all of the available VOL connector plugins until
 *              H5Fis_accessible returns true for the given filename and
 *              VOL connector.
 *
 * Return:      H5_ITER_CONT if current plugin can't open the given file
 *              H5_ITER_STOP if current plugin can open given file
 *              H5_ITER_ERROR if an error occurs while trying to determine
 *                  if current plugin can open given file.
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_open_find_connector_cb(H5PL_type_t plugin_type, const void *plugin_info, void *op_data)
{
    H5VL_file_open_find_connector_t *udata = (H5VL_file_open_find_connector_t *)op_data;
    H5VL_file_specific_args_t        vol_cb_args; /* Arguments to VOL callback */
    const H5VL_class_t *             cls = (const H5VL_class_t *)plugin_info;
    H5P_genplist_t *                 fapl_plist;
    H5P_genplist_t *                 fapl_plist_copy;
    hbool_t                          is_accessible = FALSE; /* Whether file is accessible */
    herr_t                           status;
    hid_t                            connector_id = H5I_INVALID_HID;
    hid_t                            fapl_id      = H5I_INVALID_HID;
    herr_t                           ret_value    = H5_ITER_CONT;

    FUNC_ENTER_STATIC

    HDassert(udata);
    HDassert(udata->filename);
    HDassert(udata->connector_prop);
    HDassert(cls);
    HDassert(plugin_type == H5PL_TYPE_VOL);

    /* Silence compiler */
    (void)plugin_type;

    udata->cls = cls;

    /* Attempt to register plugin as a VOL connector */
    if ((connector_id = H5VL__register_connector_by_class(cls, H5P_VOL_INITIALIZE_DEFAULT)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, H5_ITER_ERROR, "unable to register VOL connector")

    /* Setup FAPL with registered VOL connector */
    if (NULL == (fapl_plist = (H5P_genplist_t *)H5I_object_verify(udata->fapl_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5_ITER_ERROR, "not a property list")
    if ((fapl_id = H5P_copy_plist(fapl_plist, TRUE)) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTCOPY, H5_ITER_ERROR, "can't copy fapl");
    if (NULL == (fapl_plist_copy = (H5P_genplist_t *)H5I_object_verify(fapl_id, H5I_GENPROP_LST)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, H5_ITER_ERROR, "not a property list")
    if (H5P_set_vol(fapl_plist_copy, connector_id, NULL) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTSET, H5_ITER_ERROR, "can't set VOL connector on fapl")

    /* Set up VOL callback arguments */
    vol_cb_args.op_type                       = H5VL_FILE_IS_ACCESSIBLE;
    vol_cb_args.args.is_accessible.filename   = udata->filename;
    vol_cb_args.args.is_accessible.fapl_id    = fapl_id;
    vol_cb_args.args.is_accessible.accessible = &is_accessible;

    /* Check if file is accessible with given VOL connector */
    H5E_BEGIN_TRY
    {
        status = H5VL_file_specific(NULL, &vol_cb_args, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL);
    }
    H5E_END_TRY;

    /* If the file was accessible with the current VOL connector, return
     * the FAPL with that VOL connector set on it. Errors are ignored here
     * as some VOL connectors may not support H5Fis_accessible.
     */
    if (status == SUCCEED && is_accessible) {
        /* Modify 'connector_prop' to point to the VOL connector that
         * was actually used to open the file, rather than the original
         * VOL connector that was requested.
         */
        udata->connector_prop->connector_id   = connector_id;
        udata->connector_prop->connector_info = NULL;

        udata->fapl_id = fapl_id;
        ret_value      = H5_ITER_STOP;
    } /* end if */

done:
    if (ret_value != H5_ITER_STOP) {
        if (fapl_id >= 0 && H5I_dec_app_ref(fapl_id) < 0)
            HDONE_ERROR(H5E_PLIST, H5E_CANTCLOSEOBJ, H5_ITER_ERROR, "can't close fapl")
        if (connector_id >= 0 && H5I_dec_app_ref(connector_id) < 0)
            HDONE_ERROR(H5E_ID, H5E_CANTCLOSEOBJ, H5_ITER_ERROR, "can't close VOL connector ID")
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_open_find_connector_cb() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_file_open
 *
 * Purpose:	Opens a file through the VOL.
 *
 * Note:	Does not have a 'static' version of the routine, since there's
 *		no objects in the container before this operation completes.
 *
 * Return:      Success: Pointer to file VOL object
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req,
               hid_t *ret_id /*out*/)
{
    H5P_genplist_t *      fapl_plist;       /* Property list pointer                    */
    H5VL_connector_t *    connector;        /* VOL connector    */
    H5VL_object_t *       vol_obj = NULL;   /* Temporary VOL object for file */
    H5VL_container_t *    container = NULL; /* VOL container for file */
    H5VL_connector_prop_t connector_prop;   /* Property for VOL connector ID & info     */
    void *                file = NULL;      /* File pointer */
    H5VL_file_open_find_connector_t find_connector_ud;  /* Callback context for iteration over connectors */
    hbool_t               found_by_iteration = FALSE;   /* Whether connector was found by iteration */
    H5VL_object_t *       ret_value = NULL; /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Get the VOL info from the fapl */
    if (NULL == (fapl_plist = (H5P_genplist_t *)H5I_object(fapl_id)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, NULL, "not a file access property list")
    if (H5P_peek(fapl_plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL connector info")

    /* Clear the iteration callback info, so cleanup is safe */
    HDmemset(&find_connector_ud, 0, sizeof(find_connector_ud));

    /* Get the connector */
    if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (NULL == (file = H5VL__file_open(connector->cls, name, flags, fapl_id, dxpl_id, req))) {
        hbool_t is_default_conn = TRUE;

        /* Opening the file failed - Determine whether we should search
         * the plugin path to see if any other VOL connectors are available
         * to attempt to open the file with. This only occurs if the default
         * VOL connector was used for the initial file open attempt.
         */
        H5VL__is_default_conn(fapl_id, connector_prop.connector_id, &is_default_conn);

        if (is_default_conn) {
            herr_t                          iter_ret;

            /* Set up iteration context info */
            find_connector_ud.connector_prop = &connector_prop;
            find_connector_ud.filename       = name;
            find_connector_ud.cls            = NULL;
            find_connector_ud.fapl_id        = fapl_id;

            iter_ret = H5PL_iterate(H5PL_ITER_TYPE_VOL, H5VL__file_open_find_connector_cb,
                                    (void *)&find_connector_ud);
            if (iter_ret < 0)
                HGOTO_ERROR(H5E_VOL, H5E_BADITER, NULL, "iteration over VOL connector plugins failed")
            else if (iter_ret) {
                /* If one of the available VOL connector plugins is
                 * able to open the file, clear the error stack from any
                 * previous file open failures and then open the file.
                 * Otherwise, if no VOL connectors are available, throw
                 * error from original file open failure.
                 */
                H5E_clear_stack(NULL);

                if (NULL == (file = H5VL__file_open(find_connector_ud.cls, name, flags, find_connector_ud.fapl_id, dxpl_id, req)))
                    HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "can't open file '%s' with VOL connector '%s'", name, find_connector_ud.cls->name)

                /* Get the connector */
                if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
                    HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, NULL, "not a VOL connector ID")

                /* Indicate that the connector was found by iteration */
                found_by_iteration = TRUE;
            }
            else
                HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "open failed")
        } /* end if */
        else
            HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "open failed")
    } /* end if */

    /* Create container for the file */
    if (NULL == (container = H5VL_create_container(file, connector, &connector_prop)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "VOL container create failed")

    /* Create a VOL object for the file */
    /* (Note that the 'object' pointer is passed as NULL, as the container holds the actual file object) */
    if (NULL == (vol_obj = H5VL__create_object(H5VL_OBJ_FILE, NULL, container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "can't create VOL object")

    /* Check for registering ID for file */
    if (ret_id) {
        hid_t file_id; /* File ID for file pointer */

        /* Register ID for VOL object, for future API calls */
        if ((file_id = H5I_register(H5I_FILE, vol_obj, TRUE)) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, NULL, "unable to register file ID")

        /* Set file ID to return */
        *ret_id = file_id;
    } /* end if */

    /* Set return value */
    ret_value = vol_obj;

done:
    /* Clean up iteration info */
    if (found_by_iteration) {
        if (find_connector_ud.fapl_id >= 0 && H5I_dec_app_ref(find_connector_ud.fapl_id) < 0)
            HDONE_ERROR(H5E_PLIST, H5E_CANTCLOSEOBJ, NULL, "can't close fapl")
        if (find_connector_ud.connector_prop->connector_id >= 0 && H5I_dec_app_ref(find_connector_ud.connector_prop->connector_id) < 0)
            HDONE_ERROR(H5E_ID, H5E_CANTCLOSEOBJ, NULL, "can't close VOL connector ID")
    } /* end if */

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_open
 *
 * Purpose:     Opens a file
 *
 * Return:      Success:    Pointer to the file
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLfile_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5P_genplist_t *      fapl_plist = NULL;   /* FAPL property list pointer */
    H5P_genplist_t *      dxpl_plist = NULL;   /* DXPL property list pointer */
    H5VL_object_t *       vol_obj    = NULL;   /* Temporary VOL object for file */
    H5VL_container_t *    container = NULL; /* VOL container for file */
    H5VL_connector_prop_t connector_prop;      /* Property for VOL connector ID & info */
    H5VL_connector_t *    connector;           /* VOL connector */
    void *                file = NULL;      /* File pointer */
    hbool_t               new_api_ctx = FALSE; /* Whether to start a new API context */
    hbool_t               api_pushed  = FALSE; /* Indicate that a new API context was pushed */
    hbool_t             new_api_ctx_prop_reset = FALSE; /* Whether the 'new API context' property was reset */
    void *                ret_value   = NULL;  /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE5("*x", "*sIuiix", name, flags, fapl_id, dxpl_id, req);

    /* Get the VOL info from the fapl */
    if (NULL == (fapl_plist = H5P_object_verify(fapl_id, H5P_FILE_ACCESS)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a file access property list")
    if (H5P_peek(fapl_plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, NULL, "can't get VOL connector info")

    /* Check for non-default DXPL */
    if (!(H5P_DEFAULT == dxpl_id || H5P_DATASET_XFER_DEFAULT == dxpl_id)) {
        /* Check for 'new API context' flag */
        if (NULL == (dxpl_plist = H5P_object_verify(dxpl_id, H5P_DATASET_XFER)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, NULL, "not a dataset transfer property list")
        if (H5P_get(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &new_api_ctx) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "unable to get value")

        /* Start a new API context, if requested */
        if (new_api_ctx) {
            hbool_t reset_api_ctx = FALSE; /* Flag to reset the 'new API context' */

            /* Push the API context */
            if (H5CX_push() < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set API context")
            api_pushed = TRUE;

            /* Reset 'new API context' flag for next layer down */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &reset_api_ctx) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "unable to set value")
            new_api_ctx_prop_reset = TRUE;
        } /* end if */
    }     /* end if */

    /* Get connector */
    if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (NULL == (file = H5VL__file_open(connector->cls, name, flags, fapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "unable to open file")

    /* Attempt 'post open' callback, for new API contexts */
    if (new_api_ctx) {
        /* Create container for the file */
        if (NULL == (container = H5VL_create_container(file, connector, &connector_prop)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "VOL container create failed")

        /* Create a VOL object for the file */
        /* (Note that the 'object' pointer is passed as NULL, as the container holds the actual file object) */
        if (NULL == (vol_obj = H5VL__create_object(H5VL_OBJ_FILE, NULL, container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, NULL, "can't create VOL object")

        /* Set return value to VOL object */
        ret_value = vol_obj;
    }     /* end if */
    else
        /* Set return value to file object */
        ret_value = file;

done:
    /* Undo earlier actions, if performed */
    if (new_api_ctx) {
        if (new_api_ctx_prop_reset) {
            hbool_t undo_api_ctx = TRUE; /* Flag to reset the 'new API context' */

            /* Undo change to 'new API context' flag */
            if (H5P_set(dxpl_plist, H5D_XFER_PLUGIN_NEW_API_CTX_NAME, &undo_api_ctx) < 0)
                HDONE_ERROR(H5E_VOL, H5E_CANTSET, NULL, "unable to set value")
        } /* end if */

        if (api_pushed)
            (void)H5CX_pop(FALSE);
    } /* end if */

    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_get
 *
 * Purpose:	Get specific information about the file through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_get(void *obj, const H5VL_class_t *cls, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'file get' method")

    /* Call the corresponding VOL callback */
    if ((cls->file_cls.get)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "file get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_file_get
 *
 * Purpose:	Get specific information about the file through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_get(const H5VL_object_t *vol_obj, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_get(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "file get failed")

    /* Update FAPL connector info, if retrieved */
    if(H5VL_FILE_GET_FAPL == args->op_type)
        if (H5VL__update_fapl_vol(args->args.get_fapl.fapl_id, vol_obj->container) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL connector info in file access property list")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_file_get_ctx_t *ctx       = (H5VL_file_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t               ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_get(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute file 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_get
 *
 * Purpose:     Gets information about the file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_get(void *obj, hid_t connector_id, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_file_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__file_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

// QQQQ
// QQQQ: Fix me, after all public VOL callback API calls are dealing with H5VL_object_t's
// QQQQ
//    /* Update FAPL connector info, if retrieved */
//    if(H5VL_FILE_GET_FAPL == args->args.op_type)
//        if (H5VL__update_fapl_vol(args->args.get_fapl.fapl_id, vol_obj->container) < 0)
//            HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL connector info in file access property list")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_specific
 *
 * Purpose:	Perform File specific operations through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_specific(void *obj, const H5VL_class_t *cls, H5VL_file_specific_args_t *args, hid_t dxpl_id,
                    void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'file specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->file_cls.specific)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file specific failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_file_specific
 *
 * Purpose:	Perform file specific operations through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_specific(const H5VL_object_t *vol_obj, H5VL_file_specific_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    H5VL_connector_t *connector;                   /* VOL connector */
    H5VL_object_t *     reopen_vol_obj  = NULL;    /* Temporary VOL object for file */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t              ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Special treatment of file access check & delete operations */
    /* (Retrieve the VOL connector from the FAPL, since the file isn't open) */
    if (args->op_type == H5VL_FILE_IS_ACCESSIBLE || args->op_type == H5VL_FILE_DELETE) {
        H5P_genplist_t *      plist;          /* Property list pointer */
        H5VL_connector_prop_t connector_prop; /* Property for VOL connector ID & info */
        hid_t                 fapl_id;        /* File access property list for accessing the file */

        /* Get the file access property list to access the file */
        if (args->op_type == H5VL_FILE_IS_ACCESSIBLE)
            fapl_id = args->args.is_accessible.fapl_id;
        else
            fapl_id = args->args.del.fapl_id;

        /* Get the VOL info from the FAPL */
        if (NULL == (plist = (H5P_genplist_t *)H5I_object(fapl_id)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a file access property list")
        if (H5P_peek(plist, H5F_ACS_VOL_CONN_NAME, &connector_prop) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL connector info")

        /* Set actual object */
        obj = NULL;

        /* Get connector */
        if (NULL == (connector = H5I_object_verify(connector_prop.connector_id, H5I_VOL)))
            HGOTO_ERROR(H5E_VOL, H5E_BADTYPE, FAIL, "not a VOL connector ID")
    } /* end if */
    /* Set wrapper info in API context, for all other operations */
    else {
        /* Sanity check */
        HDassert(vol_obj);

        /* Set container info in API context */
        if (H5VL_set_primary_container_ctx(vol_obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
        prim_container_ctx_set = TRUE;

        /* Get actual object */
        obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

        /* Get the VOL connector */
        connector = vol_obj->container->connector;
    } /* end else */

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_specific(obj, connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file specific failed")

    /* Special treatment of file re-open operation */
    if (args->op_type == H5VL_FILE_REOPEN) {
        H5VL_container_t *container;    /* New container for reopened file */
        void *   reopen_file; /* Pointer to re-opened file */

        /* Get pointer to re-opened file */
        reopen_file = *args->args.reopen.file;
        HDassert(reopen_file);

        /* Create container for the file */
        if (NULL == (container = H5VL_create_container(reopen_file, vol_obj->container->connector, &vol_obj->container->conn_prop)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "VOL container create failed")

        /* Create a VOL object for the re-opened file struct */
        /* (Note that the 'object' pointer is passed as NULL, as the container holds the actual file object) */
        if (NULL == (reopen_vol_obj = H5VL__create_object(H5VL_OBJ_FILE, NULL, container)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't create VOL object")

        /* Update re-opened file pointer to the VOL object */
        *args->args.reopen.file = reopen_vol_obj;
    } /* end if */

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to perform 'specific' operation on a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_file_specific_ctx_t *ctx       = (H5VL_file_specific_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_specific(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute file 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_specific
 *
 * Purpose:     Performs a connector-specific operation on a file
 *
 * Note:	The 'obj' parameter is allowed to be NULL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_specific(void *obj, hid_t connector_id, H5VL_file_specific_args_t *args, hid_t dxpl_id,
                  void **req /*out*/)
{
    H5VL_file_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__file_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__file_optional
 *
 * Purpose:	Perform a connector specific operation
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'file optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->file_cls.optional)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "file optional failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_file_optional
 *
 * Purpose:	Perform a connector specific operation
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute file 'optional' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to perform 'optional' operation on a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_file_optional_ctx_t *ctx       = (H5VL_file_optional_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_optional(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute file 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_optional
 *
 * Purpose:     Performs an optional connector-specific operation on a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                  void **req /*out*/)
{
    H5VL_file_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__file_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t file_id,
                     H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5VL_object_t *vol_obj   = NULL;            /* File VOL object */
    void *         token     = NULL;            /* Request token for async operation        */
    void **        token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    herr_t         ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*s*sIui*!ii", app_file, app_func, app_line, file_id, args, dxpl_id, es_id);

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Call the corresponding internal VOL routine */
    if (H5VL__common_optional_op(file_id, H5I_FILE, H5VL__file_optional, args, dxpl_id, token_ptr, &vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute file 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token,
                        H5ARG_TRACE7(__func__, "*s*sIui*!ii", app_file, app_func, app_line, file_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLfile_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_close
 *
 * Purpose:     Closes a file through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->file_cls.close)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'file close' method")

    /* Call the corresponding VOL callback */
    if ((cls->file_cls.close)(obj, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL, "file close failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_file_close
 *
 * Purpose:     Closes a file through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_file_close(const H5VL_object_t *vol_obj, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_close(obj, vol_obj->container->connector->cls, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL, "file close failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_file_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__file_close_api_op
 *
 * Purpose:     Callback for common API wrapper to close a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__file_close_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_file_close_ctx_t *ctx       = (H5VL_file_close_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                 ret_value = SUCCEED;                       /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__file_close(obj, ctx->cls, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL, "unable to close file")

    /* Close VOL object, if API context */
    if(new_api_ctx)
        if (H5VL_free_object(ctx->obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to free file VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__file_close_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLfile_close
 *
 * Purpose:     Closes a file
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLfile_close(void *obj, hid_t connector_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_file_close_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiix", obj, connector_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.obj     = obj;
    ctx.cls     = connector->cls;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__file_close_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLfile_close() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__group_create
 *
 * Purpose:	Creates a group through the VOL
 *
 * Return:      Success: Pointer to new group
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__group_create(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                   hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.create)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'group create' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->group_cls.create)(obj, loc_params, name, lcpl_id, gcpl_id, gapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "group create failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_group_create
 *
 * Purpose:	Creates a group through the VOL
 *
 * Return:      Success: Pointer to VOL object for group
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_group_create(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                  hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *grp = NULL;          /* Group object from VOL connector */
    H5VL_container_t *grp_container = NULL;    /* Container for group */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL wrapper info")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for group */
    grp_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (grp = H5VL__group_create(obj, loc_params, grp_container->connector->cls, name, lcpl_id, gcpl_id, gapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "group create failed")

    /* Get container for group (accommodates external links) */
    if (NULL == (grp_container = H5VL__get_container_for_obj(grp, H5I_GROUP, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for group")

    /* Create VOL object for group */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_GROUP, grp, grp_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for group")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (grp && grp_container && H5VL__group_close(grp, vol_obj->container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "group close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container info")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_create_api_op
 *
 * Purpose:     Callback for common API wrapper to create a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_create_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_group_create_ctx_t *ctx       = (H5VL_group_create_ctx_t *)_ctx; /* Get pointer to context */
    void *grp = NULL;          /* Group object from VOL connector */
    herr_t                   ret_value = SUCCEED;                         /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (grp = H5VL__group_create(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->lcpl_id, ctx->gcpl_id, ctx->gapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "unable to create group")

    /* Set up object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_GROUP, grp)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for group")
    } /* end if */
    else
        ctx->ret_value = grp;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_create_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_create
 *
 * Purpose:     Creates a group
 *
 * Return:      Success:    Pointer to the new group
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLgroup_create(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
                 hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_group_create_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *        connector;              /* VOL connector struct */
    void *                  ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE9("*x", "*x*#i*siiiix", obj, loc_params, connector_id, name, lcpl_id, gcpl_id, gapl_id, dxpl_id,
             req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.lcpl_id    = lcpl_id;
    ctx.gcpl_id    = gcpl_id;
    ctx.gapl_id    = gapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_create_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__group_open
 *
 * Purpose:	Opens a group through the VOL
 *
 * Return:      Success: Pointer to group
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__group_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls, const char *name,
                 hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'group open' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->group_cls.open)(obj, loc_params, name, gapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "group open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_group_open
 *
 * Purpose:	Opens a group through the VOL
 *
 * Return:      Success: Pointer to VOL object for group
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_group_open(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, const char *name,
                hid_t gapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    void *grp = NULL;          /* Group object from VOL connector */
    H5VL_container_t *grp_container = NULL;    /* Container for group */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Set container for group */
    grp_container = vol_obj->container;

    /* Call the corresponding internal VOL routine */
    if (NULL == (grp = H5VL__group_open(obj, loc_params, grp_container->connector->cls, name, gapl_id, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "group open failed")

    /* Get container for group (accommodates external links) */
    if (NULL == (grp_container = H5VL__get_container_for_obj(grp, H5I_GROUP, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get container for group")

    /* Create VOL object for dataset */
    if (NULL == (ret_value = H5VL__new_vol_obj(H5VL_OBJ_GROUP, grp, grp_container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for group")

done:
    /* Clean up on failure */
    if (NULL == ret_value)
        if (grp && grp_container && H5VL__group_close(grp, grp_container->connector->cls, H5P_DATASET_XFER_DEFAULT, H5_REQUEST_NULL) < 0)
            HDONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, NULL, "group close failed")

    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_open_api_op
 *
 * Purpose:     Callback for common API wrapper to open a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_open_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_group_open_ctx_t *ctx       = (H5VL_group_open_ctx_t *)_ctx; /* Get pointer to context */
    void *grp = NULL;          /* Group object from VOL connector */
    herr_t                 ret_value = SUCCEED;                       /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (grp = H5VL__group_open(obj, ctx->loc_params, ctx->cls, ctx->name, ctx->gapl_id, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, FAIL, "unable to open group")

    /* Set up dataset object to return */
    if(new_api_ctx) {
        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(H5VL_OBJ_GROUP, grp)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for group")
    } /* end if */
    else
        ctx->ret_value = grp;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_open_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_open
 *
 * Purpose:     Opens a group
 *
 * Return:      Success:    Pointer to the group
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLgroup_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, const char *name,
               hid_t gapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_group_open_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *      connector;              /* VOL connector struct */
    void *                ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE7("*x", "*x*#i*siix", obj, loc_params, connector_id, name, gapl_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.name       = name;
    ctx.gapl_id    = gapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_open_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__group_get
 *
 * Purpose:	Get specific information about the group through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_get(void *obj, const H5VL_class_t *cls, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no group 'get' method")

    /* Call the corresponding VOL callback */
    if ((cls->group_cls.get)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "group 'get' operation failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_group_get
 *
 * Purpose:	Get specific information about the group through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_group_get(const H5VL_object_t *vol_obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_get(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "group 'get' operation failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_group_get_ctx_t *ctx       = (H5VL_group_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                ret_value = SUCCEED;                      /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_get(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute group 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_get
 *
 * Purpose:     Gets information about the group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_get(void *obj, hid_t connector_id, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_group_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t               ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__group_specific
 *
 * Purpose:	Specific operation on groups through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_specific(void *obj, const H5VL_class_t *cls, H5VL_group_specific_args_t *args, hid_t dxpl_id,
                     void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'group specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->group_cls.specific)(obj, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute group 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_group_specific
 *
 * Purpose:	Specific operation on groups through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_group_specific(const H5VL_object_t *vol_obj, H5VL_group_specific_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_specific(obj, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute group 'specific' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to issue specific operations on a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_group_specific_ctx_t *ctx       = (H5VL_group_specific_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                     ret_value = SUCCEED;                           /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_specific(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute group 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_specific
 *
 * Purpose:     Performs a connector-specific operation on a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_specific(void *obj, hid_t connector_id, H5VL_group_specific_args_t *args, hid_t dxpl_id,
                   void **req /*out*/)
{
    H5VL_group_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                    ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls     = connector->cls;
    ctx.args    = args;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__group_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id,
                     void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'group optional' method")

    /* Call the corresponding VOL callback */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (cls->group_cls.optional)(obj, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute group 'optional' callback");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_group_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_group_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = H5VL__group_optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute group 'optional' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to perform optional operation on a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_group_optional_ctx_t *ctx = (H5VL_group_optional_ctx_t *)_ctx; /* Get pointer to context */

    FUNC_ENTER_STATIC_NOERR

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    /* (Must capture return value from callback, for iterators) */
    ctx->ret_value = H5VL__group_optional(obj, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5VL__group_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_optional
 *
 * Purpose:     Performs an optional connector-specific operation on a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_optional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                   void **req /*out*/)
{
    H5VL_group_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                    ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.cls       = connector->cls;
    ctx.args      = args;
    ctx.dxpl_id   = dxpl_id;
    ctx.req       = req;
    ctx.ret_value = -1;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

    /* Must return value from callback, for iterators */
    if ((ret_value = ctx.ret_value) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute group 'optional' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t group_id,
                      H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5VL_object_t *vol_obj   = NULL;            /* Group VOL object */
    void *         token     = NULL;            /* Request token for async operation        */
    void **        token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    herr_t         ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE7("e", "*s*sIui*!ii", app_file, app_func, app_line, group_id, args, dxpl_id, es_id);

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Call the corresponding internal VOL routine */
    if ((ret_value = H5VL__common_optional_op(group_id, H5I_GROUP, H5VL__group_optional, args, dxpl_id, token_ptr, &vol_obj)) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute group 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token,
                        H5ARG_TRACE7(__func__, "*s*sIui*!ii", app_file, app_func, app_line, group_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLgroup_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_close
 *
 * Purpose:     Closes a group through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_close(void *obj, const H5VL_class_t *cls, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->group_cls.close)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'group close' method")

    /* Call the corresponding VOL callback */
    if ((cls->group_cls.close)(obj, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "group close failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_group_close
 *
 * Purpose:     Closes a group through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_group_close(const H5VL_object_t *vol_obj, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_close(obj, vol_obj->container->connector->cls, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "group close failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_group_close() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__group_close_api_op
 *
 * Purpose:     Callback for common API wrapper to close a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__group_close_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_group_close_ctx_t *ctx       = (H5VL_group_close_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__group_close(obj, ctx->cls, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL, "unable to close group")

    /* Close VOL object, if API context */
    if(new_api_ctx)
        if (H5VL_free_object(ctx->obj) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTDEC, FAIL, "unable to free group VOL object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__group_close_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLgroup_close
 *
 * Purpose:     Closes a group
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLgroup_close(void *obj, hid_t connector_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_group_close_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *    connector;                 /* VOL connector struct */
    herr_t                 ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiix", obj, connector_id, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.obj     = obj;
    ctx.cls     = connector->cls;
    ctx.dxpl_id = dxpl_id;
    ctx.req     = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__group_close_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLgroup_close() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_create
 *
 * Purpose:	Creates a link through the VOL
 *
 * Note:	The 'obj' parameter is allowed to be NULL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                  const H5VL_class_t *cls, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.create)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link create' method")

    /* Call the corresponding VOL callback */
    if ((cls->link_cls.create)(args, obj, loc_params, lcpl_id, lapl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "link create failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_create
 *
 * Purpose:	Creates a link through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_create(H5VL_link_create_args_t *args, const H5VL_object_t *vol_obj,
                 const H5VL_loc_params_t *loc_params, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t        ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_create(args, obj, loc_params, vol_obj->container->connector->cls, lcpl_id, lapl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "link create failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_create() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_create_api_op
 *
 * Purpose:     Callback for common API wrapper to create a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_create_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_link_create_ctx_t *ctx       = (H5VL_link_create_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_create(ctx->args, obj, ctx->loc_params, ctx->cls, ctx->lcpl_id, ctx->lapl_id, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "unable to create link")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_create_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_create
 *
 * Purpose:     Creates a link
 *
 * Note:	The 'obj' parameter is allowed to be NULL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_create(H5VL_link_create_args_t *args, void *obj, const H5VL_loc_params_t *loc_params,
                hid_t connector_id, hid_t lcpl_id, hid_t lapl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_create_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t                 ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE8("e", "*!*x*#iiiix", args, obj, loc_params, connector_id, lcpl_id, lapl_id, dxpl_id, req);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.args       = args;
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.lcpl_id    = lcpl_id;
    ctx.lapl_id    = lapl_id;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__link_create_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_create() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_copy
 *
 * Purpose:	Copys a link from src to dst.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, void *dst_obj,
                const H5VL_loc_params_t *dst_loc_params, const H5VL_class_t *cls, hid_t lcpl_id,
                hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.copy)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link copy' method")

    /* Call the corresponding VOL callback */
    if ((cls->link_cls.copy)(src_obj, src_loc_params, dst_obj, dst_loc_params, lcpl_id, lapl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "link copy failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_copy() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_copy
 *
 * Purpose:	Copys a link from src to dst.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_copy(const H5VL_object_t *src_vol_obj, const H5VL_loc_params_t *src_loc_params,
               const H5VL_object_t *dst_vol_obj, const H5VL_loc_params_t *dst_loc_params, hid_t lcpl_id,
               hid_t lapl_id, hid_t dxpl_id, void **req)
{
    void *s_obj, *d_obj;                  /* Objects to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t               ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(src_vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual objects */
    s_obj = (src_vol_obj->obj_type == H5VL_OBJ_FILE) ? src_vol_obj->container->object : src_vol_obj->object;
    if(dst_vol_obj)
        d_obj = (dst_vol_obj->obj_type == H5VL_OBJ_FILE) ? dst_vol_obj->container->object : dst_vol_obj->object;
    else
        d_obj = NULL;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_copy(s_obj, src_loc_params, d_obj, dst_loc_params, src_vol_obj->container->connector->cls, lcpl_id, lapl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "link copy failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_copy() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_copy_api_op
 *
 * Purpose:     Callback for common API wrapper to copy a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_copy_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_link_copy_ctx_t *ctx       = (H5VL_link_copy_ctx_t *)_ctx; /* Get pointer to context */
    void *dst_obj;              /* Destination VOL connector object */
    herr_t                ret_value = SUCCEED;                      /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Get actual destination object */
    dst_obj = new_api_ctx ? ((H5VL_object_t *)ctx->dst_obj)->object : ctx->dst_obj;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_copy(obj, ctx->src_loc_params, dst_obj, ctx->dst_loc_params, ctx->cls, ctx->lcpl_id, ctx->lapl_id, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "unable to copy object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_copy_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_copy
 *
 * Purpose:     Copies a link to a new location
 *
 * Note:	The 'src_obj' and 'dst_obj' parameters are allowed to be NULL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, void *dst_obj,
              const H5VL_loc_params_t *dst_loc_params, hid_t connector_id, hid_t lcpl_id, hid_t lapl_id,
              hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_copy_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t               ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE9("e", "*x*#*x*#iiiix", src_obj, src_loc_params, dst_obj, dst_loc_params, connector_id, lcpl_id,
             lapl_id, dxpl_id, req);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.src_loc_params = src_loc_params;
    ctx.dst_obj        = dst_obj;
    ctx.dst_loc_params = dst_loc_params;
    ctx.cls            = connector->cls;
    ctx.lcpl_id        = lcpl_id;
    ctx.lapl_id        = lapl_id;
    ctx.dxpl_id        = dxpl_id;
    ctx.req            = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(src_obj, dxpl_id, H5VL__link_copy_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_copy() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_move
 *
 * Purpose:	Moves a link from src to dst.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_move(void *src_obj, const H5VL_loc_params_t *src_loc_params, void *dst_obj,
                const H5VL_loc_params_t *dst_loc_params, const H5VL_class_t *cls, hid_t lcpl_id,
                hid_t lapl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.move)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link move' method")

    /* Call the corresponding VOL callback */
    if ((cls->link_cls.move)(src_obj, src_loc_params, dst_obj, dst_loc_params, lcpl_id, lapl_id, dxpl_id,
                             req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTMOVE, FAIL, "link move failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_move() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_move
 *
 * Purpose:	Moves a link from src to dst.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_move(const H5VL_object_t *src_vol_obj, const H5VL_loc_params_t *src_loc_params,
               const H5VL_object_t *dst_vol_obj, const H5VL_loc_params_t *dst_loc_params, hid_t lcpl_id,
               hid_t lapl_id, hid_t dxpl_id, void **req)
{
    void *s_obj, *d_obj;                  /* Objects to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t               ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(src_vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual objects */
    s_obj = (src_vol_obj->obj_type == H5VL_OBJ_FILE) ? src_vol_obj->container->object : src_vol_obj->object;
    if(dst_vol_obj)
        d_obj = (dst_vol_obj->obj_type == H5VL_OBJ_FILE) ? dst_vol_obj->container->object : dst_vol_obj->object;
    else
        d_obj = NULL;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_move(s_obj, src_loc_params, d_obj, dst_loc_params, src_vol_obj->container->connector->cls, lcpl_id, lapl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTMOVE, FAIL, "link move failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_move() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_move_api_op
 *
 * Purpose:     Callback for common API wrapper to move a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_move_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_link_move_ctx_t *ctx       = (H5VL_link_move_ctx_t *)_ctx; /* Get pointer to context */
    void *dst_obj;              /* Destination VOL connector object */
    herr_t                ret_value = SUCCEED;                      /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Get actual destination object */
    dst_obj = new_api_ctx ? ((H5VL_object_t *)ctx->dst_obj)->object : ctx->dst_obj;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_move(obj, ctx->src_loc_params, dst_obj, ctx->dst_loc_params, ctx->cls, ctx->lcpl_id, ctx->lapl_id, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTMOVE, FAIL, "unable to move object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_move_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_move
 *
 * Purpose:     Moves a link to another location
 *
 * Note:	The 'src_obj' and 'dst_obj' parameters are allowed to be NULL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_move(void *src_obj, const H5VL_loc_params_t *src_loc_params, void *dst_obj,
              const H5VL_loc_params_t *dst_loc_params, hid_t connector_id, hid_t lcpl_id, hid_t lapl_id,
              hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_move_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *       connector;                 /* VOL connector struct */
    herr_t               ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE9("e", "*x*#*x*#iiiix", src_obj, src_loc_params, dst_obj, dst_loc_params, connector_id, lcpl_id,
             lapl_id, dxpl_id, req);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.src_loc_params = src_loc_params;
    ctx.dst_obj        = dst_obj;
    ctx.dst_loc_params = dst_loc_params;
    ctx.cls            = connector->cls;
    ctx.lcpl_id        = lcpl_id;
    ctx.lapl_id        = lapl_id;
    ctx.dxpl_id        = dxpl_id;
    ctx.req            = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(src_obj, dxpl_id, H5VL__link_move_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_move() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_get
 *
 * Purpose:	Get specific information about the link through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_get(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
               H5VL_link_get_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link get' method")

    /* Call the corresponding VOL callback */
    if ((cls->link_cls.get)(obj, loc_params, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "link get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_get
 *
 * Purpose:	Get specific information about the link through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_get(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, H5VL_link_get_args_t *args,
              hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_get(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "link 'get' operation failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_link_get_ctx_t *ctx       = (H5VL_link_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t               ret_value = SUCCEED;                     /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_get(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute link 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_get
 *
 * Purpose:     Gets information about a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_get(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, H5VL_link_get_args_t *args,
             hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t              ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__link_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_specific
 *
 * Purpose:	Specific operation on links through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                    H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link specific' method")

    /* Call the corresponding VOL callback */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (cls->link_cls.specific)(obj, loc_params, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute link 'specific' callback");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_specific
 *
 * Purpose:	Specific operation on links through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_specific(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                   H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = H5VL__link_specific(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute link 'specific' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to issue specific operations on a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_link_specific_ctx_t *ctx = (H5VL_link_specific_ctx_t *)_ctx; /* Get pointer to context */

    FUNC_ENTER_STATIC_NOERR

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    /* (Must capture return value from callback, for iterators) */
    ctx->ret_value = H5VL__link_specific(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5VL__link_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_specific
 *
 * Purpose:     Performs a connector-specific operation on a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                  H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = -1;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__link_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

    /* Must return value from callback, for iterators */
    if ((ret_value = ctx.ret_value) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute link 'specific' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__link_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_optional(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                    H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->link_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'link optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->link_cls.optional)(obj, loc_params, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute link 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_link_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_link_optional(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                   H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_optional(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute link 'optional' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_link_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__link_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to issue optional operations on a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__link_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_link_optional_ctx_t *ctx       = (H5VL_link_optional_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                    ret_value = SUCCEED;                          /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_optional(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute link 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__link_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_optional
 *
 * Purpose:     Performs an optional connector-specific operation on a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_optional(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                  H5VL_optional_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_link_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                   ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__link_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLlink_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLlink_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a link
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLlink_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                     const char *name, hid_t lapl_id, H5VL_optional_args_t *args, hid_t dxpl_id, hid_t es_id)
{
    H5VL_object_t *   vol_obj = NULL;              /* Object for loc_id */
    H5VL_loc_params_t loc_params;                  /* Location parameters for object access */
    void *            token     = NULL;            /* Request token for async operation        */
    void **           token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t            ret_value       = SUCCEED;   /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE9("e", "*s*sIui*si*!ii", app_file, app_func, app_line, loc_id, name, lapl_id, args, dxpl_id,
             es_id);

    /* Check arguments */
    /* name is verified in H5VL_setup_name_args() */

    /* Set up object access arguments */
    if (H5VL_setup_name_args(loc_id, name, FALSE, lapl_id, &vol_obj, &loc_params) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set link access arguments")

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Call the corresponding internal VOL routine */
    if (H5VL__link_optional(vol_obj->object, &loc_params, vol_obj->container->connector->cls, args, dxpl_id, token_ptr) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute link 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token, H5ARG_TRACE9(__func__, "*s*sIui*si*!ii", app_file, app_func, app_line, loc_id, name, lapl_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_API(ret_value)
} /* end H5VLlink_optional_op() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__object_open
 *
 * Purpose:	Opens a object through the VOL
 *
 * Return:      Success: Pointer to the object
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
static void *
H5VL__object_open(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                  H5I_type_t *opened_type, hid_t dxpl_id, void **req)
{
    void *ret_value = NULL; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->object_cls.open)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "VOL connector has no 'object open' method")

    /* Call the corresponding VOL callback */
    if (NULL == (ret_value = (cls->object_cls.open)(obj, loc_params, opened_type, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "object open failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_object_open
 *
 * Purpose:	Opens a object through the VOL
 *
 * Return:      Success: Pointer to the object
 *		Failure: NULL
 *
 *-------------------------------------------------------------------------
 */
H5VL_object_t *
H5VL_object_open(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params, H5I_type_t *opened_type,
                 hid_t dxpl_id, void **req)
{
    void *v_obj;                  /* Object to operate on */
    void *obj = NULL;          /* Object from VOL connector */
    H5VL_obj_type_t vol_obj_type = 0;       /* VOL object type for object */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    H5VL_object_t *  ret_value       = NULL;  /* Return value */

    FUNC_ENTER_NOAPI(NULL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, NULL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    v_obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (NULL == (obj = H5VL__object_open(v_obj, loc_params, vol_obj->container->connector->cls, opened_type, dxpl_id, req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, NULL, "object open failed")

    /* Get the VOL object type for the object */
    if (H5VL_id_to_obj_type(*opened_type, &vol_obj_type) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "can't get VOL object type for object")
    HDassert(vol_obj_type > 0);

    /* Create VOL object for object */
    if (NULL == (ret_value = H5VL__new_vol_obj(vol_obj_type, obj, vol_obj->container)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "can't create VOL object for object")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, NULL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_open() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object_open_api_op
 *
 * Purpose:     Callback for common API wrapper to open an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_open_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_object_open_ctx_t *ctx       = (H5VL_object_open_ctx_t *)_ctx; /* Get pointer to context */
    void *new_obj = NULL;          /* Object from VOL connector */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (NULL == (new_obj = H5VL__object_open(obj, ctx->loc_params, ctx->cls, ctx->opened_type, ctx->dxpl_id, ctx->req)))
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPENOBJ, FAIL, "unable to open object")

    /* Set up dataset object to return */
    if(new_api_ctx) {
        H5VL_obj_type_t vol_obj_type = 0;       /* VOL object type for object */

        /* Get the VOL object type for the object */
        if (H5VL_id_to_obj_type(*ctx->opened_type, &vol_obj_type) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't get VOL object type for object")
        HDassert(vol_obj_type > 0);

        if (NULL == (ctx->ret_value = H5VL__create_object_with_container_ctx(vol_obj_type, new_obj)))
            HGOTO_ERROR(H5E_VOL, H5E_CANTCREATE, FAIL, "can't create VOL object for object")
    } /* end if */
    else
        ctx->ret_value = new_obj;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_open_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_open
 *
 * Purpose:     Opens an object
 *
 * Return:      Success:    Pointer to the object
 *              Failure:    NULL
 *
 *-------------------------------------------------------------------------
 */
void *
H5VLobject_open(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id, H5I_type_t *opened_type,
                hid_t dxpl_id, void **req /*out*/)
{
    H5VL_object_open_ctx_t ctx;              /* Context for common API wrapper call */
    H5VL_connector_t *      connector;              /* VOL connector struct */
    void *                 ret_value = NULL; /* Return value */

    FUNC_ENTER_API_WRAPPER(NULL)
    H5TRACE6("*x", "*x*#i*Itix", obj, loc_params, connector_id, opened_type, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params  = loc_params;
    ctx.cls         = connector->cls;
    ctx.opened_type = opened_type;
    ctx.dxpl_id     = dxpl_id;
    ctx.req         = req;
    ctx.ret_value   = NULL;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__object_open_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, NULL, "unable to execute common wrapper operation")

    /* Set return value */
    ret_value = ctx.ret_value;

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLobject_open() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__object_copy
 *
 * Purpose:	Copies an object to another destination through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name, void *dst_obj,
                  const H5VL_loc_params_t *dst_loc_params, const char *dst_name, const H5VL_class_t *cls,
                  hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->object_cls.copy)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'object copy' method")

    /* Call the corresponding VOL callback */
    if ((cls->object_cls.copy)(src_obj, src_loc_params, src_name, dst_obj, dst_loc_params, dst_name, ocpypl_id, lcpl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "object copy failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_copy() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_object_copy
 *
 * Purpose:	Copies an object to another destination through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_copy(const H5VL_object_t *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name,
                 const H5VL_object_t *dst_obj, const H5VL_loc_params_t *dst_loc_params, const char *dst_name,
                 hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req)
{
    void *s_obj, *d_obj;                  /* Objects to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    hbool_t src_container_ctx_set = FALSE; /* Whether the 'src' VOL container context was set up */
    hbool_t dst_container_ctx_set = FALSE; /* Whether the 'dst' VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Make sure that the VOL connectors are the same */
    if (src_obj->container->connector->cls->value != dst_obj->container->connector->cls->value)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "objects are accessed through different VOL connectors and can't be copied")

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(src_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Set 'src' container info in API context */
    if (H5VL__set_src_container_ctx(src_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    src_container_ctx_set = TRUE;

    /* Set 'dst' container info in API context */
    if (H5VL__set_dst_container_ctx(dst_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL wrapper info")
    dst_container_ctx_set = TRUE;

    /* Get actual objects */
    s_obj = (src_obj->obj_type == H5VL_OBJ_FILE) ? src_obj->container->object : src_obj->object;
    d_obj = (dst_obj->obj_type == H5VL_OBJ_FILE) ? dst_obj->container->object : dst_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_copy(s_obj, src_loc_params, src_name, d_obj, dst_loc_params, dst_name,
                          src_obj->container->connector->cls, ocpypl_id, lcpl_id, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "object copy failed")

done:
    /* Reset container info in API context */
    if (dst_container_ctx_set && H5VL__reset_dst_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'dst' VOL container info")
    if (src_container_ctx_set && H5VL__reset_src_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset 'src' VOL container info")
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_copy() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object_copy_api_op
 *
 * Purpose:     Callback for common API wrapper to copy a object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_copy_api_op(void *obj, hbool_t new_api_ctx, void *_ctx)
{
    H5VL_object_copy_ctx_t *ctx       = (H5VL_object_copy_ctx_t *)_ctx; /* Get pointer to context */
    void *dst_obj;              /* Destination VOL connector object */
    herr_t                  ret_value = SUCCEED;                        /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Get actual destination object */
    dst_obj = new_api_ctx ? ((H5VL_object_t *)ctx->dst_obj)->object : ctx->dst_obj;

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_copy(obj, ctx->src_loc_params, ctx->src_name, dst_obj, ctx->dst_loc_params,
                          ctx->dst_name, ctx->cls, ctx->ocpypl_id, ctx->lcpl_id, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOPY, FAIL, "unable to copy object")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_copy_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_copy
 *
 * Purpose:     Copies an object to another location
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_copy(void *src_obj, const H5VL_loc_params_t *src_loc_params, const char *src_name, void *dst_obj,
                const H5VL_loc_params_t *dst_loc_params, const char *dst_name, hid_t connector_id,
                hid_t ocpypl_id, hid_t lcpl_id, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_object_copy_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *      connector;              /* VOL connector struct */
    herr_t                 ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE11("e", "*x*#*s*x*#*siiiix", src_obj, src_loc_params, src_name, dst_obj, dst_loc_params, dst_name,
              connector_id, ocpypl_id, lcpl_id, dxpl_id, req);

    /* Check args and get class pointers */
    if (NULL == src_obj || NULL == dst_obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.src_loc_params = src_loc_params;
    ctx.src_name       = src_name;
    ctx.dst_obj        = dst_obj;
    ctx.dst_loc_params = dst_loc_params;
    ctx.dst_name       = dst_name;
    ctx.cls            = connector->cls;
    ctx.ocpypl_id      = ocpypl_id;
    ctx.lcpl_id        = lcpl_id;
    ctx.dxpl_id        = dxpl_id;
    ctx.req            = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(src_obj, dxpl_id, H5VL__object_copy_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLobject_copy() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__object_get
 *
 * Purpose:	Get specific information about the object through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_get(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                 H5VL_object_get_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->object_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'object get' method")

    /* Call the corresponding VOL callback */
    if ((cls->object_cls.get)(obj, loc_params, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_object_get
 *
 * Purpose:	Get specific information about the object through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_get(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                H5VL_object_get_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_get(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "get failed")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object_get_api_op
 *
 * Purpose:     Callback for common API wrapper to get information about a object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_get_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_object_get_ctx_t *ctx       = (H5VL_object_get_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                 ret_value = SUCCEED;                       /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_get(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "unable to execute object 'get' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_get_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_get
 *
 * Purpose:     Gets information about an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_get(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
               H5VL_object_get_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_object_get_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__object_get_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLobject_get() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__object_specific
 *
 * Purpose:	Specific operation on objects through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_specific(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                      H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->object_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'object specific' method")

    /* Call the corresponding VOL callback */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = (cls->object_cls.specific)(obj, loc_params, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "object specific failed");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_object_specific
 *
 * Purpose:	Specific operation on objects through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_specific(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                     H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    /* (Must return value from callback, for iterators) */
    if ((ret_value = H5VL__object_specific(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "object specific failed");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object_specific_api_op
 *
 * Purpose:     Callback for common API wrapper to issue specific operations on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_specific_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_object_specific_ctx_t *ctx = (H5VL_object_specific_ctx_t *)_ctx; /* Get pointer to context */

    FUNC_ENTER_STATIC_NOERR

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    /* (Must capture return value from callback, for iterators) */
    ctx->ret_value = H5VL__object_specific(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req);

    FUNC_LEAVE_NOAPI(SUCCEED)
} /* end H5VL__object_specific_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_specific
 *
 * Purpose:     Performs a connector-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_specific(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                    H5VL_object_specific_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_object_specific_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;
    ctx.ret_value  = -1;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__object_specific_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

    /* Must return value from callback, for iterators */
    if ((ret_value = ctx.ret_value) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute link 'specific' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLobject_specific() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__object_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_optional(void *obj, const H5VL_loc_params_t *loc_params, const H5VL_class_t *cls,
                      H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->object_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'object optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->object_cls.optional)(obj, loc_params, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute object 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_optional() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_object_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_object_optional(const H5VL_object_t *vol_obj, const H5VL_loc_params_t *loc_params,
                     H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_optional(obj, loc_params, vol_obj->container->connector->cls, args, dxpl_id, req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute object 'optional' callback")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_object_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__object_optional_api_op
 *
 * Purpose:     Callback for common API wrapper to issue optional operations on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__object_optional_api_op(void *obj, hbool_t H5_ATTR_UNUSED new_api_ctx, void *_ctx)
{
    H5VL_object_optional_ctx_t *ctx       = (H5VL_object_optional_ctx_t *)_ctx; /* Get pointer to context */
    herr_t                      ret_value = SUCCEED;                            /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(ctx);

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_optional(obj, ctx->loc_params, ctx->cls, ctx->args, ctx->dxpl_id, ctx->req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute object 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__object_optional_api_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_optional
 *
 * Purpose:     Performs an optional connector-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_optional(void *obj, const H5VL_loc_params_t *loc_params, hid_t connector_id,
                    H5VL_optional_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_object_optional_ctx_t ctx;                 /* Context for common API wrapper call */
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t                     ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*x*#i*!ix", obj, loc_params, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Set up context */
    ctx.loc_params = loc_params;
    ctx.cls        = connector->cls;
    ctx.args       = args;
    ctx.dxpl_id    = dxpl_id;
    ctx.req        = req;

    /* Invoke common wrapper routine */
    if (H5VL__common_api_op(obj, dxpl_id, H5VL__object_optional_api_op, &ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute common wrapper operation")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLobject_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLobject_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on an object
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLobject_optional_op(const char *app_file, const char *app_func, unsigned app_line, hid_t loc_id,
                       const char *name, hid_t lapl_id, H5VL_optional_args_t *args, hid_t dxpl_id,
                       hid_t es_id)
{
    void *obj;                  /* Object to operate on */
    H5VL_object_t *   vol_obj = NULL;              /* Object for loc_id */
    H5VL_loc_params_t loc_params;                  /* Location parameters for object access */
    void *            token     = NULL;            /* Request token for async operation        */
    void **           token_ptr = H5_REQUEST_NULL; /* Pointer to request token for async operation        */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t            ret_value       = SUCCEED;   /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE9("e", "*s*sIui*si*!ii", app_file, app_func, app_line, loc_id, name, lapl_id, args, dxpl_id,
             es_id);

    /* Check arguments */
    /* name is verified in H5VL_setup_name_args() */

    /* Set up object access arguments */
    if (H5VL_setup_name_args(loc_id, name, FALSE, lapl_id, &vol_obj, &loc_params) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set link access arguments")

    /* Set up request token pointer for asynchronous operation */
    if (H5ES_NONE != es_id)
        token_ptr = &token; /* Point at token for VOL connector to set up */

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__object_optional(obj, &loc_params, vol_obj->container->connector->cls, args, dxpl_id, token_ptr) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute object 'optional' callback")

    /* If a token was created, add the token to the event set */
    if (NULL != token)
        /* clang-format off */
        if (H5ES_insert(es_id, vol_obj->container->connector, token, H5ARG_TRACE9(__func__, "*s*sIui*si*!ii", app_file, app_func, app_line, loc_id, name, lapl_id, args, dxpl_id, es_id)) < 0)
            /* clang-format on */
            HGOTO_ERROR(H5E_VOL, H5E_CANTINSERT, FAIL, "can't insert token into event set")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_API(ret_value)
} /* end H5VLobject_optional_op() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__introspect_get_conn_cls
 *
 * Purpose:     Calls the connector-specific callback to query the connector
 *              class.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL__introspect_get_conn_cls(void *obj, const H5VL_class_t *cls, H5VL_get_conn_lvl_t lvl,
                              const H5VL_class_t **conn_cls)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_PACKAGE

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);
    HDassert(lvl >= H5VL_GET_CONN_LVL_CURR && lvl <= H5VL_GET_CONN_LVL_TERM);
    HDassert(conn_cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->introspect_cls.get_conn_cls)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'get_conn_cls' method")

    /* Call the corresponding VOL callback */
    if ((cls->introspect_cls.get_conn_cls)(obj, lvl, conn_cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector class")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__introspect_get_conn_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_introspect_get_conn_cls
 *
 * Purpose:     Calls the connector-specific callback to query the connector
 *              class.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_introspect_get_conn_cls(const H5VL_object_t *vol_obj, H5VL_get_conn_lvl_t lvl,
                             const H5VL_class_t **conn_cls)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__introspect_get_conn_cls(obj, vol_obj->container->connector->cls, lvl, conn_cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector class")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_introspect_get_conn_cls() */

/*-------------------------------------------------------------------------
 * Function:    H5VLintrospect_get_conn_cls
 *
 * Purpose:     Calls the connector-specific callback to query the connector
 *              class.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLintrospect_get_conn_cls(void *obj, hid_t connector_id, H5VL_get_conn_lvl_t lvl,
                            const H5VL_class_t **conn_cls /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiVLx", obj, connector_id, lvl, conn_cls);

    /* Check args */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "NULL obj pointer")
    if (NULL == conn_cls)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "NULL conn_cls pointer")

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__introspect_get_conn_cls(obj, connector->cls, lvl, conn_cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector class")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLintrospect_get_conn_cls() */

/*-------------------------------------------------------------------------
 * Function:	H5VL_introspect_get_cap_flags
 *
 * Purpose:     Calls the connector-specific callback to query the connector's
 *              capability flags.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_introspect_get_cap_flags(const void *info, const H5VL_class_t *cls, unsigned *cap_flags)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(cls);
    HDassert(cap_flags);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->introspect_cls.get_cap_flags)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'get_cap_flags' method")

    /* Call the corresponding VOL callback */
    if ((cls->introspect_cls.get_cap_flags)(info, cap_flags) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector capability flags")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_introspect_get_cap_flags() */

/*-------------------------------------------------------------------------
 * Function:    H5VLintrospect_get_cap_flags
 *
 * Purpose:     Calls the connector-specific callback to query the connector's
 *              capability flags.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLintrospect_get_cap_flags(const void *info, hid_t connector_id, unsigned *cap_flags /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xix", info, connector_id, cap_flags);

    /* Check args */
    if (NULL == cap_flags)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "NULL conn_cls pointer")

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL_introspect_get_cap_flags(info, connector->cls, cap_flags) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query connector's capability flags")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLintrospect_get_cap_flags() */

/*-------------------------------------------------------------------------
 * Function:	H5VL__introspect_opt_query
 *
 * Purpose:     Calls the connector-specific callback to query if an optional
 *              operation is supported.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__introspect_opt_query(void *obj, const H5VL_class_t *cls, H5VL_subclass_t subcls, int opt_type,
                           uint64_t *flags)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->introspect_cls.opt_query)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'opt_query' method")

    /* Call the corresponding VOL callback */
    if ((cls->introspect_cls.opt_query)(obj, subcls, opt_type, flags) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query optional operation support")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__introspect_opt_query() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_introspect_opt_query
 *
 * Purpose:     Calls the connector-specific callback to query if an optional
 *              operation is supported.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_introspect_opt_query(const H5VL_object_t *vol_obj, H5VL_subclass_t subcls, int opt_type, uint64_t *flags)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__introspect_opt_query(obj, vol_obj->container->connector->cls, subcls, opt_type, flags) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query optional operation support")

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_introspect_opt_query() */

/*-------------------------------------------------------------------------
 * Function:    H5VLintrospect_opt_query
 *
 * Purpose:     Calls the connector-specific callback to query if an optional
 *              operation is supported.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLintrospect_opt_query(void *obj, hid_t connector_id, H5VL_subclass_t subcls, int opt_type,
                         uint64_t *flags /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xiVSIsx", obj, connector_id, subcls, opt_type, flags);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__introspect_opt_query(obj, connector->cls, subcls, opt_type, flags) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "can't query optional operation support")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLintrospect_opt_query() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_wait
 *
 * Purpose:     Waits on an asychronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_wait(void *req, const H5VL_class_t *cls, uint64_t timeout, H5VL_request_status_t *status)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity checks */
    HDassert(req);
    HDassert(cls);
    HDassert(status);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.wait)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async wait' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.wait)(req, timeout, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request wait failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_wait() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_wait
 *
 * Purpose:     Waits on an asychronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_wait(const H5VL_request_t *request, uint64_t timeout, H5VL_request_status_t *status)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(request);

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_wait(request->token, request->connector->cls, timeout, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request wait failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_wait() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_wait
 *
 * Purpose:     Waits on a request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_wait(void *req, hid_t connector_id, uint64_t timeout, H5VL_request_status_t *status /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiULx", req, connector_id, timeout, status);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_wait(req, connector->cls, timeout, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to wait on request")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_wait() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *		operation completes
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_notify(void *req, const H5VL_class_t *cls, H5VL_request_notify_t cb, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(req);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.notify)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async notify' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.notify)(req, cb, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request notify failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_notify() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *		operation completes
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_notify(const H5VL_request_t *request, H5VL_request_notify_t cb, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(request);

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_notify(request->token, request->connector->cls, cb, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "request notify failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_notify() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_notify
 *
 * Purpose:     Registers a user callback to be invoked when an asynchronous
 *		operation completes
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_notify(void *req, hid_t connector_id, H5VL_request_notify_t cb, void *ctx)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xiVN*x", req, connector_id, cb, ctx);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_notify(req, connector->cls, cb, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "unable to register notify callback for request")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_notify() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_cancel
 *
 * Purpose:     Cancels an asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_cancel(void *req, const H5VL_class_t *cls, H5VL_request_status_t *status)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(req);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.cancel)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async cancel' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.cancel)(req, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request cancel failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_cancel() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_cancel
 *
 * Purpose:     Cancels an asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_cancel(const H5VL_request_t *request, H5VL_request_status_t *status)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(request);

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_cancel(request->token, request->connector->cls, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request cancel failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_cancel() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_cancel
 *
 * Purpose:     Cancels a request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_cancel(void *req, hid_t connector_id, H5VL_request_status_t *status /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xix", req, connector_id, status);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_cancel(req, connector->cls, status) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to cancel request")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_cancel() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_specific
 *
 * Purpose:	Specific operation on asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_specific(void *req, const H5VL_class_t *cls, H5VL_request_specific_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(req);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.specific)(req, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL,
                    "unable to execute asynchronous request 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_specific
 *
 * Purpose:	Specific operation on asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_specific(const H5VL_request_t *request, H5VL_request_specific_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(request);

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_specific(request->token, request->connector->cls, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL,
                    "unable to execute asynchronous request 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_specific
 *
 * Purpose:     Performs a connector-specific operation on an asynchronous request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_specific(void *req, hid_t connector_id, H5VL_request_specific_args_t *args)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xi*!", req, connector_id, args);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_specific(req, connector->cls, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute asynchronous request 'specific' callback")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_optional(void *req, const H5VL_class_t *cls, H5VL_optional_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(req);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.optional)(req, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute asynchronous request 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_optional
 *
 * Purpose:	Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_optional(const H5VL_request_t *request, H5VL_optional_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(request);

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_optional(request->token, request->connector->cls, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute asynchronous request 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_optional
 *
 * Purpose:     Performs an optional connector-specific operation on an asynchronous request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_optional(void *req, hid_t connector_id, H5VL_optional_args_t *args)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE3("e", "*xi*!", req, connector_id, args);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_optional(req, connector->cls, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute asynchronous request 'optional' callback")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_optional_op
 *
 * Purpose:     Performs an optional connector-specific operation on a request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_optional_op(void *req, hid_t connector_id, H5VL_optional_args_t *args)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API(FAIL)
    H5TRACE3("e", "*xi*!", req, connector_id, args);

    /* Check arguments */
    if (NULL == req)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid request")
    if (NULL == args)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments")

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_optional(req, connector->cls, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute request 'optional' callback")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5VLrequest_optional_op() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__request_free
 *
 * Purpose:     Frees an asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__request_free(void *req, const H5VL_class_t *cls)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(req);
    HDassert(cls);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->request_cls.free)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'async free' method")

    /* Call the corresponding VOL callback */
    if ((cls->request_cls.free)(req) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request free failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__request_free() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_request_free
 *
 * Purpose:     Frees an asynchronous request through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_request_free(const H5VL_request_t *request)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(request);

    /* Call the corresponding VOL callback */
    if (H5VL__request_free(request->token, request->connector->cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "request free failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_request_free() */

/*-------------------------------------------------------------------------
 * Function:    H5VLrequest_free
 *
 * Purpose:     Frees a request
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLrequest_free(void *req, hid_t connector_id)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE2("e", "*xi", req, connector_id);

    /* Get class pointer */
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if (H5VL__request_free(req, connector->cls) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTRELEASE, FAIL, "unable to free request")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLrequest_free() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__blob_put
 *
 * Purpose:     Put a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__blob_put(void *obj, const H5VL_class_t *cls, const void *buf, size_t size, void *blob_id, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);
    HDassert(size == 0 || buf);
    HDassert(blob_id);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->blob_cls.put)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'blob put' method")

    /* Call the corresponding VOL callback */
    if ((cls->blob_cls.put)(obj, buf, size, blob_id, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "blob put callback failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__blob_put() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_blob_put
 *
 * Purpose:     Put a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, August 21, 2019
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_blob_put(const H5VL_container_t *container, const void *buf, size_t size, void *blob_id, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(container);
    HDassert(size == 0 || buf);
    HDassert(blob_id);

    /* Call the corresponding VOL callback */
    if (H5VL__blob_put(container->object, container->connector->cls, buf, size, blob_id, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "blob put failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_blob_put() */

/*-------------------------------------------------------------------------
 * Function:    H5VLblob_put
 *
 * Purpose:     Put a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLblob_put(void *obj, hid_t connector_id, const void *buf, size_t size, void *blob_id, void *ctx)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*xi*xz*x*x", obj, connector_id, buf, size, blob_id, ctx);

    /* Get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding VOL callback */
    if (H5VL__blob_put(obj, connector->cls, buf, size, blob_id, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "blob put failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLblob_put() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__blob_get
 *
 * Purpose:     Get a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__blob_get(void *obj, const H5VL_class_t *cls, const void *blob_id, void *buf, size_t size, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);
    HDassert(blob_id);
    HDassert(buf);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->blob_cls.get)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'blob get' method")

    /* Call the corresponding VOL callback */
    if ((cls->blob_cls.get)(obj, blob_id, buf, size, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "blob get callback failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__blob_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_blob_get
 *
 * Purpose:     Get a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_blob_get(const H5VL_container_t *container, const void *blob_id, void *buf, size_t size, void *ctx)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(container);
    HDassert(blob_id);
    HDassert(buf);

    /* Call the corresponding VOL callback */
    if (H5VL__blob_get(container->object, container->connector->cls, blob_id, buf, size, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "blob get failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_blob_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VLblob_get
 *
 * Purpose:     Get a blob through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLblob_get(void *obj, hid_t connector_id, const void *blob_id, void *buf /*out*/, size_t size, void *ctx)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE6("e", "*xi*xxz*x", obj, connector_id, blob_id, buf, size, ctx);

    /* Get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding VOL callback */
    if (H5VL__blob_get(obj, connector->cls, blob_id, buf, size, ctx) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "blob get failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLblob_get() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__blob_specific
 *
 * Purpose:	Specific operation on blobs through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Saturday, August 17, 2019
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__blob_specific(void *obj, const H5VL_class_t *cls, void *blob_id, H5VL_blob_specific_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);
    HDassert(blob_id);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->blob_cls.specific)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'blob specific' method")

    /* Call the corresponding VOL callback */
    if ((cls->blob_cls.specific)(obj, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute blob 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__blob_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_blob_specific
 *
 * Purpose: Specific operation on blobs through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_blob_specific(const H5VL_container_t *container, void *blob_id, H5VL_blob_specific_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(container);
    HDassert(blob_id);

    /* Call the corresponding internal VOL routine */
    if (H5VL__blob_specific(container->object, container->connector->cls, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute blob 'specific' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_blob_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VLblob_specific
 *
 * Purpose: Specific operation on blobs through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLblob_specific(void *obj, hid_t connector_id, void *blob_id, H5VL_blob_specific_args_t *args)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xi*x*!", obj, connector_id, blob_id, args);

    /* Get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding VOL callback */
    if (H5VL__blob_specific(obj, connector->cls, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "blob specific operation failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLblob_specific() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__blob_optional
 *
 * Purpose:	Optional operation on blobs through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 * Programmer:	Quincey Koziol
 *		Thursday, November 14, 2019
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__blob_optional(void *obj, const H5VL_class_t *cls, void *blob_id, H5VL_optional_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity check */
    HDassert(obj);
    HDassert(cls);
    HDassert(blob_id);

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->blob_cls.optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'blob optional' method")

    /* Call the corresponding VOL callback */
    if ((cls->blob_cls.optional)(obj, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute blob 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__blob_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_blob_optional
 *
 * Purpose: Optional operation on blobs through the VOL
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_blob_optional(const H5VL_container_t *container, void *blob_id, H5VL_optional_args_t *args)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity check */
    HDassert(container);
    HDassert(blob_id);

    /* Call the corresponding internal VOL routine */
    if (H5VL__blob_optional(container->object, container->connector->cls, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "unable to execute blob 'optional' callback")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_blob_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLblob_optional
 *
 * Purpose: Optional operation on blobs through the VOL
 *
 * Return:      SUCCEED / FAIL
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLblob_optional(void *obj, hid_t connector_id, void *blob_id, H5VL_optional_args_t *args)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE4("e", "*xi*x*!", obj, connector_id, blob_id, args);

    /* Get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding VOL callback */
    if (H5VL__blob_optional(obj, connector->cls, blob_id, args) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTOPERATE, FAIL, "blob optional operation failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLblob_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__token_cmp
 *
 * Purpose:     Compares two VOL connector object tokens. Sets *cmp_value
 *              to positive if token1 is greater than token2, negative if
 *              token2 is greater than token1 and zero if token1 and
 *              token2 are equal.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__token_cmp(void *obj, const H5VL_class_t *cls, const H5O_token_t *token1, const H5O_token_t *token2,
                int *cmp_value)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity checks */
    HDassert(obj);
    HDassert(cls);
    HDassert(cmp_value);

    /* Take care of cases where one or both pointers is NULL */
    if (token1 == NULL && token2 != NULL)
        *cmp_value = -1;
    else if (token1 != NULL && token2 == NULL)
        *cmp_value = 1;
    else if (token1 == NULL && token2 == NULL)
        *cmp_value = 0;
    else {
        /* Use the class's token comparison routine to compare the tokens,
         * if there is a callback, otherwise just compare the tokens as
         * memory buffers.
         */
        if (cls->token_cls.cmp) {
            if ((cls->token_cls.cmp)(obj, token1, token2, cmp_value) < 0)
                HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "can't compare object tokens")
        } /* end if */
        else
            *cmp_value = HDmemcmp(token1, token2, sizeof(H5O_token_t));
    } /* end else */

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__token_cmp() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_token_cmp
 *
 * Purpose:     Compares two VOL connector object tokens. Sets *cmp_value
 *              to positive if token1 is greater than token2, negative if
 *              token2 is greater than token1 and zero if token1 and
 *              token2 are equal.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_token_cmp(const H5VL_object_t *vol_obj, const H5O_token_t *token1, const H5O_token_t *token2,
               int *cmp_value)
{
    void *obj;                  /* Object to operate on */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(vol_obj);
    HDassert(cmp_value);

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_cmp(obj, vol_obj->container->connector->cls, token1, token2, cmp_value) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "token compare failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_token_cmp() */

/*---------------------------------------------------------------------------
 * Function:    H5VLtoken_cmp
 *
 * Purpose:     Compares two VOL connector object tokens
 *
 * Note:        Both object tokens must be from the same VOL connector class
 *
 * Return:      Success:    Non-negative, with *cmp_value set to positive if
 *                          token1 is greater than token2, negative if token2
 *                          is greater than token1 and zero if token1 and
 *                          token2 are equal.
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLtoken_cmp(void *obj, hid_t connector_id, const H5O_token_t *token1, const H5O_token_t *token2,
              int *cmp_value)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*k*k*Is", obj, connector_id, token1, token2, cmp_value);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")
    if (NULL == cmp_value)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid cmp_value pointer")

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_cmp(obj, connector->cls, token1, token2, cmp_value) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "object token comparison failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLtoken_cmp() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__token_to_str
 *
 * Purpose:     Serialize a connector's object token into a string
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__token_to_str(void *obj, H5I_type_t obj_type, const H5VL_class_t *cls, const H5O_token_t *token,
                   char **token_str)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity checks */
    HDassert(obj);
    HDassert(cls);
    HDassert(token);
    HDassert(token_str);

    /* Use the class's token serialization routine on the token if there is a
     *  callback, otherwise just set the token_str to NULL.
     */
    if (cls->token_cls.to_str) {
        if ((cls->token_cls.to_str)(obj, obj_type, token, token_str) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTSERIALIZE, FAIL, "can't serialize object token")
    } /* end if */
    else
        *token_str = NULL;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__token_to_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_token_to_str
 *
 * Purpose:     Serialize a connector's object token into a string
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_token_to_str(const H5VL_object_t *vol_obj, H5I_type_t obj_type, const H5O_token_t *token,
                  char **token_str)
{
    void *obj;                  /* Object to operate on */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(vol_obj);
    HDassert(token);
    HDassert(token_str);

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_to_str(obj, obj_type, vol_obj->container->connector->cls, token, token_str) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSERIALIZE, FAIL, "token serialization failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_token_to_str() */

/*---------------------------------------------------------------------------
 * Function:    H5VLtoken_to_str
 *
 * Purpose:     Serialize a connector's object token into a string
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLtoken_to_str(void *obj, H5I_type_t obj_type, hid_t connector_id, const H5O_token_t *token,
                 char **token_str)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xIti*k**s", obj, obj_type, connector_id, token, token_str);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")
    if (NULL == token)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid token pointer")
    if (NULL == token_str)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid token_str pointer")

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_to_str(obj, obj_type, connector->cls, token, token_str) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSERIALIZE, FAIL, "object token to string failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLtoken_to_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__token_from_str
 *
 * Purpose:     Deserialize a string into a connector object token
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__token_from_str(void *obj, H5I_type_t obj_type, const H5VL_class_t *cls, const char *token_str,
                     H5O_token_t *token)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Sanity checks */
    HDassert(obj);
    HDassert(cls);
    HDassert(token_str);
    HDassert(token);

    /* Use the class's token deserialization routine on the token if there is a
     *  callback, otherwise just set the token to H5_TOKEN_UNDEF.
     */
    if (cls->token_cls.from_str) {
        if ((cls->token_cls.from_str)(obj, obj_type, token_str, token) < 0)
            HGOTO_ERROR(H5E_VOL, H5E_CANTUNSERIALIZE, FAIL, "can't deserialize object token string")
    } /* end if */
    else
        *token = H5O_TOKEN_UNDEF;

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__token_from_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_token_from_str
 *
 * Purpose:     Deserialize a string into a connector object token
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_token_from_str(const H5VL_object_t *vol_obj, H5I_type_t obj_type, const char *token_str,
                    H5O_token_t *token)
{
    void *obj;                  /* Object to operate on */
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Sanity checks */
    HDassert(vol_obj);
    HDassert(token);
    HDassert(token_str);

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_from_str(obj, obj_type, vol_obj->container->connector->cls, token_str, token) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTUNSERIALIZE, FAIL, "token deserialization failed")

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_token_from_str() */

/*---------------------------------------------------------------------------
 * Function:    H5VLtoken_from_str
 *
 * Purpose:     Deserialize a string into a connector object token
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *---------------------------------------------------------------------------
 */
herr_t
H5VLtoken_from_str(void *obj, H5I_type_t obj_type, hid_t connector_id, const char *token_str,
                   H5O_token_t *token)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xIti*s*k", obj, obj_type, connector_id, token_str, token);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")
    if (NULL == token)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid token pointer")
    if (NULL == token_str)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid token_str pointer")

    /* Call the corresponding internal VOL routine */
    if (H5VL__token_from_str(obj, obj_type, connector->cls, token_str, token) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTUNSERIALIZE, FAIL, "object token from string failed")

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLtoken_from_str() */

/*-------------------------------------------------------------------------
 * Function:    H5VL__optional
 *
 * Purpose:     Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5VL__optional(void *obj, const H5VL_class_t *cls, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_STATIC

    /* Check if the corresponding VOL callback exists */
    if (NULL == cls->optional)
        HGOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "VOL connector has no 'optional' method")

    /* Call the corresponding VOL callback */
    if ((ret_value = (cls->optional)(obj, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute generic 'optional' callback");

done:
    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL__optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VL_optional
 *
 * Purpose:     Optional operation specific to connectors.
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VL_optional(const H5VL_object_t *vol_obj, H5VL_optional_args_t *args, hid_t dxpl_id, void **req)
{
    void *obj;                  /* Object to operate on */
    hbool_t prim_container_ctx_set = FALSE; /* Whether the VOL container context was set up */
    herr_t  ret_value       = SUCCEED; /* Return value */

    FUNC_ENTER_NOAPI(FAIL)

    /* Set container info in API context */
    if (H5VL_set_primary_container_ctx(vol_obj) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTSET, FAIL, "can't set VOL container context")
    prim_container_ctx_set = TRUE;

    /* Get actual object */
    obj = (vol_obj->obj_type == H5VL_OBJ_FILE) ? vol_obj->container->object : vol_obj->object;

    /* Call the corresponding internal VOL routine */
    if ((ret_value = H5VL__optional(obj, vol_obj->container->connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute generic 'optional' callback");

done:
    /* Reset container info in API context */
    if (prim_container_ctx_set && H5VL_reset_primary_container_ctx() < 0)
        HDONE_ERROR(H5E_VOL, H5E_CANTRESET, FAIL, "can't reset VOL container context")

    FUNC_LEAVE_NOAPI(ret_value)
} /* end H5VL_optional() */

/*-------------------------------------------------------------------------
 * Function:    H5VLoptional
 *
 * Purpose:     Performs an optional connector-specific operation
 *
 * Return:      Success:    Non-negative
 *              Failure:    Negative
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5VLoptional(void *obj, hid_t connector_id, H5VL_optional_args_t *args, hid_t dxpl_id, void **req /*out*/)
{
    H5VL_connector_t *  connector;                 /* VOL connector struct */
    herr_t        ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API_WRAPPER(FAIL)
    H5TRACE5("e", "*xi*!ix", obj, connector_id, args, dxpl_id, req);

    /* Check args and get class pointer */
    if (NULL == obj)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid object")
    if (NULL == (connector = H5I_object_verify(connector_id, H5I_VOL)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a VOL connector ID")

    /* Call the corresponding internal VOL routine */
    if ((ret_value = H5VL__optional(obj, connector->cls, args, dxpl_id, req)) < 0)
        HERROR(H5E_VOL, H5E_CANTOPERATE, "unable to execute generic 'optional' callback");

done:
    FUNC_LEAVE_API_WRAPPER(ret_value)
} /* end H5VLoptional() */
