#include "pch.h"
#include "resource.h"
#include "CompareSession.h"

namespace fs = std::filesystem;

namespace
{
  std::wstring ToLower(const std::wstring& s)
  {
    std::wstring r = s;
    for (auto& c : r)
      c = towlower(c);
    return r;
  }

  bool IsIgnoredDir(const std::wstring& name)
  {
    return _wcsicmp(name.c_str(), L".git") == 0 || _wcsicmp(name.c_str(), L".svn") == 0 || _wcsicmp(name.c_str(), L".hg") == 0;
  }

  std::wstring FileName(const std::wstring& path)
  {
    return fs::path(path).filename().wstring();
  }

  std::wstring ShortSha(const std::wstring& sha)
  {
    return sha.substr(0, std::min<size_t>(7, sha.size()));
  }
}

// ─── CompareSession ──────────────────────────────────────────────────────────

const CompareItem* CompareSession::FindItem(const std::wstring& relPath) const
{
  return FindItem(m_root, relPath);
}

const CompareItem* CompareSession::FindItem(const CompareItem& node, const std::wstring& relPath)
{
  for (const auto& child : node.children)
  {
    if (child->relPath == relPath)
      return child.get();
    if (child->isDir)
    {
      const CompareItem* found = FindItem(*child, relPath);
      if (found)
        return found;
    }
  }
  return nullptr;
}

// ─── FilePairSession ─────────────────────────────────────────────────────────

FilePairSession::FilePairSession(const std::wstring& left, const std::wstring& right)
  : m_left(left), m_right(right)
{
}

std::wstring FilePairSession::GetTitle() const
{
  return FileName(m_left) + L" <-> " + FileName(m_right);
}

std::wstring FilePairSession::GetSummary() const
{
  return L"";   // the pane headers already show both paths
}

void FilePairSession::Swap()
{
  std::swap(m_left, m_right);
}

bool FilePairSession::Load(std::wstring& error)
{
  m_root = CompareItem();
  m_summary = SessionSummary();

  std::error_code ec;
  if (!fs::exists(m_left, ec))
  {
    error = Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), m_left.c_str());
    return false;
  }
  if (!fs::exists(m_right, ec))
  {
    error = Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), m_right.c_str());
    return false;
  }

  auto item = std::make_unique<CompareItem>();
  item->name = FileName(m_left);
  if (_wcsicmp(item->name.c_str(), FileName(m_right).c_str()) != 0)
    item->name += L" <-> " + FileName(m_right);
  item->relPath = L"*";
  item->leftPath = m_left;
  item->rightPath = m_right;
  item->status = TextFile::FilesEqual(m_left, m_right) ? ItemStatus::Identical : ItemStatus::Different;
  if (item->status == ItemStatus::Identical)
    m_summary.identical++;
  else
    m_summary.different++;
  m_root.children.push_back(std::move(item));
  return true;
}

bool FilePairSession::LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                               std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error)
{
  left = TextFile::Load(item.leftPath);
  right = TextFile::Load(item.rightPath);
  leftTitle = item.leftPath;
  rightTitle = item.rightPath;
  if (!left.exists && !left.error.empty())
  {
    error = item.leftPath + L": " + left.error;
    return false;
  }
  if (!right.exists && !right.error.empty())
  {
    error = item.rightPath + L": " + right.error;
    return false;
  }
  return true;
}

// ─── FolderSession ───────────────────────────────────────────────────────────

FolderSession::FolderSession(const std::wstring& left, const std::wstring& right)
  : m_left(left), m_right(right)
{
}

std::wstring FolderSession::GetTitle() const
{
  return FileName(m_left) + L" <-> " + FileName(m_right);
}

std::wstring FolderSession::GetSummary() const
{
  return Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_FOLDER_SUMMARY).c_str(),
    m_summary.different, m_summary.leftOnly, m_summary.rightOnly, m_summary.identical);
}

void FolderSession::Swap()
{
  std::swap(m_left, m_right);
}

bool FolderSession::Load(std::wstring& error)
{
  m_root = CompareItem();
  m_summary = SessionSummary();

  std::error_code ec;
  if (!fs::is_directory(m_left, ec))
  {
    error = Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), m_left.c_str());
    return false;
  }
  if (!fs::is_directory(m_right, ec))
  {
    error = Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_ERR_NOT_FOUND).c_str(), m_right.c_str());
    return false;
  }

  Yaswl::WinApp::WaitCursor wc;
  m_root.isDir = true;
  m_root.leftPath = m_left;
  m_root.rightPath = m_right;
  ScanDir(m_left, m_right, L"", m_root);
  return true;
}

void FolderSession::ScanDir(const fs::path& left, const fs::path& right, const std::wstring& relPath, CompareItem& node)
{
  struct Entry
  {
    std::wstring name;
    bool isDir = false;
    bool left = false;
    bool right = false;
  };
  std::map<std::wstring, Entry> entries;

  auto scan = [&](const fs::path& dir, bool isLeft)
  {
    if (dir.empty())
      return;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
    {
      std::error_code ec2;
      bool isDir = it->is_directory(ec2);
      std::wstring name = it->path().filename().wstring();
      if (isDir && IsIgnoredDir(name))
        continue;
      std::wstring key = (isDir ? L"d:" : L"f:") + ToLower(name);
      Entry& e = entries[key];
      if (e.name.empty())
        e.name = name;
      e.isDir = isDir;
      if (isLeft) e.left = true; else e.right = true;
    }
  };
  scan(left, true);
  scan(right, false);

  std::vector<const Entry*> sorted;
  for (const auto& kv : entries)
    sorted.push_back(&kv.second);
  std::sort(sorted.begin(), sorted.end(), [](const Entry* a, const Entry* b)
  {
    if (a->isDir != b->isDir)
      return a->isDir;
    return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
  });

  for (const Entry* e : sorted)
  {
    auto child = std::make_unique<CompareItem>();
    child->name = e->name;
    child->relPath = relPath.empty() ? e->name : relPath + L"\\" + e->name;
    child->isDir = e->isDir;
    child->leftPath = e->left ? (left / e->name).wstring() : L"";
    child->rightPath = e->right ? (right / e->name).wstring() : L"";

    if (e->isDir)
    {
      ScanDir(e->left ? left / e->name : fs::path(), e->right ? right / e->name : fs::path(), child->relPath, *child);
      if (!e->left)
        child->status = ItemStatus::RightOnly;
      else if (!e->right)
        child->status = ItemStatus::LeftOnly;
      else
      {
        child->status = ItemStatus::Identical;
        for (const auto& c : child->children)
        {
          if (c->status != ItemStatus::Identical)
          {
            child->status = ItemStatus::Different;
            break;
          }
        }
      }
    }
    else
    {
      if (!e->left)
      {
        child->status = ItemStatus::RightOnly;
        m_summary.rightOnly++;
      }
      else if (!e->right)
      {
        child->status = ItemStatus::LeftOnly;
        m_summary.leftOnly++;
      }
      else if (TextFile::FilesEqual(child->leftPath, child->rightPath))
      {
        child->status = ItemStatus::Identical;
        m_summary.identical++;
      }
      else
      {
        child->status = ItemStatus::Different;
        m_summary.different++;
      }
    }
    node.children.push_back(std::move(child));
  }
}

bool FolderSession::LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                             std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error)
{
  const std::wstring missing = Yaswl::GetApp()->LoadString(IDS_MISSING);

  if (!item.leftPath.empty())
  {
    left = TextFile::Load(item.leftPath);
    leftTitle = item.leftPath;
    if (!left.exists)
    {
      error = item.leftPath + L": " + left.error;
      return false;
    }
  }
  else
  {
    left = TextContent();
    leftTitle = (fs::path(m_left) / item.relPath).wstring() + L" " + missing;
  }

  if (!item.rightPath.empty())
  {
    right = TextFile::Load(item.rightPath);
    rightTitle = item.rightPath;
    if (!right.exists)
    {
      error = item.rightPath + L": " + right.error;
      return false;
    }
  }
  else
  {
    right = TextContent();
    rightTitle = (fs::path(m_right) / item.relPath).wstring() + L" " + missing;
  }
  return true;
}

// ─── PullRequestSession ──────────────────────────────────────────────────────

PullRequestSession::PullRequestSession(const std::wstring& url) : m_url(url)
{
}

std::wstring PullRequestSession::GetTitle() const
{
  std::wstring title = m_info.owner + L"/" + m_info.repo + L" #" + std::to_wstring(m_info.number);
  if (!m_info.title.empty())
    title += L": " + m_info.title;
  return title;
}

std::wstring PullRequestSession::GetSummary() const
{
  std::wstring s = Yaswl::StrFormat(Yaswl::GetApp()->LoadString(IDS_PR_SUMMARY).c_str(), static_cast<int>(m_info.files.size()));
  s += L"   " + m_info.baseRef + L" (" + ShortSha(m_info.mergeBaseSha) + L") <- " + m_info.headRef + L" (" + ShortSha(m_info.headSha) + L")";
  return s;
}

bool PullRequestSession::Load(std::wstring& error)
{
  m_root = CompareItem();
  m_summary = SessionSummary();
  m_cache.clear();

  PullRequestInfo info;
  if (!GitHubClient::ParsePullRequestUrl(m_url, info))
  {
    error = Yaswl::GetApp()->LoadString(IDS_ERR_PR_URL);
    return false;
  }
  if (!m_client.LoadPullRequest(info, error))
    return false;
  m_info = std::move(info);

  m_root.isDir = true;
  for (const auto& f : m_info.files)
  {
    std::wstring folder, name = f.path;
    size_t slash = f.path.rfind(L'/');
    if (slash != std::wstring::npos)
    {
      folder = f.path.substr(0, slash);
      name = f.path.substr(slash + 1);
    }

    CompareItem& parent = folder.empty() ? m_root : EnsureFolder(folder);
    auto item = std::make_unique<CompareItem>();
    item->name = name;
    item->relPath = f.path;
    item->prStatus = f.status;
    item->prPreviousPath = f.previousPath;
    item->additions = f.additions;
    item->deletions = f.deletions;
    if (f.status == L"added")
    {
      item->status = ItemStatus::RightOnly;
      m_summary.rightOnly++;
    }
    else if (f.status == L"removed")
    {
      item->status = ItemStatus::LeftOnly;
      m_summary.leftOnly++;
    }
    else if (f.additions == 0 && f.deletions == 0 && f.status != L"modified" && f.status != L"changed")
    {
      item->status = ItemStatus::Identical;   // pure rename / copy / mode change
      m_summary.identical++;
    }
    else
    {
      item->status = ItemStatus::Different;
      m_summary.different++;
    }
    parent.children.push_back(std::move(item));
  }

  // folder status = any child differs
  std::function<ItemStatus(CompareItem&)> finalize = [&](CompareItem& node) -> ItemStatus
  {
    ItemStatus status = ItemStatus::Identical;
    for (auto& c : node.children)
    {
      ItemStatus s = c->isDir ? finalize(*c) : c->status;
      if (s != ItemStatus::Identical)
        status = ItemStatus::Different;
    }
    if (node.isDir)
      node.status = status;
    return status;
  };
  finalize(m_root);
  CompactFolders(m_root);
  return true;
}

CompareItem& PullRequestSession::EnsureFolder(const std::wstring& path)
{
  CompareItem* node = &m_root;
  std::wstring accumulated;
  size_t start = 0;
  while (start <= path.size())
  {
    size_t slash = path.find(L'/', start);
    std::wstring part = path.substr(start, slash == std::wstring::npos ? std::wstring::npos : slash - start);
    accumulated = accumulated.empty() ? part : accumulated + L"/" + part;

    CompareItem* found = nullptr;
    for (auto& c : node->children)
    {
      if (c->isDir && c->name == part)
      {
        found = c.get();
        break;
      }
    }
    if (!found)
    {
      auto folder = std::make_unique<CompareItem>();
      folder->name = part;
      folder->relPath = accumulated;
      folder->isDir = true;
      found = folder.get();
      // keep folders before files
      auto pos = std::find_if(node->children.begin(), node->children.end(), [](const std::unique_ptr<CompareItem>& c) { return !c->isDir; });
      node->children.insert(pos, std::move(folder));
    }
    node = found;
    if (slash == std::wstring::npos)
      break;
    start = slash + 1;
  }
  return *node;
}

bool PullRequestSession::Fetch(const std::wstring& path, const std::wstring& ref, TextContent& content, std::wstring& error)
{
  std::wstring key = ref + L":" + path;
  auto it = m_cache.find(key);
  if (it != m_cache.end())
  {
    content = it->second;
    return true;
  }

  Yaswl::WinApp::WaitCursor wc;
  std::vector<uint8_t> data;
  bool notFound = false;
  if (!m_client.GetContent(m_info, path, ref, data, notFound, error))
  {
    if (!notFound)
      return false;
    content = TextContent();
    content.exists = false;
  }
  else
  {
    content = TextFile::Decode(data.data(), data.size());
  }
  m_cache[key] = content;
  return true;
}

bool PullRequestSession::LoadItem(const CompareItem& item, TextContent& left, TextContent& right,
                                  std::wstring& leftTitle, std::wstring& rightTitle, std::wstring& error)
{
  const std::wstring missing = Yaswl::GetApp()->LoadString(IDS_MISSING);
  const std::wstring leftPath = item.prPreviousPath.empty() ? item.relPath : item.prPreviousPath;

  leftTitle = leftPath;
  rightTitle = item.relPath;

  if (item.prStatus == L"added")
  {
    left = TextContent();
    leftTitle += L" " + missing;
  }
  else if (!Fetch(leftPath, m_info.mergeBaseSha, left, error))
  {
    return false;
  }
  left.source = m_info.baseRef + L" @ " + ShortSha(m_info.mergeBaseSha);

  if (item.prStatus == L"removed")
  {
    right = TextContent();
    rightTitle += L" " + missing;
  }
  else if (!Fetch(item.relPath, m_info.headSha, right, error))
  {
    return false;
  }
  right.source = m_info.headRef + L" @ " + ShortSha(m_info.headSha);
  return true;
}

void PullRequestSession::CompactFolders(CompareItem& node)
{
  for (auto& child : node.children)
  {
    if (!child->isDir)
      continue;
    // GitHub-style: a folder that only contains one folder is shown as "a/b"
    while (child->children.size() == 1 && child->children[0]->isDir)
    {
      std::unique_ptr<CompareItem> only = std::move(child->children[0]);
      child->name += L"/" + only->name;
      child->relPath = only->relPath;
      child->status = only->status;
      child->children = std::move(only->children);
    }
    CompactFolders(*child);
  }
}
