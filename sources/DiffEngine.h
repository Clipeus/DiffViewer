#pragma once

// Line-based diff model and the Myers O(ND) algorithm (linear-space variant).

enum class RowKind { Same, Added, Removed, Changed };

struct TextRange
{
  int start = 0;  // inclusive, char offset
  int end = 0;    // exclusive
};

struct DiffRow
{
  RowKind kind = RowKind::Same;
  int left = -1;    // index into DiffDocument::leftLines or -1 (filler)
  int right = -1;   // index into DiffDocument::rightLines or -1 (filler)
  int block = -1;   // index into DiffDocument::blocks or -1 for unchanged rows
  std::vector<TextRange> leftRanges;   // intra-line highlights (Changed rows only)
  std::vector<TextRange> rightRanges;
};

struct DiffBlock
{
  int firstRow = 0;
  int lastRow = 0;
  RowKind kind = RowKind::Changed;
};

struct DiffDocument
{
  std::vector<std::wstring> leftLines;
  std::vector<std::wstring> rightLines;
  std::vector<DiffRow> rows;
  std::vector<DiffBlock> blocks;
  std::wstring leftTitle;
  std::wstring rightTitle;
  std::wstring leftInfo;    // encoding etc.
  std::wstring rightInfo;
};

namespace DiffEngine
{
  struct Edit
  {
    enum Type { Equal, Delete, Insert };
    Type type;
    int a;     // position in the first sequence
    int b;     // position in the second sequence
    int len;
  };

  // Myers diff with the linear-space "middle snake" refinement. Eq is a callable bool(int i, int j).
  template <class Eq>
  class Myers
  {
  public:
    Myers(int n, int m, Eq eq) : m_n(n), m_m(m), m_eq(eq) {}

    void Run(std::vector<Edit>& out)
    {
      m_out = &out;
      out.clear();

      struct Task { bool equal; int a0; int a1; int b0; int b1; int len; };
      std::vector<Task> stack;
      stack.push_back({ false, 0, m_n, 0, m_m, 0 });

      while (!stack.empty())
      {
        Task t = stack.back();
        stack.pop_back();

        if (t.equal)
        {
          Emit(Edit::Equal, t.a0, t.b0, t.len);
          continue;
        }

        int a0 = t.a0, a1 = t.a1, b0 = t.b0, b1 = t.b1;

        int pre = 0;
        while (a0 < a1 && b0 < b1 && m_eq(a0, b0)) { ++a0; ++b0; ++pre; }
        if (pre)
          Emit(Edit::Equal, t.a0, t.b0, pre);

        int suf = 0;
        while (a1 > a0 && b1 > b0 && m_eq(a1 - 1, b1 - 1)) { --a1; --b1; ++suf; }
        if (suf)
          stack.push_back({ true, a1, 0, b1, 0, suf });

        if (a0 == a1 && b0 == b1)
          continue;
        if (a0 == a1) { Emit(Edit::Insert, a0, b0, b1 - b0); continue; }
        if (b0 == b1) { Emit(Edit::Delete, a0, b0, a1 - a0); continue; }

        int x, y, u, v;
        MiddleSnake(a0, a1, b0, b1, x, y, u, v);

        bool noProgress = (x == a0 && y == b0 && u == x && v == y) || (u == a1 && v == b1 && x == u && y == v);
        if (noProgress)
        {
          Emit(Edit::Delete, a0, b0, a1 - a0);
          Emit(Edit::Insert, a1, b0, b1 - b0);
          continue;
        }

        stack.push_back({ false, u, a1, v, b1, 0 });
        if (u > x)
          stack.push_back({ true, x, 0, y, 0, u - x });
        stack.push_back({ false, a0, x, b0, y, 0 });
      }
    }

  private:
    void Emit(Edit::Type type, int a, int b, int len)
    {
      if (len <= 0)
        return;

      if (!m_out->empty())
      {
        Edit& last = m_out->back();
        if (last.type == type)
        {
          bool contiguous = false;
          switch (type)
          {
          case Edit::Equal:  contiguous = (last.a + last.len == a) && (last.b + last.len == b); break;
          case Edit::Delete: contiguous = (last.a + last.len == a) && (last.b == b); break;
          case Edit::Insert: contiguous = (last.a == a) && (last.b + last.len == b); break;
          }
          if (contiguous)
          {
            last.len += len;
            return;
          }
        }
      }
      m_out->push_back({ type, a, b, len });
    }

    void MiddleSnake(int a0, int a1, int b0, int b1, int& x, int& y, int& u, int& v)
    {
      const int N = a1 - a0;
      const int M = b1 - b0;
      const int delta = N - M;
      const bool odd = (delta & 1) != 0;
      const int maxD = (N + M + 1) / 2 + 1;
      const int offset = maxD + 1;

      m_vf.assign(2 * maxD + 3, 0);
      m_vb.assign(2 * maxD + 3, 0);

      for (int d = 0; d <= maxD; ++d)
      {
        // forward
        for (int k = -d; k <= d; k += 2)
        {
          int xx;
          if (k == -d || (k != d && m_vf[offset + k - 1] < m_vf[offset + k + 1]))
            xx = m_vf[offset + k + 1];
          else
            xx = m_vf[offset + k - 1] + 1;
          int yy = xx - k;
          const int x0 = xx, y0 = yy;
          while (xx < N && yy < M && m_eq(a0 + xx, b0 + yy)) { ++xx; ++yy; }
          m_vf[offset + k] = xx;

          if (odd && (k - delta) >= -(d - 1) && (k - delta) <= (d - 1))
          {
            if (m_vf[offset + k] + m_vb[offset + (delta - k)] >= N)
            {
              x = a0 + x0; y = b0 + y0; u = a0 + xx; v = b0 + yy;
              return;
            }
          }
        }

        // backward
        for (int k = -d; k <= d; k += 2)
        {
          int xx;
          if (k == -d || (k != d && m_vb[offset + k - 1] < m_vb[offset + k + 1]))
            xx = m_vb[offset + k + 1];
          else
            xx = m_vb[offset + k - 1] + 1;
          int yy = xx - k;
          const int x0 = xx, y0 = yy;
          while (xx < N && yy < M && m_eq(a1 - 1 - xx, b1 - 1 - yy)) { ++xx; ++yy; }
          m_vb[offset + k] = xx;

          if (!odd && (k - delta) >= -d && (k - delta) <= d)
          {
            if (m_vb[offset + k] + m_vf[offset + (delta - k)] >= N)
            {
              x = a1 - xx; y = b1 - yy; u = a1 - x0; v = b1 - y0;
              return;
            }
          }
        }
      }

      // Should be unreachable; degrade gracefully.
      x = a0; y = b0; u = a0; v = b0;
    }

  private:
    int m_n;
    int m_m;
    Eq m_eq;
    std::vector<Edit>* m_out = nullptr;
    std::vector<int> m_vf;
    std::vector<int> m_vb;
  };

  template <class Eq>
  void Diff(int n, int m, Eq eq, std::vector<Edit>& out)
  {
    Myers<Eq> myers(n, m, eq);
    myers.Run(out);
  }

  // Fills doc.rows / doc.blocks from doc.leftLines / doc.rightLines.
  void BuildDocument(DiffDocument& doc, bool ignoreWhitespace);

  // Replaces tabs with spaces. If map is given, map[i] receives the display column of character i (size = s.size() + 1).
  std::wstring ExpandTabs(const std::wstring& s, std::vector<int>* map, int tabSize = 4);

  // Splits text on \r\n, \n or \r. A trailing line break does not produce an extra empty line.
  std::vector<std::wstring> SplitLines(const std::wstring& text);
}
