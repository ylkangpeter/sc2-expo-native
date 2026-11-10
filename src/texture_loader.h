#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>

struct TextureData {
    ID3D11ShaderResourceView* srv = nullptr;
    int width = 0;
    int height = 0;
};

class TextureLoader {
public:
    static TextureLoader& Instance();

    bool Init(ID3D11Device* device);
    void Shutdown();

    // Loads image from file (PNG, WebP, JPG, BMP) and caches it.
    TextureData LoadTexture(const std::string& filepath);
    void ReleaseTexture(const std::string& filepath);

private:
    TextureLoader() = default;
    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::string, TextureData> m_cache;
};
