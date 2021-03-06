/*
 *  Authors:
 *    Erik Klintskog (erik@sics.se)
 * 
 *  Contributors:
 *    Raphael Collet (raph@info.ucl.ac.be)
 * 
 *  Copyright:
 *    Erik Klintskog, 2002
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

#ifndef __ENGINE_INTERFACE_HH
#define __ENGINE_INTERFACE_HH

#ifdef INTERFACE
#pragma interface
#endif

#include "base.hh"
#include "value.hh"
#include "dss_enums.hh"



/* Annotations */

class Annotation {
public:
  ProtocolName       pn : PN_NBITS;
  AccessArchitecture aa : AA_NBITS;
  RCalg              rc : RC_ALG_NBITS;

  // default constructor, make an empty annotation
  Annotation() :
    pn(PN_NO_PROTOCOL), aa(AA_STATIONARY_MANAGER), rc(RC_ALG_NONE) {}

  // constructor for a specific annotation
  Annotation(ProtocolName _pn, AccessArchitecture _aa, RCalg _rc) :
    pn(_pn), aa(_aa), rc(_rc) {}

  // return whether the protocol can be used for the given type of entity
  bool hasMutableProtocol() const;
  bool hasImmutableProtocol() const;
  bool hasTransientProtocol() const;

  // check whether the annotation is complete (all parameters defined)
  bool isComplete() const { return pn && aa && rc; }

  // incrementally adjoin the given annotation to this one; return
  // true if successful (with annotation updated), false otherwise
  // (with annotation unchanged).
  bool adjoin(const Annotation&);

  // parse an Oz term representing an annotation; upon success, the
  // attributes are set and the method returns PROCEED
  OZ_Return parseTerm(TaggedRef);

  // convert annotation to an Oz term
  TaggedRef toTerm();
};



/* DP initialization */

void initDP();

#endif



