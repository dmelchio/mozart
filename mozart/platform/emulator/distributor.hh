/*
 *  Authors:
 *    Christian Schulte <schulte@ps.uni-sb.de>
 * 
 *  Copyright:
 *    Christian Schulte, 1997
 * 
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 * 
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     http://www.mozart-oz.org
 * 
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */

#ifndef __DISTRIBUTOR_H_
#define __DISTRIBUTOR_H_

#ifdef INTERFACE
#pragma interface
#endif

#include "mem.hh"

class Distributor {
public:
  USEFREELISTMEMORY;
  
  // TODO: this function is to be deprecated
  virtual int getAlternatives(void);
  
  // TODO: this function is to be deprecated
  virtual int commit(Board *, int);
  
  // TODO: this function is to be derecated
  virtual int commit(Board *, int, int);

  /*
    \brief Notify stability to the distributor. This method is called
    from checkStability when space stability is guaranted. Return
    values include -1 for old-style distributors, 0 for the
    distributor to report that is no longer needed in the space and 1
    to ensure that distributor not should be removed from the
    space. Other values can be used in the future to report errors.
  */
  virtual int notifyStable(Board *);
	
  /*
    \brief Commit a branch to the distributor. Return values include 0
    when the distributor is no longer needed after the commit (i.e. it
    is done) and 1 to indicate the space that the distributor must be
    preserved. Other values can be used for early error detection.
  */
  virtual int commitBranch(Board *, TaggedRef );
	
  virtual Distributor * gCollect(void) = 0;
  virtual Distributor * sClone(void) = 0;

};

#endif



