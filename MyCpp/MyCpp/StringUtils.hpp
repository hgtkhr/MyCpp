#pragma once
#include <cstdarg>
#include <iterator>
#include <algorithm>
#include "MyCpp/Base.hpp"

namespace MyCpp
{
	namespace Details
	{
		std::size_t XStrToYStr( const char* from, std::size_t length, wchar_t* to, std::size_t capacity );
		std::size_t XStrToYStr( const wchar_t* from, std::size_t length, char* to, std::size_t capacity );

		template < typename charX, typename charY >
		inline std::size_t XStrToYStr( const charX* from, std::size_t length, charY* to, std::size_t capacity )
		{
			if ( capacity <= 0 )
				return ( to == null ) ? length : 0;
#if defined( _MSC_VER ) && _MSC_VER >= 1400 
			auto p = std::transform(
				from
				, from + length
				, stdext::make_unchecked_array_iterator( to )
				, [] ( const charX& c ) 
					{
						return charY( c );
					} );

			return static_cast< std::size_t >( p.base() - to );
#else
			auto p = std::transform(
				from
				, from + length
				, to
				, [] ( const charX& c )
			{
				return charY( c );
			} );

			return static_cast< std::size_t >( p - to );
#endif
		}
	}

	template < typename CharTo, typename CharFrom >
	class narrow_wide_convert
	{
	public:
		narrow_wide_convert()
		{}
			
		narrow_wide_convert( const CharFrom* source, std::size_t length )
			: m_source( source )
			, m_length( length )
		{}

		~narrow_wide_convert()
		{}

		std::size_t convert( CharTo* to, std::size_t size ) const
		{
			if ( size == 0 )
				return 0;

			std::size_t len = ( m_length >= size ) ? size - 1 : m_length;
			len = Details::XStrToYStr( m_source, len, to, size );
			to[len] = CharTo();

			return len;
		}

		std::size_t requires_size() const
		{
			return Details::XStrToYStr( m_source, m_length, static_cast< CharTo* >( null ), 0 ) + 1;
		}
	private:
		const CharFrom* m_source = null;
		std::size_t m_length = 0;
	};

	template < typename CharTo, typename CharFrom >
	inline narrow_wide_convert< CharTo, CharFrom > narrow_wide_converter( const CharFrom* str, std::size_t length )
	{
		return { str, length };
	}

	template < typename CharTo, typename CharFrom >
	inline narrow_wide_convert< CharTo, CharFrom > narrow_wide_converter( const CharFrom* str )
	{
		return { str, std::char_traits< CharFrom >::length( str ) };
	}

	template < typename StringTo, typename StringFrom >
	inline StringTo narrow_wide_string( const StringFrom& from )
	{
		typedef typename StringTo::value_type CharTo;

		auto converter = narrow_wide_converter< CharTo >( from.c_str(), from.length() );
		std::vector< CharTo > buffer( converter.requires_size() );
		std::size_t r = converter.convert( cstr_t( buffer ), buffer.size() );

		return { cstr_t( buffer ), r };
	}

	template < typename T >
	inline const T& printf_arg( const T& value ) noexcept
	{
		return value;
	}

	template < typename T >
	inline const T* printf_arg( const T* value ) noexcept
	{
		return value;
	}

	template < typename charT >
	inline const charT* printf_arg( const std::vector< charT >& value ) noexcept
	{
		return cstr_t( value );
	}

	template < typename charT >
	inline const charT* printf_arg( const std::basic_string< charT >& value ) noexcept
	{
		return value.c_str();
	}

	namespace Details
	{
		template < typename ... Args >
		inline int vcsprintf( vchar& result, const char* fmt, const Args& ... args )
		{
			int r = _scprintf( fmt, printf_arg( args ) ... );

			if ( r != -1 )
			{
				result.resize( r + 1 );
				r = sprintf_s( result.data(), result.size(), fmt, printf_arg( args ) ... );
			}

			return r;
		}

		template < typename ... Args >
		inline int vcsprintf( vwchar& result, const wchar_t* fmt, const Args& ... args )
		{
			int r = _scwprintf( fmt, printf_arg( args ) ... );

			if ( r != -1 )
			{
				result.resize( r + 1 );
				r = swprintf_s( result.data(), result.size(), fmt, printf_arg( args ) ... );
			}

			return r;
		}
	}

	template < typename charT, typename ... Args >
	inline std::vector< charT > vcsprintf( const charT* fmt, const Args& ... args )
	{
		std::vector< charT > result;

		int r = Details::vcsprintf( result, fmt, args ... );
		if ( r == -1 )
			throw std::runtime_error( "vcsprintf" );

		return result;
	}

	template < typename charT, typename ... Args  >
	inline std::basic_string< charT > strprintf( const charT* fmt, const Args& ... args )
	{
		std::vector< charT > result;

		int r = Details::vcsprintf( result, fmt, args ... );
		if ( r == -1 )
			throw std::runtime_error( "strprintf" );

		return cstr_t( result );
	}

	namespace
	{
		inline int strcmp_icase( const char* s1, const char* s2 )
		{
			return _stricmp( s1, s2 );
		}

		inline int strcmp_icase( const wchar_t* s1, const wchar_t* s2 )
		{
			return _wcsicmp( s1, s2 );
		}
	}

	template < typename charT >
	inline charT* stristr( const charT* s1, const charT* s2 )
	{
		typedef std::char_traits< charT > traits;

		const static charT nullCh = charT();

		if ( traits::eq( *s2, nullCh ) )
			return const_cast< charT* >( s1 );

		const charT* haystack = s1;
		const charT* needle = s2;
		for ( ; !traits::eq( *haystack, nullCh ) && !traits::eq( *needle, nullCh ); ++haystack )
		{
			if ( strcmp_icase( haystack, needle ) == 0 )
				return const_cast< charT* >( haystack );
		}

		return null;
	}
}
