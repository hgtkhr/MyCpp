#pragma once
#include <locale>
#include "MyCpp/StringUtils.hpp"

namespace mycpp
{
	namespace details
	{
		template< typename charT>
		constexpr charT tolower( charT c )
		{
			return std::tolower( c, std::locale::classic() );
		}
	}

	template < typename CharT >
	struct ichar_traits : public std::char_traits< CharT >
	{
		using base_class_type = std::char_traits< char_t >;

		using base_class_type::char_type;
		using base_class_type::int_type;
		using base_class_type::pos_type;
		using base_class_type::off_type;
		using base_class_type::state_type;
#if MYCPP_STDCPP_VERSION >= 202002L
		using base_class_type::comparison_category;
#endif
		using base_class_type::assign;
		using base_class_type::length;
		using base_class_type::move;
		using base_class_type::copy;
		using base_class_type::not_eof;
		using base_class_type::to_char_type;
		using base_class_type::to_int_type;
		using base_class_type::eq_int_type;
		using base_class_type::eof;
		
		static constexpr bool eq( char_type c1, char_type c2 ) noexcept
		{
			return ( details::tolower( c1 ) == details::tolower( c2 ) );
		}
		
		static constexpr bool lt( char_type c1, char_type c2 ) noexcept
		{
			return ( details::tolower( c1 ) < details::tolower( c2 ) );
		}

		static constexpr int compare( const char_type* s1, const char_type* s2, std::size_t n )
		{
			const char_type* p1 = s1;
			const char_type* p2 = s2;
			std::size_t cc = n;

			for ( ; cc > 0; --cc, ++p1, ++p2 )
			{
				char_type c1 = details::tolower( *p1 );
				char_type c2 = details::tolower( *p2 );
				if ( !eq( c1, c2 ) )
					return ( lt( c1, c2 ) ) ? -1 : 1;
			}

			return 0;
		}
		
		static constexpr const char_type* find( const char_type* s, std::size_t n, const char_type& a )
		{
			const char_type* p = s;
			std::size_t cc = n;
			char_type c = details::tolower( a );

			for ( ; cc > 0; --cc, ++p )
			{
				if ( eq( details::tolower( *p ), c ) )
					return p;
			}

			return null;
		}
	};

	template < typename CharT, typename Allocator = std::allocator< CharT > >
	using basic_istring = std::basic_string< CharT, ichar_traits< CharT >, Allocator >;

	using istring = basic_istring< char >;
	using wistring = basic_istring< wchar_t >;
	using u16istring = basic_istring< char16_t >;
	using u32istring = basic_istring< char32_t >;
	using istring_t = basic_istring< char_t >;
#if MYCPP_STDCPP_VERSION >= 202002L
	using u8istring = basic_istring< char8_t >;
#endif
	template < typename CharT, typename Traits, typename Allocator >
	inline std::basic_string< CharT, ichar_traits< CharT >, Allocator > to_istring( const std::basic_string< CharT, Traits, Allocator >& s )
	{
		return { s.begin(), s.end() };
	}

	template < typename CharT, typename Allocator >
	inline std::basic_string< CharT, std::char_traits< CharT >, Allocator > to_std_string( const basic_istring< CharT, Allocator >& s )
	{
		return { s.begin(), s.end() };
	}
}

#if defined( MYCPP_GLOBALTYPEDES )
using mycpp::basic_istring;
using mycpp::istring;
using mycpp::wistring;
using mycpp::u16istring;
using mycpp::u32istring;
using mycpp::istring_t;
#if MYCPP_STDCPP_VERSION >= 202002L
using mycpp::u8istring;
#endif
#endif
