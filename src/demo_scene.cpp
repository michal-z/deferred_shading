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


    Assimp::Importer sceneImporter;
    sceneImporter.SetPropertyInteger(AI_CONFIG_PP_PTV_NORMALIZE, 1);

    const aiScene *importedScene = sceneImporter.ReadFile(meshFile, 0);
    if (!importedScene) return false;

    const uint32_t ppFlags = aiProcess_CalcTangentSpace | aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials |
        aiProcess_PreTransformVertices | aiProcess_OptimizeMeshes | aiProcess_FlipUVs |
        aiProcess_MakeLeftHanded;

    importedScene = sceneImporter.ApplyPostProcessing(ppFlags);

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

    ID3D12Device *device;
    cmdList->GetDevice(IID_PPV_ARGS(&device));
    assert(device);

    ID3D12Resource *uploadVBuf = Lib::CreateUploadBuffer(device, totalNumVertices * sizeof(SMeshVertex));
    if (!uploadVBuf) return false;
    uploadBuffers->push_back(uploadVBuf);

    ID3D12Resource *uploadIBuf = Lib::CreateUploadBuffer(device, totalNumIndices * sizeof(SMeshIndex));
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
        aiMaterial *mat = importedScene->mMaterials[mesh->mMaterialIndex];

        aiString path;
        mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);

        char fullPath[256];
        strcpy(fullPath, imageFolder);
        strcat(fullPath, path.data);

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
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&VertexBuffer));
    if (FAILED(hr)) return false;

    bufferDesc.Width = totalNumIndices * sizeof(SMeshIndex);
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&IndexBuffer));
    if (FAILED(hr)) return false;

    return true;
}

void CScene::Render()
{
}
