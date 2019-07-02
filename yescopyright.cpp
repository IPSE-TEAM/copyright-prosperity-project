#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <cmath>
#include <eosio/system.hpp> 
#include <eosio/time.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;

class [[eosio::contract("yescopyright")]] yescopyright : public eosio::contract {

public:
  using contract::contract;
  
  yescopyright(name receiver, name code,  datastream<const char*> ds): contract(receiver, code, ds),_config(_self,_self.value) {}
  
  void split(const std::string& s,char c,std::vector<std::string>& v) {
      std::string::size_type i = 0;
      std::string::size_type j = s.find(c);
      while (j != std::string::npos){
        v.push_back(s.substr(i,j-i));
        i = ++j;
        j = s.find(c,j);
        if (j == std::string::npos)
          v.push_back(s.substr(i,s.length()));
      }
  };
  
  float stof(std::string s){   
    if (s == "") return 0.5;
    std::size_t i = s.find(".");
    int digits = s.length() - i - 1;
    s.erase(i, 1); 
    return atoi(s.c_str()) / pow(10, digits);
  };
  
  [[eosio::action]]
  void transfer(name from,name to,asset quantity,std::string memo){
    check(from != to,"cannot transfer to self");
    require_auth(from);
    check(is_account(to),"to account does not exists");
    // auto sym = quantity.symbol.name;
    // name name_ = string_to_name("poseidonplan");
    symbol symbol_ = symbol("POST", 4);
    
    require_recipient(from);
    require_recipient(to);
    check(quantity.is_valid(),"invalid quantity");
    check(quantity.amount > 0,"must transfer positive quantity");
    // check(quantity.symbol.name == name_,"the token is not valid");
    check(quantity.symbol == symbol_,"symbol precision mismatch");
    check(memo.size() <= 256,"memo hash more than 256 bytes");
    
    // if someone transfer POSC to yescopyright,then process 
    // if the flag in the memo is createart,then create the copyrightart
    // if the flag in the memo is invest,then process the invest action
    
    memo.erase(memo.begin(), find_if(memo.begin(), memo.end(), [](int ch) {
        return !isspace(ch);
    }));
    memo.erase(find_if(memo.rbegin(), memo.rend(), [](int ch) {
        return !isspace(ch);
    }).base(), memo.end());

    std::vector<std::string> v;
    split(memo,';',v);
    if(v.size()>1){
      if (v[0] == "createart"){
        createart(v,quantity,from);
      }else if(v[0] == "invest"){
        invest(v,quantity,from);
      }
    }
  };
  
  void invest(std::vector<std::string> v,asset quantity,name from){
    std::string::size_type i = 0;
    // invest;silversilver;123;30000.0000 POST
    if(v.size()>3){
      name address = name(v[1]);
      uint64_t id = std::strtoull(v[2].c_str(),NULL,10);
      art_index copyrightart(get_first_receiver(),address.value);
      auto itr = copyrightart.find(id);
      if(itr != copyrightart.end()){
        std::string::size_type invest_size = v[3].size();
        uint64_t invest_quantity = std::strtoull(v[3].substr(0,invest_size-5).c_str(),NULL,10);
        invest_quantity *= std::pow(10,4);
        asset invest = asset(invest_quantity,symbol("POST",4));
        // check the deadline 
        uint64_t deadline = itr -> deadline;
        uint32_t now = current_time_point().sec_since_epoch();
        if(deadline > now){
          // check if over the totalinvest
          asset totalinvest = itr -> totalinvest;
          asset invested = itr -> invest;
          if(invested.amount >= totalinvest.amount){
            print("the invest quantity over the totalinvest,you could invest another copyright");
          }else{
            // check the status of the copyrightart,must be raisefunds status
            std::string status = itr -> status;
            if(status == "raisefunds"){
              // check the invest amount over the miniinvest amount 
              asset miniinvest = itr -> miniinvest;
              if(quantity.amount >= miniinvest.amount){
                // check the transfer amount equal the invest amount in the params
                auto invest_amount = invested.amount + quantity.amount;
                if(quantity.amount == invest.amount){
                  if(invest_amount >= totalinvest.amount){
                    copyrightart.modify(itr, get_first_receiver(), [&]( auto& row ) {
                      row.invest = totalinvest;
                    });
                    auto need_invest_amount = totalinvest.amount - invested.amount;
                    asset need_invest = asset(need_invest_amount,symbol("POST",4));
                    investtoken(from,address,id,need_invest);
                    if(invest_amount>totalinvest.amount){
                      asset returninvest = asset(quantity.amount-need_invest_amount,symbol("POST",4));
                      // SEND_INLINE_ACTION(*this,transfer,{ {_self,"active"_n} },{ "yescopyright"_n, from, returninvest, "return the token over the totalinvest"});
                      action(
                        permission_level{_self,"active"_n} ,
                        "eosio.token"_n,"transfer"_n,
                        std::make_tuple(_self,from,returninvest,std::string("return the token over the totalinvest"))
                      ).send();
                    }
                    // then send the 50% token to author and change the status of the copyrightart
                    asset halfinvest = asset(totalinvest.amount*0.5,symbol("POST",4));
                    // SEND_INLINE_ACTION(*this,transfer,{ {_self,"active"_n} },{ "yescopyright"_n, address, halfinvest, "do you best" });
                    action(
                      permission_level{_self,"active"_n} ,
                      "eosio.token"_n,"transfer"_n,
                      std::make_tuple(_self,address,halfinvest,std::string("do you best,reward the investment"))
                    ).send();
                    copyrightart.modify(itr,get_first_receiver(),[&]( auto& row) {
                      row.status = "invested"; //表示众筹达成目标
                    });
                    print("invest success and return the over totalinvest amount token");
                  }else{
                    asset alreadyinvest = asset(invest_amount,symbol("POST",4));
                    copyrightart.modify(itr, get_first_receiver(), [&]( auto& row ) {
                      row.invest = alreadyinvest;
                    });
                    investtoken(from,address,id,invest);
                    print("invest success");
                  }
                }else{
                  print("the transfer amount not equal the invest amount in the params");
                }
              }else{
                print("the invest quantity not over the miniinvest amount");
              }
            }else{
              print("the status of copyrightart is not raisefunds");
            }
          }
        }else{
          if(itr->status == "investing"){
            copyrightart.modify(itr,get_first_receiver(), [&]( auto& row) {
              row.status = "overdeadline";
            });
          }
          print("over the deadline,you could not invest");
        }
      }else{
        print("not found the art could invest");
      }
    }else{
      print("the params for invest action is not right");
    }
  };
  
  void investtoken(name from,name address,uint64_t id,asset invest){
    invest_index investdetail(get_first_receiver(),address.value);
    auto itr = investdetail.find(id);
    if(itr == investdetail.end()){
      investdetail.emplace(get_first_receiver(), [&]( auto& row){
        row.id = id;
        row.address = address;
        row.investor = from;
        row.invest = invest;
        row.status = "investing";
      });
      print("emplace investing success");
    }else{
      asset alreadyinvest = itr -> invest;
      asset invested = asset((alreadyinvest.amount+invest.amount),symbol("POST",4));
      investdetail.modify(itr, get_first_receiver(), [&]( auto& row){
        row.invest = invested;
      });
      print("modify investing success");
    }
  };
  
  void createart(std::vector<std::string> v,asset quantity,name from){
    std::string::size_type i = 0;
    // createart;silversilver;123;XIEJIANHUAI;《Rain》;fiction;10000.0000 POST;100000.0000 POST;1562740977;1589092977;2.0000 POST;1.0000 POST;0.7;Good fiction
    if(v.size()>12 && v.size()<15){
      name address = name(v[1]);
      uint64_t id = std::strtoull(v[2].c_str(), NULL, 10);
      std::string author = v[3];
      std::string artname = v[4];
      std::string category = v[5];
      
      std::string::size_type miniinvest_size = v[6].size();
      uint64_t miniinvest_quantity = std::strtoull((v[6].substr(0,miniinvest_size-5)).c_str(),NULL,10);
      miniinvest_quantity *= std::pow(10,4);
      asset miniinvest = asset(miniinvest_quantity,symbol("POST",4));
      
      std::string::size_type totalinvest_size = v[7].size();
      uint64_t totalinvest_quantity = std::strtoull((v[7].substr(0,totalinvest_size-5)).c_str(),NULL,10);
      totalinvest_quantity *= std::pow(10,4);
      asset totalinvest = asset(totalinvest_quantity,symbol("POST",4));
      
      asset invest = asset(0,symbol("POST",4));
      asset opposeinvest = asset(0,symbol("POST",4));
      
      uint32_t deadline = std::strtoull(v[8].c_str(),NULL,10);
      uint32_t finishtime = std::strtoull(v[9].c_str(),NULL,10);
      
      std::string::size_type price_size = v[10].size();
      uint64_t price_quantity = std::strtoull((v[10].substr(0,price_size-5)).c_str(),NULL,10);
      price_quantity *= std::pow(10,4);
      asset price = asset(price_quantity,symbol("POST",4));
      
      std::string::size_type earlyprice_size = v[11].size();
      uint64_t earlyprice_quantity = std::strtoull((v[11].substr(0,earlyprice_size-5)).c_str(),NULL,10);
      earlyprice_quantity *= std::pow(10,4);
      asset earlyprice = asset(earlyprice_quantity,symbol("POST",4));
      float shareratio = stof(v[12]);
      uint32_t createtime = current_time_point().sec_since_epoch();
      std::string memo = "";
      if (v.size()>13){
        memo = v[13];
      }

      // find the id of the copyrightart is exist or not,if not exist,then insert the copyrightart
      art_index copyrightart(get_first_receiver(),address.value);
      auto copyrightart_iterator = copyrightart.find(id);
      if(copyrightart_iterator == copyrightart.end()){
        // check the transfer quantity is enough
        float createratio = _config.get().createratio;
        auto totalratio = totalinvest.amount * createratio;
        print(totalratio);
        print(" ");
        check(quantity.amount >= totalratio,"you should pledge more token");
        copyrightart.emplace(get_first_receiver(), [&]( auto& row){
          row.id = id;
          row.address = address;
          row.author = author;
          row.artname = artname;
          row.category = category;
          row.miniinvest = miniinvest;
          row.totalinvest = totalinvest;
          row.invest = invest;
          row.opposeinvest = opposeinvest;
          row.deadline = deadline;
          row.finishtime = finishtime;
          row.price = price;
          row.earlyprice = earlyprice;
          row.shareratio = shareratio;
          row.createtime = createtime;
          row.status = "raisefunds"; // 创建完筹款中
        });
        print("emplace copyrightart success");
      }else{
        print("the id is already exists");
        // then return the returnratio token back 
        float returnratio = 1.0 - _config.get().createratio;
        asset returntoken = asset(quantity.amount*returnratio,symbol("POST",4));
        action(
          permission_level{_self,"active"_n} ,
          "eosio.token"_n,"transfer"_n,
          std::make_tuple(_self,from,returntoken,std::string("return the token back after deduction of fee"))
        ).send();
      }
    }else{
      print("the params for creating art is not right");
    }
  };
  
  [[eosio::action]]
  void opposeart(uint64_t id,name address,name actor){
    require_auth(actor);
    art_index copyrightart(_self,address.value);
    auto itr = copyrightart.find(id);
    check(itr != copyrightart.end(),"copyrightart Record does not exist");
    invest_index investdetail(_self,address.value);
    auto invest_itr = investdetail.find(id);
    check(invest_itr != investdetail.end(),"invest Record does not exist");
    asset invest = invest_itr -> invest;
    asset opposeinvest_ = itr -> opposeinvest;
    asset totalinvest = itr -> totalinvest;
    check(invest_itr -> status != "opposed","you opposed already");
    asset newopposeinvest = asset(opposeinvest_.amount+invest.amount,symbol("POST",4));
    float opposeratio = _config.get().opposeratio;
    if(totalinvest.amount*opposeratio < (opposeinvest_.amount + invest.amount)){
      copyrightart.modify(itr,get_first_receiver(), [&]( auto& row ) {
        row.status = "extracted";
        row.opposeinvest = newopposeinvest;
      });
      // then return the token back 
      action(
        permission_level{_self,"active"_n} ,
        "eosio.token"_n,"transfer"_n,
        std::make_tuple(_self,actor,invest,std::string("return the token back after the copyrightart was opposed"))
      ).send();
    }else{
      copyrightart.modify(itr,get_first_receiver(), [&]( auto& row ) {
        row.opposeinvest = newopposeinvest;
      });
    }
    investdetail.modify(invest_itr,get_first_receiver(), [&]( auto& row ){
      row.status = "opposed";
    });
  };
  
  [[eosio::action]]
  void extracttoken(uint64_t id,name address,name receiver){
    art_index copyrightart(_self,address.value);
    auto itr = copyrightart.find(id);
    check(itr != copyrightart.end(),"copyrightart Record does not exist");
    invest_index investdetail(_self,address.value);
    auto invest_itr = investdetail.find(id);
    check(invest_itr != investdetail.end(),"invest Record does not exist");
    check(invest_itr -> status != "extracted","you extract token already");
    check(invest_itr -> status == "opposed","the status of copyrightart could not support extract token");
    name investor = invest_itr -> investor;
    require_auth(investor);
    asset invest = invest_itr -> invest;
    action(
      permission_level{_self,"active"_n} ,
      "eosio.token"_n,"transfer"_n,
      std::make_tuple(_self,receiver,invest,std::string("extract token success after the copyrightart was opposed"))
    ).send();
    investdetail.modify(invest_itr,get_first_receiver(), [&]( auto& row ){
      row.status = "extracted";
    });
    print("extracted token success");
  }
  
  [[eosio::action]]
  void completeart(uint64_t id,name address){
    art_index copyrightart(_self,address.value);
    auto itr = copyrightart.find(id);
    check(itr != copyrightart.end(),"copyrightart Record does not exist");
    // first check: the status must be  "invested"
    // second check: the time must be before the finishtime
    
  }
  
  [[eosio::action]]
  void deleteart(uint64_t id,name address){
    require_auth(get_self());
    art_index copyrightart(get_self(),address.value);
    auto itr = copyrightart.find(id);
    check(itr != copyrightart.end(),"Record does not exist");
    copyrightart.erase(itr);
    print("delete copyrightart success");
  }
  
  [[eosio::action]]
  void read() {
    auto state = _config.get();
    print("createratio = ",state.createratio, "; returnratio = ",state.returnratio, "; opposeratio = ",state.opposeratio);
  }
  
  [[eosio::action]]
  void write(float createratio,float returnratio,float opposeratio){
    require_auth(_self);
    _config.set(config{createratio,returnratio,opposeratio},_self);
  }

private:
  // createart;silversilver;123;XIEJIANHUAI;《Rain》;fiction;10000.0000 POST;100000.0000 POST;1562740977;1589092977;2.0000 POST;1.0000 POST;0.7;Good Luck
  struct [[eosio::table]] copyrightart {
    uint64_t  id;
    name  address;
    std::string  author;
    std::string  artname;
    std::string  category;
    asset miniinvest;
    asset totalinvest;
    asset invest;
    asset opposeinvest;
    uint32_t  deadline;
    uint32_t  finishtime;
    asset price;
    asset earlyprice;
    float shareratio;
    uint32_t  createtime;
    std::string status;
    std::string  memo;
    uint64_t primary_key() const { return id; }
    uint64_t get_secondary_1() const { return address.value; }
    EOSLIB_SERIALIZE(copyrightart,(id)(address)(author)(artname)(category)(miniinvest)(totalinvest)(invest)(opposeinvest)(deadline)(finishtime)(price)(earlyprice)(shareratio)(createtime)(status)(memo))
  };
  typedef eosio::multi_index<"copyrightart"_n,copyrightart,indexed_by<"byaddress"_n,const_mem_fun<copyrightart,uint64_t,&copyrightart::get_secondary_1>>> art_index;
  
  struct [[eosio::table]] investdetail {
    uint64_t id;
    name address;
    name investor;
    asset invest;
    std::string status;
    uint64_t primary_key() const { return id; }
    uint64_t get_secondary_1() const { return address.value; }
    EOSLIB_SERIALIZE(investdetail,(id)(address)(investor)(invest)(status))
  };
  typedef eosio::multi_index<"investdetail"_n,investdetail,indexed_by<"byaddress"_n,const_mem_fun<investdetail,uint64_t,&investdetail::get_secondary_1>>> invest_index;

  struct [[eosio::table]] config {
    float createratio = 0.01;
    float returnratio = 0.9;
    float opposeratio = 0.3;
  };
  typedef eosio::singleton<"config"_n,config> config_singleton;
  config_singleton _config;
};    