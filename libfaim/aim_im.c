/*
 *  aim_im.c
 *
 *  The routines for sending/receiving Instant Messages.
 *
 */

#include <faim/aim.h>

/*
 * Send an ICBM (instant message).  
 *
 *
 * Possible flags:
 *   AIM_IMFLAGS_AWAY  -- Marks the message as an autoresponse
 *   AIM_IMFLAGS_ACK   -- Requests that the server send an ack
 *                        when the message is received (of type 0x0004/0x000c)
 *
 */
u_long aim_send_im(struct aim_session_t *sess,
		   struct aim_conn_t *conn, 
		   char *destsn, u_int flags, char *msg)
{   

  int curbyte,i;
  struct command_tx_struct *newpacket;
  
  if (strlen(msg) >= MAXMSGLEN)
    return -1;

  if (!(newpacket = aim_tx_new(0x0002, conn, strlen(msg)+256)))
    return -1;

  newpacket->lock = 1; /* lock struct */

  curbyte  = 0;
  curbyte += aim_putsnac(newpacket->data+curbyte, 
			 0x0004, 0x0006, 0x0000, sess->snac_nextid);

  /* 
   * Generate a random message cookie 
   *
   * We could cache these like we do SNAC IDs.  (In fact, it 
   * might be a good idea.)  In the message error functions, 
   * the 8byte message cookie is returned as well as the 
   * SNAC ID.
   *
   */
  for (i=0;i<8;i++)
    curbyte += aimutil_put8(newpacket->data+curbyte, (u_char) random());

  /*
   * Channel ID
   */
  curbyte += aimutil_put16(newpacket->data+curbyte,0x0001);

  /* 
   * Destination SN (prepended with byte length)
   */
  curbyte += aimutil_put8(newpacket->data+curbyte,strlen(destsn));
  curbyte += aimutil_putstr(newpacket->data+curbyte, destsn, strlen(destsn));

  /*
   * metaTLV start.
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0002);
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(msg) + 0x0d);

  /*
   * Flag data?
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0501);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0001);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0101);
  curbyte += aimutil_put8 (newpacket->data+curbyte, 0x01);

  /* 
   * Message block length.
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, strlen(msg) + 0x04);

  /*
   * Character set data? 
   */
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  /*
   * Message.  Not terminated.
   */
  curbyte += aimutil_putstr(newpacket->data+curbyte,msg, strlen(msg));

  /*
   * Set the Request Acknowledge flag.  
   */
  if (flags & AIM_IMFLAGS_ACK) {
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0003);
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0000);
  }
  
  /*
   * Set the Autoresponse flag.
   */
  if (flags & AIM_IMFLAGS_AWAY) {
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0004);
    curbyte += aimutil_put16(newpacket->data+curbyte,0x0000);
  }
  
  newpacket->commandlen = curbyte;
  newpacket->lock = 0;

  aim_tx_enqueue(sess, newpacket);

#ifdef USE_SNAC_FOR_IMS
 {
    struct aim_snac_t snac;

    snac.id = sess->snac_nextid;
    snac.family = 0x0004;
    snac.type = 0x0006;
    snac.flags = 0x0000;

    snac.data = malloc(strlen(destsn)+1);
    memcpy(snac.data, destsn, strlen(destsn)+1);

    aim_newsnac(sess, &snac);
  }

 aim_cleansnacs(sess, 60); /* clean out all SNACs over 60sec old */
#endif

  return (sess->snac_nextid++);
}

/*
 * It can easily be said that parsing ICBMs is THE single
 * most difficult thing to do in the in AIM protocol.  In
 * fact, I think I just did say that.
 *
 * Below is the best damned solution I've come up with
 * over the past sixteen months of battling with it. This
 * can parse both away and normal messages from every client
 * I have access to.  Its not fast, its not clean.  But it works.
 *
 * We should also support at least minimal parsing of 
 * Channel 2, so that we can at least know the name of the
 * room we're invited to, but obviously can't attend...
 *
 */
int aim_parse_incoming_im_middle(struct aim_session_t *sess,
				 struct command_rx_struct *command)
{
  u_int i = 0,z;
  rxcallback_t userfunc = NULL;
  u_char cookie[8];
  int channel;
  struct aim_tlvlist_t *tlvlist;
  struct aim_userinfo_s userinfo;
  u_short wastebits;

  memset(&userinfo, 0x00, sizeof(struct aim_userinfo_s));
 
  i = 10; /* Skip SNAC header */

  /*
   * Read ICBM Cookie.  And throw away.
   */
  for (z=0; z<8; z++,i++)
    cookie[z] = command->data[i];
  
  /*
   * Channel ID.
   *
   * Channel 0x0001 is the message channel.  There are 
   * other channels for things called "rendevous"
   * which represent chat and some of the other new
   * features of AIM2/3/3.5. 
   *
   * Channel 0x0002 is the Rendevous channel, which
   * is where Chat Invitiations and various client-client
   * connection negotiations come from.
   * 
   */
  channel = aimutil_get16(command->data+i);
  i += 2;
  
  /*
   *
   */
  if ((channel != 0x01) && (channel != 0x02))
    {
      printf("faim: icbm: ICBM received on an unsupported channel.  Ignoring.\n (chan = %04x)", channel);
      return 1;
    }

  /*
   * Source screen name.
   */
  memcpy(userinfo.sn, command->data+i+1, (int)command->data[i]);
  userinfo.sn[(int)command->data[i]] = '\0';
  i += 1 + (int)command->data[i];

  /*
   * Warning Level
   */
  userinfo.warnlevel = aimutil_get16(command->data+i); /* guess */
  i += 2;

  /*
   * Number of TLVs that follow.  Not needed.
   */
  wastebits = aimutil_get16(command->data+i);
  i += 2;
  
  /*
   * Read block of TLVs.  All further data is derived
   * from what is parsed here.
   */
  tlvlist = aim_readtlvchain(command->data+i, command->commandlen-i);

  /*
   * From here on, its depends on what channel we're on.
   */
  if (channel == 1)
    {
     u_int j = 0, y = 0, z = 0;
      char *msg = NULL;
      u_int icbmflags = 0;
      struct aim_tlv_t *msgblocktlv, *tmptlv;
      u_char *msgblock;
      u_short flag1,flag2;
           
      /*
       * Check Autoresponse status.  If it is an autoresponse,
       * it will contain a second type 0x0004 TLV, with zero length.
       */
      if (aim_gettlv(tlvlist, 0x0004, 2))
	icbmflags |= AIM_IMFLAGS_AWAY;
      
      /*
       * Check Ack Request status.
       */
      if (aim_gettlv(tlvlist, 0x0003, 2))
	icbmflags |= AIM_IMFLAGS_ACK;
      
      /*
       * Extract the various pieces of the userinfo struct.
       */
      /* Class. */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0001, 1)))
	userinfo.class = aimutil_get16(tmptlv->value);
      /* Member-since date. */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0002, 1)))
	{
	  /* If this is larger than 4, its probably the message block, skip */
	  if (tmptlv->length <= 4)
	    userinfo.membersince = aimutil_get32(tmptlv->value);
	}
      /* On-since date */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0003, 1)))
	userinfo.onlinesince = aimutil_get32(tmptlv->value);
      /* Idle-time */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0004, 1)))
	userinfo.idletime = aimutil_get16(tmptlv->value);
      /* Session Length (AIM) */
      if ((tmptlv = aim_gettlv(tlvlist, 0x000f, 1)))
	userinfo.sessionlen = aimutil_get16(tmptlv->value);
      /* Session Length (AOL) */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0010, 1)))
	userinfo.sessionlen = aimutil_get16(tmptlv->value);
      
      /*
       * Message block.
       *
       * XXX: Will the msgblock always be the second 0x0002? 
       */
      msgblocktlv = aim_gettlv(tlvlist, 0x0002, 1);
      if (!msgblocktlv)
	{
	  printf("faim: icbm: major error! no message block TLV found!\n");
	  aim_freetlvchain(&tlvlist);
	  return 1;
	}
      
      /*
       * Extracting the message from the unknown cruft.
       * 
       * This is a bit messy, and I'm not really qualified,
       * even as the author, to comment on it.  At least
       * its not as bad as a while loop shooting into infinity.
       *
       * "Do you believe in magic?"
       *
       */
      msgblock = msgblocktlv->value;
      j = 0;
      
      wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      
      y = aimutil_get16(msgblock+j);
      j += 2;
      for (z = 0; z < y; z++)
	wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      wastebits = aimutil_get8(msgblock+j++);
      
      /* 
       * Message string length, including flag words.
       */
      i = aimutil_get16(msgblock+j);
      j += 2;
      
      /*
       * Flag words.
       *
       * Its rumored that these can kick in some funky
       * 16bit-wide char stuff that used to really kill
       * libfaim.  Hopefully the latter is no longer true.
       *
       * Though someone should investiagte the former.
       *
       */
      flag1 = aimutil_get16(msgblock+j);
      j += 2;
      flag2 = aimutil_get16(msgblock+j);
      j += 2;
      
      if (flag1 || flag2)
	printf("faim: icbm: **warning: encoding flags are being used! {%04x, %04x}\n", flag1, flag2);
      
      /* 
       * Message string. 
       */
      i -= 4;
      msg = (char *)malloc(i+1);
      memcpy(msg, msgblock+j, i);
      msg[i] = '\0';
      
      /*
       * Call client.
       */
      userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
      if (userfunc)
	i = userfunc(sess, command, channel, &userinfo, msg, icbmflags, flag1, flag2);
      else 
	i = 0;
      
      free(msg);
    }
  else if (channel == 0x0002)
    {	
      int rendtype;
      struct aim_tlv_t *block1;
      struct aim_tlvlist_t *list2;
      struct aim_tlv_t *tmptlv;
      int a;
      
      /* Class. */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0001, 1)))
	userinfo.class = aimutil_get16(tmptlv->value);
      /* On-since date */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0003, 1)))
	userinfo.onlinesince = aimutil_get32(tmptlv->value);
      /* Idle-time */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0004, 1)))
	userinfo.idletime = aimutil_get16(tmptlv->value);
      /* Session Length (AIM) */
      if ((tmptlv = aim_gettlv(tlvlist, 0x000f, 1)))
	userinfo.sessionlen = aimutil_get16(tmptlv->value);
      /* Session Length (AOL) */
      if ((tmptlv = aim_gettlv(tlvlist, 0x0010, 1)))
	userinfo.sessionlen = aimutil_get16(tmptlv->value);

      /*
       * There's another block of TLVs embedded in the type 5 here. 
       */
      block1 = aim_gettlv(tlvlist, 0x0005, 1);
      if (!block1)
	return 1; /* major problem */

      a = 0x1a; /* skip -- not sure what this information is! */

      /*
       * XXX: Ignore if there's no data, only cookie information.
       *
       * Its probably just an accepted invitation or something.
       *  
       */
      if (block1->length <= 0x1a)
	{
	  aim_freetlvchain(&tlvlist);
	  return 1;
	}

      list2 = aim_readtlvchain(block1->value+a, block1->length-a);

      if (aim_gettlv(list2, 0x0004, 1) /* start connection */ ||  
	  aim_gettlv(list2, 0x000b, 1) /* close conncetion */)
	{
	  rendtype = 1; /* voice request */

	  /*
	   * Call client.
	   */
	  userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	  if (userfunc)
	    i = userfunc(sess, 
			 command, 
			 channel, 
			 rendtype,
			 &userinfo);
	  else 
	    i = 0;
	}
      else
	{
	  struct aim_chat_roominfo roominfo;
	  char *msg=NULL,*encoding=NULL,*lang=NULL;

	  rendtype = 0; /* chat invite */
	  if (aim_gettlv(list2, 0x2711, 1))
	    {
	      struct aim_tlv_t *nametlv;
	      
	      nametlv = aim_gettlv(list2, 0x2711, 1);
	      aim_chat_readroominfo(nametlv->value, &roominfo);
	    }
	  
	  if (aim_gettlv(list2, 0x000c, 1))
	    msg = aim_gettlv_str(list2, 0x000c, 1);
	  
	  if (aim_gettlv(list2, 0x000d, 1))
	    encoding = aim_gettlv_str(list2, 0x000d, 1);
	  
	  if (aim_gettlv(list2, 0x000e, 1))
	    lang = aim_gettlv_str(list2, 0x000e, 1);
      
	  /*
	   * Call client.
	   */
	  userfunc = aim_callhandler(command->conn, 0x0004, 0x0007);
	  if (userfunc)
	    i = userfunc(sess, 
			 command, 
			 channel, 
			 rendtype,
			 &userinfo, 
			 &roominfo, 
			 msg, 
			 encoding?encoding+1:NULL, 
			 lang?lang+1:NULL);
	  else 
	    i = 0;
      
	  free(roominfo.name);
	  free(msg);
	  free(encoding);
	  free(lang);
	}
      aim_freetlvchain(&list2);
    }

  /*
   * Free up the TLV chain.
   */
  aim_freetlvchain(&tlvlist);
  

  return i;
}

/*
 * Not real sure what this does, nor does anyone I've talk to.
 *
 * Didn't use to send it.  But now I think it might be a good
 * idea. 
 *
 */
u_long aim_seticbmparam(struct aim_session_t *sess,
			struct aim_conn_t *conn)
{
  struct command_tx_struct *newpacket;
  int curbyte;

  if(!(newpacket = aim_tx_new(0x0002, conn, 10+16)))
    return -1;

  newpacket->lock = 1;

  curbyte = aim_putsnac(newpacket->data, 0x0004, 0x0002, 0x0000, sess->snac_nextid);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put32(newpacket->data+curbyte, 0x00000003);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x1f);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x40);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x03);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0xe7);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0x03);
  curbyte += aimutil_put8(newpacket->data+curbyte,  0xe7);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);
  curbyte += aimutil_put16(newpacket->data+curbyte, 0x0000);

  newpacket->lock = 0;
  aim_tx_enqueue(sess, newpacket);

  return (sess->snac_nextid++);
}

int aim_parse_msgerror_middle(struct aim_session_t *sess,
			      struct command_rx_struct *command)
{
  u_long snacid = 0x000000000;
  struct aim_snac_t *snac = NULL;
  int ret = 0;
  rxcallback_t userfunc = NULL;

  /*
   * Get SNAC from packet and look it up 
   * the list of unrepliedto/outstanding
   * SNACs.
   *
   * After its looked up, the SN that the
   * message should've gone to will be 
   * in the ->data element of the snac struct.
   *
   */
  snacid = aimutil_get32(command->data+6);
  snac = aim_remsnac(sess, snacid);

  if (!snac)
    {
      printf("faim: msgerr: got an ICBM-failed error on an unknown SNAC ID! (%08lx)\n", snacid);
    }

  /*
   * Call client.
   */
  userfunc = aim_callhandler(command->conn, 0x0004, 0x0001);
  if (userfunc)
    ret =  userfunc(sess, command, (snac)?snac->data:"(UNKNOWN)");
  else
    ret = 0;
  
  free(snac->data);
  free(snac);

  return ret;
}


