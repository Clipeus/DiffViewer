#pragma once

#include "TextFile.h"
#include "GitHub.h"

enum class ItemStatus { Identical, Different, LeftOnly, RightOnly };

struct CompareItem
{
  std::wstring name;        // display name (file or folder name)
  std::wstring relPath;     // path relative to the comparison root
  bool isDir = false;
  ItemStatus status = ItemStatus::Identical;

  // file system sessions
  std::wstring leftPath;
  std::wstring rightPath;

  // pull request sessions
  std::wstring prStatus;
  std::wstring prPreviousPath;
  int additions = 0;
  int deletions = 0;

  std::vector<std::unique_ptr<CompareItem>> children;
};

struct SessionSummary
{
  int different = 0;
  int leftOnly = 0;
  int rightOnly = 0;
  int identical = 0;
};

class CompareSession
{
public:
  virtual ~CompareSession() = default;

  virtual std::wstring GetTitle() const = 0;
  virtual std::wstring GetSummary() const = 0;
  virtual bool IsTree() const = 0;                      // false for a single file pair
  virtual bool CanSwap() const { return false; }
  virtual void Swap() {}

  virtual bool Load(std::wstring& error) = 0;
  virtual bool LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                        std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error) = 0;

  const CompareItem& Root() const { return m_root; }
  const SessionSummary& Summary() const { return m_summary; }
  const CompareItem* FindItem(const std::wstring& relPath) const;

protected:
  static const CompareItem* FindItem(const CompareItem& node, const std::wstring& relPath);

  CompareItem m_root;
  SessionSummary m_summary;
};

class FilePairSession : public CompareSession
{
public:
  FilePairSession(const std::wstring& left, const std::wstring& right);

  std::wstring GetTitle() const override;
  std::wstring GetSummary() const override;
  bool IsTree() const override { return false; }
  bool CanSwap() const override { return true; }
  void Swap() override;
  bool Load(std::wstring& error) override;
  bool LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error) override;

private:
  std::wstring m_left;
  std::wstring m_right;
};

class FolderSession : public CompareSession
{
public:
  FolderSession(const std::wstring& left, const std::wstring& right);

  std::wstring GetTitle() const override;
  std::wstring GetSummary() const override;
  bool IsTree() const override { return true; }
  bool CanSwap() const override { return true; }
  void Swap() override;
  bool Load(std::wstring& error) override;
  bool LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error) override;

private:
  void ScanDir(const std::filesystem::path& left, const std::filesystem::path& right, const std::wstring& relPath, CompareItem& node);

  std::wstring m_left;
  std::wstring m_right;
};

class PullRequestSession : public CompareSession
{
public:
  explicit PullRequestSession(const std::wstring& url);

  std::wstring GetTitle() const override;
  std::wstring GetSummary() const override;
  bool IsTree() const override { return true; }
  bool Load(std::wstring& error) override;
  bool LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error) override;

  const PullRequestInfo& Info() const { return m_info; }

private:
  bool Fetch(const std::wstring& path, const std::wstring& ref, TextContent& content, std::wstring& error);
  CompareItem& EnsureFolder(const std::wstring& path);
  static void CompactFolders(CompareItem& node);

  std::wstring m_url;
  GitHubClient m_client;
  PullRequestInfo m_info;
  std::map<std::wstring, TextContent> m_cache;
};
