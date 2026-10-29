#include "pch.h"
#include "Json.h"

const JsonValue* JsonValue::Find(const wchar_t* key) const
{
  if (type != Type::Object)
    return nullptr;
  for (size_t i = 0; i < keys.size(); ++i)
  {
    if (keys[i] == key)
      return &values[i];
  }
  return nullptr;
}

std::wstring JsonValue::Str(const wchar_t* key, const wchar_t* def) const
{
  const JsonValue* v = Find(key);
  if (v && v->type == Type::String)
    return v->string;
  return def;
}

double JsonValue::Num(const wchar_t* key, double def) const
{
  const JsonValue* v = Find(key);
  if (v && v->type == Type::Number)
    return v->number;
  return def;
}

namespace
{
  class Parser
  {
  public:
    Parser(const std::string& text) : m_p(text.data()), m_end(text.data() + text.size()) {}

    bool Parse(JsonValue& out, std::wstring& error)
    {
      try
      {
        SkipWs();
        ParseValue(out);
        SkipWs();
        if (m_p != m_end)
          Fail("unexpected trailing characters");
        return true;
      }
      catch (const std::runtime_error& ex)
      {
        error = Yaswl::a2w(ex.what());
        return false;
      }
    }

  private:
    [[noreturn]] void Fail(const char* what)
    {
      throw std::runtime_error(std::string("JSON parse error: ") + what);
    }

    void SkipWs()
    {
      while (m_p != m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r'))
        ++m_p;
    }

    bool Match(const char* lit)
    {
      size_t n = strlen(lit);
      if (static_cast<size_t>(m_end - m_p) >= n && memcmp(m_p, lit, n) == 0)
      {
        m_p += n;
        return true;
      }
      return false;
    }

    void ParseValue(JsonValue& v)
    {
      if (m_p == m_end)
        Fail("unexpected end of input");

      char c = *m_p;
      if (c == '{')
      {
        ParseObject(v);
      }
      else if (c == '[')
      {
        ParseArray(v);
      }
      else if (c == '"')
      {
        v.type = JsonValue::Type::String;
        ParseString(v.string);
      }
      else if (Match("true"))
      {
        v.type = JsonValue::Type::Bool;
        v.boolean = true;
      }
      else if (Match("false"))
      {
        v.type = JsonValue::Type::Bool;
        v.boolean = false;
      }
      else if (Match("null"))
      {
        v.type = JsonValue::Type::Null;
      }
      else if (c == '-' || (c >= '0' && c <= '9'))
      {
        ParseNumber(v);
      }
      else
      {
        Fail("unexpected character");
      }
    }

    void ParseObject(JsonValue& v)
    {
      v.type = JsonValue::Type::Object;
      ++m_p; // {
      SkipWs();
      if (m_p != m_end && *m_p == '}')
      {
        ++m_p;
        return;
      }
      while (true)
      {
        SkipWs();
        if (m_p == m_end || *m_p != '"')
          Fail("expected member name");
        std::wstring key;
        ParseString(key);
        SkipWs();
        if (m_p == m_end || *m_p != ':')
          Fail("expected ':'");
        ++m_p;
        SkipWs();
        JsonValue member;
        ParseValue(member);
        v.keys.push_back(std::move(key));
        v.values.push_back(std::move(member));
        SkipWs();
        if (m_p == m_end)
          Fail("unterminated object");
        if (*m_p == ',')
        {
          ++m_p;
          continue;
        }
        if (*m_p == '}')
        {
          ++m_p;
          return;
        }
        Fail("expected ',' or '}'");
      }
    }

    void ParseArray(JsonValue& v)
    {
      v.type = JsonValue::Type::Array;
      ++m_p; // [
      SkipWs();
      if (m_p != m_end && *m_p == ']')
      {
        ++m_p;
        return;
      }
      while (true)
      {
        SkipWs();
        JsonValue item;
        ParseValue(item);
        v.array.push_back(std::move(item));
        SkipWs();
        if (m_p == m_end)
          Fail("unterminated array");
        if (*m_p == ',')
        {
          ++m_p;
          continue;
        }
        if (*m_p == ']')
        {
          ++m_p;
          return;
        }
        Fail("expected ',' or ']'");
      }
    }

    unsigned ParseHex4()
    {
      if (m_end - m_p < 4)
        Fail("bad \\u escape");
      unsigned v = 0;
      for (int i = 0; i < 4; ++i)
      {
        char c = *m_p++;
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else Fail("bad \\u escape");
      }
      return v;
    }

    static void AppendUtf8(std::string& out, unsigned cp)
    {
      if (cp < 0x80)
      {
        out += static_cast<char>(cp);
      }
      else if (cp < 0x800)
      {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
      }
      else if (cp < 0x10000)
      {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
      }
      else
      {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
      }
    }

    void ParseString(std::wstring& out)
    {
      ++m_p; // opening quote
      std::string bytes;
      while (true)
      {
        if (m_p == m_end)
          Fail("unterminated string");
        char c = *m_p++;
        if (c == '"')
          break;
        if (c != '\\')
        {
          bytes += c;
          continue;
        }
        if (m_p == m_end)
          Fail("unterminated escape");
        char e = *m_p++;
        switch (e)
        {
        case '"':  bytes += '"'; break;
        case '\\': bytes += '\\'; break;
        case '/':  bytes += '/'; break;
        case 'b':  bytes += '\b'; break;
        case 'f':  bytes += '\f'; break;
        case 'n':  bytes += '\n'; break;
        case 'r':  bytes += '\r'; break;
        case 't':  bytes += '\t'; break;
        case 'u':
        {
          unsigned cp = ParseHex4();
          if (cp >= 0xD800 && cp <= 0xDBFF && m_end - m_p >= 6 && m_p[0] == '\\' && m_p[1] == 'u')
          {
            m_p += 2;
            unsigned lo = ParseHex4();
            if (lo >= 0xDC00 && lo <= 0xDFFF)
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            else
              cp = 0xFFFD;
          }
          AppendUtf8(bytes, cp);
          break;
        }
        default:
          Fail("bad escape");
        }
      }
      out = Yaswl::a2w(bytes);
    }

    void ParseNumber(JsonValue& v)
    {
      const char* start = m_p;
      if (*m_p == '-') ++m_p;
      while (m_p != m_end && ((*m_p >= '0' && *m_p <= '9') || *m_p == '.' || *m_p == 'e' || *m_p == 'E' || *m_p == '+' || *m_p == '-'))
        ++m_p;
      std::string s(start, m_p);
      v.type = JsonValue::Type::Number;
      v.number = atof(s.c_str());
    }

  private:
    const char* m_p;
    const char* m_end;
  };
}

namespace Json
{
  bool Parse(const std::string& utf8, JsonValue& out, std::wstring& error)
  {
    out = JsonValue();
    Parser parser(utf8);
    return parser.Parse(out, error);
  }
}
