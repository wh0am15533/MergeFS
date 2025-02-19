#pragma once

#include <cstddef>
#include <mutex>

#include <Windows.h>

#include "Source.hpp"


class FileSource final : public Source {
  std::mutex mMutex;
  bool mNeedClose;
  HANDLE mFileHandle;
  SourceSize mFileSize;

public:
  FileSource(HANDLE fileHandle);
  FileSource(LPCWSTR filepath);
  ~FileSource();

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
