
extern "C"
{
#include <sys/file.h>
}
#include <iostream>
#include <fstream>

#include "zypp/base/Logger.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/IOStream.h"
#include "zypp/base/InterProcessMutex.h"
#include "zypp/base/String.h"

#include "zypp/Pathname.h"
#include "zypp/PathInfo.h"

#define ZYPP_LOCK_FILE "/var/run/zypp.pid"
#define LMIL MIL << "LOCK [" << _name << "] "

using namespace std;

namespace zypp
{
namespace base
{

ZYppLockedException::ZYppLockedException( const std::string & msg_r,
                                          const std::string &name,
                                          pid_t locker_pid )
    : Exception(msg_r)
    , _locker_pid (locker_pid)
    , _name(name)
{}

ZYppLockedException::~ZYppLockedException() throw()
{}
    

InterProcessMutex::InterProcessMutex( ConsumerType ctype,
                                      const std::string &name,
                                      int timeout )
  : _fd(0)
  , _name(name)
  , _timeout(timeout)
  , _type(ctype)
{

    // get the current pid
    pid_t curr_pid = getpid();
    Pathname lock_file = lockFilePath();
    int totalslept = 0;
    int k = 0;
    
    while (1)
    {
        k++;
        
        // try to create the lock file atomically, this will fail if
        // the lock exists
        if ( ( _fd = open(lock_file.c_str(), O_WRONLY | O_CREAT | O_EXCL) ) == -1 )
        {
            struct flock lock;
            
            // the file exists, lets see if someone has it locked exclusively
            if ( (_fd = open(lock_file.c_str(), O_RDONLY)) == -1 )
            {
                ZYPP_THROW(Exception(str::form(_("It %d, Can't open lock file: %s"), k, strerror(errno))));
            }
            
            memset(&lock, 0, sizeof(struct flock));
            lock.l_whence = SEEK_SET;

            // GETLK tells you the conflicting lock as if the lock you pass
            // would have been set. So set the lock type depending on wether
            // we are a writer or a reader.
            lock.l_type = ( ( _type == Writer ) ? F_WRLCK : F_RDLCK );
            

            // get lock information
            if (fcntl(_fd, F_GETLK, &lock) < 0)
            {
                close(_fd);
                ZYPP_THROW(Exception(string("Error getting lock info: ") +  strerror(errno)));
            }

            
            MIL << lock_file << " : ";
            switch ( lock.l_type )
            {
                case F_WRLCK:
                    MIL << " Write-Lock conflicts" << endl;
                    break;
                case F_RDLCK:
                    MIL << " Read-Lock conflicts" << endl;                    
                    break;
                case F_UNLCK:
                    MIL << " No lock conflicts" << endl;
                    break;
                default:
                    break;
                    
            }
            
            // F_GETLK is confusing
            // http://groups.google.com/group/comp.unix.solaris/tree/browse_frm/month/2005-09/123fae2c774bceed?rnum=61&_done=%2Fgroup%2Fcomp.unix.solaris%2Fbrowse_frm%2Fmonth%2F2005-09%3F
            // new table of access
            // F_WRLCK   Reader  Wait or abort
            // F_WRLCK   Writer  Wait or abort
            // F_RDLCK   Writer  Wait or abort
            // F_RDLCK   Reader  Can't happen, anyway, wait or abort            
            // F_UNLCK   Reader  Take reader lock
            // F_UNLCK   Writer  Take writer lock
            
            

            // wait or abort
            if (  lock.l_type != F_UNLCK )
            {
                // some lock conflicts with us.
                LMIL << "pid " << lock.l_pid << " is running and has a lock that conflicts with us." << std::endl;
                
                // abort if we have slept more or equal than the timeout, but
                // not for the case where timeout is negative which means no
                // timeout and therefore we never abort.
                if ( (totalslept >= _timeout) && (_timeout >= 0 ) )
                {
                    close(_fd);
                    ZYPP_THROW(ZYppLockedException(                                       
                                   _("This action is being run by another program already."),
                                   _name, lock.l_pid));
                }
                        
                // if not, let sleep one second and count it
                LMIL << "waiting 1 second..." << endl;
                close(_fd);
                sleep(1);
                ++totalslept;
                continue;
            }
            else if ( ( lock.l_type == F_UNLCK ) && ( _type == Reader ) )
            {
                // either there is no lock or a reader has it so we just
                // acquire a reader lock.
                // TODO to know wether there is no lock over the file or there
                // is a reader, we would need to test for a writer lock
                // that conflicts with it.
                // 
                // try to get more lock info
                lock.l_type = F_WRLCK;
 
                if (fcntl(_fd, F_GETLK, &lock) < 0)
                {
                    close(_fd);
                    ZYPP_THROW(Exception(string("Error getting lock info: ") +  strerror(errno)));
                }
                
                if ( lock.l_type == F_UNLCK )
                {
                    LMIL << "no previous readers, unlinking lock file and retrying." << endl;
                    
                    // actually there are no readers
                    // lets delete it and break, so the next loop will
                    // probably suceed in creating it. The worst thing that can
                    // happen is that another process will take it first, but
                    // we are not aiming at such level of correctness. Otherwise
                    // the code path will complicate too much.
                    memset(&lock, 0, sizeof(struct flock));
                    lock.l_type = F_WRLCK;
                    lock.l_whence = SEEK_SET;
                    lock.l_pid = getpid();

                    if (fcntl(_fd, F_SETLK, &lock) < 0)
                    {
                        close(_fd);
                        ZYPP_THROW (Exception( "Can't lock file to unlink it."));
                    }
                    filesystem::unlink(lock_file.c_str());
                    close(_fd);
                    continue;
                }
                else if ( lock.l_type == F_RDLCK )
                {
                    // there is another reader.
                    LMIL << "previous readers on lock file. taking lock as a reader." << std::endl;
                    memset(&lock, 0, sizeof(struct flock));
                    lock.l_type = F_RDLCK;
                    lock.l_whence = SEEK_SET;
                    lock.l_pid = getpid();

                    if (fcntl(_fd, F_SETLK, &lock) < 0)
                    {
                        close(_fd);
                        ZYPP_THROW (Exception( "Can't lock file for reader"));
                    }
                    // and keep the lock open.
                    break;
                }
                else
                {
                    // cant happen!
                    ERR << "impossible condition" << endl;
                    
                    break;
                }
            }
            else if ( ( lock.l_type == F_UNLCK ) && ( _type == Writer ) )
            {
                LMIL << "stale lock found" << endl;
                // Nobody conflicts with a writer lock so nobody is actually
                // locking.
                // lets delete it and break, so the next loop will
                // probably suceed in creating it. The worst thing that can
                // happen is that another process will take it first, but
                // we are not aiming at such level of correctness. Otherwise
                // the code path will complicate too much.
                memset(&lock, 0, sizeof(struct flock));
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_pid = getpid();

                if (fcntl(_fd, F_SETLK, &lock) < 0)
                {
                    close(_fd);
                    ZYPP_THROW (Exception( "Can't lock file to unlink it."));
                }
                filesystem::unlink(lock_file.c_str());
                close(_fd);
                continue;
            } 
            else 
            {
                // undefined case, just get out of the loop
                LMIL << "undefined case!" << endl;
                
                break;
            }
            
        }
        else 
        {
            // exclusive file creation succeeded. So may be we are the
            // first writer or first reader
            
            // try to lock it exclusively
            // if it fails, someone won us, so we just go for another try
            // or just abort
            LMIL << "no lock found, taking ownership of it as a " << ( (_type == Reader ) ? "reader" : "writer" ) << endl;
            struct flock lock;
            memset(&lock, 0, sizeof(struct flock));
            lock.l_whence = SEEK_SET;
            lock.l_type = F_WRLCK;
            lock.l_pid = getpid();

            if (fcntl(_fd, F_SETLK, &lock) < 0)
            {
                close(_fd);
                ZYPP_THROW (Exception( "Can't lock file to write pid."));
            }
            
            char buffer[100];
            sprintf( buffer, "%d\n", curr_pid);
            write( _fd, buffer, strlen(buffer));
            
            // by now the pid is written and the file locked.
            // If we are a reader, just downgrade the lock to
            // read shared lock.
            if ( _type == Reader )
            {
                lock.l_type = F_RDLCK;
               
                if (fcntl(_fd, F_SETLK, &lock) < 0)
                {
                    close(_fd);
                    ZYPP_THROW (Exception( "Can't set lock file to shared"));
                }
            }
            
            break;
        }           
    } // end loop       

    MIL << "Finish constructor" << endl;
    
}

InterProcessMutex::~InterProcessMutex()
{
    try
    {
        Pathname lock_file = lockFilePath();
        switch ( _type )
        {
            case Reader:
                
                break;
                
            case Writer:
                // we are the only writer, so unlink the file
                filesystem::unlink(lock_file.c_str());
                break;
                
        }
        // and finally close the file and release the lock
        close(_fd);
    }
    catch(...) {} // let no exception escape.
}


Pathname InterProcessMutex::lockFilePath() const
{
    return Pathname("/var/run/zypp-" + _name + ".pid");
}    

bool InterProcessMutex::isProcessRunning(pid_t pid_r)
{
    // it is another program, not me, see if it is still running
    Pathname procdir( Pathname("/proc") / str::numstring(pid_r) );

    PathInfo status( procdir/"status" );
    XXX << "Checking " <<  status << endl;
    bool still_running = status.isExist();

    if ( still_running )
    {
        Pathname p( procdir/"exe" );
        XXX << p << " -> " << filesystem::readlink( p ) << endl;

        p = procdir/"cmdline";
        XXX << p << ": ";
        std::ifstream infile( p.c_str() );
        for( iostr::EachLine in( infile ); in; in.next() )
        {
          XXX << *in << endl;
        }
     }

     return still_running;
}


}
}


