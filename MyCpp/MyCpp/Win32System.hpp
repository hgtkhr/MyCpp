#pragma once
#include <cstring>
#include <filesystem>
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	typedef std::filesystem::path path_t;

	namespace Details
	{
		template < typename bool IsNative = true >
		struct p2s_policy
		{
			static const string_t& string( const path_t& p ) { return p.native(); }
		};

		template <>
		struct p2s_policy< false >
		{
			static string_t string( const path_t& p ) { return p.string< char_t >(); }
		};
	}

	inline string_t to_string_t( const path_t& p )
	{
		return Details::p2s_policy< std::is_same< string_t, path_t::string_type >::value >::string( p );
	}

	typedef std::shared_ptr< CRITICAL_SECTION > SPCRITICAL_SECTION;

	constexpr std::uint32_t CSSPIN_AUTO = 4000;

	SPCRITICAL_SECTION CreateCriticalSection( std::uint32_t spinCount = CSSPIN_AUTO );

	namespace Details
	{
		struct CriticalSectionExit
		{
			typedef CRITICAL_SECTION* pointer;

			void operator () ( pointer p )
			{
				if ( p != null )
					::LeaveCriticalSection( p );
			}
		};

		struct MutexDeleter
		{
			typedef HANDLE pointer;

			void operator () ( pointer p )
			{
				if ( p != null )
				{
					::ReleaseMutex( p );
					::CloseHandle( p );
				}
			}
		};
	}

	typedef ScopedHandle< HANDLE, Details::MutexDeleter > MUTEX;
	typedef std::unique_ptr< CRITICAL_SECTION, Details::CriticalSectionExit > CRITICAL_SECTION_LOCK;

	std::pair< DWORD, MUTEX > TryLockMutex( const string_t& name, bool waitForGetOwnership );
	CRITICAL_SECTION_LOCK TryLockCriticalSection( CRITICAL_SECTION& csObj );

	inline CRITICAL_SECTION_LOCK TryLockCriticalSection( SPCRITICAL_SECTION& csObj )
	{
		return TryLockCriticalSection( *csObj );
	}

	template < typename T, DWORD Size = sizeof( T ) >
	inline DWORD Fill0( T& podStruct )
	{
		ZeroMemory( &podStruct, Size );

		return Size;
	}

	template < typename Vector, typename Source >
	std::size_t adaptive_load( Vector& v, std::size_t initn, Source source )
	{
		if ( v.size() < initn )
			v.resize( initn );

		std::size_t result = 0;

		while ( true )
		{
			result = source( v.data(), v.size() );
			if ( result == 0 || result < v.size() )
				break;

			v.resize( v.size() * 2 );
		}

		return result;
	}

	typedef std::shared_ptr< std::remove_pointer< PSID >::type > SPSID;

	SPSID GetProcessSid( HANDLE process );

	path_t GetCurrentLocation();
	path_t GetSpecialFolderLocation( const GUID& folderId );
	path_t GetTemporaryPath();
	path_t GetTemporaryFileName( const string_t& prefix );
	path_t FindFilePath( const string_t& filename, const string_t& ext = null );

	string_t ToGuidString( const GUID& guid );

	string_t GetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName );
	uint32_t GetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, void* ptr, std::uint32_t size );
	void SetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName, const string_t& value );
	void SetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, uint32_t type, const void* ptr, std::uint32_t size );

	template < typename XWORD >
	inline XWORD GetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		XWORD result;

		GetRegBinary( parentKey, subKey, valueName, &result, sizeof( XWORD ) );

		return result;
	}

	inline void SetRegDword( HKEY parentKey, const string_t& subKey, const string_t& valueName, std::uint32_t value )
	{
		SetRegBinary( parentKey, subKey, valueName, REG_DWORD, &value, sizeof( std::uint32_t ) );
	}

	inline void SetRegQword( HKEY parentKey, const string_t& subKey, const string_t& valueName, std::uint64_t value )
	{
		SetRegBinary( parentKey, subKey, valueName, REG_QWORD, &value, sizeof( std::uint64_t ) );
	}

	path_t GetProgramModuleFileName( HMODULE hmodule = null );
	path_t GetProgramModuleFileLocation( HMODULE hmodule = null );

	class Process
	{
	public:
		class Data;

		Process();
		Process( Data&& data );

		~Process();

		string_t GetName() const;
		path_t GetFileName() const;
		HANDLE GetHandle() const;
		HANDLE GetPrimaryThreadHandle() const;
		DWORD GetId() const;
		DWORD GetPrimaryThreadId() const;
		DWORD GetExitCode() const;

		void Terminate( int exitCode );
		void Suspend();
		void Resume();

		DWORD Wait( DWORD milliseconds = INFINITE, bool forInputIdle = false ) const;
	private:
		std::shared_ptr< Data > m_data;
	};

	typedef std::shared_ptr< Process > SPPROCESS;

	inline SPSID GetProcessSid( const SPPROCESS& process )
	{
		return GetProcessSid( process->GetHandle() );
	}

	HWND FindProcessWindow( const SPPROCESS& process, const string_t& wndClassName, const string_t& wndName );

	SPPROCESS GetProcess( DWORD pid );
	SPPROCESS GetProcess( HANDLE hProcess );
	SPPROCESS GetParentProcess();

	const Process* GetCurrentProcess();

	SPPROCESS OpenProcessByFileName( const path_t& fileName, bool inheritHandle = false, DWORD accessMode = 0 );
	SPPROCESS OpenCuProcessByFileName( const path_t& fileName, bool inheritHandle = false, DWORD accessMode = 0 );

	SPPROCESS StartProcess( const string_t& cmdline, const path_t& appCurrentDir = null, void* envVariables = null, int creationFlags = 0, bool inheritHandle = false,  int cmdShow = SW_SHOWDEFAULT );

	int RunElevated( const path_t& file, const string_t& parameters = null, bool waitForExit = true, int cmdShow = SW_SHOWDEFAULT );

	string_t GetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& defaultValue = null );
	void SetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& value );

	namespace Details
	{
		// unsigned
		template <
			typename Int,
			type_enable_if< std::is_unsigned< Int >::value > = enabler
		>
			inline Int GetIniInt( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< Int >( std::stoull( GetIniString( file, section, name ), null, 10 ) );
		}

		// signed
		template <
			typename Int,
			type_enable_if< !std::is_unsigned< Int >::value > = enabler
		>
			inline Int GetIniInt( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< Int >( std::stoll( GetIniString( file, section, name ), null, 10 ) );
		}

		// unsigned
		template <
			typename Int,
			type_enable_if< std::is_unsigned< Int >::value > = enabler
		>
			inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64u" ), static_cast< uint64_t >( value ) ) );
		}

		// signed
		template <
			typename Int,
			type_enable_if< !std::is_unsigned< Int >::value > = enabler
		>
			inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64" ), static_cast< uint64_t >( value ) ) );
		}
	}

	template < typename Int >
	inline Int GetIniInt( const path_t& file, const string_t& section, const string_t& name )
	{
		return Details::GetIniInt< Int >( file, section, name );
	}

	template < typename Int >
	inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
	{
		Details::SetIniInt( file, section, name, value );
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::path_t;
using MyCpp::MUTEX;
using MyCpp::SPCRITICAL_SECTION;
using MyCpp::CRITICAL_SECTION_LOCK;
using MyCpp::SPPROCESS;
using MyCpp::SPSID;
#endif
