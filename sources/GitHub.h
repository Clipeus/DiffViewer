#pragma once

class JsonValue;

struct PullRequestFile
{
  std::wstring path;
  std::wstring previousPath;   // for renamed files
  std::wstring status;         // added, removed, modified, renamed, copied, changed, unchanged
  int additions = 0;
  int deletions = 0;
};

struct PullRequestInfo
{
  std::wstring host;           // github.com or a GitHub Enterprise host
  std::wstring owner;
  std::wstring repo;
  int number = 0;

  std::wstring title;
  std::wstring htmlUrl;
  std::wstring baseRef;
  std::wstring headRef;
  std::wstring baseSha;
  std::wstring headSha;
  std::wstring mergeBaseSha;
  std::vector<PullRequestFile> files;
};

class GitHubClient
{
public:
  // Accepts https://github.com/owner/repo/pull/123[/files...], github.com/owner/repo/pull/123,
  // owner/repo/pull/123 and owner/repo#123.
  static bool ParsePullRequestUrl(const std::wstring& url, PullRequestInfo& info);

  bool LoadPullRequest(PullRequestInfo& info, std::wstring& error);

  // Fetches the raw content of a file at the given ref. notFound is set when the file does not exist at that ref.
  bool GetContent(const PullRequestInfo& info, const std::wstring& path, const std::wstring& ref,
                  std::vector<uint8_t>& data, bool& notFound, std::wstring& error);

private:
  std::wstring ApiHost(const std::wstring& host) const;
  std::wstring ApiPrefix(const std::wstring& host) const;
  std::wstring GetToken(const std::wstring& host);
  bool ApiGet(const std::wstring& host, const std::wstring& path, const wchar_t* accept,
              std::vector<uint8_t>& body, DWORD& status, std::wstring& error);
  bool ApiGetJson(const std::wstring& host, const std::wstring& path, JsonValue& json, std::wstring& error);

  std::map<std::wstring, std::wstring> m_tokens;
};
