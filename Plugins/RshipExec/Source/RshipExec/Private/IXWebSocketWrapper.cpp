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
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXBench.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXCancellationRequest.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXConnectionState.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXDNSLookup.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXExponentialBackoff.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXGetFreePort.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXNetSystem.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterrupt.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptFactory.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSetThreadName.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocket.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocketConnect.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSocketFactory.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXStrCaseCompare.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUdpSocket.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUrlParser.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXUserAgent.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocket.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketCloseConstants.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketHandshake.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketHttpHeaders.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflate.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateCodec.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketPerMessageDeflateOptions.cpp"
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXWebSocketTransport.cpp"

// Platform-specific: SelectInterruptPipe uses pipe() which isn't available on Windows
#if !PLATFORM_WINDOWS
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptPipe.cpp"
#else
#include "../ThirdParty/IXWebSocket/ixwebsocket/IXSelectInterruptEvent.cpp"
#endif

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_END

#endif // RSHIP_USE_IXWEBSOCKET
