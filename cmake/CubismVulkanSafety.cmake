# Stop failed allocations before the SDK accesses invalid handles/mappings.
function(bongo_cat_harden_vulkan_resources target)
  set(original "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismClass_Vulkan.cpp")
  file(READ "${original}" source)
  string(REPLACE "\r\n" "\n" source "${source}")
  string(REPLACE "#include \"CubismClass_Vulkan.hpp\""
    "#include \"CubismClass_Vulkan.hpp\"\n#include <stdexcept>" source "${source}")
  foreach(message IN ITEMS "failed to find suitable memory type!"
      "failed to create buffer!" "failed to allocate buffer memory!"
      "failed to create image!" "failed to allocate image memory!"
      "failed to create texture image view!" "failed to create texture sampler!")
    set(anchor "CubismLogError(\"${message}\");")
    string(FIND "${source}" "${anchor}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Vulkan resource safety patch mismatch: ${message}")
    endif()
    string(REPLACE "${anchor}" "throw std::runtime_error(\"${message}\");"
      source "${source}")
  endforeach()
  foreach(call IN ITEMS "vkBindBufferMemory(device, buffer, memory, 0)"
      "vkBindImageMemory(device, image, memory, 0)"
      "vkMapMemory(device, memory, 0, size, 0, &mapped)")
    string(FIND "${source}" "${call};" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Vulkan resource safety patch mismatch: ${call}")
    endif()
    string(REPLACE "${call};"
      "if (${call} != VK_SUCCESS) throw std::runtime_error(\"Vulkan memory operation failed\");"
      source "${source}")
  endforeach()
  string(REPLACE "maxAnistropy >= 1.0f" "maxAnistropy > 1.0f" source "${source}")
  set(output "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan/CubismClass_safe.cpp")
  file(WRITE "${output}" "${source}")
  get_target_property(sources ${target} SOURCES)
  list(REMOVE_ITEM sources "${original}")
  set_property(TARGET ${target} PROPERTY SOURCES "${sources}")
  target_sources(${target} PRIVATE "${output}")
endfunction()

# Failed shader/pipeline creation must unwind before the SDK binds null handles.
function(bongo_cat_harden_vulkan_pipelines variable)
  set(source "${${variable}}")
  foreach(message IN ITEMS "failed to create shader module!"
      "failed to create _pipeline layout!" "failed to create graphics _pipeline!")
    set(anchor "CubismLogError(\"${message}\");")
    string(FIND "${source}" "${anchor}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "Vulkan pipeline safety patch mismatch: ${message}")
    endif()
    string(REPLACE "${anchor}" "throw std::runtime_error(\"${message}\");"
      source "${source}")
  endforeach()
  set(anchor [=[    VkShaderModule vertShaderModule = CreateShaderModule(s_device, vertFileName);]=])
  string(FIND "${source}" "${anchor}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Vulkan shader ownership patch mismatch")
  endif()
  set(owned [=[    struct ShaderModules {
        VkDevice device;
        VkShaderModule vertex = VK_NULL_HANDLE, fragment = VK_NULL_HANDLE;
        ~ShaderModules() {
            if (vertex) vkDestroyShaderModule(device, vertex, nullptr);
            if (fragment) vkDestroyShaderModule(device, fragment, nullptr);
        }
    } modules{s_device};
    VkShaderModule vertShaderModule = modules.vertex = CreateShaderModule(s_device, vertFileName);]=])
  string(REPLACE "${anchor}" "${owned}" source "${source}")
  string(REPLACE "VkShaderModule fragShaderModule = CreateShaderModule(s_device, fragFileName);"
    "VkShaderModule fragShaderModule = modules.fragment = CreateShaderModule(s_device, fragFileName);"
    source "${source}")
  string(REPLACE "    vkDestroyShaderModule(s_device, vertShaderModule, nullptr);" "" source "${source}")
  string(REPLACE "    vkDestroyShaderModule(s_device, fragShaderModule, nullptr);" "" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

# The host selects depth-only formats. Cubism 5.3 also uses this image for
# offscreen masks but asks for a stencil aspect that D32/D16 do not have.
function(bongo_cat_vulkan_depth_aspects variable)
  set(source "${${variable}}")
  set(aspects "VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT")
  string(FIND "${source}" "${aspects}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Vulkan depth-aspect patch mismatch")
  endif()
  string(REPLACE "${aspects}" "VK_IMAGE_ASPECT_DEPTH_BIT" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_harden_vulkan_render_targets target)
  set(original "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderTarget_Vulkan.cpp")
  file(READ "${original}" source)
  bongo_cat_vulkan_depth_aspects(source)
  set(output "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan/CubismRenderTarget_depth.cpp")
  file(WRITE "${output}" "${source}")
  get_target_property(sources ${target} SOURCES)
  list(REMOVE_ITEM sources "${original}")
  set_property(TARGET ${target} PROPERTY SOURCES "${sources}")
  target_sources(${target} PRIVATE "${output}")
endfunction()
