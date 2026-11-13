#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_str.h>
#include <va/va_vpp.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <pulse/pulseaudio.h>
#include <alsa/asoundlib.h>
#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <libudev.h>

enum { LIBVA, LIBVA_DRM, LIBDRM, LIBPULSE, LIBASOUND, LIBXCB, LIBXCB_SHM, LIBUDEV, LIBRARY_COUNT };

static const char *const library_names[LIBRARY_COUNT] = {
    "libva.so.2", "libva-drm.so.2", "libdrm.so.2", "libpulse.so.0", "libasound.so.2", "libxcb.so.1", "libxcb-shm.so.0",
    "libudev.so.1"
};
static void *libraries[LIBRARY_COUNT];
static pthread_once_t load_once = PTHREAD_ONCE_INIT;

#define FUNCTIONS(X, V) \
    X(LIBVA, VAStatus, vaAcquireBufferHandle, (VADisplay d, VABufferID b, VABufferInfo *i), (d, b, i), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaBeginPicture, (VADisplay d, VAContextID c, VASurfaceID s), (d, c, s), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaCreateBuffer, (VADisplay d, VAContextID c, VABufferType t, unsigned int s, unsigned int n, void *p, VABufferID *b), (d, c, t, s, n, p, b), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaCreateConfig, (VADisplay d, VAProfile p, VAEntrypoint e, VAConfigAttrib *a, int n, VAConfigID *c), (d, p, e, a, n, c), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaCreateContext, (VADisplay d, VAConfigID c, int w, int h, int f, VASurfaceID *s, int n, VAContextID *x), (d, c, w, h, f, s, n, x), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaCreateImage, (VADisplay d, VAImageFormat *f, int w, int h, VAImage *i), (d, f, w, h, i), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaCreateSurfaces, (VADisplay d, unsigned int f, unsigned int w, unsigned int h, VASurfaceID *s, unsigned int n, VASurfaceAttrib *a, unsigned int m), (d, f, w, h, s, n, a, m), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDeriveImage, (VADisplay d, VASurfaceID s, VAImage *i), (d, s, i), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDestroyBuffer, (VADisplay d, VABufferID b), (d, b), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDestroyConfig, (VADisplay d, VAConfigID c), (d, c), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDestroyContext, (VADisplay d, VAContextID c), (d, c), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDestroyImage, (VADisplay d, VAImageID i), (d, i), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaDestroySurfaces, (VADisplay d, VASurfaceID *s, int n), (d, s, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaEndPicture, (VADisplay d, VAContextID c), (d, c), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, const char *, vaEntrypointStr, (VAEntrypoint e), (e), "<unknown>") \
    X(LIBVA, const char *, vaErrorStr, (VAStatus s), (s), "libva is not available") \
    X(LIBVA, VAStatus, vaExportSurfaceHandle, (VADisplay d, VASurfaceID s, uint32_t t, uint32_t f, void *x), (d, s, t, f, x), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaGetConfigAttributes, (VADisplay d, VAProfile p, VAEntrypoint e, VAConfigAttrib *a, int n), (d, p, e, a, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaGetDisplayAttributes, (VADisplay d, VADisplayAttribute *a, int n), (d, a, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA_DRM, VADisplay, vaGetDisplayDRM, (int fd), (fd), NULL) \
    X(LIBVA, VAStatus, vaGetImage, (VADisplay d, VASurfaceID s, int x, int y, unsigned int w, unsigned int h, VAImageID i), (d, s, x, y, w, h, i), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaInitialize, (VADisplay d, int *major, int *minor), (d, major, minor), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaMapBuffer, (VADisplay d, VABufferID b, void **p), (d, b, p), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaMapBuffer2, (VADisplay d, VABufferID b, void **p, uint32_t f), (d, b, p, f), vaMapBuffer(d, b, p)) \
    X(LIBVA, int, vaMaxNumEntrypoints, (VADisplay d), (d), 0) \
    X(LIBVA, int, vaMaxNumImageFormats, (VADisplay d), (d), 0) \
    X(LIBVA, int, vaMaxNumProfiles, (VADisplay d), (d), 0) \
    X(LIBVA, const char *, vaProfileStr, (VAProfile p), (p), "<unknown>") \
    X(LIBVA, VAStatus, vaPutImage, (VADisplay d, VASurfaceID s, VAImageID i, int sx, int sy, unsigned int sw, unsigned int sh, int dx, int dy, unsigned int dw, unsigned int dh), (d, s, i, sx, sy, sw, sh, dx, dy, dw, dh), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaQueryConfigEntrypoints, (VADisplay d, VAProfile p, VAEntrypoint *e, int *n), (d, p, e, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaQueryConfigProfiles, (VADisplay d, VAProfile *p, int *n), (d, p, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaQueryImageFormats, (VADisplay d, VAImageFormat *f, int *n), (d, f, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaQuerySurfaceAttributes, (VADisplay d, VAConfigID c, VASurfaceAttrib *a, unsigned int *n), (d, c, a, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, const char *, vaQueryVendorString, (VADisplay d), (d), NULL) \
    X(LIBVA, VAStatus, vaQueryVideoProcFilterCaps, (VADisplay d, VAContextID c, VAProcFilterType t, void *f, unsigned int *n), (d, c, t, f, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaQueryVideoProcPipelineCaps, (VADisplay d, VAContextID c, VABufferID *f, unsigned int n, VAProcPipelineCaps *p), (d, c, f, n, p), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaReleaseBufferHandle, (VADisplay d, VABufferID b), (d, b), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaRenderPicture, (VADisplay d, VAContextID c, VABufferID *b, int n), (d, c, b, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaSetDriverName, (VADisplay d, char *n), (d, n), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAMessageCallback, vaSetErrorCallback, (VADisplay d, VAMessageCallback c, void *x), (d, c, x), NULL) \
    X(LIBVA, VAMessageCallback, vaSetInfoCallback, (VADisplay d, VAMessageCallback c, void *x), (d, c, x), NULL) \
    X(LIBVA, VAStatus, vaSyncBuffer, (VADisplay d, VABufferID b, uint64_t t), (d, b, t), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaSyncSurface, (VADisplay d, VASurfaceID s), (d, s), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaTerminate, (VADisplay d), (d), VA_STATUS_ERROR_UNIMPLEMENTED) \
    X(LIBVA, VAStatus, vaUnmapBuffer, (VADisplay d, VABufferID b), (d, b), VA_STATUS_ERROR_UNIMPLEMENTED) \
    V(LIBDRM, drmFreeDevice, (drmDevicePtr *d), (d)) \
    V(LIBDRM, drmFreeVersion, (drmVersionPtr v), (v)) \
    X(LIBDRM, int, drmGetDevice, (int fd, drmDevicePtr *d), (fd, d), -1) \
    X(LIBDRM, int, drmGetNodeTypeFromFd, (int fd), (fd), -1) \
    X(LIBDRM, char *, drmGetRenderDeviceNameFromFd, (int fd), (fd), NULL) \
    X(LIBDRM, drmVersionPtr, drmGetVersion, (int fd), (fd), NULL) \
    V(LIBDRM, drmModeFreeFB, (drmModeFBPtr p), (p)) \
    V(LIBDRM, drmModeFreeFB2, (drmModeFB2Ptr p), (p)) \
    V(LIBDRM, drmModeFreePlane, (drmModePlanePtr p), (p)) \
    V(LIBDRM, drmModeFreePlaneResources, (drmModePlaneResPtr p), (p)) \
    X(LIBDRM, drmModeFBPtr, drmModeGetFB, (int fd, uint32_t b), (fd, b), NULL) \
    X(LIBDRM, drmModeFB2Ptr, drmModeGetFB2, (int fd, uint32_t b), (fd, b), NULL) \
    X(LIBDRM, drmModePlanePtr, drmModeGetPlane, (int fd, uint32_t p), (fd, p), NULL) \
    X(LIBDRM, drmModePlaneResPtr, drmModeGetPlaneResources, (int fd), (fd), NULL) \
    X(LIBDRM, int, drmPrimeHandleToFD, (int fd, uint32_t h, uint32_t f, int *p), (fd, h, f, p), -1) \
    X(LIBDRM, int, drmSetClientCap, (int fd, uint64_t c, uint64_t v), (fd, c, v), -1) \
    X(LIBASOUND, char *, snd_device_name_get_hint, (const void * a0, const char * a1), (a0, a1), NULL) \
    X(LIBPULSE, const char *, pa_strerror, (int a0), (a0), "the library is not available") \
    X(LIBASOUND, const char *, snd_strerror, (int a0), (a0), "the library is not available") \
    X(LIBPULSE, const pa_buffer_attr *, pa_stream_get_buffer_attr, (pa_stream * a0), (a0), NULL) \
    X(LIBPULSE, const pa_sample_spec *, pa_stream_get_sample_spec, (pa_stream * a0), (a0), NULL) \
    X(LIBXCB, const struct xcb_setup_t *, xcb_get_setup, (xcb_connection_t * a0), (a0), NULL) \
    X(LIBPULSE, int, pa_channel_map_valid, (const pa_channel_map * a0), (a0), 0) \
    X(LIBPULSE, int, pa_context_connect, (pa_context * a0, const char * a1, pa_context_flags_t a2, const pa_spawn_api * a3), (a0, a1, a2, a3), -1) \
    X(LIBPULSE, int, pa_context_errno, (const pa_context * a0), (a0), -1) \
    X(LIBPULSE, int, pa_mainloop_iterate, (pa_mainloop * a0, int a1, int * a2), (a0, a1, a2), -1) \
    X(LIBPULSE, int, pa_sample_spec_valid, (const pa_sample_spec * a0), (a0), 0) \
    X(LIBPULSE, int, pa_stream_connect_playback, (pa_stream * a0, const char * a1, const pa_buffer_attr * a2, pa_stream_flags_t a3, const pa_cvolume * a4, pa_stream * a5), (a0, a1, a2, a3, a4, a5), -1) \
    X(LIBPULSE, int, pa_stream_connect_record, (pa_stream * a0, const char * a1, const pa_buffer_attr * a2, pa_stream_flags_t a3), (a0, a1, a2, a3), -1) \
    X(LIBPULSE, int, pa_stream_disconnect, (pa_stream * a0), (a0), -1) \
    X(LIBPULSE, int, pa_stream_drop, (pa_stream * a0), (a0), -1) \
    X(LIBPULSE, int, pa_stream_get_latency, (pa_stream * a0, pa_usec_t * a1, int * a2), (a0, a1, a2), -1) \
    X(LIBPULSE, int, pa_stream_is_corked, (const pa_stream * a0), (a0), 0) \
    X(LIBPULSE, int, pa_stream_peek, (pa_stream * a0, const void ** a1, size_t * a2), (a0, a1, a2), -1) \
    X(LIBPULSE, int, pa_stream_write, (pa_stream * a0, const void * a1, size_t a2, pa_free_cb_t a3, int64_t a4, pa_seek_mode_t a5), (a0, a1, a2, a3, a4, a5), -1) \
    X(LIBPULSE, int, pa_threaded_mainloop_start, (pa_threaded_mainloop * a0), (a0), -1) \
    X(LIBASOUND, int, snd_device_name_free_hint, (void ** a0), (a0), -1) \
    X(LIBASOUND, int, snd_device_name_hint, (int a0, const char * a1, void *** a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_close, (snd_pcm_t * a0), (a0), -1) \
    X(LIBASOUND, int, snd_pcm_delay, (snd_pcm_t * a0, snd_pcm_sframes_t * a1), (a0, a1), -1) \
    X(LIBASOUND, int, snd_pcm_drain, (snd_pcm_t * a0), (a0), -1) \
    X(LIBASOUND, int, snd_pcm_htimestamp, (snd_pcm_t * a0, snd_pcm_uframes_t * a1, snd_htimestamp_t * a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1), (a0, a1), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_any, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1), (a0, a1), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_get_buffer_size_max, (const snd_pcm_hw_params_t * a0, snd_pcm_uframes_t * a1), (a0, a1), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_get_period_size_min, (const snd_pcm_hw_params_t * a0, snd_pcm_uframes_t * a1, int * a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_malloc, (snd_pcm_hw_params_t ** a0), (a0), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_access, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, snd_pcm_access_t a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_buffer_size_near, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, snd_pcm_uframes_t * a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_channels, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, unsigned int a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_format, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, snd_pcm_format_t a2), (a0, a1, a2), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_period_size_near, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, snd_pcm_uframes_t * a2, int * a3), (a0, a1, a2, a3), -1) \
    X(LIBASOUND, int, snd_pcm_hw_params_set_rate_near, (snd_pcm_t * a0, snd_pcm_hw_params_t * a1, unsigned int * a2, int * a3), (a0, a1, a2, a3), -1) \
    X(LIBASOUND, int, snd_pcm_nonblock, (snd_pcm_t * a0, int a1), (a0, a1), -1) \
    X(LIBASOUND, int, snd_pcm_open, (snd_pcm_t ** a0, const char * a1, snd_pcm_stream_t a2, int a3), (a0, a1, a2, a3), -1) \
    X(LIBASOUND, int, snd_pcm_prepare, (snd_pcm_t * a0), (a0), -1) \
    X(LIBXCB, int, xcb_connection_has_error, (xcb_connection_t * a0), (a0), -1) \
    X(LIBXCB, int, xcb_flush, (xcb_connection_t * a0), (a0), -1) \
    X(LIBXCB, int, xcb_get_image_data_length, (const xcb_get_image_reply_t * a0), (a0), -1) \
    X(LIBXCB, int, xcb_setup_pixmap_formats_length, (const xcb_setup_t * a0), (a0), -1) \
    X(LIBPULSE, pa_channel_map *, pa_channel_map_init_extend, (pa_channel_map * a0, unsigned int a1, pa_channel_map_def_t a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_context *, pa_context_new, (pa_mainloop_api * a0, const char * a1), (a0, a1), NULL) \
    X(LIBPULSE, pa_context_state_t, pa_context_get_state, (const pa_context * a0), (a0), PA_CONTEXT_FAILED) \
    X(LIBPULSE, pa_cvolume *, pa_cvolume_set, (pa_cvolume * a0, unsigned int a1, pa_volume_t a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_cvolume *, pa_sw_cvolume_multiply_scalar, (pa_cvolume * a0, const pa_cvolume * a1, pa_volume_t a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_mainloop_api *, pa_mainloop_get_api, (pa_mainloop * a0), (a0), NULL) \
    X(LIBPULSE, pa_mainloop_api *, pa_threaded_mainloop_get_api, (pa_threaded_mainloop * a0), (a0), NULL) \
    X(LIBPULSE, pa_mainloop *, pa_mainloop_new, (void), (), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_get_server_info, (pa_context * a0, pa_server_info_cb_t a1, void * a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_get_sink_info_by_name, (pa_context * a0, const char * a1, pa_sink_info_cb_t a2, void * a3), (a0, a1, a2, a3), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_get_sink_info_list, (pa_context * a0, pa_sink_info_cb_t a1, void * a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_get_sink_input_info, (pa_context * a0, uint32_t a1, pa_sink_input_info_cb_t a2, void * a3), (a0, a1, a2, a3), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_get_source_info_list, (pa_context * a0, pa_source_info_cb_t a1, void * a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_set_sink_input_mute, (pa_context * a0, uint32_t a1, int a2, pa_context_success_cb_t a3, void * a4), (a0, a1, a2, a3, a4), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_set_sink_input_volume, (pa_context * a0, uint32_t a1, const pa_cvolume * a2, pa_context_success_cb_t a3, void * a4), (a0, a1, a2, a3, a4), NULL) \
    X(LIBPULSE, pa_operation *, pa_context_subscribe, (pa_context * a0, pa_subscription_mask_t a1, pa_context_success_cb_t a2, void * a3), (a0, a1, a2, a3), NULL) \
    X(LIBPULSE, pa_operation *, pa_stream_cork, (pa_stream * a0, int a1, pa_stream_success_cb_t a2, void * a3), (a0, a1, a2, a3), NULL) \
    X(LIBPULSE, pa_operation *, pa_stream_flush, (pa_stream * a0, pa_stream_success_cb_t a1, void * a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_operation *, pa_stream_update_timing_info, (pa_stream * a0, pa_stream_success_cb_t a1, void * a2), (a0, a1, a2), NULL) \
    X(LIBPULSE, pa_operation_state_t, pa_operation_get_state, (const pa_operation * a0), (a0), PA_OPERATION_CANCELLED) \
    X(LIBPULSE, pa_stream *, pa_stream_new, (pa_context * a0, const char * a1, const pa_sample_spec * a2, const pa_channel_map * a3), (a0, a1, a2, a3), NULL) \
    X(LIBPULSE, pa_stream_state_t, pa_stream_get_state, (const pa_stream * a0), (a0), PA_STREAM_FAILED) \
    X(LIBPULSE, pa_threaded_mainloop *, pa_threaded_mainloop_new, (void), (), NULL) \
    X(LIBPULSE, pa_volume_t, pa_cvolume_avg, (const pa_cvolume * a0), (a0), 0) \
    X(LIBPULSE, pa_volume_t, pa_sw_volume_divide, (pa_volume_t a0, pa_volume_t a1), (a0, a1), 0) \
    X(LIBPULSE, pa_volume_t, pa_sw_volume_multiply, (pa_volume_t a0, pa_volume_t a1), (a0, a1), 0) \
    X(LIBPULSE, size_t, pa_frame_size, (const pa_sample_spec * a0), (a0), 0) \
    X(LIBPULSE, size_t, pa_stream_writable_size, (const pa_stream * a0), (a0), 0) \
    X(LIBASOUND, snd_pcm_sframes_t, snd_pcm_readi, (snd_pcm_t * a0, void * a1, snd_pcm_uframes_t a2), (a0, a1, a2), -1) \
    X(LIBASOUND, snd_pcm_sframes_t, snd_pcm_writei, (snd_pcm_t * a0, const void * a1, snd_pcm_uframes_t a2), (a0, a1, a2), -1) \
    X(LIBASOUND, snd_pcm_stream_t, snd_pcm_stream, (snd_pcm_t * a0), (a0), SND_PCM_STREAM_PLAYBACK) \
    X(LIBPULSE, uint32_t, pa_stream_get_index, (const pa_stream * a0), (a0), PA_INVALID_INDEX) \
    X(LIBXCB, uint32_t, xcb_generate_id, (xcb_connection_t * a0), (a0), (uint32_t) -1) \
    X(LIBXCB, uint8_t *, xcb_get_image_data, (const xcb_get_image_reply_t * a0), (a0), NULL) \
    V(LIBPULSE, pa_context_disconnect, (pa_context * a0), (a0)) \
    V(LIBPULSE, pa_context_set_state_callback, (pa_context * a0, pa_context_notify_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_context_set_subscribe_callback, (pa_context * a0, pa_context_subscribe_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_context_unref, (pa_context * a0), (a0)) \
    V(LIBPULSE, pa_mainloop_free, (pa_mainloop * a0), (a0)) \
    V(LIBPULSE, pa_operation_unref, (pa_operation * a0), (a0)) \
    V(LIBPULSE, pa_stream_set_latency_update_callback, (pa_stream * a0, pa_stream_notify_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_set_overflow_callback, (pa_stream * a0, pa_stream_notify_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_set_read_callback, (pa_stream * a0, pa_stream_request_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_set_state_callback, (pa_stream * a0, pa_stream_notify_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_set_underflow_callback, (pa_stream * a0, pa_stream_notify_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_set_write_callback, (pa_stream * a0, pa_stream_request_cb_t a1, void * a2), (a0, a1, a2)) \
    V(LIBPULSE, pa_stream_unref, (pa_stream * a0), (a0)) \
    V(LIBPULSE, pa_threaded_mainloop_free, (pa_threaded_mainloop * a0), (a0)) \
    V(LIBPULSE, pa_threaded_mainloop_lock, (pa_threaded_mainloop * a0), (a0)) \
    V(LIBPULSE, pa_threaded_mainloop_signal, (pa_threaded_mainloop * a0, int a1), (a0, a1)) \
    V(LIBPULSE, pa_threaded_mainloop_stop, (pa_threaded_mainloop * a0), (a0)) \
    V(LIBPULSE, pa_threaded_mainloop_unlock, (pa_threaded_mainloop * a0), (a0)) \
    V(LIBPULSE, pa_threaded_mainloop_wait, (pa_threaded_mainloop * a0), (a0)) \
    V(LIBASOUND, snd_pcm_hw_params_free, (snd_pcm_hw_params_t * a0), (a0)) \
    V(LIBXCB, xcb_disconnect, (xcb_connection_t * a0), (a0)) \
    V(LIBXCB, xcb_screen_next, (xcb_screen_iterator_t * a0), (a0)) \
    X(LIBXCB, xcb_connection_t *, xcb_connect, (const char * a0, int * a1), (a0, a1), NULL) \
    X(LIBXCB, xcb_format_t *, xcb_setup_pixmap_formats, (const xcb_setup_t * a0), (a0), NULL) \
    X(LIBXCB, xcb_generic_event_t *, xcb_wait_for_event, (xcb_connection_t * a0), (a0), NULL) \
    X(LIBXCB, xcb_get_geometry_cookie_t, xcb_get_geometry, (xcb_connection_t * a0, xcb_drawable_t a1), (a0, a1), (xcb_get_geometry_cookie_t) {0}) \
    X(LIBXCB, xcb_get_geometry_reply_t *, xcb_get_geometry_reply, (xcb_connection_t * a0, xcb_get_geometry_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_get_image_cookie_t, xcb_get_image, (xcb_connection_t * a0, uint8_t a1, xcb_drawable_t a2, int16_t a3, int16_t a4, uint16_t a5, uint16_t a6, uint32_t a7), (a0, a1, a2, a3, a4, a5, a6, a7), (xcb_get_image_cookie_t) {0}) \
    X(LIBXCB, xcb_get_image_reply_t *, xcb_get_image_reply, (xcb_connection_t * a0, xcb_get_image_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_grab_pointer_cookie_t, xcb_grab_pointer, (xcb_connection_t * a0, uint8_t a1, xcb_window_t a2, uint16_t a3, uint8_t a4, uint8_t a5, xcb_window_t a6, xcb_cursor_t a7, xcb_timestamp_t a8), (a0, a1, a2, a3, a4, a5, a6, a7, a8), (xcb_grab_pointer_cookie_t) {0}) \
    X(LIBXCB, xcb_grab_pointer_reply_t *, xcb_grab_pointer_reply, (xcb_connection_t * a0, xcb_grab_pointer_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_query_pointer_cookie_t, xcb_query_pointer, (xcb_connection_t * a0, xcb_window_t a1), (a0, a1), (xcb_query_pointer_cookie_t) {0}) \
    X(LIBXCB, xcb_query_pointer_reply_t *, xcb_query_pointer_reply, (xcb_connection_t * a0, xcb_query_pointer_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_screen_iterator_t, xcb_setup_roots_iterator, (const xcb_setup_t * a0), (a0), (xcb_screen_iterator_t) {0}) \
    X(LIBXCB_SHM, xcb_shm_get_image_cookie_t, xcb_shm_get_image, (xcb_connection_t * a0, xcb_drawable_t a1, int16_t a2, int16_t a3, uint16_t a4, uint16_t a5, uint32_t a6, uint8_t a7, xcb_shm_seg_t a8, uint32_t a9), (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9), (xcb_shm_get_image_cookie_t) {0}) \
    X(LIBXCB_SHM, xcb_shm_get_image_reply_t *, xcb_shm_get_image_reply, (xcb_connection_t * a0, xcb_shm_get_image_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB_SHM, xcb_shm_query_version_cookie_t, xcb_shm_query_version, (xcb_connection_t * a0), (a0), (xcb_shm_query_version_cookie_t) {0}) \
    X(LIBXCB_SHM, xcb_shm_query_version_reply_t *, xcb_shm_query_version_reply, (xcb_connection_t * a0, xcb_shm_query_version_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_translate_coordinates_cookie_t, xcb_translate_coordinates, (xcb_connection_t * a0, xcb_window_t a1, xcb_window_t a2, int16_t a3, int16_t a4), (a0, a1, a2, a3, a4), (xcb_translate_coordinates_cookie_t) {0}) \
    X(LIBXCB, xcb_translate_coordinates_reply_t *, xcb_translate_coordinates_reply, (xcb_connection_t * a0, xcb_translate_coordinates_cookie_t a1, xcb_generic_error_t ** a2), (a0, a1, a2), NULL) \
    X(LIBXCB, xcb_void_cookie_t, xcb_close_font, (xcb_connection_t * a0, xcb_font_t a1), (a0, a1), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_configure_window, (xcb_connection_t * a0, xcb_window_t a1, uint16_t a2, const void * a3), (a0, a1, a2, a3), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_create_gc, (xcb_connection_t * a0, xcb_gcontext_t a1, xcb_drawable_t a2, uint32_t a3, const void * a4), (a0, a1, a2, a3, a4), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_create_glyph_cursor, (xcb_connection_t * a0, xcb_cursor_t a1, xcb_font_t a2, xcb_font_t a3, uint16_t a4, uint16_t a5, uint16_t a6, uint16_t a7, uint16_t a8, uint16_t a9, uint16_t a10, uint16_t a11), (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_create_window, (xcb_connection_t * a0, uint8_t a1, xcb_window_t a2, xcb_window_t a3, int16_t a4, int16_t a5, uint16_t a6, uint16_t a7, uint16_t a8, uint16_t a9, xcb_visualid_t a10, uint32_t a11, const void * a12), (a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_free_cursor, (xcb_connection_t * a0, xcb_cursor_t a1), (a0, a1), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_free_gc, (xcb_connection_t * a0, xcb_gcontext_t a1), (a0, a1), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_grab_server, (xcb_connection_t * a0), (a0), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_map_window, (xcb_connection_t * a0, xcb_window_t a1), (a0, a1), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_open_font, (xcb_connection_t * a0, xcb_font_t a1, uint16_t a2, const char * a3), (a0, a1, a2, a3), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_poly_rectangle, (xcb_connection_t * a0, xcb_drawable_t a1, xcb_gcontext_t a2, uint32_t a3, const xcb_rectangle_t * a4), (a0, a1, a2, a3, a4), (xcb_void_cookie_t) {0}) \
    X(LIBXCB_SHM, xcb_void_cookie_t, xcb_shm_attach, (xcb_connection_t * a0, xcb_shm_seg_t a1, uint32_t a2, uint8_t a3), (a0, a1, a2, a3), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_ungrab_pointer, (xcb_connection_t * a0, xcb_timestamp_t a1), (a0, a1), (xcb_void_cookie_t) {0}) \
    X(LIBXCB, xcb_void_cookie_t, xcb_ungrab_server, (xcb_connection_t * a0), (a0), (xcb_void_cookie_t) {0}) \
    X(LIBUDEV, struct udev *, udev_new, (void), (), NULL) \
    X(LIBUDEV, struct udev *, udev_unref, (struct udev *u), (u), NULL) \
    X(LIBUDEV, struct udev_list_entry *, udev_list_entry_get_next, (struct udev_list_entry *e), (e), NULL) \
    X(LIBUDEV, const char *, udev_list_entry_get_name, (struct udev_list_entry *e), (e), NULL) \
    X(LIBUDEV, struct udev_device *, udev_device_unref, (struct udev_device *d), (d), NULL) \
    X(LIBUDEV, struct udev *, udev_device_get_udev, (struct udev_device *d), (d), NULL) \
    X(LIBUDEV, struct udev_device *, udev_device_new_from_syspath, (struct udev *u, const char *p), (u, p), NULL) \
    X(LIBUDEV, struct udev_device *, udev_device_new_from_devnum, (struct udev *u, char t, dev_t n), (u, t, n), NULL) \
    X(LIBUDEV, const char *, udev_device_get_devnode, (struct udev_device *d), (d), NULL) \
    X(LIBUDEV, struct udev_enumerate *, udev_enumerate_unref, (struct udev_enumerate *e), (e), NULL) \
    X(LIBUDEV, struct udev_enumerate *, udev_enumerate_new, (struct udev *u), (u), NULL) \
    X(LIBUDEV, int, udev_enumerate_add_match_subsystem, (struct udev_enumerate *e, const char *s), (e, s), -1) \
    X(LIBUDEV, int, udev_enumerate_scan_devices, (struct udev_enumerate *e), (e), -1) \
    X(LIBUDEV, struct udev_list_entry *, udev_enumerate_get_list_entry, (struct udev_enumerate *e), (e), NULL)

#define POINTER(library, type, name, parameters, arguments, failure) static type (*real_##name) parameters;
#define VOID_POINTER(library, name, parameters, arguments) static void (*real_##name) parameters;
FUNCTIONS(POINTER, VOID_POINTER)

#define RESOLVE(library, type, name, parameters, arguments, failure) \
    if (libraries[library]) real_##name = (type (*) parameters) dlsym(libraries[library], #name);
#define VOID_RESOLVE(library, name, parameters, arguments) \
    if (libraries[library]) real_##name = (void (*) parameters) dlsym(libraries[library], #name);

static void load(void) {
    for (int i = 0; i < LIBRARY_COUNT; i++) libraries[i] = dlopen(library_names[i], RTLD_NOW | RTLD_LOCAL);
    FUNCTIONS(RESOLVE, VOID_RESOLVE)
}

#define FORWARD(library, type, name, parameters, arguments, failure) \
    type name parameters { \
        pthread_once(&load_once, load); \
        return real_##name ? real_##name arguments : failure; \
    }
#define VOID_FORWARD(library, name, parameters, arguments) \
    void name parameters { \
        pthread_once(&load_once, load); \
        if (real_##name) real_##name arguments; \
    }
FUNCTIONS(FORWARD, VOID_FORWARD)
