#include "tcp_receiver.hh"
#include <iostream>

using namespace std;

TCPReceiver::TCPReceiver ():ISN(0), SYN_flag(false), FIN_flag(false), checkpoint(0) {

}

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // 接收函数
  // cout<< "收到: syn " << message.SYN << " seq " << message.seqno.get_raw_value() << " fin "<<message.FIN <<" length "<< message.sequence_length() <<endl;
  if( !SYN_flag && message.SYN ){  // 只有当之前没有收到SYN时收到SYN才能设置ISN
    ISN = message.seqno;
  } else if(!SYN_flag){
    return ;
  } 
  reassembler.insert( message.seqno.unwrap(ISN, checkpoint), message.payload, message.FIN, inbound_stream );
  checkpoint = inbound_stream.bytes_pushed();
  
  if(!SYN_flag && message.SYN){
    SYN_flag = true;
    reassembler.insert_SYN();
  }

  if(inbound_stream.is_closed() && !FIN_flag){
    // 只有等流关闭了，才能把FIN_flag设为true
    FIN_flag = true;
    reassembler.insert_FIN();
  }

}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // 发送数据
  TCPReceiverMessage re;
  if(SYN_flag)
    re.ackno =  Wrap32(Wrap32::wrap(inbound_stream.bytes_pushed() ,ISN).get_raw_value()) + uint32_t(SYN_flag) + uint32_t(FIN_flag);
  re.window_size = (uint16_t) min(inbound_stream.available_capacity(), (uint64_t)UINT16_MAX);
  // cout<< "available_capacity: "<<
  return re;
}
