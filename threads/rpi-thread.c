// engler, cs140e: starter code for trivial threads package.
#include "rpi.h"
#include "rpi-thread.h"
#include "redzone.h"

// tracing code.  set <trace_p>=0 to stop tracing
enum { trace_p = 1};
#define th_trace(args...) do {                          \
    if(trace_p) {                                       \
        trace(args);                                   \
    }                                                   \
} while(0)

/***********************************************************************
 * datastructures used by the thread code.
 *
 * you don't have to modify this.
 */

#define E rpi_thread_t
#include "libc/Q.h"

// currently only have a single run queue and a free queue.
// the run queue is FIFO.
static Q_t runq, freeq;
static rpi_thread_t *cur_thread;        // current running thread.
static rpi_thread_t *scheduler_thread;  // first scheduler thread.

// monotonically increasing thread id: won't wrap before reboot :)
static unsigned tid = 1;

/***********************************************************************
 * simplistic pool of thread blocks: used to make alloc/free faster (plus,
 * our kmalloc doesn't have free (other than reboot).
 *
 * you don't have to modify this.
 */

// total number of thread blocks we have allocated.
static unsigned nalloced = 0;

// keep a cache of freed thread blocks.  call kmalloc if run out.
static rpi_thread_t *th_alloc(void) {
    redzone_check(0);
    rpi_thread_t *t = Q_pop(&freeq);

    if(!t) {
        t = kmalloc_aligned(sizeof *t, 8);
        nalloced++;
    }
#   define is_aligned(_p,_n) (((unsigned)(_p))%(_n) == 0)
    demand(is_aligned(&t->stack[0],8), stack must be 8-byte aligned!);
    t->tid = tid++;
    return t;
}

static void th_free(rpi_thread_t *th) {
    redzone_check(0);
    // push on the front in case helps with caching.
    Q_push(&freeq, th);
}


/***********************************************************************
 * implement the code below.
 */

// stack offsets we expect.
enum {
    R4_OFFSET = 0,
    R5_OFFSET,
    R6_OFFSET,
    R7_OFFSET,
    R8_OFFSET,
    R9_OFFSET,
    R10_OFFSET,
    R11_OFFSET,
    R14_OFFSET = 8,
    LR_OFFSET = 8
};

// return pointer to the current thread.  
rpi_thread_t *rpi_cur_thread(void) {
    assert(cur_thread); 
    return cur_thread;
}

// create a new thread.
rpi_thread_t *rpi_fork(void (*code)(void *arg), void *arg) {
    redzone_check(0);
    rpi_thread_t *t = th_alloc();

    // write this so that it calls code,arg.
    void rpi_init_trampoline(void);

    /*
     * must do the "brain surgery" (k.thompson) to set up the stack
     * so that when we context switch into it, the code will be
     * able to call code(arg).
     *
     *  1. write the stack pointer with the right value.
     *  2. store arg and code into two of the saved registers.
     *  3. store the address of rpi_init_trampoline into the lr
     *     position so context switching will jump there.
     */
    
    t->fn = code; 
    t->arg = arg;

    // set saved_sp to the top of the stack
    t->saved_sp = &(t->stack[THREAD_MAXSTACK - LR_OFFSET - 1]);
    
    int lr_index = THREAD_MAXSTACK - 8 - LR_OFFSET; 
    int r4_index = THREAD_MAXSTACK - 8 - R4_OFFSET; 
    int r5_index = THREAD_MAXSTACK - 8 - R5_OFFSET; 

    // set address of trampoline to the LR_OFFSET
    t->saved_sp[LR_OFFSET] = (uint32_t)(&rpi_init_trampoline); // todo: check if this is valid function pointer
    // save code and arg at other stack offsets
    t->saved_sp[R4_OFFSET] = (uint32_t)(code); 
    t->saved_sp[R5_OFFSET] = (uint32_t)(arg); 

    th_trace("rpi_fork: tid=%d, code=[%p], arg=[%x], saved_sp=[%p]\n",
            t->tid, code, arg, t->saved_sp);

    Q_append(&runq, t);
    return t;
}


// exit current thread.
//   - if no more threads, switch to the scheduler.
//   - otherwise context switch to the new thread.
//     make sure to set cur_thread correctly!
void rpi_exit(int exitcode) {
    redzone_check(0);

    rpi_thread_t *prev_t = cur_thread; // prev_t points to cur_thread
    if (!Q_empty(&runq)) { // if can dqueue a new runnable thread
        cur_thread = Q_pop(&runq); // cur_thread now is pointing to another one
        // trace("swithcing from %p, %p: ", prev_t, cur_thread); 
        // th_trace("switching from %p to %p", prev_t, cur_thread);
        rpi_cswitch(&prev_t->saved_sp, cur_thread->saved_sp); 
        th_free(prev_t); // free the old one
    } else {  // when you switch back to the scheduler thread:
        th_trace("done running threads, back to scheduler\n");
        cur_thread = scheduler_thread; 
        rpi_cswitch(&prev_t->saved_sp, scheduler_thread->saved_sp); 
        
    }
    // panic("CUSTOM: not supposed to be in rpi_exit!"); 

    // should never return.
    not_reached();
}

// yield the current thread.
//   - if the runq is empty, return.
//   - otherwise: 
//      * add the current thread to the back 
//        of the runq (Q_append)
//      * context switch to the new thread.
//        make sure to set cur_thread correctly!
void rpi_yield(void) {
    redzone_check(0);

    rpi_thread_t *prev_t = cur_thread; // points to old thread
    if (!Q_empty(&runq)) { // if can dqueue a new runnable thread
        Q_append(&runq, prev_t); // add current thread to back of the runq
        cur_thread = Q_pop(&runq); 
        // if you switch, print the statement:
        th_trace("switching from tid=%d to tid=%d\n", prev_t->tid, cur_thread->tid);
        // trace("swithcing from %p to %p: ", prev_t, cur_thread); 
        rpi_cswitch(&prev_t->saved_sp, cur_thread->saved_sp); 
        // note: potentially run fn(arg) here ??
        th_free(prev_t); // free the old one

        
    } else {  // when you switch back to the scheduler thread:
        return;
    }

    // yield to the first thread on the run-queue
    // todo("implement the rest of rpi_yield");
}

/*
 * starts the thread system.  
 * note: our caller is not a thread!  so you have to 
 * create a fake thread (assign it to scheduler_thread)
 * so that context switching works correctly.   your code
 * should work even if the runq is empty.
 */
void rpi_thread_start(void) {
    // NOTE: commented out for part 4 and part 5
    // redzone_init();
    th_trace("starting threads!\n");

    // no other runnable thread: return.
    if(Q_empty(&runq))
        goto end;

    // setup scheduler thread block.
    if(!scheduler_thread)
        scheduler_thread = th_alloc();
    
    // first runnable thread
    // rpi_thread_t *prev_t;
    cur_thread = Q_pop(&runq); // pop queue
    rpi_cswitch(&scheduler_thread->saved_sp, cur_thread->saved_sp);
    // (*cur_thread->fn)(cur_thread->arg);  // call function
    // th_free(cur_thread); 

    // while (!Q_empty(&runq)) { // remove remaining threads in fifo order
    //     prev_t = cur_thread; // note: make sure this makes a copy
    //     cur_thread = Q_pop(&runq); // pop queue
    //     // context switch 
    //     rpi_cswitch(&prev_t->saved_sp, cur_thread->saved_sp); 

    //     // (*cur_thread->fn)(cur_thread->arg);  // call function
    //     th_free(cur_thread); 
    // }

    // context switch back into dummy thread (q. why?)
    // rpi_cswitch(&cur_thread->saved_sp, scheduler_thread->saved_sp);
    // cur_thread = scheduler_thread; 

    

end:
    redzone_check(0);
    // if not more threads should print:
    th_trace("done with all threads, returning\n");
}

// helper routine: can call from assembly with r0=sp and it
// will print the stack out.  it then exits.
// call this if you can't figure out what is going on in your
// assembly.
void rpi_print_regs(uint32_t *sp) {
    // use this to check that your offsets are correct.
    printk("cur-thread=%d\n", cur_thread->tid);
    printk("sp=%p\n", sp);

    // stack pointer better be between these.
    printk("stack=%p\n", &cur_thread->stack[THREAD_MAXSTACK]);
    assert(sp < &cur_thread->stack[THREAD_MAXSTACK]);
    assert(sp >= &cur_thread->stack[0]);
    for(unsigned i = 0; i < 9; i++) {
        unsigned r = i == 8 ? 14 : i + 4;
        printk("sp[%d]=r%d=%x\n", i, r, sp[i]);
    }
    clean_reboot();
}
