#include <dokan/dokan.h>

#include <cassert>
#include <cstddef>
#include <mutex>
#include <stdexcept>

#include <Windows.h>

#include "../Util/Common.hpp"

#include "FileSource.hpp"
#include "Util.hpp"


FileSource::FileSource(HANDLE fileHandle) :
  mNeedClose(false),
  mFileHandle(fileHandle)
{
  if (!util::IsValidHandle(fileHandle)) {
    throw std::runtime_error("invalid handle specified");
  }
  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(fileHandle, &fileSize)) {
    throw std::runtime_error("GetFileSizeEx failed");
  }
  mFileSize = fileSize.QuadPart;
}


FileSource::FileSource(LPCWSTR filepath) {
  mNeedClose = true;
  mFileHandle = CreateFileW(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (!util::IsValidHandle(mFileHandle)) {
    throw std::runtime_error("CreateFileW failed");
  }
  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(mFileHandle, &fileSize)) {
    CloseHandle(mFileHandle);
    mFileHandle = NULL;
    throw std::runtime_error("GetFileSizeEx failed");
  }
  assert(fileSize.QuadPart >= 0);
  mFileSize = fileSize.QuadPart;
}


FileSource::~FileSource() {
  if (mNeedClose && util::IsValidHandle(mFileHandle)) {
    CloseHandle(mFileHandle);
    mFileHandle = NULL;
  }
}


Source::SourceSize FileSource::GetSize() {
  return mFileSize;
}


NTSTATUS FileSource::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  std::lock_guard lock(mMutex);
  LARGE_INTEGER newOffset;
  if (!SetFilePointerEx(mFileHandle, util::CreateLargeInteger(offset), &newOffset, SEEK_SET)) {
    return NtstatusFromWin32();
  }
  if (newOffset.QuadPart != offset) {
    return NtstatusFromWin32(ERROR_SEEK);
  }
  DWORD dwReadSize = 0;
  if (!ReadFile(mFileHandle, buffer, static_cast<DWORD>(size), &dwReadSize, NULL)) {
    return NtstatusFromWin32();
  }
  if (readSize) {
    *readSize = dwReadSize;
  }
  return STATUS_SUCCESS;
}
