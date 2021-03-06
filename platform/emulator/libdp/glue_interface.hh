/*
 *  Authors:
 *    Zacharias El Banna, 2002
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Zacharias El Banna, 2002
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
#ifndef __GLUE_INTERFACE_HH
#define __GLUE_INTERFACE_HH

//#ifdef INTERFACE  
//#pragma interface
//#endif

#include "dss_object.hh"

class MAP: public Mediation_Object{
public:
  MAP();

  virtual PstInContainerInterface* createPstInContainer();
  virtual void GL_error(const char* const format, ...);
  virtual void GL_warning(const char* const format, ...);
};

class ComService: public ComServiceInterface{
public:
  MsgnLayer* a_msgnLayer; 
  
  ComService();
  ~ComService() {}

  // The CsSite Object
  virtual CsSiteInterface* unmarshalCsSite(DSite*, DssReadBuffer* const buf); 
  virtual CsSiteInterface *connectSelfReps(MsgnLayer*, DSite*); 
  
  // Mark all DSites used by the CSC. 
  virtual void m_gcSweep(); 
};



// defined in engine_interface together with inits
extern DSS_Object* dss;
extern ComService* glue_com_connection; 

#endif //GLUE_INTERFACE
