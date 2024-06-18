#pragma once

#ifndef __MYCPP_ERROR_HPP__
#define __MYCPP_ERROR_HPP__

#include <stdexcept>
#include "MyCpp/StringUtils.hpp"

#ifdef __clang__
#pragma clang diagnostic ignored "-Winvalid-token-paste"
#endif

#define FUNC_ERROR( calledFunction ) \
	_T( "[%s()] %s() Failed." ), _T( __FUNCTION__ ), _T( calledFunction )

#define FUNC_ERROR_ID( calledFunction, errorCode ) \
	_T( "[%s()] %s() Failed : 0x%08x" ), _T( __FUNCTION__ ), _T( calledFunction ), ( errorCode )

#define FUNC_ERROR_MSG( calledFunction, msg, ... ) \
	_T( "[%s()] %s() : " ## msg ), _T( __FUNCTION__ ), _T( calledFunction ), __VA_ARGS__

#define ERROR_MSG( msg, ... ) \
	_T( "[%s()] " ## msg ), _T( __FUNCTION__ ), __VA_ARGS__

namespace MyCpp
{
	namespace details
	{
		template < typename charT >
		class safe_text_buffer
		{
		public:
			safe_text_buffer() noexcept = default;

			safe_text_buffer( const safe_text_buffer& ) = delete;
			safe_text_buffer( const safe_text_buffer&& ) = delete;

			safe_text_buffer& operator = ( const safe_text_buffer& ) = delete;
			safe_text_buffer& operator = ( const safe_text_buffer&& ) = delete;

			~safe_text_buffer() noexcept
			{
				clear();
			}

			void clear() noexcept
			{
				if ( memptr != null && memptr != fixed_storage )
				{
					std::fill_n( memptr, memsize, charT() );
					std::free( reinterpret_cast< void* >( memptr ) );
				}
				else if ( memptr == fixed_storage )
				{
					std::fill_n( fixed_storage, count_of( fixed_storage ), charT() );
				}

				memptr = fixed_storage;
				memsize = count_of( fixed_storage );
			}

			std::pair< charT*, std::size_t > get() const noexcept
			{
				return std::make_pair( memptr, memsize );
			}

			std::pair< charT*, std::size_t > get( std::size_t desired ) noexcept
			{
				clear();

				if ( desired > count_of( fixed_storage ) )
				{
					memptr = reinterpret_cast< charT* >( std::malloc( desired * sizeof( charT ) ) );
					memsize = desired;
				}

				if ( memptr == null )
				{
					memptr = fixed_storage;
					memsize = count_of( fixed_storage );
				}

				std::fill_n( memptr, memsize, charT() );

				return std::make_pair( memptr, memsize );
			}
		private:
			charT fixed_storage[512] = {};
			charT* memptr = fixed_storage;
			std::size_t memsize = count_of( fixed_storage );
		};

		template < typename ... Args >
		inline void CreateErrorMessage( safe_text_buffer< char >& output, const char_t* fmt, const Args& ... args ) noexcept
		{
			safe_text_buffer< char_t > temp;
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
		details::safe_text_buffer< char > message;
		details::CreateErrorMessage( message, fmt, printf_arg( args ) ... );
		details::ThrowException< ExceptionType >( message.get().first );
	}

	template < typename ExceptionType >
	inline void exception( const ExceptionType& e )
	{
		details::ThrowException< ExceptionType >( e );
	}

	template < typename Notifer >
	inline void alert( const char* what, Notifer notfier )
	{
		details::safe_text_buffer< char_t > message;
		auto converter = narrow_wide_converter< char_t >( what );
		auto mem = message.get( converter.requires_size() );

		converter.convert( mem.first, mem.second );

		notfier( mem.first );
	}
}

#endif // ! __MYCPP_ERROR_HPP__
