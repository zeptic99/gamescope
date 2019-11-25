// DRM output stuff

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define C_SIDE

#include "drm.h"

struct drm_t g_DRM;

static uint32_t find_crtc_for_encoder(const drmModeRes *resources,
		const drmModeEncoder *encoder) {
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		/* possible_crtcs is a bitmask as described here:
		 * https://dvdhrm.wordpress.com/2012/09/13/linux-drm-mode-setting-api
		 */
		const uint32_t crtc_mask = 1 << i;
		const uint32_t crtc_id = resources->crtcs[i];
		if (encoder->possible_crtcs & crtc_mask) {
			return crtc_id;
		}
	}

	/* no match found */
	return -1;
}

static uint32_t find_crtc_for_connector(const struct drm_t *drm, const drmModeRes *resources,
		const drmModeConnector *connector) {
	int i;

	for (i = 0; i < connector->count_encoders; i++) {
		const uint32_t encoder_id = connector->encoders[i];
		drmModeEncoder *encoder = drmModeGetEncoder(drm->fd, encoder_id);

		if (encoder) {
			const uint32_t crtc_id = find_crtc_for_encoder(resources, encoder);

			drmModeFreeEncoder(encoder);
			if (crtc_id != 0) {
				return crtc_id;
			}
		}
	}

	/* no match found */
	return -1;
}

static int get_resources(int fd, drmModeRes **resources)
{
	*resources = drmModeGetResources(fd);
	if (*resources == NULL)
		return -1;
	return 0;
}

#define MAX_DRM_DEVICES 64

static int find_drm_device(drmModeRes **resources)
{
	drmDevicePtr devices[MAX_DRM_DEVICES] = { NULL };
	int num_devices, fd = -1;
	
	num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
	if (num_devices < 0) {
		printf("drmGetDevices2 failed: %s\n", strerror(-num_devices));
		return -1;
	}
	
	for (int i = 0; i < num_devices; i++) {
		drmDevicePtr device = devices[i];
		int ret;
		
		if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY)))
			continue;
		/* OK, it's a primary device. If we can get the
		 * drmModeResources, it means it's also a
		 * KMS-capable device.
		 */
		fd = open(device->nodes[DRM_NODE_PRIMARY], O_RDWR);
		if (fd < 0)
			continue;
		ret = get_resources(fd, resources);
		if (!ret)
			break;
		close(fd);
		fd = -1;
	}
	drmFreeDevices(devices, num_devices);
	
	if (fd < 0)
		printf("no drm device found!\n");
	return fd;
}

int init_drm(struct drm_t *drm, const char *device, const char *mode_str, unsigned int vrefresh)
{
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, ret, area;
	
	if (device) {
		drm->fd = open(device, O_RDWR);
		ret = get_resources(drm->fd, &resources);
		if (ret < 0 && errno == EOPNOTSUPP)
			printf("%s does not look like a modeset device\n", device);
	} else {
		drm->fd = find_drm_device(&resources);
	}
	
	if (drm->fd < 0) {
		printf("could not open drm device\n");
		return -1;
	}
	
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}
	
	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm->fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}
	
	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
		return -1;
	}
	
	/* find user requested mode: */
	if (mode_str && *mode_str) {
		for (i = 0; i < connector->count_modes; i++) {
			drmModeModeInfo *current_mode = &connector->modes[i];
			
			if (strcmp(current_mode->name, mode_str) == 0) {
				if (vrefresh == 0 || current_mode->vrefresh == vrefresh) {
					drm->mode = current_mode;
					break;
				}
			}
		}
		if (!drm->mode)
			printf("requested mode not found, using default mode!\n");
	}
	
	/* find preferred mode or the highest resolution mode: */
	if (!drm->mode) {
		for (i = 0, area = 0; i < connector->count_modes; i++) {
			drmModeModeInfo *current_mode = &connector->modes[i];
			
			if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
				drm->mode = current_mode;
				break;
			}
			
			int current_area = current_mode->hdisplay * current_mode->vdisplay;
			if (current_area > area) {
				drm->mode = current_mode;
				area = current_area;
			}
		}
	}
	
	if (!drm->mode) {
		printf("could not find mode!\n");
		return -1;
	}
	
	/* find encoder: */
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(drm->fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}
	
	if (encoder) {
		drm->crtc_id = encoder->crtc_id;
	} else {
		uint32_t crtc_id = find_crtc_for_connector(drm, resources, connector);
		if (crtc_id == 0) {
			printf("no crtc found!\n");
			return -1;
		}
		
		drm->crtc_id = crtc_id;
	}
	
	for (i = 0; i < resources->count_crtcs; i++) {
		if (resources->crtcs[i] == drm->crtc_id) {
			drm->crtc_index = i;
			break;
		}
	}
	
	drmModeFreeResources(resources);
	
	drm->connector_id = connector->connector_id;
	
	return 0;
}
