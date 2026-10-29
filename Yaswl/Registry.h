#pragma once

#include <vector>
#include <string>

namespace Yaswl
{

template<class _CharType>
class Registry
{
public:
  typedef std::basic_string<_CharType> tstring;
  typedef _CharType tchar;

public:
  static unsigned long Value(const tstring& key, const tstring& name, HKEY base_key = HKEY_CURRENT_USER)
  {
    unsigned long def;
    Value(key, name, def, base_key);
    return def;
  }

  static bool Value(const tstring& key, const tstring& name, unsigned long& val, HKEY base_key = HKEY_CURRENT_USER)
  {
    std::vector<unsigned char> buf;

    if (Value(key, name, buf, base_key) && buf.size() == sizeof(val))
    {
      val = *(unsigned long*)buf.data();
      return true;
    }

    return false;
  }

  static bool Value(const tstring& key, const tstring& name, tstring& str, HKEY base_key = HKEY_CURRENT_USER)
  {
    std::vector<unsigned char> buf;

    if (Value(key, name, buf, base_key))
    {
      str = reinterpret_cast<const tchar*>(buf.data());
      return true;
    }

    return false;
  }

  static bool Value(const tstring& key, const tstring& name, std::vector<tstring>& multistr, HKEY base_key = HKEY_CURRENT_USER)
  {
    std::vector<unsigned char> buf;

    if (Value(key, name, buf, base_key))
    {

      for (size_t i = 0; i < buf.size(); i += sizeof(tchar))
      {
        if (buf[i])
        {
          tstring str;
          str = reinterpret_cast<const tchar*>(buf.data() + i);
          i += str.size() * sizeof(tchar);
          multistr.emplace_back(str);
        }
      }

      return true;
    }

    return false;
  }

  static bool Value(const tstring& key, const tstring& name, std::vector<unsigned char>& buf, HKEY base_key = HKEY_CURRENT_USER)
  {
    HKEY hkey;

    REGSAM samDesired = KEY_READ;
    if (IsWow64Process())
      samDesired |= KEY_WOW64_64KEY;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(base_key, key.c_str(), 0, samDesired, &hkey))
    {
      Yaswl::unique_regkey skey = Yaswl::make_unique_regkey(hkey);

      unsigned long size = 0;
      if (ERROR_SUCCESS == ::RegQueryValueEx(hkey, name.c_str(), nullptr, nullptr, nullptr, &size))
      {
        buf.resize(size);
        if (ERROR_SUCCESS == ::RegQueryValueEx(hkey, name.c_str(), nullptr, nullptr, buf.data(), &size))
          return true;
      }
    }

    return false;
  }

  static bool Exist(const tstring& key, HKEY base_key = HKEY_CURRENT_USER)
  {
    HKEY hkey;

    REGSAM samDesired = KEY_READ;
    if (IsWow64Process())
      samDesired |= KEY_WOW64_64KEY;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(base_key, key.c_str(), 0, samDesired, &hkey))
    {
      ::RegCloseKey(hkey);
      return true;
    }

    return false;
  }

  static bool SetValue(const tstring& key, const tstring& name, unsigned long val, unsigned long type = REG_DWORD, HKEY base_key = HKEY_CURRENT_USER)
  {
    std::vector<unsigned char> buf((unsigned char*)&val, (unsigned char*)&val + sizeof(val));
    return SetValue(key, name, buf, type, base_key);
  }

  static bool SetValue(const tstring& key, const tstring& name, const tstring& str, unsigned long type = REG_SZ, HKEY base_key = HKEY_CURRENT_USER)
  {
    std::vector<unsigned char> buf((unsigned char*)str.c_str(), (unsigned char*)(str.c_str() + str.size()));

    for (int i = 0; i < sizeof(tchar); i++)
      buf.push_back(0); //If the data is of type REG_SZ, REG_EXPAND_SZ, or REG_MULTI_SZ, data size must include the size of the terminating null character or characters.

    return SetValue(key, name, buf, type, base_key);
  }

  static bool SetValue(const tstring& key, const tstring& name, const std::vector<unsigned char>& buf, unsigned long type = REG_BINARY, HKEY base_key = HKEY_CURRENT_USER)
  {
    HKEY hkey;

    REGSAM samDesired = KEY_WRITE;
    if (IsWow64Process())
      samDesired |= KEY_WOW64_64KEY;

    if (ERROR_SUCCESS == ::RegCreateKeyEx(base_key, key.c_str(), 0, nullptr, 0, samDesired, nullptr, &hkey, nullptr))
    {
      Yaswl::unique_regkey skey = Yaswl::make_unique_regkey(hkey);
      unsigned long size = 0;
      if (ERROR_SUCCESS == ::RegSetValueEx(hkey, name.c_str(), 0, type, buf.data(), static_cast<DWORD>(buf.size())))
        return true;
    }

    return false;
  }
};

typedef Registry<char> RegistryA;
typedef Registry<wchar_t> RegistryW;

}
