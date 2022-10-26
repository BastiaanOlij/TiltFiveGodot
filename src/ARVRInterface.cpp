////////////////////////////////////////////////////////////////////////////////////////////////
// ARVRInterface code for Tilt FIve GDNative module



#include <OS.hpp>
#include "ARVRInterface.h"
#include "VisualServer.hpp"
#include "TiltFiveManager.h"

using godot::VisualServer;

inline Transform AsCpp(godot_transform& tran) {
	static_assert(sizeof(godot_transform) == sizeof(Transform));
	return *reinterpret_cast<Transform*>(&tran);
}

inline godot_transform AsC(Transform& tran) {
	static_assert(sizeof(godot_transform) == sizeof(Transform));
	return *reinterpret_cast<godot_transform*>(&tran);
}

inline Vector2 AsCpp(godot_vector2& vec) {
	static_assert(sizeof(godot_vector2) == sizeof(Vector2));
	return *reinterpret_cast<Vector2*>(&vec);
}

inline godot_vector2 AsC(Vector2& vec) {
	static_assert(sizeof(godot_vector2) == sizeof(Vector2));
	return *reinterpret_cast<godot_vector2*>(&vec);
}

inline TiltFiveManager* GetT5Manager(const void *p_data) {
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;
	
	return arvr_data ? arvr_data->manager : nullptr;
}

inline GlassesPtr GetActiveT5Glasses(const void *p_data) {
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;
	
	return arvr_data ? (arvr_data->manager ? arvr_data->manager->activeGlasses : GlassesPtr()) : GlassesPtr();
}

////////////////////////////////////////////////////////////////
// Returns the name of this interface
godot_string godot_arvr_get_name(const void *p_data) {
	godot_string ret;

	char name[] = "TiltFive";
	godot::api->godot_string_new(&ret);
	godot::api->godot_string_parse_utf8(&ret, name);

	return ret;
}

////////////////////////////////////////////////////////////////
// Returns capabilities for this interface
godot_int godot_arvr_get_capabilities(const void *p_data) {
	godot_int ret;

	ret = 2 + 8; // 2 = ARVR_STEREO, 8 = ARVR_EXTERNAL

	return ret;
}

////////////////////////////////////////////////////////////////
// This is an AR feature not used here
godot_bool godot_arvr_get_anchor_detection_is_enabled(const void *p_data) {
	godot_bool ret;

	ret = false; // does not apply here

	return ret;
}

////////////////////////////////////////////////////////////////
// This is an AR feature not used here
void godot_arvr_set_anchor_detection_is_enabled(void *p_data, bool p_enable) {
	// we ignore this, not supported in this interface!
}

////////////////////////////////////////////////////////////////
// Informs Godot stereoscopic rendering is required
godot_bool godot_arvr_is_stereo(const void *p_data) {
	godot_bool ret;

	ret = true;

	return ret;
}

////////////////////////////////////////////////////////////////
// Returns whether our interface was successfully initialised
godot_bool godot_arvr_is_initialized(const void *p_data) {
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;

	if (arvr_data == NULL) {
		return false;
	}

	return arvr_data->manager->IsInitialized();
}

////////////////////////////////////////////////////////////////
// Initialise our interface, sets up OpenVR and starts sending
// output to our HMD
// Note that you should do any configuration using OpenVRConfig
// before initializing the interface.
godot_bool godot_arvr_initialize(void *p_data) {
	godot::Godot::print("godot_arvr_initialize");
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;
	bool isInitialized = arvr_data->manager->Initialize(); 
	if(!isInitialized)
		arvr_data->manager->Uninitialize();
	return isInitialized;
}

////////////////////////////////////////////////////////////////
// Uninitialises our interface, shuts down our HMD
void godot_arvr_uninitialize(void *p_data) {
	godot::Godot::print("godot_arvr_uninitialize");
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;
	arvr_data->manager->Uninitialize(); 
}

////////////////////////////////////////////////////////////////
// Returns the requested size of our render target
// called right before rendering, if the size changes a new
// render target will be constructed.
godot_vector2 godot_arvr_get_render_targetsize(const void *p_data) {
	auto glasses = GetActiveT5Glasses(p_data); 
	
	Vector2 result(1216, 768);
	if(glasses)
		result = glasses->GetRenderTargetSize();

	return AsC(result);
}

////////////////////////////////////////////////////////////////
// This is called while rendering to get each eyes view matrix
godot_transform godot_arvr_get_transform_for_eye(void *p_data, godot_int p_eye, godot_transform *p_cam_transform) {
	auto glasses = GetActiveT5Glasses(p_data); 

	Transform eyeTransform;
	Transform referenceFrame = AsCpp(godot::arvr_api->godot_arvr_get_reference_frame());

	Transform ret;
	godot_real worldScale = godot::arvr_api->godot_arvr_get_worldscale();

 	if (glasses) {
		eyeTransform = glasses->GetEyeToHeadTransform( (Glasses::Eye)p_eye, worldScale) * glasses->GetHeadTransform();
	} else {
		eyeTransform.translate((p_eye == 1 ? -0.035f : 0.035f) * worldScale, 0.0f, 0.0f);
	};

	ret = eyeTransform * referenceFrame * AsCpp(*p_cam_transform);

	return AsC(ret);
}

////////////////////////////////////////////////////////////////
// This is called while rendering to get each eyes projection
// matrix

void godot_arvr_fill_projection_for_eye(void *p_data, godot_real *p_projection, godot_int p_eye, godot_real p_aspect, godot_real p_z_near, godot_real p_z_far) {
	auto glasses = GetActiveT5Glasses(p_data); 

	CameraMatrix cm;

	if(glasses) {
		cm = glasses->GetProjectionForEye(p_eye == 1 ? Glasses::Left : Glasses::Right, p_aspect, p_z_near, p_z_far);
	}
	else {
		cm.set_perspective(48.0f, p_aspect, p_z_near, p_z_far);
	}	

	memcpy(p_projection, cm.matrix, sizeof(cm.matrix));
}

////////////////////////////////////////////////////////////////
// This is called after we render a frame for each eye so we
// can send the render output to OpenVR
void godot_arvr_commit_for_eye(void *p_data, godot_int p_eye, godot_rid *p_render_target, godot_rect2 *p_screen_rect) {
	auto glasses = GetActiveT5Glasses(p_data); 

	// This function is responsible for outputting the final render buffer for
	// each eye.
	// p_screen_rect will only have a value when we're outputting to the main
	// viewport.

	// For an interface that must output to the main viewport (such as with mobile
	// VR) we should give an error when p_screen_rect is not set
	// For an interface that outputs to an external device we should render a copy
	// of one of the eyes to the main viewport if p_screen_rect is set, and only
	// output to the external device if not.


	static int oncel = 3;
	if(oncel > 0 && p_eye == 1) 
	{
		uint32_t texid = godot::arvr_api->godot_arvr_get_texid(p_render_target);

		godot::Godot::print(String("Send left eye texture = " + String::num_int64(texid)));
		--oncel;
	}

	static int oncer = 3;
	if(oncer > 0 && p_eye == 2) 
	{
		uint32_t texid = godot::arvr_api->godot_arvr_get_texid(p_render_target);

		godot::Godot::print(String("Send right eye texture = " + String::num_int64(texid)));
		--oncer;
	}


	godot::Rect2 screen_rect = *(godot::Rect2 *)p_screen_rect;

	if (p_eye == 1 && !screen_rect.has_no_area()) {
		// blit as mono, attempt to keep our aspect ratio and center our render buffer
		godot_vector2 rs = godot_arvr_get_render_targetsize(p_data);
		godot::Vector2 *render_size = (godot::Vector2 *)&rs;

		float new_height = screen_rect.size.x * (render_size->y / render_size->x);
		if (new_height > screen_rect.size.y) {
			screen_rect.position.y = (0.5f * screen_rect.size.y) - (0.5f * new_height);
			screen_rect.size.y = new_height;
		} else {
			float new_width = screen_rect.size.y * (render_size->x / render_size->y);

			screen_rect.position.x = (0.5f * screen_rect.size.x) - (0.5f * new_width);
			screen_rect.size.x = new_width;
		}

		// printf("Blit: %0.2f, %0.2f - %0.2f, %0.2f\n",screen_rect.position.x,screen_rect.position.y,screen_rect.size.x,screen_rect.size.y);

		//godot::arvr_api->godot_arvr_blit(0, p_render_target, (godot_rect2 *)&screen_rect);
	}

	if (glasses && p_eye == 2) {
		glasses->SendFrame();
	}
 
}

////////////////////////////////////////////////////////////////
// Process is called by the rendering thread right before we
// render our next frame. Here we obtain our new poses.
// The HMD pose is used right away but tracker poses are used
// next frame.
void godot_arvr_process(void *p_data) {
	arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;

	// this method gets called before every frame is rendered, here is where you
	// should update tracking data, update controllers, etc.
 	if (arvr_data->manager->IsInitialized()) {
		// Call process on our ovr system.
		arvr_data->manager->Process();
	} 
}

////////////////////////////////////////////////////////////////
// Construct our interface so it can be registered
// we do not initialise anything here!
void *godot_arvr_constructor(godot_object *p_instance) {
	godot::Godot::print("godot_arvr_constructor");

	arvr_data_struct *arvr_data = (arvr_data_struct *)godot::api->godot_alloc(sizeof(arvr_data_struct));
	arvr_data->manager = new TiltFiveManager();

	return arvr_data;
}

////////////////////////////////////////////////////////////////
// Clean up our interface
void godot_arvr_destructor(void *p_data) {
	godot::Godot::print("godot_arvr_destructor");
	if (p_data != NULL) {
		arvr_data_struct *arvr_data = (arvr_data_struct *)p_data;
		
		delete arvr_data->manager;

		godot::api->godot_free(p_data);
	}
}

////////////////////////////////////////////////////////////////
// Return a texture ID for the eye if we manage the final
// output buffer.
int godot_arvr_get_external_texture_for_eye(void *p_data, int p_eye) {

	auto glasses = GetActiveT5Glasses(p_data);

	if(glasses && glasses->IsReadyToDisplay()) 
	{
		return VisualServer::get_singleton()
			->texture_get_texid(
				glasses->GetTextureForEye(p_eye == 1 ? Glasses::Left : Glasses::Right)
			);
	}

	return 0;
}

////////////////////////////////////////////////////////////////
// Receive notifications sent to our ARVROrigin node.
void godot_arvr_notification(void *p_data, int p_what) {
	// nothing to do here for now but we should implement this.
}

////////////////////////////////////////////////////////////////
// Return the camera feed that should be used for our background
// when we're dealing with AR.
int godot_arvr_get_camera_feed_id(void *) {
	return 0;
}

////////////////////////////////////////////////////////////////
// Return a texture ID for the eye if we manage the depth buffer
int godot_arvr_get_external_depth_for_eye(void *p_data, int p_eye) {
	return 0;
}

////////////////////////////////////////////////////////////////
// Structure to provide pointers to our interface functions.
const godot_arvr_interface_gdnative interface_struct = {
	GODOTVR_API_MAJOR, GODOTVR_API_MINOR,
	godot_arvr_constructor,
	godot_arvr_destructor,
	godot_arvr_get_name,
	godot_arvr_get_capabilities,
	godot_arvr_get_anchor_detection_is_enabled,
	godot_arvr_set_anchor_detection_is_enabled,
	godot_arvr_is_stereo,
	godot_arvr_is_initialized,
	godot_arvr_initialize,
	godot_arvr_uninitialize,
	godot_arvr_get_render_targetsize,
	godot_arvr_get_transform_for_eye,
	godot_arvr_fill_projection_for_eye,
	godot_arvr_commit_for_eye,
	godot_arvr_process,
	// only available in Godot 3.2+
	godot_arvr_get_external_texture_for_eye,
	godot_arvr_notification,
	godot_arvr_get_camera_feed_id,
	// only available in Godot 3.3+
	godot_arvr_get_external_depth_for_eye
};
