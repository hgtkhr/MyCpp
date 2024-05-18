#pragma once
#include <cstring>
#include <filesystem>
#include "MyCpp/StringUtils.hpp"
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	typedef std::filesystem::path path_t;

	namespace Details
	{
		template < bool IsNativeString = true >
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
		return Details::p2s_policy< std::is_same_v< string_t, path_t::string_type > >::string( p );
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
	dword GetRegDword( HKEY parentKey, const string_t& subKey, const string_t& valueName );
	qword GetRegQword( HKEY parentKey, const string_t& subKey, const string_t& valueName );
	void SetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName, const string_t& value );
	void SetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, uint32_t type, const void* ptr, std::uint32_t size );

	namespace Details
	{
		// qword - 64bit unsigned
		template < typename Xword >
		inline
		std::enable_if_t< std::is_same_v< Xword, qword >, Xword >
		GetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
		{
			return GetRegDword( HKEY parentKey, const string_t & subKey, const string_t & valueName );
		}

		// dword - 32bit unsigned
		template < typename Xword >
		inline
		std::enable_if_t< std::is_same_v< Xword, dword >, Xword >
		GetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
		{
			return GetRegQword( HKEY parentKey, const string_t & subKey, const string_t & valueName );
		}
	}

	template < typename Xword >
	inline Xword GetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		return Details::GetRegXword< Xword >( parentKey, subKey, valueName );
	}

	inline void SetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName, dword value )
	{
		SetRegBinary( parentKey, subKey, valueName, REG_DWORD, &value, sizeof( dword ) );
	}

	inline void SetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName, qword value )
	{
		SetRegBinary( parentKey, subKey, valueName, REG_QWORD, &value, sizeof( qword ) );
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
	bool GetIniBinary( const path_t& file, const string_t& section, const string_t& name, void* ptr, std::uint32_t size );
	void SetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& value );
	void SetIniBinary( const path_t& file, const string_t& section, const string_t& name, const void* ptr, std::uint32_t size );

	namespace Details
	{
		// unsigned
		template < typename Int>
		inline 
		std::enable_if_t< std::is_unsigned_v< Int >, Int >
		GetIniInt( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< Int >( std::stoull( GetIniString( file, section, name ), null, 10 ) );
		}

		// signed
		template < typename Int >
		inline 
		std::enable_if_t< !std::is_unsigned_v< Int >, Int >
		GetIniInt( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< Int >( std::stoll( GetIniString( file, section, name ), null, 10 ) );
		}

		// qword - 64bit unsigned
		template < typename Xword >
		inline
		std::enable_if_t < std::is_same_v< Xword, qword >, Xword >
		GetIniXword( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< qword >( std::stoull( GetIniString( file, section, name ), null, 16 ) );
		}

		// dword - 32bit unsigned
		template < typename Xword >
		inline 
		std::enable_if_t< std::is_same_v< Xword, dword >, Xword >
		GetIniXword( const path_t& file, const string_t& section, const string_t& name )
		{
			return static_cast< dword >( std::stoul( GetIniString( file, section, name ), null, 16 ) );
		}

		// double - floating point
		inline double GetIniDouble( const path_t& file, const string_t& section, const string_t& name )
		{
			return std::stod( GetIniString( file, section, name ) );
		}

		// Primitive Data Type 
		template 
		<
			typename DataType,
			std::enable_if_t< std::is_standard_layout_v< DataType > && std::is_trivial_v< DataType >, bool > = true
		>
		inline bool GetIniData( const path_t& file, const string_t& section, const string_t& name, DataType& data )
		{
			return GetIniBinary( file, section, name, &data, sizeof( DataType ) );
		}

		// Not Primitive Data Type 
		template
		<
			typename DataType,
			std::enable_if_t< !std::is_standard_layout_v< DataType > || !std::is_trivial_v< DataType >, bool > = true
		>
		inline bool GetIniData( const path_t& file, const string_t& section, const string_t& name, DataType& data )
		{
			return data.LoadFromIni( std::make_tuple( file, section, name ) );
		}
			
		// unsigned
		template 
		<
			typename Int,
			std::enable_if_t< std::is_unsigned_v< Int >, bool > = true
		>
		inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64u" ), static_cast< uint64_t >( value ) ) );
		}

		// signed
		template 
		<
			typename Int,
			std::enable_if_t< !std::is_unsigned_v< Int >, bool > = true
		>
		inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64" ), static_cast< uint64_t >( value ) ) );
		}

		// qword - 64bit unsigned
		inline void SetIniXword( const path_t& file, const string_t& section, const string_t& name, qword value )
		{
			SetIniString( file, section, name, strprintf( _T( "0x%016llX" ), value ) );
		}

		// dword - 32bit unsigned
		inline void SetIniXword( const path_t& file, const string_t& section, const string_t& name, dword value )
		{
			SetIniString( file, section, name, strprintf( _T( "0x%08lX" ), value ) );
		}

		// Primitive Data Type 
		template
		<
			typename DataType,
			std::enable_if_t< std::is_standard_layout_v< DataType > && std::is_trivial_v< DataType >, bool > = true
		>
		inline void SetIniData( const path_t& file, const string_t& section, const string_t& name, const DataType& data )
		{
			SetIniBinary( file, section, name, &data, sizeof( DataType ) );
		}
			
		// Not Primitive Data Type
		template
		<
			typename DataType,
			std::enable_if_t< !std::is_standard_layout_v< DataType > || !std::is_trivial_v< DataType >, bool > = true
		>
		inline void SetIniData( const path_t& file, const string_t& section, const string_t& name, const DataType& data )
		{
			data.SaveToIni( std::make_tuple( file, section, name ) );
		}
	}

	template < typename Int >
	inline Int GetIniInt( const path_t& file, const string_t& section, const string_t& name )
	{
		return Details::GetIniInt< Int >( file, section, name );
	}

	template < typename Xword >
	inline Xword GetXword( const path_t& file, const string_t& section, const string_t& name )
	{
		return Details::GetIniXword< Xword >( file, section, name );
	}

	template < typename FloatType >
	inline FloatType GetIniFloat( const path_t& file, const string_t& section, const string_t& name )
	{
		return static_cast< FloatType >( Details::GetIniDouble( file, section, name ) );
	}

	template < typename DataType >
	inline bool GetIniData( const path_t& file, const string_t& section, const string_t& name, DataType& data )
	{
		return Details::GetIniData( file, section, name, data );
	}

	template < typename Int >
	inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
	{
		Details::SetIniInt( file, section, name, value );
	}

	template < typename FloatType >
	inline void SetIniFloat( const path_t& file, const string_t& section, const string_t& name, FloatType value )
	{
		SetIniString( file, section, name, strprintf( _T( "%lf" ), static_cast< double >( value ) ) );
	}

	template < typename DataType >
	inline void SetIniData( const path_t& file, const string_t& section, const string_t& name, const DataType& data )
	{
		Details::SetIniData( file, section, name, data );
	}

	template < typename Xword >
	inline void SetIniXword( const path_t& file, const string_t& section, const string_t& name, Xword value )
	{
		Details::SetIniXword( file, section, name, value );
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
