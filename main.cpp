#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <string_view>

#include <dawn/dawn_proc.h>
#include <dawn/webgpu.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>

static void DeviceLogCallback(WGPULoggingType type, WGPUStringView message, void* userdata, void* userdata2) {
    const char* typeName = "Info";
    switch (type) {
        case WGPULoggingType_Verbose: typeName = "Verbose"; break;
        case WGPULoggingType_Info:    typeName = "Info"; break;
        case WGPULoggingType_Warning: typeName = "Warning"; break;
        case WGPULoggingType_Error:   typeName = "Error"; break;
        default: break;
    }

    if (message.length == SIZE_MAX) {
        fprintf(stderr, "[Dawn %s] %s\n", typeName, message.data);
    } else {
        fprintf(stderr, "[Dawn %s] %.*s\n", typeName, (int)message.length, message.data);
    }
    fflush(stderr);
}

static void UncapturedErrorCallback(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata, void* userdata2) {
    const char* typeStr = "Unknown";
    switch (type) {
        case WGPUErrorType_Validation: typeStr = "Validation"; break;
        case WGPUErrorType_OutOfMemory: typeStr = "Out of Memory"; break;
        case WGPUErrorType_Internal: typeStr = "Internal"; break;
        default: break;
    }
    
    if (message.length == SIZE_MAX) {
        fprintf(stderr, "[Dawn %s Error] %s\n", typeStr, message.data);
    } else {
        fprintf(stderr, "[Dawn %s Error] %.*s\n", typeStr, (int)message.length, message.data);
    }
    fflush(stderr);
}

const char* BackendTypeName(WGPUBackendType type) {
    switch (type) {
        case WGPUBackendType_Null:     return "Null";
        case WGPUBackendType_WebGPU:   return "WebGPU";
        case WGPUBackendType_D3D11:    return "D3D11";
        case WGPUBackendType_D3D12:    return "D3D12";
        case WGPUBackendType_Metal:    return "Metal";
        case WGPUBackendType_Vulkan:   return "Vulkan";
        case WGPUBackendType_OpenGL:   return "OpenGL";
        case WGPUBackendType_OpenGLES: return "OpenGLES";
        default:                       return "Unknown";
    }
}

std::string ToString(WGPUStringView view) {
    if (!view.data) return "";
    if (view.length == SIZE_MAX) return std::string(view.data);
    return std::string(view.data, view.length);
}

void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " <path_to_shader.wgsl> [adapter_index]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string shaderPath = argv[1];
    int targetAdapterIndex = (argc >= 3) ? std::stoi(argv[2]) : -1;

    dawnProcSetProcs(&dawn::native::GetProcs());
    auto instance = std::make_unique<dawn::native::Instance>();

    WGPURequestAdapterOptions adapterOpts = {};
    auto nativeAdapters = instance->EnumerateAdapters(&adapterOpts);
    
    std::cout << "Available Adapters:\n";
    for (size_t i = 0; i < nativeAdapters.size(); ++i) {
        WGPUAdapter cAdapter = nativeAdapters[i].Get();
        WGPUAdapterInfo info = {};
        wgpuAdapterGetInfo(cAdapter, &info);
        
        std::cout << "[" << i << "] " << ToString(info.device) << " (" << ToString(info.description) << ")\n";
        std::cout << "    Backend: " << BackendTypeName(info.backendType) << "\n";
        std::cout << "    DeviceID: 0x" << std::hex << info.deviceID << std::dec << "\n";
    }

    if (nativeAdapters.empty()) {
        std::cerr << "No adapters found.\n";
        return 1;
    }

    if (targetAdapterIndex >= (int)nativeAdapters.size()) {
        std::cerr << "Error: Selected index " << targetAdapterIndex << " is out of range.\n";
        return 1;
    }

    size_t selectedIdx = (targetAdapterIndex < 0) ? 0 : (size_t)targetAdapterIndex;
    std::cout << "\nUsing Adapter [" << selectedIdx << "]\n";
    WGPUAdapter selectedAdapter = nativeAdapters[selectedIdx].Get();

    WGPUDeviceDescriptor cDeviceDesc = {};
    
    WGPUUncapturedErrorCallbackInfo errInfo = {};
    errInfo.callback = UncapturedErrorCallback;
    cDeviceDesc.uncapturedErrorCallbackInfo = errInfo;
    
    WGPUDevice cDevice = wgpuAdapterCreateDevice(selectedAdapter, &cDeviceDesc);
    if (!cDevice) {
        std::cerr << "Failed to create device.\n";
        return 1;
    }

    WGPULoggingCallbackInfo logInfo = {};
    logInfo.callback = DeviceLogCallback;
    wgpuDeviceSetLoggingCallback(cDevice, logInfo);
    
    wgpu::Device device = wgpu::Device::Acquire(cDevice);

    std::ifstream file(shaderPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << shaderPath << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string code = ss.str();

    wgpu::ShaderSourceWGSL wgslDesc = {};
    wgslDesc.code = wgpu::StringView{code.data(), code.size()};
    
    wgpu::ShaderModuleDescriptor smDesc = {};
    smDesc.nextInChain = &wgslDesc;
    wgpu::ShaderModule shader = device.CreateShaderModule(&smDesc);

    wgpu::ComputePipelineDescriptor pDesc = {};
    pDesc.compute.module = shader;
    pDesc.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.CreateComputePipeline(&pDesc);

    const uint32_t bufferSize = 256;
    wgpu::BufferDescriptor bDesc = {};
    bDesc.size = bufferSize;
    bDesc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer storageBuf = device.CreateBuffer(&bDesc);

    wgpu::BufferDescriptor rDesc = {};
    rDesc.size = bufferSize;
    rDesc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readbackBuf = device.CreateBuffer(&rDesc);

    wgpu::BindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storageBuf;
    bgEntry.size = bufferSize;

    wgpu::BindGroupDescriptor bgDesc = {};
    bgDesc.layout = pipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    wgpu::BindGroup bindGroup = device.CreateBindGroup(&bgDesc);

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bindGroup);
    pass.DispatchWorkgroups(1);
    pass.End();
    
    encoder.CopyBufferToBuffer(storageBuf, 0, readbackBuf, 0, bufferSize);
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);

    bool done = false;
    readbackBuf.MapAsync(
        wgpu::MapMode::Read, 
        0, 
        bufferSize, 
        wgpu::CallbackMode::AllowProcessEvents,
        [&done](wgpu::MapAsyncStatus status, wgpu::StringView message) {
            if (status != wgpu::MapAsyncStatus::Success) {
                std::cerr << "MapAsync failed!\n";
            }
            done = true;
        }
    );

    while (!done) {
        wgpuInstanceProcessEvents(instance->Get());
    }

    const uint32_t* results = (const uint32_t*)readbackBuf.GetConstMappedRange(0, bufferSize);
    std::cout << "First 4 words of result: " 
              << results[0] << " " << results[1] << " " 
              << results[2] << " " << results[3] << "\n";

    readbackBuf.Unmap();
    return 0;
}