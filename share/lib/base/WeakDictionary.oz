%%%
%%% Authors:
%%%   Denys Duchier <duchier@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Denys Duchier, 1999
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    http://www.mozart-oz.org
%%%
%%% See the file "LICENSE" or
%%%    http://www.mozart-oz.org/LICENSE.html
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

WeakDictionary = weakDictionary(
		    new		: Boot_WeakDictionary.new
		    is		: Boot_WeakDictionary.is
		    put		: Boot_WeakDictionary.put
		    get		: Boot_WeakDictionary.get
		    condGet	: Boot_WeakDictionary.condGet
		    close	: Boot_WeakDictionary.close
		    )
