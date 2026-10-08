#ifndef BONGO_CAT_CUBISM_PLUGIN_SERVICES_HPP
#define BONGO_CAT_CUBISM_PLUGIN_SERVICES_HPP
#include <GL/glew.h>
#include "bongo_cat/model_plugin_host.h"
#ifdef SDL_CreateThread
#undef SDL_CreateThread
#endif
#define SDL_CreateThread (bongo_cat_model_plugin_host()->SDL_CreateThread)
#ifdef SDL_Delay
#undef SDL_Delay
#endif
#define SDL_Delay (bongo_cat_model_plugin_host()->SDL_Delay)
#ifdef SDL_GL_GetCurrentContext
#undef SDL_GL_GetCurrentContext
#endif
#define SDL_GL_GetCurrentContext (bongo_cat_model_plugin_host()->SDL_GL_GetCurrentContext)
#ifdef SDL_GL_GetCurrentWindow
#undef SDL_GL_GetCurrentWindow
#endif
#define SDL_GL_GetCurrentWindow (bongo_cat_model_plugin_host()->SDL_GL_GetCurrentWindow)
#ifdef SDL_GetBasePath
#undef SDL_GetBasePath
#endif
#define SDL_GetBasePath (bongo_cat_model_plugin_host()->SDL_GetBasePath)
#ifdef SDL_GetPathInfo
#undef SDL_GetPathInfo
#endif
#define SDL_GetPathInfo (bongo_cat_model_plugin_host()->SDL_GetPathInfo)
#ifdef SDL_GetThreadState
#undef SDL_GetThreadState
#endif
#define SDL_GetThreadState (bongo_cat_model_plugin_host()->SDL_GetThreadState)
#ifdef SDL_GetTicksNS
#undef SDL_GetTicksNS
#endif
#define SDL_GetTicksNS (bongo_cat_model_plugin_host()->SDL_GetTicksNS)
#ifdef SDL_Log
#undef SDL_Log
#endif
#define SDL_Log (bongo_cat_model_plugin_host()->SDL_Log)
#ifdef SDL_LogDebug
#undef SDL_LogDebug
#endif
#define SDL_LogDebug (bongo_cat_model_plugin_host()->SDL_LogDebug)
#ifdef SDL_LogError
#undef SDL_LogError
#endif
#define SDL_LogError (bongo_cat_model_plugin_host()->SDL_LogError)
#ifdef SDL_LogInfo
#undef SDL_LogInfo
#endif
#define SDL_LogInfo (bongo_cat_model_plugin_host()->SDL_LogInfo)
#ifdef SDL_LogWarn
#undef SDL_LogWarn
#endif
#define SDL_LogWarn (bongo_cat_model_plugin_host()->SDL_LogWarn)
#ifdef SDL_WaitThread
#undef SDL_WaitThread
#endif
#define SDL_WaitThread (bongo_cat_model_plugin_host()->SDL_WaitThread)
#ifdef bongo_cat_error_set
#undef bongo_cat_error_set
#endif
#define bongo_cat_error_set (bongo_cat_model_plugin_host()->bongo_cat_error_set)
#ifdef bongo_cat_gl_clear_errors
#undef bongo_cat_gl_clear_errors
#endif
#define bongo_cat_gl_clear_errors (bongo_cat_model_plugin_host()->bongo_cat_gl_clear_errors)
#ifdef bongo_cat_image_forget_texture_cache
#undef bongo_cat_image_forget_texture_cache
#endif
#define bongo_cat_image_forget_texture_cache (bongo_cat_model_plugin_host()->bongo_cat_image_forget_texture_cache)
#ifdef bongo_cat_image_free
#undef bongo_cat_image_free
#endif
#define bongo_cat_image_free (bongo_cat_model_plugin_host()->bongo_cat_image_free)
#ifdef bongo_cat_image_info
#undef bongo_cat_image_info
#endif
#define bongo_cat_image_info (bongo_cat_model_plugin_host()->bongo_cat_image_info)
#ifdef bongo_cat_image_make_alpha_mask
#undef bongo_cat_image_make_alpha_mask
#endif
#define bongo_cat_image_make_alpha_mask (bongo_cat_model_plugin_host()->bongo_cat_image_make_alpha_mask)
#ifdef bongo_cat_image_resize_rgba_take
#undef bongo_cat_image_resize_rgba_take
#endif
#define bongo_cat_image_resize_rgba_take (bongo_cat_model_plugin_host()->bongo_cat_image_resize_rgba_take)
#ifdef bongo_cat_image_texture_job_cancel
#undef bongo_cat_image_texture_job_cancel
#endif
#define bongo_cat_image_texture_job_cancel (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_cancel)
#ifdef bongo_cat_image_texture_job_cleanup_poll
#undef bongo_cat_image_texture_job_cleanup_poll
#endif
#define bongo_cat_image_texture_job_cleanup_poll (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_cleanup_poll)
#ifdef bongo_cat_image_texture_job_destroy
#undef bongo_cat_image_texture_job_destroy
#endif
#define bongo_cat_image_texture_job_destroy (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_destroy)
#ifdef bongo_cat_image_texture_job_needs_poll
#undef bongo_cat_image_texture_job_needs_poll
#endif
#define bongo_cat_image_texture_job_needs_poll (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_needs_poll)
#ifdef bongo_cat_image_texture_job_poll
#undef bongo_cat_image_texture_job_poll
#endif
#define bongo_cat_image_texture_job_poll (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_poll)
#ifdef bongo_cat_image_texture_job_start
#undef bongo_cat_image_texture_job_start
#endif
#define bongo_cat_image_texture_job_start (bongo_cat_model_plugin_host()->bongo_cat_image_texture_job_start)
#ifdef bongo_cat_image_texture_model
#undef bongo_cat_image_texture_model
#endif
#define bongo_cat_image_texture_model (bongo_cat_model_plugin_host()->bongo_cat_image_texture_model)
#ifdef bongo_cat_image_texture_model_scaled_cached
#undef bongo_cat_image_texture_model_scaled_cached
#endif
#define bongo_cat_image_texture_model_scaled_cached (bongo_cat_model_plugin_host()->bongo_cat_image_texture_model_scaled_cached)
#ifdef bongo_cat_model_json_parse
#undef bongo_cat_model_json_parse
#endif
#define bongo_cat_model_json_parse (bongo_cat_model_plugin_host()->bongo_cat_model_json_parse)
#ifdef bongo_cat_model_memory_log
#undef bongo_cat_model_memory_log
#endif
#define bongo_cat_model_memory_log (bongo_cat_model_plugin_host()->bongo_cat_model_memory_log)
#ifdef bongo_cat_model_texture_mib
#undef bongo_cat_model_texture_mib
#endif
#define bongo_cat_model_texture_mib (bongo_cat_model_plugin_host()->bongo_cat_model_texture_mib)
#ifdef bongo_cat_platform_memory_usage
#undef bongo_cat_platform_memory_usage
#endif
#define bongo_cat_platform_memory_usage (bongo_cat_model_plugin_host()->bongo_cat_platform_memory_usage)
#ifdef bongo_cat_platform_trim_memory
#undef bongo_cat_platform_trim_memory
#endif
#define bongo_cat_platform_trim_memory (bongo_cat_model_plugin_host()->bongo_cat_platform_trim_memory)
#ifdef bongo_cat_resource_trace_atlas
#undef bongo_cat_resource_trace_atlas
#endif
#define bongo_cat_resource_trace_atlas (bongo_cat_model_plugin_host()->bongo_cat_resource_trace_atlas)
#ifdef bongo_cat_resource_trace_render
#undef bongo_cat_resource_trace_render
#endif
#define bongo_cat_resource_trace_render (bongo_cat_model_plugin_host()->bongo_cat_resource_trace_render)
#ifdef bongo_cat_rhi_active_is_gl
#undef bongo_cat_rhi_active_is_gl
#endif
#define bongo_cat_rhi_active_is_gl (bongo_cat_model_plugin_host()->bongo_cat_rhi_active_is_gl)
#ifdef bongo_cat_rhi_begin_commands
#undef bongo_cat_rhi_begin_commands
#endif
#define bongo_cat_rhi_begin_commands (bongo_cat_model_plugin_host()->bongo_cat_rhi_begin_commands)
#ifdef bongo_cat_rhi_get_active_device_info
#undef bongo_cat_rhi_get_active_device_info
#endif
#define bongo_cat_rhi_get_active_device_info (bongo_cat_model_plugin_host()->bongo_cat_rhi_get_active_device_info)
#ifdef bongo_cat_rhi_get_metal_frame_info
#undef bongo_cat_rhi_get_metal_frame_info
#endif
#define bongo_cat_rhi_get_metal_frame_info (bongo_cat_model_plugin_host()->bongo_cat_rhi_get_metal_frame_info)
#ifdef bongo_cat_rhi_get_vulkan_frame_info
#undef bongo_cat_rhi_get_vulkan_frame_info
#endif
#define bongo_cat_rhi_get_vulkan_frame_info (bongo_cat_model_plugin_host()->bongo_cat_rhi_get_vulkan_frame_info)
#ifdef bongo_cat_rhi_submit_commands_checked
#undef bongo_cat_rhi_submit_commands_checked
#endif
#define bongo_cat_rhi_submit_commands_checked (bongo_cat_model_plugin_host()->bongo_cat_rhi_submit_commands_checked)
#ifdef bongo_cat_rhi_wait_idle
#undef bongo_cat_rhi_wait_idle
#endif
#define bongo_cat_rhi_wait_idle (bongo_cat_model_plugin_host()->bongo_cat_rhi_wait_idle)
#ifdef bongo_cat_sha256_bytes
#undef bongo_cat_sha256_bytes
#endif
#define bongo_cat_sha256_bytes (bongo_cat_model_plugin_host()->bongo_cat_sha256_bytes)
#ifdef bongo_cat_sha256_file
#undef bongo_cat_sha256_file
#endif
#define bongo_cat_sha256_file (bongo_cat_model_plugin_host()->bongo_cat_sha256_file)
#ifdef bongo_safe_expression_parse
#undef bongo_safe_expression_parse
#endif
#define bongo_safe_expression_parse (bongo_cat_model_plugin_host()->bongo_safe_expression_parse)
#ifdef bongo_safe_free_expression
#undef bongo_safe_free_expression
#endif
#define bongo_safe_free_expression (bongo_cat_model_plugin_host()->bongo_safe_free_expression)
#ifdef bongo_safe_free_pixels
#undef bongo_safe_free_pixels
#endif
#define bongo_safe_free_pixels (bongo_cat_model_plugin_host()->bongo_safe_free_pixels)
#ifdef bongo_safe_image_decode
#undef bongo_safe_image_decode
#endif
#define bongo_safe_image_decode (bongo_cat_model_plugin_host()->bongo_safe_image_decode)
#define bongo_json_arr_get (bongo_cat_model_plugin_host()->bongo_json_arr_get)
#define bongo_json_arr_size (bongo_cat_model_plugin_host()->bongo_json_arr_size)
#define bongo_json_doc_free (bongo_cat_model_plugin_host()->bongo_json_doc_free)
#define bongo_json_doc_get_root (bongo_cat_model_plugin_host()->bongo_json_doc_get_root)
#define bongo_json_free_text (bongo_cat_model_plugin_host()->bongo_json_free_text)
#define bongo_json_get_int (bongo_cat_model_plugin_host()->bongo_json_get_int)
#define bongo_json_get_num (bongo_cat_model_plugin_host()->bongo_json_get_num)
#define bongo_json_get_str (bongo_cat_model_plugin_host()->bongo_json_get_str)
#define bongo_json_is_arr (bongo_cat_model_plugin_host()->bongo_json_is_arr)
#define bongo_json_is_int (bongo_cat_model_plugin_host()->bongo_json_is_int)
#define bongo_json_is_num (bongo_cat_model_plugin_host()->bongo_json_is_num)
#define bongo_json_is_obj (bongo_cat_model_plugin_host()->bongo_json_is_obj)
#define bongo_json_is_str (bongo_cat_model_plugin_host()->bongo_json_is_str)
#define bongo_json_is_uint (bongo_cat_model_plugin_host()->bongo_json_is_uint)
#define bongo_json_obj_get (bongo_cat_model_plugin_host()->bongo_json_obj_get)
#define bongo_json_obj_key_at (bongo_cat_model_plugin_host()->bongo_json_obj_key_at)
#define bongo_json_obj_value_at (bongo_cat_model_plugin_host()->bongo_json_obj_value_at)
#define bongo_json_obj_size (bongo_cat_model_plugin_host()->bongo_json_obj_size)
#define bongo_json_read (bongo_cat_model_plugin_host()->bongo_json_read)
#define bongo_json_read_opts (bongo_cat_model_plugin_host()->bongo_json_read_opts)
#define bongo_json_write (bongo_cat_model_plugin_host()->bongo_json_write)
#endif
