/* Copyright (C) 2003 Jamey Sharp.
 * This file is licensed under the MIT license. See the file COPYING. */

#include "Xlibint.h"
#include "xclint.h"
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <stdio.h>

void _XFreeDisplayStructure(Display *dpy);

static XCBAuthInfo xauth;

static void *alloc_copy(const void *src, int *dstn, size_t n)
{
	void *dst;
	if(n <= 0)
	{
		*dstn = 0;
		return 0;
	}
	dst = Xmalloc(n);
	if(!dst)
		return 0;
	memcpy(dst, src, n);
	*dstn = n;
	return dst;
}

XCBConnection *XCBConnectionOfDisplay(Display *dpy)
{
	return dpy->xcl->connection;
}

void XSetAuthorization(char *name, int namelen, char *data, int datalen)
{
	_XLockMutex(_Xglobal_lock);
	Xfree(xauth.name);
	Xfree(xauth.data);

	/* if either of these allocs fail, _XConnectXCB won't use this auth
	 * data, so we don't need to check it here. */
	xauth.name = alloc_copy(name, &xauth.namelen, namelen);
	xauth.data = alloc_copy(data, &xauth.datalen, datalen);

#if 0 /* but, for the paranoid among us: */
	if((namelen > 0 && !xauth.name) || (datalen > 0 && !xauth.data))
	{
		Xfree(xauth.name);
		Xfree(xauth.data);
		xauth.name = xauth.data = 0;
		xauth.namelen = xauth.datalen = 0;
	}
#endif

	_XUnlockMutex(_Xglobal_lock);
}

int _XConnectXCB(Display *dpy, _Xconst char *display, char **fullnamep, int *screenp)
{
	char *host;
	int n = 0;
	int len;
	XCBConnection *c;

	dpy->fd = -1;

	dpy->xcl = Xcalloc(1, sizeof(XCLPrivate));
	if(!dpy->xcl)
		return 0;

	if(!XCBParseDisplay(display, &host, &n, screenp))
		return 0;

	len = strlen(host) + (1 + 20 + 1 + 20 + 1);
	*fullnamep = Xmalloc(len);
	snprintf(*fullnamep, len, "%s:%d.%d", host, n, *screenp);
	free(host);

	_XLockMutex(_Xglobal_lock);
	if(xauth.name && xauth.data)
		c = XCBConnectToDisplayWithAuthInfo(display, &xauth, 0);
	else
		c = XCBConnect(display, 0);
	_XUnlockMutex(_Xglobal_lock);

	if(!c)
		return 0;

	dpy->fd = XCBGetFileDescriptor(c);

	dpy->xcl->connection = c;
	dpy->xcl->pending_requests_tail = &dpy->xcl->pending_requests;
	return 1;
}

void _XFreeXCLStructure(Display *dpy)
{
	/* reply_data was allocated by system malloc, not Xmalloc */
	free(dpy->xcl->reply_data);
	while(dpy->xcl->pending_requests)
	{
		PendingRequest *tmp = dpy->xcl->pending_requests;
		dpy->xcl->pending_requests = tmp->next;
		free(tmp);
	}
	Xfree(dpy->xcl);
}