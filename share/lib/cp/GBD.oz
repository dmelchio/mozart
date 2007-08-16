/*
 *  Main authors:
 *     Gustavo Gutierrez <ggutierrez@cic.puj.edu.co>
 *     Alberto Delgado <adelgado@cic.puj.edu.co>
 *
 *  Contributing authors:
 *     Alejandro Arbelaez <aarbelaez@puj.edu.co>
 *
 *  Copyright:
 *     Gustavo Gutierrez, 2006
 *     Alberto Delgado, 2006
 *
 *  Last modified:
 *     $Date$
 *     $Revision$
 *
 *  This file is part of GeOz, a module for integrating gecode 
 *  constraint system to Mozart: 
 *     http://home.gna.org/geoz
 *
 *  See the file "LICENSE" for information on usage and
 *  redistribution of this file, and for a
 *     DISCLAIMER OF ALL WARRANTIES.
 *
 */

functor
import
   GBD(bool: BoolVar
       isVar: IsVar
       'reflect.size':       GetSize
       'reflect.min':        GetMin
       'reflect.max':        GetMax
       and: 	     Bool_and
       andA: 	     Bool_and_arr       
       'or': 	     Bool_or
       orA: 	     Bool_or_arr              
       xor:          Bool_xor
       'not':        Bool_not
       imp:          Bool_imp
       eqv:          Bool_eqv
       rel:          Rel
       linear:       Linear                     
      )
   at 'x-oz://boot/geoz-bool'

   System
   Space
export
   %%Declaration
   bool:   BoolVar
   dom:    GBDDom

      
   BoolVarList

   is:    IsVar
   
   %%Access
   Reflect

   %% Consistency levels for propagators
   cl:    Cl

   rt:    Rt
   
   %% Miscellaneous
   decl:            Decl

   %% propagators
   
   conj       :Bool_and
   conjA      :Bool_and_arr
   disj       :Bool_or
   disjA      :Bool_or_arr
   xor        :Bool_xor
   notB       :Bool_not
   imp        :Bool_imp
   equi       :Bool_eqv
   rel        :Rel
   linear     :Linear
   %Watch
   
   %%Branching
   %ValSel VarSel
   %%%distribute:     IntVarDistribute
   Distribute
   %intBranch:Distribute   

   %%Space
   
define

   Reflect = reflect(size: GetSize
		     min: GetMin
		     max: GetMax
		     dom:_		     
		    )		     
   
   %%This record must reflect the ConLevel
   Cl = '#'(
	   val: 0   %Value consistency
	   bnd: 1   %Bounds consistency
	   dom: 2   %Domain consistency
	   def: 3   %The default consistency for a constraint
	   )

   %%This record must reflect the IntRelType in gecode/int.hh
   Rt = '#'('=:':0     %Equality
	    '\\=:':1    %Disequality
	    '=<:':2    %Less or equal
	    '<:':3     %Less
	    '>=:':4    %Greater or equal
	    '>:':5     %Greater
	   )

   
   %%misc defines: VectorToTuple, VectorToType, VectorToList ....
   \insert misc

   fun{Decl}
      {BoolVar 0#1}
   end
   
   proc{CreateDomainTuple N Dom Root}
      if N > 0 then Root.N = {BoolVar Dom} {CreateDomainTuple N-1 Dom Root} end
   end

   proc{CreateDomainRecord As Dom Root}
      case As of nil then skip
      [] A | Ar then Root.A = {BoolVar Dom} {CreateDomainRecord Ar Dom Root}
      end
   end
   
   %%Create GeBoolVar with domain Dom per each element of the list
   proc{CreateDomainList Dom Root}
      case Root of
	 X|Xs then
	 X = {BoolVar Dom}
	 {CreateDomainList Dom Xs}
      [] nil then skip
      end
   end   


   proc{GBDDom Dom Root}
      case {VectorToType Root}
      of list then   {CreateDomainList Dom Root}
      [] tuple then  {CreateDomainTuple {Width Root} Dom Root}
      [] record then {CreateDomainRecord {Arity Root} Dom Root}
      end
   end
   
   %%Declares a list of GBD vars
   fun{BoolVarList N Desc}
      Lst = {List.make N}
   in
      {List.map Lst fun{$ X} X = {BoolVar Desc} end}
   end

   proc{GTuple T N Desc Seq}
      Seq = {Tuple.make T N}
      for I in 1..N do
	 Seq.I = {BoolVar Desc}
      end
   end

   local
      \insert GeIntVarDist
   in
      Distribute = IntVarDistribute
   end

end