#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include "../Util/RealFs.hpp"

#include "MountManager.hpp"
#include "JsonYamlInterop.hpp"
#include "Util.hpp"
#include "YamlWstring.hpp"

using namespace std::literals;
using json = nlohmann::json;



namespace {
#if defined(_M_X64)
  const auto gFilterSuffix = L"\\MFP*_x64.dll"s;
#elif defined(_M_IX86)
  const auto gFilterSuffix = L"\\MFP*_x86.dll"s;
#else
# error unsupported architecture
#endif
}



MERGEFS_ERROR_INFO MountManager::MergeFSError::GetLastErrorInfo() {
  MERGEFS_ERROR_INFO errorInfo{};
  if (!LMF_GetLastErrorInfo(&errorInfo)) {
    errorInfo.errorCode = MERGEFS_ERROR_GENERIC_FAILURE;
  }
  return errorInfo;
}


MountManager::MergeFSError::MergeFSError(const MERGEFS_ERROR_INFO& errorInfo) :
  MERGEFS_ERROR_INFO(errorInfo)
{
  switch (errorCode) {
    case MERGEFS_ERROR_SUCCESS:
      errorMessage = L"(Success)"s;
      break;

    case MERGEFS_ERROR_GENERIC_FAILURE:
      errorMessage = L"Generic failure"s;
      break;

    case MERGEFS_ERROR_WINDOWS_ERROR:
      errorMessage = L"Win32 Error: code "s + std::to_wstring(vendorError.windowsErrorCode);
      break;

    case MERGEFS_ERROR_DOKAN_MAIN_ERROR:
      errorMessage = L"Dokan Error: DokanMain returned code "s + std::to_wstring(vendorError.dokanMainResult);
      break;

    default:
      errorMessage = L"MergeFS Error: code "s + std::to_wstring(errorCode);
  }
}


MountManager::MergeFSError::MergeFSError() :
  MergeFSError(GetLastErrorInfo())
{}



MountManager::Win32ApiError::Win32ApiError(DWORD error) :
  MergeFSError(MERGEFS_ERROR_INFO{
    MERGEFS_ERROR_WINDOWS_ERROR,
    error,
  })
{}



MountManager::MountPointAlreadyInUseError::MountPointAlreadyInUseError(MOUNT_ID mountId) :
  std::runtime_error("mount point is already in use"),
  mountId(mountId)
{}



void MountManager::CheckLibMergeFSResult(BOOL ret) {
  if (!ret) {
    throw MergeFSError();
  }
}


std::wstring MountManager::ResolveMountPoint(const std::wstring& mountPoint) {
  constexpr auto IsAlpha = [](wchar_t c) constexpr {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
  };

  const auto csMountPoint = mountPoint.c_str();
  if (IsAlpha(csMountPoint[0]) && (csMountPoint[1] == L'\0' || csMountPoint[1] == L':')) {
    // skip converting because GetFullPathName returns current directory
    // see Remarks of https://docs.microsoft.com/en-us/windows/desktop/api/fileapi/nf-fileapi-getfullpathnamew
    return mountPoint;
  }

  return util::rfs::ToAbsoluteFilepath(mountPoint);
}


void WINAPI MountManager::MountCallback(MOUNT_ID mountId, const MOUNT_INFO* ptrMountInfo, int dokanMainResult) noexcept {
  try {
    auto& mountManager = GetInstance();
    MountData& mountData = mountManager.mMountDataMap.at(mountId);
    mountData.callback(mountId, dokanMainResult, mountData, ptrMountInfo);
    if (mountManager.mMountPointToMountIdMap.count(ptrMountInfo->mountPoint) && mountManager.mMountPointToMountIdMap.at(ptrMountInfo->mountPoint) == mountId) {
      mountManager.mMountPointToMountIdMap.erase(ptrMountInfo->mountPoint);
    }
    mountManager.mMountDataMap.erase(mountId);
  } catch (...) {}
}


MountManager& MountManager::GetInstance() {
  static MountManager sMountManager;
  return sMountManager;
}


MountManager::MountManager() :
  mMountDataMap()
{
  CheckLibMergeFSResult(LMF_Init());
}


void MountManager::AddPlugin(const std::wstring& filepath) {
  CheckLibMergeFSResult(LMF_AddSourcePlugin(filepath.c_str(), FALSE, nullptr));
}


void MountManager::AddPluginsByDirectory(const std::wstring& directory) {
  WIN32_FIND_DATAW win32FindDataW;
  const auto filter = directory.c_str() + gFilterSuffix;
  const auto hFind = FindFirstFileW(filter.c_str(), &win32FindDataW);
  if (hFind == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      return;
    }
    throw Win32ApiError();
  }
  do {
    try {
      AddPlugin(directory.c_str() + L"\\"s + win32FindDataW.cFileName);
    } catch (...) {
      FindClose(hFind);
      throw;
    }
  } while (FindNextFileW(hFind, &win32FindDataW));
  const auto error = GetLastError();
  FindClose(hFind);
  if (error != ERROR_SUCCESS && error != ERROR_NO_MORE_FILES) {
    throw Win32ApiError();
  }
}


void MountManager::GetPluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX& pluginInfoEx) const {
  CheckLibMergeFSResult(LMF_GetSourcePluginInfo(pluginId, &pluginInfoEx));
}


PLUGIN_INFO_EX MountManager::GetPluginInfo(PLUGIN_ID pluginId) const {
  PLUGIN_INFO_EX pluginInfoEx;
  GetPluginInfo(pluginId, pluginInfoEx);
  return pluginInfoEx;
}


std::size_t MountManager::CountPlugins() const {
  DWORD numPluginIds = 0;
  CheckLibMergeFSResult(LMF_GetSourcePlugins(&numPluginIds, nullptr, 0));
  return numPluginIds;
}


std::vector<PLUGIN_ID> MountManager::ListPluginIds() const {
  const auto numPluginIds = CountPlugins();
  std::vector<PLUGIN_ID> pluginIds(numPluginIds);
  CheckLibMergeFSResult(LMF_GetSourcePlugins(nullptr, pluginIds.data(), static_cast<DWORD>(numPluginIds)));
  return pluginIds;
}


std::vector<std::pair<PLUGIN_ID, PLUGIN_INFO_EX>> MountManager::ListPlugins() const {
  const auto pluginIds = ListPluginIds();
  std::vector<std::pair<PLUGIN_ID, PLUGIN_INFO_EX>> pluginInfos(pluginIds.size());
  for (DWORD i = 0; i < pluginIds.size(); i++) {
    const auto pluginId = pluginIds[i];
    pluginInfos[i].first = pluginId;
    GetPluginInfo(pluginId, pluginInfos[i].second);
  }
  return pluginInfos;
}


MOUNT_ID MountManager::AddMount(const std::wstring& configFilepath, std::function<void(MOUNT_ID, int, MountData&, const MOUNT_INFO*)> callback) {
  std::ifstream ifs(configFilepath);
  if (!ifs) {
    throw std::ifstream::failure("failed to load configuration file");
  }
  const auto yaml = YAML::Load(ifs);
  ifs.close();

  const auto mountPoint = yaml["mountPoint"].as<std::wstring>();

  // TODO: convert to absolute path with configuration file's directory as base directory
  const auto metadataFileName = yaml["metadata"].as<std::wstring>();

  bool writable = true;
  try {
    const auto& yamlWritable = yaml["writable"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  bool deferCopyEnabled = true;
  try {
    deferCopyEnabled = yaml["deferCopyEnabled"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  bool caseSensitive = false;
  try {
    caseSensitive = yaml["caseSensitive"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  struct SourceInitializeInfoStorage {
    std::wstring mountSource;
    std::wstring sourcePluginFilename;
    std::string sourcePluginOptionsJSON;
  };
  const auto& yamlSources = yaml["sources"];
  std::vector<SourceInitializeInfoStorage> sourceInitializeInfoStorages(yamlSources.size());
  std::vector<MOUNT_SOURCE_INITIALIZE_INFO> sourceInitializeInfos(yamlSources.size());
  for (std::size_t i = 0; i < yamlSources.size(); i++) {
    const auto& yamlSource = yamlSources[i];
    const auto& yamlPlugin = yamlSource["plugin"];

    const std::wstring mountSource = yamlSource["source"].as<std::wstring>();

    std::wstring sourcePluginFilename;
    try {
      sourcePluginFilename = yamlPlugin["filename"].as<std::wstring>();
    } catch (YAML::BadConversion&) {
    } catch (YAML::InvalidNode&) {}

    std::string sourcePluginOptionsJSON;
    try {
      const auto& yamlOptions = yamlPlugin["options"];
      try {
        sourcePluginOptionsJSON = yamlOptions.as<std::string>();
      } catch (YAML::BadConversion&) {
        const json optionsJson = yamlOptions;
        sourcePluginOptionsJSON = optionsJson.dump();
      }
    } catch (YAML::BadConversion&) {
    } catch (YAML::InvalidNode&) {}

    auto& storage = sourceInitializeInfoStorages[i];
    storage = SourceInitializeInfoStorage{
      mountSource,
      sourcePluginFilename,
      sourcePluginOptionsJSON,
    };
    sourceInitializeInfos[i] = MOUNT_SOURCE_INITIALIZE_INFO{
      storage.mountSource.c_str(),
      {},   // TODO: parse GUID
      storage.sourcePluginFilename.c_str(),
      storage.sourcePluginOptionsJSON.c_str(),
    };
  }

  // load volumeInfoOverride
  VOLUME_INFO_OVERRIDE volumeInfoOverride{
    MERGEFS_VIOF_NONE,
  };
  std::wstring vioVolumeName;
  std::wstring vioFileSystemName;
  const auto& yamlVolumeInfo = yaml["volumeInfo"];
  if (yamlVolumeInfo) {
    if (yamlVolumeInfo["volumeName"]) {
      vioVolumeName = yamlVolumeInfo["volumeName"].as<std::wstring>();
      volumeInfoOverride.VolumeName = vioVolumeName.c_str();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_VOLUMENAME;
    }
    if (yamlVolumeInfo["volumeSerialNumber"]) {
      volumeInfoOverride.VolumeSerialNumber = yamlVolumeInfo["volumeSerialNumber"].as<DWORD>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_VOLUMESERIALNUMBER;
    }
    if (yamlVolumeInfo["maximumComponentLength"]) {
      volumeInfoOverride.MaximumComponentLength = yamlVolumeInfo["maximumComponentLength"].as<DWORD>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_MAXIMUMCOMPONENTLENGTH;
    }
    if (yamlVolumeInfo["fileSystemFlags"]) {
      volumeInfoOverride.FileSystemFlags = yamlVolumeInfo["fileSystemFlags"].as<DWORD>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_FILESYSTEMFLAGS;
    }
    if (yamlVolumeInfo["fileSystemName"]) {
      vioFileSystemName = yamlVolumeInfo["fileSystemName"].as<std::wstring>();
      volumeInfoOverride.FileSystemName = vioFileSystemName.c_str();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_FILESYSTEMNAME;
    }

    if (yamlVolumeInfo["freeBytesAvailable"]) {
      volumeInfoOverride.FreeBytesAvailable = yamlVolumeInfo["freeBytesAvailable"].as<ULONGLONG>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_FREEBYTESAVAILABLE;
    }
    if (yamlVolumeInfo["totalNumberOfBytes"]) {
      volumeInfoOverride.TotalNumberOfBytes = yamlVolumeInfo["totalNumberOfBytes"].as<ULONGLONG>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_TOTALNUMBEROFBYTES;
    }
    if (yamlVolumeInfo["totalNumberOfFreeBytes"]) {
      volumeInfoOverride.TotalNumberOfFreeBytes = yamlVolumeInfo["totalNumberOfFreeBytes"].as<ULONGLONG>();
      volumeInfoOverride.overrideFlags |= MERGEFS_VIOF_TOTALNUMBEROFFREEBYTES;
    }

  }

  // TODO: error handling, restore current directory
  const std::wstring configFileDirectory = util::rfs::GetParentPath(configFilepath);
  SetCurrentDirectoryW(configFileDirectory.c_str());

  // ResolveMountPoint must be called after setting current working directory
  const std::wstring resolvedMountPoint = ResolveMountPoint(mountPoint);

  if (mMountPointToMountIdMap.count(resolvedMountPoint)) {
    throw MountPointAlreadyInUseError(mMountPointToMountIdMap.at(resolvedMountPoint));
  }

  MOUNT_INITIALIZE_INFO mountInitializeInfo{
    resolvedMountPoint.c_str(),
    writable,
    metadataFileName.c_str(),
    deferCopyEnabled,
    caseSensitive,
    static_cast<DWORD>(sourceInitializeInfos.size()),
    sourceInitializeInfos.data(),
    volumeInfoOverride,
  };

  MOUNT_ID mountId = MOUNT_ID_NULL;
  if (!LMF_Mount(&mountInitializeInfo, MountCallback, &mountId)) {
    throw MountError{
      MergeFSError(),
      configFilepath,
      resolvedMountPoint,
    };
  }

  mMountPointToMountIdMap.emplace(resolvedMountPoint, mountId);

  mMountDataMap.emplace(mountId, MountData{
    configFilepath,
    callback,
  });

  return mountId;
}


void MountManager::RemoveMount(MOUNT_ID mountId, bool safe) {
  if (safe) {
    CheckLibMergeFSResult(LMF_SafeUnmount(mountId));
  } else {
    CheckLibMergeFSResult(LMF_Unmount(mountId));
  }
}


void MountManager::GetMountInfo(MOUNT_ID mountId, MOUNT_INFO& mountInfo) const {
  CheckLibMergeFSResult(LMF_GetMountInfo(mountId, &mountInfo));
}


MOUNT_INFO MountManager::GetMountInfo(MOUNT_ID mountId) const {
  MOUNT_INFO mountInfo;
  GetMountInfo(mountId, mountInfo);
  return mountInfo;
}


const MountManager::MountData& MountManager::GetMountData(MOUNT_ID mountId) const {
  return mMountDataMap.at(mountId);
}


std::size_t MountManager::CountMounts() const {
  DWORD numMountIds = 0;
  CheckLibMergeFSResult(LMF_GetMounts(&numMountIds, nullptr, 0));
  return numMountIds;
}


std::vector<MOUNT_ID> MountManager::ListMountIds() const {
  const auto numMountIds = CountMounts();
  std::vector<MOUNT_ID> mountIds(numMountIds);
  CheckLibMergeFSResult(LMF_GetMounts(nullptr, mountIds.data(), static_cast<DWORD>(numMountIds)));
  return mountIds;
}


std::vector<std::pair<MOUNT_ID, MOUNT_INFO>> MountManager::ListMounts() const {
  const auto mountIds = ListMountIds();
  std::vector<std::pair<MOUNT_ID, MOUNT_INFO>> mountInfos(mountIds.size());
  for (DWORD i = 0; i < mountIds.size(); i++) {
    const auto mountId = mountIds[i];
    mountInfos[i].first = mountId;
    GetMountInfo(mountId, mountInfos[i].second);
  }
  return mountInfos;
}


void MountManager::UnmountAll(bool safe) {
  if (safe) {
    CheckLibMergeFSResult(LMF_SafeUnmountAll());
  } else {
    CheckLibMergeFSResult(LMF_UnmountAll());
  }
}


void MountManager::Uninit(bool safe) {
  UnmountAll(safe);
  LMF_Uninit();
}
