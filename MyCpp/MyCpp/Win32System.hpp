#pragma once
#include <cstring>
#include <filesystem>
#include "MyCpp/StringUtils.hpp"
#include "MyCpp/Win32SafeHandle.hpp"

namespace MyCpp
{
	typedef std::filesystem::path path_t;
	typedef UINT_PTR ptrint_t;
	typedef DWORD_PTR ptrlong_t;
	typedef GUID guid_t;

	inline string_t to_string_t( const path_t& p )
	{
		if constexpr ( std::is_same_v< string_t, path_t::string_type > )
			return p.native();
		else
			return p.string< char_t >();
	}

	typedef std::shared_ptr< CRITICAL_SECTION > csptr_t;

	constexpr uint CSSPIN_AUTO = 4000;

	csptr_t CreateCriticalSection( uint spinCount = CSSPIN_AUTO );

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
			typedef handle_t pointer;

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

	typedef scoped_handle_t< HANDLE, Details::MutexDeleter > mutex_t;
	typedef std::unique_ptr< CRITICAL_SECTION, Details::CriticalSectionExit > cslock_t;

	std::pair< dword, mutex_t > TryLockMutex( const string_t& name, bool waitForGetOwnership );
	cslock_t TryLockCriticalSection( CRITICAL_SECTION& csObj );

	inline cslock_t TryLockCriticalSection( csptr_t& csObj )
	{
		return TryLockCriticalSection( *csObj );
	}

	template 
	<
		typename T,
		std::enable_if_t< std::is_standard_layout_v< T >&& std::is_trivial_v< T >, bool > = true,
		dword Size = sizeof( T )
	>
	inline dword Fill0( T& podStruct )
	{
		::ZeroMemory( &podStruct, Size );
		return Size;
	}

	template < typename Vector, typename Source >
	inline std::size_t adaptive_load( Vector& v, std::size_t initn, Source source )
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

	typedef std::shared_ptr< std::remove_pointer< PSID >::type > sidptr_t;

	sidptr_t GetProcessSid( handle_t process );

	path_t GetCurrentLocation();
	path_t GetSpecialFolderLocation( const guid_t& folderId );
	path_t GetTemporaryPath();
	path_t GetTemporaryFileName( const string_t& prefix );
	path_t FindFilePath( const string_t& filename, const string_t& ext = null );

	string_t ToGuidString( const guid_t& guid );

	string_t GetRegString( hkey_t parentKey, const string_t& subKey, const string_t& valueName );
	uint GetRegBinary( hkey_t parentKey, const string_t& subKey, const string_t& valueName, void* ptr, uint size );
	dword GetRegDword( hkey_t parentKey, const string_t& subKey, const string_t& valueName );
	qword GetRegQword( hkey_t parentKey, const string_t& subKey, const string_t& valueName );
	void SetRegString( hkey_t parentKey, const string_t& subKey, const string_t& valueName, const string_t& value );
	void SetRegBinary( hkey_t parentKey, const string_t& subKey, const string_t& valueName, uint type, const void* ptr, uint size );

	namespace Details
	{
		// qword - 64bit unsigned
		template < typename Xword >
		inline
		std::enable_if_t< std::is_same_v< Xword, qword >, Xword >
		GetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
		{
			return GetRegDword( parentKey, subKey, valueName );
		}

		// dword - 32bit unsigned
		template < typename Xword >
		inline
		std::enable_if_t< std::is_same_v< Xword, dword >, Xword >
		GetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
		{
			return GetRegQword( parentKey, subKey, valueName );
		}
	}

	template < typename Xword >
	inline Xword GetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
	{
		return Details::GetRegXword< Xword >( parentKey, subKey, valueName );
	}

	inline void SetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName, dword value )
	{
		SetRegBinary( parentKey, subKey, valueName, REG_DWORD, &value, sizeof( dword ) );
	}

	inline void SetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName, qword value )
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
		handle_t GetHandle() const;
		handle_t GetPrimaryThreadHandle() const;
		dword GetId() const;
		dword GetPrimaryThreadId() const;
		dword GetExitCode() const;

		void Terminate( int exitCode );
		void Suspend();
		void Resume();

		dword Wait( dword milliseconds = INFINITE, bool forInputIdle = false ) const;
	private:
		std::shared_ptr< Data > m_data;
	};

	typedef std::shared_ptr< Process > processptr_t;

	inline sidptr_t GetProcessSid( const processptr_t& process )
	{
		return GetProcessSid( process->GetHandle() );
	}

	class Window
	{
	public:
		typedef HWND native_handle_t;

		Window() = default;
		Window( const Window& ) = default;
		Window( Window&& ) = default;
		Window( native_handle_t hwnd );

		~Window() = default;

		Window& operator = ( const Window& ) = default;
		Window& operator = ( Window&& ) = default;
		Window& operator = ( native_handle_t hwnd );

		operator native_handle_t () const
		{
			return GetHandle();
		}

		string_t Text() const;
		string_t Text( const string_t& text);

		string_t ClassName() const;

		ptrlong_t Send( uint msg, ptrint_t wparam, ptrlong_t lparam );
		ptrlong_t SendNotify( uint msg, ptrint_t wparam, ptrlong_t lparam );
		ptrlong_t SendTimeout( uint msg, ptrint_t wparam, ptrlong_t lparam, uint flags, uint milliseconds );
		ptrlong_t Post( uint msg, ptrint_t wparam, ptrlong_t lparam );

		Window GetParent() const;

		void Close();

		native_handle_t GetHandle() const
		{
			return m_hwnd;
		}
	private:
		native_handle_t m_hwnd = null;
	};

	Window FindProcessWindow( const processptr_t& process, const string_t& wndClassName, const string_t& wndName );

	processptr_t GetProcess( dword pid );
	processptr_t GetProcess( handle_t hProcess );
	processptr_t GetParentProcess();

	const Process* GetCurrentProcess();

	processptr_t OpenProcessByFileName( const path_t& fileName, bool inheritHandle = false, dword accessMode = 0 );
	processptr_t OpenCuProcessByFileName( const path_t& fileName, bool inheritHandle = false, dword accessMode = 0 );

	processptr_t StartProcess( const string_t& cmdline, const path_t& appCurrentDir = null, void* envVariables = null, int creationFlags = 0, bool inheritHandle = false,  int cmdShow = SW_SHOWDEFAULT );

	int RunElevated( const path_t& file, const string_t& parameters = null, bool waitForExit = true, int cmdShow = SW_SHOWDEFAULT );

	string_t GetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& defaultValue = null );
	bool GetIniBinary( const path_t& file, const string_t& section, const string_t& name, void* ptr, uint size );
	void SetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& value );
	void SetIniBinary( const path_t& file, const string_t& section, const string_t& name, const void* ptr, uint size );

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

		// unsigned
		template 
		<
			typename Int,
			std::enable_if_t< std::is_unsigned_v< Int >, bool > = true
		>
		inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64u" ), static_cast< std::uint64_t >( value ) ) );
		}

		// signed
		template 
		<
			typename Int,
			std::enable_if_t< !std::is_unsigned_v< Int >, bool > = true
		>
		inline void SetIniInt( const path_t& file, const string_t& section, const string_t& name, Int value )
		{
			SetIniString( file, section, name, strprintf( _T( "%I64" ), static_cast< std::int64_t >( value ) ) );
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
	}

	template < typename Int >
	inline Int GetIniInt( const path_t& file, const string_t& section, const string_t& name )
	{
		return Details::GetIniInt< Int >( file, section, name );
	}

	template < typename Xword >
	inline Xword GetIniXword( const path_t& file, const string_t& section, const string_t& name )
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
using MyCpp::ptrint_t;
using MyCpp::ptrlong_t;
using MyCpp::mutex_t;
using MyCpp::csptr_t;
using MyCpp::cslock_t;
using MyCpp::processptr_t;
using MyCpp::sidptr_t;
#endif
