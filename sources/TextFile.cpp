#include "pch.h"
#include "TextFile.h"

namespace
{
  uint64_t HashBytes(const uint8_t* data, size_t size)
  {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < size; ++i)
    {
      h ^= data[i];
      h *= 1099511628211ULL;
    }
    return h;
  }

  bool LooksBinary(const uint8_t* data, size_t size)
  {
    const size_t probe = std::min<size_t>(size, 64 * 1024);
    for (size_t i = 0; i < probe; ++i)
    {
      if (data[i] == 0)
        return true;
    }
    return false;
  }

  bool ConvertToUnicode(UINT codePage, DWORD flags, const char* data, int size, std::wstring& out)
  {
    out.clear();
    if (size == 0)
      return true;
    int n = ::MultiByteToWideChar(codePage, flags, data, size, nullptr, 0);
    if (n <= 0)
      return false;
    out.resize(n);
    n = ::MultiByteToWideChar(codePage, flags, data, size, &out[0], n);
    if (n <= 0)
    {
      out.clear();
      return false;
    }
    out.resize(n);
    return true;
  }
}

namespace TextFile
{
  bool ReadAllBytes(const std::wstring& path, std::vector<uint8_t>& data, std::wstring& error)
  {
    data.clear();
    Yaswl::unique_handle hFile = Yaswl::make_unique_handle(
      ::CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (hFile.get() == INVALID_HANDLE_VALUE)
    {
      error = Yaswl::GetSystemErrorMessage();
      hFile.release();
      return false;
    }

    LARGE_INTEGER li = {};
    if (!::GetFileSizeEx(hFile.get(), &li))
    {
      error = Yaswl::GetSystemErrorMessage();
      return false;
    }
    if (li.QuadPart > 512LL * 1024 * 1024)
    {
      error = L"The file is too large (limit 512 MB).";
      return false;
    }

    data.resize(static_cast<size_t>(li.QuadPart));
    size_t done = 0;
    while (done < data.size())
    {
      DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - done, 1 << 20));
      DWORD read = 0;
      if (!::ReadFile(hFile.get(), data.data() + done, chunk, &read, nullptr))
      {
        error = Yaswl::GetSystemErrorMessage();
        return false;
      }
      if (read == 0)
        break;
      done += read;
    }
    data.resize(done);
    return true;
  }

  TextContent Decode(const uint8_t* data, size_t size)
  {
    TextContent tc;
    tc.exists = true;
    tc.size = size;

    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE)
    {
      tc.encoding = L"UTF-16 LE";
      tc.text.assign(reinterpret_cast<const wchar_t*>(data + 2), (size - 2) / 2);
      return tc;
    }
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF)
    {
      tc.encoding = L"UTF-16 BE";
      size_t n = (size - 2) / 2;
      tc.text.resize(n);
      for (size_t i = 0; i < n; ++i)
        tc.text[i] = static_cast<wchar_t>((data[2 + 2 * i] << 8) | data[3 + 2 * i]);
      return tc;
    }

    size_t offset = 0;
    bool bom = false;
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
    {
      offset = 3;
      bom = true;
    }

    if (LooksBinary(data + offset, size - offset))
    {
      tc.binary = true;
      tc.encoding = L"Binary";
      tc.text = Yaswl::StrFormat(L"<binary file, %llu bytes, hash %016llX>",
        static_cast<unsigned long long>(size), static_cast<unsigned long long>(HashBytes(data, size)));
      return tc;
    }

    const char* p = reinterpret_cast<const char*>(data + offset);
    const int n = static_cast<int>(size - offset);
    if (ConvertToUnicode(CP_UTF8, MB_ERR_INVALID_CHARS, p, n, tc.text))
    {
      tc.encoding = bom ? L"UTF-8 BOM" : L"UTF-8";
    }
    else
    {
      ConvertToUnicode(CP_ACP, 0, p, n, tc.text);
      tc.encoding = Yaswl::StrFormat(L"ANSI (cp %u)", ::GetACP());
    }
    return tc;
  }

  TextContent Load(const std::wstring& path)
  {
    TextContent tc;
    std::vector<uint8_t> data;
    if (!ReadAllBytes(path, data, tc.error))
    {
      tc.exists = false;
      return tc;
    }
    return Decode(data.data(), data.size());
  }

  bool FilesEqual(const std::wstring& a, const std::wstring& b)
  {
    std::error_code ec;
    auto sa = std::filesystem::file_size(a, ec);
    if (ec) return false;
    auto sb = std::filesystem::file_size(b, ec);
    if (ec) return false;
    if (sa != sb)
      return false;

    std::ifstream fa(std::filesystem::path(a), std::ios::binary);
    std::ifstream fb(std::filesystem::path(b), std::ios::binary);
    if (!fa || !fb)
      return false;

    std::vector<char> ba(64 * 1024), bb(64 * 1024);
    while (fa && fb)
    {
      fa.read(ba.data(), ba.size());
      fb.read(bb.data(), bb.size());
      std::streamsize na = fa.gcount();
      std::streamsize nb = fb.gcount();
      if (na != nb)
        return false;
      if (na == 0)
        break;
      if (memcmp(ba.data(), bb.data(), static_cast<size_t>(na)) != 0)
        return false;
    }
    return true;
  }
}
