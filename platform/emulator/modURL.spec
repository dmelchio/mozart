###
### Authors:
###   Denys Duchier <duchier@ps.uni-sb.de>
###   Christian Schulte <schulte@ps.uni-sb.de>
###
### Copyright:
###   Denys Duchier, 1998
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

%builtins_all =
(
    #* URL

    'localize'	=> { in  => ['+virtualString'],
			     out => ['+record'],
			     BI  => BIurl_localize},

    'open'		=> { in  => ['+virtualString'],
			     out => ['+int'],
			     BI  => BIurl_open},

    'load'		=> { in  => ['+virtualString'],
			     out => ['value'],
			     BI  => BIurl_load},

 );
