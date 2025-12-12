/**
 * IXWebSocket Unity Build Wrapper
 *
 * This file includes all IXWebSocket source files as a single compilation unit.
 * This is the standard way to integrate third-party C++ libraries in Unreal Engine.
 *
 * Only compiled when RSHIP_USE_IXWEBSOCKET=1 (IXWebSocket is available in ThirdParty folder)
 */

#include "CoreMinimal.h"

#if RSHIP_USE_IXWEBSOCKET

// Disable warnings for third-party code
THIRD_PARTY_INCLUDES_START

// Platform-specific includes
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#endif

// Core IXWebSocket files (client-only, no HTTP/server)
// Note: Files are .inl to prevent UE from compiling them separately
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXBench.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXCancellationRequest.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXConnectionState.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXDNSLookup.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXExponentialBackoff.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXGetFreePort.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXNetSystem.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterrupt.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptFactory.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSetThreadName.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocket.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocketConnect.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocketFactory.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXStrCaseCompare.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUdpSocket.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUrlParser.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUserAgent.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocket.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketCloseConstants.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketHandshake.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketHttpHeaders.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflate.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateCodec.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateOptions.inl"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketTransport.inl"

// Platform-specific: SelectInterruptPipe uses pipe() which isn't available on Windows
#if !PLATFORM_WINDOWS
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptPipe.inl"
#else
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptEvent.inl"
#endif

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_END

#endif // RSHIP_USE_IXWEBSOCKET
