#define HAVE_SYS_VFS_H 
#define HAVE_SYS_MOUNT_H 
#define HAVE_SYS_IOCTL_H 
#define HAVE_SYS_STATVFS_H 
#define HAVE_DIRENT_H 
#define HAVE_GRP_H 
#define HAVE_PWD_H 
#define HAVE_TERMIO_H 
#define HAVE_TERMIOS_H 
#define HAVE_SYS_STAT_H 
#define HAVE_SYS_TYPES_H 
// These settings can be changed by user.

// Recommended, adds speed
#define CACHE_GETSET 1

// This too
#define CACHE_NAMECOLOR 1

// This too
#define CACHE_UIDGID 1

// Recommended. Disable if it bugs you...
#define NEW_WILDMATCH 1

// They can be disabled if you do not want them.
#define SUPPORT_BRACKETS 1

// Enable this if you want a progress meter. Slows down.
#define STARTUP_COUNTER 0

// Set this to 1 if your passwd file is quick to load.
#define PRELOAD_UIDGID 0

// File containing the settings. Can be changed.
#define SETTINGSFILE "dirrsets.hh"

// statfs() support. It was automatically configured,
// and _could_ be incorrect. Change it if you suspect
// an error.
#define HAVE_STATFS 1
