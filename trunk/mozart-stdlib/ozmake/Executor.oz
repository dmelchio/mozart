functor
export
   'class' : Executor
import
   OS Property System(showError:Print) Open(file:OpenFile)
   Path  at 'Path.ozf'
   Utils at 'Utils.ozf'
   Shell at 'Shell.ozf'
define
   %% the Executor mediates the execution of commands and provides
   %% optionall tracing and optional dry runs

   class Executor

      attr
	 Indent : ''
	 MKFILE : unit
	 RMFILE : unit

      meth exec_init
	 MKFILE<-{NewDictionary}
	 RMFILE<-{NewDictionary}
      end

      meth incr Indent<-'  '#@Indent end
      meth decr Indent<-@Indent.2 end
      meth trace(Msg)
	 if {self get_quiet($)} then skip
	 elseif {self get_verbose($)} then {Print @Indent#Msg} end
      end
      %% xtrace gives feedback even in non verbose mode
      %% but it can be shut up with --quiet
      %% it is used only for real commands and let's the user
      %% see that something is happening
      meth xtrace(Msg)
	 if {self get_quiet($)} then skip
	 elseif {self get_verbose($)} then {Print @Indent#Msg}
	 else {Print Msg} end
      end
      meth print(Msg)
	 if {self get_quiet($)} then skip
	 else {Print Msg} end
      end

      %% turn a target into an actual filename

      meth make_dst(F $)	% output of a tool
	 {Path.resolve {self get_builddir($)} {Path.maybeAddPlatform F}}
      end

      meth make_src(F $)	% input of a tool
	 FF={Path.maybeAddPlatform F}
	 DST={Path.resolve {self get_builddir($)} FF}%look in build dir
      in
	 if {Path.exists DST} orelse
	    ({self get_justprint($)} andthen Executor,simulated_exists(DST $))
	 then DST
	 else
	    SRC={Path.resolve {self get_srcdir($)} FF}%else in source dir
	 in
	    if {Path.exists SRC} orelse
	       ({self get_justprint($)} andthen Executor,simulated_exists(SRC $))
	    then SRC
	    else raise ozmake(filenotfound:{Path.toAtom FF}) end end
	 end
      end

      %% invoking building tools

      meth exec_rule(Target Rule)
	 DST = Executor,make_dst(Target $)
	 SRC = if Rule.tool==unit then unit
	       else Executor,make_src(Rule.file $) end
      in
	 case Rule.tool
	 of ozc then Executor,OZC(DST SRC Rule.options)
	 [] ozl then Executor,OZL(DST SRC Rule.options)
	 [] cc  then Executor,CC( DST SRC Rule.options)
	 [] ld  then Executor,LD( DST SRC Rule.options)
	 [] unit then Executor,NULL(Target)
	 else raise ozmake(build:unknowntool(Rule.tool)) end
	 end
      end

      %% Null tool: just check existence

      meth NULL(Target)
	 %% existence checked by make_src
	 if {self get_justprint($)} then
	    %% in a dry run just output a message but no error
	    try {self make_src(Target _)}
	    catch _ then {self xtrace('no rule to build '#Target)} end
	 else
	    {self make_src(Target _)}
	 end
      end

      %% Oz compiler

      meth OZC(DST SRC Options)
	 Executor,exec_mkdir({Path.dirname DST})
	 L1 = [SRC '-o' DST]
	 L2 = if {self get_optlevel($)}==debug then '-g'|L1 else L1 end
	 L3 = if {Member executable Options} then '-x'|L2 else '-c'|L2 end
      in
	 {self xtrace({Utils.listToVS ozc|L3})}
	 if {self get_justprint($)} then
	    %% record time of simulated build
	    Executor,SimulatedTouch(DST)
	 else
	    try {Shell.execute
		 {self get_oz_engine($)}|{self get_oz_ozc($)}|L3}
	    catch shell(CMD) then
	       raise ozmake(build:shell(CMD)) end
	    end
	 end
      end

      %% Oz linker

      meth OZL(DST SRC Options)
	 Executor,exec_mkdir({Path.dirname DST})
	 L1 = [SRC '-o' DST]
	 L2 = if {self get_optlevel($)}==debug then '-g'|L1 else L1 end
      in
	 {self xtrace({Utils.listToVS ozl|L2})}
	 if {self get_justprint($)} then
	    %% record time of simulated build
	    Executor,SimulatedTouch(DST)
	 else
	    try {Shell.execute
		 {self get_oz_engine($)}|{self get_oz_ozl($)}|L2}
	    catch shell(CMD) then
	       raise ozmake(build:shell(CMD)) end
	    end
	 end
      end

      %% C++ compiler

      meth CC(DST SRC Options)
	 Executor,exec_mkdir({Path.dirname DST})
	 L1 = [SRC '-o' DST]
	 L2 = case {self get_optlevel($)}
	      of debug then '-g'|L1
	      [] optimize then
		 if {self get_gnu($)} then '-O3' else '-O' end|L1
	      else L1 end
	 L3 = 'c++'|'-c'|L2
      in
	 {self xtrace({Utils.listToVS oztool|L3})}
	 if {self get_justprint($)} then
	    %% record time of simulated build
	    Executor,SimulatedTouch(DST)
	 else
	    try {Shell.execute
		 {self get_oz_oztool($)}|L3}
	    catch shell(CMD) then
	       raise ozmake(build:shell(CMD)) end
	    end
	 end
      end

      meth LD(DST SRC Options)
	 Executor,exec_mkdir({Path.dirname DST})
	 L1 = [ld SRC '-o' DST]
      in
	 {self xtrace({Utils.listToVS oztool|L1})}
	 if {self get_justprint($)} then
	    %% record time of simulated build
	    Executor,SimulatedTouch(DST)
	 else
	    try {Shell.execute
		 {self get_oz_oztool($)}|L1}
	    catch shell(CMD) then
	       raise ozmake(build:shell(CMD)) end
	    end
	 end
      end

      meth exec_simulated_touch(F) Executor,SimulatedTouch(F) end

      meth SimulatedTouch(DST) Key={Path.toAtom DST} in
	 {Dictionary.remove @RMFILE Key}
	 @MKFILE.Key := {OS.time}
      end

      meth SimulatedDelete(DST) Key={Path.toAtom DST} in
	 {Dictionary.remove @MKFILE Key}
	 @RMFILE.Key := {OS.time}
      end

      meth simulated_exists(F $) Key={Path.toAtom F} in
	 {HasFeature @MKFILE Key}
      end

      meth simulated_deleted(F $) Key={Path.toAtom F} in
	 {HasFeature @RMFILE Key}
      end

      meth get_simulated_mtime(F $)
	 @MKFILE.{Path.toAtom F}
      end

      %% creating a directory hierarchy
      
      meth exec_mkdir(D)
	 U={Path.expandURL {Path.toNonBaseURL D}}
      in
	 case {CondSelect U path nil}
	 of nil   then skip
	 [] [nil] then skip
	 else
	    Executor,exec_mkdir({Path.dirnameURL U})
	    Executor,MakeDir(U)
	 end
      end

      meth MakeDir(U)
	 if {self get_justprint($)} andthen Executor,simulated_exists(U $) then skip
	 elsecase {Path.safeStat U}.type
	 of 'dir' andthen {Not {self get_justprint($)} andthen Executor,simulated_deleted(U $)} then skip
	 else
	    {self xtrace('mkdir '#{Path.toString U})}
	    if {self get_justprint($)} then
	       %% record time of simulated creation
	       Executor,SimulatedTouch(U)
	    else
	       try {Path.makeDir U}
	       catch _ then raise ozmake(mkdir:{Path.toAtom U}) end end
	    end
	 end
      end

      %% removing a directory

      meth exec_rmdir(D)
	 if {self get_justprint($)} andthen Executor,simulated_deleted(D $) then skip
	 else
	    {self xtrace('rm -R '#{Path.toString D})}
	    if {self get_justprint($)} then
	       Executor,SimulatedDelete(D)
	    else
	       try {Path.removeRec D}
	       catch error(path(remove(S)) ...) then
		  raise ozmake(rmdir:S) end
	       end
	    end
	 end
      end

      %% removing a file

      meth exec_rm(F)
	 if {self get_justprint($)} andthen Executor,simulated_deleted(F $) then skip
	 else
	    {self xtrace('rm '#{Path.toString F})}
	    if {self get_justprint($)} then
	       Executor,SimulatedDelete(F)
	    else
	       try {Path.remove F}
	       catch error(path(remove(S)) ...) then
		  raise ozmake(rm:S) end
	       end
	    end
	 end
      end

      %% copying a file

      meth exec_cp(From To)
	 F={Path.expand From}
	 T={Path.expand To}
      in
	 {self xtrace('cp '#F#' '#T)}
	 if {self get_justprint($)} then
	    Executor,SimulatedTouch(T)
	 else
	    Data={Utils.slurpFile F}
	    if {Path.exists T} then {Path.remove T} end
	    File={New OpenFile init(name:T flags:[write create truncate])}
	 in
	    {File write(vs:Data)}
	    {File close}
	 end
      end

      %% installing files

      meth exec_install_file(From To)
	 Executor,exec_mkdir({Path.dirname To})
	 Executor,exec_cp(From To)
      end

      meth exec_install_exec(From To)
	 Executor,exec_install_file(From To)
	 Executor,exec_mkexec(To)
      end

      meth exec_mkexec(To)
	 {self xtrace('chmod -x '#To)}
	 if {self get_justprint($)} then skip else
	    try {Shell.execute ['chmod' '-x' To]}
	    catch _ then
	       if {Property.get 'platform.os'}\=win32 then
		  raise ozmake(mkexec:To) end
	       end
	    end
	 end
      end

      meth exec_check_for_gnu($)
	 {self trace('checking for GNU compiler')}
	 {Utils.haveGNUC {self get_oz_oztool($)}}
      end

      meth exec_write_to_file(Data File)
	 Executor,exec_mkdir({Path.dirname File})
	 {self xtrace('writing '#File)}
	 if {self get_justprint($)} then
	    Executor,SimulatedTouch(File)
	 else
	    if {Path.exists File} then {Path.remove File} end
	    Out={New OpenFile init(name:File flags:[write create truncate])}
	 in
	    {Out write(vs:Data)}
	    {Out close}
	 end 
      end

   end

end
