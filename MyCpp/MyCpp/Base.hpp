#pragma once
#include <tchar.h>
#include <string>
#include <vector>

#if defined( min )
#undef min
#endif
#if defined( max )
#undef max
#endif

#include "MyCpp/Config.hpp"
#include "MyCpp/LinkLib.hpp"

#include "IntCast.hpp"

namespace MyCpp
{
	typedef TCHAR char_t;

	typedef unsigned int uint;
	typedef unsigned long ulong;
	typedef unsigned short ushort;
	typedef unsigned char uchar;

	typedef long long longlong;
	typedef unsigned long long ulonglong;

	typedef uchar byte;
	typedef ushort word;
	typedef ulong dword;
	typedef ulonglong qword;
		
	typedef std::vector< char > vchar;
	typedef std::vector< wchar_t > vwchar;
	typedef std::vector< char_t > vchar_t;

	typedef std::basic_string< char_t > string_t;

	namespace Details
	{
		class Null
		{
		public:
			template< typename T >
			operator T () const
			{
				return {};
			}
			template< typename T >
			operator T* ( ) const
			{
				return nullptr;
			}
			template< typename T, typename C >
			operator T C::* ( ) const
			{
				return nullptr;
			}

			void operator & () const = delete;
		};
	}

	constexpr auto null = Details::Null {};

	template < typename T, std::size_t N >
	inline constexpr std::size_t length_of( T( & )[N] )
	{
		return N;
	}

	template < typename CharT >
	inline CharT* cstr_t( std::vector< CharT >& v )
	{
		return v.data();
	}

	template < typename CharT >
	inline const CharT* cstr_t( const std::vector< CharT >& v )
	{
		return v.data();
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using MyCpp::null;
using MyCpp::byte;
using MyCpp::char_t;
using MyCpp::dword;
using MyCpp::longlong;
using MyCpp::qword;
using MyCpp::string_t;
using MyCpp::uchar;
using MyCpp::uint;
using MyCpp::ulonglong;
using MyCpp::ulong;
using MyCpp::ushort;
using MyCpp::vchar;
using MyCpp::vchar_t;
using MyCpp::vwchar;
using MyCpp::word;
#endif
