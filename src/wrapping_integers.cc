#include "wrapping_integers.hh"
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // 已知绝对地址和初始序列号转化为相对地址
  uint32_t re = uint32_t(n) + zero_point.get_raw_value();

  return Wrap32 { re };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 已知相对地址，初始序列号，和检查点，得到绝对地址
  // 一个相对地址可以对应多个绝对地址，所以要返回离检查点最近的绝对地址

  // 先算检查点对应的相对地址是什么
  uint32_t re_checkpoint = uint32_t(checkpoint) + zero_point.get_raw_value();
  
  // 检查点到相对地址的两个方向的距离，有正有负
  uint64_t diff1 ; // 直接距离
  uint64_t diff2 ; // 轮回一遍之后的距离
  uint64_t re ;

  if(re_checkpoint > raw_value_) {
    diff1 = re_checkpoint - raw_value_;
    diff2 = uint32_t(uint64_t(raw_value_) + (uint64_t(1)<<32) - uint64_t(re_checkpoint));
    if(diff1 < diff2){
      re = checkpoint - diff1;
      if(re > checkpoint) re = checkpoint + diff2;  // 异常处理
    } else re = checkpoint + diff2;
  } else {
    diff1 = raw_value_ - re_checkpoint;
    diff2 = uint32_t(uint64_t(re_checkpoint) + (uint64_t(1)<<32) - uint64_t(raw_value_));
    if(diff1 < diff2) {
      re = checkpoint + diff1;
    } else {
      re = checkpoint - diff2;
      if(re > checkpoint)  re = checkpoint + diff1;  // 异常处理
    }
  }
  //cout<<"相对地址："<<raw_value_ << " 检查点："<<checkpoint<< " 起始地址："<< zero_point.get_raw_value()<<endl;
  //cout <<"diff1: "<<diff1<<"  diff2: "<< diff2<<"  re: "<<re<<endl;
  return re;
}
