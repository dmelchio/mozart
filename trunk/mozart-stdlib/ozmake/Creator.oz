functor
export
   'class' : Creator
import
   Pickle
   Utils at 'Utils.ozf'
   Path  at 'Path.ozf'
define
   class Creator

      meth publish
	 %% publish is like create but computes the filename from the
	 %% package's mogul id and places it in the user's publishing
	 %% area
	 {self makefile_read}
	 MOG = {self get_mogul($)}
	 FIL = {Utils.mogulToPackagename MOG}
	 PKG = {Path.resolve {self get_publishdir($)} FIL}
      in
	 {self set_package(PKG)}
	 Creator,create
      end

      meth create
	 {self makefile_read}
	 %% the package file
	 PKG = {self get_package($)}
	 %% it should be an ordinary file, i.e. not have a scheme
	 local S = {CondSelect {Path.toURL PKG} 'scheme' unit} in
	    if S\=unit then raise ozmake(create:hasscheme(PKG S)) end end
	 end
	 %% compute the files to be included in the package
	 Needed = Creator,GetNeeded($)
	 %% make a record with all the package stuff in it
	 REC = packed(
		  info : {self makefile_to_record($)}
		  data : for F in Needed collect:Collect do
			    FKey = {Path.toAtom {Path.maybeAddPlatform F}}
			    FNam = {self make_src(F $)}
			 in
			    {self trace('grabbing '#FKey#' from '#FNam)}
			    {Collect FKey#
			     if {self get_justprint($)} then unit else
				{Utils.slurpFile FNam}
			     end}
			 end)
      in
	 {self exec_mkdir({Path.dirname PKG})}
	 {self xtrace('writing '#PKG)}
	 if {self get_justprint($)} then skip else
	    try {Pickle.save REC PKG}
	    catch _ then raise ozmake(create:write(PKG)) end end
	 end
      end

      meth GetNeeded($)
	 {self makefile_read}
	 %% we need to get everything needed to build the targets
	 %% of the makefile except for those in src which must be
	 %% prebuilt
	 Needed = {NewDictionary}
	 Stack  = {Utils.newStackFromList {self get_install_targets($)}}
      in
	 try
	    for do F={Stack.pop} in
	       if {Not {HasFeature Needed F}} then
		  if {self target_is_src(F $)} then
		     %% it must be built and packed, but not the sources
		     %% needed to build it
		     Needed.F := true
		  else
		     R = {self get_rule(F $)}
		  in
		     if R.tool==unit then
			%% there is no rule to build it: it must be distributed
			Needed.F := true
		     else
			%% else look at all the sources needed to build it
			for D in {self get_depends(F $)} do {Stack.push D} end
		     end
		  end
	       end
	    end
	 catch empty then skip end
	 %% make sure that they are all available
	 %% we do not simulate --fullbuild because that would prevent
	 %% repackaging prebuilt files for which the sources are not
	 %% available.
	 for F in {Dictionary.keys Needed} do {self build_target(F)} end
	 %% also get what is needed in subdirectories
	 if {self get_local($)} then skip else
	    for
	       D in {self get_subdirs($)}
	       M in {self get_submans($)}
	    do
	       {self trace('entering '#D)}
	       {self incr}
	       try
		  for T in {M GetNeeded($)} do
		     Needed.{Path.resolveAtom D T} := true
		  end
	       finally {self decr} end
	       {self trace('leaving '#D)}
	    end
	 end
	 {Dictionary.keys Needed}
      end
   end
end