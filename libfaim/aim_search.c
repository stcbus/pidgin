
/*
 * aim_search.c
 *
 * TODO: Add aim_usersearch_name()
 *
 */

#include <faim/aim.h>

faim_export unsigned long aim_usersearch_address(struct aim_session_t *sess,
						 struct aim_conn_t *conn, 
						 char *address)
{
  struct command_tx_struct *newpacket;
  
  if (!address)
    return -1;

  if (!(newpacket = aim_tx_new(AIM_FRAMETYPE_OSCAR, 0x0002, conn, 10+strlen(address))))
    return -1;

  newpacket->lock = 1;

  aim_putsnac(newpacket->data, 0x000a, 0x0002, 0x0000, sess->snac_nextid);

  aimutil_putstr(newpacket->data+10, address, strlen(address));

  aim_tx_enqueue(sess, newpacket);

  aim_cachesnac(sess, 0x000a, 0x0002, 0x0000, address, strlen(address)+1);

  return sess->snac_nextid;
}

