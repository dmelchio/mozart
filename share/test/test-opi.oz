%%%
%%% Authors:
%%%   Christian Schulte (schulte@dfki.de)
%%%
%%% Copyright:
%%%   Christian Schulte, 1998
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    $MOZARTURL$
%%%
%%% See the file "LICENSE" or
%%%    $LICENSEURL$
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%


declare
{OS.chDir {OS.getEnv 'HOME'}#'/Oz/test'}

T={{Load './te.ozc'}
   c('SP': \insert SP.env
     'CP': \insert CP.env
     'OP': \insert OP.env
     'AP': \insert AP.env
    )}

{T argv(verbose:  true
	usage:    false
	help:     false
	keys:     "all"
	ignores:  "none"
	tests:    "all"
	do:       true
	time:     ""
	gc:       0
        threads:  1) _}

{System.set messages(idle:true)}
