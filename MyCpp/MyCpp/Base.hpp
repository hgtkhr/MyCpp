#pragma once
#ifndef __MYCPP_BASE_HPP__
#define __MYCPP_BASE_HPP__

#if defined( _MSVC_LANG )
#define MYCPP_STDCPP_VERSION _MSVC_LANG
#elif defined ( __cplusplus )
#define MYCPP_STDCPP_VERSION __cplusplus
#endif

#if !defined( MYCPP_STDCPP_VERSION ) || MYCPP_STDCPP_VERSION < 201703L
#error "A compiler compatible with C++17 or later is required to use the library."
#endif

#if defined( _DEBUG ) || defined ( DEBUG )
#define MYCPP_DEBUG 1
#endif

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
	typedef _TCHAR char_t;

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

	class Null
	{
	public:
		template< typename T >
		constexpr operator T () const
		{
			return {};
		}
		template< typename T >
		constexpr operator T* ( ) const
		{
			return nullptr;
		}
		template< typename T, typename C >
		constexpr operator T C::* ( ) const
		{
			return nullptr;
		}

		Null() = default;

		Null( const Null& ) = delete;
		Null( const Null&& ) = delete;

		Null& operator = ( const Null& ) = delete;
		Null& operator = ( const Null&& ) = delete;

		void operator & () const = delete;
	};

	template < typename T >
	[[nodiscard]]
	constexpr bool operator == ( const T& lhs, const Null& rhs )
	{
		return ( lhs == static_cast< T >( rhs ) );
	}

	template < typename T >
	[[nodiscard]]
	constexpr bool operator != ( const T& lhs, const Null& rhs )
	{
		return ( lhs != static_cast< T >( rhs ) );
	}

	constexpr Null null;

	template < typename T, std::size_t N >
	constexpr std::size_t count_of( T( & )[N] )
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

#endif // ! __MYCPP_BASE_HPP__
