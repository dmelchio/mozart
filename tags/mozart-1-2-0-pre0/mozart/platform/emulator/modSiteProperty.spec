###
### Authors:
###   Denys Duchier <duchier@ps.uni-sb.de>
###
### Copyright:
###   Denys Duchier, 2000
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
     'get' => { in  => ['+feature'],
		out => ['value'],
		BI  => BIsitePropertyGet},

     'put' => { in  => ['+feature','value'],
		out => [],
		BI  => BIsitePropertyPut},
     );

1;;
