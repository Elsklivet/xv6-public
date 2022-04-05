#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  Header *bp, *p;

  // BP points to the start of the block, which is one header behind
  // the payload, which is what we get a pointer to.
  bp = (Header*)ap - 1;
  // Start iterating from beginning of free list and while
  // the block we are freeing is either not greater than our iterator ptr
  // or not less than the block after the iterator ptr.
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    // If the block points to itself and the loop conditions are true, break
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  
  // If block after bp is p.next
  if(bp + bp->s.size == p->s.ptr){
    // bp consumes p.next
    bp->s.size += p->s.ptr->s.size;
    // bp's next becomes p.next
    bp->s.ptr = p->s.ptr->s.ptr;
    // Was that coalesce?
  } else
    // Otherwise bp.next = p.next
    bp->s.ptr = p->s.ptr;
  // If p's next block is bp
  if(p + p->s.size == bp){
    // p consumes bp, another coalesce
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    // Otherwise p.next = bp
    p->s.ptr = bp;

  // Free list (if that's what this is) starts at 
  // wherever p was when loop ended
  freep = p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  // Bytes plus header / headers is how many "blocks" we're requesting.
  // What are the -1 and +1 doing?
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  // No lists yet?
  if((prevp = freep) == 0){
    // Initialize all to point to base and base to zero-size
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  // For each block in list
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    // If this one fits
    if(p->s.size >= nunits){
      // If it's an exact fit
      if(p->s.size == nunits)
        // Previous block points to next block so we can use this one
        prevp->s.ptr = p->s.ptr;
      else {
        // If it is not an exact fit then subtract the number of units...
        p->s.size -= nunits;
        // Jump forward to spot we're allocing (previous area is 
        // simply left to be free; is this a haphazard "split"?)
        p += p->s.size;
        // Set size accordingly
        p->s.size = nunits;
      }

      // First free block starts at block before this one
      freep = prevp;
      // Return this block for use
      return (void*)(p + 1);
    }
    // If we reach the free blocks we request more space
    // and if we get nothing back from it we error out.
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}
