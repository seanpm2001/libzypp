/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_MediaSetAccess_H
#define ZYPP_MediaSetAccess_H

#include <iosfwd>
#include <string>
#include <vector>
#include <boost/function.hpp>

#include "zypp/base/ReferenceCounted.h"
#include "zypp/base/NonCopyable.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/media/MediaManager.h"
#include "zypp/Pathname.h"
#include "zypp/CheckSum.h"
#include "zypp/OnMediaLocation.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

    DEFINE_PTR_TYPE(MediaSetAccess);

    typedef boost::function<bool ( const Pathname &file )> FileChecker;

    class NullFileChecker
    {
      public:
      bool operator()( const Pathname &file );
    };

    class ChecksumFileChecker
    {
      public:
      ChecksumFileChecker( const CheckSum &checksum );
      bool operator()( const Pathname &file );
      private:
      CheckSum _checksum;
    };

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : MediaSetAccess
    //
    /**
     * Media access layer responsible for handling media distributed as a set.
     *
     * This is provided as a means to handle CD or DVD sets accessible through
     * dir, iso, nfs or other URL schemes other than cd/dvd (see
     * \ref MediaManager for info on different implemented media backends).
     * Currently it handles URLs containing cdN, CDN, dvdN, and DVDN strings,
     * where N is the number of particular media in the set.
     * 
     * Examples:
     * \code
     * "iso:/?iso=/path/to/iso/images/openSUSE-10.3-Alpha2plus-DVD-x86_64-DVD1.iso"
     * "dir:/path/to/cdset/sources/openSUSE-10.3/Alpha2plus/CD1"
     * \endcode
     * 
     * MediaSetAccess accesses files on desired media by rewriting
     * the original URL, replacing the digit (usually) 1 with requested media
     * number and uses \ref MediaManager to get the files from the new URL.
     * 
     * Additionaly, each media number can be assined a media verifier which
     * checks if the media we are trying to access is the desired one. See
     * \ref MediaVerifierBase for more info.
     * 
     * Code example:
     * \code
     * Url url("dir:/path/to/cdset/sources/openSUSE-10.3/Alpha2plus/CD1");
     * 
     * MediaSetAccess access(url, "/");
     * 
     * access.setVerifier(1, media1VerifierRef);
     * access.setVerifier(2, media2VerifierRef);
     * 
     * Pathname file1 = "/some/file/on/media1";
     * access.provideFile(1, file1);
     * Pathname file2 = "/some/file/on/media2";
     * access.provideFile(2, file1);
     *
     * \endcode
     */
    class MediaSetAccess : public base::ReferenceCounted, private base::NonCopyable
    {
      friend std::ostream & operator<<( std::ostream & str, const MediaSetAccess & obj );

    public:
      /**
       * creates a callback enabled media access  for \param url and \param path.
       * with only 1 media no verified
       */
      MediaSetAccess( const Url &url, const Pathname &path );
      ~MediaSetAccess();

      /**
       * Sets a \ref MediaVerifierRef verifier for given media number
       */
      void setVerifier( unsigned media_nr, media::MediaVerifierRef verifier );
      
      /**
       * provide a file from a media location.
       */
      Pathname provideFile( const OnMediaLocation & on_media_file );

      Pathname provideFile(const Pathname & file, unsigned media_nr = 1 );
      Pathname provideFile(const Pathname & file, unsigned media_nr, const FileChecker checker );

      static Url rewriteUrl (const Url & url_r, const media::MediaNr medianr);

    protected:
      Pathname provideFileInternal(const Pathname & file, unsigned media_nr, bool checkonly, bool cached);
      media::MediaAccessId getMediaAccessId (media::MediaNr medianr);
      virtual std::ostream & dumpOn( std::ostream & str ) const;

    private:
      /** Media or media set URL */
      Url _url;
      /** Path on the media relative to _url */
      Pathname _path;

      typedef std::map<media::MediaNr, media::MediaAccessId> MediaMap;
      typedef std::map<media::MediaNr, media::MediaVerifierRef > VerifierMap;

      /** Mapping between media number and Media Access ID */
      MediaMap _medias;
      /** Mapping between media number and corespondent verifier */
      VerifierMap _verifiers;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates MediaSetAccess Stream output */
    inline std::ostream & operator<<( std::ostream & str, const MediaSetAccess & obj )
    { return obj.dumpOn( str ); }


} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SOURCE_MediaSetAccess_H
