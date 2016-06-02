#include "demo_scene.h"
#include "demo_lib.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

CScene::~CScene()
{
}

bool CScene::Load(const char *meshFile, const char *imageFolder,
                  ID3D12GraphicsCommandList *cmdList,
                  eastl::vector<ID3D12Resource *> *uploadBuffers)
{
    assert(meshFile && imageFolder && cmdList && uploadBuffers);

    cmdList->GetDevice(IID_PPV_ARGS(&Device));
    assert(Device);

    Assimp::Importer sceneImporter;
    sceneImporter.SetPropertyInteger(AI_CONFIG_PP_PTV_NORMALIZE, 1);

    const aiScene *importedScene = sceneImporter.ReadFile(meshFile, 0);
    if (!importedScene) return false;

    const uint32_t ppFlags = aiProcess_CalcTangentSpace | aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials |
        aiProcess_PreTransformVertices | aiProcess_OptimizeMeshes | aiProcess_FlipUVs |
        aiProcess_MakeLeftHanded;

    importedScene = sceneImporter.ApplyPostProcessing(ppFlags);

    if (!LoadGeometry(importedScene, cmdList, uploadBuffers)) return false;

    return true;
}

void CScene::Render(ID3D12GraphicsCommandList *cmdList)
{
    assert(cmdList);
    assert(VertexBuffer);
    assert(IndexBuffer);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &VertexBufferView);
    cmdList->IASetIndexBuffer(&IndexBufferView);

    uint32_t startIndex = 0;
    int32_t baseVertex = 0;

    for (size_t i = 0; i < MeshSections.size(); ++i)
    {
        cmdList->DrawIndexedInstanced(MeshSections[i].NumIndices, 1, startIndex, baseVertex, 0);
        startIndex += MeshSections[i].NumIndices;
        baseVertex += MeshSections[i].NumVertices;
    }
}

bool CScene::LoadGeometry(const aiScene *importedScene, ID3D12GraphicsCommandList *cmdList,
                          eastl::vector<ID3D12Resource *> *uploadBuffers)
{
    MeshSections.resize(importedScene->mNumMeshes);

    uint32_t totalNumVertices = 0;
    uint32_t totalNumIndices = 0;

    for (size_t i = 0; i < MeshSections.size(); ++i)
    {
        MeshSections[i].NumVertices = importedScene->mMeshes[i]->mNumVertices;
        MeshSections[i].NumIndices = importedScene->mMeshes[i]->mNumFaces * 3;

        totalNumVertices += MeshSections[i].NumVertices;
        totalNumIndices += MeshSections[i].NumIndices;
    }

    ID3D12Resource *uploadVBuf = Lib::CreateUploadBuffer(Device, totalNumVertices * sizeof(SMeshVertex));
    if (!uploadVBuf) return false;
    uploadBuffers->push_back(uploadVBuf);

    ID3D12Resource *uploadIBuf = Lib::CreateUploadBuffer(Device, totalNumIndices * sizeof(SMeshIndex));
    if (!uploadIBuf) return false;
    uploadBuffers->push_back(uploadIBuf);

    D3D12_RANGE mapRange = {};
    SMeshVertex *vtxptr;
    uploadVBuf->Map(0, &mapRange, (void **)&vtxptr);

    SMeshIndex *idxptr;
    uploadIBuf->Map(0, &mapRange, (void **)&idxptr);

    for (size_t m = 0; m < MeshSections.size(); ++m)
    {
        aiMesh *mesh = importedScene->mMeshes[m];
        /*
        aiMaterial *mat = importedScene->mMaterials[mesh->mMaterialIndex];

        aiString path;
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);

        char fullPath[256];
        strcpy(fullPath, imageFolder);
        strcat(fullPath, path.data);
        */

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            vtxptr[v].Position.x = 10.0f * mesh->mVertices[v].x;
            vtxptr[v].Position.y = 10.0f * mesh->mVertices[v].y;
            vtxptr[v].Position.z = 10.0f * mesh->mVertices[v].z;
        }
        if (mesh->HasNormals())
        {
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
            {
                vtxptr[v].Normal.x = mesh->mNormals[v].x;
                vtxptr[v].Normal.y = mesh->mNormals[v].y;
                vtxptr[v].Normal.z = mesh->mNormals[v].z;
            }
        }
        if (mesh->HasTextureCoords(0))
        {
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
            {
                vtxptr[v].Texcoord.x = mesh->mTextureCoords[0][v].x;
                vtxptr[v].Texcoord.y = mesh->mTextureCoords[0][v].y;
            }
        }
        if (mesh->HasTangentsAndBitangents())
        {
            for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
            {
                vtxptr[v].Tangent.x = mesh->mTangents[v].x;
                vtxptr[v].Tangent.y = mesh->mTangents[v].y;
                vtxptr[v].Tangent.z = mesh->mTangents[v].z;
                vtxptr[v].Bitangent.x = mesh->mBitangents[v].x;
                vtxptr[v].Bitangent.y = mesh->mBitangents[v].y;
                vtxptr[v].Bitangent.z = mesh->mBitangents[v].z;
            }
        }
        vtxptr += mesh->mNumVertices;

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            idxptr[f * 3] = mesh->mFaces[f].mIndices[0];
            idxptr[f * 3 + 1] = mesh->mFaces[f].mIndices[1];
            idxptr[f * 3 + 2] = mesh->mFaces[f].mIndices[2];
        }
        idxptr += mesh->mNumFaces * 3;
    }
    uploadVBuf->Unmap(0, nullptr);
    uploadIBuf->Unmap(0, nullptr);

    HRESULT hr;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Height = bufferDesc.DepthOrArraySize = bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    bufferDesc.Width = totalNumVertices * sizeof(SMeshVertex);
    hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&VertexBuffer));
    if (FAILED(hr)) return false;

    bufferDesc.Width = totalNumIndices * sizeof(SMeshIndex);
    hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&IndexBuffer));
    if (FAILED(hr)) return false;

    cmdList->CopyBufferRegion(VertexBuffer, 0, uploadVBuf, 0, totalNumVertices * sizeof(SMeshVertex));
    cmdList->CopyBufferRegion(IndexBuffer, 0, uploadIBuf, 0, totalNumIndices * sizeof(SMeshIndex));

    D3D12_RESOURCE_BARRIER barrier[2] = {};
    barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[0].Transition.pResource = VertexBuffer;
    barrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    barrier[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[1].Transition.pResource = IndexBuffer;
    barrier[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;

    cmdList->ResourceBarrier(_countof(barrier), barrier);

    VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(SMeshVertex);
    VertexBufferView.SizeInBytes = totalNumVertices * sizeof(SMeshVertex);

    IndexBufferView.BufferLocation = IndexBuffer->GetGPUVirtualAddress();
    IndexBufferView.Format = sizeof(SMeshIndex) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    IndexBufferView.SizeInBytes = totalNumIndices * sizeof(SMeshIndex);

    return true;
}
