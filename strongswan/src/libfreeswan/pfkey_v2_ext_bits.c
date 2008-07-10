/*
 * RFC2367 PF_KEYv2 Key management API message parser
 * Copyright (C) 1999, 2000, 2001  Richard Guy Briggs.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * RCSID $Id: pfkey_v2_ext_bits.c 3265 2007-10-08 19:52:55Z andreas $
 */

/*
 *		Template from klips/net/ipsec/ipsec/ipsec_parse.c.
 */

char pfkey_v2_ext_bits_c_version[] = "$Id: pfkey_v2_ext_bits.c 3265 2007-10-08 19:52:55Z andreas $";

/*
 * Some ugly stuff to allow consistent debugging code for use in the
 * kernel and in user space
*/

#ifdef __KERNEL__

# include <linux/kernel.h>  /* for printk */

# include "freeswan/ipsec_kversion.h" /* for malloc switch */
# ifdef MALLOC_SLAB
#  include <linux/slab.h> /* kmalloc() */
# else /* MALLOC_SLAB */
#  include <linux/malloc.h> /* kmalloc() */
# endif /* MALLOC_SLAB */
# include <linux/errno.h>  /* error codes */
# include <linux/types.h>  /* size_t */
# include <linux/interrupt.h> /* mark_bh */

# include <linux/netdevice.h>   /* struct device, and other headers */
# include <linux/etherdevice.h> /* eth_type_trans */
# include <linux/ip.h>          /* struct iphdr */ 
# if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
#  include <linux/ipv6.h>
# endif /* defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE) */

#else /* __KERNEL__ */

# include <sys/types.h>
# include <linux/types.h>
# include <linux/errno.h>
#endif

#include <freeswan.h>
#include <pfkeyv2.h>
#include <pfkey.h>

unsigned int extensions_bitmaps[2/*in/out*/][2/*perm/req*/][SADB_MAX + 1/*ext*/] = {

/* INBOUND EXTENSIONS */
{

/* PERMITTED IN */
{
/* SADB_RESERVED */
0
,
/* SADB_GETSPI */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_SPIRANGE
,
/* SADB_UPDATE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
,
/* SADB_ADD */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_X_EXT_NAT_T_TYPE
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
| 1<<SADB_X_EXT_NAT_T_OA
,
/* SADB_DELETE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_GET */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_ACQUIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
,
/* SADB_REGISTER */
1<<SADB_EXT_RESERVED
,
/* SADB_EXPIRE */
0
,
/* SADB_FLUSH */
1<<SADB_EXT_RESERVED
,
/* SADB_DUMP */
1<<SADB_EXT_RESERVED
,
/* SADB_X_PROMISC */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_PCHANGE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_GRPSA */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_ADDFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_X_EXT_PROTOCOL
,
/* SADB_X_DELFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_X_EXT_PROTOCOL
,
/* SADB_X_DEBUG */
1<<SADB_EXT_RESERVED
| 1<<SADB_X_EXT_DEBUG
,
/* SADB_X_NAT_T_NEW_MAPPING */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
},

/* REQUIRED IN */
{
/* SADB_RESERVED */
0
,
/* SADB_GETSPI */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_SPIRANGE
,
/* SADB_UPDATE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
/*| 1<<SADB_EXT_KEY_AUTH*/
/*| 1<<SADB_EXT_KEY_ENCRYPT*/
,
/* SADB_ADD */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
/*| 1<<SADB_EXT_KEY_AUTH*/
/*| 1<<SADB_EXT_KEY_ENCRYPT*/
,
/* SADB_DELETE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_GET */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_ACQUIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_PROPOSAL
,
/* SADB_REGISTER */
1<<SADB_EXT_RESERVED
,
/* SADB_EXPIRE */
0
,
/* SADB_FLUSH */
1<<SADB_EXT_RESERVED
,
/* SADB_DUMP */
1<<SADB_EXT_RESERVED
,
/* SADB_X_PROMISC */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_PCHANGE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_GRPSA */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
/*| 1<<SADB_X_EXT_SATYPE2*/
/*| 1<<SADB_X_EXT_SA2*/
/*| 1<<SADB_X_EXT_ADDRESS_DST2*/
,
/* SADB_X_ADDFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
,
/* SADB_X_DELFLOW */
1<<SADB_EXT_RESERVED
/*| 1<<SADB_EXT_SA*/
#if 0 /* SADB_X_CLREROUTE doesn't need all these... */
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
#endif
,
/* SADB_X_DEBUG */
1<<SADB_EXT_RESERVED
| 1<<SADB_X_EXT_DEBUG
,
/* SADB_X_NAT_T_NEW_MAPPING */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
}

},

/* OUTBOUND EXTENSIONS */
{

/* PERMITTED OUT */
{
/* SADB_RESERVED */
0
,
/* SADB_GETSPI */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_UPDATE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
,
/* SADB_ADD */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_X_EXT_NAT_T_TYPE
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
| 1<<SADB_X_EXT_NAT_T_OA
,
/* SADB_DELETE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_GET */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
,
/* SADB_ACQUIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
,
/* SADB_REGISTER */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
,
/* SADB_EXPIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_FLUSH */
1<<SADB_EXT_RESERVED
,
/* SADB_DUMP */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
,
/* SADB_X_PROMISC */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_PCHANGE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_GRPSA */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_ADDFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
| 1<<SADB_X_EXT_PROTOCOL
,
/* SADB_X_DELFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
| 1<<SADB_X_EXT_PROTOCOL
,
/* SADB_X_DEBUG */
1<<SADB_EXT_RESERVED
| 1<<SADB_X_EXT_DEBUG
,
/* SADB_X_NAT_T_NEW_MAPPING */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
},

/* REQUIRED OUT */
{
/* SADB_RESERVED */
0
,
/* SADB_GETSPI */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_UPDATE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_ADD */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_DELETE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_GET */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
/* | 1<<SADB_EXT_KEY_AUTH */
/* | 1<<SADB_EXT_KEY_ENCRYPT */
,
/* SADB_ACQUIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_PROPOSAL
,
/* SADB_REGISTER */
1<<SADB_EXT_RESERVED
/* | 1<<SADB_EXT_SUPPORTED_AUTH
   | 1<<SADB_EXT_SUPPORTED_ENCRYPT */
,
/* SADB_EXPIRE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
/* | 1<<SADB_EXT_LIFETIME_HARD
   | 1<<SADB_EXT_LIFETIME_SOFT */
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_FLUSH */
1<<SADB_EXT_RESERVED
,
/* SADB_DUMP */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
,
/* SADB_X_PROMISC */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_PCHANGE */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_LIFETIME_CURRENT
| 1<<SADB_EXT_LIFETIME_HARD
| 1<<SADB_EXT_LIFETIME_SOFT
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_EXT_ADDRESS_PROXY
| 1<<SADB_EXT_KEY_AUTH
| 1<<SADB_EXT_KEY_ENCRYPT
| 1<<SADB_EXT_IDENTITY_SRC
| 1<<SADB_EXT_IDENTITY_DST
| 1<<SADB_EXT_SENSITIVITY
| 1<<SADB_EXT_PROPOSAL
| 1<<SADB_EXT_SUPPORTED_AUTH
| 1<<SADB_EXT_SUPPORTED_ENCRYPT
| 1<<SADB_EXT_SPIRANGE
| 1<<SADB_X_EXT_KMPRIVATE
| 1<<SADB_X_EXT_SATYPE2
| 1<<SADB_X_EXT_SA2
| 1<<SADB_X_EXT_ADDRESS_DST2
,
/* SADB_X_GRPSA */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
,
/* SADB_X_ADDFLOW */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
,
/* SADB_X_DELFLOW */
1<<SADB_EXT_RESERVED
/*| 1<<SADB_EXT_SA*/
| 1<<SADB_X_EXT_ADDRESS_SRC_FLOW
| 1<<SADB_X_EXT_ADDRESS_DST_FLOW
| 1<<SADB_X_EXT_ADDRESS_SRC_MASK
| 1<<SADB_X_EXT_ADDRESS_DST_MASK
,
/* SADB_X_DEBUG */
1<<SADB_EXT_RESERVED
| 1<<SADB_X_EXT_DEBUG
,
/* SADB_X_NAT_T_NEW_MAPPING */
1<<SADB_EXT_RESERVED
| 1<<SADB_EXT_SA
| 1<<SADB_EXT_ADDRESS_SRC
| 1<<SADB_EXT_ADDRESS_DST
| 1<<SADB_X_EXT_NAT_T_SPORT
| 1<<SADB_X_EXT_NAT_T_DPORT
}
}
};
