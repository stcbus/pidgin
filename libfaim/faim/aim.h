/* 
 * Main libfaim header.  Must be included in client for prototypes/macros.
 *
 * "come on, i turned a chick lesbian; i think this is the hackish equivalent"
 *                                                -- Josh Meyer
 *
 */

#ifndef __AIM_H__
#define __AIM_H__

#define FAIM_VERSION_MAJOR 0
#define FAIM_VERSION_MINOR 99
#define FAIM_VERSION_MINORMINOR 0

#include <faim/faimconfig.h>
#include <faim/aim_cbtypes.h>

#if !defined(FAIM_USEPTHREADS) && !defined(FAIM_USEFAKELOCKS)
#error pthreads or fakelocks are currently required.
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#include <io.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#endif

#ifdef FAIM_USEPTHREADS
#include <pthread.h>
#define faim_mutex_t pthread_mutex_t 
#define faim_mutex_init(x) pthread_mutex_init(x, NULL)
#define faim_mutex_lock(x) pthread_mutex_lock(x)
#define faim_mutex_unlock(x) pthread_mutex_unlock(x)
#define faim_mutex_destroy(x) pthread_mutex_destroy(x)
#elif defined(FAIM_USEFAKELOCKS)
/*
 * For platforms without pthreads, we also assume
 * we're not linking against a threaded app.  Which
 * means we don't have to do real locking.  The 
 * macros below do nothing really.  They're a joke.
 * But they get it to compile.
 */
#define faim_mutex_t char
#define faim_mutex_init(x) *x = 0
#define faim_mutex_lock(x) *x = 1;
#define faim_mutex_unlock(x) *x = 0;
#define faim_mutex_destroy(x) *x = 0;
#endif

/* Portability stuff (DMP) */

#ifdef _WIN32
#define sleep Sleep
#define socklen_t int /* this must be a POSIXy thing */
#define snprintf _snprintf /* I'm not sure whats wrong with Microsoft here */
#define close(x) closesocket(x) /* no comment */
#endif

#if defined(mach) && defined(__APPLE__)
#define gethostbyname(x) gethostbyname2(x, AF_INET) 
#endif

#if defined(_WIN32) || defined(STRICT_ANSI)
#define faim_shortfunc
#else
#define faim_shortfunc inline
#endif

#if defined(_WIN32) && !defined(WIN32_STATIC)
/*
 * For a win32 DLL, we define WIN32_INDLL if this file
 * is included while compiling the DLL. If its not 
 * defined (its included in a client app), the symbols
 * will be imported instead of exported.
 */
#ifdef WIN32_INDLL
#define faim_export __declspec(dllexport)
#else 
#define faim_export __declspec(dllimport)
#endif /* WIN32_INDLL */
#define faim_internal
#else
/*
 * Nothing normally needed for unix...
 */
#define faim_export
#define faim_internal
#endif

/* 
 * Current Maximum Length for Screen Names (not including NULL) 
 *
 * Currently only names up to 16 characters can be registered
 * however it is aparently legal for them to be larger.
 */
#define MAXSNLEN 32

/*
 * Current Maximum Length for Instant Messages
 *
 * This was found basically by experiment, but not wholly
 * accurate experiment.  It should not be regarded
 * as completely correct.  But its a decent approximation.
 *
 * Note that although we can send this much, its impossible
 * for WinAIM clients (up through the latest (4.0.1957)) to
 * send any more than 1kb.  Amaze all your windows friends
 * with utterly oversized instant messages!
 *
 * XXX: the real limit is the total SNAC size at 8192. Fix this.
 * 
 */
#define MAXMSGLEN 7987

/*
 * Current Maximum Length for Chat Room Messages
 *
 * This is actually defined by the protocol to be
 * dynamic, but I have yet to see due cause to 
 * define it dynamically here.  Maybe later.
 *
 */
#define MAXCHATMSGLEN 512

/*
 * Standard size of an AIM authorization cookie
 */
#define AIM_COOKIELEN            0x100

#define AIM_MD5_STRING "AOL Instant Messenger (SM)"

/*
 * Client info.  Filled in by the client and passed
 * in to aim_login().  The information ends up
 * getting passed to OSCAR through the initial
 * login command.
 *
 * XXX: Should this be per-session? -mid
 *
 */
struct client_info_s {
  char clientstring[100]; /* arbitrary size */
  int major;
  int minor;
  int build;
  char country[3];
  char lang[3];
  int major2;
  int minor2;
  long unknown;
};

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* 
 * These could be arbitrary, but its easier to use the actual AIM values 
 */
#define AIM_CONN_TYPE_AUTH          0x0007
#define AIM_CONN_TYPE_ADS           0x0005
#define AIM_CONN_TYPE_BOS           0x0002
#define AIM_CONN_TYPE_CHAT          0x000e
#define AIM_CONN_TYPE_CHATNAV       0x000d

/* they start getting arbitrary in rendezvous stuff =) */
#define AIM_CONN_TYPE_RENDEZVOUS    0x0101 /* these do not speak OSCAR! */
#define AIM_CONN_TYPE_RENDEZVOUS_OUT 0x0102 /* socket waiting for accept() */

/*
 * Subtypes, we need these for OFT stuff.
 */
#define AIM_CONN_SUBTYPE_OFT_DIRECTIM  0x0001
#define AIM_CONN_SUBTYPE_OFT_GETFILE   0x0002
#define AIM_CONN_SUBTYPE_OFT_SENDFILE  0x0003
#define AIM_CONN_SUBTYPE_OFT_BUDDYICON 0x0004
#define AIM_CONN_SUBTYPE_OFT_VOICE     0x0005

/*
 * Status values returned from aim_conn_new().  ORed together.
 */
#define AIM_CONN_STATUS_READY       0x0001
#define AIM_CONN_STATUS_INTERNALERR 0x0002
#define AIM_CONN_STATUS_RESOLVERR   0x0040
#define AIM_CONN_STATUS_CONNERR     0x0080
#define AIM_CONN_STATUS_INPROGRESS  0x0100

#define AIM_FRAMETYPE_OSCAR 0x0000
#define AIM_FRAMETYPE_OFT 0x0001

struct aim_conn_t {
  int fd;
  unsigned short type;
  unsigned short subtype;
  int seqnum;
  int status;
  void *priv; /* misc data the client may want to store */
  time_t lastactivity; /* time of last transmit */
  int forcedlatency; 
  struct aim_rxcblist_t *handlerlist;
  faim_mutex_t active; /* lock around read/writes */
  faim_mutex_t seqnum_lock; /* lock around ->seqnum changes */
  struct aim_conn_t *next;
};

/* struct for incoming commands */
struct command_rx_struct {
  unsigned char hdrtype; /* defines which piece of the union to use */
  union {
    struct { 
      char type;
      unsigned short seqnum;     
    } oscar;
    struct {
      unsigned short type;
      unsigned char magic[4]; /* ODC2 OFT2 */
      unsigned short hdr2len;
      unsigned char *hdr2; /* rest of bloated header */
    } oft;
  } hdr;
  unsigned short commandlen;         /* total payload length */
  unsigned char *data;             /* packet data (from 7 byte on) */
  unsigned char lock;               /* 0 = open, !0 = locked */
  unsigned char handled;            /* 0 = new, !0 = been handled */
  unsigned char nofree;		    /* 0 = free data on purge, 1 = only unlink */
  struct aim_conn_t *conn;  /* the connection it came in on... */
  struct command_rx_struct *next; /* ptr to next struct in list */
};

/* struct for outgoing commands */
struct command_tx_struct {
  unsigned char hdrtype; /* defines which piece of the union to use */
  union {
    struct {
      unsigned char type;
      unsigned short seqnum;
    } oscar;
    struct {
      unsigned short type;
      unsigned char magic[4]; /* ODC2 OFT2 */
      unsigned short hdr2len;
      unsigned char *hdr2;
    } oft;
  } hdr;
  u_int commandlen;         
  u_char *data;      
  u_int lock;               /* 0 = open, !0 = locked */
  u_int sent;               /* 0 = pending, !0 = has been sent */
  struct aim_conn_t *conn; 
  struct command_tx_struct *next; /* ptr to next struct in list */
};

/*
 * OFT session: random oft cruft, per-session.
 *
 */
struct aim_oft_session_t {
  FILE *listing;
  char *listingdir;
};

/*
 * AIM Session: The main client-data interface.  
 *
 */
struct aim_session_t {

  /* ---- Client Accessible ------------------------ */
  /* 
   * Our screen name.
   *
   */
  char sn[MAXSNLEN+1];
  
  /*
   * Pointer to anything the client wants to 
   * explicitly associate with this session.
   *
   * This is for use in the callbacks mainly. In any
   * callback, you can access this with sess->aux_data.
   *
   */
  void *aux_data;

  /* 
   * OFT Data 
   */
  struct aim_oft_session_t oft;

  /* ---- Internal Use Only ------------------------ */
  /* 
   * Connection information
   */
  struct aim_conn_t *connlist;
  faim_mutex_t connlistlock;
  
  /* 
   * TX/RX queues 
   */
  struct command_tx_struct *queue_outgoing;   
  struct command_rx_struct *queue_incoming; 
  
  /*
   * Tx Enqueuing function
   */
  int (*tx_enqueue)(struct aim_session_t *, struct command_tx_struct *);

  /*
   * This is a dreadful solution to the what-room-are-we-joining
   * problem.  (There's no connection between the service
   * request and the resulting redirect.)
   */ 
  char *pendingjoin;
  unsigned short pendingjoinexchange;

  /*
   * Outstanding snac handling 
   *
   * XXX: Should these be per-connection? -mid
   */
  struct aim_snac_t *snac_hash[FAIM_SNAC_HASH_SIZE];
  faim_mutex_t snac_hash_locks[FAIM_SNAC_HASH_SIZE];
  u_long snac_nextid;

  struct {
    char server[128];
    char username[128];
    char password[128];
  } socksproxy;

  unsigned long flags;

  struct aim_msgcookie_t *msgcookies;
};

/* Values for sess->flags */
#define AIM_SESS_FLAGS_SNACLOGIN       0x00000001
#define AIM_SESS_FLAGS_XORLOGIN        0x00000002
#define AIM_SESS_FLAGS_NONBLOCKCONNECT 0x00000004

/*
 * AIM User Info, Standard Form.
 */
struct aim_userinfo_s {
  char sn[MAXSNLEN+1];
  u_short warnlevel;
  u_short idletime;
  u_short flags;
  u_long membersince;
  u_long onlinesince;
  u_long sessionlen;  
  u_short capabilities;
  struct {
    unsigned short status;
    unsigned int ipaddr;
    char crap[0x25]; /* until we figure it out... */
  } icqinfo;
};

#define AIM_FLAG_UNCONFIRMED 	0x0001 /* "damned transients" */
#define AIM_FLAG_ADMINISTRATOR	0x0002
#define AIM_FLAG_AOL		0x0004
#define AIM_FLAG_OSCAR_PAY	0x0008
#define AIM_FLAG_FREE 		0x0010
#define AIM_FLAG_AWAY		0x0020
#define AIM_FLAG_UNKNOWN40	0x0040
#define AIM_FLAG_UNKNOWN80	0x0080

#define AIM_FLAG_ALLUSERS	0x001f

/*
 * TLV handling
 */

/* Generic TLV structure. */
struct aim_tlv_t {
  u_short type;
  u_short length;
  u_char *value;
};

/* List of above. */
struct aim_tlvlist_t {
  struct aim_tlv_t *tlv;
  struct aim_tlvlist_t *next;
};

/* TLV-handling functions */
faim_internal struct aim_tlvlist_t *aim_readtlvchain(u_char *buf, int maxlen);
faim_internal void aim_freetlvchain(struct aim_tlvlist_t **list);
faim_internal struct aim_tlv_t *aim_grabtlv(u_char *src);
faim_internal struct aim_tlv_t *aim_grabtlvstr(u_char *src);
faim_internal struct aim_tlv_t *aim_gettlv(struct aim_tlvlist_t *, u_short, int);
faim_internal char *aim_gettlv_str(struct aim_tlvlist_t *, u_short, int);
faim_internal unsigned char aim_gettlv8(struct aim_tlvlist_t *list, unsigned short type, int num);
faim_internal unsigned short aim_gettlv16(struct aim_tlvlist_t *list, unsigned short type, int num);
faim_internal unsigned long aim_gettlv32(struct aim_tlvlist_t *list, unsigned short type, int num);
faim_internal int aim_puttlv (u_char *dest, struct aim_tlv_t *newtlv);
faim_internal struct aim_tlv_t *aim_createtlv(void);
faim_internal int aim_freetlv(struct aim_tlv_t **oldtlv);
faim_export int aim_puttlv_8(unsigned char *buf, unsigned short t, unsigned char  v);
faim_internal int aim_puttlv_16(u_char *, u_short, u_short);
faim_internal int aim_puttlv_32(u_char *, u_short, u_long);
faim_internal int aim_puttlv_str(u_char *buf, u_short t, int l, char *v);
faim_internal int aim_writetlvchain(u_char *buf, int buflen, struct aim_tlvlist_t **list);
faim_internal int aim_addtlvtochain16(struct aim_tlvlist_t **list, unsigned short type, unsigned short val);
faim_internal int aim_addtlvtochain32(struct aim_tlvlist_t **list, unsigned short type, unsigned long val);
faim_internal int aim_addtlvtochain_str(struct aim_tlvlist_t **list, unsigned short type, char *str, int len);
faim_internal int aim_addtlvtochain_caps(struct aim_tlvlist_t **list, unsigned short type, unsigned short caps);
faim_internal int aim_addtlvtochain_noval(struct aim_tlvlist_t **list, unsigned short type);
faim_internal int aim_counttlvchain(struct aim_tlvlist_t **list);

/*
 * Get command from connections / Dispatch commands
 * already in queue.
 */
faim_export int aim_get_command(struct aim_session_t *, struct aim_conn_t *);
int aim_rxdispatch(struct aim_session_t *);

faim_export unsigned long aim_debugconn_sendconnect(struct aim_session_t *sess, struct aim_conn_t *conn);

int aim_logoff(struct aim_session_t *);

faim_export void aim_conn_kill(struct aim_session_t *sess, struct aim_conn_t **deadconn);

typedef int (*rxcallback_t)(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_register_callbacks(rxcallback_t *);

faim_internal unsigned long aim_genericreq_n(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype);
faim_internal unsigned long aim_genericreq_l(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype, u_long *);
faim_internal unsigned long aim_genericreq_s(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short subtype, u_short *);

faim_internal struct aim_fileheader_t *aim_oft_getfh(unsigned char *hdr);
faim_export int aim_oft_registerlisting(struct aim_session_t *sess, FILE *file, char* listingdir);
faim_export int aim_getfile_send(struct aim_conn_t *conn, FILE *tosend, struct aim_fileheader_t *fh);

/* aim_login.c */
faim_export int aim_sendconnack(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export int aim_request_login (struct aim_session_t *sess, struct aim_conn_t *conn, char *sn);
faim_export int aim_send_login (struct aim_session_t *, struct aim_conn_t *, char *, char *, struct client_info_s *, char *key);
faim_export unsigned long aim_sendauthresp(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn, int errorcode, char *errorurl, char *bosip, char *cookie, char *email, int regstatus);
faim_export int aim_gencookie(unsigned char *buf);
faim_export int aim_sendserverready(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_internal int aim_authkeyparse(struct aim_session_t *sess, struct command_rx_struct *command);
faim_export unsigned long aim_sendredirect(struct aim_session_t *sess, struct aim_conn_t *conn, unsigned short servid, char *ip, char *cookie);
faim_export void aim_purge_rxqueue(struct aim_session_t *);
faim_internal void aim_rxqueue_cleanbyconn(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_internal int aim_recv(int fd, void *buf, size_t count);

int aim_parse_unknown(struct aim_session_t *, struct command_rx_struct *command, ...);
int aim_parse_missed_im(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_last_bad(struct aim_session_t *, struct command_rx_struct *, ...);
faim_internal int aim_get_command_rendezvous(struct aim_session_t *sess, struct aim_conn_t *conn);

faim_internal struct command_tx_struct *aim_tx_new(unsigned char framing, int chan, struct aim_conn_t *conn, int datalen);
faim_internal int aim_tx_enqueue__queuebased(struct aim_session_t *, struct command_tx_struct *);
faim_internal int aim_tx_enqueue__immediate(struct aim_session_t *, struct command_tx_struct *);
faim_internal int aim_tx_enqueue(struct aim_session_t *, struct command_tx_struct *);
faim_internal int aim_tx_sendframe(struct aim_session_t *sess, struct command_tx_struct *cur);
faim_internal unsigned int aim_get_next_txseqnum(struct aim_conn_t *);
faim_export int aim_tx_flushqueue(struct aim_session_t *);
faim_internal int aim_tx_printqueue(struct aim_session_t *);
faim_export void aim_tx_purgequeue(struct aim_session_t *);

struct aim_rxcblist_t {
  u_short family;
  u_short type;
  rxcallback_t handler;
  u_short flags;
  struct aim_rxcblist_t *next;
};

faim_export int aim_conn_setlatency(struct aim_conn_t *conn, int newval);

faim_export int aim_conn_addhandler(struct aim_session_t *, struct aim_conn_t *conn, u_short family, u_short type, rxcallback_t newhandler, u_short flags);
faim_internal rxcallback_t aim_callhandler(struct aim_conn_t *conn, u_short family, u_short type);
faim_export int aim_clearhandlers(struct aim_conn_t *conn);

/*
 * Generic SNAC structure.  Rarely if ever used.
 */
struct aim_snac_t {
  u_long id;
  u_short family;
  u_short type;
  u_short flags;
  void *data;
  time_t issuetime;
  struct aim_snac_t *next;
};
faim_internal void aim_initsnachash(struct aim_session_t *sess);
faim_internal unsigned long aim_newsnac(struct aim_session_t *, struct aim_snac_t *newsnac);
faim_internal struct aim_snac_t *aim_remsnac(struct aim_session_t *, u_long id);
faim_internal int aim_cleansnacs(struct aim_session_t *, int maxage);
faim_internal int aim_putsnac(u_char *, int, int, int, u_long);


faim_internal void aim_connrst(struct aim_session_t *);
faim_internal struct aim_conn_t *aim_conn_getnext(struct aim_session_t *);
faim_export void aim_conn_close(struct aim_conn_t *deadconn);
faim_internal struct aim_conn_t *aim_getconn_type(struct aim_session_t *, int type);
faim_export struct aim_conn_t *aim_newconn(struct aim_session_t *, int type, char *dest);
faim_export int aim_conngetmaxfd(struct aim_session_t *);
faim_export struct aim_conn_t *aim_select(struct aim_session_t *, struct timeval *, int *);
faim_export int aim_conn_isready(struct aim_conn_t *);
faim_export int aim_conn_setstatus(struct aim_conn_t *, int);
faim_export int aim_conn_completeconnect(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export int aim_conn_isconnecting(struct aim_conn_t *conn);
faim_export void aim_session_init(struct aim_session_t *, unsigned long flags);
faim_export void aim_setupproxy(struct aim_session_t *sess, char *server, char *username, char *password);

/* aim_misc.c */

#define AIM_VISIBILITYCHANGE_PERMITADD    0x05
#define AIM_VISIBILITYCHANGE_PERMITREMOVE 0x06
#define AIM_VISIBILITYCHANGE_DENYADD      0x07
#define AIM_VISIBILITYCHANGE_DENYREMOVE   0x08

#define AIM_PRIVFLAGS_ALLOWIDLE           0x01
#define AIM_PRIVFLAGS_ALLOWMEMBERSINCE    0x02

#define AIM_WARN_ANON                     0x01

faim_export int aim_send_warning(struct aim_session_t *sess, struct aim_conn_t *conn, char *destsn, int anon);
faim_export unsigned long aim_bos_nop(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_flap_nop(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export unsigned long aim_bos_setidle(struct aim_session_t *, struct aim_conn_t *, u_long);
faim_export unsigned long aim_bos_changevisibility(struct aim_session_t *, struct aim_conn_t *, int, char *);
faim_export unsigned long aim_bos_setbuddylist(struct aim_session_t *, struct aim_conn_t *, char *);
faim_export unsigned long aim_bos_setprofile(struct aim_session_t *, struct aim_conn_t *, char *, char *, unsigned short);
faim_export unsigned long aim_bos_setgroupperm(struct aim_session_t *, struct aim_conn_t *, u_long);
faim_export unsigned long aim_bos_clientready(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_reqrate(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_ackrateresp(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_setprivacyflags(struct aim_session_t *, struct aim_conn_t *, u_long);
faim_export unsigned long aim_bos_reqpersonalinfo(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_reqservice(struct aim_session_t *, struct aim_conn_t *, u_short);
faim_export unsigned long aim_bos_reqrights(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_reqbuddyrights(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_reqlocaterights(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_bos_reqicbmparaminfo(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_addicbmparam(struct aim_session_t *sess,struct aim_conn_t *conn);
faim_export unsigned long aim_setversions(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export unsigned long aim_setdirectoryinfo(struct aim_session_t *sess, struct aim_conn_t *conn, char *first, char *middle, char *last, char *maiden, char *nickname, char *street, char *city, char *state, char *zip, int country, unsigned short privacy);
faim_export unsigned long aim_setuserinterests(struct aim_session_t *sess, struct aim_conn_t *conn, char *interest1, char *interest2, char *interest3, char *interest4, char *interest5, unsigned short privacy);
faim_export unsigned long aim_icq_setstatus(struct aim_session_t *sess, struct aim_conn_t *conn, unsigned long status);

faim_internal struct aim_fileheader_t *aim_getlisting(FILE *);
faim_internal int aim_oft_buildheader(char *,struct aim_fileheader_t *);
faim_internal int aim_listenestablish(u_short);
faim_internal int aim_tx_destroy(struct command_tx_struct *);

/* aim_rxhandlers.c */
faim_export int aim_rxdispatch(struct aim_session_t *);
faim_internal int aim_authparse(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_handleredirect_middle(struct aim_session_t *, struct command_rx_struct *, ...);
faim_internal int aim_parse_unknown(struct aim_session_t *, struct command_rx_struct *, ...);
int aim_parse_last_bad(struct aim_session_t *, struct command_rx_struct *, ...);
faim_internal int aim_parse_generalerrs(struct aim_session_t *, struct command_rx_struct *command, ...);
faim_internal int aim_parsemotd_middle(struct aim_session_t *sess, struct command_rx_struct *command, ...);
faim_internal int aim_parse_ratechange_middle(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_parse_evilnotify_middle(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_parse_msgack_middle(struct aim_session_t *sess, struct command_rx_struct *command);

faim_export unsigned long aim_ads_clientready(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export unsigned long aim_ads_requestads(struct aim_session_t *sess, struct aim_conn_t *conn);

/* aim_im.c */
struct aim_directim_priv {
  unsigned char cookie[8];
  char sn[MAXSNLEN+1];
  char ip[30];
};

#define AIM_IMFLAGS_AWAY 0x01 /* mark as an autoreply */
#define AIM_IMFLAGS_ACK  0x02 /* request a receipt notice */

faim_export unsigned long aim_send_im(struct aim_session_t *, struct aim_conn_t *, char *, u_int, char *);
faim_internal int aim_parse_incoming_im_middle(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_parse_outgoing_im_middle(struct aim_session_t *, struct command_rx_struct *);
faim_export unsigned long aim_seticbmparam(struct aim_session_t *, struct aim_conn_t *conn);
faim_internal int aim_parse_msgerror_middle(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_negchan_middle(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_parse_bosrights(struct aim_session_t *sess, struct command_rx_struct *command, ...);
faim_internal int aim_parse_missedcall(struct aim_session_t *sess, struct command_rx_struct *command);

faim_export struct aim_conn_t * aim_directim_initiate(struct aim_session_t *, struct aim_conn_t *, struct aim_directim_priv *, char *);
faim_export int aim_send_im_direct(struct aim_session_t *, struct aim_conn_t *, char *);
faim_export struct aim_conn_t *aim_directim_connect(struct aim_session_t *, struct aim_conn_t *, struct aim_directim_priv *);

/* aim_info.c */
#define AIM_CAPS_BUDDYICON 0x01
#define AIM_CAPS_VOICE 0x02
#define AIM_CAPS_IMIMAGE 0x04
#define AIM_CAPS_CHAT 0x08
#define AIM_CAPS_GETFILE 0x10
#define AIM_CAPS_SENDFILE 0x20

extern u_char aim_caps[6][16];
faim_internal unsigned short aim_getcap(unsigned char *capblock, int buflen);
faim_internal int aim_putcap(unsigned char *capblock, int buflen, u_short caps);

#define AIM_GETINFO_GENERALINFO 0x00001
#define AIM_GETINFO_AWAYMESSAGE 0x00003

struct aim_msgcookie_t {
  unsigned char cookie[8];
  int type;
  void *data;
  time_t addtime;
  struct aim_msgcookie_t *next;
};

struct aim_fileheader_t {
#if 0
  char  magic[4];            /* 0 */
  short hdrlen;           /* 4 */
  short hdrtype;             /* 6 */
#endif
  char  bcookie[8];       /* 8 */
  short encrypt;          /* 16 */
  short compress;         /* 18 */
  short totfiles;         /* 20 */
  short filesleft;        /* 22 */
  short totparts;         /* 24 */
  short partsleft;        /* 26 */
  long  totsize;          /* 28 */
  long  size;             /* 32 */
  long  modtime;          /* 36 */
  long  checksum;         /* 40 */
  long  rfrcsum;          /* 44 */
  long  rfsize;           /* 48 */
  long  cretime;          /* 52 */
  long  rfcsum;           /* 56 */
  long  nrecvd;           /* 60 */
  long  recvcsum;         /* 64 */
  char  idstring[32];     /* 68 */
  char  flags;            /* 100 */
  char  lnameoffset;      /* 101 */
  char  lsizeoffset;      /* 102 */
  char  dummy[69];        /* 103 */
  char  macfileinfo[16];  /* 172 */
  short nencode;          /* 188 */
  short nlanguage;        /* 190 */
  char  name[64];         /* 192 */
                          /* 256 */
};

struct aim_filetransfer_priv {
  char sn[MAXSNLEN];
  char cookie[8];
  char ip[30];
  int state;
  struct aim_fileheader_t fh;
};

#define AIM_COOKIETYPE_UNKNOWN  0x00
#define AIM_COOKIETYPE_ICBM     0x01
#define AIM_COOKIETYPE_ADS      0x02
#define AIM_COOKIETYPE_BOS      0x03
#define AIM_COOKIETYPE_IM       0x04
#define AIM_COOKIETYPE_CHAT     0x05
#define AIM_COOKIETYPE_CHATNAV  0x06
/* we'll move OFT up a bit to give breathing room.  not like it really
 * matters. */
#define AIM_COOKIETYPE_OFTIM    0x10
#define AIM_COOKIETYPE_OFTGET   0x11
#define AIM_COOKIETYPE_OFTSEND  0x12
#define AIM_COOKIETYPE_OFTVOICE 0x13
#define AIM_COOKIETYPE_OFTIMAGE 0x14
#define AIM_COOKIETYPE_OFTICON  0x15

faim_internal int aim_cachecookie(struct aim_session_t *sess, struct aim_msgcookie_t *cookie);
faim_export int aim_purgecookies(struct aim_session_t *sess, int maxage);
faim_internal struct aim_msgcookie_t *aim_uncachecookie(struct aim_session_t *sess, unsigned char *cookie, int type);
faim_internal struct aim_msgcookie_t *aim_mkcookie(unsigned char *, int, void *);
faim_internal struct aim_msgcookie_t *aim_checkcookie(struct aim_session_t *, unsigned char *, int);
faim_internal int aim_msgcookie_gettype(int reqclass);

faim_export int aim_handlerendconnect(struct aim_session_t *sess, struct aim_conn_t *cur);

#define AIM_TRANSFER_DENY_NOTSUPPORTED 0x0000
#define AIM_TRANSFER_DENY_DECLINE 0x0001
#define AIM_TRANSFER_DENY_NOTACCEPTING 0x0002
faim_export unsigned long aim_denytransfer(struct aim_session_t *sess, struct aim_conn_t *conn, char *sender, char *cookie, unsigned short code);
faim_export struct aim_conn_t *aim_accepttransfer(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn,char *cookie,char *ip, FILE *file, unsigned short rendid);


faim_export unsigned long aim_getinfo(struct aim_session_t *, struct aim_conn_t *, const char *, unsigned short);
faim_internal int aim_extractuserinfo(u_char *, struct aim_userinfo_s *);
faim_internal int aim_parse_userinfo_middle(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_parse_oncoming_middle(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_parse_offgoing_middle(struct aim_session_t *, struct command_rx_struct *);
faim_internal int aim_putuserinfo(u_char *buf, int buflen, struct aim_userinfo_s *info);
faim_export int aim_sendbuddyoncoming(struct aim_session_t *sess, struct aim_conn_t *conn, struct aim_userinfo_s *info);
faim_export int aim_sendbuddyoffgoing(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn);
faim_internal int aim_parse_locateerr(struct aim_session_t *sess, struct command_rx_struct *command);

/* aim_auth.c */
faim_export int aim_auth_sendcookie(struct aim_session_t *, struct aim_conn_t *, u_char *);
faim_export u_long aim_auth_clientready(struct aim_session_t *, struct aim_conn_t *);
faim_export unsigned long aim_auth_changepasswd(struct aim_session_t *, struct aim_conn_t *, char *, char *);

/* aim_buddylist.c */
faim_export unsigned long aim_add_buddy(struct aim_session_t *, struct aim_conn_t *, char *);
faim_export unsigned long aim_remove_buddy(struct aim_session_t *, struct aim_conn_t *, char *);
faim_internal int aim_parse_buddyrights(struct aim_session_t *sess, struct command_rx_struct *command, ...);

/* aim_search.c */
faim_export u_long aim_usersearch_address(struct aim_session_t *, struct aim_conn_t *, char *);


struct aim_chat_roominfo {
  u_short exchange;
  char *name;
  u_short instance;
};
faim_internal int aim_chat_readroominfo(u_char *buf, struct aim_chat_roominfo *outinfo);
faim_internal int aim_chat_parse_infoupdate(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_chat_parse_joined(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_chat_parse_leave(struct aim_session_t *sess, struct command_rx_struct *command);
faim_internal int aim_chat_parse_incoming(struct aim_session_t *sess, struct command_rx_struct *command);
faim_export unsigned long aim_chat_send_im(struct aim_session_t *sess, struct aim_conn_t *conn, char *msg);
faim_export unsigned long aim_chat_join(struct aim_session_t *sess, struct aim_conn_t *conn, u_short exchange, const char *roomname);
faim_export unsigned long aim_chat_clientready(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export int aim_chat_attachname(struct aim_conn_t *conn, char *roomname);
faim_export char *aim_chat_getname(struct aim_conn_t *conn);
faim_export struct aim_conn_t *aim_chat_getconn(struct aim_session_t *, char *name);

faim_export unsigned long aim_chatnav_reqrights(struct aim_session_t *sess, struct aim_conn_t *conn);
faim_export unsigned long aim_chatnav_clientready(struct aim_session_t *sess, struct aim_conn_t *conn);

faim_export unsigned long aim_chat_invite(struct aim_session_t *sess, struct aim_conn_t *conn, char *sn, char *msg, u_short exchange, char *roomname, u_short instance);

struct aim_chat_exchangeinfo {
  u_short number;
  char *name;
  char *charset1;
  char *lang1;
  char *charset2;
  char *lang2;
};
faim_internal int aim_chatnav_parse_info(struct aim_session_t *sess, struct command_rx_struct *command);
faim_export u_long aim_chatnav_createroom(struct aim_session_t *sess, struct aim_conn_t *conn, char *name, u_short exchange);
faim_export int aim_chat_leaveroom(struct aim_session_t *sess, char *name);

/* aim_util.c */
#ifdef AIMUTIL_USEMACROS
/*
 * These are really ugly.  You'd think this was LISP.  I wish it was.
 */
#define aimutil_put8(buf, data) ((*(buf) = (u_char)(data)&0xff),1)
#define aimutil_get8(buf) ((*(buf))&0xff)
#define aimutil_put16(buf, data) ( \
                                  (*(buf) = (u_char)((data)>>8)&0xff), \
				  (*((buf)+1) = (u_char)(data)&0xff),  \
				  2)
#define aimutil_get16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define aimutil_put32(buf, data) ( \
                                  (*((buf)) = (u_char)((data)>>24)&0xff), \
				  (*((buf)+1) = (u_char)((data)>>16)&0xff), \
				  (*((buf)+2) = (u_char)((data)>>8)&0xff), \
				  (*((buf)+3) = (u_char)(data)&0xff), \
                                  4)
#define aimutil_get32(buf) ((((*(buf))<<24)&0xff000000) + \
                            (((*((buf)+1))<<16)&0x00ff0000) + \
                            (((*((buf)+2))<< 8)&0x0000ff00) + \
                            (((*((buf)+3)    )&0x000000ff)))
#else
#warning Not using aimutil macros.  May have performance problems.
int aimutil_put8(u_char *, u_char);
u_char aimutil_get8(u_char *buf);
int aimutil_put16(u_char *, u_short);
u_short aimutil_get16(u_char *);
int aimutil_put32(u_char *, u_long);
u_long aimutil_get32(u_char *);
#endif

faim_export int aimutil_putstr(u_char *, const char *, int);
faim_export int aimutil_tokslen(char *toSearch, int index, char dl);
faim_export int aimutil_itemcnt(char *toSearch, char dl);
faim_export char *aimutil_itemidx(char *toSearch, int index, char dl);

faim_export int aim_snlen(const char *sn);
faim_export int aim_sncmp(const char *sn1, const char *sn2);

/* for libc's that dont have it */
faim_export char *aim_strsep(char **pp, const char *delim);

/* aim_meta.c */
faim_export char *aim_getbuilddate(void);
faim_export char *aim_getbuildtime(void);
faim_export char *aim_getbuildstring(void);
faim_internal void faimdprintf(int dlevel, const char *format, ...);

#endif /* __AIM_H__ */

