#include "pch.h"
#include "Json.h"
#include "GitHub.h"

namespace
{
  UNIQUE_VALUE(hinternet, HINTERNET, ::WinHttpCloseHandle)

  std::wstring Trim(const std::wstring& s)
  {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
  }

  std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep)
  {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (true)
    {
      size_t pos = s.find(sep, start);
      if (pos == std::wstring::npos)
      {
        parts.push_back(s.substr(start));
        break;
      }
      parts.push_back(s.substr(start, pos - start));
      start = pos + 1;
    }
    return parts;
  }

  std::wstring UrlEncodePath(const std::wstring& path)
  {
    std::string utf8 = Yaswl::w2a(path);
    std::wstring out;
    static const wchar_t* hex = L"0123456789ABCDEF";
    for (unsigned char c : utf8)
    {
      if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/')
      {
        out += static_cast<wchar_t>(c);
      }
      else
      {
        out += L'%';
        out += hex[c >> 4];
        out += hex[c & 15];
      }
    }
    return out;
  }

  std::wstring GetEnv(const wchar_t* name)
  {
    wchar_t buf[4096];
    DWORD n = ::GetEnvironmentVariable(name, buf, static_cast<DWORD>(std::size(buf)));
    if (n == 0 || n >= std::size(buf))
      return L"";
    return buf;
  }

  // Runs a console command without showing a window and captures its stdout.
  bool RunCommandCapture(const std::wstring& commandLine, std::string& output, DWORD& exitCode)
  {
    output.clear();
    exitCode = static_cast<DWORD>(-1);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!::CreatePipe(&hRead, &hWrite, &sa, 0))
      return false;
    ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    std::wstring cmd = commandLine;
    BOOL ok = ::CreateProcess(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    ::CloseHandle(hWrite);
    if (!ok)
    {
      ::CloseHandle(hRead);
      return false;
    }

    char buf[4096];
    DWORD read = 0;
    while (::ReadFile(hRead, buf, sizeof(buf), &read, nullptr) && read > 0)
      output.append(buf, read);
    ::CloseHandle(hRead);

    ::WaitForSingleObject(pi.hProcess, 15000);
    ::GetExitCodeProcess(pi.hProcess, &exitCode);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
  }
}

bool GitHubClient::ParsePullRequestUrl(const std::wstring& input, PullRequestInfo& info)
{
  std::wstring url = Trim(input);
  if (url.empty())
    return false;

  // strip scheme
  size_t scheme = url.find(L"://");
  if (scheme != std::wstring::npos)
    url = url.substr(scheme + 3);

  // strip query / fragment
  size_t cut = url.find_first_of(L"?#");
  std::wstring fragment;
  if (cut != std::wstring::npos)
  {
    fragment = url.substr(cut);
    url = url.substr(0, cut);
  }

  std::vector<std::wstring> parts;
  for (const auto& p : Split(url, L'/'))
  {
    if (!p.empty())
      parts.push_back(p);
  }
  if (parts.empty())
    return false;

  std::wstring host = L"github.com";
  size_t i = 0;
  if (parts[0].find(L'.') != std::wstring::npos && parts[0] != L"." && parts[0] != L"..")
  {
    host = parts[0];
    i = 1;
  }

  // owner/repo#123
  if (parts.size() == i + 2 && !fragment.empty() && fragment[0] == L'#')
  {
    info.host = host;
    info.owner = parts[i];
    info.repo = parts[i + 1];
    info.number = _wtoi(fragment.c_str() + 1);
    return info.number > 0;
  }

  if (parts.size() < i + 4)
    return false;
  if (_wcsicmp(parts[i + 2].c_str(), L"pull") != 0 && _wcsicmp(parts[i + 2].c_str(), L"pulls") != 0)
    return false;

  info.host = host;
  info.owner = parts[i];
  info.repo = parts[i + 1];
  info.number = _wtoi(parts[i + 3].c_str());
  return info.number > 0 && !info.owner.empty() && !info.repo.empty();
}

std::wstring GitHubClient::ApiHost(const std::wstring& host) const
{
  if (_wcsicmp(host.c_str(), L"github.com") == 0 || _wcsicmp(host.c_str(), L"www.github.com") == 0)
    return L"api.github.com";
  return host;
}

std::wstring GitHubClient::ApiPrefix(const std::wstring& host) const
{
  if (_wcsicmp(host.c_str(), L"github.com") == 0 || _wcsicmp(host.c_str(), L"www.github.com") == 0)
    return L"";
  return L"/api/v3";
}

std::wstring GitHubClient::GetToken(const std::wstring& host)
{
  auto it = m_tokens.find(host);
  if (it != m_tokens.end())
    return it->second;

  std::wstring token = GetEnv(L"GITHUB_TOKEN");
  if (token.empty())
    token = GetEnv(L"GH_TOKEN");

  if (token.empty())
  {
    std::string out;
    DWORD exitCode = 0;
    if (RunCommandCapture(L"gh auth token --hostname " + host, out, exitCode) && exitCode == 0)
      token = Trim(Yaswl::a2w(out));
  }

  m_tokens[host] = token;
  return token;
}

bool GitHubClient::ApiGet(const std::wstring& host, const std::wstring& path, const wchar_t* accept,
                          std::vector<uint8_t>& body, DWORD& status, std::wstring& error)
{
  body.clear();
  status = 0;

  unique_hinternet session = make_unique_hinternet(::WinHttpOpen(L"DiffViewer/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session)
  {
    error = L"WinHttpOpen failed: " + Yaswl::GetSystemErrorMessage();
    return false;
  }
  ::WinHttpSetTimeouts(session.get(), 15000, 15000, 30000, 60000);

  unique_hinternet connect = make_unique_hinternet(::WinHttpConnect(session.get(), ApiHost(host).c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (!connect)
  {
    error = L"WinHttpConnect failed: " + Yaswl::GetSystemErrorMessage();
    return false;
  }

  std::wstring fullPath = ApiPrefix(host) + path;
  unique_hinternet request = make_unique_hinternet(::WinHttpOpenRequest(connect.get(), L"GET", fullPath.c_str(), nullptr,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!request)
  {
    error = L"WinHttpOpenRequest failed: " + Yaswl::GetSystemErrorMessage();
    return false;
  }

  std::wstring headers = L"Accept: ";
  headers += accept;
  headers += L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
  std::wstring token = GetToken(host);
  if (!token.empty())
    headers += L"Authorization: Bearer " + token + L"\r\n";

  if (!::WinHttpSendRequest(request.get(), headers.c_str(), static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
  {
    error = L"Request to " + ApiHost(host) + L" failed: " + Yaswl::GetSystemErrorMessage();
    return false;
  }
  if (!::WinHttpReceiveResponse(request.get(), nullptr))
  {
    error = L"No response from " + ApiHost(host) + L": " + Yaswl::GetSystemErrorMessage();
    return false;
  }

  DWORD size = sizeof(status);
  ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
    WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);

  while (true)
  {
    DWORD avail = 0;
    if (!::WinHttpQueryDataAvailable(request.get(), &avail))
    {
      error = L"Read failed: " + Yaswl::GetSystemErrorMessage();
      return false;
    }
    if (avail == 0)
      break;
    size_t old = body.size();
    body.resize(old + avail);
    DWORD read = 0;
    if (!::WinHttpReadData(request.get(), body.data() + old, avail, &read))
    {
      error = L"Read failed: " + Yaswl::GetSystemErrorMessage();
      return false;
    }
    body.resize(old + read);
  }

  if (status < 200 || status >= 300)
  {
    std::wstring message;
    JsonValue json;
    std::wstring jsonError;
    if (Json::Parse(std::string(body.begin(), body.end()), json, jsonError))
      message = json.Str(L"message");
    error = Yaswl::StrFormat(L"HTTP %u for %s%s", status, fullPath.c_str(), message.empty() ? L"" : (L": " + message).c_str());
    if (status == 401 || status == 403 || status == 404)
    {
      if (token.empty())
        error += L"\n\nNo GitHub token was found. Set GITHUB_TOKEN or sign in with 'gh auth login' to access private repositories.";
    }
    return false;
  }
  return true;
}

bool GitHubClient::ApiGetJson(const std::wstring& host, const std::wstring& path, JsonValue& json, std::wstring& error)
{
  std::vector<uint8_t> body;
  DWORD status = 0;
  if (!ApiGet(host, path, L"application/vnd.github+json", body, status, error))
    return false;
  return Json::Parse(std::string(body.begin(), body.end()), json, error);
}

bool GitHubClient::LoadPullRequest(PullRequestInfo& info, std::wstring& error)
{
  Yaswl::WinApp::WaitCursor wc;

  const std::wstring repoPath = L"/repos/" + info.owner + L"/" + info.repo;

  JsonValue pr;
  if (!ApiGetJson(info.host, repoPath + L"/pulls/" + std::to_wstring(info.number), pr, error))
    return false;

  info.title = pr.Str(L"title");
  info.htmlUrl = pr.Str(L"html_url");
  if (const JsonValue* base = pr.Find(L"base"))
  {
    info.baseRef = base->Str(L"ref");
    info.baseSha = base->Str(L"sha");
  }
  if (const JsonValue* head = pr.Find(L"head"))
  {
    info.headRef = head->Str(L"ref");
    info.headSha = head->Str(L"sha");
  }
  if (info.baseSha.empty() || info.headSha.empty())
  {
    error = L"The pull request response does not contain base/head commits.";
    return false;
  }

  // The PR file list is relative to the merge base, not to the tip of the base branch.
  info.mergeBaseSha = info.baseSha;
  {
    JsonValue cmp;
    std::wstring cmpError;
    if (ApiGetJson(info.host, repoPath + L"/compare/" + info.baseSha + L"..." + info.headSha + L"?per_page=1", cmp, cmpError))
    {
      if (const JsonValue* mb = cmp.Find(L"merge_base_commit"))
      {
        std::wstring sha = mb->Str(L"sha");
        if (!sha.empty())
          info.mergeBaseSha = sha;
      }
    }
  }

  info.files.clear();
  for (int page = 1; page < 100; ++page)
  {
    JsonValue files;
    if (!ApiGetJson(info.host, repoPath + L"/pulls/" + std::to_wstring(info.number) + L"/files?per_page=100&page=" + std::to_wstring(page), files, error))
      return false;
    if (!files.IsArray())
    {
      error = L"Unexpected response for the pull request file list.";
      return false;
    }
    for (const auto& f : files.array)
    {
      PullRequestFile file;
      file.path = f.Str(L"filename");
      file.previousPath = f.Str(L"previous_filename");
      file.status = f.Str(L"status");
      file.additions = static_cast<int>(f.Num(L"additions"));
      file.deletions = static_cast<int>(f.Num(L"deletions"));
      if (!file.path.empty())
        info.files.push_back(std::move(file));
    }
    if (files.array.size() < 100)
      break;
  }
  return true;
}

bool GitHubClient::GetContent(const PullRequestInfo& info, const std::wstring& path, const std::wstring& ref,
                              std::vector<uint8_t>& data, bool& notFound, std::wstring& error)
{
  notFound = false;
  DWORD status = 0;
  std::wstring apiPath = L"/repos/" + info.owner + L"/" + info.repo + L"/contents/" + UrlEncodePath(path) + L"?ref=" + ref;
  if (ApiGet(info.host, apiPath, L"application/vnd.github.raw+json", data, status, error))
    return true;
  if (status == 404)
    notFound = true;
  return false;
}
