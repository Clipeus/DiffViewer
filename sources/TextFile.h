#pragma once

struct TextContent
{
  bool exists = false;
  bool binary = false;
  uint64_t size = 0;
  std::wstring text;       // decoded text (for binary files: a one-line description)
  std::wstring encoding;   // "UTF-8", "UTF-8 BOM", "UTF-16 LE", "UTF-16 BE", "ANSI", "Binary"
  std::wstring source;     // optional origin shown in the pane header, e.g. "master @ f0d1b15"
  std::wstring error;
};

namespace TextFile
{
  bool ReadAllBytes(const std::wstring& path, std::vector<uint8_t>& data, std::wstring& error);
  TextContent Decode(const uint8_t* data, size_t size);
  TextContent Load(const std::wstring& path);
  bool FilesEqual(const std::wstring& a, const std::wstring& b);
}
