#include "router.hh"

#include <iostream>
#include <limits>
#include<bitset>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  // 根据路由前缀，前缀长度，下一跳地址，下一跳接口索引在路由表中添加一条路由
  router_table.emplace_back(route_prefix, prefix_length, next_hop, interface_num);

}

optional<Router::router_table_item> Router::find_next_inerface(InternetDatagram data){
  size_t max_match_length = 0;
  bool iffind = false;
  size_t re_route;

  // 遍历去找
  for(size_t i=0; i < router_table.size(); i++){
    if(router_table[i].prefix_length  < max_match_length) continue;
    else{
      // 计算该数据包的网络前缀
      if(router_table[i].prefix_length == 0){
        max_match_length = router_table[i].prefix_length;
        iffind = true;
        re_route = i;
      } else {
        uint32_t data_prefix = data.header.dst >> (32U - router_table[i].prefix_length);
        uint32_t route_prefix_t = router_table[i].route_prefix >> (32U - router_table[i].prefix_length);
        if(data_prefix == route_prefix_t){
          // 网络前缀匹配
          max_match_length = router_table[i].prefix_length;
          iffind = true;
          re_route = i;
        }
      }
      
    }
  }

  // 如果找到了，返回这条路由表项
  if(iffind) return router_table[re_route];
  else return {};

}

void Router::route() {
  // 路由转发
  // 对每一个接口队列里面的数据包，找到转发的接口，调用mayberec
  for(size_t i = 0; i<interfaces_.size(); i++){
    auto cur_data = interfaces_[i].maybe_receive();
    while(cur_data.has_value()){
      InternetDatagram data = cur_data.value();
      if(data.header.ttl > 1){
        // ttl > 1
        data.header.ttl -- ;
        data.header.compute_checksum();
        auto next_interface = find_next_inerface(data);
        if(next_interface.has_value()){
          // cout<< "交给ip地址为"<< interfaces_[next_interface.value()].return_ip_address()<<" ,接口编号为 "<<next_interface.value()<< "的接口"<< endl;
          // 优先写下一跳地址，没有下一跳才写ip数据包里面的目的地址
          interfaces_[next_interface.value().interface_num].send_datagram(data, next_interface.value().next_hop.value_or(Address::from_ipv4_numeric( data.header.dst)));
        }
      }
      cur_data = interfaces_[i].maybe_receive();
    }
  }

}
