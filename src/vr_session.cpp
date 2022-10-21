#include "vr_session.hpp"
#include "main.hpp"
#include "rendervulkan.hpp"
#include "steamcompmgr.hpp"
#include "wlserver.hpp"
#include "log.hpp"
#include "ime.hpp"

#include <string.h>
#include <thread>

static LogScope openvr_log("openvr");

static bool GetVulkanInstanceExtensionsRequired( std::vector< std::string > &outInstanceExtensionList );
static bool GetVulkanDeviceExtensionsRequired( VkPhysicalDevice pPhysicalDevice, std::vector< std::string > &outDeviceExtensionList );
static void vrsession_input_thread();

struct OpenVRSession
{
    const char *pchOverlayKey = nullptr;
    const char *pchOverlayName = nullptr;
    const char *pchOverlayIcon = nullptr;
    bool bExplicitOverlayName = false;
    bool bNudgeToVisible = false;
    bool bEnableControlBar = false;
    bool bEnableControlBarKeyboard = false;
    bool bEnableControlBarClose = false;
    bool bModal = false;
    float flPhysicalWidth = 2.0f;
    float flPhysicalCurvature = 0.0f;
    float flPhysicalPreCurvePitch = 0.0f;
    vr::VROverlayHandle_t hOverlay = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t hOverlayThumbnail = vr::k_ulOverlayHandleInvalid;
    struct wlserver_input_method *pIME = nullptr;
};

OpenVRSession &GetVR()
{
    static OpenVRSession s_Global;
    return s_Global;
}

bool vr_init(int argc, char **argv)
{
    vr::EVRInitError error = vr::VRInitError_None;
    VR_Init(&error, vr::VRApplication_Background);

    if ( error != vr::VRInitError_None )
    {
        openvr_log.errorf("Unable to init VR runtime: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription( error ));
        return false;
    }

	// Reset getopt() state
	optind = 1;

	int o;
	int opt_index = -1;
	while ((o = getopt_long(argc, argv, gamescope_optstring, gamescope_options, &opt_index)) != -1)
	{
		const char *opt_name;
		switch (o) {
			case 0: // long options without a short option
				opt_name = gamescope_options[opt_index].name;
				if (strcmp(opt_name, "vr-overlay-key") == 0) {
					GetVR().pchOverlayKey = optarg;
				} else if (strcmp(opt_name, "vr-overlay-explicit-name") == 0) {
				    GetVR().pchOverlayName = optarg;
                    GetVR().bExplicitOverlayName = true;
                } else if (strcmp(opt_name, "vr-overlay-default-name") == 0) {
				    GetVR().pchOverlayName = optarg;
                } else if (strcmp(opt_name, "vr-overlay-icon") == 0) {
				    GetVR().pchOverlayIcon = optarg;
                } else if (strcmp(opt_name, "vr-overlay-show-immediately") == 0) {
				    GetVR().bNudgeToVisible = true;
                } else if (strcmp(opt_name, "vr-overlay-enable-control-bar") == 0) {
				    GetVR().bEnableControlBar = true;
                } else if (strcmp(opt_name, "vr-overlay-enable-control-bar-keyboard") == 0) {
				    GetVR().bEnableControlBarKeyboard = true;
                } else if (strcmp(opt_name, "vr-overlay-enable-control-bar-close") == 0) {
				    GetVR().bEnableControlBarClose = true;
                } else if (strcmp(opt_name, "vr-overlay-modal") == 0) {
				    GetVR().bModal = true;
                } else if (strcmp(opt_name, "vr-overlay-physical-width") == 0) {
				    GetVR().flPhysicalWidth = atof( optarg );
                    if ( GetVR().flPhysicalWidth <= 0.0f )
                        GetVR().flPhysicalWidth = 2.0f;
                } else if (strcmp(opt_name, "vr-overlay-physical-curvature") == 0) {
				    GetVR().flPhysicalCurvature = atof( optarg );
                } else if (strcmp(opt_name, "vr-overlay-physical-pre-curve-pitch") == 0) {
				    GetVR().flPhysicalPreCurvePitch = atof( optarg );
                }
				break;
			case '?':
				assert(false); // unreachable
		}
	}

    if (!GetVR().pchOverlayKey)
        GetVR().pchOverlayKey = wlserver_get_wl_display_name();

    if (!GetVR().pchOverlayName)
        GetVR().pchOverlayName = "Gamescope";

    return true;
}

// Not in public headers yet.
namespace vr
{
    const VROverlayFlags VROverlayFlags_EnableControlBar = (VROverlayFlags)(1 << 23);
    const VROverlayFlags VROverlayFlags_EnableControlBarKeyboard = (VROverlayFlags)(1 << 24);
    const VROverlayFlags VROverlayFlags_EnableControlBarClose = (VROverlayFlags)(1 << 25);
}

bool vrsession_init()
{
    // Setup the overlay.

    if ( !vr::VROverlay() )
    {
        openvr_log.errorf("SteamVR runtime version mismatch!\n");
        return false;
    }

    vr::VROverlay()->CreateDashboardOverlay(
        GetVR().pchOverlayKey,
        GetVR().pchOverlayName,
        &GetVR().hOverlay, &GetVR().hOverlayThumbnail );

    vr::VROverlay()->SetOverlayInputMethod( GetVR().hOverlay, vr::VROverlayInputMethod_Mouse );

    vr::HmdVector2_t vMouseScale = { { (float)g_nOutputWidth, (float)g_nOutputHeight } };
    vr::VROverlay()->SetOverlayMouseScale( GetVR().hOverlay, &vMouseScale );

    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_IgnoreTextureAlpha,		true );
    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_EnableControlBar,			GetVR().bEnableControlBar );
    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_EnableControlBarKeyboard,	GetVR().bEnableControlBarKeyboard );
    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_EnableControlBarClose,	GetVR().bEnableControlBarClose );
    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_WantsModalBehavior,	    GetVR().bModal );
    vrsession_update_touch_mode();

    vr::VROverlay()->SetOverlayWidthInMeters( GetVR().hOverlay,  GetVR().flPhysicalWidth );
    vr::VROverlay()->SetOverlayCurvature	( GetVR().hOverlay,  GetVR().flPhysicalCurvature );
    vr::VROverlay()->SetOverlayPreCurvePitch( GetVR().hOverlay,  GetVR().flPhysicalPreCurvePitch );

    if ( GetVR().pchOverlayIcon )
    {
        vr::EVROverlayError err = vr::VROverlay()->SetOverlayFromFile( GetVR().hOverlayThumbnail, GetVR().pchOverlayIcon );
        if( err != vr::VROverlayError_None )
        {
            openvr_log.errorf( "Unable to set thumbnail to %s: %s\n", GetVR().pchOverlayIcon, vr::VROverlay()->GetOverlayErrorNameFromEnum( err ) );
        }
    }

    // Setup misc. stuff

    g_nOutputRefresh = (int) vr::VRSystem()->GetFloatTrackedDeviceProperty( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float );

    std::thread input_thread_vrinput( vrsession_input_thread );
    input_thread_vrinput.detach();

    return true;
}

bool vrsession_visible()
{
    return vr::VROverlay()->IsOverlayVisible( GetVR().hOverlay ) || GetVR().bNudgeToVisible;
}

void vrsession_present( vr::VRVulkanTextureData_t *pTextureData )
{
    vr::Texture_t texture = { pTextureData, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
    vr::VROverlay()->SetOverlayTexture( GetVR().hOverlay, &texture );
    if ( GetVR().bNudgeToVisible )
    {
        vr::VROverlay()->ShowDashboard( GetVR().pchOverlayKey );
        GetVR().bNudgeToVisible = false;
    }
}

static void vector_append_unique_str( std::vector<const char *>& exts, const char *str )
{
    for ( auto &c_str : exts )
    {
        if ( !strcmp( c_str, str ) )
            return;
    }

    exts.push_back( str );
}

void vrsession_append_instance_exts( std::vector<const char *>& exts )
{
    static std::vector<std::string> s_exts;
    GetVulkanInstanceExtensionsRequired( s_exts );

    for (const auto &str : s_exts)
        vector_append_unique_str( exts, str.c_str() );
}

void vrsession_append_device_exts( VkPhysicalDevice physDev, std::vector<const char *>& exts )
{
    static std::vector<std::string> s_exts;
    GetVulkanDeviceExtensionsRequired( physDev, s_exts );

    for (const auto &str : s_exts)
        vector_append_unique_str( exts, str.c_str() );
}

bool vrsession_framesync( uint32_t timeoutMS )
{
    return vr::VROverlay()->WaitFrameSync( timeoutMS ) != vr::VROverlayError_None;
}

/*
static int VRButtonToWLButton( vr::EVRMouseButton mb )
{
    switch( mb )
    {
        default:
        case vr::VRMouseButton_Left:
            return BTN_LEFT;
        case vr::VRMouseButton_Right:
            return BTN_RIGHT;
        case vr::VRMouseButton_Middle:
            return BTN_MIDDLE;
    }
}
*/

bool vrsession_ime_init()
{
    GetVR().pIME = create_local_ime();
    return true;
}

static void vrsession_input_thread()
{
    pthread_setname_np( pthread_self(), "gamescope-vrinp" );

    // Josh: PollNextOverlayEvent sucks.
    // I want WaitNextOverlayEvent (like SDL_WaitEvent) so this doesn't have to spin and sleep.
    while (true)
    {
        vr::VREvent_t vrEvent;
        while( vr::VROverlay()->PollNextOverlayEvent( GetVR().hOverlay, &vrEvent, sizeof( vrEvent ) ) )
        {
            uint32_t timestamp = vrEvent.eventAgeSeconds * 1'000'000;

            switch( vrEvent.eventType )
            {
                case vr::VREvent_OverlayClosed:
                case vr::VREvent_Quit:
                    g_bRun = false;
                    nudge_steamcompmgr();
                    break;

                case vr::VREvent_KeyboardCharInput:
                {
                    if (GetVR().pIME)
                    {
                        type_text(GetVR().pIME, vrEvent.data.keyboard.cNewInput);
                    }
                    break;
                }

                case vr::VREvent_MouseMove:
                {
                    float x = vrEvent.data.mouse.x;
                    float y = g_nOutputHeight - vrEvent.data.mouse.y;

                    x /= (float)g_nOutputWidth;
                    y /= (float)g_nOutputHeight;

                    wlserver_lock();
                    wlserver_touchmotion( x, y, 0, timestamp );
                    wlserver_unlock();
                    break;
                }
                case vr::VREvent_MouseButtonUp:
                case vr::VREvent_MouseButtonDown:
                {
                    float x = vrEvent.data.mouse.x;
                    float y = g_nOutputHeight - vrEvent.data.mouse.y;

                    x /= (float)g_nOutputWidth;
                    y /= (float)g_nOutputHeight;

                    wlserver_lock();
                    if ( vrEvent.eventType == vr::VREvent_MouseButtonDown )
                        wlserver_touchdown( x, y, 0, timestamp );
                    else
                        wlserver_touchup( 0, timestamp );
                    wlserver_unlock();
                    break;
                }
            }
        }
        sleep_for_nanos(2'000'000);
    }
}

void vrsession_update_touch_mode()
{
    const bool bHideLaserIntersection = g_nTouchClickMode != WLSERVER_TOUCH_CLICK_PASSTHROUGH;
    vr::VROverlay()->SetOverlayFlag( GetVR().hOverlay, vr::VROverlayFlags_HideLaserIntersection, bHideLaserIntersection );
}

void vrsession_title( const char *title )
{
    if ( !GetVR().bExplicitOverlayName )
    {
        vr::VROverlay()->SetOverlayName( GetVR().hOverlay, (title && *title) ? title : GetVR().pchOverlayName );
    }
}

///////////////////////////////////////////////
// Josh:
// GetVulkanInstanceExtensionsRequired and GetVulkanDeviceExtensionsRequired return *space separated* exts :(
// I am too lazy to write that myself.
// This is stolen verbatim from hellovr_vulkan with the .clear removed.
// If it is broken, blame the samples.

static bool GetVulkanInstanceExtensionsRequired( std::vector< std::string > &outInstanceExtensionList )
{
    if ( !vr::VRCompositor() )
    {
        return false;
    }

    uint32_t nBufferSize = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired( nullptr, 0 );
    if ( nBufferSize > 0 )
    {
        // Allocate memory for the space separated list and query for it
        char *pExtensionStr = new char[ nBufferSize ];
        pExtensionStr[0] = 0;
        vr::VRCompositor()->GetVulkanInstanceExtensionsRequired( pExtensionStr, nBufferSize );

        // Break up the space separated list into entries on the CUtlStringList
        std::string curExtStr;
        uint32_t nIndex = 0;
        while ( pExtensionStr[ nIndex ] != 0 && ( nIndex < nBufferSize ) )
        {
            if ( pExtensionStr[ nIndex ] == ' ' )
            {
                outInstanceExtensionList.push_back( curExtStr );
                curExtStr.clear();
            }
            else
            {
                curExtStr += pExtensionStr[ nIndex ];
            }
            nIndex++;
        }
        if ( curExtStr.size() > 0 )
        {
            outInstanceExtensionList.push_back( curExtStr );
        }

        delete [] pExtensionStr;
    }

    return true;
}

static bool GetVulkanDeviceExtensionsRequired( VkPhysicalDevice pPhysicalDevice, std::vector< std::string > &outDeviceExtensionList )
{
    if ( !vr::VRCompositor() )
    {
        return false;
    }

    uint32_t nBufferSize = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired( ( VkPhysicalDevice_T * ) pPhysicalDevice, nullptr, 0 );
    if ( nBufferSize > 0 )
    {
        // Allocate memory for the space separated list and query for it
        char *pExtensionStr = new char[ nBufferSize ];
        pExtensionStr[0] = 0;
        vr::VRCompositor()->GetVulkanDeviceExtensionsRequired( ( VkPhysicalDevice_T * ) pPhysicalDevice, pExtensionStr, nBufferSize );

        // Break up the space separated list into entries on the CUtlStringList
        std::string curExtStr;
        uint32_t nIndex = 0;
        while ( pExtensionStr[ nIndex ] != 0 && ( nIndex < nBufferSize ) )
        {
            if ( pExtensionStr[ nIndex ] == ' ' )
            {
                outDeviceExtensionList.push_back( curExtStr );
                curExtStr.clear();
            }
            else
            {
                curExtStr += pExtensionStr[ nIndex ];
            }
            nIndex++;
        }
        if ( curExtStr.size() > 0 )
        {
            outDeviceExtensionList.push_back( curExtStr );
        }

        delete [] pExtensionStr;
    }

    return true;
}
