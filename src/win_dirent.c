


#ifdef _WIN32

#ifdef HAVE_BPR_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include "win_commons.h"
#include "win_dirent.h"



#ifndef FILE_ATTRIBUTE_ERROR
# define FILE_ATTRIBUTE_ERROR           (0xFFFFFFFF)
#endif /* FILE_ATTRIBUTE_ERROR */


struct dirent_dir
{
    char                directory[_MAX_DIR + 1];    /* . */
    WIN32_FIND_DATAA    find_data;                  /* The Win32 FindFile data. */
    HANDLE              hFind;                      /* The Win32 FindFile handle. */
    struct dirent       dirent;                     /* The handle's entry. */
};

struct wdirent_dir
{
    wchar_t             directory[_MAX_DIR + 1];    /* . */
    WIN32_FIND_DATAW    find_data;                  /* The Win32 FindFile data. */
    HANDLE              hFind;                      /* The Win32 FindFile handle. */
    struct wdirent      dirent;                     /* The handle's entry. */
};


static HANDLE unixem__dirent__findfile_directory(char const *name, LPWIN32_FIND_DATAA data)
{
    char    search_spec[_MAX_PATH +1];

    /* Simply add the *.*, ensuring the path separator is
     * included.
     */
    (void)lstrcpyA(search_spec, name);
    if( '\\' != search_spec[lstrlenA(search_spec) - 1] &&
        '/' != search_spec[lstrlenA(search_spec) - 1])
    {
        (void)lstrcatA(search_spec, "\\*.*");
    }
    else
    {
        (void)lstrcatA(search_spec, "*.*");
    }

    return FindFirstFileA(search_spec, data);
}

#if 0
static HANDLE unixem__dirent__wfindfile_directory(wchar_t const *name, LPWIN32_FIND_DATAW data)
{
    wchar_t search_spec[_MAX_PATH +1];

    /* Simply add the *.*, ensuring the path separator is
     * included.
     */
    lstrcpyW(search_spec, name);
    if( L'\\' != search_spec[lstrlenW(search_spec) - 1] &&
        L'/' != search_spec[lstrlenW(search_spec) - 1])
    {
        lstrcatW(search_spec, L"\\*.*");
    }
    else
    {
        lstrcatW(search_spec, L"*.*");
    }

    return FindFirstFileW(search_spec, data);
}
#endif /* 0 */

/* /////////////////////////////////////////////////////////////////////////
 * API functions
 */

DLL_VARIABLE DIR *opendir(char const *name)
{
    DIR     *result =   NULL;
    DWORD   dwAttr;

    /* Must be a valid name */
    if( !name ||
        !*name ||
        (dwAttr = GetFileAttributes(name)) == 0xFFFFFFFF)
    {
        errno = ENOENT;
    }
    /* Must be a directory */
    else if(!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        errno = ENOTDIR;
    }
    else
    {
        result = (DIR*)malloc(sizeof(DIR));

        if(result == NULL)
        {
            errno = ENOMEM;
        }
        else
        {
            result->hFind = unixem__dirent__findfile_directory(name, &result->find_data);

            if(result->hFind == INVALID_HANDLE_VALUE)
            {
                free(result);

                result = NULL;
            }
            else
            {
                /* Save the directory, in case of rewind. */
                (void)lstrcpyA(result->directory, name);
                (void)lstrcpyA(result->dirent.d_name, result->find_data.cFileName);
                result->dirent.d_mode   =   (int)result->find_data.dwFileAttributes;
            }
        }
    }

#if 0
    if(NULL != dir)
    {
        struct dirent *readdir(DIR *dir)

    }
#endif /* 0 */



    return result;
}

DLL_VARIABLE int closedir(DIR *dir)
{
    int ret;

    if(dir == NULL)
    {
        errno = EBADF;

        ret = -1;
    }
    else
    {
        /* Close the search handle, if not already done. */
        if(dir->hFind != INVALID_HANDLE_VALUE)
        {
            (void)FindClose(dir->hFind);
        }

        free(dir);

        ret = 0;
    }

    return ret;
}

DLL_VARIABLE void rewinddir(DIR *dir)
{
    /* Close the search handle, if not already done. */
    if(dir->hFind != INVALID_HANDLE_VALUE)
    {
        (void)FindClose(dir->hFind);
    }

    dir->hFind = unixem__dirent__findfile_directory(dir->directory, &dir->find_data);

    if(dir->hFind != INVALID_HANDLE_VALUE)
    {
        (void)lstrcpyA(dir->dirent.d_name, dir->find_data.cFileName);
    }
}

DLL_VARIABLE struct dirent *readdir(DIR *dir)
{
    /* The last find exhausted the matches, so return NULL. */
    if(dir->hFind == INVALID_HANDLE_VALUE)
    {
        if(FILE_ATTRIBUTE_ERROR == dir->find_data.dwFileAttributes)
        {
            errno = EBADF;
        }
        else
        {
            dir->find_data.dwFileAttributes = FILE_ATTRIBUTE_ERROR;
        }

        return NULL;
    }
    else
    {
        /* Copy the result of the last successful match to
         * dirent.
         */
        (void)lstrcpyA(dir->dirent.d_name, dir->find_data.cFileName);

        /* Attempt the next match. */
        if(!FindNextFileA(dir->hFind, &dir->find_data))
        {
            /* Exhausted all matches, so close and null the
             * handle.
             */
            (void)FindClose(dir->hFind);
            dir->hFind = INVALID_HANDLE_VALUE;
        }

        return &dir->dirent;
    }
}

#endif // _WIN32
