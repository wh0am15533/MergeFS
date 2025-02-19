#pragma once

#if !defined(FROMLIBMERGEFS) && !defined(FROMPLUGIN) && !defined(FROMCLIENT)
# define FROMCLIENT
#endif

#ifdef FROMCLIENT
# ifndef DWORD
#  include <Windows.h>
# endif
#else
# ifndef DOKAN_VERSION
#  ifdef FROMLIBMERGEFS
#   include "../dokan/dokan/dokan.h"
#  else
#   error include dokan.h before including this
#  endif
# endif
#endif


#define MERGEFS_VERSION                         ((DWORD) 0x00000001)
#define MERGEFS_PLUGIN_INTERFACE_VERSION        ((DWORD) 0x00000004)

#define MERGEFS_PLUGIN_TYPE_SOURCE        ((DWORD) 1)

#define MERGEFS_ERROR_SUCCESS                           ((DWORD) 0)
#define MERGEFS_ERROR_WINDOWS_ERROR                     ((DWORD) 0x00000001)
#define MERGEFS_ERROR_DOKAN_MAIN_ERROR                  ((DWORD) 0x00000101)
#define MERGEFS_ERROR_GENERIC_FAILURE                   ((DWORD) 0x00010000)
#define MERGEFS_ERROR_MORE_DATA                         ((DWORD) 0x00010001)
#define MERGEFS_ERROR_NOT_INITIALIZED                   ((DWORD) 0x00010101)
#define MERGEFS_ERROR_ALREADY_DEINITIALIZED             ((DWORD) 0x00010102)
#define MERGEFS_ERROR_INCOMPATIBLE_PLATFORM             ((DWORD) 0x00020001)
#define MERGEFS_ERROR_INCOMPATIBLE_OS_VERSION           ((DWORD) 0x00020002)
#define MERGEFS_ERROR_INCOMPATIBLE_DOKAN_VERSION        ((DWORD) 0x00020003)
#define MERGEFS_ERROR_INCOMPATIBLE_PLUGIN_TYPE          ((DWORD) 0x00020004)
#define MERGEFS_ERROR_INCOMPATIBLE_PLUGIN_VERSION       ((DWORD) 0x00020005)
#define MERGEFS_ERROR_INVALID_PARAMETER                 ((DWORD) 0x00030000)
#define MERGEFS_ERROR_INVALID_PLUGIN_ID                 ((DWORD) 0x00030001)
#define MERGEFS_ERROR_INVALID_SOURCECONTEXT_ID          ((DWORD) 0x00030002)
#define MERGEFS_ERROR_INVALID_MOUNT_ID                  ((DWORD) 0x00030003)
#define MERGEFS_ERROR_INVALID_FILECONTEXT_ID            ((DWORD) 0x00030004)
#define MERGEFS_ERROR_INVALID_MOUNTPOINT                ((DWORD) 0x00030005)
#define MERGEFS_ERROR_INEXISTENT_FILE                   ((DWORD) 0x00040001)
#define MERGEFS_ERROR_INEXISTENT_PLUGIN                 ((DWORD) 0x00040002)
#define MERGEFS_ERROR_INEXISTENT_SOURCE                 ((DWORD) 0x00040003)
#define MERGEFS_ERROR_INEXISTENT_MOUNT                  ((DWORD) 0x00040004)
#define MERGEFS_ERROR_ALREADY_EXISTING_FILE             ((DWORD) 0x00050001)
#define MERGEFS_ERROR_ALREADY_EXISTING_PLUGIN           ((DWORD) 0x00050002)
#define MERGEFS_ERROR_ALREADY_EXISTING_SOURCE           ((DWORD) 0x00050003)
#define MERGEFS_ERROR_ALREADY_EXISTING_MOUNT            ((DWORD) 0x00050004)
#define MERGEFS_ERROR_ALREADY_EXISTING_MOUNTPOINT       ((DWORD) 0x00050005)

#define MERGEFS_VIOF_NONE                     ((DWORD) 0x00000000)
#define MERGEFS_VIOF_VOLUMENAME               ((DWORD) 0x00000001)
#define MERGEFS_VIOF_VOLUMESERIALNUMBER       ((DWORD) 0x00000002)
#define MERGEFS_VIOF_MAXIMUMCOMPONENTLENGTH   ((DWORD) 0x00000004)
#define MERGEFS_VIOF_FILESYSTEMFLAGS          ((DWORD) 0x00000008)
#define MERGEFS_VIOF_FILESYSTEMNAME           ((DWORD) 0x00000010)
#define MERGEFS_VIOF_FREEBYTESAVAILABLE       ((DWORD) 0x00000100)
#define MERGEFS_VIOF_TOTALNUMBEROFBYTES       ((DWORD) 0x00000200)
#define MERGEFS_VIOF_TOTALNUMBEROFFREEBYTES   ((DWORD) 0x00000400)


# ifdef __cplusplus
#  define MFEXTERNC extern "C"
#  define MFNOEXCEPT noexcept
# else
#  define MFEXTERNC
#  define MFNOEXCEPT
# endif


#ifdef FROMLIBMERGEFS
# define MFPEXPORT
# define MFCIMPORT
#else
# define MFPEXPORT __declspec(dllexport)
# define MFCIMPORT __declspec(dllimport)
#endif


typedef DWORD MOUNT_ID;
typedef DWORD PLUGIN_ID;

#ifdef __cplusplus
constexpr MOUNT_ID MOUNT_ID_NULL = 0;
constexpr PLUGIN_ID PLUGIN_ID_NULL = 0;
#else
# define MOUNT_ID_NULL ((MOUNT_ID)0)
# define PLUGIN_ID_NULL ((PLUGIN_ID)0)
#endif


typedef DWORD tagPLUGIN_TYPE;

#ifdef __cplusplus
enum class PLUGIN_TYPE : tagPLUGIN_TYPE {
  Source = MERGEFS_PLUGIN_TYPE_SOURCE,
};
#else
typedef tagPLUGIN_TYPE PLUGIN_TYPE;
#endif


#pragma pack(push, 1)


typedef struct {
  DWORD errorCode;
  union {
    DWORD windowsErrorCode;
    int dokanMainResult;
  } vendorError;
} MERGEFS_ERROR_INFO;


typedef struct {
  DWORD interfaceVersion;
  PLUGIN_TYPE pluginType;
  GUID guid;
  LPCWSTR name;
  LPCWSTR description;
  DWORD version;
  LPCWSTR versionString;
} PLUGIN_INFO;


typedef struct {
  LPCWSTR filename;
  PLUGIN_INFO pluginInfo;
} PLUGIN_INFO_EX;


typedef struct {
  LPCWSTR mountSource;
  GUID sourcePluginGUID;            // set {00000000-0000-0000-0000-000000000000} if unused
  LPCWSTR sourcePluginFilename;     // set nullptr or L"" if unused
  LPCSTR sourcePluginOptionsJSON;   // set nullptr or "" if unused
} MOUNT_SOURCE_INITIALIZE_INFO;


typedef struct {
  DWORD overrideFlags;

  LPCWSTR VolumeName;
  DWORD VolumeSerialNumber;
  DWORD MaximumComponentLength;
  DWORD FileSystemFlags;
  LPCWSTR FileSystemName;
  
  ULONGLONG FreeBytesAvailable;
  ULONGLONG TotalNumberOfBytes;
  ULONGLONG TotalNumberOfFreeBytes;
} VOLUME_INFO_OVERRIDE;


typedef struct {
  LPCWSTR mountPoint;
  BOOL writable;
  LPCWSTR metadataFileName;
  BOOL deferCopyEnabled;
  BOOL caseSensitive;
  DWORD numSources;
  MOUNT_SOURCE_INITIALIZE_INFO* sources;
  VOLUME_INFO_OVERRIDE volumeInfoOverride;
} MOUNT_INITIALIZE_INFO;


typedef struct {
  LPCWSTR mountSource;
  PLUGIN_ID sourcePluginId;
  LPCSTR sourcePluginOptionsJSON;
} MOUNT_SOURCE_INFO;


typedef struct {
  LPCWSTR mountPoint;
  BOOL writable;
  LPCWSTR metadataFileName;
  BOOL deferCopyEnabled;
  BOOL caseSensitive;
  DWORD numSources;
  MOUNT_SOURCE_INFO* sources;
  // TODO: VOLUME_INFO_OVERRIDE
} MOUNT_INFO;


#ifdef FROMLIBMERGEFS
static_assert(sizeof(PLUGIN_INFO) == 3 * 4 + 1 * 16 + 3 * sizeof(void*));
static_assert(sizeof(PLUGIN_INFO_EX) == sizeof(PLUGIN_INFO) + 1 * sizeof(void*));
static_assert(sizeof(MOUNT_SOURCE_INITIALIZE_INFO) == 1 * 16 + 3 * sizeof(void*));
static_assert(sizeof(VOLUME_INFO_OVERRIDE) == 4 * 4 + 3 * 8 + 2 * sizeof(void*));
static_assert(sizeof(MOUNT_INITIALIZE_INFO) == 4 * 4 + 3 * sizeof(void*) + sizeof(VOLUME_INFO_OVERRIDE));
static_assert(sizeof(MOUNT_INFO) == sizeof(MOUNT_INITIALIZE_INFO) - sizeof(VOLUME_INFO_OVERRIDE));
#endif


#pragma pack(pop)


typedef void(WINAPI *PMountCallback)(MOUNT_ID mountId, const MOUNT_INFO* mountInfo, int dokanMainResult) MFNOEXCEPT;


#ifndef FROMPLUGIN

#ifdef FROMLIBMERGEFS
namespace Exports {
#endif

MFEXTERNC MFCIMPORT DWORD WINAPI LMF_GetLastError(BOOL* win32error) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_GetLastErrorInfo(MERGEFS_ERROR_INFO* ptrErrorInfo) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_Init() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_Uninit() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_AddSourcePlugin(LPCWSTR filename, BOOL front, PLUGIN_ID* outPluginId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_RemoveSourcePlugin(PLUGIN_ID pluginId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_GetSourcePlugins(DWORD* outNumPluginIds, PLUGIN_ID* outPluginIds, DWORD maxPluginIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_SetSourcePluginOrder(const PLUGIN_ID* pluginIds, DWORD numPluginIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_GetSourcePluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX* pluginInfoEx) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_Mount(const MOUNT_INITIALIZE_INFO* mountInitializeInfo, PMountCallback callback, MOUNT_ID* outMountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_GetMounts(DWORD* outNumMountIds, MOUNT_ID* outMountIds, DWORD maxMountIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_GetMountInfo(MOUNT_ID mountId, MOUNT_INFO* outMountInfo) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_SafeUnmount(MOUNT_ID mountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_Unmount(MOUNT_ID mountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_SafeUnmountAll() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI LMF_UnmountAll() MFNOEXCEPT;

#ifdef FROMLIBMERGEFS
}
#endif

#endif
