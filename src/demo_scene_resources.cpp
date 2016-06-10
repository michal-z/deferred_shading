#include "demo_scene_resources.h"
#include "demo_lib.h"
#include "demo_mipmaps.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

bool CSceneResources::Init(const char *meshFile, const char *imageFolder,
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

    if (!LoadData(importedScene, imageFolder, cmdList, uploadBuffers)) return false;

    return true;
}

void CSceneResources::Deinit()
{
    // TODO: Implement this
}

bool CSceneResources::LoadData(const aiScene *importedScene, const char *imageFolder,
                               ID3D12GraphicsCommandList *cmdList,
                               eastl::vector<ID3D12Resource *> *uploadBuffers)
{
    MeshSections.resize(importedScene->mNumMeshes);

    HRESULT hr;
    D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
    descHeap.NumDescriptors = (UINT)MeshSections.size();
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hr = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&SrvHeap));
    if (FAILED(hr)) return false;


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

    const uint32_t descriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = SrvHeap->GetCPUDescriptorHandleForHeapStart();

    for (size_t m = 0; m < MeshSections.size(); ++m)
    {
        aiMesh *mesh = importedScene->mMeshes[m];

        aiMaterial *mat = importedScene->mMaterials[mesh->mMaterialIndex];
        aiString path;
        char fullPath[kNumTexturesPerMesh][256];

        mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
        strcpy(fullPath[0], imageFolder);
        strcat(fullPath[0], path.data);

        for (uint32_t t = 0; t < kNumTexturesPerMesh; ++t)
        {
            MeshSections[m].Textures[t] = LoadTexture(fullPath[t], srvHandle, cmdList, uploadBuffers);
            srvHandle.ptr += descriptorSize;
        }

        XMFLOAT3 minCorner = XMFLOAT3(1000.0f, 1000.0f, 1000.0f);
        XMFLOAT3 maxCorner = XMFLOAT3(-1000.0f, -1000.0f, -1000.0f);
        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            vtxptr[v].Position.x = 10.0f * mesh->mVertices[v].x;
            vtxptr[v].Position.y = 10.0f * mesh->mVertices[v].y;
            vtxptr[v].Position.z = 10.0f * mesh->mVertices[v].z;

            if (vtxptr[v].Position.x < minCorner.x) minCorner.x = vtxptr[v].Position.x;
            if (vtxptr[v].Position.y < minCorner.y) minCorner.y = vtxptr[v].Position.y;
            if (vtxptr[v].Position.z < minCorner.z) minCorner.z = vtxptr[v].Position.z;

            if (vtxptr[v].Position.x > maxCorner.x) maxCorner.x = vtxptr[v].Position.x;
            if (vtxptr[v].Position.y > maxCorner.y) maxCorner.y = vtxptr[v].Position.y;
            if (vtxptr[v].Position.z > maxCorner.z) maxCorner.z = vtxptr[v].Position.z;
        }
        MeshSections[m].BBox.Center.x = 0.5f * (maxCorner.x + minCorner.x);
        MeshSections[m].BBox.Center.y = 0.5f * (maxCorner.y + minCorner.y);
        MeshSections[m].BBox.Center.z = 0.5f * (maxCorner.z + minCorner.z);

        MeshSections[m].BBox.Extents.x = 0.5f * (maxCorner.x - minCorner.x);
        MeshSections[m].BBox.Extents.y = 0.5f * (maxCorner.y - minCorner.y);
        MeshSections[m].BBox.Extents.z = 0.5f * (maxCorner.z - minCorner.z);

        assert(MeshSections[m].BBox.Extents.x > 0.0f);
        assert(MeshSections[m].BBox.Extents.y > 0.0f);
        assert(MeshSections[m].BBox.Extents.z > 0.0f);

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

ID3D12Resource *CSceneResources::LoadTexture(const char *filename, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle,
                                             ID3D12GraphicsCommandList *cmdList,
                                             eastl::vector<ID3D12Resource *> *uploadBuffers)
{
    int32_t imgW, imgH, imgN;
    uint8_t *imgPix = stbi_load(filename, &imgW, &imgH, &imgN, 4);
    if (!imgPix) return nullptr;

    HRESULT hr;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = (UINT64)imgW;
    textureDesc.Height = (UINT)imgH;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 0;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ID3D12Resource *texture;
    hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&texture));
    if (FAILED(hr))
    {
        stbi_image_free(imgPix);
        return nullptr;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    uint32_t numRows;
    uint64_t rowSize, bufferSize;
    Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, &numRows, &rowSize, &bufferSize);

    ID3D12Resource *uploadBuffer = Lib::CreateUploadBuffer(Device, bufferSize);
    if (uploadBuffer == nullptr)
    {
        texture->Release();
        stbi_image_free(imgPix);
        return nullptr;
    }
    uploadBuffers->push_back(uploadBuffer);

    D3D12_RANGE range = {};
    uint8_t *ptr;
    uploadBuffer->Map(0, &range, (void **)&ptr);
    for (uint32_t row = 0; row < numRows; ++row)
    {
        memcpy(ptr + row * rowSize, imgPix + row * imgW * 4, imgW * 4);
    }
    uploadBuffer->Unmap(0, nullptr);
    stbi_image_free(imgPix);

    D3D12_TEXTURE_COPY_LOCATION copyDst = {};
    copyDst.pResource = texture;
    copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION copySrc = {};
    copySrc.pResource = uploadBuffer;
    copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrc.PlacedFootprint = layout;
    cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);


    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = -1;
    Device->CreateShaderResourceView(texture, &srvDesc, srvHandle);

    return texture;
}
