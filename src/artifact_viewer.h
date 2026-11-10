#pragma once

#include <windows.h>
#include <string>
#include <vector>

class ArtifactViewer {
public:
    static ArtifactViewer& Instance();

    void Init(HINSTANCE hInstance);
    void Shutdown();

    void ShowMap(const std::string& map_name);
    void Hide();
    void Toggle(const std::string& map_name);
    bool IsVisible() const { return m_visible; }

    bool HasArtifact(const std::string& map_name) const;
    std::string ResolveArtifactMap(const std::string& input_map) const;
    void ShowNextMap();
    const std::string& GetCurrentArtifactMap() const { return m_last_map; }

private:
    ArtifactViewer() = default;
    ~ArtifactViewer() { Shutdown(); }

    bool RenderToLayeredWindow(const std::string& map_name);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    bool m_visible = false;
    std::string m_last_map = "死亡摇篮";
};
