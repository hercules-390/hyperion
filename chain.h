/* CHAIN.H      (c) Mark L. Gaubatz, 1985-2012                       */
/*              Chain and queue macros and inline routines           */
/*              Adapted for use by Hercules                          */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */
/*                                                                   */
/*********************************************************************/

#ifndef _CHAIN_H_
#define _CHAIN_H_

/*-------------------------------------------------------------------*/
/* Define local macros for inline routines                           */
/*-------------------------------------------------------------------*/

#define anchor_id "Anchor->"
#define aeyesize 8
#define eyesize 8
#define raise_chain_validation_error                                   \
    raise (SIGSEGV);
#define chain_validate_pointer(_arg)                                   \
    if (_arg == 0 || _arg == NULL)                                     \
        raise_chain_validation_error;


/*-------------------------------------------------------------------*/
/* Chain Block                                                       */
/*-------------------------------------------------------------------*/

typedef struct _CHAINBLK {              /* Chain definition block    */
        char       anchoreye[aeyesize]; /* Anchor identifier         */
        char       eyecatcher[eyesize]; /* Eyecatcher to check       */
        u_int      containersize;       /* Size of container         */
        LOCK       lock;                /* Lock for chain            */
        uintptr_t *first;               /* First entry pointer       */
        uintptr_t *last;                /* Last entry pointer        */
        uintptr_t  offset;              /* Offset to container start */
} CHAINBLK;


/*-------------------------------------------------------------------*/
/* Chain                                                             */
/*-------------------------------------------------------------------*/

typedef struct _CHAIN {                 /* Chain member definition   */
        char       eyecatcher[eyesize]; /* Eyecatcher to check       */
        u_int      containersize;       /* Size of container         */
        uintptr_t *prev;                /* -> previous chain entry   */
        uintptr_t *next;                /* -> next chain entry       */
        uintptr_t  offset;              /* Offset to container start */
        CHAINBLK  *anchor;              /* -> owning CHAINBLK        */
} CHAIN;


/*-------------------------------------------------------------------*/
/* Chain management inline routines                                  */
/*-------------------------------------------------------------------*/

INLINE uintptr_t *chain_container(CHAIN *entry)
{
    return ((uintptr_t *)entry + entry->offset);
}

INLINE CHAINBLK *chain_validate_entry_header(CHAIN *entry)
{

    /* Ensure valid entry pointers */
    chain_validate_pointer(entry);
    chain_validate_pointer(entry->anchor);

    /* Make sure this is the anchor for a chain */
    if (memcmp(entry->anchor->anchoreye, anchor_id, aeyesize))
        raise_chain_validation_error;

    /* Validate eyecatcher */
    if (memcmp(entry->anchor->eyecatcher, entry->eyecatcher, eyesize))
        raise_chain_validation_error;

    return entry->anchor;
}

INLINE void chain_init_anchor(void *container, CHAINBLK *anchor, char *eyecatcher, const u_int containersize)
{
    chain_validate_pointer(container);
    chain_validate_pointer(anchor);
    chain_validate_pointer(eyecatcher);
    memcpy(anchor->anchoreye, anchor_id, aeyesize);
    {
        register char *a = anchor->eyecatcher;
        register char *e = eyecatcher;
        register char *limit = a + eyesize;
        for (; a < limit && *e; a++, e++)
             *a = *e;
        if (a == anchor->eyecatcher)
            raise_chain_validation_error;
        for (; a < limit; a++)
             *a = ' ';
    }
    anchor->containersize = containersize;
    initialize_lock(&anchor->lock);
    anchor->first = anchor->last = 0;
    anchor->offset = (uintptr_t) container - (uintptr_t) anchor;
}

INLINE void chain_init_entry(CHAINBLK *anchor, void *container, CHAIN *entry)
{
    chain_validate_pointer(container);
    chain_validate_pointer(anchor);
    chain_validate_pointer(entry);
    memcpy(&entry->eyecatcher, &anchor->eyecatcher, eyesize);
    entry->containersize = anchor->containersize;
    entry->prev = entry->next = 0;
    entry->offset = (uintptr_t *) container - (uintptr_t *) entry;
    entry->anchor = anchor;
}


INLINE void chain_first(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (!anchor->first)
        anchor->first = anchor->last = (uintptr_t *)entry;
    else
    {
        CHAIN *next = (CHAIN *)anchor->first + entry->offset;
        entry->next = anchor->first;
        next->prev = anchor->first = (uintptr_t *)entry;
    }
}

INLINE void chain_first_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    chain_first(entry);
    release_lock(&anchor->lock);
}

INLINE void chain_before(CHAIN *entry, CHAIN *before)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if ((!before->prev && anchor->first != (uintptr_t *)before) ||
        (before->prev && anchor->first == (uintptr_t *)before))
        raise_chain_validation_error;
    entry->next = (uintptr_t *)before;
    entry->prev = before->prev;
    before->prev = (uintptr_t *)entry;
    if (!entry->prev)
        anchor->first = (uintptr_t *)entry;
}

INLINE void chain_after(CHAIN *entry, CHAIN *after)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if ((!after->next && anchor->last != (uintptr_t *)after) ||
        (after->next && anchor->last == (uintptr_t *)after))
        raise_chain_validation_error;
    entry->next = after->next;
    entry->prev = (uintptr_t *)after;
    after->next = (uintptr_t *)entry;
    if (!entry->next)
        anchor->last = (uintptr_t *)entry;
}

INLINE void chain_last(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (!anchor->last)
        anchor->first = anchor->last = (uintptr_t *)entry;
    else
    {
        CHAIN *prev = (CHAIN *)anchor->last + entry->offset;
        entry->prev = anchor->last;
        prev->next = anchor->last = (uintptr_t *)entry;
    }
}

INLINE void chain_last_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    chain_last(entry);
    release_lock(&anchor->lock);
}

INLINE void unchain(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    if (entry->prev)
    {
        CHAIN *t = (CHAIN *)entry->prev;
        t->next = entry->next;
    }
    else
        anchor->first = entry->next;

    if (entry->next)
    {
        CHAIN *t = (CHAIN *)entry->next;
        t->prev = entry->prev;
    }
    else
        anchor->last = entry->prev;

    entry->prev = entry->next = 0;
}

INLINE uintptr_t *unchain_first(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->first;
    if (!entry)
        return 0;
    unchain(entry);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_first_locked(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->first;
    if (!entry)
        return 0;
    obtain_lock(&anchor->lock);
    entry = (CHAIN *)anchor->first;
    if (entry)
        unchain(entry);
    release_lock(&anchor->lock);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_last(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->last;
    if (!entry)
        return 0;
    unchain(entry);
    return chain_container(entry);
}

INLINE uintptr_t *unchain_last_locked(CHAINBLK *anchor)
{
    register CHAIN *entry = (CHAIN *)anchor->last;
    if (!entry)
        return 0;
    obtain_lock(&anchor->lock);
    entry = (CHAIN *)anchor->last;
    if (entry)
        unchain(entry);
    release_lock(&anchor->lock);
    return chain_container(entry);
}

INLINE void unchain_locked(CHAIN *entry)
{
    register CHAINBLK *anchor = chain_validate_entry_header(entry);
    obtain_lock(&anchor->lock);
    unchain(entry);
    release_lock(&anchor->lock);
}


/*-------------------------------------------------------------------*/
/* Chain aliases                                                     */
/*-------------------------------------------------------------------*/

INLINE void chain(CHAIN *entry)
{chain_last(entry);}


/*-------------------------------------------------------------------*/
/* Queue aliases                                                     */
/*-------------------------------------------------------------------*/

#define QUEUEBLK    CHAINBLK
#define QUEUE       CHAIN

INLINE void queue_init_anchor(void *container, QUEUEBLK *anchor, char *eyecatcher, const u_int containersize)
{chain_init_anchor(container, anchor, eyecatcher, containersize);}

INLINE void queue_init_entry(QUEUEBLK *anchor, void *container, QUEUE *entry)
{chain_init_entry(anchor, container, entry);}

INLINE void queue(QUEUE *entry)
{chain_last(entry);}

INLINE void queue_fifo(QUEUE *entry)
{chain_last(entry);}

INLINE void queue_lifo(QUEUE *entry)
{chain_first(entry);}

INLINE uintptr_t *dequeue_locked(QUEUEBLK *anchor)
{return unchain_first_locked(anchor);}

INLINE uintptr_t *dequeue(QUEUEBLK *anchor)
{return unchain_first(anchor);}


/*-------------------------------------------------------------------*/
/* Undefine local macros                                             */
/*-------------------------------------------------------------------*/

#undef chain_validate_pointer
#undef raise_chain_validation_error
#undef eyesize
#undef aeyesize
#undef anchor_id

#endif /* _CHAIN_H_ */
