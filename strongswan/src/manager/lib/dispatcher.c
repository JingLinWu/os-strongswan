/**
 * @file dispatcher.c
 *
 * @brief Implementation of dispatcher_t.
 *
 */

/*
 * Copyright (C) 2007 Martin Willi
 * Hochschule fuer Technik Rapperswil
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

#include "dispatcher.h"

#include "request.h"
#include "session.h"

#include <fcgiapp.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <debug.h>
#include <utils/linked_list.h>

typedef struct private_dispatcher_t private_dispatcher_t;

/**
 * private data of the task manager
 */
struct private_dispatcher_t {

	/**
	 * public functions
	 */
	dispatcher_t public;
	
	/**
	 * fcgi socket fd
	 */
	int fd;
	
	/**
	 * thread list
	 */
	pthread_t *threads;
	
	/**
	 * number of threads in "threads"
	 */
	int thread_count;
	
	/**
	 * session locking mutex
	 */
	pthread_mutex_t mutex;
	
	/**
	 * List of sessions
	 */
	linked_list_t *sessions;
	
	/**
	 * session timeout
	 */
	time_t timeout;
	
	/**
	 * List of controllers controller_constructor_t
	 */
	linked_list_t *controllers;
	
	/** 
	 * constructor function to create session context (in constructor_entry_t)
	 */
	context_constructor_t context_constructor;
	
	/**
	 * user param to context constructor
	 */
	void *param;
	
	/**
	 * thread specific initialization handler
	 */
	void (*init)(void *param);
	
	/**
	 * argument to pass to thread intiializer
	 */
	void *init_param;
	
	/**
	 * thread specific deinitialization handler
	 */
	void (*deinit)(void *param);
	
	/**
	 * param tho thread specific deinitialization handler
	 */
	void *deinit_param;
};

typedef struct {
	/** constructor function */
	controller_constructor_t constructor;
	/** parameter to constructor */
	void *param;
} constructor_entry_t;

typedef struct {
	/** session instance */
	session_t *session;
	/** condvar to wait for session */
	pthread_cond_t cond;
	/** TRUE if session is in use */
	bool in_use;
	/** last use of the session */
	time_t used;
} session_entry_t;

/**
 * create a session and instanciate controllers
 */
static session_t* load_session(private_dispatcher_t *this)
{
	iterator_t *iterator;
	constructor_entry_t *entry;
	session_t *session;
	context_t *context = NULL;
	controller_t *controller;
	
	if (this->context_constructor)
	{
		context = this->context_constructor(this->param);
	}
	session = session_create(context);
	
	iterator = this->controllers->create_iterator(this->controllers, TRUE);
	while (iterator->iterate(iterator, (void**)&entry))
	{
		controller = entry->constructor(context, entry->param);
		session->add_controller(session, controller);
	}
	iterator->destroy(iterator);
	
	return session;
}

/**
 * create a new session entry
 */
static session_entry_t *session_entry_create(private_dispatcher_t *this)
{
	session_entry_t *entry;
	
	entry = malloc_thing(session_entry_t);
	entry->in_use = FALSE;
	pthread_cond_init(&entry->cond, NULL);
	entry->session = load_session(this);
	entry->used = time(NULL);
	
	return entry;
}

static void session_entry_destroy(session_entry_t *entry)
{
	entry->session->destroy(entry->session);
	free(entry);
}

/**
 * Implementation of dispatcher_t.add_controller.
 */
static void add_controller(private_dispatcher_t *this,
						   controller_constructor_t constructor, void *param)
{
	constructor_entry_t *entry = malloc_thing(constructor_entry_t);
	
	entry->constructor = constructor;
	entry->param = param;
	this->controllers->insert_last(this->controllers, entry);
}

/**
 * Actual dispatching code 
 */
static void dispatch(private_dispatcher_t *this)
{
	FCGX_Request fcgi_req;
	
	if (FCGX_InitRequest(&fcgi_req, this->fd, 0) == 0)
	{
		while (TRUE)
		{
			request_t *request;
			session_entry_t *current, *found = NULL;
			iterator_t *iterator;
			time_t now;
			char *sid;
			int accepted;
			
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			accepted = FCGX_Accept_r(&fcgi_req);
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			
			if (accepted != 0)
			{
				break;
			}
			
			/* prepare */
			request = request_create(&fcgi_req, TRUE);
			if (request == NULL)
			{
				continue;
			}
			sid = request->get_cookie(request, "SID");
			now = time(NULL);
			
			/* find session */
			pthread_mutex_lock(&this->mutex);
			iterator = this->sessions->create_iterator(this->sessions, TRUE);
			while (iterator->iterate(iterator, (void**)&current))
			{
				/* check all sessions for timeout */
				if (!current->in_use &&
					current->used < now - this->timeout)
				{
					iterator->remove(iterator);
					session_entry_destroy(current);
					continue;
				}
				if (!found && sid &&
					streq(current->session->get_sid(current->session), sid))
				{
					found = current;
				}
			}
			iterator->destroy(iterator);
			
			if (found)
			{	/* wait until session is unused */
				while (found->in_use)
				{
					pthread_cond_wait(&found->cond, &this->mutex);
				}
			}
			else
			{	/* create a new session if not found */
				found = session_entry_create(this);
				this->sessions->insert_first(this->sessions, found);
			}
			found->in_use = TRUE;
			pthread_mutex_unlock(&this->mutex);
		
			/* start processing */
			found->session->process(found->session, request);
			found->used = time(NULL);
			
			/* release session */
			pthread_mutex_lock(&this->mutex);
			found->in_use = FALSE;
			pthread_cond_signal(&found->cond);
			pthread_mutex_unlock(&this->mutex);
			
			/* cleanup */
			request->destroy(request);
			
			/*
		    FCGX_FPrintF(fcgi_req.out, "<ul>");
		    char **env = fcgi_req.envp;
		    while (*env)
		    {
		        FCGX_FPrintF(fcgi_req.out, "<li>%s</li>", *env);
		        env++;
		    }
		    FCGX_FPrintF(fcgi_req.out, "</ul>");
		    */
		}
	}
}

/**
 * Setup thread and start dispatching 
 */
static void start_dispatching(private_dispatcher_t *this)
{
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (this->init)
	{
		this->init(this->init_param);
	}
	if (this->deinit)
	{
		pthread_cleanup_push(this->deinit, this->deinit_param);
		dispatch(this);
		pthread_cleanup_pop(1);
	}
	else
	{
		dispatch(this);
	}
}

/**
 * Implementation of dispatcher_t.run.
 */
static void run(private_dispatcher_t *this, int threads,
				void(*init)(void *param), void *init_param,
				void(*deinit)(void *param), void *deinit_param)
{
	this->init = init;
	this->init_param = init_param;
	this->deinit = deinit;
	this->deinit_param = deinit_param;
	this->thread_count = threads;
	this->threads = malloc(sizeof(pthread_t) * threads);
	while (threads)
	{
		if (pthread_create(&this->threads[threads - 1],
						   NULL, (void*)start_dispatching, this) == 0)
		{
			threads--;
		}
	}
}

/**
 * Implementation of dispatcher_t.waitsignal.
 */
static void waitsignal(private_dispatcher_t *this)
{
	sigset_t set;
	int sig;
	
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	sigprocmask(SIG_BLOCK, &set, NULL);
	sigwait(&set, &sig);
}

/**
 * Implementation of dispatcher_t.destroy
 */
static void destroy(private_dispatcher_t *this)
{
	FCGX_ShutdownPending();
	while (this->thread_count--)
	{
		pthread_cancel(this->threads[this->thread_count]);
		pthread_join(this->threads[this->thread_count], NULL);
	}
	this->sessions->destroy_function(this->sessions, (void*)session_entry_destroy);
	this->controllers->destroy_function(this->controllers, free);
	free(this);
}

/*
 * see header file
 */
dispatcher_t *dispatcher_create(char *socket, int timeout,
								context_constructor_t constructor, void *param)
{
	private_dispatcher_t *this = malloc_thing(private_dispatcher_t);

	this->public.add_controller = (void(*)(dispatcher_t*, controller_constructor_t, void*))add_controller;
	this->public.run = (void(*)(dispatcher_t*, int threads,void(*)(void *),void *,void(*)(void *),void *))run;
	this->public.waitsignal = (void(*)(dispatcher_t*))waitsignal;
	this->public.destroy = (void(*)(dispatcher_t*))destroy;
	
	this->sessions = linked_list_create();
	this->controllers = linked_list_create();
	this->context_constructor = constructor;
	pthread_mutex_init(&this->mutex, NULL);
	this->param = param;
    this->fd = 0;
    this->timeout = timeout;
	
    FCGX_Init();
    
    if (socket)
    {
		unlink(socket);
		this->fd = FCGX_OpenSocket(socket, 10);
	}
	return &this->public;
}

