#include "MyCpp/Win32System.hpp"
#include "MyCpp/Win32SafeHandle.hpp"
#include "MyCpp/Win32Memory.hpp"
#include "MyCpp/Error.hpp"
#include "MyCpp/StringUtils.hpp"
#include "MyCpp/IntCast.hpp"

#include <boost/range/algorithm/find.hpp>

#include <Objbase.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <tlhelp32.h>
#include <psapi.h>

#if ( PSAPI_VERSION < 2 )
#pragma comment( lib, "psapi.lib" )
#endif

#pragma comment( lib, "advapi32.lib" )
#pragma comment( lib, "user32.lib" )
#pragma comment( lib, "shell32.lib" )
#pragma comment( lib, "ole32.lib" )

#define REGVALUE_ERROR( calledFunction, key, value, errorCode ) \
	_T( "[%s()] %s(\"%s\\%s\") Failed : 0x%08x" ), _T( __FUNCTION__ ), _T( calledFunction ), ( key ), ( value ), ( errorCode )

namespace MyCpp
{
	constexpr DWORD PROCESS_GET_INFO = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

	path_t GetProgramModuleFileName( HMODULE hmodule )
	{
		vchar_t buffer( MAX_PATH );

		do_read( buffer, buffer.size(),
			[&] ( LPTSTR buffer, std::size_t n )
		{
			return ::GetModuleFileName( hmodule, buffer, numeric_cast< DWORD >( n ) );
		} );

		return cstr_t( buffer );
	}

	path_t GetProgramModuleFileLocation( HMODULE hmodule )
	{
		return GetProgramModuleFileName( hmodule ).parent_path();
	}

	path_t GetTemporaryPath()
	{
		vchar_t buffer( MAX_PATH );

		do_read( buffer, buffer.size(),
			[&] ( LPTSTR buffer, std::size_t n )
		{
			return ::GetTempPath( numeric_cast< DWORD >( n ), buffer );
		} );

		return cstr_t( buffer );
	}

	path_t GetTemporaryFileName( const string_t& prefix )
	{
		string_t tempDir = to_string_t( GetTemporaryPath() );
		vchar_t buffer( tempDir.length() + prefix.length() + 10 );

		::GetTempFileName( tempDir.c_str(), prefix.c_str(), 0, cstr_t( buffer ) );

		return cstr_t( buffer );
	}

	string_t ToGuidString( const GUID& guid )
	{
		vwchar guidStr( 40 );

		do_read( guidStr, guidStr.size(),
			[&] ( LPWSTR buffer, std::size_t n )
		{
			int r = ::StringFromGUID2( guid, buffer, numeric_cast< int >( n ) );

			return ( r > 0 ) ? r : n;
		} );

		return narrow_wide_string< string_t, std::wstring >( cstr_t( guidStr ) );
	}

	SPSID GetProcessSid( HANDLE process )
	{
		typedef SPSID::element_type SID;

		HANDLE token = null;

		if ( ::OpenProcessToken( process, TOKEN_QUERY, &token ) )
		{
			DWORD bytes;
			ScopedGenericHandle processToken( token );
			::GetTokenInformation( processToken.get(), TokenUser, null, 0, &bytes );

			ScopedLocalMemory< TOKEN_USER > tokenUser( LocalAllocate< TOKEN_USER >( LPTR, bytes ) );
			if ( ::GetTokenInformation( processToken.get(), TokenUser, tokenUser.get(), bytes, &bytes ) )
			{
				if ( ::IsValidSid( tokenUser->User.Sid ) )
				{
					DWORD sidLength = ::GetLengthSid( tokenUser->User.Sid );
					ScopedLocalMemory< SID > sid( LocalAllocate< SID >( LPTR, sidLength ) );

					::CopySid( sidLength, sid.get(), tokenUser->User.Sid );

					if ( ::IsValidSid( sid.get() ) )
						return { sid.release(), LocalMemoryDeleter< SID >() };
				}
			}
		}

		exception< std::runtime_error >( FUNC_ERROR_MSG( "GetProcessSid", "Failed: 0x%08x", ::GetLastError() ) );

		return null;
	}

	SPPROCESS OpenCuProcessByFileName( const path_t& fileName, bool inheritHandle, DWORD accessMode )
	{
		SPPROCESS process = OpenProcessByFileName( fileName, inheritHandle, accessMode );

		if ( process )
		{
			SPSID current( GetProcessSid( ::GetCurrentProcess() ) );
			SPSID target( GetProcessSid( process->GetHandle() ) );

			if ( ::EqualSid( current.get(), target.get() ) )
				return process;
		}

		return null;
	}

	SPPROCESS OpenProcessByFileName( const path_t& fileName, bool inheritHandle, DWORD accessMode )
	{
		DWORD maxIndex = 0;
		std::vector< DWORD > pids( 200 );

		do_read( pids, pids.size(),
			[&] ( DWORD* pn, std::size_t n )
		{
			DWORD size;

			::EnumProcesses( pn, numeric_cast< DWORD >( n * sizeof( DWORD ) ), &size );

			size /= sizeof( DWORD );

			if ( size < n )
				maxIndex = size;

			return size;
		} );

		vchar_t exeFileName( MAX_PATH );
		string_t searchExeName = to_string_t( fileName );

		for ( DWORD i = 0; i < maxIndex; ++i )
		{
			if ( HANDLE p = ::OpenProcess( accessMode | PROCESS_GET_INFO, ( inheritHandle ) ? TRUE : FALSE, pids[i] ) )
			{
				ScopedGenericHandle process( p );

				do_read( exeFileName, exeFileName.size(),
					[&] ( char_t* s, std::size_t n )
				{
					DWORD size = numeric_cast< DWORD >( n );
					if ( ::QueryFullProcessImageName( process.get(), 0, s, &size ) )
						return size + 1;

					return 0UL;
				} );

				if ( ::_tcsicmp( cstr_t( exeFileName ), searchExeName.c_str() ) == 0 )
					return GetProcess( process.release() );
			}
		}

		return null;
	}

	namespace
	{
		void ComInitialize()
		{
			thread_local
			struct CoInitializer
			{
				CoInitializer() 
				{
					m_result = ::CoInitialize( null );
				}
				~CoInitializer()
				{
					if ( SUCCEEDED( m_result ) )
						::CoUninitialize();
				}
			private:
				HRESULT m_result;
			}
			comInitializer;
		}

		template < typename T >
		struct CoMemoryDeleter
		{
			typedef T* pointer;
			void operator () ( pointer p )
			{
				if ( p != null )
					::CoTaskMemFree( p );
			}
		};
	}

	path_t GetSpecialFolderLocation( const GUID& folderId )
	{
		ComInitialize();

		LPWSTR pstr;
		HRESULT r = ::SHGetKnownFolderPath( folderId, 0, null, &pstr );

		ScopedMemory< wchar_t, CoMemoryDeleter< wchar_t > > str( pstr );

		if ( FAILED( r ) )
			exception< std::runtime_error >( FUNC_ERROR_ID( "SHGetKnownFolderPath", r ) );

		return str.get();
	}

	path_t GetCurrentLocation()
	{
		vchar_t result( MAX_PATH );

		do_read( result, result.size(),
			[] ( LPTSTR buffer, std::size_t n )
		{
			return ::GetCurrentDirectory( numeric_cast< DWORD >( n ), buffer );
		} );

		return cstr_t( result );
	}

	std::pair< DWORD, MUTEX > TryLockMutex( const string_t& name, bool waitForGetOwnership )
	{
		MUTEX mutex( ::CreateMutex( null, TRUE, name.c_str() ) );
		DWORD lastError = ::GetLastError();

		if ( mutex 
			&& lastError == ERROR_ALREADY_EXISTS 
			&& waitForGetOwnership )
		{
			::WaitForSingleObject( mutex.get(), INFINITE );
			lastError = ::GetLastError();
		}

		return std::make_pair( lastError, std::move( mutex ) );
	}

	namespace
	{
		struct CriticalSectionDeleter
		{
			typedef LPCRITICAL_SECTION pointer;

			void operator () ( pointer p )
			{
				if ( p != null )
				{
					::DeleteCriticalSection( p );
					::LocalFree( reinterpret_cast< HLOCAL >( p ) );
				}
			}
		};
	}

	CRITICAL_SECTION_LOCK TryLockCriticalSection( CRITICAL_SECTION& csObj )
	{
		::EnterCriticalSection( &csObj );

		CRITICAL_SECTION_LOCK lock( &csObj );

		return std::move( lock );
	}


	SPCRITICAL_SECTION CreateCriticalSection( std::uint32_t spinCount )
	{
#ifdef _DEBUG
		constexpr DWORD FLAGS = 0;
#else
		constexpr DWORD FLAGS = CRITICAL_SECTION_NO_DEBUG_INFO;
#endif

		if ( spinCount == CSSPINCOUNT::AUTO )
			spinCount = 4000;

		LPCRITICAL_SECTION newCriticalSection = LocalAllocate< CRITICAL_SECTION >( LPTR, sizeof( CRITICAL_SECTION ) );

		::InitializeCriticalSectionEx( newCriticalSection, spinCount, FLAGS );

		return { newCriticalSection, CriticalSectionDeleter() };
	}

	class Process::Data
	{
	public:
		typedef std::pair< HANDLE, DWORD > HANDLEID;

		Data();
		Data( Data&& right );
		Data( const PROCESS_INFORMATION& info );

		~Data();

		const HANDLEID& GetPrimaryThreadData() const;
		const HANDLEID& GetProcessData() const;
		const path_t& GetProcessFileName() const;
		const string_t& GetProcessBaseName() const;
	private:
		HANDLEID m_process = { null, 0 };
		mutable HANDLEID m_priThread = { null, 0 };
		mutable path_t m_fileName;
		mutable string_t m_baseName;
	};

	namespace
	{
		Process::Data::HANDLEID GetPrimaryThreadHandleId( DWORD processId )
		{
			ScopedGenericHandle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, processId ) );

			if ( snapshot )
			{
				THREADENTRY32 thinfo;
				thinfo.dwSize = Fill0( thinfo );

				if ( ::Thread32First( snapshot.get(), &thinfo ) )
				{
					HANDLE threadHandle = ::OpenThread( SYNCHRONIZE | THREAD_QUERY_INFORMATION, FALSE, thinfo.th32ThreadID );
					if ( threadHandle != null )
						return std::make_pair( threadHandle, thinfo.th32ThreadID );
				}
			}

			return null;
		}
	}

	inline Process::Data::Data()
	{}

	inline Process::Data::Data( Data&& right )
	{
		m_process.swap( right.m_process );
		m_priThread.swap( right.m_priThread );
		m_fileName.swap( right.m_fileName );
		m_baseName.swap( right.m_baseName );
	}

	inline Process::Data::Data( const PROCESS_INFORMATION& info )
		: m_process( std::make_pair( info.hProcess, info.dwProcessId ) )
		, m_priThread( std::make_pair( info.hThread, info.dwThreadId ) )
	{}

	inline Process::Data::~Data()
	{
		if ( m_process.first != null
			&& m_process.first != ::GetCurrentProcess() )
			::CloseHandle( m_process.first );

		if ( m_priThread.first != null
			&& m_priThread.first != ::GetCurrentThread() )
			::CloseHandle( m_priThread.first );
	}

	inline const Process::Data::HANDLEID& Process::Data::GetPrimaryThreadData() const
	{
		if ( m_priThread.first == null )
			m_priThread = GetPrimaryThreadHandleId( m_process.second );

		return m_priThread;
	}

	inline const Process::Data::HANDLEID& Process::Data::GetProcessData() const
	{
		return m_process;
	}

	inline const path_t& Process::Data::GetProcessFileName() const
	{
		if ( m_process.first != null && m_fileName.empty() )
		{
			vchar_t buffer( MAX_PATH );

			do_read( buffer, buffer.size(),
				[&] ( char_t* s, std::size_t n )
			{
				DWORD size = numeric_cast< DWORD >( n );
				if ( ::QueryFullProcessImageName( m_process.first, 0, s, &size ) )
					return size + 1;

				return 0UL;
			} );

			m_fileName = cstr_t( buffer );
		}

		return m_fileName;
	}

	inline const string_t& Process::Data::GetProcessBaseName() const
	{
		if ( m_process.first != null && m_baseName.empty() )
			m_baseName = to_string_t( GetProcessFileName().filename() );

		return m_baseName;
	}


	Process::Process()
		: m_data( std::make_shared< Data >() )
	{}

	Process::Process( Data&& data )
		: m_data( std::make_shared< Data >( std::move( data ) ) )
	{}

	Process::~Process()
	{}

	string_t Process::GetName() const
	{
		return m_data->GetProcessBaseName();
	}

	path_t Process::GetFileName() const
	{
		return m_data->GetProcessFileName();
	}

	HANDLE Process::GetHandle() const
	{
		return m_data->GetProcessData().first;
	};

	HANDLE Process::GetPrimaryThreadHandle() const
	{
		return m_data->GetPrimaryThreadData().first;
	}

	DWORD Process::GetId() const
	{
		return m_data->GetProcessData().second;
	}

	DWORD Process::GetPrimaryThreadId() const
	{
		return m_data->GetPrimaryThreadData().second;
	}

	DWORD Process::GetExitCode() const
	{
		DWORD exitCode;

		::GetExitCodeProcess( m_data->GetProcessData().first, &exitCode );

		return exitCode;
	}

	void Process::Terminate( int exitCode )
	{
		::TerminateProcess( m_data->GetProcessData().first, exitCode );
	}

	namespace
	{
		void SuspendProcess( DWORD processId, bool suspend )
		{
			ScopedGenericHandle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, processId ) );

			if ( snapshot )
			{
				THREADENTRY32 thinfo;
				thinfo.dwSize = Fill0( thinfo );

				if ( ::Thread32First( snapshot.get(), &thinfo ) )
				{
					do
					{
						ScopedGenericHandle thread( ::OpenThread( THREAD_SUSPEND_RESUME, FALSE, thinfo.th32ThreadID ) );
						if ( thread )
						{
							if ( suspend )
								::SuspendThread( thread.get() );
							else
								::ResumeThread( thread.get() );
						}
					}
					while ( ::Thread32Next( snapshot.get(), &thinfo ) );
				}
			}
		}
	}

	void Process::Suspend()
	{
		SuspendProcess( m_data->GetProcessData().second, true );
	}

	void Process::Resume()
	{
		SuspendProcess( m_data->GetProcessData().second, false );
	}

	DWORD Process::Wait( DWORD milliseconds, bool forInputIdle ) const
	{
		if ( forInputIdle )
			return ::WaitForInputIdle( m_data->GetProcessData().first, milliseconds );
		else
			return ::WaitForSingleObject( m_data->GetProcessData().first, milliseconds );
	}

	SPPROCESS GetProcess( DWORD pid )
	{
		std::vector< DWORD > pids( 200 );

		do_read( pids, pids.size(),
			[&] ( DWORD* pn, std::size_t n )
		{
			DWORD size;

			::EnumProcesses( pn, numeric_cast< DWORD >( n * sizeof( DWORD ) ), &size );

			size /= sizeof( DWORD );

			return size;
		} );

		if ( boost::find( pids, pid ) == pids.end() )
			return null;

		HANDLE openedProcess = ::OpenProcess( PROCESS_GET_INFO, FALSE, pid );
		if ( openedProcess == null )
			return null;

		return std::make_shared< Process >( 
			std::move( Process::Data( { openedProcess, null, pid, 0 } ) ) );
	}

	SPPROCESS GetProcess( HANDLE hProcess )
	{
		return std::make_shared< Process >( 
			std::move( Process::Data( { hProcess, null, ::GetProcessId( hProcess ), 0 } ) ) );
	}

	SPPROCESS GetParentProcess()
	{
		ScopedGenericHandle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );

		if ( snapshot )
		{
			DWORD currentProcessId = ::GetCurrentProcessId();

			PROCESSENTRY32 processEntry;
			processEntry.dwSize = Fill0( processEntry );

			if ( ::Process32First( snapshot.get(), &processEntry ) )
			{
				do
				{
					if ( processEntry.th32ProcessID == currentProcessId )
					{
						HANDLE process = ::OpenProcess( PROCESS_GET_INFO, FALSE, processEntry.th32ParentProcessID );

						if ( process == null )
							return null;

						return std::make_shared< Process >(
							std::move( Process::Data( { process, null, processEntry.th32ParentProcessID, 0 } ) ) );
					}
				}
				while ( ::Process32Next( snapshot.get(), &processEntry ) );
			}
		}

		return null;
	}

	SPPROCESS StartProcess( const string_t& cmdline, const path_t& appCurrentDir, void* envVariables, int creationFlags, bool inheritHandle,  int cmdShow )
	{
		bool inQuote = false;
		bool isQuotedName = false;

		string_t appName;

		auto i = cmdline.begin();

		for ( ; i != cmdline.end(); ++i )
		{
			if ( *i == _T( '"' ) )
			{
				inQuote = ( !inQuote );
				if ( !isQuotedName )
					isQuotedName = true;

				continue;
			}
			else if ( !inQuote && _istspace( *i ) )
			{
				break;
			}

			appName += *i;
		}

		path_t appCurrent = ( !appCurrentDir.empty() ) ? 
			std::filesystem::absolute( appCurrentDir ) : GetCurrentLocation();

		appName = to_string_t( std::filesystem::absolute( appName ) );

		if ( !isQuotedName )
		{
			auto r = std::find_if( appName.begin(), appName.end(), &_istspace );
			if ( r != appName.end() )
				isQuotedName = true;
		}

		vchar_t cmdLineArgs;

		if ( isQuotedName )
			cmdLineArgs.push_back( _T( '"' ) );

		std::copy( appName.begin(), appName.end(), std::back_inserter< vchar_t >( cmdLineArgs ) );

		if ( isQuotedName )
			cmdLineArgs.push_back( _T( '"' ) );

		std::copy( i, cmdline.end(), std::back_inserter< vchar_t >( cmdLineArgs ) );

		cmdLineArgs.push_back( char_t() );

		STARTUPINFO si;

		si.cb = Fill0( si );
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = cmdShow;

		PROCESS_INFORMATION pi;

		BOOL result = ::CreateProcess(
			appName.c_str()
			, cstr_t( cmdLineArgs )
			, null
			, null
			, ( inheritHandle ) ? TRUE : FALSE
			, creationFlags
			, envVariables
			, to_string_t( appCurrent ).c_str()
			, &si
			, &pi );

		if ( result == FALSE )
			exception< std::runtime_error >( FUNC_ERROR_MSG( "CreateProcess", "Failed. CommandLine = '%s', (0x%08x)", cmdLineArgs, ::GetLastError() ) );

		return std::make_shared< Process >( 
			std::move( Process::Data( pi ) ) );
	}

	int RunElevated( const path_t& file, const string_t& parameters, bool waitForExit, int cmdShow )
	{
		path_t filePath = std::filesystem::absolute( file );

		string_t fileInfo = to_string_t( filePath.filename() );
		string_t directoryInfo = to_string_t( filePath.parent_path() );

		SHELLEXECUTEINFO shei;

		shei.cbSize = Fill0( shei );
		shei.lpVerb = _T( "runas" );
		shei.lpFile = fileInfo.c_str();
		shei.lpDirectory = directoryInfo.c_str();
		shei.lpParameters = parameters.c_str();
		shei.nShow = cmdShow;

		if ( waitForExit )
			shei.fMask |= SEE_MASK_NOCLOSEPROCESS;

		::ShellExecuteEx( &shei );

		if ( shei.hProcess != null )
		{
			::WaitForSingleObject( shei.hProcess, INFINITE );

			DWORD dwExitCode;
			::GetExitCodeProcess( shei.hProcess, &dwExitCode );

			::CloseHandle( shei.hProcess );

			return static_cast< int >( dwExitCode );
		}

		int result = pointer_int_cast< int >( shei.hInstApp );

		return ( result <= 32 ) ? result : 0;
	}

	namespace
	{
		constexpr std::size_t BUFFERSIZE = 256;

		struct FINDWINDOWINFO
		{
			HWND hwnd;
			DWORD pid;
			LPCTSTR className;
			LPCTSTR windowName;
			vchar_t* pWorkBuffer;
		};

		inline std::size_t GetWindowClassName( HWND hwnd, vchar_t& name )
		{
			return do_read( name, BUFFERSIZE,
				[&] ( LPTSTR s, std::size_t n )
			{
				return ::GetClassName( hwnd, s, numeric_cast< int >( n ) );
			} );
		}

		inline std::size_t GetWindowName( HWND hwnd, vchar_t& name )
		{
			std::size_t size = ::GetWindowTextLength( hwnd ) + 1;

			if ( name.size() < size )
				name.resize( size );

			return ::GetWindowText( hwnd, cstr_t( name ), numeric_cast< int >( name.size() ) );
		}

		BOOL CALLBACK EnumWindowsProc(
			HWND hwnd,      // 親ウィンドウのハンドル
			LPARAM lParam   // アプリケーション定義の値
			)
		{
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );
			vchar_t& buffer = *p->pWorkBuffer;

			GetWindowClassName( hwnd, buffer );

			if ( ::_tcsicmp( cstr_t( buffer ), p->className ) == 0 )
			{
				GetWindowName( hwnd, buffer );

				if ( ::_tcsicmp( cstr_t( buffer ), p->windowName ) == 0 )
				{
					DWORD pid;

					::GetWindowThreadProcessId( hwnd, &pid );

					if ( pid == p->pid )
					{
						p->hwnd = hwnd;

						return FALSE;
					}
				}
			}

			return TRUE;
		}
	}

	HWND FindProcessWindow( const SPPROCESS& process, const string_t& wndClassName, const string_t& wndName )
	{
		if ( !process )
			return null;

		vchar_t buffer( BUFFERSIZE );
		FINDWINDOWINFO fwi = { null, process->GetId(), wndClassName.c_str(), wndName.c_str(), &buffer };

		::EnumWindows( EnumWindowsProc, pointer_int_cast< LPARAM >( &fwi ) );

		return fwi.hwnd;
	}

	const Process* GetCurrentProcess()
	{
		static Process currentProcess(
			std::move( Process::Data( { ::GetCurrentProcess(), null, ::GetCurrentProcessId(), 0 } ) ) );

		return &currentProcess;
	}

	std::uint32_t GetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, void* ptr, std::uint32_t size )
	{
		HKEY hk;

		LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
		if ( r == ERROR_SUCCESS )
		{
			DWORD returnedSize;
			ScopedRegHandle regHandle( hk );

			r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, null, &returnedSize );
			if ( r == ERROR_SUCCESS  )
			{
				ScopedLocalMemory< BYTE > buffer;
				BYTE* p = reinterpret_cast< BYTE* >( ptr );
				
				if ( returnedSize > size )
				{
					buffer.reset( LocalAllocate< BYTE >( LPTR, returnedSize ) );
					p = buffer.get();
				}

				r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, p, &returnedSize );
				if ( r == ERROR_SUCCESS )
				{
					std::uint32_t n = std::min( static_cast< DWORD >( size ), returnedSize );

					if ( p != ptr )
						std::memcpy( ptr, p, n );

					return n;
				}
			}
		}

		return 0;
	}

	string_t GetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		HKEY hk;

		LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
		if ( r == ERROR_SUCCESS )
		{
			DWORD size;
			ScopedRegHandle regHandle( hk );

			r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, null, &size );
			if ( r == ERROR_SUCCESS )
			{
				DWORD dataType;
				std::vector< BYTE > buffer( size );

				r = ::RegQueryValueEx( hk, valueName.c_str(), null, &dataType, buffer.data(), &size );
				if ( r == ERROR_SUCCESS )
				{
					switch ( dataType )
					{
					case REG_DWORD:
						return strprintf( _T( "%I32u" ), *reinterpret_cast< DWORD* >( buffer.data() ) );

					case REG_QWORD:
						return strprintf( _T( "%I64u" ), *reinterpret_cast< DWORD64* >( buffer.data() ) );

					case REG_SZ:
					case REG_EXPAND_SZ:
						return { reinterpret_cast< char_t* >( buffer.data() ), ( buffer.size() / sizeof( char_t ) ) - 1 };

					default:
						;
					}

					return { buffer.begin(), buffer.end() };
				}
			}
		}

		exception< std::runtime_error >( REGVALUE_ERROR( "GetRegString", subKey, valueName, r ) );

		return null;
	}

	void SetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName, const string_t& value )
	{
		HKEY hk;
		DWORD disposition;

		LSTATUS r = ::RegCreateKeyEx( parentKey, subKey.c_str(), 0, null, REG_OPTION_NON_VOLATILE, KEY_WRITE, null, &hk, &disposition );
		if ( r == ERROR_SUCCESS )
		{
			ScopedRegHandle regHandle( hk );

			r = ::RegSetValueEx( hk, valueName.c_str(), 0, REG_SZ, reinterpret_cast< const BYTE* >( value.c_str() ), numeric_cast< DWORD >( value.length() ) );
			if ( r == ERROR_SUCCESS )
				return ;
		}

		exception< std::runtime_error >( REGVALUE_ERROR( "SetRegString", subKey, valueName, r ) );
	}

	void SetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, uint32_t type, const void* ptr, uint32_t size )
	{
		HKEY hk;
		DWORD disposition;

		LSTATUS r = ::RegCreateKeyEx( parentKey, subKey.c_str(), 0, null, REG_OPTION_NON_VOLATILE, KEY_WRITE, null, &hk, &disposition );
		if ( r == ERROR_SUCCESS )
		{
			ScopedRegHandle regHandle( hk );

			r = ::RegSetValueEx( hk, valueName.c_str(), 0, type, reinterpret_cast< const BYTE* >( ptr ), size );
			if ( r == ERROR_SUCCESS )
				return;
		}

		exception< std::runtime_error >( REGVALUE_ERROR( "SetRegBinary", subKey, valueName, r ) );
	}

	string_t GetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& defaultValue )
	{
		vchar_t buffer( 512 );

		do_read( buffer, 512, 
			[&] ( LPTSTR s, std::size_t n ) 
		{
			DWORD r = ::GetPrivateProfileString(
				section.c_str()
				, name.c_str()
				, defaultValue.c_str()
				, s
				, numeric_cast< DWORD >( n )
				, to_string_t( file ).c_str() );

			return ( r == n - 1 ) ? r + 1 : r;
		} );

		return cstr_t( buffer );
	}

	void SetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& value )
	{
		LPCTSTR pname = ( !name.empty() ) ? name.c_str() : null;
		LPCTSTR pvalue = ( !value.empty() ) ? value.c_str() : null;

		::WritePrivateProfileString( section.c_str(), pname, pvalue, to_string_t( file ).c_str() );
	}
}
