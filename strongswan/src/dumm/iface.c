/*
 * Copyright (C) 2007 Martin Willi
 * Hochschule fuer Technik Rapperswil
 * Copyright (C) 2002 Jeff Dike
 *
 * Based on the "tunctl" utility from Jeff Dike.
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
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

#include <debug.h>

#include "iface.h"

typedef struct private_iface_t private_iface_t;

struct private_iface_t {
	/** public interface */
	iface_t public;
	/** device name in guest (eth0) */
	char *guestif;
	/** device name at host (tap0) */
	char *hostif;
	/** bridge this interface is attached to */
	bridge_t *bridge;
	/** mconsole for guest */
	mconsole_t *mconsole;
};

/**
 * bring an interface up or down
 */
bool iface_control(char *name, bool up)
{
	int s;
	bool good = FALSE;
	struct ifreq ifr;
	
	memset(&ifr, 0, sizeof(struct ifreq));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (!s)
	{
		return FALSE;
	}
	if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
	{
		if (up)
		{
			ifr.ifr_flags |= IFF_UP;
		}
		else
		{
			ifr.ifr_flags &= ~IFF_UP;
		}
		if (ioctl(s, SIOCSIFFLAGS, &ifr) == 0)
		{
			good = TRUE;
		}
	}
	close(s);
	return good;
}

/**
 * Implementation of iface_t.get_guestif.
 */
static char* get_guestif(private_iface_t *this)
{
	return this->guestif;
}

/**
 * Implementation of iface_t.get_hostif.
 */
static char* get_hostif(private_iface_t *this)
{
	return this->hostif;
}

/**
 * Implementation of iface_t.set_bridge.
 */
static void set_bridge(private_iface_t *this, bridge_t *bridge)
{
	this->bridge = bridge;
}

/**
 * destroy the tap device
 */
static bool destroy_tap(private_iface_t *this)
{
	struct ifreq ifr;
	int tap;
	
	if (!iface_control(this->hostif, FALSE))
	{
		DBG1("bringing iface down failed: %m");
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, this->hostif, sizeof(ifr.ifr_name) - 1);
	
	tap = open(TAP_DEVICE, O_RDWR);
	if (tap < 0)
	{
		DBG1("unable to open tap device %s: %m", TAP_DEVICE);
		return FALSE;
	}
	if (ioctl(tap, TUNSETIFF, &ifr) < 0 ||
		ioctl(tap, TUNSETPERSIST, 0) < 0)
	{
		DBG1("removing %s failed: %m", this->hostif);
		close(tap);
		return FALSE;
	}
	close(tap);
	return TRUE;
}

/**
 * create the tap device
 */
static char* create_tap(private_iface_t *this, char *guest)
{
	struct ifreq ifr;
	int tap;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s-%s", guest, this->guestif);

	tap = open(TAP_DEVICE, O_RDWR);
	if (tap < 0)
	{
		DBG1("unable to open tap device %s: %m", TAP_DEVICE);
		return NULL;
	}
	if (ioctl(tap, TUNSETIFF, &ifr) < 0 ||
		ioctl(tap, TUNSETPERSIST, 1) < 0 ||
		ioctl(tap, TUNSETOWNER, 0))
    {
		DBG1("creating new tap device failed: %m");
		close(tap);
		return NULL;
    } 
	close(tap);
	return strdup(ifr.ifr_name);
}

/**
 * Implementation of iface_t.destroy.
 */
static void destroy(private_iface_t *this)
{
	if (this->bridge)
	{
		this->bridge->disconnect_iface(this->bridge, &this->public);
	}
	this->mconsole->del_iface(this->mconsole, this->guestif);
	destroy_tap(this);
	free(this->guestif);
	free(this->hostif);
	free(this);
}

/**
 * create the iface instance
 */
iface_t *iface_create(char *guest, char *guestif, mconsole_t *mconsole)
{
	private_iface_t *this = malloc_thing(private_iface_t);
	
	this->public.get_hostif = (char*(*)(iface_t*))get_hostif;
	this->public.get_guestif = (char*(*)(iface_t*))get_guestif;
	this->public.set_bridge = (void(*)(iface_t*, bridge_t*))set_bridge;
	this->public.destroy = (void*)destroy;

	this->mconsole = mconsole;
	this->guestif = strdup(guestif);
	this->hostif = create_tap(this, guest);
	this->bridge = NULL;
	if (this->hostif == NULL)
	{
		destroy_tap(this);
		free(this->guestif);
		free(this);
		return NULL;
	}
	if (!iface_control(this->hostif, TRUE))
	{
		DBG1("bringing iface '%s' up failed: %m", this->hostif);
	}
	if (!this->mconsole->add_iface(this->mconsole, this->guestif, this->hostif))
	{
		DBG1("creating interface '%s' in guest failed", this->guestif);
		destroy_tap(this);
		free(this->guestif);
		free(this->hostif);
		free(this);
		return NULL;
	}
	return &this->public;
}

