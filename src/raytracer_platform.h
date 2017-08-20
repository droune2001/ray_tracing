#ifndef _HANDMADE_HERO_PLATFORM_H_
#define _HANDMADE_HERO_PLATFORM_H_

/*
HANDMADE_INTERNAL:
0 - Build for public release
1 - Build for developer only

HANDMADE_SLOW:
0 - fast
1 - slow
*/

// WHAT WE NEED FOR THE PLATFORM LAYER, BARE MINIMUM,
// AND USABLE FROM PURE C.

#ifdef __cplusplus
extern "C" {
#endif
    
    //
    // NOTE: Compilers
    //
#if !defined( COMPILER_MSVC )
# define COMPILER_MSVC 0
#endif
    
#if !defined( COMPILER_LLVM )
# define COMPILER_LLVM 0
#endif
    
#if !COMPILER_MSVC && !COMPILER_LLVM
# if _MSC_VER
#  undef COMPILER_MSVC
#  define COMPILER_MSVC 1
# else
#  undef COMPILER_LLVM
#  define COMPILER_LLVM 1
# endif
#endif
    
#if COMPILER_MSVC
# include <intrin.h>
#endif
    
    //
    // NOTE: Types
    //
    
#include <stdint.h>
#include <stddef.h>
    
    typedef int8_t  int8;
    typedef int16_t int16;
    typedef int32_t int32;
    typedef int64_t int64;
    typedef int32 bool32;
    
    typedef uint8_t  uint8;
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint64_t uint64;
    
    typedef size_t memory_index;
    
    typedef float real32;
    typedef double real64;
    
    
#define internal static
#define local_persist static
#define global_variable static
    
#define Pi32 3.1415926535f
    
#if HANDMADE_SLOW
# define Assert(Expression) if(!(Expression)){*(int *)0 = 0;}
#else
# define Assert(Expression)
#endif
    
#define InvalidCodePath Assert(!"InvalidCodePath");
    
    inline uint32 SafeTruncateSize32( uint64 Value )
    {
        Assert( Value <= 0xFFFFFFFF );
        uint32 Result = (uint32)Value;
        return Result;
    }
    
#define Kilobytes(val) ((val)*1024LL)
#define Megabytes(val) (Kilobytes(val)*1024LL)
#define Gigabytes(val) (Megabytes(val)*1024LL)
#define Terabytes(val) (Gigabytes(val)*1024LL)
    
#define ArrayCount(tab) (sizeof(tab) / sizeof((tab)[0]))
    
    // * passed through each execution chain in the game
    // * when you use operating system services, you are going
    //   to need one of these thread_context (not on windows).
    // * passed to platform calls.
    // * Because not all platforms give you good information on the current thread
    //   when doing multi-threading.
    typedef struct thread_context
    {
        int Placeholder;
    } thread_context;
    
    // -----------------------------------------------------
    // Services the platform layer provides to the game.
#if HANDMADE_INTERNAL
    /*
    These are not for doing anything in the shipping game - they are
    blocking and the write doesn't protect against lost data!
    */
    typedef struct debug_read_file_result
    {
        uint32 ContentsSize;
        void *Contents;
    } debug_read_file_result;
    
#define DEBUG_PLATFORM_FREE_FILE_MEMORY( name ) void name( thread_context *Thread, void *Memory )
    typedef DEBUG_PLATFORM_FREE_FILE_MEMORY( debug_platform_free_file_memory );
    
#define DEBUG_PLATFORM_READ_ENTIRE_FILE( name ) debug_read_file_result name( thread_context *Thread, char *Filename )
    typedef DEBUG_PLATFORM_READ_ENTIRE_FILE( debug_platform_read_entire_file );
    
#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE( name ) bool32 name( thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory )
    typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE( debug_platform_write_entire_file );
    
#endif
    
    // -----------------------------------------------------
    // Services that the game provides to the plafrom layer.
    
    typedef struct game_offscreen_buffer
    {
        void *Memory;
        int Width;
        int Height;
        int Pitch;
        int BytesPerPixel;
    } game_offscreen_buffer;
    
    typedef struct game_sound_output_buffer
    {
        int SamplesPerSecond;
        int SampleCount;
        int16 *Samples;
    } game_sound_output_buffer;
    
    // can rebuild the history with just these 2 data
    typedef struct game_button_state
    {
        int HalfTransitionCount;
        bool32 EndedDown;
    } game_button_state;
    
    typedef struct game_controller_input
    {
        bool32 IsConnected;
        bool32 IsAnalog;
        real32 StickAverageX;
        real32 StickAverageY;
        
        union
        {
            game_button_state Buttons[12];
            struct
            {
                game_button_state MoveUp;
                game_button_state MoveDown;
                game_button_state MoveLeft;
                game_button_state MoveRight;
                
                game_button_state ActionUp;
                game_button_state ActionDown;
                game_button_state ActionLeft;
                game_button_state ActionRight;
                
                game_button_state LeftShoulder;
                game_button_state RightShoulder;
                
                game_button_state Start;
                game_button_state Back;
                
                // NOTE(nfauvet): all buttons must be added above this one
                game_button_state Terminator;
                
            };
        };
    } game_controller_input;
    
    typedef struct game_input
    {
        // for debugging purposes
        game_button_state MouseButtons[5];
        int32 MouseX, MouseY, MouseZ;
        
        real32 dtForFrame;
        
        game_controller_input Controllers[5]; // 4 manettes et 1 keyboard
    } game_input;
    
    typedef struct game_memory
    {
        bool32 IsInitialized;
        
        uint64 PermanentStorageSize;
        void *PermanentStorage; // REQUIRED to be cleared to zero at startup (every platform have to do it).
        
        uint64 TransientStorageSize;
        void *TransientStorage; // scratch buffer
        
        debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
        debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
        debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
    } game_memory;
    
    // accessor just to add an assert.
    inline game_controller_input *GetController( game_input *Input, int ControllerIndex )
    {
        Assert( ControllerIndex < ArrayCount( Input->Controllers ) );
        game_controller_input *Result = &Input->Controllers[ControllerIndex];
        return Result;
    }
    
#define GAME_UPDATE_AND_RENDER( name ) void name( thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer )
    typedef GAME_UPDATE_AND_RENDER( game_update_and_render );
    
#define GAME_GET_SOUND_SAMPLES( name ) void name( thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer )
    typedef GAME_GET_SOUND_SAMPLES( game_get_sound_samples );
    
#ifdef __cplusplus
}
#endif

#endif //_HANDMADE_HERO_PLATFORM_H_
