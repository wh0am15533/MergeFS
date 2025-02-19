#pragma comment(lib, "WindowsApp.lib")

#define NOMINMAX

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include "../Util/RealFs.hpp"

#include <memory>

#include <Windows.h>
#include <winrt/base.h>

#include "Util.hpp"
#include "ArchiveSourceMount.hpp"
#include "NanaZ/COMPtr.hpp"
#include "NanaZ/NanaZ.hpp"
#include "NanaZ/FileStream.hpp"

using namespace std::literals;



namespace {
#if defined(_M_X64)
  constexpr auto SevenZDllFilepath = L"7z_x64.dll";
#elif defined(_M_IX86)
  constexpr auto SevenZDllFilepath = L"7z_x86.dll";
#else
# error unsupported architecture
#endif

  // {8F3ACF43-D9DD-0000-1010-400000000000}
  constexpr GUID DPluginGUID = {0x8F3ACF43, 0xD9DD, 0x0000, {0x10, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00}};

  constexpr DWORD DMaximumComponentLength = MAX_PATH;
  constexpr DWORD DFileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH | FILE_UNICODE_ON_DISK;

  const PLUGIN_INFO gPluginInfo = {
    MERGEFS_PLUGIN_INTERFACE_VERSION,
    PLUGIN_TYPE::Source,
    DPluginGUID,
    L"Archive",
    L"Archive source plugin",
    0x00000001,
    L"0.0.1",
  };

  PLUGIN_INITIALIZE_INFO gPluginInitializeInfo{};

  std::wstring gDllPrefix;

  std::unique_ptr<NanaZ> gPtrNanaZ{};
}



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  if (fdwReason == DLL_PROCESS_ATTACH) {
    // load 7z.dll from this DLL's directory
    try {
      gDllPrefix.clear();
      const auto dllPath = util::rfs::GetModuleFilepath(hinstDLL);
      gDllPrefix = util::rfs::GetParentPath(dllPath) + L"\\";
    } catch (...) {}
  }
  return TRUE;
}



const PLUGIN_INFO* SGetPluginInfoImpl() noexcept {
  return &gPluginInfo;
}


PLUGIN_INITCODE SInitializeImpl(const PLUGIN_INITIALIZE_INFO* InitializeInfo) noexcept {
  gPluginInitializeInfo = *InitializeInfo;
  _SetPluginInitializeInfo(*InitializeInfo);
  try {
    gPtrNanaZ = std::make_unique<NanaZ>((gDllPrefix + std::wstring(SevenZDllFilepath)).c_str());
  } catch (...) {
    return PLUGIN_INITCODE::Other;
  }
  return PLUGIN_INITCODE::Success;
}


BOOL SIsSupportedImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo) noexcept {
  try {
    if (!gPtrNanaZ) {
      return FALSE;
    }
    auto& NanaZ = *gPtrNanaZ;
    const auto archiveFilepathN = FindRootFilepath(util::rfs::ToAbsoluteFilepath(InitializeMountInfo->FileName));
    if (!archiveFilepathN) {
      return FALSE;
    }
    return !NanaZ.FindFormatByStream(CreateCOMPtr(new InFileStream(archiveFilepathN.value().c_str()))).empty() ? TRUE : FALSE;
  } catch (...) {
    return FALSE;
  }
}



std::unique_ptr<SourceMountBase> MountImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) {
  return std::make_unique<ArchiveSourceMount>(*gPtrNanaZ, InitializeMountInfo, sourceContextId);
}
