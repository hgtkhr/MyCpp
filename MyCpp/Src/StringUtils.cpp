#include "MyCpp/Win32Base.hpp"
#include "MyCpp/Error.hpp"
#include "MyCpp/StringUtils.hpp"

namespace mycpp
{
	namespace details
	{
		constexpr std::size_t SINTMAX = std::numeric_limits< int >::max();

		std::size_t XStrToYStr( const char* from, std::size_t length, wchar_t* to, std::size_t capacity )
		{
			if ( length == 0 )
				return 0;

			if ( length > SINTMAX || capacity > SINTMAX )
				exception< std::length_error >( FUNC_ERROR_MSG( "XStrToYStr", "Too large size is specified." ) );

			int r = ::MultiByteToWideChar( CP_THREAD_ACP, 0, from, numeric_cast< int >( length ), to, numeric_cast< int >( capacity ) );

			if ( r == 0 )
				exception< std::runtime_error >( FUNC_ERROR_ID( "MultiByteToWideChar", ::GetLastError() ) );

			return r;
		}

		std::size_t XStrToYStr( const wchar_t* from, std::size_t length, char* to, std::size_t capacity )
		{
			if ( length == 0 )
				return 0;

			if ( length > SINTMAX || capacity > SINTMAX )
				exception< std::length_error >( FUNC_ERROR_MSG( "XStrToYStr", "Too large size is specified." ) );

			int r = ::WideCharToMultiByte( CP_THREAD_ACP, 0, from, numeric_cast< int >( length ), to, numeric_cast< int >( capacity ), null, null );

			if ( r == 0 )
				exception< std::runtime_error >( FUNC_ERROR_ID( "WideCharToMultiByte", ::GetLastError() ) );

			return r;
		}
	}
}
