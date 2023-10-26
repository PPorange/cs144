#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
#include <iostream>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ),cur_RTO_ms(initial_RTO_ms), waiting_for_ack(),waiting_for_send(), receive_window_size(1),
  checkpoint(0), Seqno(0),nums_in_flight(0), retransmissions_times(0) ,status(CLOSED), cur_spent_time(0)
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return nums_in_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissions_times;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // 在push中已经确保，队列中都是在窗口内的要传送的
  if( waiting_for_send.empty() ){
    return {};
  } else {
    TCPSenderMessage tt = waiting_for_send.front();
    // cout<<"use: maybe_send "<< tt.seqno.unwrap(isn_, checkpoint)<< ' ' << tt.payload.length() << "waiting_for_send_size :"<< waiting_for_send.size() << endl;
    waiting_for_send.pop_front();
    // cout<< waiting_for_send.size()<< endl;
    return tt;
  }
}

void TCPSender::push_in_que(TCPSenderMessage msg){
  uint64_t pack_size =  msg.sequence_length();
  //增加待回答队列长度
  nums_in_flight += pack_size;
  //改变序列号和检查点
  Seqno += pack_size;
  checkpoint = Seqno;

  // 放入队列
  waiting_for_ack.push(msg);
  waiting_for_send.push_back(msg);

}

void TCPSender::push( Reader& outbound_stream )
{
  // cout<<"used push , now statu: "<<status<<endl;
  // 从reader输出流中读取字节，生成tcpsendermessage
  /*struct TCPSenderMessage
  {
    Wrap32 seqno { 0 };
    bool SYN { false };
    Buffer payload {};
    bool FIN { false };

    // How many sequence numbers does this segment use?
    size_t sequence_length() const { return SYN + payload.size() + FIN; }
  };*/
  // 如果sender是COLSED状态, 发送SYN
  if(status == CLOSED){
    TCPSenderMessage send_SYN_msg;
    send_SYN_msg.seqno = isn_; 
    send_SYN_msg.SYN = true; 

    // 修改状态,因为有可能发送SYN+FIN的包，所以要加个判断
    if(outbound_stream.is_finished()){
      status = SYN_SENT;
      send_SYN_msg.FIN = true;
    } else {
      status = SYN_SENT;
    }
    
    // 放入待发送队列
    push_in_que(send_SYN_msg);
  }

  // 如果sender是SYN_SEND，就等待对方的SYN
  if(status == SYN_SENT) return ;

  // 如果状态是ESTABLISHED，就可以愉快的生成msg了
  if(status == ESTABLISHED){
    // 首先计算还能生成的字节数，窗口大小-已经发送的
    if(nums_in_flight > receive_window_size){
      return ;
    }

    uint64_t able_sent_nums; // 还能生成多少字节的msg
    if(receive_window_size == 0){
      // 如果接收窗口为0，要假装有一个窗口的大小可以发送
      able_sent_nums = 1;

    } else {
      able_sent_nums = receive_window_size - nums_in_flight;
    }

    while(able_sent_nums > 0 && outbound_stream.bytes_buffered()){
      uint64_t cur_sent_nums = min( TCPConfig::MAX_PAYLOAD_SIZE, min(able_sent_nums, outbound_stream.bytes_buffered()));

      // 构建TCP报文
      TCPSenderMessage cur_est_msg;
      cur_est_msg.seqno = Wrap32::wrap(Seqno , isn_);
      cur_est_msg.payload = Buffer(outbound_stream.peek(cur_sent_nums));
      outbound_stream.pop(cur_sent_nums);

      able_sent_nums -= cur_sent_nums;

      // 检查reader是否关闭，是否是最后一个tcp报文段
      if(outbound_stream.is_finished() && able_sent_nums > 0){
        // 设置fin， 切换状态
        cur_est_msg.FIN = true;
        status = FIN_WAIT;
      }

      // 放入队列
      push_in_que(cur_est_msg);

    }
     // reader中没有要传送的字节了，单独送fin报文
    if(outbound_stream.is_finished() && status == ESTABLISHED && able_sent_nums>0){
      TCPSenderMessage cur_fin_msg;
      cur_fin_msg.FIN = true;
      cur_fin_msg.seqno = Wrap32::wrap(Seqno , isn_);

      // 放入队列
      push_in_que(cur_fin_msg);

      // 切换状态
      status = FIN_WAIT;
    }

  }
  
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 发送一个空的msg, 但是序号要对
  TCPSenderMessage empty_msg;
  empty_msg.seqno = Wrap32::wrap(Seqno, isn_);
  return empty_msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 从接收方收到确认包，要对发送方一系列参数进行修改
  // 丢弃错误的包
  // 1. 没有ack的包 - 什么回应包没有ack? 猜测对端返回值也是可选项 所以返回是空ack 证明对端没有收到信息
  if (msg.ackno.has_value() == false){
    return;
  }
  // 2. 超过Seqno的 历史连接遗留
  if(msg.ackno.value().unwrap(isn_ , checkpoint) > Seqno){
    return;
  }
  // 3. 小于确认接受编号的 超时严重的包
  if(msg.ackno.value().unwrap(isn_ , checkpoint) < Seqno - nums_in_flight){
    return;
  }

  // 处理状态
  // 1.SYN_SENT到ESTABLISHED
  if(status == SYN_SENT &&  msg.ackno == Wrap32::wrap(1,isn_)){
    status = ESTABLISHED;
  }
  // 2.FIN_WAIT到FINISH
  if(status == FIN_WAIT && msg.ackno.value().unwrap(isn_ , checkpoint) == Seqno){
    status = FINISH;
  }

  // 更新窗口大小
  receive_window_size = msg.window_size;

  // 更新等待ack队列
  // 计算确认号
  uint64_t ack = msg.ackno.value().unwrap(isn_ , checkpoint);

  while(!waiting_for_ack.empty()){
    TCPSenderMessage tt = waiting_for_ack.front();
    // cout<< "ack: "<< ack<< "第一个包的最后一个字节"<<tt.seqno.unwrap(isn_ , checkpoint) + (uint64_t)tt.sequence_length()<< endl;
    if(ack >= tt.seqno.unwrap(isn_ , checkpoint) + (uint64_t)tt.sequence_length()){
      // 正确接收确认
      nums_in_flight -= (uint64_t)tt.sequence_length();
      waiting_for_ack.pop();
      // 重启计时
      if(msg.window_size != 0){
        cur_RTO_ms = initial_RTO_ms_;
      }
      cur_spent_time = 0;
      retransmissions_times = 0;
    } else break;
  }

}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // 判断是否有包超时，重传最早那个包
  //
  cur_spent_time += (uint64_t)ms_since_last_tick;
  if(cur_spent_time >= cur_RTO_ms && !waiting_for_ack.empty()){
    // 有超时,重传，清空计时器
    //cout<<"timeout, retransmissions_times:"<< retransmissions_times+1<<"   cur_spent_time:"<<cur_spent_time<<" ,cur_RTO_ms:"<< cur_RTO_ms<< endl;
    TCPSenderMessage tt = waiting_for_ack.front();
    waiting_for_send.push_front(tt);
    if(receive_window_size != 0)
      cur_RTO_ms *= 2;
    retransmissions_times ++ ;
    //if(retransmissions_times > 8) retransmissions_times = 8;
    cur_spent_time = 0;
  } else {
    //cout<<"notimeout, cur_spent_time:"<<cur_spent_time<<" ,cur_RTO_ms:"<< cur_RTO_ms<<endl;
  }

}
