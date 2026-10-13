# Defer the six compatible shaders plus copy/mask on desktop SDK renderers.
# The Android-specific extension branch retains upstream initialization.
function(bongo_cat_gl_lazy_base variable)
  set(source "${${variable}}")
  set(begin_anchor "void CubismShader_OpenGLES2::GenerateShaders()\n{")
  string(FIND "${source}" "${begin_anchor}" begin)
  if(begin LESS 0)
    message(FATAL_ERROR "OpenGL lazy shader initialization changed")
  endif()
  string(SUBSTRING "${source}" ${begin} -1 tail)
  string(FIND "${tail}" "\nvoid CubismShader_OpenGLES2::SetupShaderProgramForDrawable" end)
  if(end LESS 0)
    message(FATAL_ERROR "OpenGL lazy shader boundary changed")
  endif()
  string(SUBSTRING "${tail}" 0 ${end} original)
  set(patched "${original}")
  bongo_cat_replace_cubism_text(patched "${begin_anchor}" [=[void CubismShader_OpenGLES2::GenerateShaders()
{
#ifndef CSM_TARGET_ANDROID_ES2
    if (_shaderSets.GetSize()) return;
    for (csmInt32 i = 0; i < ShaderNames_ShaderCount; ++i)
        _shaderSets.PushBack(CSM_NEW CubismShaderSet());
    return;
#else]=] "desktop lazy initialization")
  # End the desktop/Android branch before the function closing brace.
  string(REGEX REPLACE "}([\n]*)$" "#endif\n}\n" patched "${patched}")
  string(REPLACE "${original}" "${patched}" source "${source}")
  set(basic [=[
#ifndef CSM_TARGET_ANDROID_ES2
    if (!shaderSet->ShaderProgram && shaderNameBegin < ShaderNames_NormalAtop) {
        static const char *vertices[] = {"VertShaderSrc.vert", "VertShaderSrcMasked.vert",
            "VertShaderSrcMasked.vert", "VertShaderSrc.vert", "VertShaderSrcMasked.vert", "VertShaderSrcMasked.vert"};
        static const char *fragments[] = {"FragShaderSrc.frag", "FragShaderSrcMask.frag",
            "FragShaderSrcMaskInverted.frag", "FragShaderSrcPremultipliedAlpha.frag",
            "FragShaderSrcMaskPremultipliedAlpha.frag", "FragShaderSrcMaskInvertedPremultipliedAlpha.frag"};
        CubismShaderSet *base = _shaderSets[ShaderNames_Normal + offset];
        if (!base->ShaderProgram) {
            csmString vertex = csmString("FrameworkShaders/") + vertices[offset];
            csmString fragment = csmString("FrameworkShaders/") + fragments[offset];
            base->ShaderProgram = LoadShaderProgramFromFile(vertex.GetRawString(), fragment.GetRawString());
            if (!base->ShaderProgram) throw std::runtime_error("Cannot create Live2D OpenGL shader");
            SetShaderSet(*base, static_cast<MaskType>(offset));
        }
        // Compatible add/multiply share the program and its immutable locations.
        *shaderSet = *base;
    }
#endif
]=])
  set(anchor "    CubismShaderSet* shaderSet = _shaderSets[shaderNameBegin + offset];")
  string(REGEX MATCHALL [=[CubismShaderSet\* shaderSet = _shaderSets\[shaderNameBegin \+ offset\]]=] matches "${source}")
  list(LENGTH matches count)
  if(NOT count EQUAL 2)
    message(FATAL_ERROR "OpenGL lazy shader draw sites changed")
  endif()
  string(REPLACE "${anchor}" "${anchor}\n${basic}" source "${source}")
  foreach(kind IN ITEMS Copy SetupMask)
    set(block_begin "    // ${kind}\n")
    if(kind STREQUAL "Copy")
      set(block_end "    // SetupMask\n")
    else()
      set(block_end "    // 通常\n")
    endif()
    string(FIND "${original}" "${block_begin}" block_start)
    string(SUBSTRING "${original}" ${block_start} -1 block_tail)
    string(FIND "${block_tail}" "${block_end}" block_length)
    if(block_start LESS 0 OR block_length LESS 0)
      message(FATAL_ERROR "OpenGL lazy shader locations changed: ${kind}")
    endif()
    string(SUBSTRING "${block_tail}" 0 ${block_length} locations)
    string(REPLACE "_shaderSets[ShaderNames_${kind}]" "shaderSet" locations "${locations}")
    set(anchor "    CubismShaderSet* shaderSet = _shaderSets[ShaderNames_${kind}];")
    set(lazy "${anchor}\n#ifndef CSM_TARGET_ANDROID_ES2\n    if (!shaderSet->ShaderProgram) {\n        shaderSet->ShaderProgram = LoadShaderProgramFromFile(\"FrameworkShaders/VertShaderSrc${kind}.vert\", \"FrameworkShaders/FragShaderSrc${kind}.frag\");\n        if (!shaderSet->ShaderProgram) throw std::runtime_error(\"Cannot create Live2D OpenGL ${kind} shader\");\n${locations}    }\n#endif")
    bongo_cat_replace_cubism_text(source "${anchor}" "${lazy}" "lazy copy/mask shader")
  endforeach()
  # Failed compilation must release the program and any successfully compiled
  # vertex shader; CompileShaderSource itself deletes its failed shader handle.
  bongo_cat_replace_cubism_text(source "    GLuint vertShader, fragShader;"
    "    GLuint vertShader = 0, fragShader = 0;" "initialized shader ownership")
  bongo_cat_replace_cubism_text(source [=[        CubismLogError("Vertex shader compile error!");
        return 0;]=] [=[        CubismLogError("Vertex shader compile error!");
        glDeleteProgram(shaderProgram);
        return 0;]=] "vertex compile cleanup")
  bongo_cat_replace_cubism_text(source [=[        CubismLogError("Fragment shader compile error!");
        return 0;]=] [=[        CubismLogError("Fragment shader compile error!");
        if (vertShader) glDeleteShader(vertShader);
        glDeleteProgram(shaderProgram);
        return 0;]=] "fragment compile cleanup")
  set(${variable} "#include <stdexcept>\n${source}" PARENT_SCOPE)
endfunction()
