#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include <iostream>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if(arp_buffer.find(next_hop.ipv4_numeric()) != arp_buffer.end()){
    // 在arp缓存中找到了
    // 构建以太网帧
    EthernetFrame re =  {{ arp_buffer[next_hop.ipv4_numeric()], ethernet_address_, EthernetHeader::TYPE_IPv4 },
                          serialize( dgram ) };
    // 放入缓存
    frame_buffer.push(re);
  } else {
    // 没有在arp缓存中找到
    // 如果正在arp请求，就不再查询了
    for(auto i : requesting_ip){
      cout<< i.first << "  " << i.second <<endl;
    }

    if(requesting_ip.find(next_hop.ipv4_numeric()) != requesting_ip.end()){
      // 正在查询这个ip
      // 放入未发送的ip分组缓存
      nosend_ipsegment[next_hop.ipv4_numeric()].push(dgram);
    } else {
      // 之前没有查询这个ip地址，发送arp请求
      ARPMessage request_msg;
      request_msg.opcode = ARPMessage::OPCODE_REQUEST;
      request_msg.sender_ethernet_address = ethernet_address_;
      // request_msg.target_ethernet_address = ETHERNET_BROADCAST;
      request_msg.sender_ip_address = ip_address_.ipv4_numeric();
      request_msg.target_ip_address = next_hop.ipv4_numeric();

      // 封装在以太网帧中
      EthernetFrame arp_request = {{ ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(request_msg)};

      // 放入发送队列
      frame_buffer.push(arp_request);

      // 放入未发送的ip分组缓存
      nosend_ipsegment[next_hop.ipv4_numeric()].push(dgram);

      // 放入正在查询的ip地址
      requesting_ip[next_hop.ipv4_numeric()] = cur_time;
    }
  }
}

// frame: the incoming Ethernet frame  接收帧
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 如果不是广播地址，并且不是目的地址，就丢弃
  if(frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_){
    return {}; 
  }
  if(frame.header.type == EthernetHeader::TYPE_IPv4){
    // 如果是IPV4的包,就返回
    InternetDatagram ipv4_msg ;
    if ( parse( ipv4_msg, frame.payload ) ) {
      return ipv4_msg;
    }
  } else{
    // 如果是arp报文
    ARPMessage arp_msg ;
    if( parse( arp_msg, frame.payload ) ){
      if(arp_msg.opcode == ARPMessage::OPCODE_REQUEST){
        // 如果是一个arp请求报文，先学习，看他请求的目的ip是不是我
        // 放入arp缓存和arp计时器 
        arp_buffer[arp_msg.sender_ip_address] = arp_msg.sender_ethernet_address;
        arp_timer[arp_msg.sender_ip_address] = cur_time;

        if(arp_msg.target_ip_address == ip_address_.ipv4_numeric()){
          // 他请求的正好是我，我要回给他arp响应报文
          ARPMessage reply_msg;
          reply_msg.opcode = ARPMessage::OPCODE_REPLY;
          reply_msg.sender_ethernet_address = ethernet_address_;
          reply_msg.target_ethernet_address = arp_msg.sender_ethernet_address;
          reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
          reply_msg.target_ip_address = arp_msg.sender_ip_address;

          // 将arp响应报文封装在以太网帧
          EthernetFrame arp_reply = {{ arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(reply_msg)};

          // 放入发送队列
          frame_buffer.push(arp_reply);
        }
      } else if (arp_msg.opcode == ARPMessage::OPCODE_REPLY){
        // 如果是给我的arp响应报文
        // 学习，放入arp缓存
        arp_buffer[arp_msg.sender_ip_address] = arp_msg.sender_ethernet_address;
        arp_timer[arp_msg.sender_ip_address] = cur_time;

        // 让等待这个mac地址的数据帧放入发送队列
        queue<InternetDatagram> tt = nosend_ipsegment[arp_msg.sender_ip_address];
        while(!tt.empty()){
          EthernetFrame re =  {{ arp_msg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( tt.front() ) };
          // 放入缓存
          frame_buffer.push(re);
          tt.pop();
        }
        nosend_ipsegment.erase(arp_msg.sender_ip_address);

        // 将该ip地址移出正在查询的IP
        requesting_ip.erase(arp_msg.sender_ip_address) ;
      }
    }
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  cur_time += ms_since_last_tick;

  // 每过一段时间，就要清除到期的arp缓存
  for(auto i = arp_timer.begin(); i != arp_timer.end(); ){
    if(i->second + 30000 <= cur_time){
      // 超时清除
      arp_buffer.erase(arp_buffer.find(i->first));
      i = arp_timer.erase(i);
    } else ++i;
  }

  // 每过一段时间还要清除到期的arp请求
  for(auto j = requesting_ip.begin(); j != requesting_ip.end(); ){
    if(j->second + 5000 <= cur_time){
      // 超时清除
      //cout<<"从requesting_ip中删除了这个"<< j->first << "  " << j->second <<"  cur_time: "<<cur_time<<endl;
      j = requesting_ip.erase(requesting_ip.find(j->first));
      
    } else ++j;
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if(frame_buffer.empty()) return {};
  else {
    EthernetFrame re;
    re = frame_buffer.front();
    // cout<< "发送帧 " << re.header.to_string() << endl;
    frame_buffer.pop();
    return re;
  }
}
