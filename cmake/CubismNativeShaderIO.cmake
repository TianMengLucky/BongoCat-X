function(bongo_cat_vulkan_shader_io variable)
  set(source "${${variable}}")
  set(start [=[    std::ifstream file(filename.GetRawString(), std::ios::ate | std::ios::binary);]=])
  set(finish [=[    VkShaderModuleCreateInfo createInfo{};]=])
  string(FIND "${source}" "${start}" a)
  string(FIND "${source}" "${finish}" b)
  if(a EQUAL -1 OR b LESS a)
    message(FATAL_ERROR "Vulkan shader asset-I/O patch mismatch")
  endif()
  string(SUBSTRING "${source}" 0 ${a} head)
  string(SUBSTRING "${source}" ${b} -1 tail)
  set(loader [=[    csmSizeInt fileSize = 0;
    csmByte* bytes = CubismFramework::GetLoadFileFunction()(filename.GetRawString(), &fileSize);
    if (!bytes || fileSize <= 0 || fileSize % 4 != 0)
    {
        if (bytes) CubismFramework::GetReleaseBytesFunction()(bytes);
        throw std::runtime_error("Missing or invalid Cubism Vulkan shader");
    }
    std::unique_ptr<csmByte, csmReleaseBytesFunction> owned(bytes, CubismFramework::GetReleaseBytesFunction());
    std::vector<csmUint32> buffer((size_t)fileSize / 4);
    memcpy(buffer.data(), bytes, (size_t)fileSize);

]=])
  set(source "${head}${loader}${tail}")
  string(REPLACE "reinterpret_cast<const csmUint32*>(buffer.GetPtr())" "buffer.data()" source "${source}")
  set(destructor_start "CubismRenderer_Vulkan::~CubismRenderer_Vulkan()")
  string(FIND "${source}" "${destructor_start}" a)
  string(FIND "${source}" "void CubismRenderer_Vulkan::DoStaticRelease()" b)
  if(a EQUAL -1 OR b LESS a)
    message(FATAL_ERROR "Vulkan renderer destruction patch mismatch")
  endif()
  math(EXPR length "${b} - ${a}")
  string(SUBSTRING "${source}" ${a} ${length} destructor)
  string(FIND "${destructor}" "    _depthImage.Destroy(s_device);" tail_start)
  if(tail_start EQUAL -1)
    message(FATAL_ERROR "Vulkan renderer resource cleanup patch mismatch")
  endif()
  string(SUBSTRING "${destructor}" 0 ${tail_start} destructor_head)
  set(cleanup [=[    _depthImage.Destroy(s_device);
    for (csmUint32 i = 0; i < _updateFinishedSemaphores.GetSize(); ++i)
        vkDestroySemaphore(s_device, _updateFinishedSemaphores[i], nullptr);
    vkDestroyDescriptorPool(s_device, _descriptorPool, nullptr);
    vkDestroyDescriptorPool(s_device, _offscreenDescriptorPool, nullptr);
    vkDestroyDescriptorPool(s_device, _copyDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(s_device, _descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(s_device, _copyDescriptorSetLayout, nullptr);
    for (csmUint32 i = 0; i < _descriptorSets.GetSize(); ++i)
        for (csmInt32 j = 0; j < _descriptorSets[i].GetSize(); ++j)
        {
            _descriptorSets[i][j].uniformBuffer.Destroy(s_device);
            _descriptorSets[i][j].uniformBufferMask.Destroy(s_device);
        }
    for (csmUint32 i = 0; i < _offscreenDescriptorSets.GetSize(); ++i)
        for (csmInt32 j = 0; j < _offscreenDescriptorSets[i].GetSize(); ++j)
            _offscreenDescriptorSets[i][j].uniformBuffer.Destroy(s_device);
    for (csmUint32 i = 0; i < _copyDescriptorSets.GetSize(); ++i)
        _copyDescriptorSets[i].uniformBuffer.Destroy(s_device);
    auto destroy_buffers = [](auto& sets) {
        for (csmUint32 i = 0; i < sets.GetSize(); ++i)
            for (csmInt32 j = 0; j < sets[i].GetSize(); ++j)
                sets[i][j].Destroy(s_device);
    };
    destroy_buffers(_vertexBuffers); destroy_buffers(_stagingBuffers);
    destroy_buffers(_indexBuffers); destroy_buffers(_offscreenVertexBuffers);
    destroy_buffers(_offscreenStagingBuffers); destroy_buffers(_offscreenIndexBuffers);
    for (csmUint32 i = 0; i < _copyVertexBuffer.GetSize(); ++i)
        _copyVertexBuffer[i].Destroy(s_device);
    for (csmUint32 i = 0; i < _copyStagingBuffer.GetSize(); ++i)
        _copyStagingBuffer[i].Destroy(s_device);
    for (csmUint32 i = 0; i < _copyIndexBuffer.GetSize(); ++i)
        _copyIndexBuffer[i].Destroy(s_device);
    for (csmUint32 i = 0; i < _updateCommandBuffers.GetSize(); ++i)
        if (_updateCommandBuffers[i])
            vkFreeCommandBuffers(s_device, s_commandPool, 1, &_updateCommandBuffers[i]);
    for (csmUint32 i = 0; i < _drawCommandBuffers.GetSize(); ++i)
        if (_drawCommandBuffers[i])
            vkFreeCommandBuffers(s_device, s_commandPool, 1, &_drawCommandBuffers[i]);
}

]=])
  string(REPLACE "${destructor}" "${destructor_head}${cleanup}" source "${source}")
  set(${variable} "#include <memory>\n#include <vector>\n#include <stdexcept>\n${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_metal_shader_io variable)
  set(source "${${variable}}")
  string(REPLACE "\r\n" "\n" source "${source}")
  set(helper [=[
static NSData* bongo_cat_metal_shader_data(NSString* name)
{
    using namespace Live2D::Cubism::Framework;
    std::string path = "FrameworkMetallibs/";
    path += [name UTF8String]; path += ".metallib";
    csmSizeInt size = 0;
    csmByte* bytes = CubismFramework::GetLoadFileFunction()(path, &size);
    if (!bytes) return nil;
    NSData* data = [NSData dataWithBytes:bytes length:(NSUInteger)size];
    CubismFramework::GetReleaseBytesFunction()(bytes);
    return data;
}
static id<MTLLibrary> bongo_cat_metal_shader_library(id<MTLDevice> device)
{
    NSData* data = bongo_cat_metal_shader_data(@"MetalShaders");
    if (!data) throw std::runtime_error("Missing Cubism Metal shader library");
    void* bytes = malloc(data.length);
    if (!bytes) throw std::bad_alloc();
    memcpy(bytes, data.bytes, data.length);
    dispatch_data_t block = dispatch_data_create(bytes, data.length, nil, DISPATCH_DATA_DESTRUCTOR_FREE);
    id<MTLLibrary> library = [device newLibraryWithData:block error:nil];
    dispatch_release(block);
    if (!library) throw std::runtime_error("Cannot load Cubism Metal shader library");
    return [library autorelease];
}
]=])
  set(anchor [=[#include "CubismShader_Metal.hpp"]=])
  string(FIND "${source}" "${anchor}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Metal shader asset-I/O include patch mismatch")
  endif()
  string(REPLACE "${anchor}" "${anchor}\n${helper}" source "${source}")
  string(REPLACE "[device newLibraryWithURL:libraryURL error:nil]"
    "bongo_cat_metal_shader_library(device)" source "${source}")
  string(REPLACE "[NSData dataWithContentsOfURL:vertShaderFileLibURL]"
    "bongo_cat_metal_shader_data(vertShaderFileNameStr)" source "${source}")
  string(REPLACE "[NSData dataWithContentsOfURL:fragShaderFileLibURL]"
    "bongo_cat_metal_shader_data(fragShaderFileNameStr)" source "${source}")
  set(signature [=[id<MTLRenderPipelineState> CubismShader_Metal::MakeRenderPipelineState(id<MTLDevice> device, CubismShader_Metal::ShaderProgram* shaderProgram, int blendMode)
{]=])
  string(FIND "${source}" "${signature}" signature_position)
  if(signature_position EQUAL -1)
    message(FATAL_ERROR "Metal pipeline safety patch mismatch")
  endif()
  string(REPLACE "${signature}"
    "${signature}\n    if (!shaderProgram) throw std::runtime_error(\"Missing Cubism Metal shader program\");"
    source "${source}")
  string(REPLACE "return [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];"
    "id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:renderPipelineDescriptor error:&error];\n    if (!pipeline) throw std::runtime_error(\"Cannot create Cubism Metal pipeline\");\n    return pipeline;"
    source "${source}")
  set(${variable} "#include <stdexcept>\n#include <new>\n${source}" PARENT_SCOPE)
endfunction()
