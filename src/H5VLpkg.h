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
 * Purpose:	This file contains declarations which are visible only within
 *          the H5VL package.  Source files outside the H5VL package should
 *          include H5VLprivate.h instead.
 */

#if !(defined H5VL_FRIEND || defined H5VL_MODULE)
#error "Do not include this file outside the H5VL package!"
#endif

#ifndef H5VLpkg_H
#define H5VLpkg_H

/* Get package's private header */
#include "H5VLprivate.h" /* Generic Functions                    */

/* Other private headers needed by this file */

/**************************/
/* Package Private Macros */
/**************************/

/****************************/
/* Package Private Typedefs */
/****************************/

/* Container context info */
typedef struct H5VL_container_ctx_t {
    unsigned rc;           /* Ref. count for the # of times the context was set / reset */
    H5VL_container_t *container;    /* VOL container for "outermost" container */
    void *   obj_wrap_ctx; /* "wrap context" for outermost connector */
} H5VL_container_ctx_t;

/*****************************/
/* Package Private Variables */
/*****************************/

/* Default VOL connector */
extern H5VL_connector_prop_t H5VL_def_conn_g;

/* Mapping of VOL object types to ID types */
extern const H5I_type_t H5VL_obj_to_id_g[];

/* Mapping of ID types VOL object types */
extern const H5VL_obj_type_t H5VL_id_to_obj_g[];

/******************************/
/* Package Private Prototypes */
/******************************/
H5_DLL H5VL_class_t * H5VL__new_cls(const H5VL_class_t *cls, hid_t vipl_id);
H5_DLL herr_t H5VL__free_cls(H5VL_class_t *cls);
H5_DLL herr_t  H5VL__set_def_conn(void);
H5_DLL hid_t   H5VL__register_connector(const H5VL_class_t *cls, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_class(const H5VL_class_t *cls, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_name(const char *name, hid_t vipl_id);
H5_DLL hid_t   H5VL__register_connector_by_value(H5VL_class_value_t value, hid_t vipl_id);
H5_DLL htri_t  H5VL__is_connector_registered_by_name(const char *name);
H5_DLL htri_t  H5VL__is_connector_registered_by_value(H5VL_class_value_t value);
H5_DLL hid_t   H5VL__get_connector_id(hid_t obj_id, hbool_t is_api);
H5_DLL hid_t   H5VL__get_connector_id_by_name(const char *name, hbool_t is_api);
H5_DLL hid_t   H5VL__get_connector_id_by_value(H5VL_class_value_t value, hbool_t is_api);
H5_DLL hid_t   H5VL__peek_connector_id_by_name(const char *name);
H5_DLL hid_t   H5VL__peek_connector_id_by_value(H5VL_class_value_t value);
H5_DLL herr_t  H5VL__connector_str_to_info(const char *str, hid_t connector_id, void **info);
H5_DLL ssize_t H5VL__get_connector_name(hid_t id, char *name /*out*/, size_t size);
H5_DLL void    H5VL__is_default_conn(hid_t fapl_id, hid_t connector_id, hbool_t *is_default);
H5_DLL herr_t H5VL__cmp_connector_cls(int *cmp_value, const H5VL_class_t *cls1, const H5VL_class_t *cls2);
H5_DLL herr_t H5VL__conn_close(H5VL_connector_t *connector, void **request);
H5_DLL herr_t  H5VL__register_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL size_t  H5VL__num_opt_operation(void);
H5_DLL herr_t  H5VL__find_opt_operation(H5VL_subclass_t subcls, const char *op_name, int *op_val);
H5_DLL herr_t  H5VL__unregister_opt_operation(H5VL_subclass_t subcls, const char *op_name);
H5_DLL herr_t  H5VL__term_opt_operation(void);

H5_DLL size_t H5VL__conn_inc_rc(H5VL_connector_t *connector);
H5_DLL ssize_t H5VL__conn_dec_rc(H5VL_connector_t *connector);
H5_DLL herr_t H5VL__cmp_connector_info_cls(const H5VL_class_t *vol_class, int *cmp_value, const void *info1, const void *info2);
H5_DLL H5VL_object_t *H5VL__create_object(H5VL_obj_type_t type, void *object, H5VL_container_t *container);
H5_DLL H5VL_object_t *H5VL__create_object_with_container_ctx(H5VL_obj_type_t type, void *object);
H5_DLL void * H5VL__wrap_object(const H5VL_connector_t *connector, void *wrap_ctx, void *obj, H5I_type_t obj_type);
H5_DLL void * H5VL__unwrap_object(const H5VL_connector_t *connector, void *obj);
H5_DLL H5VL_object_t *H5VL__new_vol_obj(H5VL_obj_type_t type, void *object, H5VL_container_t *container);
H5_DLL herr_t H5VL__update_fapl_vol(hid_t fapl_id, const H5VL_container_t *container);
H5_DLL H5VL_container_t * H5VL__get_container_for_obj(void *obj, H5I_type_t obj_type, H5VL_container_t *orig_container);
H5_DLL herr_t H5VL__set_src_container_ctx(const H5VL_object_t *vol_obj);
H5_DLL herr_t H5VL__set_dst_container_ctx(const H5VL_object_t *vol_obj);
H5_DLL herr_t H5VL__reset_src_container_ctx(void);
H5_DLL herr_t H5VL__reset_dst_container_ctx(void);

/* Internal VOL callback wrappers */
H5_DLL herr_t H5VL__introspect_get_conn_cls(void *obj, const H5VL_class_t *cls, H5VL_get_conn_lvl_t lvl,
                                            const H5VL_class_t **conn_cls);

/* Testing functions */
#ifdef H5VL_TESTING
H5_DLL herr_t H5VL__reparse_def_vol_conn_variable_test(void);
H5_DLL hid_t H5VL__register_using_vol_id(H5VL_obj_type_t obj_type, void *obj, hid_t connector_id);
#endif /* H5VL_TESTING */

#endif /* H5VLpkg_H */
