/*
 *  Authors:
 *    Michael Mehl (mehl@dfki.de)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Michael Mehl (1998)
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

#ifndef __SIMPLEVAR__H__
#define __SIMPLEVAR__H__

#if defined(INTERFACE)
#pragma interface
#endif

#include "var_base.hh"

class SimpleVar: public OzVariable {
public:
  SimpleVar(Board *bb) : OzVariable(OZ_VAR_SIMPLE,bb) {}

  OZ_Return bind(TaggedRef* vPtr, TaggedRef t);
  OZ_Return unify(TaggedRef* vPtr, TaggedRef *tPtr);

  OZ_Return valid(TaggedRef /* val */) { return OK; }

  void dispose(void) {
    disposeS();
    oz_freeListDispose(this, sizeof(SimpleVar));
  }

  void printStream(ostream &out,int depth = 10) {
    out << "<simple>";
  }
  void printLongStream(ostream &out,int depth = 10,
			int offset = 0) {
    printStream(out,depth); out << endl;
  }
};

#endif /* __SIMPLEVAR__H__ */