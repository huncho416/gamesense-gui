#define IMGUI_DEFINE_MATH_OPERATORS
#include "menu.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int Menu::tab = 0;
int Menu::sub[6] = {};

namespace
{
using Menu::sub;
using Menu::tab;

ImFont *fReg, *fBold, *fBoldSm, *fSmall, *fTab;
ImVec2 patUV0, patUV1;          // background tile inside the font atlas
const float PAT = 256;
const ImVec2 ONE(1, 1);
const ImU32 BLACK = IM_COL32(10, 10, 10, 255), LINE = IM_COL32(39, 39, 39, 255), EDGE = IM_COL32(60, 60, 60, 255);

// Content colours follow style.Alpha so a page can fade in; the chrome uses IM_COL32 directly.
ImU32 Col(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, (int)(a * ImGui::GetStyle().Alpha)); }
ImU32 Gray(int v) { return Col(v, v, v); }

void Text(ImDrawList* d, ImFont* f, ImVec2 p, ImU32 c, const char* s, const char* e = nullptr) { d->AddText(f, f->FontSize, p, c, s, e); }
ImVec2 Size(ImFont* f, const char* s, const char* e = nullptr) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, s, e); }

// Text cut to maxW, ending in "..." when it does not fit.
void TextEllipsis(ImDrawList* d, ImFont* f, ImVec2 p, ImU32 c, const char* s, float maxW)
{
    const char* e = s + strlen(s);
    if (Size(f, s).x <= maxW) { Text(d, f, p, c, s); return; }
    float dots = Size(f, "...").x;
    while (e > s && Size(f, s, e).x + dots > maxW) e--;
    Text(d, f, p, c, s, e);
    Text(d, f, {p.x + Size(f, s, e).x, p.y}, c, "...");
}

// The dotted background. Tiles are anchored to the screen so separate fills line up.
void Pattern(ImDrawList* d, ImVec2 a, ImVec2 b)
{
    d->PushClipRect(a, b, true);
    for (float y = floorf(a.y / PAT) * PAT; y < b.y; y += PAT)
        for (float x = floorf(a.x / PAT) * PAT; x < b.x; x += PAT)
            d->AddImage(ImGui::GetIO().Fonts->TexID, {x, y}, {x + PAT, y + PAT}, patUV0, patUV1);
    d->PopClipRect();
}

// Window frame: four border rings, dotted fill, and the gradient line under the top edge.
void Frame(ImDrawList* d, ImVec2 p, ImVec2 q)
{
    d->AddRectFilled(p, q, BLACK);
    d->AddRectFilled(p + ONE, q - ONE, EDGE);
    d->AddRectFilled(p + ONE * 2, q - ONE * 2, LINE);
    d->AddRectFilled(p + ONE * 5, q - ONE * 5, EDGE);
    Pattern(d, p + ONE * 6, q - ONE * 6);
    d->AddRectFilled({p.x + 6, p.y + 6}, {q.x - 6, p.y + 10}, IM_COL32(6, 6, 6, 255));
    float x0 = p.x + 7, x1 = q.x - 7, xm = floorf((x0 + x1) / 2);
    for (int i = 0; i < 2; i++)
    {
        float k = i ? 0.5f : 1.0f, y = p.y + 7 + i;
        ImU32 l = IM_COL32(110 * k, 152 * k, 180 * k, 255), m = IM_COL32(165 * k, 104 * k, 163 * k, 255), r = IM_COL32(196 * k, 196 * k, 128 * k, 255);
        d->AddRectFilledMultiColor({x0, y}, {xm, y + 1}, l, m, m, l);
        d->AddRectFilledMultiColor({xm, y}, {x1, y + 1}, m, r, r, m);
    }
}

// ---- windows: own drag, edge snapping and resize grip --------------------------------------------

struct Win
{
    const char* name;
    ImVec2 pos, size, min;
    bool open = true, snapped = false;
};
Win wMain{"##main", {65, 93}, {660, 563}, {480, 380}};
Win wConsole{"##console", {726, 93}, {459, 279}, {200, 110}};
Win wKeys{"##keys", {726, 373}, {237, 106}, {160, 80}};
Win* wins[] = {&wMain, &wConsole, &wKeys};
Win* dragging;
ImVec2 dragOff;

// While dragged, a window sticks 1px off the edge of a neighbour and lines up with it.
void Snap(Win& w)
{
    const float G = 1, T = 10;
    auto snap = [&](float& v, float to) { if (fabsf(v - to) >= T) return false; v = to; return true; };
    w.snapped = false;
    for (Win* o : wins)
    {
        if (o == &w || !o->open) continue;
        bool nearX = w.pos.x < o->pos.x + o->size.x + T && w.pos.x + w.size.x > o->pos.x - T;
        bool nearY = w.pos.y < o->pos.y + o->size.y + T && w.pos.y + w.size.y > o->pos.y - T;
        if (nearY && (snap(w.pos.x, o->pos.x + o->size.x + G) || snap(w.pos.x, o->pos.x - w.size.x - G))) { w.snapped = true; snap(w.pos.y, o->pos.y); }
        if (nearX && (snap(w.pos.y, o->pos.y + o->size.y + G) || snap(w.pos.y, o->pos.y - w.size.y - G))) { w.snapped = true; snap(w.pos.x, o->pos.x); }
    }
}

ImDrawList* BeginWin(Win& w)
{
    if (dragging == &w)
    {
        if (ImGui::IsMouseDown(0)) { if (ImGui::IsMouseDragging(0, 3)) { w.pos = ImGui::GetMousePos() - dragOff; Snap(w); } }
        else { dragging = nullptr; w.snapped = false; }
    }
    w.pos = {floorf(w.pos.x), floorf(w.pos.y)};
    ImGui::SetNextWindowPos(w.pos);
    ImGui::SetNextWindowSize(w.size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin(w.name, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    Frame(ImGui::GetWindowDrawList(), w.pos, w.pos + w.size);
    return ImGui::GetWindowDrawList();
}

void EndWin(Win& w)
{
    ImDrawList* d = ImGui::GetWindowDrawList();
    ImVec2 q = w.pos + w.size, m = ImGui::GetMousePos();
    ImGui::SetCursorScreenPos(q - ImVec2(18, 18));
    ImGui::InvisibleButton("##winGrip", {12, 12});
    bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
    if (ImGui::IsItemActive()) w.size = {floorf(std::max(w.min.x, m.x - w.pos.x + 8)), floorf(std::max(w.min.y, m.y - w.pos.y + 8))};
    d->AddTriangleFilled({q.x - 6, q.y - 12}, {q.x - 6, q.y - 6}, {q.x - 12, q.y - 6}, hot ? IM_COL32(150, 190, 35, 255) : IM_COL32(106, 122, 48, 255));
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0)) { dragging = &w; dragOff = m - w.pos; }
    if (w.snapped) d->AddRectFilled(w.pos, q, IM_COL32(125, 165, 20, 100));
    ImGui::End();
}

// ---- group box and widgets -----------------------------------------------------------------------

void GroupBox(ImDrawList* d, ImVec2 a, ImVec2 b, const char* title)
{
    d->AddRectFilled(a - ONE, b + ONE, Col(10, 10, 10));
    d->AddRectFilled(a, b, Gray(39));
    d->AddRectFilled(a + ONE, b - ONE, Gray(23));
    if (!title) return;
    float tw = Size(fBold, title).x;
    Pattern(d, {a.x + 9, a.y - 1}, {a.x + 17 + tw, a.y});               // the title sits in a gap in the top border
    d->AddRectFilled({a.x + 9, a.y}, {a.x + 17 + tw, a.y + 1}, Gray(23));
    Text(d, fBold, {a.x + 13, a.y - 7}, Gray(210), title);
}

struct Ctx { ImDrawList* d; ImVec2 a, b; float y; };    // box rect and the text-top of the next row
const float LX = 39;                                    // left inset of labels and wide widgets
float WideW(const Ctx& c) { return std::max(30.f, c.b.x - c.a.x - 82); }

ImGuiKey kEnabled = ImGuiKey_MouseX1, kMenu = ImGuiKey_Delete;
ImGuiKey* listening;                                    // the bind waiting for a key press, if any

const char* KeyName(ImGuiKey k)
{
    switch (k)
    {
    case ImGuiKey_None: return "-";
    case ImGuiKey_MouseLeft: return "M1";
    case ImGuiKey_MouseRight: return "M2";
    case ImGuiKey_MouseMiddle: return "M3";
    case ImGuiKey_MouseX1: return "M4";
    case ImGuiKey_MouseX2: return "M5";
    case ImGuiKey_Delete: return "DEL";
    case ImGuiKey_Insert: return "INS";
    default: return ImGui::GetKeyName(k);
    }
}

// While a bind is listening: Escape clears it, any other key or mouse button sets it.
void PollBind()
{
    ImGuiKey hit = ImGuiKey_None;
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_MouseWheelX && hit == ImGuiKey_None; k++)
        if (ImGui::IsKeyPressed((ImGuiKey)k, false)) hit = (ImGuiKey)k;
    if (hit == ImGuiKey_None) return;
    *listening = hit == ImGuiKey_Escape ? ImGuiKey_None : hit;
    listening = nullptr;
}

// bind: key shown at the right edge as "[M4]"; click it, then press a key (Escape clears).
// swatch: colour chip at the right edge; click it for a picker.
bool Checkbox(Ctx& c, const char* label, bool* v, ImGuiKey* bind = nullptr, ImVec4* swatch = nullptr, bool enabled = true)
{
    ImVec2 p{c.a.x, c.y};
    ImGui::SetCursorScreenPos({p.x + 15, p.y - 2});
    bool pressed = ImGui::InvisibleButton(label, {LX - 11 + Size(fReg, label).x, 17}) && enabled;
    bool hov = ImGui::IsItemHovered() && enabled;
    if (pressed) *v = !*v;
    ImVec2 k{p.x + 19, p.y + 3};
    c.d->AddRectFilled(k, k + ONE * 8, Col(10, 10, 10));
    if (*v) c.d->AddRectFilledMultiColor(k + ONE, k + ONE * 7, Col(152, 194, 34), Col(152, 194, 34), Col(120, 162, 10), Col(120, 162, 10));
    else { int t = !enabled ? 40 : hov ? 92 : 77, bt = !enabled ? 32 : hov ? 64 : 51; c.d->AddRectFilledMultiColor(k + ONE, k + ONE * 7, Gray(t), Gray(t), Gray(bt), Gray(bt)); }
    Text(c.d, fReg, {p.x + LX, p.y + 1}, Gray(enabled ? 205 : 90), label);
    ImGui::PushID(label);
    if (bind)
    {
        char buf[32];
        snprintf(buf, sizeof buf, "[%s]", listening == bind ? "..." : KeyName(*bind));
        ImVec2 hs = Size(fSmall, buf), hp{c.b.x - 17 - hs.x, p.y + 3};
        ImGui::SetCursorScreenPos(hp - ImVec2(3, 3));
        if (ImGui::InvisibleButton("##bind", hs + ImVec2(6, 6))) listening = bind;
        else if (listening == bind) PollBind();
        Text(c.d, fSmall, hp, listening == bind ? Col(152, 194, 34) : Gray(ImGui::IsItemHovered() ? 160 : 100), buf);
    }
    if (swatch)
    {
        ImVec2 s{c.b.x - 34, p.y + 3};
        ImGui::SetCursorScreenPos(s - ImVec2(2, 3));
        if (ImGui::InvisibleButton("##chip", {21, 15}) && enabled) ImGui::OpenPopup("##picker");
        c.d->AddRectFilled(s, s + ImVec2(17, 9), Col(10, 10, 10));
        c.d->AddRectFilled(s + ONE, s + ImVec2(16, 8), ImGui::GetColorU32(*swatch));
        ImGui::SetNextWindowPos({s.x + 17, s.y + 12}, ImGuiCond_Always, {1, 0});     // opens leftwards, so it stays on screen
        if (ImGui::BeginPopup("##picker"))
        {
            ImGui::SetNextItemWidth(170);
            ImGui::ColorPicker4("##colour", &swatch->x, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
            ImGui::EndPopup();
        }
    }
    ImGui::PopID();
    c.y += 19;
    return pressed;
}

void Combo(Ctx& c, const char* label, int* v, const char* const items[], int n)
{
    float x = c.a.x + LX, w = WideW(c);
    ImVec2 a{x, c.y + 13}, b{x + w, c.y + 33};
    Text(c.d, fReg, {x, c.y - 2}, Gray(205), label);
    ImGui::SetCursorScreenPos(a);
    if (ImGui::InvisibleButton(label, b - a)) ImGui::OpenPopup(label);
    int t = ImGui::IsItemHovered() ? 38 : 31;
    c.d->AddRectFilled(a, b, Col(10, 10, 10));
    c.d->AddRectFilledMultiColor(a + ONE, b - ONE, Gray(t), Gray(t), Gray(t + 5), Gray(t + 5));
    c.d->PushClipRect(a, {b.x - 16, b.y}, true);
    Text(c.d, fReg, {a.x + 8, a.y + 4}, Gray(150), items[*v]);
    c.d->PopClipRect();
    float ax = b.x - 12, ay = a.y + 8;
    c.d->AddRectFilled({ax, ay - 1}, {ax + 5, ay}, Col(0, 0, 0));
    for (int i = 0; i < 3; i++) c.d->AddRectFilled({ax + i, ay + i}, {ax + 5 - i, ay + i + 1}, Gray(151));

    ImGui::SetNextWindowPos({a.x, b.y + 1});
    ImGui::SetNextWindowSize({w, n * 20.f + 2});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0);
    if (ImGui::BeginPopup(label, ImGuiWindowFlags_NoMove))
    {
        ImDrawList* pd = ImGui::GetWindowDrawList();
        ImVec2 o = ImGui::GetWindowPos();
        pd->AddRectFilled(o, o + ImVec2(w, n * 20.f + 2), BLACK);
        pd->AddRectFilled(o + ONE, o + ImVec2(w - 1, n * 20.f + 1), IM_COL32(35, 35, 35, 255));
        for (int i = 0; i < n; i++)
        {
            ImVec2 ia = o + ImVec2(1, 1 + i * 20.f);
            ImGui::SetCursorScreenPos(ia);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##item", {w - 2, 20})) { *v = i; ImGui::CloseCurrentPopup(); }
            ImGui::PopID();
            bool h = ImGui::IsItemHovered();
            if (h) pd->AddRectFilled(ia, ia + ImVec2(w - 2, 20), IM_COL32(26, 26, 26, 255));
            Text(pd, h || i == *v ? fBold : fReg, ia + ImVec2(8, 4), i == *v ? IM_COL32(152, 194, 34, 255) : IM_COL32(205, 205, 205, 255), items[i]);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    c.y += 40;
}

void Slider(Ctx& c, const char* label, float* v, float lo, float hi, const char* fmt)
{
    float x = c.a.x + LX, w = WideW(c);
    ImVec2 a{x, c.y + 14}, b{x + w, c.y + 21};
    Text(c.d, fReg, {x, c.y - 2}, Gray(205), label);
    ImGui::SetCursorScreenPos({a.x, a.y - 3});
    ImGui::InvisibleButton(label, {w, 13});
    if (ImGui::IsItemActive()) *v = roundf(lo + (hi - lo) * std::clamp((ImGui::GetMousePos().x - a.x - 1) / (w - 2), 0.f, 1.f));
    float fx = a.x + 1 + floorf((w - 2) * (*v - lo) / (hi - lo));
    c.d->AddRectFilled(a, b, Col(10, 10, 10));
    c.d->AddRectFilledMultiColor(a + ONE, b - ONE, Gray(52), Gray(52), Gray(65), Gray(65));
    if (fx > a.x + 1) c.d->AddRectFilledMultiColor(a + ONE, {fx, b.y - 1}, Col(152, 192, 36), Col(152, 192, 36), Col(112, 148, 14), Col(112, 148, 14));
    c.d->AddRectFilled({a.x - 5, a.y + 3}, {a.x - 2, a.y + 4}, Gray(97));                       // "-"
    c.d->AddRectFilled({b.x + 2, a.y + 3}, {b.x + 5, a.y + 4}, Gray(97));                       // "+"
    c.d->AddRectFilled({b.x + 3, a.y + 2}, {b.x + 4, a.y + 5}, Gray(97));
    char buf[32];
    snprintf(buf, sizeof buf, fmt, *v);
    ImVec2 tp{floorf(fx - Size(fBoldSm, buf).x / 2), a.y};
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) Text(c.d, fBoldSm, tp + ImVec2((float)dx, (float)dy), Col(0, 0, 0, 200), buf);
    Text(c.d, fBoldSm, tp, Gray(215), buf);
    c.y += 30;
}

// inset/w default to the same column the combos and sliders use.
bool Button(Ctx& c, const char* label, bool enabled = true, float inset = LX + 1, float w = 0)
{
    ImVec2 a{c.a.x + inset, c.y}, b{a.x + (w ? w : WideW(c)), c.y + 23};
    ImGui::SetCursorScreenPos(a);
    bool pressed = ImGui::InvisibleButton(label, b - a) && enabled;
    int t = !enabled ? 27 : ImGui::IsItemActive() ? 24 : ImGui::IsItemHovered() ? 42 : 34;
    c.d->AddRectFilled(a, b, Col(10, 10, 10));
    c.d->AddRectFilled(a + ONE, b - ONE, Gray(45));
    c.d->AddRectFilledMultiColor(a + ONE * 2, b - ONE * 2, Gray(t), Gray(t), Gray(t - 7), Gray(t - 7));
    ImVec2 ts = Size(fBold, label);
    Text(c.d, fBold, {floorf((a.x + b.x - ts.x) / 2), a.y + 5}, Gray(enabled ? 205 : 70), label);
    c.y += 33;
    return pressed;
}

// ---- icons, drawn from primitives so no icon font is needed ---------------------------------------

enum { I_NONE, I_PERSON, I_STAR, I_TAG, I_GEAR, I_GEARS, I_BALL };

void Gear(ImDrawList* d, ImVec2 c, float r, ImU32 col)
{
    d->AddCircle(c, r * 0.62f, col, 32, r * 0.36f);
    for (int k = 0; k < 8; k++)
    {
        float ang = k * 3.14159265f / 4;
        ImVec2 dir{cosf(ang), sinf(ang)}, n{-dir.y, dir.x};
        ImVec2 p0 = c + dir * (r * 0.7f), p1 = c + dir * r;
        d->AddQuadFilled(p0 - n * (r * 0.24f), p0 + n * (r * 0.24f), p1 + n * (r * 0.16f), p1 - n * (r * 0.16f), col);
    }
}

void Icon(ImDrawList* d, int id, ImVec2 c, float s, ImU32 col, ImU32 bg)
{
    switch (id)
    {
    case I_PERSON:
        d->AddCircleFilled({c.x, c.y - 5 * s}, 5.5f * s, col, 24);
        d->AddRectFilled({c.x - 10 * s, c.y + 2 * s}, {c.x + 10 * s, c.y + 12 * s}, col, 7 * s, ImDrawFlags_RoundCornersTop);
        break;
    case I_STAR:
    {
        ImVec2 pts[10];
        for (int k = 0; k < 10; k++) { float r = (k & 1 ? 5.f : 12.f) * s, ang = -1.5707963f + k * 0.62831853f; pts[k] = c + ImVec2(cosf(ang) * r, sinf(ang) * r + s); }
        d->AddConcavePolyFilled(pts, 10, col);
        break;
    }
    case I_TAG:
    {
        const ImVec2 local[5] = {{-12, 0}, {-5, -7}, {11, -7}, {11, 7}, {-5, 7}};
        const float cs = -0.7071f, sn = 0.7071f;                // point up-right
        ImVec2 pts[5];
        for (int k = 0; k < 5; k++) pts[k] = c + ImVec2(local[k].x * cs - local[k].y * sn, local[k].x * sn + local[k].y * cs) * s;
        d->AddConvexPolyFilled(pts, 5, col);
        d->AddCircleFilled(c + ImVec2(-5 * cs, -5 * sn) * s, 2 * s, bg, 12);
        break;
    }
    case I_GEAR: Gear(d, c, 11 * s, col); break;
    case I_GEARS: Gear(d, c + ImVec2(-4, -4) * s, 9 * s, col); Gear(d, c + ImVec2(7, 7) * s, 6.5f * s, col); break;
    case I_BALL:
        d->AddCircleFilled(c + ImVec2(-2, 3) * s, 10 * s, col, 32);
        d->AddCircleFilled(c + ImVec2(2, -1) * s, 3 * s, bg, 16);
        d->AddCircleFilled(c + ImVec2(8, -8) * s, 3 * s, col, 16);
        break;
    }
}

// ---- pages: two columns of group boxes -----------------------------------------------------------

struct Box { const char* title; void (*body)(Ctx&); float h; };     // h = 0: share what is left of the column
struct Page { std::vector<Box> col[2]; float split = 0; };            // split = 0: equal columns
struct StripDef { const char* title; int n; int icons[6]; };

Page pages[6][6];
const StripDef strips[6] = {
    {},
    {"Display", 6, {I_PERSON, I_STAR, I_TAG, I_GEAR, I_PERSON, I_GEAR}},
    {"Misc", 2, {I_GEAR, I_PERSON}},
};

// Lays out a page. Drag a box's corner grip to resize it, drag its title to move it to the other column.
void Layout(ImDrawList* d, Page& pg, ImVec2 a, ImVec2 b)
{
    static Box* dragBox;
    static ImVec2 dragBoxOff;
    static int dragCol;
    const float GAP = 17, VGAP = 23;
    ImVec2 m = ImGui::GetMousePos();
    float total = b.x - a.x;
    float w0 = pg.split > 0 ? std::clamp(pg.split, 80.f, total - GAP - 80) : floorf((total - GAP) / 2) + 2;
    float x[2] = {a.x, a.x + w0 + GAP}, w[2] = {w0, total - GAP - w0};
    Box* startDrag = nullptr;

    for (int pass = 0; pass < 2; pass++)                // the dragged box is drawn last, on top
        for (int ci = 0; ci < 2; ci++)
        {
            float fixed = 0;
            int fill = 0;
            for (Box& bx : pg.col[ci]) bx.h ? fixed += bx.h : fill++;
            float fillH = fill ? std::max(40.f, floorf((b.y - a.y - VGAP * (pg.col[ci].size() - 1) - fixed) / fill)) : 0;
            float y = a.y;
            for (Box& bx : pg.col[ci])
            {
                float h = bx.h ? bx.h : fillH;
                ImVec2 ba{x[ci], y}, bb{x[ci] + w[ci], y + h};
                y += h + VGAP;
                bool isDrag = dragBox == &bx;
                if (isDrag != (pass == 1)) continue;
                if (isDrag) { ImVec2 off = m - dragBoxOff - ba; ba += off; bb += off; }

                ImGui::PushID(bx.title);
                GroupBox(d, ba, bb, bx.title);
                ImGui::SetCursorScreenPos({ba.x + 9, ba.y - 8});
                ImGui::InvisibleButton("##title", {Size(fBold, bx.title).x + 8, 16});
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 4) && !dragBox) { startDrag = &bx; dragBoxOff = m - ba; dragCol = ci; }
                if (ImGui::IsItemHovered() || isDrag) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                Ctx c{d, ba, bb, ba.y + 16};
                d->PushClipRect(ba + ONE, bb - ONE, true);
                bx.body(c);
                d->PopClipRect();

                ImGui::SetCursorScreenPos(bb - ImVec2(10, 10));
                ImGui::InvisibleButton("##grip", {10, 10});
                bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
                if (ImGui::IsItemActive()) { bx.h = std::max(40.f, floorf(m.y - ba.y + 3)); if (ci == 0) pg.split = floorf(m.x - a.x + 3); }
                d->AddTriangleFilled({bb.x - 1, bb.y - 6}, {bb.x - 1, bb.y - 1}, {bb.x - 6, bb.y - 1}, hot ? Col(150, 190, 35) : Col(106, 122, 48));
                ImGui::PopID();
            }
        }

    if (startDrag) dragBox = startDrag;
    if (dragBox && !ImGui::IsMouseDown(0))
    {
        if ((m.x > x[1] - GAP / 2) != (dragCol == 1)) std::swap(pg.col[0], pg.col[1]);
        dragBox = nullptr;
    }
}

void Strip(ImDrawList* d, ImVec2 a, ImVec2 b, const StripDef& s, int* sel)
{
    GroupBox(d, a, b, s.title);
    for (int i = 0; i < s.n; i++)
    {
        ImVec2 c{floorf(a.x + 46 + i * 75.2f), a.y + 29};
        ImGui::SetCursorScreenPos(c - ImVec2(18, 18));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##strip", {36, 36})) *sel = i;
        ImGui::PopID();
        Icon(d, s.icons[i], c, 1.2f, Gray(i == *sel ? 255 : ImGui::IsItemHovered() ? 140 : 92), Gray(23));
    }
}

// ---- placeholder page contents -------------------------------------------------------------------

const ImU32 WHITE = IM_COL32(255, 255, 255, 255);

void ControlsBody(Ctx& c)
{
    static bool on;
    static int a, b = 1, m;
    static float v[6] = {130, 30, 15, 100, 0, 250};
    static const char* const none[] = {"-", "First", "Second"};
    static const char* const anchor[] = {"-", "Primary", "Secondary"};
    Checkbox(c, "Enabled", &on, &kEnabled);
    Combo(c, "Selection", &a, none, 3);
    Combo(c, "Anchor", &b, anchor, 3);
    Combo(c, "Multi-select", &m, none, 3);
    Slider(c, "Maximum size", &v[0], 0, 1000, "%.0fpx");
    Slider(c, "Speed", &v[1], 0, 100, "%.0f%%");
    Slider(c, "Speed (active)", &v[2], 0, 100, "%.0f%%");
    Slider(c, "Speed scale", &v[3], 0, 300, "%.0f%%");
    Slider(c, "Reaction time", &v[4], 0, 1000, "%.0fms");
    Slider(c, "Switch delay", &v[5], 0, 2000, "%.0fms");
}

void OptionsBody(Ctx& c)
{
    static bool vis = true, o1, o2;
    static int pr;
    static float v[3] = {1, 550, 0};
    static const char* const prio[] = {"Center", "Nearest", "Lowest"};
    Checkbox(c, "Visible only", &vis);
    Combo(c, "Priority", &pr, prio, 3);
    Slider(c, "Deadzone", &v[0], 0, 20, "%.0fpx");
    Slider(c, "Velocity", &v[1], 0, 1000, "%.0f m/s");
    Slider(c, "Range", &v[2], 0, 500, "%.0fm");
    Checkbox(c, "Option A", &o1);
    Checkbox(c, "Option B", &o2);
}

void GeneralBody(Ctx& c)
{
    static bool master = true, fade = true;
    static float dist = 3000, start = 75;
    Checkbox(c, "Master", &master);
    Slider(c, "Global max distance", &dist, 0, 3000, "%.0fm");
    Checkbox(c, "Distance fade", &fade);
    Slider(c, "Fade start", &start, 0, 100, "%.0f%%");
}

void OtherBody(Ctx& c)
{
    static bool o[5];
    static int hide;
    static ImVec4 dot{1, 1, 1, 1};
    static const char* const items[] = {"-", "Labels", "Icons"};
    Checkbox(c, "Draw outline", &o[0]);
    Checkbox(c, "Draw marker", &o[1]);
    Checkbox(c, "Center dot", &o[2], nullptr, &dot);
    Combo(c, "Hidden items", &hide, items, 3);
    Checkbox(c, "Compact mode", &o[3]);
    Checkbox(c, "Watermark", &o[4]);
    Checkbox(c, "Console", &wConsole.open);
    Checkbox(c, "Keybind list", &wKeys.open);
}

void CategoryBody(Ctx& c)
{
    static bool o[8] = {true, true, true, true, true, false, true, true};
    static float dist = 750;
    static int style;
    static ImVec4 col[3] = {{0.59f, 0.75f, 0.14f, 1}, {0.59f, 0.75f, 0.14f, 1}, {0.43f, 0.59f, 0.82f, 1}};
    static const char* const styles[] = {"Corner", "Full"};
    Checkbox(c, "Show items", &o[0], nullptr, &col[0]);
    Checkbox(c, "Highlight", &o[1], nullptr, &col[1]);
    Slider(c, "Max distance", &dist, 0, 3000, "%.0fm");
    Checkbox(c, "Name", &o[2]);
    Checkbox(c, "Distance", &o[3]);
    Checkbox(c, "Outline", &o[4]);
    Combo(c, "Outline style", &style, styles, 2);
    Checkbox(c, "Dot", &o[5]);
    Checkbox(c, "Bar", &o[6]);
    Checkbox(c, "Marker", &o[7], nullptr, &col[2]);
}

void ExtrasBody(Ctx& c)
{
    static bool o[2];
    static ImVec4 col[2] = {{0.47f, 0.75f, 0.86f, 1}, {0.47f, 0.82f, 0.82f, 1}};
    Checkbox(c, "Type A", &o[0], nullptr, &col[0]);
    Checkbox(c, "Type B", &o[1], nullptr, &col[1]);
}

void TweaksBody(Ctx& c)
{
    static bool o[5];
    static const char* const names[] = {"Fast updates", "Extended range", "Exact values", "Highlight under cursor", "Show distance"};
    for (int i = 0; i < 5; i++) Checkbox(c, names[i], &o[i]);
}

void ExperimentalBody(Ctx& c)
{
    static bool o;
    Checkbox(c, "Override (experimental)", &o);
}

int listSel;
char listFilter[32], listRename[32];

void ItemsBody(Ctx& c)
{
    float x = c.a.x + 20, w = c.b.x - c.a.x - 40;
    ImGui::SetCursorScreenPos({x, c.a.y + 16});
    ImGui::SetNextItemWidth(w);
    ImGui::InputTextWithHint("##search", "Search...", listFilter, sizeof listFilter);
    ImVec2 la{x, c.a.y + 33}, lb{x + w, c.b.y - 51};
    c.d->AddRectFilled(la, lb, Col(10, 10, 10));
    c.d->AddRectFilled(la + ONE, lb - ONE, Gray(30));
    c.d->PushClipRect(la + ONE, lb - ONE, true);
    float y = la.y + 1;
    for (int i = 0; i < 8; i++)
    {
        char name[16];
        snprintf(name, sizeof name, "Item %d", i + 1);
        if (listFilter[0] && !strstr(name, listFilter)) continue;   // shortcut: case-sensitive substring match, swap for a real filter when the list holds real data
        ImGui::SetCursorScreenPos({la.x + 1, y});
        if (ImGui::InvisibleButton(name, {w - 2, 15})) listSel = i;
        bool s = listSel == i;
        if (s || ImGui::IsItemHovered()) c.d->AddRectFilled({la.x + 1, y}, {lb.x - 1, y + 15}, Gray(s ? 38 : 34));
        if (s) c.d->AddCircleFilled({la.x + 22, y + 8}, 2, Col(110, 150, 210));
        Text(c.d, s ? fBold : fReg, {la.x + 31, y + 1}, Gray(s ? 230 : 160), name);
        y += 15;
    }
    c.d->PopClipRect();
    c.y = c.b.y - 43;
    if (Button(c, "Reset all", listSel != 0, 20, w)) listSel = 0;
}

void InfoBody(Ctx& c)
{
    ImVec2 a = c.a + ImVec2(20, 20), b = a + ImVec2(63, 63);
    c.d->AddRectFilled(a, b, Col(10, 10, 10));
    c.d->AddRectFilled(a + ONE, b - ONE, Gray(40));
    c.d->AddRectFilled(a + ONE * 2, b - ONE * 2, Gray(15));
    Text(c.d, fBold, {a.x + 28, a.y + 25}, Gray(205), "?");
    char name[16];
    snprintf(name, sizeof name, "Item %d", listSel + 1);
    Text(c.d, fBold, {c.a.x + 89, c.a.y + 19}, Gray(230), name);
    Text(c.d, fReg, {c.a.x + 89, c.a.y + 30}, Col(152, 194, 34), "Active");
}

void AdjustBody(Ctx& c)
{
    static bool o[6];
    static ImVec4 col[2] = {{0.20f, 0.43f, 0.39f, 1}, {0.47f, 0.12f, 0.13f, 1}};
    static const char* const names[] = {"Mark as favourite", "High priority", "Hide", "Disable effects", "Prefer compact", "Override selection"};
    bool en = listSel != 0;                                     // nothing to adjust on the first (own) entry
    ImGui::SetCursorScreenPos({c.a.x + LX + 1, c.a.y + 15});
    ImGui::SetNextItemWidth(WideW(c));
    ImGui::InputTextWithHint("##rename", "Rename item", listRename, sizeof listRename);
    c.y = c.a.y + 41;
    for (int i = 0; i < 6; i++) Checkbox(c, names[i], &o[i], nullptr, i < 2 ? &col[i] : nullptr, en);
}

void ActionsBody(Ctx& c)
{
    c.y = c.a.y + 15;
    Button(c, "Copy ID", listSel != 0);
    Button(c, "Remove from list", listSel != 0);
}

void PresetsBody(Ctx& c)
{
    static int preset;
    static const char* const names[] = {"Default", "Preset 1", "Preset 2"};
    Combo(c, "Preset", &preset, names, 3);
    Button(c, "Load");
    Button(c, "Save");
    Button(c, "Import from clipboard");
    Button(c, "Export to clipboard");
}

void SettingsBody(Ctx& c)
{
    static bool key = true, accent = true;
    static ImVec4 col{0.60f, 0.76f, 0.13f, 1};
    Checkbox(c, "Menu key", &key, &kMenu);
    Checkbox(c, "Menu color", &accent, nullptr, &col);
}

void BuildPages()
{
    pages[0][0].col[0] = {{"Controls", ControlsBody}};
    pages[0][0].col[1] = {{"Options", OptionsBody}};
    pages[1][0].col[0] = {{"General", GeneralBody}};
    pages[1][0].col[1] = {{"Other", OtherBody}};
    for (int i = 1; i < 6; i++)
    {
        pages[1][i].col[0] = {{"Category", CategoryBody}};
        pages[1][i].col[1] = {{"Extras", ExtrasBody}};
    }
    pages[2][0].col[0] = {{"Tweaks", TweaksBody}};
    pages[2][0].col[1] = {{"Experimental", ExperimentalBody}};
    pages[2][1] = pages[1][1];
    pages[3][0].col[0] = {{"Items", ItemsBody}};
    pages[3][0].col[1] = {{"Information", InfoBody, 108}, {"Adjustments", AdjustBody, 262}, {"Actions", ActionsBody}};
    pages[4][0].col[0] = {{"Presets", PresetsBody}};
    pages[4][0].col[1] = {{"Settings", SettingsBody}};
    pages[5][0].col[0] = {{"Extras", ExtrasBody}};
    pages[5][0].col[1] = {{"Experimental", ExperimentalBody}};
}

// ---- the three windows ---------------------------------------------------------------------------

void MainWindow()
{
    Win& w = wMain;
    ImDrawList* d = BeginWin(w);
    ImVec2 p = w.pos, q = p + w.size;

    // sidebar
    static const char* const glyph[6] = {"X", "E", nullptr, nullptr, "L", nullptr};
    static const int icon[6] = {I_NONE, I_NONE, I_GEARS, I_PERSON, I_NONE, I_BALL};
    static const float iconScale[6] = {0, 0, 1.45f, 1.6f, 0, 1.75f};
    float sx = p.x + 79;
    d->AddRectFilled({p.x + 6, p.y + 10}, {sx, q.y - 6}, IM_COL32(11, 11, 11, 255));
    d->AddRectFilled({sx, p.y + 10}, {sx + 1, q.y - 6}, IM_COL32(0, 0, 0, 255));
    d->AddRectFilled({sx + 1, p.y + 10}, {sx + 2, q.y - 6}, LINE);
    d->PushClipRect({p.x + 6, p.y + 10}, q - ONE * 6, true);
    for (int i = 0; i < 6; i++)
    {
        ImVec2 a{p.x + 6, p.y + 20 + i * 64.f}, b{sx + 2, a.y + 64};
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##tab", {sx - a.x, 64})) tab = i;
        ImGui::PopID();
        bool sel = tab == i;
        if (sel)
        {
            Pattern(d, a, b);                                   // the selected tab opens into the content area
            d->AddRectFilled({a.x, a.y - 2}, {sx + 1, a.y - 1}, IM_COL32(0, 0, 0, 255));
            d->AddRectFilled({a.x, a.y - 1}, {sx + 2, a.y}, LINE);
            d->AddRectFilled({a.x, b.y}, {sx + 2, b.y + 1}, LINE);
            d->AddRectFilled({a.x, b.y + 1}, {sx + 1, b.y + 2}, IM_COL32(0, 0, 0, 255));
        }
        int g = sel ? 255 : ImGui::IsItemHovered() ? 150 : 100;
        ImVec2 c{floorf((a.x + sx) / 2), a.y + 32};
        if (glyph[i]) { ImVec2 ts = Size(fTab, glyph[i]); Text(d, fTab, {floorf(c.x - ts.x / 2), floorf(c.y - ts.y / 2)}, IM_COL32(g, g, g, 255), glyph[i]); }
        else Icon(d, icon[i], c, iconScale[i], IM_COL32(g, g, g, 255), IM_COL32(11, 11, 11, 255));
    }
    d->PopClipRect();

    // content, fading in after a tab change
    static int lastKey = -1;
    static float fade = 1;
    int key = tab * 8 + sub[tab];
    if (key != lastKey) { lastKey = key; fade = 0; }
    fade = std::min(1.f, fade + ImGui::GetIO().DeltaTime * 5);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
    ImGui::PushID(key);
    d->PushClipRect({sx + 2, p.y + 10}, q - ONE * 6, true);
    ImVec2 a{p.x + 103, p.y + 31}, b{q.x - 28, q.y - 27};
    if (strips[tab].n) { Strip(d, a, {b.x, a.y + 57}, strips[tab], &sub[tab]); a.y += 80; }
    Layout(d, pages[tab][sub[tab]], a, b);
    d->PopClipRect();
    ImGui::PopID();
    ImGui::PopStyleVar();
    EndWin(w);
}

void ConsoleWindow()
{
    struct Line { int kind; const char* text; };                // 0 info, 1 warning, 2 error
    static const Line lines[] = {
        {2, "io: read refused for cache (ACCESS_DENIED)"},
        {1, "core: resolve took 42 ms (expected under 5)"},
        {2, "cfg: 'profile' not found in build 2.4.1 -- defaults used"},
        {0, "Rescanning module core.dll (0x7FF6C1200000, 94.2 MB)"},
        {2, "lua: scripts/ui.lua:118: attempt to index a nil value (field 'items')"},
        {1, "overlay: window vanished, waiting"},
        {2, "net: catalogue request timed out after 10 s"},
    };
    const int n = sizeof lines / sizeof *lines;
    Win& w = wConsole;
    ImDrawList* d = BeginWin(w);
    ImVec2 a = w.pos + ImVec2(23, 30), b = w.pos + w.size - ImVec2(25, 26);
    GroupBox(d, a, b, "Console");
    ImVec2 pa = a + ImVec2(17, 19), pb = b - ImVec2(17, 15);
    d->AddRectFilled(pa, pb, BLACK);
    d->AddRectFilled(pa + ONE, pb - ONE, IM_COL32(32, 32, 32, 255));
    d->PushClipRect(pa + ONE, pb - ONE, true);
    int visible = std::max(1, (int)((pb.y - pa.y - 8) / 18));
    float y = pa.y + 7;
    for (int i = std::max(0, n - visible); i < n; i++, y += 18)   // when short, keep the newest lines in view
    {
        const Line& l = lines[i];
        ImU32 col = l.kind == 2 ? IM_COL32(226, 62, 72, 255) : l.kind == 1 ? IM_COL32(222, 186, 96, 255) : IM_COL32(205, 205, 205, 255);
        TextEllipsis(d, l.kind == 2 ? fBold : fReg, {pa.x + 12, y}, col, l.text, pb.x - pa.x - 24);
    }
    d->PopClipRect();
    EndWin(w);
}

void KeysWindow()
{
    const struct { const char* label; ImGuiKey key; bool lit; } binds[] = {{"Enabled: ", kEnabled, kEnabled != ImGuiKey_None && ImGui::IsKeyDown(kEnabled)}, {"Menu key: ", kMenu, true}};
    Win& w = wKeys;
    ImDrawList* d = BeginWin(w);
    ImVec2 a = w.pos + ImVec2(23, 28), b = w.pos + w.size - ImVec2(23, 25);
    GroupBox(d, a, b, "Key binds");
    d->PushClipRect(a + ONE, b - ONE, true);
    float y = a.y + 15;
    for (const auto& bd : binds)
    {
        if (bd.key == ImGuiKey_None) continue;                   // cleared binds are not listed
        Text(d, fReg, {a.x + 19, y}, IM_COL32(205, 205, 205, 255), bd.label);
        Text(d, fReg, {a.x + 19 + Size(fReg, bd.label).x, y}, bd.lit ? IM_COL32(178, 200, 80, 255) : IM_COL32(110, 110, 110, 255), KeyName(bd.key));
        y += 11;
    }
    d->PopClipRect();
    EndWin(w);
}

// shortcut: fonts come straight from C:\Windows\Fonts; embed them if this has to run anywhere else.
ImFont* Font(const char* file, float size)
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    char path[260];
    snprintf(path, sizeof path, "C:\\Windows\\Fonts\\%s", file);
    FILE* f = fopen(path, "rb");
    if (f) fclose(f);
    return f ? io.Fonts->AddFontFromFileTTF(path, size, &cfg) : io.Fonts->AddFontDefault();
}
} // namespace

void Menu::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    fReg = Font("verdana.ttf", 12);
    fBold = Font("verdanab.ttf", 12);
    fBoldSm = Font("verdanab.ttf", 10);
    fSmall = Font("verdana.ttf", 9);
    fTab = Font("segoeui.ttf", 52);

    // Bake the 4x4 dot tile into the font atlas so it works with any renderer backend.
    int id = io.Fonts->AddCustomRectRegular((int)PAT, (int)PAT);
    unsigned char* px;
    int tw, th;
    io.Fonts->GetTexDataAsRGBA32(&px, &tw, &th);
    const ImFontAtlasCustomRect* r = io.Fonts->GetCustomRectByIndex(id);
    static const char tile[4][5] = {"LLDL", "DLDL", "DLLL", "DLDL"};
    for (int y = 0; y < (int)PAT; y++)
        for (int x = 0; x < (int)PAT; x++)
            ((ImU32*)px)[(r->Y + y) * tw + r->X + x] = tile[y & 3][x & 3] == 'L' ? IM_COL32(18, 18, 18, 255) : IM_COL32(11, 11, 11, 255);
    io.Fonts->CalcCustomRectUV(r, &patUV0, &patUV1);

    ImGuiStyle& st = ImGui::GetStyle();                         // only the stock text inputs use these
    st.FrameRounding = 0;
    st.FrameBorderSize = 1;
    st.FramePadding = {4, 2};
    st.Colors[ImGuiCol_FrameBg] = st.Colors[ImGuiCol_FrameBgHovered] = st.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.085f, 0.085f, 0.085f, 1);
    st.Colors[ImGuiCol_Border] = ImVec4(0.17f, 0.17f, 0.17f, 1);
    st.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.80f, 1);
    st.Colors[ImGuiCol_TextDisabled] = ImVec4(0.30f, 0.30f, 0.30f, 1);
    st.PopupRounding = 0;
    st.Colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.09f, 1);
    st.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.60f, 0.76f, 0.13f, 0.35f);

    BuildPages();
}

void Menu::Render()
{
    static bool visible = true;
    if (kMenu != ImGuiKey_None && !listening && ImGui::IsKeyPressed(kMenu, false) && !ImGui::GetIO().WantTextInput) visible = !visible;
    if (visible) MainWindow();
    if (visible && wConsole.open) ConsoleWindow();
    if (wKeys.open) KeysWindow();
}
