/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	devel/devel.dmacvicar/SQLiteBackend.cc
*
*/
#include <iostream>
#include <ctime>
#include <fstream>
#include <sstream>
#include <streambuf>

#include "zypp/base/Logger.h"
#include "zypp/CapFactory.h"
#include "zypp/Source.h"

#include "zypp/ResObject.h"
#include "zypp/detail/ImplConnect.h"
#include "zypp/detail/ResObjectImplIf.h"
#include "zypp/detail/SelectionImplIf.h"

#include "serialize.h"
#include "xml_escape_parser.hpp"

using namespace std;
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
namespace storage
{ /////////////////////////////////////////////////////////////////

std::string xml_escape( const std::string &text )
{
  iobind::parser::xml_escape_parser parser;
  return parser.escape(text);
}

std::string xml_tag_enclose( const std::string &text, const std::string &tag, bool escape = false )
{
  std::string result;
  result += "<" + tag + ">";

  if ( escape)
   result += xml_escape(text);
  else
   result += text;

  result += "</" + tag + ">";
  return result;
}

/**
 * helper function that builds
 * <tagname lang="code">text</tagname>
 *
 *
 */
static std::string translatedTextToXML(const TranslatedText &text, const std::string &tagname)
{
  std::set<Locale> locales = text.locales();
  //ERR << "locale contains " << locales.size() << " translations" << std::endl;
  std::stringstream out;
  for ( std::set<Locale>::const_iterator it = locales.begin(); it != locales.end(); ++it)
  {
    //ERR << "serializing " << (*it).code() << std::endl;
    if ( *it == Locale() )
      out << "<" << tagname << ">" << xml_escape(text.text(*it)) << "</" << tagname << ">" << std::endl;
    else
      out << "<" << tagname << " lang=\"" << (*it).code() << "\">" << xml_escape(text.text(*it)) << "</" << tagname << ">" << std::endl;
  }
  return out.str();
}

template<class T>
std::string toXML( const T &obj ); //undefined

template<> 
std::string toXML( const Edition &edition )
{
  stringstream out;
  // sad the yum guys did not acll it edition
  out << "<version ver=\"" << xml_escape(edition.version()) << "\" rel=\"" << xml_escape(edition.release()) << "\"/>";
  return out.str();
}

template<> 
std::string toXML( const Arch &arch )
{
  stringstream out;
  out << xml_tag_enclose(xml_escape(arch.asString()), "arch");
  return out.str();
}

template<> 
std::string toXML( const Capability &cap )
{
  stringstream out;
  CapFactory factory;

  out << "<capability kind=\"" << cap.refers() << "\" >" <<  xml_escape(factory.encode(cap)) << "</capability>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const CapSet &caps )
{
  stringstream out;
  CapSet::iterator it = caps.begin();
  for ( ; it != caps.end(); ++it)
  {
    out << toXML((*it));
  }
  return out.str();
}

template<> 
std::string toXML( const Dependencies &dep )
{
  stringstream out;
  if ( dep[Dep::PROVIDES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::PROVIDES]), "provides") << std::endl;
  if ( dep[Dep::PREREQUIRES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::PREREQUIRES]), "prerequires") << std::endl;
  if ( dep[Dep::CONFLICTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::CONFLICTS]), "conflicts") << std::endl;
  if ( dep[Dep::OBSOLETES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::OBSOLETES]), "obsoletes") << std::endl;
  // why the YUM tag is freshen without s????
  if ( dep[Dep::FRESHENS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::FRESHENS]), "freshens") << std::endl;
  if ( dep[Dep::REQUIRES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::REQUIRES]), "requires") << std::endl;  
  if ( dep[Dep::RECOMMENDS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::RECOMMENDS]), "recommends") << std::endl;
  if ( dep[Dep::ENHANCES].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::ENHANCES]), "enhances") << std::endl;
  if ( dep[Dep::SUPPLEMENTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::SUPPLEMENTS]), "supplements") << std::endl;
  if ( dep[Dep::SUGGESTS].size() > 0 )
    out << "    " << xml_tag_enclose(toXML(dep[Dep::SUGGESTS]), "suggests") << std::endl;
  return out.str();

}

template<> 
std::string toXML( const Resolvable::constPtr &obj )
{
  stringstream out;

  out << "  <name>" << xml_escape(obj->name()) << "</name>" << std::endl;
  // is this shared? uh
  out << "  " << toXML(obj->edition()) << std::endl;
  out << "  " << toXML(obj->arch()) << std::endl;
  out << "  " << toXML(obj->deps()) << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Package::constPtr &obj )
{
  stringstream out;
  out << "<package>" << std::endl;
  // reuse Resolvable information serialize function
  toXML(static_cast<Resolvable::constPtr>(obj));
  //out << "  <do>" << std::endl;
  //out << "      " << obj->do_script() << std::endl;
  //out << "  </do>" << std::endl;
  out << "</package>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Script::constPtr &obj )
{
  stringstream out;
  out << "<script>" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << "  <do>" << std::endl;
  out << "      " << xml_escape(obj->do_script()) << std::endl;
  out << "  </do>" << std::endl;
  if ( obj->undo_available() )
  {
    out << "  <undo>" << std::endl;
    out << "      " << xml_escape(obj->undo_script()) << std::endl;
    out << "  </undo>" << std::endl;
  }
  out << "</script>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Message::constPtr &obj )
{
  stringstream out;
  out << "<message>" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  out << "  <text>" << xml_escape(obj->text().text()) << "</text>" << std::endl;
  out << "</message>" << std::endl;
  return out.str();
}

/*

NOT NEEDED FOR NOW, WE JUST LOSE THE TRANSLATION AT STORAGE TIME

static std::string localizedXMLTags( const TranslatedText &text, const std::string &tagname)
{
  stringstream out;
  std::set<Locale> locales = text.locales();
  for(std::set<Locale>::const_iterator it = locales.begin(); it != locales.end(); ++it)
  {
    Locale lcl = *it;
    out << "    <" << tagname << " lang='" << lcl.name() << "'>" << text.text(lcl) << "</" << tagname <<">" << std::endl;
  }
  return out.str();
}
*/

template<> 
std::string toXML( const Selection::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<pattern>" << std::endl;
  //out << "  <name>" << xml_escape(obj->name()) << "</name>" << std::endl;

  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;

  // access implementation
  detail::ResImplTraits<Selection::Impl>::constPtr sipp( detail::ImplConnect::resimpl( obj ) );
  out << translatedTextToXML(sipp->summary(), "summary");
  
  //out << "  <default>" << (obj->isDefault() ? "true" : "false" ) << "</default>" << std::endl;
  out << "  <uservisible>" << (obj->visible() ? "true" : "false" ) << "</uservisible>" << std::endl;
  out << "  <category>" << xml_escape(obj->category()) << "</category>" << std::endl;
  out << "  <icon>null</icon>" << std::endl;
  out << translatedTextToXML(sipp->description(), "description");
  out << "</pattern>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Atom::constPtr &obj )
{
  stringstream out;
  out << "<atom>" << std::endl;
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;

  // access implementation
  out << toXML(obj->deps()) << std::endl;
  out << "</atom>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Pattern::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<pattern>" << std::endl;
  out << "  <name>" << xml_escape(obj->name()) << "</name>" << std::endl;

  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;

  // access implementation
  detail::ResImplTraits<Pattern::Impl>::constPtr pipp( detail::ImplConnect::resimpl( obj ) );
  out << translatedTextToXML(pipp->summary(), "summary");

  out << "  <default>" << (obj->isDefault() ? "true" : "false" ) << "</default>" << std::endl;
  out << "  <uservisible>" << (obj->userVisible() ? "true" : "false" ) << "</uservisible>" << std::endl;
  out << "  <category>" << xml_escape(obj->category()) << "</category>" << std::endl;
  out << "  <icon>" << xml_escape(obj->icon().asString()) << "</icon>" << std::endl;
  out << "  <script>" << xml_escape(obj->script().asString()) << "</script>" << std::endl;
  out << translatedTextToXML(pipp->description(), "description");
  out << "</pattern>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const Product::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<product type=\"" << xml_escape(obj->category()) << "\">" << std::endl;
  out << "  <vendor>" << xml_escape(obj->vendor()) << "</vendor>" << std::endl;
  out << "  <source>" << xml_escape(obj->source().alias()) << "</source>" << std::endl;
  out << toXML(static_cast<Resolvable::constPtr>(obj)) << std::endl;
  #warning "FIXME description and displayname of products"
  out << "  <displayname>" << xml_escape(obj->displayName()) << "</displayname>" << std::endl;
  out << "  <release-notes-url>" << xml_escape(obj->releaseNotesUrl().asString()) << "</release-notes-url>" << std::endl;
  out << "  <description></description>" << std::endl;
  out << "</product>" << std::endl;

  return out.str();
}


std::string castedToXML( const Resolvable::constPtr &resolvable )
{
  stringstream out;
  if ( isKind<Package>(resolvable) )
     out << toXML(asKind<const Package>(resolvable)) << std::endl;
  if ( isKind<Patch>(resolvable) )
     out << toXML(asKind<const Patch>(resolvable)) << std::endl;
  if ( isKind<Message>(resolvable) )
     out << toXML(asKind<const Message>(resolvable)) << std::endl;
  if ( isKind<Script>(resolvable) )
     out << toXML(asKind<const Script>(resolvable)) << std::endl;
  if ( isKind<Atom>(resolvable) )
     out << toXML(asKind<const Atom>(resolvable)) << std::endl;
  if ( isKind<Product>(resolvable) )
     out << toXML(asKind<const Product>(resolvable)) << std::endl;
  if ( isKind<Pattern>(resolvable) )
     out << toXML(asKind<const Pattern>(resolvable)) << std::endl;
  if ( isKind<Selection>(resolvable) )
     out << toXML(asKind<const Selection>(resolvable)) << std::endl;
  return out.str();
}


std::string resolvableTypeToString( const Resolvable::constPtr &resolvable, bool plural )
{
  return resolvableKindToString(resolvable->kind(), plural);
}

std::string resolvableKindToString( const Resolvable::Kind &kind, bool plural)
{
  if ( kind == ResTraits<zypp::Package>::kind )
     return plural ? "packages" : "package";
  else if ( kind == ResTraits<zypp::Patch>::kind )
     return plural ? "patches" : "patch";
  else if ( kind == ResTraits<zypp::Atom>::kind )
     return plural ? "atoms" : "atom";
  else if ( kind == ResTraits<zypp::Message>::kind )
     return plural ? "messages" : "message";
  else if ( kind == ResTraits<zypp::Selection>::kind )
     return plural ? "selections" : "selection";
  else if ( kind == ResTraits<zypp::Script>::kind )
     return plural ? "scripts" : "script";
  else if ( kind == ResTraits<zypp::Pattern>::kind )
     return plural ? "patterns" : "pattern";
  else if ( kind == ResTraits<zypp::Product>::kind )
     return plural ? "products" : "product";
  else
     return "unknown";
}

template<> 
std::string toXML( const Patch::constPtr &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<patch" << std::endl; 
  out << "    patchid=\"" << xml_escape(obj->id()) << "\"" << std::endl;
  out << "    timestamp=\"" << obj->timestamp().asSeconds() << "\"" << std::endl;
  out << "    engine=\"1.0\">" << std::endl;
  // reuse Resolvable information serialize function
  out << toXML(static_cast<Resolvable::constPtr>(obj));
  Patch::AtomList at = obj->atoms();
  out << "  <atoms>" << std::endl;
  for (Patch::AtomList::iterator it = at.begin(); it != at.end(); it++)
  {
    // atoms tag here looks weird but lets follow YUM
    
    // I have a better idea to avoid the cast here (Michaels code in his tmp/)
    Resolvable::Ptr one_atom = *it;
    out << castedToXML(one_atom) << std::endl;
  }
  out << "  </atoms>" << std::endl;
  out << "</patch>" << std::endl;
  return out.str();
}

template<> 
std::string toXML( const PersistentStorage::SourceData &obj )
{
  stringstream out;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
  out << "<source  xmlns=\"http://novell.com/package/metadata/suse/source\">" << std::endl;
  out << "  <enabled>" << (obj.enabled ? "true" : "false" ) << "</enabled>" << std::endl;
  out << "  <auto-refresh>" << ( obj.autorefresh ? "true" : "false" ) << "</auto-refresh>" << std::endl;
  out << "  <product-dir>" << xml_escape(obj.product_dir) << "</product-dir>" << std::endl;
  out << "  <cache-dir>" << xml_escape(obj.cache_dir) << "</cache-dir>" << std::endl;
  out << "  <type>" << xml_escape(obj.type) << "</type>" << std::endl;
   out << "  <url>" << xml_escape(obj.url) << "</url>" << std::endl;
   out << "  <alias>" << xml_escape(obj.alias) << "</alias>" << std::endl;
  out << "</source>" << std::endl;
  return out.str();
}

/////////////////////////////////////////////////////////////////
} // namespace storage
	///////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
