%%%
%%% Authors:
%%%   Gert Smolka <smolka@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Gert Smolka, 1998
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

declare
fun {BinPacking Order}
   ComponentTypes = [glass plastic steel wood copper]
   MaxBinCapacity = 4
   proc {IsBin Bin}
      BinTypes   = [0 1 2]
      [Blue Red Green] = BinTypes
      Capacity   = {FD.int [1 3 4]}   % capacity of Bin
      Type       = {FD.int BinTypes}  % type of Bin
      Components
      [Glass Plastic Steel Wood Copper] = Components
   in
      Bin = b(type:Type    glass:Glass  plastic:Plastic
	      steel:Steel  wood:Wood    copper:Copper)
      Components ::: 0#MaxBinCapacity
      {FD.sum Components '=<:' Capacity}
      {FD.impl Wood>:0  Plastic>:0 1}    % wood requires plastic
      {FD.impl Glass>:0  Copper=:0 1}    % glass excludes copper
      {FD.impl Copper>:0  Plastic=:0 1}  % copper excludes plastic
      thread
	 case Type
	 of !Red then Capacity=3  Plastic=0  Steel=0  Wood=<:1
	 [] !Blue then Capacity=1  Plastic=0  Wood=0
	 [] !Green then Capacity=4  Glass = 0  Steel=0  Wood=<:2
	 end
      end
   end
   proc {IsPackList Xs}
      thread
	 {ForAll Xs IsBin}
	 {ForAllTail Xs  % impose order
	  proc {$ Ys}
	     case Ys of A|B|_ then
		A.type =<: B.type
		thread
		   if A.type == B.type then A.glass >=: B.glass end
		end
	     else skip end
	  end}
      end
   end
   proc {Match PackList Order}
      thread
	 {ForAll ComponentTypes
	  proc {$ C}
	     {FD.sum {Map PackList fun {$ B} B.C end} '=:' Order.C}
	  end}
      end
   end
   proc {Distribute PackList}
      NbComps = {Record.foldR Order Number.'+' 0}
      Div     = NbComps div MaxBinCapacity
      Mod     = NbComps mod MaxBinCapacity
      Min     = if Mod==0 then Div else Div+1 end
      NbBins  = {FD.int Min#NbComps}
      Types
      Capacities
   in
      {FD.distribute naive [NbBins]}
      PackList   = {MakeList NbBins} % blocks until NbBins is determined
      Types      = {Map PackList fun {$ B} B.type end}
      Capacities = {FoldR PackList
		    fun {$ Bin Cs}
		       {FoldR ComponentTypes fun {$ T Ds} Bin.T|Ds end Cs}
		    end
		    nil}
      {FD.distribute naive Types}
      {FD.distribute ff Capacities}
  end
in
   proc {$ PackList}
      {IsPackList PackList}  {Match PackList Order}  {Distribute PackList}
   end
end

{ExploreOne {BinPacking order(glass:2 plastic:4 steel:3 wood:6 copper:4)}}

/*
{SearchOne {BinPacking order(glass:2 plastic:4 steel:3 wood:6 copper:4)} _}
*/