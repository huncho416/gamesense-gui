// Dark "dotted" ImGui menu: custom chrome, sidebar tabs, group boxes and widgets.
// UI only - every control edits a local variable, nothing else.
#pragma once

namespace Menu
{
    void Init();    // once, after ImGui::CreateContext() and before the first frame (loads fonts)
    void Render();  // every frame, between ImGui::NewFrame() and ImGui::Render()

    extern int tab;     // selected sidebar tab, 0..5
    extern int sub[6];  // selected icon in each tab's top strip
}
