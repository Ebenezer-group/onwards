  cmwRequest (ReceiveBuffer<R>& buf,::std::vector<cmwAccount> const& accts):
       accountNbr(buf),path(buf){
    bool found=false;
    for(auto const& acct:accts){
      if(acct.number==accountNbr){found=true;break;}
    }
    if(!found)throw failure("Invalid account number ")<<accountNbr();

