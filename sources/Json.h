#pragma once

// Minimal JSON reader, enough for the GitHub REST API responses.

class JsonValue
{
public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Type type = Type::Null;
  bool boolean = false;
  double number = 0;
  std::wstring string;
  std::vector<JsonValue> array;
  std::vector<std::wstring> keys;      // object member names
  std::vector<JsonValue> values;       // object member values (parallel to keys)

  bool IsNull() const { return type == Type::Null; }
  bool IsObject() const { return type == Type::Object; }
  bool IsArray() const { return type == Type::Array; }

  const JsonValue* Find(const wchar_t* key) const;
  std::wstring Str(const wchar_t* key, const wchar_t* def = L"") const;
  double Num(const wchar_t* key, double def = 0) const;
};

namespace Json
{
  bool Parse(const std::string& utf8, JsonValue& out, std::wstring& error);
}
