// Interface for Plugin Loader in SFM
// by ficool2

#include "engine/iserverplugin.h"
#include "materialsystem/ishadersystem.h"
#include "tier0/icommandline.h"
#include "tier1/interface.h"
#include <Windows.h>

// 
// Forward declarations
// 

IMaterialSystem* materials = NULL;
HMODULE g_hModule = NULL;

// 
// CShaderSystem - forward declaration of the internal interface
// We redeclare only what we need to call LoadShaderDLL.
// NOTE: vtable position must match the actual IShaderSystemInternal
// in the target SFM build. If this crashes, the vtable index changed.
// 

class CShaderSystem : public IShaderSystemInternal
{
public:
	virtual bool LoadShaderDLL( const char* pFullPath, const char* pPathID, bool bModShaderDLL ) = 0;
};

// 
// CPlugin_ShaderPBR
// 

class CPlugin_ShaderPBR : public IServerPluginCallbacks
{
public:
	bool Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory ) override;
	void Unload( void ) override;
	void Pause( void ) override {}
	void UnPause( void ) override {}
	const char* GetPluginDescription( void ) override { return "ZMR PBR Shader Plugin (ficool2)"; }
	void LevelInit( char const* pMapName ) override {}
	void ServerActivate( edict_t* pEdictList, int edictCount, int clientMax ) override {}
	void GameFrame( bool simulating ) override {}
	void LevelShutdown( void ) override {}
	void ClientActive( edict_t* pEntity ) override {}
	void ClientFullyConnect( edict_t* pEntity ) override {}
	void ClientDisconnect( edict_t* pEntity ) override {}
	void ClientPutInServer( edict_t* pEntity, char const* playername ) override {}
	void SetCommandClient( int index ) override {}
	void ClientSettingsChanged( edict_t* pEdict ) override {}
	PLUGIN_RESULT ClientConnect( bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen ) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT ClientCommand( edict_t* pEntity, const CCommand& args ) override { return PLUGIN_CONTINUE; }
	PLUGIN_RESULT NetworkIDValidated( const char* pszUserName, const char* pszNetworkID ) override { return PLUGIN_CONTINUE; }
	void OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue ) override {}
	void OnEdictAllocated( edict_t* edict ) override {}
	void OnEdictFreed( const edict_t* edict ) override {}

private:
	bool LoadShaders();

	// Track whether the shader DLL was loaded so Unload() can skip if Load() failed
	bool m_bShadersLoaded = false;
};

// 
// Load
// 

bool CPlugin_ShaderPBR::Load( CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	ConColorMsg( Color( 100, 220, 100, 255 ), "[PBR Shader] Loading plugin...\n" );

	// Acquire IMaterialSystem 
	materials = static_cast<IMaterialSystem*>( interfaceFactory( MATERIAL_SYSTEM_INTERFACE_VERSION, NULL ) );
	if ( !materials )
	{
		Warning( "[PBR Shader] ERROR: Failed to get IMaterialSystem (interface version mismatch?)\n" );
		return false;
	}

	if ( !LoadShaders() )
	{
		Warning( "[PBR Shader] ERROR: Failed to load shader DLL. Plugin will not be active.\n" );
		return false;
	}

	m_bShadersLoaded = true;
	ConColorMsg( Color( 100, 220, 100, 255 ), "[PBR Shader] Loaded successfully.\n" );
	return true;
}

// 
// Unload
// Original plugin had an empty Unload(). This at least logs it.
// Actual shader unloading would require calling UnloadShaderDLL
// if the interface exposes it — it often doesn't, so we just warn.
// 

void CPlugin_ShaderPBR::Unload( void )
{
	if ( m_bShadersLoaded )
	{
		// IShaderSystemInternal typically has no UnloadShaderDLL,
		// so the shader stays in memory. This is expected behavior
		// for Source engine shader plugins.
		ConColorMsg( Color( 255, 180, 50, 255 ), 
			"[PBR Shader] Unloaded. Note: shader DLL remains in memory "
			"(Source engine does not support shader hot-unloading).\n" );
		m_bShadersLoaded = false;
	}
}

// 
// LoadShaders
//
// BUG IN ORIGINAL: dynamic_cast<> on a pure interface pointer is
// unreliable across DLL boundaries. The cast succeeds or fails
// depending on RTTI, which may not be enabled or may not match
// across different compiler settings between engine and plugin.
//
// SAFER: Use static_cast<> directly since we know the interface
// inherits from IShaderSystemInternal and we only need the vtable.
//
// BUG IN ORIGINAL: No null check on pShaderSystem after the cast.
// If the cast fails, the call to LoadShaderDLL() crashes immediately.
//
// BUG IN ORIGINAL: No null check on the return of LoadShaderDLL().
// The function returns bool but the result was ignored.
// 

bool CPlugin_ShaderPBR::LoadShaders()
{
	// QueryInterface returns IShaderSystem* (public interface).
	// We static_cast to our redeclared CShaderSystem to call LoadShaderDLL.
	// dynamic_cast is unreliable across DLL boundaries without matching RTTI.
	IShaderSystem* pRawInterface = static_cast<IShaderSystem*>(
		materials->QueryInterface( SHADERSYSTEM_INTERFACE_VERSION ) );

	if ( !pRawInterface )
	{
		Warning( "[PBR Shader] ERROR: QueryInterface for IShaderSystem failed. "
		         "SHADERSYSTEM_INTERFACE_VERSION mismatch?\n" );
		return false;
	}

	CShaderSystem* pShaderSystem = static_cast<CShaderSystem*>( pRawInterface );

	// Get the full path of this DLL so we can tell the shader system where we are.
	char szFileName[MAX_PATH];
	DWORD dwResult = GetModuleFileNameA( g_hModule, szFileName, sizeof( szFileName ) );
	if ( dwResult == 0 || dwResult == sizeof( szFileName ) )
	{
		Warning( "[PBR Shader] ERROR: GetModuleFileName failed (error %lu)\n", GetLastError() );
		return false;
	}

	ConColorMsg( Color( 180, 180, 255, 255 ), "[PBR Shader] Loading shader DLL from: %s\n", szFileName );

	// LoadShaderDLL returns bool but the original code ignored it.
	bool bResult = pShaderSystem->LoadShaderDLL( szFileName, "GAME", true );
	if ( !bResult )
	{
		Warning( "[PBR Shader] ERROR: LoadShaderDLL returned false. "
		         "Shader registration failed.\n" );
		return false;
	}

	return true;
}

// 
// Expose the plugin interface and DLL entry point
// 

EXPOSE_INTERFACE( CPlugin_ShaderPBR, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS );

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	switch ( fdwReason )
	{
		case DLL_PROCESS_ATTACH:
			g_hModule = hinstDLL;
			// Disable thread attach/detach notifications for performance
			DisableThreadLibraryCalls( hinstDLL );
			break;

		case DLL_PROCESS_DETACH:
			// lpvReserved != NULL means process is terminating (not FreeLibrary).
			// In that case we can skip cleanup.
			break;
	}

	return TRUE;
}
