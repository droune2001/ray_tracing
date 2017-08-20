/*
 TODO(nfauvet):
 
 * Fullscreen support
 * WM_SETCURSOR (cursor visibility)
 
 * Saved game locations
 * Getting a handle to our own executable file
 * Asset loading path
 * Threading (launch a thread)
 * Raw Input (multiple keyboards)
 * Sleep/timeBeginPeriod
 * ClipCursor() for multimonitor support
 * QueryCancelAutoplay
 * WM_ACTIVATEAPP (for when we are not the active application)
 * Blit seed improvement (BitBlt)
 * Hardware acceleration (OpenGL/DirectX)
 * GetKeyboardLayout (for French keyboards, international WSAD support)
*/

#include "raytracer_platform.h"

#include <windows.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

#include "win32_raytracer.h"

global_variable bool32 GlobalRunning; // no need to init to 0 or false, default behavior, saves time.
global_variable bool32 GlobalPause = false;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER pGlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof( GlobalWindowPosition ) };

internal void
CatStrings( size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest )
{
    for ( size_t Index = 0; Index < SourceACount; ++Index ) {
        *Dest++ = *SourceA++;
    }
    for ( size_t Index = 0; Index < SourceBCount; ++Index ) {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal void
Win32GetEXEFileName( win32_state *State )
{
    // NOTE(nfauvet): Never use MAX_PATH in code that is user-facing, because it
    // can be dnagerous and lead to bad results.
    DWORD SizeOfFilename = ::GetModuleFileNameA( 0, State->EXEFileName, sizeof( State->EXEFileName ) );
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for ( char *Scan = State->EXEFileName; *Scan; ++Scan ) {
        if ( *Scan == '\\' ) {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

internal int
StringLength( char *String )
{
    int Length = 0;
    while ( *String++ ) ++Length;
    return Length;
}

internal void
Win32BuildEXEPathFileName( win32_state *State, char *FileName, int DestCount, char *Dest )
{
    CatStrings( State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLength( FileName ), FileName,
               DestCount, Dest );
}


// IO
DEBUG_PLATFORM_FREE_FILE_MEMORY( DEBUGPlatformFreeFileMemory )
{
    if ( Memory ) {
        ::VirtualFree( Memory, 0, MEM_RELEASE );
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE( DEBUGPlatformReadEntireFile )
{
    debug_read_file_result Result = {};
    HANDLE FileHandle = ::CreateFileA( Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    if ( FileHandle != INVALID_HANDLE_VALUE ) 
    {
        LARGE_INTEGER FileSize;
        if ( ::GetFileSizeEx( FileHandle, &FileSize ) )	
        {
            Result.Contents = ::VirtualAlloc( 0, FileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            if ( Result.Contents ) 
            {
                // NOTE: FileSize.QuadPart is 64bits, but ReadFile takes a 32bits. 
                // We cant read a file larger than 4Gb with ReadFile. Check it.
                uint32 FileSize32 = SafeTruncateSize32( FileSize.QuadPart );
                DWORD BytesRead;
                if ( ::ReadFile( FileHandle, Result.Contents, FileSize32, &BytesRead, 0 ) 
                    && ( BytesRead == FileSize32 ) ) 
                {
                    // SUCCESS
                    Result.ContentsSize = FileSize32;
                } 
                else 
                {
                    // FAIL - Free
                    DEBUGPlatformFreeFileMemory( Thread, Result.Contents );
                    Result.Contents = 0;
                    Result.ContentsSize = 0;
                }
            } 
            else 
            {
                // FAIL
            }
        } 
        else 
        {
            // FAIL
        }
        
        if ( !::CloseHandle( FileHandle ) ) 
        {
            // FAIL
        }
    }
    
    return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE( DEBUGPlatformWriteEntireFile )
{
    bool32 Result = 0;
    
    HANDLE FileHandle = ::CreateFileA( Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0 );
    if ( FileHandle != INVALID_HANDLE_VALUE ) 
    {
        DWORD BytesWritten;
        if ( ::WriteFile( FileHandle, Memory, MemorySize, &BytesWritten, 0 ) ) 
        {
            // SUCCESS
            Result = ( BytesWritten == MemorySize );
        } 
        else 
        {
            // FAIL
        }
        
        if ( !::CloseHandle( FileHandle ) ) 
        {
            // FAIL
        }
    }
    
    return Result;
}

// Game DLL

inline FILETIME
Win32GetLastWriteTime( char *FileName )
{
    FILETIME Result = {};
    
#if 0
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = ::FindFirstFileA( Filename, &FindData );
    if ( FindHandle != INVALID_HANDLE_VALUE ) 
    {
        Result = FindData.ftLastWriteTime;
        ::FindClose( FindHandle );
    }
#else	
    WIN32_FILE_ATTRIBUTE_DATA FileInformation;
    if ( ::GetFileAttributesExA( FileName, GetFileExInfoStandard, &FileInformation ) ) 
    {
        Result = FileInformation.ftLastWriteTime;
    }
#endif
    return Result;
}

internal win32_game_code
Win32LoadGameCode( char *SourceDLLName, char *TempDLLName, char *LockFileName )
{
    win32_game_code Result = {};
    
    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if ( !::GetFileAttributesExA( LockFileName, GetFileExInfoStandard, &Ignored ) )
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime( SourceDLLName );
        ::CopyFile( SourceDLLName, TempDLLName, FALSE );
        Result.GameCodeDLL = ::LoadLibraryA( TempDLLName );
        if ( Result.GameCodeDLL ) 
        {
            Result.UpdateAndRender = ( game_update_and_render * ) ::GetProcAddress( Result.GameCodeDLL, "GameUpdateAndRender" );
            Result.GetSoundSamples = ( game_get_sound_samples * ) ::GetProcAddress( Result.GameCodeDLL, "GameGetSoundSamples" );
            Result.IsValid = ( Result.UpdateAndRender && Result.GetSoundSamples );
        }
    }
    
    if ( !Result.IsValid ) 
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }
    
    return Result;
}

internal void
Win32UnloadGameCode( win32_game_code *GameCode )
{
    Assert( GameCode );
    
    if ( GameCode->GameCodeDLL )
    {
        ::FreeLibrary( GameCode->GameCodeDLL );
        GameCode->GameCodeDLL = 0;
    }
    
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

// XInput

#define X_INPUT_GET_STATE( name ) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_STATE* pState )
typedef X_INPUT_GET_STATE( x_input_get_state );
X_INPUT_GET_STATE( XInputGetStateStub )
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

#define X_INPUT_SET_STATE( name ) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_VIBRATION* pVibration )
typedef X_INPUT_SET_STATE( x_input_set_state );
X_INPUT_SET_STATE( XInputSetStateStub )
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput( void )
{
    HMODULE hXInputLibrary = ::LoadLibraryA( "xinput1_4.dll" );
    if ( !hXInputLibrary ) 
    {
        // TODO: log
        hXInputLibrary = ::LoadLibraryA( "xinput1_3.dll" );
        if ( !hXInputLibrary ) 
        {
            // TODO: log
            hXInputLibrary = ::LoadLibraryA( "xinput9_1_0.dll" );
        }
    }
    
    if ( hXInputLibrary ) 
    {
        XInputGetState = (x_input_get_state *) ::GetProcAddress( hXInputLibrary, "XInputGetState" );
        XInputSetState = (x_input_set_state *) ::GetProcAddress( hXInputLibrary, "XInputSetState" );
    } else {
        // TODO: log
    }
}

// DirectSound

#define DIRECT_SOUND_CREATE( name ) HRESULT WINAPI name( LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter )
typedef DIRECT_SOUND_CREATE( d_sound_create );

internal void Win32InitDSound( HWND hWnd, int32 iSamplesPerSecond, int32 iBufferSize )
{
    // load lib
    HMODULE hDSoundLibrary = ::LoadLibraryA( "dsound.dll" ); // dsound3d.dll ??
    if ( hDSoundLibrary ) 
    {
        // get a directsound object
        d_sound_create *DirectSoundCreate = ( d_sound_create* ) ::GetProcAddress( hDSoundLibrary, "DirectSoundCreate" );
        
        LPDIRECTSOUND pDirectSound;
        if ( DirectSoundCreate && SUCCEEDED( DirectSoundCreate( 0, &pDirectSound, 0 ) ) ) 
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = iSamplesPerSecond;
            WaveFormat.wBitsPerSample = 16; // 16 bits audio
            WaveFormat.nBlockAlign = ( WaveFormat.nChannels * WaveFormat.wBitsPerSample ) / 8; // 2 channels, 16bits each, interleaved [RIGHT LEFT] [RIGHT LEFT], divided by bits_in_a_byte
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;
            
            if ( SUCCEEDED( pDirectSound->SetCooperativeLevel( hWnd, DSSCL_PRIORITY ) ) ) 
            {
                // create a primary buffer
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof( BufferDescription );
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER; // DSBCAPS_GLOBALFOCUS ?
                
                LPDIRECTSOUNDBUFFER pPrimaryBuffer;
                if ( SUCCEEDED( pDirectSound->CreateSoundBuffer( &BufferDescription, &pPrimaryBuffer, 0 ) ) ) 
                {
                    if ( SUCCEEDED( pPrimaryBuffer->SetFormat( &WaveFormat ) ) ) 
                    {
                        OutputDebugStringA( "SUCCESS creating the primary sound buffer.\n" );
                    } 
                    else 
                    {
                        // TODO: log
                        OutputDebugStringA( "Failed SetFormat on PrimaryBuffer\n" );
                    }
                } 
                else 
                {
                    // TODO: log
                    OutputDebugStringA( "Failed Create Primary Buffer\n" );
                }
            } 
            else 
            {
                // TODO: log
                OutputDebugStringA( "Failed SetCooperativeLevel\n" );
            }
            
            // even if cooperative and primary buffer failed, we can create the real buffer.
            // create a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof( BufferDescription );
            BufferDescription.dwBufferBytes = iBufferSize;
            BufferDescription.dwFlags = 0; // DSBCAPS_GETCURRENTPOSITION2;
            BufferDescription.lpwfxFormat = &WaveFormat;
            
            if ( SUCCEEDED( pDirectSound->CreateSoundBuffer( &BufferDescription, &pGlobalSecondaryBuffer, 0 ) ) ) 
            {
                OutputDebugStringA("SUCCESS create sound buffer\n");
            } 
            else 
            {
                // TODO: log
                OutputDebugStringA( "Failed Create Secondary Buffer\n" );
            }
        } 
        else 
        {
            // TODO: log
            OutputDebugStringA( "Failed GetProcAddress DirectSoundCreate and Call\n" );
        }
    } 
    else 
    {
        // TODO: log
        OutputDebugStringA( "Failed LoadLibrary dsound.dll\n" );
    }
}

internal win32_window_dimension Win32GetWindowDimension( HWND hWnd )
{
    win32_window_dimension d;
    
    RECT r;
    ::GetClientRect( hWnd, &r );
    d.width = r.right - r.left;
    d.height = r.bottom - r.top;
    
    return d;
}

internal void Win32ResizeDIBSection( win32_offscreen_buffer *Buffer, int width, int height )
{
    if ( Buffer->Memory ) 
    {
        ::VirtualFree( Buffer->Memory, 0, MEM_RELEASE );
    }
    
    Buffer->Width = width;
    Buffer->Height = height;
    Buffer->BytesPerPixel = 4; 
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
    
    Buffer->Info.bmiHeader.biSize = sizeof( Buffer->Info.bmiHeader );
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32; // to align with 32bits
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = Buffer->BytesPerPixel * ( Buffer->Width * Buffer->Height );
    Buffer->Memory = ::VirtualAlloc( 0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
    
    // clear to black?
}

internal void Win32CopyBufferToWindow( HDC dc, int client_width, int client_height, 
                                      win32_offscreen_buffer *Buffer )
{
    if ( ( client_width >= ( Buffer->Width * 2 ) ) && 
        ( client_height >= ( Buffer->Height * 2 ) ) )
    {
        ::StretchDIBits( dc,
                        0, 0, 2 * Buffer->Width, 2 * Buffer->Height, // FROM
                        0, 0, Buffer->Width, Buffer->Height, // TO
                        Buffer->Memory,
                        &Buffer->Info,
                        DIB_RGB_COLORS, // or DIB_PAL_COLORS for palette
                        SRCCOPY );
    }
    else
    {
        int OffsetX = 10;
        int OffsetY = 10;
        
        // Clear the back in black
        ::PatBlt( dc, 0, 0, client_width, OffsetY, BLACKNESS ); // TOP
        ::PatBlt( dc, 0, OffsetY + Buffer->Height, client_width, client_width, BLACKNESS ); // BOTTOM
        ::PatBlt( dc, 0, 0, OffsetX, client_height, BLACKNESS ); // LEFT
        ::PatBlt( dc, OffsetX + Buffer->Width, 0, client_width, client_height, BLACKNESS ); // RIGHT
        
        
        // Don't stretch. Want to see each and every pixel while learning
        // how to do a renderer. We could introduce artifacts with stretching.
        ::StretchDIBits( dc,
                        OffsetX, OffsetY, Buffer->Width, Buffer->Height, // FROM
                        0, 0, Buffer->Width, Buffer->Height, // TO
                        Buffer->Memory,
                        &Buffer->Info,
                        DIB_RGB_COLORS, // or DIB_PAL_COLORS for palette
                        SRCCOPY );
    }
}

LRESULT CALLBACK Win32MainWindowCallback(	HWND   hwnd,
                                         UINT   uMsg,
                                         WPARAM wParam,
                                         LPARAM lParam )
{
    LRESULT Result = 0; // 0 is most of the time when we handled the message
    
    switch ( uMsg )
    {
        case WM_SETCURSOR:
        {
            if ( DEBUGGlobalShowCursor )
            {
                // let windows show the cursor.
                Result = DefWindowProcA( hwnd, uMsg, wParam, lParam );
            }
            else
            {
                ::SetCursor( 0 );
            }
        } break;
        
        case WM_SIZE:
        {
        } break;
        
        case WM_DESTROY:
        {
            // TODO: catch this and try to recreate the window.
            GlobalRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            // TODO: send message to the user
            GlobalRunning = false;
        } break;
        
        case WM_ACTIVATEAPP:
        {
#if 0
            if ( wParam == TRUE ) 
            {
                ::SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 255, LWA_ALPHA );
            }
            else 
            {
                // when the window goes in background (but in TOPMOST), it will be translucent.
                ::SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 64, LWA_ALPHA );
            }
#endif
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert( !"Keyboard Input came in through a non-dispath message!" );
        } break; 
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC dc = ::BeginPaint( hwnd, &Paint );
            {
                win32_window_dimension d = Win32GetWindowDimension( hwnd );
                Win32CopyBufferToWindow( dc, d.width, d.height, &GlobalBackBuffer );
            }
            ::EndPaint( hwnd, &Paint );
        } break;
        
        default:
        {
            Result = DefWindowProcA( hwnd, uMsg, wParam, lParam );
        } break;
    }
    
    return Result;
}

internal void
Win32ClearBuffer( win32_sound_output *SoundOutput )
{
    VOID *pRegion1;
    DWORD iRegion1Size;
    VOID *pRegion2;
    DWORD iRegion2Size;
    if ( SUCCEEDED(pGlobalSecondaryBuffer->Lock( 0, SoundOutput->SecondaryBufferSize,
                                                &pRegion1, &iRegion1Size,
                                                &pRegion2, &iRegion2Size, 0 )))
    {
        uint8 *DestSample = (uint8 *)pRegion1;
        for ( DWORD ByteIndex = 0; ByteIndex < iRegion1Size; ++ByteIndex) 
        {
            *DestSample++ = 0;
        }
        
        DestSample = (uint8 *)pRegion2;
        for (DWORD ByteIndex = 0; ByteIndex < iRegion2Size; ++ByteIndex) 
        {
            *DestSample++ = 0;
        }
        
        pGlobalSecondaryBuffer->Unlock(pRegion1, iRegion1Size, pRegion2, iRegion2Size);
    }
}

internal void Win32FillSoundBuffer( 
win32_sound_output *SoundOutput, 
DWORD ByteToLock, DWORD BytesToWrite,
game_sound_output_buffer *pSourceBuffer )
{
    VOID *pRegion1;
    DWORD iRegion1Size;
    VOID *pRegion2;
    DWORD iRegion2Size;
    if ( SUCCEEDED( pGlobalSecondaryBuffer->Lock( ByteToLock, BytesToWrite,
                                                 &pRegion1, &iRegion1Size, 
                                                 &pRegion2, &iRegion2Size, 0 ))) 
    {
        int16 *DestSample = (int16*)pRegion1;
        int16 *SourceSample = pSourceBuffer->Samples;
        DWORD iRegion1SampleCount = iRegion1Size / SoundOutput->BytesPerSample;
        for ( DWORD SampleIndex = 0; SampleIndex < iRegion1SampleCount; ++SampleIndex ) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        DestSample = (int16*)pRegion2;
        DWORD iRegion2SampleCount = iRegion2Size / SoundOutput->BytesPerSample;
        for ( DWORD SampleIndex = 0; SampleIndex < iRegion2SampleCount; ++SampleIndex ) {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }
        
        pGlobalSecondaryBuffer->Unlock( pRegion1, iRegion1Size, pRegion2, iRegion2Size );
    }
}

internal real32
Win32ProcessXinputStickValue( SHORT StickValue, SHORT DeadZoneThreshold  )
{
    // Normalize the portion out of the deadzone.
    real32 ReturnValue = 0;
    if ( StickValue < -DeadZoneThreshold ) 
    {
        ReturnValue = (real32)( StickValue + DeadZoneThreshold ) / ( 32768.0f - DeadZoneThreshold );
    } 
    else if ( StickValue > DeadZoneThreshold ) 
    {
        ReturnValue = (real32)( StickValue - DeadZoneThreshold ) / ( 32767.0f - DeadZoneThreshold );
    }
    
    return ReturnValue;
}

internal void 
Win32ProcessXInputDigitalButton( DWORD XInputButtonState,
                                game_button_state *OldState,
                                DWORD ButtonBit,
                                game_button_state *NewState )
{
    NewState->EndedDown = ( ( XInputButtonState & ButtonBit ) == ButtonBit );
    NewState->HalfTransitionCount = ( OldState->EndedDown != NewState->EndedDown ) ? 1 : 0;
}

internal void 
Win32ProcessKeyboardMessage( game_button_state *NewState, bool32 bIsDown )
{
    if ( NewState->EndedDown != bIsDown ) {
        NewState->EndedDown = bIsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void
Win32GetInputFileLocation( win32_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest )
{
    //Assert( SlotIndex == 1 ); // we don't use that yet.
    char Temp[64];
    wsprintf( Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state" );
    Win32BuildEXEPathFileName( State, Temp, DestCount, Dest );
}

internal win32_replay_buffer *
Win32GetReplayBuffer( win32_state *State, unsigned int Index )
{
    Assert( Index < ArrayCount( State->ReplayBuffers ) );
    win32_replay_buffer *Result = &State->ReplayBuffers[Index];
    return Result;
}

internal void
Win32BeginRecordingInput( win32_state *State, int InputRecordingIndex )
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer( State, (unsigned int)InputRecordingIndex );
    if ( ReplayBuffer->MemoryBlock )
    {
        State->InputRecordingIndex = InputRecordingIndex;
        State->RecordingHandle = ReplayBuffer->FileHandle;
        
        // gen a separate filename for input recording (separate from state recording).
        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation( State, true, InputRecordingIndex, sizeof( FileName ), FileName );
        State->RecordingHandle = ::CreateFileA( FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0 );
        
        // Store the file sparsely if there are a lot of 0 inside.
        //DWORD Ignored;
        //::DeviceIoControl( State->RecordingHandle, FSCTL_SET_SPARSE, 0, 0, NULL, 0, &Ignored, 0 );
        
        //DWORD BytesToWrite = (DWORD)State->TotalSize;
        //Assert( State->TotalSize == BytesToWrite );
        //DWORD BytesWritten;
        //::WriteFile( State->RecordingHandle, State->GameMemoryBlock, BytesToWrite, &BytesWritten, 0 );
        
        // On va faire avancer le pointeur de fichier pour sauter la partie GameMemory qu'on sauve maintenant
        // dans la RAM. Comme ca on peut toujours ecrire dans le fichier les Input live, APRES le bloc (vide)
        // de GameMemory
#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        ::SetFilePointerEx( State->RecordingHandle, FilePosition, 0, FILE_BEGIN ); // ?
#endif
        ::CopyMemory( ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize );
    }
}

internal void
Win32EndRecordingInput( win32_state *Win32State )
{
    ::CloseHandle( Win32State->RecordingHandle );
    Win32State->InputRecordingIndex = 0;
}

internal void
Win32BeginPlayBackInput( win32_state *State, int InputPlayBackIndex )
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer( State, (unsigned int)InputPlayBackIndex );
    if ( ReplayBuffer->MemoryBlock )
    {
        State->InputPlayingIndex = InputPlayBackIndex;
        
        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation( State, true, InputPlayBackIndex, sizeof( FileName ), FileName );
        State->PlaybackHandle = ::CreateFileA( FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
        
        //DWORD BytesToRead = (DWORD)State->TotalSize;
        //Assert( State->TotalSize == BytesToRead );
        //DWORD BytesRead = 0;
        //::ReadFile( State->PlaybackHandle, State->GameMemoryBlock, BytesToRead, &BytesRead, 0 );
#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;
        ::SetFilePointerEx( State->PlaybackHandle, FilePosition, 0, FILE_BEGIN ); // ?
#endif
        ::CopyMemory( State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize );
    }
}

internal void
Win32EndPlayBackInput( win32_state *Win32State )
{
    ::CloseHandle( Win32State->PlaybackHandle );
    Win32State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput( win32_state *Win32State, game_input *NewInput )
{
    DWORD BytesWritten = 0;
    ::WriteFile( Win32State->RecordingHandle, NewInput, sizeof( *NewInput ), &BytesWritten, 0 );
}

internal void
Win32PlayBackInput( win32_state *Win32State, game_input *NewInput )
{
    DWORD BytesRead = 0;
    if ( ::ReadFile( Win32State->PlaybackHandle, NewInput, sizeof( *NewInput ), &BytesRead, 0 ) )
    {
        if ( BytesRead == 0 ) {
            // if at the end of the stream, stop and restart.
            int PlayingIndex = Win32State->InputPlayingIndex;
            Win32EndPlayBackInput( Win32State );
            Win32BeginPlayBackInput( Win32State, PlayingIndex );
            ::ReadFile( Win32State->PlaybackHandle, NewInput, sizeof( *NewInput ), &BytesRead, 0 );
        }
    }
}

internal void
ToggleFullscreen( HWND Window )
{
    // NOTE: use ChangeDisplaySettings to go into "real" fullscreen mode
    // with the resolution and frequency we want.
    
    
    // This follows Raymond Chen's prescription for fullscreen toggling
    // https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD dwStyle = GetWindowLong( Window, GWL_STYLE );
    if ( dwStyle & WS_OVERLAPPEDWINDOW )
    {
        MONITORINFO MonitorInfo = { sizeof( MonitorInfo ) };
        if ( GetWindowPlacement( Window, &GlobalWindowPosition ) &&
            GetMonitorInfo( MonitorFromWindow( Window, MONITOR_DEFAULTTOPRIMARY ), &MonitorInfo ) )
        {
            SetWindowLong( Window, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW );
            SetWindowPos( Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
        }
    }
    else
    {
        SetWindowLong( Window, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW );
        SetWindowPlacement( Window, &GlobalWindowPosition );
        SetWindowPos( Window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
    }
}

internal void Win32ProcessPendingMessages( win32_state *Win32State, game_controller_input *KeyboardController )
{
    // flush queue
    MSG Message;
    while ( ::PeekMessage( &Message, 0, 0, 0, PM_REMOVE ) ) 
    {
        if ( Message.message == WM_QUIT ) 
        {
            GlobalRunning = false;
        }
        
        switch ( Message.message ) 
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 bWasDown = ( ( Message.lParam & ( 1 << 30 ) ) != 0 ); // previous state bit
                bool32 bIsDown = ( ( Message.lParam & ( 1 << 31 ) ) == 0 );
                bool32 bAltKeyIsDown = ( Message.lParam & ( 1 << 29 ) );
                if ( bIsDown != bWasDown ) 
                {
                    if ( VKCode == 'W' ) { // Z
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveUp, bIsDown );
                    } else if ( VKCode == 'A' ) { // Q
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveLeft, bIsDown );
                    } else if ( VKCode == 'S' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveDown, bIsDown );
                    } else if ( VKCode == 'D' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->MoveRight, bIsDown );
                    } else if ( VKCode == 'Q' ) { // A
                        Win32ProcessKeyboardMessage( &KeyboardController->LeftShoulder, bIsDown );
                    } else if ( VKCode == 'E' ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->RightShoulder, bIsDown );
                    } else if ( VKCode == VK_LEFT ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionLeft, bIsDown );
                    } else if ( VKCode == VK_RIGHT ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionRight, bIsDown );
                    } else if ( VKCode == VK_UP ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionUp, bIsDown );
                    } else if ( VKCode == VK_DOWN ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->ActionDown, bIsDown );
                    } else if ( VKCode == VK_ESCAPE ) {
                        GlobalRunning = false;
                        Win32ProcessKeyboardMessage( &KeyboardController->Back, bIsDown );
                    } else if ( VKCode == VK_SPACE ) {
                        Win32ProcessKeyboardMessage( &KeyboardController->Start, bIsDown );
                    } 
#if HANDMADE_INTERNAL
                    else if ( VKCode == 'P' ) 
                    {
                        if ( bIsDown ) {
                            GlobalPause = !GlobalPause;
                        }
                    } 
                    else if ( VKCode == 'L' ) 
                    {
                        if ( bIsDown ) {
                            if ( Win32State->InputPlayingIndex == 0 ) 
                            {
                                if ( Win32State->InputRecordingIndex == 0 ) 
                                {
                                    Win32BeginRecordingInput( Win32State, 1 );
                                }
                                else 
                                {
                                    Win32EndRecordingInput( Win32State );
                                    Win32BeginPlayBackInput( Win32State, 1 );
                                }
                            } 
                            else 
                            {
                                Win32EndPlayBackInput( Win32State );
                            }
                        }
                    } 
                    //else if ( ) {
                    //}
#endif
                    if ( bIsDown ) 
                    {
                        if ( ( VKCode == VK_F4 ) && bAltKeyIsDown ) 
                        {
                            GlobalRunning = false;
                        }
                        if ( ( VKCode == VK_RETURN ) && bAltKeyIsDown ) 
                        {
                            if ( Message.hwnd )
                            {
                                ToggleFullscreen( Message.hwnd );
                            }
                        }
                    }
                    
                } // if KeyDown
            } break;
            
            default:
            {
                TranslateMessage( &Message );
                DispatchMessage( &Message );
            }
        }
    }
}

inline LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER Result;
    ::QueryPerformanceCounter( &Result );
    return Result;
}

inline real32 
Win32GetSecondsElapsed( LARGE_INTEGER Start, LARGE_INTEGER End )
{
    real32 SecondsElapsedForWork = 
        (real32)( End.QuadPart - Start.QuadPart ) / 
        (real32)GlobalPerfCounterFrequency;
    return SecondsElapsedForWork;
}

#if 0
internal void
Win32DebugDrawVertical( win32_offscreen_buffer *BackBuffer, int X, int Top, int Bottom, uint32 Color )
{
    if ( Top <= 0 ) { Top = 0; }
    if ( Bottom >= BackBuffer->Height ) { Bottom = BackBuffer->Height - 1; }
    if ( (X >= 0) && (X < BackBuffer->Width) ) {
        uint8 *Pixel = (uint8*)BackBuffer->Memory + X * BackBuffer->BytesPerPixel + Top * BackBuffer->Pitch;
        for ( int Y = Top; Y < Bottom; ++Y )
        {
            *((uint32*)Pixel) = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}

internal void
Win32DebugDrawHorizontal( win32_offscreen_buffer *BackBuffer, int Y, int Left, int Right, uint32 Color )
{
    uint8 *Pixel = (uint8*)BackBuffer->Memory +	Left * BackBuffer->BytesPerPixel + Y * BackBuffer->Pitch;
    for ( int X = Left; X < Right; ++X ) 
    {
        *((uint32*)Pixel) = Color;
        Pixel += BackBuffer->BytesPerPixel;
    }
}

internal void
Win32DebugSyncDisplay(	win32_offscreen_buffer *BackBuffer,
                      int LastPlayCursorCount, win32_debug_time_marker *LastPlayCursor,
                      int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 SecondsPerFrame )
{
    int32 PadX = 16;
    int32 PadY = 16;
    //int32 Top = PadY;
    int32 Bottom = BackBuffer->Height - PadY;
    
    real32 SoundSamplesToHPixels = (real32)(BackBuffer->Width - 2 * PadX) / (real32)SoundOutput->SecondaryBufferSize;
    int SoundSampleVPixelSize = (int)((real32)(BackBuffer->Height - 2 * PadY) / (real32)LastPlayCursorCount);
    
    for ( int PlayCursorIndex = 0; PlayCursorIndex < LastPlayCursorCount; ++PlayCursorIndex )
    {
        // remaps sound samples to pixels (minus padding)
        int Top = PadY + PlayCursorIndex * SoundSampleVPixelSize;
        
        int32 FlipPlayCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].FlipPlayCursor * SoundSamplesToHPixels);
        int32 FlipWriteCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].FlipWriteCursor * SoundSamplesToHPixels );
        int32 PlayCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputPlayCursor * SoundSamplesToHPixels);
        int32 WriteCursorX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputWriteCursor * SoundSamplesToHPixels);
        int32 OutputLocationX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputLocation * SoundSamplesToHPixels );
        int32 BytesToWrite = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].OutputByteCount * SoundSamplesToHPixels);
        int32 ExpectedX = PadX + (int)((real32)LastPlayCursor[PlayCursorIndex].ExpectedFlipPlayCursor * SoundSamplesToHPixels);
        
        Win32DebugDrawVertical( BackBuffer, PlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFAA00AA );
        Win32DebugDrawVertical( BackBuffer, WriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00AAAA );
        
        if ( PlayCursorIndex == CurrentMarkerIndex ) {
            
            Top += SoundSampleVPixelSize;
            
            Win32DebugDrawVertical( BackBuffer, PlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFFF00FF );
            Win32DebugDrawVertical( BackBuffer, WriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00FFFF );
            Win32DebugDrawVertical(BackBuffer, ExpectedX, Top, Top + SoundSampleVPixelSize, 0xFFFFFF00); // yellow
            Win32DebugDrawHorizontal(BackBuffer, Top + (SoundSampleVPixelSize / 2), PlayCursorX, WriteCursorX, 0xFF000000 ); // black
            
            Top += SoundSampleVPixelSize;
            
            Win32DebugDrawVertical( BackBuffer, OutputLocationX, Top, Top + SoundSampleVPixelSize, 0xFF00FF00); // green
            Win32DebugDrawVertical( BackBuffer, OutputLocationX + BytesToWrite, Top, Top + SoundSampleVPixelSize, 0xFF00FF00); // green
            Win32DebugDrawHorizontal( BackBuffer, Top + (SoundSampleVPixelSize / 2), OutputLocationX, OutputLocationX + BytesToWrite, 0xFFFFFFFF);
            
            Top += SoundSampleVPixelSize;
            
            Win32DebugDrawVertical( BackBuffer, FlipPlayCursorX, Top, Top + SoundSampleVPixelSize, 0xFFFF00FF );
            Win32DebugDrawVertical( BackBuffer, FlipWriteCursorX, Top, Top + SoundSampleVPixelSize, 0xFF00FFFF );
            Win32DebugDrawHorizontal( BackBuffer, Top + (SoundSampleVPixelSize / 2), FlipPlayCursorX, FlipWriteCursorX, 0xFFFFFFFF );
            
            
        }
    }
}
#endif

// ============================================================================

int CALLBACK WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow )
{
    win32_state Win32State = {};
    Win32GetEXEFileName( &Win32State );
    
    char GameCodeLockFileFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName( &Win32State, "lock.tmp", sizeof( GameCodeLockFileFullPath ), GameCodeLockFileFullPath );
    
    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName( &Win32State, "raytracer.dll", sizeof( SourceGameCodeDLLFullPath ), SourceGameCodeDLLFullPath );
    
    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName( &Win32State, "raytracer_temp.dll", sizeof( TempGameCodeDLLFullPath ), TempGameCodeDLLFullPath );
    
    LARGE_INTEGER PerfCounterFrequencyResult;
    ::QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;
    bool32 SleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
    // desired windows scheduler granularity to 1 ms
    
    Win32LoadXInput();
    Win32ResizeDIBSection( &GlobalBackBuffer, 960, 540 ); // Half of HD 1080p
    
#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif
    
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW; // | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = hInstance;
    WindowClass.hCursor = ::LoadCursorA( NULL, IDC_CROSS );
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "RaytracerWindowClass";
    
    if ( RegisterClassA( &WindowClass ) ) 
    {
        HWND hWindow = CreateWindowExA(
            0, //WS_EX_TOPMOST | WS_EX_LAYERED,
            WindowClass.lpszClassName,
            "Raytracer",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            1000, //CW_USEDEFAULT,
            600, //CW_USEDEFAULT,
            0,
            0,
            hInstance,
            0 
            );
        
        if ( hWindow ) 
        {
            bool DoVibration = false;
            
            // TODO(nfauvet): how do we reliably query this on windows.
            int32 MonitorRefreshHz = 60; 
            HDC RefreshDC = ::GetDC( hWindow );
            int32 Win32RefreshRate = ::GetDeviceCaps( RefreshDC, VREFRESH );
            ::ReleaseDC( hWindow, RefreshDC );
            if ( Win32RefreshRate > 1 ) // 0 or 1 are default values
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            
            real32 GameUpdateHz = MonitorRefreshHz / 2.0f; // 30 Hz because software rendering.
            real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;
            
            // sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof( int16 ) * 2; // [LEFT  RIGHT] LEFT RIGHT LEFT RIGHT
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            // TODO(nfauvet): compute the variance to find the lowest value.
            SoundOutput.SafetyBytes = (int)( ( (real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample / GameUpdateHz ) / 3.0f );
            
            Win32InitDSound( hWindow, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize );
            Win32ClearBuffer( &SoundOutput );
            pGlobalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING );
            
            //win32_state Win32State = {};
            Win32State.InputRecordingIndex = 0;
            Win32State.InputPlayingIndex = 0;
            
            GlobalRunning = true;
            
#if 0		// This tests the PlayCursor/WriteCursor update frequency
            GlobalRunning = true;
            while ( GlobalRunning ) 
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                pGlobalSecondaryBuffer->GetCurrentPosition( &PlayCursor, &WriteCursor );
                
                char text_buffer[512];
                _snprintf_s( text_buffer, sizeof( text_buffer ), "PC: %u WC: %u\n", PlayCursor, WriteCursor );
                ::OutputDebugStringA( text_buffer );
            }
#endif
            
            // ALLOC sound buffer
            int16 *Samples = (int16 *)::VirtualAlloc( 0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            // ALLOC all game storage
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes( 256 );
            GameMemory.TransientStorageSize = Gigabytes( 1 );
            // TODO(nfauvet): use MEM_LARGE_PAGES and call AdjustTokenPrivileges when not on WindowsXP ?
            Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = ::VirtualAlloc( BaseAddress, Win32State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = (void*)((uint8_t*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize );
            
            for ( int ReplayIndex = 0; ReplayIndex < ArrayCount( Win32State.ReplayBuffers ); ++ReplayIndex ) 
            {
                win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];
                
                // gen filename for the state recording file.
                Win32GetInputFileLocation( &Win32State, false, ReplayIndex, sizeof( ReplayBuffer->FileName ), ReplayBuffer->FileName );
                
                // create the file
                ReplayBuffer->FileHandle = ::CreateFileA( ReplayBuffer->FileName, GENERIC_WRITE | GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0 );
                
                // memory map the file
                DWORD MaxSizeHigh = ( Win32State.TotalSize >> 32 );
                DWORD MaxSizeLow = ( Win32State.TotalSize & 0xFFFFFFFF );
                ReplayBuffer->MemoryMap = ::CreateFileMappingA( 
                    ReplayBuffer->FileHandle, 0, 
                    PAGE_READWRITE, 
                    MaxSizeHigh, MaxSizeLow, 0 );
                
                // get a pointer to the mapping in RAM
                ReplayBuffer->MemoryBlock = ::MapViewOfFile( 
                    ReplayBuffer->MemoryMap, 
                    FILE_MAP_ALL_ACCESS, 0, 0, 
                    Win32State.TotalSize );
                
                // we don't care about the BaseAddress here because it is only temporary storage.
                //ReplayBuffer->MemoryBlock = ::VirtualAlloc( 0, Win32State.TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
                
                if ( ReplayBuffer->MemoryBlock ) {
                } else {
                    // TODO(nfauvet): Diagnostic
                }
            }
            
            GameMemory.DEBUGPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;
            
            // quit if memory alloc failed.
            if ( Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage )
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];
                
                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();
                uint64 LastCycleCount = __rdtsc();
                
                int DebugLastPlayCursorIndex = 0;
                win32_debug_time_marker DebugLastPlayCursor[120] = { 0 }; // 2 * GameUpdateHz
                
                bool32 bSoundIsValid = false;
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                
                win32_game_code Game = Win32LoadGameCode( SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, GameCodeLockFileFullPath );
                uint32 LoadCounter = 0;
                
                // GAME LOOP
                while ( GlobalRunning )
                {
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime( SourceGameCodeDLLFullPath );
                    
                    if ( ::CompareFileTime( &NewDLLWriteTime, &Game.DLLLastWriteTime ) != 0 )
                    {
                        Win32UnloadGameCode( &Game );
                        Game = Win32LoadGameCode( SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath, GameCodeLockFileFullPath );
                        LoadCounter = 0;
                    }
                    
                    game_controller_input *OldKeyboardController = GetController( OldInput, 0 );
                    game_controller_input *NewKeyboardController = GetController( NewInput, 0 );
                    
                    // TODO(nfauvet): Zeroing macro
                    // copy endeddown but reset transition.
                    game_controller_input ZeroKeyboard = {};
                    *NewKeyboardController = ZeroKeyboard;
                    NewKeyboardController->IsConnected = true;
                    for ( int ButtonIndex = 0; ButtonIndex < ArrayCount( NewKeyboardController->Buttons ); ++ButtonIndex ) 
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages( &Win32State, NewKeyboardController );
                    
                    if ( !GlobalPause )
                    {
                        POINT MouseP;
                        ::GetCursorPos( &MouseP ); // in desktop coordinate system.
                        ::ScreenToClient( hWindow, &MouseP );
                        NewInput->MouseX = MouseP.x;
                        NewInput->MouseY = MouseP.y;
                        NewInput->MouseZ = 0; // TODO(nfauvet): support mousewheel?
                        // ::GetKeyState( VK_LBUTTON ) can be called to know about the last
                        // keyboard state(windows tracks it for us.
                        // fortunately, there are VK codes for mouse buttons !!!
                        // this is a cheap and dirty way to ask for mouse state.
                        Win32ProcessKeyboardMessage( &NewInput->MouseButtons[0], ::GetKeyState( VK_LBUTTON ) & ( 1<< 15) );
                        Win32ProcessKeyboardMessage( &NewInput->MouseButtons[1], ::GetKeyState( VK_MBUTTON ) & ( 1 << 15 ) );
                        Win32ProcessKeyboardMessage( &NewInput->MouseButtons[2], ::GetKeyState( VK_RBUTTON ) & ( 1 << 15 ) );
                        Win32ProcessKeyboardMessage( &NewInput->MouseButtons[3], ::GetKeyState( VK_XBUTTON1 ) & ( 1 << 15 ) );
                        Win32ProcessKeyboardMessage( &NewInput->MouseButtons[4], ::GetKeyState( VK_XBUTTON2 ) & ( 1 << 15 ) );
                        
                        // xinput poll
                        DWORD MaxControllerCount = XUSER_MAX_COUNT; // 4 ?
                        if ( MaxControllerCount > ( ArrayCount( NewInput->Controllers ) - 1 ) ) { // 4  > 5 -1 ?
                            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
                        }
                        
                        // for each controller (not the keyboard)
                        for ( DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex )
                        {
                            XINPUT_STATE ControllerState;
                            
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
                            
                            if ( XInputGetState( ControllerIndex, &ControllerState ) == ERROR_SUCCESS )
                            {
                                XINPUT_GAMEPAD *pPad = &ControllerState.Gamepad;
                                
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;
                                
                                NewController->StickAverageX = Win32ProcessXinputStickValue(pPad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXinputStickValue(pPad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                
                                // Analog only if we actually moved the sticks out of the deadzone.
                                if (	( NewController->StickAverageX != 0.0f )
                                    ||	( NewController->StickAverageY != 0.0f ) ) {
                                    NewController->IsAnalog = true;
                                }
                                
                                // Fake a StickX/Y full value if the DPAD buttons have been pushed.
                                if ( pPad->wButtons & XINPUT_GAMEPAD_DPAD_UP ) {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if ( pPad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ) {
                                    NewController->StickAverageY = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if ( pPad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) {
                                    NewController->StickAverageX = 1.0f;
                                    NewController->IsAnalog = false;
                                }
                                if ( pPad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT ) {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }
                                
                                // Fake a "Move" buttons push with the StickX/Y if above a threshold
                                real32 Threshold = 0.5f;
                                Win32ProcessXInputDigitalButton( ( NewController->StickAverageX < -Threshold ) ? 1 : 0,
                                                                &OldController->MoveLeft, 1,  // FAKE Bitfield that is going to be "&" with 1 or 0
                                                                &NewController->MoveLeft );
                                Win32ProcessXInputDigitalButton( ( NewController->StickAverageX > Threshold ) ? 1 : 0,
                                                                &OldController->MoveRight, 1,
                                                                &NewController->MoveRight );
                                Win32ProcessXInputDigitalButton( ( NewController->StickAverageY < -Threshold ) ? 1 : 0,
                                                                &OldController->MoveDown, 1,
                                                                &NewController->MoveDown );
                                Win32ProcessXInputDigitalButton( ( NewController->StickAverageY > Threshold ) ? 1 : 0,
                                                                &OldController->MoveUp, 1,
                                                                &NewController->MoveUp );
                                
                                // Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveUp, XINPUT_GAMEPAD_DPAD_UP, &NewController->MoveUp );
                                // Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveDown, XINPUT_GAMEPAD_DPAD_DOWN, &NewController->MoveDown );
                                // Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveRight, XINPUT_GAMEPAD_DPAD_LEFT, &NewController->MoveRight );
                                // Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->MoveLeft, XINPUT_GAMEPAD_DPAD_RIGHT, &NewController->MoveLeft );
                                
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown );
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight );
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft );
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp );
                                
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder );
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder );
                                
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start );
                                Win32ProcessXInputDigitalButton( pPad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back );
                            } else {
                                NewController->IsConnected = false;
                            }
                        }
                        
                        if ( DoVibration ) {
                            XINPUT_VIBRATION Vibration;
                            Vibration.wLeftMotorSpeed = 60000;
                            Vibration.wRightMotorSpeed = 60000;
                            XInputSetState( 0, &Vibration );
                        }
                        
                        thread_context Thread = {};
                        
                        // GAME UPDATE + RENDER
                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackBuffer.Memory;
                        Buffer.Width = GlobalBackBuffer.Width;
                        Buffer.Height = GlobalBackBuffer.Height;
                        Buffer.Pitch = GlobalBackBuffer.Pitch;
                        Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;
                        
                        
                        if ( Win32State.InputRecordingIndex ) {
                            Win32RecordInput( &Win32State, NewInput );
                        }
                        
                        if ( Win32State.InputPlayingIndex ) {
                            Win32PlayBackInput( &Win32State, NewInput ); // overwrite input
                        }
                        
                        if ( Game.UpdateAndRender ) {
                            Game.UpdateAndRender( &Thread, &GameMemory, NewInput, &Buffer );
                        }
                        
                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed( FlipWallClock, AudioWallClock );
                        
                        // SOUND
                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if ( DS_OK == pGlobalSecondaryBuffer->GetCurrentPosition( &PlayCursor, &WriteCursor ) )
                        {
                            /* NOTE(nfauvet):
                            
                             Here is how sound output computation works.
                             
                             We define a safety value that is te number of samples we think
                             our game update loop may vary by(let's say up to 2ms).
                             
                             When we wake up to write audio, we will look and see what the play
                             cursor position is and we will forecast ahead where we think the
                             play cursor will be on the next frame boundary.
                             
                             We will then look to see if the write cursor is before that
                             by at least our safety value. If it is, the target fill position
                             is that frame boundary plus one frame. This gives us perfect
                             audio sync in the case of a card that has low enough latency.
                             
                             If the write cursor is _after_ that safety margin, then we
                             assume we can never sync the audio perfectly, so we will write
                             one frame's worth of audio plus the safety margin's worth
                             of guard samples.
                             */
                            
                            if ( !bSoundIsValid ) {
                                // at startup, reset the running sample position to the writecursor.
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                bSoundIsValid = true;
                            }
                            
                            // Lock wherever we were, running is infinite, wrap it up the circular buffer.
                            DWORD ByteToLock = ( SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample ) % SoundOutput.SecondaryBufferSize;
                            
                            DWORD ExpectedSoundBytesPerFrame = (DWORD)( ( (real32)SoundOutput.SamplesPerSecond * (real32)SoundOutput.BytesPerSample ) / GameUpdateHz );
                            real32 SecondsLeftUntilFlip = ( TargetSecondsPerFrame - FromBeginToAudioSeconds );
                            DWORD ExpectedBytesUntilFlip = (DWORD)( ( SecondsLeftUntilFlip / TargetSecondsPerFrame ) * (real32)ExpectedSoundBytesPerFrame );
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip; //ExpectedSoundBytesPerFrame;
                            
                            DWORD SafeWriteCursor = WriteCursor;
                            if ( SafeWriteCursor < PlayCursor ){
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize; // wrap/normalize relatively to the PlayCursor
                            }
                            Assert( SafeWriteCursor >= PlayCursor );
                            SafeWriteCursor += SoundOutput.SafetyBytes; // accounting for the jitter
                            
                            bool32 AudioCardIsLowLatency = ( SafeWriteCursor < ExpectedFrameBoundaryByte );
                            
                            DWORD TargetCursor = 0;
                            if ( AudioCardIsLowLatency ) {
                                TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
                            } else {
                                TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes;
                            }
                            TargetCursor = TargetCursor % SoundOutput.SecondaryBufferSize;
                            
                            DWORD BytesToWrite = 0;
                            if ( ByteToLock > TargetCursor ) {
                                // 2 sections to write to, 1 section after, and one at the beginning befiore the PlayCursor
                                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock + TargetCursor;
                            } else {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }
                            
                            // sound
                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            
                            // Ask Game for sound samples
                            if ( Game.GetSoundSamples ) {
                                Game.GetSoundSamples( &Thread, &GameMemory, &SoundBuffer );
                            }
                            
#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker = &DebugLastPlayCursor[DebugLastPlayCursorIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                            
                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if ( UnwrappedWriteCursor < PlayCursor ) {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            AudioLatencySeconds = ( (real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample ) / (real32)SoundOutput.SamplesPerSecond;
                            
#if 0
                            char text_buffer[512];
                            _snprintf_s(text_buffer, sizeof(text_buffer), "BTL: %u PC: %u WC: %u DELTA: %u (%.3fs)\n", ByteToLock, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                            ::OutputDebugStringA(text_buffer);
#endif
#endif
                            // Actually write the sound samples to the sound card.
                            Win32FillSoundBuffer( &SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer );
                            
                        } else {
                            bSoundIsValid = false;
                        }
                        
                        
                        // timing
                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsedForWork = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                        real32 SecondsElapsedForFrame = WorkSecondsElapsedForWork;
                        if ( SecondsElapsedForFrame < TargetSecondsPerFrame ) {
                            if ( SleepIsGranular ) {
                                DWORD SleepMS = (DWORD)( 1000.0f * ( TargetSecondsPerFrame - SecondsElapsedForFrame ) );
                                if ( SleepMS > 0 ) {
                                    ::Sleep( SleepMS );
                                }
                            }
                            
                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed( LastCounter, Win32GetWallClock() );
                            if ( TestSecondsElapsedForFrame < TargetSecondsPerFrame ) {
                                // Log MISSED SLEEP
                            }
                            
                            while ( SecondsElapsedForFrame < TargetSecondsPerFrame ) {	
                                SecondsElapsedForFrame = Win32GetSecondsElapsed( LastCounter, Win32GetWallClock() );
                            }
                        } else {
                            // TODO: missed framerate !!!!
                            // TODO: log it
                        }
                        
                        LARGE_INTEGER EndCounter = Win32GetWallClock();
                        real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed( LastCounter, EndCounter );
                        LastCounter = EndCounter;
                        
                        // copy display buffer
                        win32_window_dimension d = Win32GetWindowDimension(hWindow);
                        HDC dc = ::GetDC( hWindow );
                        Win32CopyBufferToWindow( dc, d.width, d.height, &GlobalBackBuffer );
                        ::ReleaseDC( hWindow, dc );
                        
                        FlipWallClock = Win32GetWallClock();
#if HANDMADE_INTERNAL
                        // debug sound
                        {
                            DWORD FlipPlayCursor;
                            DWORD FlipWriteCursor;
                            if ( DS_OK == pGlobalSecondaryBuffer->GetCurrentPosition( &FlipPlayCursor, &FlipWriteCursor ) ) {
                                Assert( DebugLastPlayCursorIndex < ArrayCount( DebugLastPlayCursor ) );
                                win32_debug_time_marker *Marker = &DebugLastPlayCursor[DebugLastPlayCursorIndex];
                                
                                Marker->FlipPlayCursor = FlipPlayCursor;
                                Marker->FlipWriteCursor = FlipWriteCursor;
                            }
                        }
#endif
                        
                        // swap
                        game_input *tmp = NewInput;
                        NewInput = OldInput;
                        OldInput = tmp;
                        
#if 0
                        uint64 EndCycleCount = __rdtsc();
                        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;
                        
                        real32 FPS = 1000.0f / MSPerFrame;
                        real64 MCPF = ( (real64)CyclesElapsed / ( 1000.0f * 1000.0f ) ); // mega cycles
                        
                        char FPSBuffer[512];
                        _snprintf_s( FPSBuffer, sizeof( FPSBuffer ), "%.02fms/f,  %.02ffps, %.02fmc/f\n", MSPerFrame, FPS, MCPF );
                        ::OutputDebugStringA( FPSBuffer );
#endif
                        
#if HANDMADE_INTERNAL
                        ++DebugLastPlayCursorIndex;
                        if ( DebugLastPlayCursorIndex == ArrayCount( DebugLastPlayCursor ) ) { // wrap
                            DebugLastPlayCursorIndex = 0;
                        }
#endif
                    } // if ( !GlobalPause )
                    
                } // while ( GlobalRunning )
            } else {
                // TODO: log, VirtualAlloc failed.
            }
        } else {
            // TODO: log
        }
    } else {
        // TODO: log
    }
    
    return(0);
}
