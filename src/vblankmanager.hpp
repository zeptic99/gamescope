// Try to figure out when vblank is and notify steamcompmgr to render some time before it

int vblank_init( void );

void vblank_mark_possible_vblank( uint64_t nanos );

extern std::atomic<uint64_t> g_uVblankDrawTimeNS;

const unsigned int g_uDefaultVBlankRedZone = 2'000'000;
const unsigned int g_uDefaultVBlankRateOfDecayPercentage = 93;

extern uint64_t g_uVblankDrawBufferRedZoneNS;
extern uint64_t g_uVBlankRateOfDecayPercentage;

void fpslimit_init( void );

void fpslimit_mark_frame( void );

bool fpslimit_use_frame_callbacks_for_focus_window( int nTargetFPS, int nVBlankCount );

void fpslimit_set_target( int nTargetFPS );
