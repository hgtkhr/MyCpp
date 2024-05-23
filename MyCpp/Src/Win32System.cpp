#include "MyCpp/Win32System.hpp"
#include "MyCpp/Win32SafeHandle.hpp"
#include "MyCpp/Win32Memory.hpp"
#include "MyCpp/Error.hpp"
#include "MyCpp/IntCast.hpp"

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
	constexpr dword PROCESS_GET_INFO = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

	path_t GetProgramModuleFileName( HMODULE hmodule )
	{
		vchar_t buffer( MAX_PATH );

		adaptive_load( buffer, buffer.size(),
			[&] ( LPTSTR buffer, std::size_t n )
		{
			return ::GetModuleFileName( hmodule, buffer, numeric_cast< dword >( n ) );
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

		adaptive_load( buffer, buffer.size(),
			[&] ( LPTSTR buffer, std::size_t n )
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

	string_t ToGuidString( const GUID& guid )
	{
		vwchar guidStr( 40 );

		adaptive_load( guidStr, guidStr.size(),
			[&] ( LPWSTR buffer, std::size_t n )
		{
			int r = ::StringFromGUID2( guid, buffer, numeric_cast< int >( n ) );

			return ( r > 0 ) ? r : n;
		} );

		return narrow_wide_string< string_t, std::wstring >( cstr_t( guidStr ) );
	}

	sidptr_t GetProcessSid( HANDLE process )
	{
		typedef typename sidptr_t::element_type SID;

		HANDLE token = null;

		if ( ::OpenProcessToken( process, TOKEN_QUERY, &token ) )
		{
			dword bytes;

			scoped_generic_handle processToken = make_scoped_handle< HANDLE >( token );
			::GetTokenInformation( processToken.get(), TokenUser, null, 0, &bytes );

			auto tokenUser = make_scoped_local_memory< TOKEN_USER >( LPTR, bytes );
			if ( ::GetTokenInformation( processToken.get(), TokenUser, tokenUser.get(), bytes, &bytes ) )
			{
				if ( ::IsValidSid( tokenUser->User.Sid ) )
				{
					dword sidLength = ::GetLengthSid( tokenUser->User.Sid );
					auto sid = make_scoped_local_memory< SID >( LPTR, sidLength );

					::CopySid( sidLength, sid.get(), tokenUser->User.Sid );

					if ( ::IsValidSid( sid.get() ) )
						return std::move( sidptr_t( sid.release(), local_memory_deleter< SID >() ) );
				}
			}
		}

		exception< std::runtime_error >( FUNC_ERROR_MSG( "GetProcessSid", "Failed: 0x%08x", ::GetLastError() ) );

		return null;
	}

	processptr_t OpenCuProcessByFileName( const path_t& fileName, bool inheritHandle, dword accessMode )
	{
		processptr_t process = OpenProcessByFileName( fileName, inheritHandle, accessMode );

		if ( process )
		{
			sidptr_t current( GetProcessSid( ::GetCurrentProcess() ) );
			sidptr_t target( GetProcessSid( process->GetHandle() ) );

			if ( ::EqualSid( current.get(), target.get() ) )
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

			adaptive_load( szSearchPath, szSearchPath.size(), [&pstr] ( LPTSTR s, std::size_t n )
			{
				return ::SearchPath( null, pstr.c_str(), null, numeric_cast< dword >( n ), s, null );
			} );

			if ( _tcslen( cstr_t( szSearchPath ) ) == 0 )
				return std::filesystem::weakly_canonical( p );

			return std::filesystem::weakly_canonical( szSearchPath.data() );
		}
	}

	processptr_t OpenProcessByFileName( const path_t& fileName, bool inheritHandle, dword accessMode )
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

		if ( fileName.has_parent_path() )
		{
			path_t exeFilePath = ComplatePath( fileName );
			string_t searchExeName = to_string_t( exeFilePath );
			for ( dword i = 0; i < maxIndex; ++i )
			{
				if ( HANDLE p = ::OpenProcess( accessMode | PROCESS_GET_INFO, ( inheritHandle ) ? TRUE : FALSE, pids[i] ) )
				{
					processptr_t process = GetProcess( p );
					string_t exeFilePath = to_string_t( process->GetFileName() );
					if ( ::_tcsicmp( exeFilePath.c_str(), searchExeName.c_str()) == 0 )
						return std::move( process );
				}
			}
		}
		else
		{
			string_t searchExeName = to_string_t( fileName );
			for ( dword i = 0; i < maxIndex; ++i )
			{
				if ( HANDLE p = ::OpenProcess( accessMode | PROCESS_GET_INFO, ( inheritHandle ) ? TRUE : FALSE, pids[i] ) )
				{
					processptr_t process = GetProcess( p );
					if ( ::_tcsicmp( process->GetName().c_str(), searchExeName.c_str() ) == 0 )
						return std::move( process );
				}
			}
		}


		return null;
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

	path_t GetSpecialFolderLocation( const GUID& folderId )
	{
		CoInitialize();

		LPWSTR psz;
		HRESULT r = ::SHGetKnownFolderPath( folderId, 0, null, &psz );

		scoped_memory_t< wchar_t, CoMemoryDeleter< wchar_t > > str( psz );

		if ( FAILED( r ) )
			exception< std::runtime_error >( FUNC_ERROR_ID( "SHGetKnownFolderPath", r ) );

		return str.get();
	}

	path_t GetCurrentLocation()
	{
		vchar_t result( MAX_PATH );

		adaptive_load( result, result.size(),
			[] ( LPTSTR buffer, std::size_t n )
		{
			return ::GetCurrentDirectory( numeric_cast< dword >( n ), buffer );
		} );

		return cstr_t( result );
	}

	std::pair< dword, mutex_t > TryLockMutex( const string_t& name, bool waitForGetOwnership )
	{
		mutex_t mutex( ::CreateMutex( null, TRUE, name.c_str() ) );
		dword lastError = ::GetLastError();

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

	cslock_t TryLockCriticalSection( CRITICAL_SECTION& csObj )
	{
		::EnterCriticalSection( &csObj );

		cslock_t lock( &csObj );

		return std::move( lock );
	}

	csptr_t CreateCriticalSection( uint spinCount )
	{
		LPCRITICAL_SECTION newCriticalSection = lcallocate< CRITICAL_SECTION >( LPTR, sizeof( CRITICAL_SECTION ) );

		if constexpr ( MYCPP_DEBUG == 1 )
			::InitializeCriticalSectionEx( newCriticalSection, spinCount, 0 );
		else
			::InitializeCriticalSectionEx( newCriticalSection, spinCount, CRITICAL_SECTION_NO_DEBUG_INFO );

		return std::move( csptr_t( newCriticalSection, CriticalSectionDeleter() ) );
	}

	class Process::Data
	{
	public:
		typedef std::pair< HANDLE, dword > HANDLEID;

		Data();
		Data( Data&& right ) noexcept;
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
		Process::Data::HANDLEID GetPrimaryThreadHandleId( dword processId )
		{
			scoped_generic_handle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, processId ) );

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

	inline Process::Data::Data( Data&& right ) noexcept
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

			adaptive_load( buffer, buffer.size(),
				[&] ( char_t* s, std::size_t n )
			{
				dword size = numeric_cast< dword >( n );
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

	dword Process::GetId() const
	{
		return m_data->GetProcessData().second;
	}

	dword Process::GetPrimaryThreadId() const
	{
		return m_data->GetPrimaryThreadData().second;
	}

	dword Process::GetExitCode() const
	{
		dword exitCode;

		::GetExitCodeProcess( m_data->GetProcessData().first, &exitCode );

		return exitCode;
	}

	void Process::Terminate( int exitCode )
	{
		::TerminateProcess( m_data->GetProcessData().first, exitCode );
	}

	namespace
	{
		inline void SuspendProcess( dword processId, bool suspend )
		{
			scoped_generic_handle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, processId ) );

			if ( snapshot )
			{
				THREADENTRY32 thinfo;
				thinfo.dwSize = Fill0( thinfo );

				if ( ::Thread32First( snapshot.get(), &thinfo ) )
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

	dword Process::Wait( dword milliseconds, bool forInputIdle ) const
	{
		if ( forInputIdle )
			return ::WaitForInputIdle( m_data->GetProcessData().first, milliseconds );
		else
			return ::WaitForSingleObject( m_data->GetProcessData().first, milliseconds );
	}

	processptr_t GetProcess( dword pid )
	{
		std::vector< dword > pids( 200 );

		adaptive_load( pids, pids.size(),
			[&] ( dword* pn, std::size_t n )
		{
			dword size;

			::EnumProcesses( pn, numeric_cast< dword >( n * sizeof( dword ) ), &size );

			size /= sizeof( dword );

			return size;
		} );

		if ( std::find( pids.begin(), pids.end(), pid) == pids.end() )
			return null;

		HANDLE openedProcess = ::OpenProcess( PROCESS_GET_INFO, FALSE, pid );
		if ( openedProcess == null )
			return null;

		return std::make_shared< Process >( 
			std::move( Process::Data( { openedProcess, null, pid, 0 } ) ) );
	}

	processptr_t GetProcess( HANDLE hProcess )
	{
		return std::make_shared< Process >( 
			std::move( Process::Data( { hProcess, null, ::GetProcessId( hProcess ), 0 } ) ) );
	}

	processptr_t GetParentProcess()
	{
		scoped_generic_handle snapshot( ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 ) );

		if ( snapshot )
		{
			dword currentProcessId = ::GetCurrentProcessId();

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

	path_t FindFilePath( const string_t& filename, const string_t& ext )
	{
		vchar_t szSearchPath( MAX_PATH );

		adaptive_load( szSearchPath, szSearchPath.size(), [&filename, &ext] ( LPTSTR s, std::size_t n )
		{
			return ::SearchPath( null, filename.c_str(), ( !ext.empty() ) ? ext.c_str() : null, numeric_cast< dword >( n ), s, null );
		} );

		return szSearchPath.data();
	}

	processptr_t StartProcess( const string_t& cmdline, const path_t& appCurrentDir, void* envVariables, int creationFlags, bool inheritHandle,  int cmdShow )
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

		return std::make_shared< Process >(
			std::move( Process::Data( pi ) ) );
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
			dword pid;
			LPCTSTR className;
			LPCTSTR windowName;
			vchar_t buffer;
		};

		inline std::size_t GetWindowClassName( HWND hwnd, vchar_t& buffer )
		{
			return adaptive_load( buffer, buffer.size(),
				[&] ( LPTSTR s, std::size_t n )
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
			dword pid;
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );

			::GetWindowThreadProcessId( hwnd, &pid );
			if ( pid == p->pid )
			{
				GetWindowClassName( hwnd, p->buffer );
				if ( ::_tcsicmp( cstr_t( p->buffer ), p->className ) == 0 )
				{
					GetWindowName( hwnd, p->buffer );
					if ( ::_tcsicmp( cstr_t( p->buffer ), p->windowName ) == 0 )
					{
						p->hwnd = hwnd;
						return FALSE;
					}
				}
			}

			// Search recursively until you finish finding or enumerating.
			::EnumChildWindows( hwnd, &EnumChildWindowsProc, lParam );

			if ( p->hwnd != null )
				return FALSE;

			return TRUE;
		}
			
		BOOL CALLBACK EnumWindowsProc(
			HWND hwnd,      // 親ウィンドウのハンドル
			LPARAM lParam   // アプリケーション定義の値
		)
		{
			dword pid;
			FINDWINDOWINFO* p = pointer_int_cast< FINDWINDOWINFO* >( lParam );

			// First, search from the parent window
			::GetWindowThreadProcessId( hwnd, &pid );
			if ( pid == p->pid )
			{
				GetWindowClassName( hwnd, p->buffer );
				if ( ::_tcsicmp( cstr_t( p->buffer ), p->className ) == 0 )
				{
					GetWindowName( hwnd, p->buffer );
					if ( ::_tcsicmp( cstr_t( p->buffer ), p->windowName ) == 0 )
					{
						p->hwnd = hwnd;
						return FALSE;
					}
				}
			}

			// If not found, also look for child windows.
			::EnumChildWindows( hwnd, &EnumChildWindowsProc, lParam );

			if ( p->hwnd != null )
				return FALSE;

			return TRUE;
		}
	}

	Window::Window( HWND hwnd )
		: m_hwnd( hwnd )
	{}

	Window& Window::operator = ( HWND hwnd )
	{
		m_hwnd = hwnd;
		return *this;
	}

	string_t Window::Text() const
	{
		vchar_t buffer;

		GetWindowName( m_hwnd, buffer );

		return cstr_t( buffer );
	}

	string_t Window::Text( const string_t& text )
	{
		string_t old = this->Text();

		::SetWindowText( m_hwnd, text.c_str() );

		return old;
	}

	string_t Window::ClassName() const
	{
		vchar_t buffer;

		GetWindowClassName( m_hwnd, buffer );

		return cstr_t( buffer );
	}

	Window::lresult_t Window::Send( uint msg, wparam_t wparam, lparam_t lparam )
	{
		return ::SendMessage( m_hwnd, msg, wparam, lparam );
	}

	Window::dword_ptr_t Window::SendTimeout( uint msg, wparam_t wparam, lparam_t lparam, uint flags, uint milliseconds )
	{
		dword_ptr_t result = 0;

		::SendMessageTimeout( m_hwnd, msg, wparam, lparam, flags, milliseconds, &result );

		return result;
	}

	Window::lresult_t Window::Post( uint msg, wparam_t wparam, lparam_t lparam )
	{
		return ::PostMessage( m_hwnd, msg, wparam, lparam );
	}

	void Window::Close()
	{
		if ( ::IsWindow( m_hwnd ) != FALSE )
			this->Send( WM_CLOSE, 0, 0 );
	}

	Window FindProcessWindow( const processptr_t& process, const string_t& wndClassName, const string_t& wndName )
	{
		if ( !process )
			return null;

		FINDWINDOWINFO fwi =
		{
			null,
			process->GetId(),
			wndClassName.c_str(),
			wndName.c_str(),
			std::move( vchar_t( FINDWINDOWINFO::BUFFER_SIZE ) )
		};

		::EnumWindows( &EnumWindowsProc, pointer_int_cast< LPARAM >( &fwi ) );
		
		return fwi.hwnd;
	}

	const Process* GetCurrentProcess()
	{
		static Process currentProcess(
			std::move( Process::Data( { ::GetCurrentProcess(), null, ::GetCurrentProcessId(), 0 } ) ) );

		return &currentProcess;
	}

	uint GetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, void* ptr, uint size )
	{
		HKEY hk;

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
		inline Xword GetRegXword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
		{
			HKEY hk;

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

	dword GetRegDword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		return GetRegXword< dword, REG_DWORD >( parentKey, subKey, valueName );
	}

	qword GetRegQword( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		return GetRegXword< qword, REG_QWORD >( parentKey, subKey, valueName );
	}

	string_t GetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName )
	{
		HKEY hk;

		LSTATUS r = ::RegOpenKeyEx( parentKey, subKey.c_str(), 0, KEY_READ, &hk );
		if ( r == ERROR_SUCCESS )
		{
			dword size;
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

	void SetRegString( HKEY parentKey, const string_t& subKey, const string_t& valueName, const string_t& value )
	{
		HKEY hk;
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

	void SetRegBinary( HKEY parentKey, const string_t& subKey, const string_t& valueName, uint type, const void* ptr, uint size )
	{
		HKEY hk;
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
			[&] ( LPTSTR s, std::size_t n ) 
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
