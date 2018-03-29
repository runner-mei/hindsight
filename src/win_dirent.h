

#ifndef _win32_dirent_h_
#define _win32_dirent_h_ 1


#ifndef _WIN32
# error This file is only currently defined for compilation on Win32 systems
#endif /* _WIN32 */


#include <stddef.h>
#include "win_commons.h"


#ifndef NAME_MAX
# define NAME_MAX   (260)   /*!< \brief The maximum number of characters (including null terminator) in a directory entry name */
#endif /* !NAME_MAX */

typedef struct dirent_dir   DIR; /*!< \brief Handle type for ANSI directory enumeration. \note dirent_dir is defined internally */
typedef struct wdirent_dir  wDIR; /*!< \brief Handle type for Unicode directory enumeration. \note dirent_dir is defined internally */

/** \brief Results structure for readdir()
 */
struct dirent
{
    char    d_name[NAME_MAX + 1];   /*!< file name (null-terminated) */
    int     d_mode;                 /*!< currently unused */    
};

/** \brief Results structure for wreaddir()
 */
struct wdirent
{
    wchar_t d_name[NAME_MAX + 1];   /*!< file name (null-terminated) */
    int     d_mode;                 /*!< currently unused */    
};

/* /////////////////////////////////////////////////////////////////////////
 * API functions
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** \brief Returns a pointer to the next directory entry.
 *
 * This function opens the directory named by filename, and returns a
 * directory to be used to in subsequent operations. NULL is returned
 * if name cannot be accessed, or if resources cannot be acquired to
 * process the request.
 *
 * \param name The name of the directory to search
 * \return The directory handle from which the entries are read or NULL
 */
DIR* opendir(char const* name);
/** \brief Identical semantics to opendir(), but for Unicode searches.
 */
wDIR* wopendir(wchar_t const* name);

/** \brief Closes a directory handle
 *
 * This function closes a directory handle that was opened with opendir()
 * and releases any resources associated with that directory handle.
 *
 * \param dir The directory handle from which the entries are read
 * \return 0 on success, or -1 to indicate error.
 */
int closedir(DIR* dir);
/** \brief Identical semantics to closedir(), but for Unicode searches.
 */
int wclosedir(wDIR* dir);

/** \brief Resets a directory search position
 *
 * This function resets the position of the named directory handle to
 * the beginning of the directory.
 *
 * \param dir The directory handle whose position should be reset
 */
void rewinddir(DIR* dir);
/** \brief Identical semantics to rewinddir(), but for Unicode searches.
 */
void wrewinddir(wDIR* dir);

/** \brief Returns a pointer to the next directory entry.
 *
 * This function returns a pointer to the next directory entry, or NULL upon
 * reaching the end of the directory or detecting an invalid seekdir() operation
 *
 * \param dir The directory handle from which the entries are read
 * \return A dirent structure or NULL
 */
struct dirent*  readdir(DIR* dir);
/** \brief Identical semantics to readdir(), but for Unicode searches.
 */
struct wdirent* wreaddir(wDIR* dir);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // _win32_dirent_h_