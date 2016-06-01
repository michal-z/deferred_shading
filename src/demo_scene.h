#pragma once

#include "demo_common.h"

struct aiScene;

class CScene
{
public:
    ID3D12RootSignature *RootSignature;

    ~CScene();
    bool Load(const char *meshFile, const char *imageFolder,
              ID3D12GraphicsCommandList *cmdList,
              eastl::vector<ID3D12Resource *> *uploadBuffers);
    void Render(ID3D12GraphicsCommandList *cmdList);

private:
    struct SMeshVertex
    {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT2 Texcoord;
        XMFLOAT3 Tangent;
        XMFLOAT3 Bitangent;
    };
    typedef uint32_t SMeshIndex;

    struct SMeshSection
    {
        uint32_t NumVertices = 0;
        uint32_t NumIndices = 0;
        ID3D12Resource *DiffuseTexture = nullptr;
    };
    eastl::vector<SMeshSection> MeshSections;

    ID3D12Device *Device = nullptr;
    ID3D12Resource *VertexBuffer = nullptr;
    ID3D12Resource *IndexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

    bool LoadGeometry(const aiScene *scene, ID3D12GraphicsCommandList *cmdList,
                      eastl::vector<ID3D12Resource *> *uploadBuffers);
};
