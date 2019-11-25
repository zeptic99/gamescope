// DRM output stuff

#pragma once

#include <xf86drm.h>
#include <xf86drmMode.h>

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
};

#ifndef C_SIDE
extern "C" {
#endif

extern struct drm_t g_DRM;

int init_drm(struct drm_t *drm, const char *device, const char *mode_str, unsigned int vrefresh);

#ifndef C_SIDE
}
#endif
