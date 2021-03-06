#pragma once
#include <cstdint>
#include <limits>
#include <algorithm>
#include <type_traits>
#include "MyCpp/Base.hpp"

namespace MyCpp
{
	namespace Details
	{
		typedef std::nullptr_t enabler_t;

		constexpr enabler_t enabler = nullptr;

		template < bool Cond >
		using type_enable_if = typename std::enable_if< Cond, enabler_t >::type;

	// unsigned int -> pointer
		template <
			typename T,
			typename U,
			type_enable_if< std::is_unsigned< U >::value > = enabler
		>
			inline
			typename std::enable_if< std::is_pointer< T >::value, T >::type
			reinterpret_pointer( U value )
		{
			return reinterpret_cast< T >( static_cast< std::uintptr_t >( value ) );
		}

		// signed int -> pointer
		template <
			typename T,
			typename U,
			type_enable_if< !std::is_unsigned< U >::value > = enabler
		>
			inline
			typename std::enable_if< std::is_pointer< T >::value, T >::type
			reinterpret_pointer( U value )
		{
			return reinterpret_cast< T >( static_cast< std::intptr_t >( value ) );
		}

		// pointer -> unsigned int 
		template <
			typename T,
			typename U,
			type_enable_if< std::is_pointer< U >::value > = enabler
		>
			inline
			typename std::enable_if< std::is_unsigned< T >::value, T >::type
			reinterpret_pointer( U value )
		{
			return static_cast< T >( reinterpret_cast< std::uintptr_t >( value ) );
		}

		// pointer -> signed int 
		template <
			typename T,
			typename U,
			type_enable_if< std::is_pointer< U >::value > = enabler
		>
			inline
			typename std::enable_if< !std::is_unsigned< T >::value, T >::type
			reinterpret_pointer( U value )
		{
			return static_cast< T >( reinterpret_cast< std::intptr_t >( value ) );
		}

		// unsigned intX -> unsigned intY
		template <
			typename T,
			typename U,
			type_enable_if< std::is_unsigned< U >::value > = enabler
		>
			inline
			typename std::enable_if< std::is_unsigned< T >::value, T >::type
			trunc_over_numeric_limits( U value )
		{
			if ( value > std::numeric_limits< T >::max() )
				return std::numeric_limits< T >::max();

			return static_cast< T >( value );
		}

		// signed intX -> unsigned intY
		template <
			typename T,
			typename U,
			type_enable_if< !std::is_unsigned< U >::value > = Details::enabler
		>
			inline
			typename std::enable_if< std::is_unsigned< T >::value, T >::type
			trunc_over_numeric_limits( U value )
		{
			typedef typename std::make_unsigned< U >::type unsigned_U;

			unsigned_U temp = static_cast< unsigned_U >( std::max( value, static_cast< U >( 0 ) ) );

			if ( temp > std::numeric_limits< T >::max() )
				return std::numeric_limits< T >::max();

			return static_cast< T >( temp );
		}

		// unsigned intX -> signed intY
		template <
			typename T,
			typename U,
			type_enable_if< std::is_unsigned< U >::value > = Details::enabler
		>
			inline
			typename std::enable_if< !std::is_unsigned< T >::value, T >::type
			trunc_over_numeric_limits( U value )
		{
			typedef typename std::make_signed< U >::type signed_U;

			signed_U temp = static_cast< signed_U >(
				std::min( value, static_cast< U >( std::numeric_limits< signed_U >::max() ) ) );

			if ( temp > std::numeric_limits< T >::max() )
				return std::numeric_limits< T >::max();

			return static_cast< T >( temp );
		}

		// signed intX -> signed intY
		template <
			typename T,
			typename U,
			type_enable_if< !std::is_unsigned< U >::value > = Details::enabler
		>
			inline
			typename std::enable_if< !std::is_unsigned< T >::value, T >::type
			trunc_over_numeric_limits( U value )
		{
			if ( value > std::numeric_limits< T >::max() )
				return std::numeric_limits< T >::max();

			return static_cast< T >( value );
		}
	}

	template < typename T, typename U >
	inline T numeric_cast( U value )
	{
		return Details::trunc_over_numeric_limits< T >( value );
	}

	template < typename T, typename U >
	inline T pointer_int_cast( U value )
	{
		return Details::reinterpret_pointer< T >( value );
	}
}
