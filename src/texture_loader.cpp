#include "texture_loader.h"
#include <windows.h>
#include <wincodec.h>
#include <iostream>
#include <vector>

TextureLoader& TextureLoader::Instance() {
    static TextureLoader instance;
    return instance;
}

bool TextureLoader::Init(ID3D11Device* device) {
    m_device = device;
    HRESULT hr = CoInitialize(NULL);
    return SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;
}

void TextureLoader::Shutdown() {
    for (auto& kv : m_cache) {
        if (kv.second.srv) {
            kv.second.srv->Release();
        }
    }
    m_cache.clear();
    m_device = nullptr;
}

TextureData TextureLoader::LoadTexture(const std::string& filepath) {
    auto it = m_cache.find(filepath);
    if (it != m_cache.end()) {
        return it->second;
    }

    TextureData result = { nullptr, 0, 0 };
    if (!m_device) return result;

    // Convert UTF-8 path to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return result;
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, &wpath[0], wlen);

    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, (LPVOID*)&pFactory);
    if (FAILED(hr)) return result;

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromFilename(wpath.c_str(), NULL, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        pFactory->Release();
        return result;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) {
        pDecoder->Release();
        pFactory->Release();
        return result;
    }

    UINT width = 0, height = 0;
    pFrame->GetSize(&width, &height);

    IWICFormatConverter* pConverter = nullptr;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return result;
    }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA,
                               WICBitmapDitherTypeNone, NULL, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        pConverter->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return result;
    }

    std::vector<BYTE> buffer(width * height * 4);
    hr = pConverter->CopyPixels(NULL, width * 4, (UINT)buffer.size(), buffer.data());
    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();

    if (FAILED(hr)) return result;

    // Create Direct3D 11 Texture2D
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = buffer.data();
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTexture = nullptr;
    hr = m_device->CreateTexture2D(&desc, &subResource, &pTexture);
    if (FAILED(hr)) return result;

    // Create Shader Resource View
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = m_device->CreateShaderResourceView(pTexture, &srvDesc, &result.srv);
    pTexture->Release();

    if (SUCCEEDED(hr)) {
        result.width = (int)width;
        result.height = (int)height;
        m_cache[filepath] = result;
    }
    return result;
}

void TextureLoader::ReleaseTexture(const std::string& filepath) {
    auto it = m_cache.find(filepath);
    if (it != m_cache.end()) {
        if (it->second.srv) {
            it->second.srv->Release();
        }
        m_cache.erase(it);
    }
}
