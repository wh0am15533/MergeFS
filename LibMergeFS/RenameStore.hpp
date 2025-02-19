#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../SDK/CaseSensitivity.hpp"


class RenameStore {
  class PathTrieTree {
  public:
    static constexpr wchar_t Delimiter = L'\\';

  private:
    bool mCaseSensitive;      // not const because of move assignment operator
    std::unordered_map<std::wstring, PathTrieTree, CaseSensitivity::CiHash, CaseSensitivity::CiEqualTo> mChildren;
    bool mValid;
    std::wstring mFilepath;

  public:
    // disable copy constructor and copy assignment operator for performance reasons
    PathTrieTree(const PathTrieTree&) = delete;
    PathTrieTree& operator=(const PathTrieTree&) = delete;

    // define move constructor and move assignment operator
    PathTrieTree(PathTrieTree&&) = default;
    PathTrieTree& operator=(PathTrieTree&&) = default;

    PathTrieTree(bool caseSensitive);
    PathTrieTree(bool caseSensitive, std::wstring_view filepath);

  private:
    void Traverse(const std::wstring& prefix, std::function<void(const std::wstring&, const std::wstring&, bool)>& callback) const;
    std::size_t CleanupUnusedNodes(std::wstring_view key);

  public:
    bool IsCaseSensitive() const;
    bool IsValid() const;
    const std::wstring& GetData() const;
    void SetData(std::wstring_view filepath);
    void Invalidate();
    bool Has(std::wstring_view key, bool validOnly) const;
    PathTrieTree* Get(std::wstring_view key, bool validOnly);
    const PathTrieTree* Get(std::wstring_view key, bool validOnly) const;
    bool Remove(std::wstring_view key);
    std::vector<std::pair<std::wstring, std::wstring>> ListChildren(bool validOnly) const;

    PathTrieTree* RetrieveRecursive(std::wstring_view key);
    const PathTrieTree* RetrieveRecursive(std::wstring_view key) const;
    bool Match(std::wstring_view key) const;
    std::optional<std::pair<std::wstring, std::wstring>> FindLongestMatch(std::wstring_view key) const;
    void Traverse(std::function<void(const std::wstring&, const std::wstring&, bool)> callback, bool self) const;
    std::pair<PathTrieTree&, bool> InsertRecursive(std::wstring_view key);
    std::pair<PathTrieTree&, bool> InsertRecursive(std::wstring_view key, std::wstring_view filepath);
    std::optional<std::pair<std::wstring, bool>> ResetEntry(std::wstring_view key);
    PathTrieTree* MoveNode(std::wstring_view from, std::wstring_view to);
  };

  const bool mCaseSensitive;
  PathTrieTree mForwardLookupTree;
  PathTrieTree mReverseLookupTree;

public:
  enum class Result {
    Success,
    Invalid,
    NotExists,
    AlreadyExists,
  };

  RenameStore(bool caseSensitive);

  void AddEntry(std::wstring_view originalFilepath, std::wstring_view renamedFilepath);
  std::vector<std::pair<std::wstring, std::wstring>> GetEntries() const;

  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInForwardLookupTree(std::wstring_view filepath) const;
  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInReverseLookupTree(std::wstring_view filepath) const;
  std::optional<bool> Exists(std::wstring_view filepath) const;
  std::optional<std::wstring> Resolve(std::wstring_view filepath) const;
  Result Rename(std::wstring_view srcFilepath, std::wstring_view destFilepath);
  bool RemoveEntry(std::wstring_view filepath);
};
