/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/Patch.cc
 *
*/
#include "zypp/Patch.h"
#include "zypp/Message.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE( Patch );

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Patch::Patch
  //	METHOD TYPE : Ctor
  //
  Patch::Patch( const sat::Solvable & solvable_r )
  : ResObject( solvable_r )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : Patch::~Patch
  //	METHOD TYPE : Dtor
  //
  Patch::~Patch()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	Patch interface forwarded to implementation
  //
  ///////////////////////////////////////////////////////////////////

  std::string Patch::category() const
  { return lookupStrAttribute( sat::SolvAttr::patchcategory ); }

  bool Patch::reboot_needed() const
  { return lookupBoolAttribute( sat::SolvAttr::needReboot ); }

  bool Patch::affects_pkg_manager() const
  { return lookupBoolAttribute( sat::SolvAttr::needRestart ); }

  bool Patch::interactive() const
  {
    if ( reboot_needed()
         || ! licenseToConfirm().empty() )
    {
      return true;
    }

    Patch::Contents c( contents() );
    for_( it, c.begin(), c.end() )
    {
      if ( it->isKind( ResKind::message )
           || ! licenseToConfirm().empty() )
      {
        return true;
      }
    }

    return false;
  }


  Patch::Contents Patch::contents() const
  {
#warning FILL contents
    return Contents();
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
