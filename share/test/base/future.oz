%%%
%%% Authors:
%%%   Michael Mehl (mehl@dfki.de)
%%%
%%% Copyright:
%%%   Michael Mehl, 1998
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

functor

import
   Space

export
   Return

define
   fun {RetA} a end

   Return =
   future([
	   adjoinAt(proc {$}
		       Ts=[a(a:b)#{ByNeed RetA}#a#b
			   a(a:b)#a#{ByNeed RetA}#b
			   a(b:a)#a#b#{ByNeed RetA}]
		    in
		       {ForAll Ts proc {$ R#A1#A2#A3}
				     true = {AdjoinAt A1 A2 A3} == R
				  end}
		    end
		    keys:[future byNeed adjoin adjoinAt])
	 
	   adjoinList(proc {$}
			 Ts=[
			     a#{ByNeed RetA}#nil
			    ]
		      in
			 {ForAll Ts proc {$ R#A1#A2}
				       true = {AdjoinList A1 A2} == R
				    end}
		      end
		      keys:[future byNeed adjoin adjoinList])

	   arity(proc {$}
		    Ts=[
			nil#{ByNeed RetA}
		       ]
		 in
		    {ForAll Ts proc {$ R#A}
				  true = {Arity A} == R
			       end}
		 end
		 keys:[future byNeed arity])
	   cycle(proc {$}
		    A = {ByNeed fun {$} A end}
		 in
		    skip
		 end
		 keys:[future byNeed cycle bug])		 
	   space(proc {$}
		    X Y S Go1 Go2
		 in
		    X=!!Y
		    S={Space.new proc {$ R} X=1 end}
		    thread
		       Go1 = unit
		       _ = {Space.merge S}
		       Go2 = unit
		    end
		    {Wait Go1} % first start thread to merge space
		    Y=1        % then bind future
		    {Wait Go2} % then wait until merge is successful
		 end
		 keys: [future space])
	  ])
end
