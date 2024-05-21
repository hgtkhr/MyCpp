#include "MyCpp/Win32Resource.hpp"
#include "MyCpp/Win32System.hpp"
#include "MyCpp/IntCast.hpp"

namespace MyCpp
{
	std::pair< void*, std::size_t > GetRawResourceData( HMODULE module, const char_t* type, const char_t* name, word language )
	{
		HRSRC resourceInfo = ::FindResourceEx( module, type, name, language );

		if ( resourceInfo != null )
		{
			dword size = ::SizeofResource( module, resourceInfo );
			HGLOBAL resouceData = ::LoadResource( module, resourceInfo );
			if ( resouceData != null )
			{
				void* ptr = ::LockResource( resouceData );
				if ( ptr != null )
					return std::make_pair( ptr, size );
			}
		}

		return null;
	}

	string_t GetStringResource( HINSTANCE instance, uint id )
	{
		vchar_t buffer( 1024 );

		adaptive_load( buffer, buffer.size(),
			[&] ( char_t* ptr, std::size_t n )
		{
			return LoadString( instance, id, ptr, numeric_cast< int >( n ) ) + 1;
		} );

		return buffer.data();
	}
}
