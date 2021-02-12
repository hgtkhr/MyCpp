#pragma once
#include <stdexcept>
#include "MyCpp/StringUtils.hpp"

#define FUNC_ERROR( calledFunction ) \
	_T( "[%s()] %s() Failed." ), _T( __FUNCTION__ ), _T( calledFunction )

#define FUNC_ERROR_ID( calledFunction, errorCode ) \
	_T( "[%s()] %s() Failed : 0x%08x" ), _T( __FUNCTION__ ), _T( calledFunction ), ( errorCode )

#define FUNC_ERROR_MSG( calledFunction, msg, ... ) \
	_T( "[%s()] %s() : " ## msg ), _T( __FUNCTION__ ), _T( calledFunction ), __VA_ARGS__

namespace MyCpp
{
	namespace Details
	{
		template < typename charT >
		struct buffer
		{
			charT fixed_storage[512];
			charT* memptr = null;
			std::size_t memsize = 0;

			void clear()
			{
				if ( memptr != null && memptr != fixed_storage )
					std::free( reinterpret_cast< void* >( memptr ) );

				memptr = null;
				memsize = 0;
			}

			std::pair< charT*, std::size_t > get() const
			{
				return std::make_pair( memptr, memsize );
			}

			std::pair< charT*, std::size_t > get( std::size_t desired )
			{
				clear();

				if ( desired > length_of( fixed_storage ) )
				{
					memptr = reinterpret_cast< charT* >( std::malloc( desired * sizeof( charT ) ) );
					memsize = desired;
				}

				if ( memptr == null )
				{
					memptr = fixed_storage;
					memsize = length_of( fixed_storage );
				}

				std::fill_n( memptr, memsize, charT() );

				return std::make_pair( memptr, memsize );
			}

			buffer()
			{
				std::fill_n( fixed_storage, length_of( fixed_storage ), charT() );
			}

			~buffer()
			{
				clear();
			}
		};

		template < typename ... Args >
		inline void CreateErrorMessage( buffer< char >& output, const char_t* fmt, const Args& ... args ) noexcept
		{
			buffer< char_t > temp;
			auto a = temp.get( _sctprintf( fmt, args ... ) + 1 );
			_sntprintf_s( a.first, a.second, _TRUNCATE, fmt, args ... );

			auto converter = narrow_wide_converter< char >( a.first );
			auto b = output.get( converter.requires_size() );
			converter.convert( b.first, b.second );
		}

		template < typename Exception >
		void ThrowException( const char* msg )
		{
			throw Exception( msg );
		}

		template < typename Exception >
		void ThrowException( const Exception& e )
		{
			throw Exception( e );
		}
	}

	template < typename ExceptionType, typename ... Args >
	inline void exception( const char_t* fmt, const Args& ... args )
	{
		Details::buffer< char > message;
		Details::CreateErrorMessage( message, fmt, printf_arg( args ) ... );
		Details::ThrowException< ExceptionType >( message.get().first );
	}

	template < typename ExceptionType >
	inline void exception( const ExceptionType& e )
	{
		Details::ThrowException< ExceptionType >( e );
	}

	template < typename Notifer >
	inline void alert( const char* what, Notifer notfier )
	{
		Details::buffer< char_t > message;
		auto converter = narrow_wide_converter< char_t >( what );
		auto mem = message.get( converter.requires_size() );
		converter.convert( mem.first, mem.second );
		notfier( mem.first );
	}
}
