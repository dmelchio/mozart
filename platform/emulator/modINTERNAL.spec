###
### Authors:
###   Christian Schulte <schulte@ps.uni-sb.de>
###
### Copyright:
###   Christian Schulte, 1998
###
### Last change:
###   $Date$ by $Author$
###   $Revision$
###
### This file is part of Mozart, an implementation 
### of Oz 3:
###    http://www.mozart-oz.org
###
### See the file "LICENSE" or
###    http://www.mozart-oz.org/LICENSE.html
### for information on usage and redistribution 
### of this file, and for a DISCLAIMER OF ALL 
### WARRANTIES.
###

# -*-perl-*-

%builtins_all =
    (
     'controlVarHandler' => { in  => ['value'],
			      out => [],
			      BI  => BIcontrolVarHandler},

     'atRedo'            => { in  => ['value', 'value'],
			      out => [],
			      BI  => BIatRedo},

     'fail'		 => { in  => [],
			      out => [],
			      BI  => BIfail},

     'skip'		 => { in  => [],
			      out => [],
			      BI  => BIskip},

     'UNKNOWN'		 => { in  => [],
			      out => [],
			      BI  => BIfail},

     'load'		 => { in  => ['value', 'value'],
			      out => [],
			      BI  => BIload},

     'native'		 => { in  => ['value', 'value'],
			      out => ['value'],
			      BI  => BIObtainNative},

     'propagate'	 => { in  => [],
			      out => [],
			      BI  => BI_prop_lpq},

     'bindFuture'	 => { in  => ['value', 'value'],
			      out => [],
			      BI  => BIbindFuture},

     'waitStatus'	 => { in  => ['value', 'value'],
			      out => ['value'],
			      BI  => BIwaitStatus},

     'varToFuture'	 => { in  => ['value', 'value'],
			      out => [],
			      BI  => BIvarToFuture},

     );

1;;