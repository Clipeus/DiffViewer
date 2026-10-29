#include "pch.h"
#include "DiffEngine.h"

namespace
{
  uint64_t HashString(const std::wstring& s)
  {
    uint64_t h = 1469598103934665603ULL;
    for (wchar_t c : s)
    {
      h ^= static_cast<uint64_t>(c);
      h *= 1099511628211ULL;
    }
    return h;
  }

  std::wstring NormalizeWhitespace(const std::wstring& s)
  {
    std::wstring r;
    r.reserve(s.size());
    bool pendingSpace = false;
    for (wchar_t c : s)
    {
      if (iswspace(c))
      {
        pendingSpace = true;
      }
      else
      {
        if (pendingSpace && !r.empty())
          r += L' ';
        pendingSpace = false;
        r += c;
      }
    }
    return r;
  }

  struct Token
  {
    int start;
    int end;
    int cls;  // 0 = word, 1 = whitespace, 2 = punctuation
  };

  int CharClass(wchar_t c)
  {
    if (iswalnum(c) || c == L'_')
      return 0;
    if (iswspace(c))
      return 1;
    return 2;
  }

  void Tokenize(const std::wstring& s, std::vector<Token>& tokens)
  {
    tokens.clear();
    const int n = static_cast<int>(s.size());
    int i = 0;
    while (i < n)
    {
      int cls = CharClass(s[i]);
      int j = i + 1;
      if (cls != 2)
      {
        while (j < n && CharClass(s[j]) == cls)
          ++j;
      }
      tokens.push_back({ i, j, cls });
      i = j;
    }
  }

  void AddRange(std::vector<TextRange>& ranges, int start, int end)
  {
    if (end <= start)
      return;
    if (!ranges.empty() && ranges.back().end == start)
      ranges.back().end = end;
    else
      ranges.push_back({ start, end });
  }

  void IntraLineDiff(const std::wstring& l, const std::wstring& r, bool ignoreWhitespace,
                     std::vector<TextRange>& lr, std::vector<TextRange>& rr)
  {
    lr.clear();
    rr.clear();

    constexpr size_t MAX_INTRALINE = 4000;
    if (l.size() > MAX_INTRALINE || r.size() > MAX_INTRALINE)
    {
      AddRange(lr, 0, static_cast<int>(l.size()));
      AddRange(rr, 0, static_cast<int>(r.size()));
      return;
    }

    std::vector<Token> tl, tr;
    Tokenize(l, tl);
    Tokenize(r, tr);

    auto eq = [&](int i, int j)
    {
      const Token& a = tl[i];
      const Token& b = tr[j];
      if (ignoreWhitespace && a.cls == 1 && b.cls == 1)
        return true;
      int la = a.end - a.start;
      int lb = b.end - b.start;
      if (la != lb)
        return false;
      return wmemcmp(l.data() + a.start, r.data() + b.start, la) == 0;
    };

    std::vector<DiffEngine::Edit> edits;
    DiffEngine::Diff(static_cast<int>(tl.size()), static_cast<int>(tr.size()), eq, edits);

    for (const auto& e : edits)
    {
      if (e.type == DiffEngine::Edit::Delete)
        AddRange(lr, tl[e.a].start, tl[e.a + e.len - 1].end);
      else if (e.type == DiffEngine::Edit::Insert)
        AddRange(rr, tr[e.b].start, tr[e.b + e.len - 1].end);
    }
  }
}

namespace DiffEngine
{
  void BuildDocument(DiffDocument& doc, bool ignoreWhitespace)
  {
    const auto& L = doc.leftLines;
    const auto& R = doc.rightLines;

    std::vector<std::wstring> keysL, keysR;
    const std::vector<std::wstring>* pKeysL = &L;
    const std::vector<std::wstring>* pKeysR = &R;
    if (ignoreWhitespace)
    {
      keysL.reserve(L.size());
      keysR.reserve(R.size());
      for (const auto& s : L) keysL.push_back(NormalizeWhitespace(s));
      for (const auto& s : R) keysR.push_back(NormalizeWhitespace(s));
      pKeysL = &keysL;
      pKeysR = &keysR;
    }

    std::vector<uint64_t> hashL(pKeysL->size()), hashR(pKeysR->size());
    for (size_t i = 0; i < pKeysL->size(); ++i) hashL[i] = HashString((*pKeysL)[i]);
    for (size_t i = 0; i < pKeysR->size(); ++i) hashR[i] = HashString((*pKeysR)[i]);

    auto eq = [&](int i, int j)
    {
      return hashL[i] == hashR[j] && (*pKeysL)[i] == (*pKeysR)[j];
    };

    std::vector<Edit> edits;
    Diff(static_cast<int>(L.size()), static_cast<int>(R.size()), eq, edits);

    doc.rows.clear();
    doc.blocks.clear();
    doc.rows.reserve(std::max(L.size(), R.size()));

    std::vector<int> pendingDel, pendingIns;

    auto flush = [&]()
    {
      if (pendingDel.empty() && pendingIns.empty())
        return;

      DiffBlock block;
      block.firstRow = static_cast<int>(doc.rows.size());
      const int blockIndex = static_cast<int>(doc.blocks.size());

      const size_t paired = std::min(pendingDel.size(), pendingIns.size());
      for (size_t k = 0; k < paired; ++k)
      {
        DiffRow row;
        row.kind = RowKind::Changed;
        row.left = pendingDel[k];
        row.right = pendingIns[k];
        row.block = blockIndex;
        IntraLineDiff(L[row.left], R[row.right], ignoreWhitespace, row.leftRanges, row.rightRanges);
        doc.rows.push_back(std::move(row));
      }
      for (size_t k = paired; k < pendingDel.size(); ++k)
      {
        DiffRow row;
        row.kind = RowKind::Removed;
        row.left = pendingDel[k];
        row.block = blockIndex;
        doc.rows.push_back(std::move(row));
      }
      for (size_t k = paired; k < pendingIns.size(); ++k)
      {
        DiffRow row;
        row.kind = RowKind::Added;
        row.right = pendingIns[k];
        row.block = blockIndex;
        doc.rows.push_back(std::move(row));
      }

      block.lastRow = static_cast<int>(doc.rows.size()) - 1;
      if (paired > 0)
        block.kind = RowKind::Changed;
      else if (!pendingDel.empty())
        block.kind = RowKind::Removed;
      else
        block.kind = RowKind::Added;
      doc.blocks.push_back(block);

      pendingDel.clear();
      pendingIns.clear();
    };

    for (const auto& e : edits)
    {
      switch (e.type)
      {
      case Edit::Equal:
        flush();
        for (int t = 0; t < e.len; ++t)
        {
          DiffRow row;
          row.kind = RowKind::Same;
          row.left = e.a + t;
          row.right = e.b + t;
          doc.rows.push_back(std::move(row));
        }
        break;
      case Edit::Delete:
        for (int t = 0; t < e.len; ++t)
          pendingDel.push_back(e.a + t);
        break;
      case Edit::Insert:
        for (int t = 0; t < e.len; ++t)
          pendingIns.push_back(e.b + t);
        break;
      }
    }
    flush();
  }

  std::wstring ExpandTabs(const std::wstring& s, std::vector<int>* map, int tabSize)
  {
    std::wstring out;
    out.reserve(s.size() + 8);
    if (map)
      map->resize(s.size() + 1);

    int col = 0;
    for (size_t i = 0; i < s.size(); ++i)
    {
      if (map)
        (*map)[i] = col;
      wchar_t c = s[i];
      if (c == L'\t')
      {
        int n = tabSize - (col % tabSize);
        out.append(n, L' ');
        col += n;
      }
      else
      {
        out += c;
        ++col;
      }
    }
    if (map)
      (*map)[s.size()] = col;
    return out;
  }

  std::vector<std::wstring> SplitLines(const std::wstring& text)
  {
    std::vector<std::wstring> lines;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i)
    {
      wchar_t c = text[i];
      if (c == L'\n' || c == L'\r')
      {
        lines.emplace_back(text, start, i - start);
        if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
          ++i;
        start = i + 1;
      }
    }
    if (start < text.size())
      lines.emplace_back(text, start, text.size() - start);
    return lines;
  }
}
