﻿#include "demo.h"
#include <new>
#include "EABase/eabase.h"
#include "demo_imgui_renderer.h"
#include "demo_scene_resources.h"
#include "demo_lib.h"
#include "demo_mipmaps.h"
#include "imgui.h"

void *operator new[](size_t size, const char* /*name*/, int /*flags*/,
                     unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return malloc(size);
}

void *operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*name*/,
                     int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return _aligned_offset_malloc(size, alignment, alignmentOffset);
}

CDemo::~CDemo()
{
    // wait for the GPU
    if (CmdQueue && FrameFence && FrameFenceEvent)
    {
        CmdQueue->Signal(FrameFence, CpuCompletedFences + 10);
        FrameFence->SetEventOnCompletion(CpuCompletedFences + 10, FrameFenceEvent);
        WaitForSingleObject(FrameFenceEvent, INFINITE);
    }

    delete SceneResources;
    delete GuiRenderer;

    if (FrameFenceEvent)
    {
        CloseHandle(FrameFenceEvent);
    }
    SAFE_RELEASE(SwapChain);
    SAFE_RELEASE(CmdQueue);
    SAFE_RELEASE(Device);
    SAFE_RELEASE(FactoryDXGI);
}

bool CDemo::Init()
{
    if (!InitWindowAndD3D12()) return false;


    HRESULT hr;

    hr = Device->CreateCommandList(1, D3D12_COMMAND_LIST_TYPE_DIRECT, FrameResources[0].CmdAllocator,
                                   nullptr, IID_PPV_ARGS(&CmdList));
    if (FAILED(hr)) return false;

    eastl::vector<ID3D12Resource *> uploadBuffers;

    GuiRenderer = new CGuiRenderer;
    if (!GuiRenderer->Init(kNumBufferedFrames, CmdList, &uploadBuffers)) return false;

    SceneResources = new CSceneResources;
    if (!SceneResources->Init("data/sponza/Sponza.fbx", "data/sponza/", CmdList, &uploadBuffers)) return false;

    CmdList->Close();
    ID3D12CommandList *cmdLists[] = { (ID3D12CommandList *)CmdList };
    CmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    CmdQueue->Signal(FrameFence, ++CpuCompletedFences);
    FrameFence->SetEventOnCompletion(CpuCompletedFences, FrameFenceEvent);
    WaitForSingleObject(FrameFenceEvent, INFINITE);

    for (size_t i = 0; i < uploadBuffers.size(); ++i)
    {
        uploadBuffers[i]->Release();
    }

    CMipmapGenerator *mipmapGen = new CMipmapGenerator;
    if (!mipmapGen->Init(Device)) return false;

    for (size_t i = 0; i < SceneResources->MeshSections.size(); ++i)
    {
        CmdList->Reset(FrameResources[0].CmdAllocator, nullptr);

        mipmapGen->Generate(SceneResources->MeshSections[i].Textures[0], CmdList);

        CmdList->Close();
        ID3D12CommandList *cmdLists[] = { (ID3D12CommandList *)CmdList };
        CmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

        CmdQueue->Signal(FrameFence, ++CpuCompletedFences);
        FrameFence->SetEventOnCompletion(CpuCompletedFences, FrameFenceEvent);
        WaitForSingleObject(FrameFenceEvent, INFINITE);
    }
    delete mipmapGen;

    if (!InitPipelineStates()) return false;
    if (!InitConstantBuffers()) return false;
    if (!InitDescriptorHeaps()) return false;

    return true;
}

void CDemo::Update()
{
    UpdateFrameStats();

    float sinv, cosv;
    XMScalarSinCos(&sinv, &cosv, (float)(0.25 * Time));

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PI / 3, (float)Resolution[0] / Resolution[1],
                                             0.1f, 20.0f);
    XMVECTOR cameraPosition = XMVectorSet(2.0f*cosv, 0.5f, -2.0f*sinv, 1.0f);
    XMMATRIX view = XMMatrixLookAtLH(cameraPosition,
                                     XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
                                     XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    XMVECTOR cameraOrientation = XMQuaternionRotationMatrix(XMMatrixInverse(nullptr, view));

    BFrustum = BoundingFrustum(proj);
    BFrustum.Transform(BFrustum, 1.0f, cameraOrientation, cameraPosition);

    XMStoreFloat4x4A((XMFLOAT4X4A *)FrameResources[FrameIndex].PerFrameCbPtr,
                     XMMatrixTranspose(view * proj));

    ImGuiIO &io = ImGui::GetIO();
    io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    io.DeltaTime = TimeDelta;

    ImGui::NewFrame();

    Render();

    SwapChain->Present(0, 0);

    CmdQueue->Signal(FrameFence, ++CpuCompletedFences);

    const uint64_t gpuCompletedFences = FrameFence->GetCompletedValue();

    if ((CpuCompletedFences - gpuCompletedFences) >= kNumBufferedFrames)
    {
        FrameFence->SetEventOnCompletion(gpuCompletedFences + 1, FrameFenceEvent);
        WaitForSingleObject(FrameFenceEvent, INFINITE);
    }

    BackBufferIndex = SwapChain->GetCurrentBackBufferIndex();
    FrameIndex = (uint32_t)(++FrameIndex % kNumBufferedFrames);
}

void CDemo::Render()
{
    assert(CmdList);

    SFrameResources *fres = &FrameResources[FrameIndex];
    fres->CmdAllocator->Reset();

    CmdList->Reset(fres->CmdAllocator, nullptr);
    CmdList->RSSetViewports(1, &Viewport);
    CmdList->RSSetScissorRects(1, &ScissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = SwapBuffers[BackBufferIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = 0;
    CmdList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = SwapBuffersHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += BackBufferIndex * DescriptorSizeRTV;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DsvHeap->GetCPUDescriptorHandleForHeapStart();
    CmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    CmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	CmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    RenderScene();

    ImGui::ShowTestWindow();
    GuiRenderer->Render(CmdList, FrameIndex);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    CmdList->ResourceBarrier(1, &barrier);

    CmdList->Close();
    ID3D12CommandList *cmdLists[] = { (ID3D12CommandList *)CmdList };
    CmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}

void CDemo::RenderScene()
{
    const SFrameResources *fres = &FrameResources[FrameIndex];

    CmdList->SetPipelineState(ScenePso);
    CmdList->SetDescriptorHeaps(1, &fres->GpuDescriptorHeap);
    CmdList->SetGraphicsRootSignature(StaticMeshRs);
    CmdList->SetGraphicsRootConstantBufferView(1, fres->PerFrameCb->GetGPUVirtualAddress());

    CmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    CmdList->IASetVertexBuffers(0, 1, &SceneResources->VertexBufferView);
    CmdList->IASetIndexBuffer(&SceneResources->IndexBufferView);

    uint32_t startIndex = 0;
    int32_t baseVertex = 0;
    
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        fres->GpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        fres->GpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    Device->CopyDescriptorsSimple(CSceneResources::kNumTexturesPerMesh * (UINT)SceneResources->MeshSections.size(),
                                  cpuHandle,
                                  SceneResources->SrvHeap->GetCPUDescriptorHandleForHeapStart(),
                                  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    for (size_t i = 0; i < SceneResources->MeshSections.size(); ++i)
    {
        const CSceneResources::SMeshSection &meshSection = SceneResources->MeshSections[i];

        if (BFrustum.Intersects(meshSection.BBox))
        {
            CmdList->SetGraphicsRootDescriptorTable(0, gpuHandle);
            CmdList->DrawIndexedInstanced(meshSection.NumIndices, 1, startIndex, baseVertex, 0);
        }

        gpuHandle.ptr += CSceneResources::kNumTexturesPerMesh * DescriptorSize;

        startIndex += meshSection.NumIndices;
        baseVertex += meshSection.NumVertices;
    }
}

LRESULT CALLBACK CDemo::Winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ImGuiIO &io = ImGui::GetIO();

    switch (msg)
    {
        case WM_LBUTTONDOWN:
            io.MouseDown[0] = true;
            return 0;
        case WM_LBUTTONUP:
            io.MouseDown[0] = false;
            return 0;
        case WM_RBUTTONDOWN:
            io.MouseDown[1] = true;
            return 0;
        case WM_RBUTTONUP:
            io.MouseDown[1] = false;
            return 0;
        case WM_MBUTTONDOWN:
            io.MouseDown[2] = true;
            return 0;
        case WM_MBUTTONUP:
            io.MouseDown[2] = false;
            return 0;
        case WM_MOUSEWHEEL:
            io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1.0f : -1.0f;
            return 0;
        case WM_MOUSEMOVE:
            io.MousePos.x = (signed short)(lparam);
            io.MousePos.y = (signed short)(lparam >> 16);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            {
                if (wparam < 256)
                {
                    io.KeysDown[wparam] = true;
                    if (wparam == VK_ESCAPE) PostQuitMessage(0);
                    return 0;
                }
            }
            break;
        case WM_KEYUP:
            {
                if (wparam < 256)
                {
                    io.KeysDown[wparam] = false;
                    return 0;
                }
            }
            break;
        case WM_CHAR:
            {
                if (wparam > 0 && wparam < 0x10000)
                {
                    io.AddInputCharacter((unsigned short)wparam);
                    return 0;
                }
            }
            break;
    }
    return DefWindowProc(win, msg, wparam, lparam);
}

void CDemo::UpdateFrameStats()
{
    static double prevTime = -1.0;
    static double prevFpsTime = 0.0;
    static int fpsFrame = 0;

    if (prevTime < 0.0)
    {
        prevTime = Lib::GetTime();
        prevFpsTime = prevTime;
    }

    Time = Lib::GetTime();
    TimeDelta = (float)(Time - prevTime);
    prevTime = Time;

    if ((Time - prevFpsTime) >= 1.0)
    {
        double fps = fpsFrame / (Time - prevFpsTime);
        double us = (1.0 / fps) * 1000000.0;
        char text[256];
        wsprintf(text, "[%d fps  %d us] %s", (int)fps, (int)us, kDemoName);
        SetWindowText(WinHandle, text);
        prevFpsTime = Time;
        fpsFrame = 0;
    }
    fpsFrame++;
}

bool CDemo::InitWindowAndD3D12()
{
    WNDCLASS winclass = {};
    winclass.lpfnWndProc = Winproc;
    winclass.hInstance = GetModuleHandle(nullptr);
    winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    winclass.lpszClassName = kDemoName;
    if (!RegisterClass(&winclass)) return false;

    RECT rect = { 0, 0, (LONG)Resolution[0], (LONG)Resolution[1] };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, FALSE))
        return false;

    WinHandle = CreateWindow(kDemoName, kDemoName,
                             WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top,
                             nullptr, nullptr, nullptr, 0);
    if (!WinHandle) return false;

    ImGuiIO &io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';
    io.ImeWindowHandle = WinHandle;
    io.RenderDrawListsFn = nullptr;
    io.DisplaySize = ImVec2((float)Resolution[0], (float)Resolution[1]);


    HRESULT res = CreateDXGIFactory1(IID_PPV_ARGS(&FactoryDXGI));
    if (FAILED(res)) return false;

#ifdef _DEBUG
    ID3D12Debug *dbg = nullptr;
    D3D12GetDebugInterface(IID_PPV_ARGS(&dbg));
    if (dbg)
    {
        dbg->EnableDebugLayer();
        SAFE_RELEASE(dbg);
    }
#endif

    res = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&Device));
    if (FAILED(res)) return false;

    D3D12_COMMAND_QUEUE_DESC descCmdQueue = {};
    descCmdQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    descCmdQueue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    descCmdQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    res = Device->CreateCommandQueue(&descCmdQueue, IID_PPV_ARGS(&CmdQueue));
    if (FAILED(res)) return false;

    DXGI_SWAP_CHAIN_DESC descSwapChain = {};
    descSwapChain.BufferCount = kNumSwapBuffers;
    descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descSwapChain.OutputWindow = WinHandle;
    descSwapChain.SampleDesc.Count = 1;
    descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    descSwapChain.Windowed = TRUE;
    IDXGISwapChain *swapChain;
    res = FactoryDXGI->CreateSwapChain(CmdQueue, &descSwapChain, &swapChain);
    if (FAILED(res)) return false;

    res = swapChain->QueryInterface(IID_PPV_ARGS(&SwapChain));
    SAFE_RELEASE(swapChain);
    if (FAILED(res)) return false;

    BackBufferIndex = SwapChain->GetCurrentBackBufferIndex();
    FrameIndex = 0;

    DescriptorSizeRTV = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTV descriptor heap (includes swap buffer descriptors)
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors = kNumSwapBuffers;
        descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        res = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&SwapBuffersHeap));
        if (FAILED(res)) return false;

        D3D12_CPU_DESCRIPTOR_HANDLE handle = SwapBuffersHeap->GetCPUDescriptorHandleForHeapStart();

        for (uint32_t i = 0; i < kNumSwapBuffers; ++i)
        {
            res = SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapBuffers[i]));
            if (FAILED(res)) return false;

            Device->CreateRenderTargetView(SwapBuffers[i], nullptr, handle);
            handle.ptr += DescriptorSizeRTV;
        }
    }

    Viewport = { 0.0f, 0.0f, (float)Resolution[0], (float)Resolution[1], 0.0f, 1.0f };
    ScissorRect = { 0, 0, (LONG)Resolution[0], (LONG)Resolution[1] };

    res = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence));
    if (FAILED(res)) return false;

    FrameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!FrameFenceEvent) return false;

    for (uint32_t i = 0; i < kNumBufferedFrames; ++i)
    {
        Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       IID_PPV_ARGS(&FrameResources[i].CmdAllocator));
        if (FAILED(res)) return false;
    }

    // depth-stencil image, view and heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors = 1;
        descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        res = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&DsvHeap));
        if (FAILED(res)) return false;

        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil.Depth = 1.0f;
        optimizedClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC imageDesc = {};
        imageDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        imageDesc.Width = (UINT64)Resolution[0];
        imageDesc.Height = (UINT)Resolution[1];
        imageDesc.DepthOrArraySize = imageDesc.MipLevels = 1;
        imageDesc.Format = DXGI_FORMAT_D32_FLOAT;
        imageDesc.SampleDesc.Count = 1;
        imageDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        imageDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        res = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &imageDesc,
                                              D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue,
                                              IID_PPV_ARGS(&DsvImage));
        if (FAILED(res)) return false;

        D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
        viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        viewDesc.Flags = D3D12_DSV_FLAG_NONE;
        Device->CreateDepthStencilView(DsvImage, &viewDesc, DsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    return true;
}

bool CDemo::InitPipelineStates()
{
    HRESULT hr;

    // scene pso
    {
        D3D12_INPUT_ELEMENT_DESC descInputLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        size_t vsSize, psSize;
        const uint8_t *vsCode = Lib::LoadFile("data/shader/scene_vs.cso", &vsSize);
        const uint8_t *psCode = Lib::LoadFile("data/shader/scene_ps.cso", &psSize);
        assert(vsCode && psCode);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.InputLayout = { descInputLayout, _countof(descInputLayout) };
        desc.VS = { vsCode, vsSize };
        desc.PS = { psCode, psSize };
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.DepthStencilState.DepthEnable = TRUE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = UINT_MAX;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;

        hr = Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&ScenePso));
        if (FAILED(hr)) return false;

        hr = Device->CreateRootSignature(0, vsCode, vsSize, IID_PPV_ARGS(&StaticMeshRs));
        if (FAILED(hr)) return false;
    }

    return true;
}

bool CDemo::InitConstantBuffers()
{
    HRESULT hr;

    for (uint32_t i = 0; i < kNumBufferedFrames; ++i)
    {
        SFrameResources *res = &FrameResources[i];

        // constant buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = 1024*16; // FIXME: control this somehow
        resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&res->PerFrameCb));
        if (FAILED(hr)) return false;

        D3D12_RANGE range = {};
        hr = res->PerFrameCb->Map(0, &range, &res->PerFrameCbPtr);
        if (FAILED(hr)) return false;
    }

    return true;
}

bool CDemo::InitDescriptorHeaps()
{
    HRESULT hr;

    D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
    descHeap.NumDescriptors = 10000; // FIXME: control this somehow
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    for (size_t i = 0; i < kNumBufferedFrames; ++i)
    {
        hr = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&FrameResources[i].GpuDescriptorHeap));
        if (FAILED(hr)) return false;
    }

    return true;
}

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprevinst, LPSTR cmdline, int cmdshow)
{
    CDemo *demo = new CDemo;
    demo->Resolution[0] = kDemoResX;
    demo->Resolution[1] = kDemoResY;

    if (!demo->Init())
    {
        delete demo;
        return 1;
    }

    MSG msg = {};
    for (;;)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        }
        else
        {
            demo->Update();
        }
    }

    delete demo;
    return 0;
}
