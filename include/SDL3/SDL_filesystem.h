/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 * # CategoryFilesystem
 *
 * SDL Filesystem API.
 */

#ifndef SDL_filesystem_h_
#define SDL_filesystem_h_

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>

#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the directory where the application was run from.
 *
 * SDL caches the result of this call internally, but the first call to this
 * function is not necessarily fast, so plan accordingly.
 *
 * **macOS and iOS Specific Functionality**: If the application is in a ".app"
 * bundle, this function returns the Resource directory (e.g.
 * MyApp.app/Contents/Resources/). This behaviour can be overridden by adding
 * a property to the Info.plist file. Adding a string key with the name
 * SDL_FILESYSTEM_BASE_DIR_TYPE with a supported value will change the
 * behaviour.
 *
 * Supported values for the SDL_FILESYSTEM_BASE_DIR_TYPE property (Given an
 * application in /Applications/SDLApp/MyApp.app):
 *
 * - `resource`: bundle resource directory (the default). For example:
 *   `/Applications/SDLApp/MyApp.app/Contents/Resources`
 * - `bundle`: the Bundle directory. For example:
 *   `/Applications/SDLApp/MyApp.app/`
 * - `parent`: the containing directory of the bundle. For example:
 *   `/Applications/SDLApp/`
 *
 * **Nintendo 3DS Specific Functionality**: This function returns "romfs"
 * directory of the application as it is uncommon to store resources outside
 * the executable. As such it is not a writable directory.
 *
 * The returned path is guaranteed to end with a path separator ('\\' on
 * Windows, '/' on most other platforms).
 *
 * \returns an absolute path in UTF-8 encoding to the application data
 *          directory. NULL will be returned on error or when the platform
 *          doesn't implement this functionality, call SDL_GetError() for more
 *          information.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPrefPath
 */
extern SDL_DECLSPEC const char * SDLCALL SDL_GetBasePath(void);

/**
 * Get the user-and-app-specific path where files can be written.
 *
 * Get the "pref dir". This is meant to be where users can write personal
 * files (preferences and save games, etc) that are specific to your
 * application. This directory is unique per user, per application.
 *
 * This function will decide the appropriate location in the native
 * filesystem, create the directory if necessary, and return a string of the
 * absolute path to the directory in UTF-8 encoding.
 *
 * On Windows, the string might look like:
 *
 * `C:\\Users\\bob\\AppData\\Roaming\\My Company\\My Program Name\\`
 *
 * On Linux, the string might look like:
 *
 * `/home/bob/.local/share/My Program Name/`
 *
 * On macOS, the string might look like:
 *
 * `/Users/bob/Library/Application Support/My Program Name/`
 *
 * You should assume the path returned by this function is the only safe place
 * to write files (and that SDL_GetBasePath(), while it might be writable, or
 * even the parent of the returned path, isn't where you should be writing
 * things).
 *
 * Both the org and app strings may become part of a directory name, so please
 * follow these rules:
 *
 * - Try to use the same org string (_including case-sensitivity_) for all
 *   your applications that use this function.
 * - Always use a unique app string for each one, and make sure it never
 *   changes for an app once you've decided on it.
 * - Unicode characters are legal, as long as they are UTF-8 encoded, but...
 * - ...only use letters, numbers, and spaces. Avoid punctuation like "Game
 *   Name 2: Bad Guy's Revenge!" ... "Game Name 2" is sufficient.
 *
 * The returned path is guaranteed to end with a path separator ('\\' on
 * Windows, '/' on most other platforms).
 *
 * \param org the name of your organization.
 * \param app the name of your application.
 * \returns a UTF-8 string of the user directory in platform-dependent
 *          notation. NULL if there's a problem (creating directory failed,
 *          etc.). This should be freed with SDL_free() when it is no longer
 *          needed.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetBasePath
 */
extern SDL_DECLSPEC char * SDLCALL SDL_GetPrefPath(const char *org, const char *app);

/**
 * The type of the OS-provided default folder for a specific purpose.
 *
 * Note that many common folders, like the Trash, the Temp folder or
 * app-specific folders like AppData are not listed here; using them properly
 * requires more treatment than fetching the folder path and using it. To use
 * these folders, see their dedicated functions.
 *
 * The folders supported per platform are:
 *
 * |             | Windows | macOS/iOS | tvOS | Unix (XDG) | Haiku | Emscripten |
 * | ----------- | ------- | --------- | ---- | ---------- | ----- | ---------- |
 * | HOME        | X       | X         |      | X          | X     | X          |
 * | DESKTOP     | X       | X         |      | X          | X     |            |
 * | DOCUMENTS   | X       | X         |      | X          |       |            |
 * | DOWNLOADS   | Vista+  | X         |      | X          |       |            |
 * | MUSIC       | X       | X         |      | X          |       |            |
 * | PICTURES    | X       | X         |      | X          |       |            |
 * | PUBLICSHARE |         | X         |      | X          |       |            |
 * | SAVEDGAMES  | Vista+  |           |      |            |       |            |
 * | SCREENSHOTS | Vista+  |           |      |            |       |            |
 * | TEMPLATES   | X       | X         |      | X          |       |            |
 * | VIDEOS      | X       | X*        |      | X          |       |            |
 *
 * Note that on macOS/iOS, the Videos folder is called "Movies".
 *
 * \since This enum is available since SDL 3.0.0.
 *
 * \sa SDL_GetUserFolder
 */
typedef enum SDL_Folder
{
    SDL_FOLDER_HOME,        /**< The folder which contains all of the current user's data, preferences, and documents. It usually contains most of the other folders. If a requested folder does not exist, the home folder can be considered a safe fallback to store a user's documents. */

    SDL_FOLDER_DESKTOP,     /**< The folder of files that are displayed on the desktop. Note that the existence of a desktop folder does not guarantee that the system does show icons on its desktop; certain GNU/Linux distros with a graphical environment may not have desktop icons. */

    SDL_FOLDER_DOCUMENTS,   /**< User document files, possibly application-specific. This is a good place to save a user's projects. */

    SDL_FOLDER_DOWNLOADS,   /**< Standard folder for user files downloaded from the internet. */

    SDL_FOLDER_MUSIC,       /**< Music files that can be played using a standard music player (mp3, ogg...). */

    SDL_FOLDER_PICTURES,    /**< Image files that can be displayed using a standard viewer (png, jpg...). */

    SDL_FOLDER_PUBLICSHARE, /**< Files that are meant to be shared with other users on the same computer. */

    SDL_FOLDER_SAVEDGAMES,  /**< Save files for games. */

    SDL_FOLDER_SCREENSHOTS, /**< Application screenshots. */

    SDL_FOLDER_TEMPLATES,   /**< Template files to be used when the user requests the desktop environment to create a new file in a certain folder, such as "New Text File.txt".  Any file in the Templates folder can be used as a starting point for a new file. */

    SDL_FOLDER_VIDEOS,      /**< Video files that can be played using a standard video player (mp4, webm...). */

    SDL_FOLDER_COUNT        /**< Total number of types in this enum, not a folder type by itself. */

} SDL_Folder;

/**
 * Finds the most suitable user folder for a specific purpose.
 *
 * Many OSes provide certain standard folders for certain purposes, such as
 * storing pictures, music or videos for a certain user. This function gives
 * the path for many of those special locations.
 *
 * This function is specifically for _user_ folders, which are meant for the
 * user to access and manage. For application-specific folders, meant to hold
 * data for the application to manage, see SDL_GetBasePath() and
 * SDL_GetPrefPath().
 *
 * The returned path is guaranteed to end with a path separator ('\\' on
 * Windows, '/' on most other platforms).
 *
 * If NULL is returned, the error may be obtained with SDL_GetError().
 *
 * \param folder the type of folder to find.
 * \returns either a null-terminated C string containing the full path to the
 *          folder, or NULL if an error happened.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC const char * SDLCALL SDL_GetUserFolder(SDL_Folder folder);


/* Abstract filesystem interface */

typedef enum SDL_PathType
{
    SDL_PATHTYPE_NONE,      /**< path does not exist */
    SDL_PATHTYPE_FILE,      /**< a normal file */
    SDL_PATHTYPE_DIRECTORY, /**< a directory */
    SDL_PATHTYPE_OTHER      /**< something completely different like a device node (not a symlink, those are always followed) */
} SDL_PathType;

typedef struct SDL_PathInfo
{
    SDL_PathType type;      /**< the path type */
    Uint64 size;            /**< the file size in bytes */
    SDL_Time create_time;   /**< the time when the path was created */
    SDL_Time modify_time;   /**< the last time the path was modified */
    SDL_Time access_time;   /**< the last time the path was read */
} SDL_PathInfo;

/**
 * Flags for path matching
 *
 * \since This datatype is available since SDL 3.0.0.
 *
 * \sa SDL_GlobDirectory
 * \sa SDL_GlobStorageDirectory
 */
typedef Uint32 SDL_GlobFlags;

#define SDL_GLOB_CASEINSENSITIVE (1u << 0)

/**
 * Create a directory.
 *
 * \param path the path of the directory to create.
 * \returns SDL_TRUE on success or SDL_FALSE on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_CreateDirectory(const char *path);

/* Callback for directory enumeration. Return 1 to keep enumerating,
   0 to stop enumerating (no error), -1 to stop enumerating and
   report an error. `dirname` is the directory being enumerated,
   `fname` is the enumerated entry. */
typedef int (SDLCALL *SDL_EnumerateDirectoryCallback)(void *userdata, const char *dirname, const char *fname);

/**
 * Enumerate a directory through a callback function.
 *
 * This function provides every directory entry through an app-provided
 * callback, called once for each directory entry, until all results have been
 * provided or the callback returns <= 0.
 *
 * \param path the path of the directory to enumerate.
 * \param callback a function that is called for each entry in the directory.
 * \param userdata a pointer that is passed to `callback`.
 * \returns SDL_TRUE on success or SDL_FALSE on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata);

/**
 * Remove a file or an empty directory.
 *
 * \param path the path of the directory to enumerate.
 * \returns SDL_TRUE on success or SDL_FALSE on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_RemovePath(const char *path);

/**
 * Rename a file or directory.
 *
 * \param oldpath the old path.
 * \param newpath the new path.
 * \returns SDL_TRUE on success or SDL_FALSE on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_RenamePath(const char *oldpath, const char *newpath);

/**
 * Copy a file.
 *
 * \param oldpath the old path.
 * \param newpath the new path.
 * \returns SDL_TRUE on success or SDL_FALSE on failure; call SDL_GetError()
 *          for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_CopyFile(const char *oldpath, const char *newpath);

/**
 * Get information about a filesystem path.
 *
 * \param path the path to query.
 * \param info a pointer filled in with information about the path, or NULL to
 *             check for the existence of a file.
 * \returns SDL_TRUE on success or SDL_FALSE if the file doesn't exist, or
 *          another failure; call SDL_GetError() for more information.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GetPathInfo(const char *path, SDL_PathInfo *info);

/**
 * Enumerate a directory tree, filtered by pattern, and return a list.
 *
 * Files are filtered out if they don't match the string in `pattern`, which
 * may contain wildcard characters '*' (match everything) and '?' (match one
 * character). If pattern is NULL, no filtering is done and all results are
 * returned. Subdirectories are permitted, and are specified with a path
 * separator of '/'. Wildcard characters '*' and '?' never match a path
 * separator.
 *
 * `flags` may be set to SDL_GLOB_CASEINSENSITIVE to make the pattern matching
 * case-insensitive.
 *
 * The returned array is always NULL-terminated, for your iterating
 * convenience, but if `count` is non-NULL, on return it will contain the
 * number of items in the array, not counting the NULL terminator.
 *
 * \param path the path of the directory to enumerate.
 * \param pattern the pattern that files in the directory must match. Can be
 *                NULL.
 * \param flags `SDL_GLOB_*` bitflags that affect this search.
 * \param count on return, will be set to the number of items in the returned
 *              array. Can be NULL.
 * \returns an array of strings on success or NULL on failure; call
 *          SDL_GetError() for more information. This is a single allocation
 *          that should be freed with SDL_free() when it is no longer needed.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC char ** SDLCALL SDL_GlobDirectory(const char *path, const char *pattern, SDL_GlobFlags flags, int *count);

/**
 * Create a secure temporary file.
 *
 * This function is not path-based to avoid race conditions. Returning a path
 * and letting the caller create the file opens a time-of-check-to-time-of-use
 * (TOCTOU) safety issue, where an attacker can use the small delay between the
 * moment the name is generated and the moment the file is created to create
 * the file first and give it undesirable attributes, such as giving itself
 * full read/write access to the file, or making the file a symlink to another,
 * sensitive file.
 *
 * \returns an open IOStream object to the file, or NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateUnsafeTempFile
 * \sa SDL_CreateTempFolder
 */
extern SDL_DECLSPEC SDL_IOStream *SDLCALL SDL_CreateSafeTempFile(void);

/**
 * Create a temporary file, with less security considerations.
 *
 * Unlike SDL_CreateSafeTempFile(), this function provides a path, which can
 * then be used like any other file on the filesystem. This has security
 * implications; an attacker could exploit the small delay between the moment
 * the name is generated and the moment the file is created to create the file
 * first and give it undesirable attributes, such as giving itself full
 * read/write access to the file, or making the file a symlink to another,
 * sensitive file.
 *
 * The path string is owned by the caller and must be freed with SDL_free().
 *
 * \returns an absolute path to the temporary file, or NULL on error; call
 *          SDL_GetError() for details. If a path is returned, it is encoded in
 *          OS-specific format, and is guaranteed to finish with a path
 *          separator.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSafeTempFile
 * \sa SDL_CreateTempFolder
 */
extern SDL_DECLSPEC char *SDLCALL SDL_CreateUnsafeTempFile(void);

/**
 * Create a temporary folder.
 *
 * Keep in mind any program running as the same user as your program can access
 * the contents of the folders and the files in it. Do not perform sensitive
 * tasks using the temporary folder. If you need one or more temporary files
 * for sensitive purposes, use SDL_CreateSafeTempFile().
 *
 * The path string is owned by the caller and must be freed with SDL_free().
 *
 * \returns an absolute path to the temporary folder, or NULL on error; call
 *          SDL_GetError() for details. If a path is returned, it is encoded in
 *          OS-specific format, and is guaranteed to finish with a path
 *          separator.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateSafeTempFile
 * \sa SDL_CreateUnsafeTempFile
 */
extern SDL_DECLSPEC char *SDLCALL SDL_CreateTempFolder(void);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_filesystem_h_ */
