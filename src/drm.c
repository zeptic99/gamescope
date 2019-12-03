// DRM output stuff

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

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

/* Pick a plane.. something that at a minimum can be connected to
 * the chosen crtc, but prefer primary plane.
 *
 * Seems like there is some room for a drmModeObjectGetNamedProperty()
 * type helper in libdrm..
 */
static int get_plane_id(struct drm_t *drm)
{
	drmModePlaneResPtr plane_resources;
	uint32_t i, j;
	int ret = -EINVAL;
	int found_primary = 0;
	
	plane_resources = drmModeGetPlaneResources(drm->fd);
	if (!plane_resources) {
		printf("drmModeGetPlaneResources failed: %s\n", strerror(errno));
		return -1;
	}
	
	for (i = 0; (i < plane_resources->count_planes) && !found_primary; i++) {
		uint32_t id = plane_resources->planes[i];
		drmModePlanePtr plane = drmModeGetPlane(drm->fd, id);
		if (!plane) {
			printf("drmModeGetPlane(%u) failed: %s\n", id, strerror(errno));
			continue;
		}
		
		if (plane->possible_crtcs & (1 << drm->crtc_index)) {
			drmModeObjectPropertiesPtr props =
			drmModeObjectGetProperties(drm->fd, id, DRM_MODE_OBJECT_PLANE);
			
			/* primary or not, this plane is good enough to use: */
			ret = id;
			
			for (j = 0; j < props->count_props; j++) {
				drmModePropertyPtr p =
				drmModeGetProperty(drm->fd, props->props[j]);
				
				if ((strcmp(p->name, "type") == 0) &&
					(props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY)) {
					/* found our primary plane, lets use that: */
					found_primary = 1;
					}
					
					drmModeFreeProperty(p);
			}
			
			drmModeFreeObjectProperties(props);
		}
		
		drmModeFreePlane(plane);
	}
	
	drmModeFreePlaneResources(plane_resources);
	
	return ret;
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
	
	drmSetClientCap(drm->fd, DRM_CLIENT_CAP_ATOMIC, 1);
	
	drm->plane_id = get_plane_id( &g_DRM );
	
	if ( drm->plane_id == 0 )
	{
		printf("could not find a suitable plane\n");
		return -1;
	}
	
	drm->plane = calloc(1, sizeof(*drm->plane));
	drm->crtc = calloc(1, sizeof(*drm->crtc));
	drm->connector = calloc(1, sizeof(*drm->connector));

#define get_resource(type, Type, id) do { 					\
		drm->type->type = drmModeGet##Type(drm->fd, id);			\
		if (!drm->type->type) {						\
			printf("could not get %s %i: %s\n",			\
					#type, id, strerror(errno));		\
			return -1;						\
		}								\
	} while (0)

	get_resource(plane, Plane, drm->plane_id);
	get_resource(crtc, Crtc, drm->crtc_id);
	get_resource(connector, Connector, drm->connector_id);

#define get_properties(type, TYPE, id) do {					\
		uint32_t i;							\
		drm->type->props = drmModeObjectGetProperties(drm->fd,		\
				id, DRM_MODE_OBJECT_##TYPE);			\
		if (!drm->type->props) {						\
			printf("could not get %s %u properties: %s\n", 		\
					#type, id, strerror(errno));		\
			return -1;						\
		}								\
		drm->type->props_info = calloc(drm->type->props->count_props,	\
				sizeof(*drm->type->props_info));			\
		for (i = 0; i < drm->type->props->count_props; i++) {		\
			drm->type->props_info[i] = drmModeGetProperty(drm->fd,	\
					drm->type->props->props[i]);		\
		}								\
	} while (0)

	get_properties(plane, PLANE, drm->plane_id);
	get_properties(crtc, CRTC, drm->crtc_id);
	get_properties(connector, CONNECTOR, drm->connector_id);
	
	drm->kms_in_fence_fd = -1;
	
	return 0;
}

static int add_connector_property(struct drm_t *drm, drmModeAtomicReq *req,
								  uint32_t obj_id, const char *name,
								  uint64_t value)
{
	struct connector *obj = drm->connector;
	unsigned int i;
	int prop_id = 0;
	
	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}
	
	if (prop_id < 0) {
		printf("no connector property: %s\n", name);
		return -EINVAL;
	}
	
	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_crtc_property(struct drm_t *drm, drmModeAtomicReq *req,
							 uint32_t obj_id, const char *name,
							 uint64_t value)
{
	struct crtc *obj = drm->crtc;
	unsigned int i;
	int prop_id = -1;
	
	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}
	
	if (prop_id < 0) {
		printf("no crtc property: %s\n", name);
		return -EINVAL;
	}
	
	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

static int add_plane_property(struct drm_t *drm, drmModeAtomicReq *req,
							  uint32_t obj_id, const char *name,
							  uint64_t value)
{
	struct plane *obj = drm->plane;
	unsigned int i;
	int prop_id = -1;
	
	for (i = 0 ; i < obj->props->count_props ; i++) {
		if (strcmp(obj->props_info[i]->name, name) == 0) {
			prop_id = obj->props_info[i]->prop_id;
			break;
		}
	}
	
	
	if (prop_id < 0) {
		printf("no plane property: %s\n", name);
		return -EINVAL;
	}
	
	return drmModeAtomicAddProperty(req, obj_id, prop_id, value);
}

int drm_atomic_commit(struct drm_t *drm, uint32_t fb_id, uint32_t width, uint32_t height, uint32_t flags)
{
	drmModeAtomicReq *req;
	uint32_t plane_id = drm->plane->plane->plane_id;
	uint32_t blob_id;
	int ret;
	
	req = drmModeAtomicAlloc();
	
	if (flags & DRM_MODE_ATOMIC_ALLOW_MODESET) {
		if (add_connector_property(drm, req, drm->connector_id, "CRTC_ID",
			drm->crtc_id) < 0)
			return -1;
		
		if (drmModeCreatePropertyBlob(drm->fd, drm->mode, sizeof(*drm->mode),
			&blob_id) != 0)
			return -1;
		
		if (add_crtc_property(drm, req, drm->crtc_id, "MODE_ID", blob_id) < 0)
			return -1;
		
		if (add_crtc_property(drm, req, drm->crtc_id, "ACTIVE", 1) < 0)
			return -1;
	}
	
	add_plane_property(drm, req, plane_id, "FB_ID", fb_id);
	add_plane_property(drm, req, plane_id, "CRTC_ID", drm->crtc_id);
	add_plane_property(drm, req, plane_id, "SRC_X", 0);
	add_plane_property(drm, req, plane_id, "SRC_Y", 0);
	add_plane_property(drm, req, plane_id, "SRC_W", width << 16);
	add_plane_property(drm, req, plane_id, "SRC_H", height << 16);
	add_plane_property(drm, req, plane_id, "CRTC_X", 0);
	add_plane_property(drm, req, plane_id, "CRTC_Y", 0);
	add_plane_property(drm, req, plane_id, "CRTC_W", drm->mode->hdisplay);
	add_plane_property(drm, req, plane_id, "CRTC_H", drm->mode->vdisplay);
	
	if (drm->kms_in_fence_fd != -1) {
		add_plane_property(drm, req, plane_id, "IN_FENCE_FD", drm->kms_in_fence_fd);
	}
	
	drm->kms_out_fence_fd = -1;
	
	add_crtc_property(drm, req, drm->crtc_id, "OUT_FENCE_PTR",
					  (uint64_t)(unsigned long)&drm->kms_out_fence_fd);
	
	
	ret = drmModeAtomicCommit(drm->fd, req, flags, NULL);
	if (ret)
	{
		if ( ret != -EBUSY ) 
		{
			printf("flip error %d\n", ret);
		}
		goto out;
	}
	
	if (drm->kms_in_fence_fd != -1) {
		close(drm->kms_in_fence_fd);
		drm->kms_in_fence_fd = -1;
	}
	
	drm->kms_in_fence_fd = drm->kms_out_fence_fd;
	
out:
	drmModeAtomicFree(req);
	
	return ret;
}

uint32_t drm_fbid_from_dmabuf( struct drm_t *drm, struct wlr_dmabuf_attributes *dma_buf )
{
	uint32_t ret = 0;
	uint32_t handles[4] = { 0 };
	drmPrimeFDToHandle( drm->fd, dma_buf->fd[0], &handles[0] );
	
	drmModeAddFB2( drm->fd, dma_buf->width, dma_buf->height, dma_buf->format, handles, dma_buf->stride, dma_buf->offset, &ret, 0 );
	
	return ret;
}
