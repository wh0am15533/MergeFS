#define NOMINMAX

#include <dokan/dokan.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>

#include <Windows.h>

#include "ArchiveSourceMountFile.hpp"
#include "ArchiveSourceMount.hpp"
#include "Util.hpp"
#include "NanaZ/COMError.hpp"

using namespace std::literals;



ArchiveSourceMountFile::ArchiveSourceMountFile(ArchiveSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) :
  ReadonlySourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId),
  sourceMount(sourceMount),
  realPath(sourceMount.GetRealPath(FileName)),
  ptrDirectoryTree(sourceMount.GetDirectoryTreeR(realPath))
{
  if (!ptrDirectoryTree) {
    throw NtstatusError(sourceMount.ReturnPathOrNameNotFoundErrorR(realPath));
  }

  fileAttributes = DirectoryTree::FilterArchiveFileAttributes(*ptrDirectoryTree);
  volumeSerialNumber = sourceMount.GetVolumeSerialNumber();
}


NTSTATUS ArchiveSourceMountFile::DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
  static_assert(sizeof(UInt32) == sizeof(DWORD));

  std::lock_guard lock(*ptrDirectoryTree->streamMutex);
  /*
  std::wstring debugStr = L"ReadFile  "s + realPath + L"  Offset: "s + std::to_wstring(Offset) + L", BufferLength: "s + std::to_wstring(BufferLength) + L", Sum: "s + std::to_wstring(Offset + BufferLength) + L"\n"s;
  OutputDebugStringW(debugStr.c_str());
  */
  if (Offset < 0) {
    return NtstatusFromWin32(ERROR_NEGATIVE_SEEK);
  }
  if (static_cast<ULONGLONG>(Offset) >= ptrDirectoryTree->fileSize) {
    if (ReadLength) {
      *ReadLength = 0;
    }
    return STATUS_SUCCESS;
  }
  UInt64 newPosition = -1;
  COMError::CheckHRESULT(ptrDirectoryTree->inStream->Seek(Offset, STREAM_SEEK_SET, &newPosition));
  if (newPosition != Offset) {
    return NtstatusFromWin32(ERROR_SEEK);
  }
  const UInt32 sizeToRead = static_cast<UInt32>(std::min<ULONGLONG>(BufferLength, ptrDirectoryTree->fileSize - Offset));
  UInt32 totalReadSize = 0;
  UInt32 readSize;
  do {
    readSize = 0;
    COMError::CheckHRESULT(ptrDirectoryTree->inStream->Read(static_cast<std::byte*>(Buffer) + totalReadSize, sizeToRead - totalReadSize, &readSize));
    totalReadSize += readSize;
  } while (readSize && totalReadSize < sizeToRead);
  if (ReadLength) {
    *ReadLength = totalReadSize;
  }
#ifdef _DEBUG
  if (totalReadSize != BufferLength) {
    std::wstring debugStr = L"ArchiveSourceMountFile::DReadFile ["s + realPath + L"] BufferLength ("s + std::to_wstring(BufferLength) + L") != totalReadSize ("s + std::to_wstring(totalReadSize) + L"), Offset = "s + std::to_wstring(Offset) + L"\n"s;
    OutputDebugStringW(debugStr.c_str());
  }
#endif
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMountFile::DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!Buffer) {
    return STATUS_SUCCESS;
  }
  Buffer->dwFileAttributes = fileAttributes;
  Buffer->ftCreationTime = ptrDirectoryTree->creationTime;
  Buffer->ftLastAccessTime = ptrDirectoryTree->lastAccessTime;
  Buffer->ftLastWriteTime = ptrDirectoryTree->lastWriteTime;
  Buffer->dwVolumeSerialNumber = volumeSerialNumber;
  Buffer->nFileSizeHigh = (ptrDirectoryTree->fileSize >> 32) & 0xFFFFFFFF;
  Buffer->nFileSizeLow = ptrDirectoryTree->fileSize & 0xFFFFFFFF;
  Buffer->nNumberOfLinks = ptrDirectoryTree->numberOfLinks;
  Buffer->nFileIndexHigh = (ptrDirectoryTree->fileIndex >> 32) & 0xFFFFFFFF;
  Buffer->nFileIndexLow = ptrDirectoryTree->fileIndex & 0xFFFFFFFF;
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMountFile::DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
  // TODO
  return STATUS_NOT_IMPLEMENTED;
}
