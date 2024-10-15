#include "MyCpp/Win32System.hpp"
#include "MyCpp/Error.hpp"
#include "MyCpp/IntCast.hpp"

#include <cstdlib>

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
	namespace
	{
		constexpr dword PROCESS_STANDARD_RIGHTS = PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE;
		constexpr dword PROCESS_LIMITED_RIGHTS = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE;

		handle_t DuplicatePrimaryThreadHandle();
			
		// Global variables are initialized at program startup,
		// so the primary thread id should be returned.
		const dword PROCESS_PRIMARY_THREAD_ID = ::GetCurrentThreadId();
		const handle_t PROCESS_PRIMARY_THREAD_HANDLE = DuplicatePrimaryThreadHandle();

		void CloseDuplicatedPrimaryThreadHandle()
		{
			::CloseHandle( PROCESS_PRIMARY_THREAD_HANDLE );
		}

		handle_t DuplicatePrimaryThreadHandle()
		{
			handle_t hPrimaryThread = null;

			if ( ::GetCurrentThreadId() != PROCESS_PRIMARY_THREAD_ID )
				exception< std::logic_error >( ERROR_MSG( "Do not call this function except at initialization time." ) );

			BOOL r = ::DuplicateHandle( ::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(), &hPrimaryThread, 0, TRUE, DUPLICATE_SAME_ACCESS );
			if ( r == FALSE )
				exception< std::runtime_error >( FUNC_ERROR_ID( "DuplicateHandle", ::GetLastError() ) );

			std::atexit( &CloseDuplicatedPrimaryThreadHandle );

			return hPrimaryThread;
		}
	}

	path_t GetProgramModuleFileName( module_handle_t hmodule )
	{
		vchar_t buffer( MAX_PATH );

		adaptive_load( buffer, buffer.size(),
			[&hmodule] ( char_t* buffer, std::size_t n )
		{
			return ::GetModuleFileName( hmodule, buffer, numeric_cast< dword >( n ) );
		} );

		return cstr_t( buffer );
	}

	path_t GetProgramModuleFileLocation( module_handle_t hmodule )
	{
		return GetProgramModuleFileName( hmodule ).parent_path();
	}

	path_t GetTemporaryPath()
	{
		vchar_t buffer( MAX_PATH );

		adaptive_load( buffer, buffer.size(),
			[] ( char_t* buffer, std::size_t n )
		{
			return ::GetTempPath( numeric_cast< dword >( n ), buffer );
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

	string_t ToGuidString( const guid_t& guid )
	{
		vwchar guidStr( 40 );

		adaptive_load( guidStr, guidStr.size(),
			[&guid] ( wchar_t* buffer, std::size_t n )
		{
			int r = ::StringFromGUID2( guid, buffer, numeric_cast< int >( n ) );
			return ( r > 0 ) ? r : n;
		} );

		return narrow_wide_string< string_t, std::wstring >( cstr_t( guidStr ) );
	}

	processptr_t OpenCuProcessByFileName( const path_t& fileName, bool inheritHandle, dword accessMode )
	{
		auto process = OpenProcessByFileName( fileName, inheritHandle, accessMode );

		if ( process )
		{
			auto current = Process::GetCurrent()->GetSid();
			auto target = process->GetSid();

			if ( ::EqualSid( current.get(), target.get() ) != FALSE )
				return process;
		}

		return null;
	}

	namespace
	{
		inline path_t ComplatePath( const path_t& p )
		{
			path_t filePath = p;

			if ( filePath.is_absolute() )
				return filePath;

			if ( filePath.has_parent_path() )
				return std::filesystem::weakly_canonical( filePath );

			if ( !filePath.has_extension() )
				filePath.replace_extension( _T( ".exe" ) );

			string_t pstr = to_string_t( p );
			vchar_t szSearchPath( MAX_PATH );

			adaptive_load( szSearchPath, szSearchPath.size(), 
				[&pstr] ( char_t* s, std::size_t n )
			{
				return ::SearchPath( null, pstr.c_str(), null, numeric_cast< dword >( n ), s, null );
			} );

			if ( _tcslen( cstr_t( szSearchPath ) ) == 0 )
				return std::filesystem::weakly_canonical( p );

			return std::filesystem::weakly_canonical( szSearchPath.data() );
		}

		inline handle_t OpenProcessStandardRightsOrLimitedRights( dword dwDesiredAccess, bool bInheritHandle, dword dwProcessId )
		{
			handle_t ph = ::OpenProcess( PROCESS_STANDARD_RIGHTS | dwDesiredAccess, ( bInheritHandle ) ? TRUE : FALSE, dwProcessId );

			if ( ph == null )
				ph = ::OpenProcess( ( dwDesiredAccess & ~PROCESS_STANDARD_RIGHTS ) | PROCESS_LIMITED_RIGHTS, ( bInheritHandle ) ? TRUE : FALSE, dwProcessId );

			return ph;
		}

		inline processptr_t FindProcessByFullPath( const path_t& fileName, bool inheritHandle, dword accessMode )
		{
			dword maxIndex = 0;
			std::vector< dword > pids( 400 );

			adaptive_load( pids, pids.size(),
				[&maxIndex] ( dword* pn, std::size_t n )
			{
				dword size;
				::EnumProcesses( pn, numeric_cast< dword >( n * sizeof( dword ) ), &size );

				size /= sizeof( dword );

				if ( size < n )
					maxIndex = size;

				return size;
			} );

			path_t searchPath = fileName;
			vchar_t szProcessPath( MAX_PATH );

			if ( !searchPath.is_absolute() )
				searchPath = ComplatePath( searchPath );

			string_t searchPathStr = to_string_t( searchPath );

			for ( const auto& pid : pids )
			{
				if ( auto ph = OpenProcessStandardRightsOrLimitedRights( accessMode, inheritHandle, pid ) )
				{
					adaptive_load( szProcessPath, szProcessPath.size(),
						[&ph] ( char_t* s, std::size_t n )
					{
						DWORD size = numeric_cast< DWORD >( n );
						if ( ::QueryFullProcessImageName( ph, 0, s, &size ) != FALSE )
							return size + 1;
						return 0UL;
					} );

					if ( ::_tcsicmp( cstr_t( szProcessPath ), to_string_t( searchPath ).c_str() ) == 0 )
						return GetProcess( ph );
				}
			}

			return null;
		}

		inline processptr_t FindProcessByFileName( const path_t& fileName, bool inheritHandle, dword accessMode )
		{
			PROCESSENTRY32 processEntry;
			processEntry.dwSize = Fill0( processEntry );

			string_t searchFileName = to_string_t( fileName );

			scoped_generic_handle processSnapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );

			if ( processSnapshot.get() == INVALID_HANDLE_VALUE )
				return null;

			if ( ::Process32First( processSnapshot.get(), &processEntry ) != FALSE )
			{
				do
				{
					if ( ::_tcsicmp( processEntry.szExeFile, searchFileName.c_str() ) == 0 )
					{
						if ( auto ph = OpenProcessStandardRightsOrLimitedRights( accessMode, inheritHandle, processEntry.th32ProcessID ) )
							return GetProcess( ph );
					}
				}
				while ( ::Process32Next( processSnapshot.get(), &processEntry ) != FALSE );
			}

			return null;
		}
	}

	processptr_t OpenProcessByFileName( const path_t& fileName, bool inheritHandle, dword accessMode )
	{
		if ( fileName.has_parent_path() )
			return FindProcessByFullPath( fileName, inheritHandle, accessMode );
		else
			return FindProcessByFileName( fileName, inheritHandle, accessMode );
	}

	namespace
	{
		void CoInitialize()
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
			CoThreadInitState;
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

	path_t GetSpecialFolderLocation( const guid_t& folderId )
	{
		CoInitialize();

		LPWSTR psz;
		HRESULT r = ::SHGetKnownFolderPath( folderId, 0, null, &psz );

		auto str = make_scoped_memory( psz, CoMemoryDeleter< wchar_t >() );

		if ( FAILED( r ) )
			exception< std::runtime_error >( FUNC_ERROR_ID( "SHGetKnownFolderPath", r ) );

		return str.get();
	}

	path_t GetCurrentLocation()
	{
		vchar_t result( MAX_PATH );

		adaptive_load( result, result.size(),
			[] ( char_t* buffer, std::size_t n )
		{
			return ::GetCurrentDirectory( numeric_cast< dword >( n ), buffer );
		} );

		return cstr_t( result );
	}

	std::pair< dword, mutex_t > TryLockMutex( const string_t& name, bool waitForGetOwnership )
	{
		mutex_t mutex( ::CreateMutex( null, TRUE, name.c_str() ) );
		dword lastError = ::GetLastError();

		if ( mutex && lastError == ERROR_ALREADY_EXISTS && waitForGetOwnership )
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

	cslock_t TryLockCriticalSection( CRITICAL_SECTION& csObj )
	{
		::EnterCriticalSection( &csObj );

		cslock_t lock( &csObj );

		return lock;
	}

	csptr_t CreateCriticalSection( uint spinCount )
	{
		CRITICAL_SECTION* newCriticalSection = lcallocate< CRITICAL_SECTION >( LPTR, sizeof( CRITICAL_SECTION ) );

#ifdef MYCPP_DEBUG
		::InitializeCriticalSectionEx( newCriticalSection, spinCount, 0 );
#else
		::InitializeCriticalSectionEx( newCriticalSection, spinCount, CRITICAL_SECTION_NO_DEBUG_INFO );
#endif

		return { newCriticalSection, CriticalSectionDeleter() };
	}

	class Process::Data
	{
	public:
		typedef PROCESS_INFORMATION ProcessInformation;

		Data() = default;
		Data( Data&& right ) noexcept = default;
		Data( const ProcessInformation& info );

		~Data();

		const ProcessInformation& GetProcessData() const;
		const path_t& GetProcessFileName() const;
		const string_t& GetProcessBaseName() const;
	private:
		mutable ProcessInformation m_process = {};
		mutable csptr_t m_critical_section = CreateCriticalSection();
		mutable path_t m_fileName;
		mutable string_t m_baseName;
	};

	inline Process::Data::Data( const ProcessInformation& info )
		: m_process( info )
	{}

	inline Process::Data::~Data()
	{
		if ( m_process.hProcess != null	&& m_process.hProcess != ::GetCurrentProcess() )
			::CloseHandle( m_process.hProcess );

		if ( m_process.hThread != null && m_process.hThread != ::GetCurrentThread() && m_process.hThread != PROCESS_PRIMARY_THREAD_HANDLE )
			::CloseHandle( m_process.hThread );
	}

	inline const Process::Data::ProcessInformation& Process::Data::GetProcessData() const
	{
		return m_process;
	}

	inline const path_t& Process::Data::GetProcessFileName() const
	{
		auto mutex = TryLockCriticalSection( m_critical_section );

		if ( m_process.hProcess != null && m_fileName.empty() )
		{
			vchar_t buffer( MAX_PATH );

			adaptive_load( buffer, buffer.size(),
				[this] ( char_t* s, std::size_t n )
			{
				dword size = numeric_cast< dword >( n );
				if ( ::QueryFullProcessImageName( m_process.hProcess, 0, s, &size ) != FALSE )
					return size + 1;
				return 0UL;
			} );

			m_fileName = cstr_t( buffer );
		}

		return m_fileName;
	}

	inline const string_t& Process::Data::GetProcessBaseName() const
	{
		auto mutex = TryLockCriticalSection( m_critical_section );

		if ( m_process.hProcess != null && m_baseName.empty() )
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

	Process::PtrSID Process::GetSid() const
	{
		handle_t token = null;

		if ( ::OpenProcessToken( m_data->GetProcessData().hProcess, TOKEN_QUERY, &token) )
		{
			dword bytes;
			auto processToken = make_scoped_handle( token );
			::GetTokenInformation( processToken.get(), TokenUser, null, 0, &bytes );

			auto tokenUser = make_scoped_local_memory< TOKEN_USER >( LPTR, bytes );
			if ( ::GetTokenInformation( processToken.get(), TokenUser, tokenUser.get(), bytes, &bytes ) != FALSE )
			{
				if ( ::IsValidSid( tokenUser->User.Sid ) != FALSE )
				{
					dword sidLength = ::GetLengthSid( tokenUser->User.Sid );
					PtrSID psid( lcallocate< SID >( LPTR, sidLength ), local_memory_deleter< SID >() );

					::CopySid( sidLength, psid.get(), tokenUser->User.Sid);

					if ( ::IsValidSid( psid.get() ) != FALSE )
						return psid;
				}
			}
		}

		exception< std::runtime_error >( FUNC_ERROR_MSG( "Process::GetSid", "Failed: 0x%08x", ::GetLastError() ) );

		return null;
	}

	Process::Ptr Process::GetParent() const
	{
		scoped_generic_handle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );

		if ( snapshot.get() == INVALID_HANDLE_VALUE )
			return null;

		dword processId = m_data->GetProcessData().dwProcessId;

		PROCESSENTRY32 processEntry;
		processEntry.dwSize = Fill0( processEntry );

		if ( ::Process32First( snapshot.get(), &processEntry ) != FALSE )
		{
			do
			{
				if ( processEntry.th32ProcessID == processId )
				{
					scoped_generic_handle process( OpenProcessStandardRightsOrLimitedRights( 0, false, processEntry.th32ParentProcessID ) );

					if ( !process )
						return null;

					return std::make_shared< Process >( 
						Process::Data( { process.release(), null, processEntry.th32ParentProcessID, 0 } ) );
				}
			}
			while ( ::Process32Next( snapshot.get(), &processEntry ) != FALSE );
		}

		return null;
	}

	Process* Process::GetCurrent()
	{
		static Process currentProcess(
			Process::Data( { ::GetCurrentProcess(), PROCESS_PRIMARY_THREAD_HANDLE, ::GetCurrentProcessId(), PROCESS_PRIMARY_THREAD_ID } ) );

		return &currentProcess;
	}

	Process::Ptr Process::Create( const string_t& cmdline, const path_t& appCurrentDir, void* envVariables, int creationFlags, bool inheritHandle, int cmdShow )
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

		appName = to_string_t( ComplatePath( appName ) );

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

		BOOL result = ::CreateProcess( appName.c_str()
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

		return std::make_shared< Process >( Data( pi ) );
	}

	string_t Process::GetName() const
	{
		return m_data->GetProcessBaseName();
	}

	path_t Process::GetFileName() const
	{
		return m_data->GetProcessFileName();
	}

	handle_t Process::GetHandle() const
	{
		return m_data->GetProcessData().hProcess;
	};

	handle_t Process::GetPrimaryThreadHandle() const
	{
		return m_data->GetProcessData().hThread;
	}

	dword Process::GetId() const
	{
		return m_data->GetProcessData().dwProcessId;
	}

	dword Process::GetPrimaryThreadId() const
	{
		return m_data->GetProcessData().dwThreadId;
	}

	dword Process::GetExitCode() const
	{
		dword exitCode;

		::GetExitCodeProcess( m_data->GetProcessData().hProcess, &exitCode );

		return exitCode;
	}

	void Process::Terminate( int exitCode )
	{
		::TerminateProcess( m_data->GetProcessData().hProcess, exitCode );
	}

	namespace
	{
		inline void SuspendProcess( dword processId, bool suspend )
		{
			scoped_generic_handle snapshot ( ::CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, processId ) );
	
			if ( snapshot.get() == INVALID_HANDLE_VALUE )
				return ; // abort function.
			
			THREADENTRY32 thinfo;
			thinfo.dwSize = Fill0( thinfo );

			if ( ::Thread32First( snapshot.get(), &thinfo ) != FALSE )
			{
				do
				{
					scoped_generic_handle thread( ::OpenThread( THREAD_SUSPEND_RESUME, FALSE, thinfo.th32ThreadID ) );
					if ( thread )
					{
						if ( suspend )
							::SuspendThread( thread.get() );
						else
							::ResumeThread( thread.get() );
					}
				}
				while ( ::Thread32Next( snapshot.get(), &thinfo ) != FALSE );
			}
		}
	}

	void Process::Suspend()
	{
		SuspendProcess( m_data->GetProcessData().dwProcessId, true );
	}

	void Process::Resume()
	{
		SuspendProcess( m_data->GetProcessData().dwProcessId, false );
	}

	dword Process::Wait( dword milliseconds, bool forInputIdle ) const
	{
		if ( forInputIdle )
			return ::WaitForInputIdle( m_data->GetProcessData().hProcess, milliseconds );
		else
			return ::WaitForSingleObject( m_data->GetProcessData().hProcess, milliseconds );
	}

	processptr_t GetProcess( dword pid )
	{
		std::vector< dword > pids( 400 );

		adaptive_load( pids, pids.size(),
			[] ( dword* pn, std::size_t n )
		{
			dword size;
			::EnumProcesses( pn, numeric_cast< dword >( n * sizeof( dword ) ), &size );

			size /= sizeof( dword );

			return size;
		} );

		if ( std::find( pids.begin(), pids.end(), pid) == pids.end() )
			return null;

		handle_t process = OpenProcessStandardRightsOrLimitedRights( 0, false, pid );
		if ( process == null )
			return null;

		return std::make_shared< Process >( Process::Data( { process, null, pid, 0 } ) );
	}

	processptr_t GetProcess( handle_t hProcess )
	{
		return std::make_shared< Process >( Process::Data( { hProcess, null, ::GetProcessId( hProcess ), 0 } ) );
	}

	path_t FindFilePath( const string_t& filename, const string_t& ext )
	{
		vchar_t szSearchPath( MAX_PATH );

		adaptive_load( szSearchPath, szSearchPath.size(), 
			[&filename, &ext] ( char_t* s, std::size_t n )
		{
			return ::SearchPath( null, filename.c_str(), ( !ext.empty() ) ? ext.c_str() : null, numeric_cast< dword >( n ), s, null );
		} );

		return szSearchPath.data();
	}

	int RunElevated( const path_t& file, const string_t& parameters, bool waitForExit, int cmdShow )
	{
		path_t filePath = ComplatePath( file );

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

			dword dwExitCode;
			::GetExitCodeProcess( shei.hProcess, &dwExitCode );

			::CloseHandle( shei.hProcess );

			return static_cast< int >( dwExitCode );
		}

		int result = pointer_int_cast< int >( shei.hInstApp );

		return ( result <= 32 ) ? result : 0;
	}

	namespace
	{
		struct FINDWINDOWINFO
		{
			static constexpr std::size_t BUFFER_SIZE = 256;
			HWND hwnd;
			DWORD pid;
			LPCTSTR className;
			LPCTSTR windowName;
			vchar_t buffer;
			bool found;
		};

		inline std::size_t GetWindowClassName( HWND hwnd, vchar_t& buffer )
		{
			return adaptive_load( buffer, buffer.size(),
				[&hwnd] ( char_t* s, std::size_t n )
			{
				return ::GetClassName( hwnd, s, numeric_cast< int >( n ) );
			} );
		}

		inline std::size_t GetWindowName( HWND hwnd, vchar_t& buffer )
		{
			std::size_t size = ::GetWindowTextLength( hwnd ) + 1;

			if ( buffer.size() < size )
				buffer.resize( size );

			return ::GetWindowText( hwnd, cstr_t( buffer ), numeric_cast< int >( buffer.size() ) );
		}

		BOOL CALLBACK EnumChildWindowsProc(
			HWND hwnd,      // 子ウィンドウのハンドル
			LPARAM lParam   // アプリケーション定義の値
		)
		{
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );

			GetWindowClassName( hwnd, p->buffer );
			if ( ::_tcsicmp( cstr_t( p->buffer ), p->className ) == 0 )
			{
				GetWindowName( hwnd, p->buffer );
				if ( p->windowName == null || ( p->windowName != null && ::_tcsicmp(cstr_t(p->buffer), p->windowName) == 0 ) )
				{
					p->hwnd = hwnd;
					p->found = true;
					return FALSE;
				}
			}

			// Search recursively until you finish finding or enumerating.
			if ( !p->found )
				::EnumChildWindows( hwnd, &EnumChildWindowsProc, lParam );

			return ( p->found ) ? FALSE : TRUE;
		}
			
		BOOL CALLBACK EnumChildWindowsProcEntry(
			HWND hwnd,      // 子ウィンドウのハンドル
			LPARAM lParam   // アプリケーション定義の値
		)
		{
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );

			// First check the first value.
			if ( EnumChildWindowsProc( hwnd, lParam ) == FALSE )
				return FALSE;

			// If not found, also look for child windows.
			::EnumChildWindows( hwnd, &EnumChildWindowsProc, lParam );

			return ( p->found ) ? FALSE : TRUE;
		}

		BOOL CALLBACK EnumWindowsProc(
			HWND hwnd,      // 親ウィンドウのハンドル
			LPARAM lParam   // アプリケーション定義の値
		)
		{
			dword pid;
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );

			::GetWindowThreadProcessId( hwnd, &pid );
			if ( pid == p->pid )
			{
				GetWindowClassName( hwnd, p->buffer );
				if ( ::_tcsicmp( cstr_t( p->buffer ), p->className ) == 0 )
				{
					GetWindowName( hwnd, p->buffer );
					if ( p->windowName == null || ( p->windowName != null && ::_tcsicmp( cstr_t( p->buffer ), p->windowName ) == 0 ) )
					{
						p->hwnd = hwnd;
						p->found = true;
						return FALSE;
					}
				}
			}

			return TRUE;
		}
	}

	string_t Window::GetText() const
	{
		long_t nLength = ::SendMessage( m_hwnd, WM_GETTEXTLENGTH, 0, 0 );
		vchar_t buffer( nLength + 1 );

		::SendMessage( m_hwnd, WM_GETTEXT, numeric_cast< WPARAM >( buffer.size() ), reinterpret_cast< LPARAM >( cstr_t( buffer ) ) );

		return cstr_t( buffer );
	}

	string_t Window::SetText( const string_t& text )
	{
		string_t old = this->GetText();

		this->Send( WM_SETTEXT, 0, reinterpret_cast< long_t >( text.c_str() ) );

		return old;
	}

	long_t Window::GetAttribute( int index ) const
	{
		return ::GetWindowLongPtr( m_hwnd, index );
	}

	long_t Window::SetAttribute( int index, long_t value )
	{
		return ::SetWindowLongPtr( m_hwnd, index, value );
	}
	
	string_t Window::GetClassNameText() const
	{
		vchar_t buffer;

		GetWindowClassName( m_hwnd, buffer );

		return cstr_t( buffer );
	}

	long_t Window::Send( uint msg, uint_t wparam, long_t lparam )
	{
		return ::SendMessage( m_hwnd, msg, wparam, lparam );
	}

	long_t Window::SendNotify( uint msg, uint_t wparam, long_t lparam )
	{
		return ::SendNotifyMessage( m_hwnd, msg, wparam, lparam );
	}

	std::pair< long_t, dword_t > Window::SendTimeout( uint msg, uint_t wparam, long_t lparam, uint flags, uint milliseconds )
	{
		dword_t msgResult = 0;
		long_t result = ::SendMessageTimeout( m_hwnd, msg, wparam, lparam, flags, milliseconds, &msgResult );

		return std::make_pair( result, msgResult );
	}

	long_t Window::Post( uint msg, uint_t wparam, long_t lparam )
	{
		return ::PostMessage( m_hwnd, msg, wparam, lparam );
	}

	Window::Ptr Window::GetParent() const
	{
		return std::make_shared< Window >( ::GetParent(m_hwnd) );
	}

	void Window::Close()
	{
		if ( ::IsWindow( m_hwnd ) != FALSE )
			this->Send( WM_CLOSE, 0, 0 );
	}

	Window::Ptr Window::GetChild( const string_t& wndClassName, const string_t& wndName ) const
	{
		FINDWINDOWINFO fwi =
		{
			null,
			0,
			wndClassName.c_str(),
			( !wndName.empty() ) ? wndName.c_str() : null,
			vchar_t( FINDWINDOWINFO::BUFFER_SIZE ),
			false
		};

		::EnumChildWindows( this->GetHandle(), &EnumChildWindowsProcEntry, pointer_int_cast< LPARAM >( &fwi ));

		if ( !fwi.found )
			return null;

		return std::make_shared< Window >( fwi.hwnd );
	}

	wndptr_t FindProcessWindow( const processptr_t& process, const string_t& wndClassName, const string_t& wndName )
	{
		if ( !process )
			return null;

		FINDWINDOWINFO fwi =
		{
			null,
			process->GetId(),
			wndClassName.c_str(),
			( !wndName.empty() ) ? wndName.c_str() : null,
			vchar_t( FINDWINDOWINFO::BUFFER_SIZE ),
			false
		};

		::EnumWindows( &EnumWindowsProc, pointer_int_cast< LPARAM >( &fwi ) );

		if ( !fwi.found )
			return null;

		return std::make_shared< Window >( fwi.hwnd );
	}

	uint GetRegBinary( hkey_t parentKey, const string_t& subKey, const string_t& valueName, void* ptr, uint size )
	{
		hkey_t hk;

		LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
		if ( r == ERROR_SUCCESS )
		{
			dword acquireSize = size;
			scoped_reg_handle regHandle( hk );

			r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, null, &acquireSize );
			if ( r == ERROR_SUCCESS  )
			{
				scoped_local_memory< byte > buffer;
				byte* acquiredData = reinterpret_cast< byte* >( ptr );
				
				if ( acquireSize > size )
				{
					buffer = make_scoped_local_memory< byte >( LPTR, acquireSize );
					acquiredData = buffer.get();
				}

				if ( ptr == null )
					exception< std::logic_error >( ERROR_MSG( "ptr is null" ) );

				r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, acquiredData, &acquireSize );
				if ( r == ERROR_SUCCESS )
				{
					uint copyBytes = std::min( static_cast< dword >( size ), acquireSize );

					if ( ptr != acquiredData )
						std::memcpy( ptr, acquiredData, copyBytes );

					return copyBytes;
				}
			}
		}

		return 0;
	}

	namespace
	{
		template < typename Xword, dword REGTYPE >
		inline Xword GetRegXword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
		{
			hkey_t hk;

			LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
			if ( r == ERROR_SUCCESS )
			{
				Xword result;

				dword size = sizeof( Xword );
				dword dataType = REGTYPE;
				scoped_reg_handle regHandle( hk );

				r = ::RegQueryValueEx( hk, valueName.c_str(), null, &dataType, reinterpret_cast< byte* >( &result ), &size );
				if ( r == ERROR_SUCCESS && dataType == REGTYPE && size == sizeof( Xword ) )
					return result;
			}

			exception< std::runtime_error >( REGVALUE_ERROR( "GetRegXword", subKey, valueName, r ) );

			return null;
		}

	}

	dword GetRegDword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
	{
		return GetRegXword< dword, REG_DWORD >( parentKey, subKey, valueName );
	}

	qword GetRegQword( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
	{
		return GetRegXword< qword, REG_QWORD >( parentKey, subKey, valueName );
	}

	string_t GetRegString( hkey_t parentKey, const string_t& subKey, const string_t& valueName )
	{
		hkey_t hk;

		LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
		if ( r == ERROR_SUCCESS )
		{
			dword size = 0;
			scoped_reg_handle regHandle( hk );
			r = ::RegQueryValueEx( hk, valueName.c_str(), null, null, null, &size );
			if ( r == ERROR_SUCCESS )
			{
				dword dataType;
				std::vector< byte > buffer( size );
				r = ::RegQueryValueEx( hk, valueName.c_str(), null, &dataType, buffer.data(), &size );
				if ( r == ERROR_SUCCESS )
				{
					switch ( dataType )
					{
					case REG_DWORD:
						return strprintf( _T( "%I32u" ), *reinterpret_cast< dword* >( buffer.data() ) );

					case REG_QWORD:
						return strprintf( _T( "%I64u" ), *reinterpret_cast< qword* >( buffer.data() ) );

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

	void SetRegString( hkey_t parentKey, const string_t& subKey, const string_t& valueName, const string_t& value )
	{
		hkey_t hk;
		dword disposition;

		LSTATUS r = ::RegCreateKeyEx( parentKey, subKey.c_str(), 0, null, REG_OPTION_NON_VOLATILE, KEY_WRITE, null, &hk, &disposition );
		if ( r == ERROR_SUCCESS )
		{
			scoped_reg_handle regHandle( hk );
			r = ::RegSetValueEx( hk, valueName.c_str(), 0, REG_SZ, reinterpret_cast< const byte* >( value.c_str() ), numeric_cast< dword >( value.length() ) );
			if ( r == ERROR_SUCCESS )
				return ;
		}

		exception< std::runtime_error >( REGVALUE_ERROR( "SetRegString", subKey, valueName, r ) );
	}

	void SetRegBinary( hkey_t parentKey, const string_t& subKey, const string_t& valueName, uint type, const void* ptr, uint size )
	{
		hkey_t hk;
		dword disposition;

		LSTATUS r = ::RegCreateKeyEx( parentKey, subKey.c_str(), 0, null, REG_OPTION_NON_VOLATILE, KEY_WRITE, null, &hk, &disposition );
		if ( r == ERROR_SUCCESS )
		{
			scoped_reg_handle regHandle( hk );
			r = ::RegSetValueEx( hk, valueName.c_str(), 0, type, reinterpret_cast< const byte* >( ptr ), size );
			if ( r == ERROR_SUCCESS )
				return;
		}

		exception< std::runtime_error >( REGVALUE_ERROR( "SetRegBinary", subKey, valueName, r ) );
	}

	string_t GetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& defaultValue )
	{
		vchar_t buffer( 512 );

		adaptive_load( buffer, buffer.size(),
			[&section, &name, &defaultValue, &file] ( char_t* s, std::size_t n ) 
		{
			dword r = ::GetPrivateProfileString( section.c_str()
												 , name.c_str()
												 , defaultValue.c_str()
												 , s
												 , numeric_cast< dword >( n )
												 , to_string_t( file ).c_str() );
			return ( r == n - 1 ) ? r + 1 : r;
		} );

		return cstr_t( buffer );
	}

	bool GetIniBinary( const path_t& file, const string_t& section, const string_t& name, void* ptr, uint size )
	{
		if ( ptr == null )
			exception< std::logic_error >( ERROR_MSG( "ptr is null" ) );

		return ( ::GetPrivateProfileStruct( section.c_str(), name.c_str(), ptr, size, to_string_t( file ).c_str() ) != FALSE );
	}

	void SetIniString( const path_t& file, const string_t& section, const string_t& name, const string_t& value )
	{
		LPCTSTR pszName = ( !name.empty() ) ? name.c_str() : null;
		LPCTSTR pszValue = ( !value.empty() ) ? value.c_str() : null;

		::WritePrivateProfileString( section.c_str(), pszName, pszValue, to_string_t( file ).c_str() );
	}

	void SetIniBinary( const path_t& file, const string_t& section, const string_t& name, const void* ptr, uint size )
	{
		LPCTSTR pszName = ( !name.empty() ) ? name.c_str() : null;

		if ( ptr == null )
			exception< std::logic_error >( ERROR_MSG( "ptr is null" ) );

		::WritePrivateProfileStruct( section.c_str(), pszName, const_cast< void* >( ptr ), size, to_string_t( file ).c_str() );
	}
}
