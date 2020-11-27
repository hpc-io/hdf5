/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * This file contains public declarations for the H5F module.
 */
#ifndef _H5Fpublic_H
#define _H5Fpublic_H

/* Public header files needed by this file */
#include "H5public.h"
#include "H5ACpublic.h"
#include "H5Ipublic.h"

/* When this header is included from a private header, don't make calls to H5check() */
#undef H5CHECK
#ifndef _H5private_H
#define H5CHECK H5check(),
#else /* _H5private_H */
#define H5CHECK
#endif /* _H5private_H */

/* When this header is included from a private HDF5 header, don't make calls to H5open() */
#undef H5OPEN
#ifndef _H5private_H
#define H5OPEN H5open(),
#else /* _H5private_H */
#define H5OPEN
#endif /* _H5private_H */

/*
 * These are the bits that can be passed to the `flags' argument of
 * H5Fcreate() and H5Fopen(). Use the bit-wise OR operator (|) to combine
 * them as needed.  As a side effect, they call H5check_version() to make sure
 * that the application is compiled with a version of the hdf5 header files
 * which are compatible with the library to which the application is linked.
 * We're assuming that these constants are used rather early in the hdf5
 * session.
 */
#define H5F_ACC_RDONLY (H5CHECK H5OPEN 0x0000u) /**< absence of rdwr => rd-only */
#define H5F_ACC_RDWR   (H5CHECK H5OPEN 0x0001u) /**< open for read and write    */
#define H5F_ACC_TRUNC  (H5CHECK H5OPEN 0x0002u) /**< overwrite existing files   */
#define H5F_ACC_EXCL   (H5CHECK H5OPEN 0x0004u) /**< fail if file already exists*/
/* NOTE: 0x0008u was H5F_ACC_DEBUG, now deprecated */
#define H5F_ACC_CREAT (H5CHECK H5OPEN 0x0010u) /**< create non-existing files  */
#define H5F_ACC_SWMR_WRITE                                                                                   \
    (H5CHECK 0x0020u) /**< indicate that this file is open for writing in a                                  \
                           single-writer/multi-reader (SWMR)  scenario.                                      \
                           Note that the process(es) opening the file for reading must                       \
                           open the file with RDONLY access, and use the special "SWMR_READ"                 \
                           access flag. */
#define H5F_ACC_SWMR_READ                                                                                    \
    (H5CHECK 0x0040u) /**< indicate that this file is                                                        \
                       * open for reading in a                                                               \
                       * single-writer/multi-reader (SWMR)                                                   \
                       * scenario.  Note that the                                                            \
                       * process(es) opening the file                                                        \
                       * for SWMR reading must also                                                          \
                       * open the file with the RDONLY                                                       \
                       * flag.  */

/**
 * Default property list identifier
 *
 * \internal Value passed to H5Pset_elink_acc_flags to cause flags to be taken from the parent file.
 * \internal ignore setting on lapl
 */
#define H5F_ACC_DEFAULT (H5CHECK H5OPEN 0xffffu)

/* Flags for H5Fget_obj_count() & H5Fget_obj_ids() calls */
#define H5F_OBJ_FILE     (0x0001u) /**< File objects */
#define H5F_OBJ_DATASET  (0x0002u) /**< Dataset objects */
#define H5F_OBJ_GROUP    (0x0004u) /**< Group objects */
#define H5F_OBJ_DATATYPE (0x0008u) /**< Named datatype objects */
#define H5F_OBJ_ATTR     (0x0010u) /**< Attribute objects */
#define H5F_OBJ_ALL      (H5F_OBJ_FILE | H5F_OBJ_DATASET | H5F_OBJ_GROUP | H5F_OBJ_DATATYPE | H5F_OBJ_ATTR)
#define H5F_OBJ_LOCAL    (0x0020u) /**< Restrict search to objects opened through current file ID
                                        (as opposed to objects opened through any file ID accessing this file) */

#define H5F_FAMILY_DEFAULT (hsize_t)0

#ifdef H5_HAVE_PARALLEL
/*
 * Use this constant string as the MPI_Info key to set H5Fmpio debug flags.
 * To turn on H5Fmpio debug flags, set the MPI_Info value with this key to
 * have the value of a string consisting of the characters that turn on the
 * desired flags.
 */
#define H5F_MPIO_DEBUG_KEY "H5F_mpio_debug_key"
#endif /* H5_HAVE_PARALLEL */

/**
 * The difference between a single file and a set of mounted files
 */
typedef enum H5F_scope_t {
    H5F_SCOPE_LOCAL  = 0, /**< specified file handle only        */
    H5F_SCOPE_GLOBAL = 1  /**< entire virtual file            */
} H5F_scope_t;

/**
 * Unlimited file size for H5Pset_external()
 */
#define H5F_UNLIMITED ((hsize_t)(-1L))

/**
 * How does file close behave?
 */
typedef enum H5F_close_degree_t {
    H5F_CLOSE_DEFAULT = 0, /**< Use the degree pre-defined by underlining VFL */
    H5F_CLOSE_WEAK    = 1, /**< File closes only after all opened objects are closed */
    H5F_CLOSE_SEMI    = 2, /**< If no opened objects, file is close; otherwise, file close fails */
    H5F_CLOSE_STRONG  = 3  /**< If there are opened objects, close them first, then close file */
} H5F_close_degree_t;

/**
 * Current "global" information about file
 */
//! [H5F_info2_t_snip]
typedef struct H5F_info2_t {
    struct {
        unsigned version;        /**< Superblock version # */
        hsize_t  super_size;     /**< Superblock size */
        hsize_t  super_ext_size; /**< Superblock extension size */
    } super;
    struct {
        unsigned version;   /**< Version # of file free space management */
        hsize_t  meta_size; /**< Free space manager metadata size */
        hsize_t  tot_space; /**< Amount of free space in the file */
    } free;
    struct {
        unsigned     version;   /**< Version # of shared object header info */
        hsize_t      hdr_size;  /**< Shared object header message header size */
        H5_ih_info_t msgs_info; /**< Shared object header message index & heap size */
    } sohm;
} H5F_info2_t;
//! [H5F_info2_t_snip]

/**
 * Types of allocation requests. The values larger than #H5FD_MEM_DEFAULT
 * should not change other than adding new types to the end. These numbers
 * might appear in files.
 *
 * \internal Please change the log VFD flavors array if you change this
 *           enumeration.
 */
typedef enum H5F_mem_t {
    H5FD_MEM_NOLIST = -1, /**< Data should not appear in the free list.
                           * Must be negative.
                           */
    H5FD_MEM_DEFAULT = 0, /**< Value not yet set.  Can also be the
                           * datatype set in a larger allocation
                           * that will be suballocated by the library.
                           * Must be zero.
                           */
    H5FD_MEM_SUPER = 1,   /**< Superblock data */
    H5FD_MEM_BTREE = 2,   /**< B-tree data */
    H5FD_MEM_DRAW  = 3,   /**< Raw data (content of datasets, etc.) */
    H5FD_MEM_GHEAP = 4,   /**< Global heap data */
    H5FD_MEM_LHEAP = 5,   /**< Local heap data */
    H5FD_MEM_OHDR  = 6,   /**< Object header data */

    H5FD_MEM_NTYPES /**< Sentinel value - must be last */
} H5F_mem_t;

/**
 * Free space section information
 */
//! [H5F_sect_info_t_snip]
typedef struct H5F_sect_info_t {
    haddr_t addr; /**< Address of free space section */
    hsize_t size; /**< Size of free space section */
} H5F_sect_info_t;
//! [H5F_sect_info_t_snip]

/**
 * Library's format versions
 */
typedef enum H5F_libver_t {
    H5F_LIBVER_ERROR    = -1,
    H5F_LIBVER_EARLIEST = 0, /**< Use the earliest possible format for storing objects */
    H5F_LIBVER_V18      = 1, /**< Use the latest v18 format for storing objects */
    H5F_LIBVER_V110     = 2, /**< Use the latest v110 format for storing objects */
    H5F_LIBVER_V112     = 3, /**< Use the latest v112 format for storing objects */
    H5F_LIBVER_V114     = 4, /**< Use the latest v114 format for storing objects */
    H5F_LIBVER_NBOUNDS
} H5F_libver_t;

#define H5F_LIBVER_LATEST H5F_LIBVER_V114

/**
 * File space handling strategy
 */
typedef enum H5F_fspace_strategy_t {
    H5F_FSPACE_STRATEGY_FSM_AGGR = 0, /**< Mechanisms: free-space managers, aggregators, and virtual file
                                         drivers This is the library default when not set */
    H5F_FSPACE_STRATEGY_PAGE =
        1, /**< Mechanisms: free-space managers with embedded paged aggregation and virtual file drivers */
    H5F_FSPACE_STRATEGY_AGGR = 2, /**< Mechanisms: aggregators and virtual file drivers */
    H5F_FSPACE_STRATEGY_NONE = 3, /**< Mechanisms: virtual file drivers */
    H5F_FSPACE_STRATEGY_NTYPES    /**< Sentinel */
} H5F_fspace_strategy_t;

/**
 * File space handling strategy for release 1.10.0
 *
 * \deprecated 1.10.1
 */
typedef enum H5F_file_space_type_t {
    H5F_FILE_SPACE_DEFAULT     = 0, /**< Default (or current) free space strategy setting */
    H5F_FILE_SPACE_ALL_PERSIST = 1, /**< Persistent free space managers, aggregators, virtual file driver */
    H5F_FILE_SPACE_ALL         = 2, /**< Non-persistent free space managers, aggregators, virtual file driver
                                         This is the library default */
    H5F_FILE_SPACE_AGGR_VFD = 3,    /**< Aggregators, Virtual file driver */
    H5F_FILE_SPACE_VFD      = 4,    /**< Virtual file driver */
    H5F_FILE_SPACE_NTYPES           /**< Sentinel */
} H5F_file_space_type_t;

//! [H5F_retry_info_t_snip]
#define H5F_NUM_METADATA_READ_RETRY_TYPES 21

/**
 * Data structure to report the collection of read retries for metadata items with checksum as
 * used by H5Fget_metadata_read_retry_info()
 */
typedef struct H5F_retry_info_t {
    unsigned  nbins;
    uint32_t *retries[H5F_NUM_METADATA_READ_RETRY_TYPES];
} H5F_retry_info_t;
//! [H5F_retry_info_t_snip]

/**
 * Callback for H5Pset_object_flush_cb() in a file access property list
 */
typedef herr_t (*H5F_flush_cb_t)(hid_t object_id, void *udata);

/*********************/
/* Public Prototypes */
/*********************/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \ingroup H5F
 *
 * \brief Checks if a file can be opened with a given file access property
 *        list
 *
 * \param[in] container_name Name of a file
 * \fapl_id
 *
 * \return \htri_t
 *
 * \details H5Fis_accessible() checks if the file specified by \p
 *          container_name can be opened with the file access property list
 *          \p fapl_id.
 *
 * \note The H5Fis_accessible() function enables files to be checked with a
 *       given file access property list, unlike H5Fis_hdf5(), which only uses
 *       the default file driver when opening a file.
 *
 * \since 1.12.0
 *
 */
H5_DLL htri_t H5Fis_accessible(const char *container_name, hid_t fapl_id);
/**
 * \example H5Fcreate.c
 *          After creating an HDF5 file with H5Fcreate(), we close it with
 *          H5Fclose().
 */
/**
 * \ingroup H5F
 *
 * \brief Creates an HDF5 file
 *
 * \param[in] filename Name of the file to create
 * \param[in] flags    File access flags. Allowable values are:
 *                     - #H5F_ACC_TRUNC: Truncate file, if it already exists,
 *                       erasing all data previously stored in the file
 *                     - #H5F_ACC_EXCL: Fail if file already exists
 * \fcpl_id
 * \fapl_id
 * \return \hid_t{file}
 *
 * \details H5Fcreate() is the primary function for creating HDF5 files; it
 *          creates a new HDF5 file with the specified name and property lists.
 *
 *          The \p filename parameter specifies the name of the new file.
 *
 *          The \p flags parameter specifies whether an existing file is to be
 *          overwritten. It should be set to either #H5F_ACC_TRUNC to overwrite
 *          an existing file or #H5F_ACC_EXCL, instructing the function to fail
 *          if the file already exists.
 *
 *          New files are always created in read-write mode, so the read-write
 *          and read-only flags, #H5F_ACC_RDWR and #H5F_ACC_RDONLY,
 *          respectively, are not relevant in this function. Further note that
 *          a specification of #H5F_ACC_RDONLY will be ignored; the file will
 *          be created in read-write mode, regardless.
 *
 *          More complex behaviors of file creation and access are controlled
 *          through the file creation and file access property lists,
 *          \p fcpl_id and \p fapl_id, respectively. The value of #H5P_DEFAULT
 *          for any property list value indicates that the library should use
 *          the default values for that appropriate property list.
 *
 *          The return value is a file identifier for the newly-created file;
 *          this file identifier should be closed by calling H5Fclose() when
 *          it is no longer needed.
 *
 * \include H5Fcreate.c
 *
 * \note  #H5F_ACC_TRUNC and #H5F_ACC_EXCL are mutually exclusive; use
 *        exactly one.
 *
 * \note An additional flag, #H5F_ACC_DEBUG, prints debug information. This
 *       flag can be combined with one of the above values using the bit-wise
 *       OR operator (\c |), but it is used only by HDF5 library developers;
 *       \Emph{it is neither tested nor supported for use in applications}.
 *
 * \attention \Bold{Special case — File creation in the case of an already-open file:}
 *            If a file being created is already opened, by either a previous
 *            H5Fopen() or H5Fcreate() call, the HDF5 library may or may not
 *            detect that the open file and the new file are the same physical
 *            file. (See H5Fopen() regarding the limitations in detecting the
 *            re-opening of an already-open file.)\n
 *            If the library detects that the file is already opened,
 *            H5Fcreate() will return a failure, regardless of the use of
 *            #H5F_ACC_TRUNC.\n
 *            If the library does not detect that the file is already opened
 *            and #H5F_ACC_TRUNC is not used, H5Fcreate() will return a failure
 *            because the file already exists. Note that this is correct
 *            behavior.\n
 *            But if the library does not detect that the file is already
 *            opened and #H5F_ACC_TRUNC is used, H5Fcreate() will truncate the
 *            existing file and return a valid file identifier. Such a
 *            truncation of a currently-opened file will almost certainly
 *            result in errors. While unlikely, the HDF5 library may not be
 *            able to detect, and thus report, such errors.\n
 *            Applications should avoid calling H5Fcreate() with an already
 *            opened file.
 *
 * \since 1.0.0
 *
 * \see H5Fopen(), H5Fclose()
 *
 */
H5_DLL hid_t H5Fcreate(const char *filename, unsigned flags, hid_t fcpl_id, hid_t fapl_id);
H5_DLL hid_t H5Fcreate_async(const char *app_file, const char *app_func, unsigned app_line,
                             const char *filename, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t es_id);
/**
 * \ingroup H5F
 *
 * \brief Opens an existing HDF5 file
 *
 * \param[in] filename Name of the file to be opened
 * \param[in] flags    File access flags. Allowable values are:
 *                     - #H5F_ACC_RDWR: Allows read and write access to file
 *                     - #H5F_ACC_RDONLY: Allows read-only access to file
 *                     - #H5F_ACC_RDWR \c | #H5F_ACC_SWMR_WRITE: Indicates that
 *                       the file is open for writing in a
 *                       single-writer/multi-writer (SWMR) scenario.
 *                     - #H5F_ACC_RDONLY \c | #H5F_ACC_SWMR_READ:  Indicates
 *                       that the file is open for reading in a
 *                       single-writer/multi-reader (SWMR) scenario.
 *                     - An additional flag, #H5F_ACC_DEBUG, prints debug
 *                       information. This flag can be combined with one of the
 *                       above values using the bit-wise OR operator (\c |), but
 *                       it is used only by HDF5 library developers;
 *                       \Emph{it is neither tested nor supported} for use in
 *                       applications.
 * \fapl_id
 * \return \hid_t{file}
 *
 * \details H5Fopen() is the primary function for accessing existing HDF5 files.
 *          This function opens the named file in the specified access mode and
 *          with the specified access property list.
 *
 *          Note that H5Fopen() does not create a file if it does not already
 *          exist; see H5Fcreate().
 *
 *          The \p filename parameter specifies the name of the file to be
 *          opened.
 *
 *          The \p fapl_id parameter specifies the file access property list.
 *          Use of #H5P_DEFAULT specifies that default I/O access properties
 *          are to be used.
 *
 *          The \p flags parameter specifies whether the file will be opened in
 *          read-write or read-only mode, #H5F_ACC_RDWR or #H5F_ACC_RDONLY,
 *          respectively. More complex behaviors of file access are controlled
 *          through the file-access property list.
 *
 *          The return value is a file identifier for the open file; this file
 *          identifier should be closed by calling H5Fclose() when it is no
 *          longer needed.
 *
 * \note  #H5F_ACC_RDWR and #H5F_ACC_RDONLY are mutually exclusive; use
 *        exactly one.
 *
 * \attention \Bold{Special cases — Multiple opens:} A file can often be opened
 *            with a new H5Fopen() call without closing an already-open
 *            identifier established in a previous H5Fopen() or H5Fcreate()
 *            call. Each such H5Fopen() call will return a unique identifier
 *            and the file can be accessed through any of these identifiers as
 *            long as the identifier remains valid. In such multiply-opened
 *            cases, the open calls must use the same flags argument and the
 *            file access property lists must use the same file close degree
 *            property setting (see the external link discussion below and
 *            H5Pset_fclose_degree()).\n
 *            In some cases, such as files on a local Unix file system, the
 *            HDF5 library can detect that a file is multiply opened and will
 *            maintain coherent access among the file identifiers.\n
 *            But in many other cases, such as parallel file systems or
 *            networked file systems, it is not always possible to detect
 *            multiple opens of the same physical file. In such cases, HDF5
 *            will treat the file identifiers as though they are accessing
 *            different files and will be unable to maintain coherent access.
 *            Errors are likely to result in these cases. While unlikely, the
 *            HDF5 library may not be able to detect, and thus report,
 *            such errors.\n
 *            It is generally recommended that applications avoid multiple
 *            opens of the same file.
 *
 * \attention \Bold{Special restriction on multiple opens of a file first
 *            opened by means of an external link:} When an external link is
 *            followed, the external file is always opened with the weak file
 *            close degree property setting, #H5F_CLOSE_WEAK (see
 *            H5Lcreate_external() and H5Pset_fclose_degree()). If the file is
 *            reopened with H5Fopen while it remains held open from such an
 *            external link call, the file access property list used in the
 *            open call must include the file close degree setting
 *            #H5F_CLOSE_WEAK or the open will fail.
 *
 * \version 1.10.0 The #H5F_ACC_SWMR_WRITE and #H5F_ACC_SWMR_READ flags were added.
 *
 * \see H5Fclose()
 *
 */
H5_DLL hid_t H5Fopen(const char *filename, unsigned flags, hid_t fapl_id);
H5_DLL hid_t H5Fopen_async(const char *app_file, const char *app_func, unsigned app_line,
                           const char *filename, unsigned flags, hid_t access_plist, hid_t es_id);
/**
 * \ingroup H5F
 *
 * \brief Returns a new identifier for a previously-opened HDF5 file
 *
 * \param[in] file_id Identifier of a file for which an additional identifier
 *                    is required
 *
 * \return \hid_t{file}
 *
 * \details H5Freopen() returns a new file identifier for an already-open HDF5
 *          file, as specified by \p file_id. Both identifiers share caches and
 *          other information. The only difference between the identifiers is
 *          that the new identifier is not mounted anywhere and no files are
 *          mounted on it.
 *
 *          The new file identifier should be closed by calling H5Fclose() when
 *          it is no longer needed.
 *
 * \note Note that there is no circumstance under which H5Freopen() can
 *       actually open a closed file; the file must already be open and have an
 *       active \p file_id. E.g., one cannot close a file with H5Fclose() on
 *       \p file_id then use H5Freopen() on \p file_id to reopen it.
 *
 */
H5_DLL hid_t H5Freopen(hid_t file_id);
H5_DLL hid_t H5Freopen_async(const char *app_file, const char *app_func, unsigned app_line, hid_t file_id,
                             hid_t es_id);
/**
 * \ingroup H5F
 *
 * \brief Flushes all buffers associated with a file to storage
 *
 * \loc_id{object_id}
 * \param[in] scope The scope of the flush action
 *
 * \return \herr_t
 *
 * \details H5Fflush() causes all buffers associated with a file to be
 *          immediately flushed to storage without removing the data from the
 *          cache.
 *
 *          \p object_id can be any object associated with the file, including
 *          the file itself, a dataset, a group, an attribute, or a named
 *          datatype.
 *
 *          \p scope specifies whether the scope of the flush action is
 *          global or local. Valid values are as follows:
 *          \scopes
 *
 * \attention HDF5 does not possess full control over buffering. H5Fflush()
 *            flushes the internal HDF5 buffers then asks the operating system
 *            (the OS) to flush the system buffers for the open files. After
 *            that, the OS is responsible for ensuring that the data is
 *            actually flushed to disk.
 *
 */
H5_DLL herr_t H5Fflush(hid_t object_id, H5F_scope_t scope);
H5_DLL herr_t H5Fflush_async(const char *app_file, const char *app_func, unsigned app_line, hid_t object_id,
                             H5F_scope_t scope, hid_t es_id);
/**
 * \example H5Fclose.c
 *          After creating an HDF5 file with H5Fcreate(), we close it with
 *          H5Fclose().
 */
/**
 * \ingroup H5F
 *
 * \brief Terminates access to an HDF5 file
 *
 * \file_id
 * \return \herr_t
 *
 * \details H5Fclose() terminates access to an HDF5 file (specified by
 *          \p file_id) by flushing all data to storage.
 *
 *          If this is the last file identifier open for the file and no other
 *          access identifier is open (e.g., a dataset identifier, group
 *          identifier, or shared datatype identifier), the file will be fully
 *          closed and access will end.
 *
 *          Use H5Fclose() as shown in the following example:
 * \include H5Fclose.c
 *
 * \note \Bold{Delayed close:} Note the following deviation from the
 *       above-described behavior. If H5Fclose() is called for a file but one
 *       or more objects within the file remain open, those objects will remain
 *       accessible until they are individually closed. Thus, if the dataset
 *       \c data_sample is open when H5Fclose() is called for the file
 *       containing it, \c data_sample will remain open and accessible
 *       (including writable) until it is explicitly closed. The file will be
 *       automatically closed once all objects in the file have been closed.\n
 *       Be warned, however, that there are circumstances where it is not
 *       possible to delay closing a file. For example, an MPI-IO file close is
 *       a collective call; all of the processes that opened the file must
 *       close it collectively. The file cannot be closed at some time in the
 *       future by each process in an independent fashion. Another example is
 *       that an application using an AFS token-based file access privilege may
 *       destroy its AFS token after H5Fclose() has returned successfully. This
 *       would make any future access to the file, or any object within it,
 *       illegal.\n
 *       In such situations, applications must close all open objects in a file
 *       before calling H5Fclose. It is generally recommended to do so in all
 *       cases.
 *
 * \see H5Fopen()
 *
 */
H5_DLL herr_t H5Fclose(hid_t file_id);
H5_DLL herr_t H5Fclose_async(const char *app_file, const char *app_func, unsigned app_line, hid_t file_id,
                             hid_t es_id);
/**
 * \ingroup H5F
 *
 * \brief Deletes an HDF5 file
 *
 * \param[in] filename Name of the file to delete
 * \fapl_id
 *
 * \return \herr_t
 *
 * \details H5Fdelete() deletes an HDF5 file \p filename with a file access
 *          property list \p fapl_id. The \p fapl_id should be configured with
 *          the same VOL connector or VFD that was used to open the file.
 *
 *          This API was introduced for use with the Virtual Object Layer
 *          (VOL). With the VOL, HDF5 "files" can map to arbitrary storage
 *          schemes such as object stores and relational database tables. The
 *          data created by these implementations may be inconvenient for a
 *          user to remove without a detailed knowledge of the storage scheme.
 *          H5Fdelete() gives VOL connector authors the ability to add
 *          connector-specific delete code to their connectors so that users
 *          can remove these "files" without detailed knowledge of the storage
 *          scheme.
 *
 *          For a VOL connector, H5Fdelete() deletes the file in a way that
 *          makes sense for the specified VOL connector.
 *
 *          For the native HDF5 connector, HDF5 files will be deleted via the
 *          VFDs, each of which will have to be modified to delete the files it
 *          creates.
 *
 *          For all implementations, H5Fdelete() will first check if the file
 *          is an HDF5 file via H5Fis_accessible(). This is done to ensure that
 *          H5Fdelete() cannot be used as an arbitrary file deletion call.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Fdelete(const char *filename, hid_t fapl_id);
/**
 * \ingroup H5F
 *
 * \brief Returns a file creation property list identifier
 *
 * \file_id
 * \return \hid_t{file creation property list}
 *
 * \details H5Fget_create_plist() returns the file creation property list
 *          identifier identifying the creation properties used to create this
 *          file. This function is useful for duplicating properties when
 *          creating another file.
 *
 *          The creation property list identifier should be released with
 *          H5Pclose().
 *
 */
H5_DLL hid_t H5Fget_create_plist(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Returns a file access property list identifier
 *
 * \file_id
 * \return \hid_t{file access property list}
 *
 * \details H5Fget_access_plist() returns the file access property list
 *          identifier of the specified file.
 *
 */
H5_DLL hid_t H5Fget_access_plist(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Determines the read/write or read-only status of a file
 *
 * \file_id
 * \param[out] intent Access mode flag as originally passed with H5Fopen()
 *
 * \return \herr_t
 *
 * \details Given the identifier of an open file, \p file_id, H5Fget_intent()
 *          retrieves the intended access mode" flag passed with H5Fopen() when
 *          the file was opened.
 *
 *          The value of the flag is returned in \p intent. Valid values are as
 *          follows:
 *          \file_access
 *
 * \note The function will not return an error if intent is NULL; it will
 *       simply do nothing.
 *
 * \version 1.10.0 C function enhanced to work with SWMR functionality.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Fget_intent(hid_t file_id, unsigned *intent);
/**
 * \ingroup H5F
 *
 * \brief Retrieves a file's file number that uniquely identifies an open file
 *
 * \file_id
 * \param[out] fileno A buffer to hold the file number
 *
 * \return \herr_t
 *
 * \details H5Fget_fileno() retrieves a file number for a file specified by the
 *          file identifier \p file_id and the pointer \p fnumber to the file
 *          number.
 *
 * \since 1.12.0
 *
 */
H5_DLL herr_t H5Fget_fileno(hid_t file_id, unsigned long *fileno);
/**
 * \ingroup H5F
 *
 * \brief Returns the number of open object identifiers for an open file
 *
 * \file_id or #H5F_OBJ_ALL for all currently-open HDF5 files
 * \param[in] types Type of object for which identifiers are to be returned
 *
 * \return Returns the number of open objects if successful; otherwise returns
 *         a negative value.
 *
 * \details Given the identifier of an open file, file_id, and the desired
 *          object types, types, H5Fget_obj_count() returns the number of open
 *          object identifiers for the file.
 *
 *          To retrieve a count of open identifiers for open objects in all
 *          HDF5 application files that are currently open, pass the value
 *          #H5F_OBJ_ALL in \p file_id.
 *
 *          The types of objects to be counted are specified in types as
 *          follows:
 *          \obj_types
 *
 *          Multiple object types can be combined with the
 *          logical \c OR operator (|). For example, the expression
 *          \c (#H5F_OBJ_DATASET|#H5F_OBJ_GROUP) would call for datasets and
 *          groups.
 *
 * \version 1.6.8, 1.8.2 C function return type changed to \c ssize_t.
 * \version 1.6.5 #H5F_OBJ_LOCAL has been added as a qualifier on the types
 *                of objects to be counted. #H5F_OBJ_LOCAL restricts the
 *                search to objects opened through current file identifier.
 *
 */
H5_DLL ssize_t H5Fget_obj_count(hid_t file_id, unsigned types);
/**
 *-------------------------------------------------------------------------
 * \ingroup H5F
 *
 * \brief Returns a list of open object identifiers
 *
 * \file_id or #H5F_OBJ_ALL for all currently-open HDF5 files
 * \param[in] types Type of object for which identifiers are to be returned
 * \param[in] max_objs Maximum number of object identifiers to place into
 *                     \p obj_id_list
 * \param[out] obj_id_list Pointer to the returned buffer of open object
 *                         identifiers
 *
 * \return Returns number of objects placed into \p obj_id_list if successful;
 *         otherwise returns a negative value.
 *
 * \details Given the file identifier \p file_id and the type of objects to be
 *          identified, types, H5Fget_obj_ids() returns the list of identifiers
 *          for all open HDF5 objects fitting the specified criteria.
 *
 *          To retrieve identifiers for open objects in all HDF5 application
 *          files that are currently open, pass the value #H5F_OBJ_ALL in
 *          \p file_id.
 *
 *          The types of object identifiers to be retrieved are specified in
 *          types using the codes listed for the same parameter in
 *          H5Fget_obj_count().
 *
 *          To retrieve a count of open objects, use the H5Fget_obj_count()
 *          function. This count can be used to set the \p max_objs parameter.
 *
 * \version 1.8.2 C function return type changed to \c ssize_t and \p
 *                max_objs parameter datatype changed to \c size_t.
 * \version 1.6.8 C function return type changed to \c ssize_t and \p
 *                max_objs parameter datatype changed to \c size_t.
 * \since 1.6.0
 *
 */
H5_DLL ssize_t H5Fget_obj_ids(hid_t file_id, unsigned types, size_t max_objs, hid_t *obj_id_list);
/**
 * \ingroup H5F
 *
 * \brief Returns pointer to the file handle from the virtual file driver
 *
 * \file_id
 * \fapl_id{fapl}
 * \param[out] file_handle Pointer to the file handle being used by the
 *                         low-level virtual file driver
 *
 * \return \herr_t
 *
 * \details Given the file identifier \p file_id and the file access property
 *          list \p fapl_id, H5Fget_vfd_handle() returns a pointer to the file
 *          handle from the low-level file driver currently being used by the
 *          HDF5 library for file I/O.
 *
 * \note For most drivers, the value of \p fapl_id will be #H5P_DEFAULT. For
 *       the \c FAMILY or \c MULTI drivers, this value should be defined
 *       through the property list functions: H5Pset_family_offset() for the
 *       \c FAMILY driver and H5Pset_multi_type() for the \c MULTI driver
 *
 * \since 1.6.0
 *
 */
H5_DLL herr_t H5Fget_vfd_handle(hid_t file_id, hid_t fapl, void **file_handle);
/**
 * \ingroup H5F
 *
 * \brief Mounts an HDF5 file
 *
 * \loc_id{loc}
 * \param[in] name Name of the group onto which the file specified by \p child
 *                 is to be mounted
 * \file_id{child}
 * \param[in] plist File mount property list identifier. Pass #H5P_DEFAULT!
 *
 * \return \herr_t
 *
 * \details H5Fmount() mounts the file specified by \p child onto the object
 *          specified by \p loc and \p name using the mount properties \p plist
 *          If the object specified by \p loc is a dataset, named datatype or
 *          attribute, then the file will be mounted at the location where the
 *          attribute, dataset, or named datatype is attached.
 *
 * \note To date, no file mount properties have been defined in HDF5. The
 *       proper value to pass for \p plist is #H5P_DEFAULT, indicating the
 *       default file mount property list.
 *
 */
H5_DLL herr_t H5Fmount(hid_t loc, const char *name, hid_t child, hid_t plist);
/**
 * \ingroup H5F
 *
 * \brief Unounts an HDF5 file
 *
 * \loc_id{loc}
 * \param[in] name Name of the mount point
 *
 * \return \herr_t
 *
 * \details Given a mount point, H5Funmount() dissociates the mount point's
 *          file from the file mounted there. This function does not close
 *          either file.
 *
 *          The mount point can be either the group in the parent or the root
 *          group of the mounted file (both groups have the same name). If the
 *          mount point was opened before the mount then it is the group in the
 *          parent; if it was opened after the mount then it is the root group
 *          of the child.
 *
 */
H5_DLL herr_t H5Funmount(hid_t loc, const char *name);
/**
 * \ingroup H5F
 *
 * \brief Returns the amount of free space in a file (in bytes)
 *
 * \file_id
 *
 * \return Returns the amount of free space in the file if successful;
 *         otherwise returns a negative value.
 *
 * \details Given the identifier of an open file, \p file_id,
 *          H5Fget_freespace() returns the amount of space that is unused by
 *          any objects in the file.
 *
 *          The interpretation of this number depends on the configured free space
 *          management strategy. For example, if the HDF5 library only tracks free
 *          space in a file from a file open or create until that file is closed,
 *          then this routine will report the free space that has been created
 *          during that interval.
 *
 * \since 1.6.1
 *
 */
H5_DLL hssize_t H5Fget_freespace(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Returns the size of an HDF5 file (in bytes)
 *
 * \file_id
 * \param[out] size Size of the file, in bytes
 *
 * \return \herr_t
 *
 * \details H5Fget_filesize() returns the size of the HDF5 file specified by
 *          \p file_id.
 *
 *          The returned size is that of the entire file, as opposed to only
 *          the HDF5 portion of the file. I.e., size includes the user block,
 *          if any, the HDF5 portion of the file, and any data that may have
 *          been appended beyond the data written through the HDF5 library.
 *
 * \version 1.6.3 Fortran subroutine introduced in this release.
 *
 * \since 1.6.3
 *
 */
H5_DLL herr_t H5Fget_filesize(hid_t file_id, hsize_t *size);
/**
 * \ingroup H5F
 *
 * \brief Retrieves the file's end-of-allocation (EOA)
 *
 * \file_id
 * \param[out] eoa The file's EOA
 *
 * \return \herr_t
 *
 * \details H5Fget_eoa() retrieves the file's EOA and returns it in the
 *          parameter eoa.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Fget_eoa(hid_t file_id, haddr_t *eoa);
/**
 * \ingroup H5F
 *
 * \brief Sets the file' EOA to the maximum of (EOA, EOF) + increment
 *
 * \file_id
 * \param[in] increment The number of bytes to be added to the maximum of
 *                      (EOA, EOF)
 *
 * \return \herr_t
 *
 * \details H5Fincrement_filesize() sets the file's EOA to the maximum of (EOA,
 *          EOF) + \p increment. The EOA is the end-of-file address stored in
 *          the file's superblock while EOF is the file's actual end-of-file.
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Fincrement_filesize(hid_t file_id, hsize_t increment);
/**
 * \ingroup H5F
 *
 * \brief Retrieves a copy of the image of an existing, open file
 *
 * \file_id
 * \param[out] buf_ptr Pointer to the buffer into which the image of the
 *                     HDF5 file is to be copied. If \p buf_ptr is NULL,
 *                     no data will be copied but the function’s return value
 *                     will still indicate the buffer size required (or a
 *                     negative value on error).
 * \param[out] buf_len Size of the supplied buffer
 *
 * \return ssize_t
 *
 * \details H5Fget_file_image() retrieves a copy of the image of an existing,
 *          open file. This routine can be used with files opened using the
 *          SEC2 (or POSIX), STDIO, and Core (or Memory) virtual file drivers
 *          (VFDs).
 *
 *          If the return value of H5Fget_file_image() is a positive value, it
 *          will be the length in bytes of the buffer required to store the
 *          file image. So if the file size is unknown, it can be safely
 *          determined with an initial H5Fget_file_image() call with buf_ptr
 *          set to NULL. The file image can then be retrieved with a second
 *          H5Fget_file_image() call with \p buf_len set to the initial call’s
 *          return value.
 *
 *          While the current file size can also be retrieved with
 *          H5Fget_filesize(), that call may produce a larger value than is
 *          needed. The value returned by H5Fget_filesize() includes the user
 *          block, if it exists, and any unallocated space at the end of the
 *          file. It is safe in all situations to get the file size with
 *          H5Fget_file_image() and it often produces a value that is more
 *          appropriate for the size of a file image buffer.
 *
 * \note \Bold{Recommended Reading:} This function is part of the file image
 *       operations feature set. It is highly recommended to study the guide
 *       "HDF5 File Image Operations" before using this feature set.\n See the
 *       "See Also" section below for links to other elements of HDF5 file
 *       image operations. \todo Fix the references.
 *
 * \attention H5Pget_file_image() will fail, returning a negative value, if the
 *            file is too large for the supplied buffer.
 *
 * \see H5LTopen_file_image(), H5Pset_file_image(), H5Pget_file_image(),
 *      H5Pset_file_image_callbacks(), H5Pget_file_image_callbacks()
 *
 * \version 1.8.13 Fortran subroutine added in this release.
 *
 * \since 1.8.0
 *
 */
H5_DLL ssize_t H5Fget_file_image(hid_t file_id, void *buf_ptr, size_t buf_len);
/**
 * \ingroup MDC
 *
 * \brief Obtains current metadata cache configuration for target file
 *
 * \file_id
 * \param[in,out] config_ptr Pointer to the H5AC_cache_config_t instance in which
 *                        the current metadata cache configuration is to be
 *                        reported. The fields of this structure are discussed
 *                        \ref H5AC-cache-config-t "here".
 * \return \herr_t
 *
 * \details H5Fget_mdc_config() loads the current metadata cache configuration
 *          into the instance of H5AC_cache_config_t pointed to by the \p config_ptr
 *          parameter.
 *
 *          Note that the \c version field of \p config_ptr must be initialized
 *          --this allows the library to support old versions of the H5AC_cache_config_t
 *          structure.
 *
 * \par General configuration section
 *  <table>
 *    <tr>
 *      <td><em>int</em> <code>version</code> </td>
 *      <td>IN: Integer field indicating the the version of the H5AC_cache_config_t in use. This field should
 *        be set to #H5AC__CURR_CACHE_CONFIG_VERSION (defined in H5ACpublic.h).</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>rpt_fcn_enabled</code> </td>
 *      <td><p>OUT: Boolean flag indicating whether the adaptive cache resize report function is enabled. This
 *          field should almost always be set to disabled (<code>0</code>). Since resize algorithm activity is
 *          reported via stdout, it MUST be set to disabled (<code>0</code>) on Windows machines.</p><p>The
 *          report function is not supported code, and can be expected to change between versions of the
 *          library. Use it at your own risk.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>open_trace_file</code> </td>
 *      <td>OUT: Boolean field indicating whether the <code>trace_file_name</code> field should be used to open
 *        a trace file for the cache. This field will always be set to <code>0</code> in this context.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>close_trace_file</code> </td>
 *      <td>OUT: Boolean field indicating whether the current trace file (if any) should be closed. This field
 *        will always be set to <code>0</code> in this context.</td></tr>
 *    <tr>
 *      <td><em>char*</em><code>trace_file_name</code> </td>
 *      <td>OUT: Full path name of the trace file to be opened if the <code>open_trace_file</code> field is set
 *        to <code>1</code>. This field will always be set to the empty string in this context.</td></tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>evictions_enabled</code> </td>
 *      <td>OUT: Boolean flag indicating whether metadata cache entry evictions are
 *        enabled.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>set_initial_size</code> </td>
 *      <td>OUT: Boolean flag indicating whether the cache should be created with a user specified initial
 *        maximum size.<p>If the configuration is loaded from the cache, this flag will always be set
 *          to <code>0</code>.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>initial_size</code> </td>
 *      <td>OUT: Initial maximum size of the cache in bytes, if applicable.<p>If the configuration is loaded
 *        from the cache, this field will contain the cache maximum size as of the time of the
 *          call.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>min_clean_fraction</code> </td>
 *      <td>OUT: Float value specifying the minimum fraction of the cache that must be kept either clean or
 *        empty when possible.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>max_size</code> </td>
 *      <td>OUT: Upper bound (in bytes) on the range of values that the adaptive cache resize code can select
 *        as the maximum cache size.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>min_size</code> </td>
 *      <td>OUT: Lower bound (in bytes) on the range of values that the adaptive cache resize code can select
 *        as the maximum cache size.</td>
 *    </tr>
 *    <tr>
 *      <td><em>long int</em> <code>epoch_length</code> </td>
 *      <td>OUT: Number of cache accesses between runs of the adaptive cache resize
 *        code.</td>
 *    </tr>
 *  </table>
 *
 * \par Increment configuration section
 *  <table>
 *    <tr>
 *      <td><em>enum H5C_cache_incr_mode</em> <code>incr_mode</code> </td>
 *      <td>OUT: Enumerated value indicating the operational mode of the automatic cache size increase code. At
 *        present, only the following values are legal:<p>\c H5C_incr__off: Automatic cache size increase is
 *        disabled.</p><p>\c H5C_incr__threshold: Automatic cache size increase is enabled using the hit rate
 *        threshold algorithm.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>lower_hr_threshold</code> </td>
 *      <td>OUT: Hit rate threshold used in the hit rate threshold cache size increase algorithm.</td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>increment</code> </td>
 *      <td>OUT: The factor by which the current maximum cache size is multiplied to obtain an initial new
 *        maximum cache size if a size increase is triggered in the hit rate threshold cache size increase
 *        algorithm.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_max_increment</code> </td>
 *      <td>OUT: Boolean flag indicating whether an upper limit will be applied to the size of cache size
 *        increases.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>max_increment</code> </td>
 *      <td>OUT: The maximum number of bytes by which the maximum cache size can be increased in a single step
 *        -- if applicable.</td>
 *    </tr>
 *    <tr>
 *      <td><em>enum H5C_cache_flash_incr_mode</em> <code>flash_incr_mode</code> </td>
 *      <td>OUT: Enumerated value indicating the operational mode of the flash cache size increase code. At
 *        present, only the following values are legal:<p>\c H5C_flash_incr__off: Flash cache size increase is
 *        disabled.</p><p>\c H5C_flash_incr__add_space: Flash cache size increase is enabled using the add space
 *        algorithm.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>flash_threshold</code> </td>
 *      <td>OUT: The factor by which the current maximum cache size is multiplied to obtain the minimum size
 *        entry / entry size increase which may trigger a flash cache size
 *        increase.</td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>flash_multiple</code> </td>
 *      <td>OUT: The factor by which the size of the triggering entry / entry size increase is multiplied to
 *        obtain the initial cache size increment. This increment may be reduced to reflect existing free space
 *        in the cache and the <code>max_size</code> field above.</td>
 *    </tr>
 * </table>
 *
 * \par Decrement configuration section
 *  <table>
 *    <tr><td colspan="2"><strong>Decrement configuration
 *          section:</strong></td>
 *    </tr>
 *    <tr>
 *      <td><em>enum H5C_cache_decr_mode</em> <code>decr_mode</code> </td>
 *      <td>OUT: Enumerated value indicating the operational mode of the automatic cache size decrease code. At
 *        present, the following values are legal:<p>H5C_decr__off: Automatic cache size decrease is disabled,
 *        and the remaining decrement fields are ignored.</p><p>H5C_decr__threshold: Automatic cache size
 *        decrease is enabled using the hit rate threshold algorithm.</p><p>H5C_decr__age_out: Automatic cache
 *        size decrease is enabled using the ageout algorithm.</p><p>H5C_decr__age_out_with_threshold:
 *        Automatic cache size decrease is enabled using the ageout with hit rate threshold
 *          algorithm</p></td>
 *    </tr>
 *    <tr><td><em>double</em> <code>upper_hr_threshold</code> </td>
 *      <td>OUT: Upper hit rate threshold. This value is only used if the decr_mode is either
 *        H5C_decr__threshold or H5C_decr__age_out_with_threshold.</td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>decrement</code> </td>
 *      <td>OUT: Factor by which the current max cache size is multiplied to obtain an initial value for the
 *        new cache size when cache size reduction is triggered in the hit rate threshold cache size reduction
 *        algorithm.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_max_decrement</code> </td>
 *      <td>OUT: Boolean flag indicating whether an upper limit should be applied to the size of cache size
 *        decreases.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>max_decrement</code> </td>
 *      <td>OUT: The maximum number of bytes by which cache size can be decreased if any single step, if
 *        applicable.</td>
 *    </tr>
 *    <tr>
 *      <td><em>int</em> <code>epochs_before_eviction</code> </td>
 *      <td>OUT: The minimum number of epochs that an entry must reside unaccessed in cache before being
 *        evicted under either of the ageout cache size reduction algorithms.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_empty_reserve</code> </td>
 *      <td>OUT: Boolean flag indicating whether an empty reserve should be maintained under either of the
 *        ageout cache size reduction algorithms.</td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>empty_reserve</code> </td>
 *      <td>OUT: Empty reserve for use with the ageout cache size reduction algorithms, if applicable.</td>
 *    </tr>
 *  </table>
 *
 * \par Parallel configuration section
 *  <table>
 *    <tr><td><em>int</em> <code>dirty_bytes_threshold</code> </td>
 *      <td>OUT: Threshold number of bytes of dirty metadata generation for triggering synchronizations of the
 *        metadata caches serving the target file in the parallel case.<p>Synchronization occurs whenever the
 *        number of bytes of dirty metadata created since the last synchronization exceeds this limit.</p></td>
 *    </tr>
 *  </table>
 *
 * \since 1.8.0
 *
 * \todo Fix the reference!
 *
 */
H5_DLL herr_t H5Fget_mdc_config(hid_t file_id, H5AC_cache_config_t *config_ptr);
/**
 * \ingroup MDC
 *
 * \brief Attempts to configure metadata cache of target file
 *
 * \file_id
 * \param[in,out] config_ptr Pointer to the H5AC_cache_config_t instance
 *                           containing the desired configuration.
 *                           The fields of this structure are discussed
 *                           \ref H5AC-cache-config-t "here".
 * \return \herr_t
 *
 * \details H5Fset_mdc_config() attempts to configure the file's metadata cache
 *          according configuration supplied in \p config_ptr.
 *
 * \par General configuration fields
 *  <table>
 *    <tr>
 *      <td><em>int</em> <code>version</code></td>
 *      <td>IN: Integer field indicating the the version of the H5AC_cache_config_t in use. This
 *        field should be set to #H5AC__CURR_CACHE_CONFIG_VERSION (defined
 *        in H5ACpublic.h).</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>rpt_fcn_enabled</code></td>
 *      <td>IN: Boolean flag indicating whether the adaptive cache resize report function is enabled. This
 *        field should almost always be set to disabled (<code>0</code>). Since resize algorithm activity is reported
 *        via stdout, it MUST be set to disabled (<code>0</code>) on Windows machines.<p>The report function is not
 *          supported code, and can be expected to change between versions of the library. Use it at your own
 *          risk.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>open_trace_File</code></td>
 *      <td>IN: Boolean field indicating whether the <code>trace_file_name</code> field should be used to open
 *        a trace file for the cache.<p>The trace file is a debuging feature that allows the capture of top level
 *          metadata cache requests for purposes of debugging and/or optimization. This field should normally be set
 *          to <code>0</code>, as trace file collection imposes considerable overhead.</p><p>This field should only be
 *          set to <code>1</code> when the <code>trace_file_name</code> contains the full path of the desired trace
 *          file, and either there is no open trace file on the cache, or the <code>close_trace_file</code> field is
 *          also <code>1</code>.</p><p>The trace file feature is unsupported unless used at the direction of The HDF
 *          Group. It is intended to allow The HDF Group to collect a trace of cache activity in cases of occult
 *          failures and/or poor performance seen in the field, so as to aid in reproduction in the lab. If you use it
 *          absent the direction of The HDF Group, you are on your
 *          own.</p></td>
 *    </tr>
 *    <tr><td><em>hbool_t</em> <code>close_trace_file</code></td>
 *      <td>IN: Boolean field indicating whether the current trace file (if any) should be closed.<p>See the
 *          above comments on the <code>open_trace_file</code> field. This field should be set to <code>0</code> unless
 *          there is an open trace file on the cache that you wish to close.</p><p>The trace file feature is
 *          unsupported unless used at the direction of The HDF Group. It is intended to allow The HDF Group to collect
 *          a trace of cache activity in cases of occult failures and/or poor performance seen in the field, so as to
 *          aid in reproduction in the lab. If you use it absent the direction of The HDF Group, you are on your
 *          own.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>char</em> <code>trace_file_name[]</code></td>
 *      <td>IN: Full path of the trace file to be opened if the <code>open_trace_file</code> field is set
 *        to <code>1</code>.<p>In the parallel case, an ascii representation of the mpi rank of the process will be
 *          appended to the file name to yield a unique trace file name for each process.</p><p>The length of the path
 *          must not exceed #H5AC__MAX_TRACE_FILE_NAME_LEN characters.</p><p>The trace file feature is
 *          unsupported unless used at the direction of The HDF Group. It is intended to allow The HDF Group to collect
 *          a trace of cache activity in cases of occult failures and/or poor performance seen in the field, so as to
 *          aid in reproduction in the lab. If you use it absent the direction of The HDF Group, you are on your
 *          own.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>evictions_enabled</code></td>
 *      <td>IN: A boolean flag indicating whether evictions from the metadata cache are enabled. This flag is
 *        initially set to enabled (<code>1</code>).<p>In rare circumstances, the raw data throughput requirements
 *          may be so high that the user wishes to postpone metadata writes so as to reserve I/O throughput for raw
 *          data. The <code>evictions_enabled</code> field exists to allow this. However, this is an extreme step, and
 *          you have no business doing it unless you have read the User Guide section on metadata caching, and have
 *          considered all other options carefully.</p><p>The <code>evictions_enabled</code> field may not be set to
 *          disabled (<code>0</code>) unless all adaptive cache resizing code is disabled via
 *          the <code>incr_mode</code>, <code>flash_incr_mode</code>, and <code>decr_mode</code> fields.</p><p>When
 *          this flag is set to disabled (<code>0</code>), the metadata cache will not attempt to evict entries to make
 *          space for new entries, and thus will grow without bound.</p><p>Evictions will be re-enabled when this field
 *          is set back to <code>1</code>. This should be done as soon as
 *          possible.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>set_initial_size</code></td>
 *      <td>IN: Boolean flag indicating whether the cache should be forced to the user specified initial
 *        size.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>initial_size</code></td>
 *      <td>IN: If <code>set_initial_size</code> is set to <code>1</code>, then <code>initial_size</code> must
 *        contain the desired initial size in bytes. This value must lie in the closed interval <code>[min_size,
 *          max_size]</code>. (see below)</td>
 *    </tr>
 *    <tr><td><em>double</em> <code>min_clean_fraction</code></td>
 *      <td>IN: This field specifies the minimum fraction of the cache that must be kept either clean or
 *        empty.<p>The value must lie in the interval [0.0, 1.0]. 0.01 is a good place to start in the serial case.
 *          In the parallel case, a larger value is needed --
 *          see <a href="/display/HDF5/Metadata+Caching+in+HDF5">Metadata Caching in HDF5</a> in the collection
 *          "Advanced Topics in HDF5."</p></td>
 *    </tr>
 *    <tr><td><em>size_t</em> <code>max_size</code></td>
 *      <td>IN: Upper bound (in bytes) on the range of values that the adaptive cache resize code can select as
 *        the maximum cache size.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>min_size</code></td>
 *      <td>IN: Lower bound (in bytes) on the range of values that the adaptive cache resize code can select as
 *        the maximum cache size.</td>
 *    </tr>
 *    <tr><td><em>long int</em> <code>epoch_length</code></td>
 *      <td>IN: Number of cache accesses between runs of the adaptive cache resize code. 50,000 is a good
 *        starting number.</td>
 *    </tr>
 *  </table>
 *
 * \par Increment configuration fields
 *  <table>
 *    <tr>
 *      <td><em>enum H5C_cache_incr_mode</em> <code>incr_mode</code></td>
 *      <td>IN: Enumerated value indicating the operational mode of the automatic cache size increase code. At
 *        present, only two values are legal:<p>\c H5C_incr__off: Automatic cache size increase is disabled, and the
 *          remaining increment fields are ignored.</p><p>\c H5C_incr__threshold: Automatic cache size increase is enabled
 *          using the hit rate threshold
 *          algorithm.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>lower_hr_threshold</code></td>
 *      <td>IN: Hit rate threshold used by the hit rate threshold cache size increment algorithm.<p>When the
 *          hit rate over an epoch is below this threshold and the cache is full, the maximum size of the cache is
 *          multiplied by increment (below), and then clipped as necessary to stay within max_size, and possibly
 *          max_increment.</p><p>This field must lie in the interval [0.0, 1.0]. 0.8 or 0.9 is a good starting
 *          point.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>increment</code></td>
 *      <td>IN: Factor by which the hit rate threshold cache size increment algorithm multiplies the current
 *        maximum cache size to obtain a tentative new cache size.<p>The actual cache size increase will be clipped
 *          to satisfy the max_size specified in the general configuration, and possibly max_increment below.</p><p>The
 *          parameter must be greater than or equal to 1.0 -- 2.0 is a reasonable value.</p><p>If you set it to 1.0,
 *          you will effectively disable cache size
 *          increases.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_max_increment</code></td>
 *      <td>IN: Boolean flag indicating whether an upper limit should be applied to the size of cache size
 *        increases.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>max_increment</code></td>
 *      <td>IN: Maximum number of bytes by which cache size can be increased in a single step -- if
 *        applicable.</td>
 *    </tr>
 *    <tr>
 *      <td><em>enum H5C_cache_flash_incr_mode</em> <code>flash_incr_mode</code></td>
 *      <td>IN: Enumerated value indicating the operational mode of the flash cache size increase code. At
 *        present, only the following values are legal:<p>\c H5C_flash_incr__off: Flash cache size increase is
 *          disabled.</p><p>\c H5C_flash_incr__add_space: Flash cache size increase is enabled using the add space
 *          algorithm.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>flash_threshold</code></td>
 *      <td>IN: The factor by which the current maximum cache size is multiplied to obtain the minimum size
 *        entry / entry size increase which may trigger a flash cache size increase.<p>At present, this value must
 *          lie in the range [0.1, 1.0].</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>flash_multiple</code></td>
 *      <td>IN: The factor by which the size of the triggering entry / entry size increase is multiplied to
 *        obtain the initial cache size increment. This increment may be reduced to reflect existing free space in
 *        the cache and the <code>max_size</code> field above.<p>At present, this field must lie in the range [0.1,
 *          10.0].</p></td>
 *    </tr>
 *  </table>
 *
 * \par Decrement configuration fields
 *  <table>
 *    <tr>
 *      <td><em>enum H5C_cache_decr_mode</em> <code>decr_mode</code></td>
 *      <td>IN: Enumerated value indicating the operational mode of the automatic cache size decrease code. At
 *        present, the following values are legal:<p>\c H5C_decr__off: Automatic cache size decrease is
 *          disabled.</p><p>\c H5C_decr__threshold: Automatic cache size decrease is enabled using the hit rate threshold
 *          algorithm.</p><p>\c H5C_decr__age_out: Automatic cache size decrease is enabled using the ageout
 *          algorithm.</p><p>\c H5C_decr__age_out_with_threshold: Automatic cache size decrease is enabled using the
 *          ageout with hit rate threshold
 *          algorithm</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>upper_hr_threshold</code></td>
 *      <td>IN: Hit rate threshold for the hit rate threshold and ageout with hit rate threshold cache size
 *        decrement algorithms.<p>When \c decr_mode is \c H5C_decr__threshold, and the hit rate over a given epoch exceeds
 *          the supplied threshold, the current maximum cache size is multiplied by decrement to obtain a tentative new
 *          (and smaller) maximum cache size.</p><p>When \c decr_mode is \c H5C_decr__age_out_with_threshold, there is no
 *          attempt to find and evict aged out entries unless the hit rate in the previous epoch exceeded the supplied
 *          threshold.</p><p>This field must lie in the interval [0.0, 1.0].</p><p>For \c H5C_incr__threshold, .9995 or
 *          .99995 is a good place to start.</p><p>For \c H5C_decr__age_out_with_threshold, .999 might be more
 *          useful.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>decrement</code></td>
 *      <td>IN: In the hit rate threshold cache size decrease algorithm, this parameter contains the factor by
 *        which the current max cache size is multiplied to produce a tentative new cache size.<p>The actual cache
 *          size decrease will be clipped to satisfy the min_size specified in the general configuration, and possibly
 *          max_decrement below.</p><p>The parameter must be be in the interval [0.0, 1.0].</p><p>If you set it to 1.0,
 *          you will effectively disable cache size decreases. 0.9 is a reasonable starting
 *          point.</p></td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_max_decrement</code></td>
 *      <td>IN: Boolean flag indicating whether an upper limit should be applied to the size of cache size
 *        decreases.</td>
 *    </tr>
 *    <tr>
 *      <td><em>size_t</em> <code>max_decrement</code></td>
 *      <td>IN: Maximum number of bytes by which the maximum cache size can be decreased in any single step --
 *        if applicable.</td>
 *    </tr>
 *    <tr>
 *      <td><em>int</em> <code>epochs_before_eviction</code></td>
 *      <td>IN: In the ageout based cache size reduction algorithms, this field contains the minimum number of
 *        epochs an entry must remain unaccessed in cache before the cache size reduction algorithm tries to evict
 *        it. 3 is a reasonable value.</td>
 *    </tr>
 *    <tr>
 *      <td><em>hbool_t</em> <code>apply_empty_reserve</code></td>
 *      <td>IN: Boolean flag indicating whether the ageout based decrement algorithms will maintain a empty
 *        reserve when decreasing cache size.</td>
 *    </tr>
 *    <tr>
 *      <td><em>double</em> <code>empty_reserve</code></td>
 *      <td>IN: Empty reserve as a fraction of maximum cache size if applicable.<p>When so directed, the ageout
 *          based algorithms will not decrease the maximum cache size unless the empty reserve can be met.</p><p>The
 *          parameter must lie in the interval [0.0, 1.0]. 0.1 or 0.05 is a good place to
 *          start.</p></td>
 *    </tr>
 *  </table>
 *
 * \par Parallel configuration fields
 *  <table>
 *    <tr>
 *      <td><em>int</em> <code>dirty_bytes_threshold</code></td>
 *      <td>IN: Threshold number of bytes of dirty metadata generation for triggering synchronizations of the
 *        metadata caches serving the target file in the parallel case.<p>Synchronization occurs whenever the number
 *          of bytes of dirty metadata created since the last synchronization exceeds this limit.</p><p>This field only
 *          applies to the parallel case. While it is ignored elsewhere, it can still draw a value out of bounds
 *          error.</p><p>It must be consistant across all caches on any given file.</p><p>By default, this field is set
 *          to 256 KB. It shouldn't be more than half the current maximum cache size times the minimum clean
 *          fraction.</p></td>
 *    </tr>
 *  </table>
 *
 * \since 1.8.0
 *
 * \todo Fix the MDC document reference!
 */
H5_DLL herr_t H5Fset_mdc_config(hid_t file_id, H5AC_cache_config_t *config_ptr);
/**
 * \ingroup MDC
 *
 * \brief Obtains target file's metadata cache hit rate
 *
 * \file_id
 * \param[out] hit_rate_ptr Pointer to the double in which the hit rate is returned. Note that
 *                          \p hit_rate_ptr is undefined if the API call fails
 * \return \herr_t
 *
 * \details H5Fget_mdc_hit_rate() queries the metadata cache of the target file to obtain its hit rate
 *          \Code{(cache hits / (cache hits + cache misses))} since the last time hit rate statistics
 *          were reset. If the cache has not been accessed since the last time the hit rate stats were
 *          reset, the hit rate is defined to be 0.0.
 *
 *          The hit rate stats can be reset either manually (via H5Freset_mdc_hit_rate_stats()), or
 *          automatically. If the cache's adaptive resize code is enabled, the hit rate stats will be
 *          reset once per epoch. If they are reset manually as well, the cache may behave oddly.
 *
 *          See the overview of the metadata cache in the special topics section of the user manual for
 *          details on the metadata cache and its adaptive resize algorithms.
 *
 */
H5_DLL herr_t H5Fget_mdc_hit_rate(hid_t file_id, double *hit_rate_ptr);
/**
 * \ingroup MDC
 *
 * \brief Obtains current metadata cache size data for specified file
 *
 * \file_id
 * \param[out] max_size_ptr Pointer to the location in which the current cache maximum size is to be
 *                          returned, or NULL if this datum is not desired
 * \param[out] min_clean_size_ptr Pointer to the location in which the current cache minimum clean
 *                                size is to be returned, or NULL if that datum is not desired
 * \param[out] cur_size_ptr Pointer to the location in which the current cache size is to be returned,
 *                          or NULL if that datum is not desired
 * \param[out] cur_num_entries_ptr Pointer to the location in which the current number of entries in
 *                                 the cache is to be returned, or NULL if that datum is not desired
 * \returns \herr_t
 *
 * \details H5Fget_mdc_size()  queries the metadata cache of the target file for the desired size
 *          information, and returns this information in the locations indicated by the pointer
 *          parameters. If any pointer parameter is NULL, the associated data is not returned.
 *
 *          If the API call fails, the values returned via the pointer parameters are undefined.
 *
 *          If adaptive cache resizing is enabled, the cache maximum size and minimum clean size
 *          may change at the end of each epoch. Current size and current number of entries can
 *          change on each cache access.
 *
 *          Current size can exceed maximum size under certain conditions. See the overview of the
 *          metadata cache in the special topics section of the user manual for a discussion of this.
 *
 */
H5_DLL herr_t H5Fget_mdc_size(hid_t file_id, size_t *max_size_ptr, size_t *min_clean_size_ptr,
                              size_t *cur_size_ptr, int *cur_num_entries_ptr);
/**
 * \ingroup MDC
 *
 * \brief Resets hit rate statistics counters for the target file
 *
 * \file_id
 * \returns \herr_t
 *
 * \details
 * \parblock
 * H5Freset_mdc_hit_rate_stats() resets the hit rate statistics counters in the metadata cache
 * associated with the specified file.
 *
 * If the adaptive cache resizing code is enabled, the hit rate statistics are reset at the beginning
 * of each epoch. This API call allows you to do the same thing from your program.
 *
 * The adaptive cache resizing code may behave oddly if you use this call when adaptive cache resizing
 * is enabled. However, the call should be useful if you choose to control metadata cache size from your
 * program.
 *
 * See "Metadata Caching in HDF5" for details about the metadata cache and the adaptive cache resizing
 * algorithms. If you have not read, understood, and thought about the material covered in that
 * documentation,
 * you should not be using this API call.
 * \endparblock
 *
 * \todo Fix the MDC document reference!
 */
H5_DLL herr_t H5Freset_mdc_hit_rate_stats(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Retrieves name of file to which object belongs
 *
 * \obj_id
 * \param[out] name Buffer for the file name
 * \param[in] size Size, in bytes, of the \p name buffer
 *
 * \return Returns the length of the file name if successful; otherwise returns
 *         a negative value.
 *
 * \details H5Fget_name() retrieves the name of the file to which the object \p
 *          obj_id belongs. The object can be a file, group, dataset,
 *          attribute, or named datatype.
 *
 *          Up to \p size characters of the file name are returned in \p name;
 *          additional characters, if any, are not returned to the user
 *          application.
 *
 *          If the length of the name, which determines the required value of
 *          size, is unknown, a preliminary H5Fget_name() call can be made by
 *          setting \p name to NULL. The return value of this call will be the
 *          size of the file name; that value plus one (1) can then be assigned
 *          to size for a second H5Fget_name() call, which will retrieve the
 *          actual name. (The value passed in with the parameter \p size must
 *          be one greater than size in bytes of the actual name in order to
 *          accommodate the null terminator; if \p size is set to the exact
 *          size of the name, the last byte passed back will contain the null
 *          terminator and the last character will be missing from the name
 *          passed back to the calling application.)
 *
 *          If an error occurs, the buffer pointed to by \p name is unchanged
 *          and the function returns a negative value.
 *
 * \since 1.6.3
 *
 */
H5_DLL ssize_t H5Fget_name(hid_t obj_id, char *name, size_t size);
/**
 * \ingroup H5F
 *
 * \brief Retrieves name of file to which object belongs
 *
 * \fgdta_obj_id
 * \param[out] file_info Buffer for global file information
 *
 * \return \herr_t
 *
 * \details H5Fget_info2() returns global information for the file associated
 *          with the object identifier \p obj_id in the H5F_info2_t \c struct
 *          named \p file_info.
 *
 *          \p obj_id is an identifier for any object in the file of interest.
 *
 *          H5F_info2_t struct is defined in H5Fpublic.h as follows:
 *          \snippet this H5F_info2_t_snip
 *
 *          The \c super sub-struct contains the following information:
 *          \li \c vers is the version number of the superblock.
 *          \li \c  super_size is the size of the superblock.
 *          \li \c super_ext_size is the size of the superblock extension.
 *
 *          The \c free sub-struct contains the following information:
 *          \li vers is the version number of the free-space manager.
 *          \li \c hdr_size is the size of the free-space manager header.
 *          \li \c tot_space is the total amount of free space in the file.
 *
 *          The \c sohm sub-struct contains shared object header message
 *          information as follows:
 *          \li \c vers is the version number of the shared object header information.
 *          \li \c hdr_size is the size of the shared object header message.
 *          \li \c msgs_info is an H5_ih_info_t struct defined in H5public.h as
 *              follows: \snippet H5public.h H5_ih_info_t_snip
 *          \li \p index_size is the summed size of all the shared object
 *              header indexes. Each index might be either a B-tree or
 *              a list.
 *          \li \p heap_size is the size of the heap.
 *
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Fget_info2(hid_t obj_id, H5F_info2_t *file_info);
/**
 * \ingroup SWMR
 *
 * \brief Retrieves the collection of read retries for metadata entries with checksum
 *
 * \file_id
 * \param[out] info Struct containing the collection of read retries for metadata
 *                  entries with checksum
 * \return \herr_t\n
 *
 * \details \Bold{Failure Modes:}
 *       \li When the input identifier is not a file identifier.
 *       \li When the pointer to the output structure is NULL.
 *       \li When the memory allocation for \p retries failed.
 *
 * \details H5Fget_metadata_read_retry_info() retrieves information regarding the number
 *          of read retries for metadata entries with checksum for the file \p file_id.
 *          This information is reported in the H5F_retry_info_t struct defined in
 *          H5Fpublic.h as follows:
 *          \snippet this H5F_retry_info_t_snip
 *          \c nbins is the number of bins for each \c retries[i] of metadata entry \c i.
 *          It is calculated based on the current number of read attempts used in the
 *          library and logarithmic base 10.
 *
 *          If read retries are incurred for a metadata entry \c i, the library will
 *          allocate memory for \Code{retries[i] (nbins * sizeof(uint32_t)} and store
 *          the collection of retries there. If there are no retries for a metadata entry
 *          \c i, \Code{retries[i]} will be NULL. After a call to this routine, users should
 *          free each \Code{retries[i]} that is non-NULL, otherwise resource leak will occur.
 *
 *          For the library default read attempts of 100 for SWMR access, nbins will be 2
 *          as depicted below:
 *          \li \Code{retries[i][0]} is the number of 1 to 9 read retries.
 *          \li \Code{retries[i][1]} is the number of 10 to 99 read retries.
 *          For the library default read attempts of 1 for non-SWMR access, \c nbins will
 *          be 0 and each \Code{retries[i]} will be NULL.
 *
 *          The following table lists the 21 metadata entries of \Code{retries[]}:
 *          <table>
 *          <tr>
 *          <th>Index for \Code{retries[]}</th>
 *          <th>Metadata entries<sup>*</sup></th>
 *          </tr>
 *          <tr><td>0</td><td>Object header (version 2)</td></tr>
 *          <tr><td>1</td><td>Object header chunk (version 2)</td></tr>
 *          <tr><td>2</td><td>B-tree header (version 2)</td></tr>
 *          <tr><td>3</td><td>B-tree internal node (version 2)</td></tr>
 *          <tr><td>4</td><td>B-tree leaf node (version 2)</td></tr>
 *          <tr><td>5</td><td>Fractal heap header</td></tr>
 *          <tr><td>6</td><td>Fractal heap direct block (optional checksum)</td></tr>
 *          <tr><td>7</td><td>Fractal heap indirect block</td></tr>
 *          <tr><td>8</td><td>Free-space header</td></tr>
 *          <tr><td>9</td><td>Free-space sections</td></tr>
 *          <tr><td>10</td><td>Shared object header message table</td></tr>
 *          <tr><td>11</td><td>Shared message record list</td></tr>
 *          <tr><td>12</td><td>Extensive array header</td></tr>
 *          <tr><td>13</td><td>Extensive array index block</td></tr>
 *          <tr><td>14</td><td>Extensive array super block</td></tr>
 *          <tr><td>15</td><td>Extensive array data block</td></tr>
 *          <tr><td>16</td><td>Extensive array data block page</td></tr>
 *          <tr><td>17</td><td>Fixed array super block</td></tr>
 *          <tr><td>18</td><td>Fixed array data block</td></tr>
 *          <tr><td>19</td><td>Fixed array data block page</td></tr>
 *          <tr><td>20</td><td>File's superblock (version 2)</td></tr>
 *          <tr><td colspan=2><sup>*</sup> All entries are of version 0 (zero) unless indicated otherwise.</td></tr>
 *          </table>
 *
 * \note   On a system that is not atomic, the library might possibly read inconsistent
 *         metadata with checksum when performing single-writer/multiple-reader (SWMR)
 *         operations for an HDF5 file. Upon encountering such situations, the library
 *         will try reading the metadata again for a set number of times to attempt to
 *         obtain consistent data. The maximum number of read attempts used by the library
 *         will be either the value set via H5Pset_metadata_read_attempts() or the library
 *         default value when a value is not set.\n
 *         When the current number of metadata read attempts used in the library is unable
 *         to remedy the reading of inconsistent metadata on a system, the user can assess
 *         the information obtained via this routine to derive a different maximum value.
 *         The information can also be helpful for debugging purposes to identify potential
 *         issues with metadata flush dependencies and SWMR implementation in general.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Fget_metadata_read_retry_info(hid_t file_id, H5F_retry_info_t *info);
/**
 * \ingroup SWMR
 *
 * \brief Retrieves free-space section information for a file
 *
 * \file_id
 *
 * \return \herr_t
 *
 * \details H5Fstart_swmr_write() will activate SWMR writing mode for a file
 *          associated with \p file_id. This routine will prepare and ensure
 *          the file is safe for SWMR writing as follows:
 *          \li Check that the file is opened with write access (#H5F_ACC_RDWR).
 *          \li Check that the file is opened with the latest library format to
 *              ensure data structures with check-summed metadata are used.
 *          \li Check that the file is not already marked in SWMR writing mode.
 *          \li Enable reading retries for check-summed metadata to remedy
 *              possible checksum failures from reading inconsistent metadata
 *              on a system that is not atomic.
 *          \li Turn off usage of the library's accumulator to avoid possible
 *              ordering problem on a system that is not atomic.
 *          \li Perform a flush of the file’s data buffers and metadata to set
 *              a consistent state for starting SWMR write operations.
 *
 *          Library objects are groups, datasets, and committed datatypes. For
 *          the current implementation, groups and datasets can remain open when
 *          activating SWMR writing mode, but not committed datatypes. Attributes
 *          attached to objects cannot remain open either.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Fstart_swmr_write(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Retrieves free-space section information for a file
 *
 * \file_id
 * \param[in] type The file memory allocation type
 * \param[in] nsects The number of free-space sections
 * \param[out] sect_info Array of instances of H5F_sect_info_t in which
 *                       the free-space section information is to be returned
 *
 * \return Returns the number of free-space sections for the specified
 *         free-space manager in the file; otherwise returns a negative value.
 *
 * \details H5Fget_free_sections() retrieves free-space section information for
 *          the free-space manager with type that is associated with file
 *          \p file_id. If type is #H5FD_MEM_DEFAULT, this routine retrieves
 *          free-space section information for all the free-space managers in
 *          the file.
 *
 *          Valid values for \p type are the following:
 *          \mem_types
 *
 *          H5F_sect_info_t is defined as follows (in H5Fpublic.h):
 *          \snippet this H5F_sect_info_t_snip
 *
 *          This routine retrieves free-space section information for \p nsects
 *          sections or at most the maximum number of sections in the specified
 *          free-space manager. If the number of sections is not known, a
 *          preliminary H5Fget_free_sections() call can be made by setting \p
 *          sect_info to NULL and the total number of free-space sections for
 *          the specified free-space manager will be returned. Users can then
 *          allocate space for entries in \p sect_info, each of which is
 *          defined as an H5F_sect_info_t \c struct.
 *
 * \attention \Bold{Failure Modes:} This routine will fail when the following
 *            is true:
 *            \li The library fails to retrieve the file creation property list
 *                associated with \p file_id.
 *            \li If the parameter \p sect_info is non-null, but the parameter
 *                \p nsects is equal to 0.
 *            \li The library fails to retrieve free-space section information
 *                for the file.
 *
 * \since 1.10.0
 *
 */
H5_DLL ssize_t H5Fget_free_sections(hid_t file_id, H5F_mem_t type, size_t nsects,
                                    H5F_sect_info_t *sect_info /*out*/);
/**
 * \ingroup H5F
 *
 * \brief Clears the external link open file cache
 *
 * \file_id
 * \return \herr_t
 *
 * \details H5Fclear_elink_file_cache() evicts all the cached child files in
 *          the specified file’s external file cache, causing them to be closed
 *          if there is nothing else holding them open.
 *
 *          H5Fclear_elink_file_cache() does not close the cache itself;
 *          subsequent external link traversals from the parent file will again
 *          cache the target file. See H5Pset_elink_file_cache_size() for
 *          information on closing the file cache.
 *
 * \see H5Pset_elink_file_cache_size(), H5Pget_elink_file_cache_size()
 *
 * \since 1.8.7
 *
 */
H5_DLL herr_t H5Fclear_elink_file_cache(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Enables the switch of version bounds setting for a file
 *
 * \file_id
 * \param[in] low The earliest version of the library that will be used for
 *                writing objects
 * \param[in] high The latest version of the library that will be used for
 *                 writing objects
 *
 * \return \herr_t
 *
 * \details H5Fset_libver_bounds() enables the switch of version bounds setting
 *          for an open file associated with \p file_id.
 *
 *          For the parameters \p low and \p high, see the description for
 *          H5Pset_libver_bounds().
 *
 * \since 1.10.2
 *
 */
H5_DLL herr_t H5Fset_libver_bounds(hid_t file_id, H5F_libver_t low, H5F_libver_t high);
/**
 * \ingroup MDC
 *
 * \brief Starts logging metadata cache events if logging was previously enabled
 *
 * \file_id
 *
 * \return \herr_t
 *
 * \details The metadata cache is a central part of the HDF5 library through
 *          which all \Emph{file metadata} reads and writes take place. File
 *          metadata is normally invisible to the user and is used by the
 *          library for purposes such as locating and indexing data. File
 *          metadata should not be confused with user metadata, which consists
 *          of attributes created by users and attached to HDF5 objects such
 *          as datasets via H5A API calls.
 *
 *          Due to the complexity of the cache, a trace/logging feature has been
 *          created that can be used by HDF5 developers for debugging and performance
 *          analysis. The functions that control this functionality will normally be
 *          of use to a very limited number of developers outside of The HDF Group.
 *          The functions have been documented to help users create logs that can
 *          be sent with bug reports.
 *
 *          Control of the log functionality is straightforward. Logging is enabled
 *          via the H5Pset_mdc_log_options() function, which will modify the file
 *          access property list used to open or create a file. This function has
 *          a flag that determines whether logging begins at file open or starts
 *          in a paused state. Log messages can then be controlled via the
 *          H5Fstart_mdc_logging() and H5Fstop_mdc_logging() functions.
 *          H5Pget_mdc_log_options() can be used to examine a file access property
 *          list, and H5Fget_mdc_logging_status() will return the current state of
 *          the logging flags.
 *
 *          The log format is described in the \Emph{Metadata Cache Logging} document.
 *
 * \note Logging can only be started or stopped if metadata cache logging was enabled
 *       via H5Pset_mdc_log_options().\n
 *       When enabled and currently logging, the overhead of the logging feature will
 *       almost certainly be significant.\n
 *       The log file is opened when the HDF5 file is opened or created and not when
 *       this function is called for the first time.\n
 *       This function opens the log file and starts logging metadata cache operations
 *       for a particular file. Calling this function when logging has already been
 *       enabled will be considered an error.
 *
 * \since 1.10.0
 *
 * \todo Fix the document reference!
 *
 */
H5_DLL herr_t H5Fstart_mdc_logging(hid_t file_id);
/**
 * \ingroup MDC
 *
 * \brief Stops logging metadata cache events if logging was previously enabled and is currently ongoing
 *
 * \file_id
 *
 * \return \herr_t
 *
 * \details The metadata cache is a central part of the HDF5 library through
 *          which all \Emph{file metadata} reads and writes take place. File
 *          metadata is normally invisible to the user and is used by the
 *          library for purposes such as locating and indexing data. File
 *          metadata should not be confused with user metadata, which consists
 *          of attributes created by users and attached to HDF5 objects such
 *          as datasets via H5A API calls.
 *
 *          Due to the complexity of the cache, a trace/logging feature has been
 *          created that can be used by HDF5 developers for debugging and performance
 *          analysis. The functions that control this functionality will normally be
 *          of use to a very limited number of developers outside of The HDF Group.
 *          The functions have been documented to help users create logs that can
 *          be sent with bug reports.
 *
 *          Control of the log functionality is straightforward. Logging is enabled
 *          via the H5Pset_mdc_log_options() function, which will modify the file
 *          access property list used to open or create a file. This function has
 *          a flag that determines whether logging begins at file open or starts
 *          in a paused state. Log messages can then be controlled via the
 *          H5Fstart_mdc_logging() and H5Fstop_mdc_logging() functions.
 *          H5Pget_mdc_log_options() can be used to examine a file access property
 *          list, and H5Fget_mdc_logging_status() will return the current state of
 *          the logging flags.
 *
 *          The log format is described in the \Emph{Metadata Cache Logging} document.
 *
 * \note Logging can only be started or stopped if metadata cache logging was enabled
 *       via H5Pset_mdc_log_options().\n
 *       This function only suspends the logging operations. The log file will remain
 *       open and will not be closed until the HDF5 file is closed.
 *
 * \since 1.10.0
 *
 */
H5_DLL herr_t H5Fstop_mdc_logging(hid_t file_id);
/**
 * \ingroup MDC
 *
 * \brief Gets the current metadata cache logging status
 *
 * \file_id
 * \param[out] is_enabled Whether logging is enabled
 * \param[out] is_currently_logging Whether events are currently being logged
 * \return \herr_t
 *
 * \details The metadata cache is a central part of the HDF5 library through
 *          which all \Emph{file metadata} reads and writes take place. File
 *          metadata is normally invisible to the user and is used by the
 *          library for purposes such as locating and indexing data. File
 *          metadata should not be confused with user metadata, which consists
 *          of attributes created by users and attached to HDF5 objects such
 *          as datasets via H5A API calls.
 *
 *          Due to the complexity of the cache, a trace/logging feature has been
 *          created that can be used by HDF5 developers for debugging and performance
 *          analysis. The functions that control this functionality will normally be
 *          of use to a very limited number of developers outside of The HDF Group.
 *          The functions have been documented to help users create logs that can
 *          be sent with bug reports.
 *
 *          Control of the log functionality is straightforward. Logging is enabled
 *          via the H5Pset_mdc_log_options() function, which will modify the file
 *          access property list used to open or create a file. This function has
 *          a flag that determines whether logging begins at file open or starts
 *          in a paused state. Log messages can then be controlled via the
 *          H5Fstart_mdc_logging() and H5Fstop_mdc_logging() functions.
 *          H5Pget_mdc_log_options() can be used to examine a file access property
 *          list, and H5Fget_mdc_logging_status() will return the current state of
 *          the logging flags.
 *
 *          The log format is described in the \Emph{Metadata Cache Logging} document.
 *
 * \note Unlike H5Fstart_mdc_logging() and H5Fstop_mdc_logging(), this function can
 *       be called on any open file identifier.
 *
 * \since 1.10.0
 */
H5_DLL herr_t H5Fget_mdc_logging_status(hid_t file_id, hbool_t *is_enabled,
                                        hbool_t *is_currently_logging);
/**
 * \ingroup SWMR
 *
 * \todo Finish this!
 */
H5_DLL herr_t H5Fformat_convert(hid_t fid);
/**
 * \ingroup H5F
 *
 * \brief Resets the page buffer statistics
 *
 * \file_id
 *
 * \return \herr_t
 *
 * \details H5Freset_page_buffering_stats() resets the page buffer statistics
 *          for a specified file identifier \p file_id.
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5Freset_page_buffering_stats(hid_t file_id);
/**
 * \ingroup H5F
 *
 * \brief Retrieves statistics about page access when it is enabled
 *
 * \file_id
 * \param[out] accesses Two integer array for the number of metadata and raw
 *                      data accesses to the page buffer
 * \param[out] hits Two integer array for the number of metadata and raw data
 *                  hits in the page buffer
 * \param[out] misses Two integer array for the number of metadata and raw data
 *                    misses in the page buffer
 * \param[out] evictions Two integer array for the number of metadata and raw
 *                       data evictions from the page buffer
 * \param[out] bypasses Two integer array for the number of metadata and raw
 *                      data accesses that bypass the page buffer
 *
 * \return \herr_t
 *
 * \details H5Fget_page_buffering_stats() retrieves page buffering statistics
 *          such as the number of metadata and raw data accesses (\p accesses),
 *          hits (\p hits), misses (\p misses), evictions (\p evictions), and
 *          accesses that bypass the page buffer (\p bypasses).
 *
 * \since 1.10.1
 *
 */
H5_DLL herr_t H5Fget_page_buffering_stats(hid_t file_id, unsigned accesses[2], unsigned hits[2],
                                          unsigned misses[2], unsigned evictions[2], unsigned bypasses[2]);
/**
 * \ingroup MDC
 *
 * \brief Obtains information about a cache image if it exists
 *
 * \file_id
 * \param[out] image_addr Offset of the cache image if it exists, or #HADDR_UNDEF if it does not
 * \param[out] image_size Length of the cache image if it exists, or 0 if it does not
 * \returns \herr_t
 *
 * \details
 * \parblock
 * H5Fget_mdc_image_info() returns information about a cache image if it exists.
 *
 * When an HDF5 file is opened in Read/Write mode, any metadata cache image will
 * be read and deleted from the file on the first metadata cache access (or, if
 * persistent free space managers are enabled, on the first file space
 * allocation / deallocation, or read of free space manager status, whichever
 * comes first).
 *
 * Thus, if the file is opened Read/Write, H5Fget_mdc_image_info() should be called
 * immediately after file open and before any other operation. If H5Fget_mdc_image_info()
 * is called after the cache image is loaded, it will correctly report that no cache image
 * exists, as the image will have already been read and deleted from the file. In the Read Only
 * case, the function may be called at any time, as any cache image will not be deleted
 * from the file.
 * \endparblock
 *
 * \since 1.10.1
 */
H5_DLL herr_t H5Fget_mdc_image_info(hid_t file_id, haddr_t *image_addr, hsize_t *image_size);
/**
 * \ingroup H5F
 *
 * \brief Retrieves the setting for whether or not a file will create minimized
 *        dataset object headers
 *
 * \file_id
 * \param[out] minimize Flag indicating whether the library will or will not
 *                      create minimized dataset object headers
 *
 * \return \herr_t
 *
 * \details H5Fget_dset_no_attrs_hint() retrieves the no dataset attributes
 *          hint setting for the file specified by the file identifier \p
 *          file_id. This setting is used to inform the library to create
 *          minimized dataset object headers when \c TRUE.
 *
 *          The setting's value is returned in the boolean pointer minimize.
 *
 * \since 1.10.5
 *
 */
H5_DLL herr_t H5Fget_dset_no_attrs_hint(hid_t file_id, hbool_t *minimize);
/**
 * \ingroup H5F
 *
 * \brief Sets the flag to create minimized dataset object headers
 *
 * \file_id
 * \param[in] minimize Flag indicating whether the library will or will not
 *                     create minimized dataset object headers
 *
 * \return \herr_t
 *
 * \details H5Fset_dset_no_attrs_hint() sets the no dataset attributes hint
 *          setting for the file specified by the file identifier \p file_id.
 *          If the boolean flag \p minimize is set to \c TRUE, then the library
 *          will create minimized dataset object headers in the file.
 *          \Bold{All} files that refer to the same file-on-disk will be
 *          affected by the most recent setting, regardless of the file
 *          identifier/handle (e.g., as returned by H5Fopen()). By setting the
 *          \p minimize flag to \c TRUE, the library expects that no attributes
 *          will be added to the dataset - attributes can be added, but they
 *          are appended with a continuation message, which can reduce
 *          performance.
 *
 * \attention This setting interacts with H5Pset_dset_no_attrs_hint(): if
 *            either is set to \c TRUE, then the created dataset's object header
 *            will be minimized.
 *
 * \since 1.10.5
 *
 */
H5_DLL herr_t H5Fset_dset_no_attrs_hint(hid_t file_id, hbool_t minimize);

#ifdef H5_HAVE_PARALLEL
/**
 * \ingroup PH5F
 *
 * \brief Sets the MPI atomicity mode
 *
 * \file_id
 * \param[in] flag Logical flag for atomicity setting. Valid values are:
 *                 \li \c 1 -- Sets MPI file access to atomic mode.
 *                 \li \c 0 -- Sets MPI file access to nonatomic mode.
 * \returns \herr_t
 *
 * \par Motivation
 * H5Fset_mpi_atomicity() is applicable only in parallel environments using MPI I/O.
 * The function is one of the tools used to ensure sequential consistency. This means
 * that a set of operations will behave as though they were performed in a serial
 * order consistent with the program order.
 *
 * \details
 * \parblock
 * H5Fset_mpi_atomicity() sets MPI consistency semantics for data access to the file,
 * \p file_id.
 *
 * If \p flag is set to \c 1, all file access operations will appear atomic, guaranteeing
 * sequential consistency. If \p flag is set to \c 0, enforcement of atomic file access
 * will be turned off.
 *
 * H5Fset_mpi_atomicity() is a collective function and all participating processes must
 * pass the same values for \p file_id and \p flag.
 *
 * This function is available only when the HDF5 library is configured with parallel support
 * (\Code{--enable-parallel}). It is useful only when used with the #H5FD_MPIO driver
 * (see H5Pset_fapl_mpio()).
 * \endparblock
 *
 * \attention
 * \parblock
 * H5Fset_mpi_atomicity() calls \Code{MPI_File_set_atomicity} underneath and is not supported
 * if the execution platform does not support \Code{MPI_File_set_atomicity}. When it is
 * supported and used, the performance of data access operations may drop significantly.
 *
 * In certain scenarios, even when \Code{MPI_File_set_atomicity} is supported, setting
 * atomicity with H5Fset_mpi_atomicity() and \p flag set to 1 does not always yield
 * strictly atomic updates. For example, some H5Dwrite() calls translate to multiple
 * \Code{MPI_File_write_at} calls. This happens in all cases where the high-level file
 * access routine translates to multiple lower level file access routines.
 * The following scenarios will raise this issue:
 * \li Non-contiguous file access using independent I/O
 * \li Partial collective I/O using chunked access
 * \li Collective I/O using filters or when data conversion is required
 *
 * This issue arises because MPI atomicity is a matter of MPI file access operations rather
 * than HDF5 access operations. But the user is normally seeking atomicity at the HDF5 level.
 * To accomplish this, the application must set a barrier after a write, H5Dwrite(), but before
 * the next read, H5Dread(), in addition to calling H5Fset_mpi_atomicity().The barrier will
 * guarantee that all underlying write operations execute atomically before the read
 * operations starts. This ensures additional ordering semantics and will normally produce
 * the desired behavior.
 * \endparblock
 *
 * \see Enabling a Strict Consistency Semantics Model in Parallel HDF5
 *
 * \since 1.8.9
 *
 * \todo Fix the reference!
 */
H5_DLL herr_t H5Fset_mpi_atomicity(hid_t file_id, hbool_t flag);
/**
 * \ingroup PH5F
 *
 * \brief Retrieves the atomicity mode in use
 *
 * \file_id
 * \param[out] flag Logical flag for atomicity setting. Valid values are:
 *                  \li 1 -- MPI file access is set to atomic mode.
 *                  \li 0 -- MPI file access is set to nonatomic mode.
 * \returns \herr_t
 *
 * \details H5Fget_mpi_atomicity() retrieves the current consistency semantics mode for
 *          data access for the file \p file_id.
 *
 *          Upon successful return, \p flag will be set to \c 1 if file access is set
 *          to atomic mode and \c 0 if file access is set to nonatomic mode.
 *
 * \see Enabling a Strict Consistency Semantics Model in Parallel HDF5
 *
 * \since 1.8.9
 *
 * \todo Fix the reference!
 */
H5_DLL herr_t H5Fget_mpi_atomicity(hid_t file_id, hbool_t *flag);
#endif /* H5_HAVE_PARALLEL */

/* API Wrappers for async routines */
/* (Must be defined _after_ the function prototype) */
/* (And must only defined when included in application code, not the library) */
#ifndef H5F_MODULE
#define H5Fcreate_async(...) H5Fcreate_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Fopen_async(...)   H5Fopen_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Freopen_async(...) H5Freopen_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Fflush_async(...)  H5Fflush_async(__FILE__, __func__, __LINE__, __VA_ARGS__)
#define H5Fclose_async(...)  H5Fclose_async(__FILE__, __func__, __LINE__, __VA_ARGS__)

/* Define "wrapper" versions of function calls, to allow compile-time values to
 *      be passed in by language wrapper or library layer on top of HDF5.
 */
#define H5Fcreate_async_wrap    H5_NO_EXPAND(H5Fcreate_async)
#define H5Fopen_async_wrap      H5_NO_EXPAND(H5Fopen_async)
#define H5Freopen_async_wrap    H5_NO_EXPAND(H5Freopen_async)
#define H5Fflush_async_wrap     H5_NO_EXPAND(H5Fflush_async)
#define H5Fclose_async_wrap     H5_NO_EXPAND(H5Fclose_async)
#endif /* H5F_MODULE */

/* Symbols defined for compatibility with previous versions of the HDF5 API.
 *
 * Use of these symbols is deprecated.
 */
#ifndef H5_NO_DEPRECATED_SYMBOLS

/* Macros */
#define H5F_ACC_DEBUG (H5CHECK H5OPEN 0x0000u) /*print debug info (deprecated)*/

/* Typedefs */

/**
 * Current "global" information about file
 */
//! [H5F_info1_t_snip]
typedef struct H5F_info1_t {
    hsize_t super_ext_size; /**< Superblock extension size */
    struct {
        hsize_t      hdr_size;  /**< Shared object header message header size */
        H5_ih_info_t msgs_info; /**< Shared object header message index & heap size */
    } sohm;
} H5F_info1_t;
//! [H5F_info1_t_snip]

/* Function prototypes */
/**
 * \ingroup H5F
 *
 * \brief Retrieves name of file to which object belongs
 *
 * \fgdta_obj_id
 * \param[out] file_info Buffer for global file information
 *
 * \return \herr_t
 *
 * \deprecated This function has been renamed from H5Fget_info() and is
 *             deprecated in favor of the macro #H5Fget_info or the function
 *             H5Fget_info2().
 *
 * \details H5Fget_info1() returns global information for the file associated
 *          with the object identifier \p obj_id in the H5F_info1_t \c struct
 *          named \p file_info.
 *
 *          \p obj_id is an identifier for any object in the file of interest.
 *
 *          H5F_info1_t struct is defined in H5Fpublic.h as follows:
 *          \snippet this H5F_info1_t_snip
 *
 *          \c super_ext_size is the size of the superblock extension.
 *
 *          The \c sohm sub-struct contains shared object header message
 *          information as follows:
 *          \li \c hdr_size is the size of the shared object header message.
 *          \li \c msgs_info is an H5_ih_info_t struct defined in H5public.h as
 *              follows: \snippet H5public.h H5_ih_info_t_snip
 *
 *          \li \p index_size is the summed size of all the shared object
 *              header indexes. Each index might be either a B-tree or
 *              a list.
 *
 * \version 1.10.0 C function H5Fget_info() renamed to H5Fget_info1() and
 *                 deprecated in this release.
 *
 * \since 1.8.0
 *
 */
H5_DLL herr_t H5Fget_info1(hid_t obj_id, H5F_info1_t *file_info);
/**
 * \ingroup H5F
 *
 * \brief Sets thelatest version of the library to be used for writing objects
 *
 * \file_id
 * \param[in] latest_format Latest format flag
 *
 * \return \herr_t
 *
 * \deprecated When?
 *
 * \todo In which version was this function deprecated?
 *
 */
H5_DLL herr_t H5Fset_latest_format(hid_t file_id, hbool_t latest_format);
/**
 * \ingroup H5F
 *
 * \brief Determines whether a file is in the HDF5 format
 *
 * \param[in] file_name File name
 *
 * \return \htri_t
 *
 * \deprecated When?
 *
 * \details H5Fis_hdf5() determines whether a file is in the HDF5 format.
 *
 * \todo In which version was this function deprecated?
 *
 */
H5_DLL htri_t H5Fis_hdf5(const char *file_name);

#endif /* H5_NO_DEPRECATED_SYMBOLS */

#ifdef __cplusplus
}
#endif
#endif /* _H5Fpublic_H */
