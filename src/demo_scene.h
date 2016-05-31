#pragma once

#include "demo_common.h"

class CScene
{
public:
    ~CScene();
    bool Load(const char *meshFile, const char *imageFolder,
              ID3D12GraphicsCommandList *cmdList,
              eastl::vector<ID3D12Resource *> *uploadBuffers);
    void Render();

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

    ID3D12Resource *VertexBuffer = nullptr;
    ID3D12Resource *IndexBuffer = nullptr;
};
