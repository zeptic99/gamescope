// DRM output stuff

#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <wlr/render/dmabuf.h>

struct plane {
	drmModePlane *plane;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct crtc {
	drmModeCrtc *crtc;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct connector {
	drmModeConnector *connector;
	drmModeObjectProperties *props;
	drmModePropertyRes **props_info;
};

struct drm_t {
	int fd;
	
	/* only used for atomic: */
	struct plane *plane;
	struct crtc *crtc;
	struct connector *connector;
	int crtc_index;
	int kms_in_fence_fd;
	int kms_out_fence_fd;
	
	drmModeModeInfo *mode;
	uint32_t crtc_id;
	uint32_t connector_id;
	
	uint32_t plane_id;
};

#ifndef C_SIDE
extern "C" {
#endif

extern struct drm_t g_DRM;

int init_drm(struct drm_t *drm, const char *device, const char *mode_str, unsigned int vrefresh);
int drm_atomic_commit(struct drm_t *drm, uint32_t fb_id, uint32_t flags);
uint32_t drm_fbid_from_dmabuf( struct drm_t *drm, struct wlr_dmabuf_attributes *dma_buf );

#ifndef C_SIDE
}
#endif
