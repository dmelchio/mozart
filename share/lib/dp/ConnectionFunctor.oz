%\define DBG
functor
export
   Connectionfunctor
define
   functor Connectionfunctor
   export
      Connect
   import
      ConnectionWrapper
\ifdef DBG
      System(show:Show showInfo showError)
      Property
\endif
   define
      RetryTimes=10
      RetryWaitTime=100
      
      proc {Parse P ?Address ?IPPort}
      % Get Address and Port to connect to out of P
	 ip_addr(addr:Address port:IPPort)=P
      end
      
      proc{Connect P}       
\ifdef DBG
	 {Show connect(P)}
	 T0 = {Property.get 'time.total'}
\endif
	 FD
	 Address IPPort
	 Done
	 proc{GetNegChannel Time}
	    if Time>0 then
	       try
		  FD={ConnectionWrapper.socket 'PF_INET' 'SOCK_STREAM' "tcp"}
		  if FD==~1 then raise no_fd end end
		  {ConnectionWrapper.connect FD Address IPPort}
	       catch X then
		  case X of system(os(_ _ 111 _) ...) then
		     Done=unit
		     {ConnectionWrapper.close FD}
\ifdef DBG
		     {System.show discovered_perm(FD Address IPPort)}
\endif
		     {ConnectionWrapper.connFailed perm}    
		  else
\ifdef DBG
		     {System.show caught(X)}
		     {System.show retrying}
\endif
       	             % Delay for a while and retry
		     {Delay RetryWaitTime}
		     {GetNegChannel Time-1}
		  end
	       end
	    else
	       Done=unit
	       {ConnectionWrapper.connFailed temp}    
	    end
	 end
      in
	 {Parse P ?Address ?IPPort}
	 {GetNegChannel RetryTimes}
	       
	 if {Not {IsDet Done}} then
         % Try tcp
	    Grant = {ConnectionWrapper.getConnGrant tcp true}
	    ReadS
	 in
	    case Grant of grant(...) then
	       {ConnectionWrapper.writeSelect FD}
	       _={ConnectionWrapper.write FD "tcp"}
	       {ConnectionWrapper.readSelect FD}
	       _ = {ConnectionWrapper.read FD 2 ReadS nil}
	       case ReadS of "ok" then 
		  {ConnectionWrapper.handover Grant settings(fd:FD)}
		  Done=unit
	       else
		  {ConnectionWrapper.freeConnGrant Grant}
	       end
	    else
	       skip
	    end
	 end

	 /*
	 if {Not {IsDet Done}} then
	 % Go ahead and try some other transport media
	 end
	 */
      
      % Either we have a connection or
      % we figured out that the remote site was perm or
      % we are out of pConnectionWrappersible transport medias
	 if {Not {IsDet Done}} then
	    {ConnectionWrapper.write FD "give_up"}
	    {ConnectionWrapper.close FD}
	    {ConnectionWrapper.connFailed noTransport}
\ifdef DBG	 
	 else
	    {System.show connectDone({Property.get 'time.total'} -T0)}
\endif
	 end
      end
   end
end